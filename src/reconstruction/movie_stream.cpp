// The movie library's CD stream ring (Sony libcds, 801d57dc..801d68b4 of
// the image loaded at 801d3000). The ring holds `count` 32-byte slot records
// followed by `count` 2016-byte payloads. The data callback (801d5900 ->
// 801d5d54, interrupt context) receives each sector of a movie frame: DMA3
// moves its 32-byte STR header into the slot and its payload after the
// records; the last sector's DMA completion (801d5a04) marks the frame's
// first slot complete. The player takes a complete frame (801d5c70) and
// frees its slots once decoded (801d5b7c).
//
// Slot states (first halfword): 0 free, 1 wrap to the ring start, 2 frame
// complete, 3 filled, 4 taken by the player.
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/program.hpp"

#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
[[noreturn]] void unrecovered(std::string_view operation, std::uint32_t address, const char *id,
                              const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}

// Ring statics.
constexpr std::uint32_t ring_slots = 0x801e8a14;     // the slot records
constexpr std::uint32_t ring_count = 0x801e8a18;     // sectors
constexpr std::uint32_t read_slot = 0x801e8a00;      // first slot of the next frame to take
constexpr std::uint32_t frame_slot = 0x801e89fc;     // first slot of the frame being filled
constexpr std::uint32_t write_slot = 0x801e89f8;     // slot the next sector fills
constexpr std::uint32_t transfer_active = 0x801e89e8; // the last sector's DMA is running
constexpr std::uint32_t seek_start = 0x801e8a0c;     // skip sectors until `start_frame`
constexpr std::uint32_t start_frame = 0x801e89dc;
constexpr std::uint32_t last_frame = 0x801e8a08; // frames at or past it end the stream
constexpr std::uint32_t memory_source = 0x801e8a04; // sectors come from memory, not the drive
constexpr std::uint32_t memory_sector = 0x801e89f0;
constexpr std::uint32_t complete_callback = 0x801e89c8;
constexpr std::uint32_t end_callback = 0x801e89cc;
constexpr std::uint32_t defer_during_mdec = 0x801e89c4; // 24-bit movies
constexpr std::uint32_t channel = 0x801e89e4;           // expected STR channel (header +2 bits 10-14)
constexpr std::uint32_t next_channel = 0x801e89d8;
constexpr std::uint32_t frame_sectors = 0x801e89bc; // u16: sectors of the frame received
constexpr std::uint32_t frame_number = 0x801e89b8;  // frame being received
constexpr std::uint32_t current_slot = 0x801e89b4;
constexpr std::uint32_t current_payload = 0x801e8a10;
constexpr std::uint32_t outcome = 0x801e8908; // why the last data callback returned

// CD controller and DMA registers (the library's pointer table, 801e8830..).
constexpr std::uint32_t cd_index = 0x1f801800;
constexpr std::uint32_t cd_data = 0x1f801802;
constexpr std::uint32_t cd_request = 0x1f801803;
constexpr std::uint32_t cd_delay = 0x1f801018;
constexpr std::uint32_t common_delay = 0x1f801020;
constexpr std::uint32_t dma_control = 0x1f8010f0;
constexpr std::uint32_t dma_interrupt_register = 0x1f8010f4;
constexpr std::uint32_t dma1_channel = 0x1f801098;

constexpr std::uint32_t payload_bytes = 2016;

// A byte of a register only software changes: from its last recorded
// store of any width, else from the observed I/O page.
std::uint32_t io_byte(const ResidentState &resident, std::uint32_t address) {
    const auto &writes = resident.hardware_writes;
    for (auto write = writes.rbegin(); write != writes.rend(); ++write)
        if (address >= write->address && address - write->address < write->width)
            return (write->value >> (8U * (address - write->address))) & 0xffU;
    const auto offset = address - 0x1f801000U;
    if (offset >= resident.io.size())
        throw field::FieldFormatError("Hardware register outside the observed I/O page");
    return resident.io[offset];
}
} // namespace

using namespace movie;

