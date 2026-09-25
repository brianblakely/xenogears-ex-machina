// Menu overlay 82f84a24... (801c5000): the equipment screen (801e0f78,
// 801e05d0) with its part panel, candidate list, part description and stat
// bars, the party and cursor sprites it shares with the other screens, and the
// menu data loader 801c72bc.
#include "xem/reconstruction/menu_overlay.hpp"
#include "xem/reconstruction/menu_save.hpp"
#include "xem/reconstruction/resident_text.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace xem::reconstruction::menu {
namespace {

// Menu state fields (offsets from *800625a0).
constexpr std::uint32_t glyph_table = 0x2dc;    // sprite table of the menu glyphs
constexpr std::uint32_t buffer_index = 0x308;   // 0/1: the draw buffer being built
constexpr std::uint32_t digits = 0x31c;         // 801c80b8's nine decimal digit glyphs
constexpr std::uint32_t input_code = 0x325;     // decoded input of this frame
constexpr std::uint32_t input_wait = 0x329;     // u8: nonzero while an input fade runs
constexpr std::uint32_t list_visible = 0x32b;   // u8: 1 hides the party sprites (a1 0)
constexpr std::uint32_t list_visible_2 = 0x33b; // u8: the same for a1 nonzero
constexpr std::uint32_t stats_block = 0x35c;    // stat-bar block (32f4 bytes)
constexpr std::uint32_t window_block = 0x428;   // window set (+148 current, +140 cursor)
constexpr std::uint32_t loaded_0 = 0x42c;       // block with loaded data at +1180
constexpr std::uint32_t loaded_1 = 0x430;       // block with loaded data at +1080
constexpr std::uint32_t list_block = 0x434;     // equipment list block (a1c bytes)
constexpr std::uint32_t loaded_2 = 0x438;       // block with loaded data at +2578
constexpr std::uint32_t title_block = 0x43c;    // title sprite block (74 bytes)
constexpr std::uint32_t party_sprites = 0x440;  // party sprite block (1c4 bytes)
constexpr std::uint32_t cursor_blocks = 0x444;  // cursor sprite blocks (78 bytes), 4 each
constexpr std::uint32_t text_blocks = 0x364;    // per index: text block, released by 801d4ea0
constexpr std::uint32_t text_blocks_2 = 0x380;  // per index: second text block

// Party list fields (*(state + 33c)).
constexpr std::uint32_t members = 0x30;     // 3 character ids, ff empty
constexpr std::uint32_t shown_flags = 0x40; // 6 bytes cleared by 801de474

// Equipment screen state (*(state + 360)) beyond menu.hpp's kept parts.
constexpr std::uint32_t kept_weapon = 0x29c;    // +0..4: record bytes 6a..6e
constexpr std::uint32_t kept_special = 0x2a1;   // +0..4: record bytes 6f..73
constexpr std::uint32_t kept_accessory = 0x2a6; // +0..4: record bytes 74..78
constexpr std::uint32_t shown_stats = 0x280;    // 9 halfwords: the stats before a change

// Character and gear records.
constexpr std::uint32_t record_weapon = 0x6a;
constexpr std::uint32_t record_special = 0x6f;
constexpr std::uint32_t record_accessories = 0x74;
constexpr std::uint32_t record_gear = 0xa0;        // u8: gear record index, ff none
constexpr std::uint32_t gear_records = 0x8006dfb4; // a4 bytes each: accessories +1..3, weapon +4
constexpr std::uint32_t gear_stride = 0xa4;

// Candidate list of the equipment screen: ids and counts, 200 each.
constexpr std::uint32_t candidate_ids = 0x801ea730;
constexpr std::uint32_t candidate_counts = 0x801ea7f8;

// 801d84b4's bar results.
constexpr std::uint32_t bar_value = 0x801ea6fc;
constexpr std::uint32_t bar_preview = 0x801ea700;
constexpr std::uint32_t bar_change = 0x801ea704; // |preview - value|
constexpr std::uint32_t bar_width = 0x801ea708;  // value's bar length
constexpr std::uint32_t bar_change_width = 0x801ea70c;
constexpr std::uint32_t bar_colour = 0x801ea710; // u8: 2 gain, 3 loss
constexpr std::uint32_t bar_sign = 0x801ea714;   // u8: glyph e3 (+) or e5 (-)

constexpr std::uint32_t full_scale = 0x1000; // 8002675c's sprite scale 1.0

std::int32_t s(std::uint32_t value) { return static_cast<std::int32_t>(value); }
std::uint32_t u(std::int32_t value) { return static_cast<std::uint32_t>(value); }

// MIPS DIV: the quotient truncates toward zero; a zero divisor leaves -1 (or
// 1 for a negative dividend) in LO (general R3000 behaviour).
std::int32_t divide(std::int32_t dividend, std::int32_t divisor) {
    if (divisor == 0)
        return dividend >= 0 ? -1 : 1;
    if (divisor == -1)
        return s(0U - u(dividend));
    return dividend / divisor;
}

// value * 100 / 10000 by the compiler's reciprocal (mult 68db8bad, sra 12,
// minus the sign): value * 0.64 truncated, over a wrapped 32-bit product.
std::int32_t percent_of_64(std::int32_t value) {
    const auto product = s(u(value) * 6400U);
    const auto high = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(product) * static_cast<std::int64_t>(0x68db8bad)) >> 32);
    return (high >> 12) - (product >> 31);
}

std::uint32_t buffer(const Overlay &o) { return o.u32(o.at(buffer_index)); }
std::uint32_t party(const Overlay &o) { return o.u32(o.at(state_party)); }
std::uint32_t tables(const Overlay &o) { return o.u32(o.at(state_tables)); }
std::uint32_t screen(const Overlay &o) { return o.u32(o.at(state_equip_screen)); }
std::uint32_t stats(const Overlay &o) { return o.u32(o.at(stats_block)); }
std::uint32_t list(const Overlay &o) { return o.u32(o.at(list_block)); }
std::uint32_t member(const Overlay &o, std::uint32_t slot) {
    return o.u8(party(o) + members + (slot & 0xffU));
}
std::uint32_t gear_index(const Overlay &o, std::uint32_t character) {
    return o.u8(character_record(character) + record_gear);
}
std::uint32_t gear_record(std::uint32_t index) { return gear_records + index * gear_stride; }

// The two buffered 40-byte quads of sprite part `part` at `base`.
std::uint32_t quad(const Overlay &o, std::uint32_t base, std::uint32_t part) {
    return base + (part * 2 + buffer(o)) * 40;
}

// 801c851c(target, x, y, w, h) with this buffer's quad of `packet`: its first
// vertex offset by (dx, dy) and its size (vertex 1 x, vertex 3 y).
void place_quad(Overlay &o, std::uint32_t target, std::uint32_t packet, std::uint32_t dx = 0,
                std::uint32_t dy = 0) {
    const auto x = o.u16(packet + 8);
    const auto y = o.u16(packet + 10);
    o.set_screen_quad_vectors(target, (x + dx) & 0xffffU, (y + dy) & 0xffffU, (o.u16(packet + 16) - x) & 0xffffU,
                (o.u16(packet + 34) - y) & 0xffffU);
}

// A 40 x 13 texture image of the text buffer at VRAM (x, y).
void load_text_image(Overlay &o, std::uint32_t x, std::uint32_t y, std::uint32_t text) {
    // The rectangle is at SP + 18 of each caller (801d8ea4, 801de5cc, 801dff5c).
    auto f = o.frame(0x20);
    o.put16(f[0x18], x);
    o.put16(f[0x1a], y);
    o.put16(f[0x1c], 0x28);
    o.put16(f[0x1e], 0xd);
    o.load_image(f[0x18], text);
    o.draw_sync();
}

// The resident sprite table lookup of the menu glyphs.
std::uint32_t build_sprite(Overlay &o, std::uint32_t id, std::uint32_t destination, std::uint32_t x,
                           std::uint32_t y) {
    return resident::sheet_quads(o, o.u32(o.at(glyph_table)), id, destination, buffer(o), x, y,
                                 full_scale);
}

} // namespace

