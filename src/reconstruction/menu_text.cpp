// Menu overlay 82f84a24... (801c5000): label, text and number drawing
// helpers. Text labels are records of 0x80 bytes: two POLY_FT4 packets (+0,
// +28, one per draw buffer), a quad of four SVECTORs (+50) for the 3D
// labels, the text image rectangle (+70), the bit-plane flag (+7c), the
// buffer that was drawn (+7d), the text's width in pixels (+7e) and +7f.
// Sprite rows are built from the menu's sprite sheet (state +2dc) through
// the resident sheet routine 8002675c.
#include "xem/reconstruction/menu_overlay.hpp"
#include "xem/reconstruction/resident_text.hpp"

namespace xem::reconstruction::menu {
namespace {
// Menu state fields (offsets from *800625a0).
constexpr std::uint32_t digits = 0x31c;         // nine decimal digits, ff for a blank
constexpr std::uint32_t sprite_sheet = 0x2dc;   // the menu's sprite sheet
constexpr std::uint32_t label_messages = 0x2e0; // offset table of label texts
constexpr std::uint32_t buffer_index = 0x308;   // 0/1: the draw buffer being built
constexpr std::uint32_t redraw_flags = 0x33c;   // block of per-part update bytes
constexpr std::uint32_t row_set = 0x336;        // u8: selects the row table's set
constexpr std::uint32_t row_cursor = 0x338;     // u8: the highlighted row
constexpr std::uint32_t row_count = 0x33a;      // u8: rows drawn by 801e86c8
constexpr std::uint32_t highlight_block = 0x348;
constexpr std::uint32_t screen_images = 0x350; // sprite rows (+1188.. counts)
constexpr std::uint32_t row_block = 0x354;     // sprite rows (+1400.. counts)
constexpr std::uint32_t windows = 0x364;       // window records, one word each
constexpr std::uint32_t text_image = 0x558;    // the label text image buffer

// Overlay tables (read at their original addresses).
constexpr std::uint32_t highlight_x = 0x801e9a00; // words: x of each position
constexpr std::uint32_t highlight_y = 0x801e9a2c; // words: y of each position
constexpr std::uint32_t row_sprites = 0x801ea1ec; // sprite id pairs, 8 per set; ffff empty
constexpr std::uint32_t name_x = 0x801ea578;      // halfwords per two slots
constexpr std::uint32_t name_y = 0x801ea5c4;

// Resident text CLUT ids of the two bit planes.
constexpr std::uint32_t text_clut_even = 0x800595d4;
constexpr std::uint32_t text_clut_odd = 0x80059414;
// Persistent game data: the character names (40 bytes apart, two lines of 20).
constexpr std::uint32_t character_names = 0x8006d634;

// Label record fields.
constexpr std::uint32_t label_stride = 0x80;
constexpr std::uint32_t label_quad = 0x50;
constexpr std::uint32_t label_rect = 0x70;
constexpr std::uint32_t label_plane = 0x7c;
constexpr std::uint32_t label_drawn = 0x7d;
constexpr std::uint32_t label_width = 0x7e;
constexpr std::uint32_t label_7f = 0x7f;
constexpr std::uint32_t packet_bytes = 0x28;

// A sprite row: `count` words at `counter` (block + offset) advance by each
// sprite's part count; packets lie at block + base + count * 50.
constexpr std::uint32_t sprite_x = 0xa0, sprite_y = 0x96, sprite_scale = 0x1000;
} // namespace

// 801c80b8: split `value` into nine decimal digits (state +31c.. from the
// hundred-millions down) and blank the leading zeros with ff (the last digit
// stays).
void Overlay::split_decimal_digits(std::uint32_t a0) {
    auto value = a0;
    auto divisor = 100000000U;
    for (std::uint32_t i = 0; i < 9; ++i) {
        const auto digit = value / divisor;
        value %= divisor;
        divisor /= 10;
        put8(at(digits + i), digit);
    }
    for (std::uint32_t i = 1; i < 9; ++i) {
        const auto here = at(i);
        if (u8(here + digits) != 0) {
            if (u8(here + digits - 1) == 0)
                put8(here + digits - 1, 0xff);
            return;
        }
        put8(here + digits - 1, 0xff);
    }
}

// 801c851c(quad, x, y, w, h): four SVECTORs (x, y, 0) of a screen rectangle,
// centred on the screen (x - a0, y - 70): top left, top right, bottom left,
// bottom right.
void Overlay::set_screen_quad_vectors(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                      std::uint32_t a3, std::uint32_t a4) {
    const auto left = a1 - 0xa0;
    const auto top = a2 - 0x70;
    const auto right = a1 + a3 - 0xa0;
    const auto bottom = a2 + a4 - 0x70;
    put16(a0 + 0x0, left);
    put16(a0 + 0x2, top);
    put16(a0 + 0x4, 0);
    put16(a0 + 0x8, right);
    put16(a0 + 0xa, top);
    put16(a0 + 0xc, 0);
    put16(a0 + 0x10, left);
    put16(a0 + 0x14, 0);
    put16(a0 + 0x18, right);
    put16(a0 + 0x1c, 0);
    put16(a0 + 0x12, bottom);
    put16(a0 + 0x1a, bottom);
}

// 801d1ee0(position, outline): the highlight sprite (sheet sprite 108) at
// the position's x, y (801e9a00, 801e9a2c) into the highlight block (state
// +348); with `outline` also the block's frame (a POLY_F4 per buffer at +50,
// two line strips per buffer at +c8 and +f8) around the text area 20 right,
// 36..20 above, as wide as block +15b; then the block's buffer bytes and
// the redraw flag +3.
void Overlay::draw_highlight(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x30);
    const auto x_entry = highlight_x + a0 * 4;
    const auto y_entry = highlight_y + a0 * 4;
    static_cast<void>(resident::sheet_quads(*this, u32(at(sprite_sheet)), 0x108,
                                            u32(at(highlight_block)), u32(at(buffer_index)),
                                            u32(x_entry), u32(y_entry), 0x1000));
    put8(u32(at(highlight_block)) + 0x158, u8(at(buffer_index)));
    if ((a1 & 0xffU) == 0)
        return;
    const auto block = [&] { return u32(at(highlight_block)); };
    const auto buffer = [&] { return u32(at(buffer_index)); };
    const auto x = [&] { return u16(x_entry); };
    const auto y = [&] { return u16(y_entry); };
    const auto left = [&] { return x() + 0x14; };
    const auto right = [&] { return x() + u8(block() + 0x15b) + 0x14; };
    const auto top = [&] { return y() - 0x24; };
    const auto bottom = [&] { return y() - 0x14; };
    // The POLY_F4 (0x24 per buffer): corners from +58.
    const auto fill = [&](std::uint32_t offset, std::uint32_t value) {
        put16(buffer() * 0x24 + block() + offset, value);
    };
    fill(0x58, left());
    fill(0x5a, top());
    fill(0x60, right());
    fill(0x62, top());
    fill(0x68, left());
    fill(0x6a, bottom());
    fill(0x70, right());
    fill(0x72, bottom());
    // Two LINE_F3 strips (0x18 per buffer).
    const auto line = [&](std::uint32_t offset, std::uint32_t value) {
        put16(buffer() * 0x18 + block() + offset, value);
    };
    line(0xd0, left());
    line(0xd2, top());
    line(0xd4, right());
    line(0xd6, top());
    line(0xd8, right());
    line(0xda, bottom());
    line(0x100, left());
    line(0x102, top());
    line(0x104, left());
    line(0x106, bottom());
    line(0x108, right());
    line(0x10a, bottom());
    put8(block() + 0x159, u8(at(buffer_index)));
    put8(u32(at(redraw_flags)) + 3, 1);
}

