#include "xem/reconstruction/field_sprite.hpp"
#include "xem/reconstruction/field_motion.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/field_sprite_model.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <string>
#include <unordered_set>

namespace xem::reconstruction::field {
namespace {
constexpr std::uint32_t sprite_bytes = 0x164;

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
void observe(const SpriteSources &sources, std::string_view operation,
             std::uint32_t machine_address, std::optional<std::uint32_t> command_pc = {}) {
    if (sources.observe_execution)
        sources.observe_execution({operation, machine_address, command_pc});
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
std::int32_t fixed_product(std::int32_t a, std::int32_t b) { return product(a, b) >> 12; }
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
SpriteAllocation allocation(const SpriteAllocator &allocate, std::uint32_t size,
                            std::uint32_t mode = 0) {
    require(static_cast<bool>(allocate), "Original sprite allocation boundary is absent");
    auto result = allocate(size, mode);
    require(result.bytes.size() == size && valid_extent(result.address, size) &&
                (size == 0 || result.address != 0),
            "Incomplete original sprite allocation or incoming bytes");
    return result;
}
void render_flip(SpriteWindow sprite) {
    const auto flags = get(sprite.bytes, 0xac);
    put(sprite.bytes, 0x3c,
        (get(sprite.bytes, 0x3c) & ~8U) | ((((flags >> 3) ^ (flags >> 2)) & 1) << 3));
}
void clear_auxiliary(SpriteWindow sprite) {
    const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 64);
    const auto auxiliary = inside(sprite, get(sprite.bytes, renderer + 0x34), 64);
    std::fill_n(sprite.bytes.begin() + static_cast<std::ptrdiff_t>(auxiliary), 64, 0);
}
void previous_frame(SpriteWindow sprite, std::uint32_t frame, std::uint32_t binding,
                    const SpriteSources &sources) {
    const auto at = inside(sprite, binding, 20);
    const auto directory = get(sprite.bytes, at);
    const auto word = resource(sources, directory, 2);
    require(signed_word(frame) >= 0, "Original previous-frame index is negative");
    if (frame >= (word & 0x1ff) + 1)
        return;
    const auto record = directory + resource(sources, directory + frame * 2, 2);
    const auto flags = resource(sources, record, 1);
    const auto count = flags & 63;
    auto pointer = record + ((word & 0x8000) ? count * 2 + 4 : count * 4 + 6);
    std::uint32_t part = 0;
    while (part != count) {
        observe(sources, "sprite_previous_frame", (word & 0x8000) ? 0x8001f750 : 0x8001f8e8,
                pointer);
        const auto command = resource(sources, pointer++, 1);
        if (command & 0x80) {
            if (command & 0x40) {
                const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 64);
                const auto target = get(sprite.bytes, renderer + 0x34);
                require_recovered(target != 0,
                                  "Original frame auxiliary allocation is unreconstructed");
                const auto destination = inside(sprite, target, 64) + (command & 7) * 8;
                if (command & 0x20) {
                    const auto operands = resource(sources, pointer, 2);
                    pointer += 2;
                    put(sprite.bytes, destination, operands, 1);
                    put(sprite.bytes, destination + 1, operands >> 8, 1);
                }
                std::uint32_t value = 0;
                if (command & 0x10)
                    value = resource(sources, pointer++, 1) << 4;
                put(sprite.bytes, destination + 6, value, 2);
            } else {
                pointer += ((command & 1) != 0) + ((command & 2) != 0);
            }
        } else {
            pointer += (flags & 0x80) ? 4 : 2;
            ++part;
        }
    }
}
void lookup_frame(SpriteWindow sprite, SpriteEnvironment &environment,
                  const SpriteSources &sources) {
    const auto pointer = get(sprite.bytes, 0x54) + ((get(sprite.bytes, 0xa8) >> 11) & 63) * 2;
    const auto value = resource(sources, pointer, 2);
    put(sprite.bytes, 0xac, (get(sprite.bytes, 0xac) & ~8U) | ((value >> 6) & 8));
    render_flip(sprite);
    schedule_sprite_frame(sprite, value & 0x1ff, environment, sources);
}
void defaults(SpriteWindow sprite, std::int32_t rate_control) {
    put(sprite.bytes, 0x3c, 0);
    put(sprite.bytes, 0x2b, 0x2d, 1);
    put(sprite.bytes, 0x40, 0);
    for (auto at : {0x3aU, 0x30U, 0x32U, 0x34U})
        put(sprite.bytes, at, 0, 2);
    for (auto at : {0xa8U, 0xacU, 0xb0U})
        put(sprite.bytes, at, 0);
    put(sprite.bytes, 0xb0, 0, 1);
    put(sprite.bytes, 0xaf, 0, 1);
    const auto rate = static_cast<std::uint32_t>(rate_control) + 1;
    const auto value =
        product(signed_word(rate * (rate << 14)), signed_half(get(sprite.bytes, 0x82, 2)));
    put(sprite.bytes, 0xac, 0x8000);
    put(sprite.bytes, 0x1c, static_cast<std::uint32_t>(truncate_shift(value, 12)));
    for (auto at : {0x64U, 0x70U, 0x44U, 0x68U})
        put(sprite.bytes, at, 0);
    put(sprite.bytes, 0x80, 0, 2);
    put(sprite.bytes, 0x8c, 16, 1);
    put(sprite.bytes, 0x84, 0, 2);
    put(sprite.bytes, 0x6c, 0);
    put(sprite.bytes, 0x50, 0);
}
void bind_inline(SpriteWindow sprite) {
    put(sprite.bytes, 0x20, sprite.address + 0xb4);
    for (auto at : {0xb4U, 0xb6U, 0xb8U})
        put(sprite.bytes, at, 0, 2);
    put(sprite.bytes, 0xe0, 0);
    put(sprite.bytes, 0x7c, sprite.address + 0xf4);
    put(sprite.bytes, 0xe8, sprite.address + 0x124);
    put(sprite.bytes, 0x24, sprite.address + 0x110);
    put(sprite.bytes, 0xec, 0);
}
void renderer_scale(SpriteWindow sprite, std::uint16_t scale) {
    const auto pointer = get(sprite.bytes, 0x20);
    if (pointer == 0)
        return;
    const auto at = inside(sprite, pointer, 12);
    put(sprite.bytes, 0x2c, scale, 2);
    for (auto offset : {10U, 8U, 6U})
        put(sprite.bytes, at + offset, scale, 2);
    put(sprite.bytes, 0x3c, get(sprite.bytes, 0x3c) | 0x10000000);
}
void apply_header(SpriteWindow sprite, std::uint32_t header, const SpriteEnvironment &environment,
                  const SpriteSources &sources) {
    install_sprite_gravity(sprite, header, environment, sources);
    const auto word = resource(sources, header, 2);
    if (!(word & 0x800))
        for (auto at : {0x14U, 0x10U, 0x0cU, 0x18U})
            put(sprite.bytes, at, 0);
    if (get(sprite.bytes, 0x20) != 0) {
        const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 64);
        if (!(word & 0x1000)) {
            for (auto at : {4U, 0U, 2U})
                put(sprite.bytes, renderer + at, 0, 2);
            update_sprite_matrix(sprite, sources);
        }
        if ((get(sprite.bytes, 0x3c) & 3) == 1) {
            put(sprite.bytes, renderer + 0x3d, 0, 1);
            put(sprite.bytes, renderer + 0x3c, 0, 1);
            if (!(get(sprite.bytes, 0x40) & 0x100000) && get(sprite.bytes, renderer + 0x34) != 0)
                clear_auxiliary(sprite);
        }
    }
    put(sprite.bytes, 0x8c, 16, 1);
    put(sprite.bytes, 0x30, 0, 2);
    put(sprite.bytes, 0x9e, 1, 2);
    const auto flags = (get(sprite.bytes, 0xa8) & 0xc03ff801) | 0x2001f800;
    put(sprite.bytes, 0xa8, flags);
    const auto auxiliary = get(sprite.bytes, 0x7c);
    if (auxiliary != 0 && (flags & 1)) {
        const auto at = inside(sprite, auxiliary, 14);
        put(sprite.bytes, at + 4, 0);
        put(sprite.bytes, at, 0);
        put(sprite.bytes, at + 12, 0, 2);
    }
}

