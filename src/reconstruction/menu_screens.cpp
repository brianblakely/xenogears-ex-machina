// Menu overlay 82f84a24... (801c5000): the item screen (801dbe54), its item
// list and description, the party status panels of target selection, and
// the use of an item on its targets.
#include "xem/reconstruction/menu.hpp"
#include "xem/reconstruction/menu_overlay.hpp"
#include "xem/reconstruction/resident_text.hpp"

namespace xem::reconstruction::menu {
namespace {
// Menu state fields (offsets from *800625a0).
constexpr std::uint32_t sprite_sheet = 0x2dc;       // sprite table for 8002675c
constexpr std::uint32_t buffer_index = 0x308;       // 0/1: the buffer being built
constexpr std::uint32_t number_digits = 0x31c;      // 801c80b8's nine digits (ff: blank)
constexpr std::uint32_t input_code = 0x325;         // decoded input of this frame
constexpr std::uint32_t target_marks_block = 0x428; // +140..142: marked party slots
constexpr std::uint32_t item_list_block = 0x42c;    // the item screen's text block
constexpr std::uint32_t item_windows = 0x43c;       // window record (+70: its buffer byte)
constexpr std::uint32_t side_windows = 0x444;       // two window records (+75: buffer byte)
constexpr std::uint32_t help_block = 0x440;         // released by 801d3674
constexpr std::uint32_t help_lines = 0x10e0;        // eight 80-byte text lines
constexpr std::uint32_t item_target = 0x4dc;        // u8: party slot the target cursor starts on
constexpr std::uint32_t status_panels = 0x1e08;     // three panel blocks of bec bytes
// Party list (*(state + 33c)) fields.
constexpr std::uint32_t party_ids = 0x30;  // three character ids, ff empty
constexpr std::uint32_t party_text = 0x38; // text line cleared by 801e8044
// Item list block (*(state + 42c), 1198 bytes): 16 visible entries of 80
// bytes with the item name text at +0 and its count text at +800; an entry's
// +50 is the text record 801c851c places, +7d the buffer it was built for,
// +7e its width. +1000/+1080 copy the selected entry, +1100 is the
// description line, +1180 the description message table, +1184 one flag per
// visible entry (text built) and +1194 the description shown flag.
constexpr std::uint32_t count_texts = 0x800;
constexpr std::uint32_t selected_name = 0x1000;
constexpr std::uint32_t selected_count = 0x1080;
constexpr std::uint32_t description = 0x1100;
constexpr std::uint32_t description_messages = 0x1180;
constexpr std::uint32_t entry_built = 0x1184;
constexpr std::uint32_t description_shown = 0x1194;
// Status panel block (bec bytes): sprite packets of 50 bytes, the layout
// sprites at +0 (count +be8), the record +62 digits at +500 (+be0), the HP
// (or gear) digits at +6e0 (+be2) and its maximum at +870 (+be3), the EP
// digits at +a00 (+be4) and maximum at +af0 (+be5), the label at +4b0 (28
// bytes per buffer); +be6 the buffer it was built for, +be7 shown.
// Overlay statics.
constexpr std::uint32_t list_row_y = 0x801ea724;   // u16: list window row
constexpr std::uint32_t scroll_rows = 0x801ea728;  // word: last scroll position
constexpr std::uint32_t scroll_step = 0x801ea72c;  // u16: scroll bar step (1068 / rows)
constexpr std::uint32_t panels_owned = 0x801e9785; // u8: status panel blocks allocated
constexpr std::uint32_t flag20_gate = 0x80059171;  // resident u8 gating items with use flag 20
// A gear's HP and maximum HP words (a4-byte records from 8006e00c), selected
// by the character record's +a0 byte.
constexpr std::uint32_t gear_hp = 0x8006e00c;
constexpr std::uint32_t character_gear = 0xa0;
constexpr std::uint32_t item_count_limit = 150;
} // namespace

// 801cd81c: the layout sprites of status panel `a0` for character `a1` on row
// `a2`: the nine sprite ids of layout `a5` (table 801ea4dc, ffff none) at
// the x words `a3` and y words `a4` (+ row * 38), the row's frame sprite
// (14b + row) at x/y word 9, then the label (801e927c) with texture
// page (0,0,180,0), a colour chosen by the character's parity (layout 0) or
// its gear flag (+a0 bit 0), placed by 801e920c at x/y halfwords +40 with
// its size and texture from tables 801ea578 / 801ea5c4.
void Overlay::status_panel_layout_sprites(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                          std::uint32_t a3, std::uint32_t a4, std::uint32_t a5) {
    const auto stack_frame = enter(0x58);
    const auto panel = a0;
    const auto character = a1 & 0xffU;
    const auto row = a2 & 0xffU;
    const auto xs = a3;
    const auto ys = a4;
    const auto layout = a5 & 0xffU;
    const auto row_y = row * 56;
    constexpr std::uint32_t sprites = 0xbe8;
    put8(panel + sprites, 0);
    for (std::uint32_t i = 0; i < 9; ++i) {
        const auto id = u32(0x801ea4dc + (layout * 9 + i) * 4);
        if (id == 0xffff)
            continue;
        const auto built = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), id, panel + u8(panel + sprites) * 0x50,
            u32(at(buffer_index)), u32(xs + i * 4), row_y + u32(ys + i * 4), 0x1000);
        put8(panel + sprites, u8(panel + sprites) + built);
    }
    resident::sheet_quads(*this, u32(at(sprite_sheet)), row + 0x14b, panel + 0x460,
                          u32(at(buffer_index)), u32(xs + 0x24), row_y + u32(ys + 0x24), 0x1000);
    const auto label = [&] { return panel + u32(at(buffer_index)) * 0x28 + 0x4b0; };
    init_text_quad(label());
    put16(label() + 0x16, get_tpage(0, 0, 0x180, 0));
    const bool first_colour = layout == 0
                                  ? (character & 1U) != 0
                                  : (u8(character_record(character) + character_gear) & 1U) == 0;
    put16(label() + 0xe, u16(first_colour ? 0x80059414U : 0x800595d4U));
    const auto entry = (layout * 3 + row) * 4;
    set_quad_rect(label(), u16(xs + 0x40), (u16(ys + 0x40) + row_y) & 0xffffU,
                  (u32(0x801ea578 + entry) << 2U) & 0xfcU, u8(0x801ea5c4 + entry), layout * 24 + 72,
                  0xd);
}

