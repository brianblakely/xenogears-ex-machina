// An invented battle memory image exercises action commit bookkeeping, the
// damage cap and explicit unsupported paths. It describes no original content.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

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

// Turn order [0, 3, 1, 2, 4..10] from cursor 0 with slot 3 ready; party slot 0
// then attacks enemy slot 3 through an invented action list.
void turns() {
    auto memory = sample();
    memory.regions[0x80101000].resize(0x10c);
    memory.put32(0x800d2d28, 0x80101000);
    for (std::uint32_t slot = 0; slot < battle::combatant_slots; ++slot) {
        memory.put16(0x800c3448 + slot * 2, 1U << slot);
        memory.put8(0x800d2dd8 + slot, slot);
    }
    memory.put8(0x800d2dd8 + 1, 3);
    memory.put8(0x800d2dd8 + 3, 1);
    memory.put8(0x800d2de4 + 3, 1);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    check(battle::select_turn(context) && memory.u8(turn + 0x2d3) == 4 &&
              memory.u8(0x800d2dd7) == 2,
          "The first ready slot from the cursor acts (as slot + 1); the cursor passes it");
    memory.put8(0x800d2de4 + 3, 0);
    check(!battle::select_turn(context) && memory.u8(turn + 0x2d3) == 0 &&
              memory.u8(0x800d2dd7) == 2,
          "Without a ready slot nobody acts and the cursor stays");
    memory.put8(0x800d2dc0, 2);
    memory.put16(0x800d2e06 + 2, 9);
    check(battle::select_turn(context) && memory.u8(turn + 0x2d3) == 2 &&
              memory.u8(0x800d2de4 + 1) == 1 && memory.u16(0x800d2e06 + 2) == 0 &&
              memory.u8(0x800d2dc0) == 0,
          "A forced slot acts first and is marked ready with its timer cleared");
    memory.put16(0x800d39e0, 8);
    rejects([&] { static_cast<void>(battle::select_turn(context)); },
            "The all-enemies pass is an explicit dependency", "80072324");
    memory.put16(0x800d39e0, 0);

    const auto record = 0x800ccce8U + 1 * battle::record_stride;
    memory.put16(record + 0x7c, 0x1000);
    memory.put8(record + 0x15d, 1);
    memory.put16(record + 0x84, 0x8000);
    memory.put8(record + 0x160, 2);
    battle::begin_turn(context);
    check(memory.u8(turn + 0x2d3) == 1 && memory.u8(0x800d3298) == 0,
          "The turn takes the slot and holds the ATB");
    check(memory.u16(record + 0x7c) == 0 && memory.u16(record + 0x84) == 0x8000 &&
              memory.u8(record + 0x160) == 1,
          "Timed statuses count down and clear at zero");

    // Party slot 0: an f7 event, then an attack on slot 3 (animation 6).
    memory.put8(turn + 0x2dc, 1);
    memory.put8(0x800d2e5c, 0xe);
    memory.put8(0x800d2e5c + 4, 9);
    memory.put8(0x800d2e5c + 8, 1);
    memory.put8(0x800d2e5c + 8 + 1, 1);
    memory.put8(0x800d2e5c + 8 + 2, 6);
    memory.put16(0x800d2e5c + 8 + 6, 8);
    battle::begin_actions(context, 0);
    std::uint32_t framed = 0;
    battle::execute_actions(context, 0, [&](std::uint32_t mask) { framed = mask; });
    constexpr std::uint32_t events = 0x800c3fe8;
    check(memory.u8(events + 0x47) == 0xf7 && memory.u16(events + 0x3a) == 9 &&
              memory.u8(events + 0x48 + 0x47) == 6 && memory.u16(events + 0x48 + 0x16) == 8 &&
              memory.u8(events + 2 * 0x48 + 0x47) == 0xfe && memory.u8(turn + 0x2da) == 2,
          "Actions queue their events; the queue closes with fe");
    check(memory.u8(0x800d2ca9) == 0 && memory.u16(0x800d2c94) == 8 && framed == 0,
          "The attack entry commits against its target mask");
    memory.put8(0x800d2e5c + 8, 3);
    rejects([&] { battle::execute_actions(context, 0, [](std::uint32_t) {}); },
            "Untranslated action types are explicit dependencies", "type 3");

    memory.put8(0x800d2dcc, 1);
    memory.put8(0x800d2dcc + 3, 1);
    memory.put16(0x800ccce8 + 0x7c, 0x800);
    memory.put8(turn + 0x2d3, 0);
    rejects([&] { battle::settle_turn(context); },
            "End-of-turn regeneration is an explicit dependency", "80085b58");
}

