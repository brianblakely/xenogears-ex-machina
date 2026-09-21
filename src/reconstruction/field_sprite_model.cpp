#include "xem/reconstruction/field_sprite_model.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction::field {
namespace {
void input(bool condition, const char *message) {
    if (!condition)
        throw SpriteInputError(message);
}
void recovered(bool condition, const char *message) {
    if (!condition)
        throw UnrecoveredSpriteBehavior(message);
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    input(at <= bytes.size() && width <= bytes.size() - at, "Model read exceeds owned bytes");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8 * i);
    return value;
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    input(at <= bytes.size() && width <= bytes.size() - at, "Model write exceeds owned bytes");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint32_t read(const SpriteSources &sources, std::uint32_t pointer, std::size_t width = 4) {
    std::optional<std::uint32_t> value;
    for (const auto &source : sources.resources) {
        if (pointer < source.address)
            continue;
        const auto at = static_cast<std::size_t>(pointer - source.address);
        if (at <= source.bytes.size() && width <= source.bytes.size() - at) {
            const auto next = get(source.bytes, at, width);
            input(!value || *value == next, "Overlapping model sources disagree");
            value = next;
        }
    }
    input(value.has_value(), "Model read requires address-qualified resource bytes");
    return *value;
}
void write(const SpriteSources &sources, std::uint32_t pointer, std::uint32_t value,
           std::size_t width = 4) {
    std::optional<std::span<std::uint8_t>> target;
    for (const auto &source : sources.mutable_resources) {
        if (pointer < source.address)
            continue;
        const auto at = static_cast<std::size_t>(pointer - source.address);
        if (at <= source.bytes.size() && width <= source.bytes.size() - at) {
            input(!target, "Model relocation requires one unambiguous mutable owner");
            target = source.bytes.subspan(at, width);
        }
    }
    input(target.has_value(), "Model relocation requires owned mutable resource bytes");
    put(*target, 0, value, width);
}
void observe(const SpriteSources &sources, std::string_view operation, std::uint32_t address) {
    if (sources.observe_execution)
        sources.observe_execution({operation, address, {}});
}
void packets(std::uint32_t model, SpriteAllocation &buffer, SpriteEnvironment &environment,
             const SpriteSources &sources) {
    auto &state = *sources.models;
    observe(sources, "sprite_model_packets", 0x8002c8cc);
    state.output = buffer.address;
    state.shading = read(sources, model + 0x14);
    state.geometry = read(sources, model + 0x10);
    state.normals = read(sources, model + 0x0c);
    state.vertices = read(sources, model + 8);
    state.auxiliary = read(sources, model + 0x18);
    state.primitive_count += read(sources, model + 4, 2);
    const auto groups = read(sources, model + 6, 2);
    for (std::uint32_t group = 0; group < groups; ++group) {
        const auto count = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(read(sources, state.geometry + 2, 2)));
        const auto row = 0x8004fe68U + read(sources, state.geometry, 1) * 40U;
        const auto handler = read(sources, row);
        state.geometry += 4;
        // A negative signed count is not an empty group in the original loop.
        input(count >= 0, "Negative model group count exceeds valid resource bounds");
        for (std::int32_t remaining = count; remaining != 0;) {
            observe(sources, "sprite_model_primitive", handler);
            recovered(handler == 0x8002d984 || handler == 0x8002d0e4,
                      "Model primitive initialization handler is unreconstructed");
            const auto command = read(sources, state.shading + 3, 1);
            if (command == 0xc4) {
                state.material_page = static_cast<std::uint16_t>(read(sources, state.shading, 2));
                if (environment.texture_mode == 1) {
                    state.material_page &= 0xffe0;
                    state.material_page =
                        static_cast<std::uint16_t>(state.material_page | environment.texture_page);
                } else if (environment.texture_mode == 2) {
                    state.material_page = static_cast<std::uint16_t>(environment.texture_page);
                }
                state.shading += 4;
                continue;
            }
            if (command == 0xc8) {
                state.material_palette =
                    static_cast<std::uint16_t>(read(sources, state.shading, 2));
                if (state.palette_mode == 0)
                    state.material_palette = static_cast<std::uint16_t>(
                        (state.material_palette & 15U) | state.palette_base);
                state.shading += 4;
                continue;
            }
            input(state.output >= buffer.address, "Primitive output precedes its owned buffer");
            const auto at = static_cast<std::size_t>(state.output - buffer.address);
            const bool quad = handler == 0x8002d0e4;
            put(buffer.bytes, at + 3, quad ? 9 : 7, 1);
            if (quad)
                put(buffer.bytes, at + 4, read(sources, state.shading));
            put(buffer.bytes, at + 12,
                read(sources, state.shading + 4, 2) |
                    (static_cast<std::uint32_t>(state.material_palette) << 16));
            put(buffer.bytes, at + 20,
                read(sources, state.shading + 6, 2) |
                    (static_cast<std::uint32_t>(state.material_page) << 16));
            put(buffer.bytes, at + 28, read(sources, state.shading + (quad ? 8 : 0), 2), 2);
            if (quad)
                put(buffer.bytes, at + 36, read(sources, state.shading + 10, 2), 2);
            else
                put(buffer.bytes, at + 7, command, 1);
            state.geometry += read(sources, row + 4);
            state.output += read(sources, row + 12);
            state.shading += read(sources, row + 8);
            --remaining;
        }
    }
    environment.texture_mode = 0;
    state.palette_mode = 1;
}
} // namespace