// 801cdb1c: the record +62 digits of status panel `a0`: character `a1`'s +62
// byte through 801c80b8, then a sprite per non-blank digit of the last three
// (state + 322..324) at x word +28 (+8 per digit) and y word +28 of row `a2`.
// The +63 byte is converted too; its count +be1 is cleared.
void Overlay::status_panel_byte62_digits(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                         std::uint32_t a3, std::uint32_t a4) {
    const auto stack_frame = enter(0x40);
    const auto panel = a0;
    const auto record = character_record(a1);
    const auto xs = a3;
    const auto ys = a4;
    split_decimal_digits(u8(record + 0x62));
    const auto row_y = (a2 & 0xffU) * 56;
    constexpr std::uint32_t sprites = 0xbe0;
    put8(panel + sprites, 0);
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto digit = u8(at(number_digits + 6 + i));
        if (digit == 0xff)
            continue;
        const auto built = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), digit, panel + u8(panel + sprites) * 0x50 + 0x500,
            u32(at(buffer_index)), i * 8 + u32(xs + 0x28), row_y + u32(ys + 0x28), 0x1000);
        put8(panel + sprites, u8(panel + sprites) + built);
    }
    split_decimal_digits(u8(record + 0x63));
    put8(panel + 0xbe1, 0);
}

