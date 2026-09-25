// Invented sound-driver objects exercise effect starts, voice claims and
// releases, pair stops and pair fields, and the tick (8003c028) and SPU
// transfer callback (8004cb3c) with invented platform inputs. They describe
// no original content.
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <iostream>

namespace resident = xem::reconstruction::resident;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call> void rejects(Call call, const char *message) {
    bool rejected = false;
    try {
        call();
    } catch (const resident::SoundError &) {
        rejected = true;
    }
    check(rejected, message);
}

constexpr std::uint32_t block = 0x80100000;
constexpr std::uint32_t bank = 0x80110000;
constexpr std::uint32_t wave = 0x80120000;

std::uint32_t record(std::uint32_t index) {
    return block + resident::voice_records + index * resident::voice_stride;
}
std::uint32_t get(resident::SoundDriver &d, std::uint32_t address, std::uint32_t size) {
    for (auto &[base, bytes] : d.objects)
        if (address >= base && address - base + size <= bytes.size()) {
            std::uint32_t value = 0;
            for (std::uint32_t i = 0; i < size; ++i)
                value |= static_cast<std::uint32_t>(bytes[address - base + i]) << (8U * i);
            return value;
        }
    throw std::runtime_error("test read outside objects");
}
void put(resident::SoundDriver &d, std::uint32_t address, std::uint32_t value, std::uint32_t size) {
    for (auto &[base, bytes] : d.objects)
        if (address >= base && address - base + size <= bytes.size()) {
            for (std::uint32_t i = 0; i < size; ++i)
                bytes[address - base + i] = static_cast<std::uint8_t>(value >> (8U * i));
            return;
        }
    throw std::runtime_error("test write outside objects");
}

// Twelve voices; voices 10 and 11 (channel 2) use hardware voices 3 and 4 and
// active bits 7 and 9. Effect 1 of bank 0 has voice data for its first voice
// only; its volume scale is 40. Wave bank 5 has one instrument.
resident::SoundDriver sample() {
    resident::SoundDriver d;
    d.flags = 0x800;
    d.effect_block = block;
    d.effect_banks = bank;
    d.wave_banks = wave;
    d.start_stamp = 0x1234;
    d.objects[block].resize(resident::voice_records + 12 * resident::voice_stride);
    d.objects[bank].resize(0x100);
    d.objects[wave].resize(0x40);
    put(d, record(10) + 0x27, 3, 1);
    put(d, record(10) + 6, 7, 1);
    put(d, record(11) + 0x27, 4, 1);
    put(d, record(11) + 6, 9, 1);
    put(d, bank + 0x10, 1, 2);
    put(d, bank + 0x16, 5, 2);
    put(d, bank + 0x18, 0x40, 2);
    put(d, bank + 0x24, 0x80, 2); // Effect 1, first voice's data
    put(d, bank + 0x41, 0x40, 1); // Effect 1 volume scale
    put(d, wave + 0x20, 5, 2);
    put(d, wave + 0x28, 0x1000, 4);
    put(d, wave + 0x30, 0x10, 4);
    put(d, wave + 0x34, 0x20, 2);
    put(d, wave + 0x36, 0x5678, 2);
    put(d, wave + 0x38, 0x9f12a345, 4);
    put(d, wave + 0x3c, 0x0765, 2);
    return d;
}

