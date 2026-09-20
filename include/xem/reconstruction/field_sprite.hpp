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

class SpriteInputError : public SpriteError {
  public:
    using SpriteError::SpriteError;
};

class UnrecoveredSpriteBehavior : public SpriteError {
  public:
    using SpriteError::SpriteError;
};

class UnrecoveredSpriteCommand : public SpriteError {
  public:
    UnrecoveredSpriteCommand(std::uint32_t command_pc, std::uint8_t opcode);
    std::uint32_t command_pc;
    std::uint8_t opcode;
    // The ordinary command dispatcher is resident code, distinct from command_pc.
    static constexpr std::uint32_t machine_address = 0x800248d4;
};

// Addresses identify original resources and storage. They are never host pointers.
struct SpriteResource {
    std::uint32_t address;
    std::span<const std::uint8_t> bytes;
};
struct SpriteExecutionPoint {
    std::string_view operation;
    std::uint32_t machine_address;
    std::optional<std::uint32_t> command_pc;
};
// Read-only execution observation. A host may throw to stop its own execution
// budget; the callback never supplies a game result. Issued stores remain owned
// by the caller, and an interrupted call has no represented continuation.
using SpriteExecutionObserver = std::function<void(SpriteExecutionPoint)>;
struct SpriteSources {
    std::span<const SpriteResource> resources;
    std::span<const SpriteResource> frame_list;
    std::span<const std::uint8_t> trigonometry;
    std::span<const std::uint8_t> replay_widths;
    std::optional<std::int32_t> incoming_replay_duration;
    SpriteExecutionObserver observe_execution{};
};
struct SpriteEnvironment {
    std::int32_t rate_control{};
    std::uint8_t platform_mode{};
    std::uint32_t variant{};
    std::uint8_t binding_control{};
    std::uint32_t frame_head{};
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
    std::uint32_t address{};
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
// Resident 80023538 through 80023658: header pointers and scaled gravity.
void install_sprite_gravity(SpriteWindow sprite, std::uint32_t header,
                            const SpriteEnvironment &environment, const SpriteSources &sources);
// Resident 80022660: the source-defined facing replay, including its own skip
// behavior, is distinct from ordinary execution of animation commands.
void replay_sprite_commands(SpriteWindow sprite, std::uint32_t target, std::uint32_t target_step,
                            SpriteEnvironment &environment, const SpriteSources &sources);
void schedule_sprite_frame(SpriteWindow sprite, std::uint32_t frame, SpriteEnvironment &environment,
                           const SpriteSources &sources);

// Resident 800248d4 / 80023210. Ordinary execution is distinct from facing
// replay: timed commands 00..7f, speed/impulse A0/A1, index B3 and relative jump
// E1 and sequencer byte C6 are recovered. A0/A1 call shared field-motion code.
// Returns the number of commands executed; unknown commands fail explicitly.
[[nodiscard]] std::uint32_t execute_sprite_commands(SpriteWindow sprite,
                                                    SpriteEnvironment &environment,
                                                    const SpriteSources &sources);
[[nodiscard]] std::uint32_t advance_sprite_timer(SpriteWindow sprite,
                                                 SpriteEnvironment &environment,
                                                 const SpriteSources &sources);
// Resident 80021d50. The 48-byte checkpoint is the existing field-return format.
// Replays through the ordinary timer, restores position/sequencer state and
// reinstates the incoming rate. It does not publish an actor as field-ready.
[[nodiscard]] std::uint32_t restore_sprite_checkpoint(SpriteWindow sprite,
                                                      std::span<const std::uint8_t> checkpoint,
                                                      SpriteEnvironment &environment,
                                                      const SpriteSources &sources);

enum class SpriteRestoreDecision { restore, actor_layer_flag, party_mode_change };
struct SpriteCheckpointDecision {
    std::array<std::uint8_t, 48> checkpoint;
    SpriteRestoreDecision decision;
    std::uint32_t record_bytes;
};
// Field overlay 800a3c8c's decision for one recreated 312-byte actor. A skipped
// actor still updates its saved animation and consumes its extension stride.
[[nodiscard]] SpriteCheckpointDecision select_sprite_checkpoint(
    std::span<const std::uint8_t> actor, std::span<const std::uint8_t> checkpoint,
    const std::array<std::uint32_t, 3> &saved_modes,
    const std::array<std::uint8_t, 3> &current_modes, std::uint32_t return_gate);

// Resident 8002435c and 80024524. Allocation supplies exact incoming bytes;
// unknown heap contents are never synthesized. The wrapper's fifth short is
// passed in the original seventh stack slot, which 8002435c never consumes.
// Result must initially own no allocations. It owns each allocation and issued
// store immediately, including when a later source dependency throws. A failed
// call has no represented continuation; rerun from fresh immutable inputs.
void construct_sprite(SpriteConstruction &result, SpriteAllocation incoming, std::uint32_t resource,
                      const std::array<std::int16_t, 4> &coordinates, SpriteEnvironment environment,
                      const SpriteSources &sources, const SpriteAllocator &allocate,
                      const SpriteConstructionObserver &observe = {});
void create_sprite(SpriteConstruction &result, std::uint32_t resource,
                   const std::array<std::int16_t, 5> &parameters, SpriteEnvironment environment,
                   const SpriteSources &sources, const SpriteAllocator &allocate,
                   const SpriteConstructionObserver &observe = {});

} // namespace xem::reconstruction::field