// 801cdc6c: the value digits of status panel `a0` for character `a1` on row
// `a2`, layout `a5`. Layout 0 shows HP / maximum HP (record +4c/+4e, three
// digits each) and EP / maximum EP (+50/+52, two digits); other layouts show
// the gear's HP / maximum HP words (8006e00c + gear * a4, five digits). The
// value is placed by digit position, the maximum packed left (x + 8 per
// shown digit). Ghidra splits the function at 801ce024 (inside the last
// loop); that tail is status_panel_values_tail.
void Overlay::status_panel_values(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                  std::uint32_t a3, std::uint32_t a4, std::uint32_t a5) {
    const auto stack_frame = enter(0x68);
    const auto panel = a0;
    const auto record = character_record(a1);
    const auto row_y = (a2 & 0xffU) * 56;
    const auto xs = a3;
    const auto ys = a4;
    const auto layout = a5 & 0xffU;
    const auto gear = [&] { return gear_hp + u8(record + character_gear) * 0xa4; };
    // Sprites of digits first..first+count (state + 31c) into packets at
    // `packets` (count byte `counter`), at x word / y word `field`; `packed`
    // advances x only for shown digits.
    const auto digits = [&](std::uint32_t first, std::uint32_t count, std::uint32_t packets,
                            std::uint32_t counter, std::uint32_t field, bool packed) {
        std::uint32_t shown = 0;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto digit = u8(at(number_digits + first + i));
            if (digit == 0xff)
                continue;
            const auto built = resident::sheet_quads(
                *this, u32(at(sprite_sheet)), digit, panel + u8(panel + counter) * 0x50 + packets,
                u32(at(buffer_index)), (packed ? shown : i) * 8 + u32(xs + field),
                row_y + u32(ys + field), 0x1000);
            ++shown;
            put8(panel + counter, u8(panel + counter) + built);
        }
    };
    const auto count = layout == 0 ? 3U : 5U;
    const auto first = layout == 0 ? 6U : 4U;
    split_decimal_digits(layout == 0 ? u16(record + 0x4c) : u32(gear()));
    put8(panel + 0xbe2, 0);
    digits(first, count, 0x6e0, 0xbe2, 0x30, false);
    split_decimal_digits(layout == 0 ? u16(record + 0x4e) : u32(gear() + 4));
    put8(panel + 0xbe3, 0);
    digits(first, count, 0x870, 0xbe3, 0x34, true);
    if (layout != 0)
        return;
    split_decimal_digits(u16(record + 0x50));
    put8(panel + 0xbe4, 0);
    digits(7, 2, 0xa00, 0xbe4, 0x38, false);
    split_decimal_digits(u16(record + 0x52));
    put8(panel + 0xbe5, 0);
    status_panel_values_tail(panel, xs, ys, row_y);
}

// 801ce024: the tail of 801cdc6c (a Ghidra split inside its last loop, from
// the loop head 801ce014): the maximum EP digits (state + 323..324) of panel
// `a0` into the packets at +af0 (count +be5), packed left from x word `a1` +
// 3c, at y word `a2` + 3c plus the row offset `a3`.
void Overlay::status_panel_values_tail(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                       std::uint32_t a3) {
    const auto panel = a0;
    std::uint32_t shown = 0;
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto digit = u8(at(number_digits + 7 + i));
        if (digit == 0xff)
            continue;
        const auto built = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), digit, panel + u8(panel + 0xbe5) * 0x50 + 0xaf0,
            u32(at(buffer_index)), shown * 8 + u32(a1 + 0x3c), a3 + u32(a2 + 0x3c), 0x1000);
        ++shown;
        put8(panel + 0xbe5, u8(panel + 0xbe5) + built);
    }
}

// 801ce0cc: build status panel `a0` for character `a1` on row `a2` with
// layout `a5` (x words `a3`, y words `a4`): layout sprites and label
// (801cd81c), +62 digits (801cdb1c) and values (801cdc6c); mark it shown
// (+be7) for the current buffer (+be6).
void Overlay::build_status_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                 std::uint32_t a3, std::uint32_t a4, std::uint32_t a5) {
    const auto stack_frame = enter(0x38);
    const auto character = a1 & 0xffU;
    const auto row = a2 & 0xffU;
    const auto layout = a5 & 0xffU;
    status_panel_layout_sprites(a0, character, row, a3, a4, layout);
    status_panel_byte62_digits(a0, character, row, a3, a4);
    status_panel_values(a0, character, row, a3, a4, layout);
    put8(a0 + 0xbe7, 1);
    put8(a0 + 0xbe6, u8(at(buffer_index)));
}

// 801d3674: when the party list's +67 flag is set, clear it and +53 and
// release the block at state + 440.
void Overlay::release_help_block() {
    const auto stack_frame = enter(0x18);
    const auto party = u32(at(state_party));
    if (u8(party + 0x67) == 0)
        return;
    put8(party + 0x53, 0);
    put8(u32(at(state_party)) + 0x67, 0);
    release(u32(at(help_block)), 0x801d36c8);
}

