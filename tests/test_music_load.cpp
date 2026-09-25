// Invented sound-driver state, staged wave bank headers, sequence event data
// and platform reads exercise the music load's resident calls: the wave bank
// load 800380d0 with its SPU allocation and upload, the sequence calls
// 80039850 and 80039a80, and the wave chunk callback 800859dc. They describe
// no original content.
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace game = xem::reconstruction;
namespace resident = xem::reconstruction::resident;
namespace {
using Input = game::PlatformInput;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string stop(const std::function<void()> &call) {
    try {
        call();
    } catch (const game::MissingDependency &error) {
        return error.dependency;
    }
    return {};
}
Input read(std::uint32_t site, std::uint32_t value) { return {Input::Kind::read, site, value}; }
void put(std::vector<std::uint8_t> &bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes.at(at + i) = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t get(const std::vector<std::uint8_t> &bytes, std::size_t at, std::size_t width = 4) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes.at(at + i)) << (8U * i);
    return value;
}
// A value inside a map of blocks keyed by address.
std::uint32_t get(const std::map<std::uint32_t, std::vector<std::uint8_t>> &blocks,
                  std::uint32_t address, std::size_t width = 4) {
    const auto found = std::prev(blocks.upper_bound(address));
    return get(found->second, address - found->first, width);
}
void put(std::map<std::uint32_t, std::vector<std::uint8_t>> &blocks, std::uint32_t address,
         std::uint32_t value, std::size_t width = 4) {
    const auto found = std::prev(blocks.upper_bound(address));
    put(found->second, address - found->first, value, width);
}

constexpr std::uint32_t pool_head = 0x80065b00;
constexpr std::uint32_t pool_end = 0x80066000;
constexpr std::uint32_t queue = 0x80140000;
constexpr std::uint32_t staging = 0x80150000;

// A driver with its statics and constants, one sound-pool block (the head,
// ending at 80065b10) and one SPU allocation (0..1010h); SPU transfers go
// through DMA channel 4 with the usual register addresses.
game::Program driver() {
    game::Program program;
    auto &d = program.resident.sound;
    for (const auto &[address, size] : resident::sound_statics)
        d.statics[address].assign(size, 0);
    for (const auto &[address, size] : resident::sound_constants)
        d.constants[address].assign(size, 0);
    d.statics[queue].assign(resident::transfer_queue_bytes, 0);
    put(d.constants, 0x80058e40, 0x8003bb64);
    put(d.constants, 0x80059458, queue);
    put(d.constants, 0x80058e0c, 0x1f8010c0);
    put(d.constants, 0x80058e10, 0x1f8010c4);
    put(d.constants, 0x80058e14, 0x1f8010c8);
    put(d.constants, 0x80058e1c, 0x1f801014);
    put(d.constants, 0x80058e30, 3);
    put(d.constants, 0x800595e4, pool_end);
    put(d.constants, 0x800589b8, 4); // The reverb type the SPU library set
    d.spu_registers = 0x1f801c00;
    d.pool = pool_head;
    d.pool_headers[pool_head] = {2, 0, pool_head + 0x10, 0};
    d.spu_blocks[0] = 0x80;
    d.spu_blocks[8] = 0x10;
    d.spu_blocks[9] = 0x10;
    program.resident.io[0x14] = 0x20; // SPU delay register as observed
    return program;
}

// A staged wave bank header of 40h bytes for bank id 22 with 3000h bytes of
// samples, whose staged data follows the header.
void stage(game::Program &program, std::uint32_t fixed) {
    std::vector<std::uint8_t> bytes(0x2000);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 7);
    put(bytes, 0x10, 0x40);
    put(bytes, 0x14, 0x3000);
    put(bytes, 0x18, 0x40);
    put(bytes, 0x20, 0x22, 2);
    put(bytes, 0x28, fixed);
    program.resident.music_blocks.push_back({staging, std::move(bytes)});
}

