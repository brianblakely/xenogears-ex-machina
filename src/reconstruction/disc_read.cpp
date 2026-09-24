// Resident file reads: 800295d8 and its call tree (80028738, 800288ec,
// 800289d0, 80028a60, 80029690, 80028998, 80028470) with the CD library calls
// read setup reaches (80041430, 800413ec, 8004b7a0, 8004c21c, 80040fb4,
// 80040fcc, 8004111c, 80042088, 80041b3c, 8004b894, 8004b54c), and the
// interrupt side that completes reads: the CD handler 80042ca8, 800415b4,
// the read callbacks 8002a68c, 8002ac24, 8002b084, 8002b2f0, the ring DMA
// callback 8002ba58, 8002a394 and the sector transfer 80042aa8. Controller and
// DMA register stores become HardwareWrite records; registers only software
// changes come from the observed I/O page, the others from platform inputs;
// sector bytes come from the disc drive service. The host-file branch, debug
// printing and timeouts stop with MissingDependency.
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>
#include <string>

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

[[noreturn]] void printf_call(std::uint32_t address) {
    throw MissingDependency({"cd_debug_print", address, {}, {}}, "symbol:printf-80019964", false,
                            "CD library debug printing (80019964) is not reconstructed");
}
std::string hex(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(8, '0');
    for (int i = 7; i >= 0; --i, value >>= 4)
        text[static_cast<std::size_t>(i)] = digits[value & 15U];
    return text;
}
// 80041430 CdIntToPos: minute, second and sector (BCD) after 150 lead-in sectors.
std::array<std::uint8_t, 4> position(std::uint32_t sector, std::uint8_t unused) {
    const auto frames = s32(sector + 150U);
    const auto seconds = frames / 75;
    const auto bcd = [](std::int32_t value) {
        return static_cast<std::uint8_t>(value / 10 * 16 + value % 10);
    };
    return {bcd(seconds / 60), bcd(seconds % 60), bcd(frames % 75), unused};
}
// 80041534 CdPosToInt of a sector header's minute, second and sector.
std::uint32_t sector_of(std::uint32_t header) {
    const auto decimal = [&](std::uint32_t shift) {
        const auto value = (header >> shift) & 0xffU;
        return (value >> 4U) * 10U + (value & 15U);
    };
    return (decimal(0) * 60U + decimal(8)) * 75U + decimal(16) - 150U;
}
} // namespace

// Resident 80028a60: with zero, wait while the disc is busy, then query once
// more. Only interrupts can end a busy disc: each poll that finds it busy
// takes the next interrupt arrival from the platform input.
void Program::disc_wait(std::uint32_t once) {
    if (once == 0)
        while (s32(disc_busy()) > 0)
            if (!deliver_interrupt())
                throw MissingDependency({"disc_idle_wait", 0x80028a6c, {}, {}},
                                        "interrupt:disc-read-completion", false,
                                        "Waiting for an earlier disc read needs the interrupt "
                                        "arrivals that complete it");
    static_cast<void>(disc_busy());
}

bool Program::deliver_interrupt() {
    auto &inputs = resident.platform;
    if (inputs.empty() || inputs.front().kind != PlatformInput::Kind::interrupt)
        return false;
    inputs.pop_front();
    interrupt_dispatch();
    return true;
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
    read.location = position(read.sector, read.location[3]);
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
// The timeout checks of one pass of the sync loops in 80041b3c and 80042088.
void cd_timeout_checks(ResidentState &resident, std::uint32_t timeout) {
    if (s32(resident.cd_sync_deadline) < s32(resident.vsync_counter) ||
        0x3c0000 < s32(resident.cd_sync_polls++))
        throw MissingDependency({"cd_sync_timeout", timeout, {}, {}}, "symbol:libcd-timeout-reset",
                                false, "The CD sync timeout and reset path is not reconstructed");
}
// One pass of the 80042088 wait before it reads the status byte.
void cd_wait_checks(ResidentState &resident, std::uint32_t timeout) {
    cd_timeout_checks(resident, timeout);
    if (resident.cd.interrupt_poll != 0)
        throw MissingDependency({"cd_interrupt_poll", 0x80042380, {}, {}},
                                "symbol:cd-command-wait-poll", false,
                                "The command writer's controller polling is not reconstructed");
}
[[noreturn]] void cd_interrupt_wait(std::uint32_t address) {
    throw MissingDependency({"cd_interrupt_wait", address, {}, {}}, "interrupt:cd-command", false,
                            "The CD command status arrives by interrupt");
}
} // namespace