void Program::verify_stream_registers() {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 11> table{{
        {0x801e8850, cd_index},
        {0x801e885c, cd_request},
        {0x801e88c0, cd_index},
        {0x801e88c8, cd_data},
        {0x801e88cc, cd_request},
        {0x801e88d0, cd_delay},
        {0x801e88d4, common_delay},
        {0x801e88d8, dma_control},
        {0x801e88dc, dma_interrupt_register},
        {0x801e88e0, dma1_channel},
        {0x801e88f0, 0x1f8010b8},
    }};
    for (const auto &[pointer, value] : table)
        if (memory(pointer) != value)
            throw field::FieldFormatError("The movie library's CD register table differs");
}

// 801d583c StSetRing(ring, count), then 801d5920 StClearRing.
void Program::stream_set_ring(std::uint32_t ring, std::uint32_t count) {
    set_memory(ring_slots, ring);
    set_memory(ring_count, count);
    stream_clear_ring();
}

// 801d5920 StClearRing: every slot free and every index at the start.
void Program::stream_clear_ring() {
    set_memory(read_slot, 0);
    set_memory(frame_slot, 0);
    set_memory(write_slot, 0);
    set_memory(transfer_active, 0);
    stream_clear_slots(0, memory(ring_count));
    set_memory(stream_deferred, 0);
    set_memory(frame_sectors, 0, 2);
    set_memory(frame_number, 0);
}

// 801d5c34: clear the first word of `count` slot records from `first`.
void Program::stream_clear_slots(std::uint32_t first, std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i)
        set_memory(memory(ring_slots) + ((first + i) << 5U), 0);
}

// 801d5af4 StSetStream(mode, start, end, complete, end_callback) with
// 801d5d34: seek to frame `start`, end at `end`.
void Program::stream_set_stream(std::uint32_t mode, std::uint32_t start, std::uint32_t end,
                                std::uint32_t complete, std::uint32_t ended) {
    set_memory(seek_start, 1);
    set_memory(start_frame, start);
    set_memory(last_frame, end);
    set_memory(memory_source, 0);
    set_memory(complete_callback, complete);
    set_memory(defer_during_mdec, mode & 1U);
    set_memory(channel, 0);
    set_memory(next_channel, 0);
    set_memory(frame_sectors, 0, 2);
    set_memory(frame_number, 0);
    set_memory(end_callback, ended);
}

// 801d5c70 StGetNext: take the complete frame at the read slot. Returns 0
// with its payload in `data` and its slot record in `header`, else 1.
std::uint32_t Program::stream_next(std::uint32_t &data, std::uint32_t &header) {
    const auto record = [&] { return memory(ring_slots) + (memory(read_slot) << 5U); };
    auto slot = record();
    if (memory(slot, 2) == 1) { // Wrap to the ring start.
        set_memory(read_slot, 0);
        if (memory(last_frame) != 0)
            set_memory(slot, 0, 2);
        slot = record();
    }
    if (memory(slot, 2) != 2)
        return 1;
    set_memory(slot, 4, 2);
    data = memory(ring_slots) + (memory(ring_count) << 5U) + memory(read_slot) * payload_bytes;
    header = slot;
    return 0;
}

// 801d5b7c StFreeRing(data): free the slots of the taken frame whose
// payload is `data`; 0, or 1 when that slot was not taken.
std::uint32_t Program::stream_free(std::uint32_t data) {
    const auto base = memory(ring_slots) + (memory(ring_count) << 5U);
    const auto index = static_cast<std::uint32_t>(s32(data - base) / 4 / 504);
    const auto slot = memory(ring_slots) + (index << 5U);
    if (s16(memory(slot, 2)) != 4)
        return 1;
    const auto sectors = s16(memory(slot + 6, 2));
    std::int32_t i = 0;
    for (; i < sectors; ++i)
        set_memory(memory(ring_slots) + ((index + static_cast<std::uint32_t>(i)) << 5U), 0, 2);
    set_memory(read_slot, index + static_cast<std::uint32_t>(i));
    return 0;
}

