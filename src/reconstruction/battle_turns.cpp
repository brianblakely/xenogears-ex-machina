#include "xem/reconstruction/battle.hpp"

#include <cstdint>

namespace xem::reconstruction::battle {
namespace {

// 8007171c addresses the records absolutely from 800ccce8.
constexpr std::uint32_t fixed_record_base = 0x800ccce8;
constexpr std::uint32_t atb_enabled = 0x800d3298;  // u8
constexpr std::uint32_t present = 0x800d2dcc;      // u8 per slot
constexpr std::uint32_t ready = 0x800d2de4;        // u8 per slot
constexpr std::uint32_t timer = 0x800d2e06;        // u16 per slot
constexpr std::uint32_t timer_reload = 0x800d2df0; // u16 per slot (timer - 0x16)
constexpr std::uint32_t alternate = 0x800d2e1c;    // u16 per slot
constexpr std::uint32_t turn_slot_offset = 0x2d3;  // u8 in the turn state
constexpr std::uint32_t saved_flags = 0x800c3aa4;  // u16 per party slot

// 80094d24: with one party member flagged c000 and two flagged 2 (or two and
// one), clear flag 2 and restore +0x7a from 800c3aa4.
void balance_party_flags(Battle &battle) {
    auto &memory = battle.memory;
    std::uint32_t downed = 0;
    std::uint32_t base = battle.record_base();
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if ((memory.u16(base + slot * record_stride + 0x7c) & 0xc000) != 0) {
            ++downed;
        }
    }
    std::uint32_t flagged = 0;
    base = battle.record_base(); // 80094d80 reload
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if ((memory.u16(base + slot * record_stride + 0x7c) & 2) != 0) {
            ++flagged;
        }
    }
    const auto restore = [&] {
        const std::uint32_t records = battle.record_base(); // 80094dec / 80094e74
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            const std::uint32_t record = records + slot * record_stride;
            const std::uint32_t flags = memory.u16(record + 0x7c);
            if ((flags & 2) != 0) {
                memory.put16(record + 0x7c, flags & 0xfffd);
                memory.put16(record + 0x7a, memory.u16(saved_flags + slot * 2));
            }
        }
    };
    if (downed == 1 && flagged == 2) { // 80094dcc
        restore();
    }
    if (downed == 2 && flagged == 1) { // 80094e5c, counts cached
        restore();
    }
}

// 80098af8: turn timer value for a slot; sets the attacker globals and, for
// party slots, the command descriptor.
std::uint32_t turn_timer_value(Battle &battle, std::uint32_t slot) {
    auto &memory = battle.memory;
    const std::uint32_t s = slot & 0xff;
    const std::uint32_t base = battle.record_base();
    const std::uint32_t record = base + s * record_stride;
    memory.put32(attacker_block_pointer, record + 0xa4);
    memory.put32(attacker_pointer, record);
    std::uint32_t speed = 9; // s0
    if (s >= 3) {
        speed = memory.u8(record + 0x5a) * 9; // 80098c00
    } else {
        std::uint32_t stat = 0;
        std::uint32_t descriptor = 0;
        if ((memory.u8(record + 0x15a) & 0x80) == 0) {
            descriptor = base + s * 0x5f0 + 0x1058; // 80098b5c
            stat = memory.u8(record + 0x5a);
        } else {
            descriptor = base + s * 0x690 + 0x2228; // 80098bac
            stat = memory.u8(record + 0x13c);
        }
        descriptor += memory.u8(base + 0x5fc2) * 40;
        memory.put32(descriptor_pointer, descriptor);
        const std::int32_t penalty = memory.s8(descriptor + 0x27);
        const auto value = static_cast<std::int32_t>(stat);
        if (penalty < value) {
            speed = static_cast<std::uint32_t>(value - penalty) * 9;
        }
    }
    if ((speed & 0xffff) >= 0xa6) { // 80098c10
        speed = 0xa0;
    }
    speed = 0xa5 - speed;
    // 80098c30: signed rand % 8 (truncating), minus 4.
    const std::int32_t jitter = static_cast<std::int32_t>(battle.rand()) % 8 - 4;
    speed -= static_cast<std::uint32_t>(jitter);
    balance_party_flags(battle);
    return speed & 0xff;
}

} // namespace

std::uint32_t turn_timer(Battle &battle, std::uint32_t slot) {
    return turn_timer_value(battle, slot);
}

void atb_tick(Battle &battle) {
    auto &memory = battle.memory;
    if (memory.u8(atb_enabled) == 0) {
        return;
    }
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        if (memory.u8(present + slot) == 0 || memory.u8(ready + slot) != 0) {
            continue;
        }
        const std::uint32_t record = fixed_record_base + slot * record_stride;
        std::uint32_t step = 1;
        if (((memory.u16(record + 0x84) | memory.u16(record + 0x86)) & 0x8000) != 0) {
            step = 2;
        }
        if ((memory.u16(record + 0x7c) & 0x1000) != 0) { // 800717a8: tick every other call
            const std::uint32_t toggle = memory.u16(alternate + slot * 2) ^ 1;
            memory.put16(alternate + slot * 2, toggle);
            if (toggle != 0) {
                continue;
            }
        }
        const std::uint32_t flags = memory.u16(record + 0x7c); // 800717dc reload
        if ((flags & 0x2000) != 0) {
            // 800717f0: count down +0x15c instead; clear 2000 at zero.
            const std::uint32_t count = memory.u8(record + 0x15c) - step;
            memory.put8(record + 0x15c, count);
            if ((count & 0xff) == 0) {
                const std::uint32_t current = memory.u16(record + 0x7c);
                memory.put8(record + 0x15c, 0);
                memory.put16(record + 0x7c, current & 0xdfff);
            }
            continue;
        }
        if ((flags & 0x80) != 0 || (memory.u16(record + 0x80) & 0x1000) != 0) {
            continue;
        }
        // 80071870: count down the timer; ready at <= 0 (signed 16-bit).
        const std::uint32_t remaining = memory.u16(timer + slot * 2) - step;
        memory.put16(timer + slot * 2, remaining);
        if (static_cast<std::int16_t>(remaining & 0xffff) <= 0) {
            memory.put8(ready + slot, 1);
            memory.put16(timer + slot * 2, 0);
        }
    }
}

void reload_turn_timer(Battle &battle) {
    auto &memory = battle.memory;
    std::uint32_t slot = memory.u8(memory.u32(turn_state_pointer) + turn_slot_offset);
    if (memory.u8(ready + slot) != 0xff) {
        memory.put8(ready + slot, 0);
    }
    slot = memory.u8(memory.u32(turn_state_pointer) + turn_slot_offset); // 800718f4 reload
    const std::uint32_t value = turn_timer_value(battle, slot);
    const std::uint32_t state = memory.u32(turn_state_pointer); // 8007190c reload
    slot = memory.u8(state + turn_slot_offset);
    memory.put16(timer + slot * 2, value);
    slot = memory.u8(state + turn_slot_offset); // 80071938 reload
    memory.put16(timer_reload + slot * 2, memory.u16(timer + slot * 2));
}

} // namespace xem::reconstruction::battle
