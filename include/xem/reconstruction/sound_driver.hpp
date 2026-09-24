#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace xem::reconstruction::resident {

class SoundError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Resident sound driver of executable dc0b2dd7...: the sound-effect paths the
// field reaches. The driver's objects live in its own pool (80065b0c); each is
// owned whole, keyed by its original address, over its pool block (the header
// 10 bytes before the object ends at the word 8 bytes before it). Code reads
// and writes them by original address, as the original does.
//
// The effect block (*800595d8) is a sequence-shaped object: a 94-byte header
// (flags +10, active voice bits +48), then voice records of 158 bytes. Effect
// banks (list 80059440, next +1c) hold an id (+14), a wave bank id (+16), a
// volume table offset (+18), two halfword voice data offsets per effect from
// +20 and the voice data. Wave banks (list 80059558, next +2c) hold an id
// (+20), a sample base (+28) and 16-byte instrument entries from +30.
struct SoundDriver {
    std::uint16_t flags{};        // 8005957c; bit 800 enables sound effects
    std::uint32_t effect_block{}; // 800595d8
    std::uint32_t effect_run{};   // 80059404: voices one effect start initializes
    std::uint32_t effect_banks{}; // 80059440
    std::uint32_t wave_banks{};   // 80059558
    std::uint32_t sequences{};    // 80059564: sequence list (next +0)
    std::uint32_t pool{};         // 80059410: first sound-pool block header
    std::uint32_t start_stamp{};  // 80059504: copied into each started voice (+0c)
    // Hardware voice bookkeeping: the owner (a voice record + 30) of each of
    // the 24 voices, voices marked when claimed or released, and voices cleared
    // when claimed or released.
    std::array<std::uint32_t, 24> voice_owners{}; // 8006252c
    std::uint32_t voice_changes{};                // 80059554
    std::uint32_t voice_holds{};                  // 800594fc
    std::map<std::uint32_t, std::vector<std::uint8_t>> objects;
    // Sound-pool block headers (10 bytes before each object: flags, 0, end,
    // next allocated header at +c), keyed by header address. Freeing a block
    // only unlinks it from this list.
    std::map<std::uint32_t, std::array<std::uint32_t, 4>> pool_headers;
    // SPU memory allocation table: 12 linked 16-byte entries (in-use bytes +0
    // and +1, next entry index +2, SPU address +4, size +8); entry 0 heads it.
    std::array<std::uint8_t, 0xc0> spu_blocks{};

    // Output volumes. The setters (80038c68, 80038d18) either apply a level
    // at once or leave a fade (step per tick, 8.8 fixed point shifted left 8,
    // over `frames` ticks) that the sound tick (8003c040..8003c12c, not part
    // of this driver code) applies; the tick commits the pairs named by
    // `commits` to the SPU. Levels are 16.16: level << 16.
    std::uint32_t commits{};                    // 8005a3c0: 3 master pair, c0 CD pair
    std::array<std::uint16_t, 2> master_pair{}; // 8005a3c4: left, right
    std::array<std::uint16_t, 2> cd_pair{};     // 8005a3d0: left, right
    std::uint16_t master{};                     // 8005a3e8: master volume applied last
    std::uint16_t cd{};                         // 8005a3ea: CD volume applied last
    std::uint16_t reverb{};                     // 8005a3ec: reverb output volume
    std::uint32_t master_level{};               // 8005a3f0
    std::uint32_t master_step{};                // 8005a3f4
    std::uint16_t master_frames{};              // 8005a3f8
    std::uint16_t master_target{};              // 8005a3fa
    std::uint32_t cd_level{};                   // 8005a3fc
    std::uint32_t cd_step{};                    // 8005a400
    std::uint16_t cd_frames{};                  // 8005a404
    std::uint16_t cd_target{};                  // 8005a406
    std::array<std::uint16_t, 2> reverb_pair{}; // 8005940c: left, right
    // A voice-shaped record whose output set_sound_mode also recomputes
    // (+0 bit 1 active, +12 level), or zero.
    std::uint32_t mode_voice{}; // 80059518
    // Voices whose owner asked for a release (8003efa0).
    std::uint32_t voice_releases{}; // 80059550
    // libspu: the SPU register base (80058e08) and the reverb output volume
    // it last stored (800589bc, 800589be).
    std::uint32_t spu_registers{};
    std::array<std::uint16_t, 2> spu_reverb_output{};
    // Read-only pitch tables (80050b78: 120 octave/semitone bytes, then from
    // 80050bf0 12 rows of 256 halfword steps), keyed from 80050b78.
    std::vector<std::uint8_t> pitch_tables;
};

