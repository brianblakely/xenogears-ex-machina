// Menu overlay 82f84a24... (801c5000): the field menu's party panels (their
// slide in and out, portraits, labels and number sprites), the amount and
// play-time windows, the detail panel numbers, and the title file-select
// panels' packets.
#include "xem/reconstruction/menu_overlay.hpp"
#include "xem/reconstruction/resident_text.hpp"

namespace xem::reconstruction::menu {
namespace {
// Menu state fields (offsets from *800625a0).
constexpr std::uint32_t slide_record = 0x24;   // per slot: a window slide (below)
constexpr std::uint32_t sprite_table = 0x2dc;  // the menu sprite bank 8002675c draws from
constexpr std::uint32_t play_digits = 0x2ec;   // seven words: the split play time (801c7f34)
constexpr std::uint32_t buffer_index = 0x308;  // 0/1: the buffer being built
constexpr std::uint32_t digits = 0x31c;        // nine digit bytes from 801c80b8 (ff: blank)
constexpr std::uint32_t flags_block = 0x33c;   // block of menu flag bytes
constexpr std::uint32_t amount_block = 0x340;  // the amount window's sprites
constexpr std::uint32_t time_block = 0x344;    // the play-time window's sprites
constexpr std::uint32_t file_block = 0x34c;    // the file-select panel packets
constexpr std::uint32_t detail_block = 0x358;  // the detail panel's sprites
constexpr std::uint32_t window_blocks = 0x364; // per window slot: its packet block
constexpr std::uint32_t window_records = 0x380; // per window slot: its record
constexpr std::uint32_t panel_blocks = 0x39c;  // per party slot: the panel's sprites
constexpr std::uint32_t slot_blocks = 0x3a8;   // per file slot: its marker packets
// A slide record (state + slot * 24): x from +0, x to +4, y from +8, y to
// +c, per-step x and y (8.8 fixed point) +10/+14, accumulated x and y
// +18/+1c, x and y decreasing +20/+21 (bytes), steps per frame +22, done +23.
constexpr std::uint32_t slide_x_from = 0x0;
constexpr std::uint32_t slide_x_to = 0x4;
constexpr std::uint32_t slide_y_from = 0x8;
constexpr std::uint32_t slide_y_to = 0xc;
constexpr std::uint32_t slide_x_step = 0x10;
constexpr std::uint32_t slide_y_step = 0x14;
constexpr std::uint32_t slide_x = 0x18;
constexpr std::uint32_t slide_y = 0x1c;
constexpr std::uint32_t slide_x_down = 0x20;
constexpr std::uint32_t slide_y_down = 0x21;
constexpr std::uint32_t slide_steps = 0x22;
constexpr std::uint32_t slide_done = 0x23;
// Flag block bytes: +30..32 the party member of each panel slot (ff: none).
constexpr std::uint32_t party_members = 0x30;
// Character record (character_record) fields the panels show: words +44
// and +48, halfwords +4c, +4e, +50, +52, bytes +62, +63, and +a0, which
// selects a gear record (a4 bytes each; the panels read halfwords 8006dfe4
// and 8006dfe6 of it).
constexpr std::uint32_t record_gear = 0xa0;
constexpr std::uint32_t gear_numbers = 0x8006dfe4;
// The two text CLUT ids (80059414, 800595d4), chosen by a low bit.
constexpr std::uint32_t odd_clut = 0x80059414;
constexpr std::uint32_t even_clut = 0x800595d4;
// Sprites the bank draws are 50 bytes: one 28-byte packet per buffer.
constexpr std::uint32_t sprite_size = 0x50;
constexpr std::uint32_t packet_size = 0x28;

// An 8.8 fixed-point position truncated toward zero (sra 8 after adding ff
// to a negative value).
std::uint32_t whole(std::uint32_t fixed) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(fixed) / 256);
}

// R3000 div quotient: a zero divisor gives -1 for a non-negative dividend and
// 1 otherwise (general MIPS behavior; the compiler emitted no check), and
// 80000000 / -1 stays 80000000.
std::uint32_t quotient(std::uint32_t dividend, std::uint32_t divisor) {
    const auto n = static_cast<std::int32_t>(dividend);
    const auto d = static_cast<std::int32_t>(divisor);
    if (d == 0)
        return n >= 0 ? 0xffffffffU : 1U;
    if (dividend == 0x80000000U && d == -1)
        return dividend;
    return static_cast<std::uint32_t>(n / d);
}

// 8002675c(bank, index, packets, buffer, x, y, 1000): sprite `index` of the
// menu bank as packets at `packets` for the current buffer; V0 is the number
// of 50-byte sprites written.
std::uint32_t sprite(Overlay &o, std::uint32_t index, std::uint32_t packets, std::uint32_t x,
                     std::uint32_t y) {
    return resident::sheet_quads(o, o.u32(o.at(sprite_table)), index, packets,
                                 o.u32(o.at(buffer_index)), x, y, 0x1000);
}

