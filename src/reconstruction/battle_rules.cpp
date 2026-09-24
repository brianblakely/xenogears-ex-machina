#include "xem/reconstruction/battle.hpp"

#include <cstdint>

namespace xem::reconstruction::battle {
namespace {

// Both functions address the records absolutely from 800ccce8, not through
// the pointer at 800c34b0.
constexpr std::uint32_t fixed_record_base = 0x800ccce8;
constexpr std::uint32_t present = 0x800d2dcc;   // u8 per slot
constexpr std::uint32_t ready = 0x800d2de4;     // u8 per slot
constexpr std::uint32_t slot_info = 0x800c3eb4; // 0x1c bytes per slot
constexpr std::uint32_t slot_info_stride = 0x1c;
constexpr std::uint32_t knocked_out = 0x800c48e8; // u16 slot mask
constexpr std::uint32_t alive_mask = 0x800d39dc;  // u16 slot mask
constexpr std::uint32_t outcome = 0x800c48ea;     // u8
constexpr std::uint32_t slot_bits = 0x800c3448;   // u16 per slot

std::uint32_t fixed_record(std::uint32_t slot) { return fixed_record_base + slot * record_stride; }

// 80089c08: the mask bit of a slot.
std::uint32_t slot_bit(const Battle &battle, std::uint32_t slot) {
    return battle.memory.u16(slot_bits + (slot & 0xff) * 2);
}

// 80089c48: every mask bit except the slot's.
std::uint32_t slot_clear_mask(const Battle &battle, std::uint32_t slot) {
    return ~slot_bit(battle, slot) & 0xffff;
}

// 80089c9c: the slot's bit in `mask`, 0 for slots >= 16.
std::uint32_t slot_in_mask(const Battle &battle, std::uint32_t mask, std::uint32_t slot) {
    const std::uint32_t s = slot & 0xff;
    if (s >= 0x10) {
        return 0;
    }
    return battle.memory.u16(slot_bits + s * 2) & mask;
}

// 800883ac: drop a knocked-out slot from its formation group (group byte
// 800c3eb4, member byte 800c3eb5, 4-byte group entries at 800d301c; enemies
// use entries 8.., slots with 800d32a1[slot*8] set add 0x10).
void leave_group(Battle &battle, std::uint32_t slot) {
    auto &memory = battle.memory;
    const std::uint32_t s = slot & 0xff;
    std::uint32_t bank = s >= 3 ? 8 : 0;
    if (memory.u8(0x800d32a1 + s * 8) != 0) {
        bank |= 0x10;
    }
    const std::uint32_t info = slot_info + s * slot_info_stride;
    const std::uint32_t count = 0x800d301c + (memory.u8(info) + bank) * 4;
    memory.put8(count, memory.u8(count) - 1);
    const std::uint32_t keep = slot_clear_mask(battle, memory.u8(info + 1));
    const std::uint32_t members = 0x800d301d + (memory.u8(info) + bank) * 4; // 80088448 reload
    memory.put8(members, memory.u8(members) & keep);
}

// 80085618 codes 0/5/7/8 (8008570c): damage to HP, or to gear HP (u32 +0x104)
// when the slot's gear byte 800c3eb8 is set.
void apply_damage(Battle &battle, std::uint32_t slot, std::uint32_t record,
                  std::uint32_t amount_address) {
    auto &memory = battle.memory;
    if (memory.u8(slot_info + 4 + slot * slot_info_stride) == 0) {
        const std::int32_t hp = memory.s16(record + 0x4c);
        const std::int32_t left = hp - memory.s16(amount_address);
        if (left > 0) {
            memory.put16(record + 0x4c, static_cast<std::uint32_t>(left));
            return;
        }
        memory.put16(record + 0x4c, 0);
        memory.put16(knocked_out, memory.u16(knocked_out) | slot_bit(battle, slot));
        memory.put16(record + 0x7c, memory.u16(record + 0x7c) | 0x8000);
    } else {
        const std::uint32_t left = memory.u32(record + 0x104) - memory.u16(amount_address);
        if (static_cast<std::int32_t>(left) > 0) { // 800857bc bgtz
            memory.put32(record + 0x104, left);
            return;
        }
        memory.put32(record + 0x104, 0);
        memory.put16(knocked_out, memory.u16(knocked_out) | slot_bit(battle, slot));
        memory.put16(record + 0x120, memory.u16(record + 0x120) | 0x8000);
        memory.put16(record + 0x7c, memory.u16(record + 0x7c) | 0x8000);
    }
    if (slot >= 3) {
        leave_group(battle, slot);
    }
}

// 80085618 code 2 (8008583c): heal HP up to +0x4e, or gear HP (+0x104) up to
// +0x108 when the gear byte is set and 800c2050 is clear.
void apply_heal(Battle &battle, std::uint32_t slot, std::uint32_t record,
                std::uint32_t amount_address) {
    auto &memory = battle.memory;
    if (memory.u8(slot_info + 4 + slot * slot_info_stride) != 0 && memory.u8(0x800c2050) == 0) {
        const std::uint32_t hp = memory.u16(amount_address) + memory.u32(record + 0x104);
        memory.put32(record + 0x104, hp);
        const std::uint32_t max = memory.u32(record + 0x108);
        if (max < hp) {
            memory.put32(record + 0x104, max);
        }
        return;
    }
    const std::uint32_t hp = (memory.u16(record + 0x4c) + memory.u16(amount_address)) & 0xffff;
    memory.put16(record + 0x4c, hp);
    const std::uint32_t max = memory.u16(record + 0x4e);
    if (max < hp) {
        memory.put16(record + 0x4c, max);
    }
}

} // namespace

void apply_results(Battle &battle, std::uint32_t queue) {
    auto &memory = battle.memory;
    const std::uint32_t event = 0x800c3fe8 + (queue & 0xff) * 0x48;
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        const std::uint32_t record = fixed_record(slot);
        const std::uint32_t amount = event + slot * 2;
        if (memory.u8(present + slot) == 0) {
            continue;
        }
        if ((memory.u16(record + 0x7c) & 0x8000) != 0) {
            memory.put16(knocked_out, memory.u16(knocked_out) | slot_bit(battle, slot));
            continue;
        }
        // Jump table 80070250 over the code byte at event + 0x18.
        switch (memory.u8(event + 0x18 + slot)) {
        case 0:
        case 5:
        case 7:
        case 8:
            apply_damage(battle, slot, record, amount);
            break;
        case 1:
        case 9: { // 80085908: EP loss, no refresh flag
            const std::int32_t left = memory.s16(record + 0x50) - memory.s16(amount);
            memory.put16(record + 0x50, left > 0 ? static_cast<std::uint32_t>(left) : 0);
            continue;
        }
        case 2:
            apply_heal(battle, slot, record, amount);
            break;
        case 3: { // 80085954: EP gain up to +0x52, no refresh flag
            if (memory.u8(slot_info + 4 + slot * slot_info_stride) != 0 &&
                memory.u8(0x800c2050) == 0) {
                continue;
            }
            const std::uint32_t ep = (memory.u16(record + 0x50) + memory.u16(amount)) & 0xffff;
            memory.put16(record + 0x50, ep);
            const std::uint32_t max = memory.u16(record + 0x52);
            if (max < ep) {
                memory.put16(record + 0x50, max);
            }
            continue;
        }
        case 10: { // 800859cc: +0xdc loss
            const std::int32_t left = static_cast<std::int32_t>(memory.u16(record + 0xdc)) -
                                      static_cast<std::int32_t>(memory.u16(amount));
            memory.put16(record + 0xdc, left > 0 ? static_cast<std::uint32_t>(left) : 0);
            break;
        }
        case 11: { // 80085a18: +0xdc gain up to +0xde
            const std::uint32_t value = (memory.u16(record + 0xdc) + memory.u16(amount)) & 0xffff;
            memory.put16(record + 0xdc, value);
            const std::uint32_t max = memory.u16(record + 0xde);
            if (max < value) {
                memory.put16(record + 0xdc, max);
            }
            break;
        }
        default: // 4, 6 and codes >= 12 (800856ec)
            continue;
        }
        // 80085a5c: per-slot refresh flag through the pointer at 800c3eac.
        memory.put8(memory.u32(0x800c3eac) + slot + 0x2eb, 1);
    }
}