std::uint32_t owned_read(SpriteWindow parent, const SpriteSources &sources, std::uint32_t address,
                         std::size_t width) {
    if (address >= parent.address &&
        static_cast<std::uint64_t>(address) + width <=
            static_cast<std::uint64_t>(parent.address) + parent.bytes.size())
        return get(parent.bytes, address - parent.address, width);
    if (sources.tasks)
        for (const auto &node : sources.tasks->nodes)
            if (address >= node.address &&
                static_cast<std::uint64_t>(address) + width <=
                    static_cast<std::uint64_t>(node.address) + node.bytes.size())
                return get(node.bytes, address - node.address, width);
    if (sources.factory_sprite && address >= sources.factory_sprite->address &&
        static_cast<std::uint64_t>(address) + width <=
            static_cast<std::uint64_t>(sources.factory_sprite->address) +
                sources.factory_sprite->bytes.size())
        return get(sources.factory_sprite->bytes, address - sources.factory_sprite->address, width);
    for (const auto &prior : sources.frame_list)
        if (address >= prior.address &&
            static_cast<std::uint64_t>(address) + width <=
                static_cast<std::uint64_t>(prior.address) + prior.bytes.size())
            return read(sources.frame_list, address, width);
    return resource(sources, address, width);
}

// Resident 80023b84/80023a48/800233a4. Main and auxiliary headers precede
// the child sprite in one allocation. Other renderer kinds remain explicit.
void create_child_sprite(SpriteWindow parent, std::uint32_t header, SpriteEnvironment &environment,
                         const SpriteSources &sources) {
    require_recovered(sources.tasks && sources.services,
                      "Child sprite task ownership is not connected");
    auto &tasks = *sources.tasks;
    const auto saved_creation = tasks.creation_flags;
    put(parent.bytes, 0xb0, get(parent.bytes, 0xb0) | 0x800);
    if (get(parent.bytes, 0xb0) & 0x100)
        tasks.creation_flags = 0;
    const auto header_word = resource(sources, header, 2);
    auto type = ((header_word >> 8) & 7) + ((header_word & 0x4000) ? 8 : 0);
    if (type == 3)
        type = (get(parent.bytes, 0x40) >> 13) & 15;
    require_recovered(type == 2 || type == 7 || type == 15,
                      "Child sprite renderer kind is unreconstructed");
    const auto owner = get(parent.bytes, 0x6c);
    observe(sources, "create_child_sprite", 0x80023b84);
    tasks.nodes.push_back(allocation(sources.services->allocate, 0x140, tasks.allocation_mode));
    auto &storage = tasks.nodes.back();
    const auto address = storage.address;
    auto bytes = std::span(storage.bytes);
    // 8001cc18 registers the main task before initializing its embedded sprite.
    put(bytes, 0, owner);
    const auto owner_serial = owned_read(parent, sources, owner + 0x10, 4);
    put(bytes, 0x0c, 0x8001cd94);
    put(bytes, 8, 0);
    const auto old_head = tasks.head;
    const auto serial = tasks.serial & 0x1fffffffU;
    tasks.head = address;
    put(bytes, 0x14, owner_serial & 0x1fffffffU);
    ++tasks.serial;
    put(bytes, 0x10, (get(bytes, 0x10) & 0xe0000000U) | serial);
    put(bytes, 0x18, old_head);
    if (tasks.creation_flags != 0) {
        ++tasks.active_flags;
        put(bytes, 0x14, get(bytes, 0x14) | 0x80000000U);
    }
    ++tasks.primary_count;
    // 8001ca58 gives the auxiliary header its own generation and list link.
    put(bytes, 0x1c, address);
    const auto auxiliary_serial = tasks.serial++ & 0x1fffffffU;
    put(bytes, 0x34, tasks.pending_head);
    tasks.pending_head = address + 0x1c;
    put(bytes, 0x2c, (get(bytes, 0x2c) & 0xe0000000U) | auxiliary_serial);
    put(bytes, 0x24, 0);
    put(bytes, 0x28, 0x8001cb48);
    ++tasks.auxiliary_count;
    put(bytes, 0x30, get(bytes, 0x10) & 0x1fffffffU);

    SpriteWindow child{address + 0x38, bytes.subspan(0x38)};
    defaults(child, environment.rate_control);
    put(bytes, 4, child.address);
    put(bytes, 0x20, child.address);
    put(bytes, 8, 0x80022df4);
    put(bytes, 0x0c, 0x80022eb8);
    // 80023958 initializes only these fields in the embedded kind-two parts.
    put(child.bytes, 0x20, child.address + 0xb4);
    for (auto at : {0xb4U, 0xb6U, 0xb8U})
        put(child.bytes, at, 0, 2);
    for (auto at : {0xe0U, 0xe8U, 0xf4U})
        put(child.bytes, at, 0);
    put(child.bytes, 0x6c, address);
    put(child.bytes, 0x86, 0x140, 2);
    put(child.bytes, 0x24, get(parent.bytes, 0x24));
    put(bytes, 0x14, get(bytes, 0x14) | 0x20000000U);
    put(child.bytes, 0x40, (get(child.bytes, 0x40) & 0xfffe1fffU) | (type << 13));
    put(child.bytes, 0x3c, (get(child.bytes, 0x3c) & ~3U) | 2);
    put(child.bytes, 0x40,
        (get(child.bytes, 0x40) & 0xffffe0ffU) | (get(parent.bytes, 0x40) & 0x1f00));
    put(child.bytes, 0x3c, (get(child.bytes, 0x3c) & ~0x18U) | (get(parent.bytes, 0x3c) & 0x18));
    put(child.bytes, 0x3d, get(parent.bytes, 0x3d, 1), 1);
    put(child.bytes, 0x40,
        (get(child.bytes, 0x40) & ~0x40000U) | (get(parent.bytes, 0x40) & 0x40000));
    put(child.bytes, 0x3c, (get(child.bytes, 0x3c) & ~4U) | 0x4000000);
    put(child.bytes, 0x18, get(parent.bytes, 0x18));
    for (auto at : {0x32U, 0x2cU, 0x34U})
        put(child.bytes, at, get(parent.bytes, at, 2), 2);
    put(child.bytes, 0xb0, (get(child.bytes, 0xb0) & ~0x200U) | (get(parent.bytes, 0xb0) & 0x200));
    if (get(parent.bytes, 0xb0) & 0x200) {
        put(child.bytes, 0x3a, get(parent.bytes, 0x3a, 2), 2);
        put(child.bytes, 0x40, (get(child.bytes, 0x40) & 0xffffe0ffU) | 0x300);
    }
    put(child.bytes, 0xa8,
        (get(child.bytes, 0xa8) & 0x3fffffffU) | (get(parent.bytes, 0xa8) & 0xc0000000U));
    put(child.bytes, 0xac, (get(child.bytes, 0xac) & ~3U) | (get(parent.bytes, 0xac) & 3));
    put(child.bytes, 0xb0, (get(child.bytes, 0xb0) & ~0x100U) | (get(parent.bytes, 0xb0) & 0x100));
    put(child.bytes, 0xac, (get(child.bytes, 0xac) & ~0x40U) | (get(parent.bytes, 0xac) & 0x40));
    put(child.bytes, 0xac,
        (get(child.bytes, 0xac) & 0xfff8007fU) | (get(parent.bytes, 0xac) & 0x7ff80));
    put(child.bytes, 0xa8, get(child.bytes, 0xa8) & ~1U);
    put(child.bytes, 0xac, (get(child.bytes, 0xac) & ~4U) | (get(parent.bytes, 0xac) & 4));
    put(child.bytes, 0x7c, (get(parent.bytes, 0xa8) & 1) ? 0 : get(parent.bytes, 0x7c));
    put(child.bytes, 0x70, parent.address);
    for (auto at : {0x44U, 0x48U, 0x74U})
        put(child.bytes, at, get(parent.bytes, at));
    put(child.bytes, 0x82, get(parent.bytes, 0x82, 2), 2);
    put(child.bytes, 0x50, get(parent.bytes, 0x50));
    put(child.bytes, 0x8d, get(parent.bytes, 0xaf, 1), 1);
    put(child.bytes, 0x78, get(parent.bytes, 0x78));
    for (std::size_t at = 0; at <= 0x14; at += 4)
        put(child.bytes, at, get(parent.bytes, at));
    for (std::uint32_t at = 0; at < 12; at += 2)
        put(child.bytes, 0xb4 + at, owned_read(parent, sources, get(parent.bytes, 0x20) + at, 2),
            2);
    apply_header(child, header, environment, sources);
    // 80024730 changes only the main callback for type7; all three kind-two
    // types obtain the auxiliary callback from the original resident table.
    if (type == 7)
        put(bytes, 8, 0x80022e8c);
    put(bytes, 0x24, resource(sources, 0x8004fd40 + type * 4, 4));
    tasks.creation_flags = saved_creation;
}

