#include "xem/reconstruction/field_actor.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace xem::reconstruction::field {
namespace {
std::int32_t s32(std::uint32_t value) noexcept { return std::bit_cast<std::int32_t>(value); }
std::int32_t s16(std::uint32_t value) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t read(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    if (at > bytes.size() || width > bytes.size() - at)
        throw ActorInitializationError("Original actor read exceeds supplied storage");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
void store(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
           std::size_t width = 4) {
    if (at > bytes.size() || width > bytes.size() - at)
        throw ActorInitializationError("Original actor write exceeds supplied storage");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t packed_coordinate(std::int32_t x, std::int32_t z) {
    // ADDU, not bitwise concatenation: negative Z borrows from the high half.
    return (static_cast<std::uint32_t>(x) << 16U) + static_cast<std::uint32_t>(z);
}
} // namespace

std::pair<std::int32_t, FieldVector>
field_height_and_normal(const std::array<FieldVector, 3> &source, std::int32_t x, std::int32_t z,
                        std::span<const std::int16_t> reciprocal) {
    auto v = source;
    for (auto &vertex : v)
        for (auto &component : vertex)
            component = s16(static_cast<std::uint32_t>(component));
    FieldVector ab{}, ac{}, normal{};
    for (std::size_t i = 0; i < 3; ++i) {
        ab[i] = v[1][i] - v[0][i];
        ac[i] = v[2][i] - v[0][i];
    }
    ab = normalize_field_vector(ab, reciprocal);
    ac = normalize_field_vector(ac, reciprocal);
    for (std::size_t i = 0; i < 3; ++i) {
        const auto j = (i + 1) % 3, k = (i + 2) % 3;
        // OuterProduct12 reloads signed halfwords into the GTE's diagonal and
        // vector registers, including when a supplied table yields large normals.
        const auto cross = static_cast<std::int64_t>(s16(static_cast<std::uint32_t>(ab[j]))) *
                               s16(static_cast<std::uint32_t>(ac[k])) -
                           static_cast<std::int64_t>(s16(static_cast<std::uint32_t>(ab[k]))) *
                               s16(static_cast<std::uint32_t>(ac[j]));
        normal[i] = s32(static_cast<std::uint32_t>(cross >> 12));
    }
    if (normal[1] == 0)
        return {0, normal};
    // MULT/LO and SUBU before signed division; the plane is not evaluated with
    // floating point or a wider unwrapped numerator.
    const auto nx = static_cast<std::uint32_t>(normal[0]) *
                    static_cast<std::uint32_t>(s16(static_cast<std::uint32_t>(x)) - v[0][0]);
    const auto nz = static_cast<std::uint32_t>(normal[2]) *
                    static_cast<std::uint32_t>(s16(static_cast<std::uint32_t>(z)) - v[0][2]);
    const auto numerator = s32(0U - nx - nz);
    const auto quotient = static_cast<std::int64_t>(numerator) / normal[1];
    return {s16(static_cast<std::uint32_t>(static_cast<std::int64_t>(v[0][1]) + quotient)), normal};
}

FieldVector normalize_field_vector(const FieldVector &vector,
                                   std::span<const std::int16_t> reciprocal) {
    FieldVector values{};
    std::uint64_t magnitude = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        values[i] = s16(static_cast<std::uint32_t>(vector[i]));
        magnitude += static_cast<std::uint64_t>(static_cast<std::int64_t>(values[i]) * values[i]);
    }
    if (magnitude == 0)
        return {};
    // The original signed ADD traps here. Do not turn that path into host UB or
    // silently return a plausible normal. Hardware exception handling is open.
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        throw ActorInitializationError("Original normalization signed ADD overflow");
    const auto word = static_cast<std::uint32_t>(magnitude);
    const auto leading = static_cast<std::uint32_t>(std::countl_zero(word)) & ~1U;
    const auto shift = (31U - leading) >> 1U;
    const auto scaled = leading >= 24 ? word << (leading - 24) : word >> (24 - leading);
    const auto index = scaled - 64U;
    if (index >= reciprocal.size())
        throw ActorInitializationError("Normalization lookup exceeds supplied original table");
    for (auto &value : values)
        value =
            s32(static_cast<std::uint32_t>(static_cast<std::int64_t>(reciprocal[index]) * value)) >>
            shift;
    return values;
}

std::int32_t field_edge_area(const FieldVector &a, const FieldVector &b, std::int32_t x,
                             std::int32_t z) {
    return field_packed_area(
        {packed_coordinate(a[0], a[2]), packed_coordinate(b[0], b[2]), packed_coordinate(x, z)});
}

std::int32_t field_packed_area(std::array<std::uint32_t, 3> points) noexcept {
    std::int64_t area = 0;
    for (std::size_t i = 0; i < 3; ++i)
        area += static_cast<std::int64_t>(s16(points[i])) *
                (s16(points[(i + 1) % 3] >> 16U) - s16(points[(i + 2) % 3] >> 16U));
    return s32(static_cast<std::uint32_t>(area));
}

FloorLocation locate_initial_floor(const CollisionLayer &layer, std::int32_t active_triangle_count,
                                   std::int32_t x, std::int32_t z,
                                   std::span<const std::int16_t> reciprocal) {
    if (active_triangle_count <= 0)
        return {};
    const auto count = static_cast<std::size_t>(active_triangle_count);
    if (count > layer.triangles.size())
        throw ActorInitializationError("Original triangle count exceeds supplied layer");
    for (std::size_t i = 0; i < count; ++i) {
        std::array<FieldVector, 3> v{};
        for (std::size_t j = 0; j < 3; ++j) {
            const auto index = layer.triangles[i].vertices[j];
            if (index < 0 || static_cast<std::size_t>(index) >= layer.vertices.size())
                throw ActorInitializationError("Original vertex index exceeds supplied layer");
            const auto &source = layer.vertices[static_cast<std::size_t>(index)];
            for (std::size_t axis = 0; axis < 3; ++axis)
                v[j][axis] = source[axis];
        }
        if (field_edge_area(v[0], v[1], x, z) >= 0 && field_edge_area(v[1], v[2], x, z) >= 0 &&
            field_edge_area(v[2], v[0], x, z) >= 0) {
            const auto [height, normal] = field_height_and_normal(v, x, z, reciprocal);
            return {
                static_cast<std::uint32_t>(i),
                {s16(static_cast<std::uint32_t>(x)), height, s16(static_cast<std::uint32_t>(z))},
                normal,
                true};
        }
    }
    return {};
}

std::uint32_t actor_terrain_attribute(std::uint32_t disabled_layers, std::int16_t layer,
                                      const std::array<std::uint16_t, 4> &triangle_indices,
                                      const CollisionPackage &mesh) {
    const auto shift = (static_cast<std::uint32_t>(layer) + 3U) & 31U;
    if (((disabled_layers >> shift) & 1U) != 0)
        return 0;
    if (layer < 0 || static_cast<std::size_t>(layer) >= mesh.layers.size() ||
        static_cast<std::size_t>(layer) >= triangle_indices.size())
        throw ActorInitializationError("Unresolved original terrain layer access");
    const auto slot = static_cast<std::size_t>(layer);
    const auto triangle = s16(triangle_indices[slot]);
    if (triangle < 0 || static_cast<std::size_t>(triangle) >= mesh.layers[slot].triangles.size())
        throw ActorInitializationError("Unresolved original terrain triangle access");
    const auto attribute =
        mesh.layers[slot].triangles[static_cast<std::size_t>(triangle)].attribute_raw & 255U;
    return read(mesh.attributes_raw, attribute * 4U);
}

RandomStep advance_field_random(std::uint32_t seed) noexcept {
    seed = seed * 0x41c64e6dU + 0x3039U;
    return {seed, static_cast<std::uint16_t>((seed >> 16U) & 0x7fffU)};
}

original::ActorDefaults original::initialize_actor_defaults(
    ActorDefaults state, std::int16_t layer_count, const CollisionPackage &mesh,
    std::span<const std::int16_t> reciprocal, const DefaultsObserver &observe) {
    const auto queries = static_cast<std::uint32_t>(std::max(0, static_cast<int>(layer_count) - 1));
    if (queries > 4 || queries > mesh.layers.size())
        throw ActorInitializationError("Original initialization layer count exceeds storage");
    state.queried_layers = queries;
    auto &out = state.actor;
    auto &desc = state.descriptor;
    auto &local = state.scratch;
    const auto emit = [&](DefaultsStage stage, std::int32_t layer = -1, FloorLocation query = {},
                          std::uint32_t terrain = 0) {
        if (observe)
            observe({stage, state, layer, query, terrain});
    };
    const auto clear = [&](std::size_t at, std::uint32_t mask) {
        store(out, at, read(out, at) & mask);
    };
    store(out, 0, 0xb0);
    store(out, 4, 0x800);
    store(out, 0x18, 0x10, 2);
    store(out, 0x1c, 0x10, 2);
    store(out, 0x1a, 0x60, 2);
    out[0x74] = 255;
    out[0x75] = 255;
    for (const std::size_t offset : {0x40U, 0x44U, 0x48U, 0x30U, 0x34U, 0x38U})
        store(out, offset, 0);
    for (const std::size_t offset : {0x64U, 0x60U, 0x62U})
        store(out, offset, 0, 2);
    for (const std::size_t offset : {0xd0U, 0xd4U, 0xd8U})
        store(out, offset, 0);
    store(out, 0xe6, 0, 2);
    store(out, 0xea, 255, 2);
    out[0xe2] = 0;
    store(out, 0xcc, 0, 2);
    store(out, 0x6e, 0, 2);
    clear(0x12c, 0xffffffdf);
    store(out, 0x11e, 0x200, 2);
    store(out, 0x1e, read(out, 0x18, 2), 2);
    clear(0x12c, 0xfffffffc);
    for (std::size_t i = 0x102; i-- > 0xfc;)
        out[i] = 0x80;
    store(out, 0x128, 0xffff, 2);
    clear(0x12c, 0xfffcffff);
    clear(0x130, 0xf0000000);
    clear(0x12c, 0xf003ffff);
    for (std::size_t slot = 0; slot < 8; ++slot) {
        const auto at = 0x8c + slot * 8;
        out[at + 2] = 0;
        store(out, at, 0xffff, 2);
        out[at + 3] = 255;
        store(out, at + 4, (read(out, at + 4) & 0xfe3cffffU) | 0x3c0000U);
        store(out, at + 4, 0xffff, 2);
    }
    store(out, 0x120, 0);
    store(out, 0xe4, 255, 2);
    store(out, 0x76, 0x100, 2);
    out[0x83] = 0;
    out[0x82] = 0;
    store(out, 0x8a, 0, 2);
    store(out, 0x88, 0, 2);
    store(out, 0x84, 0);
    out[0xcf] = 0;
    out[0xce] = 0;
    store(out, 0xe8, 0, 2);
    store(out, 0x10, 0, 2);
    store(out, 0xec, 0, 2);
    clear(0x134, 0xffffff7f);
    clear(0x12c, 0xffffe03f);
    clear(0x134, 0xffffff9f);
    emit(DefaultsStage::random_before);
    const auto random = advance_field_random(state.seed);
    state.seed = random.seed;
    emit(DefaultsStage::random_after);
    store(out, 0x102, random.value, 2);
    for (const std::size_t offset : {0xf4U, 0xf6U, 0xf8U})
        store(out, offset, 0x1000, 2);
    out[0x10d] = 255;
    out[0x80] = 255;
    for (const std::size_t offset : {0x106U, 0x104U, 0x108U})
        store(out, offset, 0x8000, 2);
    store(out, 0x124, 0xffff, 2);
    out[0xe3] = 0;
    for (const std::size_t offset : {0xeU, 0xcU, 0xaU, 8U})
        store(out, offset, 0, 2);
    clear(0x12c, 0xffffffe3);
    const auto x = s16(read(desc, 0x20, 2)), z = s16(read(desc, 0x28, 2));
    for (std::uint32_t layer = 0; layer < queries; ++layer) {
        emit(DefaultsStage::locate_before, static_cast<std::int32_t>(layer));
        const auto found = locate_initial_floor(
            mesh.layers[layer], s32(state.triangle_counts[layer]), x, z, reciprocal);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            store(local, layer * 16 + axis * 4, static_cast<std::uint32_t>(found.normal[axis]));
            store(local, 0x40 + layer * 8 + axis * 2, static_cast<std::uint32_t>(found.point[axis]),
                  2);
        }
        emit(DefaultsStage::locate_after, static_cast<std::int32_t>(layer), found);
        store(out, 8 + layer * 2, found.triangle, 2);
        const auto index = s16(found.triangle);
        if (index != -1 && static_cast<std::uint32_t>(index) >= state.triangle_counts[layer]) {
            state.triangle_counts[layer] = 0;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                store(local, layer * 16 + axis * 4, 0);
                store(local, 0x40 + layer * 8 + axis * 2, 0, 2);
            }
        }
    }
    emit(DefaultsStage::terrain_before);
    std::array<std::uint16_t, 4> triangles{};
    for (std::size_t i = 0; i < 4; ++i)
        triangles[i] = static_cast<std::uint16_t>(read(out, 8 + i * 2, 2));
    const auto terrain = actor_terrain_attribute(read(out, 4), 0, triangles, mesh);
    emit(DefaultsStage::terrain_after, -1, {}, terrain);
    store(out, 0x14, terrain);
    for (std::size_t axis = 0; axis < 3; ++axis)
        store(out, 0x50 + axis * 4, read(local, axis * 4));
    if ((read(desc, 0x58, 2) & 0x80U) == 0)
        store(desc, 0x24, static_cast<std::uint32_t>(s16(read(local, 0x42, 2))));
    for (const std::size_t offset : {0x20U, 0x24U, 0x28U})
        store(out, offset, read(desc, offset) << 16U);
    store(out, 0x72, read(desc, 0x24), 2);
    emit(DefaultsStage::complete);
    return state;
}
} // namespace xem::reconstruction::field