void update_alive(Battle &battle) {
    auto &memory = battle.memory;
    memory.put16(alive_mask, 0);
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (memory.u8(present + slot) == 0) {
            continue;
        }
        const std::uint32_t record = fixed_record(slot);
        const std::uint32_t flags = memory.u16(record + 0x7c);
        if ((flags & 0xc000) != 0) {
            if ((flags & 0x8000) != 0) {
                memory.put16(record + 0x4c, 0);
            }
            memory.put8(ready + slot, 0xff);
            continue;
        }
        if (memory.u8(slot_info + 3 + slot * slot_info_stride) != 0) {
            continue;
        }
        memory.put16(alive_mask, memory.u16(alive_mask) | slot_bit(battle, slot));
    }
    for (std::uint32_t slot = 3; slot < combatant_slots; ++slot) {
        if (memory.u8(present + slot) == 0) {
            continue;
        }
        const std::uint32_t record = fixed_record(slot);
        const bool gear = memory.u8(slot_info + 4 + slot * slot_info_stride) != 0;
        const std::uint32_t flags = memory.u16(record + (gear ? 0x120 : 0x7c));
        bool counts = false;
        if ((flags & 0xc000) != 0) {
            // 80072680: knocked-out slots still count while in the 800c3608 mask.
            if ((slot_in_mask(battle, memory.u16(0x800c3608), slot) & 0xffff) == 0) {
                if (gear) {
                    memory.put32(record + 0x104, 0);
                } else {
                    memory.put16(record + 0x4c, 0);
                }
                memory.put8(ready + slot, 0xff);
                continue;
            }
            counts = true;
        } else {
            // 8007270c: 800c3eb7 per slot and 800c3d1b per enemy (4 bytes each).
            counts = memory.u8(slot_info + 3 + slot * slot_info_stride) == 0 &&
                     memory.u8(0x800c3d1b + (slot - 3) * 4) == 0;
        }
        if (counts) {
            memory.put16(alive_mask, memory.u16(alive_mask) | slot_bit(battle, slot));
        }
    }

    if ((memory.u16(alive_mask) & 0x7f8) == 0) {
        memory.put8(outcome, 1);
    }
    if ((memory.u16(alive_mask) & 7) == 0) {
        memory.put8(outcome, 0x81);
    }
    if (memory.u8(outcome) != 0) {
        return;
    }
    // 800727c4: if every alive slot has +0x80 bit 0x1000, clear it on the
    // first one.
    std::uint32_t remaining = memory.u16(alive_mask);
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        if ((slot_in_mask(battle, remaining, slot) & 0xffff) != 0 &&
            (memory.u16(fixed_record(slot) + 0x80) & 0x1000) != 0) {
            remaining &= slot_clear_mask(battle, slot) & 0xffff;
        }
    }
    if (remaining != 0) {
        return;
    }
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        const std::uint32_t record = fixed_record(slot);
        if ((slot_in_mask(battle, memory.u16(alive_mask), slot) & 0xffff) == 0) {
            continue;
        }
        const std::uint32_t flags = memory.u16(record + 0x80);
        if ((flags & 0x1000) != 0) {
            memory.put16(record + 0x80, flags & 0xefff);
            return;
        }
    }
}

} // namespace xem::reconstruction::battle
