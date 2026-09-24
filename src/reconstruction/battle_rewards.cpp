#include "xem/reconstruction/battle.hpp"

#include <array>

#include <cstdint>

namespace xem::reconstruction::battle {
namespace {

// Module words: 801e44c4 holds the game data base (8006d634), 801e44c8 the
// combatant base (800ccce8); 801e44e8 and 801e44ec are module variables (the
// growth data block and the current combatant record).
constexpr std::uint32_t game_base_word = 0x801e44c4;
constexpr std::uint32_t record_base_word = 0x801e44c8;
constexpr std::uint32_t growth_word = 0x801e44e8;
constexpr std::uint32_t current_record_word = 0x801e44ec;

constexpr std::uint32_t option_flags = 0x8006f8ea; // u16
constexpr std::uint32_t level_a = 0x8006d902;      // u8, + id * a4
constexpr std::uint32_t skill_bits = 0x8006ecf4;   // u16, + id * 20

// srav: arithmetic shift by the low five bits of the amount.
std::uint32_t srav(std::uint32_t value, std::uint32_t amount) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> (amount & 0x1f));
}

std::uint32_t growth_block(const Battle &battle, std::uint32_t id) {
    return battle.memory.u32(growth_word) + id * 0x110;
}

// 801e3be0(id): learn the first unlearned entry j (7, or 13 with option 4000)
// whose level (+100+j) is reached and whose seven counter requirements
// (u16 +j*e) the current record's counters +90 meet; stop at the first entry
// whose level is not reached. Returns j, or 0.
std::uint32_t learn_counter_skill(Battle &battle, std::uint32_t id) {
    auto &memory = battle.memory;
    const std::uint32_t count = (memory.u16(option_flags) & 0x4000) != 0 ? 13 : 7;
    std::uint32_t learnt = 0xff;
    const auto known = memory.u16(skill_bits + id * 0x20);
    const auto block = growth_block(battle, id);
    for (std::uint32_t j = 0; j < count; ++j) {
        if ((known & srav(0x8000, j)) != 0)
            continue;
        if (memory.u8(level_a + id * 0xa4) < memory.u8(block + j + 0x100))
            break;
        const auto requirements = block + j * 14;
        std::uint32_t k = 0;
        for (; k < 7; ++k) {
            const auto record = memory.u32(current_record_word);
            if (memory.u16(record + 0x90 + k * 2) < memory.u16(requirements + k * 2))
                break;
        }
        if (k == 7) {
            learnt = j;
            break;
        }
    }
    if (learnt == 0xff)
        return 0;
    const auto bits = skill_bits + id * 0x20;
    memory.put16(bits, memory.u16(bits) | srav(0x8000, learnt));
    return learnt;
}

// 801e3d54(id): learn the first of twelve level entries (+f0+k, ff ends)
// reached and not yet set in u16 game+16c2+id*20. Returns k+1, or 0.
std::uint32_t learn_level_skill(Battle &battle, std::uint32_t id) {
    auto &memory = battle.memory;
    const auto block = growth_block(battle, id);
    const auto bits = memory.u32(game_base_word) + id * 0x20 + 0x16c2;
    std::uint32_t bit = 0x8000;
    for (std::uint32_t k = 0; k < 12; ++k, bit >>= 1) {
        const auto level = memory.u8(block + k + 0xf0);
        if (level == 0xff)
            return 0;
        if (memory.u8(level_a + id * 0xa4) < level)
            continue;
        const auto known = memory.u16(bits);
        if ((bit & known) != 0)
            continue;
        memory.put16(bits, bit | known);
        return (k + 1) & 0xff;
    }
    return 0;
}

// 801e3e14(id): for the nine entries +d0+k (ff ends), a set bit (entry-1) of
// u16 game+16c0+id*20 sets bit k+3 of u16 game+16c4+id*20.
void unlock_from_16c0(Battle &battle, std::uint32_t id) {
    auto &memory = battle.memory;
    const auto block = growth_block(battle, id);
    const auto bits = memory.u32(game_base_word) + id * 0x20;
    for (std::uint32_t k = 0; k < 9; ++k) {
        const auto entry = memory.u8(block + k + 0xd0);
        if (entry == 0xff)
            return;
        if ((memory.u16(bits + 0x16c0) & srav(0x8000, entry - 1)) != 0)
            memory.put16(bits + 0x16c4, memory.u16(bits + 0x16c4) | srav(0x8000, k + 3));
    }
}

