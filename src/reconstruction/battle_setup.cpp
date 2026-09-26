// Battle setup of the setup module (directory 12 file 4, sha256 4300fdd9...,
// loaded at 801e4000): the phases 801e5840 runs from the intro swirl.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_sprite_model.hpp"
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_text.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace xem::reconstruction::battle {
namespace {

constexpr std::uint32_t fixed_record_base = 0x800ccce8; // 801e4870 and 801e4ac0 address it
constexpr std::uint32_t party_ids = 0x800d2d24;         // u8 x 3 (7f none)
constexpr std::uint32_t slot_info = 0x800c3eb4;         // 0x1c per slot
constexpr std::uint32_t slot_info_stride = 0x1c;
constexpr std::uint32_t present = 0x800d2dcc; // u8 per slot
constexpr std::uint32_t formation_pointer = 0x800d3364;
constexpr std::uint32_t formation_copy = 0x800c3eb0;
constexpr std::uint32_t party_count = 0x800d3280;   // u8: present party members - 1
constexpr std::uint32_t group_counts = 0x800d301c;  // 4-byte entries: count, member mask
constexpr std::uint32_t enemy_data = 0x800c3dd0;    // the enemy data file
constexpr std::uint32_t enemy_scripts = 0x800d3400; // 0x40 per enemy
constexpr std::uint32_t enemy_script_armed = 0x800c3d18;
constexpr std::uint32_t enemy_reaction_armed = 0x800c3d19;
constexpr std::uint32_t slot_flags = 0x800d32a1; // u8, 8 bytes per slot
constexpr std::uint32_t slot_bits = 0x800c3448;  // u16 per slot (80089c08)
constexpr std::uint32_t demo_party = 0x800d3294; // u8: formation flag 10
constexpr std::uint32_t graphics_state_pointer = 0x800c3ea4;
constexpr std::uint32_t turn_state_pointer = 0x800c3eac;

std::uint32_t info(std::uint32_t slot) { return slot_info + slot * slot_info_stride; }
std::uint32_t formation(std::uint32_t offset) { return formation_record + offset; }

// 801e4048: reset the outcome flags and publish the party.
void reset_participants(Battle &battle, ResidentState &resident) {
    auto &memory = battle.memory;
    for (const auto address : {0x800d3338U, 0x800c3d44U, 0x800d2d50U, 0x800d2fc4U, 0x800c3d5cU,
                               0x800d2d44U, 0x800c492aU})
        memory.put8(address, 0);
    resident.departure_5941c = 0;
    resident.b_5942c = 0;
    for (std::uint32_t member = 0; member < 3; ++member) {
        memory.put8(battle_party_ids + member, memory.u8(party_ids + member));
        memory.put8(info(member) + 2, memory.u8(party_ids + member));
    }
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        memory.put8(info(slot) + 3, 0);
        memory.put8(info(slot) + 4, memory.u8(demo_party) == 0 ? memory.u8(0x8006f8e5 + slot) : 1);
        memory.put8(info(slot) + 5, 0);
    }
    memory.put8(0x800c48ea, 0);
    memory.put8(0x800d2dc0, 0);
}

// 801e4160: formation groups, presence and positions from the formation
// record and the formation data.
void place_formation(Battle &battle, const ResidentState &resident) {
    auto &memory = battle.memory;
    memory.put8(party_count, 0);
    const auto data = resident.battle_scene; // 8005949c
    memory.put32(formation_pointer, data);
    memory.put32(formation_copy, data);
    for (std::uint32_t member = 0; member < 3; ++member) {
        if ((memory.u8(info(member) + 2) & 0x7f) == 0x7f) {
            memory.put8(present + member, 0);
        } else {
            memory.put8(present + member, 1);
            memory.put8(party_count, memory.u8(party_count) + 1);
        }
        memory.put8(info(member), memory.u8(info(member) + 4) == 0
                                      ? memory.u8(formation(4 + member)) & 0x7f
                                      : member);
    }
    memory.put8(party_count, memory.u8(party_count) + 0xff);
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot) {
        const auto id = memory.u8(formation(5 + slot));
        if ((id & 0x7f) == 0x7f) {
            const auto record = fixed_record_base + slot * record_stride;
            memory.put16(record + 0x4e, 0);
            memory.put16(record + 0x4c, 0);
            memory.put8(info(slot) + 2, 0x7f);
            memory.put8(info(slot) + 3, 0);
            memory.put8(info(slot) + 4, 0);
            memory.put8(present + slot, 0);
        } else {
            memory.put8(info(slot) + 2, id & 0x7f);
            memory.put8(info(slot) + 3, memory.u8(formation(0xd + slot)) & 0x80);
            memory.put8(info(slot) + 4, memory.u8(formation(5 + slot)) & 0x80);
            memory.put8(info(slot) + 5, memory.u8(formation(0xd + slot)) & 1);
            memory.put8(present + slot, 1);
            memory.put8(info(slot), memory.u8(formation(0x15 + slot)) & 0x7f);
        }
        memory.put8(0x800c3e3d + slot, memory.u8(info(slot) + 2) + 1);
    }
    for (std::uint32_t group = 0; group < 32; ++group) {
        memory.put8(group_counts + group * 4, 0);
        memory.put8(group_counts + group * 4 + 1, 0);
    }
    // Members join their group (enemies use entries 8..; the flagged, alone
    // in entry +10).
    const auto join = [&](std::uint32_t slot, std::uint32_t entry, std::uint32_t alone) {
        if (memory.u8(info(slot) + 2) == 0x7f)
            return;
        const auto group = memory.u8(info(slot));
        if (memory.u8(info(slot) + 4) == 0) {
            const auto at = group_counts + (group + entry) * 4;
            memory.put8(info(slot) + 1, memory.u8(at));
            memory.put8(at + 1,
                        memory.u8(at + 1) | memory.u16(slot_bits + memory.u8(info(slot) + 1) * 2));
            memory.put8(at, memory.u8(at) + 1);
        } else {
            memory.put8(info(slot) + 1, 0);
            memory.put8(group_counts + (group + alone) * 4 + 1, 1);
            memory.put8(group_counts + (group + alone) * 4, 1);
        }
    };
    for (std::uint32_t member = 0; member < 3; ++member)
        join(member, 0, 0x10);
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot)
        join(slot, 8, 0x18);
    // Positions: grouped members from the group's entries (0x20 per group,
    // 4 per member), the alone ones from +100 (8 per group).
    for (std::uint32_t member = 0; member < 3; ++member) {
        if (memory.u8(info(member) + 2) == 0x7f)
            continue;
        const auto group = memory.u8(info(member));
        const auto at = memory.u8(info(member) + 4) == 0
                            ? data + memory.u8(info(member) + 1) * 4 + group * 0x20 + 4
                            : data + group * 8 + 0x100;
        memory.put16(info(member) + 0xa, memory.u16(at));
        memory.put16(info(member) + 0xc, memory.u16(at + 2));
    }
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot) {
        if (memory.u8(info(slot) + 2) == 0x7f)
            continue;
        const auto group = memory.u8(info(slot));
        const auto at = memory.u8(info(slot) + 4) == 0
                            ? data + memory.u8(info(slot) + 1) * 4 + group * 0x20 + 0x10
                            : data + group * 8 + 0x104;
        memory.put16(info(slot) + 0xa, memory.u16(at));
        memory.put16(info(slot) + 0xc, memory.u16(at + 2));
        // 8006f9f7 + (slot - 3): the byte walks on with the slot.
        memory.put8(info(slot) + 6, memory.u8(formation(0x18 + slot)) & 0x80);
    }
}

// 801e4870: enemy records (0x170 each after the file's 0x32-byte header) and
// the AI script pointers of each present enemy.
void load_enemy_records(Battle &battle) {
    auto &memory = battle.memory;
    const auto file = memory.u32(enemy_data);
    memory.put16(0x800c48e8, 0);
    memory.put16(0x800d39e0, 0);
    memory.put32(0x800c3ddc, memory.u16(file + 0x30) + file);
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot) {
        const auto enemy = slot - 3;
        const auto record = fixed_record_base + slot * record_stride;
        memory.put8(0x800c3d1b + enemy * 4, 0);
        const auto id = memory.u8(info(slot) + 2);
        if (id == 0x7f) {
            for (std::uint32_t i = 0; i < record_stride; ++i)
                memory.put8(record + i, 0);
            memory.put8(enemy_script_armed + enemy * 4, 0);
            memory.put8(enemy_reaction_armed + enemy * 4, 0);
            continue;
        }
        for (std::uint32_t i = 0; i < record_stride; ++i)
            memory.put8(record + i, memory.u8(file + 0x32 + id * record_stride + i));
        const auto scripts = memory.u16(file + id * 2) + file;
        const auto block = enemy_scripts + enemy * 0x40;
        memory.put32(block, scripts + memory.u16(scripts));
        memory.put32(block + 4, scripts + memory.u16(scripts + 2));
        if (memory.u16(scripts + 4) == 0xffff) {
            memory.put8(enemy_script_armed + enemy * 4, 0);
        } else {
            memory.put32(block + 8, scripts + memory.u16(scripts + 4));
            memory.put8(enemy_script_armed + enemy * 4, 1);
        }
        if (memory.u16(scripts + 6) == 0xffff) {
            memory.put8(enemy_reaction_armed + enemy * 4, 0);
        } else {
            memory.put32(block + 0xc, scripts + memory.u16(scripts + 6));
            memory.put8(enemy_reaction_armed + enemy * 4, 1);
        }
        for (std::uint32_t i = 0; i < 4; ++i)
            memory.put32(block + 0x10 + i * 4, 0);
        for (std::uint32_t i = 0; i < 8; ++i)
            memory.put16(block + 0x20 + i * 2, 0);
        for (std::uint32_t i = 0; i < 16; ++i)
            memory.put8(block + 0x30 + i, 0);
    }
}

// 80097d5c: each present member's derived stats: the saved stat block
// (record base + 1000/fd0/1028, per member), equipment bonuses (+28..2f),
// percentage bonuses to HP and EP (+30, +31), caps, gear record totals
// (+a4 block) and the gear's fuel cost (+2814 of its 690 block).
void derive_party(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t member = 0; member < 3; ++member) {
        if (memory.u8(party_ids + member) == 0x7f)
            continue;
        const auto base = battle.record_base();
        const auto rec = base + member * record_stride;
        const auto gear = rec + 0xa4;
        memory.put32(attacker_pointer, rec);
        memory.put32(attacker_block_pointer, gear);
        if (memory.u8(party_ids + member) == 9) {
            memory.put8(0x8006deba, 9);
            memory.put8(rec + 0x56, 9);
        }
        if (memory.u8(party_ids + member) == 10) {
            memory.put8(0x8006df5e, 10);
            memory.put8(rec + 0x56, 10);
        }
        const bool four = memory.u8(rec + 0x56) == 4;
        const auto saved = base + member * 8;
        memory.put8(saved + 0x1028, four ? memory.u8(rec + 4) + memory.u8(rec + 0x1c)
                                         : memory.u8(rec + 0x58) + memory.u8(rec + 4));
        memory.put8(saved + 0x1029, memory.u8(rec + 0x5e));
        memory.put8(saved + 0x102a, memory.u8(rec + 0x59) + memory.u8(rec + 0x2d));
        memory.put8(saved + 0x102b, memory.u8(rec + 0x5f));
        memory.put8(saved + 0x102c, memory.u8(rec + 0x5b));
        memory.put8(saved + 0x102d, memory.u8(rec + 0x5c));
        memory.put8(saved + 0x102e, memory.u8(rec + 0x5a));
        memory.put32(saved + 0xfd0, memory.u32(rec + 0x3c));
        memory.put32(saved + 0xfd4, memory.u32(rec + 0x40));
        memory.put16(base + member * 4 + 0x1000, memory.u16(rec + 0x4e));
        memory.put16(base + member * 4 + 0x1002, memory.u16(rec + 0x52));
        memory.put8(rec + 0x58, memory.u8(rec + 0x58) + memory.u8(rec + 0x28));
        memory.put16(rec + 0x34, 0);
        for (const auto stat : {0x59U, 0x5aU, 0x5bU, 0x5cU, 0x5eU, 0x5fU})
            memory.put8(rec + stat, memory.u8(rec + stat) + memory.u8(rec + stat - 0x30));
        if (memory.u8(rec + 0x5a) > 0x10)
            memory.put8(rec + 0x5a, 0x10);
        if ((memory.u16(rec + 0x32) & 0x100) != 0) {
            memory.put8(rec + 0x5e, memory.u8(rec + 0x5e) + (memory.u8(rec + 0x5e) >> 2));
            memory.put8(rec + 0x5f, memory.u8(rec + 0x5f) + (memory.u8(rec + 0x5f) >> 2));
        }
        // Percent bonuses from the unchanged maxima (x * 66666667 >> 35: x / 20).
        const auto hp = memory.u16(rec + 0x4e) * memory.u8(rec + 0x30) / 20;
        const auto ep = memory.u16(rec + 0x52) * memory.u8(rec + 0x31) / 20;
        memory.put16(rec + 0x4c, memory.u16(rec + 0x4c) + hp);
        memory.put16(rec + 0x50, memory.u16(rec + 0x50) + ep);
        memory.put16(rec + 0x4e, memory.u16(rec + 0x4e) + hp);
        memory.put16(rec + 0x52, memory.u16(rec + 0x52) + ep);
        if (memory.u16(rec + 0x4c) >= 1000)
            memory.put16(rec + 0x4c, 999);
        if (memory.u16(rec + 0x50) >= 100)
            memory.put16(rec + 0x50, 99);
        if (memory.u16(rec + 0x4e) >= 1000)
            memory.put16(rec + 0x4e, 999);
        if (memory.u16(rec + 0x52) >= 100)
            memory.put16(rec + 0x52, 99);
        memory.put8(rec + 0x158, 5);
        memory.put8(rec + 0x159, 5);
        memory.put8(rec + 0x148, 0);
        memory.put16(gear + 0x70, memory.u16(gear + 0x70) + memory.u16(gear + 0x40));
        memory.put16(gear + 0x72, memory.u16(gear + 0x72) + memory.u16(gear + 0x42));
        memory.put8(gear + 0x9c, memory.u8(gear + 0x9c) + memory.u8(gear + 0x4c));
        memory.put16(gear + 0x68,
                     memory.u16(gear + 0x68) + memory.u16(gear + 0x44) + memory.u16(gear + 0x46));
        memory.put8(gear + 0x9f, memory.u8(gear + 0x9f) + memory.u8(gear + 0x4d));
        memory.put8(gear + 0x98,
                    memory.u8(gear + 0x98) + memory.u8(gear + 0x4e) - memory.u8(gear + 0x4a));
        memory.put8(gear + 0x9e, memory.u8(gear + 0x9e) + memory.u8(gear + 0x54));
        if (const auto rate = memory.u8(gear + 0x4f); rate != 0 && memory.u8(rec + 0xa0) != 0x12) {
            const auto fuel = base + member * 0x690 + 0x2814;
            memory.put16(fuel, memory.u32(gear + 0x64) / 10 * rate * 2 / 9);
            memory.put16(fuel, memory.u16(fuel) / 10);
            memory.put16(fuel, memory.u16(fuel) * 10);
        }
        memory.put8(rec + 0x149, 0);
        const auto character = 0x8006ecf8 + memory.u8(rec + 0x56) * 0x20;
        if ((memory.u16(character) & 0x1c00) != 0)
            memory.put8(rec + 0x149, 1);
        if ((memory.u16(character) & 0x380) != 0)
            memory.put8(rec + 0x149, 2);
        if ((memory.u16(character) & 0x70) != 0)
            memory.put8(rec + 0x149, 3);
        memory.put8(gear + 0x3f, memory.u8(gear + 0x74));
        memory.put8(gear + 0x3e, memory.u8(gear + 0x74));
        memory.put8(gear + 0x3f, memory.u8(gear + 0x3f) + memory.u8(gear + 0x56));
        memory.put8(gear + 0x3e, memory.u8(gear + 0x3e) + memory.u8(gear + 0x56));
        const auto rates = [&](auto change) {
            for (std::uint32_t i = 0; i < 0x26; ++i) {
                const auto at = base + member * 0x5f0 + i * 0x28 + 0x106b;
                memory.put8(at, change(memory.u8(at)));
            }
            for (std::uint32_t i = 0; i < 0x2a; ++i) {
                const auto at = base + member * 0x690 + i * 0x28 + 0x223b;
                memory.put8(at, change(memory.u8(at)));
            }
        };
        if ((memory.u16(rec + 0x8a) & 0x2000) != 0)
            rates([](std::uint32_t value) { return value << 1; });
        if ((memory.u16(rec + 0x32) & 0x4000) != 0)
            rates([](std::uint32_t value) { return (value + 1) >> 1; });
        if (memory.u8(rec + 0x62) >= 50)
            memory.put16(character, memory.u16(character) | 8);
        if (memory.u8(rec + 0x56) == 7) {
            memory.put32(gear + 0x60, memory.u16(rec + 0x4c) * 50);
            memory.put32(gear + 0x64, memory.u16(rec + 0x4e) * 50);
            memory.put8(gear + 0x3c, memory.u8(rec + 0x58));
            memory.put16(gear + 0x70, memory.u8(rec + 0x59) * 12);
            memory.put16(gear + 0x72, memory.u8(rec + 0x5c) * 6);
            memory.put8(gear + 0x98, memory.u8(rec + 0x5a));
            const auto flags = memory.u16(gear + 0x7e);
            memory.put16(gear + 0x7e, flags | 0x3c4);
            if ((memory.u16(rec + 0x82) & 0x2000) != 0)
                memory.put16(gear + 0x7e, flags | 0x13c4);
        }
        if (memory.u8(rec + 0xa0) == 0xf) {
            const auto at = 0x8006ed0e + memory.u8(rec + 0x56) * 0x20;
            memory.put16(at, memory.u16(at) & 0x8fff);
        }
        if (memory.u8(rec + 0xa0) == 0x12) {
            memory.put16(rec + 0x7a, 0x238);
            memory.put16(gear + 0x38, 0x26ac);
            memory.put16(gear + 0x3a, 0x26ac);
            memory.put8(rec + 0x149, 0);
            memory.put16(0x8006ed6e, 0x8000);
        }
        if (memory.u16(0x8006ef64) >= 231 && (memory.u8(0x8006f989) & 0x80) == 0) {
            memory.put8(0x8006dece, 0x27);
            memory.put8(0x8006de68, 0x1e);
            memory.put8(0x8006f989, memory.u8(0x8006f989) | 0x80);
        }
    }
    memory.put8(0x800c34ad, 3);
    for (std::uint32_t member = 0; member < 3; ++member)
        if (memory.u8(party_ids + member) == 0x7f)
            memory.put8(0x800c34ad, memory.u8(0x800c34ad) - 1);
    for (std::uint32_t member = 0; member < 3; ++member) {
        memory.put32(battle.record_base() + member * 4 + 0x5f54, 0);
        memory.put32(battle.record_base() + member * 4 + 0x5f60, 0);
    }
}