// Member 0 moves from page 1 to page 3 with an invented queued button, defends
// there, and tries to escape from page 9 under two rand states.
void menu() {
    auto memory = sample();
    memory.regions[0x8006d634].resize(0x2358);
    xem::reconstruction::ResidentState resident;
    resident.pad.buffers[0][1] = 0x41;
    resident.debug_pointer = 0x80010000;
    resident.debug_word = 0xffffffff;
    auto &queue = resident.input_queue;
    queue.ring[4][3] = 0x8000;
    queue.read = 3;
    queue.count = 1;
    memory.put8(0x800c3e29, 1);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    battle::decode_input(context, resident);
    check(memory.u8(0x800d3014) == 2 && memory.u8(0x800c3e29) == 2 && memory.u8(0x800c3e28) == 1 &&
              queue.count == 0 && queue.read == 4,
          "A queued button decodes to its face code and becomes the latest face button");
    battle::decode_input(context, resident);
    check(memory.u8(0x800d3014) == 8, "An empty queue decodes to 8");
    queue.overflow = 1;
    queue.count = 1;
    battle::decode_input(context, resident);
    check(memory.u8(0x800d3014) == 8 && queue.count == 0 && queue.w50200 == 1 &&
              queue.ring[4][3] == 0x8000,
          "An overflowed queue is reset (keeping the ring) and decodes to 8");
    resident.pad.buffers[0][0] = 0xff;
    rejects([&] { battle::decode_input(context, resident); },
            "The missing-controller wait is an explicit dependency", "missing-controller");
    resident.pad.buffers[0][0] = 0;

    memory.put8(turn + 0x2dd, 1);
    memory.put8(0x800d3014, 2);
    battle::menu_step(context, resident, 0);
    check(memory.u8(turn + 0x2dd) == 3, "Code 2 on page 1 moves to page 3");
    memory.put8(0x800d3014, 4);
    battle::menu_step(context, resident, 0);
    check((memory.u8(base + 0x15a) & 1) != 0 && memory.u8(turn + 0x2de) == 1 &&
              memory.u8(base + 0x5fc2) == 0,
          "Confirming page 3 defends and ends the menu");

    bool escaped = false, stayed = false;
    for (std::uint32_t start = 1; start < 64 && !(escaped && stayed); ++start) {
        seed = start;
        memory.put8(0x800c48ea, 0);
        memory.put8(turn + 0x2dd, 9);
        memory.put8(0x800d2d24 + 1, 0x7f);
        memory.put8(0x800d2d24 + 2, 0x7f);
        memory.put16(base + 0x4c, 33);
        memory.put16(0x8006d8a0 + 0x4e, 999);
        battle::menu_step(context, resident, 0);
        if (memory.u8(0x800c48ea) == 0x40) {
            escaped = true;
            check(memory.u16(0x8006d8a0 + 0x4c) == 33, "A successful escape writes the party back");
        } else {
            stayed = true;
            check(memory.u8(0x800c48ea) == 0, "A failed escape leaves the outcome");
        }
    }
    check(escaped && stayed, "Escape succeeds for some rand states and fails for others");
    memory.put8(turn + 0x2dd, 2);
    rejects([&] { battle::menu_step(context, resident, 0); },
            "Unreconstructed pages are explicit dependencies", "0x2");
}

