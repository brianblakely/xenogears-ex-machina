// The party command menu's attack pages: entering the attack page from page 1,
// the attack page (5) with its direction targeting and confirmation, the
// combo bookkeeping and the attack's execution. Presentation (camera, target
// text, attack model, sprite text, frames) stays outside: glyph sprites are
// counted from their definition table, not built.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/field_collision.hpp"
#include "xem/reconstruction/program.hpp"

#include <cstdint>
#include <format>
#include <utility>

namespace xem::reconstruction::battle {
namespace {

constexpr std::uint32_t fixed_record_base = 0x800ccce8;
constexpr std::uint32_t command_code = 0x800d3014;    // u8: decoded menu input
constexpr std::uint32_t effects_enabled = 0x800d366c; // u8
constexpr std::uint32_t group_joined = 0x800c3e18;    // u8: 80087af0 changed group once
constexpr std::uint32_t ui_state_pointer = 0x800d2d28;
constexpr std::uint32_t graphics_state_pointer = 0x800c3ea4;
constexpr std::uint32_t combo_text_pointer = 0x800d2db4; // block of the combo sprites
constexpr std::uint32_t glyph_table_pointer = 0x800d2f5c;
constexpr std::uint32_t arrows_pointer = 0x800c3e24; // direction arrow primitives
constexpr std::uint32_t text_blocks = 0x800c3a70;    // 3 block pointers of 800861d0
constexpr std::uint32_t characters = 0x800d2d24;     // u8 character id per member
constexpr std::uint32_t slot_flags = 0x800d32a1;     // u8, 8 bytes per slot
constexpr std::uint32_t slot_bits = 0x800c3448;      // u16 per slot
constexpr std::uint32_t skill_bits = 0x8006ecf4;     // u16, 0x20 per character
constexpr std::uint32_t skill_masks = 0x800c3468;    // u16 per skill (80089c6c)
constexpr std::uint32_t combo_patterns = 0x800c3160; // 13 pointers to 7-byte histories
constexpr std::uint32_t deathblows = 0x800c31ac;     // pointer per character (+56)
constexpr std::uint32_t reload_table = 0x800c31d4;   // u8 [max AP][AP left]
constexpr std::uint32_t combo_steps = 0x800c34b3;    // u8 [step][cost], 3 per step
constexpr std::uint32_t candidates = 0x800c3e90;     // u8 x 11
constexpr std::uint32_t slot_info = 0x800c3eb4;      // 0x1c per slot; +a x, +c y
constexpr std::uint32_t timer = 0x800d2e06;          // u16 per slot
constexpr std::uint32_t committed_targets = 0x800d2c94;
constexpr std::uint32_t committed_type = 0x800d2c98;
constexpr std::uint32_t knocked_out = 0x800c48e8; // u16 mask
constexpr std::uint32_t buffer_index = 0x800ccb34;
constexpr std::uint32_t events = 0x800c3fe8;
constexpr std::uint32_t event_stride = 0x48;
// Turn-state (800c3eac block) offsets.
constexpr std::uint32_t turn_slot = 0x2d3;
constexpr std::uint32_t ap = 0x2d4;
constexpr std::uint32_t max_ap = 0x2d5;
constexpr std::uint32_t history_length = 0x2d6;
constexpr std::uint32_t history = 0x2cc; // 7 codes (input - 4)
constexpr std::uint32_t event_count = 0x2da;
constexpr std::uint32_t combo_step = 0x2dc;
constexpr std::uint32_t page = 0x2dd;
constexpr std::uint32_t cost = 0x2df;
constexpr std::uint32_t executing = 0x2e0;
constexpr std::uint32_t closed = 0x2e1;
constexpr std::uint32_t shown_page = 0x2e2;
constexpr std::uint32_t attacked = 0x2e3;
constexpr std::uint32_t combo_armed = 0x2e4;
constexpr std::uint32_t reacted = 0x2e5;
constexpr std::uint32_t target = 0x2e8;
constexpr std::uint32_t combo_started = 0x2e9;

std::uint32_t turn_state(const Battle &battle) { return battle.memory.u32(turn_state_pointer); }
std::uint32_t state(const Battle &battle, std::uint32_t offset) {
    return battle.memory.u8(turn_state(battle) + offset);
}
void set_state(Battle &battle, std::uint32_t offset, std::uint32_t value) {
    battle.memory.put8(turn_state(battle) + offset, value);
}
// Per-member menu block: +1c item availability halfwords, +3c default target.
std::uint32_t member_block(const Battle &battle, std::uint32_t member) {
    return turn_state(battle) + (member & 0xff) * 0x40;
}
std::uint32_t ui(const Battle &battle) { return battle.memory.u32(ui_state_pointer); }
// 80089c08.
std::uint32_t slot_bit(const Battle &battle, std::uint32_t slot) {
    return battle.memory.u16(slot_bits + (slot & 0xff) * 2);
}
// 80089c6c: whether `bits` holds skill `skill`'s mask (skills 0-15).
bool knows(const Battle &battle, std::uint32_t bits, std::uint32_t skill) {
    skill &= 0xff;
    return skill < 16 && (battle.memory.u16(skill_masks + skill * 2) & bits & 0xffff) != 0;
}
std::uint32_t member_skills(const Battle &battle, std::uint32_t member) {
    return battle.memory.u16(skill_bits + battle.memory.u8(characters + (member & 0xff)) * 0x20);
}
// The character's deathblow table (pointer by record +56).
std::uint32_t deathblow_table(const Battle &battle, std::uint32_t member) {
    const auto character =
        battle.memory.u8(fixed_record_base + (member & 0xff) * record_stride + 0x56);
    return battle.memory.u32(deathblows + character * 4);
}
std::uint32_t event(const Battle &battle) {
    return events + state(battle, event_count) * event_stride;
}

} // namespace

std::uint32_t draw_glyph(Battle &battle, std::uint32_t id, std::uint32_t destination,
                         std::uint32_t x, std::uint32_t y, std::uint32_t scale) {
    auto &memory = battle.memory;
    const auto table = memory.u32(glyph_table_pointer);
    const auto sprite = table + memory.u16(table + id * 2 + 4);
    const auto buffer = memory.u32(buffer_index);
    // Scaled (4.12): a negative product rounds toward zero.
    const auto scaled = [&](std::uint32_t at) {
        auto product = memory.s16(at) * static_cast<std::int32_t>(scale);
        if (product < 0)
            product += 0xfff;
        return static_cast<std::uint32_t>(product >> 12);
    };
    for (std::uint32_t part = 0; part != static_cast<std::uint32_t>(memory.s16(sprite)); ++part) {
        const auto source = sprite + 4 + part * 0x1c;
        const auto primitive = destination + part * 0x50 + buffer * 0x28;
        const auto left = scaled(source + 8);
        const auto top = scaled(source + 10);
        const auto width = scaled(source + 4);
        const auto height = scaled(source + 6);
        memory.put8(primitive + 3, 9); // SetPolyFT4 80043cb0
        memory.put8(primitive + 7, 0x2c);
        memory.put8(primitive + 7, memory.u8(primitive + 7) & 0xfd); // SetSemiTrans(0)
        memory.put8(primitive + 7, memory.u8(primitive + 7) | 1);    // SetShadeTex(1)
        // GetTPage 80043a1c (abr 0) and GetClut 80043a58.
        const auto page_x = static_cast<std::uint32_t>(memory.s16(source + 22));
        const auto page_y = static_cast<std::uint32_t>(memory.s16(source + 24));
        memory.put16(primitive + 22,
                     (static_cast<std::uint32_t>(memory.s16(source + 16)) & 3) << 7 |
                         (page_y & 0x100) >> 4 | (page_x & 0x3ff) >> 6 | (page_y & 0x200) << 2);
        memory.put16(primitive + 14,
                     static_cast<std::uint32_t>(memory.s16(source + 20)) << 6 |
                         (static_cast<std::uint32_t>(memory.s16(source + 18) >> 4) & 0x3f));
        auto u = memory.u16(source);
        auto v = memory.u16(source + 2);
        auto w = memory.u16(source + 4);
        auto h = memory.u16(source + 6);
        const auto near_x = x + left;
        const auto far_x = width + near_x;
        if (memory.u8(source + 26) == 0) {
            for (const auto [offset, value] :
                 {std::pair{8U, near_x}, {16U, far_x}, {24U, near_x}, {32U, far_x}})
                memory.put16(primitive + offset, value);
        } else { // mirrored: u starts one lower
            for (const auto [offset, value] :
                 {std::pair{8U, far_x}, {16U, near_x}, {24U, far_x}, {32U, near_x}})
                memory.put16(primitive + offset, value);
            u -= 1;
            if (((u << 16) & 0x80000000U) != 0) {
                u = 0;
                w -= 1;
            }
        }
        const auto near_y = y + top;
        const auto far_y = height + near_y;
        if (memory.u8(source + 27) == 0) {
            for (const auto [offset, value] :
                 {std::pair{10U, near_y}, {18U, near_y}, {26U, far_y}, {34U, far_y}})
                memory.put16(primitive + offset, value);
        } else {
            for (const auto [offset, value] :
                 {std::pair{10U, far_y}, {18U, far_y}, {26U, near_y}, {34U, near_y}})
                memory.put16(primitive + offset, value);
            v -= 1;
            if (((v << 16) & 0x80000000U) != 0) {
                v = 0;
                h -= 1;
            }
        }
        for (const auto [offset, value] : {std::pair{12U, u},
                                           {13U, v},
                                           {20U, u + w},
                                           {21U, v},
                                           {28U, u},
                                           {29U, v + h},
                                           {36U, u + w},
                                           {37U, v + h}})
            memory.put8(primitive + offset, value);
    }
    return static_cast<std::uint32_t>(memory.s16(sprite));
}

// 8008ac00(count): 80032498(2, 0) selects owner tag 2 (clearing its word and
// the quiet flag), then 80031bdc allocates (count + 3) * 26 bytes first fit.
std::uint32_t allocate_text_block(Battle &battle, ResidentState &resident, std::uint32_t count) {
    auto &heap = resident.heap;
    heap.tag = 2;
    heap.tag_words[2] = 0;
    heap.quiet = 0;
    auto block = resident::heap_allocate(heap, (count + 3) * 26, 0, 0x8008ac34);
    if (!block)
        throw BattleError("A quiet null text-block allocation (8008ac00) is not reconstructed");
    const auto address = block->address;
    battle.memory.regions.emplace(address, std::move(block->bytes));
    return address;
}

namespace {

// 80076a10(id, destination, x, y): sprite `id` of the glyph table at scale 1.
std::uint32_t glyph(Battle &battle, std::uint32_t id, std::uint32_t destination, std::uint32_t x,
                    std::uint32_t y) {
    return draw_glyph(battle, id, destination, x, y, 0x1000);
}

// 800320e8 on a block battle memory owns.
void release_block(Battle &battle, ResidentState &resident, std::uint32_t address,
                   std::uint32_t call_site) {
    resident::HeapBlock block{address, {}};
    if (address != 0) {
        const auto found = battle.memory.regions.find(address);
        if (found == battle.memory.regions.end())
            throw BattleError(std::format("Released block {:08x} is not battle memory", address));
        block.bytes = std::move(found->second);
        battle.memory.regions.erase(found);
    }
    if (resident::heap_release(resident.heap, block, call_site) == -1)
        battle.memory.regions.emplace(address, std::move(block.bytes)); // kept
}

// 80085d34: the AP counter sprites (AP, "/", maximum) and their buffer.
void show_ap(Battle &battle) {
    auto &memory = battle.memory;
    memory.put8(ui(battle) + 0x7b, 0);
    for (const auto [id, x] : {std::pair{state(battle, ap) + 0xf, 0x2aU},
                               {0x19U, 0x32U},
                               {state(battle, max_ap) + 0xf, 0x3aU}}) {
        const auto destination =
            memory.u32(graphics_state_pointer) + 0x9c8 + memory.u8(ui(battle) + 0x7b) * 0x50;
        memory.put8(ui(battle) + 0x7b,
                    memory.u8(ui(battle) + 0x7b) + glyph(battle, id, destination, x, 0xd0));
    }
    memory.put8(ui(battle) + 0xa4, memory.u8(buffer_index));
}

// 800879a8: every event's type ff, actor and target mask.
void reset_events(Battle &battle, std::uint32_t actor, std::uint32_t slot) {
    for (std::uint32_t queued = 0; queued < 32; ++queued) {
        const auto at = events + queued * event_stride;
        battle.memory.put8(at + 0x47, 0xff);
        battle.memory.put8(at + 0x23, actor);
        battle.memory.put16(at + 0x16, slot_bit(battle, slot));
    }
}

// 80084a7c: the attack target starts at the default target when it is a
// candidate, otherwise at the first candidate.
void choose_attack_target(Battle &battle, std::uint32_t member) {
    auto &memory = battle.memory;
    set_state(battle, target, memory.u8(member_block(battle, member) + 0x3c));
    static_cast<void>(order_candidates(battle, member));
    bool found = false;
    const auto count = memory.u8(0x800d3274);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (memory.u8(candidates + i) == memory.u8(member_block(battle, member) + 0x3c)) {
            found = true;
            break;
        }
    }
    if (!found)
        set_state(battle, target, memory.u8(candidates));
}

// 80077698: the four direction arrows as two buffers of POLY_G3 primitives
// (SetPolyG3 80043c74) in the block at 800c3e24.
void build_arrows(Battle &battle) {
    auto &memory = battle.memory;
    const auto base = memory.u32(arrows_pointer);
    for (std::uint32_t offset = 0; offset < 0xec; ++offset) // bzero 8003f8e8
        memory.put8(base + offset, 0);
    // x0, y0, x1, y1, x2, y2 of each arrow.
    constexpr std::uint32_t vertices[4][6] = {{0xc0, 0x70, 0xb0, 0x78, 0xb0, 0x68},
                                              {0xa0, 0x90, 0x98, 0x80, 0xa8, 0x80},
                                              {0x80, 0x70, 0x90, 0x68, 0x90, 0x78},
                                              {0xa0, 0x50, 0x98, 0x60, 0xa8, 0x60}};
    for (std::uint32_t arrow = 0; arrow < 4; ++arrow) {
        for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
            const auto primitive = base + (arrow * 2 + buffer) * 0x1c;
            memory.put8(primitive + 3, 6);
            memory.put8(primitive + 7, 0x30);
            for (std::uint32_t i = 0; i < 3; ++i) {
                memory.put16(primitive + 8 + i * 8, vertices[arrow][i * 2]);
                memory.put16(primitive + 10 + i * 8, vertices[arrow][i * 2 + 1]);
            }
            for (const auto [offset, value] : {std::pair{4U, 0xffU},
                                               {5U, 0U},
                                               {6U, 0U},
                                               {0xcU, 0x40U},
                                               {0xdU, 0U},
                                               {0xeU, 0U},
                                               {0x14U, 0x40U},
                                               {0x15U, 0U},
                                               {0x16U, 0U}})
                memory.put8(primitive + offset, value);
        }
    }
    memory.put32(base + 0xe0, 0xff);
    memory.put8(base + 0xe5, 1);
    memory.put8(base + 0xe4, memory.u8(buffer_index));
    memory.put8(ui(battle) + 0xc6, 1);
}