void invoke_sprite_callback(SpriteWindow sprite, const SpriteSources &sources) {
    const auto callback = get(sprite.bytes, 0x68);
    if (callback == 0)
        return;
    require_recovered(callback == 0x80076a74,
                      "Original sprite completion callback is unreconstructed");
    observe(sources, "field_sprite_callback", 0x80076a74);
    const auto sequencer = inside(sprite, get(sprite.bytes, 0x7c), 22);
    const auto actor = signed_half(get(sprite.bytes, sequencer + 20, 2));
    require_source(actor >= 0 && sources.field_actor &&
                       sources.field_actor->index == static_cast<std::uint32_t>(actor) &&
                       sources.field_actor->bytes.size() == 0x138,
                   "Field sprite callback requires its actual selected actor");
    auto bytes = sources.field_actor->bytes;
    put(bytes, 4, get(bytes, 4) | 0x10000);
}
} // namespace

void install_sprite_gravity(SpriteWindow sprite, std::uint32_t header,
                            const SpriteEnvironment &environment, const SpriteSources &sources) {
    check_window(sprite);
    require_recovered(environment.platform_mode == 0,
                      "Alternate sprite platform header installation is unreconstructed");
    put(sprite.bytes, 0x58, header);
    put(sprite.bytes, 0x64, header + resource(sources, header + 2, 2) + 2);
    const auto word = resource(sources, header, 2);
    put(sprite.bytes, 0xa8, (get(sprite.bytes, 0xa8) & 0xffcfffff) | ((word & 3) << 20));
    put(sprite.bytes, 0x54, header + resource(sources, header + 4, 2) + 4);
    auto coefficient = static_cast<std::int32_t>((word >> 2) & 63);
    if (coefficient & 32)
        coefficient -= 64;
    const auto rate = signed_word(static_cast<std::uint32_t>(environment.rate_control) + 1);
    const auto factor =
        truncate_shift(product(product(rate, rate), signed_half(get(sprite.bytes, 0x82, 2))), 12);
    put(sprite.bytes, 0x1c, static_cast<std::uint32_t>(coefficient * 1024));
    const auto divisor = (get(sprite.bytes, 0xac) >> 7) & 0xfff;
    require(divisor != 0, "Unresolved original sprite gravity zero-divisor path");
    auto ratio = static_cast<std::int32_t>(65536 / divisor);
    const auto scaled = product(coefficient * 1024, factor);
    put(sprite.bytes, 0x1c, static_cast<std::uint32_t>(scaled));
    ratio = truncate_shift(product(ratio, ratio), 8);
    const auto gravity = product(scaled, ratio);
    put(sprite.bytes, 0x1c, static_cast<std::uint32_t>(gravity));
    put(sprite.bytes, 0x1c, static_cast<std::uint32_t>(truncate_shift(gravity, 8)));
}

void bind_sprite_resource(SpriteWindow sprite, std::uint32_t pointer,
                          SpriteEnvironment &environment, const SpriteSources &sources) {
    check_window(sprite);
    if (pointer == 0)
        return;
    require_recovered(environment.platform_mode == 0,
                      "Alternate sprite platform resource binding is unreconstructed");
    if (pointer == get(sprite.bytes, 0x44))
        return;
    const auto at = inside(sprite, get(sprite.bytes, 0x24), 20);
    const auto coordinate1 = get(sprite.bytes, at + 4);
    const auto coordinate2 = get(sprite.bytes, at + 8);
    put(sprite.bytes, at + 4, coordinate1);
    put(sprite.bytes, at + 8, coordinate2);
    put(sprite.bytes, at + 12, pointer + resource(sources, pointer + 12, 4));
    put(sprite.bytes, at, pointer + resource(sources, pointer + 8, 4));
    environment.binding_control = 0;
    put(sprite.bytes, at + 16, pointer + resource(sources, pointer + 4, 4));
    put(sprite.bytes, 0x44, pointer);
    put(sprite.bytes, 0x3c, get(sprite.bytes, 0x3c) | 0x40000000);
}

void update_sprite_matrix(SpriteWindow sprite, const SpriteSources &sources) {
    check_window(sprite);
    require(sources.trigonometry.size() == 0x4000, "Original sprite trig table is incomplete");
    require_recovered(!(get(sprite.bytes, 0x40) & 1),
                      "Alternate GTE sprite matrix product is unreconstructed");
    const auto at = inside(sprite, get(sprite.bytes, 0x20), 44);
    std::array<std::int32_t, 3> sine, cosine, scales;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto index = (get(sprite.bytes, at + i * 2, 2) & 0xfff) * 4;
        sine[i] = signed_half(get(sources.trigonometry, index, 2));
        cosine[i] = signed_half(get(sources.trigonometry, index + 2, 2));
        scales[i] = signed_half(get(sprite.bytes, at + 6 + i * 2, 2));
    }
    const auto [sx, sy, sz] = sine;
    const auto [cx, cy, cz] = cosine;
    const auto yz = fixed_product(cz, -sy);
    const auto yz2 = fixed_product(sz, -sy);
    const auto add = [](std::int32_t a, std::int32_t b) {
        return signed_word(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
    };
    const auto negative_product = [](std::int32_t a, std::int32_t b) {
        return signed_word(0U - static_cast<std::uint32_t>(product(a, b))) >> 12;
    };
    const std::array<std::int32_t, 9> rotation{fixed_product(cz, cy),
                                               negative_product(sz, cy),
                                               sy,
                                               add(fixed_product(sz, cx), -fixed_product(yz, sx)),
                                               add(fixed_product(cz, cx), fixed_product(yz2, sx)),
                                               negative_product(cy, sx),
                                               add(fixed_product(yz, cx), fixed_product(sz, sx)),
                                               add(fixed_product(cz, sx), -fixed_product(yz2, cx)),
                                               fixed_product(cy, cx)};
    for (std::size_t i = 0; i < rotation.size(); ++i)
        put(sprite.bytes, at + 12 + i * 2, static_cast<std::uint32_t>(rotation[i]), 2);
    const auto scale_matrix = [&](const std::array<std::int32_t, 3> &scale, bool columns) {
        for (std::size_t i = 0; i < 9; ++i) {
            const auto value = fixed_product(signed_half(get(sprite.bytes, at + 12 + i * 2, 2)),
                                             scale[columns ? i % 3 : i / 3]);
            put(sprite.bytes, at + 12 + i * 2, static_cast<std::uint32_t>(value), i == 8 ? 4 : 2);
        }
    };
    scale_matrix(scales, false);
    const auto extra = get(sprite.bytes, 0x3a, 2);
    if (extra != 0) {
        const auto value = static_cast<std::int32_t>(extra >> 1);
        scale_matrix({value, value, value}, true);
    }
}

