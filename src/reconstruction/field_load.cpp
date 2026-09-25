// The field load 80070cc8: the map data read ahead (8005a4e0) becomes the
// loaded field: the field state reset (800705dc), the decoded components,
// descriptors, model instances, actors, sprites and the camera. Original
// addresses name correlations only; owned records are Program state.
#include "xem/reconstruction/field_actor.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_sprite_model.hpp"
#include "xem/reconstruction/field_view.hpp"
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("The field load requires field state");
    return *program.field;
}
std::int32_t s32_of(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32_of(std::int32_t value) { return std::bit_cast<std::uint32_t>(value); }
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point) {
    if (observe)
        observe(program, point, true);
}
// Constant stores of the field reset 800705dc, in program order.
struct Store {
    std::uint32_t address;
    std::uint8_t width;
    std::uint32_t value;
};
constexpr std::array<Store, 92> resets_a{{
        {0x800adb00, 2, 0xffff},
        {0x800afe9c, 2, 0x0},
        {0x800afea0, 2, 0x0},
        {0x800c2694, 2, 0x0},
        {0x800c38f8, 2, 0x0},
        {0x800c3900, 2, 0x0},
        {0x800c3908, 2, 0x0},
        {0x800b2356, 1, 0x5},
        {0x800b2346, 2, 0x3},
        {0x800b234a, 2, 0x40},
        {0x800b234c, 2, 0xff},
        {0x800c3a38, 2, 0xff},
        {0x800c3a60, 4, 0x0},
        {0x800c3a5c, 4, 0x0},
        {0x800b21bc, 4, 0x0},
        {0x800b21c0, 4, 0x0},
        {0x800b21c4, 4, 0x0},
        {0x800b2264, 4, 0x0},
        {0x800b21b8, 4, 0x0},
        {0x800b21d1, 1, 0x0},
        {0x800b21d0, 1, 0x0},
        {0x800b2094, 4, 0x0},
        {0x800b2090, 4, 0x0},
        {0x800b208c, 4, 0x0},
        {0x800b2088, 4, 0x0},
        {0x800b2084, 4, 0x0},
        {0x800b2080, 4, 0x0},
        {0x800b20b2, 2, 0x0},
        {0x800b20b0, 2, 0x0},
        {0x800b2268, 4, 0x0},
        {0x800b21ce, 1, 0x0},
        {0x800b21b0, 2, 0x0},
        {0x800b21b2, 2, 0x0},
        {0x800b21ae, 2, 0x0},
        {0x800b2078, 2, 0x0},
        {0x800b21cd, 1, 0x0},
        {0x800b219f, 1, 0x0},
        {0x800b219e, 1, 0x0},
        {0x800b219d, 1, 0x0},
        {0x800b219c, 1, 0x0},
        {0x800b21cc, 1, 0x0},
        {0x800b21cf, 1, 0x0},
        {0x800b21d2, 1, 0x0},
        {0x800adb74, 4, 0x0},
        {0x800adb4c, 4, 0x0},
        {0x800adb90, 4, 0x0},
        {0x800adb24, 4, 0x0},
        {0x800adb98, 4, 0x0},
        {0x800adb94, 4, 0x0},
        {0x800adb70, 4, 0x0},
        {0x800adb2c, 4, 0x0},
        {0x800adb68, 4, 0x0},
        {0x800adb88, 4, 0x0},
        {0x800adba8, 4, 0x0},
        {0x800b0064, 4, 0x0},
        {0x800adb18, 4, 0x0},
        {0x800adb50, 4, 0x0},
        {0x800afe84, 4, 0x0},
        {0x800afd04, 4, 0x0},
        {0x800b02c8, 1, 0x0},
        {0x800adbd4, 4, 0x0},
        {0x800adbd0, 4, 0x0},
        {0x800b22e0, 2, 0x0},
        {0x800b233c, 2, 0x0},
        {0x800b14a4, 4, 0x0},
        {0x800b236c, 2, 0x0},
        {0x800adb38, 4, 0x0},
        {0x800adb3c, 4, 0x0},
        {0x800afd14, 4, 0x20},
        {0x800b0048, 4, 0x2},
        {0x800b21ac, 2, 0x3ff},
        {0x800adc18, 4, 0x4},
        {0x800adb44, 4, 0x0},
        {0x800adb02, 2, 0x0},
        {0x800b233e, 2, 0x0},
        {0x800b2344, 2, 0x0},
        {0x800b2342, 2, 0x0},
        {0x800b2348, 2, 0x0},
        {0x800b234e, 2, 0x0},
        {0x800b2355, 1, 0x0},
        {0x800adb04, 1, 0x0},
        {0x800adb05, 1, 0x0},
        {0x800b2357, 1, 0x0},
        {0x800b2354, 1, 0x0},
        {0x800adbb4, 4, 0x0},
        {0x800b2350, 4, 0x0},
        {0x800adb54, 2, 0x0},
        {0x800b2358, 1, 0x0},
        {0x800adb84, 4, 0x0},
        {0x800adb7c, 4, 0x0},
        {0x800adb8c, 4, 0x0},
        {0x800adb64, 4, 0xff},
}};
constexpr std::array<Store, 16> resets_b{{
        {0x800adc08, 2, 0x1},
        {0x800af93a, 2, 0x1000},
        {0x800b21d4, 2, 0x720},
        {0x800b21a4, 2, 0x100},
        {0x800b21a2, 2, 0x100},
        {0x800b21a0, 2, 0x100},
        {0x800b21aa, 2, 0x200},
        {0x800b21a8, 2, 0x200},
        {0x800b21a6, 2, 0x200},
        {0x800b21b4, 2, 0x80},
        {0x800adbc4, 4, 0xff},
        {0x800b218c, 2, 0x1000},
        {0x800b2184, 2, 0x0},
        {0x800b2186, 2, 0x0},
        {0x800b2188, 2, 0x0},
        {0x800adb6c, 4, 0xffffffff},
}};
constexpr std::array<Store, 23> resets_c{{
        {0x800b2290, 2, 0x1d},
        {0x800adbec, 4, 0xffffffff},
        {0x800b21d8, 4, 0x2},
        {0x800b217a, 2, 0xffff},
        {0x800b2192, 1, 0x80},
        {0x800b2191, 1, 0x80},
        {0x800b2190, 1, 0x80},
        {0x800b2196, 1, 0xff},
        {0x800b2195, 1, 0xff},
        {0x800b2194, 1, 0xff},
        {0x800b2198, 2, 0x15e0},
        {0x800b219a, 2, 0x300c},
        {0x800adb08, 4, 0x0},
        {0x800adc0c, 4, 0x0},
        {0x800afea8, 2, 0x0},
        {0x800b226c, 4, 0x0},
        {0x800b2178, 2, 0x0},
        {0x800b2174, 2, 0x0},
        {0x800b2176, 2, 0x0},
        {0x800b2180, 4, 0x0},
        {0x800b217c, 4, 0x0},
        {0x800b218e, 2, 0x0},
        {0x800b21d6, 2, 0x8},
}};
} // namespace

