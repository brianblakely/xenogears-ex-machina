#include "xem/reconstruction/battle.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace xem::reconstruction::battle {
namespace {

// The turn procedure addresses the records absolutely from 800ccce8.
constexpr std::uint32_t fixed_record_base = 0x800ccce8;
constexpr std::uint32_t present = 0x800d2dcc;      // u8 per slot
constexpr std::uint32_t ready = 0x800d2de4;        // u8 per slot; 1 = ready
constexpr std::uint32_t turn_order = 0x800d2dd8;   // u8 slot per position
constexpr std::uint32_t order_cursor = 0x800d2dd7; // u8 position
constexpr std::uint32_t forced_turn = 0x800d2dc0;  // u8 slot + 1
constexpr std::uint32_t all_enemies = 0x800d39e0;  // u16 mask for 80072324
constexpr std::uint32_t timer = 0x800d2e06;        // u16 per slot
constexpr std::uint32_t timer_reload = 0x800d2df0; // u16 per slot
constexpr std::uint32_t atb_enabled = 0x800d3298;  // u8
constexpr std::uint32_t slot_info = 0x800c3eb4;    // 0x1c bytes per slot; +0 group, +3 flag
constexpr std::uint32_t slot_info_stride = 0x1c;
constexpr std::uint32_t slot_bits = 0x800c3448;       // u16 per slot
constexpr std::uint32_t slot_flags = 0x800d32a1;      // u8, 8 bytes per slot
constexpr std::uint32_t action_list = 0x800d2e5c;     // 8-byte entries
constexpr std::uint32_t targets = 0x800d2c94;         // u16 committed target mask
constexpr std::uint32_t outcome = 0x800c48ea;         // u8
constexpr std::uint32_t route = 0x800c48ec;           // 9 points of 6 bytes
constexpr std::uint32_t candidates = 0x800c3e90;      // u8 x 12: default-target order
constexpr std::uint32_t candidate_count = 0x800d3274; // u8
constexpr std::uint32_t formation_pointer = 0x800d3364;
constexpr std::uint32_t ui_state_pointer = 0x800d2d28;
constexpr std::uint32_t graphics_state_pointer = 0x800c3ea4;
// Turn-state (800c3eac block) offsets.
constexpr std::uint32_t turn_slot = 0x2d3;   // acting slot (+1 while selecting)
constexpr std::uint32_t event_count = 0x2da; // queued presentation events
constexpr std::uint32_t events_done = 0x2db;
constexpr std::uint32_t default_target = 0x3c; // u8 per slot, 0x40 apart

// Result accumulation (80085350/80085454): running code and amount per slot.
constexpr std::uint32_t running_code = 0x800d2d5c;   // u8 x 11
constexpr std::uint32_t running_amount = 0x800d2d70; // u16 x 11
constexpr std::uint32_t damage = 0x800d2c54;         // u32 x 11 (record base + 5f6c)
constexpr std::uint32_t result_code = 0x800d2c88;    // u8 x 11 (record base + 5fa0)

// Presentation event queue: 0x48-byte slots from 800c3fe8.
constexpr std::uint32_t events = 0x800c3fe8;
constexpr std::uint32_t event_stride = 0x48;
// +0 u16 x 11 amounts, +0x16 u16 target mask, +0x18 u8 x 11 codes, +0x23
// actor, +0x24 u16 x 11 accumulated amounts, +0x3a u16 parameter, +0x3c u8 x
// 11 accumulated codes, +0x47 type.

std::uint32_t fixed_record(std::uint32_t slot) { return fixed_record_base + slot * record_stride; }
std::uint32_t info(std::uint32_t slot) { return slot_info + (slot & 0xff) * slot_info_stride; }
std::uint32_t turn_state(const Battle &battle) { return battle.memory.u32(turn_state_pointer); }
std::uint32_t event(const Battle &battle) {
    return events + battle.memory.u8(turn_state(battle) + event_count) * event_stride;
}

// 80089c9c: the slot's bit in `mask`, 0 for slots >= 16.
std::uint32_t slot_in_mask(const Battle &battle, std::uint32_t mask, std::uint32_t slot) {
    const std::uint32_t s = slot & 0xff;
    return s >= 0x10 ? 0 : battle.memory.u16(slot_bits + s * 2) & mask;
}

// 80085350: reset the running result accumulation.
void clear_running(Battle &battle) {
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        battle.memory.put8(running_code + slot, 0xff);
        battle.memory.put16(running_amount + slot * 2, 0);
    }
}