// 801c72bc: load or release the menu data set `a0` (byte). Codes below 0x10
// first read file 2 of directory 0x10 into a new block (80028470, 800288ec,
// 800295d8, 80028a60) and relocate its offset table (8003342c); each case
// then unpacks (80032e88) the data it needs into the table directory (state
// + 330) and the screen blocks. Codes 0x10..0x17 release the same data (jump
// table 801c5048, 24 entries); the file block is released at the end.
void Overlay::load_menu_data_set(std::uint32_t a0) {
    const auto stack_frame = enter(0x28);
    const auto code = a0 & 0xffU;
    std::uint32_t file = 0;
    if (code < 0x10) {
        static_cast<void>(program.select_directory(0x10, 0));
        // 800288ec: the file's size rounded up to words (a signed quotient).
        const auto size = s(program.file_size(2));
        const auto rounded = size + 3 >= 0 ? size + 3 : size + 6;
        file = allocate(u(rounded >> 2) << 2U, 1, 0x801c72fc);
        static_cast<void>(program.read_file(2, file, 0, 0x80));
        program.disc_wait(0);
        static_cast<void>(resident::relocate_offsets(*this, file));
    }
    const auto unpack = [&](std::uint32_t entry) { // 80032e88(entry, 0)
        return resident::unpack_to_new_block(
            *this, u32(file + entry), 0, [&](std::uint32_t size, std::uint32_t mode) {
                return allocate(size, mode, resident::unpack_allocation_site);
            });
    };
    const auto set_table = [&](std::uint32_t offset, std::uint32_t entry) {
        const auto block = unpack(entry);
        put32(tables(*this) + offset, block);
    };
    const auto release_at = [&](std::uint32_t address, std::uint32_t site) {
        release(u32(address), site);
    };
    // Each present party member's own table (+20 + character * 4) from file
    // entry +10 + character * 4.
    const auto member_tables = [&](bool load, std::uint32_t site) {
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            const auto character = member(*this, slot);
            if (character == 0xff)
                continue;
            if (load)
                set_table(0x20 + character * 4, 0x10 + character * 4);
            else
                release_at(tables(*this) + 0x20 + character * 4, site);
        }
    };
    // Each present member's gear table (+4c + gear * 4) from entry +5c + gear * 4.
    const auto gear_tables = [&](bool load, std::uint32_t site) {
        for (std::uint32_t slot = 0; slot < 3; ++slot) {
            const auto character = member(*this, slot);
            if (character == 0xff || gear_index(*this, character) == 0xff)
                continue;
            const auto gear = gear_index(*this, character);
            if (!load) {
                release_at(tables(*this) + 0x4c + gear * 4, site);
                continue;
            }
            set_table(0x4c + gear * 4, 0x5c + gear * 4);
            missing("load_gear_table", 0x801c76a8, "symbol:menu-801e4998",
                    "801e4998 (gear table setup, outside the census) is not recovered");
        }
    };
    const auto put_loaded = [&](std::uint32_t block_field, std::uint32_t offset,
                                std::uint32_t entry) {
        const auto block = unpack(entry);
        put32(u32(at(block_field)) + offset, block);
    };
    switch (code) {
    case 0:
        set_table(0x1c, 4);
        put_loaded(loaded_0, 0x1180, 0x3c);
        break;
    case 1:
        set_table(8, 0x44);
        set_table(0x10, 0x48);
        set_table(0xc, 0x4c);
        set_table(0x14, 0x50);
        break;
    case 2:
        member_tables(true, 0);
        put_loaded(loaded_1, 0x1080, 0x40);
        break;
    case 3:
        set_table(0, 8);
        set_table(4, 0xc);
        set_table(0x18, 0xac);
        set_table(0x14, 0x50);
        break;
    case 4:
        member_tables(true, 0);
        put_loaded(loaded_2, 0x2578, 0xb0);
        break;
    case 5:
    case 6:
        gear_tables(true, 0);
        put_loaded(loaded_1, 0x1080, code == 5 ? 0x54 : 0x58);
        break;
    case 7:
        for (std::uint32_t i = 0; i < 4; ++i)
            put_loaded(list_block, 0xa00 + i * 4, 0xd4 + i * 4);
        break;
    case 0x10:
        release_at(tables(*this) + 0x1c, 0x801c77a4);
        release_at(u32(at(loaded_0)) + 0x1180, 0x801c7acc);
        break;
    case 0x11:
        release_at(tables(*this) + 8, 0x801c77e4);
        release_at(tables(*this) + 0x10, 0x801c7804);
        release_at(tables(*this) + 0xc, 0x801c7824);
        release_at(tables(*this) + 0x14, 0x801c7acc);
        break;
    case 0x12:
        member_tables(false, 0x801c788c);
        release_at(u32(at(loaded_1)) + 0x1080, 0x801c7acc);
        break;
    case 0x13:
        release_at(tables(*this), 0x801c78d8);
        release_at(tables(*this) + 4, 0x801c78f8);
        release_at(tables(*this) + 0x18, 0x801c7918);
        release_at(tables(*this) + 0x14, 0x801c7acc);
        break;
    case 0x14:
        member_tables(false, 0x801c7980);
        release_at(u32(at(loaded_2)) + 0x2578, 0x801c7acc);
        break;
    case 0x15:
    case 0x16:
        gear_tables(false, 0x801c7a20);
        release_at(u32(at(loaded_1)) + 0x1080, 0x801c7acc);
        break;
    case 0x17:
        release_at(u32(at(list_block)) + 0xa00, 0x801c7a6c);
        release_at(u32(at(list_block)) + 0xa04, 0x801c7a8c);
        release_at(u32(at(list_block)) + 0xa08, 0x801c7aac);
        release_at(u32(at(list_block)) + 0xa0c, 0x801c7acc);
        break;
    default: // 8..f, 18..: nothing
        break;
    }
    if (code < 0x10)
        release(file, 0x801c7ae4);
}

// 801c865c: the party-slot bit of slot `a1` (halfword table 801e96a8)
// within `a0`. (menu_items.cpp's use_item applies the same test.)
std::uint32_t Overlay::party_slot_bit(std::uint32_t a0, std::uint32_t a1) {
    return u16(0x801e96a8 + (a1 & 0xffU) * 2) & a0;
}

// 801d3344: the screen title sprite (glyph sprite 107) at (a0, a1): allocate
// its block (state + 43c, 74 bytes) once (party + 49), build the sprite and
// its quad (801c851c: w 8, h a2).
void Overlay::draw_screen_title(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x30);
    if (u8(party(*this) + 0x49) == 0) {
        const auto block = allocate(0x74, 0, 0x801d3380);
        put32(at(title_block), block);
        bzero(block, 0x74);
    }
    static_cast<void>(build_sprite(*this, 0x107, u32(at(title_block)), a0, a1));
    set_screen_quad_vectors(u32(at(title_block)) + 0x50, a0 & 0xffffU, a1 & 0xffffU, 8, a2 & 0xffffU);
    put8(u32(at(title_block)) + 0x70, u8(at(buffer_index)));
    put8(party(*this) + 0x49, 1);
}

// 801d3444: drop the title sprite (party + 49) and release its block.
void Overlay::release_screen_title() {
    const auto stack_frame = enter(0x18);
    put8(party(*this) + 0x49, 0);
    release(u32(at(title_block)), 0x801d3470);
}

// 801d3488: the two party-window sprites (glyph sprites 164 and 165 at the
// positions of words 801ea164 and the row word 801ea16c + a0 * 4) unless
// the list flag (state + 32b, or + 33b when a1) is 1. The block (state +
// 440, 1c4 bytes) is allocated once (party + 67); four quads follow.
void Overlay::draw_party_window_sprites(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x30);
    const auto hidden = u8(at((a1 & 0xffU) == 0 ? list_visible : list_visible_2));
    if (hidden == 1)
        return;
    if (u8(party(*this) + 0x67) == 0) {
        const auto block = allocate(0x1c4, 0, 0x801d3504);
        put32(at(party_sprites), block);
        bzero(block, 0x1c4);
        put8(party(*this) + 0x67, 1);
    }
    const auto row = u32(0x801ea16c + (a0 & 0xffU) * 4);
    for (std::uint32_t i = 0; i < 2; ++i)
        static_cast<void>(build_sprite(*this, 0x164 + i, u32(at(party_sprites)) + i * 0xa0,
                                       u32(0x801ea164 + i * 4), row));
    for (std::uint32_t i = 0; i < 4; ++i) {
        const auto block = u32(at(party_sprites));
        place_quad(*this, block + 0x140 + i * 0x20, quad(*this, block, i));
    }
    put8(u32(at(party_sprites)) + 0x1c0, u8(at(buffer_index)));
    put8(party(*this) + 0x53, 1);
}

