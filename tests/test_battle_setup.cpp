// Invented battle memory exercises the battle's start: the prologue, setup
// phases 1 and 2, the load prologue, the swirl's draw buffers, SetDispMask
// and explicit unsupported paths. It describes no original
// content.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_heap.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace battle = xem::reconstruction::battle;
namespace resident_heap = xem::reconstruction::resident;
using xem::reconstruction::Program;

namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call> void missing(Call call, std::string_view symbol, const char *message) {
    bool stopped = false;
    try {
        call();
    } catch (const xem::reconstruction::MissingDependency &error) {
        stopped = error.dependency.find(symbol) != std::string::npos;
    }
    check(stopped, message);
}

constexpr std::uint32_t graphics = 0x80110000;
constexpr std::uint32_t turn_state = 0x8011c000;
constexpr std::uint32_t scene = 0x8011d000;
constexpr std::uint32_t enemies = 0x8011e000;
constexpr std::uint32_t info = 0x800c3eb4; // 1ch per slot
constexpr std::uint32_t present = 0x800d2dcc;

// One free heap block 80140008..80160000.
constexpr std::uint32_t heap_first = 0x80140008;
void free_heap(Program &program) {
    auto &heap = program.resident.heap;
    heap.head = heap_first;
    heap.headers = {{0x80140000, {0x80160008, 0}}, {0x80160000, {0, resident_heap::heap_end_tag}}};
    heap.held = {{heap_first, std::vector<std::uint8_t>(0x1fff8, 0)}};
}

// A program in battle mode: the overlay, the game data, the formation record
// and the blocks the setup addresses.
Program sample() {
    Program program;
    auto &memory = program.battle.emplace();
    auto &resident = program.resident;
    resident.game_state = 0x8006d634;
    resident.game_data.assign(xem::reconstruction::game_data_bytes, 0);
    memory.regions[battle::overlay_base].resize(battle::overlay_end - battle::overlay_base);
    memory.regions[battle::formation_record - 4].resize(0x24);
    memory.regions[battle::battle_party_ids].resize(3);
    memory.regions[graphics].resize(0xa2b4);
    memory.regions[turn_state].resize(0x2f8);
    memory.regions[scene].resize(0x200);
    memory.regions[enemies].resize(0x400);
    memory.put32(battle::record_base_pointer, 0x800ccce8);
    memory.put32(0x800c3ea4, graphics);
    memory.put32(0x800c3eac, turn_state);
    memory.put32(0x800c3dd0, enemies);
    resident.battle_scene = scene;
    free_heap(program);
    return program;
}

// Phase 1: member 0 alone, enemy 0 in slot 3 (group 1), placed from the
// scene's group entries; the enemy's record and scripts from its file.
void participants() {
    auto program = sample();
    auto &memory = *program.battle;
    for (const auto [slot, id] : {std::pair{0U, 0U}, {1U, 0x7fU}, {2U, 0x7fU}})
        memory.put8(0x800d2d24 + slot, id);
    const auto record = [](std::uint32_t offset) { return battle::formation_record + offset; };
    memory.put8(record(4), 2); // member 0's group
    for (std::uint32_t slot = 3; slot < battle::combatant_slots; ++slot)
        memory.put8(record(5 + slot), 0x7f);
    memory.put8(record(5 + 3), 0);    // slot 3: enemy 0
    memory.put8(record(0x15 + 3), 1); // group 1
    memory.put16(scene + 2 * 0x20 + 4, 0x40);
    memory.put16(scene + 2 * 0x20 + 6, 0x50);
    memory.put16(scene + 1 * 0x20 + 0x10, 0x60);
    memory.put16(scene + 1 * 0x20 + 0x12, 0x70);
    memory.put16(enemies + 0, 0x200);  // enemy 0's scripts at +200
    memory.put16(enemies + 0x30, 0x300);
    memory.put8(enemies + 0x32 + 0x5a, 7); // record byte 5a
    memory.put16(enemies + 0x200, 8);
    memory.put16(enemies + 0x202, 0xa);
    memory.put16(enemies + 0x204, 0xffff);
    memory.put16(enemies + 0x206, 0xc);
    program.run_battle([&](battle::Battle &context) {
        battle::setup_participants(context, program.resident);
    });
    check(memory.u8(present) == 1 && memory.u8(present + 1) == 0 && memory.u8(present + 3) == 1 &&
              memory.u8(present + 4) == 0 && memory.u8(0x800d3280) == 0,
          "Presence follows the party ids and the formation");
    check(memory.u8(info) == 2 && memory.u16(info + 0xa) == 0x40 && memory.u16(info + 0xc) == 0x50,
          "The member stands at its group's first entry");
    const auto enemy = info + 3 * 0x1c;
    check(memory.u8(enemy + 2) == 0 && memory.u8(enemy) == 1 && memory.u16(enemy + 0xa) == 0x60 &&
              memory.u16(enemy + 0xc) == 0x70,
          "The enemy stands at its group's enemy entry");
    check(memory.u8(0x800ccce8 + 3 * battle::record_stride + 0x5a) == 7 &&
              memory.u32(0x800c3ddc) == enemies + 0x300,
          "The enemy record comes from the enemy data file");
    check(memory.u32(0x800d3400) == enemies + 0x208 && memory.u32(0x800d3404) == enemies + 0x20a &&
              memory.u8(0x800c3d18) == 0 && memory.u32(0x800d340c) == enemies + 0x20c &&
              memory.u8(0x800c3d19) == 1,
          "The enemy's scripts are armed as the file names them");
    check(memory.u8(0x800c34ad) == 1 && memory.u8(graphics + 0x853d) == 1 &&
              memory.u8(graphics + 0x1e4 + 0x853d) == 0,
          "One member is counted and shown");
}

