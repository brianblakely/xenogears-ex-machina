// Field overlay 800739c0 (the move phase): the field update, camera follow and
// view matrices, actor facing and sprite orientation. Original addresses name
// correlations only; owned records are Program state.
#include "xem/reconstruction/field_collision.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_motion.hpp"
#include "xem/reconstruction/field_view.hpp"
#include "xem/reconstruction/program.hpp"

#include <bit>
#include <climits>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Move phase read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Move phase write exceeds owned storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t add(std::int32_t a, std::int32_t b) { return s32(u32(a) + u32(b)); }
std::int32_t subtract(std::int32_t a, std::int32_t b) { return s32(u32(a) - u32(b)); }
std::int32_t product(std::int32_t a, std::int32_t b) { return s32(u32(a) * u32(b)); }
// The high halfword of a 16.16 word, as LH of its upper half reads it.
std::int32_t high(std::int32_t value) { return s16(u32(value) >> 16U); }
// DIV without a zero check: the zero-divisor result is not reconstructed.
std::int32_t quotient(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0)
        throw field::FieldFormatError("Original MIPS division by zero is not a recovered result");
    if (numerator == INT32_MIN && denominator == -1)
        return numerator;
    return numerator / denominator;
}
std::uint32_t packed(std::int32_t x, std::int32_t z) { return (u32(x) << 16U) + u32(z); }
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point,
              bool completed) {
    if (observe)
        observe(program, point, completed);
}
void diagnostic(const FieldState &state, std::uint32_t address) {
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_move_diagnostic", address, {}, {}},
                                "symbol:field-diagnostic-output", false,
                                "Unsuppressed move-phase diagnostic output is not recovered");
}
std::int32_t random(ResidentState &resident) {
    const auto step = field::advance_field_random(resident.random_seed);
    resident.random_seed = step.seed;
    return step.value;
}
std::int32_t ratan2(const ResidentState &resident, std::int32_t y, std::int32_t x) {
    return field::collision_atan(y, x, resident.math.angle).angle;
}
std::int32_t planar_length(const ResidentState &resident, std::int32_t x, std::int32_t z) {
    return field::collision_planar_length(x, z, resident.math.square_root);
}
const field::CollisionLayer &top_layer(const FieldState &state, std::size_t &slot) {
    const auto layer = static_cast<std::int32_t>(state.layer_count) - 1;
    if (layer < 0 || static_cast<std::size_t>(layer) >= state.collision.layers.size() ||
        layer >= static_cast<std::int32_t>(state.triangle_counts.size()))
        throw field::FieldFormatError("Camera floor query outside the loaded collision layers");
    slot = static_cast<std::size_t>(layer);
    return state.collision.layers[slot];
}
// Field 8007b1c4 on the top layer.
field::FloorLocation camera_floor(const ResidentState &resident, const FieldState &state,
                                  std::int32_t x, std::int32_t z) {
    std::size_t slot = 0;
    const auto &layer = top_layer(state, slot);
    return field::locate_initial_floor(layer, state.triangle_counts[slot], s16(u32(x)), s16(u32(z)),
                                       resident.math.reciprocal);
}

// Field 8007234c/80072398: blocked octants counted upward or downward from
// start; zero when all eight are blocked.
std::int32_t blocked_run(const FieldState &state, std::uint8_t mask, std::uint32_t start,
                         bool upward) {
    for (std::int32_t count = 0; count < 8; ++count) {
        if ((mask & state.heading_octants[start & 7U]) == 0)
            return count;
        start = upward ? start + 1U : start - 1U;
    }
    return 0;
}

