#include "xem/reconstruction/battle.hpp"

#include <array>
#include <cstdint>

// Formula types 0 (80094ee4) and 3 (80095d4c) and their helpers. Every compiler reciprocal in
// these functions is exact over the whole 32-bit domain, so plain C++ division
// reproduces it: multu 0xcccccccd + srl 2/3 is u32 x/5 and x/10; mult
// 0x66666667 + sra 2/3 minus the sign is int32 x/10 and x/20; mult 0x55555556
// minus the sign is x/3; mult 0x51eb851f + sra 5 minus the sign is x/100. The
// two unsigned-shift variants (80097384, 800973c8) only see non-negative
// products.
namespace xem::reconstruction::battle {
namespace {

constexpr std::uint32_t damage_offset = 0x5f6c;       // u32 per target slot
constexpr std::uint32_t result_offset = 0x5fa0;       // u8 per target slot
constexpr std::uint32_t slot_flags = 0x800cce42;      // absolute slot*170 + 800cce42 (record +15a)
constexpr std::uint32_t slot_multiplier = 0x800cce2a; // absolute slot*170 + 800cce2a (record +142)
constexpr std::uint32_t item_used = 0x800c34ae;       // u8
constexpr std::uint32_t item_table = 0x8006f8ba;      // resident byte table
constexpr std::uint32_t self_result = 0x800d2c88;     // absolute u8 per slot
constexpr std::uint32_t self_amount = 0x800d2c54;     // absolute u32 per slot
constexpr std::uint32_t ether_failed = 0x800d2dc4;    // u8

std::int32_t rand_percent(Battle &battle) { return static_cast<std::int32_t>(battle.rand()) % 100; }

std::uint32_t status_pair(const BattleMemory &memory, std::uint32_t address) {
    return memory.u16(address) | memory.u16(address + 2);
}

bool gear_slot(const Battle &battle, std::uint32_t slot) {
    return (battle.memory.u8(battle.record(slot) + 0x15a) & 0x80) != 0;
}

// 80096ab8: hit outcome (1 hit, 2 half, 3 miss, 5 forced).
std::int32_t hit_outcome(Battle &battle) {
    auto &m = battle.memory;
    const auto descriptor = m.u32(descriptor_pointer);
    const auto accuracy = static_cast<std::int32_t>(m.u8(m.u32(attacker_pointer) + 0x5e));
    const auto evasion = static_cast<std::int32_t>(m.u8(m.u32(target_pointer) + 0x5f));
    std::int32_t bonus = 0; // s2
    std::int32_t guard = 0; // s3
    if ((m.u16(descriptor + 0xa) & 0x40) != 0 &&
        (m.u8(slot_flags + m.u8(target_slot) * record_stride) & 0x80) != 0) {
        return 3;
    }
    const auto attacker = m.u32(attacker_pointer);
    if (m.u8(attacker + 0x56) == 4) {
        // 80096b5c: both checks consult the game-data table at 8006f8ba.
        const auto kinds = m.u8(descriptor + 0x10);
        if (((kinds & 0x80) != 0 && m.u8(item_table + m.u8(attacker + 0x6f)) == 0) ||
            ((kinds & 0x10) != 0 && m.u8(item_table + m.u8(attacker + 0x72)) == 0)) {
            m.put8(item_used, 1); // 80096be4
            return 3;
        }
    }
    if ((m.u16(descriptor + 0xa) & 0x200) != 0) {
        if ((m.u16(m.u32(target_pointer) + 0x34) & 8) != 0 ||
            (m.u8(slot_flags + m.u8(target_slot) * record_stride) & 0x80) != 0) {
            return 3;
        }
    }
    const auto flags = m.u16(descriptor + 0xa);
    if ((flags & 0x1000) != 0) {
        return 3;
    }
    const auto target = m.u32(target_pointer);
    const auto target_status = status_pair(m, target + 0x84);
    if ((target_status & 0x100) != 0) {
        return 3;
    }
    if ((m.u16(target + 0x7c) & 0x2000) != 0 || (m.u16(target + 0x80) & 0x1000) != 0 ||
        (flags & 0x8000) != 0) {
        return 1;
    }
    if ((flags & 2) != 0) {
        return 5;
    }
    const auto actor = m.u32(attacker_pointer);
    if ((m.u16(actor + 0x7c) & 0x400) != 0) {
        bonus -= 50;
    }
    if ((status_pair(m, actor + 0x84) & 0x1000) != 0) {
        bonus += 30;
    }
    if ((target_status & 0x800) != 0) {
        guard += 50;
    }
    const auto margin = accuracy + static_cast<std::int8_t>(m.u8(descriptor + 0x15)) - evasion;
    if ((m.u8(battle.record(m.u8(target_slot)) + 0x15a) & 1) != 0) {
        return rand_percent(battle) < 0x5f ? 2 : 1; // 80096d90
    }
    if ((target_status & 0x20) != 0) {
        return static_cast<std::int16_t>(rand_percent(battle) - margin) < 50 ? 1 : 3;
    }
    if ((status_pair(m, m.u32(target_pointer) + 0x84) & 0x40) != 0) {
        return static_cast<std::int16_t>(rand_percent(battle) - margin) < 50 ? 1 : 2;
    }
    if (static_cast<std::int16_t>(rand_percent(battle) - margin) >=
        static_cast<std::int16_t>(bonus - (guard - 90))) {
        return 3; // 80096f30
    }
    return static_cast<std::int16_t>(rand_percent(battle) - margin) <
                   static_cast<std::int16_t>(bonus - (guard - 85))
               ? 1
               : 2;
}

// 8009b46c: party bonus by party records (0x56 == 0 or 3) below half and
// quarter HP, then a random 3/2 or 4/2 boost.
void wounded_party_bonus(Battle &battle, std::uint16_t &value) {
    auto &m = battle.memory;
    std::uint32_t count = 0;
    const auto below = [&](std::uint32_t slot) {
        const auto record = battle.record(slot);
        std::uint32_t max;
        std::uint32_t current;
        if ((m.u8(slot_flags + slot * record_stride) & 0x80) != 0) {
            max = m.u32(record + 0x108);
            current = m.u32(record + 0x104);
        } else {
            max = m.u16(record + 0x4e);
            current = m.u16(record + 0x4c);
        }
        if (current < max >> 1) {
            ++count;
        }
        if (current < max >> 2) {
            ++count;
        }
    };
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (m.u8(battle.record(slot) + 0x56) == 0) {
            below(slot);
        }
        if (m.u8(battle.record(slot) + 0x56) == 3) {
            below(slot);
        }
    }
    if ((count & 0xff) != 0) {
        value = static_cast<std::uint16_t>(value + (count & 0xff) * (value >> 1u));
    }
    const auto flags = m.u16(m.u32(attacker_pointer) + 0x32);
    const std::uint32_t scale = (flags & 0x400) != 0 ? 4 : 3;
    const std::int32_t chance = (flags & 0x200) != 0 ? 0x3c : 10;
    if (rand_percent(battle) < chance) {
        value = static_cast<std::uint16_t>(scale * value >> 1);
    }
}

// 80096fbc: attack value.
std::int16_t attack_value(Battle &battle) {
    auto &m = battle.memory;
    std::array<std::uint8_t, 4> parts{}; // sp+0x10..0x13
    std::uint32_t initialized;
    std::uint32_t base; // a3
    if (gear_slot(battle, m.u8(attacker_slot))) {
        for (std::uint32_t i = 0; i < 3; ++i) {
            parts[i] =
                static_cast<std::uint8_t>(m.u8(m.u32(attacker_block_pointer) + i * 8 + 0x12));
        }
        initialized = 3;
        const auto block = m.u32(attacker_block_pointer);
        const auto product = m.u8(block + 0x3c) * m.u8(block + 0x3f);
        base = product;
        if ((status_pair(m, block + 0x80) & 0x1000) != 0) {
            base = product + (m.u8(block + 0x3c) << 1);
        }
    } else {
        for (std::uint32_t i = 0; i < 4; ++i) {
            parts[i] = static_cast<std::uint8_t>(m.u8(m.u32(attacker_pointer) + i * 8 + 4));
        }
        initialized = 4;
        base = m.u8(m.u32(attacker_pointer) + 0x58);
    }
    const auto descriptor = m.u32(descriptor_pointer);
    const auto attacker = m.u32(attacker_pointer);
    const auto kinds = m.u8(descriptor + 0x10);
    auto ether = m.u8(attacker + 0x5b); // t1
    if ((kinds & 0x10) != 0 && initialized < 4) {
        throw BattleError("attack value reads the unset stack byte sp+0x13 at 80097134");
    }
    if ((kinds & 0x08) != 0) {
        throw BattleError("attack value reads the unset stack byte sp+0x14 at 8009714c");
    }
    std::uint32_t sum = 0; // a1
    for (std::uint32_t i = 0; i < 4; ++i) {
        if ((kinds & (0x80u >> i)) != 0) {
            sum += parts[i];
        }
    }
    if ((status_pair(m, attacker + 0x8c) & 1) != 0) {
        sum += (sum & 0xffff) >> 1;
    }
    if ((m.u16(descriptor + 0xa) & 0x100) != 0) {
        std::uint32_t scale = (status_pair(m, attacker + 0x88) & 0x8000) != 0 ? 5 : 4;
        if ((m.u16(attacker + 0x80) & 0x400) != 0) {
            scale -= 1;
        }
        ether = (ether & 0xffff) * (scale & 0xff) >> 2; // non-negative product
        if ((status_pair(m, m.u32(attacker_pointer) + 0x88) & 0x2000) != 0) {
            ether = (ether & 0xffff) << 1;
        }
    } else if (!gear_slot(battle, m.u8(attacker_slot))) {
        const auto status = status_pair(m, attacker + 0x84);
        std::uint32_t scale = (status & 0x2000) != 0 ? 5 : 4;
        if ((m.u16(attacker + 0x7c) & 0x200) != 0) {
            scale -= 1;
        }
        if ((status & 0x400) != 0) {
            const auto tenth = (m.u16(attacker + 0x4e) / 10) & 0xffff;
            // 800972b0 divu has no zero guard; the R3000A quotient is then
            // ffffffff.
            const auto quotient = tenth == 0 ? 0xffffffffU : m.u16(attacker + 0x4c) / tenth;
            scale = scale + 10 - quotient;
        }
        base = (base & 0xffff) * (scale & 0xff) >> 2; // product below 2^24
    }
    const auto flags = m.u16(descriptor + 0xa);
    if ((flags & 0x20) != 0) {
        base = 0;
    }
    std::uint16_t value; // sp+0x18
    switch (m.u8(descriptor + 0x1a)) {
    case 0:
        value = static_cast<std::uint16_t>(sum + base);
        break;
    case 1:
        value = static_cast<std::uint16_t>(ether);
        break;
    case 2: {
        if (m.u8(m.u32(attacker_pointer) + 0x56) == 4) {
            sum = (sum & 0xffff) * 6 / 10; // 8009737c
        }
        const auto power = m.u8(descriptor + 0x11);
        const auto product = (flags & 0x100) != 0 ? (ether & 0xffff) * power
                                                  : ((sum & 0xffff) + (base & 0xffff)) * power;
        value = static_cast<std::uint16_t>(product / 20); // 800973c0
        break;
    }
    default:
        throw BattleError("attack value leaves sp+0x18 uninitialized for descriptor type >= 3 at "
                          "80097338");
    }
    if (m.u8(attacker_slot) < 3) {
        if (m.u8(m.u32(attacker_pointer) + 0x56) == 7) {
            wounded_party_bonus(battle, value);
        }
        if (m.u8(m.u32(attacker_pointer) + 0x56) == 4 ||
            (m.u8(m.u32(descriptor_pointer) + 0x22) & 0x20) != 0) {
            const auto target = m.u32(target_pointer);
            if ((m.u16(target + 0x38) & 0x20) != 0) {
                value = static_cast<std::uint16_t>(value + (value >> 2u));
            }
            if ((m.u32(target + 0x38) & 0x60) == 0x60) {
                value = static_cast<std::uint16_t>(value + (value >> 2u));
            }
        }
        if (m.u8(m.u32(attacker_pointer) + 0x56) == 8) {
            const auto offset = m.u8(attacker_slot) * record_stride;
            if ((m.u8(slot_flags + offset) & 0x80) == 0 &&
                (m.u16(m.u32(descriptor_pointer) + 0xa) & 0x100) != 0) {
                // 80097530: non-negative product, so the +3 rounding never applies.
                value = static_cast<std::uint16_t>(value * m.u8(slot_multiplier + offset) >> 2);
            }
        }
        if (m.u8(m.u32(attacker_pointer) + 0x56) == 10) {
            value = static_cast<std::uint16_t>(value + value / 5u);
        }
        if ((m.u8(m.u32(descriptor_pointer) + 0x22) & 0x10) != 0) {
            const auto target = m.u32(target_pointer);
            if ((m.u16(target + 0x38) & 0x10) != 0) {
                value = static_cast<std::uint16_t>(value + (value >> 2u));
                if ((m.u16(target + 0x38) & 0x10) != 0) {
                    value = static_cast<std::uint16_t>(value + (value >> 2u));
                }
            }
        }
    }
    return static_cast<std::int16_t>(value); // lh
}

// 80097610: defense value.
std::int16_t defense_value(Battle &battle) {
    auto &m = battle.memory;
    // A gear target leaves a2 unset here and zeroes it at 80097770.
    std::uint32_t armor = 0; // a2
    std::uint32_t body;      // t0
    if (gear_slot(battle, m.u8(target_slot))) {
        body = m.u16(m.u32(target_block_pointer) + 0x70);
    } else {
        armor = m.u8(m.u32(target_pointer) + 0x59);
        body = m.u8(m.u32(target_pointer) + 0x2d);
    }
    const auto descriptor = m.u32(descriptor_pointer);
    const auto target = m.u32(target_pointer);
    auto ether = m.u8(target + 0x5c); // a1
    if ((m.u16(descriptor + 0xa) & 0x100) != 0) {
        if ((status_pair(m, target + 0x88) & 0x4000) != 0) {
            ether = ether * 3 >> 1;
        }
    } else if (!gear_slot(battle, m.u8(target_slot))) {
        if ((status_pair(m, target + 0x84) & 0x100) != 0) {
            armor = (armor & 0xffff) * 3 >> 1;
        }
    }
    std::uint32_t value; // a3
    switch (m.u8(descriptor + 0x1b)) {
    case 0:
        value = body + armor;
        break;
    case 1:
        value = ether;
        break;
    case 2:
        value = body;
        break;
    default:
        throw BattleError("defense value returns the unset register a3 for descriptor type >= 3 "
                          "at 800977b4");
    }
    if (gear_slot(battle, m.u8(target_slot))) {
        return static_cast<std::int16_t>(value);
    }
    std::uint32_t downed = 0;
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if ((m.u16(battle.record(slot) + 0x7c) & 0x8000) != 0) {
            ++downed;
        }
    }
    const auto ally_flags = [&] { return m.u16(m.u32(target_pointer) + 0x32); };
    const auto ether_command = [&] {
        return (m.u16(m.u32(descriptor_pointer) + 0xa) & 0x100) != 0;
    };
    if ((ally_flags() & 4) != 0 && downed != 0 && !ether_command()) {
        value = (value & 0xffff) * (4 - downed) >> 2; // 800978a8, non-negative
    }
    if ((ally_flags() & 2) != 0 && downed != 0 && !ether_command()) {
        value = (value & 0xffff) * (downed + 2) >> 1;
    }
    if ((ally_flags() & 1) != 0 && downed != 0) {
        value = (value & 0xffff) * (downed + 2) >> 1;
    }
    return static_cast<std::int16_t>(value);
}