// 801e3ea4: id 8's nine level entries (growth + 950 + k, ff ends) set bit
// 1000>>k of u16 game+17c4 once its level (8006de22) reaches them.
void learn_id8_skills(Battle &battle) {
    auto &memory = battle.memory;
    const auto growth = memory.u32(growth_word);
    const auto game = memory.u32(game_base_word);
    for (std::uint32_t k = 0; k < 9; ++k) {
        const auto level = memory.u8(growth + k + 0x950);
        if (level == 0xff)
            return;
        if (memory.u8(0x8006de22) < level)
            continue;
        const auto known = memory.u16(game + 0x17c4);
        const auto bit = srav(0x1000, k);
        if ((known & bit) == 0)
            memory.put16(game + 0x17c4, bit | known);
    }
}

// 801e3f28(id): for the thirteen entries +e0+k (0 ends), a set bit (entry-1)
// of u16 game+16c2+id*20 sets bit k of u16 game+16c6+id*20.
void unlock_from_16c2(Battle &battle, std::uint32_t id) {
    auto &memory = battle.memory;
    const auto block = growth_block(battle, id);
    const auto bits = memory.u32(game_base_word) + id * 0x20;
    for (std::uint32_t k = 0; k < 13; ++k) {
        const auto entry = memory.u8(block + k + 0xe0);
        if (entry == 0)
            return;
        if ((memory.u16(bits + 0x16c2) & srav(0x8000, entry - 1)) != 0)
            memory.put16(bits + 0x16c6, memory.u16(bits + 0x16c6) | srav(0x8000, k));
    }
}

// 801e3fb0: id 7 values from the current record: game+e58 = maxHP*200,
// game+e30 = ATK/5+1, game+e64/e66 = maxHP*10. The multiply-high
// (0xcccccccd, >>34) is exact u32 division by 5.
void update_id7(Battle &battle) {
    auto &memory = battle.memory;
    auto record = memory.u32(current_record_word);
    const auto game = memory.u32(game_base_word);
    memory.put32(game + 0xe58, memory.u16(record + 0x4e) * 200);
    memory.put8(game + 0xe30, memory.u8(record + 0x58) / 5 + 1);
    record = memory.u32(current_record_word);
    memory.put16(game + 0xe64, memory.u16(record + 0x4e) * 10);
    memory.put16(game + 0xe66, memory.u16(record + 0x4e) * 10);
}

// 801e3a18: skills for each party slot not knocked out (+7c bit 8000).
void learn_skills(Battle &battle) {
    auto &memory = battle.memory;
    const auto current_id = [&] { return memory.u8(memory.u32(current_record_word) + 0x56); };
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto record = memory.u32(record_base_word) + slot * record_stride;
        memory.put32(current_record_word, record);
        if ((memory.u16(record + 0x7c) & 0x8000) != 0)
            continue;
        auto id = memory.u8(record + 0x56);
        if (id >= 9 || id < 7) {
            if (const auto learnt = learn_counter_skill(battle, id); (learnt & 0xff) != 0)
                memory.put8(memory.u32(record_base_word) + slot + 0x101c, learnt);
        }
        id = current_id();
        if (id != 10) {
            if (const auto learnt = learn_level_skill(battle, id); (learnt & 0xff) != 0)
                memory.put8(memory.u32(record_base_word) + slot + 0x101f, learnt);
            id = current_id();
        }
        if (id < 7 || (id >= 9 && id != 10))
            unlock_from_16c0(battle, id);
        if (id = current_id(); id >= 11 || id < 8)
            unlock_from_16c2(battle, id);
        if (current_id() == 8)
            learn_id8_skills(battle);
        if (current_id() == 7)
            update_id7(battle);
    }
}

// 801e403c: tier byte game+16d7+id*20 for ids 0..10: 3->4, 4->5, 5->6 at the
// levels growth +cc/+cd/+ce; 6->7 at level 50 with option 4000.
void advance_tiers(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t id = 0; id < 11; ++id) {
        const auto game = memory.u32(game_base_word);
        const auto tier = game + 0x16c0 + id * 0x20 + 0x17;
        const auto character = game + 0x26c + id * 0xa4;
        const auto value = memory.u8(tier);
        if (value >= 3 && value <= 5) {
            if (memory.u8(character + 0x62) >= memory.u8(growth_block(battle, id) + 0xc9 + value))
                memory.put8(tier, value + 1);
        } else if (value == 6) {
            if (memory.u8(character + 0x62) >= 50 && (memory.u16(option_flags) & 0x4000) != 0)
                memory.put8(tier, 7);
        }
    }
}