// 80041b3c(0, 0): wait for the last command's status (2 complete, 5 error).
// Inside an interrupt handler the wait polls the controller itself.
std::int32_t Program::cd_sync() {
    auto &cd = resident.cd;
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0; // 8004b54c(-1)
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018eb0;
    for (;;) {
        cd_timeout_checks(resident, 0x80041bf8);
        if (cd.interrupt_poll != 0)
            cd_poll();
        const auto status = cd.sync_status;
        if (status == 2 || status == 5) {
            cd.sync_status = 2;
            return status;
        }
        if (cd.interrupt_poll == 0)
            cd_interrupt_wait(0x80041d88);
    }
}

// 80041c80..80041d24: serve the controller's pending interrupts in place,
// then restore the register index.
void Program::cd_poll() {
    auto &cd = resident.cd;
    const auto index = platform_read(resident.platform, 0x80041c8c, 1) & 3U;
    for (auto result = cd_getintr(); result != 0; result = cd_getintr()) {
        if ((result & 4U) != 0 && cd.sync_callback != 0)
            cd_callback(cd.sync_callback, cd.ready_status, cd.ready_result);
        if ((result & 2U) != 0 && cd.ready_callback != 0)
            cd_callback(cd.ready_callback, cd.sync_status, cd.sync_result);
    }
    io_write(cd.registers[0], index, 1);
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
    std::vector<std::uint8_t> sent;
    for (std::uint32_t i = 0; s32(i) < s32(command_entry(cd.parameter_counts, command)); ++i) {
        sent.push_back(argument(i));
        write(cd.registers[2], sent.back());
    }
    cd.command = command;
    write(cd.registers[1], command);
    resident.drive.command(command, sent);
    if (nowait)
        return 0;
    // 80042250: the status was just cleared, so completion needs an interrupt.
    resident.cd_sync_deadline = resident.vsync_counter + 0x3c0;
    resident.cd_sync_polls = 0;
    resident.cd_sync_label = 0x80018edc;
    cd_wait_checks(resident, 0x800422f0);
    cd_interrupt_wait(0x80042428);
}

// 80042ca8: serve the controller until it reports nothing, then restore
// the register index.
void Program::cd_interrupt() {
    auto &cd = resident.cd;
    const auto index = platform_read(resident.platform, 0x80042cd0, 1) & 3U;
    for (auto result = cd_getintr(); result != 0; result = cd_getintr()) {
        if ((result & 4U) != 0 && cd.sync_callback != 0)
            cd_callback(cd.sync_callback, cd.ready_status, cd.ready_result);
        if ((result & 2U) != 0 && cd.ready_callback != 0)
            cd_callback(cd.ready_callback, cd.sync_status, cd.sync_result);
    }
    io_write(cd.registers[0], index, 1);
}