// Field 800726e8: heading changes from blocked octants and shoulder buttons.
void camera_heading(FieldState &state) {
    auto &c = state.camera;
    const auto octant = [&](std::int32_t heading) { return (u32(heading) & 0xfffU) >> 9U; };
    const auto turn = [&](bool left) {
        c.heading_velocity = left ? -0x400000 : 0x400000;
        c.heading = left ? c.heading - 0x200U : c.heading + 0x200U;
        c.heading_steps = 8;
    };
    const auto [block_a, block_b] = c.heading_blocks;
    const auto held = state.control_inputs.held_buttons;
    if (block_a != 0xff && block_b != 0xff) {
        if (c.heading_steps == 0) {
            if ((state.heading_octants[octant(c.heading_half)] & block_a) != 0) {
                if (c.heading_velocity != -0x400000 && c.heading_velocity != 0x400000) {
                    c.heading_velocity = 0x400000;
                    c.heading += 0x200U;
                }
                c.heading_steps = 8;
            }
            const auto current = octant(c.heading_half);
            if ((state.heading_octants[current] & block_b) != 0) {
                const auto up = blocked_run(state, block_b, current, true);
                const auto down = blocked_run(state, block_b, current, false);
                turn(down < up);
            }
        }
        const auto free = [&](std::int32_t offset) {
            return (state.heading_octants[octant(s16(c.heading_half) + offset)] & block_b) == 0;
        };
        if ((held & 4U) != 0 && (c.flags & 0x8000U) == 0 && c.heading_steps == 0 && free(-0x200))
            turn(true);
        if ((held & 8U) != 0 && (c.flags & 0x8000U) == 0 && c.heading_steps == 0 && free(0x200))
            turn(false);
    }
    if (c.heading_steps != 0) {
        c.heading_high += u32(c.heading_velocity);
        c.heading_half = static_cast<std::uint16_t>(c.heading_high >> 16U);
        c.heading_steps = static_cast<std::int16_t>(c.heading_steps - 1);
        if (c.heading_steps != 0) {
            diagnostic(state, 0x80072a1c);
            return;
        }
    }
    c.heading_half = static_cast<std::uint16_t>(c.heading);
    diagnostic(state, 0x80072a1c);
}

// Field 8007cd80: walk the top floor layer from the clamped camera point toward
// the followed point. Returns nullopt inside the walkable mesh; otherwise the
// crossed edge (x, y, z of both ends) and the unclamped and clamped points.
struct CameraEdge {
    std::optional<std::array<std::array<std::int16_t, 3>, 2>> edge;
    std::array<std::int16_t, 4> segment;
};
std::optional<CameraEdge> camera_edge(const ResidentState &resident, const FieldState &state,
                                      std::int32_t x, std::int32_t z) {
    const auto &bounds = state.camera.bounds;
    std::int32_t cx = x, cz = z;
    if (x < bounds[0])
        cx = bounds[0];
    else if (bounds[0] + bounds[2] < x)
        cx = bounds[0] + bounds[2];
    if (bounds[1] < z)
        cz = bounds[1];
    else if (z < bounds[1] + bounds[3])
        cz = bounds[1] + bounds[3];
    std::size_t slot = 0;
    const auto &layer = top_layer(state, slot);
    const auto point = packed(x, z), clamped = packed(cx, cz);
    const auto vertex = [&](std::int32_t index) -> const std::array<std::int16_t, 4> & {
        if (index < 0 || static_cast<std::size_t>(index) >= layer.vertices.size())
            throw field::FieldFormatError("Camera edge walk vertex outside the layer");
        return layer.vertices[static_cast<std::size_t>(index)];
    };
    const auto triangle = [&](std::int32_t index) -> const field::CollisionTriangle & {
        if (index < 0 || static_cast<std::size_t>(index) >= layer.triangles.size())
            throw field::FieldFormatError("Camera edge walk triangle outside the layer");
        return layer.triangles[static_cast<std::size_t>(index)];
    };
    const auto flat = [&](std::int32_t index) {
        const auto &v = vertex(index);
        return packed(v[0], v[2]);
    };
    const auto negative = [](std::uint32_t a, std::uint32_t b, std::uint32_t p) {
        return field::field_packed_area({a, b, p}) < 0;
    };
    const auto walkable = [&](std::int32_t index) {
        const auto attribute = triangle(index).attribute_raw & 0xffU;
        const auto at = attribute * 4U;
        if (at + 4 > state.collision.attributes_raw.size())
            throw field::FieldFormatError("Camera edge walk attribute outside the table");
        return (word(state.collision.attributes_raw, at) & 0x800000U) != 0;
    };
    auto next = static_cast<std::int32_t>(camera_floor(resident, state, cx, cz).triangle);
    std::int32_t current = next, count = 0;
    std::uint32_t code = 0;
    for (;;) {
        current = next;
        const auto &t = triangle(current);
        const auto va = flat(t.vertices[0]), vb = flat(t.vertices[1]), vc = flat(t.vertices[2]);
        code = (negative(va, vb, point) ? 1U : 0U) | (negative(vb, vc, point) ? 2U : 0U) |
               (negative(vc, va, point) ? 4U : 0U);
        const auto adjacent = [&](std::size_t edge) { return s16(t.adjacent_raw[edge]); };
        switch (code) {
        case 0:
            count = 0xff;
            break;
        case 1:
            next = adjacent(0);
            break;
        case 2:
            next = adjacent(1);
            break;
        case 3:
            code = negative(vb, point, clamped) ? 1U : 2U;
            next = adjacent(code == 1 ? 0 : 1);
            break;
        case 4:
            next = adjacent(2);
            break;
        case 5:
            code = negative(va, point, clamped) ? 4U : 1U;
            next = adjacent(code == 4 ? 2 : 0);
            break;
        case 6:
            code = negative(vc, point, clamped) ? 2U : 4U;
            next = adjacent(code == 4 ? 2 : 1);
            break;
        default:
            next = -1;
        }
        // Triangle -1 reads a byte before the table; both outcomes then leave.
        if (next == -1 || !walkable(next))
            break;
        if (++count < 0xf0)
            continue;
        if (count != 0xf0)
            return std::nullopt;
        break;
    }
    CameraEdge result{};
    result.segment = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(z),
                      static_cast<std::int16_t>(cx), static_cast<std::int16_t>(cz)};
    const auto &t = triangle(current);
    const auto point3 = [&](std::int16_t index) {
        const auto &v = vertex(index);
        return std::array<std::int16_t, 3>{v[0], v[1], v[2]};
    };
    if (code == 1)
        result.edge = {{point3(t.vertices[0]), point3(t.vertices[1])}};
    else if (code == 2)
        result.edge = {{point3(t.vertices[1]), point3(t.vertices[2])}};
    else if (code == 4)
        result.edge = {{point3(t.vertices[2]), point3(t.vertices[0])}};
    return result;
}