void schedule_sprite_frame(SpriteWindow sprite, std::uint32_t frame, SpriteEnvironment &environment,
                           const SpriteSources &sources) {
    check_window(sprite);
    if ((get(sprite.bytes, 0x3c) & 3) != 1) {
        put(sprite.bytes, 0x34, 0, 2);
        return;
    }
    const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 64);
    if (get(sprite.bytes, 0x40) & 0x100000) {
        put(sprite.bytes, 0x40, get(sprite.bytes, 0x40) & ~0x100000U);
        if (get(sprite.bytes, renderer + 0x34) != 0)
            clear_auxiliary(sprite);
    }
    if (get(sprite.bytes, 0x40) & 0x20000) {
        std::unordered_set<std::uint32_t> seen;
        auto node = environment.frame_head;
        while (node != 0) {
            require(seen.insert(node).second, "Original sprite frame list is cyclic");
            observe(sources, "sprite_frame_list", 0x8001d2b0);
            if (node == sprite.address) {
                const auto binding = get(sprite.bytes, 0x24);
                if (binding != 0x8005a474 && binding != 0x8006be10 &&
                    !(get(sprite.bytes, 0x40) & 0x80000))
                    previous_frame(sprite, get(sprite.bytes, 0x34, 2), binding, sources);
                put(sprite.bytes, 0x34, frame, 2);
                return;
            }
            const auto next_renderer = read(sources.frame_list, node + 0x20, 4);
            node = read(sources.frame_list, next_renderer + 0x38, 4);
        }
    }
    put(sprite.bytes, 0x34, frame, 2);
    put(sprite.bytes, 0x40, get(sprite.bytes, 0x40) | 0x20000);
    put(sprite.bytes, renderer + 0x38, environment.frame_head);
    environment.frame_head = sprite.address;
}

void replay_sprite_commands(SpriteWindow sprite, std::uint32_t target, std::uint32_t target_step,
                            SpriteEnvironment &environment, const SpriteSources &sources) {
    check_window(sprite);
    auto duration = sources.incoming_replay_duration;
    for (;;) {
        const auto pointer = get(sprite.bytes, 0x64);
        auto counter = (get(sprite.bytes, 0xa8) >> 22) & 63;
        if (pointer == target && counter == target_step)
            return;
        observe(sources, "sprite_facing_replay", 0x80022660, pointer);
        const auto opcode = resource(sources, pointer, 1);
        if (opcode < 0x80) {
            put(sprite.bytes, 0x64, pointer + 1);
            if (opcode < 0x10) {
                schedule_sprite_frame(sprite, get(sprite.bytes, 0x34, 2) + 1, environment, sources);
            } else if (opcode < 0x20) {
                put(sprite.bytes, 0xa8,
                    (get(sprite.bytes, 0xa8) & 0xfffe07ff) |
                        ((((get(sprite.bytes, 0xa8) >> 11) + 1) & 63) << 11));
                lookup_frame(sprite, environment, sources);
            } else if (opcode < 0x30) {
                schedule_sprite_frame(sprite, get(sprite.bytes, 0x34, 2) - 1, environment, sources);
            }
            if (opcode < 0x40)
                duration = static_cast<std::int32_t>((opcode & 15) + 1);
            require(duration.has_value(), "Original facing replay requires incoming S3 duration");
            counter = ((get(sprite.bytes, 0xa8) >> 22) + 1) & 63;
            if (counter == 0)
                counter = 63;
            put(sprite.bytes, 0x9e,
                get(sprite.bytes, 0x9e, 2) + static_cast<std::uint32_t>(*duration), 2);
            put(sprite.bytes, 0xa8, (get(sprite.bytes, 0xa8) & 0xf03fffff) | (counter << 22));
            continue;
        }
        if (opcode == 0x80 || opcode == 0x81 || opcode == 0x82)
            return;
        if ((opcode == 0x86 || opcode == 0x87 || opcode == 0x97) && pointer == target)
            return;
        if (opcode == 0xb3) {
            put(sprite.bytes, 0xa8,
                (get(sprite.bytes, 0xa8) & 0xfffe07ff) |
                    ((resource(sources, pointer + 1, 1) & 63) << 11));
        } else if (opcode == 0xbe) {
            const auto value = resource(sources, pointer + 1, 2);
            put(sprite.bytes, 0xac, (get(sprite.bytes, 0xac) & ~8U) | ((value >> 6) & 8));
            render_flip(sprite);
            if (get(sprite.bytes, 0x34, 2) != (value & 0x1ff))
                schedule_sprite_frame(sprite, value & 0x1ff, environment, sources);
            put(sprite.bytes, 0x9e, get(sprite.bytes, 0x9e, 2) + 1 + ((value >> 11) & 15), 2);
        } else if (opcode == 0xe2) {
            const auto delta = signed_half(resource(sources, pointer + 1, 2));
            put(sprite.bytes, 0x8c, get(sprite.bytes, 0x8c, 1) - 3, 1);
            for (unsigned i = 0; i < 3; ++i) {
                const auto cursor = std::bit_cast<std::int8_t>(sprite.bytes[0x8c]);
                put(sprite.bytes, static_cast<std::size_t>(0x8e + cursor) + i,
                    (pointer + 3) >> (8 * i), 1);
            }
            put(sprite.bytes, 0x64, get(sprite.bytes, 0x64) + static_cast<std::uint32_t>(delta));
            continue;
        }
        require(sources.replay_widths.size() == 256, "Original replay width table is incomplete");
        const auto width = sources.replay_widths[opcode];
        require(width != 0, "Original facing replay has a zero-width command");
        put(sprite.bytes, 0x64, get(sprite.bytes, 0x64) + width);
    }
}

void select_sprite_orientation(SpriteWindow sprite, std::int16_t angle,
                               SpriteEnvironment &environment, const SpriteSources &sources) {
    check_window(sprite);
    put(sprite.bytes, 0x80, static_cast<std::uint16_t>(angle), 2);
    auto flags = get(sprite.bytes, 0xac);
    put(sprite.bytes, 0xac, ((angle + 0x400) & 1) ? flags | 4 : flags & ~4U);
    if (get(sprite.bytes, 0x48) == 0)
        return;
    const auto old = (get(sprite.bytes, 0xa8) >> 17) & 7;
    const auto mode = (get(sprite.bytes, 0xa8) >> 20) & 3;
    const auto header = get(sprite.bytes, 0x58);
    if (mode == 0) {
        const auto flip = ((angle + 0x400) & 0xfff) > 0x800;
        flags = get(sprite.bytes, 0xac);
        put(sprite.bytes, 0xac, flip ? flags | 4 : flags & ~4U);
        put(sprite.bytes, 0xa8, get(sprite.bytes, 0xa8) & 0xfff1ffff);
        put(sprite.bytes, 0x5c, header + 6);
        put(sprite.bytes, 0x54, header + 4 + resource(sources, header + 4, 2));
    } else if (mode == 1 || mode == 2) {
        auto group = static_cast<std::uint32_t>((angle + (mode == 1 ? 0x600 : 0x500)) >>
                                                (mode == 1 ? 10 : 9)) &
                     (mode == 1 ? 3U : 7U);
        const auto flip = group > (mode == 1 ? 2U : 4U);
        if (mode == 2 && flip)
            group = (group - 5) ^ 3;
        const auto index = (mode == 1 && group == 3) ? 1 : group;
        flags = get(sprite.bytes, 0xac);
        put(sprite.bytes, 0xac, flip ? flags | 4 : flags & ~4U);
        const auto slot = header + 4 + index * 2;
        put(sprite.bytes, 0x54, slot + resource(sources, slot, 2));
        put(sprite.bytes, 0xa8, (get(sprite.bytes, 0xa8) & 0xfff1ffff) | ((group & 7) << 17));
    }
    if (old != ((get(sprite.bytes, 0xa8) >> 17) & 7)) {
        const auto delay = get(sprite.bytes, 0x9e, 2);
        const auto target = get(sprite.bytes, 0x64);
        const auto step = (get(sprite.bytes, 0xa8) >> 22) & 63;
        put(sprite.bytes, 0xa8, (get(sprite.bytes, 0xa8) & 0xf03fffff) | 0x1f800);
        put(sprite.bytes, 0x64, header + 2 + resource(sources, header + 2, 2));
        replay_sprite_commands(sprite, target, step, environment, sources);
        put(sprite.bytes, 0x9e, delay, 2);
    }
    render_flip(sprite);
}