void effect_start() {
    auto d = sample();
    // Voice 11 holds hardware voice 4 from an earlier start; it has no data now.
    d.voice_owners[4] = record(11) + 0x30;
    put(d, block + 0x48, 1U << 9U, 4);
    resident::start_effect(d, 1, 2, 0x7f, 0x40);
    const auto v = record(10);
    check(d.effect_run == 2 && get(d, v + 8, 4) == 1 && get(d, v + 0xc, 4) == 0x1234 &&
              get(d, v + 7, 1) == 0x20,
          "Both voices take the id, stamp and code byte");
    check(get(d, v, 2) == (0x40b | 0x8000) && get(d, v + 2, 2) == 0x170 &&
              get(d, v + 0x76, 2) == 0x3f80 && get(d, v + 0x74, 2) == 0x4000 &&
              get(d, v + 0x78, 4) == 0x7f000000 && get(d, v + 0x10, 4) == bank + 0x80 &&
              get(d, v + 0x14, 4) == bank + 0x80 && get(d, v + 0x25, 1) == 5 &&
              get(d, v + 0x2c, 4) == wave,
          "A voice with data starts at the scaled volume and effect data");
    check(get(d, v + 0x4c, 4) == 0x1080 && get(d, v + 0x50, 4) == 0x180 &&
              get(d, v + 0x54, 1) == 5 && get(d, v + 0x55, 1) == 6 && get(d, v + 0x56, 1) == 7 &&
              get(d, v + 0x57, 1) == 0x45 && get(d, v + 0x58, 1) == 3 &&
              get(d, v + 0x59, 1) == 0x12 && get(d, v + 0x5b, 1) == 0xa &&
              get(d, v + 0x28, 1) == 0x1f && get(d, v + 0x5a, 1) == 0x1f &&
              get(d, v + 0x6c, 2) == 0x5678,
          "The first instrument of the effect's wave bank loads");
    check(d.voice_owners[3] == v + 0x30 && get(d, v + 0x30, 2) == 3 &&
              get(d, v + 0x36, 2) == 0xffff && get(d, v + 0x34, 2) == 0x200,
          "The voice claims its free hardware voice");
    check(d.voice_owners[4] == 0 && get(d, record(11), 2) == 0 && d.voice_changes == 0x18 &&
              get(d, block + 0x48, 4) == 1U << 7U && get(d, block + 0x10, 2) == 0x8000,
          "A voice without data releases its hardware voice and active bit");

    // A higher-priority holder keeps its hardware voice.
    auto held = sample();
    held.voice_owners[3] = record(0) + 0x30;
    put(held, record(0) + 0x34, 0x300, 2);
    resident::start_effect(held, 1, 2, 0x7f, 0x40);
    check(held.voice_owners[3] == record(0) + 0x30 && get(held, record(10) + 0x36, 2) == 0,
          "A lower-priority start does not take a held voice");

    auto disabled = sample();
    disabled.flags = 0;
    resident::start_effect(disabled, 1, 2, 0x7f, 0x40);
    check(disabled.effect_run == 0 && get(disabled, record(10), 2) == 0,
          "Effects are ignored while flag 800 is clear");
    auto missing = sample();
    resident::start_effect(missing, 0x10001, 2, 0x7f, 0x40);
    check(get(missing, record(10) + 8, 4) == 0, "An effect of an absent bank starts nothing");
    auto outside = sample();
    rejects([&] { resident::start_effect(outside, 1, 6, 0x7f, 0x40); },
            "A pair beyond the effect block is rejected");
}

void pairs() {
    auto d = sample();
    put(d, record(10), 1, 2);
    d.voice_owners[3] = record(10) + 0x30;
    put(d, block + 0x48, (1U << 7U) | 1U, 4);
    resident::set_effect_pair(d, 3, 0x76, 0x7f);
    check(get(d, record(10) + 0x76, 2) == 0x7f00 && get(d, record(10) + 2, 2) == 0x100 &&
              get(d, record(11) + 0x76, 2) == 0,
          "Only voices in use take a pair field");
    resident::set_effect_pair(d, 3, 0x74, 0x40);
    check(get(d, record(10) + 0x74, 2) == 0x4000, "The field offset is selected");
    resident::stop_effect_pair(d, 3);
    check(get(d, record(10), 2) == 0 && d.voice_owners[3] == 0 && d.voice_changes == 8 &&
              get(d, block + 0x48, 4) == 1,
          "Stopping a pair releases its voices and active bits");
}

