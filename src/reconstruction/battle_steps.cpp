#include "xem/reconstruction/battle.hpp"

#include <cstdint>

namespace xem::reconstruction::battle {
namespace {

constexpr std::uint32_t damage_offset = 0x5f6c;         // u32 per target slot
constexpr std::uint32_t result_offset = 0x5fa0;         // u8 per target slot
constexpr std::uint32_t message_offset = 0x5fc7;        // u8
constexpr std::uint32_t fixed_record_base = 0x800ccce8; // absolute record base
constexpr std::uint32_t ether_failed = 0x800d2dc4;      // u8

// rand() % 100 as the 0x51eb851f multiply-high sequence computes it: a signed
// division truncating toward zero, which C++ % reproduces for every int32.
std::int32_t rand_percent(Battle &battle) { return static_cast<std::int32_t>(battle.rand()) % 100; }

std::uint32_t damage_address(const Battle &battle, std::uint32_t slot) {
    return battle.record_base() + slot * 4 + damage_offset;
}

std::uint32_t result_address(const Battle &battle, std::uint32_t slot) {
    return battle.record_base() + slot + result_offset;
}

void set_message(Battle &battle, std::uint32_t code) {
    battle.memory.put8(battle.record_base() + message_offset, code);
}

// 80099498: party-state gate on the formation mode byte 800c34ad.
std::uint32_t party_state_gate(Battle &battle) {
    const std::uint32_t mode = battle.memory.u8(0x800c34ad);
    if (mode != 2 && mode != 3) {
        return 0;
    }
    std::uint32_t count = 0;
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if ((battle.memory.u16(battle.record(slot) + 0x7c) & 0xc002) != 0) {
            count = (count + 1) & 0xff;
        }
    }
    if (mode == 2) {
        return count == 0 ? 1 : 0;
    }
    return count == 2 ? 0 : 1;
}

// 8009b684: message code for a status kind and flag (jump table 80070478).
void status_message(Battle &battle, std::uint32_t kind, std::uint32_t flags) {
    const std::uint32_t f = flags & 0xffff;
    std::uint32_t code = 0;
    switch (kind & 0xff) {
    case 0:
        switch (f) {
        case 0x0001:
            code = 0x08;
            break;
        case 0x0200:
            code = 0x05;
            break;
        case 0x0400:
            code = 0x04;
            break;
        case 0x0800:
            code = 0x03;
            break;
        case 0x1000:
            code = 0x02;
            break;
        case 0x2000:
            code = 0x01;
            break;
        default:
            return;
        }
        break;
    case 2:
        switch (f) {
        case 0x0001:
            code = 0x0d;
            break;
        case 0x0020:
            code = 0x07;
            break;
        case 0x0400:
            code = 0x0c;
            break;
        case 0x0800:
            code = 0x0b;
            break;
        case 0x1000:
            code = 0x0a;
            break;
        case 0x2000:
            code = 0x09;
            break;
        default:
            return;
        }
        break;
    case 5:
        switch (f) {
        case 0x0800:
            code = 0x12;
            break;
        case 0x1000:
            code = 0x11;
            break;
        case 0x1800:
            code = 0x13;
            break;
        case 0x2000:
            code = 0x10;
            break;
        case 0x4000:
            code = 0x0f;
            break;
        case 0x8000:
            code = 0x0e;
            break;
        default:
            return;
        }
        break;
    case 7:
        switch (f) {
        case 0x0001:
        case 0x0004:
            code = 0x1a;
            break;
        case 0x0002:
        case 0x0008:
            code = 0x19;
            break;
        case 0x1000:
            code = 0x18;
            break;
        case 0x4000:
            code = 0x16;
            break;
        case 0x8000:
            code = 0x15;
            break;
        default:
            return;
        }
        break;
    case 9:
        switch (f) {
        case 0x0100:
            code = 0x21;
            break;
        case 0x0200:
            code = 0x22;
            break;
        case 0x0400:
            code = 0x1f;
            break;
        case 0x0800:
            code = 0x20;
            break;
        case 0x1000:
            code = 0x1e;
            break;
        case 0x2000:
            code = 0x1d;
            break;
        case 0x4000:
            code = 0x1c;
            break;
        case 0x8000:
            code = 0x1b;
            break;
        default:
            return;
        }
        break;
    default:
        return;
    }
    set_message(battle, code);
}

// 80097964: roll and apply a status (chance, kind, flags) to the target.
// Returns 1 when applied, 0 otherwise.
std::uint32_t apply_status(Battle &battle, std::uint32_t chance, std::uint32_t kind,
                           std::uint32_t flags) {
    BattleMemory &m = battle.memory;
    if ((m.u8(battle.record(m.u8(target_slot)) + 0x15a) & 0x80) != 0) {
        return 0;
    }
    if (static_cast<std::int32_t>(chance & 0xff) < rand_percent(battle)) {
        return 0;
    }
    const std::uint32_t k = kind & 0xff;
    const std::uint32_t target = m.u32(target_pointer);

    // First pass: immunity checks and clears (jump table 800703c0).
    switch (k) {
    case 0: // 80097a28
        if ((m.u16(target + 0x7e) & (flags & 0xfffd)) != 0) {
            return 0;
        }
        if ((flags & 0x1000) != 0) {
            m.put16(target + 0x84, m.u16(target + 0x84) & 0x7fff);
        }
        break;
    case 2: // 80097a68
        if ((flags & m.u16(target + 0x82)) != 0) {
            return 0;
        }
        break;
    case 5: { // 80097a90
        const std::uint32_t state = m.u16(target + 0x7c);
        if (((state | m.u16(target + 0x7e)) & 1) != 0) {
            return 0;
        }
        if ((flags & 0x8000) != 0) {
            m.put16(target + 0x7c, state & 0xefff);
        }
        break;
    }
    case 7: // 80097acc
    case 9: // 80097b40
        if (k == 7) {
            if (((m.u16(target + 0x80) | m.u16(target + 0x82)) & 1) != 0) {
                return 0;
            }
            if ((flags & 0xa) != 0) {
                const std::uint32_t state = m.u16(target + 0x88);
                if ((state & 5) == 0) {
                    break;
                }
                m.put16(target + 0x88, state & 0xfffa);
                return 1;
            }
            if ((flags & 5) != 0) {
                const std::uint32_t state = m.u16(target + 0x88);
                if ((state & 0xa) == 0) {
                    break;
                }
                m.put16(target + 0x88, state & 0xfff5);
                return 1;
            }
        }
        // 80097b44
        if ((flags & 0xf000) != 0) {
            if ((m.u16(target + 0x8e) & 0xf000) != 0) {
                set_message(battle, 0x39);
                return 0;
            }
            m.put16(target + 0x8c, m.u16(target + 0x8c) & 0x0fff);
        }
        if ((flags & 0x0f00) != 0) {
            if ((m.u16(target + 0x8e) & 0x0f00) != 0) {
                set_message(battle, 0x39);
                return 0;
            }
            m.put16(target + 0x8c, m.u16(target + 0x8c) & 0xf0ff);
        }
        break;
    default:
        break;
    }

    // Second pass: set the status (jump table 800703e8).
    switch (k) {
    case 0: // 80097bf4
        m.put16(target + 0x7c, (flags | m.u16(target + 0x7c)) & 0xfffd);
        if ((flags & 2) != 0) {
            if ((party_state_gate(battle) & 0xff) != 0) {
                m.put16(target + 0x7c, flags | m.u16(target + 0x7c));
                set_message(battle, 0x31);
                m.put16(target + 0x7a, 0xffef);
            } else {
                set_message(battle, 0x32);
            }
        }
        break;
    case 2: // 80097c84
        m.put16(target + 0x80, flags | m.u16(target + 0x80));
        if ((flags & 0x800) != 0) {
            m.put16(target + 0x7a, m.u16(target + 0x7a) | 0x20);
        }
        break;
    case 5: // 80097cb8: +84, +88, +8c
    case 7:
    case 9: {
        const std::uint32_t address = target + k * 2 + 0x7a;
        m.put16(address, flags | m.u16(address));
        break;
    }
    default:
        break;
    }
    status_message(battle, k, flags & 0xffff);
    return 1;
}

// 800995a0: store a status duration into record(slot)+0x15c+index.
void set_status_duration(Battle &battle, std::uint32_t slot, std::uint32_t kind,
                         std::uint32_t flags, std::uint32_t amount) {
    BattleMemory &m = battle.memory;
    std::uint32_t index = 0xf;
    if ((m.u16(fixed_record_base + 0x8a + m.u8(attacker_slot) * record_stride) & 0x2000) != 0) {
        amount = (amount & 0xff) << 1;
    }
    const std::uint32_t record = fixed_record_base + (slot & 0xff) * record_stride;
    const std::uint32_t k = kind & 0xff;
    const std::uint32_t f = flags & 0xffff;
    if (k == 0) {
        index = f == 0x1000 ? 1 : (f != 0x2000 ? 0xf : 0);
    }
    if (k == 2) {
        if (f == 0x800) {
            index = 3;
        } else if (f == 0x1000) {
            index = 2;
        }
    }
    if (k == 5) {
        if (f == 0x4000) {
            index = 5;
        } else if (f == 0x8000) {
            index = 4;
        }
    }
    if (k == 7) {
        if (f == 0x4000) {
            index = 0xa;
        } else if (f == 0x1000) {
            index = 0xb;
        } else if (f == 0x8000) {
            index = 9;
        }
    }
    if (k == 9) {
        switch (f) {
        case 0x0100:
        case 0x0200:
        case 0x0400:
        case 0x0800:
            index = 0xd;
            break;
        case 0x1000:
        case 0x2000:
        case 0x4000:
        case 0x8000:
            index = 0xc;
            break;
        default:
            break;
        }
    }
    if (index == 0xf) {
        return;
    }
    if (k == 5 || k == 7 || k == 9) {
        const std::uint32_t target = m.u32(target_pointer);
        const std::uint32_t state = m.u16(target + 0x88) | m.u16(target + 0x8a);
        if ((state & 5) != 0) {
            amount = (amount & 0xff) << 1;
        }
        if ((state & 0xa) != 0) {
            amount = (amount & 0xff) >> 1;
        }
    }
    if ((m.u16(record + 0x32) & 0x40) != 0 && (k == 5 || k == 7 || k == 9)) {
        amount = (amount & 0xff) << 1;
    }
    m.put8(record + 0x15c + index, amount & 0xff);
}

} // namespace

