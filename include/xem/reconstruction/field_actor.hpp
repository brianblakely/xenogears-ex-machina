#pragma once

#include "xem/reconstruction/field_events.hpp"
#include "xem/reconstruction/packed_field.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>

namespace xem::reconstruction::field {

class ActorInitializationError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

using FieldVector = std::array<std::int32_t, 3>;
// 80048d7c/80048dd8 and 8004a480 in resident executable dc0b2dd7...;
// field 8007b07c/8007b1c4 in event_source_overlay_sha256. See EVID-REF-019/030.
// The reciprocal table is original resource data supplied by the caller.
[[nodiscard]] FieldVector normalize_field_vector(const FieldVector &vector,
                                                 std::span<const std::int16_t> reciprocal);
[[nodiscard]] std::int32_t field_edge_area(const FieldVector &a, const FieldVector &b,
                                           std::int32_t x, std::int32_t z);
struct FloorLocation {
    std::uint32_t triangle{};
    FieldVector point{};
    FieldVector normal{};
    // The original returns zero and clears components on failure. This flag
    // distinguishes that from a successful triangle-zero query for observers.
    bool matched{};
};
[[nodiscard]] FloorLocation locate_initial_floor(const CollisionLayer &layer,
                                                 std::int32_t active_triangle_count, std::int32_t x,
                                                 std::int32_t z,
                                                 std::span<const std::int16_t> reciprocal);
[[nodiscard]] std::uint32_t
actor_terrain_attribute(std::uint32_t disabled_layers, std::int16_t layer,
                        const std::array<std::uint16_t, 4> &triangle_indices,
                        const CollisionPackage &mesh);

struct RandomStep {
    std::uint32_t seed;
    std::uint16_t value;
};
// Resident 8003fa38. This is the original rand contract, not host libc rand.
[[nodiscard]] RandomStep advance_field_random(std::uint32_t seed) noexcept;

namespace original {
struct ActorDefaults {
    std::array<std::uint8_t, 0x138> actor;
    std::array<std::uint8_t, 0x5c> descriptor;
    // Original local storage: four 16-byte normals, then four 8-byte points.
    // Required input even when there are no queries; unwritten bytes survive.
    std::array<std::uint8_t, 0x60> scratch;
    std::uint32_t seed;
    std::array<std::uint32_t, 4> triangle_counts;
    std::uint32_t queried_layers{};
    [[nodiscard]] EventActor event_state() const { return read_event_actor(actor); }
};
enum class DefaultsStage {
    random_before,
    random_after,
    locate_before,
    locate_after,
    terrain_before,
    terrain_after,
    complete
};
struct DefaultsObservation {
    DefaultsStage stage;
    const ActorDefaults &state;
    std::int32_t layer{-1};
    FloorLocation query{};
    std::uint32_t terrain{};
};
// Read-only observer for exact original boundary comparisons; not a service or
// source of game behavior. References are valid only during the call.
using DefaultsObserver = std::function<void(const DefaultsObservation &)>;
// Field 80080a74: owned result retains the original correlation bytes and all
// unknown fields. Allocation, sprite creation and ready-state publication are
// separate required operations, not supplied by this function.
[[nodiscard]] ActorDefaults initialize_actor_defaults(ActorDefaults incoming,
                                                      std::int16_t layer_count,
                                                      const CollisionPackage &mesh,
                                                      std::span<const std::int16_t> reciprocal,
                                                      const DefaultsObserver &observe = {});
} // namespace original
} // namespace xem::reconstruction::field