// 801e53cc(window): prepare window `window`'s packets (state +364 record):
// clear its redraw bytes (+20, +27 + window), two grey (68) semi-transparent
// POLY_G4 at +4b0 with draw modes at +4f8 (page at state +47c, +480, window
// 0, 0, 100, 100), and per buffer pair four textured quads each at +140,
// +1e0, +280 and +320 (white, raw texture) with the pages and CLUTs of the
// four texture descriptors at state +470, +488, +4a0 and +4b8.
void Overlay::prepare_window_packets(std::uint32_t a0) {
    const auto stack_frame = enter(0x40);
    const auto index = a0 & 0xffU;
    const auto record = u32(state() + index * 4 + windows);
    auto f = frame(0x40);
    const auto rect = f[0x18];
    put16(rect + 2, 0);
    put16(rect + 0, 0);
    put16(rect + 6, 0x100);
    put16(rect + 4, 0x100);
    put8(u32(at(redraw_flags)) + index + 0x20, 0);
    put8(u32(at(redraw_flags)) + index + 0x27, 0);
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto shade = record + i * 0x24;
        set_poly_g4(shade + 0x4b0);
        for (const auto corner : {0x4b4U, 0x4bcU, 0x4c4U, 0x4ccU})
            for (std::uint32_t c = 0; c < 3; ++c)
                put8(shade + corner + c, 0x68);
        set_semi_trans(shade + 0x4b0, 1);
        const auto page = get_tpage(0, 0, s32(at(0x47c)), s32(at(0x480)));
        set_draw_mode(record + i * 0xc + 0x4f8, 0, 0, page & 0xffffU, rect);
    }
    // Texture descriptors: page depth, CLUT x, y, page x, y (state words).
    struct Quad {
        std::uint32_t packet;
        std::uint32_t descriptor;
    };
    for (std::uint32_t i = 0; i < 4; ++i) {
        const auto base = record + i * packet_bytes;
        for (const auto [packet, descriptor] :
             {Quad{0x140, 0x470}, Quad{0x1e0, 0x488}, Quad{0x280, 0x4a0}, Quad{0x320, 0x4b8}}) {
            set_poly_ft4(base + packet);
            set_shade_tex(base + packet, 1);
            for (std::uint32_t c = 4; c < 7; ++c)
                put8(base + packet + c, 0xff);
            put16(base + packet + 0x16, get_tpage(u32(at(descriptor)), 0, s32(at(descriptor + 0xc)),
                                                  s32(at(descriptor + 0x10))));
            put16(base + packet + 0xe, get_clut(s32(at(descriptor + 4)), s32(at(descriptor + 8))));
        }
    }
}