void select_sprite_animation(SpriteWindow sprite, std::int32_t animation,
                             SpriteEnvironment &environment, const SpriteSources &sources) {
    check_window(sprite);
    const auto pointer = get(sprite.bytes, 0x48);
    if (pointer == 0) {
        put(sprite.bytes, 0x64, 0);
        return;
    }
    put(sprite.bytes, 0xb0,
        get(sprite.bytes, 0x44) == pointer ? get(sprite.bytes, 0xb0) & ~0x400U
                                           : get(sprite.bytes, 0xb0) | 0x400);
    require_recovered(environment.platform_mode == 0,
                      "Alternate sprite platform animation selection is unreconstructed");
    const auto selected_resource = animation < 0 ? get(sprite.bytes, 0x4c) : pointer;
    bind_sprite_resource(sprite, selected_resource, environment, sources);
    put(sprite.bytes, 0xaf, static_cast<std::uint32_t>(animation), 1);
    const auto binding = inside(sprite, get(sprite.bytes, 0x24), 20);
    const auto directory = get(sprite.bytes, binding + 16);
    const auto index = animation < 0 ? ~static_cast<std::uint32_t>(animation)
                                     : static_cast<std::uint32_t>(animation);
    const auto header = directory + resource(sources, directory + 2 + 2 * index, 2);
    put(sprite.bytes, 0x40, get(sprite.bytes, 0x40) | 0x100000);
    put(sprite.bytes, 0x58, header);
    apply_header(sprite, header, environment, sources);
    select_sprite_orientation(sprite,
                              static_cast<std::int16_t>(signed_half(get(sprite.bytes, 0x80, 2))),
                              environment, sources);
}

std::uint32_t upload_sprite_images(std::uint32_t address, std::int16_t x, std::int16_t y,
                                   const SpriteSources &sources,
                                   const SpriteImageUploader &upload) {
    const auto count = signed_word(resource(sources, address, 4));
    auto cursor = address + (static_cast<std::uint32_t>(count) + 1U) * 4U;
    for (std::int32_t index = 0; index < count; ++index) {
        observe(sources, "sprite_image_record", 0x8002dde4);
        const auto kind = resource(sources, cursor, 4);
        if (kind != 0x1100 && kind != 0x1101)
            return 1;
        const auto origin_x = kind == 0x1100 ? x : signed_half(resource(sources, cursor + 4, 2));
        const auto origin_y = kind == 0x1100 ? y : signed_half(resource(sources, cursor + 6, 2));
        const auto width = signed_half(resource(sources, cursor + 12, 2));
        const auto height = signed_half(resource(sources, cursor + 14, 2));
        require_source(width >= 0 && height >= 0,
                       "Negative original image dimensions require hardware service recovery");
        SpriteImageUpload request{
            {static_cast<std::int16_t>(signed_half(static_cast<std::uint32_t>(origin_x) +
                                                   resource(sources, cursor + 8, 2))),
             static_cast<std::int16_t>(signed_half(static_cast<std::uint32_t>(origin_y) +
                                                   resource(sources, cursor + 10, 2))),
             static_cast<std::int16_t>(width), static_cast<std::int16_t>(height)},
            cursor + 16,
            {}};
        const auto size = static_cast<std::uint32_t>(width * height) * 2U;
        require_source(size <= 0x200000 && valid_extent(request.source_address, size),
                       "Original image exceeds address-qualified resource bounds");
        request.bytes.reserve(size);
        for (std::uint32_t i = 0; i < size; ++i)
            request.bytes.push_back(
                static_cast<std::uint8_t>(resource(sources, request.source_address + i, 1)));
        require_recovered(static_cast<bool>(upload), "LoadImage service is not connected");
        observe(sources, "sprite_image_upload", 0x80044894);
        upload(request);
        cursor = request.source_address + size;
    }
    return 0;
}

namespace {
// Resident 80021c20 / 80021ca0: the sprite's byte stack of loop counts
// (+8e, indexed by the signed byte +8c, growing downward).
std::uint32_t pop_sprite_count(SpriteWindow sprite) {
    const auto top = get(sprite.bytes, 0x8c, 1);
    const auto value = get(sprite.bytes,
                           inside(sprite,
                                  sprite.address + 0x8e +
                                      static_cast<std::uint32_t>(std::bit_cast<std::int8_t>(
                                          static_cast<std::uint8_t>(top))),
                                  1),
                           1);
    put(sprite.bytes, 0x8c, top + 1, 1);
    return value;
}
void push_sprite_count(SpriteWindow sprite, std::uint32_t value) {
    const auto top = (get(sprite.bytes, 0x8c, 1) - 1) & 0xffU;
    put(sprite.bytes, 0x8c, top, 1);
    const auto at = inside(
        sprite,
        sprite.address + 0x8e +
            static_cast<std::uint32_t>(std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(top))),
        1);
    put(sprite.bytes, at, value, 1);
}
} // namespace