// 80085388: clear the current event's per-slot results (the slot index is
// re-read for every store).
void clear_event(Battle &battle) {
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        battle.memory.put16(event(battle) + slot * 2, 0);
        battle.memory.put8(event(battle) + 0x18 + slot, 0xff);
        battle.memory.put16(event(battle) + 0x24 + slot * 2, 0);
        battle.memory.put8(event(battle) + 0x3c + slot, 0xff);
    }
}

// 800785d4: complete the current event with its actor and parameter and
// advance the count.
void push_event(Battle &battle, std::uint32_t actor, std::uint32_t parameter) {
    auto &memory = battle.memory;
    memory.put8(event(battle) + 0x23, actor);
    memory.put16(event(battle) + 0x3a, parameter);
    const auto state = turn_state(battle);
    memory.put8(state + event_count, memory.u8(state + event_count) + 1);
}

// 80085454: copy the resolver's damage and codes into event `queue` and
// accumulate them into the running amounts (code 2 is healing).
void accumulate_results(Battle &battle, std::uint32_t queue) {
    auto &memory = battle.memory;
    const std::uint32_t slot_event = events + (queue & 0xff) * event_stride;
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        const auto amount_address = damage + slot * 4;
        const auto code_address = result_code + slot;
        const auto kind_address = running_code + slot;
        const auto total_address = running_amount + slot * 2;
        memory.put16(slot_event + slot * 2, memory.u16(amount_address));
        memory.put8(slot_event + 0x18 + slot, memory.u8(code_address));
        const auto code = memory.u8(code_address);
        const auto kind = memory.u8(kind_address);
        const auto amount = memory.s16(amount_address);
        const auto total = memory.s16(total_address);
        const auto replace = [&] {
            memory.put16(total_address, memory.u16(amount_address));
            memory.put8(kind_address, memory.u8(code_address));
        };
        if (code == 2) {
            if (kind == 2) {
                memory.put16(total_address, memory.u16(total_address) + memory.u16(amount_address));
            } else if (kind == 0 || kind == 5) {
                if (amount - total >= 0) {
                    memory.put16(total_address, static_cast<std::uint32_t>(amount - total));
                    memory.put8(kind_address, 2);
                } else {
                    memory.put16(total_address, static_cast<std::uint32_t>(total - amount));
                }
            } else {
                replace();
            }
        } else if (code == 0 || code == 5) {
            if (kind == 0 || kind == 5) {
                memory.put16(total_address, memory.u16(total_address) + memory.u16(amount_address));
            } else if (kind == 2) {
                if (amount - total < 0) {
                    memory.put16(total_address, static_cast<std::uint32_t>(total - amount));
                } else {
                    memory.put16(total_address, static_cast<std::uint32_t>(amount - total));
                    memory.put8(kind_address, 0);
                }
            } else {
                replace();
            }
        }
        memory.put16(slot_event + 0x24 + slot * 2, memory.u16(total_address));
        memory.put8(slot_event + 0x3c + slot, memory.u8(kind_address));
    }
}