// 801da4a8: open the item screen: 801d22f4(2), the help line (801e8018 of
// 801ea548 into state + 10e0), a cleared item list block (1198 bytes) at
// state + 42c, then 801c72bc(0).
void Overlay::open_item_screen() {
    const auto stack_frame = enter(0x18);
    set_markers(2);
    layout_labels_row4(8, at(help_lines), 0x801ea548);
    const auto block = allocate(0x1198, 0, 0x801da4e0);
    put32(at(item_list_block), block);
    bzero(block, 0x1198);
    load_menu_data_set(0);
}

// 801da5bc: build the 16 visible entries of the item list from scroll row
// `a0` (inventory index row * 2 + entry). An empty id clears its count and a
// zero count its id (entry not built); otherwise the count is capped at 99,
// the item name (80033818) and the two-digit count (tens c3 when zero) are
// rendered (80034eac) into a scratch image uploaded to (180 + 18 * column,
// 80 + d * line), and the text records are set (801e7c50, 801c851c) with
// the item's greyed flag: item flag 80, or 0 when flag 20 is set and
// 80059171 is clear.
void Overlay::build_item_list(std::uint32_t a0) {
    const auto stack_frame = enter(0x58);
    const auto row = a0;
    const auto image = allocate(0x3f6, 0, 0x801da5f0);
    auto f = frame(0x18);
    const auto codes = f[0]; // two text codes (tens, units)
    const auto text = f[8];  // their decoded bytes
    const auto rect = f[0x10];
    put8(codes + 1, 0);
    put8(codes + 3, 0);
    const auto list = [&] { return u32(at(item_list_block)); };
    for (std::uint32_t entry = 0; entry < 16; ++entry) {
        const auto name = entry * 0x80;
        const auto count = count_texts + name;
        const auto index = row * 2 + entry;
        const auto id = item_ids + index;
        const auto held = item_counts + index;
        if (u8(id) == 0) {
            put8(held, 0);
            put8(list() + entry + entry_built, 0);
            continue;
        }
        if (u8(held) == 0) {
            put8(id, 0);
            put8(list() + entry + entry_built, 0);
            continue;
        }
        if (u8(held) >= 100)
            put8(held, 99);
        put8(list() + name + 0x7e, layout_text(resident::item_name(*this, u8(id)), image, 0x24, 0));
        const auto tens = u8(held) / 10;
        put8(codes, (tens & 0xffU) == 0 ? 0xc3U : tens + 16);
        put8(codes + 2, u8(held) % 10 + 16);
        decode_text(codes, text, 2);
        put8(list() + name + 0x87e, layout_text(text, image, 0x24, 1));
        put16(rect, (entry & 1U) * 24 + 0x180);
        put16(rect + 2, entry / 2 * 13 + 0x80);
        put16(rect + 4, 0x28);
        put16(rect + 6, 13);
        load_image(rect, image);
        draw_sync();
        const auto flags = u8(u32(u32(at(state_tables)) + item_table) + u8(id) * 16 + 6);
        std::uint32_t greyed = flags & 0x80U;
        if ((flags & 0x20U) != 0 && u8(flag20_gate) == 0)
            greyed = 0;
        set_label_packets(list() + name, entry, 0x80, greyed | 1U);
        set_label_packets(list() + count, entry, 0x80, greyed | 2U);
        const auto column = entry & 1U;
        const auto y = (entry / 2 << 4U | 0xeU) & 0xffffU;
        set_screen_quad_vectors(list() + name + 0x50, (column * 0x88 + 40) & 0xfff8U, y,
                                u8(list() + name + 0x7e), 13);
        set_screen_quad_vectors(list() + count + 0x50, (column * 0x88 + 0x90) & 0xfff8U, y,
                                u8(list() + name + 0x87e), 13);
        put8(list() + name + 0x7d, u8(at(buffer_index)));
        put8(list() + name + 0x87d, u8(at(buffer_index)));
        put8(list() + entry + entry_built, 1);
    }
    release(image, 0x801da954);
    put8(u32(at(state_party)) + 0x48, 1);
}

