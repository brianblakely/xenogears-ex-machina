// Menu overlay 82f84a24... (801c5000): the per-frame screen drawing of modes 0
// and 2 (801d1b20, 801d1be8). Every shown window, panel and list links its
// prebuilt packets for the buffer being built into ordering-table entry 4 (or
// a panel's own entry); 3D panels project their corners through the loaded
// GTE matrices first (RotTransPers4/3, resident_gte.hpp).
#include "xem/reconstruction/menu_overlay.hpp"
#include "xem/reconstruction/resident_gte.hpp"
#include "xem/reconstruction/resident_text.hpp"

#include <array>

namespace xem::reconstruction::menu {
namespace {
// Menu state fields (offsets from *800625a0).
constexpr std::uint32_t draw_environment = 0x1d4; // current buffer's environment block
constexpr std::uint32_t frame_counter = 0x2d8;    // frames since last cleared
constexpr std::uint32_t sprite_sheet = 0x2dc;     // the menu sprite sheet (8002675c)
constexpr std::uint32_t buffer_index = 0x308;     // 0/1: the buffer being built
constexpr std::uint32_t file_screen_state = 0x32c;      // file screen block
constexpr std::uint32_t shown_flags = 0x33c;      // block of per-window "shown" bytes
constexpr std::uint32_t party_windows = 0x39c;    // three party window blocks
constexpr std::uint32_t slot_blocks = 0x3a8;      // 32 file-slot blocks
constexpr std::uint32_t panel_growth = 0x380;     // seven opening-panel records
constexpr std::uint32_t panel_blocks = 0x364;     // seven 3D panel blocks
constexpr std::uint32_t palette_step = 0x4cc;     // 0-5: file-slot palette row
constexpr std::uint32_t palette_timer = 0x4d0;    // frames within a palette row
constexpr std::uint32_t cursor_level = 0x4d4;     // pulsing cursor colour 4..7c
constexpr std::uint32_t file_screen_mode = 0x4d8; // 0 none, 2 file selection
constexpr std::uint32_t cursor_falling = 0x4d9;   // 1 while the cursor colour falls
// Overlay words: the file screen's slot for each cursor position (+4f7c).
constexpr std::uint32_t slot_of_cursor = 0x801e981c;
// Ordering-table entries in an environment block (+70, 16 words).
constexpr std::uint32_t ordering_table = 0x70;
constexpr std::uint32_t window_entry = 0x80; // entry 4: windows and panels

std::int32_t signed_word(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// A block pointer of the menu state.
std::uint32_t block(const Overlay &o, std::uint32_t slot) { return o.u32(o.at(slot)); }
std::uint32_t buffer(const Overlay &o) { return o.u32(o.at(buffer_index)); }
// Whether shown byte `flag` of the window-flag block is set.
bool shown(const Overlay &o, std::uint32_t flag) { return o.u8(block(o, shown_flags) + flag) != 0; }
// AddPrim (80043b48) into ordering-table entry 4 of the current buffer.
void draw(Overlay &o, std::uint32_t packet) {
    o.add_prim(o.u32(o.at(draw_environment)) + window_entry, packet);
}
// A window record: its packet pair at +0, corner SVECTORs at +50 and the
// buffer's packet index at +7d, projected by 801ce198.
void draw_window(Overlay &o, std::uint32_t record) {
    o.project_quads(1, record + 0x50, record, o.u8(record + 0x7d));
}
// The file screen's selected slot: the slot byte (+4fae) of the slot that
// the cursor (+4f7c) names through the overlay table.
std::uint32_t selected_slot_index(const Overlay &o) {
    return o.u32(slot_of_cursor + o.u32(block(o, file_screen_state) + 0x4f7c) * 4);
}
std::uint32_t selected_slot(const Overlay &o) {
    return o.u8(block(o, file_screen_state) + selected_slot_index(o) + 0x4fae);
}
// RotTransPers4 of the four consecutive SVECTORs at `vectors` into the
// vertices of the POLY_?T4/G4 packet (+8, +10, +18, +20); the depth cue and
// flag go to the caller's frame words `depth` and `flag`.
void project_quad(Overlay &o, std::uint32_t vectors, std::uint32_t packet, std::uint32_t depth,
                  std::uint32_t flag) {
    static_cast<void>(resident::rot_trans_pers4(
        o, o.program.resident.gte, vectors, vectors + 8, vectors + 0x10, vectors + 0x18, packet + 8,
        packet + 0x10, packet + 0x18, packet + 0x20, depth, flag));
}
// A panel block of the seven 3D panels.
std::uint32_t panel(const Overlay &o, std::uint32_t index) {
    return o.u32(o.at(panel_blocks) + (index & 0xffU) * 4);
}
// The texture corners of a panel's two edge pieces of this buffer (packets at
// `first` + buffer * 28 and 50 further): u0/u1 left/right, v0/v1 top/bottom.
void set_edge_uvs(Overlay &o, std::uint32_t block_address, std::uint32_t first, std::uint32_t u0,
                  std::uint32_t u1, std::uint32_t v0, std::uint32_t v1) {
    for (std::uint32_t k = 0; k < 2; ++k) {
        const auto packet = buffer(o) * 0x28 + block_address + first + k * 0x50;
        o.put8(packet + 0xc, u0);
        o.put8(packet + 0xd, v0);
        o.put8(packet + 0x14, u1);
        o.put8(packet + 0x15, v0);
        o.put8(packet + 0x1c, u0);
        o.put8(packet + 0x1d, v1);
        o.put8(packet + 0x24, u1);
        o.put8(packet + 0x25, v1);
    }
}
// Half of a 16-bit extent less the two 8-pixel corners, as the original's
// signed division by two.
std::uint32_t half_inner(std::uint32_t extent) {
    return static_cast<std::uint32_t>(signed_word((extent & 0xffffU) - 0x10) / 2);
}
} // namespace

// 801ce198: for `count` quads, project the four SVECTORs at `vectors` + 20*i
// (RotTransPers4) into packet `index` + 2*i of `packets` (28 bytes each) and
// link it into ordering-table entry 4.
void Overlay::project_quads(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x68);
    auto f = frame(0x68);
    auto index = a3;
    for (std::int32_t i = 0; i < signed_word(a0); ++i) {
        const auto packet = a2 + index * 0x28;
        project_quad(*this, a1 + static_cast<std::uint32_t>(i) * 0x20, packet, f[0x28], f[0x2c]);
        index += 2;
        draw(*this, packet);
    }
}

// 801ce2b4: link `count` packets of `packets` (28 bytes each, every other one
// from `index`: this buffer's) into ordering-table entry 4.
void Overlay::draw_packets(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x30);
    auto index = a2;
    for (std::int32_t i = 0; i < signed_word(a0); ++i) {
        draw(*this, a1 + index * 0x28);
        index += 2;
    }
}

// 801ce338: the header block (+348): its mode packet (+128, c bytes per
// buffer) and, while shown byte 4 is set, its packet (28 bytes per buffer).
void Overlay::draw_header() {
    const auto stack_frame = enter(0x18);
    const auto header = block(*this, 0x348);
    draw(*this, u8(header + 0x159) * 0xc + 0x128 + header);
    if (shown(*this, 4))
        draw(*this, u8(block(*this, 0x348) + 0x158) * 0x28 + block(*this, 0x348));
}

