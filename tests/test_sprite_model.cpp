#include "xem/reconstruction/field_sprite_model.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
// Authored synthetic bytes. Expectations follow reviewed resident8002c59c,
// 8002c8cc,8002cd64,8002d984 and8002d0e4; no original game fixture is embedded.
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    check(offset <= bytes.size() && width <= bytes.size() - offset, "Synthetic write extent");
    for (std::size_t i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width = 4) {
    check(offset <= bytes.size() && width <= bytes.size() - offset, "Synthetic read extent");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8);
    return value;
}
template <typename Error, typename Function> void rejects(Function &&function) {
    try {
        function();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error("Unsupported model operation must fail explicitly");
}

struct Fixture {
    static constexpr std::uint32_t sprite_address = 0x80010000;
    static constexpr std::uint32_t model_address = 0x80020000;
    static constexpr std::uint32_t output_address = 0x80030000;
    static constexpr std::uint32_t old_address = 0x80040000;
    std::array<std::uint8_t, 356> sprite{};
    std::array<std::uint8_t, 256> model{};
    std::array<std::uint8_t, 16 * 40> table{};
    std::array<field::SpriteResource, 2> resources;
    std::array<field::SpriteWindow, 1> owned;
    field::SpriteModelState state{};
    field::SpriteHeapControls heap{};
    field::SpriteEnvironment environment{};
    field::SpriteServices services{};
    std::vector<std::uint8_t> incoming = std::vector<std::uint8_t>(144);
    std::vector<std::uint32_t> released;
    std::uint32_t allocation_count{};

    Fixture()
        : resources{{{model_address, model}, {0x8004fe68, table}}},
          owned{{{model_address, model}}} {
        put(sprite, 0x20, sprite_address + 0xb4);
        put(sprite, 0xe0, old_address);
        put(sprite, 0xe4, old_address + 16);
        put(sprite, 0xe8, 0xabcd1234);
        put(sprite, 0x64, 0x80050000); // Dispatcher owns PC, not the model operation.
        put(model, 0, 0x4000, 2);
        put(model, 4, 2, 2);
        put(model, 6, 2, 2);
        put(model, 8, 0x38);
        put(model, 12, 0x40);
        put(model, 16, 0x80);
        put(model, 20, 0xa0);
        put(model, 24, 0x87654321);
        put(model, 0x34, 72);
        put(model, 0x80, 5, 1);
        put(model, 0x82, 1, 2);
        put(model, 0x8c, 13, 1);
        put(model, 0x8e, 1, 2);
        // Two control words precede a triangle, another page update precedes a quad.
        put(model, 0xa0, 0xc40017ab);
        put(model, 0xa4, 0xc800123d);
        put(model, 0xa8, 0x25001122);
        put(model, 0xac, 0x55663344);
        put(model, 0xb0, 0xc400035f);
        put(model, 0xb4, 0x2dabcd89);
        put(model, 0xb8, 0x56781234);
        put(model, 0xbc, 0xdef09abc);
        for (auto tag : {5U, 13U}) {
            const auto at = tag * 40;
            put(table, at, tag == 5 ? 0x8002d984 : 0x8002d0e4);
            put(table, at + 4, 8);
            put(table, at + 8, tag == 5 ? 8 : 12);
            put(table, at + 12, tag == 5 ? 32 : 40);
        }
        state.material_page = 0xaaaa;
        state.material_palette = 0xbbbb;
        state.palette_base = 0x89abbe00;
        state.palette_mode = 0;
        state.primitive_count = 0xfffffffe;
        state.buffers.push_back({old_address, std::vector<std::uint8_t>(32, 0x71)});
        environment.texture_page = 0xcafe0014;
        environment.texture_mode = 1;
        heap.allocation_class = 8;
        heap.class_eight_context = 0x34567890;
        heap.class_five_context = 0x12345678;
        heap.allocation_cursor = 9;
        heap.tag = 0xabba;
        for (std::size_t i = 0; i < incoming.size(); ++i)
            incoming[i] = static_cast<std::uint8_t>(17 * i + 5);
        services.release = [&](std::uint32_t address) {
            check(heap.allocation_class == 5 && heap.class_five_context == 0 &&
                      heap.allocation_cursor == 0,
                  "F5 selects its heap class before releasing packet ownership");
            check((get(model, 0, 2) & 0x20) != 0,
                  "Original relocation precedes releasing the old allocation");
            released.push_back(address);
        };
        services.allocate = [&](std::uint32_t size, std::uint32_t mode) {
            check(size == 144 && mode == 0 && heap.tag == 0x25,
                  "Double-buffer request uses original size, mode and tag");
            check(released.size() == allocation_count + 1,
                  "Release precedes each new packet allocation");
            const auto address = output_address + 0x1000 * allocation_count++;
            return field::SpriteAllocation{address, incoming};
        };
    }
    field::SpriteSources sources() {
        field::SpriteSources result{resources, {}, {}, {}, {}};
        result.mutable_resources = owned;
        result.models = &state;
        result.heap = &heap;
        result.services = &services;
        return result;
    }
    void construct() {
        field::construct_sprite_model({sprite_address, sprite}, model_address, environment,
                                      sources());
    }
};

void relocate_nested_and_idempotent() {
    Fixture f;
    put(f.model, 8, 0xfffffff8);
    put(f.model, 0x1c, 0x40);
    put(f.model, 0x40, 1); // Inclusive count: two triples, visited in descending order.
    put(f.model, 0x44, 0x11111111);
    put(f.model, 0x48, 0x20);
    put(f.model, 0x4c, 0xffffff00);
    put(f.model, 0x50, 0x22222222);
    put(f.model, 0x54, 0x30);
    put(f.model, 0x58, 0x40);
    field::relocate_sprite_model(Fixture::model_address, f.sources());
    check(get(f.model, 0, 2) == 0x4020 && get(f.model, 8) == 0x8001fff8 &&
              get(f.model, 12) == 0x80020040 && get(f.model, 16) == 0x80020080 &&
              get(f.model, 20) == 0x800200a0 && get(f.model, 0x1c) == 0x80020040,
          "Relocation preserves unrelated flags and wraps original unsigned pointer additions");
    check(get(f.model, 0x44) == 0x11111111 && get(f.model, 0x48) == 0x80020020 &&
              get(f.model, 0x4c) == 0x8001ff00 && get(f.model, 0x50) == 0x22222222 &&
              get(f.model, 0x54) == 0x80020030 && get(f.model, 0x58) == 0x80020040,
          "Nested records rebase only their two pointer words");
    const auto once = f.model;
    field::relocate_sprite_model(Fixture::model_address, f.sources());
    check(f.model == once, "Already relocated models are idempotent");
}

void relocation_keeps_original_store_order() {
    Fixture f;
    f.owned[0].bytes = std::span(f.model).first(16);
    rejects<field::SpriteInputError>(
        [&] { field::relocate_sprite_model(Fixture::model_address, f.sources()); });
    check(get(f.model, 0, 2) == 0x4020 && get(f.model, 8) == 0x80020038 && get(f.model, 12) == 0x40,
          "Header stores are flags,+8,+16,+12,+20; failing+16 cannot commit+12");
    Fixture nested;
    put(nested.model, 0x1c, 0x40);
    put(nested.model, 0x40, 1);
    put(nested.model, 0x48, 0x20);
    put(nested.model, 0x4c, 0x30);
    nested.owned[0].bytes = std::span(nested.model).first(84);
    rejects<field::SpriteInputError>(
        [&] { field::relocate_sprite_model(Fixture::model_address, nested.sources()); });
    check(get(nested.model, 0x1c) == 0x80020040 && get(nested.model, 0x48) == 0x20 &&
              get(nested.model, 0x4c) == 0x30,
          "Optional pointer publishes first; descending traversal fails before earlier triples");
}

void packet_bytes_controls_and_duplication() {
    Fixture f;
    auto expected = std::vector<std::uint8_t>(f.incoming.begin(), f.incoming.begin() + 72);
    // Explicit expected bytes for two source-reviewed packet layouts.
    expected[3] = 7;
    expected[7] = 0x25;
    put(expected, 12, 0xbe0d3344);
    put(expected, 20, 0x17b45566);
    put(expected, 28, 0x1122, 2);
    expected[35] = 9;
    put(expected, 36, 0x2dabcd89);
    put(expected, 44, 0xbe0d1234);
    put(expected, 52, 0x03545678);
    put(expected, 60, 0x9abc, 2);
    put(expected, 68, 0xdef0, 2);
    const auto first = expected;
    expected.insert(expected.end(), first.begin(), first.end());
    f.construct();
    check(f.state.buffers.size() == 1 &&
              f.state.buffers.front().address == Fixture::output_address &&
              f.state.buffers.front().bytes == expected,
          "Packet initialization preserves untouched allocator bytes and copies the whole first "
          "half");
    check(f.incoming[3] != 7 && f.released == std::vector<std::uint32_t>{Fixture::old_address},
          "Incoming service bytes are separately owned and old allocation is released once");
    check(get(f.sprite, 0xe0) == Fixture::output_address &&
              get(f.sprite, 0xe4) == Fixture::output_address + 72 &&
              get(f.sprite, 0xe8) == Fixture::model_address && get(f.sprite, 0x64) == 0x80050000,
          "F5 publishes both frame pointers and model without taking ownership of command PC");
    check(
        f.state.primitive_count == 0 && f.state.material_page == 0x354 &&
            f.state.material_palette == 0xbe0d && f.state.palette_mode == 1 &&
            f.environment.texture_mode == 0 && f.environment.texture_page == 0xcafe0014 &&
            f.state.palette_base == 0x89abbe00,
        "Control records, wrapping primitive count and completion reset preserve unrelated words");
    check(f.state.geometry == Fixture::model_address + 0x98 &&
              f.state.shading == Fixture::model_address + 0xc0 &&
              f.state.output == Fixture::output_address + 72 &&
              f.state.vertices == Fixture::model_address + 0x38 &&
              f.state.normals == Fixture::model_address + 0x40 && f.state.auxiliary == 0x87654321 &&
              f.heap.class_eight_context == 0x34567890,
          "Shared cursors use separate geometry/shading/output strides and preserve class8");
}

void unsupported_handler_preserves_prior_effects() {
    Fixture f;
    f.model[0x8c] = 6;
    put(f.table, 6 * 40, 0x8002cdcc);
    put(f.table, 6 * 40 + 4, 8);
    put(f.table, 6 * 40 + 8, 4);
    put(f.table, 6 * 40 + 12, 20);
    rejects<field::UnrecoveredSpriteBehavior>([&] { f.construct(); });
    check(
        f.state.buffers.size() == 1 && f.state.buffers.front().bytes[3] == 7 &&
            get(f.state.buffers.front().bytes, 12) == 0xbe0d3344 &&
            std::equal(f.state.buffers.front().bytes.begin() + 32,
                       f.state.buffers.front().bytes.end(), f.incoming.begin() + 32),
        "Unsupported later handler retains earlier packet stores without clearing or copying rest");
    check(get(f.model, 0, 2) == 0x4020 && get(f.sprite, 0xe0) == Fixture::output_address &&
              get(f.sprite, 0xe4) == Fixture::output_address + 72 &&
              get(f.sprite, 0xe8) == 0xabcd1234 && f.environment.texture_mode == 1 &&
              f.state.palette_mode == 0 && f.state.output == Fixture::output_address + 32,
          "Failure preserves committed relocation/ownership; final model/reset remain unexecuted");
}

void fragmented_control_requires_its_input_before_mutation() {
    Fixture f;
    f.environment.texture_mode = 2;
    // Only the command byte is qualified: C4 still loads its original halfword
    // before applying mode2's replacement, even though that value is overwritten.
    std::array<field::SpriteResource, 3> fragments{{
        {Fixture::model_address, std::span(f.model).first(0xa0)},
        {Fixture::model_address + 0xa3, std::span(f.model).subspan(0xa3, 1)},
        {0x8004fe68, f.table},
    }};
    auto sources = f.sources();
    sources.resources = fragments;
    rejects<field::SpriteInputError>([&] {
        field::construct_sprite_model({Fixture::sprite_address, f.sprite}, Fixture::model_address,
                                      f.environment, sources);
    });
    check(
        f.state.material_page == 0xaaaa && f.state.material_palette == 0xbbbb &&
            f.state.shading == Fixture::model_address + 0xa0 && f.state.buffers.size() == 1 &&
            f.state.buffers.front().bytes == f.incoming,
        "A qualified C4 command cannot replace material state without its original halfword read");
    check(get(f.model, 0, 2) == 0x4020 && get(f.sprite, 0xe0) == Fixture::output_address &&
              f.environment.texture_mode == 2 && f.state.palette_mode == 0,
          "Missing C4 input preserves prior allocation/relocation without completion resets");
}

void replacement_owns_one_double_buffer() {
    Fixture f;
    f.construct();
    const auto relocated = f.model;
    f.construct();
    check(f.model == relocated && f.state.buffers.size() == 1 &&
              f.state.buffers.front().address == Fixture::output_address + 0x1000 &&
              f.released ==
                  std::vector<std::uint32_t>{Fixture::old_address, Fixture::output_address} &&
              get(f.sprite, 0xe0) == Fixture::output_address + 0x1000 &&
              get(f.sprite, 0xe4) == Fixture::output_address + 0x1000 + 72,
          "Repeated F5 releases old owned double buffer and retains one replacement");
}
} // namespace

int main() {
    try {
        relocate_nested_and_idempotent();
        relocation_keeps_original_store_order();
        packet_bytes_controls_and_duplication();
        unsupported_handler_preserves_prior_effects();
        fragmented_control_requires_its_input_before_mutation();
        replacement_owns_one_double_buffer();
        std::cout << "sprite model reconstruction checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