// Pool headers A (the head) -> B (a sequence) -> C (a wave bank); the effect
// block also sits on the sequence list after B.
void releases() {
    auto d = sample();
    constexpr std::uint32_t head = 0x80130000, seq = 0x80130110, wav = 0x80130310;
    d.pool = head;
    d.pool_headers = {{head, {0x8000, 0, 0x80130100, seq - 0x10}},
                      {seq - 0x10, {2, 0, 0x80130300, wav - 0x10}},
                      {wav - 0x10, {2, 0, 0x80130400, 0}}};
    d.objects[seq].resize(resident::voice_records + resident::voice_stride);
    d.objects[wav].resize(0x40);
    d.sequences = seq;
    put(d, seq, block, 4);         // Next on the sequence list
    put(d, seq + 0x10, 0x8001, 2); // Playing
    put(d, seq + 0x14, 1, 1);      // One voice
    put(d, seq + resident::voice_records + 0x27, 5, 1);
    d.voice_owners[5] = seq + resident::voice_records + 0x30;
    resident::release_sequence(d, seq);
    check(get(d, seq + 0x10, 2) == 1 && d.voice_owners[5] == 0 && d.voice_changes == 0x20 &&
              d.sequences == block && d.pool_headers.at(head)[3] == wav - 0x10,
          "Releasing a playing sequence stops it, unlinks it and frees its block");
    rejects([&] { resident::release_sequence(d, seq); },
            "A sequence no longer on the list is rejected");

    // Wave bank C behind B on the wave list; its SPU block is table entry 2.
    put(d, wav + 0x28, 0x1010, 4);
    put(d, wave + 0x2c, wav, 4);
    d.spu_blocks[2] = 2;      // Entry 0 -> 2 -> 1
    d.spu_blocks[16 * 2] = 1; // In use
    d.spu_blocks[16 * 2 + 2] = 1;
    d.spu_blocks[16 * 2 + 4] = 0x10;
    d.spu_blocks[16 * 2 + 5] = 0x10;
    resident::release_wave_bank(d, wav);
    check(get(d, wave + 0x2c, 4) == 0 && d.spu_blocks[2] == 1 && d.spu_blocks[16 * 2] == 0 &&
              d.spu_blocks[16 * 2 + 2] == 0 && d.spu_blocks[16 * 2 + 4] == 0 &&
              d.pool_headers.at(head)[3] == 0,
          "Releasing a wave bank unlinks it, frees its SPU entry and its block");
    d.wave_banks = wav;
    put(d, wav + 0x2c, 0, 4);
    rejects([&] { resident::release_wave_bank(d, wav); },
            "A wave bank whose SPU block is not allocated is rejected");
}