// 801e7c50(label, slot, row, mode): both buffers' packets of a text label
// (801e927c). Mode 0: the label's text lies in the image at (140, 0): u
// 0 or 80 by bit 0 of slot / 2, v (slot + row) / 4 * 13, the plane by bit 0
// of slot. Otherwise at (180, 80): u slot bit 0 * 60, v slot / 2 * 13 + row,
// plane (mode & 7f) - 1; without mode bit 80 the quad is also
// semi-transparent (page abr 1) and dark (20). The quad is 13 high and as
// wide as the text (+7e); the CLUT follows the plane.
void Overlay::set_label_packets(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                std::uint32_t a3) {
    const auto stack_frame = enter(0x40);
    const auto label = a0;
    const auto slot = static_cast<std::int32_t>(a1);
    const auto parity = a1 & 1U;
    const auto half = static_cast<std::uint32_t>(slot / 2);
    const auto column = (half & 1U) << 7U;
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto packet = label + i * packet_bytes;
        std::uint32_t semi = 0;
        init_text_quad(packet);
        std::uint32_t u = 0, v = 0;
        if ((a3 & 0xffU) == 0) {
            put8(label + label_plane, parity);
            put16(packet + 0x16, get_tpage(0, 0, 0x140, 0));
            u = column;
            v = static_cast<std::uint32_t>(static_cast<std::int32_t>(a1 + a2) / 4) * 13;
            put8(packet + 0xc, u);
            put8(packet + 0xd, v);
            put8(packet + 0x14, u + u8(label + label_width));
            put8(packet + 0x15, v);
            put8(packet + 0x1c, u);
            put8(packet + 0x1d, v + 13);
            put8(packet + 0x24, u + u8(label + label_width));
            put8(packet + 0x25, v + 13);
        } else {
            if ((a3 & 0x80U) == 0) {
                semi = 0x20;
                set_semi_trans(packet, 1);
                put8(packet + 4, semi);
                put8(packet + 5, semi);
                put8(packet + 6, semi);
            }
            put8(label + label_plane, (a3 & 0x7fU) + 0xff);
            put16(packet + 0x16, semi | get_tpage(0, 0, 0x180, 0x80));
            u = parity * 0x60;
            v = half * 13 + a2;
            put8(packet + 0xc, u);
            put8(packet + 0xd, v);
            put8(packet + 0x15, v);
            put8(packet + 0x1c, u);
            put8(packet + 0x1d, v + 13);
            put8(packet + 0x14, u + u8(label + label_width));
            put8(packet + 0x25, v + 13);
            put8(packet + 0x24, u + u8(label + label_width));
        }
        put16(packet + 0xe, u16(u8(label + label_plane) != 0 ? text_clut_odd : text_clut_even));
    }
    put8(label + label_7f, 0);
}

