#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

#include <cstdint>
#include <format>

namespace xem::reconstruction::battle {
namespace {

constexpr std::uint32_t fixed_record_base = 0x800ccce8;
constexpr std::uint32_t command_code = 0x800d3014; // u8: decoded menu input
constexpr std::uint32_t face_code = 0x800c3e29;    // u8: latest face-button code
constexpr std::uint32_t previous_face = 0x800c3e28;
constexpr std::uint32_t paused = 0x800c3444;          // u8
constexpr std::uint32_t outcome = 0x800c48ea;         // u8
constexpr std::uint32_t effects_enabled = 0x800d366c; // u8: gates 8008aa74
// Turn-state (800c3eac block) offsets.
constexpr std::uint32_t page = 0x2dd; // command page
constexpr std::uint32_t events_done = 0x2db;
constexpr std::uint32_t menu_done = 0x2de;
constexpr std::uint32_t repeat_armed = 0x2f6; // a second press of the same face button
constexpr std::uint32_t turn_slot = 0x2d3;

std::uint32_t turn_state(const Battle &battle) { return battle.memory.u32(turn_state_pointer); }
// Per-member page availability halfwords (+1c + 2 * item, 0x40 per member); 0
// means the item is available.
std::uint32_t item(const Battle &battle, std::uint32_t member, std::uint32_t offset) {
    return battle.memory.u16(turn_state(battle) + (member & 0xff) * 0x40 + offset);
}
void set_page(Battle &battle, std::uint32_t value) {
    battle.memory.put8(turn_state(battle) + page, value);
}
void set_repeat(Battle &battle, std::uint32_t value) {
    battle.memory.put8(turn_state(battle) + repeat_armed, value);
}

// 8008aa40: menu sound effect `id` of the effect bank *8005919c (80039db8).
void effect(ResidentState &resident, std::uint32_t id) {
    resident::start_bank_effect(resident.sound, resident.sound.system_bank, id);
}
// 8008aa74: the same while 800d366c enables menu effects.
void menu_effect(Battle &battle, ResidentState &resident, std::uint32_t id) {
    if (battle.memory.u8(effects_enabled) != 0)
        effect(resident, id);
}

bool debug_enabled(const ResidentState &resident) {
    if (resident.debug_pointer != 0x80010000)
        throw BattleError("The debug word pointer 8005917c does not name 80010000");
    return resident.debug_word != 0xffffffff;
}

// A face button (code 0-3) press on a page that moves to `target` on a
// repeated press (the 2f6/800c3e29 pair) unless `blocked`.
void repeat_or_move(Battle &battle, ResidentState &resident, std::uint32_t face, bool blocked,
                    std::uint32_t target) {
    if (battle.memory.u8(turn_state(battle) + repeat_armed) == 0 ||
        battle.memory.u8(face_code) != face) {
        set_repeat(battle, 1);
        menu_effect(battle, resident, 0x4f);
        return;
    }
    if (blocked)
        menu_effect(battle, resident, 0x4f);
    else
        set_page(battle, target);
    set_repeat(battle, 0);
}

// 8008115c: page 1.
void page_1(Battle &battle, ResidentState &resident, std::uint32_t member) {
    switch (battle.memory.u8(command_code)) {
    case 0:
        if (item(battle, member, 0x26) != 0)
            return menu_effect(battle, resident, 0x4f);
        return set_page(battle, 7);
    case 1:
        if (item(battle, member, 0x32) != 0)
            return menu_effect(battle, resident, 0x4f);
        return set_page(battle, 2);
    case 2:
        return set_page(battle, 3);
    case 3:
        if (item(battle, member, 0x30) == 0)
            return set_page(battle, 4);
        return repeat_or_move(battle, resident, 3, item(battle, member, 0x2a) != 0, 10);
    case 4:
    case 6:
    case 7:
        if (battle.memory.u8(turn_state(battle) + (member & 0xff) * 0x40 + 0x3c) == 0xff)
            return menu_effect(battle, resident, 0x4f);
        throw BattleError("The attack page entry (80087a38, 80084a7c, camera 80077698) is not "
                          "reconstructed");
    default:
        return;
    }
}

// 80081504: page 3.
void page_3(Battle &battle, ResidentState &resident, std::uint32_t member) {
    switch (battle.memory.u8(command_code)) {
    case 0:
        if (item(battle, member, 0x2e) == 0)
            return set_page(battle, 1);
        return repeat_or_move(battle, resident, 0, item(battle, member, 0x26) != 0, 7);
    case 1:
        if (item(battle, member, 0x32) != 0)
            return menu_effect(battle, resident, 0x4f);
        return set_page(battle, 2);
    case 2:
        if (item(battle, member, 0x2c) != 0)
            return menu_effect(battle, resident, 0x4f);
        return set_page(battle, 9);
    case 3:
        if (item(battle, member, 0x30) == 0)
            return set_page(battle, 4);
        return repeat_or_move(battle, resident, 3, item(battle, member, 0x2a) != 0, 10);
    case 4:
    case 6:
    case 7:
        defend(battle, member);
        battle.memory.put8(turn_state(battle) + 0x2ea, 0);
        battle.memory.put8(turn_state(battle) + menu_done, 1);
        return;
    default:
        return;
    }
}

// 80082504: page 9.
void page_9(Battle &battle, ResidentState &resident, std::uint32_t member) {
    switch (battle.memory.u8(command_code)) {
    case 0:
        if (item(battle, member, 0x26) == 0)
            return set_page(battle, 7);
        if (battle.memory.u8(turn_state(battle) + repeat_armed) == 0 ||
            battle.memory.u8(face_code) != 0) {
            set_repeat(battle, 1);
            return menu_effect(battle, resident, 0x4f);
        }
        if (item(battle, member, 0x2e) != 0)
            menu_effect(battle, resident, 0x4f);
        else
            set_page(battle, 1);
        return set_repeat(battle, 0);
    case 1:
        if (item(battle, member, 0x32) != 0)
            return menu_effect(battle, resident, 0x4f);
        return set_page(battle, 8);
    case 2:
        return set_page(battle, 3);
    case 3:
        if (item(battle, member, 0x2a) == 0)
            return set_page(battle, 10);
        return repeat_or_move(battle, resident, 3, item(battle, member, 0x30) != 0, 4);
    case 4:
    case 6:
    case 7:
        if (try_escape(battle, battle.memory.u8(turn_state(battle) + turn_slot)))
            battle.memory.put8(outcome, 0x40);
        battle.memory.put8(turn_state(battle) + menu_done, 1);
        return;
    default:
        return;
    }
}

} // namespace

void defend(Battle &battle, std::uint32_t member) {
    auto &memory = battle.memory;
    memory.put8(battle.record_base() + 0x5fc2, 0);
    const auto record = battle.record(member & 0xff); // 800c34b0 reloaded per access
    memory.put8(record + 0x15a, memory.u8(record + 0x15a) | 1);
    if ((memory.u8(battle.record(member & 0xff) + 0x15a) & 0x80) != 0 &&
        (memory.u16(battle.record(member & 0xff) + 0x126) & 0x10) != 0) {
        const auto current = battle.record(member & 0xff);
        memory.put16(current + 0x120, memory.u16(current + 0x120) & 0xfe4f);
        memory.put16(current + 0x7c, memory.u16(current + 0x7c) & 0xefff);
    }
    if (memory.u8(0x800d2c34) == 4)
        memory.put8(battle.record_base() + 0x5fc7, 0x3d);
}

bool try_escape(Battle &battle, std::uint32_t slot) {
    static_cast<void>(slot); // 8009a9d0 takes the acting slot and ignores it
    battle.memory.put8(battle.record_base() + 0x5fc2, 0);
    const auto roll = static_cast<std::int32_t>(battle.rand());
    if (roll % 100 >= 50)
        return false;
    write_back_party(battle);
    return true;
}

void write_back_party(Battle &battle) {
    auto &memory = battle.memory;
    constexpr std::uint32_t characters = 0x8006d8a0; // 0xa4 per character
    constexpr std::uint32_t gears = characters + 0x70c;
    for (std::uint32_t member = 0; member < 3; ++member) {
        if (memory.u8(0x800d2d24 + member) == 0x7f)
            continue;
        const auto record = fixed_record_base + member * record_stride;
        const auto gear = record + 0xa4;
        const auto character = characters + memory.u8(record + 0x56) * 0xa4;
        if (memory.u8(record + 0x56) == 7 && (memory.u8(record + 0x15a) & 0x80) != 0) {
            memory.put16(record + 0x4c, (memory.u32(gear + 0x60) + 1) / 50);
            if (memory.u16(record + 0x4c) == 0)
                memory.put16(record + 0x4c, 1);
        }
        memory.put16(character + 0x4c, memory.u16(record + 0x4c));
        memory.put16(character + 0x50, memory.u16(record + 0x50));
        if (memory.u16(character + 0x4e) < memory.u16(character + 0x4c))
            memory.put16(character + 0x4c, memory.u16(character + 0x4e));
        if (memory.u16(character + 0x52) < memory.u16(character + 0x50))
            memory.put16(character + 0x50, memory.u16(character + 0x52));
        for (std::uint32_t counter = 0; counter < 7; ++counter)
            memory.put16(character + 0x90 + counter * 2, memory.u16(record + 0x90 + counter * 2));
        memory.put16(character + 0x3a, memory.u16(record + 0x3a));
        if ((memory.u16(record + 0x7c) & 0xc000) != 0)
            memory.put16(character + 0x4c, 1);
        const auto id = memory.s8(record + 0xa0);
        if (id < 0 || (id >= 7 && (id >= 17 || id < 8)))
            continue;
        const auto machine = gears + static_cast<std::uint32_t>(id) * 0xa4;
        memory.put32(machine + 0x60, memory.u32(gear + 0x60));
        memory.put16(machine + 0x38, memory.u16(gear + 0x38));
        if (memory.u32(machine + 0x64) < memory.u32(machine + 0x60))
            memory.put32(machine + 0x60, memory.u32(machine + 0x64));
        if (memory.u16(machine + 0x3a) < memory.u16(machine + 0x38))
            memory.put16(machine + 0x38, memory.u16(machine + 0x3a));
        if ((memory.u16(gear + 0x7c) & 0x8000) != 0)
            memory.put32(machine + 0x60, memory.u32(machine + 0x64) / 10);
    }
}

void decode_input(Battle &battle, ResidentState &resident) {
    auto &memory = battle.memory;
    auto &queue = resident.input_queue;
    // 80035734(0): an absent pad (status ff) holds here showing a notice.
    if (resident.pad_status[0] == 0xff)
        throw BattleError("The missing-controller wait (8001fab4, 80037ee4) is not reconstructed");
    if (memory.u8(paused) != 0)
        throw BattleError("The battle pause loop (80037ee4/80037e8c) is not reconstructed");
    std::uint32_t code = 8;
    if (queue.overflow != 0) {
        queue.reset();
    } else {
        while (queue.dequeue()) {
            if (memory.u8(outcome) != 0 || memory.u8(turn_state(battle) + events_done) != 0) {
                code = 0xff;
                break;
            }
            const std::uint32_t buttons = queue.current[4]; // 800594a4
            const std::uint32_t keys = queue.current[2];    // 8005948c
            std::uint32_t face = 4;
            for (const auto [mask, value] :
                 {std::pair{0x2000U, 0U}, {0x4000U, 1U}, {0x8000U, 2U}, {0x1000U, 3U}}) {
                if ((buttons & mask) != 0) {
                    face = value;
                    break;
                }
            }
            if (face < 4) {
                menu_effect(battle, resident, 0x4c);
                const auto last = memory.u8(face_code);
                code = face;
                memory.put8(face_code, face);
                memory.put8(previous_face, last);
                break;
            }
            if ((keys & 0x20) != 0) {
                code = 4;
                menu_effect(battle, resident, 0x4d);
                break;
            }
            if ((keys & 0x40) != 0) {
                code = 5;
                menu_effect(battle, resident, 0x4e);
                break;
            }
            if ((keys & 0x80) != 0 || (keys & 0x10) != 0) {
                code = (keys & 0x80) != 0 ? 6 : 7;
                menu_effect(battle, resident, 0x4d);
                break;
            }
            if ((keys & 1) != 0) {
                code = 12;
                if (debug_enabled(resident))
                    throw BattleError("The debug gauge fill (800d32a0) is not reconstructed");
                break;
            }
            if ((keys & 2) != 0) {
                if (debug_enabled(resident))
                    throw BattleError("The debug capture (8003747c, 800374e8) is not "
                                      "reconstructed");
                break;
            }
            if ((keys & 0x100) != 0) {
                code = 13;
                break;
            }
            if ((keys & 0x800) != 0) {
                code = 14;
                if (memory.u8(0x800ccc58) != 0)
                    throw BattleError("The battle pause (8001fab4, 80037ee4) is not "
                                      "reconstructed");
                break;
            }
        }
    }
    memory.put8(command_code, code);
}

void menu_step(Battle &battle, ResidentState &resident, std::uint32_t member) {
    const auto current = battle.memory.u8(turn_state(battle) + page);
    switch (current) {
    case 1:
        return page_1(battle, resident, member);
    case 3:
        return page_3(battle, resident, member);
    case 9:
        return page_9(battle, resident, member);
    default:
        if (current - 1 >= 0x65)
            return;
        throw BattleError(
            std::format("Command page {:#x} (table 8006fe7c) is not reconstructed", current));
    }
}

} // namespace xem::reconstruction::battle