// 80078998 (action list type 1): commit the entry's action and queue its
// result event.
void queue_attack(Battle &battle, std::uint32_t actor, std::uint32_t index) {
    auto &memory = battle.memory;
    const std::uint32_t entry = action_list + index * 8;
    if (memory.u8(entry + 3) != 0) {
        throw BattleError("Named enemy actions (8007893c: 80078658 text, 800787e0, 8007887c) are "
                          "not reconstructed");
    }
    const auto state = turn_state(battle);
    memory.put8(state + 0x2dc, memory.u8(entry + 1) + 1);
    commit_action(battle, actor, memory.u16(entry + 6), memory.u8(entry + 2));
    // 80085c88: accumulate and apply the results of the current event.
    const auto queue = memory.u8(turn_state(battle) + event_count);
    accumulate_results(battle, queue);
    apply_results(battle, queue);
    memory.put8(memory.u32(ui_state_pointer) + 0xad, 0);
    memory.put8(event(battle) + 0x47, memory.u8(entry + 2));
    memory.put16(event(battle) + 0x16, memory.u16(targets));
    push_event(battle, actor, memory.u16(entry + 4));
    // A retargeted action updates the first pending type-fd event's mask.
    if (memory.u16(entry + 6) == memory.u16(targets))
        return;
    const auto count = memory.u8(turn_state(battle) + event_count);
    for (std::uint32_t queued = 0; queued < count; ++queued) {
        const auto slot_event = events + queued * event_stride;
        if (memory.u8(slot_event + 0x47) == 0xfd) {
            memory.put16(slot_event + 0x16, memory.u16(targets));
            return;
        }
    }
}

std::uint32_t group(const Battle &battle, std::uint32_t slot) {
    return battle.memory.u8(info(slot));
}

// 800877e0: the approach route from `actor` to `target` (9 six-byte points
// at 800c48ec: x, y, flag): the actor's position, then up to 7 formation
// points listed for the two groups at formation +140.
void plan_route(Battle &battle, std::uint32_t actor, std::uint32_t target) {
    auto &memory = battle.memory;
    for (std::uint32_t point = 0; point < 9; ++point) {
        memory.put16(route + point * 6, 0xffff);
        memory.put16(route + point * 6 + 2, 0xffff);
    }
    memory.put16(route, memory.u16(info(actor) + 0xa));
    memory.put8(route + 4, 0);
    memory.put16(route + 2, memory.u16(info(actor) + 0xc));
    for (std::uint32_t step = 1; step < 8; ++step) {
        const auto formation = memory.u32(formation_pointer);
        const auto list =
            formation + group(battle, actor) * 0x40 + group(battle, target) * 8 + 0x140;
        const auto point = memory.u8(list + step);
        if (point == 0xff)
            break;
        memory.put16(route + step * 6, memory.u16(formation + (point & 7) * 0x20));
        memory.put16(route + step * 6 + 2, memory.u16(formation + (point & 7) * 0x20 + 2));
        memory.put8(route + step * 6 + 4, point & 0x80);
    }
}

// 80087edc: move `actor` into `target`'s formation group when it differs
// and has fewer than 4 members, taking the first free member position.
void join_group(Battle &battle, std::uint32_t actor, std::uint32_t target) {
    auto &memory = battle.memory;
    if (group(battle, actor) == group(battle, target))
        return;
    const std::uint32_t bank = target < 3 && actor >= 3 ? 8 : 0;
    const auto entry = [&] { return 0x800d301c + (group(battle, target) + bank) * 4; };
    if (memory.u8(entry()) >= 4)
        return;
    leave_group(battle, actor);
    memory.put8(entry(), memory.u8(entry()) + 1);
    std::uint32_t member = 0;
    while (member < 4 && (slot_in_mask(battle, memory.u8(entry() + 1), member) & 0xffff) != 0)
        ++member;
    memory.put8(info(actor) + 1, member);
    memory.put8(info(actor), group(battle, target));
    const auto own = 0x800d301d + (group(battle, actor) + bank) * 4;
    memory.put8(own, memory.u8(own) | memory.u16(slot_bits + memory.u8(info(actor) + 1) * 2));
    const auto position = memory.u32(formation_pointer) + memory.u8(info(actor) + 1) * 4 +
                          group(battle, actor) * 0x20 + (actor < 3 ? 4U : 0x10U);
    memory.put16(info(actor) + 0xa, memory.u16(position));
    memory.put16(info(actor) + 0xc, memory.u16(position + 2));
}