// 801d36e0: the character portrait panel `a0` of party slot `a1`: its quad
// (801e927c), texture page (0, 0, 180, 0) and palette (80059414 for odd
// character ids, else 800595d4; with a2, odd gear index + b), then 801e920c
// with the slot's image position (801ea578 / 801ea584 and 801ea5c4 /
// 801ea5d0 per slot) at the row of mode a3 (801ea17c, 801ea18c).
void Overlay::draw_portrait_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto slot = a1 & 0xffU;
    const auto mode = a3 & 0xffU;
    init_text_quad(a0 + buffer(*this) * 40);
    const auto page = get_tpage(0, 0, 0x180, 0);
    const auto packet = a0 + buffer(*this) * 40;
    put16(packet + 22, page);
    const bool gear = (a2 & 0xffU) != 0;
    const auto odd = gear ? ((gear_index(*this, member(*this, slot)) + 11) & 1U) != 0
                          : (member(*this, slot) & 1U) != 0;
    put16(packet + 14, u16(odd ? 0x80059414 : 0x800595d4));
    const auto height = gear ? 0x60U : 0x48U;
    const auto x = u32(0x801ea17c + mode * 4) - (gear ? 48U : 36U);
    const auto image = u32((gear ? 0x801ea584U : 0x801ea578U) + slot * 4);
    const auto image_y = u8((gear ? 0x801ea5d0U : 0x801ea5c4U) + slot * 4);
    set_quad_rect(a0 + buffer(*this) * 40, x & 0xffffU, u16(0x801ea18c + mode * 4),
              (image << 2) & 0xfcU, image_y, height, 0xd);
    set_screen_quad_vectors(a0 + 0x50, x & 0xffffU, u16(0x801ea18c + mode * 4), height, 0xd);
    put8(a0 + 0x7d, u8(at(buffer_index)));
}

// 801d4ea0: clear index `a0`'s flags (party + 20 + a0, + 27 + a0) and
// release its two text blocks (state + 364 / 380 + a0 * 4).
void Overlay::release_text_blocks(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    const auto index = a0 & 0xffU;
    put8(party(*this) + 0x20 + index, 0);
    put8(party(*this) + 0x27 + index, 0);
    release(u32(at(text_blocks) + index * 4), 0x801d4ef4);
    release(u32(at(text_blocks_2) + index * 4), 0x801d4f10);
}

// 801d7f50: the stat names of the bar panel at (a0, a1) for rows a2..6:
// frame e0 of the glyph table is read first (80026338; its results are
// unused), then each row's name sprite (801ea45c + (a2 * 7 + row) * 4) at
// the row offsets 801e9d40 / 801e9d5c is built into the stat block (count at
// +32f3, 80 bytes per part); odd rows' parts are dimmed to 40 and unshaded.
// Last, each part's quad is placed at +2420.
void Overlay::draw_stat_names(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x68);
    auto f = frame(0x68);
    resident::sheet_part_texture(*this, u32(at(glyph_table)), 0xe0, f[0x20], f[0x24], f[0x28],
                                 f[0x2c], f[0x30], f[0x34]);
    put8(stats(*this) + 0x32f3, 0);
    const auto first = a2 & 0xffU;
    for (std::int32_t row = 0; row < 7 - s(first); ++row) {
        const auto index = u(row);
        const auto block = stats(*this);
        const auto parts = u8(block + 0x32f3);
        const auto id = u32(0x801ea45c + (first * 7 + index) * 4);
        const auto count =
            build_sprite(*this, id, block + parts * 80, a0 + u32(0x801e9d40 + index * 4),
                         a1 + u32(0x801e9d5c + index * 4));
        put8(stats(*this) + 0x32f3, u8(stats(*this) + 0x32f3) + count);
        if ((index & 1U) == 0)
            continue;
        for (auto part = parts; s(part) < s(u8(stats(*this) + 0x32f3)); ++part) {
            const auto packet = quad(*this, stats(*this), part);
            set_shade_tex(packet, 0);
            put8(packet + 4, 0x40);
            put8(packet + 5, 0x40);
            put8(packet + 6, 0x40);
        }
    }
    for (std::uint32_t part = 0; s(part) < s(u8(stats(*this) + 0x32f3)); ++part)
        place_quad(*this, stats(*this) + 0x2420 + part * 0x20, quad(*this, stats(*this), part));
}

// 801d827c: make the two buffered POLY_G4 bars at `a0` (36 bytes each) in
// colour `a1`: 0 (ff, 80, 80), 1 (80, ff, 80), 2 (ff, 0, 0), 3 (0, 0, ff)
// on the left vertices, black on the right.
void Overlay::make_stat_bar(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x28);
    std::array<std::uint32_t, 3> colour{};
    switch (a1 & 0xffU) {
    case 0:
        colour = {0xff, 0x80, 0x80};
        break;
    case 1:
        colour = {0x80, 0xff, 0x80};
        break;
    case 2:
        colour = {0xff, 0, 0};
        break;
    case 3:
        colour = {0, 0, 0xff};
        break;
    default:
        missing("bar_colour", 0x801d8328, "state:uninitialized-stack",
                "Another colour leaves 801d827c's colour bytes uninitialized on the stack");
    }
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto packet = a0 + i * 36;
        set_poly_g4(packet);
        for (std::uint32_t c = 0; c < 3; ++c) {
            put8(packet + 4 + c, colour[c]);
            put8(packet + 12 + c, colour[c]);
            put8(packet + 20 + c, 0);
            put8(packet + 28 + c, 0);
        }
    }
}

// 801d83ac: tint `a2` sprite parts of base `a0` from part a3 (every other
// quad: this buffer's) unshaded in colour a1: 0 (80, 40, 40), 1 (40, 40,
// 80), 2 (40, 40, 40).
void Overlay::tint_sprite_parts(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    std::array<std::uint32_t, 3> colour{};
    switch (a1 & 0xffU) {
    case 0:
        colour = {0x80, 0x40, 0x40};
        break;
    case 1:
        colour = {0x40, 0x40, 0x80};
        break;
    case 2:
        colour = {0x40, 0x40, 0x40};
        break;
    default:
        missing("part_tint", 0x801d8460, "state:uninitialized-stack",
                "Another colour leaves 801d83ac's red and blue bytes uninitialized on the stack");
    }
    const auto count = s(a2 & 0xffU);
    auto index = a3 & 0xffU;
    for (std::int32_t i = 0; i < count; ++i, index += 2) {
        const auto packet = a0 + index * 40;
        set_shade_tex(packet, 0);
        for (std::uint32_t c = 0; c < 3; ++c)
            put8(packet + 4 + c, colour[c]);
    }
}

// 801d84b4: the bar of stat value `a0` against preview `a1` (halfwords) on
// scale `a2`: bar length value * 100 / a2 * 0.64, the change |a1 - a0| and
// its length, colour 2 and sign glyph e3 for a gain (or none), 3 and e5 for a
// loss (801ea6fc..801ea714).
void Overlay::measure_stat_bar(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto value = a0 & 0xffffU;
    const auto preview = a1 & 0xffffU;
    const auto scale = s(a2);
    const auto length = percent_of_64(divide(s(value * 100), scale));
    put32(bar_value, value);
    put32(bar_preview, preview);
    put32(bar_change, preview - value);
    put32(bar_width, u(length));
    if (s(preview - value) >= 0) {
        put8(bar_colour, 2);
        put8(bar_sign, 0xe3);
    } else {
        put8(bar_colour, 3);
        put8(bar_sign, 0xe5);
        put32(bar_change, value - preview);
    }
    put32(bar_change_width, u(percent_of_64(divide(s(u32(bar_change) * 100), scale))));
}

// 801d85dc: the largest of the seven halfwords at `a1` and at `a2`
// (unsigned; starts from 0).
std::uint32_t Overlay::largest_stat(std::uint32_t /*a0: unused*/, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x18);
    std::uint32_t largest = 0;
    for (const auto values : {a1, a2})
        for (std::uint32_t i = 0; i < 7; ++i)
            if (largest < u16(values + i * 2))
                largest = u16(values + i * 2);
    return largest;
}