// 80096494: element adjustment of the caller's attack (sp+0x10), defense
// (sp+0x12) and hit (sp+0x14).
void element_adjust(Battle &battle, std::uint16_t &attack, std::uint16_t &defense,
                    std::uint8_t &hit) {
    auto &m = battle.memory;
    const bool target_gear = gear_slot(battle, m.u8(target_slot)); // t5
    const auto descriptor = m.u32(descriptor_pointer);
    std::uint32_t element = m.u8(descriptor + 0x22) & 0x3f; // t1
    const auto weak = m.u8(m.u32(target_pointer) + 0x38) & 0x3f;
    const auto own = (gear_slot(battle, m.u8(attacker_slot))
                          ? status_pair(m, m.u32(attacker_block_pointer) + 0x84)
                          : status_pair(m, m.u32(attacker_pointer) + 0x8c)) >>
                     12;
    const bool ether = (m.u16(descriptor + 0xa) & 0x100) != 0; // t6
    if (element == 0 && own != 0) {
        element = own;
    }
    const bool own_match = (element & own) != 0;
    std::uint32_t scale = 10; // a3
    if ((element & weak) != 0) {
        scale = (m.u16(m.u32(target_pointer) + 0x38) & 0x40) != 0 ? 0x12 : 0xf;
        if (own_match) {
            scale += 2;
        }
    }
    const auto guard = target_gear ? status_pair(m, m.u32(target_block_pointer) + 0x84)
                                   : status_pair(m, m.u32(target_pointer) + 0x8c);
    const auto resist = (guard & 0xf00) >> 8;
    const auto resist_step = [&](bool applies) {
        if (applies) {
            scale -= 3;
            if ((guard & 4) != 0) {
                scale -= 3;
            }
            if ((guard & 8) != 0) {
                hit = 4;
            }
        }
    };
    if ((element & resist) != 0) {
        resist_step(ether);
        resist_step((guard & 2) != 0);
    }
    // 800966c0: single elements 1/2/4/8 test guard bits 200/100/800/400.
    std::uint32_t boost = 0;
    switch (element & 0xff) {
    case 1:
        boost = guard & 0x200;
        break;
    case 2:
        boost = guard & 0x100;
        break;
    case 4:
        boost = guard & 0x800;
        break;
    case 8:
        boost = guard & 0x400;
        break;
    default:
        break;
    }
    if (boost != 0) {
        scale += 3;
    }
    if (static_cast<std::int8_t>(scale) <= 0) {
        scale = 1;
    }
    attack = static_cast<std::uint16_t>(attack * static_cast<std::int8_t>(scale) / 10);
    defense = static_cast<std::uint16_t>(10 * defense / 10);
    if ((m.u16(m.u32(attacker_pointer) + 0x32) & 0x10) != 0) {
        attack = static_cast<std::uint16_t>(attack + attack / 5u);
    }
    if ((m.u16(m.u32(target_pointer) + 0x32) & 0x10) != 0) {
        defense = static_cast<std::uint16_t>(defense + defense / 5u);
    }
}

} // namespace

