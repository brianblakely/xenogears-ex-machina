// Resident disc and libcd calls the movie library reaches beyond the file
// reads of disc_read.cpp: the blocking command forms CdControlF (80040fe4)
// and CdControlB (80041248) with their waits (the status wait of 80042088,
// CdSync 80041b3c), CdReady (80041dbc), and the resident directory and
// read-mode helpers 800284b4, 800289d0, 8002a428 and 8002a498.
//
// A main-thread wait spins on a status byte that the CD interrupt handler
// sets. Each pass first delivers the interrupt arrivals recorded before its
// load of that byte, then takes the load's recorded value, which must equal
// the status the reconstructed handler left: the pass count, which the
// timeout counter 8005a22c keeps, is platform timing.
#include "xem/reconstruction/program.hpp"

#include <bit>
#include <string>

namespace xem::reconstruction {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }

std::uint32_t command_entry(const std::array<std::uint32_t, 32> &table, std::uint8_t command) {
    if (command >= table.size())
        throw field::FieldFormatError("CD command outside the libcd command tables");
    return table[command];
}
std::uint32_t table_bytes(std::span<const std::uint8_t> data, std::uint32_t offset,
                          std::uint32_t width, const char *table) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError(std::string(table) + " read outside its loaded extent");
    std::uint32_t result = 0;
    for (std::uint32_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
[[noreturn]] void host_file(std::uint32_t address) {
    throw MissingDependency({"disc_host_file", address, {}, {}}, "symbol:host-file-io", false,
                            "The host-file read path (8004fe48 nonzero) is not reconstructed");
}
// The deadline and poll checks of one pass of a libcd wait (8004252c resets
// the drive after a timeout, which is not reconstructed).
void timeout_pass(ResidentState &resident, std::uint32_t site) {
    if (s32(resident.cd_sync_deadline) < s32(resident.vsync_counter) ||
        0x3c0000 < s32(resident.cd_sync_polls++))
        throw MissingDependency({"cd_wait_timeout", site, {}, {}}, "symbol:libcd-timeout-reset",
                                false, "The CD wait timeout and reset path is not reconstructed");
}
} // namespace

std::array<std::uint8_t, 4> cd_position(std::uint32_t sector) {
    const auto frames = s32(sector + 150U);
    const auto seconds = frames / 75;
    const auto bcd = [](std::int32_t value) {
        return static_cast<std::uint8_t>(value / 10 * 16 + value % 10);
    };
    return {bcd(seconds / 60), bcd(seconds % 60), bcd(frames % 75), 0};
}

std::uint32_t cd_sector(const std::array<std::uint8_t, 4> &location) {
    const auto decimal = [](std::uint8_t value) { return (value >> 4U) * 10U + (value & 15U); };
    return (decimal(location[0]) * 60U + decimal(location[1])) * 75U + decimal(location[2]) - 150U;
}

bool Program::deliver_pending_front() {
    using Kind = PlatformInput::Kind;
    const auto &inputs = resident.platform;
    bool any = false;
    while (!inputs.empty() && inputs.front().site == 0 &&
           (inputs.front().kind == Kind::interrupt || inputs.front().kind == Kind::tick)) {
        static_cast<void>(deliver_interrupt());
        any = true;
    }
    return any;
}

// 800284b4: the directory group (a multiple of four) and index within it of
// the selected directory (8004fe14), both zero when none matches.
std::array<std::uint32_t, 2> Program::current_directory() const {
    const auto &read = resident.disc_read;
    for (std::uint32_t i = 0; i < 64; ++i)
        if (table_bytes(read.directories, i * 2, 2, "Directory table") == read.directory + 1U)
            return {i & ~3U, i & 3U};
    return {0, 0};
}
void Program::current_directory(std::uint32_t group, std::uint32_t index) {
    const auto [first, second] = current_directory();
    set_memory(group, first);
    set_memory(index, second);
}

// 800289d0: the file's first sector (record bytes 0-2) in the selected
// directory.
std::uint32_t Program::file_sector(std::uint32_t file) {
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x800289e8);
    const auto &read = resident.disc_read;
    return table_bytes(read.files, (file + read.directory - 1U) * 7U, 3, "File table");
}

// 8002a498: end the resident read state (8004fe34 1, 8004fe38 the argument).
void Program::cancel_disc_read(std::uint32_t offset) {
    auto &read = resident.disc_read;
    read.w_fe34 = 1;
    read.offset = offset;
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x8002a4c4);
}

