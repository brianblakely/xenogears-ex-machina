#include "xem/reconstruction/field_battle.hpp"

namespace xem::reconstruction::field {

// Source-reviewed 80093568..80093664 (exclusive), with operand callees
// 800acdec -> 800acdb8 / 800a3018. The shared EventProgram and EventVariables
// implement their bounded byte and typed variable storage contracts.
BattleRequestResult execute_battle_request(EventContext &context, BattleRequestState &state,
                                           std::uint32_t music_result, std::uint8_t mode_source) {
    if (context.current_actor == nullptr)
        throw EventError("Battle request requires the current event actor");
    auto &actor = *context.current_actor;
    const auto pc = static_cast<std::uint32_t>(actor.pc);
    if (context.program.byte(pc) != 0x71)
        throw EventError("Battle request requires primary opcode 0x71 at the working PC");

    auto retry = BattleRequestRetry::none;
    if (state.field_active == 0)
        retry = BattleRequestRetry::field_inactive;
    else if (context.control.gate_values[1] == 0)
        retry = BattleRequestRetry::gate_e4_clear;
    else if (context.control.gate_values[2] == 0)
        retry = BattleRequestRetry::gate_ec_clear;
    else if (state.menu_gate != 0)
        retry = BattleRequestRetry::menu_gate_set;
    else if (music_result == 0xffffffffU)
        retry = BattleRequestRetry::music_pending;
    else if (state.gate_90 != 0)
        retry = BattleRequestRetry::gate_90_set;
    if (retry != BattleRequestRetry::none) {
        context.control.break_requested = 1;
        return {retry, std::nullopt};
    }

    state.mode = mode_source; // The original store precedes the operand reader call.
    const auto operand = context.program.word(pc + 1U);
    std::int32_t selector = static_cast<std::int32_t>(operand & 0x7fffU);
    if ((operand & 0x8000U) == 0) {
        if (context.variables == nullptr)
            throw EventError("Battle selector requires the original field variable bank");
        selector = context.variables->read(operand);
    }
    const BattleRequestResolution resolution{state, context.control, actor.pc, selector};

    state.selector = static_cast<std::uint8_t>(selector);
    state.resident_flag = 0;
    state.field_active = 0;
    context.control.gate_values[0] = 0;
    state.pending = 1;
    context.control.break_requested = 1;
    actor.pc = static_cast<std::uint16_t>(pc + 3U);
    return {BattleRequestRetry::none, resolution};
}

} // namespace xem::reconstruction::field