// 801e7e68(labels, ids, row, count): lay out `count` label texts (pairs of
// message ids, state +2e0 table) two per 1c x d image cell at (140 + 20 *
// bit 1 of i, (i + row) / 4 * 13) in the text image buffer (state +558),
// the first in plane 0 and the second in plane 1; set up both labels'
// packets (801e7c50) and load the cell into VRAM.
void Overlay::layout_label_pairs(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                 std::uint32_t a3) {
    const auto stack_frame = enter(0x40);
    const auto count = static_cast<std::int32_t>(a3);
    auto label = a0;
    auto ids = a1;
    const auto glyphs = [&](std::uint32_t window) { program.dialogue_glyphs(window); };
    for (std::int32_t i = 0; i < count; i += 2) {
        const auto slot = static_cast<std::uint32_t>(i);
        const auto first = resident::offset_table_entry(*this, u32(at(label_messages)), u8(ids));
        put8(label + label_width,
             resident::layout_text_line(*this, first, u32(at(text_image)), 0x18, 0, glyphs));
        const auto second =
            resident::offset_table_entry(*this, u32(at(label_messages)), u8(ids + 1));
        put8(label + label_stride + label_width,
             resident::layout_text_line(*this, second, u32(at(text_image)), 0x18, 1, glyphs));
        const auto rect = label + label_rect;
        // Bit 1 of the (even) slot picks the column: x 140 or 160.
        put16(rect, ((slot << 4U) & 0x20U) + 0x140);
        put16(rect + 2, static_cast<std::uint32_t>(static_cast<std::int32_t>(slot + a2) / 4) * 13);
        put16(rect + 4, 0x1c);
        put16(rect + 6, 0xd);
        // The rectangle is copied to the second label (lwl/lwr, swl/swr).
        put32(label + label_stride + label_rect, u32(rect));
        put32(label + label_stride + label_rect + 4, u32(rect + 4));
        set_label_packets(label, slot, a2, 0);
        set_label_packets(a0 + slot * label_stride + label_stride, slot + 1, a2, 0);
        load_image(rect, u32(at(text_image)));
        draw_sync();
        label += 2 * label_stride;
        ids += 2;
    }
}

// 801e8018(count, labels, ids): 801e7e68(labels, ids, 4, count).
void Overlay::layout_labels_row4(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x18);
    layout_label_pairs(a1, a2, 4, a0 & 0xffU);
}

// 801e8044(count, bytes): clear `count` bytes (signed address compare).
void Overlay::clear_bytes(std::uint32_t a0, std::uint32_t a1) {
    const auto count = a0 & 0xffU;
    if (count == 0)
        return;
    const auto end = a1 + count;
    auto address = a1;
    do
        put8(address++, 0);
    while (static_cast<std::int32_t>(address) < static_cast<std::int32_t>(end));
}

