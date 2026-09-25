#include "xem/reconstruction/battle.hpp"

#include <array>
#include <cstdint>
#include <format>

namespace xem::reconstruction::battle {
namespace {

// The helpers address the records absolutely from 800ccce8.
constexpr std::uint32_t fixed_record_base = 0x800ccce8;
constexpr std::uint32_t present = 0x800d2dcc;   // u8 per slot
constexpr std::uint32_t slot_info = 0x800c3eb4; // 0x1c bytes per slot; +0 group, +3 flag
constexpr std::uint32_t slot_info_stride = 0x1c;
constexpr std::uint32_t slot_bits = 0x800c3448;    // u16 per slot
constexpr std::uint32_t slot_flags = 0x800d32a1;   // u8, 8 bytes per slot
constexpr std::uint32_t action_list = 0x800d2e5c;  // 32 entries of 8 bytes
constexpr std::uint32_t event_types = 0x800c402f;  // u8, 32 entries of 0x48 bytes
constexpr std::uint32_t enemy_blocks = 0x800d3400; // 0x40 per enemy: +0 script pointer
constexpr std::uint32_t variables = 0x800d3420;    // u16 per variable in the enemy block
constexpr std::uint32_t bytes = 0x800d3430;        // u8 per variable in the enemy block

// Handler (jal target) of each action case 0x01..0x74 in the table at
// 8006fc3c; 80079934 is the common tail (case 62 has no handler), 8007a7bc the
// default.
constexpr std::array<std::uint32_t, 0x74> action_handlers{
    0x8007a828, 0x8007a874, 0x8007a8b4, 0x8007a900, 0x8007a92c, 0x8007a968, 0x8007a9a8, 0x8007a9d0,
    0x8007aa1c, 0x8007aa60, 0x8007aab8, 0x8007aaf4, 0x8007ab30, 0x8007ab68, 0x8007aba0, 0x8007abd8,
    0x8007ac30, 0x8007ac80, 0x8007acdc, 0x8007ad24, 0x8007ad6c, 0x8007adb0, 0x8007adf4, 0x8007ae38,
    0x8007ae98, 0x8007aef0, 0x8007af5c, 0x8007afac, 0x8007affc, 0x8007b040, 0x8007b084, 0x8007b0c8,
    0x8007b134, 0x8007b198, 0x8007b208, 0x8007b264, 0x8007b2c0, 0x8007b310, 0x8007b360, 0x8007b3b0,
    0x8007b3e4, 0x8007b424, 0x8007b4b8, 0x8007b578, 0x8007b608, 0x8007b6c0, 0x8007b7b0, 0x8007b8d4,
    0x8007b914, 0x8007b958, 0x8007b98c, 0x8007b9c8, 0x8007ba04, 0x8007ba44, 0x8007ba88, 0x8007bab8,
    0x8007bae8, 0x8007bb2c, 0x8007bb70, 0x8007bbd8, 0x8007bc40, 0x8007bc84, 0x8007bce8, 0x8007bd5c,
    0x8007bea8, 0x8007c040, 0x8007c1a4, 0x8007c33c, 0x8007c4a0, 0x8007c580, 0x8007c678, 0x8007c75c,
    0x8007c840, 0x8007c9d4, 0x8007cb20, 0x8007cc50, 0x8007cd10, 0x8007cdd0, 0x8007cea4, 0x8007cfb8,
    0x8007d0cc, 0x8007d148, 0x8007d1a8, 0x8007d1dc, 0x8007d30c, 0x8007d344, 0x8007d478, 0x8007d5b0,
    0x8007d610, 0x8007d6a8, 0x8007d7b4, 0x8007d8c0, 0x8007da1c, 0x8007db78, 0x8007dcf8, 0x8007de78,
    0x8007dfd4, 0x80079934, 0x8007e154, 0x8007e1d0, 0x8007e234, 0x8007e334, 0x8007e438, 0x8007e554,
    0x8007e674, 0x8007e6a0, 0x8007e6f0, 0x8007e740, 0x8007e780, 0x8007a7bc, 0x8007a7bc, 0x8007e7c0,
    0x8007e7e4, 0x8007e8ac, 0x8007e8e0, 0x8007e934,
};

// Condition handlers of the table at 8006fe0c for 0x81..0x98 and 0x9b.
constexpr std::array<std::uint32_t, 0x1c> condition_handlers{
    0,          0x8007e954, 0x8007e98c, 0x8007e9d0, 0x8007ea08, 0x8007ea4c, 0x8007ea84,
    0x8007eac8, 0x8007eb08, 0x8007eb50, 0x8007eb90, 0x8007ebd8, 0x8007ec10, 0x8007ec54,
    0x8007ec94, 0x8007ecdc, 0x8007ed14, 0x8007ed58, 0x8007ed98, 0x8007ede0, 0x8007ee28,
    0x8007ee70, 0x8007eea8, 0x8007eed0, 0x8007eee8, 0,          0,          0x8007ef44,
};

std::uint32_t variable(std::uint32_t enemy, std::uint32_t index) {
    return variables + (enemy & 0xff) * 0x40 + index * 2;
}
std::uint32_t byte_variable(std::uint32_t enemy, std::uint32_t index) {
    return bytes + (enemy & 0xff) * 0x40 + index;
}

// 80089c08: the mask bit of a slot.
std::uint32_t slot_bit(const Battle &battle, std::uint32_t slot) {
    return battle.memory.u16(slot_bits + (slot & 0xff) * 2);
}

// 80089c9c: the slot's bit in `mask`, 0 for slots >= 16.
std::uint32_t slot_in_mask(const Battle &battle, std::uint32_t mask, std::uint32_t slot) {
    const std::uint32_t s = slot & 0xff;
    if (s >= 0x10)
        return 0;
    return battle.memory.u16(slot_bits + s * 2) & mask;
}

// 80079e7c: the first slot in `mask`, 11 when none.
std::uint32_t first_slot(const Battle &battle, std::uint32_t mask) {
    std::uint32_t slot = 0;
    for (; slot < 11; ++slot)
        if ((slot_in_mask(battle, mask & 0xffff, slot) & 0xffff) != 0)
            break;
    return slot & 0xff;
}

// 8001bd40: random byte in lo..hi (signed div by hi - lo + 1).
std::uint32_t range_random(Battle &battle, std::uint32_t lo, std::uint32_t hi) {
    const std::uint32_t low = lo & 0xff;
    if (low == 0xff)
        return 0xff;
    const std::uint32_t high = hi & 0xff;
    if (high == 0)
        return 0;
    if (high == low)
        return low;
    const std::int32_t span = static_cast<std::int32_t>(high) - static_cast<std::int32_t>(low);
    if (span >= 0xff)
        return battle.rand() & 0xff;
    const auto value = static_cast<std::int32_t>(battle.rand() & 0xff);
    const std::int32_t divisor = span + 1;
    const std::int32_t remainder = divisor == 0 ? value : value % divisor; // MIPS div by zero
    return (lo + static_cast<std::uint32_t>(remainder)) & 0xff;
}

// 8007a628: the slot is present, active and not down (c002); with `any` zero
// also not flagged 0x20 at +0x84.
bool targetable(const Battle &battle, std::uint32_t slot, std::uint32_t any) {
    const auto &memory = battle.memory;
    const std::uint32_t s = slot & 0xff;
    if (memory.u8(present + s) == 0)
        return false;
    if (memory.u8(slot_info + 3 + s * slot_info_stride) != 0)
        return false;
    const std::uint32_t record = fixed_record_base + s * record_stride;
    if ((memory.u16(record + 0x7c) & 0xc002) != 0)
        return false;
    if ((any & 0xff) != 0)
        return true;
    return (memory.u16(record + 0x84) & 0x20) == 0;
}

// 8007a828 (op 01): list[n*8 + b1] = b2; a zero b1 starts the next entry.
std::uint32_t op_list_byte(Battle &battle, std::uint32_t pc, std::uint32_t count) {
    auto &memory = battle.memory;
    memory.put8(action_list + (count & 0xff) * 8 + memory.u8(pc + 1), memory.u8(pc + 2));
    if (memory.u8(pc + 1) == 0) // 8007a858 reload
        ++count;
    return count & 0xff;
}

// 8007aaf4 (op 0c): byte[b1] %= b2 (divu).
void op_byte_modulo(Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    auto &memory = battle.memory;
    const std::uint32_t address = byte_variable(enemy, memory.u8(pc + 1));
    const std::uint32_t value = memory.u8(address);
    const std::uint32_t divisor = memory.u8(pc + 2);
    memory.put8(address, divisor == 0 ? value : value % divisor); // MIPS divu by zero
}

// 8007bc84 (op 3e): byte[b1] = random 0..b2 (8001bd40).
void op_byte_random(Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    auto &memory = battle.memory;
    const std::uint32_t value = range_random(battle, 0, memory.u8(pc + 2));
    memory.put8(byte_variable(enemy, memory.u8(pc + 1)), value);
}

// 8007bea8 (op 41): var[b1] = the bit of a random party slot passing
// 8007a628(b2), in the enemy's group and without 800d32a1 set; 0 when all
// three fail.
void op_random_party_target(Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    auto &memory = battle.memory;
    std::array<std::uint8_t, 3> tried{};
    memory.put16(variable(enemy, memory.u8(pc + 1)), 0);
    const std::uint32_t own_group = slot_info + ((enemy & 0xff) + 3) * slot_info_stride;
    while ((tried[0] & tried[1] & tried[2]) == 0) {
        const std::uint32_t slot = range_random(battle, 0, 2) & 0xff;
        if (tried[slot] != 0)
            continue;
        if (targetable(battle, slot, memory.u8(pc + 2)) &&
            memory.u8(own_group) == memory.u8(slot_info + slot * slot_info_stride) &&
            memory.u8(slot_flags + slot * 8) == 0) {
            const std::uint32_t bit = slot_bit(battle, slot);
            memory.put16(variable(enemy, memory.u8(pc + 1)), bit);
            return;
        }
        tried[slot] = 1;
    }
}

// 8007cfb8 (op 50): byte[b1] = the number of targetable enemy slots 3..10 in
// the group of the first slot of var[b2].
void op_count_group(Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    auto &memory = battle.memory;
    std::uint32_t count = 0;
    for (std::uint32_t slot = 3; slot < 11; ++slot) {
        if (!targetable(battle, slot, 0))
            continue;
        const std::uint32_t first =
            first_slot(battle, memory.u16(variable(enemy, memory.u8(pc + 2))));
        if (memory.u8(slot_info + slot * slot_info_stride) ==
            memory.u8(slot_info + first * slot_info_stride))
            ++count;
    }
    memory.put8(byte_variable(enemy, memory.u8(pc + 1)), count);
}

// 8007d148 (op 52): list[n*8 + b1] (u16, two byte stores) = var[b2].
void op_list_variable(Battle &battle, std::uint32_t pc, std::uint32_t enemy, std::uint32_t count) {
    auto &memory = battle.memory;
    const std::uint32_t value = memory.u16(variable(enemy, memory.u8(pc + 2)));
    const std::uint32_t entry = (count & 0xff) * 8;
    memory.put8(action_list + entry + memory.u8(pc + 1), value);
    memory.put8(action_list + entry + memory.u8(pc + 1) + 1, value >> 8); // 8007d188 reload
}

// 8007e1d0 (op 64): var[b1] = the enemy's own slot bit.
void op_self_bit(Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    auto &memory = battle.memory;
    const std::uint32_t bit = slot_bit(battle, enemy + 3);
    memory.put16(variable(enemy, memory.u8(pc + 1)), bit);
}

// 8007e98c (cond 82): var[b1] == b2 | b3 << 8.
bool cond_variable_equal(const Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    const auto &memory = battle.memory;
    const std::uint32_t value = memory.u8(pc + 2) | memory.u8(pc + 3) << 8;
    return memory.u16(variable(enemy, memory.u8(pc + 1))) == value;
}

// 8007e9d0 (cond 83): byte[b1] <= b2.
bool cond_byte_at_most(const Battle &battle, std::uint32_t pc, std::uint32_t enemy) {
    const auto &memory = battle.memory;
    return memory.u8(byte_variable(enemy, memory.u8(pc + 1))) <= memory.u8(pc + 2);
}

// 8007ef6c: one action; returns the action count.
std::uint32_t run_action(Battle &battle, std::uint32_t &pc, std::uint32_t enemy,
                         std::uint32_t count) {
    const std::uint32_t opcode = battle.memory.u8(pc);
    if (opcode - 1 >= 0x74)
        throw BattleError(
            std::format("Enemy script action {:#04x} (default 8007a7bc) at {:08x}", opcode, pc));
    switch (opcode) {
    case 0x01:
        count = op_list_byte(battle, pc, count);
        break;
    case 0x0c:
        op_byte_modulo(battle, pc, enemy);
        break;
    case 0x3e:
        op_byte_random(battle, pc, enemy);
        break;
    case 0x41:
        op_random_party_target(battle, pc, enemy);
        break;
    case 0x50:
        op_count_group(battle, pc, enemy);
        break;
    case 0x52:
        op_list_variable(battle, pc, enemy, count);
        break;
    case 0x62: // 8007f89c: no handler
        break;
    case 0x64:
        op_self_bit(battle, pc, enemy);
        break;
    default:
        throw BattleError(std::format("Enemy script action {:#04x} (handler {:08x}) at {:08x}",
                                      opcode, action_handlers[opcode - 1], pc));
    }
    pc += 4; // 8007f89c
    return count & 0xff;
}

// 8007f8c0: one condition, or with 99 the OR of the conditions that follow.
bool run_condition(Battle &battle, std::uint32_t &pc, std::uint32_t enemy) {
    std::uint32_t result = 0;
    std::uint32_t any = 0;
    std::uint32_t chained = 0;
    for (;;) {
        const std::uint32_t opcode = battle.memory.u8(pc);
        const std::uint32_t index = opcode - 0x80;
        if (index < 0x1c) {
            switch (opcode) {
            case 0x80:
            case 0x9a:
                result = 1;
                break;
            case 0x99:
                any = 1;
                break;
            case 0x82:
                result = cond_variable_equal(battle, pc, enemy) ? 1 : 0;
                break;
            case 0x83:
                result = cond_byte_at_most(battle, pc, enemy) ? 1 : 0;
                break;
            default:
                throw BattleError(
                    std::format("Enemy script condition {:#04x} (handler {:08x}) at {:08x}", opcode,
                                condition_handlers[index], pc));
            }
        }
        pc += 4; // 8007fb14
        if ((any & 0xff) == 0)
            return (result & 0xff) != 0;
        chained |= result;
        if (battle.memory.u8(pc) < 0x80)
            return (chained & 0xff) != 0;
    }
}

// 80079948: skip the remaining conditions, then to the next condition 80..ef.
// The second loop also passes fd/ff, so a failed last rule runs on past its
// terminator, as the original does.
void skip_rule(const Battle &battle, std::uint32_t &pc) {
    while (battle.memory.u8(pc) >= 0x80)
        pc += 4;
    while (((battle.memory.u8(pc) - 0x80) & 0xff) >= 0x70)
        pc += 4;
}

} // namespace

// 800799c8. a1 (flag) is never read.
void run_enemy_script(Battle &battle, std::uint32_t slot, std::uint32_t flag) {
    static_cast<void>(flag);
    auto &memory = battle.memory;
    const std::uint32_t enemy = slot - 3;
    std::uint32_t pc = memory.u32(enemy_blocks + (enemy & 0xff) * 0x40);
    for (std::uint32_t i = 0; i < 0x100; ++i)
        memory.put8(action_list + i, 0);
    for (std::int32_t offset = 0x8b8; offset >= 0; offset -= 0x48)
        memory.put8(event_types + static_cast<std::uint32_t>(offset), 0xff);
    std::uint32_t count = 0;
    for (;;) {
        const std::uint32_t opcode = memory.u8(pc);
        if (opcode == 0xfd || opcode == 0xff)
            return;
        if (opcode < 0x80)
            count = run_action(battle, pc, enemy & 0xff, count & 0xff);
        else if (!run_condition(battle, pc, enemy & 0xff))
            skip_rule(battle, pc);
    }
}

bool run_reaction_script(Battle &battle, std::uint32_t slot) {
    auto &memory = battle.memory;
    const std::uint32_t enemy = (slot - 3) & 0xff;
    const auto record = fixed_record_base + (enemy + 3) * record_stride;
    // A knocked-out enemy reacts only with its +34 bit 800.
    if ((memory.u16(record + 0x7c) & 0x8000) != 0 && (memory.u16(record + 0x34) & 0x800) == 0)
        return false;
    memory.put8(action_list, 0);
    bool special = false;
    if (memory.u8(0x800c3d18 + enemy * 4) != 0) {
        std::uint32_t pc = memory.u32(enemy_blocks + enemy * 0x40 + 8);
        for (std::uint32_t i = 0; i < 0x100; ++i)
            memory.put8(action_list + i, 0);
        std::uint32_t count = 0;
        for (;;) {
            const std::uint32_t opcode = memory.u8(pc);
            if (opcode == 0xfd || opcode == 0xff)
                break;
            if (opcode >= 0x80) {
                if (!run_condition(battle, pc, enemy))
                    skip_rule(battle, pc);
                continue;
            }
            if (opcode == 0x62)
                special = true;
            count = run_action(battle, pc, enemy, count & 0xff);
        }
    }
    if (memory.u8(action_list) != 0)
        throw BattleError("The reaction's actions (80079ab0 runs the executor 800793f0 inside the "
                          "command menu) are not reconstructed");
    return special;
}

} // namespace xem::reconstruction::battle
