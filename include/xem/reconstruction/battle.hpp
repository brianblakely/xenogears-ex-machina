#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace xem::reconstruction::battle {

class BattleError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Battle-mode memory: the battle overlay (sha256 1830b4ef...) image loaded at
// 8006faf0 with its static data up to 800d39f0 (resident mode table row
// 800180ac), plus the heap blocks battle setup allocates. Each region is owned
// whole and addressed by original address, as the original code addresses it.
struct BattleMemory {
    std::map<std::uint32_t, std::vector<std::uint8_t>> regions;

    [[nodiscard]] std::uint32_t u8(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u16(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u32(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s8(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s16(std::uint32_t address) const;
    void put8(std::uint32_t address, std::uint32_t value);
    void put16(std::uint32_t address, std::uint32_t value);
    void put32(std::uint32_t address, std::uint32_t value);
};

inline constexpr std::uint32_t overlay_base = 0x8006faf0;
inline constexpr std::uint32_t overlay_end = 0x800d39f0;

// Combatant records: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes from the
// base pointer at 800c34b0.
inline constexpr std::uint32_t record_base_pointer = 0x800c34b0;
inline constexpr std::uint32_t record_stride = 0x170;
// Heap block of turn and menu state allocated by battle setup.
inline constexpr std::uint32_t turn_state_pointer = 0x800c3eac;
inline constexpr std::uint32_t combatant_slots = 11;

// Resolver globals.
inline constexpr std::uint32_t descriptor_pointer = 0x800c3dfc;     // current command descriptor
inline constexpr std::uint32_t attacker_pointer = 0x800c3e00;       // attacker record
inline constexpr std::uint32_t attacker_slot = 0x800c3e04;          // u8
inline constexpr std::uint32_t target_pointer = 0x800c3e34;         // target record
inline constexpr std::uint32_t target_slot = 0x800c3e50;            // u8
inline constexpr std::uint32_t target_tail_pointer = 0x800c3d60;    // target record + 148
inline constexpr std::uint32_t attacker_block_pointer = 0x800d2d6c; // attacker record + a4
inline constexpr std::uint32_t target_block_pointer = 0x800d2dc8;   // target record + a4

// A battle computation over battle memory and the resident rand state
// (8005a1fc).
struct Battle {
    BattleMemory &memory;
    std::uint32_t &seed;

    [[nodiscard]] std::uint32_t record_base() const { return memory.u32(record_base_pointer); }
    [[nodiscard]] std::uint32_t record(std::uint32_t slot) const {
        return record_base() + slot * record_stride;
    }
    // Resident rand 8003fa38: 0..7fff.
    std::uint32_t rand();
};

// 80085ccc: commit an action (attacker slot, target slot mask, animation)
// and resolve it (800941a4).
void commit_action(Battle &battle, std::uint32_t attacker, std::uint32_t targets,
                   std::uint32_t animation);

// Formula functions of the table at 800c348c, called with the resolver
// globals set for one target.
void physical_formula(Battle &battle); // 80094ee4 (type 0)

// Per-target steps the resolver calls around the formula.
void post_adjust(Battle &battle);         // 800946f4
void counter_check(Battle &battle);       // 800968c0
void ether_check(Battle &battle);         // 80096824
void status_effect_958d8(Battle &battle); // 800958d8
void status_effect_95a78(Battle &battle); // 80095a78
void status_effect_95b44(Battle &battle); // 80095b44

// 80085618: apply queued results (event slot `queue & 0xff`, 0x48 bytes from
// 800c3fe8: u16 amounts, u8 codes at +0x18) to HP, EP, gear HP and record
// +0xdc, marking knockouts.
void apply_results(Battle &battle, std::uint32_t queue);
// 8007252c: rebuild the alive mask (800d39dc) and set the battle outcome
// (800c48ea) when either side is defeated.
void update_alive(Battle &battle);

// 8007171c: one ATB tick for every present slot that is not ready.
void atb_tick(Battle &battle);
// 800718bc: reload the acting slot's turn timer (80098af8).
void reload_turn_timer(Battle &battle);

// Post-battle module (directory 10 file 4, sha256 f474fd48..., loaded at
// 801de000) and the persistent game data (8006d634, 2358 bytes) are
// addressable through the same memory during victory processing.
inline constexpr std::uint32_t result_module_base = 0x801de000;
inline constexpr std::uint32_t result_module_bytes = 0x6500;

// 801e2794: victory rewards: experience split and level-ups, skills, flags,
// write-back to persistent party state and drop rolls.
void grant_rewards(Battle &battle);
// 801e2280 up to its UI setup call at 801e23d4: the experience pool
// (800d2c84), defeated mask (800d2c9c) and gold (8006ef58, capped at 9999999)
// from knocked-out enemies.
void total_rewards(Battle &battle);
// 801e1444: add up to 8 drops (id, count and category arrays) to the
// inventory lists (801e1370).
void add_drops(Battle &battle, std::uint32_t ids, std::uint32_t counts, std::uint32_t categories);
// 801e2acc: split the experience pool and apply level-ups (801e308c).
void distribute_experience(Battle &battle);

} // namespace xem::reconstruction::battle