// 801d8644: the stat bars at (a1, a2) for rows a4..6. The values are the
// shown stats (tables + b8), compared with the screen's saved stats (screen
// + 280) when a3; the scale is the largest of both (801d85dc). Each row
// makes its bar (801d84b4, 801d827c at +2030) and quad (+3100), its value
// digits (801c80b8; digit sprites at 801e9d80 / 801e9d84 into +eb0, 320
// bytes a row) and their quads (+2a00). With a3 and a change, the change
// bar (+2228, +31e0) and its signed digits (+1770, quads +2d80) follow and
// are tinted by the change colour (801d83ac).
void Overlay::draw_stat_bars(std::uint32_t /*a0: unused*/, std::uint32_t a1, std::uint32_t a2,
                        std::uint32_t a3, std::uint32_t a4) {
    const auto stack_frame = enter(0xa8);
    const bool compare = (a3 & 0xffU) != 0;
    const auto first = a4 & 0xffU;
    const auto shown = tables(*this) + 0xb8;
    const auto before = compare ? screen(*this) + shown_stats : shown;
    const auto largest = largest_stat(0, before, shown);
    const auto x_of = [&](std::uint32_t address) { return a1 + u32(address); };
    for (std::int32_t row_signed = 0; row_signed < 7 - s(first); ++row_signed) {
        const auto row = u(row_signed);
        const auto dy = row * 8;
        const auto digits_base = 0xeb0 + row * 320;
        const auto change_base = 0x1770 + row * 320;
        put8(stats(*this) + 0x32c0 + row, 0);
        put8(stats(*this) + 0x32ce + row, 0);
        measure_stat_bar(u16(before + row * 2), u16(shown + row * 2), largest);
        make_stat_bar(stats(*this) + 0x2030 + row * 72, 0);
        set_screen_quad_vectors(stats(*this) + 0x3100 + row * 32, (u16(0x801e9d78) + a1) & 0xffffU,
                  (a2 + u16(0x801e9d7c) + dy) & 0xffffU, u16(bar_width), 6);
        put8(stats(*this) + 0x32dc + row, u8(at(buffer_index)));
        split_decimal_digits(u32(bar_value));
        for (std::uint32_t digit = 0; digit < 4; ++digit) {
            const auto glyph = u8(at(digits + 5 + digit));
            if (glyph == 0xff)
                continue;
            const auto block = stats(*this);
            const auto parts = u8(block + 0x32c0 + row);
            const auto count =
                build_sprite(*this, glyph, block + digits_base + parts * 80,
                             x_of(0x801e9d80) + digit * 8, a2 + u32(0x801e9d84) + dy);
            put8(stats(*this) + 0x32c0 + row, u8(stats(*this) + 0x32c0 + row) + count);
        }
        for (std::uint32_t part = 0; s(part) < s(u8(stats(*this) + 0x32c0 + row)); ++part)
            place_quad(*this, stats(*this) + 0x2a00 + row * 128 + part * 32,
                       quad(*this, stats(*this) + row * 320, part) + 0xeb0);
        if ((row & 1U) != 0)
            tint_sprite_parts(stats(*this) + digits_base, 2, u8(stats(*this) + 0x32c0 + row),
                      u8(at(buffer_index)));
        put8(stats(*this) + 0x32c7 + row, u8(at(buffer_index)));
        if (compare) {
            make_stat_bar(stats(*this) + 0x2228 + row * 72, u8(bar_colour));
            // A gain starts at the value's end, a loss that far back.
            auto start = a1 + u32(0x801e9d78) + u32(bar_width);
            if (u8(bar_colour) != 2)
                start -= u32(bar_change_width);
            set_screen_quad_vectors(stats(*this) + 0x31e0 + row * 32, start & 0xffffU,
                      (a2 + u16(0x801e9d7c) + dy) & 0xffffU, u16(bar_change_width), 6);
            put8(stats(*this) + 0x32e3 + row, u8(at(buffer_index)));
            if (u32(bar_change) != 0) {
                const auto sign = build_sprite(*this, u8(bar_sign), stats(*this) + change_base,
                                               x_of(0x801e9d80) + 32, a2 + u32(0x801e9d84) + dy);
                put8(stats(*this) + 0x32ce + row, u8(stats(*this) + 0x32ce + row) + sign);
                split_decimal_digits(u32(bar_change));
                auto offset = 0x28U;
                for (std::uint32_t digit = 0; digit < 3; ++digit) {
                    const auto glyph = u8(at(digits + 6 + digit));
                    if (glyph == 0xff)
                        continue;
                    const auto block = stats(*this);
                    const auto parts = u8(block + 0x32ce + row);
                    const auto count =
                        build_sprite(*this, glyph, block + change_base + parts * 80,
                                     x_of(0x801e9d80) + offset, a2 + u32(0x801e9d84) + dy);
                    offset += 8;
                    put8(stats(*this) + 0x32ce + row, u8(stats(*this) + 0x32ce + row) + count);
                }
                for (std::uint32_t part = 0; s(part) < s(u8(stats(*this) + 0x32ce + row)); ++part)
                    place_quad(*this, stats(*this) + 0x2d80 + row * 128 + part * 32,
                               quad(*this, stats(*this) + row * 320, part) + 0x1770);
                tint_sprite_parts(stats(*this) + change_base, (u8(bar_colour) - 2) & 0xffU,
                          u8(stats(*this) + 0x32ce + row), u8(at(buffer_index)));
                put8(stats(*this) + 0x32d5 + row, u8(at(buffer_index)));
                put8(stats(*this) + 0x32f2, 1);
            }
        }
        put8(stats(*this) + 0x32ea + row, 1);
    }
}

// 801d8de4: the stat panel of party slot `a0` (80, 90 with a1, else 98,
// 26): stat names (801d7f50) and bars (801d8644, comparing when a2) from
// row a3; marks it shown (party + 8) with this buffer (stat block + 32f1).
void Overlay::draw_stat_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x30);
    const bool lower = (a1 & 0xffU) != 0;
    const auto x = lower ? 0x80U : 0x98U;
    const auto y = lower ? 0x90U : 0x26U;
    draw_stat_names(x, y, a3 & 0xffU);
    draw_stat_bars(a0 & 0xffU, x, y, a2 & 0xffU, a3 & 0xffU);
    put8(party(*this) + 8, 1);
    put8(stats(*this) + 0x32f1, u8(at(buffer_index)));
}