// 801da9a8: the description of the entry under the cursor (`a0` visible
// entry, `a1` scroll row). An empty entry clears the party text line and the
// shown flag. Otherwise the item's message (80033728 of the table at +1180)
// is rendered into a cleared image, uploaded to (140,4e,3c,d) and set as the
// description line; the entry's name and count texts are copied to +1000 /
// +1080 and placed at (10,93) and (78 or 7c,93), opaque at 80 grey. Items
// with use flags c0 also show their target kind (help lines 801e8070 of
// 801ea550: all / one / none by flags 4000 and 1000, and 3 + (+4 & 3)).
void Overlay::show_item_description(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x40);
    const auto entry = a0;
    const auto id = item_ids + a1 * 2 + entry;
    const auto list = [&] { return u32(at(item_list_block)); };
    if (u8(id) == 0) {
        clear_bytes(8, u32(at(state_party)) + party_text);
        put8(list() + description_shown, 0);
        return;
    }
    const auto image = allocate(0x618, 0, 0x801da9ec);
    bzero(image, 0x618);
    const auto message =
        resident::offset_table_entry(*this, u32(list() + description_messages), u8(id));
    put8(list() + 0x117e, layout_text(message, image, 0x39, 0));
    {
        auto f = frame(0x28);
        const auto rect = f[0x20];
        put16(rect, 0x140);
        put16(rect + 2, 0x4e);
        put16(rect + 4, 0x3c);
        put16(rect + 6, 0xd);
        load_image(rect, image);
    }
    draw_sync();
    set_label_packets(list() + description, 0, 0, 0);
    set_quad_rect(list() + u32(at(buffer_index)) * 0x28 + description, 0x1c, 0xa1, 0, 0x4e,
                  u8(list() + 0x117e), 13);
    set_screen_quad_vectors(list() + description + 0x50, 0x1c, 0xa1, u8(list() + 0x117e), 13);
    release(image, 0x801dab0c);
    const auto name = entry << 7U;
    const auto count_x = u8(list() + name + 0x87e) == 0x10 ? 4U : 0U;
    memmove(list() + selected_name, list() + name, 0x80);
    memmove(list() + selected_count, list() + name + count_texts, 0x80);
    set_screen_quad_vectors(list() + selected_name + 0x50, 0x10, 0x93, u8(list() + name + 0x7e),
                            13);
    set_screen_quad_vectors(list() + selected_count + 0x50, count_x | 0x78U, 0x93,
                            u8(list() + name + 0x87e), 13);
    // An opaque packet of this buffer at 80 grey.
    const auto opaque = [&](std::uint32_t packet) {
        for (std::uint32_t i = 0; i < 3; ++i)
            put8(packet + 4 + i, 0x80);
        set_semi_trans(packet, 0);
    };
    opaque(list() + u32(at(buffer_index)) * 0x28 + selected_name);
    opaque(list() + u32(at(buffer_index)) * 0x28 + selected_count);
    clear_bytes(8, u32(at(state_party)) + party_text);
    const auto item = u32(u32(at(state_tables)) + item_table) + u8(id) * 16;
    if ((u8(item + 6) & 0xc0U) != 0) {
        const auto targets = u16(item + 4);
        std::uint32_t kind = 2;
        if ((targets & 0x4000U) == 0)
            kind = (targets & 0x1000U) == 0 ? 1U : 0U;
        place_label(8, at(help_lines), 0x801ea550, 0x801e9ea0, u32(at(state_party)) + party_text,
                    kind, 0, 1);
        const auto range = ((u8(item + 4) & 3U) + 3) & 0xffU;
        place_label(8, at(help_lines), 0x801ea550, 0x801e9ea0, u32(at(state_party)) + party_text,
                    range, 0, 1);
        opaque(state() + (kind << 7U) + help_lines + u32(at(buffer_index)) * 0x28);
        opaque(state() + (range << 7U) + help_lines + u32(at(buffer_index)) * 0x28);
    }
    put8(list() + 0x107d, u8(at(buffer_index)));
    put8(list() + 0x10fd, u8(at(buffer_index)));
    put8(list() + 0x117d, u8(at(buffer_index)));
    put8(list() + description_shown, 1);
}

