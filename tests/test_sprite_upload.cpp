#include "xem/reconstruction/field_sprite.hpp"

#include <iostream>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
struct Fixture {
    static constexpr std::uint32_t resource_address = 0x80010000;
    static constexpr std::uint32_t sprite_address = 0x80020000;
    std::vector<std::uint8_t> data = std::vector<std::uint8_t>(256);
    std::vector<std::uint8_t> sprite = std::vector<std::uint8_t>(356);
    std::array<std::uint8_t, 256> widths{};
    std::array<field::SpriteResource, 1> resources;
    field::SpriteUploadState state;
    field::SpriteServices services;
    field::SpriteEnvironment environment{};
    std::vector<field::SpriteImageUpload> uploads;
    std::vector<std::uint32_t> releases;
    std::vector<field::SpriteAllocation> released_stacks;
    unsigned allocations{};
    Fixture() : resources{{{resource_address, data}}} {
        put(data, 0x20, 2);
        // These words are skipped, not followed as offsets.
        put(data, 0x24, 0xffffffff);
        put(data, 0x28, 0xeeeeeeee);
        put(data, 0x2c, 0x1100);
        put(data, 0x30, 555, 2);
        put(data, 0x32, 666, 2);
        put(data, 0x34, 0x7fff, 2);
        put(data, 0x36, 0xfffc, 2);
        put(data, 0x38, 2, 2);
        put(data, 0x3a, 1, 2);
        put(data, 0x3c, 0x12345678);
        put(data, 0x40, 0x1101);
        put(data, 0x44, 0xffff, 2);
        put(data, 0x46, 20, 2);
        put(data, 0x48, 4, 2);
        put(data, 0x4a, 0xfffe, 2);
        put(data, 0x4c, 1, 2);
        put(data, 0x4e, 1, 2);
        put(data, 0x50, 0xabcd, 2);
        data[0x80] = 0xfc;
        put(data, 0x81, 0xffff9f, 3);
        data[0x84] = 0x97;
        put(sprite, 0x24, sprite_address + 0x110);
        put(sprite, 0x114, 100, 2);
        put(sprite, 0x116, 0xfffd, 2);
        put(sprite, 0x64, resource_address + 0x80);
        widths[0xfc] = 4;
        services.upload_state = &state;
        services.allocate = [&](auto size, auto mode, auto) {
            check(size == 8192 && mode == allocations, "FC stack size and allocation modes");
            ++allocations;
            return field::SpriteAllocation{0x80100000 + allocations * 8192,
                                           std::vector<std::uint8_t>(8192, 0xa5)};
        };
        services.release = [&](auto address) {
            releases.push_back(address);
            released_stacks.push_back(address == state.inner_stack.address ? state.inner_stack
                                                                           : state.outer_stack);
        };
        services.upload_image = [&](const auto &request) { uploads.push_back(request); };
    }
    field::SpriteSources sources() {
        field::SpriteSources result{resources, {}, {}, widths, {}};
        result.services = &services;
        result.replay_entry_sp = 0x801fff00; // Invented original caller coordinate.
        return result;
    }
};
void original_record_and_lifetime_order() {
    Fixture fixture;
    try {
        static_cast<void>(field::execute_sprite_commands({Fixture::sprite_address, fixture.sprite},
                                                         fixture.environment, fixture.sources()));
        check(false, "Next command must remain unsupported");
    } catch (const field::UnrecoveredSpriteCommand &error) {
        check(error.opcode == 0x97 && error.command_pc == Fixture::resource_address + 0x84,
              "FC advances only after its service calls complete");
    }
    check(fixture.uploads.size() == 2 && fixture.allocations == 2 &&
              fixture.releases == std::vector<std::uint32_t>{0x80104000, 0x80102000} &&
              fixture.state.outer_stack.bytes.empty() && fixture.state.inner_stack.bytes.empty(),
          "Two temporary allocations release in reverse order after ordered uploads");
    check(fixture.state.resource == Fixture::resource_address + 0x20 && fixture.state.x == 100 &&
              fixture.state.y == -3,
          "Signed24 operand-relative address and descriptor coordinates are computed");
    check(fixture.uploads[0].rectangle == std::array<std::int16_t, 4>{-32669, -7, 2, 1} &&
              fixture.uploads[0].source_address == Fixture::resource_address + 0x3c &&
              fixture.uploads[0].bytes == std::vector<std::uint8_t>{0x78, 0x56, 0x34, 0x12} &&
              fixture.uploads[1].rectangle == std::array<std::int16_t, 4>{3, 18, 1, 1} &&
              fixture.uploads[1].bytes == std::vector<std::uint8_t>{0xcd, 0xab},
          "1100 and1101 coordinates differ; halfword wrap and sequential pixel cursor match");
    const auto read = [](const auto &bytes, std::size_t at) {
        std::uint32_t word = 0;
        for (std::uint32_t i = 0; i < 4; ++i)
            word |= static_cast<std::uint32_t>(bytes.at(at + i)) << (8U * i);
        return word;
    };
    const auto &outer = fixture.released_stacks.at(1);
    check(read(outer.bytes, 0x1f00) == 0x801fff00 - 0x28 - 0x88 &&
              read(outer.bytes, 0x1ef4) == outer.address &&
              read(outer.bytes, 0x1ef8) == 0x8001fec8 &&
              read(outer.bytes, 0x1ebc) == (0x8001fb40U & 0x1ffffffU) >> 2U &&
              read(outer.bytes, 0x1ec4) == outer.address &&
              read(outer.bytes, 0x1ec8) == Fixture::resource_address + 0x81 &&
              read(outer.bytes, 0x1ecc) == 0x8001fb90,
          "Released outer stack retains computed source caller and register stores");
    check(read(fixture.released_stacks.at(0).bytes, 0x1f00) == outer.address + 0x1efc - 0x28,
          "Released inner stack retains its actual outer caller coordinate");
}
void malformed_records_and_interrupted_service() {
    Fixture fixture;
    put(fixture.data, 0x40, 0x9999);
    check(field::upload_sprite_images(Fixture::resource_address + 0x20, 100, -3, fixture.sources(),
                                      fixture.services.upload_image) == 1 &&
              fixture.uploads.size() == 1,
          "Unknown record returns original failure after retaining earlier upload");
    put(fixture.data, 0x20, 0xffffffff);
    check(field::upload_sprite_images(Fixture::resource_address + 0x20, 0, 0, fixture.sources(),
                                      {}) == 0,
          "Negative signed count performs no image service call");
    Fixture interrupted;
    interrupted.services.upload_image = [](const auto &) {
        throw std::runtime_error("Synthetic GPU service interruption");
    };
    bool failed = false;
    try {
        static_cast<void>(
            field::execute_sprite_commands({Fixture::sprite_address, interrupted.sprite},
                                           interrupted.environment, interrupted.sources()));
    } catch (const std::runtime_error &) {
        failed = true;
    }
    check(failed && interrupted.allocations == 2 && interrupted.releases.empty() &&
              interrupted.state.outer_stack.bytes.size() == 8192 &&
              interrupted.state.inner_stack.bytes.size() == 8192,
          "An interrupted service retains owned temporary storage without inventing cleanup");
    Fixture missing;
    missing.resources[0].bytes = std::span(missing.data).first(0x3e);
    try {
        static_cast<void>(field::upload_sprite_images(Fixture::resource_address + 0x20, 0, 0,
                                                      missing.sources(),
                                                      missing.services.upload_image));
        check(false, "Unqualified pixel suffix must reject");
    } catch (const field::SpriteInputError &) {
    }
    check(missing.uploads.empty(), "A missing pixel range is never uploaded as invented zeros");
    Fixture unqualified;
    auto unqualified_sources = unqualified.sources();
    unqualified_sources.replay_entry_sp.reset();
    try {
        static_cast<void>(
            field::execute_sprite_commands({Fixture::sprite_address, unqualified.sprite},
                                           unqualified.environment, unqualified_sources));
        check(false, "An upload cannot invent its original caller stack coordinate");
    } catch (const field::UnrecoveredSpriteBehavior &) {
    }
    check(unqualified.allocations == 0 && unqualified.uploads.empty(),
          "Missing source stack context stops before reserving upload storage");
    Fixture invalid_stack;
    auto invalid_sources = invalid_stack.sources();
    invalid_sources.replay_entry_sp = 0x80000010;
    try {
        static_cast<void>(
            field::execute_sprite_commands({Fixture::sprite_address, invalid_stack.sprite},
                                           invalid_stack.environment, invalid_sources));
        check(false, "The retained original stack frame cannot wrap below RAM");
    } catch (const field::SpriteInputError &) {
    }
    check(invalid_stack.allocations == 0, "Invalid original stack bounds stop before allocation");
}
} // namespace
int main() {
    try {
        original_record_and_lifetime_order();
        malformed_records_and_interrupted_service();
        std::cout << "Sprite upload reconstruction passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
