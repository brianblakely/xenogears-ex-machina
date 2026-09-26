// Resident text, sprite-sheet and resource helpers (executable dc0b2dd7...)
// over memory at original addresses. See resident_text.hpp.
#include "xem/reconstruction/resident_text.hpp"

#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/program.hpp"

namespace xem::reconstruction::resident {
namespace {
std::uint32_t u8(const Memory &memory, std::uint32_t address) { return memory.read(address, 1); }
std::uint32_t u16(const Memory &memory, std::uint32_t address) { return memory.read(address, 2); }
std::uint32_t u32(const Memory &memory, std::uint32_t address) { return memory.read(address, 4); }
std::int32_t s16(const Memory &memory, std::uint32_t address) {
    return static_cast<std::int16_t>(memory.read(address, 2));
}
void put8(Memory &memory, std::uint32_t address, std::uint32_t value) {
    memory.write(address, value & 0xffU, 1);
}
void put16(Memory &memory, std::uint32_t address, std::uint32_t value) {
    memory.write(address, value & 0xffffU, 2);
}
void put32(Memory &memory, std::uint32_t address, std::uint32_t value) {
    memory.write(address, value, 4);
}

constexpr std::uint32_t text_state = 0x80059360; // pointer to the resident text state
constexpr std::uint32_t tim_cursor = 0x8005a37c; // OpenTIM / ReadTIM position
constexpr std::uint32_t gpu_debug = 0x800568d2;  // libgpu checking level

[[noreturn]] void missing(std::string_view operation, std::uint32_t address, const char *id,
                          const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}

// A sprite part (1c bytes) of a sheet.
constexpr std::uint32_t part_stride = 0x1c;
constexpr std::uint32_t part_u = 0x0, part_v = 0x2, part_w = 0x4, part_h = 0x6;
constexpr std::uint32_t part_x = 0x8, part_y = 0xa;
constexpr std::uint32_t part_depth = 0x10, part_clut_x = 0x12, part_clut_y = 0x14;
constexpr std::uint32_t part_page_x = 0x16, part_page_y = 0x18;
constexpr std::uint32_t part_mirror_x = 0x1a, part_mirror_y = 0x1b;
// POLY_FT4 fields.
constexpr std::uint32_t quad_bytes = 0x28;      // one draw buffer's packet
constexpr std::uint32_t quad_part_bytes = 0x50; // both buffers' packets of a part
constexpr std::uint32_t quad_x0 = 0x8, quad_y0 = 0xa, quad_x1 = 0x10, quad_y1 = 0x12;
constexpr std::uint32_t quad_x2 = 0x18, quad_y2 = 0x1a, quad_x3 = 0x20, quad_y3 = 0x22;
constexpr std::uint32_t quad_uv0 = 0xc, quad_clut = 0xe, quad_uv1 = 0x14, quad_page = 0x16;
constexpr std::uint32_t quad_uv2 = 0x1c, quad_uv3 = 0x24;

// A part's offset or size scaled by scale / 1000h (mult keeps the low word;
// a negative product rounds toward zero).
std::uint32_t scaled(const Memory &memory, std::uint32_t at, std::uint32_t scale) {
    auto product = static_cast<std::int32_t>(static_cast<std::uint32_t>(s16(memory, at)) * scale);
    if (product < 0)
        product += 0xfff;
    return static_cast<std::uint32_t>(product >> 12);
}

// The start of a part's quad: SetPolyFT4 (80043cb0), SetSemiTrans(0)
// (80043bfc), SetShadeTex(1) (80043c24), GetTPage(depth, 0, page x, y)
// (80043a1c) and GetClut(x, y) (80043a58).
void start_quad(Memory &memory, std::uint32_t packet, std::uint32_t part) {
    put8(memory, packet + 3, 9);
    put8(memory, packet + 7, 0x2c);
    put8(memory, packet + 7, u8(memory, packet + 7) & 0xfdU);
    put8(memory, packet + 7, u8(memory, packet + 7) | 1U);
    put16(memory, packet + quad_page,
          gpu::texture_page(static_cast<std::uint32_t>(s16(memory, part + part_depth)), 0,
                            s16(memory, part + part_page_x), s16(memory, part + part_page_y)));
    const auto clut_x = s16(memory, part + part_clut_x);
    const auto clut_y = s16(memory, part + part_clut_y);
    put16(memory, packet + quad_clut,
          static_cast<std::uint32_t>(clut_y) << 6U |
              (static_cast<std::uint32_t>(clut_x >> 4) & 0x3fU));
}

// The quad's texture corners from u, v and the texel extent w, h.
void quad_texture(Memory &memory, std::uint32_t packet, std::uint32_t u, std::uint32_t v,
                  std::uint32_t w, std::uint32_t h) {
    put8(memory, packet + quad_uv0, u);
    put8(memory, packet + quad_uv0 + 1, v);
    put8(memory, packet + quad_uv1, u + w);
    put8(memory, packet + quad_uv1 + 1, v);
    put8(memory, packet + quad_uv2, u);
    put8(memory, packet + quad_uv2 + 1, v + h);
    put8(memory, packet + quad_uv3, u + w);
    put8(memory, packet + quad_uv3 + 1, v + h);
}

// A texture start one texel back (a mirrored edge), clamped at zero, where
// the extent then shrinks by one; the test is bit 15 of the decrement.
void step_back(std::uint32_t &start, std::uint32_t &extent) {
    start -= 1;
    if ((start & 0x8000U) != 0) {
        start = 0;
        extent -= 1;
    }
}

std::uint32_t text_table(const Memory &memory, std::uint32_t offset, std::uint32_t index) {
    return offset_table_entry(memory, u32(memory, u32(memory, text_state) + offset), index);
}
} // namespace

// 80033728(table, index): table + the halfword at table + 4 + index * 2.
std::uint32_t offset_table_entry(const Memory &memory, std::uint32_t table, std::uint32_t index) {
    return table + u16(memory, index * 2 + table + 4);
}

// 800337e8: entry `id` of the accessory name table (text state +44).
std::uint32_t accessory_name(const Memory &memory, std::uint32_t id) {
    return text_table(memory, 0x44, id);
}
// 80033818: entry `id` of the item name table (text state +58).
std::uint32_t item_name(const Memory &memory, std::uint32_t id) {
    return text_table(memory, 0x58, id);
}
// 80033848: entry `id` of the weapon name table (text state +5c).
std::uint32_t weapon_name(const Memory &memory, std::uint32_t id) {
    return text_table(memory, 0x5c, id);
}
// 80033a2c: entry `id` of the gear accessory name table (text state +c8).
std::uint32_t gear_accessory_name(const Memory &memory, std::uint32_t id) {
    return text_table(memory, 0xc8, id);
}
// 80033a5c: entry `id` of the gear weapon name table (text state +cc).
std::uint32_t gear_weapon_name(const Memory &memory, std::uint32_t id) {
    return text_table(memory, 0xcc, id);
}

// 8003342c(list): add the list's address to each of its `count` words after
// the count (the count is re-read for each comparison, unsigned).
std::uint32_t relocate_offsets(Memory &memory, std::uint32_t list) {
    if (u32(memory, list) == 0)
        return 0;
    std::uint32_t entry = 1;
    auto at = list + 4;
    do {
        put32(memory, at, u32(memory, at) + list);
        ++entry;
        at += 4;
    } while (!(u32(memory, list) < entry));
    return u32(memory, list);
}

// 80032e88(packed, mode): allocate the unpacked size (the packed data's first
// word) with `mode` and unpack into it through 80032eb4.
std::uint32_t unpack_to_new_block(Memory &memory, std::uint32_t packed, std::uint32_t mode,
                                  const Allocate &allocate) {
    const auto block = allocate(u32(memory, packed), mode);
    if (block == 0)
        return 0;
    static_cast<void>(field::decode_packed_through(
        [&](std::size_t position) {
            return static_cast<std::uint8_t>(
                u8(memory, packed + static_cast<std::uint32_t>(position)));
        },
        [&](std::size_t index, std::uint8_t value) {
            put8(memory, block + static_cast<std::uint32_t>(index), value);
        },
        [&](std::size_t index) {
            return static_cast<std::uint8_t>(u8(memory, block + static_cast<std::uint32_t>(index)));
        }));
    return block;
}

// 8002dd20(list): from the last TIM to the first. OpenTIM (800471b4) sets
// the TIM cursor; ReadTIM (800471c4 through 80047518) checks the id word 10h,
// takes the flags (bit 3: a CLUT block follows), the CLUT rectangle and data
// (+4, +c of that block) and the pixel rectangle and data (+4, +c of the next
// block), and advances the cursor past both blocks. Its TIM_IMAGE lives on
// 8002dd20's frame and is only read there.
void load_tim_list(Memory &memory, std::uint32_t list, const std::function<void()> &draw_sync,
                   const std::function<void(std::uint32_t rect, std::uint32_t data)> &load_image) {
    for (auto k = u32(memory, list) - 1; k != 0xffffffffU; --k) {
        put32(memory, tim_cursor, list + (u32(memory, list + 4 + k * 4) & ~3U));
        const auto tim = u32(memory, tim_cursor);
        if (u32(memory, tim) != 0x10)
            missing("load_tim_list", 0x8002dd70, "symbol:readtim-not-tim-80047518",
                    "A list entry without the TIM id leaves ReadTIM's image unset (the "
                    "original then loads uninitialized stack words)");
        auto at = tim + 4;
        const auto flags = u32(memory, at);
        at += 4;
        if (u8(memory, gpu_debug) == 2)
            missing("load_tim_list", 0x80047570, "symbol:printf-80019964",
                    "libgpu TIM messages are not reconstructed");
        std::uint32_t clut_rect = 0, clut_data = 0, clut_words = 0;
        if ((flags & 8U) != 0) {
            const auto bytes = u32(memory, at);
            clut_rect = at + 4;
            clut_data = at + 0xc;
            clut_words = bytes >> 2U;
            at += clut_words * 4;
        }
        const auto pixel_rect = at + 4;
        const auto pixel_data = at + 0xc;
        put32(memory, tim_cursor,
              u32(memory, tim_cursor) + (clut_words + (u32(memory, at) >> 2U) + 2) * 4);
        if (clut_data != 0) {
            draw_sync();
            load_image(clut_rect, clut_data);
        }
        draw_sync();
        load_image(pixel_rect, pixel_data);
    }
}

// 80026338: the six results stored in order through their word addresses.
void sheet_part_texture(Memory &memory, std::uint32_t sheet, std::uint32_t id,
                        std::uint32_t count_out, std::uint32_t depth_out, std::uint32_t clut_x_out,
                        std::uint32_t clut_y_out, std::uint32_t vram_x_out,
                        std::uint32_t vram_y_out) {
    const auto sprite = offset_table_entry(memory, sheet, id);
    const auto part = sprite + 4;
    put32(memory, count_out, static_cast<std::uint32_t>(s16(memory, sprite)));
    // u in the high halfword, shifted back arithmetically: >> 2 when the
    // page depth is nonzero, >> 4 when it is zero.
    const auto u_high = static_cast<std::int32_t>(u16(memory, part + part_u) << 16U);
    const auto u_cells = s16(memory, part + part_depth) != 0 ? u_high >> 18 : u_high >> 20;
    put32(memory, depth_out, static_cast<std::uint32_t>(s16(memory, part + part_depth)));
    put32(memory, clut_x_out, static_cast<std::uint32_t>(s16(memory, part + part_clut_x)));
    put32(memory, clut_y_out, static_cast<std::uint32_t>(s16(memory, part + part_clut_y)));
    put32(memory, vram_x_out,
          static_cast<std::uint32_t>(
              static_cast<std::int16_t>(u16(memory, part + part_page_x) & 0xffc0U) + u_cells));
    put32(memory, vram_y_out,
          static_cast<std::uint32_t>(
              static_cast<std::int16_t>(u16(memory, part + part_page_y) & 0xff00U) +
              s16(memory, part + part_v)));
}

// 8002675c: each part's quad at x + scaled offset, of the scaled size; a
// mirrored axis swaps the quad's edges and starts the texture one texel back.
std::uint32_t sheet_quads(Memory &memory, std::uint32_t sheet, std::uint32_t id,
                          std::uint32_t packets, std::uint32_t buffer, std::uint32_t x,
                          std::uint32_t y, std::uint32_t scale) {
    const auto sprite = offset_table_entry(memory, sheet, id);
    const auto factor = scale & 0xffffU;
    for (std::uint32_t index = 0; index != static_cast<std::uint32_t>(s16(memory, sprite));
         ++index) {
        const auto part = sprite + 4 + index * part_stride;
        const auto packet = packets + index * quad_part_bytes + buffer * quad_bytes;
        const auto left = scaled(memory, part + part_x, factor);
        const auto top = scaled(memory, part + part_y, factor);
        const auto width = scaled(memory, part + part_w, factor);
        const auto height = scaled(memory, part + part_h, factor);
        start_quad(memory, packet, part);
        auto u = u16(memory, part + part_u);
        auto v = u16(memory, part + part_v);
        auto w = u16(memory, part + part_w);
        auto h = u16(memory, part + part_h);
        const auto near_x = x + left;
        const auto far_x = width + near_x;
        const bool mirror_x = u8(memory, part + part_mirror_x) != 0;
        put16(memory, packet + quad_x0, mirror_x ? far_x : near_x);
        put16(memory, packet + quad_x1, mirror_x ? near_x : far_x);
        put16(memory, packet + quad_x2, mirror_x ? far_x : near_x);
        put16(memory, packet + quad_x3, mirror_x ? near_x : far_x);
        if (mirror_x)
            step_back(u, w);
        const auto near_y = y + top;
        const auto far_y = height + near_y;
        const bool mirror_y = u8(memory, part + part_mirror_y) != 0;
        put16(memory, packet + quad_y0, mirror_y ? far_y : near_y);
        put16(memory, packet + quad_y1, mirror_y ? far_y : near_y);
        put16(memory, packet + quad_y2, mirror_y ? near_y : far_y);
        put16(memory, packet + quad_y3, mirror_y ? near_y : far_y);
        if (mirror_y)
            step_back(v, h);
        quad_texture(memory, packet, u, v, w, h);
    }
    return static_cast<std::uint32_t>(s16(memory, sprite));
}

// 800263e4: as 8002675c, with the scaled offset and size negated on each
// flipped axis; the texture steps back on an axis whose drawn edges came out
// reversed (x3 < x0, y3 < y0) rather than on the part's mirror bytes.
std::uint32_t sheet_quads_flipped(Memory &memory, std::uint32_t sheet, std::uint32_t id,
                                  std::uint32_t packets, std::uint32_t buffer, std::uint32_t x,
                                  std::uint32_t y, std::uint32_t scale, std::uint32_t flip_x,
                                  std::uint32_t flip_y) {
    const auto sprite = offset_table_entry(memory, sheet, id);
    const auto factor = scale & 0xffffU;
    for (std::uint32_t index = 0; index != static_cast<std::uint32_t>(s16(memory, sprite));
         ++index) {
        const auto part = sprite + 4 + index * part_stride;
        const auto packet = packets + index * quad_part_bytes + buffer * quad_bytes;
        auto left = scaled(memory, part + part_x, factor);
        auto top = scaled(memory, part + part_y, factor);
        auto width = scaled(memory, part + part_w, factor);
        auto height = scaled(memory, part + part_h, factor);
        start_quad(memory, packet, part);
        if ((flip_x & 0xffU) != 0) {
            width = 0U - width;
            left = 0U - left;
        }
        if ((flip_y & 0xffU) != 0) {
            top = 0U - top;
            height = 0U - height;
        }
        auto u = u16(memory, part + part_u);
        auto v = u16(memory, part + part_v);
        auto w = u16(memory, part + part_w);
        auto h = u16(memory, part + part_h);
        const auto near_x = x + left;
        const auto far_x = width + near_x;
        const bool mirror_x = u8(memory, part + part_mirror_x) != 0;
        put16(memory, packet + quad_x0, mirror_x ? far_x : near_x);
        put16(memory, packet + quad_x1, mirror_x ? near_x : far_x);
        put16(memory, packet + quad_x2, mirror_x ? far_x : near_x);
        put16(memory, packet + quad_x3, mirror_x ? near_x : far_x);
        const auto near_y = y + top;
        const auto far_y = height + near_y;
        const bool mirror_y = u8(memory, part + part_mirror_y) != 0;
        put16(memory, packet + quad_y0, mirror_y ? far_y : near_y);
        put16(memory, packet + quad_y1, mirror_y ? far_y : near_y);
        put16(memory, packet + quad_y2, mirror_y ? near_y : far_y);
        put16(memory, packet + quad_y3, mirror_y ? near_y : far_y);
        if (s16(memory, packet + quad_x3) < s16(memory, packet + quad_x0))
            step_back(u, w);
        if (s16(memory, packet + quad_y3) < s16(memory, packet + quad_y0))
            step_back(v, h);
        quad_texture(memory, packet, u, v, w, h);
    }
    return static_cast<std::uint32_t>(s16(memory, sprite));
}

// 80034eac: set up the layout window for one line (stores in the original
// order; the width halfword is stored raw, then made odd), run 80033df0 on
// it and return the first line record's width (+58) * 4.
std::uint32_t layout_text_line(Memory &memory, std::uint32_t text, std::uint32_t image,
                               std::uint32_t width, std::uint32_t plane,
                               const std::function<void(std::uint32_t window)> &glyphs) {
    const auto window = layout_window;
    const auto odd = width | 1U;
    put16(memory, window + 0xa, width);
    put16(memory, window + 0xc, 1); // one line
    put16(memory, window + 0x8, static_cast<std::uint32_t>(static_cast<std::int16_t>(odd)) << 2U);
    put16(memory, window + 0xa, odd);
    put32(memory, window + 0x1c, text);
    put8(memory, window + 0x68, 1);
    put16(memory, window + 0x12, odd + 3); // image row stride in halfwords
    put16(memory, window + 0x84, 0);
    put8(memory, window + 0x6c, 0);
    put8(memory, window + 0x6a, 0);
    put32(memory, window + 0x2c, image);
    put16(memory, window + 0x10, 0);
    put16(memory, window + 0x2, 0);
    put16(memory, window + 0x0, 0);
    put8(memory, window + 0x69, 100); // glyph budget
    put32(memory, window + 0x28, layout_line);
    put16(memory, layout_line + 0x58, 0);         // the line's width
    put8(memory, layout_line + 0x5a, plane & 1U); // the line's bit plane
    glyphs(window);
    return static_cast<std::uint32_t>(s16(memory, u32(memory, window + 0x28) + 0x58)) << 2U;
}

} // namespace xem::reconstruction::resident