// 8009b098: the demonstration party's members 1 and 2.
void demo_members(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t member = 1; member < 3; ++member) {
        const auto rec = battle.record(member);
        memory.put16(rec + 0x4c, 100);
        memory.put16(rec + 0x4e, 100);
        memory.put8(rec + 0x5e, 0x14);
        memory.put8(rec + 0x5f, 0xf);
        memory.put16(rec + 0x7a, 0x1fbf);
        memory.put8(rec + 0x149, 0);
    }
}

// 801e4ac0: derived stats, then each slot's gear flags (800d32a1, record
// +15a bit 80) and the panel states of the graphics block (+853d, 1e4 per
// member).
void derive_flags(Battle &battle) {
    auto &memory = battle.memory;
    derive_party(battle);
    if (memory.u8(demo_party) != 0)
        demo_members(battle);
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto flags = fixed_record_base + member * record_stride + 0x15a;
        const auto panel = memory.u32(graphics_state_pointer) + member * 0x1e4 + 0x853d;
        if (memory.u8(info(member) + 2) == 0x7f) {
            memory.put8(slot_flags + member * 8, 0);
            memory.put8(flags, memory.u8(flags) & 0x7f);
            memory.put8(panel, 0);
            continue;
        }
        memory.put8(panel, 1);
        if (memory.u8(info(member) + 4) == 0) {
            memory.put8(slot_flags + member * 8, 0);
            memory.put8(flags, memory.u8(flags) & 0x7f);
        } else {
            memory.put8(slot_flags + member * 8, 1);
            memory.put8(flags, memory.u8(flags) | 0x80);
            if (memory.u8(info(member) + 2) != 7)
                memory.put8(panel, 2);
        }
    }
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot)
        memory.put8(slot_flags + slot * 8,
                    memory.u8(info(slot) + 2) == 0x7f || memory.u8(info(slot) + 4) == 0 ? 0 : 1);
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto rec = fixed_record_base + member * record_stride;
        memory.put8(slot_flags + member * 8 + 4, memory.u8(rec + 0x62));
        memory.put8(slot_flags + member * 8 + 5, memory.u8(rec + 0x63));
    }
}

// 801e4cd0: the item lists (800d2ce0 ids, 800d2cb0 counts, 800d2fe4) from the
// inventory (8006f65a ids, 8006f5c4 counts, capped at 99), and the ids 32..48
// of 8006f3d0 with their 8006f36c bytes at 800c3d70 and 800d3688.
void setup_items(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t i = 0; i < 48; ++i) {
        memory.put8(0x800d2ce0 + i, 0);
        memory.put8(0x800d2cb0 + i, 0);
        memory.put8(0x800d2fe4 + i, 0);
    }
    for (std::uint32_t i = 0; i < 150; ++i) {
        if (memory.u8(0x8006f5c4 + i) >= 100)
            memory.put8(0x8006f5c4 + i, 99);
        if (memory.u8(0x8006f5c4 + i) == 0)
            memory.put8(0x8006f65a + i, 0);
    }
    std::uint32_t listed = 0;
    for (std::uint32_t i = 0; i < 150 && (listed & 0xff) < 48; ++i) {
        const auto id = memory.u8(0x8006f65a + i);
        if (id == 0 || id >= 49)
            continue;
        const auto at = listed & 0xff;
        memory.put8(0x800d2ce0 + at, id);
        memory.put8(0x800d2cb0 + at, memory.u8(0x8006f5c4 + i));
        memory.put8(0x800d2fe4 + at, memory.u8(0x8006f65a + i));
        ++listed;
    }
    memory.put8(memory.u32(turn_state_pointer) + 0x2d9, 0x2f);
    listed = 0;
    for (std::uint32_t i = 0; i < 100; ++i) {
        const auto id = memory.u8(0x8006f3d0 + i);
        if (id - 50U >= 23U)
            continue;
        const auto at = listed & 0xff;
        memory.put8(0x800c3d70 + at, id);
        memory.put8(0x800d3688 + at, memory.u8(0x8006f36c + i));
        ++listed;
    }
    for (; (listed & 0xff) < 48; ++listed) {
        memory.put8(0x800c3d70 + (listed & 0xff), 0);
        memory.put8(0x800d3688 + (listed & 0xff), 0);
    }
}

// 801e4e7c: the initial turn timers (80078508: 80098af8 for each present
// slot, ff otherwise, ready flags and alternates cleared), the turn order (11
// distinct 8001bd40(0, 10) draws), enemies flagged 200 at timer 1, then every
// present timer less the smallest one plus one.
void setup_turn_order(Battle &battle) {
    auto &memory = battle.memory;
    memory.put8(0x800d2caa, 0);
    std::array<std::uint8_t, 11> drawn{};
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        const auto value = memory.u8(present + slot) == 0 ? 0xffU : turn_timer(battle, slot);
        memory.put16(0x800d2e06 + slot * 2, value);
        memory.put16(0x800d2df0 + slot * 2, value);
        memory.put8(0x800d2de4 + slot, 0);
        memory.put16(0x800d2e1c + slot * 2, 0);
    }
    for (std::uint32_t found = 0; found < 11;) {
        // 8001bd40(0, 10): 0 + (rand & ff) % 11.
        const auto slot = (battle.rand() & 0xff) % 11;
        if (drawn[slot] != 0)
            continue;
        drawn[slot] = 1;
        memory.put8(0x800d2dd8 + found, slot);
        ++found;
    }
    memory.put8(0x800d2dd7, 0);
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot) {
        if (memory.u8(present + slot) == 0 ||
            (memory.u16(fixed_record_base + slot * record_stride + 0x34) & 0x200) == 0)
            continue;
        memory.put16(0x800d2e06 + slot * 2, 1);
        memory.put16(0x800d2df0 + slot * 2, 1);
    }
    std::int32_t least = 0xffff;
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot)
        if (memory.u8(present + slot) != 0 && memory.s16(0x800d2e06 + slot * 2) < least)
            least = memory.s16(0x800d2e06 + slot * 2);
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot)
        if (memory.u8(present + slot) != 0)
            memory.put16(0x800d2e06 + slot * 2,
                         memory.u16(0x800d2e06 + slot * 2) - static_cast<std::uint32_t>(least - 1));
}

// 801e5014: the turn state's per-slot tables (0x40 each): command masks and
// menu layouts for the party, and every slot's default target (800841e0)
// with its facing (80085310).
void setup_commands(Battle &battle) {
    auto &memory = battle.memory;
    if ((memory.u8(formation(1)) & 0x20) != 0)
        memory.put8(0x800c3d48, 1);
    const auto state = [&](std::uint32_t slot) {
        return memory.u32(turn_state_pointer) + slot * 0x40;
    };
    const auto target = [&](std::uint32_t slot) {
        memory.put8(state(slot) + 0x3c, order_candidates(battle, slot));
        const auto chosen = memory.u8(state(slot) + 0x3c);
        memory.put8(info(slot) + 6,
                    memory.u16(info(chosen) + 0xa) < memory.u16(info(slot) + 0xa) ? 1 : 0);
    };
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto rec = fixed_record_base + member * record_stride;
        memory.put8(0x800d32a4 + member * 8,
                    memory.u8(0x8006ed0b + memory.u8(party_ids + member) * 0x20));
        const auto layout = memory.u32(0x800c20f0 + memory.u8(rec + 0x56) * 4);
        for (std::uint32_t i = 0; i < 8; ++i)
            memory.put8(state(member) + i, memory.u8(layout + i));
        for (std::uint32_t i = 0; i < 4; ++i) {
            const auto source =
                memory.u32(memory.u8(party_ids + member) == 7 ? 0x800c2138U : 0x800c2130U);
            memory.put8(state(member) + 8 + i, memory.u8(source + i));
            memory.put8(state(member) + 0xc + i, memory.u8(memory.u32(0x800c2134) + i));
        }
        target(member);
        for (std::uint32_t i = 0; i < 16; ++i)
            memory.put16(state(member) + 0x1c + i * 2,
                         memory.u16(rec + 0x7a) & memory.u16(0x800c3234 + i * 2));
        if (memory.u8(rec + 0xa0) == 0xff || (memory.u8(formation(1)) & 0x40) != 0) {
            memory.put16(state(member) + 0x2a, memory.u16(0x800c3242));
            memory.put16(rec + 0x7a, memory.u16(rec + 0x7a) | memory.u16(0x800c3242));
        }
        if ((memory.u8(formation(1)) & 0x80) != 0) {
            memory.put16(state(member) + 0x2c, memory.u16(0x800c3244));
            memory.put16(rec + 0x7a, memory.u16(rec + 0x7a) | memory.u16(0x800c3244));
        }
    }
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot)
        target(slot);
}

} // namespace

void setup_participants(Battle &battle, ResidentState &resident) {
    reset_participants(battle, resident);
    place_formation(battle, resident);
    load_enemy_records(battle);
    derive_flags(battle);
}

void setup_turns(Battle &battle, ResidentState &resident) {
    setup_items(battle);
    setup_turn_order(battle);
    setup_commands(battle);
    const auto arrows = allocate_block(battle, resident, 0xec, 0);
    battle.memory.put32(0x800c3e24, arrows);
    for (std::uint32_t i = 0; i < 0xec; ++i)
        battle.memory.put8(arrows + i, 0);
}

} // namespace xem::reconstruction::battle

