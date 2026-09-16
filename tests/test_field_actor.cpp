#include "xem/reconstruction/field_actor.hpp"
#include "xem/reconstruction/field_return.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

namespace field = xem::reconstruction::field;
namespace original = field::original;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Function> void rejects(Function &&function) {
    try {
        function();
    } catch (const field::ActorInitializationError &) {
        return;
    }
    throw std::runtime_error("Unresolved actor initialization must fail explicitly");
}
void put(std::span<std::uint8_t> data, std::size_t at, std::uint32_t value, std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        data[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t at) {
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < 4; ++i)
        result |= static_cast<std::uint32_t>(data[at + i]) << (i * 8);
    return result;
}
std::array<std::int16_t, 192> invented_reciprocals() {
    std::array<std::int16_t, 192> table;
    table.fill(4096);
    return table;
}
field::CollisionPackage flat_mesh() {
    field::CollisionPackage mesh;
    for (const auto y : std::array<std::int16_t, 2>{7, 17}) {
        field::CollisionLayer layer;
        layer.vertices = {{{0, y, 0, 0}}, {{0, y, 32, 0}}, {{32, y, 0, 0}}};
        layer.triangles.push_back({{0, 1, 2}, {0xffff, 0xffff, 0xffff}, 0xab00});
        mesh.layers.push_back(layer);
    }
    mesh.attributes_raw.resize(4);
    put(mesh.attributes_raw, 0, 0x12340020);
    return mesh;
}
original::ActorDefaults incoming() {
    original::ActorDefaults state{};
    for (std::size_t i = 0; i < state.actor.size(); ++i)
        state.actor[i] = static_cast<std::uint8_t>(i * 31 + 17);
    for (std::size_t i = 0; i < state.descriptor.size(); ++i)
        state.descriptor[i] = static_cast<std::uint8_t>(i * 13 + 7);
    for (std::size_t i = 0; i < state.scratch.size(); ++i)
        state.scratch[i] = static_cast<std::uint8_t>(i * 7 + 5);
    put(state.descriptor, 0x20, 4);
    put(state.descriptor, 0x24, 123);
    put(state.descriptor, 0x28, 4);
    put(state.descriptor, 0x58, 0, 2);
    state.seed = 1;
    state.triangle_counts = {1, 1, 0, 0};
    return state;
}
void connected_initialization_and_events() {
    const auto mesh = flat_mesh();
    const auto table = invented_reciprocals();
    const auto before = incoming();
    const auto result = original::initialize_actor_defaults(before, 3, mesh, table);
    check(result.queried_layers == 2 && result.seed == 0x41c67ea6 &&
              word(result.descriptor, 0x24) == 7 && word(result.actor, 0x24) == 0x70000 &&
              word(result.actor, 0x14) == 0x12340020,
          "Defaults compose RNG, both floor queries, terrain and fixed-point positions");
    for (const std::size_t at : {0x2cU, 0x3cU, 0x68U, 0xdcU, 0x110U, 0x114U, 0x118U})
        check(word(before.actor, at) == word(result.actor, at), "Unknown actor words survive");
    for (const std::size_t at : {12U, 28U, 44U, 60U, 80U, 84U, 88U, 92U})
        check(word(before.scratch, at) == word(result.scratch, at), "Unwritten scratch survives");
    auto event = result.event_state();
    check(event.pc == 0 && event.selected_slot == 0 && event.flags == 0xb0,
          "Initialization shares semantic event state with restored actors");
    for (const auto &slot : event.slots)
        check(slot.priority() == 15 && slot.resume_pc == 0xffff && slot.countdown == 0 &&
                  slot.event_tag == 255,
              "Every event slot is initialized");
    original::ActorReturnRecord returned{};
    returned.actor = result.actor;
    check(returned.event_state().slots[0].control_bits == event.slots[0].control_bits,
          "Snapshot and initialization use one original event layout");
    field::select_event_slot(event, 0);
    const std::array<std::uint8_t, 4> code{0x01, 3, 0, 0x00};
    const std::array<std::array<std::uint16_t, 32>, 1> entries{};
    field::EventVariables vars;
    field::EventContext context{{code, entries}, &vars, {}, &event};
    context.control.budget_mode = 1;
    const auto batch = field::run_event_batch(context, 1, field::execute_core_event);
    check(batch.dispatched == 1 && event.pc == 3,
          "A default actor executes through the recovered event batch and branch");
}
void arithmetic_and_original_order_boundaries() {
    const auto zero = field::advance_field_random(0);
    const auto high = field::advance_field_random(0xffffffff);
    check(zero.seed == 0x3039 && zero.value == 0 && high.seed == 0xbe39e1cc && high.value == 15929,
          "Original RNG wraps, shifts and masks independently of host libc");
    const auto table = invented_reciprocals();
    check(field::normalize_field_vector({0, 0, 0}, {}) == field::FieldVector{},
          "Zero normal has zero semantic output despite the original preceding-table read");
    check(field::normalize_field_vector({1, 0, 0}, table) == field::FieldVector{4096, 0, 0},
          "Normalization uses original table scaling");
    check(field::normalize_field_vector({65535, 0, 0}, table) == field::FieldVector{-4096, 0, 0},
          "GTE input truncates to signed halfwords");
    rejects([&] { static_cast<void>(field::normalize_field_vector({-32768, -32768, 0}, table)); });
    rejects([&] { static_cast<void>(field::normalize_field_vector({1, 0, 0}, {})); });
    auto wide_table = invented_reciprocals();
    wide_table.fill(32767);
    auto short_edge_mesh = flat_mesh();
    short_edge_mesh.layers[0].vertices[1][2] = 15;
    short_edge_mesh.layers[0].vertices[2][0] = 15;
    // Normalized edge = 32767*15 >> 3 = 61438, loaded as -4098 in GTE.
    // Cross-product Y is therefore (-4098 * -4098) >> 12 = 4100.
    check(field::locate_initial_floor(short_edge_mesh.layers[0], 1, 1, 1, wide_table).normal[1] ==
              4100,
          "OuterProduct12 reloads normalized components as signed GTE halfwords");
    // Packed (Z,X) coordinates are (-3,-3), (4,-2), (0,0): 7*3 - 1*3.
    check(field::field_edge_area({-2, 0, -3}, {-2, 0, 4}, 0, 0) == 18,
          "Signed Z borrows from packed X before the original determinant");
    auto state = incoming();
    put(state.descriptor, 0x58, 0x80, 2);
    put(state.descriptor, 0x24, 0xabcdfff9);
    const auto preserved = original::initialize_actor_defaults(state, 3, flat_mesh(), table);
    check(word(preserved.descriptor, 0x24) == 0xabcdfff9 &&
              word(preserved.actor, 0x24) == 0xfff90000,
          "Descriptor ownership flag preserves Y and position conversion wraps");
    std::vector<original::DefaultsStage> order;
    static_cast<void>(original::initialize_actor_defaults(
        state, 3, flat_mesh(), table, [&](const original::DefaultsObservation &item) {
            order.push_back(item.stage);
            if (item.stage == original::DefaultsStage::random_after)
                check(item.state.actor[0x102] == state.actor[0x102] &&
                          item.state.actor[0x103] == state.actor[0x103],
                      "Random return is observed before its actor halfword store");
        }));
    check(order ==
              std::vector{
                  original::DefaultsStage::random_before, original::DefaultsStage::random_after,
                  original::DefaultsStage::locate_before, original::DefaultsStage::locate_after,
                  original::DefaultsStage::locate_before, original::DefaultsStage::locate_after,
                  original::DefaultsStage::terrain_before, original::DefaultsStage::terrain_after,
                  original::DefaultsStage::complete},
          "Observable effects retain original operation ordering");
}
void failure_and_unobserved_boundaries() {
    auto state = incoming();
    auto mesh = flat_mesh();
    const auto table = invented_reciprocals();
    put(state.scratch, 0, 11);
    put(state.scratch, 4, 0xfffffff4);
    put(state.scratch, 8, 13);
    put(state.scratch, 0x42, 0xfff1, 2);
    const auto no_queries = original::initialize_actor_defaults(state, -1, mesh, table);
    check(no_queries.queried_layers == 0 && no_queries.scratch == state.scratch &&
              word(no_queries.actor, 0x50) == 11 && word(no_queries.actor, 0x54) == 0xfffffff4 &&
              word(no_queries.descriptor, 0x24) == 0xfffffff1,
          "Zero-query path consumes incoming scratch instead of fabricated zeros");
    put(state.descriptor, 0x20, 0xffffffff);
    unsigned misses = 0;
    const auto miss = original::initialize_actor_defaults(
        state, 3, mesh, table, [&](const original::DefaultsObservation &item) {
            if (item.stage == original::DefaultsStage::locate_after && !item.query.matched)
                ++misses;
        });
    check(misses == 2 && word(miss.actor, 8) == 0 && word(miss.actor, 0x14) == 0x12340020 &&
              word(miss.descriptor, 0x24) == 0,
          "No match returns zero outputs but still selects the original triangle-zero terrain");
    state = incoming();
    state.triangle_counts[0] = 0;
    check(word(original::initialize_actor_defaults(state, 3, mesh, table).actor, 0x14) ==
              0x12340020,
          "Mutable zero triangle count does not erase physical triangle storage");
    rejects([&] { static_cast<void>(original::initialize_actor_defaults(state, 4, mesh, table)); });
    state.triangle_counts[0] = 2;
    rejects([&] { static_cast<void>(original::initialize_actor_defaults(state, 3, mesh, table)); });
    state.triangle_counts[0] = 1;
    mesh.layers[0].triangles[0].vertices[0] = -1;
    rejects([&] { static_cast<void>(original::initialize_actor_defaults(state, 3, mesh, table)); });
    check(field::actor_terrain_attribute(1U << 2, -1, {}, {}) == 0,
          "Disabled terrain branch returns before invalid layer addressing");
    rejects([&] { static_cast<void>(field::actor_terrain_attribute(0, -1, {}, {})); });
}

