#pragma once

#include "xem/reconstruction/field_actor.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace xem::reconstruction::field {

class CollisionError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
using CollisionEdge = std::array<FieldVector, 2>;
struct CollisionActor {
    std::uint32_t flags{}, layer_flags{};
    std::int16_t layer{}, triangle{};
    FieldVector position{};
    bool operator==(const CollisionActor &) const = default;
};
struct CollisionArea {
    std::uint32_t site;
    std::array<std::uint32_t, 3> points;
    std::int32_t value;
};
struct CollisionHeight {
    std::string_view site;
    std::int32_t triangle;
    std::array<FieldVector, 3> vertices;
    FieldVector point, normal;
};
struct CollisionStep {
    std::int32_t triangle, counter;
    std::uint32_t mask, target, origin;
    std::array<FieldVector, 3> vertices;
    std::int32_t next_triangle;
    std::uint32_t edge_mask, terrain;
};
struct CollisionAttribute {
    std::int32_t triangle;
    std::uint8_t index;
    std::uint32_t source_word, effective_word;
};
// Optional observations of computed source intermediates. No game behavior is
// supplied by the observer and ordinary runtime calls allocate no trace vectors.
struct CollisionTrace {
    std::optional<std::int32_t> counter;
    std::optional<std::uint32_t> mask, attribute_mask;
    std::optional<bool> initial_special;
    std::string_view reason;
    std::vector<CollisionStep> steps;
    std::vector<CollisionArea> areas;
    std::vector<CollisionHeight> heights;
    std::vector<CollisionAttribute> attribute_reads;
};
struct CollisionResult {
    std::int32_t value{-1};
    // Missing output means that the original leaves caller storage untouched.
    std::optional<FieldVector> point;
    std::optional<CollisionEdge> edge;
    std::optional<std::uint32_t> attribute;
    std::int32_t terminal_triangle{-1};
};
[[nodiscard]] std::uint32_t pack_collision_xz(std::int32_t x, std::int32_t z) noexcept;
// Field 8007bef4/8007c694, EVID-REF-024. Candidate Y is never read.
// Component includes bytes preceding triangle tables: selected neighbor -1
// still reads the attribute byte at triangle_base-2 before checking the sentinel.
// Bounds errors report unsupported original accesses, not successful collision.
[[nodiscard]] CollisionResult
query_field_collision(std::span<const std::uint8_t> component,
                      std::span<const std::int16_t> reciprocal, const CollisionActor &actor,
                      const FieldVector &candidate, bool ordinary, std::int32_t mode,
                      std::uint8_t attribute_control, CollisionTrace *trace = nullptr);

struct CollisionAngle {
    std::int32_t angle;
    std::optional<std::int32_t> table_index;
    std::string_view division_path;
};
struct CollisionSquareRoot {
    std::int32_t value;
    std::uint32_t leading_zeroes;
    std::optional<std::uint32_t> scale_shift, table_index, shifted_word;
};
// Original 8004b32c and 80048c4c. Tables are caller-owned original data.
// BREAK division paths and negative square-root inputs remain unsupported.
[[nodiscard]] CollisionAngle collision_atan(std::int32_t z, std::int32_t x,
                                            std::span<const std::int16_t> table);
[[nodiscard]] CollisionSquareRoot collision_sqrt(std::int32_t magnitude,
                                                 std::span<const std::int16_t> table);
[[nodiscard]] std::int32_t collision_planar_length(std::int32_t x, std::int32_t z,
                                                   std::span<const std::int16_t> table);
struct CollisionTables {
    std::span<const std::int16_t> reciprocal, square_root, angle;
    std::span<const std::uint8_t> trigonometry;
};
struct CollisionProjection {
    std::int32_t direction;
    FieldVector velocity;
    std::string_view branch;
    std::optional<FieldVector> normal_input, normal;
    std::optional<std::int32_t> length;
};
struct CollisionSlope {
    FieldVector velocity, normal_input, normal;
    std::array<std::int32_t, 2> length_input;
    std::int32_t length;
};
[[nodiscard]] CollisionProjection project_collision_edge(std::int16_t direction,
                                                         const CollisionEdge &edge,
                                                         const FieldVector &velocity,
                                                         const CollisionTables &tables);
[[nodiscard]] CollisionSlope project_collision_slope(const FieldVector &velocity,
                                                     std::int32_t actor_y, std::int16_t floor,
                                                     const CollisionTables &tables);
struct SweepActor {
    CollisionActor query;
    std::uint32_t terrain_flags{};
    std::uint8_t linked_actor{};
    std::int16_t floor{}, forced_floor{};
    bool operator==(const SweepActor &) const = default;
};
struct CollisionSweepQuery {
    std::string_view stage;
    FieldVector candidate;
    std::int32_t mode;
    CollisionResult result;
    CollisionTrace trace;
};
struct CollisionSweepTrace {
    std::vector<CollisionSweepQuery> queries;
    std::optional<CollisionProjection> projection;
    std::optional<CollisionSlope> slope;
    std::string_view slope_reason;
};
struct CollisionSweepResult {
    std::int32_t value;
    SweepActor actor;
    FieldVector velocity;
    CollisionEdge edge;
};
[[nodiscard]] bool uses_ordinary_collision_sweep(const SweepActor &actor,
                                                 std::uint32_t collision_mode) noexcept;
// Field 8007bac0/8007b814. Failure preserves velocity and actor, but retains
// preceding edge writes. Selection is explicit for other original callers.
[[nodiscard]] CollisionSweepResult
sweep_field_collision(std::span<const std::uint8_t> component, const CollisionTables &tables,
                      SweepActor actor, const FieldVector &velocity, CollisionEdge edge,
                      std::int16_t direction, bool ordinary, std::uint32_t collision_mode,
                      std::uint8_t attribute_control, CollisionSweepTrace *trace = nullptr);
// Connected original byte windows: actor312, velocity12, edge16. Selects the
// same ordinary/special path as 80082bb8. Edge padding and unknown actor bytes
// survive; this does not perform the later position/layer integration.
[[nodiscard]] std::int32_t
apply_field_collision_sweep(std::span<std::uint8_t> actor, std::span<std::uint8_t> velocity,
                            std::span<std::uint8_t> edge, std::int16_t direction,
                            std::span<const std::uint8_t> component, const CollisionTables &tables,
                            std::uint32_t collision_mode, std::uint8_t attribute_control,
                            CollisionSweepTrace *trace = nullptr);

} // namespace xem::reconstruction::field