std::uint32_t execute_sprite_commands(SpriteWindow sprite, SpriteEnvironment &environment,
                                      const SpriteSources &sources) {
    check_window(sprite);
    require_recovered(environment.platform_mode == 0,
                      "Alternate original sprite VM 800c11cc is unreconstructed");
    auto duration = sources.incoming_replay_duration;
    std::uint32_t commands = 0;
    while (get(sprite.bytes, 0x9e, 2) == 0) {
        const auto pointer = get(sprite.bytes, 0x64);
        observe(sources, "sprite_command", 0x800248d4, pointer);
        const auto opcode = resource(sources, pointer, 1);
        ++commands;
        if (opcode < 0x80) {
            put(sprite.bytes, 0x64, pointer + 1);
            if (opcode < 0x10) {
                schedule_sprite_frame(sprite, get(sprite.bytes, 0x34, 2) + 1, environment, sources);
            } else if (opcode < 0x20) {
                put(sprite.bytes, 0xa8,
                    (get(sprite.bytes, 0xa8) & 0xfffe07ff) |
                        ((((get(sprite.bytes, 0xa8) >> 11) + 1) & 63) << 11));
                lookup_frame(sprite, environment, sources);
            } else if (opcode < 0x30) {
                schedule_sprite_frame(sprite, get(sprite.bytes, 0x34, 2) - 1, environment, sources);
            }
            if (opcode < 0x40)
                duration = static_cast<std::int32_t>((opcode & 15) + 1);
            require(duration.has_value(),
                    "Original ordinary frame command requires incoming S3 duration");
            auto delay = truncate_shift(
                product(*duration,
                        static_cast<std::int32_t>((get(sprite.bytes, 0xac) >> 7) & 0xfff)),
                8);
            if (delay == 0)
                delay = 1;
            const auto flags = get(sprite.bytes, 0xa8);
            auto ordinal = ((flags >> 22) + 1) & 63;
            if (ordinal == 0)
                ordinal = 63;
            put(sprite.bytes, 0x9e, get(sprite.bytes, 0x9e, 2) + static_cast<std::uint32_t>(delay),
                2);
            put(sprite.bytes, 0xa8, (flags & 0xf03fffff) | (ordinal << 22));
            return commands;
        }
        if (opcode == 0x80 || (opcode == 0x81 && get(sprite.bytes, 0xaf, 1) == 63)) {
            put(sprite.bytes, 0xa8, get(sprite.bytes, 0xa8) & 0xcfffffffU);
            if (get(sprite.bytes, 0x68) != 0) {
                invoke_sprite_callback(sprite, sources);
                return commands;
            }
            const auto animation = std::bit_cast<std::int8_t>(sprite.bytes[0xb0]);
            if (animation >= 0)
                select_sprite_animation(sprite, animation, environment, sources);
            put(sprite.bytes, 0xa8, get(sprite.bytes, 0xa8) & 0xcfffffffU);
            return commands;
        } else if (opcode == 0x82) {
            // Restart the current animation (+af), keeping the motion word
            // +10, and run it at once (800248d4 re-entered).
            if (get(sprite.bytes, 0x68) != 0)
                invoke_sprite_callback(sprite, sources);
            const auto animation = std::bit_cast<std::int8_t>(sprite.bytes[0xaf]);
            const auto motion = get(sprite.bytes, 0x10);
            select_sprite_animation(sprite, animation, environment, sources);
            put(sprite.bytes, 0x10, motion);
            put(sprite.bytes, 0x9e, 0, 2);
            return commands + execute_sprite_commands(sprite, environment, sources);
        } else if (opcode == 0x81) {
            put(sprite.bytes, 0x9e, 0, 2);
            invoke_sprite_callback(sprite, sources);
            put(sprite.bytes, 0xa8, (get(sprite.bytes, 0xa8) & 0xcfffffffU) | 0x10000000);
            return commands;
        } else if (opcode == 0x8d) {
            const auto binding = get(sprite.bytes, 0x24);
            const auto x = owned_read(sprite, sources, binding + 4, 2);
            const auto y = owned_read(sprite, sources, binding + 6, 2);
            observe(sources, "sprite_texture_page", 0x8002cc10);
            // 80043a1c with format/blend zero, then 8002cc10's low-five-bit
            // mask: Y bit9's high texture-page bit is deliberately discarded.
            environment.texture_page = ((x & 0x3ffU) >> 6) | ((y & 0x100U) >> 4);
            environment.texture_mode = 1;
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0x96) {
            require_recovered(sources.tasks != nullptr, "Sprite task ownership is not connected");
            remove_sprite_tasks(*sources.tasks, get(sprite.bytes, 0x6c), sprite, sources);
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xa3) {
            const auto flags = get(sprite.bytes, 0xa8);
            if ((flags & 1) != 0) {
                const auto reference = owned_read(sprite, sources, get(sprite.bytes, 0x7c) + 4, 4);
                if (reference != 0) {
                    put(sprite.bytes, 0x1c, reference);
                    static_cast<void>(store_sprite_command_pc(
                        sprite, static_cast<std::uint8_t>(opcode), sources.replay_widths));
                    continue;
                }
            }
            const auto operand = std::bit_cast<std::int8_t>(
                static_cast<std::uint8_t>(resource(sources, pointer + 1, 1)));
            const auto scale = signed_half(get(sprite.bytes, 0x82, 2));
            const auto scaled = truncate_shift(product(operand * 64, scale), 12);
            const auto divisor = (get(sprite.bytes, 0xac) >> 7) & 0xfffU;
            require_recovered(divisor != 0, "Original A3 zero-divisor behavior is unreconstructed");
            const auto quotient = static_cast<std::int32_t>(0x10000U / divisor);
            const auto squared = truncate_shift(product(quotient, quotient), 8);
            const auto initial =
                product(signed_word(static_cast<std::uint32_t>(scaled) << 5), squared);
            put(sprite.bytes, 0x1c, static_cast<std::uint32_t>(initial));
            const auto rate = signed_word(static_cast<std::uint32_t>(environment.rate_control) + 1);
            put(sprite.bytes, 0x1c,
                static_cast<std::uint32_t>(
                    product(truncate_shift(initial, 8), product(rate, rate))));
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xbc) {
            const auto selector = resource(sources, pointer + 1, 1);
            if (selector == 0xa4 || selector == 0xa5) {
                require_recovered(sources.tasks != nullptr,
                                  "BC task-owner storage is not connected");
                const auto owner = get(sprite.bytes, 0x6c);
                bool found = false;
                for (auto &node : sources.tasks->nodes) {
                    if (owner < node.address ||
                        static_cast<std::uint64_t>(owner) + 24 >
                            static_cast<std::uint64_t>(node.address) + node.bytes.size())
                        continue;
                    const auto at = static_cast<std::size_t>(owner - node.address) + 20;
                    const auto flags = get(node.bytes, at);
                    put(node.bytes, at,
                        selector == 0xa4 ? flags | 0x40000000U : flags & ~0x40000000U);
                    found = true;
                    break;
                }
                require_source(found, "BC task owner has no owned node");
                for (const auto offset : {0U, 4U, 8U})
                    put(sprite.bytes, offset, get(sprite.bytes, offset + 2, 2) << 16);
            } else if (selector == 0x96) {
                const auto owner = get(sprite.bytes, 0x70);
                require_source(owner != 0, "BC parent sprite pointer is absent");
                for (const auto offset : {0U, 4U, 8U}) {
                    const auto integer = owned_read(sprite, sources, owner + offset + 2, 2);
                    put(sprite.bytes, offset, integer << 16);
                }
            } else {
                require_recovered(false, "Original BC position selector is unreconstructed");
            }
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0x94) {
            if ((get(sprite.bytes, 0x3c) & 3) == 2) {
                const auto owner = get(sprite.bytes, 0x70);
                require_source(owner != 0, "94 parent sprite pointer is absent");
                const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 4);
                put(sprite.bytes, renderer + 2, owned_read(sprite, sources, owner + 0x32, 2), 2);
                put(sprite.bytes, 0x3c, get(sprite.bytes, 0x3c) | 0x10000000U);
            }
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xa0 || opcode == 0xa1) {
            if (opcode == 0xa0) {
                const auto operand = static_cast<std::uint8_t>(resource(sources, pointer + 1, 1));
                static_cast<void>(apply_animation_speed(sprite, operand, environment.rate_control,
                                                        sources.trigonometry));
            } else {
                std::optional<SpriteResource> reference;
                std::array<std::uint8_t, 4> external{};
                if (get(sprite.bytes, 0xa8) & 1) {
                    const auto address = get(sprite.bytes, 0x7c);
                    if (address >= sprite.address &&
                        static_cast<std::uint64_t>(address) + 4 <=
                            static_cast<std::uint64_t>(sprite.address) + sprite.bytes.size()) {
                        reference = SpriteResource{
                            address, sprite.bytes.subspan(address - sprite.address, 4)};
                    } else {
                        put(external, 0, resource(sources, address, 4));
                        reference = SpriteResource{address, external};
                    }
                }
                const auto operand =
                    reference && get(reference->bytes, 0) != 0
                        ? std::uint8_t{0}
                        : static_cast<std::uint8_t>(resource(sources, pointer + 1, 1));
                static_cast<void>(
                    apply_animation_impulse(sprite, operand, environment.rate_control, reference));
            }
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xb3) {
            put(sprite.bytes, 0xa8,
                (get(sprite.bytes, 0xa8) & 0xfffe07ff) |
                    ((resource(sources, pointer + 1, 1) & 63) << 11));
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xc6) {
            if (get(sprite.bytes, 0xa8) & 1) {
                const auto sequencer = inside(sprite, get(sprite.bytes, 0x7c), 14);
                put(sprite.bytes, sequencer + 12, resource(sources, pointer + 1, 1), 2);
            }
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xf5) {
            const auto operand = pointer + 1;
            auto delta = resource(sources, operand, 3);
            if (delta & 0x800000U)
                delta |= 0xff000000U;
            construct_sprite_model(sprite, operand + delta, environment, sources);
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xfc) {
            require_recovered(sources.services && sources.services->upload_state,
                              "Sprite upload ownership is not connected");
            auto &services = *sources.services;
            auto &state = *services.upload_state;
            require(state.outer_stack.bytes.empty() && state.inner_stack.bytes.empty(),
                    "Interrupted sprite upload has no represented continuation");
            require_recovered(static_cast<bool>(services.release),
                              "Sprite upload release service is not connected");
            state.outer_stack = allocation(services.allocate, 8192, 0);
            const auto operand = pointer + 1;
            auto delta = resource(sources, operand, 3);
            if (delta & 0x800000U)
                delta |= 0xff000000U;
            const auto binding = inside(sprite, get(sprite.bytes, 0x24), 8);
            state.resource = operand + delta;
            state.x = static_cast<std::int16_t>(signed_half(get(sprite.bytes, binding + 4, 2)));
            state.y = static_cast<std::int16_t>(signed_half(get(sprite.bytes, binding + 6, 2)));
            state.inner_stack = allocation(services.allocate, 8192, 1);
            static_cast<void>(upload_sprite_images(state.resource, state.x, state.y, sources,
                                                   services.upload_image));
            services.release(state.inner_stack.address);
            state.inner_stack = {};
            services.release(state.outer_stack.address);
            state.outer_stack = {};
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xe0) {
            const auto operand = pointer + 1;
            const auto delta = signed_half(resource(sources, operand, 2));
            create_child_sprite(sprite, operand + static_cast<std::uint32_t>(delta), environment,
                                sources);
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xe1) {
            const auto delta = signed_half(resource(sources, pointer + 1, 2));
            put(sprite.bytes, 0x64, get(sprite.bytes, 0x64) + static_cast<std::uint32_t>(delta));
        } else if (opcode == 0xa6) {
            // 8001fbe4 A6: add a rate-scaled speed to +10 unless +a8 bit 0.
            if ((get(sprite.bytes, 0xa8) & 1) != 1) {
                const auto operand = std::bit_cast<std::int8_t>(
                    static_cast<std::uint8_t>(resource(sources, pointer + 1, 1)));
                const auto rate =
                    signed_word(static_cast<std::uint32_t>(environment.rate_control) + 1);
                const auto scaled = truncate_shift(
                    product(product(operand * 16, rate), signed_half(get(sprite.bytes, 0x82, 2))),
                    12);
                const auto divisor =
                    static_cast<std::int32_t>((get(sprite.bytes, 0xac) >> 7) & 0xfff);
                require_recovered(divisor != 0,
                                  "Original A6 zero-divisor behavior is unreconstructed");
                const auto delta = signed_word(static_cast<std::uint32_t>(scaled) << 16) / divisor;
                put(sprite.bytes, 0x10,
                    get(sprite.bytes, 0x10) + static_cast<std::uint32_t>(delta));
            }
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xce) {
            // 8001fbe4 CE: add to (or with bit 12 set) the renderer's +2 angle
            // or an auxiliary record's +4 (group in bits 9-11).
            const auto value = resource(sources, pointer + 1, 2);
            const auto renderer = get(sprite.bytes, 0x20);
            if (renderer != 0) {
                auto amount = (value & 0x1ffU) * 8;
                if ((get(sprite.bytes, 0xac) >> 2) & 1)
                    amount = 0U - amount;
                const auto group = (value >> 9) & 7U;
                const auto set = ((value >> 12) & 1U) != 0;
                const auto at = inside(sprite, renderer, 0x38);
                if (group != 0) {
                    if (const auto records = get(sprite.bytes, at + 0x34); records != 0) {
                        const auto record = inside(sprite, records + group * 8, 8);
                        put(sprite.bytes, record + 4,
                            set ? amount : get(sprite.bytes, record + 4, 2) + amount, 2);
                    }
                } else {
                    put(sprite.bytes, at + 2, set ? amount : get(sprite.bytes, at + 2, 2) + amount,
                        2);
                    put(sprite.bytes, 0x3c, get(sprite.bytes, 0x3c) | 0x10000000U);
                }
            }
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0x83 || opcode == 0x84 || opcode == 0x88 || opcode == 0x89) {
            // Dispatched to 8001fbe4, whose table starts at 8a: no effect.
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xb4) {
            // 8001fbe4 B4: push a loop count.
            push_sprite_count(sprite, resource(sources, pointer + 1, 1));
            static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                      sources.replay_widths));
        } else if (opcode == 0xe4) {
            // Loop end: a nonzero count is pushed back less one and jumps as E1.
            const auto count = pop_sprite_count(sprite) & 0xffU;
            if (count == 0) {
                static_cast<void>(store_sprite_command_pc(sprite, static_cast<std::uint8_t>(opcode),
                                                          sources.replay_widths));
            } else {
                push_sprite_count(sprite, count - 1);
                const auto delta = signed_half(resource(sources, pointer + 1, 2));
                put(sprite.bytes, 0x64,
                    get(sprite.bytes, 0x64) + static_cast<std::uint32_t>(delta));
            }
        } else {
            throw UnrecoveredSpriteCommand(pointer, static_cast<std::uint8_t>(opcode));
        }
    }
    return commands;
}