namespace xem::reconstruction {
namespace {
std::int16_t s16(std::uint32_t value) { return static_cast<std::int16_t>(value); }
// A byte as the original reads it: battle memory, else a heap header or the
// bytes the heap holds (a copy past the end of a block reads its neighbour).
std::uint32_t original_byte(const battle::BattleMemory &memory, const resident::Heap &heap,
                            std::uint32_t address) {
    const auto region = memory.regions.upper_bound(address);
    if (region != memory.regions.begin() &&
        address - std::prev(region)->first < std::prev(region)->second.size())
        return memory.u8(address);
    if (const auto header = heap.headers.upper_bound(address); header != heap.headers.begin()) {
        const auto &[at, words] = *std::prev(header);
        if (address - at < 8)
            return (words[(address - at) / 4] >> (8U * ((address - at) % 4))) & 0xffU;
    }
    if (const auto held = heap.held.upper_bound(address); held != heap.held.begin()) {
        const auto &[at, bytes] = *std::prev(held);
        if (address - at < bytes.size())
            return bytes[address - at];
    }
    throw battle::BattleError("A setup copy reads memory no Program state owns");
}
// 8003f99c memmove (forward: the copies here do not overlap).
void copy(battle::BattleMemory &memory, const resident::Heap &heap, std::uint32_t to,
          std::uint32_t from, std::uint32_t size) {
    for (std::uint32_t i = 0; i < size; ++i)
        memory.put8(to + i, original_byte(memory, heap, from + i));
}
// 8003342c: a table's offsets (after its count) become addresses.
void relocate_table(battle::BattleMemory &memory, std::uint32_t table) {
    battle::ResidentView view{memory};
    static_cast<void>(resident::relocate_offsets(view, table));
}

// The stage setup 801e7210 and its callees.
constexpr std::uint32_t stage_record = 0x800d33e4;    // 800d3368 entry 1f
constexpr std::uint32_t primitive_table = 0x8004fe50; // resident, 17 rows of 28h
constexpr std::uint32_t lights = 0x800c3d50;          // two 8002709c blocks
constexpr std::uint32_t light_records = 0x800c3db4;   // two 18h-byte records (80027d64)

std::uint32_t allocate(battle::BattleMemory &memory, resident::Heap &heap, std::uint32_t size,
                       std::uint32_t mode, std::uint32_t site) {
    auto block = resident::heap_allocate(heap, size, mode, site);
    if (!block)
        throw battle::BattleError("A quiet null allocation in the stage setup");
    const auto address = block->address;
    memory.regions.emplace(address, std::move(block->bytes));
    return address;
}
field::GteMatrix battle_matrix(const battle::BattleMemory &memory, std::uint32_t at) {
    field::GteMatrix matrix{};
    for (std::uint32_t i = 0; i < 9; ++i)
        matrix.r[i] = s16(memory.u16(at + 2 * i));
    matrix.pad = s16(memory.u16(at + 0x12));
    for (std::uint32_t i = 0; i < 3; ++i)
        matrix.t[i] = static_cast<std::int32_t>(memory.u32(at + 0x14 + 4 * i));
    return matrix;
}
void put_rotation(battle::BattleMemory &memory, std::uint32_t at,
                  const std::array<std::int16_t, 9> &rotation) {
    for (std::uint32_t i = 0; i < 9; ++i)
        memory.put16(at + 2 * i, static_cast<std::uint16_t>(rotation[i]));
}
// MULT/MFLO: the low word of a signed product.
std::int32_t low(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

// 8002c3e8 (`relocate`) / 8002c4bc: a model group's offsets become addresses
// (+4 bit 0 set) or offsets again (cleared): each 38h-byte model's four table
// offsets (+18..+24) and its optional list (+2c), whose entries' two offsets
// go from the last (index +0) down.
void rebase_model_group(battle::BattleMemory &memory, std::uint32_t group, bool relocate) {
    const auto flags = memory.u32(group + 4);
    if (((flags & 1U) != 0) == relocate)
        return;
    memory.put32(group + 4, relocate ? flags | 1U : flags & ~1U);
    const auto shift = [&](std::uint32_t at) {
        memory.put32(at, relocate ? memory.u32(at) + group : memory.u32(at) - group);
    };
    const auto count = static_cast<std::int32_t>(memory.u32(group));
    for (std::uint32_t k = 0; static_cast<std::int32_t>(k) < count; ++k) {
        const auto model = group + 0x18 + 0x38 * k;
        for (const auto at : {0U, 8U, 4U, 0xcU})
            shift(model + at);
        auto list = memory.u32(model + 0x14);
        if (list == 0)
            continue;
        if (relocate) {
            list += group;
            memory.put32(model + 0x14, list);
        }
        if (const auto entries = memory.u32(list); entries != 0xffffffffU)
            for (auto i = static_cast<std::int32_t>(entries); i >= 0; --i) {
                const auto entry = list + 4 + 0xc * static_cast<std::uint32_t>(i);
                shift(entry + 4);
                shift(entry + 8);
            }
        if (!relocate)
            memory.put32(model + 0x14, list - group);
    }
}

// 8009eba8(group, slot): relocate the group and list its models (+10, 38h
// apart) in a new table; the slot holds the table and the count.
void list_models(battle::BattleMemory &memory, resident::Heap &heap, std::uint32_t group,
                 std::uint32_t slot) {
    resident::heap_select_tag(heap, 4, 0); // 80032498
    rebase_model_group(memory, group, true);
    const auto count = memory.u32(group);
    const auto table = allocate(memory, heap, count << 2U, 0, 0x8009ebe0);
    memory.put32(slot, table);
    memory.put32(slot + 4, count);
    for (std::uint32_t k = 0; k < count; ++k)
        memory.put32(table + 4 * k, group + 0x10 + 0x38 * k);
}

// 8009ec4c(slot, hierarchy, 0, 0, ...): a root record and a 7ch-byte part
// for each hierarchy pair (model, parent; ffff none) up to the first model
// id past the slot's count; a part of a model gets its packets for both
// buffers (8002cb54, 8002c8cc mode 0, 8003f968). `group` holds the models.
std::uint32_t build_parts(battle::BattleMemory &memory, ResidentState &resident, std::uint32_t slot,
                          std::uint32_t hierarchy, std::uint32_t group) {
    auto &heap = resident.heap;
    resident::heap_select_tag(heap, 4, 0); // 80032498
    const auto listed = [&](std::uint32_t pair) {
        const auto id = memory.u16(hierarchy + 4 * pair);
        return id < memory.u32(slot + 4) || id == 0xffff;
    };
    std::uint32_t count = 0;
    while (listed(count))
        ++count;
    if (count == 0)
        throw MissingDependency({"battle_stage_setup", 0x8009ecd8, {}, {}},
                                "symbol:battle-stage-without-parts", false,
                                "A stage without model parts is not reconstructed");
    const auto root = allocate(memory, heap, (count + 1) * 0x7c, 0, 0x8009ecfc);
    const auto clear = [&](std::uint32_t part) {
        for (const auto at : {0x56U, 0x58U})
            memory.put16(part + at, 0);
        for (const auto at : {0x5cU, 0x60U, 0x64U, 0x70U, 0x74U, 0x78U})
            memory.put32(part + at, 0);
    };
    for (const auto at : {4U, 5U, 6U})
        memory.put8(root + at, 1);
    for (const auto at : {0x4cU, 0x4eU, 0x50U})
        memory.put16(root + at, 0x1000);
    memory.put32(root, 0);
    memory.put8(root + 7, 0);
    memory.put16(root + 8, 0xffff);
    memory.put16(root + 0xa, count + 1);
    memory.put32(root + 0x68, 0);
    memory.put32(root + 0x6c, 0);
    memory.put16(root + 0x54, 0);
    clear(root);
    const std::array resources{
        field::SpriteResource{group, memory.tail(group)},
        field::SpriteResource{primitive_table, memory.tail(primitive_table)}};
    field::SpriteSources sources{};
    sources.resources = resources;
    sources.models = &resident.sprite_models;
    for (std::uint32_t index = 1; index <= count; ++index) {
        const auto id = memory.u16(hierarchy + 4 * (index - 1));
        const auto parent = memory.u16(hierarchy + 4 * (index - 1) + 2);
        const auto part = root + 0x7c * index;
        memory.put32(part, parent == 0xffff ? 0 : root + 0x7c * (parent + 1U));
        memory.put16(part + 0xa, index);
        for (const auto at : {4U, 5U, 7U})
            memory.put8(part + at, 1);
        for (const auto at : {0x4cU, 0x4eU, 0x50U})
            memory.put16(part + at, 0x1000);
        memory.put8(part + 6, 0);
        memory.put16(part + 0x52, 0);
        memory.put16(part + 8, id);
        if (id == 0xffff) {
            memory.put32(part + 0x68, 0);
            memory.put32(part + 0x6c, 0);
        } else {
            const auto model = memory.u32(memory.u32(slot) + id * 4U);
            heap.allocation_class = 0x25; // 800324b8
            const auto size = memory.u32(model + 0x34);
            const auto packets = allocate(memory, heap, size * 2, 0, 0x8002cb84);
            memory.put32(part + 0x68, packets);
            memory.put32(part + 0x6c, packets + size);
            auto &bytes = memory.regions.at(packets);
            field::SpriteAllocation buffer{packets, std::move(bytes)};
            field::initialize_model_packets(model, buffer, resident.sprite, sources);
            bytes = std::move(buffer.bytes);
            std::copy_n(bytes.begin(), size, bytes.begin() + size);
        }
        memory.put16(part + 0x54, 0);
        clear(part);
    }
    return root;
}

// 801e70e8(images): relocate the image list, then the bounds of its pixel
// sections (1101): 800d2d30/800d2d34 the least corner, 800d2d2c/800c3ea8
// the extent.
void texture_bounds(battle::BattleMemory &memory, std::uint32_t images) {
    relocate_table(memory, images);
    std::uint32_t left = 0x800;
    std::uint32_t top = 0x800;
    std::int32_t right = -0x800;
    std::int32_t bottom = -0x800;
    const auto count = static_cast<std::int32_t>(memory.u32(images));
    for (std::uint32_t k = 0; static_cast<std::int32_t>(k) < count; ++k) {
        const auto image = memory.u32(images + 4 + 4 * k);
        if (memory.u16(image) != 0x1101)
            continue;
        const auto x = memory.u16(image + 4) + memory.u16(image + 8);
        const auto y = memory.u16(image + 6) + memory.u16(image + 10);
        left = std::min(left, x);
        top = std::min(top, y);
        right = std::max(right, static_cast<std::int32_t>(x + memory.u16(image + 12)));
        bottom = std::max(bottom, static_cast<std::int32_t>(y + memory.u16(image + 14)));
    }
    memory.put16(0x800d2d30, left);
    memory.put16(0x800d2d34, top);
    memory.put16(0x800d2d2c, static_cast<std::uint32_t>(right) - left);
    memory.put16(0x800c3ea8, static_cast<std::uint32_t>(bottom) - top);
}

// 800aa898(record, 800c3d0c, scripts, 0) and 800aa934(record, record,
// 800c3d0c, 0): the record's motion state and its first script; then
// 800aad54(record, 800c3d0c, -1, 1, 0): one step of the record's velocities
// into the root part (its angles, then its position through the root's
// local matrix, ApplyMatrix 80049cec, and the record's scale +1c) and the
// script's commands, of which only the stop (0) is reconstructed.
void start_stage_motion(battle::BattleMemory &memory, ResidentState &resident, std::uint32_t record,
                        std::uint32_t scripts) {
    memory.put16(record + 0x3c, 0xffff);
    memory.put8(record + 0x5c, 0xff);
    memory.put8(record + 0x39, 0x6b);
    memory.put32(record + 8, scripts);
    for (const auto at : {0xcU, 0x10U, 0x14U, 0x18U})
        memory.put32(record + at, 0);
    memory.put8(record + 0x2b, 0);
    memory.put16(record + 0x98, 0xffff);
    memory.put16(record + 0x58, 0);
    for (const auto at : {0x35U, 0x37U, 0x38U})
        memory.put8(record + at, 0);
    memory.put16(record + 0x3a, 0xffff);
    for (std::uint32_t at = 0x70; at < 0x8e; at += 2)
        memory.put16(record + at, 0);
    memory.put16(record + 0x8e, 1);
    memory.put8(record + 0x36, 0);
    memory.put16(record + 0x1e, 0xffff);
    memory.put32(record + 0x10, memory.u32(memory.u32(record + 8)));
    memory.put16(record + 0x42, 0);
    memory.put16(record + 0x40, 0);
    for (const auto at : {0x50U, 0x54U, 0x4cU})
        memory.put32(record + at, 0);
    memory.put8(record + 0x23, 0);
    memory.put16(record + 0x10a, memory.u16(0x800c3e30));
    const auto script = memory.u32(record + 0x10);
    if (script == 0)
        return;
    resident::heap_select_tag(resident.heap, 4, 0); // 80032498
    const auto add = [&](std::uint32_t to, std::uint32_t from) {
        memory.put16(record + to, memory.u16(record + to) + memory.u16(record + from));
    };
    add(0x70, 0x76);
    add(0x72, 0x78);
    add(0x74, 0x7a);
    add(0x7c, 0x82);
    add(0x80, 0x86);
    add(0x7e, 0x84);
    const auto root = memory.u32(record + 4);
    for (std::uint32_t axis = 0; axis < 3; ++axis)
        memory.put16(root + 0x54 + 2 * axis,
                     memory.u16(root + 0x54 + 2 * axis) +
                         static_cast<std::uint32_t>(memory.s16(record + 0x70 + 2 * axis) >> 3));
    field::GteVector velocity{};
    for (std::uint32_t axis = 0; axis < 3; ++axis)
        velocity[axis] = static_cast<std::int16_t>(
            low(memory.s16(record + 0x7c + 2 * axis), memory.s16(root + 0x4c + 2 * axis)) >> 12);
    auto &gte = resident.gte;
    gte.transform.r = battle_matrix(memory, root + 0x2c).r;
    gte.set_vector(0, velocity);
    gte.mvmva(0, 0, 3);
    for (std::uint32_t axis = 0; axis < 3; ++axis)
        memory.put32(root + 0x5c + 4 * axis,
                     static_cast<std::uint32_t>(
                         (low(memory.s16(record + 0x1c), gte.mac(1 + axis)) >> 12) +
                         static_cast<std::int32_t>(memory.u32(root + 0x5c + 4 * axis))));
    if ((memory.u16(script) & 0xffU) != 0)
        throw MissingDependency({"battle_stage_setup", 0x800ab10c, {}, {}},
                                "symbol:battle-motion-commands", false,
                                "Stage motion commands other than the stop are not reconstructed");
}

// 8009ef3c(root, scale): each part's local rotation from its angles
// (RotMatrixYXZ 8004a92c when +6 is set, else 8003f738) with its position;
// the root's placed rotation is that rotation times the diagonal scale
// (MulMatrix0 8004920c through 1f800000), a part's without a parent its
// local matrix. The parts' changed marks (+4, +5) end cleared.
void pose_parts(battle::BattleMemory &memory, ResidentState &resident, std::uint32_t root,
                std::int32_t scale) {
    const auto &trigonometry = resident.math.trigonometry;
    const auto rotate = [&](std::uint32_t part, std::uint32_t to) {
        const field::GteVector angles{s16(memory.u16(part + 0x54)), s16(memory.u16(part + 0x56)),
                                      s16(memory.u16(part + 0x58))};
        put_rotation(memory, to,
                     memory.u8(part + 6) != 0 ? field::rotation_matrix_yxz(angles, trigonometry)
                                              : field::rotation_matrix(angles, trigonometry).r);
    };
    const auto count = memory.u16(root + 0xa);
    for (std::uint32_t i = 0; i < 3; ++i)
        memory.put32(root + 0x40 + 4 * i, memory.u32(root + 0x5c + 4 * i));
    rotate(root, root + 0x2c);
    const auto scaled = [&](std::uint32_t at) {
        return static_cast<std::int16_t>(low(scale, memory.s16(root + at)) >> 12);
    };
    field::GteMatrix placed{};
    placed.r = {scaled(0x4c), 0, 0, 0, scaled(0x4e), 0, 0, 0, scaled(0x50)};
    const auto local = battle_matrix(memory, root + 0x2c);
    resident.gte.transform.r = local.r;
    field::multiply_rotation(local, placed);
    put_rotation(memory, root + 0xc, placed.r);
    memory.put16(root + 0x1e, static_cast<std::uint16_t>(placed.pad));
    for (std::uint32_t i = 0; i < 3; ++i)
        memory.put32(root + 0x20 + 4 * i, memory.u32(root + 0x40 + 4 * i));
    for (std::uint32_t k = 1; k < count; ++k) {
        const auto part = root + 0x7c * k;
        if (memory.u8(part + 5) != 0) {
            rotate(part, part + 0xc);
            memory.put8(part + 5, 0);
        }
        const auto parent = memory.u32(part);
        if (parent != 0 && memory.u8(parent + 4) == 1)
            memory.put8(part + 4, 1);
        if (memory.u8(part + 4) == 0)
            continue;
        for (std::uint32_t i = 0; i < 3; ++i)
            memory.put32(part + 0x20 + 4 * i, memory.u32(part + 0x5c + 4 * i));
        if (parent != 0)
            throw MissingDependency(
                {"battle_stage_setup", 0x8009f108, {}, {}}, "symbol:battle-part-parent", false,
                "Stage parts under a parent (CompMatrix) are not reconstructed");
        for (std::uint32_t at = 0; at < 0x20; at += 4)
            memory.put32(part + 0x2c + at, memory.u32(part + 0xc + at));
    }
    for (std::uint32_t k = 1; k < count; ++k)
        memory.put8(root + 0x7c * k + 4, 0);
}

// 8002709c for a light object (type 1) of the scene data: 34ch bytes of 16
// shaded POLY_FT4 packets with the object's CLUT (+1a, +1c) and its fields
// at +320..+34b; without fog colors +344 is zero. GetDrawEnv (80044e64)
// copies the draw environment only into its own frame.
std::uint32_t build_light(battle::BattleMemory &memory, resident::Heap &heap,
                          std::uint32_t object) {
    resident::heap_select_tag(heap, 4, 0); // 80032498
    const auto field = [&](std::uint32_t at) { return memory.u16(object + at); };
    const auto block = allocate(memory, heap, 0x34c, 0, 0x800270fc);
    const auto tail = block + 0x320;
    memory.put32(tail + 8, field(0x14));
    memory.put32(tail + 0xc, field(0x16));
    memory.put16(tail + 0x1c, field(0));
    memory.put16(tail + 0x1e, field(4));
    memory.put16(tail + 0x26, 0);
    memory.put16(tail + 0x28, field(0x24));
    memory.put16(tail + 0x2a, field(0x26));
    memory.put16(tail + 0x20, field(8));
    memory.put32(tail + 0x10, static_cast<std::int32_t>(memory.u32(object + 8)) < 0
                                  ? 0U - field(0x20)
                                  : field(0x20));
    memory.put16(tail + 0x16, field(0x12));
    memory.put16(tail + 0x14, field(0x10));
    memory.put16(tail + 0x18, field(0x1e));
    memory.put16(tail + 0x1a, field(0x12) & 0xffU); // its remainder by 100h
    const auto clut = (field(0x1c) << 6U | (field(0x1a) >> 4U & 0x3fU)) & 0xffffU; // GetClut
    for (std::uint32_t i = 0; i < 16; ++i) {
        const auto packet = block + i * 0x28;
        memory.put8(packet + 3, 9); // SetPolyFT4 80043cb0
        memory.put8(packet + 7, 0x2c);
        memory.put8(packet + 7, memory.u8(packet + 7) | 1); // SetShadeTex 80043c24
        memory.put16(packet + 0xe, clut);
    }
    memory.put16(tail + 0x24, 0);
    return block;
}
} // namespace

// 80032e88(item, mode): allocate the item's size (its first word) and decode
// it there (80032eb4).
std::uint32_t Program::unpack_battle_item(battle::Battle &context, std::uint32_t item,
                                          std::uint32_t mode) {
    auto &memory = context.memory;
    // Interrupts recorded before the decode began.
    deliver_leading_arrivals();
    const auto size = memory.u32(item);
    auto block = resident::heap_allocate(resident.heap, size, mode, 0x80032e94);
    if (!block)
        throw battle::BattleError("A quiet null allocation in 80032e88 is not reconstructed");
    // The decoder reads a flag past an item that ends its block: the next
    // heap header.
    const auto tail = memory.tail(item);
    std::vector<std::uint8_t> source(tail.begin(), tail.end());
    for (std::uint32_t i = 0; i < 8; ++i)
        source.push_back(static_cast<std::uint8_t>(original_byte(
            memory, resident.heap, item + static_cast<std::uint32_t>(tail.size()) + i)));
    const auto decoded = field::decode_packed_block(source);
    if (decoded.data.size() != size || block->bytes.size() < size)
        throw battle::BattleError("An archive item decodes to another size than it declares");
    std::ranges::copy(decoded.data, block->bytes.begin());
    const auto address = block->address;
    memory.regions.emplace(address, std::move(block->bytes));
    deliver_arrivals(0x80032f4c); // interrupts that arrived while it decoded
    return address;
}

void Program::release_battle_block(battle::Battle &context, std::uint32_t address,
                                   std::uint32_t call_site) {
    resident::HeapBlock block{address, {}};
    if (address != 0) {
        const auto found = context.memory.regions.find(address);
        if (found == context.memory.regions.end())
            throw battle::BattleError("A released block is not battle memory");
        block.bytes = std::move(found->second);
        context.memory.regions.erase(found);
    }
    if (resident::heap_release(resident.heap, block, call_site) == -1)
        context.memory.regions.emplace(address, std::move(block.bytes)); // kept
}

void Program::load_battle_image(battle::Battle &context, FrameServices &services,
                                std::uint32_t rect, std::uint32_t source) {
    auto &memory = context.memory;
    std::array<std::int16_t, 4> area{s16(memory.u16(rect)), s16(memory.u16(rect + 2)),
                                     s16(memory.u16(rect + 4)), s16(memory.u16(rect + 6))};
    static_cast<void>(load_image(area, rect, source, &services));
    memory.put16(rect + 4, static_cast<std::uint16_t>(area[2]));
    memory.put16(rect + 6, static_cast<std::uint16_t>(area[3]));
}

// 8002dde4(images, 0, ...): each image section (1100 palette, 1101 pixels)
// at its own position; `frame` is 8002dde4's stack pointer, its rectangle at
// +10.
void Program::upload_battle_images(battle::Battle &context, FrameServices &services,
                                   std::uint32_t images, std::uint32_t frame) {
    auto &memory = context.memory;
    auto at = images + (memory.u32(images) + 1) * 4;
    for (std::uint32_t section = 0; section < memory.u32(images); ++section) {
        const auto kind = memory.u32(at);
        if (kind != 0x1100 && kind != 0x1101)
            break;
        std::array<std::int16_t, 4> area{
            static_cast<std::int16_t>(s16(memory.u16(at + 4)) + s16(memory.u16(at + 8))),
            static_cast<std::int16_t>(s16(memory.u16(at + 6)) + s16(memory.u16(at + 10))),
            s16(memory.u16(at + 12)), s16(memory.u16(at + 14))};
        static_cast<void>(load_image(area, frame + 0x10, at + 16, &services));
        // The next section follows the rectangle as LoadImage left it.
        at += 16 + static_cast<std::uint32_t>(static_cast<std::int32_t>(area[2]) * area[3] * 2);
    }
}

// 800a8bf0(1f, c4, stage, stage, 0, 0, 0, 0, 0): model record 1f (11ch
// bytes, 800d3368) of the stage file (relocated by 8003342c: +4 images, +8
// model group, +c hierarchy, +10 motion data): its images uploaded
// (8002dde4) without a texture offset, the model group copied (800c3b70,
// from the top) and listed in the first free slot of 800c3acc (8009eba8),
// the parts (8009ec4c), two semi-transparent POLY_FT4 packets in the scene's
// colors (+478), then the group trimmed after its first model's primitive
// data (8002c644, 80031f70), unrelocated (8002c4bc), copied into a block of
// its size and listed again in place of the original (8009f794, 8009eba8).
// `frame` is 801e7210's stack pointer.
void Program::register_stage_model(battle::Battle &context, FrameServices &services,
                                   std::uint32_t frame, std::uint32_t stage) {
    auto &memory = context.memory;
    auto &heap = resident.heap;
    resident::heap_select_tag(heap, 4, 0); // 80032498
    if (memory.u32(stage_record) != 0)
        return;
    const auto record = allocate(memory, heap, 0x11c, 0, 0x800a8c8c);
    relocate_table(memory, stage);
    relocate_table(memory, memory.u32(stage + 0x10));
    memory.put8(record + 0x62, 0);
    memory.put8(record + 0x63, 0);
    const auto header = memory.u32(memory.u32(stage + 0x10) + 4);
    const auto geometry = memory.u32(stage + 8);
    const auto hierarchy = memory.u32(stage + 0xc);
    memory.put32(stage_record, record);
    memory.put16(record + 0x24, memory.u16(header + 2));
    memory.put16(record + 0x26, memory.u16(header + 4));
    memory.put16(record + 0x28, memory.u16(header + 6));
    memory.put8(record + 0x2a, memory.u8(header + 10));
    memory.put16(record + 0x4a, memory.u16(header + 0xc));
    if ((memory.u16(header + 0xc) & 0x200U) != 0)
        throw MissingDependency({"battle_stage_setup", 0x800a8dd4, {}, {}},
                                "symbol:battle-stage-80030988", false,
                                "Stage models with flag 200 (80030988) are not reconstructed");
    upload_battle_images(context, services, memory.u32(stage + 4), frame - 0xc8 - 0x48);
    const auto size = hierarchy - geometry;
    const auto group = allocate(memory, heap, size, 1, 0x800a8e50);
    memory.put32(0x800c3b70, group);
    copy(memory, heap, group, geometry, size);
    std::uint32_t free = 0;
    while (memory.u32(0x800c3acc + free * 8) != 0 && ++free < 0x14) {
    }
    memory.put32(0x800c3b6c, free);
    const auto slot = 0x800c3acc + free * 8;
    list_models(memory, heap, group, slot);
    memory.put32(record, slot);
    memory.put32(record + 4, build_parts(memory, resident, slot, hierarchy, group));
    for (const auto at : {0x90U, 0x92U, 0x94U, 0x96U})
        memory.put16(record + at, 0);
    memory.put32(record + 0xac, 0);
    for (std::uint32_t packet = 0; packet < 2; ++packet) {
        const auto p = record + 0xb8 + packet * 0x28;
        memory.put8(p + 3, 9); // SetPolyFT4 80043cb0
        memory.put8(p + 7, 0x2c);
        memory.put8(p + 7, memory.u8(p + 7) | 2); // SetSemiTrans 80043bfc
        for (std::uint32_t c = 0; c < 3; ++c)
            memory.put8(p + 4 + c, memory.u8(resident.battle_scene_data + 0x478 + c));
        memory.put16(p + 0xe, 0x1ccU << 6U | 0x30U >> 4U); // GetClut(30, 1cc)
        memory.put16(p + 0x16, gpu::texture_page(0, 2, 0x380, 0));
        for (const auto [at, value] : {std::pair{0xcU, 0xc0U},
                                       {0xdU, 0xc0U},
                                       {0x14U, 0xfeU},
                                       {0x15U, 0xc0U},
                                       {0x1cU, 0xc0U},
                                       {0x1dU, 0xfeU},
                                       {0x24U, 0xfeU},
                                       {0x25U, 0xfeU}})
            memory.put8(p + at, value);
    }
    memory.put16(record + 0x1c, memory.u16(header + 8));
    memory.put8(record + 0x10c, memory.u8(header + 0xe));
    if (memory.u8(record + 0x10c) != 0)
        throw MissingDependency({"battle_stage_setup", 0x800a9130, {}, {}},
                                "symbol:battle-stage-800aa6e0", false,
                                "Stage model records (800aa6e0) are not reconstructed");
    memory.put8(record + 0x10e, memory.u8(header + 0x10));
    if (memory.u8(record + 0x10e) != 0)
        throw MissingDependency({"battle_stage_setup", 0x800a915c, {}, {}},
                                "symbol:battle-stage-effects", false,
                                "Stage model effect records are not reconstructed");
    memory.put8(record + 0x10d, memory.u8(header + 0x12));
    if (memory.u8(record + 0x10d) != 0)
        throw MissingDependency({"battle_stage_setup", 0x800a932c, {}, {}},
                                "symbol:battle-stage-800a7064", false,
                                "Stage model sprites (800a7064) are not reconstructed");
    memory.put8(record + 0x22, 0);
    memory.put8(record + 0x20, 0x1f);
    memory.put8(record + 0x34, 1); // entries past 10 have no slot flag (800c3eb7)
    // 8002c644: the group keeps its models up to the first one's primitive
    // data (+24).
    if (const auto flags = memory.u32(group + 4); (flags & 2U) == 0) {
        memory.put32(group + 4, flags | 2U);
        const auto end = memory.u32(group + 0x24) - group;
        resident::HeapBlock block{group, std::move(memory.regions.at(group))};
        memory.regions.erase(group);
        static_cast<void>(resident::heap_trim(heap, block, end));
        memory.regions.emplace(group, std::move(block.bytes));
    }
    rebase_model_group(memory, group, false);
    const auto kept = heap.headers.at(group - 8)[0] - group - 8; // 80031894
    const auto models = allocate(memory, heap, kept, 0, 0x800a94b8);
    copy(memory, heap, models, group, kept);
    release_battle_block(context, group, 0x800a94e0);
    // 8009f794(slot, 0): the slot's table is released.
    if (const auto table = memory.u32(slot); table != 0) {
        release_battle_block(context, table, 0x8009f81c);
        memory.put32(slot, 0);
    }
    list_models(memory, heap, models, slot);
    memory.put32(record + 0xa8, models);
}

// 801e7210(8005949c, 80059520 word, stage, origin, colors, tint): without a
// stage it returns zero. Otherwise the stage globals are cleared, the stage
// model registered (800a8bf0), its texture bounds taken (801e70e8) and its
// parts placed from the stage's position table (+14); the scene data moves
// into a new block (800658c8, 8005949c) and its motion section (+514,
// relocated) starts the record's motion (800aa898, 800aa934) before the
// parts are posed (8009ef3c); the origin (9 halfwords) and color matrix come
// from the scene data (+464, +46c) with the GTE color matrix and back color
// (+474); its six objects (+360, 28h each) make lights (type 1, 8002709c)
// and set +35e (type 5), which is the result and copies the fog color
// (+458) to `tint`; the actor and light lists (+50c, +510) are published
// (801e7ec4). After DrawSync the stage file is released.
std::uint32_t Program::battle_stage_setup(FrameServices &services, std::uint32_t stack,
                                          std::uint32_t stage, std::uint32_t origin,
                                          std::uint32_t colors, std::uint32_t tint) {
    deliver_leading_arrivals();
    std::uint32_t result = 0;
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        auto &heap = resident.heap;
        const auto scene = resident.battle_scene;
        if (stage == 0 || scene == 0)
            return;
        resident::heap_select_tag(heap, 4, 0); // 80032498
        for (const auto address : {0x800c3e38U, 0x800c3ea0U, 0x800d3344U, 0x800d39ccU, 0x800d3348U,
                                   lights + 4, lights, light_records + 0x18, light_records})
            memory.put32(address, 0);
        memory.put16(0x800d361a, 0);
        resident::heap_set_keep(heap, stage, false); // 800320b8
        const auto frame = stack - 0xa0;
        register_stage_model(context, services, frame, stage);
        texture_bounds(memory, memory.u32(stage + 4));
        const auto record = memory.u32(stage_record);
        const auto root = memory.u32(record + 4);
        memory.put32(0x800c3e38, root);
        memory.put32(0x800c3e48, memory.u32(record));
        auto position = memory.u32(stage + 0x14);
        for (std::uint32_t k = 1; k < memory.u16(root + 0xa); ++k, position += 8) {
            const auto part = root + 0x7c * k;
            for (std::uint32_t axis = 0; axis < 3; ++axis)
                memory.put32(part + 0x5c + 4 * axis,
                             static_cast<std::uint32_t>(memory.s16(position + 2 * axis)));
            memory.put16(part + 0x52, memory.u16(position + 6));
        }
        const auto size = memory.u32(scene - 4);
        const auto data = allocate(memory, heap, size, 0, 0x801e73b8);
        resident.battle_scene_data = data;
        copy(memory, heap, data, scene, size);
        resident::heap_set_keep(heap, scene - 4, false); // 800320b8
        release_battle_block(context, scene - 4, 0x801e73ec);
        const auto actors = memory.u32(data + 0x50c) == 0 ? 0 : data + memory.u32(data + 0x50c);
        const auto light_list = data + memory.u32(data + 0x510);
        const auto light_entries = memory.u32(data + 0x510) == 0 ? 0 : light_list + 4;
        const auto section = data + memory.u32(data + 0x514);
        relocate_table(memory, section);
        const auto table = memory.u32(section + 4);
        start_stage_motion(memory, resident, record, section + 8);
        pose_parts(memory, resident, root, memory.s16(record + 0x1c));
        const auto first = table + memory.u32(table);
        memory.put16(0x800d2fc8, memory.u16(first));
        for (std::uint32_t i = 0; i < 3; ++i)
            memory.put16(origin + 2 * i, memory.u16(data + 0x464 + 2 * i));
        for (std::uint32_t at = 6; at < 0x12; at += 2)
            memory.put16(origin + at, 0);
        for (std::uint32_t row = 0; row < 3; ++row) {
            memory.put16(colors + 6 * row, memory.u16(data + 0x46c + 2 * row));
            memory.put16(colors + 6 * row + 2, 0);
            memory.put16(colors + 6 * row + 4, 0);
        }
        memory.put32(0x800d2fd0, first + 2);
        memory.put32(0x800d2fc0, colors);
        auto &gte = resident.gte;
        for (std::uint32_t k = 0; k < 5; ++k) // SetColorMatrix 80049f5c
            gte.set_control(16 + k, memory.u32(colors + 4 * k));
        for (std::uint32_t k = 0; k < 3; ++k) // SetBackColor 8004a0ec
            gte.set_control(13 + k, memory.u8(data + 0x474 + k) << 4U);
        std::uint32_t made = 0;
        std::uint32_t placed = 0;
        for (std::uint32_t k = 0; k < 6; ++k) {
            const auto object = data + 0x360 + k * 0x28;
            switch (memory.u16(object + 0x18)) {
            case 1:
            case 2:
                if (memory.u32(lights + made * 4) == 0 && made < 2) {
                    if (memory.u16(object + 0x18) == 2)
                        throw MissingDependency({"battle_stage_setup", 0x801e76b8, {}, {}},
                                                "symbol:battle-fog-light", false,
                                                "Lights in the fog colors are not reconstructed");
                    memory.put32(lights + made * 4, build_light(memory, heap, object));
                }
                ++made;
                break;
            case 3:
                if (memory.u32(0x800c3ea0) == 0)
                    throw MissingDependency({"battle_stage_setup", 0x801e7758, {}, {}},
                                            "symbol:battle-stage-801e7914", false,
                                            "Stage object type 3 (801e7914) is not reconstructed");
                break;
            case 5:
                memory.put8(data + 0x35e, 1);
                break;
            case 7:
                if (memory.u32(light_records + placed * 0x18) == 0 && placed < 2 && k + 1 < 4)
                    throw MissingDependency({"battle_stage_setup", 0x801e77f4, {}, {}},
                                            "symbol:battle-stage-80027d64", false,
                                            "Stage object type 7 (80027d64) is not reconstructed");
                ++placed;
                break;
            default:
                break;
            }
        }
        result = memory.u8(data + 0x35e);
        if (result != 0 && tint != 0)
            for (std::uint32_t c = 0; c < 3; ++c)
                memory.put8(tint + c, memory.u8(data + 0x458 + c));
        // 801e7ec4(actors, light entries, light count).
        if (actors != 0 && light_entries != 0) {
            const auto count = memory.u32(light_list);
            memory.put32(0x800d3344, actors);
            memory.put32(0x800d39cc, light_entries);
            memory.put32(0x800d3348, count);
            memory.put8(0x800d2f64, 1);
            for (std::uint32_t i = 0;
                 static_cast<std::int32_t>(i) < static_cast<std::int32_t>(memory.u32(0x800d3348));
                 ++i)
                memory.put8(memory.u32(0x800d39cc) + 0xe * i + 0xd, 0);
            if (count == 0) {
                memory.put32(0x800d3344, 0);
                memory.put32(0x800d39cc, 0);
            }
        }
        for (std::uint32_t i = 0; i < 4; ++i)
            memory.put8(0x800d2d10 + i, memory.u8(resident.battle_scene_data + 0x340 + i));
        // The uploads' completions and the frame's interrupts before DrawSync.
        deliver_pending_arrivals();
        draw_sync(services);
        release_battle_block(context, stage, 0x801e78cc);
        resident.battle_scene = data;
    });
    return result;
}

