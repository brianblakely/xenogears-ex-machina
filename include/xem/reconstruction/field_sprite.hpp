#pragma once

#include "xem/reconstruction/resident_heap.hpp"

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
struct SpriteTaskState;
struct SpriteServices;
struct SpriteWindow;
struct SpriteModelState;
// Allocation contexts the sprite code clears before selecting a heap tag. The
// heap's own tag, class and quiet flag belong to resident::Heap.
struct SpriteHeapControls {
    std::uint32_t class_eight_context{}; // 80059fc4
    std::uint32_t class_five_context{};  // 80059fb8
};
// The current field factory lends its actual actor for callback 80076a74.
// A callback selecting another actor requires that actor's owned context.
struct SpriteFieldActor {
    std::uint32_t index;
    std::span<std::uint8_t> bytes;
};
struct SpriteSources {
    std::span<const SpriteResource> resources;
    std::span<const SpriteResource> frame_list;
    std::span<const std::uint8_t> trigonometry;
    std::span<const std::uint8_t> replay_widths;
    std::optional<std::int32_t> incoming_replay_duration;
    SpriteExecutionObserver observe_execution{};
    SpriteTaskState *tasks{};
    SpriteServices *services{};
    std::optional<SpriteFieldActor> field_actor{};
    // Borrowed current factory allocation, so child operations resolve their
    // parent's live descriptor without substituting captured intermediate bytes.
    std::optional<SpriteResource> factory_sprite{};
    std::span<SpriteWindow> mutable_resources{};
    SpriteModelState *models{};
    SpriteHeapControls *heap{};
    resident::Heap *allocator{}; // Tag, class and quiet flag of the next allocation.
};
struct SpriteEnvironment {
    std::int32_t rate_control{};
    std::uint8_t platform_mode{};
    std::uint32_t variant{};
    std::uint8_t binding_control{};
    std::uint32_t frame_head{};
    std::uint32_t texture_page{};      // 80059310: 8002cc34 writes a full word
    std::uint32_t texture_mode{};      // 80050108
    std::uint32_t platform_argument{}; // 800591a8: 80022000's argument under platform_mode
    // 800591b3: bits 6-11 of a bound resource's directory word, when not zero,
    // under platform_mode (80022224).
    std::uint8_t platform_directory_bits{};
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
using SpriteReleaser = std::function<void(std::uint32_t address)>;
struct SpriteImageUpload {
    std::array<std::int16_t, 4> rectangle;
    std::uint32_t source_address;
    std::vector<std::uint8_t> bytes;
};
using SpriteImageUploader = std::function<void(const SpriteImageUpload &)>;
struct SpriteUploadState {
    std::uint32_t resource{}; // 800592e4
    std::int16_t x{}, y{};    // 800592e8 / 800592ea
    // The original reserves two temporary stacks on its heap. Native calls use
    // the native stack, retaining the original allocation/release service order.
    SpriteAllocation outer_stack, inner_stack;
};
struct SpriteServices {
    SpriteAllocator allocate;
    SpriteReleaser release;
    SpriteImageUploader upload_image;
    SpriteUploadState *upload_state{};
};
// Resident 8002dde4 as called by FC: sequential raw image blocks, not the
// separate 80022a70 format. The return value is ignored by its original caller.
[[nodiscard]] std::uint32_t upload_sprite_images(std::uint32_t resource, std::int16_t x,
                                                 std::int16_t y, const SpriteSources &sources,
                                                 const SpriteImageUploader &upload);

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