// 800415b4: read and acknowledge one controller interrupt. Returns 0 (none),
// 1 (acknowledged, completion follows), 2 (command status), 4 (data status)
// or 6 (error: both).
std::uint32_t Program::cd_getintr() {
    auto &cd = resident.cd;
    const auto read = [&](std::uint32_t site) { return platform_read(resident.platform, site, 1); };
    io_write(cd.registers[0], 1, 1);
    auto type = read(0x800415e0) & 7U;
    if (type == 0)
        return 0;
    // The type must read the same twice in a row.
    while ((read(0x80041618) & 7U) != type)
        type = read(0x80041608) & 7U;
    std::array<std::uint8_t, 8> response{};
    std::uint32_t count = 0;
    while (count < response.size() && (read(0x8004163c) & 0x20U) != 0)
        response[count++] = static_cast<std::uint8_t>(read(0x8004165c));
    io_write(cd.registers[0], 1, 1);
    io_write(cd.flag_register, 7, 1);
    io_write(cd.registers[2], 7, 1);
    std::uint32_t errors = 0;
    if (type != 3 || command_entry(cd.ack_updates, cd.command) != 0) {
        if ((cd.status & 0x10U) == 0 && (response[0] & 0x10U) != 0)
            ++cd.shell_opened;
        cd.status = response[0];
        cd.status2 = response[1];
        errors = response[0] & 0x1dU;
    }
    if (type == 5 && cd.debug > 0)
        printf_call(0x8004179c);
    switch (type) {
    case 1: // Data ready: the drive's next sector is in its buffer.
        resident.drive.data_ready();
        if (errors != 0 && count == 1)
            errors = 0;
        cd.ready_status = errors != 0 ? 5 : 1;
        cd.ready_result = response;
        io_write(cd.registers[0], 0, 1);
        io_write(cd.flag_register, 0, 1);
        return 4;
    case 2: // Complete.
        cd.sync_status = errors != 0 ? 5 : 2;
        cd.sync_result = response;
        return 2;
    case 3: // Acknowledged.
        cd.sync_result = response;
        if (errors != 0) {
            cd.sync_status = 5;
            return 2;
        }
        if (command_entry(cd.completes, cd.command) != 0) {
            cd.sync_status = 3;
            return 1;
        }
        cd.sync_status = 2;
        return 2;
    case 4: // Data end.
        cd.end_status = 4;
        cd.ready_status = 4;
        cd.end_result = response;
        cd.ready_result = response;
        return 4;
    case 5: // Error.
        cd.ready_status = 5;
        cd.sync_status = 5;
        cd.sync_result = response;
        cd.ready_result = response;
        return 6;
    default:
        printf_call(0x80041b18);
    }
}

void Program::cd_callback(std::uint32_t address, std::uint8_t status,
                          const std::array<std::uint8_t, 8> &result) {
    switch (address) {
    case 0x8002a68c:
        disc_command_done(status, result);
        break;
    case 0x8002b084:
        disc_data(status);
        break;
    case 0x8002b2f0:
        disc_ring_data(status);
        break;
    case 0x8002ac24:
        disc_list_data(status);
        break;
    default:
        throw MissingDependency({"cd_callback", 0x80042d18, {}, {}}, "symbol:" + hex(address),
                                false, "A CD callback is not reconstructed");
    }
}