// 801db39c: show (`a0` 1) or hide the item screen's texts through 801e8f60
// (windows 3 and 4) and 801e8eac: the window at state + 43c, each visible
// entry's name and count whose built packet is not a 20 code, the two
// windows at state + 444, the selected entry and description, and the eight
// help lines, each at the buffer it was built for.
void Overlay::show_item_screen_texts(std::uint32_t a0) {
    const auto stack_frame = enter(0x28);
    const auto shown = a0 & 0xffU;
    shade_window_quads(3, shown);
    shade_window_quads(4, shown);
    const auto window = u32(at(item_windows));
    shade_quad(window + u8(window + 0x70) * 0x28, shown);
    for (std::uint32_t entry = 0; entry < 16; ++entry) {
        const auto name = entry << 7U;
        const auto packet =
            u32(at(item_list_block)) + name + u8(u32(at(item_list_block)) + name + 0x7d) * 0x28;
        if (u8(packet + 4) == 0x20)
            continue;
        shade_quad(packet, shown);
        const auto list = u32(at(item_list_block));
        shade_quad(list + count_texts + name + u8(list + name + 0x87d) * 0x28, shown);
    }
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto side = u32(at(side_windows + i * 4));
        shade_quad(side + u8(side + 0x75) * 0x28, shown);
    }
    for (const auto text : {selected_name, selected_count, description}) {
        const auto list = u32(at(item_list_block));
        shade_quad(list + text + u8(list + text + 0x7d) * 0x28, shown);
    }
    for (std::uint32_t line = 0; line < 8; ++line) {
        const auto base = state();
        shade_quad(base + help_lines + line * 0x80 +
                       u8(base + help_lines + line * 0x80 + 0x7d) * 0x28,
                   shown);
    }
}

// 801db5e4: the target selection's party status panels. The three panel
// blocks (state + 1e08) are allocated once (flag 801e9785) and cleared; each
// party slot with a character is built (801ce0cc) on its row with layout 0
// (positions 801ea054 / 801ea0dc) or, for mode `a0` 2, layout 1 (801ea098 /
// 801ea120) when the character has a gear (+a0 not ff); an empty slot's
// panel is hidden. Then the party list's +46 is set and the three cursor
// quads of the target block (+148 selects each slot's buffer) are placed
// at x 90..a0, y 30 + 38 * slot, 10 high.
void Overlay::build_target_panels(std::uint32_t a0) {
    const auto stack_frame = enter(0x30);
    const auto panel = [&](std::uint32_t slot) { return u32(at(status_panels + slot * 4)); };
    if (u8(panels_owned) == 0) {
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            const auto block = allocate(0xbec, 0, 0x801db614);
            put32(at(status_panels + slot * 4), block);
            bzero(block, 0xbec);
        }
        put8(panels_owned, 1);
    }
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        bzero(panel(slot), 0xbec);
    const bool gear = (a0 & 0xffU) == 2;
    const auto xs = gear ? 0x801ea098U : 0x801ea054U;
    const auto ys = gear ? 0x801ea120U : 0x801ea0dcU;
    const auto layout = gear ? 1U : 0U;
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto block = panel(slot);
        const auto character = u8(u32(at(state_party)) + party_ids + slot);
        if (character == 0xff) {
            put8(block + 0xbe7, 0);
            continue;
        }
        if (gear && u8(character_record(character) + character_gear) == 0xff)
            continue;
        build_status_panel(block, character, slot, xs, ys, layout);
    }
    put8(u32(at(state_party)) + 0x46, 1);
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto top = 0x30 + slot * 56;
        const auto marks = u32(at(target_marks_block));
        const auto quad = marks + (slot * 2 + u8(marks + 0x148 + slot)) * 0x28;
        put16(quad + 8, 0x90);
        put16(quad + 0xa, top);
        put16(quad + 0x10, 0xa0);
        put16(quad + 0x12, top);
        put16(quad + 0x18, 0x90);
        put16(quad + 0x1a, top + 0x10);
        put16(quad + 0x20, 0xa0);
        put16(quad + 0x22, top + 0x10);
    }
}