// 80080b64: close the member's command with event type fe.
void close_command(Battle &battle, std::uint32_t member) {
    battle.memory.put8(event(battle) + 0x47, 0xfe);
    battle.memory.put8(event(battle) + 0x23, member);
    battle.memory.put8(effects_enabled, 0);
}

// 8009413c: a member in character slot 4 (800d2d24) clears the UI's +b7 and
// +b0; with `close` it also calls 8007fb70.
void end_member_menu(Battle &battle, std::uint32_t member, bool close) {
    if (battle.memory.u8(characters + (member & 0xff)) != 4)
        return;
    battle.memory.put8(ui(battle) + 0xb7, 0);
    battle.memory.put8(ui(battle) + 0xb0, 0);
    if (close)
        throw BattleError("The slot-4 menu close 8007fb70 (8008fa60, 8007765c) is not "
                          "reconstructed");
}

// 8008189c: while no combo started, the camera and target cursor follow the
// target, each direction arrow shows whether a target lies that way, and the
// target name window follows (unless 800d2c34 is 4).
void view_target(Battle &battle, const ResidentState &resident, std::uint32_t member,
                 const MenuPresent &present) {
    auto &memory = battle.memory;
    if (state(battle, combo_started) != 0)
        return;
    present(MenuPresentation::camera, slot_bit(battle, state(battle, target)));
    present(MenuPresentation::target_camera, slot_bit(battle, state(battle, target)));
    for (std::uint32_t direction = 0; direction < 4; ++direction) {
        const auto found = direction_target(battle, resident, state(battle, target), direction);
        memory.put8(memory.u32(arrows_pointer) + direction + 0xe6,
                    (found & 0xff) == state(battle, target) ? 0 : 1);
    }
    if (memory.u8(0x800d2c34) != 4)
        present(MenuPresentation::target_text, member & 0xff);
}