// A store to an original address; a store narrower than the Program value
// that owns it replaces those bytes of its aligned word.
void Program::store_original(std::uint32_t address, std::uint32_t value, std::size_t width) {
    try {
        set_memory(address, value, width);
        return;
    } catch (const field::FieldFormatError &) {
        if (width == 4)
            throw;
    }
    const auto aligned = address & ~3U;
    const auto shift = (address & 3U) * 8U;
    const auto mask = ((1U << (8U * width)) - 1U) << shift;
    set_memory(aligned, (memory(aligned) & ~mask) | ((value << shift) & mask));
}

// 80070594: the identity rotation (8003f738 of zero angles) with a zero
// translation; the halfword after the rotation keeps its value.
void Program::identity_matrix(std::uint32_t address) {
    const auto m = field::rotation_matrix({0, 0, 0}, resident.math.trigonometry);
    for (std::uint32_t i = 0; i < 9; ++i)
        set_memory(address + 2 * i, static_cast<std::uint16_t>(m.r[i]), 2);
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(address + 0x14 + 4 * i, 0);
}

// 800705dc: reset the field state for a new map.
void Program::reset_field_state() {
    auto &state = loaded(*this);
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"reset_field_state", 0x800705f8, {}, {}},
                                "symbol:field-debug-8028125c", false,
                                "The diagnostic overlay call 8028125c is not recovered");
    // 80071f64(0, 0, 140, e0): both draw buffers' areas.
    for (const auto block : {0x800b249cU, 0x800ba590U}) {
        set_memory(block, 0, 2);
        set_memory(block + 2, block == 0x800b249cU ? 0 : 0x100, 2);
        set_memory(block + 4, 0x140, 2);
        set_memory(block + 6, 0xe0, 2);
    }
    resident.input_queue.reset(); // 80035db0
    for (const auto &store : resets_a)
        store_original(store.address, store.value, store.width);
    // 8003f738 of zero angles into 800b00e8 (rotation only).
    const auto m = field::rotation_matrix({0, 0, 0}, resident.math.trigonometry);
    for (std::uint32_t i = 0; i < 9; ++i)
        set_memory(0x800b00e8 + 2 * i, static_cast<std::uint16_t>(m.r[i]), 2);
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(0x800b22e6 - 2 * i, 0xffff, 2);
    for (const auto &store : resets_b)
        store_original(store.address, store.value, store.width);
    for (std::uint32_t i = 0; i < 16; ++i)
        set_memory(0x800b228e - 2 * i, 0x1d, 2);
    for (const auto &store : resets_c)
        store_original(store.address, store.value, store.width);
    if (resident.w_4f30c == 0)
        for (std::uint32_t i = 0; i < 3; ++i) {
            set_memory(0x8005a444 + 4 * i, 0xff);
            set_memory(0x8006f990 + 4 * i, 0xff);
        }
    for (std::uint32_t i = 0; i < 32; ++i)
        set_memory(0x800b22de - 2 * i, 0xffff, 2);
    set_memory(0x800b229c, 0);
    set_memory(0x800b2298, 0);
    set_memory(0x80050100, 2);
    // The variable bank: the first half from the game data (+1930), the
    // second half cleared.
    auto &data = resident.game_data;
    if (data.size() < game_data_bytes)
        throw field::FieldFormatError("The field reset requires the game data");
    auto &words = resident.variables.words;
    for (std::size_t i = 0; i < 0x200; ++i) {
        words[i] = static_cast<std::uint16_t>(data[0x1930 + 2 * i] | data[0x1931 + 2 * i] << 8U);
        words[0x200 + i] = 0;
    }
    resident.gte.screen.h = 0x200; // SetGeomScreen (8004a14c)
    for (const auto address : {0x800af990U, 0x800af85cU, 0x800afa64U, 0x800afaa4U})
        identity_matrix(address);
    for (const auto address : {0x800afa54U, 0x800afa56U, 0x800afa58U, 0x800afa5cU, 0x800afa5eU,
                               0x800afa60U})
        set_memory(address, 0, 2);
    set_memory(0x800afac4, 0x3000);
    // 8003f738 of the angles at 800afa54 into 800afa64 (rotation only).
    const auto angles = field::GteVector{static_cast<std::int16_t>(memory(0x800afa54, 2)),
                                         static_cast<std::int16_t>(memory(0x800afa56, 2)),
                                         static_cast<std::int16_t>(memory(0x800afa58, 2))};
    const auto turn = field::rotation_matrix(angles, resident.math.trigonometry);
    for (std::uint32_t i = 0; i < 9; ++i)
        set_memory(0x800afa64 + 2 * i, static_cast<std::uint16_t>(turn.r[i]), 2);
    state.draw_block = 0x800b249c;
    reset_camera(); // 8007254c
    // 80070c84.
    for (std::uint32_t i = 0; i < 3; ++i) {
        set_memory(0x800b06a4 + 6 * i, 0xff, 2);
        set_memory(0x800b06a6 + 6 * i, 0xff, 2);
    }
    set_memory(0x800adb0c, 0);
    // 800864b4: no positional emitters.
    for (auto &emitter : state.emitters) {
        emitter[0] = 0xffff;
        emitter[1] = 0xffff;
    }
    // 800a9274: no particle emitters.
    for (std::size_t slot = 0; slot < state.particle_slots.size(); ++slot) {
        state.particle_slots[slot] = 0;
        state.reload.particle_ids[slot] = -1;
    }
    // 800abd18: five 80h x e0h sprites (SPRT), opaque, with a draw
    // mode per buffer.
    for (std::uint32_t i = 0; i < 5; ++i) {
        const auto tpage = gpu::texture_page(1, 0, static_cast<std::int32_t>(0x280U + 0x40U * i), 0);
        set_draw_mode(0x800b0188 + 0x18U * i, tpage, {0, 0, 0xff, 0xff});
        set_draw_mode(0x800b0194 + 0x18U * i, tpage, {0, 0, 0xff, 0xff});
        const auto poly = 0x800b0200 + 0x28U * i;
        set_memory(poly + 3, 4, 1); // SetPolyFT3 (80043d14)
        set_memory(poly + 7, 0x64, 1);
        set_memory(poly + 8, 0x80U * i, 2);
        for (std::uint32_t c = 4; c < 7; ++c)
            set_memory(poly + c, 0x80, 1);
        set_memory(poly + 0xa, 0, 2);
        set_memory(poly + 0xc, 0, 1);
        set_memory(poly + 0xd, 0, 1);
        set_memory(poly + 0x10, 0x80, 2);
        set_memory(poly + 0x12, 0xe0, 2);
        set_memory(poly + 7, memory(poly + 7, 1) & 0xfdU, 1); // SetSemiTrans(0)
        set_memory(poly + 0xe, 0xe8U << 6U, 2);               // GetClut(0, e8)
        for (std::uint32_t at = 0; at < 0x14; at += 4)
            set_memory(poly + 0x14 + at, memory(poly + at));
    }
}