// Phase 2: two inventory items, the turn order a permutation of the slots
// and the arrows' block from the heap.
void turns() {
    auto program = sample();
    auto &memory = *program.battle;
    for (const auto [slot, id] : {std::pair{0U, 0U}, {1U, 0x7fU}, {2U, 0x7fU}})
        memory.put8(0x800d2d24 + slot, id);
    memory.put8(present, 1);
    memory.put8(present + 3, 1);
    memory.put32(0x800d3364, scene); // phase 1's formation data
    auto &data = program.resident.game_data; // from 8006d634
    data[0x8006f65a - 0x8006d634] = 5;
    data[0x8006f5c4 - 0x8006d634] = 120;
    data[0x8006f65a + 1 - 0x8006d634] = 60; // not a battle item
    data[0x8006f5c4 + 1 - 0x8006d634] = 1;
    // Command layouts and masks: every table pointer at an empty area.
    for (const auto pointer : {0x800c2130U, 0x800c2134U, 0x800c2138U})
        memory.put32(pointer, 0x800c2200);
    for (std::uint32_t i = 0; i < 0x10; ++i)
        memory.put32(0x800c20f0 + i * 4, 0x800c2200);
    const auto seed = program.resident.random_seed;
    program.run_battle(
        [&](battle::Battle &context) { battle::setup_turns(context, program.resident); });
    check(memory.u8(0x800d2ce0) == 5 && memory.u8(0x800d2cb0) == 99 &&
              memory.u8(0x800d2ce0 + 1) == 0 && data[0x8006f5c4 - 0x8006d634] == 99,
          "Battle items are listed with counts capped at 99");
    std::array<bool, 11> seen{};
    for (std::uint32_t i = 0; i < 11; ++i)
        seen.at(memory.u8(0x800d2dd8 + i)) = true;
    check(std::ranges::all_of(seen, [](bool value) { return value; }) &&
              program.resident.random_seed != seed,
          "The turn order draws every slot once");
    check(memory.u16(0x800d2e06 + 1 * 2) == 0xff && memory.u16(0x800d2df0 + 1 * 2) == 0xff,
          "Absent slots keep timer ff");
    check(memory.u32(0x800c3e24) == heap_first && memory.contains(heap_first, 0xec),
          "The arrows' block is allocated");
}

// 80070f40's prologue: three blocks from the heap and the formation record
// the selector picks.
void prologue() {
    auto program = sample();
    auto &memory = *program.battle;
    auto &resident = program.resident;
    memory.regions[battle::formation_table].resize(0x210);
    memory.put8(battle::formation_table + 0x20 + 3, 0x42);
    resident.battle_request.selector = 1;
    resident.debug_word = 0xffffffffU;
    program.battle_prologue();
    check(memory.u32(0x800c3ea4) == heap_first && memory.contains(memory.u32(0x800d2d28), 0x10c) &&
              memory.contains(memory.u32(0x800c3eac), 0x2f8),
          "The graphics, UI and turn blocks are allocated");
    check(memory.u8(battle::formation_record + 3) == 0x42 && memory.u8(0x800c3e28) == 0xff,
          "The formation record is the selector's table entry");
    resident.b_5947c = 1;
    free_heap(program);
    missing([&] { program.battle_prologue(); }, "symbol:battle-event-8005947c",
            "The event battle is an explicit dependency");
}

// 800b8284's environments and the swirl's buffer swap.
void environments() {
    auto program = sample();
    auto &memory = *program.battle;
    program.battle_load_prologue(5);
    check(memory.u8(0x800d36b8) == 5 && memory.u16(0x800c4a86) == 10 &&
              memory.u16(0x800c8af8) == 0x100 && memory.u16(0x800c4a8a) == 0xd8,
          "The load prologue keeps the mode and sets both display ranges");
    auto &resident = program.resident;
    auto &gpu = resident.gpu;
    gpu.services = 0x80056888;
    gpu.functions[4] = 0x80046560;
    gpu.functions[11] = 0x80045d5c;
    gpu.otc_registers = {0x1f8010e0, 0x1f8010e4, 0x1f8010e8, 0x1f8010f0};
    using Kind = xem::reconstruction::PlatformInput::Kind;
    resident.platform = {{Kind::read, 0x80045de4, 0}, {Kind::read, 0x80045de4, 0}};
    memory.put32(0x800ccb00, 0x800c4a20);
    program.swap_battle_draw_buffer();
    check(memory.u32(0x800ccb00) == 0x800c8a90 && memory.u32(0x800ccb04) == 0x800c8b00 &&
              memory.u32(0x800c8b00 + 0xfff * 4) == ((0x800c8b00 + 0xffe * 4) & 0xffffffU),
          "The other buffer becomes current with a cleared ordering table");
    program.swap_battle_draw_buffer();
    check(memory.u32(0x800ccb00) == 0x800c4a20 && resident.platform.empty(),
          "Swapping twice returns to the first buffer");
    program.set_display_mask(0);
    check(gpu.commands.back().value == 0x03000001U &&
              std::ranges::all_of(gpu.display_environment, [](auto b) { return b == 0xff; }),
          "SetDispMask(0) disables the display and forgets the last environment");
}

} // namespace

int main() {
    try {
        participants();
        turns();
        prologue();
        environments();
        std::cout << "Battle setup: four source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
