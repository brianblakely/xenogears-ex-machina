#include "xem/reconstruction/field_sprite.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Function> void rejects(Function &&function) {
    try {
        function();
    } catch (const field::SpriteError &) {
        return;
    }
    throw std::runtime_error("Unqualified sprite operation must fail explicitly");
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    check(at <= bytes.size() && width <= bytes.size() - at, "Test write out of bounds");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    check(at <= bytes.size() && width <= bytes.size() - at, "Test read out of bounds");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (i * 8);
    return value;
}
struct Fixture {
    static constexpr std::uint32_t address = 0x80001000;
    static constexpr std::uint32_t resource = 0x80010000;
    field::SpriteAllocation incoming{address, std::vector<std::uint8_t>(356)};
    std::vector<std::uint8_t> data = std::vector<std::uint8_t>(512);
    std::vector<std::uint8_t> trig = std::vector<std::uint8_t>(16384);
    std::vector<std::uint8_t> widths = std::vector<std::uint8_t>(256);
    std::array<field::SpriteResource, 1> regions;
    field::SpriteEnvironment environment{1, 0, 19, 31, 0x80009900};
    Fixture() : regions{{{resource, data}}} {
        for (std::size_t i = 0; i < incoming.bytes.size(); ++i)
            incoming.bytes[i] = static_cast<std::uint8_t>(17 * i + 29);
        put(incoming.bytes, 0x82, 4096, 2);
        put(data, 4, 0x20);
        put(data, 8, 0x10);
        put(data, 12, 0x1f0);
        put(data, 0x10, 0x403, 2);
        put(data, 0x20, 1, 2);
        put(data, 0x22, 0x20, 2);
        put(data, 0x40, 2, 2);
        put(data, 0x42, 0x7e, 2);
        for (std::size_t i = 0; i < 5; ++i)
            put(data, 0x44 + i * 2, static_cast<std::uint32_t>(0xa0 - (0x44 + i * 2)), 2);
        put(data, 0xa0, 11, 2);
        data[0xc0] = 0x80;
        for (std::size_t i = 0; i < 4096; ++i)
            put(trig, i * 4 + 2, 4096, 2);
    }
    field::SpriteSources sources() { return {regions, {}, trig, widths, {}}; }
    field::SpriteConstruction construct() {
        return field::construct_sprite(
            incoming, resource, {100, -1, 300, -4}, environment, sources(),
            [](std::uint32_t size, std::uint32_t mode) {
                check(size == 48 && mode == 0, "Part allocation boundary");
                return field::SpriteAllocation{0x80002000, std::vector<std::uint8_t>(size, 0xa7)};
            });
    }
};
void connected_creation_and_binding() {
    Fixture fixture;
    std::vector<std::uint32_t> allocations;
    std::vector<std::string> stages;
    auto result = field::create_sprite(
        Fixture::resource, {100, -1, 300, -4, 12345}, fixture.environment, fixture.sources(),
        [&](std::uint32_t size, std::uint32_t mode) {
            check(mode == 0, "Original allocator mode");
            allocations.push_back(size);
            if (size == 356)
                return fixture.incoming;
            check(size == 48, "Decoded part count");
            return field::SpriteAllocation{0x80002000, std::vector<std::uint8_t>(size, 0xa7)};
        },
        [&](const field::SpriteConstructionObservation &item) {
            stages.emplace_back(item.stage);
            if (item.stage == "constructor-after-defaults")
                check(get(item.sprite, 0x1c) == 65536 && get(item.sprite, 0x82, 2) == 4096,
                      "Defaults use preserved allocation scale before renderer scale");
        });
    const auto &bytes = result.sprite.bytes;
    check(allocations == std::vector<std::uint32_t>{356, 48} && bytes.size() == 356 &&
              result.parts.bytes == std::vector<std::uint8_t>(48, 0xa7),
          "Connected wrapper has exact sprite and part ownership");
    check(get(bytes, 0x86, 2) == 356 && get(bytes, 0x20) == Fixture::address + 0xb4 &&
              get(bytes, 0x24) == Fixture::address + 0x110 &&
              get(bytes, 0x110) == Fixture::resource + 0x10 && get(bytes, 0x114, 2) == 300 &&
              get(bytes, 0x116, 2) == 65532 && get(bytes, 0x118, 2) == 100 &&
              get(bytes, 0x11a, 2) == 65535 && get(bytes, 0x60) == Fixture::resource + 0x24,
          "Constructor binds inline storage, original coordinates and byte directory extent");
    check(get(bytes, 0x64) == Fixture::resource + 0xc0 &&
              get(bytes, 0x54) == Fixture::resource + 0xa0 && ((get(bytes, 0xa8) >> 17) & 7) == 2 &&
              get(bytes, 0x9e, 2) == 1 &&
              result.environment.frame_head == fixture.environment.frame_head &&
              result.environment.binding_control == 0,
          "Header, facing and actual replay converge at the initial command target");
    check(stages == std::vector<std::string>{"constructor-after-defaults",
                                             "constructor-after-inline", "constructor-after-scale",
                                             "allocate-parts-before", "allocate-parts-after",
                                             "binding-before", "binding-after", "animation-before",
                                             "constructor-after"},
          "Constructor effects retain original call order");
    for (const auto at : {0U, 4U, 8U, 0x74U, 0x78U, 0xd4U, 0xd8U, 0xdcU})
        check(get(bytes, at) == get(fixture.incoming.bytes, at),
              "Unwritten allocation words survive");
    const auto before = bytes;
    result.environment.binding_control = 29;
    field::bind_sprite_resource({result.sprite.address, result.sprite.bytes}, Fixture::resource,
                                result.environment, fixture.sources());
    check(result.sprite.bytes == before && result.environment.binding_control == 29,
          "Same-resource binding does not clear global control");
    field::bind_sprite_resource({result.sprite.address, result.sprite.bytes}, 0, result.environment,
                                {});
    check(result.sprite.bytes == before, "Null binding requires no resource data");
}
void real_replay_and_list_effects() {
    Fixture fixture;
    auto result = fixture.construct();
    auto &bytes = result.sprite.bytes;
    field::SpriteWindow sprite{result.sprite.address, bytes};
    put(bytes, 0x40, 0x80000);
    put(bytes, 0x9e, 9, 2);
    fixture.data[0xc0] = 0xa0;
    fixture.data[0xc1] = 99;
    fixture.data[0xc2] = 0x13;
    fixture.data[0xc3] = 0x05;
    fixture.data[0xc4] = 0x80;
    fixture.widths[0xa0] = 2;
    field::replay_sprite_commands(sprite, Fixture::resource + 0xc4, 2, result.environment,
                                  fixture.sources());
    check(get(bytes, 0x34, 2) == 12 && get(bytes, 0x9e, 2) == 19 &&
              get(bytes, 0x64) == Fixture::resource + 0xc4 &&
              result.environment.frame_head == Fixture::address &&
              get(bytes, 0xec) == fixture.environment.frame_head,
          "Recovered replay skips A0, looks up a frame, advances and prepends once");
    put(bytes, 0x64, Fixture::resource + 0xc0);
    put(bytes, 0xa8, 63U << 22);
    put(bytes, 0x9e, 0xfff8, 2);
    fixture.data[0xc0] = 0x30;
    fixture.data[0xc1] = 0x3f;
    field::replay_sprite_commands(sprite, Fixture::resource + 0xc2, 63, result.environment,
                                  fixture.sources());
    check(get(bytes, 0x9e, 2) == 9 && ((get(bytes, 0xa8) >> 22) & 63) == 63,
          "Replay wraps the timer and saturates wrapped ordinal to 63");
    put(bytes, 0x64, Fixture::resource + 0x1a0);
    put(bytes, 0x8c, 1, 1);
    fixture.data[0x1a0] = 0xe2;
    fixture.data[0x1a1] = 3;
    fixture.data[0x1a2] = 0;
    field::replay_sprite_commands(sprite, Fixture::resource + 0x1a3, 63, result.environment,
                                  fixture.sources());
    check(bytes[0x8c] == 0xa3 && get(bytes, 0x32, 2) == 0x0101,
          "Relative replay call reloads a cursor that aliases its own storage");
    put(bytes, 0x64, Fixture::resource + 0x1a0);
    put(bytes, 0x8c, 0xd9, 1);
    field::replay_sprite_commands(sprite, Fixture::resource + 0x1a6, 63, result.environment,
                                  fixture.sources());
    check(get(bytes, 0x64) == Fixture::resource + 0x1a6,
          "Relative replay call reloads a command pointer aliased by its return stack");
}
void matrix_storage_and_explicit_failures() {
    Fixture fixture;
    auto result = fixture.construct();
    auto &bytes = result.sprite.bytes;
    field::SpriteWindow sprite{result.sprite.address, bytes};
    const auto translation = std::vector<std::uint8_t>(bytes.begin() + 0xd4, bytes.begin() + 0xe0);
    put(bytes, 0xba, 4096, 2);
    put(bytes, 0xbc, 8192, 2);
    put(bytes, 0xbe, static_cast<std::uint16_t>(-4096), 2);
    field::update_sprite_matrix(sprite, fixture.sources());
    check(get(bytes, 0xc0, 2) == 4096 && get(bytes, 0xc8, 2) == 8192 &&
              get(bytes, 0xd0) == 0xfffff000 &&
              std::equal(translation.begin(), translation.end(), bytes.begin() + 0xd4),
          "Matrix row scale stores final word including padding and preserves translation");
    put(bytes, 0x3a, 4097, 2);
    field::update_sprite_matrix(sprite, fixture.sources());
    check(get(bytes, 0xc0, 2) == 2048 && get(bytes, 0xc8, 2) == 4096 &&
              get(bytes, 0xd0) == 0xfffff800,
          "Extra unsigned sprite scale halves before column scale");
    put(bytes, 0x64, Fixture::resource + 0xc0);
    fixture.data[0xc0] = 0x40;
    rejects([&] {
        field::replay_sprite_commands(sprite, 0, 0, result.environment, fixture.sources());
    });
    auto sources = fixture.sources();
    sources.incoming_replay_duration = -2;
    put(bytes, 0x64, Fixture::resource + 0xc0);
    put(bytes, 0xa8, 0);
    put(bytes, 0x9e, 1, 2);
    field::replay_sprite_commands(sprite, Fixture::resource + 0xc1, 1, result.environment, sources);
    check(get(bytes, 0x9e, 2) == 65535, "Incoming replay duration is explicit and signed");
    put(bytes, 0x40, 1);
    rejects([&] { field::update_sprite_matrix(sprite, fixture.sources()); });
    result.environment.platform_mode = 1;
    rejects([&] {
        field::bind_sprite_resource(sprite, Fixture::resource, result.environment, sources);
    });
    rejects([&] { field::select_sprite_animation(sprite, -1, result.environment, sources); });
    auto short_allocation = fixture.incoming;
    short_allocation.bytes.resize(355);
    rejects([&] {
        static_cast<void>(field::construct_sprite(short_allocation, Fixture::resource, {},
                                                  fixture.environment, fixture.sources(), {}));
    });
    rejects([&] {
        static_cast<void>(
            field::create_sprite(Fixture::resource, {}, fixture.environment, fixture.sources(),
                                 [](auto, auto) { return field::SpriteAllocation{0, {}}; }));
    });
    rejects([&] {
        static_cast<void>(field::construct_sprite(
            fixture.incoming, Fixture::resource, {}, fixture.environment, fixture.sources(),
            [&](auto size, auto) {
                return field::SpriteAllocation{Fixture::address, std::vector<std::uint8_t>(size)};
            }));
    });
    auto bad = fixture.incoming;
    bad.address = 0xffffff00;
    rejects([&] {
        static_cast<void>(field::construct_sprite(bad, Fixture::resource, {}, fixture.environment,
                                                  fixture.sources(), {}));
    });
}