// 80078b34 (action list type 2): queue the approach event (type fd) to the
// entry's targets, plan the route and change formation group unless the
// entry's +4 is 1, then frame the camera (presentation) and complete it.
void queue_approach(Battle &battle, std::uint32_t actor, std::uint32_t index, std::uint32_t first,
                    const FrameTargets &frame) {
    auto &memory = battle.memory;
    const std::uint32_t entry = action_list + index * 8;
    memory.put8(event(battle) + 0x47, 0xfd);
    memory.put16(event(battle) + 0x3a, 0);
    memory.put16(event(battle) + 0x16, memory.u16(entry + 6));
    plan_route(battle, actor, first);
    if (memory.u8(entry + 4) != 1) {
        if (memory.u8(slot_flags + actor * 8) != 0)
            throw BattleError("The flagged-slot group change 800881b8 is not reconstructed");
        join_group(battle, actor, first);
    }
    frame(memory.u16(entry + 6) | memory.u16(slot_bits + actor * 2)); // 800bc404
    push_event(battle, actor, memory.u16(entry + 4));
}

// Action list types 0x01..0x10 (table 8006fb38) from `index`; `more` is the
// executor's flag, cleared by a type-0 entry: it stops after entry 31 once
// clear.
void run_actions(Battle &battle, std::uint32_t actor, std::uint32_t index, bool more,
                 const FrameTargets &frame) {
    auto &memory = battle.memory;
    for (; index < 32 || more; ++index) {
        const std::uint32_t entry = action_list + index * 8;
        std::uint32_t first = 0; // first slot of the entry's target mask
        for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
            if ((slot_in_mask(battle, memory.u16(entry + 6), slot) & 0xffff) != 0) {
                first = slot;
                break;
            }
        }
        clear_event(battle);
        switch (const auto type = memory.u8(entry); type) {
        case 0:
            more = false;
            break;
        case 1:
            queue_attack(battle, actor, index);
            break;
        case 2:
            queue_approach(battle, actor, index, first, frame);
            break;
        case 0xe: // 800787e0: event f7 with the entry's +4 byte
            memory.put8(event(battle) + 0x23, actor);
            memory.put8(event(battle) + 0x47, 0xf7);
            memory.put16(event(battle) + 0x3a, memory.u8(entry + 4));
            memory.put8(turn_state(battle) + event_count,
                        memory.u8(turn_state(battle) + event_count) + 1);
            break;
        default:
            throw BattleError(std::string("Action list type ") + std::to_string(type) +
                              " (table 8006fb38) is not reconstructed");
        }
    }
    // 80079674: close the queue.
    if (memory.u8(slot_flags + actor * 8) != 0) {
        memory.put8(event(battle) + 0x23, actor);
        memory.put8(event(battle) + 0x47, 0x1b);
        const auto state = turn_state(battle);
        memory.put8(state + event_count, memory.u8(state + event_count) + 1);
    }
    memory.put8(event(battle) + 0x23, actor);
    memory.put8(event(battle) + 0x47, 0xfe);
    memory.put8(0x800d366c, 0);
}

// 80083ff4: whether `actor` may take `slot` as its default target.
bool may_target(const Battle &battle, std::uint32_t actor, std::uint32_t slot) {
    const auto &memory = battle.memory;
    if (memory.u8(present + slot) == 0 || memory.u8(info(slot) + 3) != 0)
        return false;
    if (memory.u8(slot_flags + slot * 8) != 0)
        return (memory.u16(fixed_record(slot) + 0x120) & 0xc001) == 0;
    const auto relation = memory.u32(formation_pointer) + memory.u8(info(slot)) * 8 +
                          memory.u8(info(actor)) * 0x40 + 0x140;
    if (memory.u8(relation) != 0)
        return false;
    return (memory.u16(fixed_record(slot) + 0x7c) & 0xc001) == 0;
}