// 8002a428: send Setmode `mode` for the resident reads with the command
// callback 8002a68c and read state 9.
void Program::disc_set_mode(std::uint32_t mode) {
    auto &read = resident.disc_read;
    resident.disc_pending = 9;
    resident.cd.ready_callback = 0x8002a68c; // 80040fb4
    read.b_59f18 = {static_cast<std::uint8_t>(mode), 0, 0, 0};
    static_cast<void>(cd_control(0x0e, &read.b_59f18));
}

// 80040fe4 CdControlF: like CdControl (8004111c), but each command waits
// for its acknowledgement; returns 1 once accepted, else 0 after four tries.
std::int32_t Program::cd_control_wait(std::uint8_t command,
                                      const std::array<std::uint8_t, 4> *parameter,
                                      std::array<std::uint8_t, 8> *result) {
    auto &cd = resident.cd;
    const auto ready = cd.ready_callback;
    for (auto attempt = 3; attempt != -1; --attempt) {
        cd.ready_callback = 0;
        if (command != 1 && (cd.status & 0x10U) != 0)
            static_cast<void>(cd_command(1, nullptr, false));
        if (parameter != nullptr && command_entry(cd.setloc_first, command) != 0 &&
            cd_command(2, parameter, false, result) != 0)
            continue;
        cd.ready_callback = ready;
        if (cd_command(command, parameter, false, result) == 0)
            return 1;
    }
    cd.ready_callback = ready;
    return 0;
}

// 80041248 CdControlB: CdControlF, then CdSync(0, result); 1 when the
// command completed (status 2).
std::int32_t Program::cd_control_blocking(std::uint8_t command,
                                          const std::array<std::uint8_t, 4> *parameter,
                                          std::array<std::uint8_t, 8> *result) {
    if (cd_control_wait(command, parameter, result) == 0)
        return 0;
    return cd_sync(result) == 2 ? 1 : 0;
}

// 80042250..8004247c, the waiting tail of 80042088: wait for the command's
// status, copy its response and return -1 for an error (status 5), else 0.
std::int32_t Program::cd_command_wait(std::array<std::uint8_t, 8> *result) {
    auto &cd = resident.cd;
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0; // 8004b54c(-1)
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018edc;
    const auto status = [&](std::uint32_t site) {
        static_cast<void>(deliver_pending_front()); // Arrivals recorded before this load.
        const auto value = platform_read(resident.platform, site, 1);
        if (value != cd.sync_status)
            throw PlatformInputError("The recorded CD command status differs from the handler's");
        return value;
    };
    if (status(0x80042274) == 0)
        do {
            timeout_pass(resident, 0x800422f0);
            if (cd.interrupt_poll != 0) {
                const auto index = platform_read(resident.platform, 0x80042384, 1) & 3U;
                for (auto pending = cd_getintr(); pending != 0; pending = cd_getintr()) {
                    if ((pending & 4U) != 0 && cd.sync_callback != 0)
                        cd_callback(cd.sync_callback, cd.ready_status, cd.ready_result);
                    if ((pending & 2U) != 0 && cd.ready_callback != 0)
                        cd_callback(cd.ready_callback, cd.sync_status, cd.sync_result);
                }
                io_write(cd.registers[0], index, 1);
            }
        } while (status(0x80042420) == 0);
    if (result != nullptr)
        *result = cd.sync_result;
    return cd.sync_status == 5 ? -1 : 0;
}

// 80040f94 / 80041dbc CdReady(1, result) as a data callback calls it: one pass that
// serves the controller when polling, then takes a pending end (8005678a)
// or data (80056789) status with its response; 0 when neither is pending.
std::uint32_t Program::cd_ready(std::array<std::uint8_t, 8> &result) {
    auto &cd = resident.cd;
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0;
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018eb8;
    timeout_pass(resident, 0x80041e80);
    if (cd.interrupt_poll != 0) {
        const auto index = platform_read(resident.platform, 0x80041f14, 1) & 3U;
        for (auto pending = cd_getintr(); pending != 0; pending = cd_getintr()) {
            if ((pending & 4U) != 0 && cd.sync_callback != 0)
                cd_callback(cd.sync_callback, cd.ready_status, cd.ready_result);
            if ((pending & 2U) != 0 && cd.ready_callback != 0)
                cd_callback(cd.ready_callback, cd.sync_status, cd.sync_result);
        }
        io_write(cd.registers[0], index, 1);
    }
    if (const auto status = cd.end_status; status != 0) {
        cd.end_status = 0;
        result = cd.end_result;
        return status;
    }
    if (const auto status = cd.ready_status; status != 0) {
        cd.ready_status = 0;
        result = cd.ready_result;
        return status;
    }
    return 0;
}

