// The field load 80070cc8: the map data read ahead (8005a4e0) becomes the
// loaded field: the field state reset (800705dc), the decoded components,
// descriptors, model instances, actors, sprites and the camera. Original
// addresses name correlations only; owned records are Program state.
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/original_layout.hpp"
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

void Program::load_field(FrameServices &, const ProgramObserver &observe) {
    reset_field_state();
    observed(observe, *this, {"load_reset", 0x80070d1c, {}, {}});
    // The bundle's first 100h bytes (its header) to 800b1f78.
    const auto &bundle = resident.preload_block;
    if (bundle.bytes.size() < 0x100)
        throw field::FieldFormatError("The field load requires the read-ahead map data");
    for (std::uint32_t at = 0; at < 0x100; at += 4)
        set_memory(0x800b1f78 + at, memory(bundle.address + at));
    // The compass: 4 x 4 ring quads, five marks, then the letters.
    for (std::uint32_t row = 0; row < 4; ++row)
        for (std::uint32_t column = 0; column < 4; ++column)
            compass_record(0x800b06bc + 0x70U * (row * 4 + column), column, row, 0);
    for (std::uint32_t mark = 4; mark < 9; ++mark)
        compass_record(0x800b0dbc + 0x70U * (mark - 4), mark, mark, 1);
    compass_letters();
    observed(observe, *this, {"load_compass", 0x80070e88, {}, {}});
    throw MissingDependency({"load_field", 0x80070e88, {}, {}}, "symbol:field-load-80070cc8",
                            false, "The field load after its compass is not reconstructed");
}

} // namespace xem::reconstruction