// 801ce3c8: while shown byte 2f is set, the four cursor packets of block +428
// whose enable byte (+140 + i) is set, each at index 2*i + its buffer byte
// (+148 + i).
void Overlay::draw_cursors() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x2f))
        return;
    for (std::uint32_t i = 0; i < 4; ++i) {
        const auto cursors = block(*this, 0x428);
        if (u8(cursors + i + 0x140) != 0)
            draw(*this, (i * 2 + u8(cursors + i + 0x148)) * 0x28 + cursors);
    }
}

// 801ce464: shown byte 5: block +340's list (count +320, index +324) and its
// frame packet (+2d0); shown byte 6: block +344's seven packets and the four
// at +230 (index +370).
void Overlay::draw_lists_340_344() {
    const auto stack_frame = enter(0x18);
    if (shown(*this, 5)) {
        const auto list = block(*this, 0x340);
        draw_packets(u32(list + 0x320), list, u8(list + 0x324));
        const auto again = block(*this, 0x340);
        draw(*this, u8(again + 0x324) * 0x28 + 0x2d0 + again);
    }
    if (shown(*this, 6)) {
        const auto list = block(*this, 0x344);
        draw_packets(7, list, u8(list + 0x370));
        const auto again = block(*this, 0x344);
        draw_packets(4, again + 0x230, u8(again + 0x370));
    }
}

// 801ce540: each of the three party windows (+39c + 4*i) shown by byte i:
// its two frame packets (+0 and +50 at the buffer index +1270) and its seven
// packet lists, counts at +1279, +1273..+1277 and +1271.
void Overlay::draw_party_windows() {
    const auto stack_frame = enter(0x20);
    struct List {
        std::uint16_t count;
        std::uint16_t packets;
    };
    static constexpr std::array<List, 7> lists{{{0x1279, 0xa0},
                                                {0x1273, 0xaf0},
                                                {0x1274, 0xbe0},
                                                {0x1275, 0xcd0},
                                                {0x1276, 0xd70},
                                                {0x1277, 0xe10},
                                                {0x1271, 0x910}}};
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto window = u32(at(party_windows) + i * 4);
        if (!shown(*this, i))
            continue;
        draw(*this, window + u8(window + 0x1270) * 0x28);
        draw(*this, window + u8(window + 0x1270) * 0x28 + 0x50);
        for (const auto &list : lists)
            draw_packets(u8(window + list.count), window + list.packets, u8(window + 0x1270));
    }
}

// 801ce660: while shown byte 7 is set, the twelve projected quad groups of
// block +358: each (count, SVECTORs, packets, buffer index); counts and
// indices are bytes of the block (a count of 0 is the fixed 1).
void Overlay::draw_status_quads() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 7))
        return;
    struct Group {
        std::uint16_t count; // byte offset, or 0 for a single quad
        std::uint16_t vectors;
        std::uint16_t packets;
        std::uint16_t index;
    };
    static constexpr std::array<Group, 12> groups{{{0, 0x1ea0, 0, 0x2ae0},
                                                   {0, 0x1ec0, 0x50, 0x2ae0},
                                                   {0x2aed, 0x2aa0, 0x1e00, 0x2aee},
                                                   {0x2aec, 0x1ee0, 0xa0, 0x2ae0},
                                                   {0x2ae7, 0x2780, 0x1630, 0x2ae0},
                                                   {0x2ae8, 0x2820, 0x17c0, 0x2ae0},
                                                   {0x2ae9, 0x28c0, 0x1950, 0x2ae0},
                                                   {0x2aea, 0x2960, 0x1ae0, 0x2ae0},
                                                   {0x2ae1, 0x2300, 0xaf0, 0x2ae0},
                                                   {0x2ae3, 0x23c0, 0xcd0, 0x2ae0},
                                                   {0x2ae5, 0x25c0, 0x11d0, 0x2ae0},
                                                   {0x2aeb, 0x2a00, 0x1c70, 0x2ae0}}};
    for (const auto &group : groups) {
        const auto status = block(*this, 0x358);
        project_quads(group.count == 0 ? 1U : u8(status + group.count), status + group.vectors,
                  status + group.packets, u8(status + group.index));
    }
}

// 801ce860: the seven rows of block +35c whose enable byte (+32ea + i) is set.
// While +32f2 is set a row first projects its highlight quad (SVECTORs +31e0 +
// 20*i, POLY_G4 at +2228 + 48*i, 24 per buffer by +32e3 + i) and its quad
// group (count +32ce + i, SVECTORs +2d80 + 80*i, packets +1770 + 140*i, index
// +32d5 + i); every enabled row then projects its bar (+3100 + 20*i into
// +2030 + 48*i by +32dc + i) and its group (+32c0 + i, +2a00 + 80*i, +eb0 +
// 140*i, +32c7 + i).
void Overlay::draw_rows_35c() {
    const auto stack_frame = enter(0x48);
    auto f = frame(0x48);
    for (std::uint32_t i = 0; i < 7; ++i) {
        const auto rows = block(*this, 0x35c);
        if (u8(rows + i + 0x32ea) == 0)
            continue;
        if (u8(rows + 0x32f2) != 0) {
            const auto packet = 0x2228 + i * 0x48 + rows + u8(rows + i + 0x32e3) * 0x24;
            project_quad(*this, i * 0x20 + 0x31e0 + rows, packet, f[0x28], f[0x2c]);
            const auto again = block(*this, 0x35c);
            draw(*this, 0x2228 + i * 0x48 + again + u8(again + i + 0x32e3) * 0x24);
            const auto group = block(*this, 0x35c);
            project_quads(u8(group + i + 0x32ce), i * 0x80 + 0x2d80 + group, i * 0x140 + 0x1770 + group,
                      u8(group + i + 0x32d5));
        }
        const auto bar = block(*this, 0x35c);
        const auto packet = 0x2030 + i * 0x48 + bar + u8(bar + i + 0x32dc) * 0x24;
        project_quad(*this, i * 0x20 + 0x3100 + bar, packet, f[0x28], f[0x2c]);
        const auto again = block(*this, 0x35c);
        draw(*this, 0x2030 + i * 0x48 + again + u8(again + i + 0x32dc) * 0x24);
        const auto group = block(*this, 0x35c);
        project_quads(u8(group + i + 0x32c0), i * 0x80 + 0x2a00 + group, i * 0x140 + 0xeb0 + group,
                  u8(group + i + 0x32c7));
    }
}

// 801ceb5c: while shown byte 8 is set, block +35c's frame quads (count +32f3,
// SVECTORs +2420, packets +0, index +32f1) and its rows (801ce860).
void Overlay::draw_row_block() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 8))
        return;
    const auto rows = block(*this, 0x35c);
    project_quads(u8(rows + 0x32f3), rows + 0x2420, rows, u8(rows + 0x32f1));
    draw_rows_35c();
}

// 801cebb4: while shown byte 4b is set, the five windows (+80 each) of block
// +360 whose enable byte (+294 + i) is set, all at the block's index +299.
void Overlay::draw_windows_360() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x4b))
        return;
    for (std::uint32_t i = 0; i < 5; ++i) {
        const auto windows = block(*this, 0x360);
        if (u8(windows + i + 0x294) != 0) {
            const auto window = i * 0x80 + windows;
            project_quads(1, window + 0x50, window, u8(windows + 0x299));
        }
    }
}