std::uint32_t advance_sprite_timer(SpriteWindow sprite, SpriteEnvironment &environment,
                                   const SpriteSources &sources) {
    check_window(sprite);
    if (environment.rate_control == -1)
        return 0;
    std::uint32_t commands = 0;
    for (std::uint32_t iteration = 1;; ++iteration) {
        observe(sources, "sprite_timer", 0x80023210, get(sprite.bytes, 0x64));
        const auto timer = get(sprite.bytes, 0x9e, 2);
        if (timer != 0) {
            put(sprite.bytes, 0x9e, timer - 1, 2);
            if (timer == 1)
                commands += execute_sprite_commands(sprite, environment, sources);
        }
        if (iteration == static_cast<std::uint32_t>(environment.rate_control) + 1)
            return commands;
    }
}

std::uint32_t restore_sprite_checkpoint(SpriteWindow sprite,
                                        std::span<const std::uint8_t> checkpoint,
                                        SpriteEnvironment &environment,
                                        const SpriteSources &sources) {
    check_window(sprite);
    require(checkpoint.size() == 48, "Incomplete original sprite checkpoint");
    const auto target = signed_half(get(checkpoint, 0x18, 2));
    require(target >= 0 && target <= 63, "Original sprite checkpoint has unreachable six-bit step");
    const auto saved_rate = environment.rate_control;
    environment.rate_control = 0;
    put(sprite.bytes, 0x80, get(checkpoint, 0x10, 2), 2);
    put(sprite.bytes, 0xaf, get(checkpoint, 0x14, 1), 1);
    put(sprite.bytes, 0xb0, get(checkpoint, 0x16, 1), 1);
    for (std::size_t i = 0; i < 3; ++i) {
        const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 12);
        put(sprite.bytes, renderer + 6 + i * 2, get(checkpoint, 0x24 + i * 2, 2), 2);
    }
    const auto animation = std::bit_cast<std::int8_t>(sprite.bytes[0xaf]);
    put(sprite.bytes, 0x82, get(checkpoint, 0x2c, 2), 2);
    put(sprite.bytes, 0x2c, get(checkpoint, 0x2a, 2), 2);
    select_sprite_animation(sprite, animation, environment, sources);
    std::uint32_t commands = 0;
    while (((get(sprite.bytes, 0xa8) >> 22) & 63) != static_cast<std::uint32_t>(target)) {
        observe(sources, "sprite_checkpoint", 0x80021d50, get(sprite.bytes, 0x64));
        commands += advance_sprite_timer(sprite, environment, sources);
        for (const auto position : {0U, 8U, 4U})
            put(sprite.bytes, position,
                get(sprite.bytes, position) + get(sprite.bytes, position + 12));
        put(sprite.bytes, 0x10, get(sprite.bytes, 0x10) + get(sprite.bytes, 0x1c));
    }
    for (const auto position : {0U, 4U, 8U})
        put(sprite.bytes, position, get(checkpoint, position));
    auto sequencer = inside(sprite, get(sprite.bytes, 0x7c), 8);
    put(sprite.bytes, sequencer, get(checkpoint, 0x1c));
    environment.rate_control = saved_rate;
    sequencer = inside(sprite, get(sprite.bytes, 0x7c), 8);
    put(sprite.bytes, sequencer + 4, get(checkpoint, 0x20));
    return commands;
}