// The digit bytes state + `first` .. + `first + count - 1` (ff: blank) as
// sprites into `block + packets`, counted in the byte `block + counter`
// (cleared first). A digit's x is `x` plus 8 per digit position, or with
// `packed` 8 per digit drawn before it.
void draw_digits(Overlay &o, std::uint32_t block, std::uint32_t counter, std::uint32_t packets,
                 std::uint32_t first, std::uint32_t count, std::uint32_t x, std::uint32_t y,
                 bool packed) {
    o.put8(block + counter, 0);
    std::uint32_t drawn = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto digit = o.u8(o.state() + first + i);
        if (digit == 0xff)
            continue;
        const auto column = packed ? drawn : i;
        const auto used = sprite(o, digit, block + packets + o.u8(block + counter) * sprite_size,
                                 x + column * 8, y);
        o.put8(block + counter, o.u8(block + counter) + used);
        ++drawn;
    }
}

// The four corners of a w x h quad at (x, y) into packet + 8/+10/+18/+20
// (POLY_FT4 layout), as halfwords.
void quad_corners(Overlay &o, std::uint32_t packet, std::uint32_t x, std::uint32_t y,
                  std::uint32_t w, std::uint32_t h) {
    o.put16(packet + 0x8, x);
    o.put16(packet + 0xa, y);
    o.put16(packet + 0x10, x + w);
    o.put16(packet + 0x12, y);
    o.put16(packet + 0x18, x);
    o.put16(packet + 0x1a, y + h);
    o.put16(packet + 0x20, x + w);
    o.put16(packet + 0x22, y + h);
}
} // namespace

// 801c81e0: start slide `a5` (a byte) from (a0, a1) to (a2, a3) in `a4`
// steps per frame: record the ends, the direction of each axis, and per-step
// deltas in 8.8 fixed point with the longer axis moving one pixel a step;
// clear the accumulated offsets and the done flag.
void Overlay::start_slide(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4, std::uint32_t a5) {
    const auto slide = at((a5 & 0xffU) * slide_record);
    put32(slide + slide_x_from, a0);
    put32(slide + slide_y_from, a1);
    put32(slide + slide_x_to, a2);
    put32(slide + slide_y_to, a3);
    const auto less = [](std::uint32_t a, std::uint32_t b) {
        return static_cast<std::int32_t>(a) < static_cast<std::int32_t>(b);
    };
    std::uint32_t dx = 0;
    if (less(a2, a0)) {
        dx = a0 - a2;
        put8(slide + slide_x_down, 1);
    } else {
        dx = a2 - a0;
        put8(slide + slide_x_down, 0);
    }
    std::uint32_t dy = 0;
    if (less(a3, a1)) {
        dy = a1 - a3;
        put8(slide + slide_y_down, 1);
    } else {
        dy = a3 - a1;
        put8(slide + slide_y_down, 0);
    }
    if (less(dx, dy)) {
        const auto step = quotient(dx << 8U, dy);
        put32(slide + slide_y_step, 0x100);
        put32(slide + slide_x_step, step);
    } else {
        const auto step = quotient(dy << 8U, dx);
        put32(slide + slide_x_step, 0x100);
        put32(slide + slide_y_step, step);
    }
    put8(slide + slide_steps, a4);
    put32(slide + slide_x, 0);
    put32(slide + slide_y, 0);
    put8(slide + slide_done, 0);
}

// 801c8324: advance slide `a0` (a byte) by its steps for this frame, then set
// its done flag once the position along the longer axis (the one stepping
// 100) has passed its end.
void Overlay::step_slide(std::uint32_t a0) {
    const auto stack_frame = enter(0x8);
    const auto slide = at((a0 & 0xffU) * slide_record);
    const auto advance = [&](std::uint32_t offset, std::uint32_t step, std::uint32_t down) {
        put32(slide + offset, u8(slide + down) != 0 ? u32(slide + offset) - u32(slide + step)
                                                    : u32(slide + offset) + u32(slide + step));
    };
    for (std::uint32_t i = 0; i < u8(slide + slide_steps); ++i) {
        advance(slide_x, slide_x_step, slide_x_down);
        advance(slide_y, slide_y_step, slide_y_down);
    }
    const bool along_x = u32(slide + slide_x_step) == 0x100;
    const auto position = static_cast<std::int32_t>(
        whole(u32(slide + (along_x ? slide_x : slide_y))) +
        u32(slide + (along_x ? slide_x_from : slide_y_from)));
    const auto end = s32(slide + (along_x ? slide_x_to : slide_y_to));
    const bool down = u8(slide + (along_x ? slide_x_down : slide_y_down)) != 0;
    if (down ? position < end : end < position)
        put8(slide + slide_done, 1);
}

// 801c8574: play menu sound `a0` (Overlay::play_sound).
void Overlay::play_menu_sound(std::uint32_t a0) { play_sound(a0); }

// 801d28a8: open window slot 0 at (d4, b2), 60 x 10, and draw the amount
// window's number at (d8, b6) (801d5ba4).
void Overlay::open_amount_window() {
    const auto stack_frame = enter(0x30);
    open_window(0, 0xd4, 0xb2, 0x60, 0x10, 0, 0, 4, 0);
    draw_amount(0xd8, 0xb6);
}