// 8002a68c: advance the read state machine (8004fe1c) when a command
// completes. Status 2 is success; otherwise the machine records a retry
// reason (8004fe20) and asks for the drive status (state 10).
void Program::disc_command_done(std::uint8_t status, const std::array<std::uint8_t, 8> &result) {
    auto &read = resident.disc_read;
    auto &cd = resident.cd;
    const auto state = resident.disc_pending;
    const bool ok = status == 2;
    const auto recover = [&](std::uint32_t reason) {
        read.retry_reason = reason;
        resident.disc_pending = 10;
        static_cast<void>(cd_control(1, nullptr));
    };
    const auto suspend = [&] { // Replace the data callback until the read resumes.
        read.saved_callback = cd.sync_callback;
        cd.sync_callback = 0;
    };
    const auto resume = [&] { // Setloc to the read position again.
        resident.disc_pending = 1;
        ++read.requests;
        ++read.w_5a494[0];
        cd.sync_callback = read.saved_callback;
        static_cast<void>(cd_control(2, &read.location));
    };
    switch (state) {
    case 0:
        break;
    case 1: // Setloc done: read.
        if (ok) {
            ++read.w_5a48c[0];
            ++resident.disc_pending;
            static_cast<void>(cd_control(6, nullptr));
        } else {
            ++read.w_5a48c[1];
            suspend();
            recover(3);
        }
        break;
    case 2:
        if (ok) {
            ++read.w_5a48c[0];
            cd.ready_callback = 0;
            resident.disc_pending = 0;
        } else {
            ++read.w_5a48c[1];
            suspend();
            recover(3);
        }
        break;
    case 3: // Setloc done: seek.
        if (ok) {
            ++resident.disc_pending;
            static_cast<void>(cd_control(0x15, nullptr));
        } else {
            recover(1);
        }
        break;
    case 4:
    case 5:
        if (ok) {
            cd.ready_callback = 0;
            resident.disc_pending = 0;
        } else {
            recover(state == 4 ? 1 : 2);
        }
        break;
    case 6:
    case 8:
        if (ok) {
            resume();
        } else {
            ++read.w_5a494[1];
            recover(state == 6 ? 3 : 5);
        }
        break;
    case 7:
        if (ok) {
            resident.disc_pending = 6;
            ++read.w_5a4a4[1];
            static_cast<void>(cd_control(9, nullptr));
        } else {
            ++read.w_5a4b4;
            recover(4);
        }
        break;
    case 9:
        if (ok) {
            resident.disc_pending = 5;
            static_cast<void>(cd_control(9, nullptr));
        } else {
            ++read.w_5a4b4;
            recover(6);
        }
        break;
    case 10: // Drive status: retry unless the shell is open.
        if (ok && (result[0] & 0x10U) == 0) {
            resident.disc_pending = 11;
            static_cast<void>(cd_control(0x13, nullptr));
        } else {
            resident.disc_pending = 10;
            static_cast<void>(cd_control(1, nullptr));
        }
        break;
    case 11: // Retry by reason.
        if (!ok) {
            resident.disc_pending = 10;
            static_cast<void>(cd_control(1, nullptr));
            break;
        }
        switch (read.retry_reason) {
        case 1:
            resident.disc_pending = 3;
            static_cast<void>(cd_control(2, &read.location));
            break;
        case 2:
        case 3:
            resident.disc_pending = read.retry_reason == 2 ? 5 : 6;
            static_cast<void>(cd_control(9, nullptr));
            break;
        case 4:
            resident.disc_pending = 7;
            static_cast<void>(cd_control(8, nullptr));
            break;
        case 5:
        case 6:
            resident.disc_pending = read.retry_reason == 5 ? 12 : 9;
            static_cast<void>(cd_control(0xe, &read.b_59f18));
            break;
        default:
            break;
        }
        break;
    case 12: // Mode set: filter.
        if (ok) {
            resident.disc_pending = 8;
            read.b_59f14 = {1, static_cast<std::uint8_t>(read.offset)};
            const std::array<std::uint8_t, 4> filter{read.b_59f14[0], read.b_59f14[1], 0, 0};
            static_cast<void>(cd_control(0xd, &filter));
        } else {
            recover(5);
        }
        break;
    default:
        break;
    }
}

// 8002b204 (and its copies in 8002ac24 and 8002b2f0): a failed data interrupt.
// After three in a row the read waits, then stops and restarts the drive.
void Program::disc_data_failed(bool counted) {
    auto &read = resident.disc_read;
    auto &cd = resident.cd;
    if (counted)
        ++read.w_5a4dc;
    read.saved_callback = cd.sync_callback;
    cd.sync_callback = 0;
    read.location = position(read.sector, read.location[3]);
    if (s32(read.w_5a4dc) < 3) {
        read.retry_reason = 3;
    } else {
        // A busy loop of 10000 x 2000 iterations precedes this; it has no state.
        read.w_5a4dc = 0;
        read.retry_reason = 4;
        ++read.w_5a4a4[0];
    }
    resident.disc_pending = 10;
    cd.ready_callback = 0x8002a68c;
    static_cast<void>(cd_control(1, nullptr));
}

// 8002a394: after the last sector, Setloc to the first sector of `file` (a
// file of the selected directory), or pause when it is not positive.
void Program::disc_continue(std::uint32_t file) {
    auto &read = resident.disc_read;
    if (s32(file) > 0) {
        // 800289d0: the file's first sector.
        const auto first = bytes(read.files, (file + read.directory - 1U) * 7U, 3, "File table");
        read.location = position(first, read.location[3]);
        resident.disc_pending = 3;
        resident.cd.ready_callback = 0x8002a68c;
        static_cast<void>(cd_control(2, &read.location));
    } else {
        resident.disc_pending = 5;
        resident.cd.ready_callback = 0x8002a68c;
        static_cast<void>(cd_control(9, nullptr));
    }
}