// 801d5980 StUnSetRing: in a critical section, remove the data and DMA
// callbacks and clear the controller's index and interrupt enables.
void Program::stream_unset_ring() {
    verify_stream_registers();
    // EnterCriticalSection (800404d4, BIOS syscall 1) masks interrupts.
    if (resident.cd.w_564cc == 1)
        unrecovered("stream_unset_ring", 0x801d59a4, "symbol:libcd-80040ce4",
                    "The alternate callback reset (80040ce4, 80040cd0) is not reconstructed");
    cd_dma_callback(0);               // 800413ec
    resident.cd.sync_callback = 0;    // 80040fcc
    io_write(cd_index, 0, 1);
    io_write(cd_request, 0, 1);
    // ExitCriticalSection (800404e4, BIOS syscall 2).
}

// 801d586c: Setmode `mode` (CdControlF), and for a stream (mode bit 100)
// the header mode (8005a470: 1 unless whole sectors) and the ring's DMA
// (801d5a04) and data (801d5900) callbacks; then ReadS. Returns whether
// ReadS was accepted.
std::int32_t Program::stream_read(std::uint32_t mode) {
    const std::array<std::uint8_t, 4> setmode{static_cast<std::uint8_t>(mode), 0, 0, 0};
    deliver_arrivals(0x801d5888);
    static_cast<void>(cd_control_wait(0x0e, &setmode, nullptr));
    if ((mode & 0x100U) != 0) {
        resident.cd.stream_header_mode = (mode & 0x20U) != 0 ? 0 : 1;
        cd_dma_callback(0x801d5a04);            // 800413ec
        resident.cd.sync_callback = 0x801d5900; // 80040fcc
    }
    deliver_arrivals(0x801d58e4);
    return cd_control_wait(0x1b, nullptr, nullptr);
}

// 801d5a94: the location after the last sector the stream saw and that
// sector's frame; -1 while sector headers are not read (8005a470 set).
std::uint32_t Program::stream_position(std::array<std::uint8_t, 4> &location) {
    if (resident.cd.stream_header_mode != 0)
        return 0xffffffffU;
    const std::array<std::uint8_t, 4> seen{
        static_cast<std::uint8_t>(memory(stream_location, 1)),
        static_cast<std::uint8_t>(memory(stream_location + 1, 1)),
        static_cast<std::uint8_t>(memory(stream_location + 2, 1)),
        static_cast<std::uint8_t>(memory(stream_location + 3, 1))};
    location = cd_position(cd_sector(seen) + 1U);
    return memory(stream_frame);
}

// 801d5a04, the DMA3 completion of a frame's last sector (and the direct
// call of a memory-sourced stream): mark the frame complete at its first
// slot, record its last sector's location and frame, and start the next.
void Program::stream_frame_complete() {
    const auto slot = memory(ring_slots) + (memory(frame_slot) << 5U);
    set_memory(slot, 2, 2);
    set_memory(stream_location, memory(slot + 28));
    set_memory(stream_frame, memory(slot + 8));
    set_memory(frame_slot, memory(write_slot));
    if (memory(complete_callback) != 0)
        unrecovered("stream_frame_complete", 0x801d5a74, "symbol:stream-complete-callback",
                    "A frame-complete stream callback is not reconstructed");
    set_memory(transfer_active, 0);
}