void wave_bank() {
    auto program = driver();
    stage(program, 0);
    auto &resident = program.resident;
    auto &d = resident.sound;
    resident.platform = {read(0x8004cd8c, 0xc000)};
    const auto bank = program.load_wave_bank(staging, 0x2000, 0);
    check(bank == 0x80065fc0 && d.wave_banks == bank &&
              d.pool_headers.at(pool_head)[3] == bank - 16 &&
              d.pool_headers.at(bank - 16) == std::array<std::uint32_t, 4>{2, 0, pool_end, 0},
          "80039024 places the bank at the top of the pool and links it after the head");
    const auto &object = d.objects.at(bank);
    check(object.size() == 0x40 && get(object, 0x28) == 0x1010 && get(object, 0x2c) == 0 &&
              object[0x31] == static_cast<std::uint8_t>(0x31 * 7),
          "The header is copied; the SPU address and list link follow");
    const std::vector<std::uint8_t> spu(d.spu_blocks.begin(), d.spu_blocks.end());
    check(spu[16] == 0x80 && get(spu, 20) == 0x1010 && get(spu, 24) == 0x3000 && spu[2] == 1,
          "800393b8 allocates the samples after the first SPU allocation");
    check(get(d.statics, 0x80059584) == 0x2fd0 && get(d.statics, 0x80059588) == 0x1040,
          "8003827c queues the staged samples and keeps the rest of the upload");
    const std::vector<game::HardwareWrite> writes{
        {0x1f801da6, 0x202, 2},          {0x1f801daa, 0xc020, 2},   {0x1f801014, 0x20000020, 4},
        {0x1f8010c0, staging + 0x40, 4}, {0x1f8010c4, 0x7f0010, 4}, {0x1f8010c8, 0x01000201, 4}};
    check(resident.platform.empty() && resident.hardware_writes == writes && (d.flags & 0x10U) != 0,
          "The upload starts at once as a DMA write from the recorded control register");
    check(stop([&] { static_cast<void>(program.load_wave_bank(staging, 0x2000, 0)); }) ==
              "symbol:sound-error-00000016",
          "A bank id already loaded reaches the driver error handler");

    auto fixed = driver();
    stage(fixed, 0x20000);
    fixed.resident.platform = {read(0x8004cd8c, 0)};
    const auto at = fixed.load_wave_bank(staging, 0x2000, 0);
    check(get(fixed.resident.sound.objects.at(at), 0x28) == 0x20000,
          "A header's fixed SPU address is allocated there (800395b8)");
    auto taken = driver();
    stage(taken, 0x800);
    check(stop([&] { static_cast<void>(taken.load_wave_bank(staging, 0x2000, 0)); }) ==
              "symbol:sound-error-0000001f",
          "A fixed address inside an allocation fails");
}

constexpr std::uint32_t data = 0x80062648;
constexpr std::uint32_t wave = 0x80120000;

// Sequence event data of two tracks (the second empty) with one table
// entry, reverb type 4, and a wave bank with id 22.
game::Program sequence_program(std::uint8_t reverb) {
    auto program = driver();
    auto &d = program.resident.sound;
    d.flags = 0x1000;
    std::vector<std::uint8_t> bytes(0x100);
    put(bytes, 0x10, 0x1234, 2);
    bytes[0x14] = 2;
    bytes[0x15] = 1;
    put(bytes, 0x16, 0x22, 2);
    put(bytes, 0x18, 0x5678, 2);
    bytes[0x1a] = reverb;
    bytes[0x1b] = 0x50;
    bytes[0x1c] = 0x11;
    bytes[0x1d] = 0x22;
    put(bytes, 0x20, 0x40, 2);
    put(bytes, 0x22, 0x60, 2);
    for (const auto [offset, value] :
         {std::pair{0x40U, 3U}, {0x41U, 0x44U}, {0x42U, 0x33U}, {0x43U, 0x22U}, {0x44U, 0x11U}})
        bytes[offset] = static_cast<std::uint8_t>(value);
    program.resident.disc_transfers.push_back({data, std::move(bytes)});
    auto &bank = d.objects[wave];
    bank.assign(0x40, 0);
    put(bank, 0x20, 0x22, 2);
    put(bank, 0x28, 0x1000);
    put(bank, 0x30, 0x10);
    d.wave_banks = wave;
    return program;
}

