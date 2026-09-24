#include "xem/reconstruction/battle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace xem::reconstruction::battle {
namespace {

// Post-battle module words.
constexpr std::uint32_t game_data_pointer = 0x801e44c4; // -> 8006d634
constexpr std::uint32_t records_pointer = 0x801e44c8;   // -> 800ccce8
constexpr std::uint32_t growth_pointer = 0x801e44e8;    // *(800d2c08)
constexpr std::uint32_t current_pointer = 0x801e44ec;   // record being processed
constexpr std::uint32_t pool_a = 0x801e44f0;            // u32
constexpr std::uint32_t pool_b = 0x801e44f4;            // u32
constexpr std::uint32_t old_levels = 0x801e44f8;        // u8 pair per party slot
constexpr std::uint32_t party_ids = 0x800d2d24;         // u8 per party slot

std::int32_t s32(std::uint32_t value) { return static_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t s16(std::uint32_t value) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

// MIPS div: a zero divisor leaves -1 (non-negative dividend) or +1; 80000000 / -1
// leaves 80000000.
std::int32_t mips_div(std::int32_t dividend, std::int32_t divisor) {
    if (divisor == 0)
        return dividend >= 0 ? -1 : 1;
    if (dividend == std::numeric_limits<std::int32_t>::min() && divisor == -1)
        return dividend;
    return dividend / divisor;
}

// MIPS divu: a zero divisor leaves ffffffff.
std::uint32_t mips_divu(std::uint32_t dividend, std::uint32_t divisor) {
    return divisor == 0 ? 0xffffffffU : dividend / divisor;
}

// The 0x51eb851f multiply-high remainder: a signed division by 100 truncating
// toward zero, which C++ % reproduces for every int32.
std::int32_t percent(std::uint32_t value) { return s32(value) % 100; }

// The +0/+1 roll of 801e3610 and 801e38cc: (x - 50 + r) << 16 > 0x310000.
std::uint32_t roll_increment(std::int32_t quotient, std::uint32_t random) {
    const auto sum = u32(quotient) - 50 + u32(percent(random));
    return s32(sum << 16) > 0x310000 ? 1 : 0;
}

std::uint32_t current(const Battle &battle) { return battle.memory.u32(current_pointer); }

// 8006f8ea bit 8000, read through the game data pointer.
bool capped(const Battle &battle) {
    return (battle.memory.u16(battle.memory.u32(game_data_pointer) + 0x22b6) & 0x8000) != 0;
}

// Growth block of the current record's character id (+0x110 per id).
std::uint32_t growth_block(const Battle &battle) {
    return battle.memory.u32(growth_pointer) + battle.memory.u8(current(battle) + 0x56) * 0x110;
}

// 801e3610(stat, target, cap, level): stat + 0/1, 0 when it wraps to 100 hex,
// capped at 200.
std::uint32_t grow_stat(Battle &battle, std::uint32_t stat, std::uint32_t target, std::uint32_t cap,
                        std::uint32_t level) {
    const auto random = battle.rand();
    const auto quotient =
        mips_div(s32(((target & 0xff) - (stat & 0xff)) * 100), s32((cap & 0xff) - (level & 0xff)));
    auto result = stat + roll_increment(quotient, random);
    if ((result & 0xff) >= 201)
        result = 200;
    return result & 0xff;
}

// 801e3700(max_hp, level): new maximum HP.
std::uint32_t grow_max_hp(Battle &battle, std::uint32_t max_hp, std::uint32_t level_argument) {
    const auto level = level_argument & 0xff;
    std::uint32_t gain = 0;
    if (level < 100) {
        const auto random = percent(battle.rand());
        const auto span =
            battle.memory.u16(growth_block(battle) + 0xb8) - (level - 99) - (max_hp & 0xffff);
        gain = u32(mips_div(s32(u32(random) * span), s32((100 - level) * 100))) + 2;
    } else {
        const auto random = percent(battle.rand());
        // 801e3848: the divisor is 0 at level 201.
        const auto step =
            mips_div(s32(battle.memory.u16(growth_block(battle) + 0xba) - (max_hp & 0xffff)),
                     s32((201 - level) * 100));
        gain = ((u32(random) * u32(step)) << 1) + 2;
    }
    auto result = s32(gain << 16) < 0 ? max_hp : gain + max_hp;
    if (s16(result) >= 1000)
        result = 999;
    return result & 0xffff;
}

// 801e38cc(max_ep, level): max EP + 0/1, capped at 99.
std::uint32_t grow_max_ep(Battle &battle, std::uint32_t max_ep, std::uint32_t level) {
    std::uint32_t target = 0;
    std::uint32_t cap = 0;
    if ((level & 0xff) < 100) {
        target = battle.memory.u8(growth_block(battle) + 0xc8);
        cap = 99;
    } else {
        target = battle.memory.u8(growth_block(battle) + 0xc9);
        cap = 200;
    }
    const auto random = battle.rand();
    // 801e398c: the divisor is 0 at level 99 (and 200).
    const auto quotient =
        mips_div(s32((target - (max_ep & 0xff)) * 100), s32(cap - (level & 0xff)));
    auto result = max_ep + roll_increment(quotient, random);
    if ((result & 0xff) >= 100)
        result = 99;
    return result & 0xff;
}

struct Growth {
    std::uint32_t stat;   // record offset
    std::uint32_t target; // growth block offset
};

// 801e3610 over stats in order; `high` selects the target byte +1 at level >= 100.
template <std::size_t N>
void grow_stats(Battle &battle, std::uint32_t high, std::uint32_t cap, std::uint32_t level,
                const std::array<Growth, N> &stats) {
    for (const auto &growth : stats) {
        const auto stat = battle.memory.u8(current(battle) + growth.stat);
        const auto target = battle.memory.u8(growth_block(battle) + high + growth.target);
        const auto value = grow_stat(battle, stat, target, cap, level);
        battle.memory.put8(current(battle) + growth.stat, value);
    }
}

// 801e335c: level-A growth: max HP (+4e), then +58, +59, +5e, +5f.
void grow_level_a(Battle &battle) {
    auto &memory = battle.memory;
    const auto record = current(battle);
    const auto level = memory.u8(record + 0x62);
    const std::uint32_t high = level >= 100 ? 1 : 0;
    const std::uint32_t cap = level >= 100 ? 200 : 100;
    const auto max_hp = memory.u16(record + 0x4e);
    const auto stat_level = memory.u8(record + 0x62);
    const auto hp = grow_max_hp(battle, max_hp, level);
    memory.put16(current(battle) + 0x4e, hp);
    static constexpr std::array<Growth, 4> stats{
        {{0x58, 0xbc}, {0x59, 0xbe}, {0x5e, 0xc0}, {0x5f, 0xc2}}};
    grow_stats(battle, high, cap, stat_level, stats);
}

// 801e3500: level-B growth: max EP (+52, read as a byte), then +5b, +5c.
void grow_level_b(Battle &battle) {
    auto &memory = battle.memory;
    const auto record = current(battle);
    const auto level = memory.u8(record + 0x63);
    const std::uint32_t high = level >= 100 ? 1 : 0;
    const std::uint32_t cap = level >= 100 ? 200 : 100;
    const auto max_ep = memory.u8(record + 0x52);
    const auto stat_level = memory.u8(record + 0x63);
    const auto ep = grow_max_ep(battle, max_ep, level) & 0xff;
    memory.put16(current(battle) + 0x52, ep);
    static constexpr std::array<Growth, 2> stats{{{0x5b, 0xc4}, {0x5c, 0xc6}}};
    grow_stats(battle, high, cap, stat_level, stats);
}

// One track of 801e308c: gain levels at `level` while `rest` <= 0, loading
// each level's requirement from growth data +0xbac into `next`.
std::int32_t gain_levels(Battle &battle, std::int32_t rest, std::uint32_t level, std::uint32_t next,
                         void (*grow)(Battle &)) {
    auto &memory = battle.memory;
    do {
        auto record = current(battle);
        const auto raised = memory.u8(record + level) + 1;
        memory.put8(record + level, raised);
        if ((raised & 0xff) >= 100 && capped(battle)) {
            record = current(battle);
            memory.put8(record + level, memory.u8(record + level) - 1);
        }
        record = current(battle);
        const auto table = memory.u32(growth_pointer) + memory.u8(record + level) * 4;
        memory.put32(record + next, memory.u32(table + 0xbac));
        grow(battle);
        rest = s32(u32(rest) + memory.u32(current(battle) + next));
    } while (rest <= 0);
    return rest;
}

// 801e308c: add pools A/B to the totals (+3c/+40) and to-next counters
// (+44/+48), raising levels +62/+63.
void level_up(Battle &battle) {
    auto &memory = battle.memory;
    auto record = current(battle);
    const auto a = memory.u32(pool_a);
    const auto b = memory.u32(pool_b);
    memory.put32(record + 0x40, memory.u32(record + 0x40) + b);
    memory.put32(record + 0x3c, memory.u32(record + 0x3c) + a);
    if (memory.u8(record + 0x62) == 99 && capped(battle))
        memory.put32(pool_a, 0);
    if (memory.u8(current(battle) + 0x63) == 99 && capped(battle))
        memory.put32(pool_b, 0);
    record = current(battle);
    if (memory.u8(record + 0x62) == 99)
        memory.put32(pool_a, 0);
    if (memory.u8(record + 0x63) == 99)
        memory.put32(pool_b, 0);

    auto rest = s32(memory.u32(record + 0x44) - memory.u32(pool_a));
    if (rest > 0)
        memory.put32(record + 0x44, u32(rest));
    else
        rest = gain_levels(battle, rest, 0x62, 0x44, grow_level_a);

    record = current(battle);
    const auto pool = memory.u32(pool_b);
    const auto next = memory.u32(record + 0x48);
    memory.put32(record + 0x44, u32(rest));
    rest = s32(next - pool);
    if (rest > 0)
        memory.put32(record + 0x48, u32(rest));
    else
        rest = gain_levels(battle, rest, 0x63, 0x48, grow_level_b);
    memory.put32(current(battle) + 0x48, u32(rest));
}

// 801e2eb0(experience, slot, reserve): set pools A/B for the current record.
void split_experience(Battle &battle, std::uint32_t experience, std::uint32_t slot_argument,
                      std::uint32_t reserve) {
    auto &memory = battle.memory;
    if (s16(reserve) == 1) {
        const auto share = (experience * 3) >> 2;
        memory.put32(pool_a, share);
        memory.put32(pool_b, share);
        return;
    }
    const auto slot = s16(slot_argument);
    const auto records = memory.u32(records_pointer); // t1, loaded once
    const auto combatant = records + u32(slot) * record_stride;
    auto weight_a = memory.u8(combatant + 0x158);
    auto weight_b = memory.u8(combatant + 0x159);
    if (weight_a < 2)
        weight_a = 1;
    if (weight_b < 2)
        weight_b = 1;
    const auto total = weight_a + weight_b;
    memory.put32(pool_a, mips_divu(experience * weight_a, total));
    memory.put32(pool_b, mips_divu(experience * weight_b, total));
    if (capped(battle)) {
        memory.put32(pool_a, experience);
        memory.put32(pool_b, experience);
    }
    if (memory.u32(pool_a) == 0)
        memory.put32(pool_a, 1);
    if (memory.u32(pool_b) == 0)
        memory.put32(pool_b, 1);
    const auto record = current(battle);
    if ((memory.u16(record + 0x32) & 0x2000) != 0) {
        const auto a = memory.u32(pool_a);
        const auto b = memory.u32(pool_b);
        memory.put32(pool_a, (a >> 1) + a);
        memory.put32(pool_b, (b >> 1) + b);
    }
    if ((memory.u16(record + 0x32) & 0x1000) != 0) {
        const auto b = memory.u32(pool_b);
        memory.put32(pool_b, (b >> 1) + b);
    }
    // Result-screen copy at records + fe8 + slot*8.
    const auto shown = records + u32(slot) * 8;
    memory.put32(shown + 0xfe8, memory.u32(pool_a));
    memory.put32(shown + 0xfec, memory.u32(pool_b));
}

std::uint32_t character_record(const Battle &battle, std::uint32_t id) {
    return id * 0xa4 + 0x26c + battle.memory.u32(game_data_pointer);
}

} // namespace

void distribute_experience(Battle &battle) {
    auto &memory = battle.memory;

    // sp+0x10: s16 per character id 0..10, 100 = reserve. Entry 11 is the
    // unused halfword sp+0x26; later entries overwrite saved registers.
    std::array<std::int32_t, 12> share{};
    for (std::uint32_t id = 0; id < 11; ++id)
        share[id] = 100;

    std::uint32_t absent = 0;
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (memory.u8(party_ids + slot) == 0xff) {
            ++absent;
            continue;
        }
        const auto combatant = memory.u32(records_pointer) + slot * record_stride;
        memory.put32(current_pointer, combatant);
        const auto id = memory.u8(combatant + 0x56);
        if (id >= share.size())
            throw BattleError("experience split writes past its id table at 801e2b90/801e2ba4");
        if ((memory.u16(combatant + 0x7c) & 0xc000) != 0) {
            ++absent;
            share[id] = 0xff;
        } else {
            share[id] = static_cast<std::int32_t>(slot);
        }
        memory.put8(old_levels + slot * 2, memory.u8(current(battle) + 0x62));
        memory.put8(old_levels + slot * 2 + 1, memory.u8(current(battle) + 0x63));
    }