// 801d5900 -> 801d5d54 StCdInterrupt, the ring's data callback: receive
// one sector. Sectors of another channel, out of sequence, before the
// start frame or past the last frame are dropped; a frame that does not fit
// before the ring's end restarts at its start behind a wrap slot.
void Program::stream_interrupt() {
    verify_stream_registers();
    if (memory(transfer_active) == 1)
        return;
    const auto drop = [&](std::uint32_t reason) {
        if (memory(memory_source) != 0)
            set_memory(memory_sector, memory(memory_sector) + 1);
        set_memory(outcome, reason);
    };
    if (memory(defer_during_mdec) != 0 &&
        (platform_read(resident.platform, 0x801d5d8c, 4) & 0x01000000U) != 0) {
        // The MDEC output DMA is running: its completion (801d30c4) takes
        // this sector.
        set_memory(stream_deferred, 1);
        drop(1);
        return;
    }
    std::array<std::uint8_t, 8> response{};
    if (cd_ready(response) == 5) // 80040f94 -> 80041dbc CdReady(1)
        return;
    if ((response[0] & 4U) != 0) { // The drive is playing audio.
        set_memory(outcome, 3);
        return;
    }
    auto slot = memory(ring_slots) + (memory(write_slot) << 5U);
    set_memory(current_slot, slot);
    if (memory(slot, 2) != 0) { // The ring is full.
        drop(4);
        return;
    }
    io_write(cd_index, 0, 1);
    io_write(cd_request, 0, 1);
    io_write(cd_index, 0, 1);
    io_write(cd_request, 0x80, 1); // Request the sector's data.
    io_write(cd_delay, 0x20943, 4);
    io_write(common_delay, 0x1323, 4);
    if (resident.cd.stream_header_mode == 0)
        unrecovered("stream_interrupt", 0x801d5f0c, "symbol:stream-sector-header",
                    "Reading whole-sector headers from the data FIFO is not reconstructed");
    if (memory(memory_source) != 0)
        unrecovered("stream_interrupt", 0x801d5f90, "symbol:stream-memory-source",
                    "A memory-sourced stream is not reconstructed");
    // The 32-byte STR header into the slot record (DMA3, burst).
    stream_dma(slot, 8, 0, 0x11000000U, 0);
    if ((platform_read(resident.platform, 0x801d5fd0, 4) & 0x01000000U) != 0)
        while ((platform_read(resident.platform, 0x801d5fe8, 4) & 0x01000000U) != 0) {
        }
    // Bytes 28-31 of the record take the handler's four-byte location
    // buffer, which only a whole-sector read fills: here its stack slot's
    // earlier contents, a recorded input.
    set_memory(slot + 28, platform_read(resident.platform, 0x801d600c, 4));
    io_write(cd_delay, 0x20843, 4);
    io_write(common_delay, 0x1325, 4);
    if (memory(seek_start) == 1) {
        if (const auto wanted = memory(start_frame); wanted != 0) {
            if (wanted != memory(slot + 8, 2)) { // Before the start frame.
                set_memory(slot, 0, 2);
                if (memory(memory_source) != 0)
                    set_memory(memory_sector, memory(memory_sector) + 1);
                return;
            }
            set_memory(seek_start, 0);
        }
    }
    if (memory(slot, 2) != 0x160 || ((memory(slot + 2, 2) >> 10U) & 0x1fU) != memory(channel)) {
        // Not a movie sector of the selected channel.
        if (memory(memory_source) != 0)
            set_memory(memory_sector, 0);
        set_memory(outcome, 5);
        set_memory(slot, 0, 2);
        return;
    }
    const auto discard_frame = [&] {
        set_memory(frame_number, 0);
        set_memory(frame_sectors, 0, 2);
        stream_clear_slots(memory(frame_slot), memory(write_slot) - memory(frame_slot));
        set_memory(write_slot, memory(frame_slot));
        set_memory(slot, 0, 2);
    };
    const bool in_sequence = s16(memory(frame_sectors, 2)) == static_cast<std::int32_t>(memory(slot + 4, 2)) &&
                             (memory(frame_number) == 0 || memory(frame_number) == memory(slot + 8, 2));
    if (!in_sequence) {
        discard_frame();
        drop(6);
        return;
    }
    if (memory(slot + 4, 2) == 0) { // A frame's first sector.
        set_memory(frame_sectors, 0, 2);
        set_memory(frame_number, memory(slot + 8, 2));
        if (memory(last_frame) != 0 && !(memory(frame_number) < memory(last_frame))) {
            discard_frame();
            set_memory(seek_start, 1);
            if (memory(end_callback) != 0)
                unrecovered("stream_interrupt", 0x801d62b0, "symbol:stream-end-callback",
                            "A stream end callback is not reconstructed");
            drop(7);
            return;
        }
        if (memory(ring_count) - memory(write_slot) - 1U < memory(slot + 6, 2)) {
            // The frame does not fit before the ring's end.
            if (memory(last_frame) == 0) {
                set_memory(slot, 1, 2);
                set_memory(seek_start, 1);
                if (memory(end_callback) != 0)
                    unrecovered("stream_interrupt", 0x801d635c, "symbol:stream-end-callback",
                                "A stream end callback is not reconstructed");
                drop(8);
                return;
            }
            if (s16(memory(memory(ring_slots), 2)) != 0) { // The ring start is busy.
                set_memory(slot, 0, 2);
                drop(9);
                return;
            }
            set_memory(slot, 1, 2); // Wrap: continue in the first slot.
            set_memory(write_slot, 0);
            const auto first = memory(ring_slots);
            for (std::uint32_t i = 0; i < 8; ++i)
                set_memory(first + 4 * i, memory(slot + 4 * i));
            slot = first;
            set_memory(current_slot, slot);
        }
        set_memory(frame_slot, memory(write_slot));
    }
    set_memory(outcome, 10);
    set_memory(frame_sectors, memory(frame_sectors, 2) + 1, 2);
    set_memory(current_payload, memory(ring_slots) + (memory(ring_count) << 5U) +
                                    memory(write_slot) * payload_bytes);
    std::uint32_t mode = 0x11000000U;
    if (memory(defer_during_mdec) != 0) {
        io_write(cd_delay, 0x20943, 4);
        io_write(common_delay, 0x1323, 4);
    } else {
        io_write(cd_delay, 0x21020843U, 4);
        mode = 0x11400100U;
    }
    const bool last = memory(slot + 6, 2) - 1U == memory(slot + 4, 2);
    if (last)
        set_memory(transfer_active, 1);
    stream_dma(memory(current_payload), 504, 0, mode, last ? 1 : 0);
    if (last) {
        set_memory(frame_sectors, 0, 2);
        set_memory(frame_number, 0);
        set_memory(channel, memory(next_channel));
    }
    io_write(common_delay, 0x1325, 4);
    set_memory(memory(current_slot), 3, 2);
    set_memory(write_slot, memory(write_slot) + 1);
}