// Field 800723e4: where the camera segment meets the crossed edge (X/Z).
std::array<std::int16_t, 2> edge_intersection(const ResidentState &resident,
                                              const std::array<std::int16_t, 4> &line,
                                              const std::array<std::int16_t, 4> &segment) {
    const auto a = field::normalize_field_vector({line[2] - line[0], 0, line[3] - line[1]},
                                                 resident.math.reciprocal);
    const auto b = field::normalize_field_vector(
        {segment[2] - segment[0], 0, segment[3] - segment[1]}, resident.math.reciprocal);
    const auto denominator = subtract(product(b[0], a[2]), product(b[2], a[0])) >> 12;
    std::int32_t k = 0;
    if (denominator != 0)
        k = quotient(
            subtract(product(segment[1] - line[1], a[0]), product(segment[0] - line[0], a[2])),
            denominator);
    return {static_cast<std::int16_t>(u32(segment[0]) + u32(product(k, b[0]) >> 12)),
            static_cast<std::int16_t>(u32(segment[1]) + u32(product(k, b[2]) >> 12))};
}

// Field 80073684: rotate the eye goal about the target goal by the heading.
void orbit_eye(ResidentState &resident, field::FieldCamera &c) {
    field::push_matrix(resident.matrix_stack, resident.gte.transform);
    const auto rotation = field::rotation_matrix(
        {c.heading_x, static_cast<std::int16_t>(c.heading_half), c.heading_z},
        resident.math.trigonometry);
    field::GteLong offset{};
    for (std::size_t i = 0; i < 3; ++i)
        offset[i] = subtract(c.target_goal[i], c.eye_goal[i]);
    const auto moved = field::apply_matrix_lv(rotation, offset);
    c.eye_goal[0] = add(moved[0], c.target_goal[0]);
    c.eye_goal[2] = add(moved[2], c.target_goal[2]);
    resident.gte.transform = field::pop_matrix(resident.matrix_stack);
}