void post_adjust(Battle &battle) {
    BattleMemory &m = battle.memory;
    const std::uint32_t slot = m.u8(target_slot);
    if (m.u8(result_address(battle, slot)) == 0) {
        // Exact kill by a party attacker counts in attacker+3a (cap 65000).
        if (m.u32(damage_address(battle, slot)) == m.u16(m.u32(target_pointer) + 0x4c) &&
            m.u8(attacker_slot) < 3) {
            const std::uint32_t attacker = m.u32(attacker_pointer);
            const std::uint32_t count = m.u16(attacker + 0x3a) + 1;
            m.put16(attacker + 0x3a, count & 0xffff);
            if (0xfde8 < (count & 0xffff)) {
                m.put16(attacker + 0x3a, (count - 1) & 0xffff);
            }
        }
        // 80094794
        const std::uint32_t target = m.u32(target_pointer);
        const std::uint32_t state = m.u16(target + 0x80);
        m.put16(target + 0x80, state & 0xefff);
        if ((state & 0x2000) != 0 && rand_percent(battle) < 0x46) {
            m.put16(target + 0x80, m.u16(target + 0x80) & 0xdfff);
        }
        // 8009481c
        const std::uint32_t descriptor = m.u32(descriptor_pointer);
        if ((m.u16(descriptor + 0xa) & 0x100) != 0) {
            if ((m.u16(target + 0x36) & 0x4000) != 0) {
                m.put8(result_address(battle, slot), 2);
            }
            if ((m.u16(target + 0x36) & 0x2000) != 0 && (m.u8(descriptor + 0x22) & 0xf) == 0) {
                m.put8(result_address(battle, slot), 2);
            }
        }
        // 800948dc
        if ((m.u16(target + 0x32) & 0x80) != 0) {
            const std::int32_t chance = m.u8(target + 0x56) != 0 ? 0x3c : 0x50;
            const std::uint32_t address = damage_address(battle, slot);
            const std::uint32_t damage = m.u32(address);
            if (rand_percent(battle) < chance) {
                m.put32(address, damage >> 1);
            } else {
                m.put32(address, (damage >> 1) + damage);
            }
        }
        // 800949b0
        if ((m.u16(target + 0x32) & 0x20) != 0) {
            const std::uint32_t attacker_index = m.u8(attacker_slot);
            m.put8(result_address(battle, attacker_index), 0);
            m.put32(damage_address(battle, attacker_index), m.u32(damage_address(battle, slot)));
        }
        // 80094a1c
        const std::uint32_t attacker_index = m.u8(attacker_slot);
        if (attacker_index < 3 && attacker_index != slot) {
            const std::uint32_t record = battle.record(slot);
            const std::uint32_t flags = m.u16(record + 0x7c);
            if ((flags & 2) != 0) {
                m.put16(record + 0x7c, flags & 0xfffd);
                m.put16(record + 0x7a, m.u16(0x800c3aa4 + slot * 2));
            }
        }
        // 80094a90
        if (m.u8(target + 0x56) == 3 && !(m.u32(0x800d2c54 + slot * 4) < m.u16(target + 0x4c))) {
            for (std::uint32_t enemy = 3; enemy < combatant_slots; ++enemy) {
                const std::uint32_t address = fixed_record_base + 0x80 + enemy * record_stride;
                m.put16(address, m.u16(address) & 0xffdf);
            }
        }
    }
    // 80094b24
    const std::uint32_t target = m.u32(target_pointer);
    if ((m.u16(target + 0x36) & 0x8000) != 0) {
        const std::uint32_t address = result_address(battle, slot);
        const std::uint32_t result = m.u8(address);
        if (result == 2) {
            m.put8(address, 0);
        } else if (result == 0 || result == 5) {
            m.put8(address, 2);
        }
    }
    // 80094bb8
    if ((m.u16(target + 0x86) & 0x80) != 0 && m.u8(result_address(battle, slot)) == 2) {
        const std::uint32_t address = damage_address(battle, slot);
        m.put32(address, m.u32(address) << 1);
    }
    // 80094c14
    if ((m.u16(target + 0x8a) & 0x200) != 0 && m.u8(result_address(battle, slot)) == 1) {
        m.put32(damage_address(battle, slot), 0);
    }
}