    const auto records = memory.u32(records_pointer);
    const auto penalty = memory.s8(records + 0x5fc4); // 800d2cac
    auto experience = memory.u32(records + 0x5f9c);   // 800d2c84
    if (penalty != 0)
        experience -= (experience >> 2) * u32(penalty);
    // 801e2c2c multu by aaaaaaab; hi >> 1 below.
    const auto third_high =
        static_cast<std::uint32_t>((std::uint64_t{experience} * 0xaaaaaaabU) >> 32);
    const auto active = 3 - (absent & 0xffff);

    for (std::uint32_t id = 0; id < 11; ++id) {
        const auto entry = share[id];
        if (entry == 0xff)
            continue;
        memory.put32(current_pointer, character_record(battle, id));
        if (entry < 3)
            split_experience(battle, mips_divu(experience, active), u32(entry), 0);
        else
            split_experience(battle, third_high >> 1, 0xff, 1);
        level_up(battle);
    }

    // Result-screen level deltas (+1022) and stat snapshot (+1040).
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto id = memory.u8(party_ids + slot);
        if (id == 0xff)
            continue;
        memory.put32(current_pointer, character_record(battle, id));
        auto record = current(battle);
        memory.put8(memory.u32(records_pointer) + slot * 2 + 0x1022,
                    memory.u8(record + 0x62) - memory.u8(old_levels + slot * 2));
        record = current(battle);
        memory.put8(memory.u32(records_pointer) + slot * 2 + 0x1023,
                    memory.u8(record + 0x63) - memory.u8(old_levels + slot * 2 + 1));
        record = current(battle);
        const auto shown = memory.u32(records_pointer) + slot * 8;
        if (memory.u8(record + 0x56) == 4)
            memory.put8(shown + 0x1040, memory.u8(record + 4) + memory.u8(record + 0x1c));
        else
            memory.put8(shown + 0x1040, memory.u8(record + 0x58) + memory.u8(record + 4));
        memory.put8(shown + 0x1041, memory.u8(current(battle) + 0x5e));
        memory.put8(shown + 0x1042,
                    memory.u8(current(battle) + 0x59) + memory.u8(current(battle) + 0x2d));
        memory.put8(shown + 0x1043, memory.u8(current(battle) + 0x5f));
        memory.put8(shown + 0x1044, memory.u8(current(battle) + 0x5b));
        memory.put8(shown + 0x1045, memory.u8(current(battle) + 0x5c));
        memory.put8(shown + 0x1046, memory.u8(current(battle) + 0x5a));
    }
}

} // namespace xem::reconstruction::battle