// 801e8070(count, labels, unused, offsets, shown, index, position, mode):
// place label `index` by mode (jump table 801c5278): 0 clears `count` shown
// bytes and puts the buffer's quad at the position's x (801e9a00 entry
// position + index) + 16h + offsets[index], y - 22h..-15h; 4 puts it at
// (4c, 12)..(4c + width, 1f) (the width of the first label); 1, 2, 3, 5 and 6
// set the label's 3D quad (801c851c, 13 high, as wide as its text) at x, y
// from overlay tables (1: 801e9ec4[index], 801e9ee4; 2: 801e9ee8[index],
// 801e9f28[position]; 5: as 2 from 801e9f08; 3: 18, 801e9f30[position]; 6:
// 801e9f68[index], 801e9f70[index]). Other modes place nothing. Then the
// label's drawn buffer and shown[index] = 1.
void Overlay::place_label(std::uint32_t a0, std::uint32_t a1, std::uint32_t, std::uint32_t a3,
                          std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7) {
    const auto stack_frame = enter(0x30);
    const auto labels = a1;
    const auto shown = a4;
    const auto index = a5 & 0xffU;
    const auto position = a6 & 0xffU;
    const auto mode = a7 & 0xffU;
    const auto label = labels + index * label_stride;
    const auto packet = [&] { return u32(at(buffer_index)) * packet_bytes + label; };
    const auto place_quad = [&](std::uint32_t x, std::uint32_t y) {
        set_screen_quad_vectors(label + label_quad, x, y, u8(label + label_width), 0xd);
    };
    switch (mode) {
    case 0: {
        clear_bytes(a0 & 0xffU, shown);
        const auto entry = (position + index) * 4;
        const auto x = [&] { return u16(highlight_x + entry) + u16(index * 4 + a3) + 0x16; };
        const auto y = [&] { return u16(highlight_y + entry); };
        put16(packet() + 0x8, x());
        put16(packet() + 0xa, y() - 0x22);
        put16(packet() + 0x10, u8(label + label_width) + x());
        put16(packet() + 0x12, y() - 0x22);
        put16(packet() + 0x18, x());
        put16(packet() + 0x1a, y() - 0x15);
        put16(packet() + 0x20, u8(label + label_width) + x());
        put16(packet() + 0x22, y() - 0x15);
        break;
    }
    case 1:
        place_quad(u16(0x801e9ec4 + index * 4), u16(0x801e9ee4));
        break;
    case 2:
    case 5: {
        const auto from = mode == 5 ? 8U : 0U;
        place_quad(u16(0x801e9ee8 + (from + index) * 4), u16(0x801e9f28 + position * 4));
        break;
    }
    case 3:
        place_quad(0x18, u16(0x801e9f30 + position * 4));
        break;
    case 4:
        put16(packet() + 0x8, 0x4c);
        put16(packet() + 0xa, 0x12);
        put16(packet() + 0x10, u8(labels + label_width) + 0x4c);
        put16(packet() + 0x12, 0x12);
        put16(packet() + 0x18, 0x4c);
        put16(packet() + 0x1a, 0x1f);
        put16(packet() + 0x20, u8(labels + label_width) + 0x4c);
        put16(packet() + 0x22, 0x1f);
        break;
    case 6:
        place_quad(u16(0x801e9f68 + index * 4), u16(0x801e9f70 + index * 4));
        break;
    default:
        break;
    }
    put8(label + label_drawn, u8(at(buffer_index)));
    put8(shown + index, 1);
}

