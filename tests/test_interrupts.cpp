// Invented register values, sectors and tables exercise the interrupt
// dispatcher (8004b9b4), its VSync, DMA and CD paths, the disc drive service
// and interrupt arrivals during a disc wait. They describe no original
// content or observation.
#include "xem/reconstruction/program.hpp"

#include <iostream>

namespace game = xem::reconstruction;
namespace {
using Input = game::PlatformInput;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
Input read(std::uint32_t site, std::uint32_t value) { return {Input::Kind::read, site, value}; }
template <typename Error, typename Call> bool fails(Call call) {
    try {
        call();
    } catch (const Error &) {
        return true;
    }
    return false;
}
template <typename Call> std::uint32_t stop_address(Call call) {
    try {
        call();
    } catch (const game::MissingDependency &error) {
        return error.point.machine_address;
    }
    return 0;
}
bool wrote(const game::ResidentState &resident, std::uint32_t address, std::uint32_t value,
           std::uint32_t width) {
    for (const auto &write : resident.hardware_writes)
        if (write == game::HardwareWrite{address, value, width})
            return true;
    return false;
}

// A dispatcher serving VSync (bit 0), CD (bit 2) and DMA (bit 3).
game::Program sample() {
    game::Program program;
    auto &resident = program.resident;
    auto &irq = resident.interrupts;
    irq.initialized = 1;
    irq.mask = 0x0d;
    irq.registers = {0x1f801070, 0x1f801074, 0x1f8010f0};
    irq.handlers[0] = 0x8004bf78;
    irq.handlers[2] = 0x80042ca8;
    irq.handlers[3] = 0x8004c098;
    irq.vsync_callbacks[4] = 0x8003634c;
    irq.hook_stack = 0x800588bc;
    resident.io[0x74] = 0x0d; // I_MASK
    resident.debug_word = 0xffffffff;
    auto &cd = resident.cd;
    cd.registers = {0x1f801800, 0x1f801801, 0x1f801802};
    cd.flag_register = 0x1f801803;
    cd.dma_interrupt_register = 0x1f8010f4;
    cd.dma_services = 0x8005890c;
    cd.dma_set_callback = 0x8004c21c;
    cd.parameter_counts[2] = 3;
    cd.parameter_counts[0xe] = 1;
    cd.transfer_registers = {0x1f801020, 0x1f801018, 0x1f8010f0, 0x1f8010b0, 0x1f8010b4};
    resident.cd_dma_register = 0x1f8010b8;
    return program;
}
// The dispatcher's reads around a single pass: bits, then none, then none.
std::deque<Input> pass(std::uint32_t bits, std::deque<Input> handler) {
    std::deque<Input> inputs{read(0x8004ba34, bits)};
    inputs.insert(inputs.end(), handler.begin(), handler.end());
    inputs.push_back(read(0x8004bac8, 0));
    inputs.push_back(read(0x8004baf0, 0));
    return inputs;
}

void platform_reads() {
    std::deque<Input> inputs{read(0x100, 0xff), read(0x104, 0xffffff80), read(0x108, 0x100)};
    check(game::platform_read(inputs, 0x100, 1) == 0xff, "A byte read returns its value");
    check(game::platform_read(inputs, 0x104, 1, true) == 0xffffff80,
          "A sign-extending byte read keeps its extension");
    check(fails<game::PlatformInputError>([&] { game::platform_read(inputs, 0x108, 1); }),
          "A value wider than its load is malformed");
    check(fails<game::PlatformInputError>([&] { game::platform_read(inputs, 0x10c, 4); }),
          "A read from another site is malformed");
    inputs.clear();
    check(fails<game::PlatformInputError>([&] { game::platform_read(inputs, 0x100, 4); }),
          "A missing read is malformed");
    inputs.push_back({Input::Kind::interrupt});
    check(fails<game::PlatformInputError>([&] { game::platform_read(inputs, 0x100, 4); }),
          "An arrival is not a read");
}

void vsync() {
    auto program = sample();
    auto &resident = program.resident;
    resident.vsync_counter = 9;
    resident.pad.vsyncs = 3;
    // A digital controller on port 0 holding cross (bit 6 of byte 3, active low).
    resident.pad.buffers[0] = {0, 0x41, 0xff, 0xbf};
    resident.pad.buffers[1] = {0xff};
    resident.pad.remap_bits = {0x40, 0, 0, 0, 0, 0, 0, 0};
    resident.pad.remap_index = {0};
    resident.pad.clock = {59, 59, 59, 99};
    resident.pad.actuators[0] = {0, 0, 0, 0, 2, 0, 0, 0};
    resident.platform = pass(1, {});
    program.interrupt_dispatch();
    check(resident.platform.empty() && resident.cd.interrupt_poll == 0,
          "One VSync pass consumes its three reads and leaves interrupt context");
    check(wrote(resident, 0x1f801070, 0xfffe, 2), "The served bit is acknowledged in I_STAT");
    check(resident.vsync_counter == 10 && resident.pad.vsyncs == 4, "The VSync is counted");
    const auto &current = resident.input_queue.current;
    check(current[0] == 0x40 && current[2] == 0x40 && current[4] == 0x40 && current[1] == 0,
          "A new press is held, pressed and repeated");
    check(resident.pad.held[0] == 0x40 && resident.pad.repeat_delay[0] == 1,
          "The repeat delay restarts with the press");
    check(resident.pad.analog[0] == std::array<std::uint8_t, 4>{0, 0, 0, 0} &&
              resident.pad.type == 0,
          "The failed port 1 transfer leaves type 0");
    check(resident.input_queue.count == 1 && resident.input_queue.ring[0][0] == 0x40,
          "The entry is queued");
    check(resident.pad.clock == std::array<std::uint8_t, 4>{0, 0, 0, 100} &&
              resident.pad.clock_stopped == 1,
          "The play clock stops at 100 hours");
    check(resident.pad.actuators[0][4] == 1 && resident.pad.actuators[0][6] == 1,
          "An actuator count steps down");

    auto unknown = sample();
    unknown.resident.interrupts.vsync_callbacks[1] = 0x80012340;
    unknown.resident.platform = pass(1, {});
    check(stop_address([&] { unknown.interrupt_dispatch(); }) == 0x8004bfc0,
          "An unrecovered VSync callback stops at its call");
    auto handler = sample();
    handler.resident.interrupts.handlers[0] = 0x80012340;
    handler.resident.platform = pass(1, {});
    check(stop_address([&] { handler.interrupt_dispatch(); }) == 0x8004ba94,
          "An unrecovered handler stops at the dispatcher's call");
    auto uninitialized = sample();
    uninitialized.resident.interrupts.initialized = 0;
    check(stop_address([&] { uninitialized.interrupt_dispatch(); }) == 0x8004ba00,
          "An uninitialized dispatcher only prints");
}

void pending() {
    auto program = sample();
    auto &resident = program.resident;
    // A bit the dispatcher does not serve (bit 6) stays pending.
    resident.platform = {read(0x8004ba34, 0x40), read(0x8004baf0, 0x40)};
    resident.io[0x74] = 0x4d;
    program.interrupt_dispatch();
    check(resident.interrupts.unexpected == 1 && resident.hardware_writes.empty(),
          "An unserved pending bit is counted, not acknowledged");
    resident.interrupts.unexpected = 2049;
    resident.platform = {read(0x8004ba34, 0x40), read(0x8004baf0, 0x40)};
    check(stop_address([&] { program.interrupt_dispatch(); }) == 0x8004bb3c,
          "A long run of unserved bits reaches the diagnostic");
    auto missing = sample();
    missing.resident.platform = {read(0x8004ba34, 1)};
    check(fails<game::PlatformInputError>([&] { missing.interrupt_dispatch(); }),
          "A dispatcher without its loop read has malformed input");
}

void dma() {
    auto program = sample();
    auto &resident = program.resident;
    resident.cd.dma_callback = 0x8002ba40;
    resident.disc_error = 1;
    resident.disc_read.w_fe00 = 0;
    // DICR flags channel 3; the acknowledgement keeps only that flag.
    resident.platform = pass(8, {read(0x8004c0c0, 0x88880000), read(0x8004c118, 0x88880000),
                                 read(0x8004c15c, 0x80880000), read(0x8004c180, 0x00880000),
                                 read(0x8004c198, 0x00880000)});
    program.interrupt_dispatch();
    check(resident.platform.empty() && resident.disc_error == 0,
          "The channel 3 callback ends the read");
    check(wrote(resident, 0x1f8010f4, 0x08880000, 4), "Only the served flag is written back");
    auto unknown = sample();
    unknown.resident.interrupts.dma_callbacks[2] = 0x80012340;
    unknown.resident.platform =
        pass(8, {read(0x8004c0c0, 0x04000000), read(0x8004c118, 0x04000000)});
    check(stop_address([&] { unknown.interrupt_dispatch(); }) == 0x8004c138,
          "An unrecovered DMA callback stops at its call");
    auto forced = sample();
    forced.resident.platform = pass(8, {read(0x8004c0c0, 0), read(0x8004c180, 0x80000000)});
    check(stop_address([&] { forced.interrupt_dispatch(); }) == 0x8004c1b8,
          "A master flag without channels reaches the diagnostic");
}

// A CD acknowledgement and completion of Setloc, then ReadN; then one sector.
void cd() {
    auto program = sample();
    auto &resident = program.resident;
    auto &cd = resident.cd;
    cd.ready_callback = 0x8002a68c;
    cd.sync_callback = 0x8002b084;
    cd.command = 2;
    cd.ack_updates[2] = 1;
    cd.sync_status = 0;
    cd.mode = 0xa0;
    resident.drive.mode = 0xa0;
    resident.disc_pending = 1;
    resident.disc_error = 1;
    // Acknowledged Setloc: status 2, one response byte; then ReadN is sent
    // while its own sync wait polls the controller (nothing pending).
    resident.platform =
        pass(4, {read(0x80042cd0, 0x18), read(0x800415e0, 0xe3), read(0x80041618, 0xe3),
                 read(0x8004163c, 0x38), read(0x8004165c, 0x02), read(0x8004163c, 0x18),
                 read(0x80041c8c, 0x19), read(0x800415e0, 0xe0), read(0x800415e0, 0xe0)});
    program.interrupt_dispatch();
    check(resident.platform.empty(), "The CD pass consumes every read");
    check(resident.disc_pending == 2 && cd.command == 6 && resident.disc_read.w_5a48c[0] == 1,
          "A completed Setloc advances to ReadN");
    check(cd.status == 2 && cd.sync_result[0] == 2, "The response is recorded");
    check(wrote(resident, 0x1f801801, 6, 1) && wrote(resident, 0x1f801803, 7, 1),
          "ReadN is written and the interrupt acknowledged");
    check(resident.drive.reading && !resident.drive.next,
          "ReadN without a new Setloc keeps the drive position");

    // ReadN was acknowledged; sector 1000 (a whole-sector read of a
    // four-byte file) arrives.
    cd.sync_status = 2;
    auto &read_state = resident.disc_read;
    read_state.sector = 1000;
    read_state.size = 4;
    read_state.destination = 0x80100000;
    resident.drive.next = 1000;
    resident.drive.read_sector = [](std::uint32_t lba) {
        game::RawSector sector{};
        sector[12] = 0x00;
        sector[13] = static_cast<std::uint8_t>(0x15);
        sector[14] = static_cast<std::uint8_t>(0x25); // 00:15:25 = 1150 - 150
        sector[15] = 2;
        for (std::size_t i = 24; i < sector.size(); ++i)
            sector[i] = static_cast<std::uint8_t>(i + lba);
        return sector;
    };
    const std::deque<Input> transfer{read(0x80042af4, 0x3b3b), read(0x80042b34, 0x40),
                                     read(0x80042b64, 0)};
    std::deque<Input> handler{read(0x80042cd0, 0x18), read(0x800415e0, 0xe1),
                              read(0x80041618, 0xe1), read(0x8004163c, 0x38),
                              read(0x8004165c, 0x22), read(0x8004163c, 0x18)};
    for (int i = 0; i < 3; ++i)
        handler.insert(handler.end(), transfer.begin(), transfer.end());
    // Pause is sent from the callback; its sync wait polls the controller.
    handler.insert(handler.end(), {read(0x80041c8c, 0x18), read(0x800415e0, 0xe0)});
    handler.push_back(read(0x800415e0, 0xe0));
    resident.platform = pass(4, handler);
    resident.hardware_writes.clear();
    program.interrupt_dispatch();
    check(resident.platform.empty() && resident.drive.delivered == std::vector<std::uint32_t>{1000},
          "One data interrupt delivers one sector");
    check(read_state.sector == 1001 && read_state.size == 0 && resident.disc_error == 0 &&
              cd.sync_callback == 0,
          "The last sector ends the read");
    check(read_state.w_59ef8[0] == 0x02251500, "The header lands at 80059ef8");
    check(resident.disc_transfers.size() == 2 && resident.disc_transfers[0].address == 0x80100000 &&
              resident.disc_transfers[0].bytes == std::vector<std::uint8_t>{0, 1, 2, 3},
          "The file bytes and the tail go to RAM");
    check(resident.disc_transfers[1].address == 0x800596f8 &&
              resident.disc_transfers[1].bytes.size() == 0x7fc,
          "The rest of the sector fills the tail buffer");
    check(wrote(resident, 0x1f801801, 9, 1) && resident.disc_pending == 5,
          "A read without a following file pauses");
}

void drive() {
    game::DiscDrive drive;
    drive.data_ready();
    check(fails<game::PlatformInputError>([&] { drive.transfer(4); }),
          "An unidentified delivered sector cannot be read");
    const std::array<std::uint8_t, 3> position{0x00, 0x02, 0x10};
    drive.command(2, position);
    drive.command(6, {});
    check(drive.next == 10 && drive.reading, "ReadN starts at the Setloc position");
    drive.read_sector = [](std::uint32_t) { return game::RawSector{}; };
    drive.data_ready();
    check(drive.next == 11 && drive.delivered == std::vector<std::uint32_t>{10},
          "Each data interrupt advances one sector");
    check(drive.transfer(0x800).size() == 0x800, "A data-only read transfers 800h bytes");
    check(fails<game::PlatformInputError>([&] { drive.transfer(4); }),
          "Reading past the sector is malformed");
    drive.command(9, {});
    check(!drive.reading, "Pause ends delivery");
    check(fails<game::PlatformInputError>([&] { drive.command(2, {}); }),
          "Setloc without its position is malformed");
}

// A read waiting for the previous one: an arrival ends it (DMA callback).
void arrivals() {
    game::Program program = sample();
    auto &resident = program.resident;
    auto &files = resident.disc_read;
    files.file_table = 0x80010004;
    files.directory_table = 0x80018004;
    files.files.assign(0x8000, 0);
    files.directories.assign(0x7a, 0);
    files.directory = 10;
    files.files[12 * 7] = 0x39;
    files.files[12 * 7 + 3] = 4;
    resident.cd.sync_status = 2;
    resident.cd.dma_callback = 0x8002ba40;
    resident.disc_error = 1;
    check(stop_address([&] { program.read_file(3, 0x80100000, 0, 0); }) == 0x80028a6c,
          "Without arrivals the wait stops");
    resident.disc_error = 1;
    resident.platform = {{Input::Kind::interrupt}};
    const auto handler = pass(8, {read(0x8004c0c0, 0x08880000), read(0x8004c118, 0x08880000),
                                  read(0x8004c15c, 0x00880000), read(0x8004c180, 0x00880000),
                                  read(0x8004c198, 0x00880000)});
    resident.platform.insert(resident.platform.end(), handler.begin(), handler.end());
    check(program.read_file(3, 0x80100000, 0, 0) == 0 && resident.platform.empty(),
          "The arrival completes the earlier read, then this one starts");
    check(resident.disc_read.sector == 0x39 && resident.disc_error == 1,
          "The new read is set up after the wait");
}

} // namespace

int main() {
    const std::pair<const char *, void (*)()> tests[] = {{"platform_reads", platform_reads},
                                                         {"vsync", vsync},
                                                         {"pending", pending},
                                                         {"dma", dma},
                                                         {"cd", cd},
                                                         {"drive", drive},
                                                         {"arrivals", arrivals}};
    for (const auto &[name, test] : tests) {
        try {
            test();
        } catch (const std::exception &error) {
            std::cerr << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
