#include "xem/reconstruction/field_collision.hpp"

#include "xem/reconstruction/field_motion.hpp"

#include <bit>
#include <limits>

namespace xem::reconstruction::field {
namespace {
std::int32_t s32(std::uint32_t value) noexcept { return std::bit_cast<std::int32_t>(value); }
std::int16_t s16(std::uint32_t value) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t u32(std::int32_t value) noexcept { return static_cast<std::uint32_t>(value); }
std::uint32_t read(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    if (at > bytes.size() || width > bytes.size() - at)
        throw CollisionError("Original collision read exceeds supplied storage");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
void store(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
           std::size_t width = 4) {
    if (at > bytes.size() || width > bytes.size() - at)
        throw CollisionError("Original collision write exceeds supplied storage");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
FieldVector vector_at(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width) {
    FieldVector value{};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto word = read(bytes, at + width * i, width);
        value[i] = width == 2 ? s16(word) : s32(word);
    }
    return value;
}
void store_vector(std::span<std::uint8_t> bytes, std::size_t at, const FieldVector &value,
                  std::size_t width) {
    for (std::size_t i = 0; i < 3; ++i)
        store(bytes, at + width * i, u32(value[i]), width);
}
} // namespace

std::uint32_t pack_collision_xz(std::int32_t x, std::int32_t z) noexcept {
    return (u32(x) << 16U) + u32(z);
}

CollisionResult query_field_collision(std::span<const std::uint8_t> component,
                                      std::span<const std::int16_t> reciprocal,
                                      const CollisionActor &actor, const FieldVector &candidate,
                                      bool ordinary, std::int32_t mode,
                                      std::uint8_t attribute_control, CollisionTrace *trace) {
    if (trace)
        *trace = {};
    const auto layer = actor.layer;
    if (layer < 0 || layer >= 4 || static_cast<std::uint32_t>(layer) >= read(component, 0))
        throw CollisionError("Unresolved original collision layer access");
    const auto triangle_base = read(component, 0x18U + 8U * static_cast<std::size_t>(layer));
    const auto vertex_base = read(component, 0x1cU + 8U * static_cast<std::size_t>(layer));
    const auto attribute_base = read(component, 0x14);
    CollisionResult result;
    std::int32_t triangle = actor.triangle;
    const auto finish = [&](std::int32_t value, std::string_view reason) {
        result.value = value;
        result.terminal_triangle = triangle;
        if (trace)
            trace->reason = reason;
        return result;
    };
    if (triangle == -1)
        return finish(-1, "initial-triangle-missing");
    // A whole-component range check alone permits reads into adjacent layers.
    // This parser validates each layer's own triangle and vertex bounds.
    const auto package = parse_collision_package(component);
    const auto &mesh = package.layers[static_cast<std::size_t>(layer)];
    const auto check_triangle = [&](std::int32_t index) {
        if (index < 0 || static_cast<std::size_t>(index) >= mesh.triangles.size())
            throw CollisionError("Original triangle index exceeds selected layer");
    };
    const auto vertices = [&](std::int32_t index) {
        check_triangle(index);
        std::array<FieldVector, 3> values{};
        const auto at =
            static_cast<std::size_t>(triangle_base) + static_cast<std::size_t>(index) * 14U;
        for (std::size_t i = 0; i < 3; ++i) {
            const auto vertex = s16(read(component, at + 2U * i, 2));
            if (vertex < 0 || static_cast<std::size_t>(vertex) >= mesh.vertices.size())
                throw CollisionError("Original vertex index exceeds selected layer");
            values[i] = vector_at(
                component,
                static_cast<std::size_t>(vertex_base) + static_cast<std::size_t>(vertex) * 8U, 2);
        }
        return values;
    };
    check_triangle(triangle);
    const auto x = s32(u32(actor.position[0]) + u32(candidate[0])) >> 16;
    const auto z = s32(u32(actor.position[2]) + u32(candidate[2])) >> 16;
    result.point = FieldVector{s16(u32(x)), 0, s16(u32(z))};
    const auto target = pack_collision_xz(x, z);
    const auto origin = pack_collision_xz(actor.position[0] >> 16, actor.position[2] >> 16);
    const auto attribute_mask =
        ((actor.layer_flags >> ((static_cast<std::uint32_t>(layer) + 3U) & 31U)) & 1U) != 0 ||
                attribute_control != 0
            ? 0U
            : 0xffffffffU;
    if (trace)
        trace->attribute_mask = attribute_mask;
    const auto terrain = [&](std::int32_t index) {
        if (index != -1)
            check_triangle(index);
        const auto offset = static_cast<std::int64_t>(triangle_base) + index * 14 + 12;
        if (offset < 0)
            throw CollisionError("Original neighbor attribute precedes supplied component");
        const auto identifier =
            static_cast<std::uint8_t>(read(component, static_cast<std::size_t>(offset), 1));
        const auto source =
            read(component, static_cast<std::size_t>(attribute_base) + identifier * 4U);
        const auto effective = source & attribute_mask;
        if (trace)
            trace->attribute_reads.push_back({index, identifier, source, effective});
        return effective;
    };
    const auto area = [&](std::array<std::uint32_t, 3> points, std::uint32_t site) {
        const auto value = field_packed_area(points);
        if (trace)
            trace->areas.push_back({site, points, value});
        return value;
    };
    const auto height = [&](std::int32_t index, std::string_view site) {
        const auto v = vertices(index);
        const auto [y, normal] =
            field_height_and_normal(v, (*result.point)[0], (*result.point)[2], reciprocal);
        (*result.point)[1] = y;
        if (trace)
            trace->heights.push_back({site, index, v, *result.point, normal});
        return y;
    };
    bool initial_special = false;
    if (ordinary) {
        initial_special = (terrain(triangle) & 0x400000U) != 0 || mode == 0x80;
        if (trace)
            trace->initial_special = initial_special;
    }
    std::int32_t counter = 0, prior_triangle = triangle;
    std::uint32_t mask = 0;
    std::string_view reason;
    for (;;) {
        prior_triangle = triangle;
        const auto prior_counter = counter;
        const auto v = vertices(triangle);
        std::array<std::uint32_t, 3> packed{};
        for (std::size_t i = 0; i < 3; ++i)
            packed[i] = pack_collision_xz(v[i][0], v[i][2]);
        mask = 0;
        for (std::uint32_t i = 0; i < 3; ++i)
            if (area({packed[i], packed[(i + 1) % 3], target}, i) < 0)
                mask |= 1U << i;
        const auto original_mask = mask;
        if (mask == 0) {
            counter = 255;
        } else if (mask == 7) {
            triangle = -1;
        } else {
            // These arms are absent from the generic decompiler output; their
            // complete original dispatch windows are qualified in EVID-REF-024.
            if (mask == 3 || mask == 5 || mask == 6) {
                const auto vertex = mask == 3 ? 1U : mask == 5 ? 0U : 2U;
                const auto site = mask == 3 ? 3U : mask == 5 ? 4U : 5U;
                const auto negative = area({packed[vertex], target, origin}, site) < 0;
                mask = mask == 3   ? (negative ? 1U : 2U)
                       : mask == 5 ? (negative ? 4U : 1U)
                                   : (negative ? 2U : 4U);
            }
            const auto offset = mask == 1 ? 6U : mask == 2 ? 8U : 10U;
            triangle = s16(read(component,
                                static_cast<std::size_t>(triangle_base) +
                                    static_cast<std::size_t>(prior_triangle) * 14U + offset,
                                2));
        }
        // Deliberately precedes the -1 test, including the source byte before
        // this layer's triangle table. A fabricated zero changes terrain gates.
        const auto value = terrain(triangle);
        if (ordinary)
            result.attribute = value;
        if (trace)
            trace->steps.push_back({prior_triangle, prior_counter, original_mask, target, origin, v,
                                    triangle, mask, value});
        if ((!ordinary && (((actor.flags >> 9U) & 3U) & (value >> 3U)) != 0) ||
            (((actor.flags >> 8U) & 7U) & (value >> 5U)) != 0) {
            triangle = -1;
            reason = "actor-terrain-flags";
            break;
        }
        if ((value & 0x800000U) != 0 && layer == 0) {
            triangle = -1;
            reason = "layer-zero-terrain";
            break;
        }
        if (ordinary && (value & 0x400000U) != 0 && !initial_special &&
            height(triangle, "terrain") < s16(u32(actor.position[1] >> 16))) {
            triangle = -1;
            reason = "terrain-floor-height";
            break;
        }
        if (triangle == -1) {
            reason = "missing-neighbor";
            break;
        }
        ++counter;
        if (counter >= 32) {
            reason = counter == 32 ? "iteration-limit" : "inside";
            break;
        }
    }
    if (trace) {
        trace->counter = counter;
        trace->mask = mask;
    }
    if (triangle != -1 && counter != 32) {
        if (mode != -1)
            height(triangle, "final");
        return finish(0, mode == -1 ? "probe" : "height");
    }
    if (mask == 1 || mask == 2 || mask == 4) {
        const auto v = vertices(prior_triangle);
        const auto first = mask == 1 ? 0U : mask == 2 ? 1U : 2U;
        result.edge = CollisionEdge{v[first], v[(first + 1) % 3]};
    }
    return finish(-1, reason);
}

CollisionAngle collision_atan(std::int32_t z, std::int32_t x, std::span<const std::int16_t> table) {
    if (table.size() != 1025)
        throw CollisionError("Original atan table requires 1025 signed halfwords");
    const auto negative_z = z < 0, negative_x = x < 0;
    if (negative_z)
        z = s32(0U - u32(z));
    if (negative_x)
        x = s32(0U - u32(x));
    if (z == 0 && x == 0)
        return {0, std::nullopt, "zero"};
    const auto z_smaller = z < x;
    const auto small = z_smaller ? z : x, large = z_smaller ? x : z;
    const auto reduced = (u32(small) & 0x7fe00000U) != 0;
    const auto numerator = reduced ? small : s32(u32(small) << 10U);
    const auto denominator = reduced ? large >> 10 : large;
    if (denominator == 0)
        throw CollisionError("Unresolved original atan BREAK 7");
    if (numerator == std::numeric_limits<std::int32_t>::min() && denominator == -1)
        throw CollisionError("Unresolved original atan BREAK 6");
    const auto index = numerator / denominator;
    if (index < 0 || static_cast<std::size_t>(index) >= table.size())
        throw CollisionError("Original atan lookup exceeds supplied table");
    std::int32_t angle = table[static_cast<std::size_t>(index)];
    if (!z_smaller)
        angle = s32(1024U - u32(angle));
    if (negative_x)
        angle = s32(2048U - u32(angle));
    if (negative_z)
        angle = s32(0U - u32(angle));
    return {angle, index,
            z_smaller ? (reduced ? "z-smaller-reduced" : "z-smaller-shifted")
                      : (reduced ? "x-smaller-reduced" : "x-smaller-shifted")};
}
CollisionSquareRoot collision_sqrt(std::int32_t magnitude, std::span<const std::int16_t> table) {
    if (table.size() != 192)
        throw CollisionError("Original square-root table requires 192 signed halfwords");
    if (magnitude < 0)
        throw CollisionError("Unqualified negative original square-root input");
    if (magnitude == 0)
        return {0, 32, {}, {}, {}};
    const auto word = u32(magnitude);
    const auto leading = static_cast<std::uint32_t>(std::countl_zero(word));
    const auto even_leading = leading & ~1U;
    const auto scale = (31U - even_leading) >> 1U;
    const auto normalized =
        even_leading >= 24 ? word << (even_leading - 24U) : word >> (24U - even_leading);
    const auto index = normalized - 64U;
    if (index >= table.size())
        throw CollisionError("Original square-root lookup exceeds supplied table");
    const auto shifted = u32(table[index]) << (scale & 31U);
    return {static_cast<std::int32_t>(shifted >> 12U), leading, scale, index, shifted};
}
std::int32_t collision_planar_length(std::int32_t x, std::int32_t z,
                                     std::span<const std::int16_t> table) {
    const auto a = s16(u32(x)), b = s16(u32(z));
    return collision_sqrt(s32(u32(a * a) + u32(b * b)), table).value;
}
CollisionProjection project_collision_edge(std::int16_t direction, const CollisionEdge &edge,
                                           const FieldVector &velocity,
                                           const CollisionTables &tables) {
    auto points = edge;
    for (auto &point : points)
        for (auto &component : point)
            component = s16(u32(component));
    const auto &a = points[0], &b = points[1];
    auto angle = (0U - u32(collision_atan(b[2] - a[2], b[0] - a[0], tables.angle).angle)) & 0xfffU;
    const auto relative = (0xc00U - u32(direction) + angle) & 0xfffU;
    if (relative - 0x80U >= 0xf01U)
        return {static_cast<std::int32_t>(angle), {}, "stop", {}, {}, {}};
    FieldVector delta{};
    if (relative < 0x800) {
        delta = {a[0] - b[0], 0, a[2] - b[2]};
        angle = (angle + 0x800U) & 0xfffU;
    } else {
        delta = {b[0] - a[0], 0, b[2] - a[2]};
    }
    const auto normal = normalize_field_vector(delta, tables.reciprocal);
    const auto speed =
        collision_planar_length(velocity[0] >> 12, velocity[2] >> 12, tables.square_root);
    const FieldVector result{s32(u32(normal[0]) * u32(speed)), 0, s32(u32(normal[2]) * u32(speed))};
    return {static_cast<std::int32_t>(angle), result, "slide", delta, normal, speed};
}
CollisionSlope project_collision_slope(const FieldVector &velocity, std::int32_t actor_y,
                                       std::int16_t floor, const CollisionTables &tables) {
    const FieldVector delta{s32(0U - u32(velocity[0])) >> 8,
                            s32((u32(floor) << 16U) - u32(actor_y)) >> 8,
                            s32(0U - u32(velocity[2])) >> 8};
    const auto normal = normalize_field_vector(delta, tables.reciprocal);
    const std::array<std::int32_t, 2> input{velocity[0] >> 8, velocity[2] >> 8};
    const auto speed = collision_planar_length(input[0], input[1], tables.square_root);
    const FieldVector result{s32(0U - u32(speed) * u32(normal[0])) >> 4,
                             s32(u32(speed) * u32(normal[1])) >> 4,
                             s32(0U - u32(speed) * u32(normal[2])) >> 4};
    return {result, delta, normal, input, speed};
}
bool uses_ordinary_collision_sweep(const SweepActor &actor, std::uint32_t collision_mode) noexcept {
    return (actor.query.flags & 0x41800U) == 0 && actor.linked_actor == 255 && collision_mode == 0;
}
CollisionSweepResult
sweep_field_collision(std::span<const std::uint8_t> component, const CollisionTables &tables,
                      SweepActor actor, const FieldVector &velocity, CollisionEdge edge,
                      std::int16_t direction, bool ordinary, std::uint32_t collision_mode,
                      std::uint8_t attribute_control, CollisionSweepTrace *trace) {
    if (trace)
        *trace = {};
    const auto query = [&](const FieldVector &candidate, std::int32_t mode,
                           std::string_view stage) {
        CollisionTrace detail;
        auto result =
            query_field_collision(component, tables.reciprocal, actor.query, candidate, ordinary,
                                  mode, attribute_control, trace ? &detail : nullptr);
        if (result.edge)
            edge = *result.edge;
        if (trace)
            trace->queries.push_back({stage, candidate, mode, result, std::move(detail)});
        return result;
    };
    const std::array<std::int32_t, 3> probes =
        ordinary ? std::array{-0x100, 0x100, 0} : std::array{0, -0x100, 0x100};
    auto working = velocity;
    for (const auto offset : probes) {
        const auto trig = planar_trigonometry(tables.trigonometry, u32(direction) + u32(offset));
        const FieldVector candidate{s32(u32(velocity[0]) + (u32(trig.cosine) << 6U)), 0,
                                    s32(u32(velocity[2]) - (u32(trig.sine) << 6U))};
        const auto result = query(candidate, -1,
                                  offset < 0   ? "probe-left"
                                  : offset > 0 ? "probe-right"
                                               : "probe-center");
        if (result.value == -1) {
            const auto projection = project_collision_edge(direction, edge, velocity, tables);
            working = projection.velocity;
            if (trace)
                trace->projection = projection;
            break;
        }
    }
    auto result = query(working, 0, "floor");
    if (result.value == -1)
        return {-1, actor, velocity, edge};
    auto floor = static_cast<std::int16_t>((*result.point)[1]);
    const auto actor_y = actor.query.position[1];
    if (ordinary) {
        const auto integer_y = s16(u32(actor_y >> 16));
        const auto terrain = *result.attribute;
        std::string_view reason;
        if (floor < integer_y)
            reason = "upward-floor";
        else if ((terrain & 0x200000U) != 0)
            reason = "terrain-200000";
        else if ((terrain & 0x420000U) != 0) {
            if ((actor.terrain_flags & 0x420000U) != 0)
                reason = "existing-terrain-420000";
        } else if (floor < integer_y + 64) {
            reason = "within-64-height";
        }
        if (!reason.empty()) {
            const auto slope = project_collision_slope(working, actor_y, floor, tables);
            working = slope.velocity;
            if (trace) {
                trace->slope = slope;
                trace->slope_reason = reason;
            }
            result = query(working, 0, "slope-floor");
            if (result.value == -1)
                return {-1, actor, velocity, edge};
            floor = static_cast<std::int16_t>((*result.point)[1]);
            actor.query.flags |= 0x04000000U;
        }
    } else if ((actor.query.flags & 0x40000U) != 0) {
        floor = actor.forced_floor;
    } else if (s32(u32(floor) << 16U) < actor_y && collision_mode == 0) {
        return {-1, actor, velocity, edge};
    }
    working[1] = s32((u32(floor) << 16U) - u32(actor_y));
    actor.floor = s16(u32(s32(u32(actor_y) + u32(working[1])) >> 16));
    return {0, actor, working, edge};
}
std::int32_t apply_field_collision_sweep(
    std::span<std::uint8_t> bytes, std::span<std::uint8_t> velocity, std::span<std::uint8_t> edge,
    std::int16_t direction, std::span<const std::uint8_t> component, const CollisionTables &tables,
    std::uint32_t collision_mode, std::uint8_t attribute_control, CollisionSweepTrace *trace) {
    if (bytes.size() != 312 || velocity.size() != 12 || edge.size() != 16)
        throw CollisionError("Incomplete original sweep storage");
    const auto layer = s16(read(bytes, 0x10, 2));
    if (layer < 0 || layer >= 4)
        throw CollisionError("Original sweep triangle slot outside actor");
    SweepActor actor{{read(bytes, 0), read(bytes, 4), layer,
                      s16(read(bytes, 8U + 2U * static_cast<std::size_t>(layer), 2)),
                      vector_at(bytes, 0x20, 4)},
                     read(bytes, 0x14),
                     bytes[0x74],
                     s16(read(bytes, 0x72, 2)),
                     s16(read(bytes, 0xec, 2))};
    const auto result =
        sweep_field_collision(component, tables, actor, vector_at(velocity, 0, 4),
                              {vector_at(edge, 0, 2), vector_at(edge, 8, 2)}, direction,
                              uses_ordinary_collision_sweep(actor, collision_mode), collision_mode,
                              attribute_control, trace);
    store_vector(edge, 0, result.edge[0], 2);
    store_vector(edge, 8, result.edge[1], 2);
    if (result.value == 0) {
        store(bytes, 0, result.actor.query.flags);
        store(bytes, 0x72, u32(result.actor.floor), 2);
        store_vector(velocity, 0, result.velocity, 4);
    }
    return result.value;
}

} // namespace xem::reconstruction::field
