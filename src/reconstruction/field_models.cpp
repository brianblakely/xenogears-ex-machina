// Field overlay 800748e8: per-descriptor model matrices and the resident model
// renderer (8002c700) with its primitive routines (table 8004fe50). Packets are
// written into each model instance's packet buffer for the current draw buffer
// and linked into the frame's ordering tables. Original addresses name
// correlations only; owned records are Program state.
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Model read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Model write exceeds owned storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int16_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }

// A matrix in original layout (rotation, sign halfword, translation).
field::GteMatrix read_matrix(std::span<const std::uint8_t> data, std::size_t offset) {
    field::GteMatrix m{};
    for (std::size_t i = 0; i < 9; ++i)
        m.r[i] = s16(word(data, offset + i * 2, 2));
    m.pad = s16(word(data, offset + 0x12, 2));
    for (std::size_t i = 0; i < 3; ++i)
        m.t[i] = s32(word(data, offset + 0x14 + i * 4));
    return m;
}

// The shape a primitive routine writes: vertex slot spacing within the packet,
// the length tag it ORs into the packet's link word, and the packet size.
struct PrimitiveShape {
    std::uint32_t slot;
    std::uint32_t tag;
    std::uint32_t size;
    bool quad;
    bool farthest; // Sort by the largest vertex depth (mode 2) instead of the average.
};
// Routine entry points of table 8004fe50 and their prologue constants.
std::optional<PrimitiveShape> primitive_shape(std::uint32_t routine) {
    switch (routine) {
    case 0x8002e038: // Flat triangle
        return PrimitiveShape{4, 0x04000000, 0x14, false, false};
    case 0x8002e04c: // Textured triangle
        return PrimitiveShape{8, 0x07000000, 0x20, false, false};
    case 0x8002e024: // Gouraud triangle
        return PrimitiveShape{8, 0x06000000, 0x1c, false, false};
    case 0x8002e010: // Gouraud textured triangle
        return PrimitiveShape{0xc, 0x09000000, 0x28, false, false};
    case 0x8002e254:
        return PrimitiveShape{4, 0x05000000, 0x18, true, false};
    case 0x8002e268:
        return PrimitiveShape{8, 0x09000000, 0x28, true, false};
    case 0x8002e240:
        return PrimitiveShape{8, 0x08000000, 0x24, true, false};
    case 0x8002e22c:
        return PrimitiveShape{0xc, 0x0c000000, 0x34, true, false};
    case 0x8002e470:
        return PrimitiveShape{4, 0x04000000, 0x14, false, true};
    case 0x8002e484:
        return PrimitiveShape{8, 0x07000000, 0x20, false, true};
    case 0x8002e45c:
        return PrimitiveShape{8, 0x06000000, 0x1c, false, true};
    case 0x8002e448:
        return PrimitiveShape{0xc, 0x09000000, 0x28, false, true};
    case 0x8002e674:
        return PrimitiveShape{4, 0x05000000, 0x18, true, true};
    case 0x8002e688:
        return PrimitiveShape{8, 0x09000000, 0x28, true, true};
    case 0x8002e660:
        return PrimitiveShape{8, 0x08000000, 0x24, true, true};
    case 0x8002e64c:
        return PrimitiveShape{0xc, 0x0c000000, 0x34, true, true};
    default:
        return std::nullopt;
    }
}
} // namespace

std::span<std::uint8_t> Program::descriptor_bytes(std::size_t index) {
    auto &state = *field;
    if (index < state.actors.size())
        return state.actors[index].descriptor;
    const auto piece = index - state.actors.size();
    if (piece >= state.pieces.size())
        throw field::FieldFormatError("Descriptor outside the owned field descriptors");
    return state.pieces[piece].descriptor;
}

