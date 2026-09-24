// An invented battle memory image exercises action commit bookkeeping, the
// damage cap and explicit unsupported paths. It describes no original content.
#include "xem/reconstruction/battle.hpp"

#include <array>
#include <iostream>
#include <string_view>

namespace battle = xem::reconstruction::battle;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call>
void rejects(Call call, const char *message, std::string_view reason = {}) {
    bool rejected = false;
    try {
        call();
    } catch (const battle::BattleError &error) {
        rejected = std::string_view(error.what()).find(reason) != std::string_view::npos;
    }
    check(rejected, message);
}

constexpr std::uint32_t base = 0x800ccce8;
constexpr std::uint32_t turn = 0x80100000;

// Party slot 0 attacks enemy slot 3 with command 1 (descriptor power 20).
battle::BattleMemory sample() {
    battle::BattleMemory memory;
    memory.regions[battle::overlay_base].resize(battle::overlay_end - battle::overlay_base);
    memory.regions[turn].resize(0x300);
    memory.put32(battle::record_base_pointer, base);
    memory.put32(0x800c3eac, turn);
    memory.put8(turn + 0x2dc, 2); // Command 2 - 1
    memory.put32(0x800c348c, 0x80094ee4);
    const auto descriptor = base + 0x1058 + 1 * 0x28;
    memory.put8(descriptor + 0x11, 20);
    memory.put8(descriptor + 0x15, 100); // Accuracy bonus
    const auto attacker = base;
    memory.put8(attacker + 0x58, 200); // Attack
    memory.put8(attacker + 0x5e, 100); // Accuracy
    memory.put8(attacker + 0x55, 1);
    const auto target = base + 3 * battle::record_stride;
    memory.put16(target + 0x4c, 500);
    memory.put16(target + 0x4e, 500);
    return memory;
}

void commit() {
    auto memory = sample();
    std::uint32_t seed = 12345;
    battle::Battle context{memory, seed};
    battle::commit_action(context, 0, 1U << 3U, 6);
    const auto amount = memory.u32(base + 0x5f6c + 3 * 4);
    check(seed != 12345, "Resolving an attack consumes the resident rand state");
    check(memory.u8(0x800d2ca9) == 0 && memory.u8(0x800d2caa) == 1 && memory.u16(0x800d2c94) == 8 &&
              memory.u16(0x800d2c98) == 1,
          "The commit records attacker, command and targets; the resolver replaces the "
          "animation word (base + 5fb0) with the shown command");
    check(memory.u8(base + 0x5fa0 + 3) != 0xff && memory.u8(base + 0x5fa0 + 4) == 0xff &&
              memory.u8(base + 0x5fa0 + 11) == 0xff,
          "Only the target's result is written; other slots stay untouched");
    check(amount <= 9999, "Damage is capped at 9999");
    check(memory.u16(base + 0x90 + 1 * 2) == 1, "The party command usage counter advances");
    check(memory.u32(battle::attacker_pointer) == base &&
              memory.u32(battle::target_pointer) == base + 3 * battle::record_stride,
          "The resolver publishes attacker and target records");

    auto gear = sample();
    gear.put8(base + 0x15a, 0x80);
    battle::Battle geared{gear, seed};
    rejects([&] { battle::commit_action(geared, 0, 8, 6); },
            "The gear resolver is an explicit dependency");
    auto other = sample();
    other.put8(base + 0x1058 + 0x28 + 0x16, 1);
    battle::Battle typed{other, seed};
    rejects([&] { battle::commit_action(typed, 0, 8, 6); },
            "Formula types other than 0 are explicit dependencies");
    battle::BattleMemory empty;
    rejects([&] { static_cast<void>(empty.u8(0x80000000)); },
            "Reads outside the owned battle regions are rejected");
}

