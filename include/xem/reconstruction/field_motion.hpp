#pragma once

#include "xem/reconstruction/field_events.hpp"
#include "xem/reconstruction/field_sprite.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace xem::reconstruction::field {

struct MotionControl {
    std::int32_t current_actor_index{}; // 80065b08
    std::uint16_t held_buttons{};       // 800afe9c
    std::uint32_t input_updated{};      // 800adb68
};
struct MotionPrefixResult {
    // No value means the original returns with the whole body inhibited.
    // A value is only register S4 at 80082c8c: the active body must still run.
    std::optional<std::int16_t> active_body_mode;
    bool operator==(const MotionPrefixResult &) const = default;
};
// Field 80082bb8..80082c8c, EVID-REF-023. Always stores the actor index;
// does not change the actor's animation, countdown, position or velocity.
[[nodiscard]] MotionPrefixResult begin_field_motion(std::uint32_t actor_flags,
                                                    std::int16_t previous_mode,
                                                    std::int32_t actor_index,
                                                    MotionControl &control);

struct PlanarTrigonometry {
    std::int16_t sine;
    std::int16_t cosine;
    bool operator==(const PlanarTrigonometry &) const = default;
};
// Resident 8003f8b0/8003f8cc. Exact caller-supplied 4096-pair little-endian
// table, shared with SpriteSources::trigonometry; never host trigonometry.
[[nodiscard]] PlanarTrigonometry planar_trigonometry(std::span<const std::uint8_t> table,
                                                     std::uint32_t angle);

struct SpritePlanarVector {
    std::int32_t scalar;
    std::int32_t x;
    std::int32_t z;
    bool operator==(const SpritePlanarVector &) const = default;
};
// Resident 80022974. Products and shifts retain the original low-word order.
// Zero division remains explicitly unsupported, not a successful zero vector.
[[nodiscard]] SpritePlanarVector
sprite_planar_vector(std::int32_t speed, std::uint32_t packed_flags, PlanarTrigonometry trig);
[[nodiscard]] SpritePlanarVector rebuild_sprite_velocity(SpriteWindow sprite,
                                                         std::span<const std::uint8_t> table);

// Resident sprite command A0, 80021958..800219a4, not field event opcode A0.
[[nodiscard]] std::int32_t animation_command_speed(std::uint8_t operand, std::int16_t scale,
                                                   std::int32_t rate_control);
// Stores speed before rebuilding X/Z. Does not advance the sprite command PC.
[[nodiscard]] SpritePlanarVector apply_animation_speed(SpriteWindow sprite, std::uint8_t operand,
                                                       std::int32_t rate_control,
                                                       std::span<const std::uint8_t> table);

struct FieldPlanarVector {
    std::int16_t angle;
    std::int32_t x;
    std::int32_t z;
    std::optional<SpritePlanarVector> sprite;
    bool operator==(const FieldPlanarVector &) const = default;
};
// Ordinary party path of field 80081f80. actor_flags is actor+04, whereas
// begin_field_motion uses actor+00. The stop sentinel precedes actor
// flags/table/divisor reads. Ratio, alternate actor and Gear paths throw.
// Updates sprite direction/X/Z only; final collision displacement is separate.
[[nodiscard]] FieldPlanarVector update_field_party_velocity(
    SpriteWindow sprite, std::uint16_t direction, std::uint16_t descriptor_flags,
    std::optional<std::uint32_t> actor_flags, std::span<const std::uint8_t> table);

// Resident 80024edc..80024efc: add a source width to the post-handler PC.
// This neither executes a command nor claims that every VM path stores a PC.
[[nodiscard]] std::uint32_t store_sprite_command_pc(SpriteWindow sprite, std::uint8_t opcode,
                                                    std::span<const std::uint8_t> widths);

// Primary field event 21, 8009e094 -> resident 80021bcc. The caller supplies
// the divisor and sprite belonging to context.current_actor_index. The full
// low halfword is stored in actor+76; only low twelve bits reach sprite+ac.
// This changes no vector and advances the current event PC by three.
void execute_motion_divisor(EventContext &context, std::uint16_t &actor_divisor,
                            SpriteWindow sprite);

} // namespace xem::reconstruction::field
