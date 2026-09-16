#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace xem::reconstruction::field {

class SpriteError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Addresses identify original resources and storage. They are never host pointers.
struct SpriteResource {
    std::uint32_t address;
    std::span<const std::uint8_t> bytes;
};
struct SpriteSources {
    std::span<const SpriteResource> resources;
    std::span<const SpriteResource> frame_list;
    std::span<const std::uint8_t> trigonometry;
    std::span<const std::uint8_t> replay_widths;
    std::optional<std::int32_t> incoming_replay_duration;
};
struct SpriteEnvironment {
    std::int32_t rate_control;
    std::uint8_t platform_mode;
    std::uint32_t variant;
    std::uint8_t binding_control;
    std::uint32_t frame_head;
    bool operator==(const SpriteEnvironment &) const = default;
};

// A borrowed, address-qualified window, not an allocation-size claim. Existing
// captures can include pointed storage after the 180-byte common prefix. The
// constructor below owns exactly 356 bytes, never a captured 512-byte window.
struct SpriteWindow {
    std::uint32_t address;
    std::span<std::uint8_t> bytes;
};
struct SpriteAllocation {
    std::uint32_t address;
    std::vector<std::uint8_t> bytes;
    bool operator==(const SpriteAllocation &) const = default;
};
using SpriteAllocator = std::function<SpriteAllocation(std::uint32_t bytes, std::uint32_t mode)>;

struct SpriteConstruction {
    SpriteAllocation sprite;
    SpriteAllocation parts;
    SpriteEnvironment environment;
};
struct SpriteConstructionObservation {
    std::string_view stage;
    std::uint32_t address;
    std::span<const std::uint8_t> sprite;
    const SpriteEnvironment &environment;
    std::optional<std::uint32_t> allocation_bytes;
    const SpriteAllocation *allocation;
};
using SpriteConstructionObserver = std::function<void(const SpriteConstructionObservation &)>;

// Resident 800222bc/80022224, 80022090, 800223b0 and 800245d8. Unknown
// platform/model branches fail explicitly. All byte inputs remain caller-owned.
void bind_sprite_resource(SpriteWindow sprite, std::uint32_t resource,
                          SpriteEnvironment &environment, const SpriteSources &sources);
void update_sprite_matrix(SpriteWindow sprite, const SpriteSources &sources);
void select_sprite_orientation(SpriteWindow sprite, std::int16_t angle,
                               SpriteEnvironment &environment, const SpriteSources &sources);
void select_sprite_animation(SpriteWindow sprite, std::int32_t animation,
                             SpriteEnvironment &environment, const SpriteSources &sources);
// Resident 80022660: the source-defined facing replay, including its own skip
// behavior, is distinct from ordinary execution of animation commands.
void replay_sprite_commands(SpriteWindow sprite, std::uint32_t target, std::uint32_t target_step,
                            SpriteEnvironment &environment, const SpriteSources &sources);
void schedule_sprite_frame(SpriteWindow sprite, std::uint32_t frame, SpriteEnvironment &environment,
                           const SpriteSources &sources);

// Resident 8002435c and 80024524. Allocation supplies exact incoming bytes;
// unknown heap contents are never synthesized. The wrapper's fifth short is
// passed in the original seventh stack slot, which 8002435c never consumes.
[[nodiscard]] SpriteConstruction construct_sprite(SpriteAllocation incoming, std::uint32_t resource,
                                                  const std::array<std::int16_t, 4> &coordinates,
                                                  SpriteEnvironment environment,
                                                  const SpriteSources &sources,
                                                  const SpriteAllocator &allocate,
                                                  const SpriteConstructionObserver &observe = {});
[[nodiscard]] SpriteConstruction
create_sprite(std::uint32_t resource, const std::array<std::int16_t, 5> &parameters,
              SpriteEnvironment environment, const SpriteSources &sources,
              const SpriteAllocator &allocate, const SpriteConstructionObserver &observe = {});
} // namespace xem::reconstruction::field