// 801cec40: while shown byte 9 is set, the screen image block (+350). When
// its dim request (+1192) differs from the applied state (+1193), both packet
// lists (count +118c at +8c0 by index +1191; count +1188 at +0 by +1190) of
// this buffer are made semi-transparent (dimmed, colour 20) or opaque (colour
// 80), untextured-shading off and blend mode bit 20 set in the tpage word; the
// state is then applied. Both lists are drawn.
void Overlay::draw_screen_image() {
    const auto stack_frame = enter(0x40);
    if (!shown(*this, 9))
        return;
    const auto images = [&] { return block(*this, 0x350); };
    if (u8(images() + 0x1192) != u8(images() + 0x1193)) {
        const auto dim = u8(images() + 0x1192) != 0;
        const auto level = dim ? 0x20U : 0x80U;
        const auto restyle = [&](std::uint32_t count, std::uint32_t index, std::uint32_t packets) {
            for (std::uint32_t i = 0; signed_word(i) < signed_word(u32(images() + count)); ++i) {
                const auto packet = [&] {
                    return (i * 2 + u8(images() + index)) * 0x28 + packets + images();
                };
                set_semi_trans(packet(), dim ? 1U : 0U);
                set_shade_tex(packet(), 0);
                put16(packet() + 0x16, u16(packet() + 0x16) | 0x20U);
                put8(packet() + 4, level);
                put8(packet() + 5, level);
                put8(packet() + 6, level);
            }
        };
        restyle(0x118c, 0x1191, 0x8c0);
        restyle(0x1188, 0x1190, 0);
        put8(images() + 0x1193, u8(images() + 0x1192));
    }
    draw_packets(u32(images() + 0x118c), images() + 0x8c0, u8(images() + 0x1191));
    draw_packets(u32(images() + 0x1188), images(), u8(images() + 0x1190));
}

// 801cf308: while shown byte a is set, block +354's two lists (count +1404
// at +500 by index +1409; count +1400 at +0 by +1408).
void Overlay::draw_lists_354() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0xa))
        return;
    const auto lists = block(*this, 0x354);
    draw_packets(u32(lists + 0x1404), lists + 0x500, u8(lists + 0x1409));
    const auto again = block(*this, 0x354);
    draw_packets(u32(again + 0x1400), again, u8(again + 0x1408));
}

// 801cf37c: in file selection (+4d8 == 2), the cursor box of every slot that
// shares the selected slot's slot byte (only the selected one when that byte
// is ff): the box colour becomes the pulsing level (+4d4), its POLY_F4 (+b0,
// 18 per buffer) takes the projection of the SVECTORs at +e0..+f8, and the box
// and its mode packet (+140, c per buffer) are drawn.
void Overlay::draw_slot_cursors() {
    const auto stack_frame = enter(0x40);
    if (u8(at(file_screen_mode)) != 2)
        return;
    auto f = frame(0x40);
    const auto selected = selected_slot(*this);
    for (std::uint32_t i = 0; i < 0x20; ++i) {
        const auto screen = block(*this, file_screen_state);
        if (selected != u8(screen + i + 0x4fae))
            continue;
        if (selected == 0xff && i != selected_slot_index(*this))
            continue;
        const auto slot = u32(at(slot_blocks) + i * 4);
        const auto box = [&] { return buffer(*this) * 0x18 + slot; };
        put8(box() + 0xb4, u8(at(cursor_level)));
        put8(box() + 0xb5, u8(at(cursor_level)));
        put8(box() + 0xb6, u8(at(cursor_level)));
        static_cast<void>(resident::rot_trans_pers4(
            *this, program.resident.gte, slot + 0xe0, slot + 0xe8, slot + 0xf0, slot + 0xf8,
            box() + 0xb8, box() + 0xbc, box() + 0xc0, box() + 0xc4, f[0x28], f[0x2c]));
        draw(*this, slot + buffer(*this) * 0x18 + 0xb0);
        draw(*this, slot + buffer(*this) * 0xc + 0x140);
    }
}

// 801cf5e4: for rows `first` .. `end` - 1 of the file screen that are in use
// (+58 + 5c*row) and hold icons (+b97 + 200*row), each icon's slot block
// (+3a8 + 4*slot, from `slot` on) gets its POLY_FT4 (28 per buffer) texture
// corners: u = row * 10 .. + 10, v the row's palette-step byte (+5c*row +
// 4*(+4cc)) .. + 10, and the clut of palette row 1c1 + row / 16, column row *
// 10; its quad (SVECTORs +e0) is projected and drawn.
void Overlay::draw_slot_icons(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x40);
    auto row = a0;
    auto slot = a1;
    for (; signed_word(row) < signed_word(a2); ++row) {
        const auto rows = block(*this, file_screen_state);
        if (u8(rows + row * 0x5c + 0x58) == 0 || u8(rows + row * 0x200 + 0xb97) == 0)
            continue;
        const auto u = row << 4U;
        for (std::uint32_t icon = 0;
             signed_word(icon) < signed_word(u8(block(*this, file_screen_state) + row * 0x200 + 0xb97));
             ++icon, ++slot) {
            const auto icon_block = u32(at(slot_blocks) + slot * 4);
            const auto packet = [&] { return buffer(*this) * 0x28 + icon_block; };
            const auto v = [&] {
                return u8(u32(at(palette_step)) * 4 + block(*this, file_screen_state) + row * 0x5c);
            };
            put8(packet() + 0xc, u);
            put8(packet() + 0xd, v());
            put8(packet() + 0x14, u + 0x10);
            put8(packet() + 0x15, v());
            put8(packet() + 0x1c, u);
            put8(packet() + 0x1d, v() + 0x10);
            put8(packet() + 0x24, u + 0x10);
            put8(packet() + 0x25, v() + 0x10);
            // GetClut (80043a58); the row divides by 16 as a signed value.
            const auto clut = get_clut(signed_word(u), signed_word(row) / 16 + 0x1c1);
            put16(icon_block + buffer(*this) * 0x28 + 0xe, clut);
            project_quads(1, icon_block + 0xe0, icon_block, buffer(*this));
        }
    }
}

// 801cf8d8: while the file screen is up (+4d8), each of the two memory-card
// headers whose card is present (+4fe4 + side) is built while shown byte 68
// is set: its label sprite (115, or 122 for the side the cursor is on while
// shown byte 2f is set) at x 1e + 90*side and its card-name sprite (162 +
// side) at x 1b + 90*side, both at y 36 and scale 1000, from the sheet +2dc
// into the header packets (+4d94 + f0*side, 8002675c). A built header links
// the name's part packets and the label.
void Overlay::draw_card_headers() {
    const auto stack_frame = enter(0x58);
    if (u8(at(file_screen_mode)) == 0)
        return;
    std::array<bool, 2> built{};
    std::uint32_t parts = 0;
    for (std::uint32_t side = 0; side < 2; ++side) {
        const auto header = 0x4d94 + side * 0xf0;
        const auto screen = block(*this, file_screen_state);
        if (u8(screen + side + 0x4fe4) != 0 && shown(*this, 0x68)) {
            auto normal = true;
            // Signed division: the side the cursor's slot is on.
            if (shown(*this, 0x2f) &&
                signed_word(side) == signed_word(selected_slot_index(*this)) / 16)
                normal = false;
            const auto label = normal ? 0x115U : 0x122U;
            static_cast<void>(resident::sheet_quads(
                *this, u32(at(sprite_sheet)), label, header + block(*this, file_screen_state),
                buffer(*this), 0x1e + side * 0x90, 0x36, 0x1000));
            parts = resident::sheet_quads(*this, u32(at(sprite_sheet)), side + 0x162,
                                          header + block(*this, file_screen_state) + 0x50, buffer(*this),
                                          0x1b + side * 0x90, 0x36, 0x1000);
            built.at(side) = true;
        }
        if (!built.at(side))
            continue;
        for (std::uint32_t i = 0; signed_word(i) < signed_word(parts); ++i)
            draw(*this, header + block(*this, file_screen_state) + (i * 2 + buffer(*this)) * 0x28 + 0x50);
        draw(*this, header + block(*this, file_screen_state) + buffer(*this) * 0x28);
    }
}