// 801d66f8(3, address, words, 0, control, interrupt): start a DMA3
// transfer from the CD data FIFO once it holds data, with its completion
// interrupt enabled or not. The transfer's bytes come from the drive.
void Program::stream_dma(std::uint32_t address, std::uint32_t words, std::uint32_t blocks,
                         std::uint32_t control, std::uint32_t interrupt) {
    constexpr std::uint32_t channel_control = 0x1f8010b8;
    if ((platform_read(resident.platform, 0x801d673c, 4) & 0x01000000U) != 0)
        for (std::uint32_t polls = 0;; ++polls) {
            if (polls == 0x10000)
                unrecovered("stream_dma", 0x801d67bc, "symbol:printf-80019964",
                            "The DMA busy message is not reconstructed");
            if ((platform_read(resident.platform, 0x801d676c, 4) & 0x01000000U) == 0)
                break;
        }
    const auto enables = io_byte(resident, dma_interrupt_register + 2);
    io_write(dma_interrupt_register + 2, interrupt == 1 ? enables | 8U : enables & ~8U, 1);
    io_write(dma_control, io_latch(dma_control, 4) | (1U << 15U), 4);
    io_write(0x1f8010b0, address, 4);
    io_write(0x1f8010b4, (words << 16U) | blocks, 4);
    if ((platform_read(resident.platform, 0x801d6854, 1) & 0x40U) == 0)
        while ((platform_read(resident.platform, 0x801d6868, 1) & 0x40U) == 0) {
        }
    io_write(channel_control, control, 4);
    dma_store(address, resident.drive.transfer(words * 4U));
}

} // namespace xem::reconstruction