// 801db920: use the item at visible entry `a1` of scroll row `a0`. Only an
// item with use flag 80 (and, with flag 20, 80059171 set) opens target
// selection: a window (801d397c), the list, description and status panels
// (redrawn after each use), and each frame the marked targets: the whole
// party (item +4 bit 0) or the target cursor's slot. Left/right (1, 3) move
// the cursor (801d9704), 4 uses the item on the marked targets (use_item,
// 801dbba4..801dbc90), 5 cancels; running out of the item ends it. Returns
// the last marked targets (0 when cancelled or unusable).
std::uint32_t Overlay::use_item_on_targets(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x68);
    const auto row = a0;
    const auto entry = a1;
    const auto target = u8(at(item_target));
    put8(panels_owned, 0);
    const auto index = row * 2 + entry;
    const auto item = u32(u32(at(state_tables)) + item_table) + u8(item_ids + index) * 16;
    const auto use = u8(item + 6);
    std::uint32_t targets = 0;
    if ((use & 0x80U) != 0) {
        targets = 1;
        if ((use & 0x20U) != 0)
            targets = u8(flag20_gate) != 0 ? 1U : 0U;
    }
    const bool whole_party = (u16(item + 4) & 1U) != 0;
    if ((targets & 0xffffU) == 0)
        return targets & 0xffU;
    open_window(2, 0x10, 0xe, 0x90, 0xb0, 0, 0, 4, 0);
    const auto marks = [&] { return u32(at(target_marks_block)) + 0x140; };
    const auto held = item_counts + index;
    bool redraw = true;
    while (true) {
        menu_frame();
        targets = 0;
        if (redraw) {
            redraw = false;
            build_item_list(row);
            show_item_description(entry, row);
            show_item_screen_texts(1);
            build_target_panels(0);
        }
        put8(marks() + 2, 0);
        put8(marks() + 1, 0);
        put8(marks(), 0);
        if (whole_party) {
            for (std::uint32_t slot = 0; slot < 3; ++slot) {
                if (u8(u32(at(state_party)) + party_ids + slot) == 0xff)
                    continue;
                targets |= 1U << slot;
                put8(marks() + slot, 1);
            }
        } else {
            targets = 1U << (target & 31U);
            put8(marks() + target, 1);
        }
        put8(u32(at(state_party)) + 0x2f, 1);
        if (u8(held) == 0)
            break;
        const auto code = u8(at(input_code));
        if (code == 1 || code == 3)
            missing("use_item", 0x801dbc84, "symbol:menu-801d9704",
                    "Moving the target cursor (801d9704, outside the census) is not recovered");
        if (code == 4) {
            Menu menu{*program.menu, program.resident.sound, [&] { entering_sound(); }};
            use_item(menu, index, targets);
            redraw = true;
        } else if (code == 5) {
            targets = 0;
            break;
        }
    }
    put8(marks() + 2, 0);
    put8(marks() + 1, 0);
    put8(marks(), 0);
    show_item_screen_texts(0);
    put8(u32(at(state_party)) + 0x46, 0);
    menu_frame();
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        release(u32(at(status_panels + slot * 4)), 0x801dbcf0);
    put8(panels_owned, 0);
    release_text_blocks(2);
    return targets & 0xffU;
}

// 801dbdb4: the item list's scroll extent from the last held inventory index
// (A1 is kept when no id is set): up to 16 entries fit (row 74, no
// scrolling); otherwise row 4a, (last - 16) / 2 + 1 scroll rows and a bar
// step of 1068 / rows.
void Overlay::measure_item_list(std::uint32_t a0, std::uint32_t a1) {
    static_cast<void>(a0); // A0 is the loop counter; its entry value is unused.
    auto last = a1;
    for (std::uint32_t i = 0; i < item_count_limit; ++i)
        if (u8(item_ids + i) != 0)
            last = i;
    if (static_cast<std::int32_t>(last) < 16) {
        put16(list_row_y, 0x74);
        put32(scroll_rows, 0);
        put16(scroll_step, 0);
        return;
    }
    const auto rows = (static_cast<std::int32_t>(last) - 16) / 2 + 1;
    put16(list_row_y, 0x4a);
    put32(scroll_rows, static_cast<std::uint32_t>(rows));
    put16(scroll_step, static_cast<std::uint32_t>(0x1068 / rows));
}

