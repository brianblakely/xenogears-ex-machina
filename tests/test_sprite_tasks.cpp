#include "xem/reconstruction/field_sprite_factory.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (i * 8);
    return value;
}

struct Fixture {
    static constexpr std::uint32_t parent_address = 0x80010000;
    static constexpr std::uint32_t child_address = 0x80020000;
    static constexpr std::uint32_t resource_address = 0x80030000;
    std::array<std::uint8_t, 356> parent{};
    std::array<std::uint8_t, 128> data{};
    std::array<std::uint8_t, 64> callbacks{};
    std::array<std::uint8_t, 256> widths{};
    std::array<std::uint8_t, 16384> trig{};
    std::array<field::SpriteResource, 2> resources;
    field::SpriteTaskState tasks{};
    field::SpriteEnvironment environment{};
    field::SpriteServices services{};
    std::uint32_t allocations{};

    Fixture() : resources{{{resource_address, data}, {0x8004fd40, callbacks}}} {
        put(parent, 0, 0x00120000);
        put(parent, 4, 0xfffd0000);
        put(parent, 8, 0x000a0000);
        put(parent, 0x10, 0xc1234567);
        put(parent, 0x20, parent_address + 0xb4);
        put(parent, 0x24, parent_address + 0x110);
        put(parent, 0x2c, 0x1000, 2);
        put(parent, 0x3c, 0x00005a19);
        put(parent, 0x40, 0x00041f00);
        put(parent, 0x44, 0x80050000);
        put(parent, 0x48, 0x80060000);
        put(parent, 0x64, resource_address + 0x60);
        put(parent, 0x6c, parent_address);
        put(parent, 0x74, 0x80070000);
        put(parent, 0x78, 0x80080000);
        put(parent, 0x7c, parent_address + 0xf4);
        put(parent, 0x82, 0x1000, 2);
        put(parent, 0xa8, 0xc0000001);
        put(parent, 0xac, 0x8044);
        put(parent, 0xaf, 0x93, 1);
        for (auto at : {0xbaU, 0xbcU, 0xbeU})
            put(parent, at, 0x1000, 2);
        put(data, 0x20, 0x0200, 2);
        put(data, 0x22, 0x1c, 2);
        put(data, 0x24, 0x0c, 2);
        data[0x60] = 0xe0;
        put(data, 0x61, 0xffbf, 2); // Operand-relative negative displacement.
        data[0x63] = 0x81;
        widths[0xe0] = 3;
        widths[0x8d] = 1;
        put(callbacks, 8, 0x80025718);
        for (std::size_t i = 0; i < 4096; ++i)
            put(trig, i * 4 + 2, 0x1000, 2);
        tasks.head = 0x80090000;
        tasks.pending_head = 0x800a0000;
        tasks.serial = 0x3ffffffe;
        tasks.primary_count = 7;
        tasks.auxiliary_count = 9;
        tasks.active_flags = 3;
        tasks.creation_flags = 7;
        tasks.allocation_mode = 2;
        services.allocate = [&](std::uint32_t size, std::uint32_t mode, std::uint32_t) {
            check(size == 320 && mode == 2, "E0 exact allocation and original mode");
            ++allocations;
            return field::SpriteAllocation{child_address, std::vector<std::uint8_t>(size, 0xa5)};
        };
    }
    field::SpriteSources sources(bool with_callbacks = true) {
        field::SpriteSources result{
            std::span(resources).first(with_callbacks ? 2 : 1), {}, trig, widths, {}};
        result.tasks = &tasks;
        result.services = &services;
        result.factory_sprite = field::SpriteResource{parent_address, parent};
        return result;
    }
    void run() {
        check(field::execute_sprite_commands({parent_address, parent}, environment, sources()) == 2,
              "Parent creates child then returns from command81 without executing child timer");
    }
};