// 801cfb48: while the file screen is up, the two connector lines (LINE_F3,
// 18 per buffer at +50 and +80) of every file slot but the last of each card
// (slots f and 1f) whose card is present: red on the selected slot group in
// file selection, green otherwise; each line takes the projection of three
// SVECTORs (+100/108/118, +120/130/138) and is drawn.
void Overlay::draw_slot_lines() {
    const auto stack_frame = enter(0x48);
    if (u8(at(file_screen_mode)) == 0)
        return;
    auto f = frame(0x48);
    const auto selected = selected_slot(*this);
    for (std::uint32_t i = 0; i < 0x20; ++i) {
        const auto slot = u32(at(slot_blocks) + i * 4);
        const auto screen = block(*this, file_screen_state);
        // Signed division by 16: the slot's card side.
        const auto side = static_cast<std::uint32_t>(signed_word(i) / 16);
        if (u8(screen + side + 0x4fe4) == 0 || side * 16 == i - 0xf)
            continue;
        auto highlighted = false;
        if (selected == u8(screen + i + 0x4fae) && u8(at(file_screen_mode)) == 2)
            highlighted = selected != 0xff || i == selected_slot_index(*this);
        const auto line = [&](std::uint32_t offset) {
            return buffer(*this) * 0x18 + slot + offset;
        };
        put8(line(0x54), highlighted ? 0xffU : 0U);
        put8(line(0x55), highlighted ? 0U : 0xffU);
        put8(line(0x56), 0);
        put8(line(0x84), highlighted ? 0xffU : 0U);
        put8(line(0x85), highlighted ? 0U : 0xffU);
        put8(line(0x86), 0);
        struct Line {
            std::uint16_t v0, v1, v2, packet;
        };
        for (const auto &l : {Line{0x100, 0x108, 0x118, 0x50}, Line{0x120, 0x130, 0x138, 0x80}}) {
            static_cast<void>(resident::rot_trans_pers3(
                *this, program.resident.gte, slot + l.v0, slot + l.v1, slot + l.v2,
                line(l.packet) + 8, line(l.packet) + 0xc, line(l.packet + 0x10), f[0x20], f[0x24]));
            draw(*this, slot + buffer(*this) * 0x18 + l.packet);
        }
    }
}

// 801cff64: while shown byte 52 is set, the scrolling strip of block +44c:
// the texture corners of its sprite (18 per buffer by +7b8) span u 20 and v
// 61..68 from the scroll (+7b0) + 20; its two arrows blink (drawn when the
// frame counter mod 6 is 4 or 5, or always in state 2), its twelve packets
// (+30) and the sprite are drawn. In state 1 the scroll advances by +7b4 and
// wraps to 0 past 100.
void Overlay::draw_scroll_strip() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x52))
        return;
    const auto strip = [&] { return block(*this, 0x44c); };
    const auto sprite = [&] { return u8(strip() + 0x7b8) * 0x18 + strip(); };
    put16(sprite() + 8, 0x20);
    put16(sprite() + 0xa, 0x61);
    put16(sprite() + 0xc, u16(strip() + 0x7b0) + 0x20);
    put16(sprite() + 0xe, 0x61);
    put16(sprite() + 0x10, 0x20);
    put16(sprite() + 0x12, 0x68);
    put16(sprite() + 0x14, u16(strip() + 0x7b0) + 0x20);
    put16(sprite() + 0x16, 0x68);
    if (u32(at(frame_counter)) % 6 >= 4 || u8(block(*this, shown_flags) + 0x52) == 2)
        draw_packets(2, strip() + 0x3f0, u8(strip() + 0x7b8));
    draw_packets(0xc, strip() + 0x30, u8(strip() + 0x7b8));
    draw(*this, sprite());
    if (u8(block(*this, shown_flags) + 0x52) == 1) {
        const auto scroll = u32(strip() + 0x7b0) + u32(strip() + 0x7b4);
        put32(strip() + 0x7b0, scroll);
        if (signed_word(scroll) > 0x100)
            put32(strip() + 0x7b0, 0);
    }
}

// 801d01d0: the file screen's frame: cursor boxes (801cf37c), the icons of
// both cards (801cf5e4 rows 0-f from slot 0, rows 10-1f from slot 10), the
// connector lines (801cfb48), the card headers (801cf8d8) and the strip
// (801cff64); then the palette step advances every 15 frames (0-5) and the
// cursor level pulses by 4 between 4 and 7c.
void Overlay::draw_file_screen() {
    const auto stack_frame = enter(0x18);
    draw_slot_cursors();
    draw_slot_icons(0, 0, 0x10);
    draw_slot_icons(0x10, 0x10, 0x20);
    draw_slot_lines();
    draw_card_headers();
    draw_scroll_strip();
    const auto timer = u32(at(palette_timer)) + 1;
    put32(at(palette_timer), timer);
    if (timer == 0xf) {
        put32(at(palette_timer), 0);
        const auto step = u32(at(palette_step)) + 1;
        put32(at(palette_step), step);
        if (step == 6)
            put32(at(palette_step), 0);
    }
    if (u8(at(cursor_falling)) == 0) {
        const auto level = u32(at(cursor_level)) + 4;
        put32(at(cursor_level), level);
        if (signed_word(level) < 0x81)
            return;
        put8(at(cursor_falling), 1);
        put32(at(cursor_level), 0x7c);
    } else {
        const auto level = u32(at(cursor_level)) - 4;
        put32(at(cursor_level), level);
        if (signed_word(level) >= 0)
            return;
        put8(at(cursor_falling), 0);
        put32(at(cursor_level), 4);
    }
}