void counter_check(Battle &battle) {
    BattleMemory &m = battle.memory;
    const std::uint32_t target = m.u32(target_pointer);
    if ((m.u16(target + 0x7c) & 0xa000) != 0) {
        return;
    }
    const std::uint32_t state = m.u16(target + 0x80);
    if ((state & 0x1000) != 0) {
        return;
    }
    const std::uint32_t attacker = m.u8(attacker_slot);
    if (attacker < 3 && (state & 0x2000) == 0) {
        return;
    }
    if ((m.u16(target + 0x88) & 0x1000) == 0) {
        return;
    }
    const std::uint32_t kind = m.u16(m.u32(descriptor_pointer) + 0xa);
    if ((kind & 0x2000) != 0 || (kind & 0x100) != 0) {
        return;
    }
    if ((m.u8(battle.record(attacker) + 0x15a) & 0x80) != 0) {
        return;
    }
    const std::int32_t chance = m.u8(target + 0x56) != 0 ? 0x32 : 0x3c;
    if (chance < rand_percent(battle)) {
        return;
    }
    if (m.u8(m.u32(descriptor_pointer) + 0x16) == 2) {
        return;
    }
    // 80096a00: swap attacker and target; use the target's command 20.
    const std::uint32_t target_index = m.u8(target_slot);
    const std::uint32_t attacker_record = m.u32(attacker_pointer);
    m.put8(attacker_slot, target_index);
    m.put32(target_pointer, attacker_record);
    m.put8(target_slot, attacker);
    m.put32(attacker_pointer, target);
    const std::uint32_t base = battle.record_base();
    m.put32(descriptor_pointer, base + target_index * 0x5f0 + 0x1378);
    m.put8(base + target_index + result_offset, 7);
    m.put32(base + m.u8(attacker_slot) * 4 + damage_offset, 0);
    m.put8(base + message_offset, 0x34);
}

