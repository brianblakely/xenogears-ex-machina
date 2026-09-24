#include "xem/reconstruction/battle.hpp"

#include "xem/reconstruction/field_actor.hpp"

namespace xem::reconstruction::battle {
namespace {

// Offsets from the record base (800ccce8 in the observed battles).
constexpr std::uint32_t damage = 0x5f6c;       // u32 per slot
constexpr std::uint32_t result_code = 0x5fa0;  // u8 per slot; ff = untouched
constexpr std::uint32_t party_totals = 0x5f54; // u32 per party slot
constexpr std::uint32_t gear_totals = 0x5f60;  // u32 per party slot in gear
constexpr std::uint32_t target_mask = 0x5fac;  // u16 (800d2c94)
constexpr std::uint32_t shown_command = 0x5fb0;
constexpr std::uint32_t attacker_index = 0x5fc1; // u8 (800d2ca9)
constexpr std::uint32_t command_index = 0x5fc2;  // u8 (800d2caa)
constexpr std::uint32_t party_commands = 0x1058; // 28-byte descriptors, 5f0 per party slot
constexpr std::uint32_t enemy_commands = 0x35d8;
constexpr std::uint32_t gear_commands = 0x2228; // 690 per party slot

constexpr std::uint32_t formula_table = 0x800c348c;
constexpr std::uint32_t resolve_status = 0x800d2db8; // u8, cleared per action
constexpr std::uint32_t item_used = 0x800c34ae;      // u8
constexpr std::uint32_t ether_failed = 0x800d2dc4;   // u8

bool in_gear(const Battle &battle, std::uint32_t slot) {
    return (battle.memory.u8(battle.record(slot) + 0x15a) & 0x80) != 0;
}

// 80097d08: mark slots 11..0 untouched with no damage.
void clear_results(Battle &battle) {
    for (std::int32_t slot = 11; slot >= 0; --slot) {
        const auto index = static_cast<std::uint32_t>(slot);
        battle.memory.put8(battle.record_base() + result_code + index, 0xff);
        battle.memory.put32(battle.record_base() + damage + index * 4, 0);
    }
}

// The command flags 8009ac48 and 8009ab38 test: a gear record's +124 bit
// 2000, otherwise the record's +88 bit 400.
bool command_flag(const Battle &battle, std::uint32_t slot) {
    const auto record = battle.record(slot);
    return in_gear(battle, slot) ? (battle.memory.u16(record + 0x124) & 0x2000) != 0
                                 : (battle.memory.u16(record + 0x88) & 0x400) != 0;
}
// Mark a slot's command descriptors with `value` in their halfword +0x18
// (the listed descriptor offsets of its gear or on-foot table).
void mark_commands(Battle &battle, std::uint32_t slot, std::uint32_t value) {
    if (in_gear(battle, slot)) {
        const auto table = battle.record_base() + slot * 0x690 + gear_commands;
        for (const auto offset : {0x348U, 0x398U, 0x3c0U, 0x3e8U, 0x410U, 0x438U, 0x460U})
            battle.memory.put16(table + offset, value);
    } else {
        const auto table = battle.record_base() + slot * 0x5f0 + party_commands;
        for (const auto offset :
             {0x370U, 0x3c0U, 0x3e8U, 0x410U, 0x438U, 0x460U, 0x488U, 0x4b0U, 0x4d8U, 0x500U})
            battle.memory.put16(table + offset, value);
    }
}
// 8009ac48(slot, check): unless `check` is set and the descriptor lacks flag
// 100, a flagged slot's commands take 2000 and the flag clears.
void release_commands(Battle &battle, std::uint32_t slot, bool check) {
    if (check && (battle.memory.u16(battle.memory.u32(descriptor_pointer) + 0xa) & 0x100) == 0)
        return;
    if (!command_flag(battle, slot))
        return;
    mark_commands(battle, slot, 0x2000);
    const auto record = battle.record(slot);
    if (in_gear(battle, slot))
        battle.memory.put16(record + 0x124, battle.memory.u16(record + 0x124) & 0xdfff);
    else
        battle.memory.put16(record + 0x88, battle.memory.u16(record + 0x88) & 0xfbff);
}
// 8009ab38(slot): a flagged slot's commands take 1.
void hold_commands(Battle &battle, std::uint32_t slot) {
    if (command_flag(battle, slot))
        mark_commands(battle, slot, 1);
}

// 80094c78: add the damage entry at the (post-loop) target index to each
// untouched party slot's total. After the resolver loop the index is 11.
void add_party_totals(Battle &battle) {
    const auto base = battle.record_base();
    const auto amount = battle.memory.u32(base + damage + battle.memory.u8(target_slot) * 4);
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (battle.memory.u8(base + result_code + slot) != 0)
            continue;
        const auto total = base + slot * 4 + (in_gear(battle, slot) ? gear_totals : party_totals);
        battle.memory.put32(total, battle.memory.u32(total) + amount);
    }
}

// 80099fb0: publish descriptor bytes +20..+23 and the command; an element
// byte without bits 3f takes the attacker's status bits (+8c | +8e) >> 12.
void publish_descriptor(Battle &battle) {
    const auto base = battle.record_base();
    const auto descriptor = battle.memory.u32(descriptor_pointer);
    const auto attacker = battle.memory.u32(attacker_pointer);
    const auto status =
        (battle.memory.u16(attacker + 0x8c) | battle.memory.u16(attacker + 0x8e)) >> 12;
    for (std::uint32_t i = 0; i < 4; ++i)
        battle.memory.put8(base + 0x5fbc + i, battle.memory.u8(descriptor + 0x20 + i));
    battle.memory.put8(base + 0x5fc0, battle.memory.u8(base + command_index));
    if (const auto element = battle.memory.u8(base + 0x5fbe); (element & 0x3f) == 0)
        battle.memory.put8(base + 0x5fbe, status | element);
}

// 800941a4: resolve the committed action against every target in the mask.
void resolve(Battle &battle) {
    clear_results(battle);
    auto &memory = battle.memory;
    memory.put8(resolve_status, 0);
    memory.put8(item_used, 0);
    const auto base = battle.record_base();
    const auto slot = memory.u8(base + attacker_index);
    memory.put8(attacker_slot, slot);
    const auto attacker = battle.record(slot);
    memory.put32(attacker_pointer, attacker);
    memory.put32(attacker_block_pointer, attacker + 0xa4);
    if (in_gear(battle, slot))
        throw BattleError("The gear action resolver 8009c198 is not reconstructed");
    const auto command = memory.u8(base + command_index);
    const auto descriptor = slot < 3 ? base + slot * 0x5f0 + party_commands + command * 0x28
                                     : base + enemy_commands + command * 0x28;
    memory.put32(descriptor_pointer, descriptor);
    release_commands(battle, slot, true);
    if ((memory.u16(descriptor + 0xa) & 0x10) != 0)
        throw BattleError("The gear action resolver 8009c198 is not reconstructed");
    memory.put8(ether_failed, 0);
    if ((memory.u16(descriptor + 0xa) & 0x100) != 0 && memory.u8(attacker + 0x56) == 1)
        ether_check(battle);
    if (memory.u8(attacker + 0x56) == 4 && command - 4 < 2)
        memory.put8(descriptor + 0x22, memory.u8(attacker + (command - 3) * 8 + 4));
    // An attacker with +80 bit 20 aimed at party slots strikes the party
    // member whose record +56 (at the fixed record addresses) is 3.
    if ((memory.u16(attacker + 0x80) & 0x20) != 0) {
        const auto aimed = memory.u16(0x800d2c94);
        if ((aimed & 7) != 0 && aimed < 7)
            for (std::uint32_t member = 0; member < 3; ++member)
                if (memory.u8(0x800ccd3e + member * record_stride) == 3)
                    memory.put16(0x800d2c94, 1U << member);
    }
    memory.put8(target_slot, 0);
    for (std::uint32_t bit = 1;; bit <<= 1) {
        const auto record_base = battle.record_base();
        if ((bit & memory.u16(record_base + target_mask)) != 0) {
            const auto target = memory.u8(target_slot);
            const auto record = record_base + target * record_stride;
            memory.put32(target_pointer, record);
            memory.put32(target_block_pointer, record + 0xa4);
            memory.put32(target_tail_pointer, record + 0x148);
            if (target < 3)
                counter_check(battle);
            const auto type = memory.u8(memory.u32(descriptor_pointer) + 0x16);
            const auto formula = memory.u32(formula_table + type * 4);
            if (formula == 0x80094ee4)
                physical_formula(battle);
            else if (formula == 0x80095d4c)
                formula_type3(battle);
            else
                throw BattleError("Only formulas 80094ee4 and 80095d4c of table 800c348c are "
                                  "reconstructed");
            post_adjust(battle);
            const auto current = battle.record_base();
            if (memory.u8(current + result_code + memory.u8(target_slot)) == 0) {
                const auto flags = memory.u16(memory.u32(descriptor_pointer) + 0xa);
                if ((flags & 0x800) != 0)
                    status_effect_95b44(battle);
                else if ((flags & 0x4000) != 0)
                    status_effect_95a78(battle);
                else if ((flags & 4) != 0)
                    status_effect_958d8(battle);
            }
            const auto shown = memory.u32(descriptor_pointer);
            const auto now = battle.record_base();
            memory.put16(now + shown_command, (memory.u16(shown + 0xa) & 1) != 0
                                                  ? memory.u16(shown + 2)
                                                  : memory.u8(now + command_index));
        }
        const auto next = memory.u8(target_slot) + 1;
        memory.put8(target_slot, next);
        if ((next & 0xff) >= combatant_slots)
            break;
    }
    publish_descriptor(battle);
    hold_commands(battle, memory.u8(attacker_slot));
    const auto actor = memory.u32(attacker_pointer);
    if (memory.u8(actor + 0x56) == 4 && memory.u8(item_used) == 0)
        throw BattleError("Character 4's item bookkeeping 8009afd8 is not reconstructed");
    if (memory.u8(attacker_slot) < 3) {
        const auto used = memory.u8(battle.record_base() + command_index);
        if (used < 7) {
            const auto counter = actor + used * 2 + 0x90;
            const auto count = memory.u16(counter);
            if (count <= 0xfde7)
                memory.put16(counter, count + memory.u8(actor + 0x55) + memory.u8(actor + 0xa1));
        }
    }
    add_party_totals(battle);
}

std::uint8_t *byte_at(std::map<std::uint32_t, std::vector<std::uint8_t>> &regions,
                      std::uint32_t address, std::uint32_t size) {
    auto found = regions.upper_bound(address);
    if (found != regions.begin()) {
        --found;
        if (address - found->first + std::uint64_t{size} <= found->second.size())
            return found->second.data() + (address - found->first);
    }
    throw BattleError("Battle code reaches memory outside its owned regions");
}

} // namespace