void frame_metadata_and_list_boundaries() {
    Fixture fixture;
    auto result = fixture.construct();
    auto &bytes = result.sprite.bytes;
    field::SpriteWindow sprite{result.sprite.address, bytes};
    put(bytes, 0x40, 0x20000);
    put(bytes, 0x34, 1, 2);
    std::fill(bytes.begin() + 0x124, bytes.end(), 0xaa);
    result.environment.frame_head = result.sprite.address;
    put(fixture.data, 0x10, 0x8003, 2);
    put(fixture.data, 0x12, 0x120, 2);
    fixture.data[0x130] = 1;
    const std::array<std::uint8_t, 7> metadata{0xf2, 0xfd, 0xff, 16, 0, 1, 2};
    std::copy(metadata.begin(), metadata.end(), fixture.data.begin() + 0x136);
    field::schedule_sprite_frame(sprite, 2, result.environment, fixture.sources());
    check(get(bytes, 0x134, 2) == 0xfffd && get(bytes, 0x13a, 2) == 256 &&
              get(bytes, 0x136) == 0xaaaaaaaa && get(bytes, 0x34, 2) == 2 &&
              result.environment.frame_head == sprite.address,
          "Compact previous-frame metadata changes only original coordinate and scale fields");
    put(bytes, 0x34, 1, 2);
    put(bytes, 0xe8, 0);
    rejects(
        [&] { field::schedule_sprite_frame(sprite, 2, result.environment, fixture.sources()); });
    put(bytes, 0x40, 0xa0000);
    std::vector<std::uint8_t> node(256, 0x73);
    constexpr std::uint32_t other = 0x80003000;
    put(node, 0x20, other + 0xb4);
    put(node, 0xec, sprite.address);
    const std::array<field::SpriteResource, 1> nodes{{{other, node}}};
    auto sources = fixture.sources();
    sources.frame_list = nodes;
    result.environment.frame_head = other;
    field::schedule_sprite_frame(sprite, 19, result.environment, sources);
    check(get(bytes, 0x34, 2) == 19 && result.environment.frame_head == other,
          "Existing frame queue position follows supplied other-node bytes");
    put(node, 0xec, other);
    rejects([&] { field::schedule_sprite_frame(sprite, 20, result.environment, sources); });
    put(node, 0xec, 0);
    field::schedule_sprite_frame(sprite, 21, result.environment, sources);
    check(result.environment.frame_head == sprite.address && get(bytes, 0xec) == other,
          "Stale queued flag reinserts the sprite when the source list does not contain it");
}