// Party slot 0 and enemy slot 3 present; event slot 0 carries 20 damage for
// the enemy (code 0) and 5 damage for the party member (code 0).
void knockout_and_victory() {
    auto memory = sample();
    for (std::uint32_t slot = 0; slot < battle::combatant_slots; ++slot)
        memory.put16(0x800c3448 + slot * 2, 1U << slot);
    memory.put8(0x800d2dcc, 1);
    memory.put8(0x800d2dcc + 3, 1);
    constexpr std::uint32_t event = 0x800c3fe8;
    for (std::uint32_t slot = 0; slot < battle::combatant_slots; ++slot)
        memory.put8(event + 0x18 + slot, 4); // No effect
    memory.put8(event + 0x18, 0);
    memory.put16(event, 5);
    memory.put8(event + 0x18 + 3, 0);
    memory.put16(event + 3 * 2, 20);
    const auto party = 0x800ccce8U;
    const auto enemy = 0x800ccce8U + 3 * battle::record_stride;
    memory.put16(party + 0x4c, 30);
    memory.put16(enemy + 0x4c, 15);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    battle::apply_results(context, 0);
    check(memory.u16(party + 0x4c) == 25 && (memory.u16(party + 0x7c) & 0x8000) == 0,
          "Damage below the remaining HP subtracts");
    check(memory.u16(enemy + 0x4c) == 0 && (memory.u16(enemy + 0x7c) & 0x8000) != 0 &&
              memory.u16(0x800c48e8) == 8,
          "Damage reaching zero knocks out and records the slot");
    battle::update_alive(context);
    check(memory.u16(0x800d39dc) == 1 && memory.u8(0x800c48ea) == 1, "No enemy alive is a victory");
    memory.put16(party + 0x7c, 0x8000);
    memory.put8(0x800c48ea, 0);
    battle::update_alive(context);
    check(memory.u16(0x800d39dc) == 0 && memory.u8(0x800c48ea) == 0x81,
          "No party member alive is a defeat, which overrides victory");
}

// Enemy slots 3 and 4 are knocked out and present; slot 4 is excluded. The
// party has 9999990 gold; inventory list 0 already holds 98 of item 7.
void rewards() {
    auto memory = sample();
    memory.regions[0x8006d634].resize(0x2358);
    for (std::uint32_t enemy = 0; enemy < 8; ++enemy)
        memory.put16(0x800c3448 + enemy * 2, 1U << enemy);
    for (const std::uint32_t slot : {3U, 4U}) {
        const auto record = 0x800ccce8U + slot * battle::record_stride;
        memory.put8(0x800d2dcc + slot, 1);
        memory.put16(record + 0x7c, 0x8000);
        memory.put32(record + 0x14c, 100);
        memory.put16(record + 0x156, 30);
    }
    memory.put8(0x800c3d1b + 1 * 4, 1); // Enemy 1 (slot 4) excluded
    memory.put32(0x8006ef58, 9999990);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    battle::total_rewards(context);
    check(memory.u32(0x800d2c84) == 100 && memory.u16(0x800d2c9c) == 1,
          "Only counted enemies add experience and their defeat bit");
    check(memory.u32(0x8006ef58) == 9999999, "Gold is capped at 9999999");

    // Drops: item 7 x3 (list 0, stacks to the 99 cap), item 9 x2 (list 1, new
    // entry), a category-5 entry (ignored).
    constexpr std::uint32_t ids = 0x800d2000, counts = 0x800d2008, categories = 0x800d2010;
    memory.put8(0x8006f3d0, 7);
    memory.put8(0x8006f36c, 98);
    memory.put8(0x8006f4fc, 4);
    memory.put8(0x8006f434, 1);
    for (const auto &[slot, id, count, category] :
         {std::array{0U, 7U, 3U, 0U}, std::array{1U, 9U, 2U, 1U}, std::array{2U, 5U, 1U, 5U}}) {
        memory.put8(ids + slot, id);
        memory.put8(counts + slot, count);
        memory.put8(categories + slot, category);
    }
    battle::add_drops(context, ids, counts, categories);
    check(memory.u8(0x8006f36c) == 99, "A stacked item is capped at 99");
    check(memory.u8(0x8006f4fd) == 9 && memory.u8(0x8006f435) == 2 && memory.u8(0x8006f4fc) == 4 &&
              memory.u8(0x8006f434) == 1,
          "A new item takes the first free entry of its category's list");
}