// 8007254c: the camera's initial state.
void Program::reset_camera() {
    set_memory(0x800af984, 8);
    set_memory(0x800af988, 8);
    set_memory(0x800af9ec, 0x00400000);
    set_memory(0x800af9f0, 0x08000000);
    set_memory(0x800af9e6, 0x800, 2);
    for (const auto address : {0x800afa38U, 0x800afa34U, 0x800afa30U, 0x800afa44U, 0x800afa40U,
                               0x800afa3cU})
        set_memory(address, 0);
    set_memory(0x800afa28, 0, 2);
    set_memory(0x800afa2c, 0, 2);
    set_memory(0x800af9d8, 0);
    set_memory(0x800af98e, 0, 2);
    set_memory(0x800af98c, 0, 2);
    set_memory(0x800af9f4, 0, 1);
    set_memory(0x800af9f5, 0, 1);
    set_memory(0x800af9f6, 0, 2);
    set_memory(0x800af9e4, 0, 2);
    set_memory(0x800af9e8, 0, 2);
    set_memory(0x800afa0c, 0x800);
    set_memory(0x800af9d0, 0, 2);
    set_memory(0x800af9d2, 0, 2);
    set_memory(0x800af9d4, 0, 2);
    identity_matrix(0x800af9b0);
    for (const auto address : {0x800af880U, 0x800af884U, 0x800af888U})
        set_memory(address, 0);
    set_memory(0x800af8a4, 0x10000000);
    set_memory(0x800af8d4, 0x10000000);
    set_memory(0x800af9fc, 0x1e, 2);
    set_memory(0x800af9f8, 0x200);
    for (const auto address : {0x800af890U, 0x800af894U, 0x800af898U, 0x800af8a0U, 0x800af8a8U,
                               0x800af8e0U, 0x800af8e4U, 0x800af8e8U, 0x800af8b0U, 0x800af8b4U,
                               0x800af8b8U, 0x800af8c0U, 0x800af8c4U, 0x800af8c8U, 0x800af8d0U,
                               0x800af8d8U})
        set_memory(address, 0);
    set_memory(0x800afa00, 0, 2);
    set_memory(0x800afa10, 0, 2);
    set_memory(0x800afa1c, 0, 2);
    set_memory(0x800af9fe, 0x1000, 2);
    set_memory(0x800af934, 0, 2);
    set_memory(0x800af93c, 0, 2);
    set_memory(0x800af93e, 0, 2);
    set_memory(0x800af960, 0, 2);
}

// 8007a44c: the four texture coordinates of a POLY_FT4, each clamped to
// 0..ff (negative halfwords become 0).
void Program::set_quad_uv(std::uint32_t packet, const std::array<std::int32_t, 8> &uv) {
    constexpr std::array<std::uint32_t, 8> offsets{0xc, 0xd, 0x14, 0x15, 0x1c, 0x1d, 0x24, 0x25};
    for (std::size_t i = 0; i < 8; ++i) {
        auto value = static_cast<std::int16_t>(uv[i]) < 0 ? 0 : uv[i];
        if (static_cast<std::int16_t>(value) >= 0x100)
            value = 0xff;
        set_memory(packet + offsets[i], static_cast<std::uint32_t>(value) & 0xffU, 1);
    }
}