// 800819a4: once per menu, fix the target, drop the target displays, face
// the camera on member and target and queue the approach event (fd).
// 800819a4 after its target camera call (800819e4).
void start_combo_rest(Battle &battle, ResidentState &resident, std::uint32_t member,
                      const MenuPresent &present) {
    auto &memory = battle.memory;
    const auto m = member & 0xff;
    memory.put8(member_block(battle, m) + 0x3c, state(battle, target));
    if (memory.u8(ui(battle) + 0xae) != 0) { // 8007fce8
        release_block(battle, resident, memory.u32(0x800d367c), 0x8007fd10);
        memory.put8(ui(battle) + 0xae, 0);
    }
    if (memory.u8(ui(battle) + 0x96) != 0) { // 8007fdec
        release_block(battle, resident, memory.u32(0x800c3de8), 0x8007fe14);
        memory.put8(ui(battle) + 0x96, 0);
    }
    memory.put8(ui(battle) + 0xc6, 0); // 80077980
    const auto chosen = memory.u8(member_block(battle, m) + 0x3c);
    present(MenuPresentation::camera, slot_bit(battle, m) | slot_bit(battle, chosen));
    if (memory.u8(characters + m) != 4) {
        memory.put8(event(battle) + 0x23, member);
        memory.put8(event(battle) + 0x47, 0xfd);
        memory.put16(event(battle) + 0x3a, 0);
        memory.put16(event(battle) + 0x16,
                     slot_bit(battle, memory.u8(member_block(battle, m) + 0x3c)));
        set_state(battle, event_count, state(battle, event_count) + 1);
    }
    end_member_menu(battle, m, false);
}