// 801d28fc: open window slot 1 at (cc, c6), 50 x 10, draw the play time at
// (d0, ca) (801d5cf8) and set flag byte +6.
void Overlay::open_play_time_window() {
    const auto stack_frame = enter(0x30);
    open_window(1, 0xcc, 0xc6, 0x50, 0x10, 0, 0, 4, 0);
    draw_play_time(0xd0, 0xca);
    put8(u32(at(flags_block)) + 6, 1);
}

// 801d29a8: slide the three party panels in (`a0` nonzero) from off screen or
// back out, eight steps per frame, drawing each present member's panel
// (801d5a50) and running menu frames (801c7bf4) until one slide is done.
// Opening then snaps the panels to their places, draws them once more, plays
// sound 5d, opens the amount window unless `a1` (801d28a8) and sets flag
// bytes +21 and +6; closing clears flag bytes +2, +1, +0 and, unless `a1`,
// +20 and +5. Either ends with one more frame.
void Overlay::slide_party_panels(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x30);
    const bool opening = (a0 & 0xffU) != 0;
    const bool keep_windows = (a1 & 0xffU) != 0;
    // The panels' shown places (x, y) and their place off screen.
    constexpr std::uint32_t shown[3][2] = {{0x60, 6}, {0x68, 0x3e}, {0x70, 0x76}};
    constexpr std::uint32_t hidden[3][2] = {{0x100, 0x86}, {0x108, 0x3e}, {0x110, 0xfffffff6U}};
    if (opening) {
        // The callee ignores the fourth register (flag block + c).
        layout_labels_row4(8, at(0x6e0), 0x801ea528);
    } else {
        clear_bytes(8, u32(at(flags_block)) + 0xc);
    }
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto &from = opening ? hidden[slot] : shown[slot];
        const auto &to = opening ? shown[slot] : hidden[slot];
        start_slide(from[0], from[1], to[0], to[1], 8, slot);
    }
    const auto member = [&](std::uint32_t slot) {
        return u8(u32(at(flags_block)) + party_members + slot);
    };
    const auto draw_panels = [&] {
        for (std::uint32_t slot = 0; slot < 3; ++slot)
            if (const auto m = member(slot); m != 0xff)
                draw_party_panel(slot, m);
    };
    const auto any_done = [&] {
        return u8(at(slide_done)) != 0 || u8(at(slide_record + slide_done)) != 0 ||
               u8(at(2 * slide_record + slide_done)) != 0;
    };
    while (!any_done()) {
        draw_panels();
        menu_frame();
        for (std::uint32_t slot = 0; slot < 3; ++slot)
            if (member(slot) != 0xff)
                step_slide(slot);
    }
    if (opening) {
        const auto state_block = state();
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            put32(state_block + slot * slide_record + slide_x_from, shown[slot][0]);
            put32(state_block + slot * slide_record + slide_y_from, shown[slot][1]);
        }
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            put32(state_block + slot * slide_record + slide_y, 0);
            put32(state_block + slot * slide_record + slide_x, 0);
        }
        draw_panels();
        play_menu_sound(0x5d);
        if (!keep_windows)
            open_amount_window();
        put8(u32(at(flags_block)) + 0x21, 1);
        put8(u32(at(flags_block)) + 6, 1);
    } else {
        const auto flags = u32(at(flags_block));
        put8(flags + 2, 0);
        put8(flags + 1, 0);
        put8(flags + 0, 0);
        if (!keep_windows) {
            put8(u32(at(flags_block)) + 0x20, 0);
            put8(u32(at(flags_block)) + 5, 0);
        }
    }
    menu_frame();
}

// 801d397c: open window slot `a0` (a byte) at (a1, a2), a3 x a4 (halfwords).
// Slots 2 and up first get a cleared 720-byte packet block (+364) and a
// cleared 18-byte record (+380), set up by 801e53cc. With `a5` the record is
// filled directly (slot +10, 0 +11, the rectangle +0..+6, zero +8/+a, `a6`
// +12, `a7` +c) and flag byte +27 + slot set; otherwise 801d4d1c builds the
// window with a6, a7 and a8.
void Overlay::open_window(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7,
                        std::uint32_t a8) {
    const auto stack_frame = enter(0x58);
    const auto slot = a0 & 0xffU;
    const auto height = a4 & 0xffffU;
    if (slot >= 2) {
        const auto packets = allocate(0x720, 0, 0x801d39e4);
        put32(at(window_blocks + slot * 4), packets);
        bzero(packets, 0x720);
        const auto record = allocate(0x18, 0, 0x801d3a10);
        put32(at(window_records + slot * 4), record);
        bzero(record, 0x18);
        prepare_window_packets(slot);
    }
    const auto record = u32(at(window_records + slot * 4));
    if ((a5 & 0xffU) != 0) {
        put8(record + 0x10, a0);
        put8(record + 0x11, 0);
        put16(record + 0, a1);
        put16(record + 2, a2);
        put16(record + 4, a3);
        put16(record + 6, height);
        put16(record + 8, 0);
        put16(record + 0xa, 0);
        put8(u32(at(flags_block)) + slot + 0x27, 1);
        put8(record + 0x12, a6);
        put32(record + 0xc, a7);
    } else {
        build_panel(slot, a1 & 0xffffU, a2 & 0xffffU, a3 & 0xffffU, height, a6 & 0xffU, a7,
                  a8 & 0xffU);
    }
}