// 801e8474(steps, pairs): reveal a two-column sprite list one row per two
// frames: step k (1..steps) rebuilds k first-column sprites (pairs[i].first,
// screen images block +0, count +1188) unless k is the last step, and from
// step 2 the k - 1 second-column sprites (pairs[i].second, +8c0, count
// +118c), stamping each column's buffer (+1190, +1191); then two menu frames
// (801c7bf4).
void Overlay::reveal_sprite_columns(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x50);
    const auto steps = static_cast<std::int32_t>(a0);
    const auto pairs = a1;
    const auto block = [&] { return u32(at(screen_images)); };
    put8(block() + 0x1192, 0);
    put8(block() + 0x1193, 0);
    put8(u32(at(redraw_flags)) + 9, 1);
    const auto row = [&](std::uint32_t counter, std::uint32_t base, std::uint32_t id) {
        const auto parts = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), id, u32(block() + counter) * 0x50 + base + block(),
            u32(at(buffer_index)), sprite_x, sprite_y, sprite_scale);
        put32(block() + counter, parts + u32(block() + counter));
    };
    for (std::int32_t k = 1, second = 0; k <= steps; ++k, ++second) {
        if (k != steps) {
            put32(block() + 0x1188, 0);
            for (std::int32_t i = 0; i < k; ++i)
                row(0x1188, 0, u32(pairs + static_cast<std::uint32_t>(i) * 8));
            put8(block() + 0x1190, u8(at(buffer_index)));
        }
        put32(block() + 0x118c, 0);
        if (k != 1) {
            for (std::int32_t i = 0; i < second; ++i)
                row(0x118c, 0x8c0, u32(pairs + static_cast<std::uint32_t>(i) * 8 + 4));
            put8(block() + 0x1191, u8(at(buffer_index)));
        }
        for (std::uint32_t frame_step = 0; frame_step < 2; ++frame_step)
            menu_frame();
    }
}

namespace {
// Row table entry `i` of set `set` (801ea1ec: eight id pairs per set).
constexpr std::uint32_t row_entry(std::uint32_t set, std::uint32_t i) {
    return row_sprites + (i * 2 + set * 8) * 4;
}
} // namespace

// 801e86c8(set): reveal the row list of set `set` + state +336 (801ea1ec)
// over four steps: step k builds the first k rows' first sprites (row block
// +0, count +1400; ffff marks an empty row, which also stops the frame
// waits from then on) counting them in state +33a, then their second sprites
// (+500, count +1404), each column stamped with its buffer (+1408, +1409)
// and followed by two menu frames while no row was empty.
void Overlay::reveal_row_list(std::uint32_t a0) {
    const auto stack_frame = enter(0x50);
    const auto set = a0 & 0xffU;
    const auto block = [&] { return u32(at(row_block)); };
    put32(block() + 0x1400, 0);
    put32(block() + 0x1404, 0);
    put8(u32(at(redraw_flags)) + 10, 1);
    bool waits = true;
    const auto row = [&](std::uint32_t counter, std::uint32_t base, std::uint32_t id) {
        const auto parts = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), id, u32(block() + counter) * 0x50 + base + block(),
            u32(at(buffer_index)), sprite_x, sprite_y, sprite_scale);
        put32(block() + counter, parts + u32(block() + counter));
    };
    const auto two_frames = [&] {
        for (std::uint32_t frame_step = 0; frame_step < 2; ++frame_step)
            menu_frame();
    };
    for (std::uint32_t k = 1; k < 5; ++k) {
        put32(block() + 0x1400, 0);
        put8(at(row_count), 0);
        for (std::uint32_t i = 0; i < k; ++i) {
            const auto entry = row_entry(set + u8(at(row_set)), i);
            if (u32(entry) == 0xffff) {
                waits = false;
                continue;
            }
            row(0x1400, 0, u32(entry));
            put8(at(row_count), u8(at(row_count)) + 1);
        }
        put8(block() + 0x1408, u8(at(buffer_index)));
        if (waits)
            two_frames();
        put32(block() + 0x1404, 0);
        for (std::uint32_t i = 0; i < k; ++i) {
            const auto entry = row_entry(set + u8(at(row_set)), i);
            if (u32(entry) != 0xffff)
                row(0x1404, 0x500, u32(entry + 4));
        }
        put8(block() + 0x1409, u8(at(buffer_index)));
        if (waits)
            two_frames();
    }
}

