#include "xem/reconstruction/field_motion.hpp"

#include <bit>

namespace xem::reconstruction::field {
namespace {
std::int32_t signed_word(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int16_t signed_half(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
void require(bool condition, const char *message) {
    if (!condition)
        throw SpriteError(message);
}
void check_sprite(SpriteWindow sprite) {
    require(sprite.address != 0 && sprite.bytes.size() >= 0xb4 &&
                sprite.bytes.size() <= (std::uint64_t{1} << 32U) - sprite.address,
            "Incomplete original motion sprite window");
}
std::uint32_t read(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    require(at <= bytes.size() && width <= bytes.size() - at,
            "Motion read exceeds supplied original bytes");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
void store(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
           std::size_t width = 4) {
    require(at <= bytes.size() && width <= bytes.size() - at,
            "Motion write exceeds supplied original bytes");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t product(std::int32_t a, std::int32_t b) {
    return signed_word(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t velocity_scalar(std::int32_t speed, std::uint32_t packed_flags) {
    const auto divisor = static_cast<std::int32_t>((packed_flags >> 7U) & 0xfffU);
    require(divisor != 0, "Original sprite velocity zero-divisor behavior is unreconstructed");
    const auto numerator = signed_word(static_cast<std::uint32_t>(speed >> 4) << 8U);
    return numerator / divisor;
}
SpritePlanarVector velocity_components(std::int32_t scalar, PlanarTrigonometry trig) {
    const auto x = product(trig.cosine >> 2, scalar) >> 6;
    const auto z =
        signed_word(0U - static_cast<std::uint32_t>(product(trig.sine >> 2, scalar))) >> 6;
    return {scalar, x, z};
}
} // namespace

MotionPrefixResult begin_field_motion(std::uint32_t actor_flags, std::int16_t previous_mode,
                                      std::int32_t actor_index, MotionControl &control,
                                      const FieldPassState &pass) {
    control.current_actor_index = actor_index;
    if ((actor_flags & 0x01000000U) != 0)
        return {};
    std::int16_t mode = ((actor_flags & 0x4000U) != 0 && (control.held_buttons & 0x40U) != 0 &&
                         pass.input_updated == 1)
                            ? 2
                            : 1;
    if ((actor_flags & 0x1800U) != 0 && (previous_mode == 1 || previous_mode == 2))
        mode = previous_mode;
    return {mode};
}

PlanarTrigonometry planar_trigonometry(std::span<const std::uint8_t> table, std::uint32_t angle) {
    require(table.size() == 0x4000, "Original motion trig table must contain 4096 pairs");
    const auto at = (angle & 0xfffU) * 4U;
    return {signed_half(read(table, at, 2)), signed_half(read(table, at + 2U, 2))};
}

SpritePlanarVector sprite_planar_vector(std::int32_t speed, std::uint32_t packed_flags,
                                        PlanarTrigonometry trig) {
    return velocity_components(velocity_scalar(speed, packed_flags), trig);
}

SpritePlanarVector rebuild_sprite_velocity(SpriteWindow sprite,
                                           std::span<const std::uint8_t> table) {
    check_sprite(sprite);
    const auto scalar =
        velocity_scalar(signed_word(read(sprite.bytes, 0x18)), read(sprite.bytes, 0xac));
    const auto vector =
        velocity_components(scalar, planar_trigonometry(table, read(sprite.bytes, 0x32, 2)));
    // The original stores X in the sine-call delay slot, then stores Z.
    store(sprite.bytes, 0x0c, static_cast<std::uint32_t>(vector.x));
    store(sprite.bytes, 0x14, static_cast<std::uint32_t>(vector.z));
    return vector;
}

std::int32_t animation_command_speed(std::uint8_t operand, std::int16_t scale,
                                     std::int32_t rate_control) {
    const auto signed_operand = static_cast<std::int32_t>(std::bit_cast<std::int8_t>(operand));
    const auto rate = signed_word(static_cast<std::uint32_t>(rate_control) + 1U);
    auto value = product(product(signed_operand * 16, rate), scale);
    if (value < 0)
        value = signed_word(static_cast<std::uint32_t>(value) + 0xfffU);
    return signed_word(static_cast<std::uint32_t>(value >> 12) << 8U);
}

SpritePlanarVector apply_animation_speed(SpriteWindow sprite, std::uint8_t operand,
                                         std::int32_t rate_control,
                                         std::span<const std::uint8_t> table) {
    check_sprite(sprite);
    const auto speed =
        animation_command_speed(operand, signed_half(read(sprite.bytes, 0x82, 2)), rate_control);
    store(sprite.bytes, 0x18, static_cast<std::uint32_t>(speed));
    return rebuild_sprite_velocity(sprite, table);
}

FieldPlanarVector update_field_party_velocity(SpriteWindow sprite, std::uint16_t direction,
                                              std::uint16_t descriptor_flags,
                                              std::optional<std::uint32_t> actor_flags,
                                              std::span<const std::uint8_t> table) {
    require((descriptor_flags & 0x40U) != 0, "Unreconstructed field velocity ratio path");
    check_sprite(sprite);
    if ((direction & 0x8000U) != 0) {
        store(sprite.bytes, 0x0c, 0);
        store(sprite.bytes, 0x14, 0);
        return {signed_half(read(sprite.bytes, 0x32, 2)), 0, 0, std::nullopt};
    }
    require(actor_flags.has_value() && (*actor_flags & 0x82000U) == 0,
            "Unreconstructed alternate actor velocity path");
    store(sprite.bytes, 0x32, direction, 2);
    const auto vector = rebuild_sprite_velocity(sprite, table);
    const auto x = static_cast<std::uint32_t>(vector.x) & 0xfffff000U;
    const auto z = static_cast<std::uint32_t>(vector.z) & 0xfffff000U;
    store(sprite.bytes, 0x0c, x);
    store(sprite.bytes, 0x14, z);
    return {signed_half(direction), signed_word(x), signed_word(z), vector};
}

std::uint32_t store_sprite_command_pc(SpriteWindow sprite, std::uint8_t opcode,
                                      std::span<const std::uint8_t> widths) {
    require(widths.size() == 256, "Incomplete original sprite command-width table");
    check_sprite(sprite);
    const auto value = read(sprite.bytes, 0x64) + widths[opcode];
    store(sprite.bytes, 0x64, value);
    return value;
}

void execute_motion_divisor(EventContext &context, std::uint16_t &actor_divisor,
                            SpriteWindow sprite) {
    if (context.current_actor == nullptr)
        throw EventError("Motion divisor requires a current event actor");
    auto &actor = *context.current_actor;
    const auto opcode = context.program.byte(actor.pc);
    if (opcode != 0x21)
        throw UnsupportedInstruction(actor.pc, opcode);
    const auto operand = context.program.word(static_cast<std::uint32_t>(actor.pc) + 1U);
    std::int32_t value = operand & 0x7fff;
    if ((operand & 0x8000U) == 0) {
        if (context.variables == nullptr)
            throw EventError("Motion divisor requires the selected event variable bank");
        value = context.variables->read(operand);
    }
    // Actor store precedes the resident sprite call. A bounded sprite error
    // retains that already-issued write and leaves the PC unchanged.
    actor_divisor = static_cast<std::uint16_t>(value);
    check_sprite(sprite);
    store(sprite.bytes, 0xac,
          (read(sprite.bytes, 0xac) & 0xfff8007fU) |
              ((static_cast<std::uint32_t>(value) & 0xfffU) << 7U));
    actor.pc = static_cast<std::uint16_t>(static_cast<std::uint32_t>(actor.pc) + 3U);
}

SpriteImpulse apply_animation_impulse(SpriteWindow sprite, std::uint8_t operand,
                                      std::int32_t rate_control,
                                      std::optional<SpriteResource> reference) {
    check_sprite(sprite);
    std::int32_t value = 0;
    if ((read(sprite.bytes, 0xa8) & 1U) != 0) {
        const auto pointer = read(sprite.bytes, 0x7c);
        require(reference && pointer != 0 && reference->address == pointer &&
                    reference->bytes.size() >= 4 &&
                    reference->bytes.size() <= (std::uint64_t{1} << 32U) - pointer,
                "Sprite impulse needs its address-qualified original velocity reference");
        value = signed_word(read(reference->bytes, 0));
    }
    const bool used_reference = value != 0;
    if (!used_reference)
        value = animation_command_speed(operand, signed_half(read(sprite.bytes, 0x82, 2)),
                                        rate_control);
    store(sprite.bytes, 0x10, static_cast<std::uint32_t>(value));
    const auto numerator = signed_word(static_cast<std::uint32_t>(value) << 8U);
    const auto divisor = static_cast<std::int32_t>((read(sprite.bytes, 0xac) >> 7U) & 0xfffU);
    require(divisor != 0, "Original sprite impulse zero-divisor behavior is unreconstructed");
    const auto velocity = numerator / divisor;
    store(sprite.bytes, 0x10, static_cast<std::uint32_t>(numerator));
    store(sprite.bytes, 0x10, static_cast<std::uint32_t>(velocity));
    return {velocity, numerator, used_reference};
}

VerticalEffect field_vertical_step(VerticalState state, std::int32_t gravity, std::int16_t floor,
                                   std::int16_t layer, std::int16_t previous_layer,
                                   std::uint32_t terrain) {
    state.y = signed_word(static_cast<std::uint32_t>(state.y) +
                          static_cast<std::uint32_t>(state.velocity));
    const auto integrated_y = state.y;
    if (layer != previous_layer)
        state.flags &= 0xfbffffffU;
    VerticalBranch branch;
    if ((state.flags & 0x04000000U) == 0 &&
        signed_half(static_cast<std::uint32_t>(state.y) >> 16U) < floor) {
        state.velocity = signed_word(static_cast<std::uint32_t>(state.velocity) +
                                     static_cast<std::uint32_t>(gravity));
        state.flags |= 0x1000U;
        state.marker = state.velocity;
        branch = VerticalBranch::airborne;
    } else {
        if ((terrain & 0x00420000U) == 0)
            state.marker = 0;
        if (state.velocity > 0)
            state.velocity = 0;
        state.flags &= 0xffbfefffU;
        state.y = signed_word(static_cast<std::uint32_t>(floor) << 16U);
        branch = VerticalBranch::floor;
    }
    state.flags &= 0xfbffffffU;
    return {state, integrated_y, branch};
}

VerticalEffect apply_field_vertical(std::span<std::uint8_t> actor, SpriteWindow sprite,
                                    std::int16_t previous_layer, const CollisionPackage &mesh) {
    require(actor.size() >= 0x138, "Incomplete original vertical actor window");
    check_sprite(sprite);
    const VerticalState before{signed_word(read(actor, 0x24)),
                               signed_word(read(sprite.bytes, 0x10)), read(actor, 0),
                               signed_word(read(actor, 0xf0))};
    // This delay-slot store precedes the terrain call and survives its failure.
    store(actor, 0x24,
          static_cast<std::uint32_t>(before.y) + static_cast<std::uint32_t>(before.velocity));
    const auto layer = signed_half(read(actor, 0x10, 2));
    std::array<std::uint16_t, 4> triangles;
    for (std::size_t i = 0; i < triangles.size(); ++i)
        triangles[i] = static_cast<std::uint16_t>(read(actor, 8 + i * 2, 2));
    const auto terrain = actor_terrain_attribute(read(actor, 4), layer, triangles, mesh);
    const auto effect = field_vertical_step(before, signed_word(read(sprite.bytes, 0x1c)),
                                            signed_half(read(sprite.bytes, 0x84, 2)), layer,
                                            previous_layer, terrain);
    if (layer != previous_layer)
        store(actor, 0, before.flags & 0xfbffffffU);
    if (effect.branch == VerticalBranch::airborne) {
        store(sprite.bytes, 0x10, static_cast<std::uint32_t>(effect.state.velocity));
        store(actor, 0, effect.state.flags);
        store(actor, 0xf0, static_cast<std::uint32_t>(effect.state.marker));
    } else {
        if ((terrain & 0x00420000U) == 0)
            store(actor, 0xf0, 0);
        if (before.velocity > 0)
            store(sprite.bytes, 0x10, 0);
        store(actor, 0,
              (layer == previous_layer ? before.flags : before.flags & 0xfbffffffU) & 0xffbfefffU);
        store(actor, 0x24, static_cast<std::uint32_t>(effect.state.y));
    }
    store(actor, 0, effect.state.flags);
    return effect;
}

} // namespace xem::reconstruction::field
