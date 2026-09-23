#include "xem/reconstruction/field_sprite.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"

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
struct HostExecutionLimit {};
template <typename Function> void exhausts_host_limit(Function &&function) {
    try {
        function();
    } catch (const HostExecutionLimit &) {
        return;
    }
    throw std::runtime_error("Host execution limit was not reached");
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
        field::SpriteConstruction result;
        field::construct_sprite(result, incoming, resource, {100, -1, 300, -4}, environment,
                                sources(), [](std::uint32_t size, std::uint32_t mode) {
                                    check(size == 48 && mode == 0, "Part allocation boundary");
                                    return field::SpriteAllocation{
                                        0x80002000, std::vector<std::uint8_t>(size, 0xa7)};
                                });
        return result;
    }
};
void connected_creation_and_binding() {
    Fixture fixture;
    std::vector<std::uint32_t> allocations;
    std::vector<std::string> stages;
    field::SpriteConstruction result;
    field::create_sprite(
        result, Fixture::resource, {100, -1, 300, -4, 12345}, fixture.environment,
        fixture.sources(),
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

void interrupted_constructor_retains_ownership() {
    Fixture fixture;
    // The animation directory points outside the qualified source after the
    // sprite and its part block have already been allocated and bound.
    put(fixture.data, 0x22, 0x1000, 2);
    field::SpriteConstruction result;
    rejects([&] {
        field::create_sprite(result, Fixture::resource, {}, fixture.environment, fixture.sources(),
                             [&](std::uint32_t size, std::uint32_t mode) {
                                 check(mode == 0, "Interrupted constructor allocator mode");
                                 if (size == 356)
                                     return fixture.incoming;
                                 check(size == 48, "Interrupted constructor part extent");
                                 return field::SpriteAllocation{
                                     0x80002000, std::vector<std::uint8_t>(size, 0xa7)};
                             });
    });
    check(result.sprite.address == Fixture::address && result.sprite.bytes.size() == 356 &&
              result.parts.address == 0x80002000 && result.parts.bytes.size() == 48 &&
              get(result.sprite.bytes, 0xe0) == result.parts.address &&
              get(result.sprite.bytes, 0x86, 2) == 356 && result.environment.binding_control == 0,
          "Constructor failure retains both owned allocations and committed binding effects");
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
        field::SpriteConstruction partial;
        field::construct_sprite(partial, short_allocation, Fixture::resource, {},
                                fixture.environment, fixture.sources(), {});
    });
    rejects([&] {
        field::SpriteConstruction partial;
        field::create_sprite(partial, Fixture::resource, {}, fixture.environment, fixture.sources(),
                             [](auto, auto) { return field::SpriteAllocation{0, {}}; });
    });
    rejects([&] {
        field::SpriteConstruction partial;
        field::construct_sprite(partial, fixture.incoming, Fixture::resource, {},
                                fixture.environment, fixture.sources(), [&](auto size, auto) {
                                    return field::SpriteAllocation{Fixture::address,
                                                                   std::vector<std::uint8_t>(size)};
                                });
    });
    auto bad = fixture.incoming;
    bad.address = 0xffffff00;
    rejects([&] {
        field::SpriteConstruction partial;
        field::construct_sprite(partial, bad, Fixture::resource, {}, fixture.environment,
                                fixture.sources(), {});
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

void ordinary_timer_and_checkpoint() {
    Fixture fixture;
    auto result = fixture.construct();
    auto &bytes = result.sprite.bytes;
    field::SpriteWindow sprite{result.sprite.address, bytes};
    put(bytes, 0x40, 0x80000);
    put(bytes, 0xac, 0x10000); // 2x ordinary command duration; replay is unscaled.
    put(bytes, 0x9e, 1, 2);
    result.environment.rate_control = 0;
    fixture.data[0xc0] = 0xb3;
    fixture.data[0xc1] = 62;
    fixture.widths[0xb3] = 2;
    fixture.data[0xc2] = 0xe1;
    put(fixture.data, 0xc3, 4, 2);
    fixture.data[0xc6] = 0x13;
    put(fixture.data, 0xa0 + 63 * 2, 22, 2);
    check(field::advance_sprite_timer(sprite, result.environment, fixture.sources()) == 3 &&
              get(bytes, 0x9e, 2) == 8 && get(bytes, 0x34, 2) == 22 &&
              get(bytes, 0x64) == Fixture::resource + 0xc7,
          "Ordinary timer dispatches B3/E1/timed lookup with scaled duration");
    auto before = bytes;
    result.environment.rate_control = -1;
    check(field::advance_sprite_timer(sprite, result.environment, {}) == 0 && bytes == before,
          "Disabled timer requires no resources");
    result.environment.rate_control = 0;
    put(bytes, 0x9e, 0, 2);
    before = bytes;
    check(field::advance_sprite_timer(sprite, result.environment, {}) == 0 && bytes == before,
          "Zero timer does not dispatch commands");
    put(bytes, 0x9e, 0xffff, 2);
    check(field::advance_sprite_timer(sprite, result.environment, {}) == 0 &&
              get(bytes, 0x9e, 2) == 0xfffe,
          "Negative signed timer decrements its original halfword");
    put(bytes, 0x9e, 1, 2);
    put(bytes, 0x64, Fixture::resource + 0xc0);
    fixture.data[0xc0] = 0xa2;
    rejects([&] {
        static_cast<void>(
            field::advance_sprite_timer(sprite, result.environment, fixture.sources()));
    });
    fixture.data[0xc0] = 0xe1;
    put(fixture.data, 0xc1, 0, 2);
    auto bounded = fixture.sources();
    std::uint32_t remaining = 8;
    bounded.observe_execution = [&](field::SpriteExecutionPoint) {
        if (remaining == 0)
            throw HostExecutionLimit{};
        --remaining;
    };
    exhausts_host_limit([&] {
        static_cast<void>(field::execute_sprite_commands(sprite, result.environment, bounded));
    });

    fixture.data[0xc0] = 0x40;
    rejects([&] {
        static_cast<void>(
            field::execute_sprite_commands(sprite, result.environment, fixture.sources()));
    });
    put(bytes, 0x64, Fixture::resource + 0xc0);
    put(bytes, 0xa8, 63U << 22);
    auto context = fixture.sources();
    context.incoming_replay_duration = -2;
    check(field::execute_sprite_commands(sprite, result.environment, context) == 1 &&
              get(bytes, 0x9e, 2) == 0xfffc && ((get(bytes, 0xa8) >> 22) & 63) == 63,
          "Ordinary incoming duration multiplies signed values and saturates the six-bit step");
    put(bytes, 0x64, Fixture::resource + 0xc0);
    put(bytes, 0x9e, 0, 2);
    context.incoming_replay_duration = 0;
    check(field::execute_sprite_commands(sprite, result.environment, context) == 1 &&
              get(bytes, 0x9e, 2) == 1,
          "Zero ordinary scaled delay becomes one");

    result = fixture.construct();
    std::array<std::uint8_t, 48> checkpoint{};
    for (const auto offset : {0x24U, 0x26U, 0x28U, 0x2aU, 0x2cU})
        put(checkpoint, offset, 4096, 2);
    put(checkpoint, 0x18, 1, 2);
    put(checkpoint, 0, 0xffffff00);
    put(checkpoint, 4, 0x12345678);
    put(checkpoint, 8, 0xdeadbeef);
    put(checkpoint, 0x1c, 0x77889900);
    put(checkpoint, 0x20, 0x01020304);
    fixture.data[0xc0] = 0x33;
    result.environment.rate_control = 5;
    check(
        field::restore_sprite_checkpoint({result.sprite.address, result.sprite.bytes}, checkpoint,
                                         result.environment, fixture.sources()) == 1 &&
            result.environment.rate_control == 5 && get(result.sprite.bytes, 0x9e, 2) == 4 &&
            std::equal(checkpoint.begin(), checkpoint.begin() + 12, result.sprite.bytes.begin()) &&
            get(result.sprite.bytes, 0xf4) == 0x77889900 &&
            get(result.sprite.bytes, 0xf8) == 0x01020304,
        "Checkpoint uses temporary zero rate, ordinary VM and saved position/sequencer words");
    put(checkpoint, 0x18, 0, 2);
    fixture.data[0xc0] = 0x80;
    check(field::restore_sprite_checkpoint({result.sprite.address, result.sprite.bytes}, checkpoint,
                                           result.environment, fixture.sources()) == 0,
          "Zero checkpoint step never executes unsupported terminator");
    put(checkpoint, 0x18, 64, 2);
    rejects([&] {
        static_cast<void>(
            field::restore_sprite_checkpoint({result.sprite.address, result.sprite.bytes},
                                             checkpoint, result.environment, fixture.sources()));
    });
}

void host_execution_limits() {
    Fixture fixture;
    auto result = fixture.construct();
    auto &bytes = result.sprite.bytes;
    field::SpriteWindow sprite{result.sprite.address, bytes};
    fixture.data[0xc0] = 0xc6;
    fixture.data[0xc1] = 73;
    fixture.data[0xc2] = 0xe1;
    put(fixture.data, 0xc3, 0, 2);
    fixture.widths[0xc6] = 2;
    put(bytes, 0x9e, 0, 2);
    auto sources = fixture.sources();
    std::uint32_t steps = 0;
    sources.observe_execution = [&](field::SpriteExecutionPoint point) {
        check(point.operation == "sprite_command" && point.machine_address == 0x800248d4 &&
                  point.command_pc == Fixture::resource + (steps == 0 ? 0xc0U : 0xc2U),
              "Host receives distinct machine and command locations");
        if (++steps == 4)
            throw HostExecutionLimit{};
    };
    exhausts_host_limit([&] {
        static_cast<void>(field::execute_sprite_commands(sprite, result.environment, sources));
    });
    check(steps == 4 && get(bytes, 0x100, 2) == 73 &&
              get(bytes, 0x64) == Fixture::resource + 0xc2 && get(bytes, 0x9e, 2) == 0,
          "Host interruption preserves C6 and PC stores before a nonterminating E1");

    fixture.data[0xc0] = 0xe2;
    put(fixture.data, 0xc1, 0, 2);
    put(bytes, 0x64, Fixture::resource + 0xc0);
    put(bytes, 0x8c, 16, 1);
    steps = 0;
    sources.observe_execution = [&](field::SpriteExecutionPoint point) {
        check(point.operation == "sprite_facing_replay" && point.machine_address == 0x80022660 &&
                  point.command_pc == Fixture::resource + 0xc0,
              "Facing replay exposes its own operation and original command pointer");
        if (++steps == 3)
            throw HostExecutionLimit{};
    };
    exhausts_host_limit([&] {
        field::replay_sprite_commands(sprite, Fixture::resource + 0xc4, 0, result.environment,
                                      sources);
    });
    check(get(bytes, 0x8c, 1) == 10 && get(bytes, 0x64) == Fixture::resource + 0xc0,
          "Facing interruption retains both completed return-stack pushes");

    // A supported, finite original loop can exceed any particular host budget.
    result.environment.rate_control = 4999;
    put(bytes, 0x9e, 0, 2);
    check(field::advance_sprite_timer(sprite, result.environment, {}) == 0,
          "Game timer retains its source loop count without a built-in host ceiling");
}

void ordinary_motion_commands() {
    Fixture fixture;
    auto result = fixture.construct();
    auto &bytes = result.sprite.bytes;
    field::SpriteWindow sprite{result.sprite.address, bytes};
    fixture.data[0xc0] = 0xa0;
    fixture.data[0xc1] = 2;
    fixture.data[0xc2] = 0xa1;
    fixture.data[0xc3] = 0xfd;
    fixture.data[0xc4] = 0x33;
    fixture.widths[0xa0] = 2;
    fixture.widths[0xa1] = 2;
    put(bytes, 0x9e, 1, 2);
    result.environment.rate_control = 0;
    check(field::advance_sprite_timer(sprite, result.environment, fixture.sources()) == 3 &&
              get(bytes, 0x18) == 8192 && get(bytes, 0xc) == 8192 && get(bytes, 0x14) == 0 &&
              get(bytes, 0x10) == static_cast<std::uint32_t>(-12288) &&
              get(bytes, 0x64) == Fixture::resource + 0xc5 && get(bytes, 0x9e, 2) == 4,
          "Ordinary A0/A1 share motion effects and commit command PCs before the timed frame");
    put(bytes, 0x7c, Fixture::resource + 0x1e0);
    put(fixture.data, 0x1e0, static_cast<std::uint32_t>(-7680));
    put(bytes, 0x64, Fixture::resource + 0xc2);
    put(bytes, 0x9e, 0, 2);
    check(field::execute_sprite_commands(sprite, result.environment, fixture.sources()) == 2 &&
              get(bytes, 0x10) == static_cast<std::uint32_t>(-7680),
          "A1 consumes an address-qualified external reference word");
    put(bytes, 0x7c, sprite.address + 0x10);
    put(bytes, 0x10, 123);
    put(bytes, 0x64, Fixture::resource + 0xc2);
    put(bytes, 0x9e, 0, 2);
    const std::span<const std::uint8_t> data = fixture.data;
    const std::array<field::SpriteResource, 2> without_operand{
        {{Fixture::resource, data.first(0xc3)}, {Fixture::resource + 0xc4, data.subspan(0xc4)}}};
    auto sources = fixture.sources();
    sources.resources = without_operand;
    check(field::execute_sprite_commands(sprite, result.environment, sources) == 2 &&
              get(bytes, 0x10) == 123,
          "A1 reads an aliased active-sprite reference and bypasses the absent operand");
}

void negative_animation_and_checkpoint_policy() {
    Fixture fixture;
    auto result = fixture.construct();
    auto alternate = fixture.data;
    constexpr std::uint32_t alternate_address = Fixture::resource + 0x1000;
    // The complemented index 1 selects directory+24 = header+44.
    put(alternate, 0x20, 2, 2);
    put(alternate, 0x24, 0x24, 2);
    put(alternate, 0x44, 2, 2);
    put(alternate, 0x46, 0x78, 2);
    for (std::size_t i = 0; i < 5; ++i)
        put(alternate, 0x48 + i * 2, static_cast<std::uint32_t>(0xa0 - (0x48 + i * 2)), 2);
    std::array<field::SpriteResource, 2> resources{
        {{Fixture::resource, fixture.data}, {alternate_address, alternate}}};
    auto sources = fixture.sources();
    sources.resources = resources;
    put(result.sprite.bytes, 0x4c, 0);
    field::select_sprite_animation({result.sprite.address, result.sprite.bytes}, -1,
                                   result.environment, sources);
    check(get(result.sprite.bytes, 0x44) == Fixture::resource &&
              get(result.sprite.bytes, 0x58) == Fixture::resource + 0x40 &&
              get(result.sprite.bytes, 0xaf, 1) == 255,
          "Null alternate resource preserves the existing original binding");
    put(result.sprite.bytes, 0x4c, alternate_address);
    field::select_sprite_animation({result.sprite.address, result.sprite.bytes}, -2,
                                   result.environment, sources);
    check(get(result.sprite.bytes, 0x44) == alternate_address &&
              get(result.sprite.bytes, 0x58) == alternate_address + 0x44 &&
              get(result.sprite.bytes, 0xaf, 1) == 254,
          "Negative animation binds pointer+4c and complements the full index");

    std::array<std::uint8_t, 312> actor{};
    std::array<std::uint8_t, 48> checkpoint{};
    put(actor, 4, 0x1000000);
    put(actor, 0xea, 0xfffe, 2);
    put(actor, 0x134, 0x80);
    put(actor, 0x12c, 0x1000);
    auto decision = field::select_sprite_checkpoint(actor, checkpoint, {0, 1, 2}, {0, 1, 2}, 0);
    check(decision.decision == field::SpriteRestoreDecision::actor_layer_flag &&
              get(decision.checkpoint, 0x14, 2) == 65534 && decision.record_bytes == 400,
          "Skipped actor still updates signed animation and consumes both extensions");
    put(actor, 4, 0);
    put(actor, 0, 0x600);
    decision = field::select_sprite_checkpoint(actor, checkpoint, {256, 1, 2}, {0, 1, 2}, 1);
    check(decision.decision == field::SpriteRestoreDecision::party_mode_change,
          "Policy compares saved mode words with current mode bytes");
    put(actor, 0x124, 65535, 2);
    put(checkpoint, 0x14, 9, 2);
    decision = field::select_sprite_checkpoint(actor, checkpoint, {0, 1, 2}, {0, 1, 2}, 1);
    check(decision.decision == field::SpriteRestoreDecision::restore &&
              get(decision.checkpoint, 0x14, 2) == 9,
          "Actor sentinel preserves saved animation");
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
void child_motion_and_parent_position_commands() {
    Fixture fixture;
    constexpr std::uint32_t parent_address = 0x80002000;
    std::array<std::uint8_t, 356> parent{};
    std::array<std::uint8_t, 356> child{};
    put(parent, 0, 0x0007ffff);
    put(parent, 4, 0xfffd1234);
    put(parent, 8, 0x80000011);
    put(parent, 0x32, 0x1234, 2);
    put(child, 0x20, Fixture::address + 0xb4);
    put(child, 0x3c, 2);
    put(child, 0x64, Fixture::resource + 0xc0);
    put(child, 0x70, parent_address);
    put(child, 0x82, 4096, 2);
    put(child, 0xac, 0x8000);
    fixture.data[0xc0] = 0xa3;
    fixture.data[0xc1] = 0xfe;
    fixture.data[0xc2] = 0xbc;
    fixture.data[0xc3] = 0x96;
    fixture.data[0xc4] = 0x94;
    fixture.data[0xc5] = 0x97;
    fixture.widths[0xa3] = 2;
    fixture.widths[0xbc] = 2;
    fixture.widths[0x94] = 1;
    auto sources = fixture.sources();
    sources.factory_sprite = field::SpriteResource{parent_address, parent};
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
        check(false, "Next child command must stop explicitly");
    } catch (const field::UnrecoveredSpriteCommand &error) {
        check(error.opcode == 0x97 && error.command_pc == Fixture::resource + 0xc5,
              "Three recovered commands advance to the exact next dependency");
    }
    check(get(child, 0x1c) == 0xffffc000 && get(child, 0) == 0x00070000 &&
              get(child, 4) == 0xfffd0000 && get(child, 8) == 0x80000000 &&
              get(child, 0xb6, 2) == 0x1234 && get(child, 0x3c) == 0x10000002,
          "A3 signed scaling, BC parent integer coordinates and 94 facing copy");

    put(child, 0x64, Fixture::resource + 0xc0);
    put(child, 0xa8, 1);
    put(child, 0x7c, Fixture::address + 0xf4);
    put(child, 0xf8, 0x89abcdef);
    put(child, 0xac, 0);
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
    } catch (const field::SpriteError &) {
    }
    check(get(child, 0x1c) == 0x89abcdef,
          "A3 nonzero sequencer reference bypasses arithmetic and zero divisor");
    put(child, 0x64, Fixture::resource + 0xc2);
    put(child, 0, 0);
    put(child, 4, 0);
    put(child, 8, 0);
    sources.factory_sprite.reset();
    const std::array<field::SpriteResource, 1> prior{{{parent_address, parent}}};
    sources.frame_list = prior;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
    } catch (const field::UnrecoveredSpriteCommand &) {
    }
    check(get(child, 0) == 0x00070000 && get(child, 4) == 0xfffd0000 && get(child, 8) == 0x80000000,
          "BC resolves an earlier factory's owned parent sprite");
    put(child, 0x64, Fixture::resource + 0xc0);
    put(child, 0xa8, 0);
    rejects([&] {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
    });
    check(get(child, 0x64) == Fixture::resource + 0xc0,
          "A3 zero divisor stops before the original PC store");
    put(child, 0x64, Fixture::resource + 0xc2);
    fixture.data[0xc3] = 0x95;
    rejects([&] {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
    });
    check(get(child, 0x64) == Fixture::resource + 0xc2,
          "Unrecovered BC selector does not silently advance");

    field::SpriteTaskState tasks{};
    tasks.nodes.push_back({0x80003000, std::vector<std::uint8_t>(320)});
    sources.tasks = &tasks;
    put(child, 0x6c, 0x80003000);
    put(child, 0, 0x1234abcd);
    put(child, 4, 0xfffe0001);
    put(child, 8, 0x7000abcd);
    fixture.data[0xc3] = 0xa4;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
    } catch (const field::UnrecoveredSpriteCommand &) {
    }
    check(get(tasks.nodes[0].bytes, 20) == 0x40000000 && get(child, 0) == 0x12340000 &&
              get(child, 4) == 0xfffe0000 && get(child, 8) == 0x70000000,
          "BC A4 protects its owned task and truncates its own coordinates");
    put(child, 0x64, Fixture::resource + 0xc2);
    fixture.data[0xc3] = 0xa5;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::address, child},
                                                         fixture.environment, sources));
    } catch (const field::UnrecoveredSpriteCommand &) {
    }
    check(get(tasks.nodes[0].bytes, 20) == 0,
          "BC A5 clears the same protection bit without destroying the task");
}

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
    std::uint32_t commands = 0;
    field::SpriteRestoreDecision decision = field::SpriteRestoreDecision::restore;
    if (operation == 0 || operation == 2) {
        field::SpriteConstruction result;
        if (operation == 0)
            field::construct_sprite(result, std::move(sprite), argument,
                                    {parameters[0], parameters[1], parameters[2], parameters[3]},
                                    environment, sources, allocate, observe);
        else
            field::create_sprite(result, argument, parameters, environment, sources, allocate,
                                 observe);
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
    } else if (operation == 8) {
        commands = field::execute_sprite_commands(window, environment, sources);
    } else if (operation == 9) {
        commands = field::advance_sprite_timer(window, environment, sources);
    } else if (operation == 10) {
        commands = field::restore_sprite_checkpoint(window, parts.bytes, environment, sources);
    } else if (operation == 11) {
        check(resources.size() == 1 && resources[0].bytes.size() == 15, "Invalid policy modes");
        const auto modes = resources[0].bytes;
        const auto selected = field::select_sprite_checkpoint(
            sprite.bytes, parts.bytes, {get(modes, 0), get(modes, 4), get(modes, 8)},
            {modes[12], modes[13], modes[14]}, argument);
        parts.bytes.assign(selected.checkpoint.begin(), selected.checkpoint.end());
        commands = selected.record_bytes;
        decision = selected.decision;
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
    if (operation >= 8)
        output.word(commands);
    if (operation == 11)
        output.word(static_cast<std::uint32_t>(decision));
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
        interrupted_constructor_retains_ownership();
        real_replay_and_list_effects();
        matrix_storage_and_explicit_failures();
        frame_metadata_and_list_boundaries();
        ordinary_timer_and_checkpoint();
        host_execution_limits();
        ordinary_motion_commands();
        child_motion_and_parent_position_commands();
        negative_animation_and_checkpoint_policy();
        std::cout << "Field sprite reconstruction passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