// 801e8978(count, selected, pairs): rebuild a two-column sprite list at
// once (screen images block, counts +1188, +118c): the first sprite of the
// selected pair is its highlighted form (id + 13); stamp both columns'
// buffers, draw the highlight at `selected` with its frame (801d1ee0) and
// set redraw flag +4.
void Overlay::build_sprite_columns(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x48);
    const auto count = a0 & 0xffU;
    const auto selected = a1 & 0xffU;
    const auto block = [&] { return u32(at(screen_images)); };
    put32(block() + 0x1188, 0);
    put32(block() + 0x118c, 0);
    const auto row = [&](std::uint32_t counter, std::uint32_t base, std::uint32_t id) {
        const auto parts = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), id, u32(block() + counter) * 0x50 + base + block(),
            u32(at(buffer_index)), sprite_x, sprite_y, sprite_scale);
        put32(block() + counter, parts + u32(block() + counter));
    };
    auto pair = a2;
    for (std::uint32_t i = 0; i < count; ++i, pair += 8) {
        row(0x1188, 0, i == selected ? u32(pair) + 13 : u32(pair));
        row(0x118c, 0x8c0, u32(pair + 4));
    }
    put8(block() + 0x1190, u8(at(buffer_index)));
    put8(block() + 0x1191, u8(at(buffer_index)));
    draw_highlight(selected, 1);
    put8(u32(at(redraw_flags)) + 4, 1);
}

// 801e8b4c(set): rebuild the state +33a rows of set `set` + state +336 at
// once (row block, counts +1400, +1404), the row at state +338 highlighted
// (first id + 13); stamp both columns' buffers, draw the highlight at
// position row + 7 with its frame (801d1ee0) and set redraw flag +4.
void Overlay::build_row_list(std::uint32_t a0) {
    const auto stack_frame = enter(0x48);
    const auto set = a0 & 0xffU;
    const auto block = [&] { return u32(at(row_block)); };
    put32(block() + 0x1400, 0);
    put32(block() + 0x1404, 0);
    const auto row = [&](std::uint32_t counter, std::uint32_t base, std::uint32_t id) {
        const auto parts = resident::sheet_quads(
            *this, u32(at(sprite_sheet)), id, u32(block() + counter) * 0x50 + base + block(),
            u32(at(buffer_index)), sprite_x, sprite_y, sprite_scale);
        put32(block() + counter, parts + u32(block() + counter));
    };
    for (std::uint32_t i = 0; i < u8(at(row_count)); ++i) {
        const auto first = u32(row_entry(set + u8(at(row_set)), i));
        row(0x1400, 0, i == u8(at(row_cursor)) ? first + 13 : first);
        row(0x1404, 0x500, u32(row_entry(set + u8(at(row_set)), i) + 4));
    }
    put8(block() + 0x1408, u8(at(buffer_index)));
    put8(block() + 0x1409, u8(at(buffer_index)));
    draw_highlight(u8(at(row_cursor)) + 7, 1);
    put8(u32(at(redraw_flags)) + 4, 1);
}

// 801e8da8(character, slot): draw a character's name (game data 8006d634 +
// (character / 2) * 40: two lines of 20 bytes, planes 0 and 1; none for ff)
// into a cleared 3f6-byte image and load it as the 40x13 cell of slot / 2
// at (801ea578 entry + 180, 801ea5c4 entry).
void Overlay::load_character_name(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x30);
    auto f = frame(0x30);
    const auto image = allocate(0x3f6, 0, 0x801e8dcc);
    static_cast<void>(bzero(image, 0x3f6));
    const auto character = a0 & 0xffU;
    if (character != 0xff) {
        const auto name = (character >> 1U) * 40 + character_names;
        const auto glyphs = [&](std::uint32_t window) { program.dialogue_glyphs(window); };
        static_cast<void>(resident::layout_text_line(*this, name, image, 0x24, 0, glyphs));
        static_cast<void>(resident::layout_text_line(*this, name + 20, image, 0x24, 1, glyphs));
    }
    const auto rect = f[0x10];
    const auto entry = (a1 << 1U) & 0x1fcU;
    put16(rect, u16(name_x + entry) + 0x180);
    put16(rect + 4, 0x28);
    put16(rect + 6, 0xd);
    put16(rect + 2, u16(name_y + entry));
    load_image(rect, image);
    draw_sync();
    release(image, 0x801e8e84);
}