// 800841e0: order the slots `actor` may target (same formation group first)
// into 800c3e90 and return the first: the lowest-HP one, among the same group
// when it has members.
std::uint32_t choose_default_target(Battle &battle, std::uint32_t actor) {
    auto &memory = battle.memory;
    std::uint8_t found[12];
    std::uint8_t grouped[12];
    memory.put8(candidate_count, 0);
    for (std::uint32_t i = 0; i < 12; ++i) {
        found[i] = 0xff;
        memory.put8(candidates + i, 0xff);
        grouped[i] = 0;
    }
    std::uint32_t count = 0;
    const auto first = actor < 3 ? 3U : 0U;
    const auto last = actor < 3 ? combatant_slots : 3U;
    for (auto slot = first; slot < last; ++slot) {
        if (may_target(battle, actor, slot)) {
            found[count++] = static_cast<std::uint8_t>(slot);
            memory.put8(candidate_count, memory.u8(candidate_count) + 1);
        }
    }
    std::uint32_t out = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (memory.u8(info(actor)) == memory.u8(info(found[i]))) {
            memory.put8(candidates + out, found[i]);
            found[i] = 0xff;
            grouped[out] = 1;
            ++out;
        }
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        if (found[i] != 0xff)
            memory.put8(candidates + out++, found[i]);
    }
    const auto hp = [&](std::uint32_t position) {
        return memory.u16(fixed_record(memory.u8(candidates + position)) + 0x4c);
    };
    for (std::uint32_t k = 1; k < count; ++k) {
        if (grouped[0] != 0 && grouped[k] == 0)
            continue;
        if (hp(k) < hp(0)) {
            const auto head = memory.u8(candidates);
            memory.put8(candidates, memory.u8(candidates + k));
            memory.put8(candidates + k, head);
        }
    }
    return memory.u8(candidates);
}

// 8009ada0: whether the slot regenerates at the end of its turn.
bool regenerates(const Battle &battle, std::uint32_t slot) {
    const auto &memory = battle.memory;
    const auto record = battle.record(slot & 0xff);
    if ((memory.u16(record + 0x7c) & 0x8000) != 0)
        return false;
    return (memory.u16(record + 0x7c) & 0x800) != 0 || (memory.u16(record + 0x80) & 0x200) != 0 ||
           (memory.u16(record + 0x120) & 0x280) != 0 || (memory.u16(record + 0x124) & 0x8000) != 0;
}

} // namespace

bool select_turn(Battle &battle) {
    auto &memory = battle.memory;
    if (memory.u16(all_enemies) != 0)
        throw BattleError("The all-enemies turn pass 80072324 is not reconstructed");
    const auto state = turn_state(battle);
    if (const auto forced = memory.u8(forced_turn); forced != 0) {
        memory.put8(state + turn_slot, forced);
        memory.put8(ready + forced - 1, 1);
        memory.put8(forced_turn, 0);
        memory.put16(timer + (forced - 1) * 2, 0);
    } else {
        memory.put8(state + turn_slot, 0);
        const auto start = memory.u8(order_cursor);
        auto position = start;
        do {
            const auto slot = memory.u8(turn_order + position);
            if (memory.u8(ready + slot) == 1) {
                memory.put8(state + turn_slot, slot + 1);
                memory.put8(order_cursor, (position + 1) % combatant_slots);
                break;
            }
            position = (position + 1) % combatant_slots;
        } while (position != start);
    }
    // 80071b94 entry.
    memory.put8(0x800c3e8c, 1);
    memory.put8(0x800d2caf, 0);
    return memory.u8(turn_state(battle) + turn_slot) != 0;
}