// 801d8ea4: the part panel of party slot `a0` in the equipment screen state
// (+0 per row, 128 bytes a row): rows of part names drawn into a 3f6-byte text
// buffer (80034eac, width 24) from the resident name tables (800337e8
// accessories, 80033848 weapons; 80033a2c / 80033a5c for gear parts), each
// row's image loaded at VRAM (140 + 20 * (row & 2) / 2, 27 + 13 * (row / 4))
// when needed, then its window (801e7c50, 801c851c at column a1 ? 28 : d0 and
// the rows of 801e9d88). Mode a1: 0 weapon and accessories, 1 and 2 other
// part groups (four rows). a2 shows the kept parts (screen + 29c..);
// a3 the gear record's parts. Row 4 of a character's panel is its portrait.
void Overlay::draw_part_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x88);
    const auto slot = a0 & 0xffU;
    const auto mode = a1 & 0xffU;
    const bool gear = (a3 & 0xffU) != 0;
    std::uint32_t rows = 5;
    std::uint32_t column = 0xd0;
    std::uint32_t row_base = 0;
    const auto record = character_record(member(*this, slot));
    std::uint32_t weapon = record + record_weapon;
    std::uint32_t specials = record + record_special;
    std::uint32_t accessories = record + record_accessories;
    if (mode != 0) {
        rows = 4;
        column = 0x28;
        row_base = mode == 1 ? 5 : 9;
        put8(screen(*this) + 0x298, 0);
    }
    if (gear) {
        const auto g = gear_record(gear_index(*this, member(*this, slot)));
        weapon = g + 4;
        specials = g - 4;
        accessories = g + 1;
    }
    if ((a2 & 0xffU) != 0) {
        weapon = screen(*this) + kept_weapon;
        specials = screen(*this) + kept_special;
        accessories = screen(*this) + kept_accessory;
    }
    const auto text = allocate(0x3f6, 0, 0x801d9034);
    const auto glyphs = [&](std::uint32_t window) { program.dialogue_glyphs(window); };
    const auto render = [&](std::uint32_t name, std::uint32_t plane) { // 80034eac
        return resident::layout_text_line(*this, name, text, 0x24, plane, glyphs);
    };
    // Weapon-table names: 80033848, gear 80033a5c; accessories 800337e8,
    // gear 80033a2c.
    const auto weapon_name = [&](std::uint32_t id) {
        return gear ? resident::gear_weapon_name(*this, id) : resident::weapon_name(*this, id);
    };
    const auto accessory_name = [&](std::uint32_t id) {
        return gear ? resident::gear_accessory_name(*this, id)
                    : resident::accessory_name(*this, id);
    };
    // 801d9304's image flag lives on the stack across rows; a row that does not
    // set it reads the previous row's value.
    std::optional<bool> load_row;
    auto special = specials;
    for (std::uint32_t row = 0; s(row) < s(rows); ++row, ++special) {
        const auto base = row * 128;
        if (row == 0) {
            const auto id = u8(mode == 2 ? specials : weapon);
            put8(screen(*this) + 0x7e, render(weapon_name(id), 0));
        } else if (row < 5 && !(row == 4 && !gear)) {
            if (mode == 0 && gear && row == 1) {
                // A gear's first accessory row names its weapon's +3 part.
                put8(screen(*this) + 0xfe, render(weapon_name(u8(weapon + 3)), 1));
            } else {
                std::uint32_t name = 0;
                if (mode >= 2)
                    name = weapon_name(u8(special));
                else if (mode == 0 && gear)
                    name = accessory_name(u8(accessories + row - 2));
                else
                    name = accessory_name(u8(accessories + row - 1));
                put8(screen(*this) + base + 0x7e, render(name, row % 2));
            }
        }
        if ((row & 1U) != 0)
            load_row = true;
        else if (!gear)
            load_row = false;
        else if (mode == 0 && row == 4)
            load_row = true;
        if (!load_row)
            missing("part_panel_image", 0x801d9308, "state:uninitialized-stack",
                    "801d8ea4 reads its image flag before any row set it");
        if (*load_row)
            load_text_image(*this, (row & 2U) << 4U | 0x140U, (row / 4) * 13 + 39, text);
        set_label_packets(screen(*this) + base, row, 0xc, 0);
        if (row == 4 && !gear) {
            // The portrait: page (0, 0, 180, 0), the character's palette and
            // its image corners from 801ea584 / 801ea5d0.
            const auto page = get_tpage(0, 0, 0x180, 0);
            put16(screen(*this) + buffer(*this) * 40 + 0x200 + 22, page);
            const auto odd = ((gear_index(*this, member(*this, slot)) + 11) & 1U) != 0;
            put16(screen(*this) + buffer(*this) * 40 + 0x200 + 14,
                  u16(odd ? 0x80059414 : 0x800595d4));
            const auto packet = screen(*this) + base + buffer(*this) * 40;
            const auto left = u32(0x801ea584 + slot * 4) * 4;
            const auto top = u8(0x801ea5d0 + slot * 4);
            put8(packet + 12, left);
            put8(packet + 13, top);
            put8(packet + 20, left + 0x60);
            put8(packet + 21, top);
            put8(packet + 28, left);
            put8(packet + 29, top + 13);
            put8(packet + 36, left + 0x60);
            put8(packet + 37, top + 13);
            put8(screen(*this) + base + 0x7e, 0x60);
        }
        set_screen_quad_vectors(screen(*this) + base + 0x50, column & 0xffffU,
                  u16(0x801e9d88 + (row + row_base) * 4), u8(screen(*this) + base + 0x7e), 0xd);
        put8(screen(*this) + row + 0x294, 1);
    }
    put8(screen(*this) + 0x299, u8(at(buffer_index)));
    put8(party(*this) + 0x4b, 1);
    release(text, 0x801d96c8);
}

// 801da518: leave the equipment screen's shared parts: the title and text
// blocks 3 and 4 (801d3444, 801d4ea0), party + 48, data set 10's release
// (801c72bc) and the blocks of state + 42c and table + 1c.
void Overlay::close_equipment_shared() {
    const auto stack_frame = enter(0x18);
    release_screen_title();
    release_text_blocks(3);
    release_text_blocks(4);
    put8(party(*this) + 0x48, 0);
    load_menu_data_set(0x10);
    release(u32(u32(at(loaded_0)) + 0x1180), 0x801da56c);
    release(u32(at(loaded_0)), 0x801da584);
    release(u32(tables(*this) + 0x1c), 0x801da5a4);
}

// 801db02c: cursor sprite block `a0` (state + 444 + a0 * 4, 78 bytes): frame
// 4 (+70), timer 0 (+74).
void Overlay::open_cursor_sprite(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    const auto field = at(cursor_blocks) + (a0 & 0xffU) * 4;
    const auto block = allocate(0x78, 0, 0x801db040);
    put32(field, block);
    bzero(block, 0x78);
    put32(u32(field) + 0x70, 4);
    put8(u32(field) + 0x74, 0);
}

// 801db0a8: animate and place cursor `a3` at entry `a0`: every sixth call
// the frame (+70) steps down from 4 to 0 and wraps; its sprite (glyph 15b +
// frame) goes at the entry's position for layout `a2`: 0 two columns (x 1c +
// 136 per column, y 11 + 16 per row), 1 the same from row a1 * 2 (hidden
// outside 16 entries), 2 two columns at y 14, 3 one column (x a0, y 14 + 13
// per row). Shown state at party + 50 + a3.
void Overlay::draw_cursor_sprite(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {
    const auto stack_frame = enter(0x38);
    const auto index = a3 & 0xffU;
    const auto block = u32(at(cursor_blocks) + index * 4);
    put8(block + 0x74, u8(block + 0x74) + 1);
    if ((u8(block + 0x74) & 0xffU) >= 6) {
        put32(block + 0x70, u32(block + 0x70) - 1);
        if (s(u32(block + 0x70)) < 0)
            put32(block + 0x70, 4);
        put8(block + 0x74, 0);
    }
    const auto entry = s(a0);
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    bool shown = true;
    switch (a2 & 0xffU) {
    case 0:
        x = u((entry % 2) * 136 + 28);
        y = u((entry / 2) * 16 + 17);
        break;
    case 1: {
        const auto top = s(a1 << 1U);
        if (entry < top || entry >= s(u(top) + 16)) {
            shown = false;
            break;
        }
        x = u((entry % 2) * 136 + 24);
        y = u(((entry - top) / 2) * 16 + 17);
        break;
    }
    case 2:
        x = u((entry % 2) * 136 + 24);
        y = u((entry / 2) * 16 + 20);
        break;
    case 3:
        x = 0xa0;
        y = u(entry * 13 + 20);
        break;
    default:
        missing("cursor_layout", 0x801db238, "state:uninitialized-register",
                "Another layout leaves 801db0a8's cursor position uninitialized");
    }
    if (!shown) {
        put8(party(*this) + 0x50 + index, 0);
        return;
    }
    static_cast<void>(build_sprite(*this, u32(block + 0x70) + 0x15b, block, 0, 0));
    place_quad(*this, block + 0x50, quad(*this, block, 0), x, y);
    put8(block + 0x75, u8(at(buffer_index)));
    put8(party(*this) + 0x50 + index, 1);
}

// 801db340: release cursor block `a0` and clear its shown flag.
void Overlay::release_cursor_sprite(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    const auto index = a0 & 0xffU;
    release(u32(at(cursor_blocks) + index * 4), 0x801db364);
    put8(party(*this) + 0x50 + index, 0);
}

// 801de2c8: open the equipment screen's list block (state + 434, a1c bytes),
// its window set (801e8018 with layout 801ea558 + a0 * 6), data set 7
// (801c72bc), window 3 (801d22f4), cursor 0 (801db02c) and the party
// sprites (801d3488).
void Overlay::open_equipment_list(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    const auto block = allocate(0xa1c, 0, 0x801de2dc);
    put32(at(list_block), block);
    bzero(block, 0xa1c);
    const auto gear = a0 & 0xffU;
    // A3 (party + 40) is also loaded; 801e8018 does not read it.
    layout_labels_row4(6, at(0x14e0), 0x801ea558 + gear * 6);
    load_menu_data_set(7);
    set_markers(3);
    open_cursor_sprite(0);
    draw_party_window_sprites(1, gear);
}

// 801de474: the equipment screen's frame windows: clear party + 40..45,
// draw windows 2..5 (special parts, a0) or 0..1 (801e8070 with layout
// 801ea558 + a1 * 6 and shapes 801e9ea0), drop text block 4 when set
// (party + 24) and draw window 4 (801d397c) of height 72 or 5a.
void Overlay::draw_equipment_frames(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x40);
    for (std::uint32_t i = 0; i < 6; ++i)
        put8(party(*this) + shown_flags + i, 0);
    const bool special = (a0 & 0xffU) != 0;
    const std::uint32_t first = special ? 2 : 0;
    const std::uint32_t end = special ? 6 : 2;
    const std::uint32_t height = special ? 0x72 : 0x5a;
    const auto layout = 0x801ea558 + (a1 & 0xffU) * 6;
    for (auto window = first; window < end; ++window)
        place_label(6, at(0x14e0), layout, 0x801e9ea0, party(*this) + shown_flags, window & 0xffU,
                  window & 0xffU, 3);
    if (u8(party(*this) + 0x24) != 0)
        release_text_blocks(4);
    open_window(4, 0x10, 0xc, 0x80, height, 0, 1, 4, 0);
}