// 800413ec(0), inlined where the callbacks end a transfer.
namespace {
constexpr std::uint32_t sector_header = 0x80059ef8;
constexpr std::uint32_t sector_tail = 0x800596f8;
} // namespace

// Transfer the header (three words) and `size` bytes of the buffered sector
// to `destination`, the rest of a short final sector to the tail buffer.
// Shared by 8002b084 and 8002ac24.
namespace {
std::int32_t words_of(std::int32_t size) {
    const auto rounded = size + 3;
    return (rounded >= 0 ? rounded : size + 6) >> 2;
}
} // namespace

// 8002b084: data callback of plain reads.
void Program::disc_data(std::uint8_t status) {
    auto &read = resident.disc_read;
    if (status != 1) {
        disc_data_failed(true);
        return;
    }
    if (s32(read.w_fe34) <= 0) {
        const auto size = s32(read.size);
        if (size >= 0x800) {
            cd_get_sector(sector_header, 3);
            cd_get_sector(read.destination, 0x200);
        } else if (size > 0) {
            cd_get_sector(sector_header, 3);
            const auto words = words_of(size);
            cd_get_sector(read.destination, static_cast<std::uint32_t>(words));
            cd_get_sector(sector_tail, static_cast<std::uint32_t>(0x200 - words));
        }
        const auto sector = sector_of(read.w_59ef8[0]);
        if (sector != read.sector) {
            ++read.skipped[0];
            disc_data_failed(true);
            return;
        }
        read.sector = sector + 1U;
        read.destination += 0x800;
        read.size -= 0x800;
        if (s32(read.size) > 0)
            return;
    }
    resident.cd.sync_callback = 0;
    read.size = 0;
    disc_continue(read.offset);
    resident.disc_error = 0;
}

// 8002b2f0: data callback of ring reads. Each sector goes to the next free
// ring slot, which records its sequence number; 8002ba58 completes it.
void Program::disc_ring_data(std::uint8_t status) {
    auto &read = resident.disc_read;
    auto &stream = resident.disc_stream;
    if (status != 1) {
        disc_data_failed(true);
        return;
    }
    if (s32(read.w_fe34) > 0) {
        resident.cd.sync_callback = 0;
        cd_dma_callback(0);
        read.size = 0;
        disc_continue(read.offset);
        resident.disc_error = 0;
        return;
    }
    if (s32(read.size) > 0) {
        const auto blocks = stream.active_block_count;
        if (blocks <= 0)
            throw MissingDependency({"disc_ring_data", 0x8002b3e4, {}, {}},
                                    "symbol:ring-without-blocks", false,
                                    "A ring read without blocks tests an unset slot");
        const auto slot_word = [&](std::uint32_t slot, std::uint32_t offset) -> std::uint32_t {
            return bytes(read.ring.bytes, read.ring_slots - read.ring.address + slot * 8U + offset,
                         2, "Disc ring");
        };
        std::uint32_t slot = 0;
        for (std::int32_t tried = 0;;) {
            slot = read.w_fe10;
            read.w_fe10 = slot + 1U;
            if (s32(read.w_fe10) >= blocks)
                read.w_fe10 = 0;
            if (slot_word(slot, 0) == 0 || ++tried >= blocks)
                break;
        }
        if (slot_word(slot, 0) != 0) {
            disc_data_failed(false);
            return;
        }
        cd_get_sector(sector_header, 3);
        const auto sector = sector_of(read.w_59ef8[0]);
        if (sector != read.sector) {
            ++read.skipped[2];
            cd_get_sector(sector_tail, 0x200);
            disc_data_failed(true);
            return;
        }
        const auto at = read.ring_slots - read.ring.address + slot * 8U;
        const auto put16 = [&](std::uint32_t offset, std::uint32_t value) {
            if (at + offset + 2 > read.ring.bytes.size())
                throw field::FieldFormatError("Disc ring slot outside its owned header");
            read.ring.bytes[at + offset] = static_cast<std::uint8_t>(value);
            read.ring.bytes[at + offset + 1] = static_cast<std::uint8_t>(value >> 8U);
        };
        put16(0, 1);
        put16(2, read.h_fe26);
        ++read.h_fe26;
        cd_get_sector((slot << 11U) + read.destination, 0x200);
        read.size -= 0x800;
        ++read.sector;
        if (s32(read.size) > 0)
            return;
    }
    resident.cd.sync_callback = 0;
    read.size = 0;
}