// 80094ee4: formula type 0.
void physical_formula(Battle &battle) {
    auto &m = battle.memory;
    const auto descriptor = m.u32(descriptor_pointer);
    auto power = static_cast<std::int32_t>(m.u8(descriptor + 0x11)); // s1
    const std::uint32_t immune = (m.u16(descriptor + 0xa) & 0x100) != 0 ? 0x4000 : 0x8000;
    if ((m.u16(m.u32(target_pointer) + 0x34) & immune) != 0) {
        m.put8(battle.record_base() + m.u8(target_slot) + result_offset, 0); // 80094f64
        return;
    }
    auto hit = static_cast<std::uint8_t>(hit_outcome(battle));        // sp+0x14
    auto attack = static_cast<std::uint16_t>(attack_value(battle));   // sp+0x10
    auto defense = static_cast<std::uint16_t>(defense_value(battle)); // sp+0x12

    const auto attacker = m.u32(attacker_pointer);
    if ((status_pair(m, attacker + 0x88) & 8) != 0) {
        attack = static_cast<std::uint16_t>(attack + attack / 5u);
    }
    if ((status_pair(m, attacker + 0x88) & 2) != 0) {
        attack = static_cast<std::uint16_t>(attack + attack / 10u);
    }
    if ((status_pair(m, attacker + 0x88) & 4) != 0) {
        attack = static_cast<std::uint16_t>(attack - attack / 5u);
    }
    if ((status_pair(m, attacker + 0x88) & 1) != 0) {
        attack = static_cast<std::uint16_t>(attack - attack / 10u);
    }
    const auto target = m.u32(target_pointer);
    if ((status_pair(m, target + 0x88) & 4) != 0) {
        defense = static_cast<std::uint16_t>(defense + defense / 5u);
    }
    if ((status_pair(m, target + 0x88) & 1) != 0) {
        defense = static_cast<std::uint16_t>(defense + defense / 10u);
    }
    if ((status_pair(m, target + 0x88) & 8) != 0) {
        defense = static_cast<std::uint16_t>(defense - defense / 5u);
    }
    if ((status_pair(m, target + 0x88) & 2) != 0) {
        defense = static_cast<std::uint16_t>(defense - defense / 10u);
    }

    if ((m.u8(m.u32(descriptor_pointer) + 0x22) & 0x10) != 0) {
        if ((m.u16(target + 0x82) & 0x40) == 0) {
            m.put16(target + 0x80, m.u16(target + 0x80) | 0x40);
        }
        // 800951bc: the attacker's own result goes to the absolute tables.
        if ((status_pair(m, m.u32(attacker_pointer) + 0x8c) & 0x4000) != 0) {
            const auto slot = m.u8(attacker_slot);
            m.put8(self_result + slot, 3);
            const auto tenth = (m.u16(m.u32(attacker_pointer) + 0x52) / 10u) & 0xffff;
            m.put32(self_amount + m.u8(attacker_slot) * 4, tenth << 1);
        }
        if ((status_pair(m, m.u32(attacker_pointer) + 0x8c) & 0x1000) != 0) {
            const auto slot = m.u8(attacker_slot);
            m.put8(self_result + slot, 2);
            const auto tenth = (m.u16(m.u32(attacker_pointer) + 0x4e) / 10u) & 0xffff;
            m.put32(self_amount + m.u8(attacker_slot) * 4, tenth << 1);
        }
    }
    if ((m.u8(m.u32(descriptor_pointer) + 0x22) & 0x20) != 0) {
        const auto victim = m.u32(target_pointer);
        if ((m.u16(victim + 0x82) & 0x80) == 0) {
            m.put16(victim + 0x80, m.u16(victim + 0x80) | 0x80);
        }
    }
    if (const auto victim = m.u32(target_pointer); (m.u16(victim + 0x80) & 0x40) != 0) {
        defense = static_cast<std::uint16_t>(defense - (defense >> 2u));
        m.put16(victim + 0x80, m.u16(victim + 0x80) & 0xffbf);
    }
    if (const auto actor = m.u32(attacker_pointer); (m.u16(actor + 0x80) & 0x80) != 0) {
        attack = static_cast<std::uint16_t>(attack - (attack >> 2u));
        m.put16(actor + 0x80, m.u16(actor + 0x80) & 0xff7f);
    }
    if ((m.u16(m.u32(descriptor_pointer) + 0xa) & 0x400) != 0) {
        power = 0x14;
    }
    element_adjust(battle, attack, defense, hit);

    const bool ether = (m.u16(m.u32(descriptor_pointer) + 0xa) & 0x100) != 0;
    const std::int32_t attack_scale = ether ? 5 : 4;
    const std::int32_t defense_scale = ether ? 4 : 3;
    std::int32_t amount = attack_scale * attack; // s0
    if (defense != 0) {
        amount -= defense_scale * defense;
    }
    if (m.u8(m.u32(descriptor_pointer) + 0x1a) < 2) {
        amount = power * amount / 20; // 80095454
    }
    if (amount <= 0) {
        amount = 0;
    } else if (amount < 10) {
        amount += static_cast<std::int32_t>(battle.rand()) % 2; // 80095490
    } else {
        amount += static_cast<std::int32_t>(battle.rand()) % (amount / 10 + 2); // 800954dc
    }

    // 800954ec: jump table 80070370 on (s8)(hit - 1) < 5 unsigned.
    const auto result = battle.record_base() + m.u8(target_slot) + result_offset;
    switch (hit) {
    case 1:
        if (amount <= 0) {
            amount = 1;
        }
        m.put8(result, 0);
        break;
    case 2:
        amount /= 2;
        m.put8(result, 5);
        break;
    case 3:
        amount = 0;
        m.put8(result, 4);
        break;
    case 4:
        m.put8(result, 2);
        break;
    case 5:
        m.put8(result, 7);
        break;
    default:
        break;
    }
    if (m.u8(ether_failed) != 0 && (m.u16(m.u32(descriptor_pointer) + 0xa) & 0x100) != 0 &&
        amount != 0) {
        amount /= 3; // 80095630
    }
    if (amount >= 10000) {
        amount = 9999;
    }
    if (amount < 0) {
        amount = 0;
    }
    m.put32(battle.record_base() + m.u8(target_slot) * 4 + damage_offset,
            static_cast<std::uint32_t>(amount));
}