// 801de5cc: build the candidate list for part `a2` of party slot `a0` and
// draw rows a1.. of it; returns the scroll limit (entries - 8, at least 0).
// Candidates come from the weapon (8006f3d0) or accessory (8006f4fc)
// inventories, or the gear lists (8006f754, 8006f84e) with a4: weapons the
// character can use (801c865c; gear 801c8678) of a class below 5 and id
// below 50, or with a3 (special parts) of the kept part's class and id 50 or
// more; accessories the character can use whose group bits (+e; gear +8)
// are free or held by the replaced part (list entry 0 stays empty for
// removing). Each shown row renders its name and count ("nn", c3 for a
// leading zero; 80033b34) into VRAM rows 180 / 198 and its windows.
std::uint32_t Overlay::build_candidate_list(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                 std::uint32_t a3, std::uint32_t a4) {
    const auto stack_frame = enter(0xd0);
    const auto slot = a0 & 0xffU;
    const auto part = a2;
    const bool special = (a3 & 0xffU) != 0;
    const bool gear = (a4 & 0xffU) != 0;
    std::uint32_t kind = 0;        // 0 weapon, 1..4 special part, 5 accessory
    std::uint32_t count = 100;     // inventory entries searched
    std::uint32_t kept_entry = 0;  // the kept special part's table entry
    std::uint32_t own_groups = 0;  // group bits of the replaced accessory
    std::uint32_t used_groups = 0; // group bits of all equipped accessories
    const auto kept = [&](std::uint32_t offset) { return u8(screen(*this) + offset); };
    if (special) {
        kind = part + 1;
        if (!gear)
            kept_entry = u32(tables(*this)) + kept(kept_weapon + part) * 16;
        else
            kept_entry = u32(tables(*this) + 0x18) + kept(kept_weapon + part) * 20;
    } else if (part != 0) {
        kind = 5;
        const auto groups_of = [&](std::uint32_t id) {
            return gear ? u16(u32(tables(*this) + 0x14) + id * 28 + 8)
                        : u16(u32(tables(*this) + 4) + id * 16 + 14);
        };
        count = gear ? 150 : 200;
        own_groups = groups_of(kept(kept_accessory - 1 + part));
        for (std::uint32_t i = 0; i < 3; ++i)
            used_groups |= groups_of(kept(kept_accessory + i));
    }
    for (std::uint32_t i = 0; i < 200; ++i) {
        put8(candidate_ids + i, 0);
        put8(candidate_counts + i, 0);
    }
    std::uint32_t length = kind == 5 ? 1 : 0;
    const auto character = member(*this, slot);
    const auto usable = [&](std::uint32_t mask) {
        if (!gear)
            return (party_slot_bit(mask, character) & 0xffffU) != 0;
        missing("gear_usable", 0x801deaf4, "symbol:menu-801c8678",
                "801c8678 (gear part test, outside the census) is not recovered");
    };
    const auto groups_allow = [&](std::uint32_t groups) {
        return groups == 0 || (own_groups & groups) != 0 || (used_groups & groups) == 0;
    };
    for (std::uint32_t i = 0; s(i) < s(count); ++i) {
        bool candidate = false;
        if (!gear) {
            const auto weapon_id = u8(0x8006f3d0 + i);
            const auto weapon = u32(tables(*this)) + weapon_id * 16;
            const auto accessory = u32(tables(*this) + 4) + u8(0x8006f4fc + i) * 16;
            if (kind == 0)
                candidate = usable(u16(weapon)) && u8(weapon + 6) < 5 && weapon_id < 50;
            else if (kind < 5)
                candidate =
                    usable(u16(weapon)) && u8(weapon + 6) == u8(kept_entry + 6) && weapon_id >= 50;
            else if (kind == 5)
                candidate = usable(u16(accessory)) && groups_allow(u16(accessory + 14));
        } else {
            const auto weapon_id = u8(0x8006f754 + i);
            const auto weapon = u32(tables(*this) + 0x18) + weapon_id * 20;
            const auto accessory = u32(tables(*this) + 0x14) + u8(0x8006f84e + i) * 28;
            if (kind == 0)
                candidate = usable(u32(weapon + 4)) && u8(weapon + 15) < 5 && weapon_id < 50;
            else if (kind < 5)
                candidate = usable(u32(weapon + 4)) && u8(weapon + 15) == u8(kept_entry + 15) &&
                            weapon_id >= 50;
            else if (kind == 5)
                candidate = usable(u32(accessory)) && groups_allow(u16(accessory + 8));
        }
        if (!candidate)
            continue;
        // Ids and counts: the list's ids, its counts length bytes before.
        const auto ids = gear ? (kind != 5 ? 0x8006f754U : 0x8006f84eU)
                              : (kind != 5 ? 0x8006f3d0U : 0x8006f4fcU);
        const auto counts = ids - (gear ? (kind != 5 ? 100U : 150U) : (kind != 5 ? 100U : 200U));
        put8(candidate_ids + length, u8(ids + i));
        put8(candidate_counts + length, u8(counts + i));
        ++length;
    }
    const auto text = allocate(0x3f6, 0, 0x801ded48);
    const auto glyphs = [&](std::uint32_t window) { program.dialogue_glyphs(window); };
    const auto codec = name_codec(*program.menu);
    for (std::uint32_t row = 0; row < 8; ++row) {
        const auto entry = a1 + row;
        const auto base = row * 128;
        const auto id = u8(candidate_ids + entry);
        if (id == 0) {
            put8(list(*this) + row + 0xa10, 0);
            continue;
        }
        std::uint32_t name = 0;
        if (!gear)
            name =
                kind != 5 ? resident::weapon_name(*this, id) : resident::accessory_name(*this, id);
        else
            name = kind != 5 ? resident::gear_weapon_name(*this, id)
                             : resident::gear_accessory_name(*this, id);
        put8(list(*this) + base + 0x7e,
             resident::layout_text_line(*this, name, text, 0x24, 0, glyphs));
        // The count as two codes: tens (c3, a space, for none) and units.
        auto f = frame(0x30);
        const auto number = u8(candidate_counts + entry);
        const auto tens = number / 10;
        put8(f[0x21], 0);
        put8(f[0x23], 0);
        put8(f[0x20], (tens & 0xffU) != 0 ? tens + 16 : 0xc3);
        put8(f[0x22], number - tens * 10 + 16);
        const std::array<std::uint8_t, 4> codes{
            static_cast<std::uint8_t>(u8(f[0x20])), static_cast<std::uint8_t>(u8(f[0x21])),
            static_cast<std::uint8_t>(u8(f[0x22])), static_cast<std::uint8_t>(u8(f[0x23]))};
        const auto decoded = menu::decode_text(codec, codes, 2); // 80033b34
        for (std::uint32_t i = 0; i < decoded.size(); ++i)
            put8(f[0x28] + i, decoded[i]);
        put8(list(*this) + base + 0x47e,
             resident::layout_text_line(*this, f[0x28], text, 0x24, 1, glyphs));
        load_text_image(*this, (row & 1U) * 24 + 384, (row / 2) * 13 + 128, text);
        set_label_packets(list(*this) + base, row, 0x80, 0x81);
        set_label_packets(list(*this) + 0x400 + base, row, 0x80, 0x82);
        const auto y = (0x12 + row * 13) & 0xffffU;
        set_screen_quad_vectors(list(*this) + base + 0x50, 0xa8, y, u8(list(*this) + base + 0x7e), 13);
        set_screen_quad_vectors(list(*this) + 0x400 + base + 0x50, 0x10c, y, u8(list(*this) + base + 0x47e), 13);
        put8(list(*this) + base + 0x7d, u8(at(buffer_index)));
        put8(list(*this) + base + 0x47d, u8(at(buffer_index)));
        put8(list(*this) + row + 0xa10, 1);
    }
    release(text, 0x801df048);
    draw_portrait_panel(list(*this) + 0x800, slot, a4 & 0xffU, 0);
    put8(party(*this) + 0x4c, 1);
    const auto limit = s(length) - 8;
    return limit < 0 ? 0 : u(limit);
}