// Field 80072a38: eye and target goals around the followed point.
void camera_place(ResidentState &resident, FieldState &state, const field::GteLong &position,
                  std::int32_t floor) {
    auto &c = state.camera;
    const auto walk = camera_edge(resident, state, high(position[0]), high(position[2]));
    if (walk) {
        if (!walk->edge)
            throw MissingDependency({"field_camera_edge", 0x8007cd80, {}, {}},
                                    "symbol:camera-edge-uninitialized-stack", false,
                                    "Camera edge walk left without an edge; the original "
                                    "then reads uninitialized stack");
        const auto &[first, second] = *walk->edge;
        const auto hit =
            edge_intersection(resident, {first[0], first[2], second[0], second[2]}, walk->segment);
        c.target_goal[0] = s32(u32(hit[0]) << 16U);
        c.target_goal[2] = s32(u32(hit[1]) << 16U);
        if (state.camera_floor_fixed != 0) {
            c.target_goal[1] = add(position[1], -0x200000);
        } else if (state.camera_floor_latched == 0) {
            c.target_goal[1] = s32(u32(floor) << 16U);
            state.camera_floor_latched = 1;
        }
    } else {
        c.target_goal[0] = position[0];
        state.camera_floor_latched = 0;
        c.target_goal[1] = add(position[1], -0x200000);
        c.target_goal[2] = position[2];
    }
    const auto angle = u32((s16(c.elevation) * 0x5b >> 3) + 0xc00);
    const auto trig =
        field::planar_trigonometry(resident.math.trigonometry, static_cast<std::uint16_t>(angle));
    const auto height = s32(0U - (u32(product(trig.cosine, c.projection)) << 5U)) >> 16;
    c.eye_goal[1] = add(s32(u32(product(height, c.distance)) << 4U), c.target_goal[1]);
    const auto depth = s32(u32(product(trig.sine, c.projection)) << 5U) >> 16;
    c.eye_goal[2] = add(s32(u32(product(depth, c.distance)) << 4U), c.target_goal[2]);
    c.eye_goal[0] = c.target_goal[0];
    orbit_eye(resident, c);
    if ((c.flags & 1U) != 0) {
        if (c.steps != 0) {
            c.start = add(c.start, c.step);
            c.distance = static_cast<std::int16_t>(c.start >> 16);
        }
        c.steps = static_cast<std::uint16_t>(c.steps - 1U);
        if (c.steps == 0)
            c.flags &= 0xfffeU;
    }
    if ((c.flags & 8U) != 0) {
        c.elevation_value = add(c.elevation_value, c.elevation_step);
        c.elevation = static_cast<std::uint16_t>(c.elevation_value >> 16);
        c.elevation_steps = static_cast<std::uint16_t>(c.elevation_steps - 1U);
        if (c.elevation_steps == 0)
            c.flags &= 0xfff7U;
    }
}

// Field 80072d74: projection interpolation, eye/target follow and shake.
void camera_follow(ResidentState &resident, FieldState &state) {
    auto &c = state.camera;
    if ((c.flags & 0x10U) != 0) {
        if (c.projection_steps != 0) {
            c.projection_value = add(c.projection_value, c.projection_step);
            c.projection = c.projection_value >> 16;
        }
        c.projection_steps = static_cast<std::int16_t>(c.projection_steps - 1);
        if (c.projection_steps < 0) {
            c.flags &= 0xffefU;
            c.projection_steps = 0;
        }
    }
    if (c.counter != 0) {
        c.target_a = 1;
        c.target_b = 1;
        --c.counter;
    }
    const auto follow = [](std::int32_t &current, std::int32_t goal, std::int32_t divisor,
                           std::int32_t threshold) {
        if (high(current) == high(goal))
            return;
        const auto difference = subtract(goal, current);
        const auto part = difference >> 16;
        if (product(part, part) < threshold)
            return;
        current = add(current, quotient(difference, divisor));
    };
    const auto target_threshold = product(c.target_a, c.target_a);
    const auto eye_threshold = product(c.target_b, c.target_b);
    for (const std::size_t axis : {0U, 2U, 1U})
        follow(c.eye[axis], c.eye_goal[axis], c.target_b, eye_threshold);
    for (const std::size_t axis : {0U, 2U, 1U})
        follow(c.target[axis], c.target_goal[axis], c.target_a, target_threshold);
    c.shake_offset = {};
    if (c.shake == 0)
        return;
    if (c.shake_time != 0) {
        for (std::size_t i = 0; i < 3; ++i)
            c.shake_amplitude[i] = add(c.shake_amplitude[i], c.shake_step[i]);
    } else if (c.shake_stop != 0) {
        c.shake_amplitude = {};
        c.shake = 0;
        c.shake_stop = 0;
    }
    for (std::size_t i = 0; i < 3; ++i)
        c.shake_offset[i] = product(random(resident), high(c.shake_amplitude[i]));
    for (std::size_t i = 0; i < 3; ++i)
        if (c.shake_offset[i] < 0) {
            c.shake_offset[i] = 0;
            c.shake_amplitude[i] = 0;
        }
    if (c.shake_time > 0)
        --c.shake_time;
}