void ether_check(Battle &battle) {
    BattleMemory &m = battle.memory;
    const std::int32_t chance = static_cast<std::int32_t>(m.u8(m.u32(attacker_pointer) + 0x5b) +
                                                          m.u8(m.u32(descriptor_pointer) + 0x14));
    if (rand_percent(battle) < chance) {
        return;
    }
    set_message(battle, 0x38);
    m.put8(ether_failed, 1);
}

void status_effect_958d8(Battle &battle) {
    BattleMemory &m = battle.memory;
    const std::int32_t roll = rand_percent(battle);
    const std::uint32_t descriptor = m.u32(descriptor_pointer);
    if (static_cast<std::int32_t>(m.u8(descriptor + 0x1c)) < roll) {
        return;
    }
    if (m.u8(descriptor + 0x1d) != 0x6e) {
        return;
    }
    // Clear the target status words selected by desc+1e bits 15..10.
    const std::uint32_t target = m.u32(target_pointer);
    const std::uint32_t flags = m.u16(descriptor + 0x1e);
    for (std::uint32_t bit = 0; bit < 6; ++bit) {
        if ((flags & (0x8000u >> bit)) != 0) {
            m.put16(target + 0x84 + bit * 2, 0);
        }
    }
    set_message(battle, 0x3a);
}