// A Program whose driver has the statics and constants the tick reads, no
// sequences and no claimed voices; the SPU and root counter bases are the
// usual register pages.
namespace game = xem::reconstruction;
using Input = game::PlatformInput;
game::Program tick_sample() {
    game::Program program;
    auto &d = program.resident.sound;
    for (const auto &[address, size] : resident::sound_statics)
        d.statics[address].assign(size, 0);
    for (const auto &[address, size] : resident::sound_constants)
        d.constants[address].assign(size, 0);
    const auto word = [&](std::uint32_t address, std::uint32_t value) {
        auto &bytes = d.constants.at(address);
        for (std::uint32_t i = 0; i < 4; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    auto &shapes = d.constants.at(0x800508a4);
    for (std::uint32_t i = 0; i < 4; ++i)
        shapes[0x40 + i] = static_cast<std::uint8_t>(0x1f801c00U >> (8U * i));
    word(0x80056400, 0x1f801100);
    d.spu_registers = 0x1f801c00;
    return program;
}
Input read(std::uint32_t site, std::uint32_t value) { return {Input::Kind::read, site, value}; }
std::uint32_t statics(game::Program &program, std::uint32_t address) {
    const auto &statics = program.resident.sound.statics;
    const auto found = std::prev(statics.upper_bound(address));
    const auto offset = address - found->first;
    std::uint32_t value = 0;
    for (std::size_t i = 0; offset + i < found->second.size() && i < 4; ++i)
        value |= static_cast<std::uint32_t>(found->second[offset + i]) << (8U * i);
    return value;
}
template <typename Error, typename Call> bool raises(Call call) {
    try {
        call();
    } catch (const Error &) {
        return true;
    }
    return false;
}

void tick() {
    auto skipped = tick_sample();
    check(skipped.sound_tick(0x40) == 0 && skipped.resident.sound.start_stamp == 0,
          "Event flag 40 skips the tick before reading the counter");

    auto program = tick_sample();
    auto &resident = program.resident;
    resident.platform = {read(0x800406b0, 100), read(0x800406b0, 130)};
    check(program.sound_tick(0) == 0 && resident.platform.empty(),
          "A tick reads root counter 2 before and after its work");
    check(resident.sound.start_stamp == 1 && statics(program, 0x80059540) == 1 &&
              statics(program, 0x800595c4) == 30,
          "The tick counts itself and accumulates its counter time");
    check(resident.hardware_writes.empty(), "An idle driver writes no SPU register");

    resident.platform = {read(0x800406b0, 200), read(0x800406b0, 10)};
    static_cast<void>(program.sound_tick(0));
    check(statics(program, 0x80059540) == 1 && statics(program, 0x800595c4) == 30,
          "A counter that wrapped adds no time and no count");

    // Key on, key off with its ADSR release, and the SPU interrupt request.
    resident.sound.voice_holds = 0x5;
    resident.sound.voice_changes = 0x2;
    resident.sound.statics.at(0x8005955c)[0] = 1;
    resident.hardware_writes.clear();
    resident.platform = {read(0x800406b0, 0), read(0x8003eb90, 0x1234), read(0x8004d6b4, 0x8000),
                         read(0x8004d6c4, 0x8040), read(0x800406b0, 5)};
    static_cast<void>(program.sound_tick(0));
    const std::vector<game::HardwareWrite> expected{
        {0x1f801d88, 5, 2}, {0x1f801d8a, 0, 2}, {0x1f801c1a, 0x1206, 2},
        {0x1f801d8c, 2, 2}, {0x1f801d8e, 0, 2}, {0x1f801daa, 0x8040, 2}};
    check(resident.hardware_writes == expected && resident.sound.voice_holds == 0 &&
              resident.sound.voice_changes == 0 && statics(program, 0x8005955c) == 0,
          "Keys go on, released voices fade and go off, and the SPU interrupt is enabled");

    auto missing = tick_sample();
    check(raises<game::PlatformInputError>([&] { missing.sound_tick(0); }),
          "A tick without its counter input stops");
    auto wrong = tick_sample();
    wrong.resident.platform = {read(0x8003eb90, 1)};
    check(raises<game::PlatformInputError>([&] { wrong.sound_tick(0); }),
          "A read from another site is malformed input");
    auto wide = tick_sample();
    wide.resident.platform = {read(0x800406b0, 0x10000)};
    check(raises<game::PlatformInputError>([&] { wide.sound_tick(0); }),
          "A counter value wider than its halfword load is malformed input");

    // A playing sequence whose voice reads an opcode without a recovered
    // handler stops naming that handler.
    auto unknown = tick_sample();
    auto &d = unknown.resident.sound;
    constexpr std::uint32_t seq = 0x80130000;
    d.sequences = seq;
    auto &object = d.objects[seq];
    object.assign(resident::voice_records + resident::voice_stride + 0x10, 0);
    const auto put_seq = [&](std::uint32_t at, std::uint32_t value, std::uint32_t size) {
        for (std::uint32_t i = 0; i < size; ++i)
            object[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    put_seq(0x10, 0x8000, 2);     // playing
    put_seq(0x14, 1, 1);          // one voice
    put_seq(0x48, 1, 4);          // active voices
    put_seq(0x50, 0xffffffff, 4); // one step due
    put_seq(0x70, 1, 4);
    put_seq(0x94, 1, 2);                         // voice active
    put_seq(0x94 + 0x14, seq + 0x94 + 0x150, 4); // events at the object's end
    object[0x94 + 0x150] = 0x82;
    auto &table = d.constants.at(0x80050624);
    for (std::uint32_t i = 0; i < 4; ++i)
        table[2 * 4 + i] = static_cast<std::uint8_t>(0x8003cd00U >> (8U * i));
    unknown.resident.platform = {read(0x800406b0, 0)};
    bool named = false;
    try {
        static_cast<void>(unknown.sound_tick(0));
    } catch (const game::MissingDependency &error) {
        named = error.dependency == "symbol:8003cd00";
    }
    check(named, "An opcode without a recovered handler names the handler");
}

// Run one DMA interrupt whose only flagged channel is 4 (SPU), whose
// callback is 8004cb3c, with the given SPU control register reads between
// the DMA handler's reads.
void spu_dma(game::Program &program, std::vector<Input> spu) {
    auto &resident = program.resident;
    auto &irq = resident.interrupts;
    irq.initialized = 1;
    irq.mask = 8;
    irq.handlers[3] = 0x8004c098;
    irq.registers = {0x1f801070, 0x1f801074, 0x1f8010f0};
    irq.dma_callbacks[4] = 0x8004cb3c;
    resident.io[0x74] = 8;
    resident.cd.dma_interrupt_register = 0x1f8010f4;
    resident.platform = {read(0x8004ba34, 8), read(0x8004c0c0, 0x90900000),
                         read(0x8004c118, 0x90900000)};
    resident.platform.insert(resident.platform.end(), spu.begin(), spu.end());
    for (const auto &input :
         {read(0x8004c15c, 0x00900000), read(0x8004c180, 0x00900000), read(0x8004c198, 0x00900000),
          read(0x8004bac8, 0), read(0x8004baf0, 0)})
        resident.platform.push_back(input);
    program.interrupt_dispatch();
}

void transfer() {
    auto program = tick_sample();
    auto &resident = program.resident;
    auto &callback = resident.sound.constants.at(0x80058e40);
    for (std::uint32_t i = 0; i < 4; ++i)
        callback[i] = static_cast<std::uint8_t>(0x8003bb64U >> (8U * i));
    constexpr std::uint32_t queue = 0x80140000;
    resident.sound.constants[queue].assign(resident::transfer_queue_bytes, 0);
    auto &pointer = resident.sound.constants.at(0x80059458);
    for (std::uint32_t i = 0; i < 4; ++i)
        pointer[i] = static_cast<std::uint8_t>(queue >> (8U * i));
    resident.sound.flags = 0x10;
    spu_dma(program, {read(0x8004cb64, 0xc031), read(0x8004cb74, 0x20), read(0x8004cb98, 0x20),
                      read(0x8004cb98, 0)});
    const std::vector<game::HardwareWrite> writes{
        {0x1f801070, 0xfff7, 2}, {0x1f8010f4, 0x10900000, 4}, {0x1f801daa, 0xc001, 2}};
    check(resident.platform.empty() && resident.sound.flags == 0 &&
              resident.hardware_writes == writes,
          "The transfer callback leaves transfer mode, waits and finishes the queue step");

    auto queued = tick_sample();
    queued.resident.sound.constants.at(0x80058e40) = callback;
    queued.resident.sound.constants[queue] = resident.sound.constants.at(queue);
    queued.resident.sound.constants.at(0x80059458) = pointer;
    queued.resident.sound.statics.at(0x800594f4)[0] = 1;
    bool next = false;
    try {
        spu_dma(queued, {read(0x8004cb64, 0), read(0x8004cb74, 0)});
    } catch (const game::MissingDependency &error) {
        next = error.point.machine_address == 0x8003bf14;
    }
    check(next, "A queued transfer of an unrecovered type stops");

    // A chunked upload continues: 900h bytes left queue one 800h chunk,
    // which starts at once as a DMA write of 32 blocks.
    auto upload = tick_sample();
    auto &d = upload.resident.sound;
    d.constants.at(0x80058e40) = callback;
    d.statics[queue] = resident.sound.constants.at(queue);
    d.constants.at(0x80059458) = pointer;
    const auto put = [&](std::map<std::uint32_t, std::vector<std::uint8_t>> &blocks,
                         std::uint32_t address, std::uint32_t value) {
        auto found = std::prev(blocks.upper_bound(address));
        for (std::uint32_t i = 0; i < 4; ++i)
            found->second[address - found->first + i] =
                static_cast<std::uint8_t>(value >> (8U * i));
    };
    put(d.statics, queue + 16, 0x80038b4c); // entry 0 finished; its callback
    put(d.statics, 0x800595a4, 0x80150000); // staging block
    put(d.statics, 0x800595dc, 0x1010);     // SPU address
    put(d.statics, 0x800595e0, 0x900);      // bytes left
    put(d.constants, 0x80058e0c, 0x1f8010c0);
    put(d.constants, 0x80058e10, 0x1f8010c4);
    put(d.constants, 0x80058e14, 0x1f8010c8);
    put(d.constants, 0x80058e1c, 0x1f801014);
    put(d.constants, 0x80058e30, 3);
    upload.resident.io[0x14] = 0x20; // SPU delay register as observed
    d.flags = 0x10;                  // a transfer is running
    spu_dma(upload, {read(0x8004cb64, 0xc030), read(0x8004cb74, 0)});
    check(statics(upload, 0x800595e0) == 0x100 && statics(upload, 0x800595dc) == 0x1810 &&
              statics(upload, 0x80059510) == 1 && statics(upload, 0x800594f4) == 1 &&
              statics(upload, 0x80058e60) == 32 && statics(upload, 0x80058e5c) == 0x80150000,
          "The upload queues and starts its next 800h chunk");
    const auto &w = upload.resident.hardware_writes;
    check(w.size() == 9 && w[3] == game::HardwareWrite{0x1f801da6, 0x202, 2} &&
              w[4] == game::HardwareWrite{0x1f801daa, 0xc020, 2} &&
              w[5] == game::HardwareWrite{0x1f801014, 0x20000020, 4} &&
              w[8] == game::HardwareWrite{0x1f8010c8, 0x01000201, 4},
          "The chunk goes to the SPU by DMA channel 4");

    auto event = tick_sample();
    bool delivered = false;
    try {
        spu_dma(event, {read(0x8004cb64, 0), read(0x8004cb74, 0)});
    } catch (const game::MissingDependency &error) {
        delivered = error.point.machine_address == 0x80040e18;
    }
    check(delivered, "Without a library callback the BIOS event is not reconstructed");
}
} // namespace

int main() {
    try {
        effect_start();
        pairs();
        releases();
        tick();
        transfer();
        std::cout << "Sound driver: five source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