// 801d4f2c: panel `a0`'s portrait frame and portrait at (a2, a3): bank
// sprite 14b + slot, then the portrait quad (packet +50 of the panel block,
// per buffer) with texture page (0, 0, 180, 0), the CLUT by the member's low
// bit, and the slot's texture corner (801ea578, 801ea5c4) at the offset
// (801e9b58, 801e9b5c), 48 x d (801e920c). `a1` is not read.
void Overlay::draw_panel_portrait(std::uint32_t a0, std::uint32_t, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto slot = a0 & 0xffU;
    const auto block = u32(at(panel_blocks + slot * 4));
    static_cast<void>(sprite(*this, slot + 0x14b, block, a2, a3));
    init_text_quad(block + u32(at(buffer_index)) * packet_size + 0x50);
    const auto tpage = get_tpage(0, 0, 0x180, 0);
    put16(block + u32(at(buffer_index)) * packet_size + 0x66, tpage);
    const auto portrait = block + u32(at(buffer_index)) * packet_size;
    const auto member = u8(u32(at(flags_block)) + party_members + slot);
    put16(portrait + 0x5e, u16((member & 1U) != 0 ? odd_clut : even_clut));
    set_quad_rect(block + u32(at(buffer_index)) * packet_size + 0x50,
              (u16(0x801e9b58) + a2) & 0xffffU, (u16(0x801e9b5c) + a3) & 0xffffU,
              (u32(0x801ea578 + slot * 4) << 2U) & 0xfcU, u8(0x801ea5c4 + slot * 4), 0x48, 0xd);
}

// 801d50ec: panel `a0`'s twenty label sprites: the bank sprite of each word
// of 801ea34c other than ffff at (a1, a2) plus the offsets 801e9a78 (x) and
// 801e9ac8 (y), into the panel block from +a0, counted in +1279.
void Overlay::draw_panel_labels(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x38);
    const auto block = u32(at(panel_blocks + (a0 & 0xffU) * 4));
    put8(block + 0x1279, 0);
    for (std::uint32_t i = 0; i < 20; ++i) {
        const auto label = u32(0x801ea34c + i * 4);
        if (label == 0xffff)
            continue;
        const auto used = sprite(*this, label, block + 0xa0 + u8(block + 0x1279) * sprite_size,
                                 a1 + u32(0x801e9a78 + i * 4), a2 + u32(0x801e9ac8 + i * 4));
        put8(block + 0x1279, u8(block + 0x1279) + used);
    }
}

// 801d51ec: member `a1`'s record halfwords +4c and +4e as three-digit numbers
// on panel `a0` at (a2, a3) plus the offsets 801e9b28/801e9b30: the first by
// digit position (sprites from +af0, counted in +1273), the second packed
// (from +be0, counted in +1274).
void Overlay::draw_panel_numbers_4c_4e(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto record = character_record(a1);
    const auto block = u32(at(panel_blocks + (a0 & 0xffU) * 4));
    split_decimal_digits(u16(record + 0x4c));
    draw_digits(*this, block, 0x1273, 0xaf0, digits + 6, 3, a2 + u32(0x801e9b28),
                a3 + u32(0x801e9b2c), false);
    split_decimal_digits(u16(record + 0x4e));
    draw_digits(*this, block, 0x1274, 0xbe0, digits + 6, 3, a2 + u32(0x801e9b30),
                a3 + u32(0x801e9b34), true);
}

// 801d53d0: member `a1`'s record halfwords +50 and +52 as two-digit numbers on
// panel `a0` at (a2, a3) plus 801e9b38/801e9b40: the first by digit position
// (from +cd0, counted in +1275), the second packed (from +d70, in +1276).
void Overlay::draw_panel_numbers_50_52(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto record = character_record(a1);
    const auto block = u32(at(panel_blocks + (a0 & 0xffU) * 4));
    split_decimal_digits(u16(record + 0x50));
    draw_digits(*this, block, 0x1275, 0xcd0, digits + 7, 2, a2 + u32(0x801e9b38),
                a3 + u32(0x801e9b3c), false);
    split_decimal_digits(u16(record + 0x52));
    draw_digits(*this, block, 0x1276, 0xd70, digits + 7, 2, a2 + u32(0x801e9b40),
                a3 + u32(0x801e9b44), true);
}