void embedded_child_ownership() {
    Fixture fixture;
    fixture.run();
    const auto &tasks = fixture.tasks;
    check(fixture.allocations == 1 && tasks.nodes.size() == 1 &&
              tasks.nodes[0].address == Fixture::child_address &&
              tasks.nodes[0].bytes.size() == 320,
          "One allocation owns both task headers and child sprite");
    const auto bytes = std::span(tasks.nodes[0].bytes);
    const auto child = bytes.subspan(0x38);
    check(get(bytes, 0) == Fixture::parent_address && get(bytes, 0x1c) == Fixture::child_address &&
              get(bytes, 4) == Fixture::child_address + 0x38 &&
              get(bytes, 0x20) == Fixture::child_address + 0x38,
          "Task owners and both payloads refer to live embedded storage");
    check(tasks.head == Fixture::child_address &&
              tasks.pending_head == Fixture::child_address + 0x1c &&
              get(bytes, 0x18) == 0x80090000 && get(bytes, 0x34) == 0x800a0000,
          "Both lists prepend without replacing existing tails");
    check(tasks.serial == 0x40000000 && get(bytes, 0x10) == 0xbffffffe &&
              get(bytes, 0x2c) == 0xbfffffff && get(bytes, 0x14) == 0xa1234567 &&
              get(bytes, 0x30) == 0x1ffffffe,
          "Generations wrap low29 independently and preserve incoming high bits");
    check(tasks.primary_count == 8 && tasks.auxiliary_count == 10 && tasks.active_flags == 4 &&
              tasks.creation_flags == 7,
          "Task counters and active creation flag follow original registration");
    check(get(bytes, 8) == 0x80022df4 && get(bytes, 0x0c) == 0x80022eb8 &&
              get(bytes, 0x24) == 0x80025718 && get(bytes, 0x28) == 0x8001cb48,
          "Original update, destructor and auxiliary dispatch are installed");
    check(get(child, 0x20) == Fixture::child_address + 0xec &&
              get(child, 0x6c) == Fixture::child_address && get(child, 0x86, 2) == 320 &&
              get(child, 0x70) == Fixture::parent_address && get(child, 0x7c) == 0,
          "Child parts and task owner stay embedded; sequencer ownership is not duplicated");
    check(get(child, 0) == 0x00120000 && get(child, 4) == 0xfffd0000 &&
              get(child, 8) == 0x000a0000 && get(child, 0x10) == 0 &&
              get(child, 0x3c) == 0x04005a1a && get(child, 0x40) == 0x00045f00,
          "Position inheritance, header velocity reset and renderer flags");
    check(get(child, 0x58) == Fixture::resource_address + 0x20 &&
              get(child, 0x64) == Fixture::resource_address + 0x3e && get(child, 0x9e, 2) == 1 &&
              get(child, 0x8d, 1) == 0x93 && get(child, 0xc0, 2) == 0x1000 &&
              get(child, 0xc8, 2) == 0x1000 && get(child, 0xd0, 2) == 0x1000,
          "Shared header and matrix operations initialize child without executing its timer");
    check(get(child, 0x28) == 0x2da5a5a5 && get(fixture.parent, 0xb0) == 0x800 &&
              get(fixture.parent, 0x64) == Fixture::resource_address + 0x63,
          "Unwritten allocation bytes survive and parent PC advances only after E0 completes");
}