// 801e5384: the party (8006f368 filtered by the availability mask
// 8006f364 & 8006f366, or the demonstration party), each member's record and
// gear record from the game data (8006d8a0, 8006dfac; 0xa4 each), the setup
// archive's contents (relocated in place by 8003342c), the uploads of its
// images (8002dde4, 80033698, 80078310), then the enemy data files of the
// formation's enemy set (directory 13 files 2n+2 and 2n+3) by a list read.
void Program::setup_battle_party(battle::Battle &context, FrameServices &services,
                                 std::uint32_t stack) {
    auto &memory = context.memory;
    constexpr std::uint32_t ids = 0x800d2d24;
    const bool demo = (memory.u8(battle::formation_record + 1) & 0x10) != 0;
    memory.put8(0x800d3294, demo ? 1 : 0);
    const auto available = memory.u16(0x8006f364) & memory.u16(0x8006f366);
    if (demo) {
        memory.put8(ids + 1, 10);
        memory.put8(ids + 2, 10);
        memory.put8(ids, memory.u8(0x8006f368) & 0x7f);
    } else {
        std::uint32_t count = 0;
        for (std::uint32_t candidate = 0; candidate < 3; ++candidate) {
            const auto id = memory.u8(0x8006f368 + candidate);
            // 80089c9c: the character's bit (800c3448) within the mask.
            if (id < 0x10 && (memory.u16(0x800c3448 + id * 2) & available & 0x7ff) != 0)
                memory.put8(ids + count++, id & 0x7f);
        }
        for (; count < 3; ++count)
            memory.put8(ids + count, 0x7f);
    }
    const auto archive = resident.battle_archive;
    relocate_table(memory, archive);
    const auto item = [&](std::uint32_t offset) { return memory.u32(archive + offset); };
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto id = memory.u8(ids + member);
        if (id == 0x7f)
            continue;
        const auto record = 0x800ccce8 + member * battle::record_stride;
        copy(memory, resident.heap, record, 0x8006d8a0 + id * 0xa4, 0xa4);
        if (memory.u8(0x800d3294) != 0 && member - 1 < 2)
            memory.put8(record + 0xa0, 0x11);
        auto gear = memory.u8(record + 0xa0);
        if (gear == 0xff)
            gear = 0;
        copy(memory, resident.heap, record + 0xa4, 0x8006dfac + gear * 0xa4, 0xa4);
        auto block = unpack_battle_item(context, item(id * 4 + 0x14), 1);
        copy(memory, resident.heap, 0x800cdd40 + member * 0x5f0, block, 0x5f0);
        release_battle_block(context, block, 0x801e55cc);
        block = unpack_battle_item(context, item(gear * 4 + 0x44), 1);
        copy(memory, resident.heap, 0x800cef10 + member * 0x690, block, 0x690);
        release_battle_block(context, block, 0x801e55f8);
    }
    auto block = unpack_battle_item(context, item(0x10), 1);
    copy(memory, resident.heap, 0x800d02c0, block, 8000);
    release_battle_block(context, block, 0x801e5644);
    block = unpack_battle_item(context, item(0xc), 1);
    copy(memory, resident.heap, 0x800d2200, block, 0x300);
    release_battle_block(context, block, 0x801e566c);
    // 8002dde4's frame: 801e5840 -18, 801e5384 -48, 8002dde4 -48.
    block = unpack_battle_item(context, item(8), 1);
    upload_battle_images(context, services, block, stack - 0xa8);
    release_battle_block(context, block, 0x801e56a4);
    memory.put32(0x800d2f5c, unpack_battle_item(context, item(4), 0));
    // 80033698(0, 1f0): the text palettes (80050190) and their CLUT ids; its
    // rectangle lies in its frame (801e5384 -48 -28, +10).
    {
        std::array<std::int16_t, 4> area{0, 0x1f0, 0x20, 1};
        static_cast<void>(load_image(area, stack - 0x78, 0x80050190, &services));
        resident.text_cluts[0] = static_cast<std::uint16_t>(0x1f0U << 6U | 0U);
        resident.text_cluts[1] = static_cast<std::uint16_t>(0x1f0U << 6U | 1U);
    }
    memory.put32(0x800d329c, unpack_battle_item(context, item(0x40), 0));
    // 80078310(portraits, 61): each member's portrait image (0x460 apart;
    // 0xb for the demonstration's members 1 and 2) placed by its glyph
    // (80026338 over the glyph table 800d2f5c, id 61 + member).
    block = unpack_battle_item(context, item(0x90), 1);
    for (std::uint32_t member = 0; member < 3; ++member) {
        auto id = memory.u8(ids + member);
        if (id == 0x7f)
            continue;
        if (memory.u8(0x800d3294) != 0 && member - 1 < 2)
            id = 0xb;
        resident.gpu.tim_cursor = block + id * 0x460; // OpenTIM
        // ReadTIM (80047518): magic 10, flags, then the palette and pixel
        // blocks, each a length word and a rectangle.
        const auto tim = resident.gpu.tim_cursor;
        if (memory.u32(tim) != 0x10)
            throw battle::BattleError("A portrait is not a TIM image");
        if (resident.gpu.debug == 2)
            throw MissingDependency({"read_tim", 0x80047570, {}, {}}, "symbol:printf-80019964",
                                    false, "libgpu debug messages are not reconstructed");
        auto at = tim + 8;
        std::uint32_t palette = 0;
        std::uint32_t words = 0;
        if ((memory.u32(tim + 4) & 8) != 0) {
            palette = at + 4;
            words = memory.u32(at) >> 2;
            at += words << 2;
        }
        const auto pixels = at + 4;
        resident.gpu.tim_cursor += (words + (memory.u32(at) >> 2) + 2) * 4;
        // 80026338: the glyph's rectangle fields.
        const auto table = memory.u32(0x800d2f5c);
        const auto glyph = table + memory.u16(table + 4 + (0x61 + member) * 2);
        const auto shift = memory.u16(glyph + 20) == 0 ? 20 : 18;
        const auto offset = static_cast<std::int32_t>(memory.u16(glyph + 4) << 16) >> shift;
        if (palette == 0)
            throw battle::BattleError("A portrait without a palette is not reconstructed");
        memory.put16(palette, memory.u16(glyph + 22));
        memory.put16(palette + 2, memory.u16(glyph + 24));
        memory.put16(pixels,
                     static_cast<std::uint32_t>(s16(memory.u16(glyph + 26) & 0xffc0) + offset +
                                                static_cast<std::int32_t>(member) * 6));
        memory.put16(pixels + 2, static_cast<std::uint32_t>(s16(memory.u16(glyph + 28) & 0xff00) +
                                                            s16(memory.u16(glyph + 6))));
        load_battle_image(context, services, palette, palette + 8);
        load_battle_image(context, services, pixels, pixels + 8);
        draw_sync(services);
    }
    release_battle_block(context, block, 0x801e56fc);
    block = unpack_battle_item(context, item(0x94), 1);
    copy(memory, resident.heap, 0x800d2500, block + 800, 0x300);
    release_battle_block(context, block, 0x801e5724);
    memory.put32(0x800d39f0, unpack_battle_item(context, item(0x98), 0));
    release_battle_block(context, archive, 0x801e5748);
    // The enemy data files of the formation's enemy set.
    static_cast<void>(select_directory(0xc, 1));
    const auto set = memory.u8(battle::formation_record);
    const auto data = battle::allocate_block(context, resident, file_words(set * 2 + 2), 0);
    memory.put32(0x800c3dd0, data);
    memory.put32(0x800d33ec, data);
    memory.put16(0x800d33e8, set * 2 + 2);
    const auto models = battle::allocate_block(context, resident, file_words(set * 2 + 3), 1);
    memory.put32(0x800c3dec, models);
    memory.put32(0x800d33f4, models);
    memory.put16(0x800d33f8, 0);
    memory.put32(0x800d33fc, 0);
    memory.put16(0x800d33f0, set * 2 + 3);
    // 80029afc(800d33e8, 0, 80): the list is the reader's while it runs.
    resident.disc_read.list = {0x800d33e8, memory.take(0x800d33e8, 0x12)};
    static_cast<void>(read_files(0));
}