// 801d55b4: member `a1`'s record words +44 and +48 as seven-digit numbers on
// panel `a0` at (a2, a3) plus 801e9b48/801e9b50, by digit position (from
// +e10 counted in +1277, from +1040 counted in +1278).
void Overlay::draw_panel_numbers_44_48(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto record = character_record(a1);
    const auto block = u32(at(panel_blocks + (a0 & 0xffU) * 4));
    split_decimal_digits(u32(record + 0x44));
    draw_digits(*this, block, 0x1277, 0xe10, digits + 2, 7, a2 + u32(0x801e9b48),
                a3 + u32(0x801e9b4c), false);
    split_decimal_digits(u32(record + 0x48));
    draw_digits(*this, block, 0x1278, 0x1040, digits + 2, 7, a2 + u32(0x801e9b50),
                a3 + u32(0x801e9b54), false);
}

// 801d5794: member `a1`'s record bytes +62 and +63 as three-digit numbers on
// panel `a0` at (a2, a3) plus 801e9b18/801e9b20, by digit position (from
// +910 counted in +1271, from +a00 counted in +1272); the second number's
// packets are then tinted: texture shading on, color (0, 80, 0).
void Overlay::draw_panel_numbers_62_63(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x40);
    const auto record = character_record(a1);
    const auto block = u32(at(panel_blocks + (a0 & 0xffU) * 4));
    split_decimal_digits(u8(record + 0x62));
    draw_digits(*this, block, 0x1271, 0x910, digits + 6, 3, a2 + u32(0x801e9b18),
                a3 + u32(0x801e9b1c), false);
    split_decimal_digits(u8(record + 0x63));
    draw_digits(*this, block, 0x1272, 0xa00, digits + 6, 3, a2 + u32(0x801e9b20),
                a3 + u32(0x801e9b24), false);
    for (std::uint32_t i = 0; i < u8(block + 0x1272); ++i) {
        const auto packet = [&] { return block + 0xa00 + (i * 2 + u32(at(buffer_index))) * packet_size; };
        set_shade_tex(packet(), 0);
        put8(packet() + 4, 0);
        put8(packet() + 5, 0x80);
        put8(packet() + 6, 0);
    }
}

// 801d5a50: draw panel `a0` (a byte; ff: none) for member `a1` at its slide
// position (the whole part of the accumulated offsets plus the start): the
// portrait, labels and numbers (801d4f2c..801d5794); then set the slot's
// flag byte and record the buffer drawn in the panel block (+1270).
void Overlay::draw_party_panel(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x28);
    const auto slot = a0 & 0xffU;
    if (slot == 0xff)
        return;
    const auto slide = at(slot * slide_record);
    const auto block = u32(at(panel_blocks + slot * 4));
    const auto x = whole(u32(slide + slide_x)) + u32(slide + slide_x_from);
    const auto y = whole(u32(slide + slide_y)) + u32(slide + slide_y_from);
    const auto member = a1 & 0xffU;
    draw_panel_portrait(slot, member, x, y);
    draw_panel_labels(slot, x, y);
    draw_panel_numbers_4c_4e(slot, member, x, y);
    draw_panel_numbers_50_52(slot, member, x, y);
    draw_panel_numbers_44_48(slot, member, x, y);
    draw_panel_numbers_62_63(slot, member, x, y);
    put8(u32(at(flags_block)) + slot, 1);
    put8(block + 0x1270, u8(at(buffer_index)));
}

// 801d5ba4: the amount (word 8006ef58) as nine digit sprites at (a0 + 8 per
// digit position, a1) into the amount block (counted in the word +320), the
// bank sprite 10 at (a0 + 50, a1) at +2d0; set flag byte +5 and record the
// buffer drawn (+324).
void Overlay::draw_amount(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x38);
    split_decimal_digits(u32(0x8006ef58));
    put32(u32(at(amount_block)) + 0x320, 0);
    for (std::uint32_t i = 0; i < 9; ++i) {
        const auto digit = u8(at(digits + i));
        if (digit == 0xff)
            continue;
        const auto block = u32(at(amount_block));
        const auto used = sprite(*this, digit, block + u32(block + 0x320) * sprite_size,
                                 a0 + i * 8, a1);
        const auto counted = u32(at(amount_block));
        put32(counted + 0x320, used + u32(counted + 0x320));
    }
    static_cast<void>(sprite(*this, 0x10, u32(at(amount_block)) + 0x2d0, a0 + 0x50, a1));
    put8(u32(at(flags_block)) + 5, 1);
    put8(u32(at(amount_block)) + 0x324, u8(at(buffer_index)));
}