SpriteCheckpointDecision select_sprite_checkpoint(std::span<const std::uint8_t> actor,
                                                  std::span<const std::uint8_t> checkpoint,
                                                  const std::array<std::uint32_t, 3> &saved_modes,
                                                  const std::array<std::uint8_t, 3> &current_modes,
                                                  std::uint32_t return_gate) {
    require(actor.size() == 0x138 && checkpoint.size() == 48,
            "Incomplete original actor or sprite checkpoint");
    SpriteCheckpointDecision result;
    std::copy(checkpoint.begin(), checkpoint.end(), result.checkpoint.begin());
    const auto animation = signed_half(get(actor, 0xea, 2));
    if (signed_half(get(actor, 0x124, 2)) != -1 && animation != 255)
        put(result.checkpoint, 0x14, static_cast<std::uint32_t>(animation), 2);
    bool changed = false;
    for (std::size_t i = 0; i < saved_modes.size(); ++i)
        changed = changed || saved_modes[i] != current_modes[i];
    result.decision = SpriteRestoreDecision::restore;
    if (get(actor, 4) & 0x1000000)
        result.decision = SpriteRestoreDecision::actor_layer_flag;
    else if (return_gate && (get(actor, 0) & 0x600) && changed)
        result.decision = SpriteRestoreDecision::party_mode_change;
    result.record_bytes =
        0x174 + ((get(actor, 0x134) & 0x80) ? 12U : 0U) + ((get(actor, 0x12c) & 0x1000) ? 16U : 0U);
    return result;
}

UnrecoveredSpriteCommand::UnrecoveredSpriteCommand(std::uint32_t at, std::uint8_t value)
    : SpriteError("Unreconstructed ordinary sprite command at " + std::to_string(at) + ": " +
                  std::to_string(value)),
      command_pc(at), opcode(value) {}

void construct_sprite(SpriteConstruction &result, SpriteAllocation incoming, std::uint32_t pointer,
                      const std::array<std::int16_t, 4> &coordinates,
                      SpriteEnvironment initial_environment, const SpriteSources &sources,
                      const SpriteAllocator &allocate, const SpriteConstructionObserver &observe) {
    require(result.sprite.address == 0 && result.parts.address == 0 &&
                result.sprite.bytes.empty() && result.parts.bytes.empty(),
            "Sprite construction requires an unowned output");
    result.sprite = std::move(incoming);
    result.environment = initial_environment;
    auto &environment = result.environment;
    require(result.sprite.bytes.size() == sprite_bytes,
            "Constructor requires exact 356-byte ownership");
    SpriteWindow sprite{result.sprite.address, result.sprite.bytes};
    check_window(sprite);
    require_recovered(environment.platform_mode == 0,
                      "Alternate platform sprite construction is unreconstructed");
    const auto emit = [&](std::string_view name, std::optional<std::uint32_t> count = {},
                          const SpriteAllocation *block = nullptr) {
        if (observe)
            observe({name, sprite.address, sprite.bytes, environment, count, block});
    };
    defaults(sprite, environment.rate_control);
    emit("constructor-after-defaults");
    bind_inline(sprite);
    emit("constructor-after-inline");
    renderer_scale(sprite, 0x1000);
    emit("constructor-after-scale");
    put(sprite.bytes, 0x3c, (get(sprite.bytes, 0x3c) & ~3U) | 1);
    put(sprite.bytes, 0x40, get(sprite.bytes, 0x40) & 0xfffe1fff);
    const auto sequencer = inside(sprite, get(sprite.bytes, 0x7c), 28);
    put(sprite.bytes, 0xa8, get(sprite.bytes, 0xa8) | 1);
    put(sprite.bytes, sequencer + 24, 0);
    put(sprite.bytes, 0x6c, sprite.address);
    const auto variant = environment.variant & 15;
    put(sprite.bytes, 0x3c,
        (get(sprite.bytes, 0x3c) & 0xff00ffff) | (variant << 20) | (variant << 16));
    const auto directory = pointer + resource(sources, pointer + 8, 4);
    const auto count = ((resource(sources, directory, 2) >> 9) & 63) * 24;
    emit("allocate-parts-before", count);
    result.parts = allocation(allocate, count);
    auto &parts = result.parts;
    require(parts.bytes.empty() ||
                static_cast<std::uint64_t>(parts.address) + parts.bytes.size() <= sprite.address ||
                static_cast<std::uint64_t>(sprite.address) + sprite.bytes.size() <= parts.address,
            "Original sprite and part allocations overlap");
    emit("allocate-parts-after", count, &parts);
    const auto renderer = inside(sprite, get(sprite.bytes, 0x20), 52);
    put(sprite.bytes, renderer + 44, parts.address);
    put(sprite.bytes, renderer + 48, parts.address);
    const auto binding = inside(sprite, get(sprite.bytes, 0x24), 20);
    put(sprite.bytes, binding + 4, static_cast<std::uint16_t>(coordinates[2]), 2);
    put(sprite.bytes, binding + 6, static_cast<std::uint16_t>(coordinates[3]), 2);
    put(sprite.bytes, binding + 8, static_cast<std::uint16_t>(coordinates[0]), 2);
    put(sprite.bytes, binding + 10, static_cast<std::uint16_t>(coordinates[1]), 2);
    put(sprite.bytes, 0x48, pointer);
    emit("binding-before");
    bind_sprite_resource(sprite, pointer, environment, sources);
    emit("binding-after");
    const auto animation_directory = get(sprite.bytes, binding + 16);
    const auto animations = resource(sources, animation_directory, 2) & 63;
    put(sprite.bytes, 0x60, animation_directory + 2 * (animations + 1));
    emit("animation-before");
    select_sprite_animation(sprite, 0, environment, sources);
    emit("constructor-after");
}

void create_sprite(SpriteConstruction &result, std::uint32_t pointer,
                   const std::array<std::int16_t, 5> &parameters, SpriteEnvironment environment,
                   const SpriteSources &sources, const SpriteAllocator &allocate,
                   const SpriteConstructionObserver &observe) {
    require(result.sprite.address == 0 && result.parts.address == 0 &&
                result.sprite.bytes.empty() && result.parts.bytes.empty(),
            "Sprite construction requires an unowned output");
    auto incoming = allocation(allocate, sprite_bytes);
    put(incoming.bytes, 0x86, sprite_bytes, 2);
    construct_sprite(result, std::move(incoming), pointer,
                     {parameters[0], parameters[1], parameters[2], parameters[3]}, environment,
                     sources, allocate, observe);
}

} // namespace xem::reconstruction::field