// The history pattern (800c3160) the combo history matches, 13 when none.
std::uint32_t find_combo(const Battle &battle) {
    std::uint32_t k = 0;
    for (; k < 13; ++k) {
        const auto pattern = battle.memory.u32(combo_patterns + k * 4);
        bool match = false;
        for (std::uint32_t i = 0; i < 7; ++i) {
            match = state(battle, history + i) == battle.memory.u8(pattern + i);
            if (!match)
                break;
        }
        if (match)
            break;
    }
    return k;
}

// 80085eb4(4, member): the history matches a combo the member knows.
bool knows_combo(const Battle &battle, std::uint32_t member) {
    if (state(battle, history_length) == 0)
        return false;
    const auto k = find_combo(battle);
    if (k >= 13)
        return false;
    return knows(battle, member_skills(battle, member),
                 battle.memory.u8(deathblow_table(battle, member) + k));
}

// 800861d0 up to its frame: record the input in the combo history, show the
// history and the combo it completes. A known, available deathblow's name
// (80086028) is not reconstructed.
void record_combo(Battle &battle, ResidentState &resident, std::uint32_t code, std::uint32_t member,
                  const MenuPresent &present) {
    auto &memory = battle.memory;
    for (std::uint32_t i = 0; i < 3; ++i)
        memory.put32(text_blocks + i * 4, allocate_text_block(battle, resident, 0x1e));
    set_state(battle, history + state(battle, history_length), (code + 0xfc) & 0xff);
    set_state(battle, history_length, state(battle, history_length) + 1);
    const auto last = history + state(battle, history_length) - 1;
    if (state(battle, combo_armed) != 0)
        set_state(battle, last, 0xff);
    const auto k = find_combo(battle);
    if (state(battle, combo_armed) != 0)
        set_state(battle, last, (code + 0xfc) & 0xff);
    const auto sprites = memory.u32(combo_text_pointer);
    memory.put8(sprites + 0x5d81, 0);
    memory.put8(sprites + 0x5d80, 0);
    for (std::uint32_t i = 0; i < state(battle, history_length); ++i) {
        std::uint32_t id = 0;
        switch (state(battle, history + i)) {
        case 0:
            id = 0x5d;
            break;
        case 2:
            id = 0x5e;
            break;
        case 3:
            id = 0x5f;
            break;
        default: // the id register keeps its previous value
            throw BattleError("A combo history entry other than 0, 2 or 3 is not reconstructed");
        }
        const auto destination = sprites + 0x5640 + memory.u8(sprites + 0x5d81) * 0x50;
        memory.put8(sprites + 0x5d81, memory.u8(sprites + 0x5d81) +
                                          glyph(battle, id, destination, 0x50 + i * 0x10, 0xd0));
    }
    const auto m = member & 0xff;
    const auto table = deathblow_table(battle, m);
    const auto name = [&] {
        throw BattleError("The deathblow name display 80086028 is not reconstructed");
    };
    if (k < 13 && knows(battle, member_skills(battle, m), memory.u8(table + k)) &&
        memory.u16(member_block(battle, m) + 0x20) == 0)
        name();
    // Longer deathblows the history leads into (the second from 800c31ac + 13,
    // the third from + 20).
    if (k < 9 && k != 6 && k != 7) {
        const auto index = k == 8 ? 19U : 13U + k;
        if (knows(battle, member_skills(battle, m), memory.u8(table + index)) &&
            memory.u16(member_block(battle, m) + 0x1c) == 0 &&
            memory.u16(member_block(battle, m) + 0x20) == 0)
            name();
    }
    if (k == 0 || k == 1 || k == 3) {
        const auto index = k == 3 ? 22U : k == 1 ? 21U : 20U;
        if (knows(battle, member_skills(battle, m), memory.u8(table + index)) &&
            memory.u16(member_block(battle, m) + 0x1e) == 0 &&
            memory.u16(member_block(battle, m) + 0x20) == 0)
            name();
    }
    memory.put8(sprites + 0x5d90, memory.u8(buffer_index));
    memory.put8(sprites + 0x5d8f, memory.u8(buffer_index));
    present(MenuPresentation::frame, 0);
    memory.put8(ui(battle) + 0xa8, 1); // in the frame call's delay slot
}