// 801d5cf8: the play time (state words 2ec..304) as digit sprites at a1 and
// x = a0 + 0, 8, 10 (hours), 20, 28 (minutes), 38, 40 (seconds), sprite i at
// +50 * i of the play-time block, with separators (bank sprite ee) at a0 +
// 18 (+230) and a0 + 30 (+2d0); record the buffer drawn (+370).
void Overlay::draw_play_time(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x40);
    constexpr std::uint32_t column[7] = {0, 8, 0x10, 0x20, 0x28, 0x38, 0x40};
    for (std::uint32_t i = 0; i < 7; ++i)
        static_cast<void>(sprite(*this, u32(at(play_digits + i * 4)),
                                 u32(at(time_block)) + i * sprite_size, a0 + column[i], a1));
    static_cast<void>(sprite(*this, 0xee, u32(at(time_block)) + 0x230, a0 + 0x18, a1));
    static_cast<void>(sprite(*this, 0xee, u32(at(time_block)) + 0x2d0, a0 + 0x30, a1));
    put8(u32(at(time_block)) + 0x370, u8(at(buffer_index)));
}

// 801d5ed4: the detail panel's portrait for panel slot `a0` in layout `a1`
// (zero: the character; otherwise the gear, 18 further left): the frame
// sprite 14b + slot at (18 - shift, e) with its screen rectangle at +1ea0
// (30 x 30, 801c851c); the portrait quad at +50 per buffer with texture page
// (0, 0, 180, 0), the CLUT by the member's low bit (layout 0) or by the low
// bit of its record byte +a0 plus b, the texture corner (801ea578,
// 801ea5c4)[a1 * 3 + a0] and extents (a1 * 18 + 48, d) (801e920c); and the
// portrait's screen rectangle at +1ec0 ((801e9d38) - shift, (801e9d3c)).
void Overlay::draw_detail_portrait(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x38);
    const auto slot = a0 & 0xffU;
    const auto layout = a1 & 0xffU;
    const auto shift = layout != 0 ? 0x18U : 0U;
    static_cast<void>(sprite(*this, slot + 0x14b, u32(at(detail_block)), 0x18 - shift, 0xe));
    {
        const auto block = u32(at(detail_block));
        const auto frame_packet = block + u32(at(buffer_index)) * packet_size;
        set_screen_quad_vectors(block + 0x1ea0, u16(frame_packet + 8), u16(frame_packet + 0xa), 0x30, 0x30);
    }
    init_text_quad(u32(at(buffer_index)) * packet_size + 0x50 + u32(at(detail_block)));
    const auto tpage = get_tpage(0, 0, 0x180, 0);
    put16(u32(at(detail_block)) + u32(at(buffer_index)) * packet_size + 0x66, tpage);
    const auto portrait = u32(at(detail_block)) + u32(at(buffer_index)) * packet_size;
    const auto member = u8(u32(at(flags_block)) + party_members + slot);
    const auto odd = layout == 0 ? (member & 1U) != 0
                                 : ((u8(character_record(member) + record_gear) + 0xbU) &
                                    1U) != 0;
    put16(portrait + 0x5e, u16(odd ? odd_clut : even_clut));
    const auto corner = (layout * 3 + slot) * 4;
    const auto extent = (layout * 0x18 + 0x48) & 0xffffU;
    set_quad_rect(u32(at(buffer_index)) * packet_size + 0x50 + u32(at(detail_block)), 0, 0,
              (u32(0x801ea578 + corner) << 2U) & 0xfcU, u8(0x801ea5c4 + corner), extent, 0xd);
    set_screen_quad_vectors(u32(at(detail_block)) + 0x1ec0, (u16(0x801e9d38) - shift) & 0xffffU,
              u16(0x801e9d3c), extent, 0xd);
}

// 801d680c: the detail panel's two numbers for panel slot `a0` in layout
// `a1`: layout 0 shows the member's record halfwords +50 and +52 as two
// digits, otherwise the halfwords 8006dfe4 and 8006dfe6 + a4 * (record byte
// +a0) as four digits. The first is drawn by digit position at (801e9d00 - a1 *
// 18, 801e9d04 (+8 with gear)) from +1950 (counted in +2ae9), the second
// packed at (801e9d08 - a1 * 18 (-20 with gear), 801e9d0c (+10 with gear))
// from +1ae0 (counted in +2aea); each number's sprites then get screen
// rectangles (801c851c) at +28c0 / +2960, 20 bytes apart.
void Overlay::draw_detail_numbers(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x68);
    const auto slot = a0 & 0xffU;
    const auto layout = a1 & 0xffU;
    const bool gear = layout != 0;
    const auto count = gear ? 4U : 2U;
    const auto first = gear ? digits + 5 : digits + 7;
    const auto shift = layout * 0x18;
    const auto member = [&] { return u8(u32(at(flags_block)) + party_members + slot); };
    const auto gear_record = [&] {
        return gear_numbers + u8(character_record(member()) + record_gear) * character_stride;
    };
    const auto rectangles = [&](std::uint32_t counter, std::uint32_t packets,
                                std::uint32_t rects) {
        for (std::uint32_t i = 0; i < u8(u32(at(detail_block)) + counter); ++i) {
            const auto block = u32(at(detail_block));
            const auto packet = block + packets + (i * 2 + u32(at(buffer_index))) * packet_size;
            set_screen_quad_vectors(block + rects + i * 0x20, u16(packet + 8), u16(packet + 0xa),
                      (u16(packet + 0x10) - u16(packet + 8)) & 0xffffU,
                      (u16(packet + 0x22) - u16(packet + 0xa)) & 0xffffU);
        }
    };

    std::uint32_t x = u32(0x801e9d00);
    std::uint32_t y = u32(0x801e9d04);
    if (gear) {
        split_decimal_digits(u16(gear_record()));
        y += 8;
    } else {
        split_decimal_digits(u16(character_record(member()) + 0x50));
    }
    draw_digits(*this, u32(at(detail_block)), 0x2ae9, 0x1950, first, count, x - shift, y, false);
    rectangles(0x2ae9, 0x1950, 0x28c0);

    if (gear) {
        split_decimal_digits(u16(gear_record() + 2));
        x = u32(0x801e9d08) - 0x20;
        y = u32(0x801e9d0c) + 0x10;
    } else {
        split_decimal_digits(u16(character_record(member()) + 0x52));
        x = u32(0x801e9d08);
        y = u32(0x801e9d0c);
    }
    draw_digits(*this, u32(at(detail_block)), 0x2aea, 0x1ae0, first, count, x - shift, y, true);
    rectangles(0x2aea, 0x1ae0, 0x2960);
}