void status_effect_95a78(Battle &battle) {
    BattleMemory &m = battle.memory;
    std::uint32_t descriptor = m.u32(descriptor_pointer);
    const std::uint32_t applied = apply_status(battle, m.u8(descriptor + 0x1c),
                                               m.u8(descriptor + 0x1d), m.u16(descriptor + 0x1e));
    descriptor = m.u32(descriptor_pointer);
    if ((m.u16(descriptor + 0xa) & 0x4000) != 0) {
        set_status_duration(battle, m.u8(target_slot), m.u8(descriptor + 0x1d),
                            m.u16(descriptor + 0x1e), 5);
        return;
    }
    set_status_duration(battle, m.u8(target_slot), m.u8(descriptor + 0x1d),
                        m.u16(descriptor + 0x1e), m.u8(descriptor + 0x11));
    if (static_cast<std::int8_t>(applied & 0xff) != 1) {
        m.put8(result_address(battle, m.u8(target_slot)), 6);
    }
}

void status_effect_95b44(Battle &battle) {
    BattleMemory &m = battle.memory;
    const std::uint32_t attacker = m.u32(attacker_pointer);
    const std::uint32_t applied =
        apply_status(battle, m.u8(attacker + 2), m.u8(attacker + 3), m.u16(attacker));
    if (static_cast<std::int8_t>(applied & 0xff) != 1) {
        return;
    }
    const std::uint32_t descriptor = m.u32(descriptor_pointer);
    set_status_duration(battle, m.u8(target_slot), m.u8(descriptor + 0x1d),
                        m.u16(descriptor + 0x1e), 5);
}

} // namespace xem::reconstruction::battle