void begin_turn(Battle &battle) {
    auto &memory = battle.memory;
    clear_running(battle);
    const auto state = turn_state(battle);
    for (std::uint32_t member = 0; member < 3; ++member) // 80071a08
        memory.put8(state + 0x2eb + member, 0);
    memory.put8(atb_enabled, 0);
    memory.put8(state + turn_slot, memory.u8(state + turn_slot) - 1);
    memory.put8(0x800c4922, memory.u8(state + turn_slot));
    memory.put8(state + event_count, 0);
    memory.put8(state + events_done, 0);
    for (std::uint32_t i = 0; i < 5; ++i)
        memory.put8(0x800d2ca4 + i, 0);
    memory.put16(targets, 0);
    count_down_statuses(battle, memory.u8(state + turn_slot));
}

std::uint32_t prepare_turn(Battle &battle) {
    auto &memory = battle.memory;
    const auto slot = memory.u8(turn_state(battle) + turn_slot);
    if (slot >= 3)
        return 0; // enemies branch to their script
    for (std::uint32_t enemy = 0; enemy < 8; ++enemy)
        memory.put8(0x800c3d1a + enemy * 4, 0);
    // The acting member's marker: four vertices in the draw buffer 800ccb34.
    const auto graphics = memory.u32(graphics_state_pointer);
    const auto buffer = memory.u32(0x800ccb34);
    const auto x = memory.u16(0x800c3254 + (memory.u8(0x800d3280) * 3 + slot) * 2) + slot * 0x60;
    const auto marker = graphics + buffer * 0x18 + 0x63d0;
    for (const auto [offset, value] : {std::pair{0U, x + 0x10},
                                       {2U, 8U},
                                       {4U, x + 0x28},
                                       {6U, 8U},
                                       {8U, x + 0x10},
                                       {10U, 0x20U},
                                       {12U, x + 0x28},
                                       {14U, 0x20U}})
        memory.put16(marker + offset, value);
    memory.put8(graphics + 0x6414, buffer);
    memory.put8(graphics + 0x6415, 1);
    const auto record = fixed_record(memory.u8(turn_state(battle) + turn_slot));
    if ((memory.u16(record + 0x7c) & 0x2080) != 0 || (memory.u16(record + 0x80) & 0x1000) != 0)
        return 3;
    return (memory.u16(record + 0x80) & 0x2000) != 0 ? 2 : 1;
}

void count_down_statuses(Battle &battle, std::uint32_t slot) {
    auto &memory = battle.memory;
    const auto record = fixed_record(slot & 0xff);
    if ((memory.u8(record + 0x15a) & 0x80) != 0)
        throw BattleError("The gear status countdown 80099cf0 is not reconstructed");
    const auto counter = record + 0x15c;
    const auto tick = [&](std::uint32_t index) {
        const auto value = (memory.u8(counter + index) + 0xff) & 0xff;
        memory.put8(counter + index, value);
        return value == 0;
    };
    const auto clear = [&](std::uint32_t offset, std::uint32_t bit) {
        memory.put16(record + offset, memory.u16(record + offset) & ~bit & 0xffff);
    };
    if ((memory.u16(record + 0x7c) & 0x1000) != 0 && tick(1))
        clear(0x7c, 0x1000);
    if ((memory.u16(record + 0x80) & 0x1000) != 0 && tick(2))
        clear(0x80, 0x1000);
    if ((memory.u16(record + 0x80) & 0x800) != 0 && tick(3)) {
        clear(0x80, 0x800);
        clear(0x7a, 0x20);
    }
    // Timed statuses: the bit set in the low halfword and clear in the high.
    struct Timed {
        std::uint32_t offset, bit, counter;
    };
    for (const auto [offset, bit, index] :
         {Timed{0x84, 0x8000, 4}, Timed{0x84, 0x4000, 5}, Timed{0x84, 0x2000, 6},
          Timed{0x84, 0x1000, 7}, Timed{0x84, 0x800, 8}, Timed{0x88, 0x8000, 9},
          Timed{0x88, 0x4000, 10}, Timed{0x88, 0x1000, 11}}) {
        if ((memory.u32(record + offset) & (bit << 16 | bit)) == bit && tick(index))
            clear(offset, bit);
    }
    for (const auto [mask, index] : {std::pair{0xf000U, 12U}, std::pair{0xf00U, 13U}}) {
        if ((memory.u16(record + 0x8c) & mask) != 0 && (memory.u16(record + 0x8e) & mask) == 0 &&
            tick(index))
            clear(0x8c, mask);
    }
}