// 801e56e8: file slot `a0`'s marker packets at its place (801e9894,
// 801e9914)[a0]: per buffer a 10 x 10 textured quad (+0, +28; page (0, 0,
// 140, 80)) and a semi-transparent flat quad in grey 80 (+b0, +c8), each
// with the screen rectangle at +e0 (801c851c); then two draw modes (+140,
// +14c) for page (0, 2, 140, 80) with the texture window (0, 0, 100, 100).
void Overlay::build_file_slot_marker(std::uint32_t a0) {
    const auto stack_frame = enter(0x48);
    const auto place_x = 0x801e9894 + a0 * 4;
    const auto place_y = 0x801e9914 + a0 * 4;
    const auto block = u32(at(slot_blocks + a0 * 4));
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto textured = block + i * packet_size;
        init_text_quad(textured);
        quad_corners(*this, textured, u16(place_x), u16(place_y), 0x10, 0x10);
        put16(textured + 0x16, get_tpage(0, 0, 0x140, 0x80));
        const auto flat = block + 0xb0 + i * 0x18;
        set_poly_f4(flat);
        put8(flat + 4, 0x80);
        put8(flat + 5, 0x80);
        put8(flat + 6, 0x80);
        set_semi_trans(flat, 1);
        put16(flat + 0x8, u16(place_x));
        put16(flat + 0xa, u16(place_y));
        put16(flat + 0xc, u16(place_x) + 0x10);
        put16(flat + 0xe, u16(place_y));
        put16(flat + 0x10, u16(place_x));
        put16(flat + 0x12, u16(place_y) + 0x10);
        put16(flat + 0x14, u16(place_x) + 0x10);
        put16(flat + 0x16, u16(place_y) + 0x10);
        set_screen_quad_vectors(block + 0xe0, u16(place_x), u16(place_y), 0x10, 0x10);
    }
    auto f = frame(0x48);
    const auto window = f[0x18];
    put16(window + 2, 0);
    put16(window + 0, 0);
    put16(window + 6, 0x100);
    put16(window + 4, 0x100);
    set_draw_mode(block + 0x140, 0, 0, get_tpage(0, 2, 0x140, 0x80) & 0xffffU, window);
    set_draw_mode(block + 0x14c, 0, 0, get_tpage(0, 2, 0x140, 0x80) & 0xffffU, window);
}

// 801e5924: file slot `a0`'s outline at its place: per buffer two green
// (0, ff, 0) three-point lines, top-right (+50, +68: (x, y), (x+10, y),
// (x+10, y+10)) and left-bottom (+80, +98: (x, y), (x, y+10), (x+10, y+10)),
// with their screen rectangles at +100 and +120 (801c851c).
void Overlay::build_file_slot_outline(std::uint32_t a0) {
    const auto stack_frame = enter(0x40);
    const auto place_x = 0x801e9894 + a0 * 4;
    const auto place_y = 0x801e9914 + a0 * 4;
    const auto block = u32(at(slot_blocks + a0 * 4));
    const auto line = [&](std::uint32_t packet, std::uint32_t corner_x, std::uint32_t corner_y,
                          std::uint32_t rect) {
        set_line_f3(packet);
        put8(packet + 4, 0);
        put8(packet + 5, 0xff);
        put8(packet + 6, 0);
        put16(packet + 0x8, u16(place_x));
        put16(packet + 0xa, u16(place_y));
        put16(packet + 0xc, u16(place_x) + corner_x);
        put16(packet + 0xe, u16(place_y) + corner_y);
        put16(packet + 0x10, u16(place_x) + 0x10);
        put16(packet + 0x12, u16(place_y) + 0x10);
        set_screen_quad_vectors(block + rect, u16(place_x), u16(place_y), 0x10, 0x10);
    };
    for (std::uint32_t i = 0; i < 2; ++i) {
        line(block + 0x50 + i * 0x18, 0x10, 0, 0x100);
        line(block + 0x80 + i * 0x18, 0, 0x10, 0x120);
    }
}