// 80079840: an enemy target remembers its attacker, the committed command
// (800d2ca4, 5 bytes) and whether the attack knocked it out.
void remember_attack(Battle &battle, std::uint32_t member, std::uint32_t slot) {
    auto &memory = battle.memory;
    if ((slot & 0xff) < 3)
        return;
    const auto block = 0x800d3400 + ((slot - 3) & 0xff) * 0x40;
    memory.put16(block + 0x2e, slot_bit(battle, member));
    for (std::uint32_t i = 0; i < 5; ++i)
        memory.put8(block + 0x39 + i, memory.u8(0x800d2ca4 + i));
    const auto chosen = memory.u8(member_block(battle, member) + 0x3c);
    memory.put8(block + 0x3e, (memory.u16(knocked_out) & slot_bit(battle, chosen)) != 0 ? 1 : 0);
}

// 80087af0(member, cost): execute the attack; returns whether the combo
// completes a known deathblow or the target's reaction ran action 62.
bool execute_attack(Battle &battle, std::uint32_t member, std::uint32_t cost_paid) {
    auto &memory = battle.memory;
    const auto m = member & 0xff;
    bool result = false;
    if (memory.u8(group_joined) == 0) {
        if (memory.u8(slot_flags + m * 8) != 0)
            throw BattleError("The flagged-slot group change 800881b8 is not reconstructed");
        if (memory.u8(characters + m) != 4)
            join_target_group(battle, m, memory.u8(member_block(battle, m) + 0x3c));
        memory.put8(group_joined, 1);
    }
    clear_current_event(battle);
    if (memory.u8(slot_flags + m * 8) == 0) {
        const auto step = state(battle, combo_step);
        if (step < 8) {
            set_state(battle, combo_step, memory.u8(combo_steps + step * 3 + (cost_paid & 0xff)));
        } else if (knows(battle, member_skills(battle, m), step - 8)) {
            result = true;
        } else {
            set_state(battle, combo_step, 7);
        }
    } else {
        set_state(battle, combo_step, state(battle, combo_step) + 1);
    }
    const auto record = [&] {
        return fixed_record_base + memory.u8(member_block(battle, m) + 0x3c) * record_stride;
    };
    if ((memory.u16(record() + 0x34) & 0x800) != 0) {
        memory.put8(event(battle) + 0x23, member);
        memory.put8(event(battle) + 0x47, 0xf3);
        memory.put16(event(battle) + 0x3a,
                     slot_bit(battle, memory.u8(member_block(battle, m) + 0x3c)));
        set_state(battle, event_count, state(battle, event_count) + 1);
    }
    commit_action(battle, m, slot_bit(battle, memory.u8(member_block(battle, m) + 0x3c)),
                  (state(battle, combo_step) - 1) & 0xffff);
    const auto queue = state(battle, event_count);
    memory.put8(event(battle) + 0x23, member);
    memory.put16(event(battle) + 0x16, memory.u16(committed_targets));
    memory.put8(event(battle) + 0x47, memory.u8(committed_type));
    set_state(battle, event_count, state(battle, event_count) + 1);
    remember_attack(battle, m, memory.u8(member_block(battle, m) + 0x3c));
    const auto chosen = memory.u8(member_block(battle, m) + 0x3c);
    if (chosen >= 3 && run_reaction_script(battle, chosen))
        result = true;
    apply_event(battle, queue);
    return result;
}