// Member 0 on an invented attack page: candidates 3 (right of it) and 4
// (above it) around slot 0; a one-part glyph sprite; heap-free paths only.
void attack() {
    auto memory = sample();
    xem::reconstruction::ResidentState resident;
    resident.math.angle.assign(1025, 0); // invented: every ratio maps to angle 0
    constexpr std::uint32_t info = 0x800c3eb4;
    const auto place = [&](std::uint32_t slot, std::uint32_t x, std::uint32_t y) {
        memory.put16(info + slot * 0x1c + 0xa, x);
        memory.put16(info + slot * 0x1c + 0xc, y);
    };
    for (std::uint32_t i = 0; i < 11; ++i)
        memory.put8(0x800c3e90 + i, 0xff);
    memory.put8(0x800c3e90, 3);
    memory.put8(0x800c3e90 + 1, 4);
    memory.put8(0x800c3e90 + 2, 5);
    place(0, 100, 100);
    place(3, 150, 100);
    place(4, 100, 50);
    place(5, 180, 100);
    std::uint32_t seed = 1;
    battle::Battle context{memory, seed};
    check(battle::direction_target(context, resident, 0, 0) == 3,
          "Direction 0 takes the nearest candidate at angle 0");
    check(battle::direction_target(context, resident, 0, 1) == 4,
          "Direction 1 takes the candidate at angle -400");
    check(battle::direction_target(context, resident, 0, 2) == 0,
          "A direction without candidates keeps the origin");

    // Glyph table at 80101000: sprite 1 has one part 8x16 at (2, 3), uv (5, 6).
    constexpr std::uint32_t table = 0x80101000;
    memory.regions[table].resize(0x100);
    memory.regions[0x80102000].resize(0x100);
    memory.put32(0x800d2f5c, table);
    memory.put32(0x800ccb34, 1);
    memory.put16(table + 4 + 2, 0x10);
    memory.put16(table + 0x10, 1);
    const auto part = table + 0x14;
    memory.put16(part, 5);
    memory.put16(part + 2, 6);
    memory.put16(part + 4, 8);
    memory.put16(part + 6, 16);
    memory.put16(part + 8, 2);
    memory.put16(part + 10, 3);
    memory.put16(part + 16, 2);     // texture page mode
    memory.put16(part + 18, 0x20);  // CLUT x
    memory.put16(part + 20, 0x1f0); // CLUT y
    memory.put16(part + 22, 0x340); // page x
    memory.put16(part + 24, 0x100); // page y
    check(battle::draw_glyph(context, 1, 0x80102000, 40, 50) == 1, "A glyph returns its parts");
    const auto prim = 0x80102000 + 0x28U;
    check(memory.u8(prim + 3) == 9 && memory.u8(prim + 7) == 0x2d && memory.u16(prim + 8) == 42 &&
              memory.u16(prim + 16) == 50 && memory.u16(prim + 10) == 53 &&
              memory.u16(prim + 26) == 69 && memory.u8(prim + 12) == 5 &&
              memory.u8(prim + 20) == 13 && memory.u8(prim + 29) == 22 &&
              memory.u16(prim + 22) == (0x100U | 0x10U | 0xdU) &&
              memory.u16(prim + 14) == (0x1f0U << 6 | 2U),
          "A glyph part becomes a textured quad in the draw buffer's half");

    // Page 0x64 while the command is closed stops at once.
    memory.put8(turn + 0x2dd, 0x64);
    memory.put8(turn + 0x2e1, 1);
    memory.put8(0x800d366c, 1);
    battle::menu_step(context, resident, 0, [](battle::MenuPresentation, std::uint32_t) {
        throw std::runtime_error("no presentation expected");
    });
    check(memory.u8(turn + 0x2e2) == 0xff && memory.u8(0x800d366c) == 0,
          "Page 0x64 resets the shown page and stops while the command is closed");
    memory.put8(turn + 0x2dd, 5);
    memory.put8(turn + 0x2e1, 0);
    rejects([&] { battle::menu_step(context, resident, 0); },
            "Attack pages without presentation brackets are explicit dependencies",
            "presentation bracket");

    // Result screens: a Cross wait repeats until Cross, then clears the flags.
    memory.regions[0x80103000].resize(0x100);
    memory.put32(0x800d2d28, 0x80103000);
    memory.put8(0x800d3014, 8);
    check(battle::result_screen_step(context, resident, 0x801e1f24, 0) == 0x801e1f24,
          "Without Cross the gold screen waits another frame");
    memory.put8(0x80103000 + 0xb1, 1);
    memory.put8(0x800d3014, 4);
    check(battle::result_screen_step(context, resident, 0x801e1f24, 0) == 0x801e1f88 &&
              memory.u8(0x80103000 + 0xb1) == 0,
          "Cross closes the gold screen's flags");
    check(battle::result_screen_step(context, resident, 0x801e1f88, 0) == 0x8008fa98,
          "The gold screen then closes window 0 over a frame");
    rejects(
        [&] { static_cast<void>(battle::result_screen_step(context, resident, 0x801e1dac, 1)); },
        "Screen contents are explicit dependencies", "learned skills");
}

// The resident battle epilogue 8001b758.
void epilogue() {
    xem::reconstruction::Program program;
    auto &resident = program.resident;
    resident.game_state = 0x8006d634;
    resident.game_data.assign(xem::reconstruction::game_data_bytes, 0);
    const auto selector = 0x8006f94e - 0x8006d634;
    resident.game_data[selector] = 0x00;
    resident.game_data[selector + 1] = 0x05; // 500: at least 400
    resident.mode_loaded = 3;
    program.finish_battle_mode(1, 0);
    check(resident.next_mode == 3 && resident.battle_request.resident_flag == 1,
          "A victory on a selector of 400 or more selects mode 3");
    program.finish_battle_mode(0x40, 1);
    check(resident.next_mode == 6, "An escape with 800d3338 set selects mode 6");
    resident.w_4f30c = 7;
    resident.game_data[selector + 4] = 9;
    program.finish_battle_mode(0x81, 0);
    check(resident.next_mode == 1 && resident.w_4f30c == 0 &&
              resident.game_data[selector] == 0xea && resident.game_data[selector + 1] == 1 &&
              resident.game_data[selector + 4] == 0,
          "A defeat selects mode 1 with map selector 1ea");
    resident.battle_request.resident_flag = 0;
    resident.b_5947c = 1;
    program.finish_battle_mode(1, 0);
    check(resident.next_mode == 2 && resident.battle_request.resident_flag == 0,
          "With 8005947c set a victory selects mode 2 and leaves 800594f8");
}
} // namespace

int main() {
    try {
        commit();
        knockout_and_victory();
        rewards();
        timers();
        enemy_script();
        turns();
        menu();
        attack();
        epilogue();
        std::cout << "Battle actions: nine source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