// 801dbe54: the item screen. Each frame redraws the list when the scroll row
// changed (801da5bc, scroll bar 801d3344), the cursor (801db0a8) and the
// description when the cursor moved (801da9a8), the windows once, and the
// selection mark. Input (jump table 801c50fc): 0/1 right/down and 2/3
// left/up move the cursor over two columns of eight rows, scrolling at the
// edges; 9/a page by eight rows; 4 selects an entry, uses it when selected
// again (801db920) or swaps it with the selected one (801dbd4c); 5 clears
// the selection or leaves. Returns 1.
std::uint32_t Overlay::item_screen() {
    const auto stack_frame = enter(0x50);
    constexpr std::uint32_t none = 0xff;
    bool running = true;
    bool windows = true;
    std::int32_t scroll = 0;
    std::uint32_t shown_scroll = none;
    std::int32_t cursor = 0;
    std::uint32_t shown_cursor = none;
    std::uint32_t selected = none;
    open_item_screen();
    bool held = false;
    for (std::uint32_t i = 0; i < item_count_limit; ++i)
        held = held || u8(item_ids + i) != 0;
    if (!held)
        missing("item_screen", 0x801dbea0, "symbol:stale-a1-801dbdb4",
                "An empty inventory keeps 801dbdb4's stale A1 (left by 801da4a8)");
    measure_item_list(0, 0);
    open_cursor_sprite(0);
    open_cursor_sprite(1);
    const auto last_row = [&] { return s32(scroll_rows); };
    const auto position = [&] { return static_cast<std::uint32_t>(scroll * 2 + cursor); };
    do {
        menu_frame();
        if (static_cast<std::uint32_t>(scroll) != shown_scroll) {
            build_item_list(static_cast<std::uint32_t>(scroll));
            shown_scroll = static_cast<std::uint32_t>(scroll);
            // Signed product, divided by 100 toward zero.
            const auto step =
                static_cast<std::int32_t>(u16(scroll_step) * static_cast<std::uint32_t>(scroll));
            draw_screen_title(0xc, static_cast<std::uint32_t>(step / 100 + 18), u16(list_row_y));
        }
        draw_cursor_sprite(static_cast<std::uint32_t>(cursor), static_cast<std::uint32_t>(scroll),
                           0, 0);
        if (static_cast<std::uint32_t>(cursor) != shown_cursor) {
            show_item_description(static_cast<std::uint32_t>(cursor),
                                  static_cast<std::uint32_t>(scroll));
            shown_cursor = static_cast<std::uint32_t>(cursor);
        }
        if (windows) {
            open_window(3, 0xc, 0xa, 0x124, 0x84, 0, 1, 4, 1);
            open_window(4, 8, 0x8f, 0x130, 0x22, 0, 1, 4, 0);
            windows = false;
            zoom_in();
            slide_party_panels(0, 0);
        }
        draw_cursor_sprite(selected, static_cast<std::uint32_t>(scroll), 1, 1);
        switch (u8(at(input_code))) {
        case 0: // one entry right
            if (cursor + 1 < 16) {
                cursor += 1;
            } else if (last_row() < ++scroll) {
                --scroll;
            } else {
                cursor = 14;
            }
            shown_cursor = none;
            break;
        case 1: // one row down
            if (cursor + 2 < 16)
                cursor += 2;
            else if (last_row() < ++scroll)
                --scroll;
            shown_cursor = none;
            break;
        case 2: // one entry left
            if (cursor - 1 >= 0) {
                cursor -= 1;
            } else if (--scroll < 0) {
                ++scroll;
            } else {
                cursor = 1;
            }
            shown_cursor = none;
            break;
        case 3: // one row up
            if (cursor - 2 >= 0)
                cursor -= 2;
            else if (--scroll < 0)
                ++scroll;
            shown_cursor = none;
            break;
        case 4:
            if (selected == none) {
                selected = position();
            } else if (position() == selected) {
                const auto used = use_item_on_targets(static_cast<std::uint32_t>(scroll),
                                                      static_cast<std::uint32_t>(cursor));
                selected = none;
                if ((used & 0xffU) != 0) {
                    shown_scroll = none;
                    shown_cursor = none;
                }
            } else {
                missing("item_screen", 0x801dc048, "symbol:menu-801dbd4c",
                        "Swapping two inventory entries (801dbd4c, outside the census) is not "
                        "recovered");
            }
            break;
        case 5:
            if (selected == none)
                running = false;
            else
                selected = none;
            break;
        case 9: // eight rows down
            scroll += 8;
            if (last_row() < scroll)
                scroll = last_row();
            shown_cursor = none;
            break;
        case 0xa: // eight rows up
            scroll -= 8;
            if (scroll < 0)
                scroll = 0;
            shown_cursor = none;
            break;
        default: // 6..8 and codes above a
            break;
        }
    } while (running);
    clear_markers();
    release_cursor_sprite(0);
    release_cursor_sprite(1);
    clear_bytes(8, u32(at(state_party)) + party_text);
    return 1;
}

// 801e31c0: apply consumable `a2` to character `a1` with table directory `a0`
// (menu::apply_item_effect); nonzero when an HP or EP item found nothing to
// restore.
std::uint32_t Overlay::apply_consumable(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x30);
    Menu menu{*program.menu, program.resident.sound};
    return apply_item_effect(menu, a0, a1, a2);
}

} // namespace xem::reconstruction::menu