// Setup phase 3: 801e6290 (801e5924, 801e5ee8) then 801e62b8 (801e5d2c,
// 801e5e78), over the graphics block (800c3ea4) and the UI block (800d2d28).
void Program::setup_battle_panels(battle::Battle &context) {
    auto &memory = context.memory;
    const auto ui = memory.u32(0x800d2d28);
    const auto graphics = memory.u32(0x800c3ea4);
    const auto word = [&](std::uint32_t offset) {
        return static_cast<std::int32_t>(memory.u32(graphics + offset));
    };
    // 801e5924: the gauge panels.
    for (std::uint32_t member = 0; member < 3; ++member)
        memory.put8(ui + 0x7c + member, 1);
    // 80026338(800d2f5c, 5c, ...): glyph 5c's fields at +a234..+a248.
    battle::glyph_fields(context, memory.u32(0x800d2f5c), 0x5c, graphics + 0xa234);
    // GetClut 80043a58 on the rows above the glyph's CLUT.
    for (const auto [at, row] :
         {std::pair{0xa2aeU, 0}, {0xa2acU, -1}, {0xa2b2U, -2}, {0xa2b0U, -3}})
        memory.put16(graphics + at, static_cast<std::uint32_t>(word(0xa240) + row) << 6 |
                                        (static_cast<std::uint32_t>(word(0xa23c) >> 4) & 0x3f));
    const auto page =
        gpu::texture_page(memory.u32(graphics + 0xa238), 0, word(0xa244), word(0xa248));
    for (std::uint32_t i = 0; i < 8; ++i) {
        // SetPolyGT4 80043cd8, SetShadeTex(0): lit 80 at the top, black below.
        const auto quad = graphics + 0x5a0 + i * 0x34;
        memory.put8(quad + 3, 0xc);
        memory.put8(quad + 7, 0x3c & 0xfe);
        for (const auto at : {4U, 5U, 6U, 0x10U, 0x11U, 0x12U})
            memory.put8(quad + at, 0x80);
        for (const auto at : {0x1cU, 0x1dU, 0x1eU, 0x28U, 0x29U, 0x2aU})
            memory.put8(quad + at, 0);
        memory.put16(quad + 0x1a, page);
        // SetPolyG4 80043cc4: grey 4f at the top.
        const auto shade = graphics + 0x740 + i * 0x24;
        memory.put8(shade + 3, 8);
        memory.put8(shade + 7, 0x38);
        for (const auto at : {0x14U, 0x15U, 0x16U, 0x1cU, 0x1dU, 0x1eU})
            memory.put8(shade + at, 0x4f);
    }
    for (std::uint32_t i = 0; i < 12; ++i) { // SetLineF2 80043d78, white
        const auto line = graphics + 0x908 + i * 0x10;
        memory.put8(line + 3, 3);
        memory.put8(line + 7, 0x40);
        for (const auto at : {4U, 5U, 6U})
            memory.put8(line + at, 0xff);
    }
    // 801e5ee8: each present member's gauge glyphs (52, then 53 dimmed by
    // 80076c34) at half scale (80076a6c), counting parts in UI +78, then its
    // portrait (61 + member) and two digit glyphs (90, 91, parts at +1e2 and
    // +1e3), placed from the column table 800c3254 by layout 800d3280.
    const auto buffer = memory.u32(0x800ccb34);
    for (std::uint32_t member = 0; member < 3; ++member) {
        if (memory.u8(0x800c3eb6 + member * 0x1c) == 0x7f)
            continue;
        const auto x =
            memory.u16(0x800c3254 + (memory.u8(0x800d3280) * 3 + member) * 2) + member * 0x60;
        const auto gauge = graphics + member * 0x1e0;
        const auto count = ui + 0x78 + member;
        memory.put8(count, memory.u8(count) + battle::draw_glyph(context, 0x52,
                                                                 gauge + memory.u8(count) * 0x50,
                                                                 x + 0x44, 0x24, 0x800));
        const auto first = memory.u8(count);
        memory.put8(count, first + battle::draw_glyph(context, 0x53, gauge + first * 0x50, x + 0x44,
                                                      0x24, 0x800));
        for (auto part = first * 2; part < memory.u8(count) * 2; part += 2) {
            // 80076c34: semi-transparent (80043bfc), textured (80043c24),
            // color 40, blending mode 1 (texture page bit 40).
            const auto sprite = gauge + (part + buffer) * 0x28;
            memory.put8(sprite + 7, (memory.u8(sprite + 7) | 2) & 0xfe);
            for (const auto at : {4U, 5U, 6U})
                memory.put8(sprite + at, 0x40);
            memory.put16(sprite + 0x16, memory.u16(sprite + 0x16) | 0x40);
        }
        battle::draw_glyph(context, 0x61 + member, graphics + 0x818 + member * 0x50, x + 0x1c, 0x14,
                           0x1000);
        const auto digits = graphics + 0x835c + member * 0x1e4;
        memory.put8(digits + 0x1e2,
                    battle::draw_glyph(context, 0x90, digits, x + 0x38, 0x27, 0x1000));
        memory.put8(digits + 0x1e3,
                    battle::draw_glyph(context, 0x91, digits + 0xa0, x + 0x3c, 0x27, 0x1000));
        memory.put8(digits + 0x1e0, buffer);
    }
    memory.put8(ui + 0xa2, buffer);
    memory.put8(ui + 0x83, buffer);
    // 801e5d2c: two white semi-transparent quads (SetPolyF4 80043c9c) and
    // their draw modes (SetDrawMode 800454dc, window 0, 0, 100, 100) at
    // blending mode 2.
    const std::array<std::int16_t, 4> window{0, 0, 0x100, 0x100};
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto quad = graphics + 0x63c8 + i * 0x18;
        memory.put8(quad + 3, 5);
        for (const auto at : {4U, 5U, 6U})
            memory.put8(quad + at, 0xff);
        memory.put8(quad + 7, 0x28 | 2);
        draw_mode_packet(graphics + 0x63f8 + i * 0xc,
                         gpu::texture_page(0, 2, word(0xa244), word(0xa248)), &window);
    }
    memory.put32(graphics + 0x6410, 0xff);
    memory.put8(graphics + 0x6415, 0);
    memory.put8(graphics + 0x6416, 0);
    // 801e5e78: ten text images (800c3e5c, 8008ac00(4)) of messages 0-9
    // (800338d8: the table at *(*80059360 + 48)), each drawn by 80034eac(text,
    // image, 2, 0): the resident text record 80059fd8 set to one line three
    // columns wide (+a; +8 its pixel width, +12 the image stride), the image
    // (+2c), its line record 8005a068 (+28) on the even plane, up to 100
    // glyphs (+69), then 80033df0.
    constexpr std::uint32_t record = 0x80059fd8;
    constexpr std::uint32_t width = 2 | 1;
    for (std::uint32_t i = 0; i < 10; ++i) {
        const auto image = battle::allocate_text_block(context, resident, 4);
        memory.put32(0x800c3e5c + i * 4, image);
        const auto table = memory.u32(memory.u32(0x80059360) + 0x48);
        memory.put32(record + 0x1c, table + memory.u16(table + 4 + i * 2));
        memory.put16(record, 0);
        memory.put16(record + 2, 0);
        memory.put16(record + 8, width << 2);
        memory.put16(record + 0xa, width);
        memory.put16(record + 0xc, 1);
        memory.put16(record + 0x10, 0);
        memory.put16(record + 0x12, width + 3);
        memory.put32(record + 0x28, 0x8005a068);
        memory.put32(record + 0x2c, image);
        memory.put8(record + 0x68, 1);
        memory.put8(record + 0x69, 100);
        memory.put8(record + 0x6a, 0);
        memory.put8(record + 0x6c, 0);
        memory.put16(record + 0x84, 0);
        memory.put16(0x8005a068 + 0x58, 0);
        memory.put8(0x8005a068 + 0x5a, 0);
        dialogue_glyphs(record);
    }
}

void Program::battle_prologue() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        for (const auto [pointer, size] :
             {std::pair{0x800c3ea4U, 0xa2b4U}, std::pair{0x800d2d28U, 0x10cU},
              std::pair{0x800c3eacU, 0x2f8U}})
            memory.put32(pointer, battle::allocate_block(context, resident, size, 0));
        for (const auto [pointer, size] :
             {std::pair{0x800c3ea4U, 0xa2b4U}, std::pair{0x800d2d28U, 0x10cU},
              std::pair{0x800c3eacU, 0x2f8U}})
            for (std::uint32_t i = 0; i < size; ++i) // 8003f8e8 bzero
                memory.put8(memory.u32(pointer) + i, 0);
        resident.b_5959c = 0;
        memory.put8(0x800c3e29, 0xff);
        memory.put8(0x800c3e28, 0xff);
        memory.put8(0x800d366c, 0);
        memory.put32(0x800c3e54, resident.music.current_sequence);
        if (resident.b_5947c != 0)
            throw MissingDependency({"battle_prologue", 0x8007100c, {}, {}},
                                    "symbol:battle-event-8005947c", false,
                                    "The event battle path of 80070f40 is not reconstructed");
        if (resident.battle_request.resident_flag != 0)
            throw MissingDependency({"battle_prologue", 0x80071064, {}, {}},
                                    "symbol:battle-module-801e0a34", false,
                                    "The 801e0000 module path of 80070f40 is not reconstructed");
        if (resident.debug_word != 0xffffffffU)
            throw MissingDependency({"battle_prologue", 0x8007110c, {}, {}},
                                    "symbol:battle-debug-80280000", false,
                                    "The debug module load of 80070f40 is not reconstructed");
        // 8003f99c(8006f9dc, 800658dc + selector * 20, 20).
        const auto source = battle::formation_table + resident.battle_request.selector * 0x20U;
        for (std::uint32_t i = 0; i < 0x20; ++i)
            memory.put8(battle::formation_record + i, memory.u8(source + i));
    });
}

