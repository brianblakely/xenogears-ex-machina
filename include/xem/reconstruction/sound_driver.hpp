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
    std::uint32_t voice_limit{};  // 80059478; 80039db8 starts effects at voice limit - 2
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
};

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

// 80039db8 with the argument 801c8574 builds: when effects are enabled, start
// effect `effect` of effect bank object `bank` (its id at +14) on two voices
// from voice_limit - 2 (code 8000 | voice) at volume 6000, pan 4000.
void start_bank_effect(SoundDriver &driver, std::uint32_t bank, std::uint32_t effect);

// 80039c4c: stop a sequence (clear flag 8000) and release its voices.
void stop_sequence(SoundDriver &driver, std::uint32_t sequence);
// 80038310: unlink a wave bank, free its SPU block (800396e0) and its pool
// block.
void release_wave_bank(SoundDriver &driver, std::uint32_t wave);
// 800399d4: stop a playing sequence, unlink it, free its child blocks and,
// unless flag 4000 marks it static, free its pool block.
void release_sequence(SoundDriver &driver, std::uint32_t sequence);

} // namespace xem::reconstruction::resident