void creation_flag_restoration_and_failure() {
    Fixture suppressed;
    put(suppressed.parent, 0xb0, 0x100);
    suppressed.run();
    check(suppressed.tasks.active_flags == 3 && suppressed.tasks.creation_flags == 7 &&
              get(suppressed.tasks.nodes.front().bytes, 0x14) == 0x21234567,
          "Parent flag8 suppresses active count only while creating the child");

    Fixture missing;
    put(missing.parent, 0xb0, 0x100);
    bool failed = false;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::parent_address, missing.parent},
                                                         missing.environment,
                                                         missing.sources(false)));
    } catch (const field::SpriteInputError &) {
        failed = true;
    }
    check(failed && missing.allocations == 1 && missing.tasks.nodes.size() == 1 &&
              missing.tasks.head == Fixture::child_address && missing.tasks.primary_count == 8 &&
              missing.tasks.creation_flags == 0 &&
              get(missing.parent, 0x64) == Fixture::resource_address + 0x60,
          "Missing original dispatch bytes retain issued stores and stop before PC/flag restore");

    Fixture other_kind;
    put(other_kind.data, 0x20, 0, 2);
    failed = false;
    try {
        static_cast<void>(
            field::execute_sprite_commands({Fixture::parent_address, other_kind.parent},
                                           other_kind.environment, other_kind.sources()));
    } catch (const field::UnrecoveredSpriteBehavior &) {
        failed = true;
    }
    check(failed && other_kind.allocations == 0 && other_kind.tasks.nodes.empty() &&
              get(other_kind.parent, 0xb0) == 0x800,
          "Unsupported renderer kind remains an explicit dependency after parent marking");
}
void field_completion_callback() {
    Fixture fixture;
    std::array<std::uint8_t, 312> actor{};
    put(actor, 4, 0x80000020);
    put(fixture.parent, 0x64, Fixture::resource_address + 0x63);
    put(fixture.parent, 0x68, 0x80076a74);
    put(fixture.parent, 0x108, 19, 2);
    auto sources = fixture.sources();
    sources.field_actor = field::SpriteFieldActor{19, actor};
    check(field::execute_sprite_commands({Fixture::parent_address, fixture.parent},
                                         fixture.environment, sources) == 1 &&
              get(actor, 4) == 0x80010020 && get(fixture.parent, 0xa8) == 0xd0000001 &&
              get(fixture.parent, 0x9e, 2) == 0 &&
              get(fixture.parent, 0x64) == Fixture::resource_address + 0x63,
          "81 runs actual selected actor callback and returns with its original command PC");
    put(fixture.parent, 0xaf, 63, 1);
    put(fixture.parent, 0xa8, 0xf0000001);
    static_cast<void>(field::execute_sprite_commands({Fixture::parent_address, fixture.parent},
                                                     fixture.environment, sources));
    check(get(fixture.parent, 0xa8) == 0xc0000001,
          "81 sentinel takes80 completion behavior before callback");
    put(fixture.parent, 0xaf, 0, 1);
    put(fixture.parent, 0x108, 65535, 2);
    bool failed = false;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::parent_address, fixture.parent},
                                                         fixture.environment, sources));
    } catch (const field::SpriteInputError &) {
        failed = true;
    }
    check(failed && get(fixture.parent, 0xa8) == 0xc0000001,
          "Negative callback actor index cannot select the supplied actor or finish81");
}

void owned_task_timer_and_motion() {
    for (const bool repeat : {false, true}) {
        Fixture fixture;
        fixture.tasks.head = 0;
        fixture.tasks.pending_head = 0;
        put(fixture.parent, 0xac, repeat ? 0x8040 : 0x8000);
        fixture.data[0x3e] = 0x31;
        fixture.run();
        auto child = std::span(fixture.tasks.nodes.front().bytes).subspan(0x38);
        put(child, 0, 0xfffffff0);
        put(child, 4, 0);
        put(child, 8, 100);
        put(child, 0x0c, 49);
        put(child, 0x10, static_cast<std::uint32_t>(-17));
        put(child, 0x14, static_cast<std::uint32_t>(-1));
        put(child, 0x1c, 9);
        put(child, 0x3a, 512, 2);
        field::advance_sprite_tasks(fixture.tasks, fixture.environment, fixture.sources());
        check(get(child, 0) == (repeat ? 16U : 0U) && get(child, 4) == 0xfffffff0 &&
                  get(child, 8) == 100 &&
                  get(child, 0x10) == (repeat ? 1U : static_cast<std::uint32_t>(-8)),
              "Owned task preserves shift-before-scale, signed truncation, wrapping and gravity "
              "order");
        check(get(child, 0x9e, 2) == (repeat ? 1U : 2U) &&
                  get(child, 0x64) == Fixture::resource_address + 0x3f &&
                  fixture.tasks.current == Fixture::child_address && fixture.tasks.next == 0,
              "Task performs timer before motion and flag6 requests exactly one additional pass");
    }

    Fixture grounded;
    grounded.tasks.head = 0;
    grounded.tasks.pending_head = 0;
    grounded.data[0x3e] = 0x31;
    grounded.run();
    auto child = std::span(grounded.tasks.nodes.front().bytes).subspan(0x38);
    put(child, 0x3c, get(child, 0x3c) & ~0x4000000U);
    put(child, 0x0c, 16);
    put(child, 0x10, 32);
    const auto x = get(child, 0), y = get(child, 4);
    bool failed = false;
    try {
        field::advance_sprite_tasks(grounded.tasks, grounded.environment, grounded.sources());
    } catch (const field::UnrecoveredSpriteBehavior &) {
        failed = true;
    }
    check(failed && get(child, 0) == x + 16 && get(child, 4) == y && get(child, 0x10) == 32 &&
              get(child, 0x9e, 2) == 2,
          "Grounded task stops at field dependency after timer and planar stores");
}