void sequences() {
    auto program = sequence_program(4);
    auto &resident = program.resident;
    auto &d = resident.sound;
    const auto seq = program.open_sequence(data);
    const auto &object = d.objects.at(seq);
    check(seq == 0x80065b20 && object.size() == 0x4c4 &&
              d.pool_headers.at(pool_head)[3] == seq - 16 && d.sequences == seq &&
              get(object, 0) == 0 && get(object, 8) == data,
          "80038f18 places the sequence after the head and 8003b9e4 links it first");
    check(get(object, 0xc) == seq + 0x344 && get(object, 0x344 + 12) == 0x11223344,
          "8003b0ac fills the table after the voices");
    check(get(object, 0x10, 2) == 1 && get(object, 0x12, 2) == 0x1234 && object[0x14] == 2 &&
              get(object, 0x16, 2) == 0x22 && object[0x41] == 4 && get(object, 0x44, 2) == 0x5000 &&
              get(object, 0x70) == 0x7f000000 && get(object, 0x50) == 0x10000,
          "8003b22c copies the attributes and 8003b370 resets playback");
    check(get(d.statics, 0x80059409, 1) == 4 && d.reverb == 0x5000 &&
              get(d.statics, 0x8005940a, 1) == 0x11 && get(d.statics, 0x8005940b, 1) == 0x22 &&
              d.reverb_pair[0] == 0x5000 && d.spu_reverb_output[0] == 0x5000 &&
              resident.hardware_writes.size() == 2 &&
              resident.hardware_writes[0] == game::HardwareWrite{0x1f801d84, 0x5000, 2},
          "The same reverb type only applies the depth (80038934)");
    constexpr std::uint32_t r = 0x94;
    check(get(object, r, 2) == 0x8401 && get(object, r + 2, 2) == 0x170 && object[r + 6] == 0 &&
              get(object, r + 0x10) == data + 0x60 && get(object, r + 0x2c) == wave &&
              object[r + 0x27] == 0xff && get(object, r + 0x34, 2) == 0x100 &&
              get(object, r + 0x158, 2) == 0 && get(object, 0x48) == 1,
          "8003b424 sets up each track with data and loads its first instrument");

    program.start_sequence(seq, 0x7f, 0);
    const auto &started = d.objects.at(seq);
    check(get(started, 0x10, 2) == 0x8001 && get(started, 0x70) == 0x7f000000 &&
              get(started, 0x7a, 2) == 0x7f00 && get(started, 0x78, 2) == 0,
          "80039a80 restarts the sequence at full volume and marks it playing");
    program.start_sequence(seq, 0x40, 4);
    check(get(d.objects.at(seq), 0x78, 2) == 4 &&
              get(d.objects.at(seq), 0x74) == static_cast<std::uint32_t>((0x400000 / 4) << 8),
          "A fade stores its ticks and per-tick step");
    check(stop([&] { program.start_sequence(0, 0x7f, 0); }) == "symbol:sound-error-00000005",
          "Starting no sequence reaches the driver error handler");

    auto other = sequence_program(5);
    check(stop([&] { static_cast<void>(other.open_sequence(data)); }) == "symbol:800389bc",
          "Another reverb type needs the reverb work area reallocation");
}

void chunk() {
    game::Program program;
    auto &resident = program.resident;
    auto &read = resident.disc_read;
    constexpr std::uint32_t ring = 0x80160000;
    read.ring = {ring, std::vector<std::uint8_t>(0x34)};
    put(read.ring.bytes, 0, 2);
    put(read.ring.bytes, 4 + 8, 3, 2);
    read.ring_payload = {ring + 0x34, std::vector<std::uint8_t>(0x1000)};
    std::fill_n(read.ring_payload.bytes.begin() + 0x800, 0x800, 0x5a);
    resident.disc_stream.ring_buffer = ring;
    resident.music_blocks.push_back({staging, std::vector<std::uint8_t>(0x2000)});
    auto &music = resident.music;
    music.wave_staging = staging;
    music.wave_chunk_index = 1;
    program.consume_music_chunk(ring + 0x34 + 0x800);
    const auto &bytes = resident.music_blocks[0].bytes;
    check(music.wave_chunk_index == 2 && bytes[0x800] == 0x5a && bytes[0xfff] == 0x5a &&
              bytes[0x7ff] == 0 && bytes[0x1000] == 0 && get(read.ring.bytes, 12, 2) == 0,
          "800859dc stages the chunk at its index and releases its ring slot");
    music.wave_chunk_index = 5;
    program.consume_music_chunk(0);
    check(music.wave_chunk_index == 5, "Indices past 4 ignore the chunk");
}
} // namespace

int main() {
    try {
        wave_bank();
        sequences();
        chunk();
        std::cout << "Music load resident calls passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