// 8007a7f4(record, column, row, style): a compass quad from the overlay's
// corner tables (800adce0 by column, 800add28 by row), texture coordinates
// (800add70, 800addb8) and texture page and palette (800ade00 by style);
// the packet is copied to the second buffer.
void Program::compass_record(std::uint32_t record, std::uint32_t column, std::uint32_t row,
                             std::uint32_t style) {
    const auto packet = record + 0x20;
    set_memory(packet + 3, 9, 1); // SetPolyFT4 (80043cb0)
    set_memory(packet + 7, 0x2c, 1);
    const auto columns = 0x800adce0U + column * 8U, rows = 0x800add28U + row * 8U;
    for (std::uint32_t i = 0; i < 4; ++i) {
        set_memory(record + 8 * i + 2, 0, 2);
        set_memory(record + 8 * i, overlay_half(columns + 2 * i), 2);
        set_memory(record + 8 * i + 4, overlay_half(rows + 2 * i), 2);
    }
    for (std::uint32_t c = 4; c < 7; ++c)
        set_memory(packet + c, 0x80, 1);
    const auto styles = 0x800ade00U + style * 12U;
    const auto signed_half = [&](std::uint32_t address) {
        return static_cast<std::int32_t>(static_cast<std::int16_t>(overlay_half(address)));
    };
    const auto tpage = gpu::texture_page(static_cast<std::uint32_t>(signed_half(styles)),
                                         static_cast<std::uint32_t>(signed_half(styles + 2)),
                                         signed_half(styles + 4), signed_half(styles + 6));
    set_memory(packet + 0x16, tpage, 2);
    const auto clut_x = static_cast<std::uint32_t>(signed_half(styles + 8));
    const auto clut_y = static_cast<std::uint32_t>(signed_half(styles + 0xa));
    set_memory(packet + 0xe, (clut_y << 6U | (clut_x >> 4U & 0x3fU)) & 0xffffU, 2); // GetClut
    const auto us = 0x800add70U + column * 8U, vs = 0x800addb8U + row * 8U;
    set_quad_uv(packet, {signed_half(us), signed_half(vs), signed_half(us + 2), signed_half(vs + 2),
                         signed_half(us + 4), signed_half(vs + 4), signed_half(us + 6),
                         signed_half(vs + 6)});
    for (std::uint32_t at = 0; at < 0x28; at += 4)
        set_memory(record + 0x48 + at, memory(packet + at));
}

// 8007a5c4: the four compass letters (records 800b0fec + 70h * k) from the
// overlay's corner table 800ade30 and texture bytes 800ade70.
void Program::compass_letters() {
    for (std::uint32_t k = 0; k < 4; ++k) {
        const auto record = 0x800b0fecU + 0x70U * k;
        const auto packet = record + 0x20;
        set_memory(packet + 3, 9, 1); // SetPolyFT4 (80043cb0)
        set_memory(packet + 7, 0x2c, 1);
        const auto corners = 0x800ade30U + 0x10U * k;
        for (std::uint32_t i = 0; i < 4; ++i) {
            set_memory(record + 8 * i + 2, 0, 2);
            set_memory(record + 8 * i, overlay_half(corners + 4 * i), 2);
            set_memory(record + 8 * i + 4, overlay_half(corners + 4 * i + 2), 2);
        }
        for (std::uint32_t c = 4; c < 7; ++c)
            set_memory(packet + c, 0x80, 1);
        const auto bytes = 0x800ade70U + 8U * k;
        std::array<std::int32_t, 8> uv{};
        for (std::uint32_t i = 0; i < 8; ++i)
            uv[i] = overlay_byte(bytes + i) + (i % 2 == 1 ? 0xc0 : 0);
        set_quad_uv(packet, uv);
        set_memory(packet + 7, memory(packet + 7, 1) | 2U, 1); // SetSemiTrans(1)
        set_memory(packet + 0x16, gpu::texture_page(0, 2, 0x280, 0x1c0), 2);
        set_memory(packet + 0xe, (0xf2U << 6U) | (0x100U >> 4U & 0x3fU), 2); // GetClut(100, f2)
        for (std::uint32_t at = 0; at < 0x28; at += 4)
            set_memory(record + 0x48 + at, memory(packet + at));
    }
}

// A new heap block the field load fills (80031bdc at `site`): its bytes are
// raw heap contents until a record adopts them.
std::uint32_t Program::load_block(std::uint32_t size, std::uint32_t mode, std::uint32_t site) {
    auto block = resident::heap_allocate(resident.heap, size, mode, site);
    if (!block)
        throw field::FieldFormatError("A field load allocation failed");
    resident.heap_contents[block->address] = std::move(block->bytes);
    return block->address;
}

// A RAM byte through whatever Program state holds it: owned records,
// heap headers and heap-held bytes, then globals.
std::uint8_t Program::ram_byte(std::uint32_t address) {
    address = 0x80000000U | (address & 0x1fffffU);
    if (const auto bytes = owned_span(address); !bytes.empty())
        return bytes[0];
    const auto &heap = resident.heap;
    if (const auto after = heap.headers.upper_bound(address); after != heap.headers.begin()) {
        const auto &[at, words] = *std::prev(after);
        if (address - at < 8)
            return static_cast<std::uint8_t>(words[(address - at) / 4] >> (8U * (address % 4)));
    }
    if (const auto after = heap.held.upper_bound(address); after != heap.held.begin()) {
        const auto &[at, bytes] = *std::prev(after);
        if (address - at < bytes.size())
            return bytes[address - at];
    }
    return static_cast<std::uint8_t>(memory(address, 1));
}