// 80081b58 after 800861d0: the execution, the timer reload and the close.
void finish_attack(Battle &battle, std::uint32_t member) {
    auto &memory = battle.memory;
    const auto m = member & 0xff;
    set_state(battle, reacted, execute_attack(battle, m, state(battle, cost)) ? 1 : 0);
    // 100 * table / 56 through the multiply-high 92492493.
    const auto scaled = static_cast<std::int32_t>(
        memory.u8(reload_table + state(battle, max_ap) * 8 + state(battle, ap)) * 100);
    const auto high = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(scaled) * static_cast<std::int32_t>(0x92492493U)) >> 32);
    memory.put16(timer + m * 2, static_cast<std::uint32_t>((high + scaled) >> 5));
    set_state(battle, attacked, 1);
}

// 80082024: the command closes once no AP remain or the attack reacted.
void close_when_done(Battle &battle, std::uint32_t member) {
    if (state(battle, ap) == 0) {
        close_command(battle, member & 0xff);
        set_state(battle, closed, 1);
    }
    if (state(battle, reacted) != 0) {
        close_command(battle, member & 0xff);
        set_state(battle, closed, 1);
    }
}

} // namespace

std::uint32_t direction_target(const Battle &battle, const ResidentState &resident,
                               std::uint32_t origin, std::uint32_t direction) {
    const auto &memory = battle.memory;
    const auto from = origin & 0xff;
    const auto way = direction & 0xff;
    std::uint32_t result = origin;
    std::uint32_t best = 0xffffff;
    const auto x = [&](std::uint32_t slot) { return memory.u16(slot_info + slot * 0x1c + 0xa); };
    const auto y = [&](std::uint32_t slot) { return memory.u16(slot_info + slot * 0x1c + 0xc); };
    for (std::uint32_t i = 0; i < 11; ++i) {
        const auto slot = memory.u8(candidates + i);
        if (slot == 0xff || slot == from)
            continue;
        const auto dy = static_cast<std::int32_t>(y(slot)) - static_cast<std::int32_t>(y(from));
        const auto dx = static_cast<std::int32_t>(x(slot)) - static_cast<std::int32_t>(x(from));
        // ratan2 8004b32c.
        const auto angle =
            static_cast<std::uint32_t>(field::collision_atan(dy, dx, resident.math.angle).angle);
        bool hit = false;
        switch (way) {
        case 0:
            hit = ((angle + 0x200) & 0xffff) < 0x400;
            break;
        case 1:
            hit = ((angle + 0x600) & 0xffff) < 0x400;
            break;
        case 2:
            hit = ((angle + 0x800) & 0xffff) < 0x200 || ((angle - 0x600) & 0xffff) < 0x201;
            break;
        case 3:
            hit = ((angle - 0x200) & 0xffff) < 0x400;
            break;
        default:
            break;
        }
        if (!hit)
            continue;
        const auto square = [](std::int32_t d) {
            const auto u = static_cast<std::uint32_t>(d);
            return u * u;
        };
        const auto distance = static_cast<std::int32_t>(square(dy) + square(dx));
        if (distance < static_cast<std::int32_t>(best)) {
            best = static_cast<std::uint32_t>(distance);
            result = slot;
        }
    }
    return result & 0xff;
}

