// Battle setup of the setup module (directory 12 file 4, sha256 4300fdd9...,
// loaded at 801e4000): the phases 801e5840 runs from the intro swirl.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

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
        memory.put8(info(member),
                    memory.u8(info(member) + 4) == 0 ? memory.u8(formation(4 + member)) & 0x7f
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
            memory.put8(at + 1, memory.u8(at + 1) | memory.u16(slot_bits + memory.u8(info(slot) + 1) * 2));
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
            memory.put16(0x800d2e06 + slot * 2, memory.u16(0x800d2e06 + slot * 2) -
                                                    static_cast<std::uint32_t>(least - 1));
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

void Program::setup_battle_phase(std::uint32_t phase) {
    switch (phase & 0xff) {
    case 0:
        throw MissingDependency({"battle_setup", 0x801e5384, {}, {}}, "symbol:battle-setup-party",
                                false, "The party setup phase 801e5384 is not reconstructed");
    case 1:
        run_battle([&](battle::Battle &context) { battle::setup_participants(context, resident); });
        break;
    case 2:
        run_battle([&](battle::Battle &context) { battle::setup_turns(context, resident); });
        break;
    case 3:
        throw MissingDependency({"battle_setup", 0x801e6290, {}, {}}, "symbol:battle-setup-panels",
                                false, "The panel setup phase 801e6290 is not reconstructed");
    default:
        break;
    }
}

} // namespace xem::reconstruction

namespace xem::reconstruction::battle {

std::uint32_t allocate_block(Battle &battle, ResidentState &resident, std::uint32_t size,
                             std::uint32_t mode) {
    auto &heap = resident.heap;
    heap.tag = 2; // 80032498(2, 0)
    heap.tag_words[2] = 0;
    heap.quiet = 0;
    auto block = resident::heap_allocate(heap, size, mode, 0x8008abe0);
    if (!block)
        throw BattleError("A quiet null allocation (8008abb8) is not reconstructed");
    const auto address = block->address;
    battle.memory.regions.emplace(address, std::move(block->bytes));
    return address;
}

} // namespace xem::reconstruction::battle