void Program::battle_after_load() {
    run_battle([](battle::Battle &context) {
        battle::update_alive(context);
        context.memory.put8(0x800c3e4c, 2);
    });
}

void Program::battle_after_scene(std::uint32_t result) {
    run_battle([&](battle::Battle &context) {
        context.memory.put8(0x800c4a38, result);
        context.memory.put32(0x800d2d40, 0x800c4a39); // 800a5e9c
        context.memory.put32(0x800d2d48, 0x800c8aa9);
    });
}

// 8001bbac: in directory 12, the 4-byte marker block and a spacer block that
// ends at it (both from the top) place the setup module at 801e4000; the
// effect header (file 2) and the archive (file 3) get blocks; one list read
// brings files 2, 3 and 4 (801e4000). Once the first file has arrived (the
// status leaves 3), the effect header becomes the driver's effect bank
// (80038428) and each member's effect of the battle mode starts (80039db8)
// unless the mode is 4.
void Program::battle_setup_files() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        auto &heap = resident.heap;
        resident::heap_select_tag(heap, 2, 0); // 80032498
        static_cast<void>(select_directory(0xc, 0));
        const auto allocate = [&](std::uint32_t size, std::uint32_t site) {
            auto block = resident::heap_allocate(heap, size, 1, site);
            if (!block)
                throw battle::BattleError("A quiet null allocation in 8001bbac");
            const auto address = block->address;
            memory.regions.emplace(address, std::move(block->bytes));
            return address;
        };
        resident.battle_marker = allocate(4, 0x8001bbdc);
        resident.battle_spacer = allocate(resident.battle_marker + 0x7fe1c000U, 0x8001bbf4);
        resident.battle_effects = allocate(file_words(2), 0x8001bc14);
        resident.battle_archive = allocate(file_words(3), 0x8001bc2c);
        auto &read = resident.disc_read;
        std::vector<std::uint8_t> list(0x1a);
        if (read.list.address == 0x8006f9bc && read.list.bytes.size() >= 0x1a)
            list.assign(read.list.bytes.begin(), read.list.bytes.begin() + 0x1a);
        const auto put = [&](std::uint32_t at, std::uint32_t value, std::uint32_t width) {
            for (std::uint32_t i = 0; i < width; ++i)
                list[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
        };
        put(0, 2, 2);
        put(4, resident.battle_effects, 4);
        put(8, 3, 2);
        put(0xc, resident.battle_archive, 4);
        put(0x10, 4, 2);
        put(0x14, battle::setup_module_base, 4);
        put(0x18, 0, 2);
        memory.put32(battle::formation_record - 4, 0);
        read.list = {0x8006f9bc, std::move(list)};
        static_cast<void>(read_files(0));
        while (disc_busy() == 3)
            if (!deliver_interrupt())
                throw MissingDependency({"battle_setup_files", 0x8001bc9c, {}, {}},
                                        "interrupt:disc-read-completion", false,
                                        "Waiting for the first setup file needs its arrivals");
        // 80038428: the effect header becomes a driver object.
        const auto effects = resident.battle_effects;
        auto bytes = std::move(memory.regions.at(effects));
        memory.regions.erase(effects);
        resident.sound.objects.emplace(effects, std::move(bytes));
        resident::link_effect_bank(resident.sound, effects);
        const auto mode = resident.battle_request.mode;
        if (mode != 4)
            for (std::uint32_t member = 0; member < 3; ++member)
                if (const auto effect = memory.u8(0x8004f388 + mode * 3 + member); effect != 0xff)
                    resident::start_bank_effect(resident.sound, effects, effect);
    });
}

// 8001bb0c: 800379d8(scene, 0, 80059470, 80059520, 8005949c) for the
// formation's scene (8006f9de): in directory 15, when the scene is below half
// the count file 5 records, allocate the stage file (2n+6) and the scene data
// file (2n+7) from the top as kept blocks (tag 4) and start their list read;
// the scene data is the second file after its first word. The directory of
// the caller is restored (800284b4, 80028470).
void Program::battle_scene_files() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        auto &read = resident.disc_read;
        const auto scene = memory.u8(battle::formation_record + 2);
        // 800284b4: the caller's directory as a table row and column.
        std::uint32_t row = 0;
        std::uint32_t column = 0;
        for (std::uint32_t i = 0; i < 0x40; ++i)
            if (read.directories.size() >= 2 * i + 2 &&
                static_cast<std::uint32_t>(read.directories[2 * i] | read.directories[2 * i + 1]
                                                                         << 8U) ==
                    read.directory + 1U) {
                row = i / 4 * 4;
                column = i % 4;
                break;
            }
        static_cast<void>(select_directory(0xc, 3));
        auto &heap = resident.heap;
        resident::heap_select_tag(heap, 4, 0); // 80032498
        // 80028928(5): a negative record size is a count (its negated low
        // halfword).
        const auto record = static_cast<std::int32_t>(file_size(5));
        const auto count =
            record < 0 ? static_cast<std::int32_t>(static_cast<std::int16_t>(-record)) : 0;
        if (static_cast<std::int32_t>(scene) >= count / 2)
            throw MissingDependency({"battle_scene_files", 0x80037a68, {}, {}},
                                    "symbol:battle-scene-missing", false,
                                    "A scene past the scene count is not reconstructed");
        const auto allocate = [&](std::int32_t file, std::uint32_t site) {
            auto block = resident::heap_allocate(heap, file_words(static_cast<std::uint32_t>(file)),
                                                 1, site);
            if (!block)
                throw battle::BattleError("A quiet null allocation in 800379d8");
            const auto address = block->address;
            resident::heap_set_keep(heap, address, true); // 800320a4
            memory.regions.emplace(address, std::move(block->bytes));
            return address;
        };
        const auto data = allocate(static_cast<std::int32_t>(scene * 2 + 7), 0x80037a8c);
        const auto stage = allocate(static_cast<std::int32_t>(scene * 2 + 6), 0x80037ab0);
        std::vector<std::uint8_t> list(0x12);
        const auto put = [&](std::uint32_t at, std::uint32_t value, std::uint32_t width) {
            for (std::uint32_t i = 0; i < width; ++i)
                list[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
        };
        if (read.list.address == 0x8005a1dc && read.list.bytes.size() >= 0x12)
            list.assign(read.list.bytes.begin(), read.list.bytes.begin() + 0x12);
        put(0, scene * 2 + 6, 2);
        put(4, stage, 4);
        put(8, scene * 2 + 7, 2);
        put(0xc, data, 4);
        put(0x10, 0, 2);
        resident.scene_list_tail = 0;
        read.list = {0x8005a1dc, std::move(list)};
        static_cast<void>(read_files(0));
        resident.battle_stage = stage;
        resident.battle_stage_b = 0;
        resident.battle_scene = data + 4;
        resident.battle_scene_data = data + 4;
        static_cast<void>(select_directory(row, column));
    });
}

// 800b8098 up to its call of 8001bbac: the mode (800d36b8), then 800b8284:
// the geometry offset and both buffers' display and draw environments, with
// their display ranges.
void Program::battle_load_prologue(std::uint32_t mode) {
    battle->put8(0x800d36b8, mode);
    set_geometry_offset(0xa0, 0xa4); // 8004a12c
    set_default_display_environment(0x800c4a7c, 0, 0xe0, 0x140, 0xe0);
    set_default_display_environment(0x800c8aec, 0, 0, 0x140, 0xe0);
    set_default_draw_environment(0x800c4a20, 0, 0, 0x140, 0xe0);
    set_default_draw_environment(0x800c8a90, 0, 0xe0, 0x140, 0xe0);
    for (const auto [address, value] : {std::pair{0x800c8af6U, 10U},
                                        {0x800c4a86U, 10U},
                                        {0x800c8af8U, 0x100U},
                                        {0x800c4a88U, 0x100U},
                                        {0x800c8af4U, 0U},
                                        {0x800c4a84U, 0U},
                                        {0x800c8afaU, 0xd8U},
                                        {0x800c4a8aU, 0xd8U}})
        battle->put16(address, value);
}

// 800a8b0c (after the swirl's final disc wait, 80028a60): the effect globals,
// the effect lists of the scene data's counts (800a2234: +348 entries of 14h
// bytes at 800c3d0c; 800a2ca4: +34a + 1 records of 7ch at 800c3d04, each two
// semi-transparent POLY_FT4 packets), and the cleared tables 800c3bac,
// 800d3368, 800c3acc and 800d330a.
void Program::battle_effect_lists() {
    disc_wait(0);
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        auto &heap = resident.heap;
        memory.put32(0x800c3e88, 0);
        memory.put16(0x800c3cf0, 0);
        memory.put8(0x800c3d6c, 0);
        memory.put8(0x800c3d68, 0);
        memory.put16(0x800c3b7c, 0);
        memory.put8(0x800c3b74, 1);
        const auto data = resident.battle_scene_data;
        const auto allocate = [&](std::uint32_t size, std::uint32_t site) {
            resident::heap_select_tag(heap, 4, 0); // 80032498
            auto block = resident::heap_allocate(heap, size, 0, site);
            if (!block)
                throw battle::BattleError("A quiet null allocation in 800a8b0c");
            const auto address = block->address;
            memory.regions.emplace(address, std::move(block->bytes));
            return address;
        };
        // 800a2234: a free list of 14h-byte entries.
        const auto lists = memory.s16(data + 0x348);
        if (lists > 0) {
            memory.put16(0x800c3d12, static_cast<std::uint32_t>(lists));
            const auto block = allocate(static_cast<std::uint32_t>(lists) * 0x14, 0x800a226c);
            memory.put32(0x800c3d0c, block);
            memory.put16(0x800c3d10, 0); // 800a22e8
            for (std::uint32_t i = 0; i < memory.u16(0x800c3d12); ++i)
                memory.put8(block + i * 0x14, 0);
        }
        // 800a2ca4 with 800a2d5c: records of two packets each.
        const auto records = memory.s16(data + 0x34a);
        memory.put16(0x800c3d08, static_cast<std::uint32_t>(records));
        memory.put16(0x800c3d0a, 0);
        const auto block = allocate(static_cast<std::uint32_t>(records + 1) * 0x7c, 0x800a2ce0);
        memory.put32(0x800c3d04, block);
        for (std::int32_t record = 0; record < memory.s16(0x800c3d08) + 1; ++record) {
            const auto at = block + static_cast<std::uint32_t>(record) * 0x7c;
            memory.put16(at + 0x16, 0xffff);
            memory.put16(at + 0x1e, 0);
            for (std::uint32_t packet = 0; packet < 2; ++packet) {
                const auto p = at + 0x2c + packet * 0x28;
                memory.put8(p + 3, 9); // SetPolyFT4 80043cb0
                memory.put8(p + 7, 0x2c);
                memory.put8(p + 7, memory.u8(p + 7) | 2); // SetSemiTrans 80043bfc
                const auto q = at + packet * 0x28;
                memory.put16(q + 0x3a, 0x1cdU << 6U); // GetClut(0, 1cd)
                memory.put8(q + 0x48, 0xf);
                memory.put16(q + 0x42, gpu::texture_page(0, 1, 0x380, 0));
                memory.put8(q + 0x38, 0);
                memory.put8(q + 0x39, 0xc1);
                memory.put8(q + 0x40, 0);
                memory.put8(q + 0x41, 0xc1);
                memory.put8(q + 0x49, 0xc1);
                memory.put8(q + 0x50, 0xf);
                memory.put8(q + 0x51, 0xc1);
            }
        }
        for (std::uint32_t i = 0; i < 9; ++i) // 800b00d0
            memory.put32(0x800c3bac + i * 4, 0);
        for (std::uint32_t i = 0; i < 32; ++i)
            memory.put32(0x800d3368 + i * 4, 0);
        for (std::uint32_t i = 0; i <= 0x98; i += 8)
            memory.put32(0x800c3acc + i, 0);
        memory.put16(0x800d330a, 0);
        memory.put16(0x800d331e, 0);
    });
}

// 80071278..80071308, once the setup frames end: release the effect header
// block, stop its voices (8003a094, unless mode 4) and unlink it (8003852c),
// release the marker and the spacer that held the setup module, then
// 80070e2c/80070eb0 (only with 800c3d48) and 800c3e4c = 1 unless 800d2fc4.
// The original releases the bank before the driver reads it; releasing
// leaves a block's bytes as they are, so the unlink runs first here.
void Program::battle_release_setup() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        const auto bank = resident.battle_effects;
        if (resident.battle_request.mode != 4)
            resident::stop_bank_voices(resident.sound, bank);
        resident::unlink_effect_bank(resident.sound, bank);
        {
            const auto found = resident.sound.objects.find(bank);
            if (found == resident.sound.objects.end())
                throw battle::BattleError("The effect header is not a driver object");
            resident::HeapBlock block{bank, std::move(found->second)};
            resident.sound.objects.erase(found);
            if (resident::heap_release(resident.heap, block, 0x80071280) == -1)
                resident.sound.objects.emplace(bank, std::move(block.bytes));
        }
        release_battle_block(context, resident.battle_marker, 0x800712c4);
        release_battle_block(context, resident.battle_spacer, 0x800712d4);
        if (memory.u8(0x800c3d48) != 0)
            throw MissingDependency({"battle_release_setup", 0x80070e44, {}, {}},
                                    "symbol:battle-801e5000-module", false,
                                    "The 800c3d48 module paths 80070e2c and 80070eb0 are not "
                                    "reconstructed");
        if (memory.u8(0x800d2fc4) == 0)
            memory.put8(0x800c3e4c, 1);
    });
}

// 800b81bc, before the battle main loop:
// - 800b88c4: the rate from the enemies present (800ccc5c; 80059198 ends
//   clear) and the next draw buffer's ordering table.
// - 800b8840: the sprite and task state, the battle's display globals and
//   both 5000h sprite arenas (80024f64).
// - 801e62e0: the setup module's task (801e7098) holding A0.
// - The wave bank of 800595ac is released (80038310), the four light
//   vectors come from the scene data and the display is enabled.
void Program::battle_renderer_setup(std::uint32_t task_argument) {
    auto &data = *battle;
    auto &sprite = resident.sprite;
    // 800b88c4: enemy slots 3..10 on the field (+2 below 11h) and present (+4).
    std::int32_t enemies = 0;
    for (std::uint32_t slot = 3; slot < 11; ++slot)
        if (data.u8(battle::info(slot) + 2) < 0x11 && data.u8(battle::info(slot) + 4) != 0)
            ++enemies;
    data.put32(0x800c3d58, static_cast<std::uint32_t>(enemies));
    const auto rate = std::max(enemies / 2 - 1, 0);
    sprite.rate_control = 0;
    resident.sprite_models.depth_shift = 2;
    data.put32(0x800ccc5c, static_cast<std::uint32_t>(rate));
    const auto draw = data.u32(0x800ccb00) == 0x800c4a20 ? 0x800c8a90U : 0x800c4a20U;
    data.put32(0x800ccb00, draw);
    data.put32(0x800ccb04, draw + 0x70);
    clear_ordering_table(draw + 0x70, 0x1000);
    data.put32(0x800ccb34, 1);
    for (std::uint32_t i = 0; i < 4; ++i) // dither, draw on display, background, red
        data.put8(0x800c8aa8 + i, data.u8(0x800c4a38 + i));
    data.put32(0x800ccb00, 0x800c8a90);
    data.put8(0x800ccc58, 0);
    // 800b8840.
    auto &tasks = resident.sprite_tasks;
    sprite.platform_mode = 1;
    tasks.active_flags = 0;
    tasks.creation_flags = 0;
    sprite.platform_argument = 0x2000;
    data.put32(0x800c3e20, 0); // 800bed30
    data.put32(0x800c3610, 0);
    data.put16(0x800d2e54, 0);
    data.put32(0x800d2d68, 0); // 800be108
    data.put32(0x800c374c, 0);
    tasks.head = 0; // 8001c944
    tasks.pending_head = 0;
    tasks.primary_count = 0;
    tasks.auxiliary_count = 0;
    tasks.wait_count = 0;
    data.put32(0x800c3674, 0x200); // 800bb7f8
    data.put32(0x800c3678, 0xffffffffU);
    data.put8(0x800c3cc4, 0);
    data.put32(0x800c3cbc, 1);
    data.put32(0x800c3cc0, 0); // 800bc2f0(0)
    data.put32(0x800c3cbc, 1);
    for (const auto [pointer, site] :
         {std::pair{0x800c3680U, 0x800bc3b0U}, {0x800c3684U, 0x800bc3d8U}})
        if (data.u32(pointer) != 0)
            throw MissingDependency({"battle_renderer_setup", site, {}, {}},
                                    "symbol:battle-camera-callback", false,
                                    "A pending 800c3680/800c3684 callback is not reconstructed");
    // 80024f64(5000, 0): both sprite arenas, no uploads or releases, an empty
    // frame list.
    resident.sprite_arena_bytes = 0x5000;
    const auto arenas = load_block(0xa000, 0, 0x80024f78);
    resident.sprite_arenas = {arenas, arenas + 0x5000};
    resident.sprite_releases = {0, 0};
    resident.sprite_uploads[0] = 0;
    sprite.frame_head = 0;
    data.put16(0x800c3d14, 0); // 800bcd8c
    data.put8(0x800d2fdc, 0);  // 800b7c28
    resident.sprite_models.lod = 0;
    // 801e62e0: 801e7098 in allocation mode 1; 8001cd08(0, 78h) allocates
    // the task and its 78h bytes and registers it without an owner (8001cc18).
    tasks.allocation_mode = 1;
    auto block = resident::heap_allocate(resident.heap, 0x94, tasks.allocation_mode, 0x8001cd24);
    if (!block)
        throw battle::BattleError("A quiet null allocation in 8001cd08");
    const auto address = block->address;
    auto &bytes = block->bytes;
    const auto get = [&](std::uint32_t at) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
        return value;
    };
    const auto put = [&](std::uint32_t at, std::uint32_t value) {
        for (std::uint32_t i = 0; i < 4; ++i)
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    // Without an owner the owner generation is the word at 00000010.
    put(0, 0);
    put(0xc, 0x8001cd94);
    put(8, 0);
    put(0x14, resident.null_owner_generation & 0x1fffffffU);
    put(0x10, (get(0x10) & 0xe0000000U) | (tasks.serial & 0x1fffffffU));
    put(0x18, tasks.head);
    tasks.head = address;
    ++tasks.serial;
    ++tasks.primary_count; // 800591ac is clear: no flag 31
    put(0xc, 0x8001ce44);
    put(4, 0);
    put(8, 0x801e6fec); // 8001cd6c
    put(0x20, task_argument);
    tasks.nodes.push_back({address, std::move(bytes)});
    tasks.allocation_mode = 0;
    // 80038310(*800595ac).
    resident::release_wave_bank(resident.sound, resident.battle_wave);
    const auto light = [&](std::uint32_t to, std::uint32_t from) { // 80021b04
        for (std::uint32_t i = 0; i < 6; i += 2)
            data.put16(to + i, data.u16(resident.battle_scene_data + from + i));
    };
    light(0x800d30a0, 0x482);
    light(0x800d3354, 0x482);
    light(0x800d30a8, 0x47c);
    light(0x800d335c, 0x47c);
    set_display_mask(1);
}