// 8007008c: decode component `index` of the bundle (8005a4e0) into owned
// memory at `destination` (80032eb4). A stream may read past the bundle's
// end into the memory after it.
void Program::decode_component(std::uint32_t index, std::uint32_t destination) {
    const auto &bundle = resident.preload_block;
    const auto offset = memory(bundle.address + 0x130 + 4 * index);
    if (offset >= bundle.bytes.size())
        throw field::FieldFormatError("A component lies outside the read-ahead bundle");
    std::vector<std::uint8_t> source(bundle.bytes.begin() + offset, bundle.bytes.end());
    const auto end = bundle.address + static_cast<std::uint32_t>(bundle.bytes.size());
    for (std::uint32_t i = 0; i < 0x40; ++i)
        source.push_back(ram_byte(end + i));
    const auto decoded = field::decode_packed_block(source, 0x200000);
    const auto target = owned_span(destination);
    if (decoded.data.size() > target.size())
        throw field::FieldFormatError("A decoded component overruns its destination");
    std::ranges::copy(decoded.data, target.begin());
}

// 800771f8: load each TIM image of a list into VRAM: OpenTIM (800471b4),
// then ReadTIM (800471c4, 80047518) until a word other than 10h; the CLUT
// and pixel rectangles are loaded where they lie.
void Program::load_tim_images(FrameServices &services, std::uint32_t tim) {
    auto &gpu = resident.gpu;
    auto &cursor = gpu.tim_cursor;
    cursor = tim;
    for (;;) {
        auto at = cursor;
        if (memory(at) != 0x10)
            return;
        at += 4;
        const auto flags = memory(at);
        at += 4;
        if (gpu.debug == 2)
            throw MissingDependency({"load_tim_images", 0x80047570, {}, {}},
                                    "symbol:printf-80019964", false,
                                    "libgpu TIM messages are not reconstructed");
        std::uint32_t clut_rect = 0, clut_words = 0;
        if ((flags & 8U) != 0) {
            clut_rect = at + 4;
            clut_words = memory(at) >> 2U;
            at += clut_words * 4U;
        }
        const auto image_rect = at + 4;
        cursor += (clut_words + (memory(at) >> 2U) + 2U) * 4U;
        if (clut_rect != 0)
            load_image_at(services, clut_rect, clut_rect + 8);
        load_image_at(services, image_rect, image_rect + 8);
    }
}

// 80022a70(data, x, y): load each image of a list side by side from
// (x, y), 40h apart, through 80022a0c, which runs LoadImage on a stack in a
// 2000h heap block. `frame` is 80022a70's stack frame (its rectangle at +10).
void Program::load_images_across(FrameServices &services, std::uint32_t data, std::uint32_t x,
                                 std::uint32_t y, std::uint32_t frame) {
    const auto count = memory(data);
    for (std::uint32_t k = 0; s32_of(k) < s32_of(count); ++k) {
        const auto entry = data + memory(data + 4 + 4 * k);
        std::array<std::int16_t, 4> rect{static_cast<std::int16_t>(x + 0x40U * k),
                                         static_cast<std::int16_t>(y),
                                         static_cast<std::int16_t>(memory(entry, 2)),
                                         static_cast<std::int16_t>(memory(entry + 2, 2))};
        resident.image_upload = {frame + 0x10, entry + 4};
        const auto stack = load_block(0x2000, 1, 0x80022a1c);
        resident.switched_stacks.push_back({stack + 0x1f00 - 0x800, 0x804});
        static_cast<void>(load_image(rect, frame + 0x10, entry + 4, &services));
        static_cast<void>(release_owned_block(stack, 0x80022a54));
    }
}

// 8002c3e8: relocate a model group's offsets to addresses, once (+4 bit 0):
// each 38h-byte model's four table offsets and its optional list at +2c,
// whose entries' two offsets are relocated from the last (index +0) down.
void Program::relocate_model_group(std::uint32_t group) {
    const auto flags = memory(group + 4);
    const auto count = memory(group);
    if ((flags & 1U) != 0)
        return;
    set_memory(group + 4, flags | 1U);
    for (std::uint32_t k = 0; s32_of(k) < s32_of(count); ++k) {
        const auto model = group + 0x18 + 0x38 * k;
        for (const auto at : {0U, 8U, 4U, 0xcU})
            set_memory(model + at, memory(model + at) + group);
        const auto list = memory(model + 0x14);
        if (list == 0)
            continue;
        set_memory(model + 0x14, list + group);
        const auto entries = memory(list + group);
        if (entries == 0xffffffffU)
            continue;
        for (auto i = s32_of(entries); i >= 0; --i) {
            const auto entry = list + group + 4 + 0xcU * static_cast<std::uint32_t>(i);
            set_memory(entry + 4, memory(entry + 4) + group);
            set_memory(entry + 8, memory(entry + 8) + group);
        }
    }
}

// 80030a30(index, light): light `index` of the light matrix (80059f64) is
// the normalized reverse of its direction (80048d68), its colors a column
// of the color matrix (80059f84), loaded into the GTE.
void Program::set_field_light(std::uint32_t index, std::uint32_t light) {
    const field::FieldVector reverse{-s32_of(memory(light)), -s32_of(memory(light + 4)),
                                     -s32_of(memory(light + 8))};
    const auto normal = field::normalize_field_vector(reverse, resident.math.reciprocal);
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(0x80059f64 + 6 * index + 2 * i, static_cast<std::uint16_t>(normal[i]), 2);
    for (std::uint32_t c = 0; c < 3; ++c)
        set_memory(0x80059f84 + 6 * c + 2 * index, memory(light + 0xc + 2 * c, 2), 2);
    for (std::uint32_t k = 0; k < 5; ++k)
        resident.gte.set_control(16 + k, memory(0x80059f84 + 4 * k));
}