void relocate_sprite_model(std::uint32_t model, const SpriteSources &sources) {
    observe(sources, "sprite_model_relocation", 0x8002c59c);
    const auto flags = read(sources, model, 2);
    if ((flags & 0x20) != 0)
        return;
    write(sources, model, flags | 0x20, 2);
    for (const auto offset : {8U, 0x10U, 0x0cU, 0x14U})
        write(sources, model + offset, read(sources, model + offset) + model);
    if (const auto offset = read(sources, model + 0x1c); offset != 0) {
        const auto table = model + offset;
        write(sources, model + 0x1c, table);
        auto count = read(sources, table);
        auto record = table + count * 12U + 4U;
        while (count != 0xffffffffU) {
            observe(sources, "sprite_model_relocation_record", 0x8002c59c);
            --count;
            write(sources, record + 4, read(sources, record + 4) + model);
            write(sources, record + 8, read(sources, record + 8) + model);
            record -= 12;
        }
    }
}

void construct_sprite_model(SpriteWindow sprite, std::uint32_t model,
                            SpriteEnvironment &environment, const SpriteSources &sources) {
    recovered(sources.models && sources.heap && sources.services,
              "Sprite model ownership and heap services are not connected");
    auto &heap = *sources.heap;
    heap.allocation_class = 5;
    heap.class_five_context = 0;
    heap.allocation_cursor = 0;
    relocate_sprite_model(model, sources);
    const auto renderer = get(sprite.bytes, 0x20);
    input(renderer >= sprite.address, "Model renderer precedes owned sprite");
    const auto at = static_cast<std::size_t>(renderer - sprite.address);
    const auto previous = get(sprite.bytes, at + 0x2c);
    auto &state = *sources.models;
    if (previous != 0) {
        const auto old = std::find_if(state.buffers.begin(), state.buffers.end(),
                                      [&](const auto &item) { return item.address == previous; });
        input(old != state.buffers.end(), "Previous model buffer has no owned allocation");
        recovered(static_cast<bool>(sources.services->release), "Model release service is absent");
        sources.services->release(previous);
        state.buffers.erase(old);
    }
    observe(sources, "sprite_model_allocation", 0x8002cb54);
    heap.tag = 0x25;
    const auto size = read(sources, model + 0x34);
    input(size <= 0x100000, "Original model packet allocation exceeds resource bounds");
    recovered(static_cast<bool>(sources.services->allocate), "Model allocation service is absent");
    auto buffer = sources.services->allocate(size * 2U, 0);
    input(buffer.bytes.size() == size * 2U && (size == 0 || buffer.address != 0) &&
              static_cast<std::uint64_t>(buffer.address) + buffer.bytes.size() <= (1ULL << 32),
          "Incomplete original model allocation or incoming bytes");
    state.buffers.push_back(std::move(buffer));
    auto &owned = state.buffers.back();
    put(sprite.bytes, at + 0x2c, owned.address);
    put(sprite.bytes, at + 0x30, owned.address + size);
    packets(model, owned, environment, sources);
    std::copy_n(owned.bytes.begin(), size, owned.bytes.begin() + size);
    put(sprite.bytes, at + 0x34, model);
}

} // namespace xem::reconstruction::field
