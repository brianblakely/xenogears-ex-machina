// Invented driver objects, volumes and pitch tables exercise the resident
// sound output mode (800386c4 and callees) and the voice update 8003ebf0.
// They describe no original content or observation.
#include "xem/reconstruction/program.hpp"

#include <iostream>

namespace game = xem::reconstruction;
namespace resident = game::resident;
namespace {
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

constexpr std::uint32_t sequence = 0x80067000;
constexpr std::size_t first_voice = resident::voice_records;
constexpr std::size_t second_voice = resident::voice_records + resident::voice_stride;

// One sequence with two voices: the first sounding, the second silent.
game::Program sample() {
    game::Program program;
    auto &sound = program.resident.sound;
    std::vector<std::uint8_t> bytes(resident::voice_records + 2 * resident::voice_stride);
    put(bytes, 0x14, 2, 1);
    put(bytes, first_voice, 0x0009, 2);
    sound.objects.emplace(sequence, std::move(bytes));
    sound.sequences = sequence;
    sound.spu_registers = 0x1f801c00;
    sound.master = 0x3fff;
    sound.cd = 0x7fff;
    sound.reverb = 0x2800;
    return program;
}
std::vector<std::uint8_t> &voices(game::Program &program) {
    return program.resident.sound.objects.at(sequence);
}

void modes() {
    for (const auto [mode, bits, master, reverb] :
         {std::tuple{0, 0x000U, std::array<std::uint16_t, 2>{0x3fff, 0x3fff},
                     std::array<std::uint16_t, 2>{0x2800, 0x2800}},
          std::tuple{1, 0x100U, std::array<std::uint16_t, 2>{0x3fff, 0x3fff},
                     std::array<std::uint16_t, 2>{0x2800, 0x2800}},
          std::tuple{2, 0x300U, std::array<std::uint16_t, 2>{0x3fff, 0xc001},
                     std::array<std::uint16_t, 2>{0xd800, 0x2800}},
          std::tuple{3, 0x500U, std::array<std::uint16_t, 2>{0xc001, 0x3fff},
                     std::array<std::uint16_t, 2>{0x2800, 0xd800}},
          std::tuple{7, 0x000U, std::array<std::uint16_t, 2>{0x3fff, 0x3fff},
                     std::array<std::uint16_t, 2>{0x2800, 0x2800}}}) {
        auto program = sample();
        auto &sound = program.resident.sound;
        sound.flags = 0x0b01; // Earlier stereo bits are replaced; others stay.
        program.set_sound_mode(mode);
        check(sound.flags == (0x0801U | bits), "The mode selects bits 700");
        check(sound.master_pair == master && sound.reverb_pair == reverb,
              "The mode negates one half of the master and reverb pairs");
        check(sound.cd_pair[0] == 0x7fff && sound.cd_pair[1] == 0x7fff,
              "The CD pair ignores the mode");
        check(sound.commits == 0xc3, "All three pairs are marked for the tick");
        const std::vector<game::HardwareWrite> writes{{0x1f801d84, reverb[0], 2},
                                                      {0x1f801d86, reverb[1], 2}};
        check(program.resident.hardware_writes == writes && sound.spu_reverb_output == reverb,
              "libspu stores the reverb output volume");
        check(get(voices(program), first_voice + 2, 2) == 0x100 &&
                  get(voices(program), second_voice + 2, 2) == 0,
              "Sounding voices recompute their output");
        check(resident::sound_mode(sound) == (bits == 0       ? 0U
                                              : bits == 0x100 ? 1U
                                                              : 2U),
              "80038824 reports mono, stereo or a negating mode");
    }
    auto program = sample();
    program.resident.sound.flags = 0x4000;
    bool stopped = false;
    try {
        program.set_sound_mode(1);
    } catch (const game::MissingDependency &error) {
        stopped = error.point.machine_address == 0x8003885c;
    }
    check(stopped, "The CD mix stops as a missing dependency");
}

void mode_voice() {
    auto program = sample();
    auto &sound = program.resident.sound;
    std::vector<std::uint8_t> record(0x70);
    put(record, 0, 1, 2);
    put(record, 0x12, 0x40, 2);
    sound.objects.emplace(0x80068000, std::move(record));
    sound.mode_voice = 0x80068000;
    program.set_sound_mode(2);
    const auto &stereo = sound.objects.at(0x80068000);
    check(get(stereo, 0x38, 2) == 0x2000 && get(stereo, 0x3a, 2) == 0 &&
              get(stereo, 0x64, 2) == 0 && get(stereo, 0x66, 2) == 0x2000 &&
              get(stereo, 0x36, 2) == 1 && get(stereo, 0x62, 2) == 1,
          "A stereo mode puts the mode voice on one side at level << 7");
    program.set_sound_mode(0);
    const auto &mono = sound.objects.at(0x80068000);
    check(get(mono, 0x38, 2) == 0x1000 && get(mono, 0x3a, 2) == 0x1000 &&
              get(mono, 0x64, 2) == 0x1000 && get(mono, 0x66, 2) == 0x1000,
          "Mono puts it on both sides at level << 6");
}

void volumes() {
    resident::SoundDriver sound;
    sound.flags = 0x300;
    resident::set_master_volume(sound, 0x3fff, 0);
    check(sound.master == 0x3fff && sound.master_level == 0x3fff0000 &&
              sound.master_pair == std::array<std::uint16_t, 2>{0x3fff, 0xc001} &&
              sound.commits == 3 && sound.master_target == 0x3fff,
          "An immediate master volume applies the wide pair");
    resident::set_master_volume(sound, 0x1fff, 16);
    check(sound.master_frames == 16 && sound.master_target == 0x1fff &&
              static_cast<std::int32_t>(sound.master_step) == ((0x1fff00 - 0x3fff00) / 16) * 256,
          "A master fade stores its per-tick step");
    sound.master_frames = 0;
    resident::set_master_volume(sound, 0x3fff, 8);
    check(sound.master_frames == 0, "No fade is stored at the current level");
    resident::set_cd_volume(sound, 0x7fff, 0);
    check(sound.cd == 0x7fff && sound.cd_pair[1] == 0x7fff && sound.cd_level == 0x7fff0000 &&
              sound.commits == 0xc3,
          "An immediate CD volume sets both halves");
    resident::set_cd_volume(sound, 0, 10);
    check(sound.cd_frames == 10 && sound.cd_target == 0 &&
              static_cast<std::int32_t>(sound.cd_step) == (-0x7fff00 / 10) * 256,
          "A CD fade to silence");
}

void pan_law_and_pitch() {
    for (const auto [flags, pan, first, second] :
         {std::tuple{0x100U, 0, 0x3f7e, 0}, std::tuple{0x100U, 0x4000, 0x2cfe, 0x2cfe},
          std::tuple{0x100U, 0x7f00, 0xb3, 0x3f34}, std::tuple{0x300U, 0x2000, 0x363e, 0x167f},
          std::tuple{0x000U, 0, 0x2cfe, 0x2cfe}, std::tuple{0x000U, 0x7f00, 0x2cfe, 0x2cfe}}) {
        auto program = sample();
        auto &sound = program.resident.sound;
        sound.flags = static_cast<std::uint16_t>(flags);
        auto &bytes = voices(program);
        put(bytes, 0x72, 0x7fff, 2); // Sequence volume.
        put(bytes, first_voice + 2, 0x100, 2);
        put(bytes, first_voice + 0x7a, 0x7fff, 2);
        put(bytes, first_voice + 0x76, 0x7fff, 2);
        put(bytes, first_voice + 0x74, static_cast<std::uint32_t>(pan), 2);
        resident::update_voices(sound, sequence, sequence + first_voice, 2);
        check(get(bytes, first_voice + 0x38, 2) == static_cast<std::uint32_t>(first) &&
                  get(bytes, first_voice + 0x3a, 2) == static_cast<std::uint32_t>(second),
              "The pan law gives the expected pair");
        check(get(bytes, first_voice + 0x36, 2) == 1 && get(bytes, first_voice + 2, 2) == 0,
              "The output is marked for the tick and the changes are consumed");
    }
    auto program = sample();
    auto &sound = program.resident.sound;
    sound.pitch_tables.assign(resident::pitch_table_bytes, 0);
    sound.pitch_tables[0x13] = 0x72; // Note 13xx: octave 7, semitone 2.
    const auto step = 0x80050bf0U + (0x34 + 2 * 256) * 2 - resident::pitch_table_address;
    sound.pitch_tables[step] = 0x00;
    sound.pitch_tables[step + 1] = 0x10; // 1000
    auto &bytes = voices(program);
    put(bytes, first_voice + 2, 0x203, 2);
    put(bytes, first_voice + 0x6a, 0x1300, 2);
    put(bytes, first_voice + 0xd0, 0x30, 2);
    put(bytes, 0x7e, 4, 2);
    put(bytes, first_voice + 0x27, 5, 1);
    resident::update_voices(sound, sequence, sequence + first_voice, 2);
    check(get(bytes, first_voice + 0x44, 2) == 0x2000,
          "A pitch change shifts the table step left by the octave above 6");
    // The claim writes the owner's +6 (voice +36) after the pitch marked it.
    const auto owner = sequence + first_voice + 0x30;
    check(sound.voice_owners[5] == owner && sound.voice_changes == 0x20 &&
              sound.voice_holds == 0x20 && sound.voice_releases == 0x20 &&
              get(bytes, first_voice + 0x30, 2) == 5 && get(bytes, first_voice + 0x36, 2) == 0xffff,
          "A key-on claims and holds the voice; the release request marks it");
    put(bytes, first_voice + 2, 0x200, 2);
    put(bytes, first_voice + 0x36, 0, 2);
    resident::update_voices(sound, sequence, sequence + first_voice, 2);
    check(get(bytes, first_voice + 0x36, 2) == 4, "A pitch change alone marks bit 4");
    bool refused = false;
    try {
        resident::update_voices(sound, sequence, sequence + first_voice, 0x10000);
    } catch (const resident::SoundError &) {
        refused = true;
    }
    check(refused, "A zero 16-bit voice count is refused");
}

void next_mode() {
    game::Program program;
    program.resident.mode_loaded = 2;
    program.set_next_mode(2);
    check(program.resident.next_mode == 2 && program.resident.mode_loaded == 2,
          "The loaded mode stays cached");
    program.set_next_mode(1);
    check(program.resident.next_mode == 1 && program.resident.mode_loaded == 0xffffffffU,
          "Another mode forgets the cached one");
    program.resident.mode_loaded = 2;
    // An allocated block goes back to the heap and the pointer is cleared.
    auto &heap = program.resident.heap;
    heap.head = 0x80100008;
    heap.headers = {{0x80100000, {0x80100018, 0x00400000}}, {0x80100010, {0x80100018, 0x200000}}};
    program.resident.mode_block = {0x80100008, std::vector<std::uint8_t>(8, 0xab)};
    program.set_next_mode(1);
    check(program.resident.mode_block.address == 0 && heap.held.contains(0x80100008) &&
              heap.headers.at(0x80100000)[1] == 0x84000000U,
          "Another mode releases the cached block");
    // A pointer into bytes the heap does not hold is refused.
    program.resident.mode_loaded = 2;
    program.resident.mode_block = {0x80200008, {}};
    bool refused = false;
    try {
        program.set_next_mode(1);
    } catch (const resident::HeapError &) {
        refused = true;
    }
    check(refused, "A stale pointer outside heap-held bytes is refused");
}
} // namespace

int main() {
    try {
        modes();
        mode_voice();
        volumes();
        pan_law_and_pitch();
        next_mode();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