// 8002ba58: DMA callback of ring reads: the oldest filled slot (state 1 with
// the expected sequence) becomes ready (3); after the last one the read
// continues or ends.
void Program::disc_ring_transferred() {
    auto &read = resident.disc_read;
    const auto blocks = resident.disc_stream.active_block_count;
    if (blocks < 0)
        throw MissingDependency({"disc_ring_transferred", 0x8002bad8, {}, {}},
                                "symbol:ring-without-blocks", false,
                                "A ring without blocks marks an unset slot");
    const auto field16 = [&](std::uint32_t slot, std::uint32_t offset) -> std::uint32_t {
        return bytes(read.ring.bytes, read.ring_slots - read.ring.address + slot * 8U + offset, 2,
                     "Disc ring");
    };
    std::int32_t slot = 0;
    while (slot < blocks && !(field16(static_cast<std::uint32_t>(slot), 0) == 1 &&
                              field16(static_cast<std::uint32_t>(slot), 2) == read.h_fe28))
        slot = static_cast<std::int16_t>(slot + 1);
    if (slot == blocks)
        return;
    const auto at = read.ring_slots - read.ring.address + static_cast<std::uint32_t>(slot) * 8U;
    read.ring.bytes.at(at) = 3;
    read.ring.bytes.at(at + 1) = 0;
    ++read.h_fe28;
    if (s32(read.size) > 0 || s32(resident.disc_error) >= 2)
        return;
    read.size = 0;
    cd_dma_callback(0);
    disc_continue(read.offset);
    resident.disc_error = 0;
}

// 8002ac24: data callback of list reads: files from the list at 8004fe0c
// (halfword file, word destination per eight bytes; 8004fe10 is the current
// entry) are read in one pass, skipping sectors between them.
void Program::disc_list_data(std::uint8_t status) {
    auto &read = resident.disc_read;
    auto &cd = resident.cd;
    if (status != 1) {
        disc_data_failed(true);
        return;
    }
    const auto finish = [&] {
        disc_continue(read.offset);
        read.w_fe00 = 0;
        resident.disc_error = 0;
    };
    if (s32(read.w_fe34) > 0) {
        cd.sync_callback = 0;
        cd_dma_callback(0);
        read.size = 0;
        finish();
        return;
    }
    const auto size = s32(read.size);
    if (read.w_fe3c == 0) {
        if (size >= 0x800) {
            cd_get_sector(sector_header, 3);
            cd_get_sector(read.destination, 0x200);
        } else if (size > 0) {
            cd_get_sector(sector_header, 3);
            const auto words = words_of(size);
            cd_get_sector(read.destination, static_cast<std::uint32_t>(words));
            cd_get_sector(sector_tail, static_cast<std::uint32_t>(0x200 - words));
        }
    }
    const auto sector = sector_of(read.w_59ef8[0]);
    if (sector != read.sector && read.w_fe3c == 0) {
        ++read.skipped[1];
        disc_data_failed(true);
        return;
    }
    read.destination += 0x800;
    read.size -= 0x800;
    ++read.sector;
    if (s32(read.size) > 0)
        return;
    // Next list entry.
    const auto index = read.w_fe10 + 1U;
    const auto entry = read.w_fe0c + index * 8U - read.list.address;
    const auto file = bytes(read.list.bytes, entry, 2, "Disc read list");
    read.w_fe10 = index;
    read.destination = bytes(read.list.bytes, entry + 4U, 4, "Disc read list");
    if (file == 0 || read.destination == 0) {
        read.size = 0;
        cd.sync_callback = 0;
        finish();
        return;
    }
    // 80028a18 and 80028808: the file's first sector and word-rounded size
    // in the directory of the current read.
    if (resident.disc_stream.host_file_table != 0)
        host_file(0x8002882c);
    const auto record = (file + read.read_directory - 1U) * 7U;
    const auto first = bytes(read.files, record, 3, "File table");
    read.size =
        static_cast<std::uint32_t>(words_of(s32(bytes(read.files, record + 3, 4, "File table"))))
        << 2U;
    const auto current = read.sector;
    if (current < first && current + read.w_fde0 >= first) {
        // Close ahead: read on, discarding the sectors before the file.
        read.w_fe3c = 1;
        read.size = (current - first) << 11U;
        read.w_fe10 = index - 1U;
        return;
    }
    read.w_fe3c = 0;
    if (first != read.sector) {
        read.sector = first;
        read.saved_callback = cd.sync_callback;
        cd.sync_callback = 0;
        read.location = position(first, read.location[3]);
        resident.disc_pending = 6;
        cd.ready_callback = 0x8002a68c;
        static_cast<void>(cd_control(9, nullptr));
    }
    --read.w_fe00;
}