void texture_page_selection() {
    const std::array<std::array<std::uint16_t, 3>, 8> cases{{{0, 0, 0},
                                                             {63, 255, 0},
                                                             {64, 256, 17},
                                                             {1023, 511, 31},
                                                             {1024, 512, 0},
                                                             {0, 768, 16},
                                                             {65535, 65535, 31},
                                                             {640, 300, 26}}};
    for (const auto &[x, y, page] : cases) {
        Fixture fixture;
        fixture.data[0x60] = 0x8d;
        fixture.data[0x61] = 0x81;
        put(fixture.parent, 0x114, x, 2);
        put(fixture.parent, 0x116, y, 2);
        fixture.environment.texture_page = 0xdeadffff;
        fixture.environment.texture_mode = 19;
        check(field::execute_sprite_commands({Fixture::parent_address, fixture.parent},
                                             fixture.environment, fixture.sources()) == 2 &&
                  fixture.environment.texture_page == page &&
                  fixture.environment.texture_mode == 1 &&
                  get(fixture.parent, 0x64) == Fixture::resource_address + 0x61,
              "8D uses original texture-page bit packing, low-five-bit mask and mode store");
    }
    Fixture fixture;
    fixture.tasks.head = 0;
    fixture.tasks.pending_head = 0;
    fixture.data[0x3e] = 0x8d;
    fixture.data[0x3f] = 0x81;
    fixture.run();
    put(fixture.parent, 0x114, 448, 2);
    put(fixture.parent, 0x116, 256, 2);
    field::advance_sprite_tasks(fixture.tasks, fixture.environment, fixture.sources());
    check(fixture.environment.texture_page == 23 && fixture.environment.texture_mode == 1,
          "Child8D reads its actual live parent descriptor after child construction");

    auto child = std::span(fixture.tasks.nodes.front().bytes).subspan(0x38);
    put(child, 0x64, Fixture::resource_address + 0x3e);
    put(child, 0x9e, 0, 2);
    auto sources = fixture.sources();
    sources.factory_sprite.reset();
    fixture.environment.texture_page = 7;
    fixture.environment.texture_mode = 13;
    bool failed = false;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::child_address + 0x38, child},
                                                         fixture.environment, sources));
    } catch (const field::SpriteInputError &) {
        failed = true;
    }
    check(failed && fixture.environment.texture_page == 7 &&
              fixture.environment.texture_mode == 13 &&
              get(child, 0x64) == Fixture::resource_address + 0x3e,
          "Absent actual parent descriptor blocks8D before globals or PC are changed");
}
} // namespace

int main() {
    try {
        embedded_child_ownership();
        creation_flag_restoration_and_failure();
        field_completion_callback();
        owned_task_timer_and_motion();
        texture_page_selection();
        std::cout << "sprite task reconstruction checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