void enter_attack_page(Battle &battle, ResidentState &resident, std::uint32_t member,
                       const MenuPresent &present) {
    static_cast<void>(resident);
    auto &memory = battle.memory;
    const auto m = member & 0xff;
    memory.put8(effects_enabled, 0);
    // 80087a38 up to the attack model.
    show_ap(battle);
    reset_events(battle, m, memory.u8(member_block(battle, m) + 0x3c));
    memory.put8(effects_enabled, 0);
    approach_route(battle, m, memory.u8(member_block(battle, m) + 0x3c));
    // 80080ae4 (the next slot to act) only parameterizes the model.
    present(MenuPresentation::attack_model, m);
    resume_attack_entry(battle, resident, m);
}

void resume_attack_entry(Battle &battle, ResidentState &resident, std::uint32_t member) {
    static_cast<void>(resident);
    battle.memory.put8(effects_enabled, 1);
    battle.memory.put8(group_joined, 0);
    choose_attack_target(battle, member & 0xff);
    build_arrows(battle);
    set_state(battle, page, 5);
}

void attack_page(Battle &battle, ResidentState &resident, std::uint32_t member,
                 const MenuPresent &present) {
    auto &memory = battle.memory;
    memory.put8(effects_enabled, 0);
    if (state(battle, closed) != 0) {
        memory.put8(effects_enabled, 0);
        return;
    }
    const auto m = member & 0xff;
    set_state(battle, cost, 0);
    view_target(battle, resident, m, present);
    resume_attack_view(battle, resident, m, present);
}