// 80095d4c: formula type 3, a chance roll then an HP or EP transfer between
// attacker and target (both slots receive the same amount).
void formula_type3(Battle &battle) {
    auto &m = battle.memory;
    const auto descriptor = m.u32(descriptor_pointer);
    std::int32_t chance; // s0
    switch (m.u8(descriptor + 0x18)) {
    case 0:
        chance = static_cast<std::int32_t>(m.u8(m.u32(attacker_pointer) + 0x60));
        break;
    case 1:
        chance = static_cast<std::int32_t>(m.u8(descriptor + 0x1c));
        break;
    default:
        throw BattleError("formula type 3 compares the caller's s0 for descriptor +18 >= 2 at "
                          "80095ddc");
    }
    const auto missed = [&] {
        m.put8(battle.record_base() + m.u8(target_slot) + result_offset, 6); // 80095f6c
    };
    if (chance < rand_percent(battle)) {
        missed();
        return;
    }
    // 80095e14: kinds 4 and above leave s1 unset, but no reachable path reads it.
    const auto kind = m.u8(m.u32(descriptor_pointer) + 0x1a);
    std::uint32_t base = 0; // s1
    switch (kind) {
    case 0:
        base = m.u16(m.u32(attacker_pointer) + 0x4e);
        break;
    case 1:
        base = m.u16(m.u32(target_pointer) + 0x4e);
        break;
    case 2:
        base = m.u16(m.u32(attacker_pointer) + 0x52);
        break;
    case 3:
        base = m.u16(m.u32(target_pointer) + 0x52);
        break;
    default:
        break;
    }
    // 80095e9c: the product is below 2^24, so mult 0x66666667 + srl 3 is /20.
    auto amount = base * m.u8(m.u32(descriptor_pointer) + 0x11) / 20; // a2
    if (kind == 5) {
        amount = m.u16(m.u32(target_pointer) + 0x4c) - 1U; // 80095edc
    }
    // 80095ee4: jump table 80070388 on kind < 6.
    std::uint8_t attacker_result;
    std::uint8_t target_result;
    switch (kind) {
    case 0:
    case 1:
    case 5:
        attacker_result = 2; // 80095f04
        target_result = 0;
        break;
    case 2:
    case 3:
        if ((status_pair(m, m.u32(target_pointer) + 0x88) & 0x200) != 0) {
            missed();
            return;
        }
        attacker_result = 3; // 80095f90
        target_result = 1;
        break;
    default:
        return; // kind 4 and kinds >= 6 exit at 80096000 without writing
    }
    m.put8(battle.record_base() + m.u8(attacker_slot) + result_offset, attacker_result);
    m.put8(battle.record_base() + m.u8(target_slot) + result_offset, target_result);
    const auto value = amount & 0xffff; // 80095fd0
    m.put32(battle.record_base() + m.u8(attacker_slot) * 4 + damage_offset, value);
    m.put32(battle.record_base() + m.u8(target_slot) * 4 + damage_offset, value);
}

} // namespace xem::reconstruction::battle