// 8006fdec(view): the camera matrix (800af990) from the eye, target and up
// (80073750), the field rotation (800afa54 -> 800afa64) under it, the three
// lights and background color from the bundle, then the model light matrix
// (80030b14) under the field rotation.
void Program::setup_field_view(std::uint32_t view) {
    auto &gte = resident.gte;
    const auto matrix = [&](std::uint32_t address) { return memory_matrix(address); };
    const auto words = [&](std::uint32_t address) {
        return field::GteLong{s32_of(memory(address)), s32_of(memory(address + 4)),
                              s32_of(memory(address + 8))};
    };
    auto camera = matrix(0x800af990);
    field::build_view(gte, resident.math.reciprocal, camera, words(0x800af880), words(0x800af890),
                      words(0x800af8a0));
    set_memory_matrix(0x800af990, camera);
    const auto angles = field::GteVector{static_cast<std::int16_t>(memory(0x800afa54, 2)),
                                         static_cast<std::int16_t>(memory(0x800afa56, 2)),
                                         static_cast<std::int16_t>(memory(0x800afa58, 2))};
    auto field_turn =
        field::rotation_matrix(angles, resident.math.trigonometry, matrix(0x800afa64));
    set_memory_matrix(0x800afa64, field_turn);
    gte.transform.r = camera.r; // MulMatrix2 (80049bdc)
    field::multiply_rotation(camera, field_turn);
    set_memory_matrix(0x800afa64, field_turn);
    auto source = view;
    const auto next_half = [&](std::uint32_t step) {
        const auto value = memory(source, 2);
        source += step;
        return value;
    };
    for (std::uint32_t n = 0; n < 3; ++n) {
        const auto light = 0x800afac8 + 0x14 * n;
        set_memory(light, u32_of(static_cast<std::int16_t>(next_half(2))));
        set_memory(light + 4, u32_of(static_cast<std::int16_t>(next_half(2))));
        set_memory(light + 8, u32_of(static_cast<std::int16_t>(next_half(4))));
        set_memory(light + 0xc, (next_half(2) << 3U) & 0xffffU, 2);
        set_memory(light + 0xe, (next_half(2) << 3U) & 0xffffU, 2);
        set_memory(light + 0x10, (memory(source, 2) << 3U) & 0xffffU, 2);
        source += 4;
        if (n == 2)
            for (const auto copy : {0x800afadcU, 0x800afaf0U})
                for (std::uint32_t at = 0; at < 0x14; at += 4)
                    set_memory(copy + at, memory(0x800afac8 + at));
        set_field_light(n, light);
    }
    for (std::uint32_t c = 0; c < 3; ++c)
        set_memory(0x800afb04 + 2 * c, (memory(source + 2 * c, 2) << 4U) & 0xffffU, 2);
    gte.transform = matrix(0x800af990); // SetRotMatrix, SetTransMatrix
    // RotTrans (8004a6dc) of the vector at 800afa5c into 800afa78.
    const auto moved = field::rot_trans(
        gte.transform, {static_cast<std::int16_t>(memory(0x800afa5c, 2)),
                        static_cast<std::int16_t>(memory(0x800afa5e, 2)),
                        static_cast<std::int16_t>(memory(0x800afa60, 2))});
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(0x800afa78 + 4 * i, u32_of(moved[i]));
    // 80030b14: the light matrix under the field rotation.
    const auto &light_source = resident.sprite_models.light_source;
    gte.transform.r = light_source.r;
    auto lit = matrix(0x800afa64);
    field::multiply_rotation(light_source, lit);
    for (std::uint32_t k = 0; k < 4; ++k)
        gte.set_control(8 + k, u32_of(static_cast<std::uint16_t>(lit.r[k * 2])) |
                                   u32_of(static_cast<std::uint16_t>(lit.r[k * 2 + 1])) << 16U);
    gte.set_control(12, u32_of(lit.r[8]));
    gte.transform = matrix(0x800afa64); // SetRotMatrix, SetTransMatrix
}

// 8002cb54, 8002c8cc (mode zero) and the copy for the second buffer: a
// model instance's packet buffers (class 25h), one per draw buffer.
void Program::build_model_instance(std::uint32_t instance, std::uint32_t mode) {
    auto &heap = resident.heap;
    const auto model = memory(instance + 4);
    heap.allocation_class = 0x25; // 800324b8
    const auto size = memory(model + 0x34);
    const auto packets = load_block(size * 2U, 0, 0x8002cb84);
    set_memory(instance + 8, packets);
    set_memory(instance + 0xc, packets + size);
    if (mode != 0)
        throw MissingDependency({"build_model_instance", 0x8002c920, {}, {}},
                                "symbol:field-model-mode", false,
                                "Model packets other than mode zero are not recovered");
    auto &state = loaded(*this);
    std::vector<field::SpriteResource> resources;
    for (const auto &resource : state.resources)
        resources.push_back({resource.address, resource.bytes});
    const auto geometry = state.reload.geometry_address;
    resources.push_back({geometry, resident.heap_contents.at(geometry)});
    field::SpriteSources sources{};
    sources.resources = resources;
    sources.models = &resident.sprite_models;
    auto &contents = resident.heap_contents.at(packets);
    field::SpriteAllocation buffer{packets, std::move(contents)};
    field::initialize_model_packets(model, buffer, resident.sprite, sources);
    contents = std::move(buffer.bytes);
    // 8003f968: the second buffer copies the first.
    std::copy_n(contents.begin(), size, contents.begin() + size);
}

// 8002c644 with 80031f70: mark the model trimmed (+4 bit 1) and, when the
// pseudo-block before it (-8) has room, end it after the model's data.
void Program::trim_model(std::uint32_t model) {
    const auto flags = memory(model + 4);
    if ((flags & 2U) != 0)
        return;
    const auto size = memory(model + 0x24) - model;
    set_memory(model + 4, flags | 2U);
    const auto next = memory(model - 8);
    if (size + 16U < next - (model - 8U) - 16U)
        throw MissingDependency({"trim_model", 0x80031fa8, {}, {}}, "symbol:heap-trim-80031f70",
                                false, "Trimming a model's pseudo-block is not recovered");
}