// Field 800aaa74: the model's bounding centre in view (kept in 800b00e8's
// translation) and a screen test of two projected corners of its radius.
bool Program::model_culled(std::uint32_t instance) {
    auto &state = *field;
    auto &gte = resident.gte;
    const auto center =
        field::GteVector{s16(memory(instance + 0x18, 2)), s16(memory(instance + 0x1a, 2)),
                         s16(memory(instance + 0x1c, 2))};
    gte.set_vector(0, center); // RotTrans (8004a6dc)
    gte.mvmva(0, 0, 0);
    state.cull_view.t = {gte.mac(1), gte.mac(2), gte.mac(3)};
    gte.transform.r = state.cull_view.r; // SetRotMatrix and SetTransMatrix
    gte.transform.t = state.cull_view.t;
    const auto radius = s16(memory(instance + 0x20, 2));
    const auto project = [&](std::int16_t corner) {
        gte.set_vector(0, {corner, corner, 0}); // RotTransPers (8004a64c)
        gte.rtps();
        return gte.sxy(2);
    };
    const auto low = project(static_cast<std::int16_t>(-radius));
    const auto high = project(radius);
    const auto margin_y = s32(state.cull_margins[1]);
    const auto margin_x = s32(state.cull_margins[0]);
    const auto low_x = s16(low), low_y = s16(low >> 16U);
    const auto high_x = s16(high), high_y = s16(high >> 16U);
    return !(low_y < margin_y + 0xe0 && -margin_y < high_y && low_x < margin_x + 0x140 &&
             -margin_x < high_x);
}

// Resident 8002c700: each primitive group of a model through the routine its
// type selects for the sort mode.
void Program::draw_model(std::uint32_t model, std::uint32_t packets, std::uint32_t table,
                         std::int32_t mode) {
    auto &r = resident.sprite_models;
    if (r.lod != 0)
        throw MissingDependency({"draw_model", 0x8003101c, {}, {}}, "symbol:model-level-of-detail",
                                false, "The model detail test 8003101c is not recovered");
    auto groups = memory(model + 6, 2);
    r.geometry = memory(model + 0x10);
    r.auxiliary = memory(model + 0x18);
    r.normals = memory(model + 0xc);
    r.vertices = memory(model + 8);
    r.primitive_count += memory(model + 4, 2);
    r.output = packets;
    r.table = table;
    for (; groups != 0; --groups) {
        const auto record = r.geometry;
        // Resident table 8004fe50: per primitive type, the routine for sort
        // modes 0-5, a spare routine, the record stride, a size and the
        // packet size (28 bytes a row).
        const auto entry = 0x8004fe50U + memory(record, 1) * 0x28U;
        if (mode < 0 || mode >= 6)
            throw MissingDependency({"draw_model", 0x8002c7f0, {}, {}},
                                    "symbol:model-primitive-mode", false,
                                    "A primitive sort mode outside 0-5 is not recovered");
        const auto routine = memory(entry + u32(mode) * 4);
        const auto count = s16(memory(record + 2, 2));
        r.geometry = record + 4;
        draw_primitives(routine, record + 4, count);
        r.geometry += u32(count) * memory(entry + 0x1c);
    }
}