// Private original replay transport, not an oracle: stdin supplies one 522-byte
// actor/descriptor/scratch/seed/counts/layer record. The two files contain the
// exact collision component and the 192 original little-endian reciprocals.
// stdout emits each boundary's five u32 metadata fields then a 524-byte state,
// and finally the returned 524-byte state. No original bytes are embedded here.
std::vector<std::uint8_t> file(const char *path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Cannot read original replay input");
    return {std::istreambuf_iterator<char>(stream), {}};
}
void write_u32(std::uint32_t value) {
    std::array<std::uint8_t, 4> bytes{};
    put(bytes, 0, value);
    std::cout.write(reinterpret_cast<const char *>(bytes.data()), 4);
}
void write_state(const original::ActorDefaults &state) {
    for (const std::span<const std::uint8_t> bytes :
         {std::span<const std::uint8_t>(state.actor),
          std::span<const std::uint8_t>(state.descriptor),
          std::span<const std::uint8_t>(state.scratch)})
        std::cout.write(reinterpret_cast<const char *>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
    write_u32(state.seed);
    for (const auto count : state.triangle_counts)
        write_u32(count);
    write_u32(state.queried_layers);
}
void original_replay(const char *collision_path, const char *table_path) {
    const auto mesh = field::parse_collision_package(file(collision_path));
    const auto table_bytes = file(table_path);
    check(table_bytes.size() == 384, "Original reciprocal table must contain 192 halfwords");
    std::array<std::int16_t, 192> table{};
    for (std::size_t i = 0; i < table.size(); ++i)
        table[i] = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
            table_bytes[i * 2] | (static_cast<std::uint32_t>(table_bytes[i * 2 + 1]) << 8U)));
    std::array<std::uint8_t, 522> bytes{};
    std::cin.read(reinterpret_cast<char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    check(std::cin.gcount() == static_cast<std::streamsize>(bytes.size()) && std::cin.peek() == EOF,
          "Incomplete or trailing original defaults input");
    original::ActorDefaults state{};
    std::copy_n(bytes.begin(), 312, state.actor.begin());
    std::copy_n(bytes.begin() + 312, 92, state.descriptor.begin());
    std::copy_n(bytes.begin() + 404, 96, state.scratch.begin());
    state.seed = word(bytes, 500);
    for (std::size_t i = 0; i < 4; ++i)
        state.triangle_counts[i] = word(bytes, 504 + i * 4);
    const auto layer = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(bytes[520] | (static_cast<std::uint32_t>(bytes[521]) << 8U)));
    const auto result = original::initialize_actor_defaults(
        state, layer, mesh, table, [](const original::DefaultsObservation &item) {
            write_u32(static_cast<std::uint32_t>(item.stage));
            write_u32(static_cast<std::uint32_t>(item.layer));
            write_u32(item.query.triangle);
            write_u32(item.query.matched ? 1 : 0);
            write_u32(item.terrain);
            write_state(item.state);
        });
    write_state(result);
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--original") {
            original_replay(argv[2], argv[3]);
            return 0;
        }
        check(argc == 1, "Usage: test-field-actor [--original collision.bin reciprocals.bin]");
        connected_initialization_and_events();
        arithmetic_and_original_order_boundaries();
        failure_and_unobserved_boundaries();
        std::cout << "Three connected actor initialization groups passed (synthetic fixtures)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