// 8007aa44: an actor's ground shadow: a 30h-wide square quad (semi-
// transparent, abr 2) and its packet for both buffers.
void Program::build_shadow(std::uint32_t shadow) {
    const auto packet = shadow + 0x20;
    set_memory(packet + 3, 9, 1); // SetPolyFT4 (80043cb0)
    set_memory(packet + 7, 0x2c, 1);
    constexpr std::array<std::int16_t, 12> corners{0x18, 0, 0x18, -0x18, 0, 0x18,
                                                   0x18, 0, -0x18, -0x18, 0, -0x18};
    for (std::uint32_t i = 0; i < 4; ++i)
        for (std::uint32_t axis = 0; axis < 3; ++axis)
            set_memory(shadow + 8 * i + 2 * axis, static_cast<std::uint16_t>(corners[3 * i + axis]),
                       2);
    for (std::uint32_t c = 4; c < 7; ++c)
        set_memory(packet + c, 0x80, 1);
    set_memory(packet + 0x16, gpu::texture_page(0, 2, 0x280, 0x1e0), 2);
    set_memory(packet + 0xe, (0xf3U << 6U) | (0x100U >> 4U & 0x3fU), 2); // GetClut(100, f3)
    set_memory(packet + 7, memory(packet + 7, 1) | 2U, 1);              // SetSemiTrans(1)
    set_quad_uv(packet, {0, 0xe0, 0xf, 0xe0, 0, 0xef, 0xf, 0xef});
    for (std::uint32_t at = 0; at < 0x28; at += 4)
        set_memory(shadow + 0x48 + at, memory(packet + at));
}

// 80080f44(index): event actor `index`'s record (138h bytes, cleared), its
// defaults (80080a74) and its ground shadow (8007aa44).
void Program::create_field_actor(std::uint32_t index) {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    if (s32_of(index) >= s32_of(reload.event_actors))
        return;
    set_memory(0x800b2180, memory(0x800b2180) + 1);
    const auto descriptor = reload.descriptor_table + 0x5cU * index;
    const auto actor = load_block(0x138, 0, 0x80080f88);
    set_memory(descriptor + 0x4c, actor);
    for (std::uint32_t at = 0; at < 0x138; at += 4)
        set_memory(actor + at, 0);
    set_memory(descriptor + 0x5a, 0, 2);
    if ((memory(descriptor + 0x58, 2) & 0x2000U) != 0)
        throw MissingDependency({"create_field_actor", 0x80081028, {}, {}},
                                "symbol:field-model-animation", false,
                                "Actors of animated models are not recovered");
    // 80080a74.
    field::original::ActorDefaults defaults{};
    std::ranges::copy(owned_span(actor).first(0x138), defaults.actor.begin());
    std::ranges::copy(owned_span(descriptor).first(0x5c), defaults.descriptor.begin());
    defaults.seed = resident.random_seed;
    for (std::size_t i = 0; i < 4; ++i)
        defaults.triangle_counts[i] = u32_of(state.triangle_counts[i]);
    defaults = field::original::initialize_actor_defaults(defaults, state.layer_count, state.collision,
                                                          resident.math.reciprocal);
    if (defaults.queried_layers == 0)
        throw MissingDependency({"create_field_actor", 0x80080a74, {}, {}},
                                "state:actor-defaults-stack", false,
                                "Without a queried layer the defaults read uninitialized stack");
    resident.random_seed = defaults.seed;
    std::ranges::copy(defaults.actor, owned_span(actor).begin());
    std::ranges::copy(defaults.descriptor, owned_span(descriptor).begin());
    for (std::size_t i = 0; i < 4; ++i)
        state.triangle_counts[i] = s32_of(defaults.triangle_counts[i]);
    const auto shadow = load_block(0x70, 0, 0x800810a8);
    set_memory(descriptor + 8, shadow);
    build_shadow(shadow);
}

// 80071318..800715a0: the descriptors (5ch each) from the bundle's 10h-byte
// records: flags, auxiliary halfwords, position; a model instance for each
// descriptor without flag 40, then the event actors (80080f44).
void Program::load_descriptors() {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    const auto bundle = resident.preload_block.address;
    const auto count = memory(bundle + 0x18c, 2);
    state.descriptor_count = count;
    reload.descriptor_table = load_block(count * 0x5cU, 0, 0x80071344);
    for (std::uint32_t at = 0; at < count * 0x5cU; at += 4)
        set_memory(reload.descriptor_table + at, 0);
    auto &heap = resident.heap;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto record = bundle + 0x190 + 0x10 * i;
        const auto descriptor = reload.descriptor_table + 0x5c * i;
        set_memory(descriptor + 0x58, memory(record, 2), 2);
        set_memory(descriptor + 0x50, memory(record + 2, 2), 2);
        set_memory(descriptor + 0x52, memory(record + 4, 2), 2);
        set_memory(descriptor + 0x54, memory(record + 6, 2), 2);
        for (std::uint32_t axis = 0; axis < 3; ++axis) {
            const auto value = memory(record + 8 + 2 * axis, 2);
            set_memory(descriptor + 0x20 + 4 * axis, value);
            set_memory(descriptor + 0x40 + 4 * axis, value);
        }
        const auto flags = memory(descriptor + 0x58, 2);
        if ((flags & 0x40U) == 0) {
            const auto instance = load_block(0x24, 0, 0x80071454);
            set_memory(descriptor, instance);
            const auto geometry = reload.geometry_address;
            const auto group = geometry + memory(geometry + 4 + 4 * memory(record + 0xe, 2));
            set_memory(instance + 4, group + 0x10);
            build_model_instance(instance, (flags & 0xcU) >> 2U);
            if ((flags & 0x2000U) != 0) {
                heap.tag = 3; // 80032498(3, 0)
                heap.tag_words[3] = 0;
                heap.quiet = 0;
                throw MissingDependency({"load_descriptors", 0x800303c8, {}, {}},
                                        "symbol:field-model-animation", false,
                                        "Animated model instances (800303c8) are not recovered");
            }
            trim_model(memory(instance + 4));
        } else {
            set_memory(descriptor + 0x58, flags | 0x20U, 2);
            for (const auto at : {0x50U, 0x52U, 0x54U})
                set_memory(descriptor + at, 0, 2);
        }
        create_field_actor(i);
    }
}