// Binary transport for independent original-capture comparisons. It only moves
// supplied bytes; all candidate effects execute in the production C++ module.
struct Transport {
    std::vector<std::uint8_t> data;
    std::size_t cursor = 0;
    std::uint32_t word() {
        const auto value = get(data, cursor);
        cursor += 4;
        return value;
    }
    std::vector<std::uint8_t> blob() {
        const auto size = word();
        check(size <= data.size() - std::min(cursor, data.size()), "Truncated sprite transport");
        const auto first = data.begin() + static_cast<std::ptrdiff_t>(cursor);
        cursor += size;
        return {first, first + size};
    }
    field::SpriteAllocation block() {
        const auto address = word();
        return {address, blob()};
    }
    field::SpriteEnvironment environment() {
        field::SpriteEnvironment result;
        result.rate_control = std::bit_cast<std::int32_t>(word());
        const auto platform = word();
        result.variant = word();
        const auto binding = word();
        result.frame_head = word();
        check(platform <= 255 && binding <= 255, "Invalid sprite transport byte");
        result.platform_mode = static_cast<std::uint8_t>(platform);
        result.binding_control = static_cast<std::uint8_t>(binding);
        return result;
    }
    void word(std::uint32_t value) {
        const auto at = data.size();
        data.resize(at + 4);
        put(data, at, value);
    }
    void blob(std::span<const std::uint8_t> value) {
        check(value.size() <= std::numeric_limits<std::uint32_t>::max(),
              "Oversize sprite transport");
        word(static_cast<std::uint32_t>(value.size()));
        data.insert(data.end(), value.begin(), value.end());
    }
    void block(const field::SpriteAllocation &value) {
        word(value.address);
        blob(value.bytes);
    }
    void environment(const field::SpriteEnvironment &value) {
        word(static_cast<std::uint32_t>(value.rate_control));
        word(value.platform_mode);
        word(value.variant);
        word(value.binding_control);
        word(value.frame_head);
    }
};
void original_transport(const char *input_path, const char *output_path) {
    std::ifstream input(input_path, std::ios::binary);
    check(input.is_open(), "Cannot open sprite input transport");
    Transport incoming{{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()}};
    Transport output, observations;
    const auto operation = incoming.word();
    const auto argument = incoming.word();
    std::array<std::int16_t, 5> parameters;
    for (auto &parameter : parameters)
        parameter = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(incoming.word()));
    auto environment = incoming.environment();
    auto sprite = incoming.block();
    auto parts = incoming.block();
    const auto target_step = incoming.word();
    std::vector<field::SpriteAllocation> resource_storage, list_storage;
    const auto read_blocks = [&](auto &blocks) {
        const auto count = incoming.word();
        check(count <= 4096, "Excess sprite source regions");
        for (std::uint32_t i = 0; i < count; ++i)
            blocks.push_back(incoming.block());
    };
    read_blocks(resource_storage);
    read_blocks(list_storage);
    std::vector<field::SpriteResource> resources, list;
    for (const auto &block : resource_storage)
        resources.push_back({block.address, block.bytes});
    for (const auto &block : list_storage)
        list.push_back({block.address, block.bytes});
    const auto trig = incoming.blob();
    const auto widths = incoming.blob();
    std::optional<std::int32_t> duration;
    if (incoming.word())
        duration = std::bit_cast<std::int32_t>(incoming.word());
    check(incoming.cursor == incoming.data.size(), "Trailing sprite input transport");
    field::SpriteSources sources{resources, list, trig, widths, duration};
    std::vector<std::uint32_t> allocations;
    std::uint32_t stage_count = 0;
    const auto allocate = [&](std::uint32_t size, std::uint32_t mode) {
        check(mode == 0, "Unexpected original allocation mode");
        allocations.push_back(size);
        const auto &block = operation == 2 && allocations.size() == 1 ? sprite : parts;
        check(block.bytes.size() == size, "Original allocation size differs");
        return block;
    };
    const auto observe = [&](const field::SpriteConstructionObservation &item) {
        ++stage_count;
        observations.blob(
            {reinterpret_cast<const std::uint8_t *>(item.stage.data()), item.stage.size()});
        observations.blob(item.sprite);
        observations.environment(item.environment);
        observations.word(item.allocation_bytes.has_value());
        if (item.allocation_bytes)
            observations.word(*item.allocation_bytes);
        observations.word(item.allocation != nullptr);
        if (item.allocation)
            observations.block(*item.allocation);
    };
    field::SpriteWindow window{sprite.address, sprite.bytes};
    if (operation == 0 || operation == 2) {
        auto result = operation == 0
                          ? field::construct_sprite(
                                std::move(sprite), argument,
                                {parameters[0], parameters[1], parameters[2], parameters[3]},
                                environment, sources, allocate, observe)
                          : field::create_sprite(argument, parameters, environment, sources,
                                                 allocate, observe);
        sprite = std::move(result.sprite);
        parts = std::move(result.parts);
        environment = result.environment;
    } else if (operation == 1) {
        field::bind_sprite_resource(window, argument, environment, sources);
    } else if (operation == 3) {
        field::replay_sprite_commands(window, argument, target_step, environment, sources);
    } else if (operation == 4) {
        field::select_sprite_orientation(window, parameters[0], environment, sources);
    } else if (operation == 5) {
        field::update_sprite_matrix(window, sources);
    } else if (operation == 6) {
        field::select_sprite_animation(window, std::bit_cast<std::int32_t>(argument), environment,
                                       sources);
    } else if (operation == 7) {
        field::schedule_sprite_frame(window, argument, environment, sources);
    } else {
        throw std::runtime_error("Unknown original sprite operation");
    }
    output.environment(environment);
    output.block(sprite);
    output.block(parts);
    output.word(static_cast<std::uint32_t>(allocations.size()));
    for (const auto size : allocations)
        output.word(size);
    output.word(stage_count);
    output.data.insert(output.data.end(), observations.data.begin(), observations.data.end());
    std::ofstream stream(output_path, std::ios::binary);
    stream.write(reinterpret_cast<const char *>(output.data.data()),
                 static_cast<std::streamsize>(output.data.size()));
    check(static_cast<bool>(stream), "Cannot write sprite output transport");
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--original") {
            original_transport(argv[2], argv[3]);
            return 0;
        }
        check(argc == 1, "Usage: test-field-sprite [--original INPUT OUTPUT]");
        connected_creation_and_binding();
        real_replay_and_list_effects();
        matrix_storage_and_explicit_failures();
        frame_metadata_and_list_boundaries();
        std::cout << "Field sprite reconstruction passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