// 801e5acc: the 32 file slots: each a cleared 158-byte block (+3a8 + slot *
// 4) with its marker (801e56e8) and outline (801e5924) packets.
void Overlay::build_file_slots() {
    const auto stack_frame = enter(0x18);
    for (std::uint32_t slot = 0; slot < 32; ++slot) {
        const auto block = allocate(0x158, 0, 0x801e5ae0);
        put32(at(slot_blocks + slot * 4), block);
        bzero(block, 0x158);
        build_file_slot_marker(slot);
        build_file_slot_outline(slot);
    }
}

// 801e5b88: the file-select block's 32 glyph quads, per buffer (+28 * (2 *
// i + buffer)): glyph i at column i % 21 (801e9994) + 8 per row, row i / 21
// (801e99e8), c x 10, textured from (i % 16 * 10, i / 16 * 10 - 20) (the
// lower row edge at - 11), page (0, 0, 140, 80), CLUT (0, 1c0).
void Overlay::build_file_glyphs() {
    const auto stack_frame = enter(0x30);
    for (std::uint32_t i = 0; i < 32; ++i) {
        const auto row = i / 21;
        const auto column_x = 0x801e9994 + (i - row * 21) * 4;
        const auto row_y = 0x801e99e8 + row * 4;
        const auto u = (i % 16) * 16;
        const auto v = (i / 16) * 16;
        for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
            const auto offset = (i * 2 + buffer) * packet_size;
            const auto packet = [&] { return u32(at(file_block)) + offset; };
            init_text_quad(packet());
            put16(packet() + 0x8, u16(column_x) + row * 8);
            put16(packet() + 0xa, u16(row_y));
            put16(packet() + 0x10, u16(column_x) + row * 8 + 0xc);
            put16(packet() + 0x12, u16(row_y));
            put16(packet() + 0x18, u16(column_x) + row * 8);
            put16(packet() + 0x1a, u16(row_y) + 0x10);
            put16(packet() + 0x20, u16(column_x) + row * 8 + 0xc);
            put16(packet() + 0x22, u16(row_y) + 0x10);
            put8(packet() + 0xc, u);
            put8(packet() + 0xd, v - 0x20);
            put8(packet() + 0x14, u + 0xc);
            put8(packet() + 0x15, v - 0x20);
            put8(packet() + 0x1c, u);
            put8(packet() + 0x1d, v - 0x11);
            put8(packet() + 0x24, u + 0xc);
            put8(packet() + 0x25, v - 0x11);
            put16(packet() + 0x16, get_tpage(0, 0, 0x140, 0x80));
            put16(packet() + 0xe, get_clut(0, 0x1c0));
        }
    }
}

// 801e5e4c: the file-select banner, per buffer: a 20 x 20 textured quad at
// (801e99f0, 801e99f8) (+a00 + 28 * buffer; page (0, 0, 140, 80)) and a
// Gouraud band from (0, 4a) to (140, 8a) (+a50 + 24 * buffer) with corner
// colors (80, 0, 80), (0, 0, 80) above and (10, 0, 10), (0, 0, 10) below.
void Overlay::build_file_banner() {
    const auto stack_frame = enter(0x38);
    for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
        const auto quad = [&] { return u32(at(file_block)) + 0xa00 + buffer * packet_size; };
        init_text_quad(quad());
        quad_corners(*this, quad(), u16(0x801e99f0), u16(0x801e99f8), 0x20, 0x20);
        put16(quad() + 0x16, get_tpage(0, 0, 0x140, 0x80));
        const auto band = [&] { return u32(at(file_block)) + 0xa50 + buffer * 0x24; };
        set_poly_g4(band());
        constexpr std::uint8_t colors[4][3] = {
            {0x80, 0, 0x80}, {0, 0, 0x80}, {0x10, 0, 0x10}, {0, 0, 0x10}};
        for (std::uint32_t corner = 0; corner < 4; ++corner)
            for (std::uint32_t c = 0; c < 3; ++c)
                put8(band() + 4 + corner * 8 + c, colors[corner][c]);
        constexpr std::uint32_t points[4][2] = {{0, 0x4a}, {0x140, 0x4a}, {0, 0x8a}, {0x140, 0x8a}};
        for (std::uint32_t corner = 0; corner < 4; ++corner) {
            put16(band() + 8 + corner * 8, points[corner][0]);
            put16(band() + 0xa + corner * 8, points[corner][1]);
        }
    }
}

// 801e6450: the file-select panel block: a cleared 2dc0-byte block (+34c)
// with the glyph quads (801e5b88) and banner (801e5e4c).
void Overlay::build_file_select_panels() {
    const auto stack_frame = enter(0x20);
    const auto block = allocate(0x2dc0, 0, 0x801e645c);
    put32(at(file_block), block);
    bzero(block, 0x2dc0);
    build_file_glyphs();
    build_file_banner();
}

} // namespace xem::reconstruction::menu
