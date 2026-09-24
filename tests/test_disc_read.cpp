// Invented file and directory tables exercise resident read_file (800295d8)
// in its CD and ring modes, the list read 80029afc and each explicit stop. They describe no
// original content or observation.
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
constexpr std::uint32_t list_address = 0x80130000;

// Files 5, 3 and 4 of directory 1 (records 14, 12 and 13), then the zero file.
game::Program listed() {
    auto program = sample();
    auto &read = program.resident.disc_read;
    read.directory = 10;
    put(read.files, 13 * 7, 0x20000, 3);
    put(read.files, 13 * 7 + 3, 0x800, 4);
    put(read.files, 14 * 7, 0x30000, 3);
    put(read.files, 14 * 7 + 3, 0x801, 4);
    read.list = {list_address, std::vector<std::uint8_t>(3 * 8 + 2, 0)};
    const std::array<std::uint32_t, 3> files{5, 3, 4}, spare{0xaaaa, 0xbbbb, 0xcccc};
    for (std::size_t i = 0; i < 3; ++i) {
        put(read.list.bytes, i * 8, files[i], 2);
        put(read.list.bytes, i * 8 + 2, spare[i], 2);
        put(read.list.bytes, i * 8 + 4, 0x80100000 + 0x1000 * files[i], 4);
    }
    return program;
}

void list_read() {
    auto program = listed();
    auto &resident = program.resident;
    check(program.read_files(0x12345) == 0, "A list read returns zero");
    const auto &read = resident.disc_read;
    for (std::size_t i = 0; i < 3; ++i)
        check(get(read.list.bytes, i * 8, 2) == 3 + i &&
                  get(read.list.bytes, i * 8 + 4, 4) == 0x80103000 + 0x1000 * i,
              "The list is sorted by file with its destinations");
    check(get(read.list.bytes, 2, 2) == 0xaaaa && get(read.list.bytes, 10, 2) == 0xbbbb &&
              get(read.list.bytes, 18, 2) == 0xcccc,
          "Sorting leaves the unused halfwords in place");
    check(read.file == 3 && read.sector == 0x12345 && read.size == 0x1004 &&
              read.destination == 0x80103000 && read.offset == 0x2345 &&
              read.read_directory == 10 && read.w_fe0c == list_address && read.w_fe00 == 3,
          "The first sorted file is set up with the list's count");
    check(resident.disc_error == 3 && resident.disc_pending == 1 && read.requests == 1,
          "The list read is active for all its files");
    check(resident.cd.ready_callback == 0x8002a68c && resident.cd.sync_callback == 0x8002ac24 &&
              resident.cd.dma_callback == 0x8002ba40,
          "List reads install their callbacks");
    check(resident.hardware_writes == setloc, "The first file's Setloc goes to the controller");

    auto one = listed();
    put(one.resident.disc_read.list.bytes, 8, 0, 2);
    check(one.read_files(0) == 0 && one.resident.disc_read.file == 5 &&
              get(one.resident.disc_read.list.bytes, 0, 2) == 5,
          "A single entry needs no sorting");
}

void list_seek() {
    auto seek = listed();
    put(seek.resident.disc_read.list.bytes, 12, 0, 4); // File 3 sorts first.
    check(seek.read_files(3) == 0, "A list without a first destination returns zero");
    const auto &resident = seek.resident;
    check(resident.disc_pending == 3 && resident.disc_error == 0 && resident.disc_read.size == 0 &&
              resident.disc_read.w_fe00 == 3 && resident.disc_read.requests == 0,
          "The seek-only list sets state 3 and no active read");
    check(resident.disc_read.location == std::array<std::uint8_t, 4>{0x16, 0x36, 0x15, 0} &&
              resident.hardware_writes == setloc,
          "It seeks to the argument's file");
    auto pause = listed();
    put(pause.resident.disc_read.list.bytes, 12, 0, 4);
    check(pause.read_files(0) == 0 && pause.resident.disc_pending == 5, "A zero argument pauses");
    check(pause.resident.hardware_writes ==
              std::vector<Write>{{0x1f801800, 0, 1}, {0x1f801801, 9, 1}},
          "Pause has no parameters");

    auto none = listed();
    none.resident.disc_read.list.address = 0;
    check(none.read_files(0) == -3, "A missing list is rejected");
    auto empty = listed();
    put(empty.resident.disc_read.list.bytes, 0, 0, 2);
    check(empty.read_files(0) == -3 && empty.resident.disc_error == 0,
          "An empty list is rejected before any state changes");
    auto busy = listed();
    busy.resident.disc_pending = 1;
    stops([&] { busy.read_files(0); }, 0x80028a6c, "A list read waits for an earlier read");
    auto truncated = listed();
    truncated.resident.disc_read.list.bytes.resize(3 * 8);
    bool rejected = false;
    try {
        static_cast<void>(truncated.read_files(0));
    } catch (const game::field::FieldFormatError &) {
        rejected = true;
    }
    check(rejected, "A list without its terminating halfword is rejected");
}
} // namespace

