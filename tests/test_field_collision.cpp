#include "xem/reconstruction/field_collision.hpp"

#include <bit>
#include <iostream>
#include <limits>

namespace f = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Fn> void rejects(Fn fn, const char *message) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    check(rejected, message);
}
void put(std::span<std::uint8_t> data, std::size_t at, std::uint32_t value, std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        data[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t word(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::vector<std::uint8_t> mesh(bool second = false, std::uint32_t attribute = 0) {
    const std::size_t triangles = second ? 2U : 1U, vertices = second ? 6U : 3U;
    const auto vertex_base = 0x30U + triangles * 14U;
    const auto attribute_base = vertex_base + vertices * 8U;
    std::vector<std::uint8_t> data(attribute_base + 68U);
    put(data, 0, 1);
    put(data, 4, static_cast<std::uint32_t>(triangles * 14));
    put(data, 0x14, static_cast<std::uint32_t>(attribute_base));
    put(data, 0x18, 0x30);
    put(data, 0x1c, static_cast<std::uint32_t>(vertex_base));
    for (std::size_t t = 0; t < triangles; ++t) {
        for (std::size_t i = 0; i < 3; ++i) {
            put(data, 0x30 + t * 14 + i * 2, static_cast<std::uint32_t>(t * 3 + i), 2);
            put(data, 0x36 + t * 14 + i * 2,
                second && i == 1 ? static_cast<std::uint32_t>(1 - t) : 0xffffU, 2);
        }
    }
    const std::array<f::FieldVector, 6> points{
        {{0, 7, 0}, {0, 7, 16}, {16, 7, 0}, {16, 11, 16}, {16, 11, 0}, {0, 11, 16}}};
    for (std::size_t i = 0; i < vertices; ++i)
        for (std::size_t a = 0; a < 3; ++a)
            put(data, vertex_base + i * 8 + a * 2, word(points[i][a]), 2);
    put(data, attribute_base + 4, attribute);
    return data;
}
struct Fixture {
    std::array<std::int16_t, 192> reciprocal{}, roots{};
    std::array<std::int16_t, 1025> angles{};
    std::array<std::uint8_t, 16384> trig{};
    f::SweepActor actor{{0, 0, 0, 0, {4 << 16, 7 << 16, 4 << 16}}, 0, 255, 7, 5};
    f::CollisionEdge edge{{{0, 7, 0}, {16, 7, 0}}};
    Fixture() {
        reciprocal.fill(4096);
        roots.fill(4096);
        for (std::size_t i = 0; i < angles.size(); ++i)
            angles[i] = static_cast<std::int16_t>(i / 2);
    }
    f::CollisionTables tables() const { return {reciprocal, roots, angles, trig}; }
    f::CollisionResult query(const std::vector<std::uint8_t> &data, std::int32_t x, std::int32_t z,
                             f::CollisionTrace &trace, bool ordinary = true, std::int32_t mode = -1,
                             std::uint8_t control = 0) {
        return f::query_field_collision(
            data, reciprocal, actor.query,
            {std::bit_cast<std::int32_t>((word(x) << 16U) - word(actor.query.position[0])),
             std::numeric_limits<std::int32_t>::min(),
             std::bit_cast<std::int32_t>((word(z) << 16U) - word(actor.query.position[2]))},
            ordinary, mode, control, &trace);
    }
};
void queries() {
    Fixture v;
    f::CollisionTrace t;
    auto r = v.query(mesh(), 4, 4, t);
    check(r.value == 0 && r.point == f::FieldVector{4, 0, 4} && !r.edge && r.attribute == 0 &&
              t.counter == 256 && t.mask == 0 && t.heights.empty(),
          "inside probe effects");
    r = v.query(mesh(true), 12, 12, t, true, 0);
    check(r.value == 0 && r.point == f::FieldVector{12, 11, 12} && r.terminal_triangle == 1 &&
              t.steps.size() == 2 && t.steps[0].mask == 2 && t.steps[1].mask == 0 &&
              v.actor.query.triangle == 0,
          "adjacent triangle height and original slot");
    auto bytes = mesh(false, 0x20);
    bytes[0x2e] = 1;
    r = v.query(bytes, -2, 4, t);
    check(r.value == -1 && r.attribute == 0x20 && t.attribute_reads.back().triangle == -1 &&
              t.attribute_reads.back().index == 1 &&
              r.edge == f::CollisionEdge{{{0, 7, 0}, {0, 7, 16}}},
          "missing neighbor reads pre-table source byte");
    bytes = mesh(false, 0x400000);
    bytes[0x2e] = 1;
    rejects([&] { (void)v.query(bytes, -2, 4, t); }, "missing-neighbor height is unsupported");
    for (const auto index : {-2, 1, 3, 32767}) {
        v.actor.query.triangle = static_cast<std::int16_t>(index);
        rejects([&] { (void)v.query(mesh(), 4, 4, t); }, "per-layer initial triangle bounds");
        v.actor.query.triangle = 0;
        bytes = mesh();
        put(bytes, 0x36, word(index), 2);
        rejects([&] { (void)v.query(bytes, -2, 4, t); }, "per-layer neighbor bounds");
    }
    for (const auto index : {-1, 3, 7, 32767}) {
        bytes = mesh();
        put(bytes, 0x30, word(index), 2);
        rejects([&] { (void)v.query(bytes, 4, 4, t); }, "per-layer vertex bounds");
    }
    v.actor.query.triangle = -1;
    bytes = mesh();
    bytes.resize(0x20);
    r = v.query(bytes, 4, 4, t);
    check(r.value == -1 && !r.point && !r.edge && !r.attribute && t.steps.empty(),
          "initial sentinel before geometry");
    v.actor.query.triangle = 0;
    bytes = mesh();
    put(bytes, 0x36, 0, 2);
    r = v.query(bytes, -2, 4, t);
    check(r.value == -1 && t.reason == "iteration-limit" && t.counter == 32 &&
              t.steps.size() == 32 && t.attribute_reads.size() == 33 && r.terminal_triangle == 0,
          "32-step limit");
    const std::array<std::array<std::int32_t, 6>, 3> decisions{
        {{4, 2, -2, 20, 3, 1}, {4, 2, -2, -2, 5, 4}, {10, 5, 20, -2, 6, 2}}};
    for (const auto &d : decisions) {
        v.actor.query.position = {d[0] * 65536, 7 << 16, d[1] * 65536};
        r = v.query(mesh(), d[2], d[3], t);
        check(r.value == -1 && t.steps[0].mask == word(d[4]) && t.mask == word(d[5]) &&
                  t.areas.size() == 4,
              "double-edge original origin selection");
    }
    v.actor.query.position = {4 << 16, 20 << 16, 4 << 16};
    bytes = mesh(true, 0x400000);
    put(bytes, 0x30 + 14 + 12, 1, 2);
    r = v.query(bytes, 12, 12, t);
    check(r.value == -1 && t.reason == "terrain-floor-height" &&
              r.point == f::FieldVector{12, 11, 12},
          "special terrain transition");
    r = v.query(bytes, 12, 12, t, true, 0x80);
    check(r.value == 0 && t.initial_special == true && r.point == f::FieldVector{12, 11, 12},
          "mode80 height bypass");
    bytes = mesh(false, 8);
    put(bytes, 0x3c, 0xaa01, 2);
    v.actor.query.flags = 0x200;
    check(v.query(bytes, 4, 4, t).value == 0, "ordinary flag rule");
    r = v.query(bytes, 4, 4, t, false);
    check(r.value == -1 && !r.attribute && !r.edge, "special extra actor flag rule");
    bytes = mesh(false, 0x800000);
    put(bytes, 0x3c, 1, 2);
    check(v.query(bytes, 4, 4, t).value == -1 && v.query(bytes, 4, 4, t, true, -1, 1).value == 0,
          "terrain control mask");
    v.actor.query.layer_flags = 8;
    check(v.query(bytes, 4, 4, t).attribute == 0, "disabled layer mask");
}
void layer_floors() {
    Fixture v;
    std::array<std::uint8_t, 0x138> actor{};
    put(actor, 0x20, 4U << 16U);
    put(actor, 0x24, 7U << 16U);
    put(actor, 0x28, 4U << 16U);
    auto bytes = mesh();
    auto r = f::query_layer_floor(bytes, v.reciprocal, actor, 0, 0);
    check(r.value == 0 && r.floor == 7 && r.upper == 7 && r.triangle == 0, "layer floor height");
    bytes[0x30 + 13] = 2;
    check(f::query_layer_floor(bytes, v.reciprocal, actor, 0, 0).upper == 15,
          "nonnegative signed extent raises the upper bound");
    bytes[0x30 + 13] = 0xfe;
    check(f::query_layer_floor(bytes, v.reciprocal, actor, 0, 0).upper == 7,
          "negative extent contributes nothing");
    put(actor, 0x30, 1);
    put(actor, 0x72, 99, 2);
    check(f::query_layer_floor(bytes, v.reciprocal, actor, 0, 0).floor == 99,
          "a moving actor keeps its selected floor on its own layer");
    put(actor, 0x30, 0);
    bytes = mesh(false, 0x800000);
    put(bytes, 0x3c, 1, 2);
    r = f::query_layer_floor(bytes, v.reciprocal, actor, 0, 0);
    check(r.value == 0 && r.floor == 0x7fffffff && r.upper == 0x7fffffff && r.triangle == 0,
          "masked terrain opens the layer");
    check(f::query_layer_floor(bytes, v.reciprocal, actor, 0, 1).floor == 7,
          "attribute control disables the terrain mask");
    put(actor, 8, 0xffff, 2);
    r = f::query_layer_floor(mesh(), v.reciprocal, actor, 0, 0);
    check(r.value == -1 && !r.floor && !r.triangle, "missing triangle leaves outputs untouched");
    put(actor, 8, 0, 2);
    put(actor, 0x30, 0xfff00000U);
    check(f::query_layer_floor(mesh(), v.reciprocal, actor, 0, 0).value == -1,
          "missing neighbor fails the layer");
    rejects([&] { (void)f::query_layer_floor(mesh(), v.reciprocal, actor, 1, 0); },
            "layer beyond the component's count");
}
void arithmetic() {
    Fixture v;
    check(f::pack_collision_xz(4, -1) == 0x3ffffU &&
              f::field_packed_area({0, 16, f::pack_collision_xz(2, 3)}) == 32,
          "NCLIP packing and borrow");
    check(f::collision_atan(1, 2, v.angles).angle == 256 &&
              f::collision_atan(-1, -2, v.angles).angle == -1792 &&
              f::collision_atan(2, 1, v.angles).angle == 768 &&
              !f::collision_atan(0, 0, v.angles).table_index,
          "atan signed quadrants");
    check(f::collision_atan(0x200000, 0x400000, v.angles).division_path == "z-smaller-reduced",
          "atan reduced divide");
    rejects([&] { (void)f::collision_atan(std::numeric_limits<std::int32_t>::min(), 0, v.angles); },
            "atan BREAK");
    auto root = f::collision_sqrt(25, v.roots);
    check(root.value == 4 && root.leading_zeroes == 27 && root.table_index == 36,
          "sqrt original bins");
    v.roots[36] = 5120;
    check(f::collision_sqrt(25, v.roots).value == 5, "supplied root coefficient");
    v.roots[0] = -1;
    root = f::collision_sqrt(64, v.roots);
    check(root.shifted_word == 0xfffffff8U && root.value == 0xfffff,
          "signed root load and logical final shift");
    check(f::collision_sqrt(0, v.roots).leading_zeroes == 32, "sqrt zero delay slot");
    rejects([&] { (void)f::collision_planar_length(-32768, -32768, v.roots); },
            "negative wrapped length unqualified");
    v.roots.fill(4096);
    for (const auto relative : {127, 128, 3968, 3969}) {
        const auto p =
            f::project_collision_edge(static_cast<std::int16_t>((0xc00 - relative) & 0xfff), v.edge,
                                      {65536, -700, 0}, v.tables());
        const auto expected = relative == 128 ? -65536 : relative == 3968 ? 65536 : 0;
        check(p.velocity == f::FieldVector{expected, 0, 0} &&
                  p.branch == (expected == 0 ? "stop" : "slide"),
              "edge inclusive gate and cleared Y");
    }
}
void sweeps() {
    Fixture v;
    f::CollisionSweepTrace t;
    auto r = f::sweep_field_collision(mesh(), v.tables(), v.actor, {65536, 123, 0}, v.edge, 0, true,
                                      0, 0, &t);
    check(r.value == 0 && r.velocity == f::FieldVector{65536, 0, 0} &&
              r.actor.query.flags == 0x4000000 &&
              r.actor.query.position == v.actor.query.position && t.queries.size() == 5 &&
              t.queries[0].stage == "probe-left" && t.queries[1].stage == "probe-right" &&
              t.queries[2].stage == "probe-center" && t.queries[4].stage == "slope-floor",
          "ordinary sweep connected query and slope");
    v.actor.query.flags = 0x800;
    r = f::sweep_field_collision(mesh(), v.tables(), v.actor, {65536, 123, 0}, v.edge, 0, false, 0,
                                 0, &t);
    check(r.value == 0 && r.actor.query.flags == 0x800 && !t.slope &&
              t.queries[0].stage == "probe-center",
          "special probe order");
    v.actor.query.position[1] = 10 << 16;
    v.actor.floor = 99;
    r = f::sweep_field_collision(mesh(), v.tables(), v.actor, {65536, 123, 0}, v.edge, 0, false, 0,
                                 0);
    check(r.value == -1 && r.actor == v.actor && r.velocity == f::FieldVector{65536, 123, 0},
          "special rejection preserves stores");
    v.actor.query.flags = 0x40000;
    r = f::sweep_field_collision(mesh(), v.tables(), v.actor, {65536, 123, 0}, v.edge, 0, false, 0,
                                 0);
    check(r.value == 0 && r.actor.floor == 5 && r.velocity[1] == -5 * 65536, "forced floor");
    v.actor.query.flags = 0;
    v.actor.query.position = {10 << 16, 7 << 16, 4 << 16};
    v.reciprocal.fill(8192);
    v.roots.fill(8192);
    r = f::sweep_field_collision(mesh(), v.tables(), v.actor, {65536, 123, 0}, v.edge, 0, true, 0,
                                 0, &t);
    check(r.value == -1 && r.actor == v.actor && r.velocity == f::FieldVector{65536, 123, 0} &&
              r.edge == f::CollisionEdge{{{0, 7, 16}, {16, 7, 0}}} &&
              t.queries.back().stage == "slope-floor",
          "failed slope preserves edge but not actor stores");
    std::array<std::uint8_t, 312> actor{};
    actor.fill(0xa5);
    std::array<std::uint8_t, 12> velocity{};
    std::array<std::uint8_t, 16> edge{};
    edge.fill(0x5a);
    put(actor, 0, 0);
    put(actor, 4, 0);
    put(actor, 8, 0, 2);
    put(actor, 0x10, 0, 2);
    put(actor, 0x14, 0);
    put(actor, 0x20, 10U << 16U);
    put(actor, 0x24, 7U << 16U);
    put(actor, 0x28, 4U << 16U);
    actor[0x74] = 255;
    put(velocity, 0, 65536);
    put(velocity, 4, 123);
    const auto old_actor = actor;
    const auto old_velocity = velocity;
    check(f::apply_field_collision_sweep(actor, velocity, edge, 0, mesh(), v.tables(), 0, 0) ==
                  -1 &&
              actor == old_actor && velocity == old_velocity && edge[6] == 0x5a &&
              edge[7] == 0x5a && edge[14] == 0x5a && edge[15] == 0x5a,
          "raw failure and edge padding preservation");
}
} // namespace
int main() {
    try {
        layer_floors();
        queries();
        arithmetic();
        sweeps();
        std::cout << "field collision: 3 focused groups passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