// 800413ac(buffer, words) = 80042aa8 CdGetSector: request the data buffer,
// program DMA channel 3 and wait for it. The bytes come from the drive.
void Program::cd_get_sector(std::uint32_t buffer, std::uint32_t words) {
    auto &cd = resident.cd;
    const auto &registers = cd.transfer_registers;
    io_write(cd.registers[0], 0, 1);
    io_write(cd.flag_register, 0x80, 1);
    io_write(registers[0], 0x20943, 4);
    io_write(registers[1], 0x1323, 4);
    io_write(registers[2], platform_read(resident.platform, 0x80042af4, 4) | 0x8000U, 4);
    io_write(registers[3], buffer, 4);
    io_write(registers[4], words | 0x10000U, 4);
    // Wait for the data FIFO, start the transfer, wait for its end.
    while ((platform_read(resident.platform, 0x80042b34, 1) & 0x40U) == 0) {
    }
    io_write(resident.cd_dma_register, 0x11000000, 4);
    dma_store(buffer, resident.drive.transfer(words * 4U));
    if ((platform_read(resident.platform, 0x80042b64, 4) & 0x1000000U) != 0)
        while ((platform_read(resident.platform, 0x80042b7c, 4) & 0x1000000U) != 0) {
        }
    io_write(registers[1], 0x1325, 4);
}

void Program::dma_store(std::uint32_t address, std::span<const std::uint8_t> data) {
    const auto end = std::uint64_t{address} + data.size();
    const bool global = std::ranges::any_of(original_globals(), [&](const OriginalGlobal &item) {
        return item.address < end && address < std::uint64_t{item.address} + item.width;
    });
    if (global) {
        write_original(*this, address, data);
        return;
    }
    const auto inside = [&](resident::HeapBlock &block) {
        if (address < block.address || end > std::uint64_t{block.address} + block.bytes.size())
            return false;
        std::ranges::copy(data, block.bytes.begin() + (address - block.address));
        return true;
    };
    if (inside(resident.disc_read.ring))
        return;
    for (auto &block : resident.music_blocks)
        if (inside(block))
            return;
    for (auto &[at, bytes] : resident.heap.held)
        if (address < at + bytes.size() && at < end)
            throw field::FieldFormatError("Disc DMA writes into a free heap block");
    // Extend an adjacent or overlapping transfer block, else start one.
    for (auto &block : resident.disc_transfers) {
        const auto block_end = std::uint64_t{block.address} + block.bytes.size();
        if (address >= block.address && address <= block_end) {
            const auto offset = address - block.address;
            if (offset + data.size() > block.bytes.size())
                block.bytes.resize(offset + data.size());
            std::ranges::copy(data, block.bytes.begin() + offset);
            return;
        }
    }
    resident.disc_transfers.push_back({address, {data.begin(), data.end()}});
}

} // namespace xem::reconstruction