// 800b39c0(time, mode, r, g, b): the fade task. Its node is battle BSS
// (800c3c50 once 800c3558 names it; 800c3c00 in 800c3554 while 800c355c is
// set) holding a main task (+0) and an auxiliary task (+1c). A new node is
// registered without an owner (8001cc18) with its auxiliary task
// (8001ca58), running 800b36bc (+8), destroyed by 800b383c (+c), the
// auxiliary running 800b3878. A running fade starts from the colour it
// reached (+48..4a to +45..47). +41 mode, +42..44 target colour, +38 and
// +3c the steps (time * 2), then the first step.
void Program::start_battle_fade(battle::Battle &context, std::uint32_t time, std::uint32_t mode,
                                std::uint32_t red, std::uint32_t green, std::uint32_t blue) {
    auto &memory = context.memory;
    if (memory.u8(0x800d3638) != 0)
        return;
    constexpr std::uint32_t current = 0x800c3558;
    constexpr std::uint32_t alternate = 0x800c3554;
    std::uint32_t node = 0;
    bool fresh = false;
    if (memory.u8(0x800c355c) == 0) {
        node = memory.u32(current);
        if (node == 0) {
            node = 0x800c3c50;
            memory.put32(current, node);
            fresh = true;
        }
    } else if (memory.u32(current) == 0) {
        node = 0x800c3c00;
        memory.put32(alternate, node);
        fresh = true;
    } else {
        node = memory.u32(alternate);
    }
    auto &tasks = resident.sprite_tasks;
    if (fresh) {
        // 8001cc18(0, node): a null owner's generation is the word at 00000010.
        memory.put32(node, 0);
        memory.put32(node + 0xc, 0x8001cd94);
        memory.put32(node + 8, 0);
        memory.put32(node + 0x14, resident.null_owner_generation & 0x1fffffffU);
        memory.put32(node + 0x10,
                     (memory.u32(node + 0x10) & 0xe0000000U) | (tasks.serial & 0x1fffffffU));
        ++tasks.serial;
        memory.put32(node + 0x18, tasks.head);
        tasks.head = node;
        if (tasks.creation_flags != 0) {
            ++tasks.active_flags;
            memory.put32(node + 0x14, memory.u32(node + 0x14) | 0x80000000U);
        }
        ++tasks.primary_count;
        // 8001ca58(node, node + 1c): the auxiliary task on the pending list.
        const auto auxiliary = node + 0x1c;
        memory.put32(auxiliary, node);
        const auto serial = tasks.serial & 0x1fffffffU;
        ++tasks.serial;
        memory.put32(auxiliary + 0x18, tasks.pending_head);
        tasks.pending_head = auxiliary;
        memory.put32(auxiliary + 0x10, (memory.u32(auxiliary + 0x10) & 0xe0000000U) | serial);
        memory.put32(auxiliary + 8, 0);
        memory.put32(auxiliary + 0xc, 0x8001cb48);
        ++tasks.auxiliary_count;
        memory.put32(auxiliary + 0x14, memory.u32(node + 0x10) & 0x1fffffffU);
        memory.put32(node + 0x14, memory.u32(node + 0x14) & 0x7fffffffU);
        if (tasks.creation_flags != 0)
            --tasks.active_flags;
        memory.put32(node + 8, 0x800b36bc);      // 8001cd6c
        memory.put32(auxiliary + 8, 0x800b3878); // 8001cd64
        memory.put32(node + 0xc, 0x800b383c);    // 8001cd74
        memory.put32(node + 4, node);
        memory.put32(node + 0x20, node);
        for (const auto at : {0x40U, 0x45U, 0x46U, 0x47U})
            memory.put8(node + at, 0);
    } else {
        for (std::uint32_t i = 0; i < 3; ++i)
            memory.put8(node + 0x45 + i, memory.u8(node + 0x48 + i));
    }
    memory.put8(node + 0x41, mode);
    memory.put8(node + 0x42, red);
    memory.put8(node + 0x43, green);
    memory.put8(node + 0x44, blue);
    memory.put32(node + 0x38, time << 1U);
    memory.put32(node + 0x3c, time << 1U);
    step_battle_fade(context, node);
}

// 800b36bc: one fade step. With steps left (+3c), the colour +48..4a moves
// from the start (+45..47) toward the target (+42..44): the difference
// scaled by the steps left / 2 (GTE GPF, sf 0) and divided by the total / 2
// is taken from the target. With none left the colour is the target; a
// black target ends the fade through the destroy callback of 800c3558.
void Program::step_battle_fade(battle::Battle &context, std::uint32_t task) {
    auto &memory = context.memory;
    const auto left = static_cast<std::int32_t>(memory.u32(task + 0x3c));
    if (left == 0) {
        memory.put8(task + 0x49, memory.u8(task + 0x43));
        memory.put8(task + 0x48, memory.u8(task + 0x42));
        memory.put8(task + 0x4a, memory.u8(task + 0x44));
        if ((memory.u8(task + 0x42) | memory.u8(task + 0x49) | memory.u8(task + 0x44)) == 0)
            throw MissingDependency({"battle_fade", 0x800b3714, {}, {}},
                                    "symbol:battle-fade-destroy", false,
                                    "A finished black fade's destroy callback is not "
                                    "reconstructed");
        return;
    }
    memory.put32(task + 0x3c, static_cast<std::uint32_t>(left - 1));
    auto &gte = resident.gte;
    gte.set_data(8, static_cast<std::uint32_t>((left - 1) >> 1));
    for (std::uint32_t i = 0; i < 3; ++i)
        gte.set_data(9 + i, static_cast<std::uint32_t>(memory.u8(task + 0x42 + i)) -
                                memory.u8(task + 0x45 + i));
    gte.execute(0x3dU); // GPF sf=0 lm=0
    const auto total = static_cast<std::int32_t>(memory.u32(task + 0x38)) >> 1;
    if (total == 0)
        throw MissingDependency({"battle_fade", 0x800b37a8, {}, {}}, "symbol:battle-fade-zero",
                                false, "A fade step dividing by zero steps is not reconstructed");
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto scaled = static_cast<std::int32_t>(gte.data(9 + i)) / total;
        memory.put8(task + 0x48 + i,
                    (memory.u8(task + 0x42 + i) - static_cast<std::uint32_t>(scaled)) & 0xffU);
    }
}

void Program::battle_opening() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        start_battle_fade(context, 0, 2, 0xff, 0xff, 0xff);
        if (memory.u8(0x800c3d48) == 0)
            start_battle_fade(context, 0x14, 2, 0, 0, 0);
        if (resident.b_694f8 != 0) {
            // 800397fc(80062648, 7f, 0): 80039850 then 80039a80.
            const auto sequence = open_sequence(0x80062648);
            start_sequence(sequence, 0x7f, 0);
            memory.put32(0x800c3e54, sequence);
        }
        memory.put32(0x800d3364, resident.battle_scene);
        memory.put32(0x800c3eb0, resident.battle_scene);
    });
}

// 80077990: the glyph palettes' rows read back from VRAM, the window and
// cursor glyphs, the UI block's texture rectangles and four draw modes.
// - Four rectangles at +8950 (x 1, 198 wide, 1 high) on the CLUT rows at
//   +a240 - 1, +a240, - 2 and - 3, each stored (StoreImage 800448f8) four
//   times: to +8970, +8afc, +8c88, +8e14 and on at +630 per rectangle.
// - Glyphs 4b, 50, 4d and 4e (80026338) into +a24c, +a264, +a27c, +a294;
//   the first two then take image (3c0, 34) and (3c8, 34).
// - UI RECTs at +0 (0, 0, 100, 100), +8 (the page of glyph 5c), +10 and +18
//   (8 x 10 at the first two glyphs), +20 and +28 (10 x 8 at the last two).
// - Draw modes at +8908 and +8914 over UI +8, +8920 and +892c over UI +0,
//   all on glyph 5c's page.
// Then 80070f40 sets 800d3298.
void Program::battle_opening_images(FrameServices &services) {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        const auto graphics = memory.u32(0x800c3ea4);
        const auto rects = graphics + 0x8950;
        const auto row = memory.u16(graphics + 0xa240);
        for (std::uint32_t i = 0; i < 4; ++i) {
            memory.put16(rects + i * 8, 1);
            memory.put16(rects + i * 8 + 4, 0xc6);
            memory.put16(rects + i * 8 + 6, 1);
        }
        memory.put16(rects + 2, row - 1U);
        memory.put16(rects + 0xa, row);
        memory.put16(rects + 0x12, row - 2U);
        memory.put16(rects + 0x1a, row - 3U);
        for (std::uint32_t copy = 0; copy < 4; ++copy)
            for (std::uint32_t i = 0; i < 4; ++i) {
                const auto rect = rects + i * 8;
                const auto destination = graphics + 0x8970 + i * 0x630 + copy * 0x18c;
                std::array<std::int16_t, 4> area{s16(memory.u16(rect)), s16(memory.u16(rect + 2)),
                                                 s16(memory.u16(rect + 4)),
                                                 s16(memory.u16(rect + 6))};
                std::vector<std::uint8_t> words(0x18c);
                store_image(services, area, rect, destination, words);
                memory.put16(rect + 4, static_cast<std::uint16_t>(area[2]));
                memory.put16(rect + 6, static_cast<std::uint16_t>(area[3]));
                for (std::uint32_t at = 0; at < words.size(); ++at)
                    memory.put8(destination + at, words[at]);
            }
        const auto table = memory.u32(0x800d2f5c);
        for (const auto [id, fields] :
             {std::pair{0x4bU, 0xa24cU}, {0x50U, 0xa264U}, {0x4dU, 0xa27cU}, {0x4eU, 0xa294U}})
            battle::glyph_fields(context, table, id, graphics + fields);
        const auto ui = memory.u32(0x800d2d28);
        const auto word = [&](std::uint32_t offset) { return memory.u32(graphics + offset); };
        const auto rect = [&](std::uint32_t index, std::uint32_t x, std::uint32_t y,
                              std::uint32_t w, std::uint32_t h) {
            memory.put16(ui + index * 8, x);
            memory.put16(ui + index * 8 + 2, y);
            memory.put16(ui + index * 8 + 4, w);
            memory.put16(ui + index * 8 + 6, h);
        };
        rect(0, 0, 0, 0x100, 0x100);
        memory.put32(graphics + 0xa25c, 0x3c0);
        memory.put32(graphics + 0xa274, 0x3c8);
        memory.put32(graphics + 0xa260, 0x34);
        memory.put32(graphics + 0xa278, 0x34);
        rect(1, (word(0xa244) & 0x3f) << 1, word(0xa248), 0x100, 0x100);
        rect(2, (word(0xa25c) & 0x3f) << 1, word(0xa260), 8, 0x10);
        rect(3, (word(0xa274) & 0x3f) << 1, word(0xa278), 8, 0x10);
        rect(4, (word(0xa28c) & 0x3f) * 2 + 0xe, word(0xa290), 0x10, 8);
        rect(5, (word(0xa2a4) & 0x3f) * 2 + 0xe, word(0xa2a8), 0x10, 8);
        for (const auto [packet, area] :
             {std::pair{0x8908U, 8U}, {0x8914U, 8U}, {0x8920U, 0U}, {0x892cU, 0U}}) {
            const auto page =
                gpu::texture_page(word(0xa238), 0, static_cast<std::int32_t>(word(0xa244)),
                                  static_cast<std::int32_t>(word(0xa248)));
            const std::array<std::int16_t, 4> window{
                s16(memory.u16(ui + area)), s16(memory.u16(ui + area + 2)),
                s16(memory.u16(ui + area + 4)), s16(memory.u16(ui + area + 6))};
            draw_mode_packet(graphics + packet, page, &window);
        }
        memory.put8(0x800d3298, 1); // 80070f40 after the call
    });
}

void Program::battle_adjust_party() { run_battle(battle::adjust_party); }

void Program::battle_place_party() { run_battle(battle::place_party); }

void Program::setup_battle_phase(std::uint32_t phase, FrameServices &services,
                                 std::uint32_t stack) {
    switch (phase & 0xff) {
    case 0:
        run_battle([&](battle::Battle &context) { setup_battle_party(context, services, stack); });
        break;
    case 1:
        run_battle([&](battle::Battle &context) { battle::setup_participants(context, resident); });
        break;
    case 2:
        run_battle([&](battle::Battle &context) { battle::setup_turns(context, resident); });
        break;
    case 3:
        run_battle([&](battle::Battle &context) { setup_battle_panels(context); });
        break;
    default:
        break;
    }
}

} // namespace xem::reconstruction

namespace xem::reconstruction::battle {

void glyph_fields(Battle &battle, std::uint32_t table, std::uint32_t id, std::uint32_t fields) {
    auto &memory = battle.memory;
    const auto glyph = table + memory.u16(table + 4 + id * 2);
    const auto part = glyph + 4;
    const auto shift = memory.u16(part + 16) == 0 ? 20 : 18;
    const auto offset = static_cast<std::int32_t>(memory.u16(part) << 16) >> shift;
    const std::array<std::int32_t, 6> values{memory.s16(glyph),
                                             memory.s16(part + 16),
                                             memory.s16(part + 18),
                                             memory.s16(part + 20),
                                             s16(memory.u16(part + 22) & 0xffc0) + offset,
                                             s16(memory.u16(part + 24) & 0xff00) +
                                                 memory.s16(part + 2)};
    for (std::uint32_t i = 0; i < values.size(); ++i)
        memory.put32(fields + i * 4, static_cast<std::uint32_t>(values.at(i)));
}

void adjust_party(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t member = 0; member < 3; ++member) {
        if (memory.u8(party_ids + member) == 0x7f)
            continue;
        const auto rec = battle.record(member);
        const auto gear = rec + 0xa4;
        memory.put32(attacker_pointer, rec);
        memory.put32(attacker_block_pointer, gear);
        memory.put16(0x800c3aa4 + member * 2, memory.u16(rec + 0x7a));
        for (std::uint32_t part = 0; part < 4; ++part) {
            const auto removed = memory.u8(0x800d2d10 + part);
            if (removed == 0)
                continue;
            memory.put8(gear + 0x98, memory.u8(gear + 0x98) - removed);
            memory.put8(gear + 0x98, memory.u8(gear + 0x98) + memory.u8(gear + 0x50 + part));
        }
        if (memory.u8(gear + 0x98) >= 17)
            memory.put8(gear + 0x98, 0x10);
    }
    memory.put8(0x8006de1a, 7);
    if ((memory.u16(0x8006ee0e) & 0x2000) != 0)
        memory.put16(0x8006edf6, memory.u16(0x8006edf6) | 0x800);
    if (memory.u16(0x8006ef64) < 0xbb) {
        for (const auto [address, value] : {std::pair{0x8006e020U, 10U},
                                            {0x8006e0c4U, 10U},
                                            {0x8006e72cU, 9U},
                                            {0x8006e7d0U, 9U},
                                            {0x8006e874U, 8U},
                                            {0x8006e918U, 0xcU},
                                            {0x8006e9bcU, 0xcU},
                                            {0x8006e802U, 0x58U},
                                            {0x8006e42bU, 0U},
                                            {0x8006e950U, 0x28U}})
            memory.put8(address, value);
    }
}

