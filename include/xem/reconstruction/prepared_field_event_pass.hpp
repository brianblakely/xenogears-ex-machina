#pragma once

#include "xem/reconstruction/field_battle.hpp"
#include "xem/reconstruction/packed_field.hpp"

#include <optional>
#include <string>
#include <vector>

namespace xem::reconstruction::field {

// Owned inputs and working state for one prepared field-event scheduler pass.
// Not fresh field initialization, complete field state, or a native save.
// All storage is owned. Borrowed EventContext views exist only during execution.
struct PreparedFieldEventPass {
    EventPackage events;
    std::vector<EventActor> actors;
    std::vector<std::uint32_t> descriptor_flags;
    EventVariables variables;
    EventControl control;
    FieldPassState pass;
    std::int32_t single_actor_mode{};
    std::uint8_t party_processing_mode{};
    std::array<std::int32_t, 3> party_indices{255, 255, 255};
    std::optional<BattleRequestState> battle;
    std::optional<std::uint32_t> music_result;
    std::optional<std::uint8_t> battle_mode_source;
    bool attempted{};
};

struct ConnectedStop {
    std::string kind; // blocked, budget, or error; never a whole-game success
    std::string dependency;
    std::string detail;
    std::uint32_t completed_handlers{}; // Primary dispatches; FE includes its child.
    std::uint32_t diagnostic_requests{};
    bool event_pass_completed{};
    std::int32_t actor{-1};
    std::int32_t slot{-1};
    std::int32_t pc{-1};
    std::int32_t opcode{-1};
};

// Executes the existing scheduler and handlers against one shared working state.
// No expected output, trace callback, fallback handler or state injection API.
// Stops after this prepared event pass: the enclosing field-update tail is not
// connected yet. This is not a simulation tick. A stopped state is diagnostic;
// rerun from the original prepared input, never restart a partial scheduler pass.
[[nodiscard]] ConnectedStop run_connected_events(PreparedFieldEventPass &state,
                                                  std::uint32_t instruction_limit);

} // namespace xem::reconstruction::field