void Program::load_field(FrameServices &services, std::uint32_t frame,
                         const ProgramObserver &observe) {
    reset_field_state();
    observed(observe, *this, {"load_reset", 0x80070d1c, {}, {}});
    // The bundle's first 100h bytes (its header) to 800b1f78.
    const auto bundle = resident.preload_block.address;
    if (resident.preload_block.bytes.size() < 0x100)
        throw field::FieldFormatError("The field load requires the read-ahead map data");
    for (std::uint32_t at = 0; at < 0x100; at += 4)
        set_memory(0x800b1f78 + at, memory(bundle + at));
    // The compass: 4 x 4 ring quads, five marks, then the letters.
    for (std::uint32_t row = 0; row < 4; ++row)
        for (std::uint32_t column = 0; column < 4; ++column)
            compass_record(0x800b06bc + 0x70U * (row * 4 + column), column, row, 0);
    for (std::uint32_t mark = 4; mark < 9; ++mark)
        compass_record(0x800b0dbc + 0x70U * (mark - 4), mark, mark, 1);
    compass_letters();
    observed(observe, *this, {"load_compass", 0x80070e88, {}, {}});
    const auto size = [&](std::uint32_t index) { return memory(bundle + 0x10c + 4 * index); };
    // Component 0: TIM image lists, loaded into VRAM.
    const auto images = load_block(size(0) + 0x10, 1, 0x80070ea0);
    decode_component(0, images);
    for (std::uint32_t k = 0; s32_of(k) < s32_of(memory(images)); ++k)
        load_tim_images(services, images + memory(images + 4 + 4 * k));
    // Component 4: image lists placed at their header entry's position,
    // unless the entry's +6 is set.
    const auto placed = load_block(size(4) + 0x10, 0, 0x80070f10);
    decode_component(4, placed);
    for (std::uint32_t k = 0; s32_of(k) < s32_of(memory(placed)); ++k)
        if (memory(0x800b1f7e + 8 * k, 2) == 0)
            load_images_across(services, placed + memory(placed + 4 + 4 * k),
                               memory(0x800b1f78 + 8 * k, 2), memory(0x800b1f7a + 8 * k, 2),
                               frame - 0x38);
    draw_sync(services);
    static_cast<void>(release_owned_block(images, 0x80070fa0));
    static_cast<void>(release_owned_block(placed, 0x80070fa8));
    observed(observe, *this, {"load_images", 0x80070fb0, {}, {}});
    auto &state = loaded(*this);
    auto &reload = state.reload;
    // Component 2: the model groups, relocated in place.
    reload.geometry_address = load_block(size(2) + 0x10, 0, 0x80070fc8);
    const auto geometry = reload.geometry_address;
    decode_component(2, geometry);
    for (std::uint32_t k = 0; s32_of(k) < s32_of(memory(geometry)); ++k)
        relocate_model_group(geometry + memory(geometry + 4 + 4 * k));
    // Component 6 at 800658dc.
    decode_component(6, 0x800658dc);
    // Component 5: the event package, its actor count and bytecode.
    reload.events_address = load_block(size(5) + 0x10, 0, 0x80071088);
    decode_component(5, reload.events_address);
    reload.event_actors = memory(reload.events_address + 0x80);
    reload.event_bytecode = reload.events_address + 0x84 + reload.event_actors * 0x40;
    // Components 8 (trigger zones) and 7 (messages).
    reload.zones_address = load_block(size(8) + 0x10, 0, 0x800710f0);
    decode_component(8, reload.zones_address);
    state.messages_address = load_block(size(7) + 0x10, 0, 0x80071134);
    decode_component(7, state.messages_address);
    // Component 1: collision; its layer count, active triangles per layer
    // and the attribute, triangle and vertex tables.
    const auto collision = load_block(size(1) + 0x10, 0, 0x80071178);
    state.collision_address = collision;
    decode_component(1, collision);
    state.layer_count = static_cast<std::int16_t>(memory(collision));
    for (std::uint32_t i = 0; i < 4; ++i)
        state.triangle_counts[i] = s32_of(memory(collision + 4 + 4 * i) / 14U);
    set_memory(0x800afb20, collision + memory(collision + 0x14));
    for (std::uint32_t i = 0; s32_of(i) < state.layer_count; ++i) {
        set_memory(0x800afb24 + 4 * i, collision + memory(collision + 0x18 + 8 * i));
        set_memory(0x800afb34 + 4 * i, collision + memory(collision + 0x1c + 8 * i));
    }
    set_memory(0x800afd10, u32_of(s32_of(memory(0x800afb24) - memory(0x800afb20)) >> 2));
    state.collision = field::parse_collision_package(
        std::span(resident.heap_contents.at(collision)).first(size(1)));
    // Component 3: the sprite bundle.
    state.sprite_bundle_address = load_block(size(3) + 0x10, 0, 0x800712b8);
    decode_component(3, state.sprite_bundle_address);
    for (const auto address : {0x800af9dcU, 0x800af9deU, 0x800af9e0U, 0x800af9e2U})
        set_memory(address, 1, 2);
    setup_field_view(bundle + 0x154);
    observed(observe, *this, {"load_components", 0x80071318, {}, {}});
    load_descriptors();
    observed(observe, *this, {"load_descriptors", 0x800715a0, {}, {}});
    throw MissingDependency({"load_field", 0x800715a0, {}, {}}, "symbol:field-load-80070cc8",
                            false, "The field load after its descriptors is not reconstructed");
}

} // namespace xem::reconstruction
