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
};

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

struct EventContext {
    EventProgram program;
    EventVariables *variables{};
    EventControl control;
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

// A real handler implementation is required. Unknown instructions never succeed.
using EventDispatch = std::function<void(EventContext &, std::uint8_t)>;
void execute_core_event(EventContext &context, std::uint8_t opcode);
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
    // The original clears these at pass entry; their downstream meanings remain open.
    std::uint32_t unknown_pass_state_a{};
    std::uint32_t unknown_pass_state_b{};
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