std::uint32_t BattleMemory::u8(std::uint32_t address) const {
    return *byte_at(const_cast<BattleMemory *>(this)->regions, address, 1);
}
std::uint32_t BattleMemory::u16(std::uint32_t address) const {
    const auto *bytes = byte_at(const_cast<BattleMemory *>(this)->regions, address, 2);
    return static_cast<std::uint32_t>(bytes[0] | bytes[1] << 8);
}
std::uint32_t BattleMemory::u32(std::uint32_t address) const {
    const auto *bytes = byte_at(const_cast<BattleMemory *>(this)->regions, address, 4);
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 |
           static_cast<std::uint32_t>(bytes[2]) << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
}
std::int32_t BattleMemory::s8(std::uint32_t address) const {
    return static_cast<std::int8_t>(u8(address));
}
std::int32_t BattleMemory::s16(std::uint32_t address) const {
    return static_cast<std::int16_t>(u16(address));
}
void BattleMemory::put8(std::uint32_t address, std::uint32_t value) {
    *byte_at(regions, address, 1) = static_cast<std::uint8_t>(value);
}
void BattleMemory::put16(std::uint32_t address, std::uint32_t value) {
    auto *bytes = byte_at(regions, address, 2);
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
}
void BattleMemory::put32(std::uint32_t address, std::uint32_t value) {
    auto *bytes = byte_at(regions, address, 4);
    for (std::uint32_t i = 0; i < 4; ++i)
        bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
}

std::uint32_t Battle::rand() {
    const auto step = field::advance_field_random(seed);
    seed = step.seed;
    return step.value;
}

void commit_action(Battle &battle, std::uint32_t attacker, std::uint32_t targets,
                   std::uint32_t animation) {
    auto &memory = battle.memory;
    const auto turn = memory.u32(turn_state_pointer);
    const auto alive = memory.u16(0x800d39dc);
    memory.put16(0x800c48e8, 0);
    memory.put8(0x800d2ca9, attacker);
    memory.put16(0x800d2c94, targets);
    memory.put16(0x800d2c98, animation);
    memory.put16(0x800d2c96, alive);
    memory.put8(0x800d2caa, memory.u8(turn + 0x2dc) + 0xff);
    resolve(battle);
}

} // namespace xem::reconstruction::battle