// The average-depth (mode 0) and farthest-depth (mode 2) routines for
// triangles and quads. Each primitive owns a packet slot whether drawn or not;
// the cursor keeps only its low 24 bits once a primitive passes the test that
// masks it, as the original's delay slots do.
void Program::draw_primitives(std::uint32_t routine, std::uint32_t record, std::int32_t count) {
    const auto shape = primitive_shape(routine);
    if (!shape)
        throw MissingDependency({"draw_primitives", routine, {}, {}},
                                "symbol:model-primitive-routine", false,
                                "This primitive routine of table 8004fe50 is not recovered");
    if (count < 0)
        throw MissingDependency({"draw_primitives", routine, {}, {}}, "symbol:model-negative-group",
                                false, "A negative primitive count loops in the original");
    auto &r = resident.sprite_models;
    auto &gte = resident.gte;
    auto cursor = r.output - shape->size;
    auto drawn = r.drawn;
    const auto shift = r.depth_shift + (shape->farthest ? 2U : 0U);
    const auto vertex = [&](std::uint32_t index) {
        const auto at = r.vertices + index * 8;
        return field::GteVector{s16(memory(at, 2)), s16(memory(at + 2, 2)), s16(memory(at + 4, 2))};
    };
    // Some corner's row (whole SXY word, so negative rows fail) and some
    // corner's column below the limits.
    const auto rows = [&](const std::array<std::uint32_t, 4> &sxy, std::size_t corners) {
        return std::any_of(sxy.begin(), sxy.begin() + static_cast<std::ptrdiff_t>(corners),
                           [&](std::uint32_t value) { return value < r.y_limit; });
    };
    const auto columns = [&](const std::array<std::uint32_t, 4> &sxy, std::size_t corners) {
        return std::any_of(sxy.begin(), sxy.begin() + static_cast<std::ptrdiff_t>(corners),
                           [&](std::uint32_t value) { return (value & 0xffffU) < r.x_limit; });
    };
    const auto on_screen = [&](const std::array<std::uint32_t, 4> &sxy, std::size_t corners) {
        return rows(sxy, corners) && columns(sxy, corners);
    };
    // The routines test the sign of GTE data register 31 (MFC2 $31, LZCR,
    // 0..32) where the error bit of FLAG (control 31) was probably intended;
    // the test never rejects unless LZCS makes it negative.
    const auto rejected = [&] { return static_cast<std::int32_t>(gte.data(31)) < 0; };
    const auto link = [&](std::uint32_t depth) {
        const auto entry = r.table + (depth >> shift) * 4;
        const auto previous = memory(entry);
        set_memory(entry, cursor);
        set_memory(cursor, previous | shape->tag);
    };
    for (std::int32_t n = 0; n < count; ++n, record += 8) {
        const auto indices = memory(record);
        gte.set_vector(0, vertex(indices & 0xffffU));
        gte.set_vector(1, vertex((indices >> 16U) & 0x1fffU)); // (w >> 13) & fff8
        gte.set_vector(2, vertex(memory(record + 4, 2)));
        gte.rtpt();
        cursor += shape->size;
        std::array<std::uint32_t, 4> sxy{gte.sxy(0), gte.sxy(1), gte.sxy(2), 0};
        if (!shape->quad) {
            if (rejected() || !on_screen(sxy, 3))
                continue;
            gte.nclip();
            const auto facing = gte.mac(0);
            if (!shape->farthest) {
                gte.avsz3();
                cursor &= 0xffffffU;
                if (facing <= 0)
                    continue;
                for (std::uint32_t i = 0; i < 3; ++i)
                    set_memory(cursor + 8 + i * shape->slot, sxy[i]);
                ++drawn;
                if (gte.otz() == 0)
                    continue;
                link(gte.otz());
            } else {
                set_memory(cursor + 8, sxy[0]);
                if (facing <= 0)
                    continue;
                set_memory(cursor + 8 + shape->slot, sxy[1]);
                set_memory(cursor + 8 + 2 * shape->slot, sxy[2]);
                cursor &= 0xffffffU;
                ++drawn;
                const auto depth = std::max({gte.sz(1), gte.sz(2), gte.sz(3)});
                if (depth == 0)
                    continue;
                link(depth);
            }
            continue;
        }
        if (!shape->farthest)
            cursor &= 0xffffffU; // Masked on every primitive that reaches its flag test.
        if (rejected())
            continue;
        gte.nclip();
        if (gte.mac(0) <= 0)
            continue;
        gte.set_vector(0, vertex(memory(record + 6, 2)));
        gte.rtps();
        sxy[3] = gte.sxy(2);
        if (rejected())
            continue;
        if (!shape->farthest)
            gte.avsz4();
        if (!rows(sxy, 4))
            continue;
        if (shape->farthest)
            set_memory(cursor + 8, sxy[0]); // In the delay slot of the column test.
        if (!columns(sxy, 4))
            continue;
        if (!shape->farthest) {
            ++drawn;
            if (gte.otz() == 0)
                continue;
            link(gte.otz());
            for (std::uint32_t i = 0; i < 4; ++i)
                set_memory(cursor + 8 + i * shape->slot, sxy[i]);
            continue;
        }
        for (std::uint32_t i = 1; i < 3; ++i)
            set_memory(cursor + 8 + i * shape->slot, sxy[i]);
        if (gte.sz(0) == 0)
            continue;
        set_memory(cursor + 8 + 3 * shape->slot, sxy[3]);
        if (gte.sz(1) == 0 || gte.sz(2) == 0)
            continue;
        cursor &= 0xffffffU;
        if (gte.sz(3) == 0)
            continue;
        ++drawn;
        link(std::max({gte.sz(0), gte.sz(1), gte.sz(2), gte.sz(3)}));
    }
    r.drawn = drawn;
    r.output = cursor + shape->size;
}