// Slots 0 and 3 present; slot 0 hastened (+84 bit 8000) with 2 left, slot 3
// frozen (+7c bit 80) with 1 left.
void timers() {
    auto memory = sample();
    memory.put8(0x800d3298, 1);
    memory.put8(0x800d2dcc, 1);
    memory.put8(0x800d2dcc + 3, 1);
    memory.put16(0x800ccce8 + 0x84, 0x8000);
    memory.put16(0x800d2e06, 2);
    memory.put16(0x800ccce8 + 3 * battle::record_stride + 0x7c, 0x80);
    memory.put16(0x800d2e06 + 3 * 2, 1);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    battle::atb_tick(context);
    check(memory.u8(0x800d2de4) == 1 && memory.u16(0x800d2e06) == 0,
          "A hastened slot steps by two and becomes ready at zero");
    check(memory.u8(0x800d2de4 + 3) == 0 && memory.u16(0x800d2e06 + 3 * 2) == 1,
          "A frozen slot's timer does not move");
    memory.put8(0x800d3298, 0);
    memory.put8(0x800d2de4, 0);
    memory.put16(0x800d2e06, 5);
    battle::atb_tick(context);
    check(memory.u16(0x800d2e06) == 5, "Timers hold while a turn runs (800d3298 clear)");
}

// Enemy 0 (slot 3, bit 8) runs an invented script: a rule whose condition
// fails, then an always-true rule that queues action 7 with the enemy's own
// bit as the second word.
void enemy_script() {
    auto memory = sample();
    memory.put16(0x800c3448 + 3 * 2, 8);
    memory.put8(0x800d2e5c + 0x10, 0x55); // Stale action list byte
    constexpr std::uint32_t script = 0x800d1000;
    memory.put32(0x800d3400, script);
    const std::array<std::uint8_t, 28> code{
        0x82, 0, 5, 0, // var[0] == 5 (false)
        0x01, 0, 9, 0, // list byte 0 = 9 (skipped)
        0x80, 0, 0, 0, // always
        0x64, 1, 0, 0, // var[1] = own bit
        0x52, 2, 1, 0, // list word 2 = var[1]
        0x01, 0, 7, 0, // list byte 0 = 7, next entry
        0xfd, 0, 0, 0,
    };
    for (std::uint32_t i = 0; i < code.size(); ++i)
        memory.put8(script + i, code[i]);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    battle::run_enemy_script(context, 3, 0);
    check(memory.u8(0x800d2e5c) == 7 && memory.u16(0x800d2e5c + 2) == 8,
          "The true rule queues the action and the enemy's own bit");
    check(memory.u8(0x800d2e5c + 0x10) == 0 && memory.u8(0x800c402f) == 0xff &&
              memory.u8(0x800c402f + 0x8b8) == 0xff,
          "The list is cleared and every event type reset before the script runs");
    memory.put8(script + 12, 0x02);
    rejects([&] { battle::run_enemy_script(context, 3, 0); },
            "Untranslated script actions are explicit dependencies",
            "action 0x02 (handler 8007a874) at 800d100c");
    memory.put8(script + 12, 0x00);
    rejects([&] { battle::run_enemy_script(context, 3, 0); },
            "Action 00 takes the dispatcher default", "action 0x00 (default 8007a7bc)");
    memory.put8(script + 8, 0x81);
    rejects([&] { battle::run_enemy_script(context, 3, 0); },
            "Untranslated conditions are explicit dependencies",
            "condition 0x81 (handler 8007e954)");
}
} // namespace

int main() {
    try {
        commit();
        knockout_and_victory();
        rewards();
        timers();
        enemy_script();
        std::cout << "Battle actions: five source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
