// Invented sound-driver objects exercise effect starts, voice claims and
// releases, pair stops and pair fields. They describe no original content.
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
} // namespace

int main() {
    try {
        effect_start();
        pairs();
        releases();
        std::cout << "Sound driver: three source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