// 801df0d4: commit the previewed part (menu::swap_equipment).
std::uint32_t Overlay::commit_equipment(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                 std::uint32_t a3) {
    const auto stack_frame = enter(0x28);
    Menu ctx{*program.menu, program.resident.sound};
    return swap_equipment(ctx, a0, a1, a2, a3);
}

// 801df5d0: save the shown stats (tables + b8.., 9 halfwords) at screen +
// 280 and keep party slot a0's parts at screen + 29c / 2a1 / 2a6: record
// bytes 6a.., 6f.., 74.. (five each) or, with a1, its gear record's +4..,
// -4.. and +1.. (four each).
void Overlay::keep_equipped_parts(std::uint32_t a0, std::uint32_t a1) {
    for (std::uint32_t i = 0; i < 9; ++i)
        put16(screen(*this) + shown_stats + i * 2, u16(tables(*this) + 0xb8 + i * 2));
    const auto slot = a0 & 0xffU;
    const bool gear = (a1 & 0xffU) != 0;
    for (std::uint32_t i = 0; i < (gear ? 4U : 5U); ++i) {
        const auto character = member(*this, slot);
        const auto base =
            gear ? gear_record(gear_index(*this, character)) : character_record(character);
        put8(screen(*this) + kept_weapon + i, u8(base + (gear ? 4U : record_weapon) + i));
        put8(screen(*this) + kept_special + i, u8(base + (gear ? 0U - 4U : record_special) + i));
        put8(screen(*this) + kept_accessory + i, u8(base + (gear ? 1U : record_accessories) + i));
    }
}

// 801dfb68: preview list entry a3 + a2 in part a1 of party slot a0: weapon
// (+6a) or accessory a1 (+73 + a1, parts 1..3), or with a4 special part a1
// (+6f + a1); with a5 in the gear record (+4, + a1, -4 + a1).
void Overlay::preview_candidate(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4, std::uint32_t a5) {
    const auto id = u8(candidate_ids + a3 + a2);
    const auto character = member(*this, a0 & 0xffU);
    const bool gear = (a5 & 0xffU) != 0;
    const auto record = character_record(character);
    const auto base = gear ? gear_record(gear_index(*this, character)) : record;
    if ((a4 & 0xffU) != 0) {
        put8(base + (gear ? 0U - 4U : record_special) + a1, id);
        return;
    }
    if (a1 == 0)
        put8(base + (gear ? 4U : record_weapon), id);
    else if (s(a1) >= 0 && s(a1) < 4)
        put8(base + (gear ? 0U : record_accessories - 1) + a1, id);
}

// 801dff5c: the three-line description of list entry a1 + a2 (a5: the
// equipped part instead) for part a0 (a3 special, a4 gear, a6 party slot):
// text entries id * 3 + line of the list's text tables (list + a00..a0c by
// weapon/accessory and gear) rendered at VRAM rows 8..10 into windows +880..
// (801e7c50, 801c851c at 10, 96 + 16 * line); list + a18 marks it shown.
void Overlay::draw_part_description(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                        std::uint32_t a4, std::uint32_t a5, std::uint32_t a6) {
    const auto stack_frame = enter(0x40);
    auto id = u8(candidate_ids + a1 + a2);
    const bool current = (a5 & 0xffU) != 0;
    const bool special = (a3 & 0xffU) != 0;
    const auto gear = a4 & 0xffU;
    if (current)
        id = 0xff;
    if (id == 0) {
        put8(list(*this) + 0xa18, 0);
        return;
    }
    const auto group = ((!special && a0 != 0) ? 1U : 0U) + gear * 2;
    if ((group & 0xffU) > 3)
        missing("part_description", 0x801e027c, "state:uninitialized-register",
                "801dff5c's text table is uninitialized for a gear flag above 1");
    const auto table = u32(list(*this) + 0xa00 + (group & 0xffU) * 4);
    if (current) {
        const auto character = member(*this, a6);
        const auto record = character_record(character);
        switch (group & 0xffU) {
        case 0:
            id = u8(special ? record + record_special + a0 : record + record_weapon);
            break;
        case 1:
            id = u8(record + record_accessories - 1 + a0);
            break;
        case 2: {
            const auto g = gear_record(gear_index(*this, character));
            id = u8(special ? g - 4 + a0 : g + 4);
            break;
        }
        default:
            id = u8(gear_record(gear_index(*this, character)) + a0);
            break;
        }
    }
    if ((id & 0xffffU) == 0) {
        put8(list(*this) + 0xa18, 0);
        return;
    }
    const auto text = allocate(0x3f6, 0, 0x801e0284);
    bzero(text, 0x3f6);
    const auto glyphs = [&](std::uint32_t window) { program.dialogue_glyphs(window); };
    for (std::uint32_t line = 0; line < 3; ++line) {
        const auto base = line * 128;
        const auto entry = resident::offset_table_entry(*this, table, id * 3 + line);
        put8(list(*this) + base + 0x8fe,
             resident::layout_text_line(*this, entry, text, 0x24, 0, glyphs));
        const auto row = line + 8;
        load_text_image(*this, (row & 1U) * 24 + 384, (row / 2) * 13 + 128, text);
        set_label_packets(list(*this) + 0x880 + base, row, 0x80, 0x81);
        set_screen_quad_vectors(list(*this) + 0x880 + base + 0x50, 0x10, (line * 16 + 150) & 0xfffeU,
                  u8(list(*this) + base + 0x8fe), 13);
        put8(list(*this) + base + 0x8fd, u8(at(buffer_index)));
    }
    put8(list(*this) + 0xa18, 1);
    release(text, 0x801e03e0);
}