// 801e41b4: levels 50/60/70 set bits 8/4/2 of u16 8006ecf8+id*20 for each
// party slot's id (800ccd3e + slot*170).
void mark_level_flags(Battle &battle) {
    auto &memory = battle.memory;
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto id = memory.u8(0x800ccd3e + slot * record_stride);
        const auto level = level_a + id * 0xa4;
        const auto flags = 0x8006ecf8 + id * 0x20;
        if (memory.u8(level) >= 50)
            memory.put16(flags, memory.u16(flags) | 8);
        if (memory.u8(level) >= 60)
            memory.put16(flags, memory.u16(flags) | 4);
        if (memory.u8(level) >= 70)
            memory.put16(flags, memory.u16(flags) | 2);
    }
}

// 801e2888: write party HP/EP, counters and gear HP/fuel back to game data
// for each slot with a character (800d2d24[slot] != ff). The multiply-highs
// (0x51eb851f >>36, 0xcccccccd >>35) are exact u32 division by 50 and 10.
void write_back(Battle &battle) {
    auto &memory = battle.memory;
    const auto game = memory.u32(game_base_word);
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (memory.u8(0x800d2d24 + slot) == 0xff)
            continue;
        const auto record = memory.u32(record_base_word) + slot * record_stride;
        const auto id = memory.u8(record + 0x56);
        const auto character = id * 0xa4 + 0x26c + game;
        const auto gear = memory.u8(record + 0xa0) * 0xa4 + 0x978 + game;
        const auto block = record + 0xa4;
        if (id == 7 && (memory.u8(0x800cce42 + slot * record_stride) & 0x80) != 0) {
            memory.put16(record + 0x4c, (memory.u32(record + 0x104) + 1) / 50);
            if (memory.u16(record + 0x4c) == 0)
                memory.put16(record + 0x4c, 1);
        }
        memory.put16(character + 0x4c, memory.u16(record + 0x4c));
        const auto hp = memory.u16(character + 0x4c);
        const auto max_hp = memory.u16(character + 0x4e);
        memory.put16(character + 0x50, memory.u16(record + 0x50));
        if (max_hp < hp)
            memory.put16(character + 0x4c, memory.u16(character + 0x4e));
        if (memory.u16(character + 0x52) < memory.u16(character + 0x50))
            memory.put16(character + 0x50, memory.u16(character + 0x52));
        for (std::uint32_t k = 0; k < 7; ++k)
            memory.put16(character + 0x90 + k * 2, memory.u16(record + 0x90 + k * 2));
        memory.put16(character + 0x3a, memory.u16(record + 0x3a));
        if ((memory.u16(record + 0x7c) & 0xc000) != 0)
            memory.put16(character + 0x4c, 1);
        if (const auto gear_id = memory.u8(record + 0xa0); gear_id >= 17 || gear_id == 7)
            continue;
        const auto max_gear_hp = memory.u32(gear + 0x64);
        memory.put32(gear + 0x60, memory.u32(block + 0x60));
        const auto gear_hp = memory.u32(gear + 0x60);
        memory.put16(gear + 0x38, memory.u16(block + 0x38));
        if (max_gear_hp < gear_hp)
            memory.put32(gear + 0x60, max_gear_hp);
        if (memory.u16(gear + 0x3a) < memory.u16(gear + 0x38))
            memory.put16(gear + 0x38, memory.u16(gear + 0x3a));
        if ((memory.u16(block + 0x7c) & 0x8000) != 0)
            memory.put32(gear + 0x60, memory.u32(gear + 0x64) / 10);
    }
}

// 801e42c4: roll one drop per defeated enemy (mask base+5fb4, bit i): slot 1
// at chance +150 (forced when a party record has +32 bit 800), else slot 2 at
// +151; category/id to base+100c+i/base+1014+i. rand()%100 is the signed
// multiply-high (0x51eb851f, >>37) remainder, exact for every s32.
void roll_drops(Battle &battle) {
    auto &memory = battle.memory;
    const auto base = memory.u32(record_base_word);
    bool forced = false;
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto record = slot * record_stride + base;
        memory.put32(current_record_word, record);
        if ((memory.u16(record + 0x32) & 0x800) != 0)
            forced = true;
    }
    const auto roll = [&] { return static_cast<std::int32_t>(battle.rand()) % 100; };
    for (std::uint32_t i = 0; i < 8; ++i) {
        memory.put8(memory.u32(record_base_word) + i + 0x1014, 0);
        const auto records = memory.u32(record_base_word);
        if ((memory.u16(records + 0x5fb4) & (1U << i)) == 0)
            continue;
        const auto enemy = i * record_stride + 0x450 + records;
        memory.put32(current_record_word, enemy);
        std::uint32_t drop = 0;
        if (roll() >= static_cast<std::int32_t>(memory.u8(enemy + 0x150)) && !forced) {
            if (roll() >= static_cast<std::int32_t>(memory.u8(enemy + 0x151)))
                continue;
            drop = 1;
        }
        memory.put8(memory.u32(record_base_word) + i + 0x100c, memory.u8(enemy + 0x154 + drop));
        memory.put8(memory.u32(record_base_word) + i + 0x1014, memory.u8(enemy + 0x152 + drop));
    }
}

} // namespace