void place_party(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto id = memory.u8(party_ids + member);
        if (id == 0x7f)
            continue;
        memory.put16(0x800c3e0c + member * 4, memory.u16(0x8006ecf4 + id * 0x20));
        memory.put16(0x800c3e0e + member * 4, memory.u16(0x8006ecf6 + id * 0x20));
    }
}

std::uint32_t allocate_block(Battle &battle, ResidentState &resident, std::uint32_t size,
                             std::uint32_t mode) {
    auto &heap = resident.heap;
    resident::heap_select_tag(heap, 2, 0); // 80032498
    auto block = resident::heap_allocate(heap, size, mode, 0x8008abe0);
    if (!block)
        throw BattleError("A quiet null allocation (8008abb8) is not reconstructed");
    const auto address = block->address;
    battle.memory.regions.emplace(address, std::move(block->bytes));
    return address;
}

namespace {

constexpr std::uint32_t window_blocks = 0x800d2e38; // word per window: 5a8h bytes
constexpr std::uint32_t window_places = 0x800d2d90; // word per window: eh bytes
constexpr std::uint32_t ui_pointer = 0x800d2d28;
constexpr std::uint32_t draw_buffer = 0x800ccb34;

// GetClut 80043a58.
std::uint32_t clut(std::uint32_t x, std::uint32_t y) { return y << 6U | ((x >> 4U) & 0x3fU); }

// 80076b00 on the POLY_FT4 at `quad`: semi-transparent (80043bfc),
// textured (80043c24(0)), colour 80, blending bit 40 of the texture page
// from 800595a0.
void window_glyph_quad(BattleMemory &memory, const ResidentState &resident, std::uint32_t quad) {
    memory.put8(quad + 7, ((memory.u8(quad + 7) | 2U) & 0xfeU));
    for (const auto at : {4U, 5U, 6U})
        memory.put8(quad + at, 0x80);
    const auto page = memory.u16(quad + 0x16);
    memory.put16(quad + 0x16, resident.window_blend != 0 ? page | 0x40U : page & 0xffbfU);
}

// 80077364(quads, set): four POLY_FT4 (SetPolyFT4, SetShadeTex(1), colour
// ff) on the page and CLUT of glyph field set `set` (+a238 + set * 18h).
void window_edge_quads(BattleMemory &memory, std::uint32_t quads, std::uint32_t set) {
    const auto fields = memory.u32(0x800c3ea4) + set * 0x18;
    const auto word = [&](std::uint32_t at) { return memory.u32(fields + at); };
    for (std::uint32_t i = 0; i < 4; ++i) {
        const auto quad = quads + i * 0x28;
        memory.put8(quad + 3, 9);
        memory.put8(quad + 7, 0x2c | 1);
        for (const auto at : {4U, 5U, 6U})
            memory.put8(quad + at, 0xff);
        memory.put16(quad + 0x16,
                     gpu::texture_page(word(0xa238), 0, static_cast<std::int32_t>(word(0xa244)),
                                       static_cast<std::int32_t>(word(0xa248))));
        memory.put16(quad + 0xe, clut(word(0xa23c), word(0xa240)));
    }
}

// One edge of a window frame (8008de04, 8008e430, 8008ea70, 8008f0a8): two
// POLY_FT4 at +base (+50 apart, this draw buffer's of each pair), each on
// the corners `corners` (x, y) with texture corners from the glyph fields at
// +image (u from the x field's low 6 bits * 2), then 80076b00 on both.
struct Edge {
    std::uint32_t base;
    std::array<std::array<std::int32_t, 8>, 2> corners;
    std::uint32_t image; // graphics offset of the x field; y follows at +4
    std::int32_t u0, u1, v1;
};

void window_edge(BattleMemory &memory, const ResidentState &resident, std::uint32_t block,
                 const Edge &edge) {
    const auto buffer = memory.u32(draw_buffer);
    const auto graphics = memory.u32(0x800c3ea4);
    const auto u = static_cast<std::int32_t>((memory.u8(graphics + edge.image) & 0x3fU) * 2U);
    const auto v = static_cast<std::int32_t>(memory.u8(graphics + edge.image + 4));
    const std::array<std::array<std::int32_t, 2>, 4> uv{{{u + edge.u0, v},
                                                         {u + edge.u1, v},
                                                         {u + edge.u0, v + edge.v1},
                                                         {u + edge.u1, v + edge.v1}}};
    for (std::uint32_t k = 0; k < 2; ++k) {
        const auto quad = block + buffer * 0x28 + edge.base + k * 0x50;
        for (std::uint32_t i = 0; i < 4; ++i) {
            memory.put16(quad + 8 + i * 8,
                         static_cast<std::uint32_t>(edge.corners.at(k).at(i * 2)));
            memory.put16(quad + 0xa + i * 8,
                         static_cast<std::uint32_t>(edge.corners.at(k).at(i * 2 + 1)));
            memory.put8(quad + 0xc + i * 8, static_cast<std::uint32_t>(uv.at(i)[0]) & 0xffU);
            memory.put8(quad + 0xd + i * 8, static_cast<std::uint32_t>(uv.at(i)[1]) & 0xffU);
        }
    }
    for (std::uint32_t k = 0; k < 2; ++k)
        window_glyph_quad(memory, resident, block + (2 * k + buffer) * 0x28 + edge.base);
}

// A 16-bit coordinate as the original's halfword arithmetic leaves it.
std::int32_t h16(std::int32_t value) { return static_cast<std::int16_t>(value); }

// 8008f6e4(id, x, y, w, h): the window's backing quads' corners (+3c8, this
// draw buffer's), its corner glyphs (8008dc34) and its four edges.
void build_window(Battle &battle, const ResidentState &resident, std::uint32_t id, std::int32_t x,
                  std::int32_t y, std::int32_t w, std::int32_t h) {
    auto &memory = battle.memory;
    const auto block = memory.u32(window_blocks + id * 4);
    const auto ui = memory.u32(ui_pointer);
    memory.put8(ui + id + 0xb0, 0);
    const auto buffer = memory.u32(draw_buffer);
    const auto backing = block + buffer * 0x24 + 0x3c8;
    for (const auto [at, value] : {std::pair{0U, x},
                                   {2U, y},
                                   {8U, x + w},
                                   {0xaU, y},
                                   {0x10U, x},
                                   {0x12U, y + h},
                                   {0x18U, x + w},
                                   {0x1aU, y + h}})
        memory.put16(backing + at, static_cast<std::uint32_t>(value));
    // 8008dc34: the corner glyphs (f0, f2, f5, f7; 4a, 4c, 4f, 51 once
    // 800c3e4c is set) counted at +5a0, then 80076b00 on their quads.
    const bool set = memory.u8(0x800c3e4c) != 0;
    const std::array<std::uint32_t, 4> corners =
        set ? std::array<std::uint32_t, 4>{0x4a, 0x4c, 0x4f, 0x51}
            : std::array<std::uint32_t, 4>{0xf0, 0xf2, 0xf5, 0xf7};
    const auto right = h16(x + w - 8);
    const auto bottom = h16(y + h - 8);
    memory.put32(block + 0x5a0, 0);
    for (const auto [glyph, gx, gy] : {std::tuple{corners[0], x, y},
                                       {corners[1], right, y},
                                       {corners[2], x, bottom},
                                       {corners[3], right, bottom}}) {
        const auto count = memory.u32(block + 0x5a0);
        memory.put32(block + 0x5a0, count + draw_glyph(battle, glyph, block + count * 0x50,
                                                       static_cast<std::uint32_t>(gx),
                                                       static_cast<std::uint32_t>(gy), 0x1000));
    }
    for (std::uint32_t k = 0; k < 4; ++k)
        window_glyph_quad(memory, resident, block + (2 * k + buffer) * 0x28);
    // Each edge is two halves of (length - 10h) / 2.
    const auto half = [](std::int32_t length) { return h16(((length & 0xffff) - 0x10) / 2); };
    {
        const auto x0 = h16(x + 8);
        const auto x1 = h16(x0 + half(w));
        const auto x2 = h16(x1 + half(w));
        const auto top = h16(y - 8);
        const auto low = h16(y + 8);
        window_edge(memory, resident, block,
                    {0x140,
                     {{{x0, top, x1, top, x0, low, x1, low}, {x1, top, x2, top, x1, low, x2, low}}},
                     0xa25c,
                     0,
                     7,
                     0x10}); // 8008de04: top
        const auto top2 = h16(y + h - 8);
        const auto low2 = h16(y + h + 8);
        window_edge(
            memory, resident, block,
            {0x1e0,
             {{{x0, top2, x1, top2, x0, low2, x1, low2}, {x1, top2, x2, top2, x1, low2, x2, low2}}},
             0xa274,
             -8,
             -1,
             0x10}); // 8008e430: bottom
    }
    {
        const auto y0 = h16(y + 8);
        const auto y1 = h16(y0 + half(h));
        const auto y2 = h16(y1 + half(h));
        const auto left = h16(x - 8);
        const auto inner = h16(x + 8);
        window_edge(memory, resident, block,
                    {0x280,
                     {{{left, y0, inner, y0, left, y1, inner, y1},
                       {left, y1, inner, y1, left, y2, inner, y2}}},
                     0xa28c,
                     0xe,
                     0x1e,
                     7}); // 8008ea70: left
        const auto left2 = h16(x + w - 8);
        const auto inner2 = h16(x + w + 8);
        window_edge(memory, resident, block,
                    {0x320,
                     {{{left2, y0, inner2, y0, left2, y1, inner2, y1},
                       {left2, y1, inner2, y1, left2, y2, inner2, y2}}},
                     0xa2a4,
                     0xe,
                     0x1e,
                     7}); // 8008f0a8: right
    }
    memory.put8(block + 0x5a4, buffer);
    memory.put8(ui + id + 0xb0, 1);
}

} // namespace
} // namespace xem::reconstruction::battle

namespace xem::reconstruction {

void Program::open_battle_window(battle::Battle &context, std::uint32_t id, std::int16_t x,
                                 std::int16_t y, std::int16_t w, std::int16_t h, bool deferred,
                                 bool frame) {
    using namespace battle;
    auto &memory = context.memory;
    id &= 0xffU;
    if (memory.u8(memory.u32(ui_pointer) + id + 0xb0) == 0) {
        const auto block = allocate_block(context, resident, 0x5a8, 0);
        memory.put32(window_blocks + id * 4, block);
        for (std::uint32_t i = 0; i < 0x5a8; ++i)
            memory.put8(block + i, 0);
        const auto place = allocate_block(context, resident, 0xe, 0);
        memory.put32(window_places + id * 4, place);
        for (std::uint32_t i = 0; i < 0xe; ++i)
            memory.put8(place + i, 0);
        // 80077454: the two backing quads (SetPolyG4, semi-transparent, the
        // window colour) and their draw modes over UI RECT 0, then the four
        // edges' quads on glyph field sets 1-4.
        const auto graphics = memory.u32(0x800c3ea4);
        const auto ui = memory.u32(ui_pointer);
        for (std::uint32_t q = 0; q < 2; ++q) {
            const auto quad = block + q * 0x24 + 0x3c0;
            memory.put8(quad + 3, 8);
            memory.put8(quad + 7, 0x38);
            for (const auto at : {4U, 0xcU, 0x14U, 0x1cU})
                for (std::uint32_t c = 0; c < 3; ++c)
                    memory.put8(quad + at + c, resident.window_color.at(c));
            memory.put8(quad + 7, memory.u8(quad + 7) | 2U);
            const auto page = gpu::texture_page(
                0, resident.window_blend, static_cast<std::int32_t>(memory.u32(graphics + 0xa25c)),
                static_cast<std::int32_t>(memory.u32(graphics + 0xa260)));
            const std::array<std::int16_t, 4> area{static_cast<std::int16_t>(memory.u16(ui)),
                                                   static_cast<std::int16_t>(memory.u16(ui + 2)),
                                                   static_cast<std::int16_t>(memory.u16(ui + 4)),
                                                   static_cast<std::int16_t>(memory.u16(ui + 6))};
            draw_mode_packet(block + q * 0xc + 0x408, page, &area);
        }
        for (std::uint32_t set = 1; set <= 4; ++set)
            window_edge_quads(memory, block + 0x140 + (set - 1) * 0xa0, set);
    }
    if (deferred) {
        const auto place = memory.u32(window_places + id * 4);
        memory.put8(place + 0xc, id);
        for (const auto [at, value] : {std::pair{0U, x}, {2U, y}, {4U, w}, {6U, h}})
            memory.put16(place + at, static_cast<std::uint16_t>(value));
        memory.put16(place + 8, 0);
        memory.put16(place + 0xa, 0);
        const auto ui = memory.u32(ui_pointer);
        memory.put8(ui + id + 0xbf, 0);
        memory.put8(ui + id + 0xb8, 1);
        return;
    }
    build_window(context, resident, id, x, y, w, h);
    if (frame)
        throw MissingDependency({"open_battle_window", 0x8008f9f4, {}, {}}, "symbol:battle-frame",
                                false,
                                "A window opened with a frame (800716d8) is not reconstructed");
}

// 8007819c: windows 5 (8, 2a, 70 x 12) and 4 (20, c8, f4 x 12) built at once
// (UI +b5 and +b4 then cleared), then four pairs of text quads: a text
// image block (8008ac00(39h)) in 800d3720 and 800d3780 (+c0 per pair), the
// RECT (3c0, pair * d, 3c, d) at 800d3718 copied to 800d3778, and 800780a8
// on 800d36c8 (even line) and 800d3728 (odd line).
void Program::battle_opening_windows() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        open_battle_window(context, 5, 8, 0x2a, 0x70, 0x12, false, false);
        memory.put8(memory.u32(0x800d2d28) + 0xb5, 0);
        open_battle_window(context, 4, 0x20, 200, 0xf4, 0x12, false, false);
        memory.put8(memory.u32(0x800d2d28) + 0xb4, 0);
        // 800780a8(quads, line): two POLY_FT4 (colour 80, opaque, textured)
        // on the text CLUT of the line's parity (+5c) and the texture page
        // of its pair's row (3c0, line / 2 * d); +5d cleared.
        const auto text_quads = [&](std::uint32_t quads, std::uint32_t line) {
            for (std::uint32_t quad = quads; quad < quads + 0x50; quad += 0x28) {
                memory.put8(quad + 3, 9);
                memory.put8(quad + 7, 0x2c);
                for (const auto at : {4U, 5U, 6U})
                    memory.put8(quad + at, 0x80);
                memory.put8(quad + 7, (memory.u8(quad + 7) & 0xfdU) | 1U);
                memory.put8(quads + 0x5c, line & 1U);
                memory.put16(quad + 0xe, resident.text_cluts.at(memory.u8(quads + 0x5c) != 0));
                memory.put16(
                    quad + 0x16,
                    gpu::texture_page(0, 0, 0x3c0, static_cast<std::int32_t>(line / 2 * 0xd)));
            }
            memory.put8(quads + 0x5d, 0);
        };
        for (std::uint32_t pair = 0; pair < 4; ++pair) {
            const auto step = pair * 0xc0;
            const auto block = battle::allocate_text_block(context, resident, 0x39);
            memory.put32(0x800d3720 + step, block);
            memory.put32(0x800d3780 + step, block);
            const auto rect = 0x800d3718 + step;
            memory.put16(rect, 0x3c0);
            memory.put16(rect + 2, pair * 0xd);
            memory.put16(rect + 4, 0x3c);
            memory.put16(rect + 6, 0xd);
            for (std::uint32_t i = 0; i < 8; ++i)
                memory.put8(0x800d3778 + step + i, memory.u8(rect + i));
            text_quads(0x800d36c8 + step, pair * 2);
            text_quads(0x800d3728 + step, pair * 2 + 1);
        }
    });
}

} // namespace xem::reconstruction