// 801e8eac(packet, style): a textured quad's shading: raw texture off, then
// 0 opaque grey (80), 1 semi-transparent (page abr 1) dark (21), 2 grey
// (80), 3 dark (21); other styles only turn raw texture off.
void Overlay::shade_quad(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x20);
    set_shade_tex(a0, 0);
    std::uint32_t shade = 0;
    switch (a1 & 0xffU) {
    case 0:
        set_semi_trans(a0, 0);
        shade = 0x80;
        break;
    case 1:
        put16(a0 + 0x16, u16(a0 + 0x16) | 0x20U);
        set_semi_trans(a0, 1);
        shade = 0x21;
        break;
    case 2:
        shade = 0x80;
        break;
    case 3:
        shade = 0x21;
        break;
    default:
        return;
    }
    put8(a0 + 4, shade);
    put8(a0 + 5, shade);
    put8(a0 + 6, shade);
}

// 801e8f60(window, dim): shade window `window`'s quads of the current buffer
// (+71c) with style 2, or 3 when `dim`: four at +0, +140, +1e0, +280 and
// +320, two at +410 (every other packet), and one at +3c0.
void Overlay::shade_window_quads(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x28);
    const auto style = (a1 & 0xffU) != 0 ? 3U : 2U;
    const auto record = [&] { return u32((a0 & 0xffU) * 4 + state() + windows); };
    struct Group {
        std::uint32_t base;
        std::uint32_t count;
    };
    for (const auto [base, count] : {Group{0, 4}, Group{0x140, 4}, Group{0x1e0, 4}, Group{0x280, 4},
                                     Group{0x320, 4}, Group{0x410, 2}})
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto window = record();
            shade_quad((i * 2 + u8(window + 0x71c)) * packet_bytes + base + window, style);
        }
    const auto window = record();
    shade_quad(u8(window + 0x71c) * packet_bytes + 0x3c0 + window, style);
}

// 801e91c4(packet): semi-transparent, shaded grey (80).
void Overlay::set_quad_translucent(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    set_semi_trans(a0, 1);
    set_shade_tex(a0, 0);
    put8(a0 + 4, 0x80);
    put8(a0 + 5, 0x80);
    put8(a0 + 6, 0x80);
}

// 801e920c(packet, x, y, u, v, w, h): a quad's corners at x, y of size w, h
// and its texture from u, v of the same size.
void Overlay::set_quad_rect(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                            std::uint32_t a4, std::uint32_t a5, std::uint32_t a6) {
    put16(a0 + 0x8, a1);
    put16(a0 + 0xa, a2);
    put16(a0 + 0x12, a2);
    put16(a0 + 0x18, a1);
    put8(a0 + 0xc, a3);
    put8(a0 + 0x1c, a3);
    put16(a0 + 0x10, a1 + a5);
    put16(a0 + 0x1a, a2 + a6);
    put16(a0 + 0x20, a1 + a5);
    put16(a0 + 0x22, a2 + a6);
    put8(a0 + 0xd, a4);
    put8(a0 + 0x14, a3 + a5);
    put8(a0 + 0x15, a4);
    put8(a0 + 0x1d, a4 + a6);
    put8(a0 + 0x24, a3 + a5);
    put8(a0 + 0x25, a4 + a6);
}

// 801e927c(packet): an opaque shaded grey (80) POLY_FT4.
void Overlay::init_text_quad(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    set_poly_ft4(a0);
    set_semi_trans(a0, 0);
    set_shade_tex(a0, 0);
    put8(a0 + 4, 0x80);
    put8(a0 + 5, 0x80);
    put8(a0 + 6, 0x80);
}

} // namespace xem::reconstruction::menu