// Field 800748e8.
void Program::frame_models() {
    auto &state = *field;
    auto &gte = resident.gte;
    auto &r = resident.sprite_models;
    auto &c = state.camera;
    const auto &trig = resident.math.trigonometry;
    r.drawn = 0;
    r.primitive_count = 0;
    if (state.sprite_gate != 0) {
        r.fog_color = state.fog_color; // 8002c6e0
        // SetFarColor (8004a10c).
        for (std::uint32_t i = 0; i < 3; ++i)
            gte.set_control(21 + i, u32(state.far_color[i]) << 4U);
        // SetFogNearFar (80048ab0).
        const std::int32_t near = state.fog_range[0], far = state.fog_range[1];
        const auto span = far - near;
        if (span >= 100) {
            if (c.projection == 0)
                throw MissingDependency({"frame_models", 0x80048b44, {}, {}},
                                        "symbol:fog-zero-projection", false,
                                        "SetFogNearFar with a zero projection is not recovered");
            const auto a = ((-near * far) / span) << 8;
            gte.set_control(27, u32(std::clamp(a / c.projection, -0x8000, 0x7fff)));
            gte.set_control(28, u32(((far << 12) / span) << 12));
        }
    }
    field::scale_matrix(state.matrix_afa84, {c.scale, c.scale, c.scale});
    gte.transform.r = c.scaled_world.r;
    const auto view = field::compose_matrix(c.scaled_world, state.sprite_view); // M38
    r.lod = 0;
    state.piece_drift_total[1] += state.piece_drift[2];
    state.piece_drift_total[0] += state.piece_drift[0];
    state.piece_drift_total[2] += state.piece_drift[1];
    const auto drift = [&](std::span<std::uint8_t> d) {
        put(d, 0x20, u32(s32(word(d, 0x20)) + state.piece_drift[0]));
        put(d, 0x24, u32(s32(word(d, 0x24)) + state.piece_drift[2]));
        put(d, 0x28, u32(s32(word(d, 0x28)) + state.piece_drift[1]));
    };
    for (std::size_t i = 0; i < state.descriptor_count; ++i) {
        auto d = descriptor_bytes(i);
        for (std::size_t k = 0; k < 0x20; k += 4)
            put(d, 0x2c + k, word(d, 0xc + k));
        const auto flags = word(d, 0x58, 2);
        if ((flags & 0x40U) != 0)
            continue;
        bool always = false;
        field::GteMatrix placed{}; // M58
        bool general = true;
        if (i < state.actors.size()) {
            const auto &a = state.actors[i].storage;
            const auto axis = word(a, 0x12c) & 3U;
            general = false;
            if (axis != 0) {
                field::GteVector angles{};
                angles[axis - 1] = s16(word(a, 0x70, 2));
                auto turned = field::rotation_matrix(angles, trig);
                const auto local = read_matrix(d, 0xc);
                gte.transform.r = local.r;
                field::multiply_rotation(local, turned);
                turned.t = local.t;
                gte.transform.r = c.scaled_world.r;
                placed = field::compose_matrix(c.scaled_world, turned);
            } else if (word(a, 0x128, 2) != 0xffff) {
                throw MissingDependency({"frame_models", 0x801e72cc, i, {}},
                                        "symbol:model-actor-bone-rotation", false,
                                        "The actor rotation routine 801e72cc is not recovered");
            } else if (a[0x75] != 0xff) {
                throw MissingDependency({"frame_models", 0x80074c84, i, {}},
                                        "symbol:model-attached-actor", false,
                                        "Models attached to another descriptor are not recovered");
            } else {
                general = true;
            }
        } else if ((state.piece_drift_mode & 0x7fU) == 0) {
            drift(d);
        }
        if (general) {
            if ((state.piece_drift_mode & 0x7fU) == 1)
                drift(d);
            always = (state.piece_drift_mode & 0x80U) != 0;
            // M58 = M38 applied to the descriptor's matrix, on the GTE.
            gte.transform.r = view.r;
            gte.transform.t = view.t;
            placed = field::compose_matrix(view, read_matrix(d, 0xc));
            const auto orient = flags & 3U;
            if (orient != 0) {
                if (orient == 1) {
                    // MulMatrix0 (8004920c) into M58's rotation.
                    auto product = read_matrix(d, 0xc);
                    gte.transform.r = state.matrix_afa84.r;
                    field::multiply_rotation(state.matrix_afa84, product);
                    placed.r = product.r;
                    placed.pad = product.pad;
                } else {
                    placed.r = read_matrix(d, 0xc).r; // 8007409c
                    field::scale_matrix(placed, {c.scale, c.scale, c.scale});
                }
                gte.transform.r = c.orbit.r;
                field::multiply_rotation(c.orbit, placed);
            }
        }
        const auto instance = word(d, 0);
        if (instance == 0)
            throw MissingDependency({"frame_models", 0x80074f98, i, {}},
                                    "symbol:model-without-instance", false,
                                    "A drawn descriptor without a model instance reads low memory");
        if (s16(memory(instance + 0x12, 2)) == 1) {
            // Lit models: the light matrix is the light source times the
            // model rotation at half scale (80030b14); 80030c40 sets the
            // background color.
            auto lit = placed;
            field::scale_matrix(lit, {0x800, 0x800, 0x800});
            gte.transform.r = r.light_source.r;
            auto light = lit;
            field::multiply_rotation(r.light_source, light);
            for (std::uint32_t k = 0; k < 4; ++k)
                gte.set_control(8 + k, u32(static_cast<std::uint16_t>(light.r[k * 2])) |
                                           u32(static_cast<std::uint16_t>(light.r[k * 2 + 1]))
                                               << 16U);
            gte.set_control(12, u32(light.r[8]));
            for (std::uint32_t k = 0; k < 3; ++k)
                gte.set_control(13 + k, ((state.back_color[k] & 0xffffU) >> 4U) << 4U);
        }
        r.lod = 0;
        if ((flags & 0x20U) != 0)
            continue;
        if ((flags & 0x2000U) != 0 && memory(instance + 0x14) != 0)
            throw MissingDependency({"frame_models", 0x800305d8, i, {}}, "symbol:model-animation",
                                    false, "Model animation 800305d8 is not recovered");
        gte.transform = placed;
        if (model_culled(instance) && !always)
            continue;
        gte.transform = placed;
        const auto table = state.draw_block + ((flags & 0x8000U) != 0 ? 0x40d0U : 0xccU);
        draw_model(memory(instance + 4), memory(instance + 8 + state.draw_buffer * 4), table,
                   s16(memory(instance + 0x12, 2)));
    }
}

} // namespace xem::reconstruction