// 800288ec: the file's byte size rounded up to words, as a signed MIPS
// quotient.
std::uint32_t Program::file_words(std::uint32_t file) {
    const auto size = s32(file_size(static_cast<std::int32_t>(file))); // 80028738
    const auto rounded = s32(static_cast<std::uint32_t>(size) + 3U);
    return static_cast<std::uint32_t>(
               (rounded >= 0 ? rounded : s32(static_cast<std::uint32_t>(size) + 6U)) >> 2)
           << 2U;
}

// 8002a2d0: position the drive at `file` of the selected directory (Setloc,
// read state 3) or, for no file, pause it (state 5), with the command
// callback 8002a68c. Nothing while the disc is busy.
void Program::seek_file(std::int32_t file) {
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x8002a2e4);
    if (disc_busy() != 0) // 800286cc
        return;
    auto &read = resident.disc_read;
    read.read_directory = read.directory;
    if (file > 0) {
        read.location = cd_position(file_sector(static_cast<std::uint32_t>(file))); // 80041430
        resident.disc_pending = 3;
        resident.cd.ready_callback = 0x8002a68c; // 80040fb4
        static_cast<void>(cd_control(0x02, &read.location));
    } else {
        resident.disc_pending = 5;
        resident.cd.ready_callback = 0x8002a68c;
        static_cast<void>(cd_control(0x09, nullptr));
    }
}

std::int32_t Program::file_count(std::uint32_t file) {
    const auto &read = resident.disc_read;
    const auto size = s32(table_bytes(read.files, (file + read.directory - 1U) * 7U + 3U, 4,
                                      "File table"));
    return size < 0 ? static_cast<std::int16_t>(-size) : 0;
}

// Resident 8002a260: allocate count * 808 + 24 bytes, store the count,
// then select (80028a94) and reset (80028aac) the ring. The header is the
// ring the disc reads fill; the payload after it holds the chunks.
std::uint32_t Program::allocate_stream_ring(std::uint32_t blocks, std::uint32_t mode) {
    if (s32(blocks) < 1)
        return 0;
    auto block = resident::heap_allocate(resident.heap, blocks * 0x808U + 0x24U, mode, 0x8002a284);
    if (!block)
        return 0;
    auto &read = resident.disc_read;
    if (!read.ring.bytes.empty())
        throw MissingDependency({"stream_ring", 0x8002a29c, {}, {}}, "state:disc-ring-replacement",
                                false, "Replacing a Program-owned disc ring is not connected");
    for (std::size_t i = 0; i < 4; ++i)
        block->bytes[i] = static_cast<std::uint8_t>(blocks >> (8U * i));
    const auto header = static_cast<std::ptrdiff_t>(blocks * 8U + 0x24U);
    read.ring = {block->address, {block->bytes.begin(), block->bytes.begin() + header}};
    read.ring_payload = {block->address + static_cast<std::uint32_t>(header),
                         {block->bytes.begin() + header, block->bytes.end()}};
    static_cast<void>(field::select_disc_stream_ring(resident.disc_stream, block->address));
    static_cast<void>(field::reset_disc_stream_ring(resident.disc_stream, read.ring.bytes));
    return block->address;
}

// 8004293c CD_datasync(0): wait until the CD DMA (channel 3) is idle. Its
// busy bit comes from the observed I/O page, which a completed read leaves
// idle; a busy channel would need the polls' platform timing.
void Program::cd_datasync_wait() {
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0;
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018f30;
    timeout_pass(resident, 0x800429e4);
    const auto offset = resident.cd_dma_register - 0x1f801000U;
    if (offset > resident.io.size() - 4)
        throw field::FieldFormatError("DMA3 control register outside the observed I/O page");
    std::uint32_t control = 0;
    for (std::uint32_t i = 0; i < 4; ++i)
        control |= static_cast<std::uint32_t>(resident.io[offset + i]) << (8U * i);
    if ((control & 0x1000000U) != 0)
        throw MissingDependency({"cd_datasync", 0x80042a6c, {}, {}}, "timing:cd-dma-busy", false,
                                "Waiting for a busy CD DMA needs the polls' platform timing");
}

} // namespace xem::reconstruction