void stream_read() {
    auto program = sample();
    auto &resident = program.resident;
    resident.disc_read.directory = 10;
    const std::array<std::uint16_t, 6> parameters{1, 2, 3, 4, 5, 0xffff};
    check(program.read_stream(3, ring, 0x12345, parameters) == 0, "A stream read returns zero");
    const auto &read = resident.disc_read;
    check(read.file == 3 && read.sector == 0x12345 && read.size == 0x1004 &&
              read.offset == 0x2345 && read.read_directory == 10,
          "The stream's file, rounded size and low offset are recorded");
    check(read.destination == ring + 0x34 && read.ring_slots == ring + 4 &&
              resident.disc_stream.ring_buffer == ring &&
              resident.disc_stream.active_block_count == 2,
          "The ring's payload is the destination");
    check(read.h_59f24 == parameters && read.w_59f3c == 0 && read.w_fe0c == 0,
          "The six parameters are stored and stream state cleared");
    check(get(read.ring.bytes, 8, 4) == 2 && get(read.ring.bytes, 12, 4) == 0,
          "The ring's slots are reset");
    check(resident.disc_pending == 1 && resident.disc_error == 1 && read.requests == 1 &&
              resident.cd.ready_callback == 0x8002a68c && resident.cd.sync_callback == 0x8002b5d0 &&
              resident.cd.dma_callback == 0x8002bb50,
          "Stream reads install their callbacks");
    auto expected = setloc;
    expected.insert(expected.begin(), {0x1f8010f4, 0x980000, 4});
    check(resident.hardware_writes == expected, "The stream's Setloc goes to the controller");

    auto short_ring = sample();
    put(short_ring.resident.disc_read.ring.bytes, 0, 1, 4);
    check(short_ring.read_stream(3, ring, 0, parameters) == -4, "A one-slot ring is rejected");
    auto no_ring = sample();
    check(no_ring.read_stream(3, 0, 0, parameters) == -4, "A missing ring is rejected");
    auto no_file = sample();
    no_file.resident.disc_read.directory = 10;
    check(no_file.read_stream(0, ring, 0, parameters) == -3 &&
              no_file.read_stream(4, ring, 0, parameters) == -3 &&
              no_file.resident.hardware_writes.empty(),
          "Missing or empty files are rejected before any command");
}

// File b8 of directory 1 (record c1) holds 1001 bytes; the heap has one free
// 1ff8-byte block. Map data 0 reads ahead into slot 0.
void preload() {
    auto program = sample();
    auto &resident = program.resident;
    put(resident.disc_read.files, 0xc1 * 7, 0x2000, 3);
    put(resident.disc_read.files, 0xc1 * 7 + 3, 0x1001, 4);
    auto &heap = resident.heap;
    heap.head = 0x80100008;
    heap.headers = {{0x80100000, {0x80102008, 0x84000000}},
                    {0x80102000, {0, xem::reconstruction::resident::heap_end_tag}}};
    heap.held = {{0x80100008, std::vector<std::uint8_t>(0x1ff8, 0x5a)}};
    check(program.select_directory(0, 1) == 10, "Directory 1 is selected");
    check(program.preload_field(0, 0) == -1, "Starting a read-ahead returns -1");
    const auto block = resident.preload_block.address;
    check(resident.preload_slot == 0 && resident.preload_id == 0 &&
              resident.preload_size == 0x1004 && resident.preload_block.bytes.size() == 0x1004,
          "8001b53c sizes the block from the rounded file size and records the slot");
    check((heap.headers.at(block - 8)[1] & xem::reconstruction::resident::heap_keep) != 0,
          "The read-ahead block is kept");
    check(resident.disc_read.destination == block && resident.disc_read.file == 0xb8 &&
              resident.disc_read.sector == 0x2000,
          "File id + b8 is read into the block");
    check(program.preload_field(0, 0) == 0, "The same data in the same slot is ready");
    check(program.preload_field(2, 0) == -1 && resident.preload_id == 0,
          "Other data waits while the disc is busy");
}

int main() {
    try {
        preload();
        cd_read();
        ring_read();
        rejected();
        stops_explicitly();
        list_read();
        list_seek();
        stream_read();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "disc read reconstruction passed\n";
    return 0;
}