// Field 80072150: view composition, world matrices and the scaled world
// matrix left loaded for drawing.
void view_setup(ResidentState &resident, FieldState &state) {
    auto &c = state.camera;
    const auto &trig = resident.math.trigonometry;
    c.orbit = field::rotation_matrix(c.orbit_angles, trig, c.orbit);
    c.orbit.t = {};
    const auto composed = field::compose_matrix(c.orbit, c.previous_view);
    c.previous_view.r = composed.r;
    c.previous_view.t = composed.t;
    state.world_matrix = field::rotation_matrix(c.world_angles, trig, state.world_matrix);
    state.world_matrix.t = {};
    c.scaled_world = field::rotation_matrix(c.world_angles, trig, c.scaled_world);
    field::multiply_rotation(c.previous_view, c.scaled_world);
    resident.gte.transform.r = c.previous_view.r;
    resident.gte.transform.t = c.previous_view.t;
    c.scaled_world.t = field::rot_trans(resident.gte.transform, c.anchor);
    field::scale_matrix(c.scaled_world, {c.scale, c.scale, c.scale});
    resident.gte.transform.r = c.scaled_world.r;
    resident.gte.transform.t = c.scaled_world.t;
}
} // namespace

// Field 80073230.
void Program::camera_update() {
    auto &state = *field;
    auto &c = state.camera;
    if (c.mode == 1) {
        state.camera_settle = 0;
        state.camera_release = 0;
        if ((c.scripted & 1U) != 0) {
            if (c.target_steps != 0)
                for (std::size_t i = 0; i < 3; ++i)
                    c.scripted_target[i] = add(c.scripted_target[i], c.target_step[i]);
            c.target_steps = static_cast<std::int16_t>(c.target_steps - 1);
            if (c.target_steps == 0)
                c.scripted &= 0xfffeU;
            c.target_goal = c.scripted_target;
        }
        if ((c.scripted & 2U) != 0) {
            if (c.eye_steps != 0)
                for (std::size_t i = 0; i < 3; ++i)
                    c.scripted_eye[i] = add(c.scripted_eye[i], c.eye_step[i]);
            c.eye_steps = static_cast<std::int16_t>(c.eye_steps - 1);
            if (c.eye_steps == 0)
                c.scripted &= 0xfffdU;
            c.eye_goal = c.scripted_eye;
        }
    } else if (c.mode == 0 || c.mode == 2) {
        if (c.mode == 0) {
            state.camera_release = 0;
            if ((state.camera_settle & 3) == 0) {
                c.target_a = c.target_a < 9 ? 8 : c.target_a - 2;
                c.target_b = c.target_b < 9 ? 8 : c.target_b - 2;
            }
            ++state.camera_settle;
        } else {
            state.camera_settle = 0;
            if (++state.camera_release >= 0x41)
                c.mode = 0;
        }
        camera_heading(state);
        const auto &followed = state.actors.at(state.followed_actor).storage;
        const field::GteLong position{s32(word(followed, 0x20)), s32(word(followed, 0x24)),
                                      s32(word(followed, 0x28))};
        camera_place(resident, state, position, s16(word(followed, 0x72, 2)));
        if ((c.flags & 0x4000U) == 0) {
            const auto floor =
                camera_floor(resident, state, high(c.eye_goal[0]), high(c.eye_goal[2]));
            const auto y = s16(u32(floor.point[1]));
            if (y < high(c.eye_goal[1]))
                c.eye_goal[1] = s32(u32(y) << 16U);
        }
        if (c.mode == 2) {
            const auto target = planar_length(resident, high(c.target_goal[0]) - high(c.target[0]),
                                              high(c.target_goal[2]) - high(c.target[2]));
            const auto eye = planar_length(resident, high(c.eye_goal[0]) - high(c.eye[0]),
                                           high(c.eye_goal[2]) - high(c.eye[2]));
            if (target < 0x80 && eye < 0x80)
                c.mode = 0;
        }
    }
    camera_follow(resident, state);
    c.heading_half &= 0xfffU;
}

