// Resident file reads: 800295d8 and its call tree (80028738, 800288ec,
// 800289d0, 80028a60, 80029690, 80028998, 80028470) with the CD library calls
// read setup reaches (80041430, 800413ec, 8004b7a0, 8004c21c, 80040fb4,
// 80040fcc, 8004111c, 80042088, 80041b3c, 8004b894, 8004b54c). Controller and
// DMA register stores become HardwareWrite records; register reads come from
// the observed I/O page. Paths that wait for an interrupt, the host-file
// branch and debug printing stop with MissingDependency.
#include "xem/reconstruction/program.hpp"

#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }

std::uint32_t bytes(std::span<const std::uint8_t> data, std::uint32_t offset, std::uint32_t width,
                    const char *table) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError(std::string(table) + " read outside its loaded extent");
    std::uint32_t result = 0;
    for (std::uint32_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}

std::uint32_t command_entry(const std::array<std::uint32_t, 32> &table, std::uint8_t command) {
    if (command >= table.size())
        throw field::FieldFormatError("CD command outside the libcd command tables");
    return table[command];
}

[[noreturn]] void host_file(std::uint32_t address) {
    throw MissingDependency({"disc_host_file", address, {}, {}}, "symbol:host-file-io", false,
                            "The host-file read path (8004fe48 nonzero; 8004c318, 8004c338, "
                            "8004c348, 8004c398) is not reconstructed");
}

// 80041430 CdIntToPos: the sector as BCD minute, second and sector; the
// fourth byte is left as it was.
void set_position(std::uint32_t sector, std::array<std::uint8_t, 4> &location) {
    const auto frames = s32(sector + 150U);
    const auto seconds = frames / 75;
    const auto bcd = [](std::int32_t value) {
        return static_cast<std::uint8_t>(value / 10 * 16 + value % 10);
    };
    location[2] = bcd(frames % 75);
    location[1] = bcd(seconds % 60);
    location[0] = bcd(seconds / 60);
}

[[noreturn]] void printf_call(std::uint32_t address) {
    throw MissingDependency({"cd_debug_print", address, {}, {}}, "symbol:printf-80019964", false,
                            "CD library debug printing (80019964) is not reconstructed");
}
} // namespace

// Resident 80028a60: with zero, wait while the disc is busy, then query once
// more. Only an interrupt can end a busy disc; that wait stops here.
void Program::disc_wait(std::uint32_t once) {
    if (once == 0 && s32(disc_busy()) > 0)
        throw MissingDependency({"disc_idle_wait", 0x80028a6c, {}, {}},
                                "interrupt:disc-read-completion", false,
                                "Waiting for an earlier disc read needs its completion interrupts");
    static_cast<void>(disc_busy());
}

// Resident 80028738: the file's byte size (record bytes 3-6).
std::uint32_t Program::file_size(std::int32_t file) {
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x8002876c);
    const auto &read = resident.disc_read;
    const auto index = u32(file) + read.directory - 1U;
    return bytes(read.files, index * 7U + 3U, 4, "File table");
}

std::int32_t Program::select_directory(std::uint32_t base, std::uint32_t index) {
    auto &read = resident.disc_read;
    const auto entry = bytes(read.directories, (base + index) * 2U, 2, "Directory table");
    read.directory = entry - 1U;
    if (s32(read.directory) >= 0)
        return s32(read.directory);
    read.directory = 0;
    return -1;
}

std::int32_t Program::read_file(std::int32_t file, std::uint32_t destination, std::uint32_t offset,
                                std::uint32_t mode) {
    if (file <= 0 || s32(file_size(file)) <= 0 || destination == 0)
        return -3;
    disc_wait(0);
    auto &read = resident.disc_read;
    read.read_directory = read.directory;
    // 800289d0: the file's first sector (record bytes 0-2).
    read.sector = bytes(read.files, (u32(file) + read.directory - 1U) * 7U, 3, "File table");
    // 800288ec: the size rounded up to words, as a signed MIPS quotient.
    const auto size = s32(file_size(file));
    const auto rounded = s32(u32(size) + 3U);
    read.size = u32((rounded >= 0 ? rounded : s32(u32(size) + 6U)) >> 2) << 2U;
    return read_setup(u32(file), destination, offset, mode);
}

// Resident 80028808: the byte size of `file` in the directory of the current
// read (8004fe18), rounded up to words as a signed MIPS quotient.
std::uint32_t Program::read_size(std::int32_t file) {
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x8002882c);
    const auto &read = resident.disc_read;
    const auto index = u32(file) + read.read_directory - 1U;
    const auto size = s32(bytes(read.files, index * 7U + 3U, 4, "File table"));
    const auto rounded = s32(u32(size) + 3U);
    return u32((rounded >= 0 ? rounded : s32(u32(size) + 6U)) >> 2) << 2U;
}

