// Invented file and directory tables exercise resident read_file (800295d8)
// in its CD and ring modes and each explicit stop. They describe no original
// content or observation.
#include "xem/reconstruction/program.hpp"

#include <iostream>

namespace game = xem::reconstruction;
namespace {
using Write = game::HardwareWrite;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(std::vector<std::uint8_t> &bytes, std::size_t at, std::uint32_t value, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t get(const std::vector<std::uint8_t> &bytes, std::size_t at, std::size_t width) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
template <typename Call> void stops(Call call, std::uint32_t address, const char *message) {
    bool stopped = false;
    try {
        call();
    } catch (const game::MissingDependency &error) {
        stopped = error.point.machine_address == address;
    }
    check(stopped, message);
}

constexpr std::uint32_t ring = 0x80120000;

// Directory 1 begins after file 10. File 3 of it (record 12) starts at sector
// 12345 and holds 1001 bytes; file 4 (record 13) is empty.
game::Program sample() {
    game::Program program;
    auto &resident = program.resident;
    auto &read = resident.disc_read;
    read.file_table = 0x80010004;
    read.directory_table = 0x80018004;
    read.files.assign(0x8000, 0);
    read.directories.assign(0x7a, 0);
    put(read.directories, 2, 11, 2);
    put(read.files, 12 * 7, 0x12345, 3);
    put(read.files, 12 * 7 + 3, 0x1001, 4);
    resident.vsync_counter = 100;
    resident.cd_dma_register = 0x1f8010b8;
    auto &cd = resident.cd;
    cd.sync_status = 2;
    cd.parameter_counts[2] = 3;
    cd.registers = {0x1f801800, 0x1f801801, 0x1f801802};
    cd.dma_services = 0x8005890c;
    cd.dma_set_callback = 0x8004c21c;
    cd.dma_interrupt_register = 0x1f8010f4;
    cd.dma_callback = 0x8002ba40;
    resident.io[0xf6] = 0x98; // DICR: channel 3 and master enable
    read.ring = {ring, std::vector<std::uint8_t>(0x24 + 2 * 8, 0xee)};
    put(read.ring.bytes, 0, 2, 4);
    return program;
}
const std::vector<Write> setloc{{0x1f801800, 0, 1},
                                {0x1f801802, 0x16, 1},
                                {0x1f801802, 0x36, 1},
                                {0x1f801802, 0x15, 1},
                                {0x1f801801, 2, 1}};

void cd_read() {
    for (const auto mode : {0U, 0x80U}) {
        auto program = sample();
        auto &resident = program.resident;
        check(program.select_directory(0, 1) == 10 && resident.disc_read.directory == 10,
              "A directory selects its first file index less one");
        check(program.read_file(3, 0x80100000, 0x12345, mode) == 0, "A CD read returns zero");
        const auto &read = resident.disc_read;
        check(read.sector == 0x12345 && read.size == 0x1004 && read.read_directory == 10 &&
                  read.destination == 0x80100000 && read.offset == 0x2345 && read.file == 3,
              "Read setup records the file, rounded size and low offset");
        check(read.location == std::array<std::uint8_t, 4>{0x16, 0x36, 0x15, 0},
              "The sector becomes a BCD position after 150 lead-in sectors");
        check(resident.disc_error == 1 && resident.disc_pending == 1 && read.requests == 1,
              "The read is marked active and counted");
        check(resident.cd.ready_callback == 0x8002a68c && resident.cd.sync_callback == 0x8002b084 &&
                  resident.cd.dma_callback == 0,
              "CD reads install their callbacks and clear the DMA callback");
        check(resident.cd.position == read.location && resident.cd.command == 2 &&
                  resident.cd.sync_status == 0,
              "Setloc records its parameter and clears the command status");
        check(resident.cd_sync_label == 0x80018eb0 && resident.cd_sync_deadline == 100 + 0x3c0 &&
                  resident.cd_sync_polls == 1,
              "The command waits for the previous status once");
        auto expected = setloc;
        expected.insert(expected.begin(), {0x1f8010f4, 0x900000, 4});
        check(resident.hardware_writes == expected,
              "DICR loses the channel 3 enable, then Setloc goes to the controller");
    }
}

void ring_read() {
    auto program = sample();
    auto &resident = program.resident;
    resident.disc_read.directory = 10;
    check(program.read_file(3, ring, 0, 0x100) == 0, "A ring read returns zero");
    const auto &read = resident.disc_read;
    check(resident.disc_stream.ring_buffer == ring && read.destination == ring + 0x34 &&
              read.ring_slots == ring + 4 && resident.disc_stream.active_block_count == 2,
          "The ring's payload becomes the destination");
    check(get(read.ring.bytes, 4, 4) == 0 && get(read.ring.bytes, 8, 4) == 2 &&
              get(read.ring.bytes, 12, 4) == 0 && get(read.ring.bytes, 20, 4) == 0xeeeeeeee,
          "The ring's slots are reset and the count stored at offset 8");
    check(resident.cd.sync_callback == 0x8002b2f0 && resident.cd.dma_callback == 0x8002ba58,
          "Ring reads install their own callbacks");
    auto expected = setloc;
    expected.insert(expected.begin(), {0x1f8010f4, 0x980000, 4});
    check(resident.hardware_writes == expected, "DICR keeps the channel 3 enable");

    auto staged = sample();
    staged.resident.disc_read.directory = 10;
    // The control byte is the low byte of mode | a0.
    check(staged.read_file(3, ring, 0, 0x200) == 0 &&
              staged.resident.disc_read.b_59f18 == std::array<std::uint8_t, 4>{0xa0},
          "A staged ring read only prepares the ring");
    check(staged.resident.hardware_writes.empty() && staged.resident.disc_pending == 0,
          "A staged ring read issues no command");

    auto empty = sample();
    empty.resident.disc_read.directory = 10;
    put(empty.resident.disc_read.ring.bytes, 0, 0, 4);
    check(empty.read_file(3, ring, 0, 0x100) == -4 && empty.resident.hardware_writes.empty(),
          "An empty ring fails before any command");
}

void rejected() {
    auto program = sample();
    program.resident.disc_read.directory = 10;
    check(program.read_file(0, 0x80100000, 0, 0) == -3, "File zero does not exist");
    check(program.read_file(4, 0x80100000, 0, 0) == -3, "An empty file cannot be read");
    check(program.read_file(3, 0, 0, 0) == -3, "A read needs a destination");
    check(program.resident.disc_error == 0, "Rejected reads change no read state");
    check(program.select_directory(0, 0) == -1 && program.resident.disc_read.directory == 0,
          "An empty directory selects zero and returns -1");
}

void stops_explicitly() {
    auto host = sample();
    host.resident.disc_stream.host_file_table = 0x80100000;
    stops([&] { host.read_file(3, 0x80100000, 0, 0); }, 0x8002876c,
          "The host-file branch stops at its first host call");
    auto busy = sample();
    busy.resident.disc_read.directory = 10;
    busy.resident.disc_pending = 1;
    stops([&] { busy.read_file(3, 0x80100000, 0, 0); }, 0x80028a6c,
          "Waiting for an earlier read needs its interrupts");
    auto pending = sample();
    pending.resident.disc_read.directory = 10;
    pending.resident.cd.sync_status = 0;
    stops([&] { pending.read_file(3, 0x80100000, 0, 0); }, 0x80041d88,
          "A command without status waits for an interrupt");
    auto status = sample();
    status.resident.disc_read.directory = 10;
    status.resident.cd.status = 0x10;
    stops([&] { status.read_file(3, 0x80100000, 0, 0); }, 0x80042428,
          "A blocking Getstat waits for its interrupt");
    auto service = sample();
    service.resident.disc_read.directory = 10;
    service.resident.cd.dma_set_callback = 0x8004c000;
    stops([&] { service.read_file(3, 0x80100000, 0, 0); }, 0x8004b7b8,
          "Only the recovered DMA callback service is called");
    auto debug = sample();
    debug.resident.disc_read.directory = 10;
    debug.resident.cd.debug = 2;
    stops([&] { debug.read_file(3, 0x80100000, 0, 0); }, 0x800420e4,
          "Command tracing needs the debug printer");
    auto poll = sample();
    poll.resident.disc_read.directory = 10;
    poll.resident.cd.interrupt_poll = 1;
    stops([&] { poll.read_file(3, 0x80100000, 0, 0); }, 0x800415b4,
          "Polling the controller is not recovered");
    auto timeout = sample();
    timeout.resident.disc_read.directory = 10;
    timeout.resident.vsync_counter = 0x7fffff00;
    stops([&] { timeout.read_file(3, 0x80100000, 0, 0); }, 0x800429e4,
          "A wrapped deadline reaches the timeout path");
    auto unowned = sample();
    unowned.resident.disc_read.directory = 10;
    unowned.resident.disc_read.ring.bytes.clear();
    bool rejected = false;
    try {
        static_cast<void>(unowned.read_file(3, ring, 0, 0x100));
    } catch (const game::field::FieldFormatError &) {
        rejected = true;
    }
    check(rejected, "A ring without owned header bytes is rejected");
}
} // namespace

int main() {
    try {
        cd_read();
        ring_read();
        rejected();
        stops_explicitly();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "disc read reconstruction passed\n";
    return 0;
}
