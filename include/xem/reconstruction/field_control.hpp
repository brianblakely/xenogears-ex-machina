#pragma once

#include "xem/reconstruction/field_battle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace xem::reconstruction::field {

class UnrecoveredEncounter : public EventError {
  public:
    using EventError::EventError;
};

// Additional per-actor control fields. Flags and working PC belong to EventActor.
struct ControlActor {
    std::uint32_t terrain_flags{};                  // Original actor +14
    std::array<std::int16_t, 3> integer_position{}; // +22,+26,+2a
    std::array<std::int16_t, 3> cached_position{};  // +68,+6a,+6c
    std::uint16_t direction{};                      // +104
    std::uint16_t animation_mode{};                 // +e8
    bool operator==(const ControlActor &) const = default;
};

namespace original {
[[nodiscard]] ControlActor read_control_actor(std::span<const std::uint8_t, 0x138> bytes);
}

// Caller inputs in addition to shared EventControl and BattleRequestState gates.
struct EncounterInputs {
    std::uint32_t music_result{};   // 8004f308
    std::uint32_t encounter_gate{}; // 800b2298
    std::int16_t inhibition{};      // 800b2176
    std::uint8_t enabled_byte{};    // 800adb04
};
enum class EncounterExit {
    none,
    field_inactive,
    gate_e4_clear,
    gate_ec_clear,
    music_pending,
    encounter_gate_clear,
    inhibited,
    menu_gate_one,
    enabled_byte_clear
};
// 80079288: only the eight early returns, before counter/RNG/selection effects.
// none means the unrecovered active path would execute, not a successful poll.
[[nodiscard]] EncounterExit encounter_early_exit(const BattleRequestState &battle,
                                                 const EventControl &events,
                                                 const EncounterInputs &inputs) noexcept;

struct ControlInputs {
    std::uint16_t held_buttons{};
    std::uint16_t pressed_buttons{};
    std::array<std::int16_t, 4> dialogue_status{};
    std::uint8_t preserve_nonplayer_motion{};
    std::uint32_t jump_contact{};
    std::int16_t jump_mode{};
    std::uint16_t repeat_delay{};
    std::uint32_t jump_setting{};
    std::uint8_t alternate_directions{};
    // Original data supplied by the caller, never embedded in distributable source.
    std::array<std::array<std::uint16_t, 16>, 2> direction_tables{};
    std::int16_t camera_angle{};
    EncounterInputs encounter;
};

struct ControlState {
    std::uint16_t stationary_counter{};   // 800adb02; compared as signed after increment
    std::uint16_t repeat_remaining{};     // 800b2342
    std::uint32_t latched_jump_setting{}; // 800adb28
    std::uint32_t updated{};              // 800adb68
    bool operator==(const ControlState &) const = default;
};

enum class ControlEligibility { not_control_owner, dialogue, inhibited, available };
enum class JumpRequest {
    control_unavailable,
    not_pressed,
    repeat_delay,
    contact,
    jump_or_airborne,
    counter_terrain,
    terrain_mask,
    held_retry,
    pressed
};
enum class ControlCall { encounter, terrain };
enum class TerrainCall { none, normal, alternate };
struct ControlEffect {
    ControlEligibility eligibility{ControlEligibility::not_control_owner};
    JumpRequest jump{JumpRequest::control_unavailable};
    // Observations of real nested logic, not callbacks supplying game behavior.
    std::array<ControlCall, 2> calls{};
    std::size_t call_count{};
    EncounterExit encounter_exit{EncounterExit::none};
    TerrainCall terrain_call{TerrainCall::none};
};

// 80081f5c: exactly zero or minus one; no state changes.
[[nodiscard]] std::int32_t terrain_jump_result(std::uint32_t actor_flags,
                                               std::uint32_t terrain_flags) noexcept;

// Original 8009f5f4 body, callable both directly and by the 0c wrapper. It has no
// bytecode operands; the dispatcher separately selects/validates its opcode.
// Always advances working u16 PC on supported returns. Active encounter selection
// throws before counter/jump/direction changes and remains required recovery work.
[[nodiscard]] ControlEffect request_field_control(EventContext &context, ControlActor &actor,
                                                  ControlState &state, const ControlInputs &inputs,
                                                  const BattleRequestState &battle);

// Primary a7 invokes the body; primary 0c (8009f5a8) additionally requests a
// scheduler break and restores its saved PC, even when control was blocked.
// This computes requests, not movement, animation, camera, or field readiness.
[[nodiscard]] ControlEffect execute_control_event(EventContext &context, std::uint8_t opcode,
                                                  ControlActor &actor, ControlState &state,
                                                  const ControlInputs &inputs,
                                                  const BattleRequestState &battle);

} // namespace xem::reconstruction::field