// Field 800739c0.
void Program::field_move(const ProgramObserver &observe) {
    field_update(observe);
    auto &state = *field;
    auto &c = state.camera;
    const auto &trig = resident.math.trigonometry;
    state.sprite_view = field::rotation_matrix(state.sprite_view_angles, trig, state.sprite_view);
    state.sprite_view.t = {};
    c.view_angle = static_cast<std::int16_t>(
        ratan2(resident, subtract(c.target[2], c.eye[2]), subtract(c.target[0], c.eye[0])) - 0x400);
    state.control_inputs.camera_angle =
        static_cast<std::int16_t>(ratan2(resident, subtract(c.target_goal[2], c.eye_goal[2]),
                                         subtract(c.target_goal[0], c.eye_goal[0])) -
                                  0x400);
    const auto length = planar_length(resident, subtract(c.target[0], c.eye[0]) >> 16,
                                      subtract(c.target[2], c.eye[2]) >> 16);
    state.elevation_angle = ratan2(resident, length, subtract(c.target[1], c.eye[1]) >> 16);
    camera_update();
    observed(observe, *this, {"field_move_camera", 0x80073aec, {}, {}}, true);
    field::GteLong eye{}, target{};
    for (std::size_t i = 0; i < 3; ++i) {
        eye[i] = add(c.eye[i], c.shake_offset[i]);
        target[i] = add(c.target[i], c.shake_offset[i]);
    }
    if (state.camera_cut == 0) {
        c.previous_view = c.view;
        field::build_view(resident.gte, resident.math.reciprocal, c.view, eye, target, c.up);
    } else {
        field::build_view(resident.gte, resident.math.reciprocal, c.previous_view, eye, target,
                          c.up);
        c.view = c.previous_view;
    }
    view_setup(resident, state); // 800722f4 reloads the same matrix.
    diagnostic(state, 0x80072330);
    observed(observe, *this, {"field_move_view", 0x80073c64, {}, {}}, true);
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &actor = state.actors[i];
        const auto flags = word(actor.descriptor, 0x58, 2);
        if ((flags & 0xf40U) == 0 || (flags & 0x20U) != 0)
            continue;
        auto &a = actor.storage;
        if ((word(a, 4) & 0x100000U) != 0 || (word(a, 4) & 0x600U) == 0x200)
            continue;
        if ((word(a, 0) & 0x8000U) == 0) {
            std::int32_t target_angle = 0, speed = 0x200;
            if ((word(a, 0x14) & 0x200000U) != 0 && (word(a, 0) & 0x1800U) == 0) {
                target_angle = s32((((word(a, 0x14) >> 11U) - 2U) & 7U) << 9U);
            } else {
                speed =
                    (word(a, 4) & 0x2000U) != 0 ? state.party_turn_speed : s16(word(a, 0x11e, 2));
                target_angle = s16(word(a, 0x106, 2));
            }
            // Field 80073988.
            const auto facing = state.camera_cut != 0 ? u32(target_angle) & 0xfffU
                                                      : field::turn_toward(s16(word(a, 0x108, 2)),
                                                                           target_angle, speed);
            put(a, 0x108, facing, 2);
        }
        if (state.orientation_hold != 0)
            continue;
        if ((word(a, 4) & 0x1000000U) == 0) {
            const auto angle = static_cast<std::int16_t>(static_cast<std::uint16_t>(c.view_angle) +
                                                         word(a, 0x108, 2));
            sprite_call(i, [&](field::SpriteWindow sprite, const field::SpriteSources &sources) {
                field::select_sprite_orientation(sprite, angle, resident.sprite, sources);
            });
        } else {
            // Resident 80021fe0: store the angle, then 80022974 rebuilds velocity.
            auto &bytes = actor.sprite.sprite.bytes;
            if (bytes.size() < 0x34)
                throw field::FieldFormatError("Sprite velocity requires the descriptor sprite");
            put(bytes, 0x32, word(a, 0x108, 2), 2);
            static_cast<void>(field::rebuild_sprite_velocity(
                {actor.sprite.sprite.address, actor.sprite.sprite.bytes}, trig));
        }
    }
    diagnostic(state, 0x80073e14);
    observed(observe, *this, {"field_move", 0x800739c0, {}, {}}, true);
}

} // namespace xem::reconstruction