// 801d02d8: while shown byte b is set, the slot list block (+34c). Without
// its detail view (+2dbc) it draws the 32 slot packets and the selection
// marker (POLY_FT4 at +a00, 28 per buffer): u from the selected slot byte *
// 10 .. + f, v from that slot's palette-step byte .. + f, clut of palette row
// 1c1 + byte / 16, column byte * 10. With the detail view it draws the three
// pages (+87c each) whose enable byte (+1310) is set: seven packet lists, the
// page frame and its tab, then the view's three shared lists (+240c, +2c7c,
// +277c). Both then draw the mode packet (+a50, 24 per buffer).
void Overlay::draw_slot_list() {
    const auto stack_frame = enter(0x60);
    if (!shown(*this, 0xb))
        return;
    const auto slots = [&] { return block(*this, 0x34c); };
    if (u8(slots() + 0x2dbc) == 0) {
        draw_packets(0x20, slots(), buffer(*this));
        const auto marker = [&] { return buffer(*this) * 0x28 + slots() + 0xa00; };
        const auto palette_v = [&] {
            const auto screen = block(*this, file_screen_state);
            return u8(u32(at(palette_step)) * 4 + screen + selected_slot(*this) * 0x5c);
        };
        put8(marker() + 0xc, selected_slot(*this) << 4U);
        put8(marker() + 0xd, palette_v());
        put8(marker() + 0x14, (selected_slot(*this) << 4U) + 0xf);
        put8(marker() + 0x15, palette_v());
        put8(marker() + 0x1c, selected_slot(*this) << 4U);
        put8(marker() + 0x1d, palette_v() + 0xf);
        put8(marker() + 0x24, (selected_slot(*this) << 4U) + 0xf);
        put8(marker() + 0x25, palette_v() + 0xf);
        const auto byte = selected_slot(*this);
        put16(marker() + 0xe, get_clut(signed_word(byte << 4U), signed_word((byte >> 4U) + 0x1c1)));
        draw(*this, marker());
    } else {
        struct List {
            std::uint16_t count;
            std::uint16_t packets;
        };
        static constexpr std::array<List, 6> lists{{{0x1308, 0x320},
                                                    {0x1309, 0x410},
                                                    {0x130a, 0x500},
                                                    {0x130b, 0x5f0},
                                                    {0x130c, 0x6e0},
                                                    {0x130d, 0x780}}};
        for (std::uint32_t page = 0; page < 3; ++page) {
            const auto offset = page * 0x87c;
            const auto base = 0xa98 + offset;
            if (u8(slots() + offset + 0x1310) == 0)
                continue;
            draw_packets(u8(slots() + offset + 0x1312), base + slots() + 0x50,
                      u8(slots() + offset + 0x130e));
            for (const auto &list : lists)
                draw_packets(u8(slots() + offset + list.count), base + slots() + list.packets,
                          u8(slots() + offset + 0x130f));
            draw(*this, base + slots() + u8(slots() + 0x130f) * 0x28);
            draw(*this, base + slots() + u8(slots() + offset + 0x1311) * 0x28 + 0x820);
        }
        draw_packets(0xb, slots() + 0x240c, u8(slots() + 0x130f));
        draw_packets(4, slots() + 0x2c7c, u8(slots() + 0x130f));
        draw_packets(0x10, slots() + 0x277c, u8(slots() + 0x130f));
    }
    draw(*this, buffer(*this) * 0x24 + 0xa50 + slots());
}

// 801d0954: project the four SVECTORs at `vectors` into packet `index` of
// `packets` (28 bytes each) and link it into ordering-table entry `entry`.
void Overlay::project_panel_piece(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x40);
    auto f = frame(0x40);
    const auto packet = a1 + a2 * 0x28;
    project_quad(*this, a0, packet, f[0x28], f[0x2c]);
    add_prim(a3 * 4 + ordering_table + u32(at(draw_environment)), packet);
}

// 801d09f0: draw 3D panel `panel` (block +364 + 4*panel) at its buffer index
// (+71c) into its ordering-table entry (+718): four body pieces (SVECTORs
// +510.., packets +0..), with `titled` the title pieces (+6d0/+410, +6f0/+460,
// +6b0/+3c0), the four corners and four edges (+590..+670 into +140..+370),
// then its backdrop quad (POLY_G4 +4b0, 24 per buffer; SVECTORs +690) and
// mode packet (+4f8, c per buffer).
void Overlay::draw_panel(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x48);
    auto f = frame(0x48);
    const auto block_address = u32(at(panel_blocks) + a0 * 4);
    const auto piece = [&](std::uint32_t vectors, std::uint32_t packets) {
        project_panel_piece(block_address + vectors, block_address + packets, u8(block_address + 0x71c),
                  u32(block_address + 0x718));
    };
    for (std::uint32_t i = 0; i < 4; ++i)
        piece(0x510 + i * 0x20, i * 0x50);
    if ((a1 & 0xffU) != 0) {
        for (std::uint32_t i = 0; i < 2; ++i)
            piece(0x6d0 + i * 0x20, 0x410 + i * 0x50);
        piece(0x6b0, 0x3c0);
    }
    for (std::uint32_t i = 0; i < 8; ++i)
        piece(0x590 + i * 0x20, 0x140 + i * 0x50);
    const auto backdrop = [&] { return u8(block_address + 0x71c) * 0x24 + block_address; };
    static_cast<void>(resident::rot_trans_pers4(
        *this, program.resident.gte, block_address + 0x690, block_address + 0x698,
        block_address + 0x6a0, block_address + 0x6a8, backdrop() + 0x4b8, backdrop() + 0x4c0,
        backdrop() + 0x4c8, backdrop() + 0x4d0, f[0x28], f[0x2c]));
    const auto entry = [&] {
        return u32(block_address + 0x718) * 4 + ordering_table + u32(at(draw_environment));
    };
    add_prim(entry(), block_address + u8(block_address + 0x71c) * 0x24 + 0x4b0);
    add_prim(entry(), block_address + u8(block_address + 0x71c) * 0xc + 0x4f8);
}

// 801d0c78: each 3D panel shown (byte 20 + i) is drawn (801d09f0, titled by
// +71d). A panel without its own view (+714 zero) is drawn with the menu's
// default view: the loaded matrix is pushed (8004960c), the identity rotation
// of zero angles (8003f738) with translation (0, 0, 200) (80049d9c) is loaded
// (80049efc, 80049f8c) and the previous matrix popped (800496ac) afterwards.
void Overlay::draw_panels() {
    const auto stack_frame = enter(0x60);
    auto &gte = program.resident.gte;
    for (std::uint32_t i = 0; i < 7; ++i) {
        if (!shown(*this, 0x20 + i))
            continue;
        if (u32(panel(*this, i) + 0x714) != 0) {
            draw_panel(i, u8(panel(*this, i) + 0x71d));
            continue;
        }
        field::push_matrix(program.resident.matrix_stack, gte.transform);
        auto view = field::rotation_matrix({0, 0, 0}, program.resident.math.trigonometry);
        view.t = {0, 0, 0x200};
        gte.transform.r = view.r;
        gte.transform.t = view.t;
        draw_panel(i, u8(panel(*this, i) + 0x71d));
        gte.transform = field::pop_matrix(program.resident.matrix_stack);
    }
}

// 801d0d90: the four state-block windows (+4e0 + 80*i, index +55d) shown by
// bytes 34..37.
void Overlay::draw_state_windows_4e0() {
    const auto stack_frame = enter(0x20);
    for (std::uint32_t i = 0; i < 4; ++i)
        if (shown(*this, 0x34 + i))
            draw(*this, 0x4e0 + i * 0x80 + state() + u8(state() + i * 0x80 + 0x55d) * 0x28);
}

// 801d0e20: a delay loop of seven iterations; V0 ends at -1.
std::uint32_t Overlay::delay_seven() { return 0xffffffffU; }

// 801d0e38: the six state-block windows (+ae0 + 80*i) shown by bytes 14..19
// whose own enable byte (+b5f + 80*i) is set.
void Overlay::draw_state_windows_ae0() {
    const auto stack_frame = enter(0x18);
    for (std::uint32_t i = 0; i < 6; ++i)
        if (shown(*this, 0x14 + i) && u8(state() + i * 0x80 + 0xb5f) != 0)
            draw_window(*this, i * 0x80 + 0xae0 + state());
}

// 801d0ebc: a delay loop of five iterations; V0 ends at -1.
std::uint32_t Overlay::delay_five() { return 0xffffffffU; }

// 801d0ed4: the eight state-block windows (+10e0 + 80*i) shown by bytes
// 38..3f.
void Overlay::draw_state_windows_10e0() {
    const auto stack_frame = enter(0x20);
    for (std::uint32_t i = 0; i < 8; ++i)
        if (shown(*this, 0x38 + i))
            draw_window(*this, 0x10e0 + i * 0x80 + state());
}