// 801e05d0: the equipment screen of party slot a0 (a2: gear parts) until it
// is left. Each frame (801c7bf4) redraws what changed: the part panel and
// frame windows (801d8ea4, 801de474) on a slot or special-part change, the
// candidate list and its scroll title (801de5cc, 801d3344) on a scroll, the
// list cursor (801db0a8), the preview of the selected entry with rebuilt
// bonuses and stats, its description and stat bars (801dfb68, 801e36d4,
// 801e3a80, 801dff5c, 801d8de4), and the part cursor's box (window set +148
// quad at 8c..9c by 801e9dbc). The first frame opens windows 2, 3 and 5
// (801d397c) and, with a1, waits for the input fade (801d1e80, 801d29a8).
// Inputs (state + 325; jump table 801c515c): in part mode 0/2 toggle a
// character 4's special parts, 1/3 step the part (skipping the two middle
// ones for gear specials), 4 opens the list, 5 leaves, 9/a switch the slot
// (801d9704); in list mode 1/3 step and scroll, 4 commits (801df0d4) and 5
// cancels (801df890).
void Overlay::equipment_screen_loop(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0xb0);
    auto slot = a0;
    const auto fade = a1 & 0xffU;
    const auto gear = a2 & 0xffU;
    bool running = true;
    bool panel = true;         // fp: redraw the part panel
    std::uint32_t special = 0; // s6: special parts shown
    std::uint32_t drawn_slot = 0xff;
    std::uint32_t part = 0; // s2
    bool first = true;
    std::uint32_t row = 0; // s3
    std::uint32_t drawn_part = 0xff;
    std::uint32_t drawn_row = 0xff;
    bool previewed = false; // sp+48
    bool cursor = false;    // sp+50
    bool committed = false; // sp+58
    bool listing = false;   // sp+60
    std::uint32_t drawn_top = 0xff;
    std::uint32_t top = 0;   // s1
    std::uint32_t limit = 0; // sp+80
    put8(party(*this) + 7, 0);
    open_equipment_list(gear);
    keep_equipped_parts(slot & 0xffU, gear);
    const auto same_slot = [&] { return (slot & 0xffU) == (drawn_slot & 0xffU); };
    const auto windows = [&] { return u32(at(window_block)); };
    do {
        menu_frame();
        if (panel || !same_slot()) {
            draw_part_panel(slot & 0xffU, (special + 1) & 0xffU, listing ? 1U : 0U, gear);
            draw_equipment_frames(special & 0xffU, gear);
            panel = false;
        }
        if (top != drawn_top || !same_slot()) {
            limit = build_candidate_list(slot & 0xffU, top, part, special & 0xffU, gear);
            std::uint32_t y = 0;
            std::uint32_t height = 0x64;
            if (limit != 0) {
                y = u(divide(s(top * 100), s(limit)) / 2);
                height = 0x32;
            }
            draw_screen_title(0x94, y + 18, height);
        }
        if (cursor)
            draw_cursor_sprite(row, top, 3, 0);
        else
            put8(party(*this) + 0x50, 0);
        if (row != drawn_row || !same_slot() || top != drawn_top) {
            if (previewed)
                preview_candidate(slot & 0xffU, part, row, top, special & 0xffU, gear);
            previewed = true;
            if (gear == 0) {
                rebuild_equipment_bonuses(tables(*this), member(*this, slot));
                update_equipment_stats(tables(*this), member(*this, slot));
            } else {
                missing("gear_preview_stats", 0x801e0840, "symbol:menu-801dfe2c",
                        "801dfe2c (gear stats, outside the census) is not recovered");
            }
            draw_part_description(part, row, top, special & 0xffU, gear, 0, slot & 0xffU);
            draw_stat_panel(slot & 0xffU, 1, listing ? 1U : 0U, gear);
            drawn_row = row;
            drawn_top = top;
        }
        if (part != drawn_part || !same_slot()) {
            draw_part_description(part, row, top, special & 0xffU, gear, 1, slot & 0xffU);
            const auto packet = windows() + u8(windows() + 0x148) * 40;
            const auto y = u16(0x801e9dbc + ((special & 0xffU) * 4 + part) * 4);
            put16(packet + 8, 0x8c);
            put16(packet + 10, y);
            put16(packet + 16, 0x9c);
            put16(packet + 18, y);
            put16(packet + 24, 0x8c);
            put16(packet + 26, y + 16);
            put16(packet + 32, 0x9c);
            put16(packet + 34, y + 16);
            drawn_part = part;
            drawn_slot = slot;
        }
        if (first) {
            open_window(2, 0x94, 0xa, 0x94, 0x74, 0, 1, 4, 1);
            open_window(3, 0x6c, 0x87, 0xc4, 0x48, 0, 1, 4, 0);
            open_window(5, 8, 0x8e, 0x60, 0x40, 0, 1, 4, 0);
            if (fade != 0) {
                zoom_in();
                slide_party_panels(0, 0);
                while (u8(at(input_wait)) != 0)
                    menu_frame();
            }
            put8(windows() + 0x140, 1);
            put8(party(*this) + 6, 0);
            first = false;
            put8(party(*this) + 0x21, 0);
        }
        put8(party(*this) + 0x2f, 1);
        if (committed) {
            committed = false;
            put8(at(input_code), 4);
        }
        const auto input = u8(at(input_code));
        if (!listing) {
            const bool gear_specials = (special & 0xffU) != 0 && gear != 0;
            switch (input) {
            case 0:
            case 2:
                if (member(*this, slot) == 4) {
                    special ^= 1U;
                    drawn_part = 0xff;
                    panel = true;
                    part = 0;
                }
                break;
            case 1:
                if (s(++part) >= 4)
                    part = 0;
                if (gear_specials && part - 1 < 2)
                    part = 3;
                break;
            case 3:
                if (s(--part) < 0)
                    part = 3;
                if (gear_specials && part - 1 < 2)
                    part = 0;
                break;
            case 4:
                keep_equipped_parts(slot & 0xffU, gear);
                top = 0;
                listing = true;
                drawn_top = 0xff;
                drawn_row = 0xff;
                put8(windows() + 0x140, 0);
                panel = true;
                cursor = true;
                row = 0;
                put8(party(*this) + 0x50, 1);
                break;
            case 5:
                running = false;
                break;
            case 9:
            case 10:
                missing("equipment_switch_slot", input == 9 ? 0x801e0d14 : 0x801e0d38,
                        "symbol:menu-801d9704",
                        "801d9704 (next party slot, outside the census) is not recovered");
            default:
                break;
            }
        } else {
            bool reset = false;
            switch (input) {
            case 1:
                if (s(++row) >= 8) {
                    ++top;
                    row = 7;
                    if (s(limit) < s(top))
                        top = limit;
                }
                break;
            case 3:
                if (s(--row) < 0) {
                    --top;
                    row = 0;
                    if (s(top) < 0)
                        top = 0;
                }
                break;
            case 4: {
                const auto result = commit_equipment(slot & 0xffU, part & 0xffU, special & 0xffU, gear);
                drawn_slot = 0xff;
                if ((result & 0xffU) != 0) {
                    special ^= 1U;
                    panel = true;
                    committed = true;
                    missing("equipment_second_weapon", 0x801e0e40, "symbol:menu-801e0434",
                            "801e0434 (after a second-weapon change, outside the census) is not "
                            "recovered");
                }
                keep_equipped_parts(slot & 0xffU, gear);
                reset = true;
                cursor = false;
                put8(windows() + 0x140, 1);
                draw_cursor_sprite(0, top, 3, 0);
                row = 0;
                put8(party(*this) + 0x50, 0);
                break;
            }
            case 5:
                missing("equipment_cancel", 0x801e0db4, "symbol:menu-801df890",
                        "801df890 (restore the kept parts, outside the census) is not recovered");
            default:
                break;
            }
            if (reset) {
                row = 0;
                top = 0;
                listing = false;
                drawn_row = 0xff;
                panel = true;
                previewed = false;
                put8(stats(*this) + 0x32f2, 0);
            }
        }
    } while (running);
    clear_markers();
    release_cursor_sprite(0);
}

// 801e0f78: run the equipment screen for party slot a0 (a1: wait for the
// input fade): allocate and clear the stat block (state + 35c, 32f4 bytes)
// and the screen state (state + 360, 2ac bytes), load data set 3, run the
// screen (801e05d0) and release data set 3 (13). Returns 1.
std::uint32_t Overlay::run_equipment_screen(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x20);
    const auto block = allocate(0x32f4, 0, 0x801e0f94);
    put32(at(stats_block), block);
    bzero(block, 0x32f4);
    const auto state_block = allocate(0x2ac, 0, 0x801e0fb8);
    put32(at(state_equip_screen), state_block);
    bzero(state_block, 0x2ac);
    load_menu_data_set(3);
    equipment_screen_loop(a0 & 0xffU, a1 & 0xffU, 0);
    load_menu_data_set(0x13);
    return 1;
}

// 801e36d4: rebuild a character's equipment bonuses (menu::equipment_bonuses).
void Overlay::rebuild_equipment_bonuses(std::uint32_t a0, std::uint32_t a1) {
    Menu ctx{*program.menu, program.resident.sound};
    equipment_bonuses(ctx, a0, a1);
}

// 801e3a80: the equipment screen's shown stats (menu::equipment_stats).
void Overlay::update_equipment_stats(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x8);
    Menu ctx{*program.menu, program.resident.sound};
    equipment_stats(ctx, a0, a1);
}

} // namespace xem::reconstruction::menu
