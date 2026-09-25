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
namespace {
std::int16_t s16(std::uint32_t value) { return static_cast<std::int16_t>(value); }
// 800288ec: a byte size rounded up to words, as a signed MIPS quotient.
std::uint32_t word_size(std::uint32_t size) {
    const auto rounded = static_cast<std::int32_t>(size + 3U);
    const auto adjusted = rounded >= 0 ? rounded : static_cast<std::int32_t>(size + 6U);
    return static_cast<std::uint32_t>(adjusted >> 2) << 2U;
}
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
} // namespace

// 80032e88(item, mode): allocate the item's size (its first word) and decode
// it there (80032eb4).
std::uint32_t Program::unpack_battle_item(battle::Battle &context, std::uint32_t item,
                                          std::uint32_t mode) {
    auto &memory = context.memory;
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
    // 8003342c: the archive's offsets become addresses.
    const auto archive = resident.battle_archive;
    for (std::uint32_t entry = 1; entry <= memory.u32(archive); ++entry)
        memory.put32(archive + entry * 4, memory.u32(archive + entry * 4) + archive);
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
    // 8002dde4(images, 0, ...): each image section (1100 palette, 1101
    // pixels) at its own position; its rectangle lies in 8002dde4's frame
    // (801e5840 -18, 801e5384 -48, 8002dde4 -48, +10).
    block = unpack_battle_item(context, item(8), 1);
    {
        const auto rect = stack - 0x98;
        auto at = block + (memory.u32(block) + 1) * 4;
        for (std::uint32_t section = 0; section < memory.u32(block); ++section) {
            const auto kind = memory.u32(at);
            if (kind != 0x1100 && kind != 0x1101)
                break;
            std::array<std::int16_t, 4> area{
                static_cast<std::int16_t>(s16(memory.u16(at + 4)) + s16(memory.u16(at + 8))),
                static_cast<std::int16_t>(s16(memory.u16(at + 6)) + s16(memory.u16(at + 10))),
                s16(memory.u16(at + 12)), s16(memory.u16(at + 14))};
            static_cast<void>(load_image(area, rect, at + 16, &services));
            // The next section follows the rectangle as LoadImage left it.
            at += 16 + static_cast<std::uint32_t>(static_cast<std::int32_t>(area[2]) * area[3] * 2);
        }
    }
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
        memory.put16(pixels, static_cast<std::uint32_t>(s16(memory.u16(glyph + 26) & 0xffc0) +
                                                        offset + static_cast<std::int32_t>(member) * 6));
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
    const auto data = battle::allocate_block(context, resident, word_size(file_size(static_cast<std::int32_t>(set * 2 + 2))), 0);
    memory.put32(0x800c3dd0, data);
    memory.put32(0x800d33ec, data);
    memory.put16(0x800d33e8, set * 2 + 2);
    const auto models = battle::allocate_block(context, resident, word_size(file_size(static_cast<std::int32_t>(set * 2 + 3))), 1);
    memory.put32(0x800c3dec, models);
    memory.put32(0x800d33f4, models);
    memory.put16(0x800d33f8, 0);
    memory.put32(0x800d33fc, 0);
    memory.put16(0x800d33f0, set * 2 + 3);
    // 80029afc(800d33e8, 0, 80): the list is the reader's while it runs.
    resident.disc_read.list = {0x800d33e8, memory.take(0x800d33e8, 0x12)};
    static_cast<void>(read_files(0));
}

void Program::battle_prologue() {
    run_battle([&](battle::Battle &context) {
        auto &memory = context.memory;
        for (const auto [pointer, size] : {std::pair{0x800c3ea4U, 0xa2b4U},
                                           std::pair{0x800d2d28U, 0x10cU},
                                           std::pair{0x800c3eacU, 0x2f8U}})
            memory.put32(pointer, battle::allocate_block(context, resident, size, 0));
        for (const auto [pointer, size] : {std::pair{0x800c3ea4U, 0xa2b4U},
                                           std::pair{0x800d2d28U, 0x10cU},
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
        throw MissingDependency({"battle_setup", 0x801e6290, {}, {}}, "symbol:battle-setup-panels",
                                false, "The panel setup phase 801e6290 is not reconstructed");
    default:
        break;
    }
}

} // namespace xem::reconstruction

namespace xem::reconstruction::battle {

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
        for (const auto [address, value] :
             {std::pair{0x8006e020U, 10U}, {0x8006e0c4U, 10U}, {0x8006e72cU, 9U},
              {0x8006e7d0U, 9U}, {0x8006e874U, 8U}, {0x8006e918U, 0xcU}, {0x8006e9bcU, 0xcU},
              {0x8006e802U, 0x58U}, {0x8006e42bU, 0U}, {0x8006e950U, 0x28U}})
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