void grant_rewards(Battle &battle) {
    auto &memory = battle.memory;
    std::uint32_t knocked_out = 0;
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        if ((memory.u16(0x800ccd64 + slot * record_stride) & 0x8000) != 0)
            ++knocked_out;
    if (knocked_out == 3)
        return;
    memory.put32(growth_word, memory.u32(memory.u32(record_base_word) + 0x5f20));
    distribute_experience(battle);
    learn_skills(battle);
    advance_tiers(battle);
    mark_level_flags(battle);
    write_back(battle);
    roll_drops(battle);
    // 801e2844: scenario byte 8006db2c == 12 sets u16 8006ed6e to 4000, or
    // c000 when 8006e7ab is set.
    if (memory.u8(0x8006db2c) == 0x12) {
        const auto extra = memory.u8(0x8006e7ab);
        memory.put16(0x8006ed6e, 0x4000);
        if (extra != 0)
            memory.put16(0x8006ed6e, 0xc000);
    }
}

void total_rewards(Battle &battle) {
    auto &memory = battle.memory;
    constexpr std::uint32_t experience = 0x800d2c84;
    constexpr std::uint32_t defeated = 0x800d2c9c;
    memory.put32(experience, 0);
    memory.put16(defeated, 0);
    if (memory.u8(0x800d2fc4) != 0)
        throw BattleError("The skipped-reward path 801e23dc is past the compared boundary");
    std::uint32_t gold = 0;
    for (std::uint32_t enemy = 0; enemy < 8; ++enemy) {
        const auto slot = 3 + enemy;
        const auto record = 0x800ccce8 + slot * record_stride;
        if (memory.u8(0x800d2dcc + slot) == 0 || memory.u8(0x800c3eb7 + slot * 0x1c) != 0 ||
            (memory.u16(record + 0x7c) & 0x8000) == 0 || memory.u8(0x800c3d1b + enemy * 4) != 0)
            continue;
        memory.put32(experience, memory.u32(experience) + memory.u32(record + 0x14c));
        gold += memory.u16(record + 0x156);
        // 80089c08 with the enemy index, not the slot.
        memory.put16(defeated, memory.u16(defeated) | memory.u16(0x800c3448 + enemy * 2));
    }
    constexpr std::uint32_t party_gold = 0x8006ef58;
    const auto total = gold + memory.u32(party_gold);
    memory.put32(party_gold, total > 9999999 ? 9999999 : total);
}

namespace {
// 801e1370: add `count` of item `id` to an inventory list of `size` entries
// (ids, counts): stack onto the item (capped at 99) or take the first free
// entry; a full list drops the item.
void add_item(Battle &battle, std::uint32_t id, std::uint32_t count, std::uint32_t ids,
              std::uint32_t counts, std::uint32_t size) {
    auto &memory = battle.memory;
    for (std::uint32_t entry = 0; entry < size; ++entry) {
        if (memory.u8(ids + entry) != (id & 0xff))
            continue;
        const auto held = memory.u8(counts + entry);
        memory.put8(counts + entry, held + (count & 0xff) < 100 ? held + count : 99);
        return;
    }
    for (std::uint32_t entry = 0; entry < size; ++entry)
        if (memory.u8(ids + entry) == 0) {
            memory.put8(ids + entry, id);
            memory.put8(counts + entry, count);
            return;
        }
}
} // namespace

void add_drops(Battle &battle, std::uint32_t ids, std::uint32_t counts, std::uint32_t categories) {
    auto &memory = battle.memory;
    // Jump table 801de000: ids, counts and size of the five inventory lists.
    struct List {
        std::uint32_t ids, counts, size;
    };
    constexpr std::array<List, 5> lists{{{0x8006f3d0, 0x8006f36c, 100},
                                         {0x8006f4fc, 0x8006f434, 200},
                                         {0x8006f65a, 0x8006f5c4, 150},
                                         {0x8006f754, 0x8006f6f0, 100},
                                         {0x8006f84e, 0x8006f7b8, 150}}};
    for (std::uint32_t drop = 0; drop < 8; ++drop) {
        const auto id = memory.u8(ids + drop);
        const auto category = memory.u8(categories + drop);
        if (id == 0 || category >= lists.size())
            continue;
        const auto &list = lists[category];
        add_item(battle, id, memory.u8(counts + drop), list.ids, list.counts, list.size);
    }
}

} // namespace xem::reconstruction::battle