// Resident 8002a394: seek to the first sector of `file` (state 3), or pause
// the drive (state 5) when `file` is not positive. The disc callback 8002a68c
// continues from that state.
void Program::seek_file(std::int32_t file) {
    auto &read = resident.disc_read;
    if (file < 1) {
        resident.disc_pending = 5;
        resident.cd.ready_callback = 0x8002a68c; // 80040fb4
        static_cast<void>(cd_control(9, nullptr));
        return;
    }
    // 800289d0: the first sector, from the selected directory (8004fe14).
    const auto sector = bytes(read.files, (u32(file) + read.directory - 1U) * 7U, 3, "File table");
    set_position(sector, read.location);
    resident.disc_pending = 3;
    resident.cd.ready_callback = 0x8002a68c; // 80040fb4
    static_cast<void>(cd_control(2, &read.location));
}

std::int32_t Program::read_files(std::int32_t offset) {
    auto &read = resident.disc_read;
    auto &list = read.list.bytes;
    if (read.list.address == 0)
        return -3;
    const auto file_of = [&](std::uint32_t entry) {
        return bytes(list, entry * 8U, 2, "File list");
    };
    std::uint32_t count = 0;
    while (file_of(count) != 0)
        ++count;
    if (count == 0)
        return -3;
    // Selection sort by file number; each swap moves the file halfword and
    // the destination word, never the unused halfword between them.
    const auto put = [&](std::uint32_t offset_in_list, std::uint32_t value, std::uint32_t width) {
        for (std::uint32_t i = 0; i < width; ++i)
            list[offset_in_list + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    for (std::uint32_t i = 0; s32(i) < s32(count - 1U); ++i) {
        auto lowest = i;
        auto file = file_of(i);
        for (auto j = i + 1U; j < count; ++j) {
            if (file_of(j) < file) {
                lowest = j;
                file = file_of(j);
            }
        }
        const auto moved = file_of(lowest);
        const auto first_file = file_of(i);
        const auto first_destination = bytes(list, i * 8U + 4U, 4, "File list");
        put(i * 8U, moved, 2);
        put(i * 8U + 4U, bytes(list, lowest * 8U + 4U, 4, "File list"), 4);
        put(lowest * 8U, first_file, 2);
        put(lowest * 8U + 4U, first_destination, 4);
    }
    disc_wait(0);
    read.read_directory = read.directory;
    read.w_59ef8 = {};
    const auto file = file_of(0);
    const auto destination = bytes(list, 4, 4, "File list");
    read.w_fe10 = 0;
    read.w_fe0c = read.list.address;
    resident.disc_error = count;
    read.w_fe00 = count;
    read.destination = destination;
    if (file == 0 || destination == 0) {
        seek_file(offset);
    } else {
        read.file = file;
        read.sector = bytes(read.files, (file + read.directory - 1U) * 7U, 3, "File table");
        read.size = read_size(s32(file));
        read.offset = u32(offset) & 0xffffU;
        read.w_fe3c = 0;
        read.w_fe34 = 0;
        read.w_5a4dc = 0;
        set_position(read.sector, read.location);
        if (resident.disc_stream.host_file_table != 0)
            host_file(0x80029cf4);
        resident.disc_pending = 1;
        cd_dma_callback(0x8002ba40);
        resident.cd.ready_callback = 0x8002a68c; // 80040fb4
        resident.cd.sync_callback = 0x8002ac24;  // 80040fcc
        ++read.requests;
        static_cast<void>(cd_control(2, &read.location));
        return 0;
    }
    read.size = 0;
    resident.disc_error = 0;
    return 0;
}

std::int32_t Program::read_stream(std::int32_t file, std::uint32_t ring, std::uint32_t offset,
                                  const std::array<std::uint16_t, 6> &parameters) {
    auto &read = resident.disc_read;
    if (ring == 0)
        return -4;
    if (read.ring.address != ring || read.ring.bytes.size() < 4)
        throw field::FieldFormatError("The stream ring's header is not Program-owned");
    const auto count = bytes(read.ring.bytes, 0, 4, "Disc ring");
    if (count < 2)
        return -4;
    if (file <= 0 || s32(file_size(file)) <= 0)
        return -3;
    disc_wait(0);
    read.read_directory = read.directory;
    read.w_59ef8 = {};
    static_cast<void>(field::select_disc_stream_ring(resident.disc_stream, ring)); // 80028a94
    read.file = u32(file);
    read.sector = bytes(read.files, (u32(file) + read.directory - 1U) * 7U, 3, "File table");
    // 800288ec: the size rounded up to words, as a signed MIPS quotient.
    const auto size = s32(file_size(file));
    const auto rounded = s32(u32(size) + 3U);
    read.size = u32((rounded >= 0 ? rounded : s32(u32(size) + 6U)) >> 2) << 2U;
    read.destination = ring + count * 8U + 0x24U;
    read.ring_slots = ring + 4U;
    resident.disc_error = 1;
    read.offset = offset & 0xffffU;
    read.w_fe10 = 0;
    resident.disc_stream.active_block_count = s32(count);
    read.h_fe26 = 0;
    read.h_fe28 = 0;
    read.w_fe0c = 0;
    read.w_fe34 = 0;
    read.w_5a4dc = 0;
    read.h_59f24 = parameters;
    read.w_59f3c = 0;
    read.h_59f40 = {};
    read.w_59f4c = {};
    static_cast<void>(field::reset_disc_stream_ring(resident.disc_stream, read.ring.bytes));
    set_position(read.sector, read.location);
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x8002a0d4);
    resident.disc_pending = 1;
    cd_dma_callback(0x8002bb50);
    resident.cd.ready_callback = 0x8002a68c; // 80040fb4
    resident.cd.sync_callback = 0x8002b5d0;  // 80040fcc
    ++read.requests;
    static_cast<void>(cd_control(2, &read.location));
    return 0;
}

// 80028a94 then the ring checks of 80029690: select `destination` as the ring,
// point the read at its payload and reset its slots (80028aac). Returns the
// ring's block count; zero means an empty ring.
std::int32_t Program::select_ring(std::uint32_t destination) {
    auto &read = resident.disc_read;
    auto &stream = resident.disc_stream;
    static_cast<void>(field::select_disc_stream_ring(stream, destination));
    if (read.ring.address != stream.ring_buffer || read.ring.bytes.empty())
        throw field::FieldFormatError("The selected disc ring's header is not Program-owned");
    const auto count = s32(bytes(read.ring.bytes, 0, 4, "Disc ring"));
    if (count == 0)
        return 0;
    read.destination = stream.ring_buffer + (u32(count) << 3U) + 0x24U;
    read.ring_slots = stream.ring_buffer + 4U;
    stream.active_block_count = count;
    read.h_fe26 = 0;
    read.h_fe28 = 0;
    stream.expected_sequence = 0;
    static_cast<void>(field::reset_disc_stream_ring(stream, read.ring.bytes));
    return count;
}

// Resident 80029690.
std::int32_t Program::read_setup(std::uint32_t file, std::uint32_t destination,
                                 std::uint32_t offset, std::uint32_t mode) {
    auto &read = resident.disc_read;
    auto &cd = resident.cd;
    read.file = file;
    read.w_59ef8 = {};
    resident.disc_error = 1;
    read.destination = destination;
    read.offset = offset & 0xffffU;
    read.w_fe10 = 0;
    read.w_fe0c = 0;
    read.w_fe34 = 0;
    read.w_5a4dc = 0;
    set_position(read.sector, read.location);
    std::uint32_t sync = 0x8002b084;
    if ((mode & 0x100U) != 0) {
        if (select_ring(destination) == 0)
            return -4;
        if (resident.disc_stream.host_file_table != 0)
            host_file(0x800297d8);
        resident.disc_pending = 1;
        cd_dma_callback(0x8002ba58);
        sync = 0x8002b2f0;
    } else if ((mode & 0x200U) != 0) {
        if (select_ring(destination) == 0)
            return -4;
        read.h_59f60 = 0;
        read.b_59f18 = {static_cast<std::uint8_t>(mode | 0xa0U), 0, 0, 0};
        if (resident.disc_stream.host_file_table != 0)
            host_file(0x80029918);
        return 0;
    } else {
        if (resident.disc_stream.host_file_table != 0)
            host_file(0x800299a0);
        resident.disc_pending = 1;
        cd_dma_callback(0);
    }
    cd.ready_callback = 0x8002a68c; // 80040fb4
    cd.sync_callback = sync;        // 80040fcc
    ++read.requests;
    static_cast<void>(cd_control(2, &read.location));
    return 0;
}

// 800413ec: DMA channel 3 callback through 8004b7a0, which calls the service
// at *8005892c + 4; only 8004c21c is recovered.
void Program::cd_dma_callback(std::uint32_t function) {
    auto &cd = resident.cd;
    if (!cd.dma_set_callback || *cd.dma_set_callback != 0x8004c21c)
        throw MissingDependency({"dma_service", 0x8004b7b8, {}, {}}, "symbol:dma-service-table",
                                false, "The DMA service at *8005892c + 4 is not 8004c21c");
    // 8004c21c(3, function): store the callback, then set or clear the
    // channel's interrupt enable (bit 19) with the master enable (bit 23).
    if (function == cd.dma_callback)
        return;
    cd.dma_callback = function;
    const auto address = cd.dma_interrupt_register;
    const auto offset = address - 0x1f801000U;
    if (offset > resident.io.size() - 4 || (address & 3U) != 0)
        throw field::FieldFormatError("DMA interrupt register outside the observed I/O page");
    const auto control = (bytes(resident.io, offset, 4, "I/O page") & 0xffffffU) | 0x800000U;
    const auto value = function != 0 ? control | 0x80000U : control & ~0x80000U;
    resident.hardware_writes.push_back({address, value, 4});
}

// 8004111c CdControl(command, parameter, 0): up to four attempts.
std::int32_t Program::cd_control(std::uint8_t command,
                                 const std::array<std::uint8_t, 4> *parameter) {
    auto &cd = resident.cd;
    const auto ready = cd.ready_callback;
    for (auto attempt = 3; attempt != -1; --attempt) {
        cd.ready_callback = 0;
        if (command != 1 && (cd.status & 0x10U) != 0)
            static_cast<void>(cd_command(1, nullptr, false));
        if (parameter != nullptr && command_entry(cd.setloc_first, command) != 0 &&
            cd_command(2, parameter, false) != 0)
            continue;
        cd.ready_callback = ready;
        if (cd_command(command, parameter, true) == 0)
            return 1;
    }
    cd.ready_callback = ready;
    return 0;
}

namespace {
// The checks of one pass of the sync loops in 80041b3c and 80042088 before
// they read the status byte: timeout, then controller polling (8004b894).
void cd_wait_checks(ResidentState &resident, std::uint32_t timeout) {
    if (s32(resident.cd_sync_deadline) < s32(resident.vsync_counter) ||
        0x3c0000 < s32(resident.cd_sync_polls++))
        throw MissingDependency({"cd_sync_timeout", timeout, {}, {}}, "symbol:libcd-timeout-reset",
                                false, "The CD sync timeout and reset path is not reconstructed");
    if (resident.cd.interrupt_poll != 0)
        throw MissingDependency({"cd_interrupt_poll", 0x800415b4, {}, {}},
                                "symbol:cd-interrupt-800415b4", false,
                                "Polling the CD controller (800415b4) is not reconstructed");
}
[[noreturn]] void cd_interrupt_wait(std::uint32_t address) {
    throw MissingDependency({"cd_interrupt_wait", address, {}, {}}, "interrupt:cd-command", false,
                            "The CD command status arrives by interrupt");
}
} // namespace

// 80041b3c(0, 0): wait for the last command's status (2 complete, 5 error).
std::int32_t Program::cd_sync() {
    auto &cd = resident.cd;
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0; // 8004b54c(-1)
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018eb0;
    cd_wait_checks(resident, 0x80041bf8);
    const auto status = cd.sync_status;
    if (status != 2 && status != 5)
        cd_interrupt_wait(0x80041d88);
    cd.sync_status = 2;
    return status;
}

// 80042088: send a command and its parameters to the controller.
std::int32_t Program::cd_command(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter,
                                 bool nowait) {
    auto &cd = resident.cd;
    if (cd.debug >= 2)
        printf_call(0x800420e4);
    if (command_entry(cd.parameter_counts, command) != 0 && parameter == nullptr) {
        if (cd.debug > 0)
            printf_call(0x8004213c);
        return -2;
    }
    static_cast<void>(cd_sync());
    const auto argument = [&](std::uint32_t i) {
        if (parameter == nullptr || i >= parameter->size())
            throw field::FieldFormatError("CD parameter read outside its owned bytes");
        return (*parameter)[i];
    };
    if (command == 2)
        for (std::uint32_t i = 0; i < 4; ++i)
            cd.position[i] = argument(i);
    if (command == 0xe)
        cd.mode = argument(0);
    cd.sync_status = 0;
    if (command_entry(cd.clear_ready, command) != 0)
        cd.ready_status = 0;
    const auto write = [&](std::uint32_t address, std::uint8_t value) {
        if (address - 0x1f801000U >= resident.io.size())
            throw field::FieldFormatError("CD register address outside the I/O page");
        resident.hardware_writes.push_back({address, value, 1});
    };
    write(cd.registers[0], 0);
    for (std::uint32_t i = 0; s32(i) < s32(command_entry(cd.parameter_counts, command)); ++i)
        write(cd.registers[2], argument(i));
    cd.command = command;
    write(cd.registers[1], command);
    if (nowait)
        return 0;
    // 80042250: the status was just cleared, so completion needs an interrupt.
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0;
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018edc;
    cd_wait_checks(resident, 0x800422f0);
    cd_interrupt_wait(0x80042428);
}

} // namespace xem::reconstruction
