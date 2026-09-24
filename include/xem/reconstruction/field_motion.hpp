#pragma once

#include "xem/reconstruction/field_actor.hpp"
#include "xem/reconstruction/field_events.hpp"
#include "xem/reconstruction/field_sprite.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace xem::reconstruction::field {

struct MotionControl {
    std::int32_t current_actor_index{}; // 80065b08
    std::uint16_t held_buttons{};       // 800afe9c
};
struct MotionPrefixResult {
    // No value means the original returns with the whole body inhibited.
    // A value is only register S4 at 80082c8c: the active body must still run.
    std::optional<std::int16_t> active_body_mode;
    bool operator==(const MotionPrefixResult &) const = default;
};
// Field 80082bb8..80082c8c, EVID-REF-023. Always stores the actor index;
// does not change the actor's animation, countdown, position or velocity.
// pass is the EventContext::pass shared by the actual scheduler/control calls.
[[nodiscard]] MotionPrefixResult
begin_field_motion(std::uint32_t actor_flags, std::int16_t previous_mode, std::int32_t actor_index,
                   MotionControl &control, const FieldPassState &pass);

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
// Field 80081f80. actor is the descriptor's 312-byte record: divisor +76,
// layer flags +04 and ratio scales +f4/+f8. The stop sentinel clears X/Z; the
// ordinary party path rebuilds the sprite vector; ratio paths use the resident
// cosine/sine pair. The 801e8670 object-table path fails explicitly.
// Updates sprite direction/X/Z (and +18 on one path) only.
[[nodiscard]] FieldPlanarVector update_field_velocity(SpriteWindow sprite, std::uint16_t direction,
                                                      std::uint16_t descriptor_flags,
                                                      std::span<const std::uint8_t> actor,
                                                      std::span<const std::uint8_t> table);

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

struct SpriteImpulse {
    std::int32_t velocity;
    std::int32_t division_numerator;
    bool used_reference;
    bool operator==(const SpriteImpulse &) const = default;
};
// Resident sprite command A1, 800219ac..80021a44 (EVID-REF-022).
// When +a8 bit zero is set, reference must start at the original +7c pointer
// and contain its word, even if that word is zero. A nonzero word bypasses
// operand scaling, but not the final shift/division. An unsupported zero divisor
// retains the scaled value stored before the division. No PC advance.
[[nodiscard]] SpriteImpulse
apply_animation_impulse(SpriteWindow sprite, std::uint8_t operand, std::int32_t rate_control,
                        std::optional<SpriteResource> reference = std::nullopt);

struct VerticalState {
    std::int32_t y;
    std::int32_t velocity;
    std::uint32_t flags;
    std::int32_t marker; // Actor +f0; wider ownership remains separate.
    bool operator==(const VerticalState &) const = default;
};
enum class VerticalBranch { airborne, floor };
struct VerticalEffect {
    VerticalState state;
    std::int32_t integrated_y;
    VerticalBranch branch;
    bool operator==(const VerticalEffect &) const = default;
};
// Field 8008505c..8008515c, inside 80084a40. Integrates old velocity,
// then applies gravity or the previously selected signed floor. The caller
// still owns floor selection and the following ceiling/layer/rollback stages.
[[nodiscard]] VerticalEffect field_vertical_step(VerticalState state, std::int32_t gravity,
                                                 std::int16_t floor, std::int16_t layer,
                                                 std::int16_t previous_layer,
                                                 std::uint32_t terrain);
// Connected byte-window form: stores integrated Y before calling the recovered
// terrain reader. Actor and sprite storage must belong to the same descriptor.
// Unknown bytes survive; invalid terrain retains the already-issued Y store.
[[nodiscard]] VerticalEffect apply_field_vertical(std::span<std::uint8_t> actor,
                                                  SpriteWindow sprite, std::int16_t previous_layer,
                                                  const CollisionPackage &mesh);

} // namespace xem::reconstruction::field