// 801d0f54: the six state-block windows (+14e0 + 80*i) shown by bytes
// 40..45.
void Overlay::draw_state_windows_14e0() {
    const auto stack_frame = enter(0x20);
    for (std::uint32_t i = 0; i < 6; ++i)
        if (shown(*this, 0x40 + i))
            draw_window(*this, 0x14e0 + i * 0x80 + state());
}

// 801d0fd4: the state-block window at +17e0 (index +185d) while shown byte 4e
// is set.
void Overlay::draw_state_window_17e0() {
    const auto stack_frame = enter(0x18);
    if (shown(*this, 0x4e))
        draw(*this, u8(state() + 0x185d) * 0x28 + 0x17e0 + state());
}

// 801d1030: while shown byte 2e is set, the three windows +1de0 + 4*i: a
// projected one (+7f set) by 801ce198, a flat one's packet directly.
void Overlay::draw_windows_1de0() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x2e))
        return;
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto window = u32(at(0x1de0) + i * 4);
        if (u8(window + 0x7f) != 0)
            draw_window(*this, window);
        else
            draw(*this, window + u8(window + 0x7d) * 0x28);
    }
}

// 801d10dc: the six state-block windows (+18e0 + 80*i) shown by bytes 54..59
// whose own enable byte (+195f + 80*i) is set.
void Overlay::draw_state_windows_18e0() {
    const auto stack_frame = enter(0x18);
    for (std::uint32_t i = 0; i < 6; ++i)
        if (shown(*this, 0x54 + i) && u8(state() + i * 0x80 + 0x195f) != 0)
            draw_window(*this, i * 0x80 + 0x18e0 + state());
}

// 801d1160: the four state-block windows (+1be0 + 80*i, index +1c5d) shown by
// bytes 5c..5f.
void Overlay::draw_state_windows_1be0() {
    const auto stack_frame = enter(0x20);
    for (std::uint32_t i = 0; i < 4; ++i)
        if (shown(*this, 0x5c + i))
            draw(*this, 0x1be0 + i * 0x80 + state() + u8(state() + i * 0x80 + 0x1c5d) * 0x28);
}

// 801d11f0: the state block's windows (801d0d90 .. 801d1030, with the two
// delay loops).
void Overlay::draw_state_windows() {
    const auto stack_frame = enter(0x18);
    draw_state_windows_4e0();
    static_cast<void>(delay_seven());
    draw_state_windows_ae0();
    static_cast<void>(delay_five());
    draw_state_windows_18e0();
    draw_state_windows_1be0();
    draw_state_windows_10e0();
    draw_state_windows_14e0();
    draw_state_window_17e0();
    draw_windows_1de0();
}

// 801d12d4: a list window record while its enable byte (+be7) is set: its two
// frame packets (+460, +4b0 at index +be6), seven packet lists (counts +be8,
// +be0..+be5) and, with `scroll_marks`, the five packets at +2d0.
void Overlay::draw_list_window(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x20);
    const auto record = a0;
    if (u8(record + 0xbe7) == 0)
        return;
    draw(*this, record + u8(record + 0xbe6) * 0x28 + 0x460);
    draw(*this, record + u8(record + 0xbe6) * 0x28 + 0x4b0);
    struct List {
        std::uint16_t count;
        std::uint16_t packets;
    };
    static constexpr std::array<List, 7> lists{{{0xbe8, 0},
                                                {0xbe0, 0x500},
                                                {0xbe1, 0x5f0},
                                                {0xbe2, 0x6e0},
                                                {0xbe3, 0x870},
                                                {0xbe4, 0xa00},
                                                {0xbe5, 0xaf0}}};
    for (const auto &list : lists)
        draw_packets(u8(record + list.count), record + list.packets, u8(record + 0xbe6));
    if ((a1 & 0xffU) != 0)
        draw_packets(5, record + 0x2d0, u8(record + 0xbe6));
}

// 801d13f8: while shown byte 46 is set, the three list windows +1e08 + 4*i
// with their scroll marks.
void Overlay::draw_list_windows() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x46))
        return;
    for (std::uint32_t i = 0; i < 3; ++i)
        draw_list_window(u32(at(0x1e08) + i * 4), 1);
}

// 801d1464: while shown byte 49 is set, window block +43c (index +70).
void Overlay::draw_window_43c() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x49))
        return;
    const auto window = block(*this, 0x43c);
    project_quads(1, window + 0x50, window, u8(window + 0x70));
}

// 801d14b0: while shown byte 53 is set, the four quads of block +440
// (SVECTORs +140, index +1c0).
void Overlay::draw_quads_440() {
    const auto stack_frame = enter(0x18);
    if (!shown(*this, 0x53))
        return;
    const auto quads = block(*this, 0x440);
    project_quads(4, quads + 0x140, quads, u8(quads + 0x1c0));
}

// 801d14fc: while shown byte 48 is set, the sixteen rows of block +42c whose
// enable byte (+1184 + i) is set (windows +80*i and +800 + 80*i), then, with
// +1194 set, the windows at +1000, +1080 and +1100.
void Overlay::draw_rows_42c() {
    const auto stack_frame = enter(0x20);
    if (!shown(*this, 0x48))
        return;
    for (std::uint32_t i = 0; i < 0x10; ++i) {
        if (u8(block(*this, 0x42c) + i + 0x1184) == 0)
            continue;
        draw_window(*this, i * 0x80 + block(*this, 0x42c));
        draw_window(*this, 0x800 + i * 0x80 + block(*this, 0x42c));
    }
    if (u8(block(*this, 0x42c) + 0x1194) == 0)
        return;
    for (const std::uint32_t window : {0x1000U, 0x1080U, 0x1100U})
        draw_window(*this, block(*this, 0x42c) + window);
}

// 801d1640: while shown byte 4a is set, the fourteen rows of block +430 whose
// enable byte (+1084 + i) is set (windows +80*i and +700 + 80*i); with +1092
// set, the windows at +e00, +e80, +f00 and +f80; then the window at +1000.
void Overlay::draw_rows_430() {
    const auto stack_frame = enter(0x20);
    if (!shown(*this, 0x4a))
        return;
    for (std::uint32_t i = 0; i < 0xe; ++i) {
        if (u8(block(*this, 0x430) + i + 0x1084) == 0)
            continue;
        draw_window(*this, i * 0x80 + block(*this, 0x430));
        draw_window(*this, 0x700 + i * 0x80 + block(*this, 0x430));
    }
    if (u8(block(*this, 0x430) + 0x1092) != 0)
        for (const std::uint32_t window : {0xe00U, 0xe80U, 0xf00U, 0xf80U})
            draw_window(*this, block(*this, 0x430) + window);
    draw_window(*this, block(*this, 0x430) + 0x1000);
}

// 801d17c4: while shown byte 4c is set, the eight rows of block +434 whose
// enable byte (+a10 + i) is set (windows +80*i and +400 + 80*i), the window at
// +800 and, with +a18 set, the windows at +880, +900 and +980.
void Overlay::draw_rows_434() {
    const auto stack_frame = enter(0x20);
    if (!shown(*this, 0x4c))
        return;
    for (std::uint32_t i = 0; i < 8; ++i) {
        if (u8(block(*this, 0x434) + i + 0xa10) == 0)
            continue;
        draw_window(*this, i * 0x80 + block(*this, 0x434));
        draw_window(*this, 0x400 + i * 0x80 + block(*this, 0x434));
    }
    draw_window(*this, block(*this, 0x434) + 0x800);
    if (u8(block(*this, 0x434) + 0xa18) != 0)
        for (const std::uint32_t window : {0x880U, 0x900U, 0x980U})
            draw_window(*this, block(*this, 0x434) + window);
}

