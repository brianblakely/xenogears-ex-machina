#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/field_motion.hpp"

#include <bit>
#include <unordered_set>

namespace xem::reconstruction::field {
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw SpriteError(message);
}
void require_recovered(bool condition, const char *message) {
    if (!condition)
        throw UnrecoveredSpriteBehavior(message);
}
void require_source(bool condition, const char *message) {
    if (!condition)
        throw SpriteInputError(message);
}
bool valid_extent(std::uint32_t address, std::size_t size) {
    return size <= (std::uint64_t{1} << 32) - address;
}
void check_window(SpriteWindow sprite) {
    require(sprite.address != 0 && sprite.bytes.size() >= 0xb4 &&
                valid_extent(sprite.address, sprite.bytes.size()),
            "Incomplete original sprite window");
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    require(at <= bytes.size() && width <= bytes.size() - at, "Sprite read exceeds supplied bytes");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8 * i);
    return value;
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    require(at <= bytes.size() && width <= bytes.size() - at,
            "Sprite write exceeds supplied bytes");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::int32_t signed_word(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int32_t signed_half(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t product(std::int32_t a, std::int32_t b) {
    return signed_word(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t truncate_shift(std::int32_t value, unsigned bits) {
    if (value < 0)
        value = signed_word(static_cast<std::uint32_t>(value) + ((1U << bits) - 1));
    return value >> bits;
}
std::size_t inside(SpriteWindow sprite, std::uint32_t pointer, std::size_t size) {
    require(pointer >= sprite.address, "Sprite pointer precedes supplied window");
    const auto at = static_cast<std::size_t>(pointer - sprite.address);
    require(at <= sprite.bytes.size() && size <= sprite.bytes.size() - at,
            "Sprite pointer exceeds supplied window");
    return at;
}
std::uint32_t read(std::span<const SpriteResource> sources, std::uint32_t pointer,
                   std::size_t size) {
    require_source(valid_extent(pointer, size), "Sprite source read wraps address space");
    std::optional<std::uint32_t> value;
    for (const auto &source : sources) {
        require_source(valid_extent(source.address, source.bytes.size()),
                       "Sprite source extent wraps address space");
        if (pointer < source.address)
            continue;
        const auto at = static_cast<std::size_t>(pointer - source.address);
        if (at <= source.bytes.size() && size <= source.bytes.size() - at) {
            const auto next = get(source.bytes, at, size);
            require_source(!value || *value == next, "Overlapping sprite sources disagree");
            value = next;
        }
    }
    require_source(value.has_value(),
                   "Sprite read requires missing address-qualified source bytes");
    return *value;
}
std::uint32_t resource(const SpriteSources &sources, std::uint32_t pointer, std::size_t size) {
    return read(sources.resources, pointer, size);
}
SpriteAllocation allocation(const SpriteAllocator &allocate, std::uint32_t size) {
    require(static_cast<bool>(allocate), "Original sprite allocation boundary is absent");
    auto result = allocate(size, 0);
    require(result.bytes.size() == size && valid_extent(result.address, size) &&
                (size == 0 || result.address != 0),
            "Incomplete original sprite allocation or incoming bytes");
    return result;
}
std::optional<SpriteWindow> owned_window(SpriteTaskState &state, std::uint32_t address,
                                         std::size_t size) {
    for (auto &allocation : state.nodes) {
        if (address < allocation.address)
            continue;
        const auto offset = static_cast<std::size_t>(address - allocation.address);
        if (offset <= allocation.bytes.size() && size <= allocation.bytes.size() - offset)
            return SpriteWindow{address, std::span(allocation.bytes).subspan(offset)};
    }
    return {};
}
void advance_task_motion(SpriteWindow sprite, const SpriteSources &sources) {
    if (sources.observe_execution)
        sources.observe_execution({"sprite_task_motion", 0x80022cdc, {}});
    // 80022cac scales the already-shifted motion with truncation toward zero.
    const auto motion = [&](std::size_t at) {
        auto value = signed_word(get(sprite.bytes, at)) >> 4;
        const auto scale = get(sprite.bytes, 0x3a, 2);
        if (scale != 0)
            value = truncate_shift(product(value, static_cast<std::int32_t>(scale)), 10);
        return static_cast<std::uint32_t>(value) * 16U;
    };
    put(sprite.bytes, 0, get(sprite.bytes, 0) + motion(0x0c));
    put(sprite.bytes, 8, get(sprite.bytes, 8) + motion(0x14));
    require_recovered((get(sprite.bytes, 0x3c) & 0x4000000U) != 0,
                      "Grounded sprite task motion 80022b2c requires field 800ba8f4");
    put(sprite.bytes, 4, get(sprite.bytes, 4) + motion(0x10));
    put(sprite.bytes, 0x10, get(sprite.bytes, 0x10) + get(sprite.bytes, 0x1c));
}
} // namespace

std::array<std::int32_t, 3> initial_sprite_bounds(SpriteWindow sprite,
                                                  const SpriteSources &sources) {
    check_window(sprite);
    const auto binding = inside(sprite, get(sprite.bytes, 0x24), 20);
    const auto directory = get(sprite.bytes, binding + 16);
    const auto header = directory + resource(sources, directory + 2, 2);
    const auto first_frame = header + resource(sources, header + 4, 2) + 4;
    auto index = resource(sources, first_frame, 1);
    if (index != 0)
        --index;
    const auto frames = get(sprite.bytes, binding);
    if (resource(sources, frames + index * 2, 2) < index)
        index = 0;
    const auto record = frames + resource(sources, frames + index * 2 + 2, 2);
    const auto height =
        truncate_shift(product(static_cast<std::int32_t>(resource(sources, record + 3, 1)),
                               signed_half(get(sprite.bytes, 0x2c, 2))),
                       12);
    const auto third =
        truncate_shift(product(static_cast<std::int32_t>(resource(sources, record + 1, 1)),
                               signed_half(get(sprite.bytes, 0x2c, 2))),
                       12);
    const auto first =
        truncate_shift(product(static_cast<std::int32_t>(resource(sources, record + 2, 1)),
                               signed_half(get(sprite.bytes, 0x2c, 2))),
                       12);
    return {first, height, third};
}

void advance_sprite_tasks(SpriteTaskState &state, SpriteEnvironment &environment,
                          const SpriteSources &input_sources) {
    if (state.wait_count != 0) {
        if (--state.wait_count == 0)
            state.wait_flag = 0;
        return;
    }
    auto sources = input_sources;
    sources.tasks = &state;
    state.next = state.head;
    std::unordered_set<std::uint32_t> seen;
    while (state.next != 0) {
        const auto current = state.next;
        require(seen.insert(current).second, "Original sprite task list is cyclic");
        if (sources.observe_execution)
            sources.observe_execution({"sprite_task", 0x8001c964, {}});
        const auto node = owned_window(state, current, 28);
        const auto next = node ? get(node->bytes, 24) : resource(sources, current + 24, 4);
        const auto callback = node ? get(node->bytes, 8) : resource(sources, current + 8, 4);
        state.current = current;
        state.next = next;
        if (callback == 0)
            continue;
        require_recovered(callback == 0x80022df4,
                          "Original sprite task update callback is unreconstructed");
        require_source(node.has_value(), "Sprite task update requires an owned task node");
        const auto sprite = owned_window(state, get(node->bytes, 4), 0xb4);
        require_source(sprite.has_value(), "Sprite task update requires its owned sprite");
        if (sources.observe_execution)
            sources.observe_execution({"sprite_task_callback", 0x80022df4, {}});
        static_cast<void>(advance_sprite_timer(*sprite, environment, sources));
        advance_task_motion(*sprite, sources);
        if (get(sprite->bytes, 0x64) != 0) {
            if ((get(sprite->bytes, 0xac) & 0x40) == 0)
                continue;
            static_cast<void>(advance_sprite_timer(*sprite, environment, sources));
            advance_task_motion(*sprite, sources);
            if (get(sprite->bytes, 0x64) != 0)
                continue;
        }
        if (sources.observe_execution)
            sources.observe_execution({"sprite_task_destruction", get(node->bytes, 12), {}});
        require_recovered(false,
                          "Completed sprite task requires its original destruction callback");
    }
}

void remove_sprite_tasks(SpriteTaskState &state, std::uint32_t owner, SpriteWindow sprite,
                         const SpriteSources &sources) {
    const auto node_at = [&](std::uint32_t address) -> std::span<std::uint8_t> {
        for (auto &allocation : state.nodes) {
            if (address < allocation.address)
                continue;
            const auto offset = static_cast<std::size_t>(address - allocation.address);
            if (offset <= allocation.bytes.size() && 28 <= allocation.bytes.size() - offset)
                return std::span(allocation.bytes).subspan(offset, 28);
        }
        throw SpriteInputError("Missing owned sprite task node");
    };
    for (auto *head : {&state.pending_head, &state.head}) {
        std::uint32_t previous = 0;
        auto address = *head;
        std::unordered_set<std::uint32_t> seen;
        while (address != 0) {
            require(seen.insert(address).second, "Original sprite removal list is cyclic");
            if (sources.observe_execution)
                sources.observe_execution({"remove_sprite_task", 0x8001ce74, {}});
            auto node = node_at(address);
            const auto flags = get(node, 20);
            bool matches = false;
            if (get(node, 0) == owner && (flags & 0x40000000U) == 0) {
                const auto generation =
                    owner == sprite.address ? get(sprite.bytes, 16) : get(node_at(owner), 16);
                matches = (flags & 0x1fffffffU) == (generation & 0x1fffffffU);
            }
            if (matches) {
                const auto next = get(node, 24);
                if (previous == 0)
                    *head = next;
                else
                    put(node_at(previous), 24, next);
                if (state.next == address)
                    state.next = next;
                require_recovered(get(node, 12) == 0,
                                  "Sprite task destruction callback is unreconstructed");
            } else {
                previous = address;
            }
            // The original reloads this link after the callback.
            address = get(node, 24);
        }
    }
}

void create_field_sprite(SpriteConstruction &result, std::span<std::uint8_t> actor,
                         std::span<std::uint8_t> descriptor, const FieldSpriteArguments &arguments,
                         FieldSpriteEnvironment &environment, const SpriteSources &input_sources,
                         const SpriteAllocator &allocate, const SpriteReleaser &release,
                         const SpriteConstructionObserver &observe) {
    auto sources = input_sources;
    sources.tasks = &environment.tasks;
    sources.heap = &environment.heap;
    sources.field_actor = SpriteFieldActor{arguments.actor_index, actor};
    require(result.sprite.address == 0 && result.parts.address == 0 &&
                result.sprite.bytes.empty() && result.parts.bytes.empty(),
            "Field sprite creation requires an unowned output");
    require(actor.size() == 0x138 && descriptor.size() == 0x5c,
            "Field sprite creation requires a complete actor and descriptor");
    require_recovered(!(get(descriptor, 0x58) & 0x10000),
                      "Field sprite creation requires unreconstructed existing-sprite destruction");
    require_recovered(arguments.mode != 0 || arguments.part_variant == 0,
                      "Alternate field sprite part constructor 80024294 is unreconstructed");
    const auto select_allocation_class = [&] {
        environment.heap.allocation_class = 8;
        environment.heap.class_eight_context = 0;
        environment.heap.allocation_cursor = 0;
    };
    select_allocation_class();
    put(actor, 0x127, arguments.resource_slot, 1);
    put(actor, 0x126, arguments.tag, 1);
    put(actor, 0x134, (get(actor, 0x134) & 0xfffffff0) | (arguments.part_variant & 15));
    put(actor, 0x130, (get(actor, 0x130) & 0xcfffffff) | ((arguments.mode & 3) << 28));
    put(actor, 0x134, (get(actor, 0x134) & ~16U) | ((arguments.defer_initial_step & 1) << 4));
    std::array<std::int16_t, 5> parameters;
    parameters[0] = 0x100;
    if (arguments.mode == 0) {
        const auto coordinates = 0x800b1f78U + arguments.resource_slot * 8;
        const auto second = resource(sources, coordinates + 2, 2);
        const auto first = resource(sources, coordinates, 2);
        parameters[1] = static_cast<std::int16_t>(signed_half(arguments.resource_slot + 0x1e0));
        parameters[2] = static_cast<std::int16_t>(signed_half(first));
        parameters[3] = static_cast<std::int16_t>(signed_half(second));
        parameters[4] = 0x40;
    } else {
        parameters[1] = static_cast<std::int16_t>(
            signed_half(arguments.resource_slot + (arguments.mode == 1 ? 0xe0U : 0xe3U)));
        parameters[2] = arguments.mode == 1 ? 0x280 : 0x2a0;
        parameters[3] =
            static_cast<std::int16_t>(signed_half(arguments.resource_slot * 64 + 0x100));
        parameters[4] = 8;
    }
    try {
        create_sprite(result, arguments.resource, parameters, environment.sprite, sources, allocate,
                      observe);
    } catch (...) {
        if (!result.sprite.bytes.empty())
            environment.sprite = result.environment;
        throw;
    }
    environment.sprite = result.environment;
    // Both caller state and the owned construction retain the latest shared
    // environment if an animation or task dependency interrupts this call.
    struct RetainEnvironment {
        SpriteConstruction &result;
        FieldSpriteEnvironment &environment;
        ~RetainEnvironment() { result.environment = environment.sprite; }
    } retain{result, environment};
    SpriteWindow sprite{result.sprite.address, result.sprite.bytes};
    sources.factory_sprite = SpriteResource{sprite.address, sprite.bytes};
    put(descriptor, 4, sprite.address);
    if (arguments.mode != 0) {
        require(static_cast<bool>(release), "Original sprite part-release boundary is absent");
        const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 52);
        require(get(sprite.bytes, renderer + 44) == result.parts.address,
                "Original owned sprite part address differs");
        release(result.parts.address);
        result.parts = {};
        result.parts = allocation(allocate, 32 * 24);
        require(static_cast<std::uint64_t>(result.parts.address) + result.parts.bytes.size() <=
                        sprite.address ||
                    static_cast<std::uint64_t>(sprite.address) + sprite.bytes.size() <=
                        result.parts.address,
                "Original replacement parts overlap sprite allocation");
        put(sprite.bytes, renderer + 48, result.parts.address);
        put(sprite.bytes, renderer + 44, result.parts.address);
    }
    put(descriptor, 0x5a, get(descriptor, 0x5a, 2) | 1, 2);
    const auto bounds = initial_sprite_bounds(sprite, sources);
    put(sprite.bytes, 0x40, (get(sprite.bytes, 0x40) & 0xffffe0ff) | 0x300);
    put(sprite.bytes, 0x2c, 0xc00, 2);
    put(sprite.bytes, 0x82, 0x2000, 2);
    if (environment.return_mode == 0) {
        for (const auto at : {0U, 4U, 8U})
            put(sprite.bytes, at, get(actor, 0x20 + at));
        for (const auto at : {0x10U, 0x0cU, 0x10U, 0x14U})
            put(sprite.bytes, at, 0);
        put(sprite.bytes, 0x1c, 0x10000);
        put(sprite.bytes, 0x84, get(descriptor, 0x24), 2);
        put(actor, 0x1a, arguments.mode == 0 ? static_cast<std::uint32_t>(bounds[1]) << 1 : 0x40,
            2);
    }
    if (environment.field_gate != 0)
        put(sprite.bytes, 0x40, get(sprite.bytes, 0x40) | 0x40000);
    select_sprite_animation(sprite, 0, environment.sprite, sources);
    put(sprite.bytes, 0x32, 0, 2);
    static_cast<void>(rebuild_sprite_velocity(sprite, sources.trigonometry));
    select_allocation_class();
    const auto sequencer = inside(sprite, get(sprite.bytes, 0x7c), 22);
    put(sprite.bytes, sequencer + 20, arguments.actor_index, 2);
    put(sprite.bytes, 0x68, 0x80076a74);
    if (arguments.defer_initial_step == 0) {
        static_cast<void>(advance_sprite_timer(sprite, environment.sprite, sources));
        advance_sprite_tasks(environment.tasks, environment.sprite, sources);
        const auto active_sequencer = inside(sprite, get(sprite.bytes, 0x7c), 14);
        if (get(sprite.bytes, active_sequencer + 12, 2) == 255) {
            put(actor, 0xea, 255, 2);
            put(actor, 4, get(actor, 4) | 0x1000000);
            for (const auto at : {0U, 4U, 8U})
                put(sprite.bytes, at, get(actor, 0x20 + at));
        }
    }
    for (const auto at : {0U, 4U, 8U}) {
        const auto position = static_cast<std::uint32_t>(signed_half(get(actor, 0x22 + at, 2)));
        put(descriptor, 0x20 + at, position);
        put(descriptor, 0x40 + at, position);
    }
    put(sprite.bytes, 0x84, get(descriptor, 0x24), 2);
    put(sprite.bytes, 0, get(actor, 0x20));
    put(sprite.bytes, 4, get(actor, 0x24));
    ++environment.initialized_count;
    put(sprite.bytes, 8, get(actor, 0x28));
}
} // namespace xem::reconstruction::field
