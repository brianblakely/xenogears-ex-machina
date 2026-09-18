#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string_view>

namespace xem::reconstruction::field {

// Authored semantic storage. These are not packed PS1 RAM structures.
inline constexpr std::string_view event_source_overlay_sha256 =
    "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc";

struct EventSlot {
    std::uint16_t resume_pc{};
    std::uint8_t countdown{};
    std::uint8_t event_tag{};
    std::uint32_t control_bits{};
    [[nodiscard]] std::uint32_t priority() const noexcept;
};

struct EventActor {
    std::uint32_t flags{};
    std::uint32_t layer_flags{};
    std::array<EventSlot, 8> slots{};
    std::uint16_t pc{};
    std::uint8_t selected_slot{};
    std::array<std::uint16_t, 4> return_pcs{}; // Actor +78; original four-entry call stack
    std::uint32_t model_and_bounds_flags{};    // Actor +12c; stack depth is bits 6..8
};

namespace original {
// Shared correlation of an original actor record with semantic event storage.
// Original addresses remain byte values; this never interprets them as host pointers.
[[nodiscard]] EventActor read_event_actor(std::span<const std::uint8_t, 0x138> bytes);
} // namespace original

struct EventDescriptor {
    std::uint32_t flags{};
    EventActor *actor{};
};

struct EventProgram {
    std::span<const std::uint8_t> bytecode;
    std::span<const std::array<std::uint16_t, 32>> entries;
    [[nodiscard]] std::uint8_t byte(std::uint32_t offset) const;
    [[nodiscard]] std::uint16_t word(std::uint32_t offset) const;
    [[nodiscard]] std::uint16_t entry(std::int32_t actor, std::uint32_t event) const;
};

struct EventVariables {
    std::array<std::uint16_t, 1024> words{};
    std::array<std::uint8_t, 128> unsigned_bitmap{};
    [[nodiscard]] bool is_unsigned(std::uint16_t byte_reference) const;
    [[nodiscard]] std::int32_t read(std::uint16_t byte_reference) const;
    void write(std::uint16_t byte_reference, std::int32_t value);
};

struct EventControl {
    std::int32_t budget_mode{};
    std::int32_t break_requested{};
    std::int32_t batch_limit{};
    std::int32_t post_initialization{};
    // Original gates at ADBE0, ADBE4 and ADBEC: their producers remain under review.
    std::array<std::int32_t, 3> gate_values{};
    std::int32_t diagnostic_suppression{};
    [[nodiscard]] bool blocked() const noexcept;
};

// Shared globals reset once per scheduler pass, then written/read by handlers
// and the following field motion stage. They have one owner across those calls.
struct FieldPassState {
    std::uint32_t input_updated{}; // 800adb68
    std::uint32_t unknown_c4268{}; // 800c4268; downstream meaning unrecovered
    bool operator==(const FieldPassState &) const = default;
};

struct EventContext {
    EventProgram program;
    EventVariables *variables{};
    EventControl control;
    FieldPassState pass;
    EventActor *current_actor{};
    EventDescriptor *current_descriptor{};
    std::int32_t current_actor_index{};
};

class EventError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class UnsupportedInstruction : public EventError {
  public:
    UnsupportedInstruction(std::uint16_t at, std::uint8_t value);
    std::uint16_t pc;
    std::uint8_t opcode;
};

class UnsupportedExtendedInstruction : public EventError {
  public:
    UnsupportedExtendedInstruction(std::uint16_t at, std::uint8_t value);
    std::uint16_t pc;
    std::uint8_t opcode;
};

// A real handler implementation is required. Unknown instructions never succeed.
using EventDispatch = std::function<void(EventContext &, std::uint8_t)>;
void execute_core_event(EventContext &context, std::uint8_t opcode);

// Primary 05/06/0d at 800a17f4/800a1730/800a18b8. The two calls save
// PC+3/PC+5 before reading their raw u16 target. Returns whether the original
// diagnostic service was requested; printing itself is a separate service.
// Encoded depths beyond the four owned entries are explicitly unsupported.
[[nodiscard]] bool execute_event_call(EventContext &context, std::uint8_t opcode);

// Primary FE at 800869b8 increments the u16 PC before looking up and invoking
// its extended instruction. A dispatcher must execute recovered handlers and
// explicitly reject unknown ones, for example with UnsupportedExtendedInstruction.
// The increment survives a bounded lookup/dispatch error, as already-issued state.
void run_extended_event(EventContext &context, const EventDispatch &dispatch);

// Extended A2 at 8008825c, called at its extended PC after the FE increment.
// Only ffffffff is pending: it backs up to FE; all other values advance one.
// This always requests a break but preserves budget mode. The caller owns the
// authoritative music result; sequencing, transfer and readiness are separate.
void wait_music_load_extended(EventContext &context, std::uint32_t music_result);
void select_event_slot(EventActor &actor, std::uint16_t idle_entry);

enum class BatchExit { nonpositive_limit, control_gate, handler_break, limit, safeguard };
struct BatchResult {
    BatchExit reason;
    std::uint32_t dispatched{};
    // The original requests a diagnostic service at the safeguard; service execution is separate.
    bool diagnostic_requested{};
};
[[nodiscard]] BatchResult run_event_batch(EventContext &context, std::int32_t requested_limit,
                                          const EventDispatch &dispatch);

struct SchedulerState {
    std::span<EventDescriptor> descriptors;
    std::int32_t event_actor_count{};
    std::int32_t single_actor_mode{};
    std::uint8_t party_processing_mode{};
    std::array<std::int32_t, 3> party_indices{255, 255, 255};
};
struct ScheduleResult {
    std::uint32_t visited{};
    std::uint32_t dispatched_actors{};
    std::uint32_t diagnostic_requests{};
    bool gate_stopped_pass{};
};
[[nodiscard]] ScheduleResult schedule_actor_events(EventContext &context, SchedulerState &state,
                                                   const EventDispatch &dispatch);

} // namespace xem::reconstruction::field