// 801d1914: while shown byte 4d is set, block +438: the thirteen rows whose
// enable byte (+2596 + i) is set draw their packet (+80*i) and its partner
// (+680 + 80*i), both at the row's index (+7d); then the window at +d00, and
// for each row its packet list (count +257c + i at +d80 + 190*i, index +2589 +
// i) and, with +25a3 + i set, its POLY_G4 (+21d0 + 48*i, 24 per buffer by
// +25b0 + i).
void Overlay::draw_rows_438() {
    const auto stack_frame = enter(0x20);
    if (!shown(*this, 0x4d))
        return;
    const auto rows = [&] { return block(*this, 0x438); };
    for (std::uint32_t i = 0; i < 0xd; ++i) {
        if (u8(rows() + i + 0x2596) == 0)
            continue;
        const auto row = i * 0x80 + rows();
        draw(*this, row + u8(row + 0x7d) * 0x28);
        draw(*this, 0x680 + i * 0x80 + rows() + u8(rows() + i * 0x80 + 0x7d) * 0x28);
    }
    project_quads(1, rows() + 0xd50, rows() + 0xd00, u8(rows() + 0xd7d));
    for (std::uint32_t i = 0; i < 0xd; ++i) {
        draw_packets(u8(rows() + i + 0x257c), 0xd80 + i * 400 + rows(), u8(rows() + i + 0x2589));
        if (u8(rows() + i + 0x25a3) != 0)
            draw(*this, 0x21d0 + i * 0x48 + rows() + u8(rows() + i + 0x25b0) * 0x24);
    }
}

// 801d1aac: the two windows +444 + 4*i shown by bytes 50, 51 (index +75).
void Overlay::draw_windows_444() {
    const auto stack_frame = enter(0x18);
    for (std::uint32_t i = 0; i < 2; ++i) {
        if (!shown(*this, 0x50 + i))
            continue;
        const auto window = u32(at(0x444) + i * 4);
        project_quads(1, window + 0x50, window, u8(window + 0x75));
    }
}

// 801d1b20: mode 0's screen: opening panels (801d3b00), the state block's
// windows, cursors, header, slot list and file screen, party windows, status
// quads, the rows of +35c, the windows of +360, lists and blocks +340..+44c,
// the 3D panels (801d0c78), the screen image and the lists of +354.
void Overlay::draw_field_menu_screen() {
    const auto stack_frame = enter(0x18);
    grow_opening_panels();
    draw_state_windows();
    draw_cursors();
    draw_header();
    draw_slot_list();
    draw_file_screen();
    draw_party_windows();
    draw_status_quads();
    draw_row_block();
    draw_windows_360();
    draw_lists_340_344();
    draw_list_windows();
    draw_windows_444();
    draw_rows_42c();
    draw_rows_430();
    draw_rows_434();
    draw_rows_438();
    draw_window_43c();
    draw_quads_440();
    draw_panels();
    draw_screen_image();
    draw_lists_354();
}

// 801d1be8: mode 2's screen: opening panels, the state block's windows,
// cursors, header, slot list, file screen, screen image, the lists of +354
// and the 3D panels.
void Overlay::draw_title_file_screen() {
    const auto stack_frame = enter(0x18);
    grow_opening_panels();
    draw_state_windows();
    draw_cursors();
    draw_header();
    draw_slot_list();
    draw_file_screen();
    draw_screen_image();
    draw_lists_354();
    draw_panels();
}

// 801d3b00: each opening panel record (+380 + 4*i) shown by byte 27 + i and not
// yet open (+11): its width (+8) and height (+a) grow by 20 up to the target
// (+4, +6), the record opens when both reach it, and the panel is rebuilt
// (801d4d1c) centred on the target rectangle (+0, +2) with the record's panel
// (+10), view (+12), ordering-table entry (+c) and title (+13).
void Overlay::grow_opening_panels() {
    const auto stack_frame = enter(0x28);
    for (std::uint32_t i = 0; i < 7; ++i) {
        const auto record = u32(at(panel_growth) + i * 4);
        if (!shown(*this, 0x27 + i) || u8(record + 0x11) != 0)
            continue;
        std::uint32_t reached = 0;
        for (const std::uint32_t axis : {0U, 2U}) {
            const auto grown = u16(record + 8 + axis) + 0x20;
            if (signed_word(grown) < signed_word(u16(record + 4 + axis))) {
                put16(record + 8 + axis, grown);
            } else {
                put16(record + 8 + axis, u16(record + 4 + axis));
                ++reached;
            }
        }
        if (reached == 2)
            put8(record + 0x11, 1);
        const auto width = u16(record + 8);
        const auto height = u16(record + 0xa);
        const auto x = (u16(record) + (u16(record + 4) >> 1U) - (width >> 1U)) & 0xffffU;
        const auto y = (u16(record + 2) + (u16(record + 6) >> 1U) - (height >> 1U)) & 0xffffU;
        build_panel(u8(record + 0x10), x, y, width, height, u8(record + 0x12), u32(record + 0xc),
                  u8(record + 0x13));
    }
}

// 801d3c4c: the title bar of panel `panel` at (x, y) with height `height`
// (stack word): sprite 105 of the sheet +2dc (+410 at y, and flipped
// vertically at +460 at y + height - 8, 800263e4) and sprite 106 (+3c0 at y +
// 8), then the corner SVECTORs (801c851c) of the bar's top cap (+6d0, 8x8),
// bottom cap (+6f0, 8 by -8 from y + height) and body (+6b0, 8 by height - 8
// from y + 8). The width (A3) is not read.
void Overlay::build_panel_title(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4) {
    const auto stack_frame = enter(0x48);
    static_cast<void>(a3);
    const auto x = a1 & 0xffffU;
    const auto y = a2 & 0xffffU;
    const auto height = a4;
    const auto block_address = panel(*this, a0);
    static_cast<void>(resident::sheet_quads(*this, u32(at(sprite_sheet)), 0x105,
                                            block_address + 0x410, buffer(*this), x, y, 0x1000));
    static_cast<void>(resident::sheet_quads_flipped(*this, u32(at(sprite_sheet)), 0x105,
                                                    block_address + 0x460, buffer(*this), x,
                                                    y + (height & 0xffffU) - 8, 0x1000, 0, 1));
    static_cast<void>(resident::sheet_quads(*this, u32(at(sprite_sheet)), 0x106,
                                            block_address + 0x3c0, buffer(*this), x, y + 8,
                                            0x1000));
    set_screen_quad_vectors(block_address + 0x6d0, x, y, 8, 8);
    set_screen_quad_vectors(block_address + 0x6f0, x, (a2 + height) & 0xffffU, 8, 0xfff8);
    set_screen_quad_vectors(block_address + 0x6b0, x, (a2 + 8) & 0xffffU, 8, (height + 0xfff8) & 0xffffU);
}