void begin_actions(Battle &battle, std::uint32_t actor) {
    auto &memory = battle.memory;
    memory.put8(turn_state(battle) + event_count, 0);
    clear_running(battle);
    clear_event(battle);
    memory.put8(event(battle) + 0x23, actor);
}

void execute_actions(Battle &battle, std::uint32_t actor, const FrameTargets &frame) {
    run_actions(battle, actor & 0xff, 0, true, frame);
}

void resume_actions(Battle &battle, std::uint32_t actor, std::uint32_t index, bool more,
                    const FrameTargets &frame) {
    // 80078c6c: the rest of a type-2 entry after its camera call.
    push_event(battle, actor & 0xff, battle.memory.u16(action_list + (index & 0xff) * 8 + 4));
    run_actions(battle, actor & 0xff, (index & 0xff) + 1, more, frame);
}

void order_targets(Battle &battle) {
    auto &memory = battle.memory;
    // 80072270: party slots flagged in 800d2c9e lose their ready state.
    if ((memory.u16(0x800d2c9e) & 7) != 0) {
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            if ((slot_in_mask(battle, memory.u16(0x800d2c9e), slot) & 0xffff) == 0)
                continue;
            memory.put8(ready + slot, 0);
            memory.put16(timer + slot * 2, memory.u16(timer_reload + slot * 2));
            memory.put8(memory.u32(ui_state_pointer) + slot + 0x7c, 1);
        }
    }
    memory.put16(0x800d2c9e, 0);
    memory.put8(memory.u32(graphics_state_pointer) + 0x6415, 0);
    for (std::uint32_t slot = 0; slot < combatant_slots; ++slot) {
        const auto target = choose_default_target(battle, slot);
        memory.put8(turn_state(battle) + slot * 0x40 + default_target, target);
        // 80085310: whether the target's +0a is below the slot's.
        const auto chosen = memory.u8(turn_state(battle) + slot * 0x40 + default_target);
        memory.put8(info(slot) + 6, memory.u16(info(chosen) + 0xa) < memory.u16(info(slot) + 0xa));
    }
}

void settle_turn(Battle &battle) {
    auto &memory = battle.memory;
    const auto state = turn_state(battle);
    for (std::uint32_t member = 0; member < 3; ++member) // 80071a08
        memory.put8(state + 0x2eb + member, 0);
    update_alive(battle);
    if (memory.u8(outcome) == 0 && regenerates(battle, memory.u8(turn_state(battle) + turn_slot)))
        throw BattleError("End-of-turn regeneration 80085b58 (presentation 800be538) is not "
                          "reconstructed");
    update_alive(battle);
}

void finish_turn(Battle &battle) {
    reload_turn_timer(battle);
    battle.memory.put8(atb_enabled, 1);
}

void approach_route(Battle &battle, std::uint32_t actor, std::uint32_t target) {
    plan_route(battle, actor & 0xff, target & 0xff);
}

void join_target_group(Battle &battle, std::uint32_t actor, std::uint32_t target) {
    join_group(battle, actor & 0xff, target & 0xff);
}

void clear_current_event(Battle &battle) { clear_event(battle); }

void apply_event(Battle &battle, std::uint32_t queue) {
    accumulate_results(battle, queue & 0xff);
    apply_results(battle, queue & 0xff);
    battle.memory.put8(battle.memory.u32(ui_state_pointer) + 0xad, 0);
}

std::uint32_t order_candidates(Battle &battle, std::uint32_t actor) {
    return choose_default_target(battle, actor & 0xff);
}

} // namespace xem::reconstruction::battle
