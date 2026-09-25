#pragma once

#include "xem/reconstruction/sound_driver.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

// Item and equipment actions of the main menu overlay (sha256 82f84a24...,
// loaded at 801c5000).
namespace xem::reconstruction::menu {

class MenuError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Menu-mode memory: the heap block holding the overlay image and its static
// data (801c5000..801f34a0 on the observed route), the menu state pointer word
// 800625a0, the heap blocks the actions reach through it (menu state, party
// list, data table directory, each data table and the equipment screen state)
// and, for the duration of a call, the persistent game data (8006d634, 2358
// bytes). Each region is owned whole and addressed by original address, as the
// original code addresses it.
struct MenuMemory {
    std::map<std::uint32_t, std::vector<std::uint8_t>> regions;
    // While the overlay runs (menu_overlay.hpp): the stack below its entry
    // SP, [stack_base, stack_base + stack.size()), where callee frames keep
    // their locals. Transient machine memory, never exported as state.
    std::uint32_t stack_base{};
    std::vector<std::uint8_t> stack;

    [[nodiscard]] std::uint32_t u8(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u16(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u32(std::uint32_t address) const;
    void put8(std::uint32_t address, std::uint32_t value);
    void put16(std::uint32_t address, std::uint32_t value);
    void put32(std::uint32_t address, std::uint32_t value);
    // `size` owned bytes at `address`, within one region.
    [[nodiscard]] std::span<std::uint8_t> bytes(std::uint32_t address, std::uint32_t size);
    [[nodiscard]] std::span<const std::uint8_t> bytes(std::uint32_t address,
                                                      std::uint32_t size) const;
    // The owned bytes from `address` to the end of its region.
    [[nodiscard]] std::span<const std::uint8_t> tail(std::uint32_t address) const;
};

inline constexpr std::uint32_t overlay_base = 0x801c5000;
inline constexpr std::uint32_t state_pointer = 0x800625a0;

// Menu state offsets.
inline constexpr std::uint32_t state_effect_bank = 0x2e4;  // sound effect bank object
inline constexpr std::uint32_t state_sound = 0x32a;        // u8: menu sounds enabled
inline constexpr std::uint32_t state_tables = 0x330;       // data table directory
inline constexpr std::uint32_t state_party = 0x33c;        // party list; +30: 3 ids, ff empty
inline constexpr std::uint32_t state_equip_screen = 0x360; // equipment screen state

// Data table directory (*(state + 330)): 16-byte entries of the weapon (+0),
// accessory (+4) and consumable (+1c) tables; the equipment screen's shown
// stats are halfwords from +b8.
inline constexpr std::uint32_t weapon_table = 0x0;
inline constexpr std::uint32_t accessory_table = 0x4;
inline constexpr std::uint32_t item_table = 0x1c;

// Character records in the game data: a4 bytes each from 8006d8a0.
inline constexpr std::uint32_t character_records = 0x8006d8a0;
inline constexpr std::uint32_t character_stride = 0xa4;
// Consumable inventory: 150 counts, then 150 item ids.
inline constexpr std::uint32_t item_counts = 0x8006f5c4;
inline constexpr std::uint32_t item_ids = 0x8006f65a;

// A menu computation over menu memory and the resident sound driver.
struct Menu {
    MenuMemory &memory;
    resident::SoundDriver &sound;
    // Called as 801c8574 (the menu sound) is entered; hosts that order
    // interrupt arrivals by it (menu::Overlay) set it.
    std::function<void()> entering_sound{};

    [[nodiscard]] std::uint32_t state() const { return memory.u32(state_pointer); }
    [[nodiscard]] std::uint32_t tables() const { return memory.u32(state() + state_tables); }
    // Character id in party slot `slot` (ff when empty).
    [[nodiscard]] std::uint32_t party_member(std::uint32_t slot) const {
        return memory.u8(memory.u32(state() + state_party) + 0x30 + slot);
    }
};

[[nodiscard]] inline constexpr std::uint32_t character_record(std::uint32_t character) {
    return character_records + (character & 0xff) * character_stride;
}

// 801e31c0: apply consumable `item` to `character` (HP, EP, permanent stat
// and maximum raises, the +78 counter). Returns nonzero when an HP or EP item
// found nothing to restore. `tables` is the data table directory.
std::uint32_t apply_item_effect(Menu &menu, std::uint32_t tables, std::uint32_t character,
                                std::uint32_t item);
// 801db920 from 801dbba4 to 801dbc90: the confirmed use of inventory entry
// `index` on the party slots in `targets`: apply the effect to each target
// (801c865c selects them), play the result sound (801c8574) and, when any
// target took the item, consume one.
void use_item(Menu &menu, std::uint32_t index, std::uint32_t targets);

// 801df0d4: commit the equipment change of party slot `slot`, part `part` (0
// weapon, 1-3 accessories). The equipment screen previews its selection in the
// character record and keeps the replaced part at screen state + 29c/2a5/2a1;
// the inventory takes the replaced part and gives up the selected one. `gear`
// selects the gear records and lists; `special` the +2a1 parts. Returns 1 when
// character 4's weapon was replaced.
std::uint32_t swap_equipment(Menu &menu, std::uint32_t slot, std::uint32_t part,
                             std::uint32_t special, std::uint32_t gear);
// 801e36d4: rebuild `character`'s equipment bonuses and weapon values from the
// weapon and accessory tables of directory `tables`.
void equipment_bonuses(Menu &menu, std::uint32_t tables, std::uint32_t character);
// 801e3a80: the equipment screen's shown stats (`tables` + b8..c4) for
// `character`.
void equipment_stats(Menu &menu, std::uint32_t tables, std::uint32_t character);

} // namespace xem::reconstruction::menu