// 801d3db0: panel `panel`'s frame corners: sprites fd, ff, 102 and 104 of the
// sheet +2dc built at (0, 0) one after another from the block's start (part
// count kept in +710, 50 bytes per part, 8002675c), the corner SVECTORs
// (801c851c) of the four 16x16 corners (+510 top left .. +570 bottom right,
// mirrored by negative extents) around the rectangle (x, y, width, height),
// and this buffer's first four part packets made semi-transparent at colour
// 80 (801e91c4).
void Overlay::build_panel_corners(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4) {
    const auto stack_frame = enter(0x48);
    const auto block_address = panel(*this, a0);
    put32(block_address + 0x710, 0);
    for (const std::uint32_t sprite : {0xfdU, 0xffU, 0x102U, 0x104U}) {
        const auto parts = resident::sheet_quads(*this, u32(at(sprite_sheet)), sprite,
                                                 block_address + u32(block_address + 0x710) * 0x50,
                                                 buffer(*this), 0, 0, 0x1000);
        put32(block_address + 0x710, parts + u32(block_address + 0x710));
    }
    const auto left = (a1 - 8) & 0xffffU;
    const auto top = (a2 + 8) & 0xffffU;
    const auto right = (a1 + a3 + 8) & 0xffffU;
    const auto bottom = (a2 + a4 - 8) & 0xffffU;
    set_screen_quad_vectors(block_address + 0x510, left, top, 0x10, 0xfff0);
    set_screen_quad_vectors(block_address + 0x530, right, top, 0xfff0, 0xfff0);
    set_screen_quad_vectors(block_address + 0x550, left, bottom, 0x10, 0x10);
    set_screen_quad_vectors(block_address + 0x570, right, bottom, 0xfff0, 0x10);
    for (std::uint32_t k = 0; k < 4; ++k)
        set_quad_translucent(block_address + (k * 2 + buffer(*this)) * 0x28);
}

// 801d3ff8: panel `panel`'s top edge: texture corners u 0..7, v 84..94 of
// this buffer's two edge pieces (+140), the corner SVECTORs (801c851c) of
// its two halves (+590, +5b0), (width - 10) / 2 wide and 16 high from x + 8
// at y - 8, and both pieces made semi-transparent at colour 80 (801e91c4).
void Overlay::build_panel_top_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto block_address = panel(*this, a0);
    set_edge_uvs(*this, block_address, 0x140, 0, 7, 0x84, 0x94);
    const auto half = half_inner(a3);
    const auto y = (a2 - 8) & 0xffffU;
    set_screen_quad_vectors(block_address + 0x590, (a1 + 8) & 0xffffU, y, half & 0xffffU, 0x10);
    set_screen_quad_vectors(block_address + 0x5b0, (a1 + half + 8) & 0xffffU, y, half & 0xffffU, 0x10);
    for (std::uint32_t k = 0; k < 2; ++k)
        set_quad_translucent(block_address + (k * 2 + buffer(*this)) * 0x28 + 0x140);
}

// 801d433c: panel `panel`'s bottom edge, as 801d3ff8: u 8..f, v 84..94
// (+1e0), halves (+5d0, +5f0) at y + height - 8 (height the stack word).
void Overlay::build_panel_bottom_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4) {
    const auto stack_frame = enter(0x38);
    const auto block_address = panel(*this, a0);
    set_edge_uvs(*this, block_address, 0x1e0, 8, 0xf, 0x84, 0x94);
    const auto y = (a2 + a4 - 8) & 0xffffU;
    const auto half = half_inner(a3);
    set_screen_quad_vectors(block_address + 0x5d0, (a1 + 8) & 0xffffU, y, half & 0xffffU, 0x10);
    set_screen_quad_vectors(block_address + 0x5f0, (a1 + half + 8) & 0xffffU, y, half & 0xffffU, 0x10);
    for (std::uint32_t k = 0; k < 2; ++k)
        set_quad_translucent(block_address + (k * 2 + buffer(*this)) * 0x28 + 0x1e0);
}

// 801d4688: panel `panel`'s left edge, as 801d3ff8: u 10..20, v 84..8b
// (+280), halves (+610, +630) 16 wide and (height - 10) / 2 high (height in
// A3) from y + 8 at x - 8.
void Overlay::build_panel_left_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto block_address = panel(*this, a0);
    set_edge_uvs(*this, block_address, 0x280, 0x10, 0x20, 0x84, 0x8b);
    const auto x = (a1 - 8) & 0xffffU;
    const auto half = half_inner(a3);
    set_screen_quad_vectors(block_address + 0x610, x, (a2 + 8) & 0xffffU, 0x10, half & 0xffffU);
    set_screen_quad_vectors(block_address + 0x630, x, (a2 + half + 8) & 0xffffU, 0x10, half & 0xffffU);
    for (std::uint32_t k = 0; k < 2; ++k)
        set_quad_translucent(block_address + (k * 2 + buffer(*this)) * 0x28 + 0x280);
}

// 801d49d0: panel `panel`'s right edge, as 801d4688: u 10..20, v 8c..93
// (+320), halves (+650, +670) from y + 8 at x + width - 8 (height the low
// half of the stack word).
void Overlay::build_panel_right_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4) {
    const auto stack_frame = enter(0x38);
    const auto block_address = panel(*this, a0);
    set_edge_uvs(*this, block_address, 0x320, 0x10, 0x20, 0x8c, 0x93);
    const auto x = (a1 + a3 - 8) & 0xffffU;
    const auto half = half_inner(a4);
    set_screen_quad_vectors(block_address + 0x650, x, (a2 + 8) & 0xffffU, 0x10, half & 0xffffU);
    set_screen_quad_vectors(block_address + 0x670, x, (a2 + half + 8) & 0xffffU, 0x10, half & 0xffffU);
    for (std::uint32_t k = 0; k < 2; ++k)
        set_quad_translucent(block_address + (k * 2 + buffer(*this)) * 0x28 + 0x320);
}

// 801d4d1c: rebuild 3D panel `panel` (byte) at the rectangle (x, y, width,
// height; halfwords) while hidden (shown byte 20 + panel cleared): the body's
// corner SVECTORs (+690, 801c851c), the frame corners, the four edges and,
// with `titled` (byte), the title bar; then it records titled (+71d), view
// (+714, byte; zero draws with the default view), ordering-table entry
// (+718) and the buffer built (+71c) and is shown again.
void Overlay::build_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7) {
    const auto stack_frame = enter(0x40);
    const auto index = a0 & 0xffU;
    const auto x = a1 & 0xffffU;
    const auto y = a2 & 0xffffU;
    const auto width = a3 & 0xffffU;
    const auto height = a4 & 0xffffU;
    const auto view = a5 & 0xffU;
    const auto titled = a7 & 0xffU;
    const auto block_address = panel(*this, index);
    put8(block(*this, shown_flags) + index + 0x20, 0);
    set_screen_quad_vectors(block_address + 0x690, x, y, width, height);
    build_panel_corners(index, x, y, width, height);
    build_panel_top_edge(index, x, y, width);
    build_panel_bottom_edge(index, x, y, width, height);
    build_panel_left_edge(index, x, y, height);
    build_panel_right_edge(index, x, y, width, height);
    if (titled != 0)
        build_panel_title(index, x, y, width, height);
    put8(block_address + 0x71d, titled);
    put32(block_address + 0x714, view);
    put32(block_address + 0x718, a6);
    put8(block_address + 0x71c, u8(at(buffer_index)));
    put8(block(*this, shown_flags) + index + 0x20, 1);
}

} // namespace xem::reconstruction::menu