inline constexpr std::uint32_t pitch_table_address = 0x80050b78;
inline constexpr std::uint32_t pitch_table_bytes = 0x1878;

inline constexpr std::uint32_t spu_block_table = 0x8006f9fc;

inline constexpr std::uint32_t voice_records = 0x94;
inline constexpr std::uint32_t voice_stride = 0x158;

// 8003a344 (field +76) and 8003a55c (field +74): for the effect voice pair
// (channel & fe) ^ 8, each voice in use takes value << 8 in the field and
// 100 in its halfword +2.
void set_effect_pair(SoundDriver &driver, std::uint32_t channel, std::uint32_t field,
                     std::uint32_t value);
// 8003a20c: stop the effect voice pair and release its hardware voices.
void stop_effect_pair(SoundDriver &driver, std::uint32_t channel);
// 80039f9c: when effects are enabled, start effect `id` on the voice pair of
// `channel` (8003b644). `volume` and `pan` are the original byte arguments.
void start_effect(SoundDriver &driver, std::uint32_t id, std::uint32_t channel,
                  std::uint32_t volume, std::uint32_t pan);

// 80039c4c: stop a sequence (clear flag 8000) and release its voices.
void stop_sequence(SoundDriver &driver, std::uint32_t sequence);
// 80038310: unlink a wave bank, free its SPU block (800396e0) and its pool
// block.
void release_wave_bank(SoundDriver &driver, std::uint32_t wave);
// 800399d4: stop a playing sequence, unlink it, free its child blocks and,
// unless flag 4000 marks it static, free its pool block.
void release_sequence(SoundDriver &driver, std::uint32_t sequence);

// Output modes selected by the title Sound menu. The mode lives in bits 700 of
// the driver flags: 0 mono, 100 stereo, 300 wide stereo (the right channel of
// the master pair and the left of the reverb pair are negated) and 500, which
// no menu entry selects (the opposite channels negated).
enum class SoundMode : std::int32_t { mono = 0, stereo = 1, wide = 2, reverse_wide = 3 };
// 800386c4 up to its libspu call: select the mode bits (any other value
// selects mono) and reapply the volumes. Program::set_sound_mode is the whole
// original function.
void select_sound_mode(SoundDriver &driver, std::int32_t mode);
// 800387c4..8003880c: recompute the output of the mode voice (80059518), if
// one is active: full level on the left only in a stereo mode, half level on
// both sides in mono.
void update_mode_voice(SoundDriver &driver);
// 80038824: 0 mono, 1 stereo, 2 either negating mode.
[[nodiscard]] std::uint32_t sound_mode(const SoundDriver &driver);
// 80038c68: master volume `volume`, at once (frames 0) or as a fade.
void set_master_volume(SoundDriver &driver, std::uint32_t volume, std::uint32_t frames);
// 80038d18: CD input volume, at once or as a fade. The mode does not apply.
void set_cd_volume(SoundDriver &driver, std::uint32_t volume, std::uint32_t frames);
// 80038df4: recompute the master, CD and reverb pairs for the current mode
// and mark them for the tick's commit.
void reapply_volumes(SoundDriver &driver);
// 8003e680: or `bits` into the change flags (+2) of each sounding voice.
void mark_sequence_voices(SoundDriver &driver, std::uint32_t sequence, std::uint32_t bits);
// 80038748..80038770: 8003e680 on every sequence of the sequence list.
void mark_sequences(SoundDriver &driver, std::uint32_t bits);
// 8003ebf0: apply the changes each sounding voice of `sequence` flags in +2:
// 100 its left/right output (the pan law of the current mode), 200 its pitch,
// 1 a key-on claim of its hardware voice, 2 a release request. `voices` is
// the first voice record, `count` the caller's voice count.
void update_voices(SoundDriver &driver, std::uint32_t sequence, std::uint32_t voices,
                   std::uint32_t count);

} // namespace xem::reconstruction::resident
