#pragma once

#include "xem/reconstruction/field_events.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace xem::reconstruction::field::original {

// Original field-return correlation format, not a native save or agent schema.
// Actors still contain uninterpreted bytes and original pointer values. The native
// runtime must translate resource ownership into handles at its own boundary.
using Bytes = std::vector<std::uint8_t>;
template <std::size_t Size> using Block = std::array<std::uint8_t, Size>;

class ReturnFormatError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Five separately owned original regions, in their copied order. Their full
// gameplay interpretation remains open; concatenation does not merge ownership.
struct ReturnGlobals {
    Block<0x38> object_state;          // 800b007c
    Block<0x74> transform_state;       // 800afa54
    Block<0x400> collision_attributes; // *800afb20
    Block<0x2e4> field_state;          // 800b2078
    Block<0x1c8> camera_state;         // 800af880
    bool operator==(const ReturnGlobals &) const = default;
};

struct SpriteCaptureInput {
    Block<0xc0> sprite;
    Block<8> sequencer; // *sprite+7c, first two words
    Block<12> settings; // *sprite+20, halfwords at +6,+8,+a
};

struct ActorCaptureInput {
    Block<8> descriptor_auxiliary; // descriptor +50..+57
    std::uint32_t descriptor_flags{};
    Block<0x138> actor;
    SpriteCaptureInput sprite;
    std::optional<Block<12>> extension_110; // actor +134 bit 7
    std::optional<Block<16>> extension_114; // actor +12c bit 12
};

struct FieldCaptureInput {
    std::uint32_t descriptor_count{};
    ReturnGlobals globals;
    std::span<const ActorCaptureInput> actors; // event actor count, distinct from descriptors
    std::array<std::uint16_t, 1024> variables;
    std::array<std::uint8_t, 3> party_modes;
};

struct FieldCaptureResult {
    Bytes storage;
    std::size_t bytes_used{};
    // Separate resident storage, outside the snapshot image.
    std::array<std::uint32_t, 3> saved_party_modes;
};

// 80021ebc: update only the fields written by the original. Padding and the
// reserved word at +c survive from the previous snapshot; they are not zeroed.
[[nodiscard]] Block<48> capture_sprite_return(const SpriteCaptureInput &input,
                                              const Block<48> &previous);
// 800a3f4c: preserve old unwritten bytes. Supplied storage is never mutated,
// including on a malformed input or a capacity failure.
[[nodiscard]] FieldCaptureResult capture_field_return(const FieldCaptureInput &input,
                                                      std::span<const std::uint8_t> previous);

struct ActorReturnRecord {
    Block<8> descriptor_auxiliary;
    std::uint32_t stored_descriptor_flags{};
    Block<48> sprite_checkpoint;
    Block<0x138> actor;
    std::optional<Block<12>> extension_110;
    std::optional<Block<16>> extension_114;
    [[nodiscard]] EventActor event_state() const;
};

struct ParsedFieldReturn {
    std::uint8_t descriptor_count{};
    ReturnGlobals globals;
    std::vector<ActorReturnRecord> actors;
    std::array<std::uint16_t, 1024> variables;
    std::size_t bytes_used{};
};

// The event count comes from the loaded event resource, not snapshot byte zero.
// Read only within supplied storage; trailing reserved bytes remain allowed.
[[nodiscard]] ParsedFieldReturn parse_field_return(std::span<const std::uint8_t> storage,
                                                   std::size_t event_actor_count);

struct ActorRestoreTarget {
    Block<8> descriptor_auxiliary;
    std::uint32_t descriptor_flags{};
    Block<0x138> actor;
    std::optional<Block<12>> extension_110;
    std::optional<Block<16>> extension_114;
};

// Original allocation addresses are correlation inputs from the caller's actual
// allocator. The record proves requested sizes/data, not allocator placement.
// Native state must use its own ownership model instead of these addresses.
using RestoreAllocation =
    std::function<std::uint32_t(std::size_t actor_index, std::span<const std::uint8_t> contents)>;
struct FieldRestoreResult {
    std::uint32_t descriptor_count{};
    ReturnGlobals globals;
    std::vector<ActorRestoreTarget> actors;
    std::array<std::uint16_t, 1024> variables;
    // 800a3474 deliberately skips these. 800a28d4 must rebuild sprites and
    // 800a3c8c must restore/replay their checkpoints before field return is ready.
    std::vector<Block<48>> pending_sprite_checkpoints;
    std::size_t bytes_used{};
};

// Reconstruct the data-copy stage of 800a3474, preserving newly allocated +118
// ownership and descriptor high flag bits. Optional extensions require a real
// allocation callback; omitted services fail explicitly before invoking one.
[[nodiscard]] FieldRestoreResult
restore_field_return_data(const ParsedFieldReturn &snapshot,
                          std::span<const ActorRestoreTarget> initialized,
                          const RestoreAllocation &allocate = {});

} // namespace xem::reconstruction::field::original
