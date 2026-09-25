// The victory result screens of the post-battle module (801de000): the steps
// between frames that wait for acknowledgments and close the windows.
// Presentation (the screens' text, windows, sounds and loading) is not
// reconstructed; a step that reaches it stops with a BattleError.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

#include <cstdint>
#include <format>
#include <utility>

namespace xem::reconstruction::battle {
namespace {

constexpr std::uint32_t command_code = 0x800d3014; // u8: decoded input, 4 = Cross
constexpr std::uint32_t ui_state_pointer = 0x800d2d28;
constexpr std::uint32_t window_blocks = 0x800d2e38;  // pointer per window
constexpr std::uint32_t window_records = 0x800d2d90; // pointer per window

std::uint32_t ui(const Battle &battle) { return battle.memory.u32(ui_state_pointer); }
void set_ui(Battle &battle, std::uint32_t offset, std::uint32_t value) {
    battle.memory.put8(ui(battle) + offset, value);
}
bool acknowledged(const Battle &battle) { return battle.memory.u8(command_code) == 4; }

// 800320e8 on a block battle memory owns.
void release_block(Battle &battle, ResidentState &resident, std::uint32_t address,
                   std::uint32_t call_site) {
    resident::HeapBlock block{address, {}};
    const auto found = battle.memory.regions.find(address);
    if (found == battle.memory.regions.end())
        throw BattleError(std::format("Released block {:08x} is not battle memory", address));
    block.bytes = std::move(found->second);
    battle.memory.regions.erase(found);
    if (resident::heap_release(resident.heap, block, call_site) == -1)
        battle.memory.regions.emplace(address, std::move(block.bytes)); // kept
}

// 8008fa60(window) up to its frame: the window stops showing (+b0, +b8).
std::uint32_t close_window(Battle &battle, std::uint32_t window) {
    set_ui(battle, 0xb0 + window, 0);
    set_ui(battle, 0xb8 + window, 0);
    return 0x8008fa98;
}

[[noreturn]] void presentation(std::uint32_t site, const char *what) {
    throw BattleError(std::format("The result screens' {} after frame {:08x} are not "
                                  "reconstructed",
                                  what, site));
}

} // namespace

std::uint32_t result_screen_step(Battle &battle, ResidentState &resident, std::uint32_t site,
                                 std::uint32_t window) {
    switch (site) {
    case 0x801e1a64: // 801e196c: the summary waits for Cross
        if (!acknowledged(battle))
            return 0x801e1a64;
        set_ui(battle, 0xcf, 0);
        return 0x801e1ab4; // 801e1aa4's first frame
    case 0x801e1c40:       // 801e1c10 after its first frame
        set_ui(battle, 0xcf, 1);
        if (!acknowledged(battle))
            return 0x801e1c70;
        set_ui(battle, 0xcf, 0);
        return 0x801e1c9c;
    case 0x801e1c70: // the experience screen waits for Cross
        if (!acknowledged(battle))
            return 0x801e1c70;
        set_ui(battle, 0xcf, 0);
        return 0x801e1c9c;
    case 0x801e1d4c: // a member's level-up screen is shown
        set_ui(battle, 0xcf, 1);
        if (!acknowledged(battle))
            return 0x801e1d88;
        set_ui(battle, 0xa1, 0);
        return 0x801e1dac;
    case 0x801e1d88: // the level-up screen waits for Cross
        if (!acknowledged(battle))
            return 0x801e1d88;
        set_ui(battle, 0xa1, 0);
        return 0x801e1dac;
    case 0x801e1f24: // the gold and items screen waits for Cross
        if (!acknowledged(battle))
            return 0x801e1f24;
        for (const auto offset : {0xcfU, 0xacU, 0xb0U, 0xb1U, 0xb2U})
            set_ui(battle, offset, 0);
        return 0x801e1f88;
    case 0x801e1f88: // 801e1e10 closes windows 0, 1 and 2
        return close_window(battle, 0);
    case 0x8008fa98: // 8008fa60 after its frame: release the window's blocks
        window &= 0xff;
        release_block(battle, resident, battle.memory.u32(window_blocks + window * 4), 0x8008faa8);
        release_block(battle, resident, battle.memory.u32(window_records + window * 4), 0x8008fabc);
        if (window == 0 || window == 1)
            return close_window(battle, window + 1);
        if (window == 2) { // 801e1fb8 after 801e1e10
            for (const auto offset : {0xa0U, 0xa1U, 0x8fU})
                set_ui(battle, offset, 0);
            return 0x801e20c0;
        }
        if (window == 5) // 801e252c closes window 5, then 4
            return close_window(battle, 4);
        presentation(site, "post-battle release after window 4 (801e252c)");
    case 0x801e20c0:
        presentation(site, "block releases and sound reset 80039ff8");
    case 0x801e1ab4:
    case 0x801e1b18:
        presentation(site, "damage count (sound 80039e60, 801df270, 801df4c0)");
    case 0x801e1c9c:
        presentation(site, "level-up windows (8008f8f4, 801dfaa8, 801e03b8, 801e03fc)");
    case 0x801e1dac:
        presentation(site, "learned skills (801e0acc)");
    default:
        presentation(site, "steps");
    }
}

} // namespace xem::reconstruction::battle