void resume_attack_view(Battle &battle, ResidentState &resident, std::uint32_t member,
                        const MenuPresent &present) {
    auto &memory = battle.memory;
    const auto m = member & 0xff;
    switch (memory.u8(command_code)) {
    case 5:
        if (state(battle, ap) == state(battle, max_ap)) {
            present(MenuPresentation::model_reset, 0);
            memory.put8(effects_enabled, 1);
            memory.put8(ui(battle) + 0x7b, 0);
            memory.put8(ui(battle) + 0xaf, 0);
            set_state(battle, page, 1);
            memory.put8(ui(battle) + 0xc6, 0); // 80077980
            end_member_menu(battle, m, true);
        } else {
            close_command(battle, m);
            set_state(battle, closed, 1);
        }
        break;
    case 4:
    case 6:
    case 7: {
        const auto item = memory.u8(command_code) == 4   ? 0x20U
                          : memory.u8(command_code) == 6 ? 0x1cU
                                                         : 0x1eU;
        if (memory.u16(member_block(battle, m) + item) != 0) {
            memory.put8(command_code, 5);
            play_menu_effect(resident, 0x4f);
        }
        break;
    }
    default:
        break;
    }
    if (state(battle, combo_started) == 0) {
        const auto code = memory.u8(command_code);
        if (code < 4) {
            set_state(battle, target,
                      direction_target(battle, resident, state(battle, target), code));
            reset_events(battle, state(battle, turn_slot), state(battle, target));
            play_menu_effect(resident, 0x4c);
        }
        approach_route(battle, member & 0xff, state(battle, target));
    }
    memory.put8(ui(battle) + 0xaf, 1);
    switch (memory.u8(command_code)) {
    case 4:
        if (knows_combo(battle, member) && static_cast<std::int32_t>(state(battle, ap)) - 3 >= 0)
            set_state(battle, combo_armed, state(battle, combo_armed) + 1);
        set_state(battle, cost, state(battle, cost) + 3);
        break;
    case 6:
        set_state(battle, cost, state(battle, cost) + 1);
        break;
    case 7:
        set_state(battle, cost, state(battle, cost) + 2);
        break;
    default:
        return;
    }
    if (static_cast<std::int32_t>(state(battle, ap)) -
            static_cast<std::int32_t>(state(battle, cost)) <
        0) {
        play_menu_effect(resident, 0x4f);
        return close_when_done(battle, m);
    }
    play_menu_effect(resident, 0x4d);
    if (state(battle, combo_started) == 0) { // 800819a4
        present(MenuPresentation::combo_camera, 0);
        set_state(battle, combo_started, 1); // in the camera call's delay slot
        start_combo_rest(battle, resident, member, present);
    }
    confirm_attack(battle, resident, m, present);
}

void resume_combo(Battle &battle, ResidentState &resident, std::uint32_t member,
                  const MenuPresent &present) {
    start_combo_rest(battle, resident, member, present);
    confirm_attack(battle, resident, member & 0xff, present);
}

void confirm_attack(Battle &battle, ResidentState &resident, std::uint32_t member,
                    const MenuPresent &present) {
    auto &memory = battle.memory;
    const auto m = member & 0xff;
    if (memory.u8(ui(battle) + 0xcb) != 0) {
        set_state(battle, page, 0x64);
        set_state(battle, shown_page, 0xff);
        set_state(battle, executing, 1);
    } else {
        set_state(battle, page, 5);
        set_state(battle, shown_page, 5);
    }
    set_state(battle, ap, state(battle, ap) - state(battle, cost));
    show_ap(battle);
    record_combo(battle, resident, memory.u8(command_code), m, present);
    resume_attack_confirm(battle, resident, m);
}

void resume_attack_confirm(Battle &battle, ResidentState &resident, std::uint32_t member) {
    for (std::uint32_t i = 0; i < 3; ++i) // 800861d0 after its frame
        release_block(battle, resident, battle.memory.u32(text_blocks + i * 4), 0x80086b40);
    finish_attack(battle, member & 0xff);
    close_when_done(battle, member & 0xff);
}

} // namespace xem::reconstruction::battle
