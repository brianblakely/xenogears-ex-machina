#include "xem/reconstruction/field_control.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction::field {

ControlActor original::read_control_actor(std::span<const std::uint8_t, 0x138> bytes) {
    const auto word = [&](std::size_t offset) {
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) |
                                          (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U));
    };
    ControlActor result;
    result.terrain_flags =
        static_cast<std::uint32_t>(word(0x14)) | (static_cast<std::uint32_t>(word(0x16)) << 16U);
    for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
        result.integer_position[coordinate] =
            std::bit_cast<std::int16_t>(word(0x22 + coordinate * 4));
        result.cached_position[coordinate] =
            std::bit_cast<std::int16_t>(word(0x68 + coordinate * 2));
    }
    result.direction = word(0x104);
    result.animation_mode = word(0xe8);
    return result;
}

EncounterExit encounter_early_exit(const BattleRequestState &battle, const EventControl &events,
                                   const EncounterInputs &inputs) noexcept {
    if (battle.field_active == 0)
        return EncounterExit::field_inactive;
    if (events.gate_values[1] == 0)
        return EncounterExit::gate_e4_clear;
    if (events.gate_values[2] == 0)
        return EncounterExit::gate_ec_clear;
    if (inputs.music_result == 0xffffffffU)
        return EncounterExit::music_pending;
    if (inputs.encounter_gate == 0)
        return EncounterExit::encounter_gate_clear;
    if (inputs.inhibition == -1)
        return EncounterExit::inhibited;
    if (battle.menu_gate == 1)
        return EncounterExit::menu_gate_one;
    if (inputs.enabled_byte == 0)
        return EncounterExit::enabled_byte_clear;
    return EncounterExit::none;
}

std::int32_t terrain_jump_result(std::uint32_t actor_flags, std::uint32_t terrain_flags) noexcept {
    return (((actor_flags >> 9U) & 3U) & (terrain_flags >> 3U)) != 0 ? -1 : 0;
}

// EVID-REF-021; corrected field overlay event_source_overlay_sha256.
// Preserve the original branching around held retry and alternate cooldown.
ControlEffect request_field_control(EventContext &context, ControlActor &actor, ControlState &state,
                                    const ControlInputs &inputs, const BattleRequestState &battle) {
    if (context.current_actor == nullptr)
        throw EventError("Control request requires the current event actor");
    auto &event_actor = *context.current_actor;
    ControlEffect effect;
    if ((event_actor.flags & 0x4000U) == 0) {
        if (inputs.preserve_nonplayer_motion == 0)
            event_actor.flags |= 0x01000000U;
    } else if (std::ranges::find(inputs.dialogue_status, 0) != inputs.dialogue_status.end() ||
               inputs.encounter.inhibition != 0) {
        effect.eligibility =
            std::ranges::find(inputs.dialogue_status, 0) != inputs.dialogue_status.end()
                ? ControlEligibility::dialogue
                : ControlEligibility::inhibited;
        actor.direction = 0x8000;
    } else {
        effect.eligibility = ControlEligibility::available;
        if ((inputs.held_buttons >> 12U) != 0) {
            effect.calls[effect.call_count++] = ControlCall::encounter;
            effect.encounter_exit = encounter_early_exit(battle, context.control, inputs.encounter);
            if (effect.encounter_exit == EncounterExit::none)
                throw UnrecoveredEncounter("Active direction-triggered encounter selection");
        }
        state.updated = 1;
        if ((actor.terrain_flags & 0x00400000U) == 0) {
            state.stationary_counter = 0;
        } else if (actor.integer_position == actor.cached_position) {
            state.stationary_counter = static_cast<std::uint16_t>(state.stationary_counter + 1U);
        }
        const bool exceeded = std::bit_cast<std::int16_t>(state.stationary_counter) > 32;
        if (exceeded)
            state.stationary_counter = 32;
        const bool held_retry = exceeded && (inputs.held_buttons & 0x80U) != 0 &&
                                (event_actor.flags & 0x1800U) == 0 && inputs.jump_contact == 0xff;
        const bool pressed = (inputs.pressed_buttons & 0x80U) != 0;
        const bool alternate = inputs.jump_mode != 0 && !held_retry;
        effect.jump = JumpRequest::not_pressed;
        bool test_terrain = false;
        if (held_retry) {
            test_terrain = true;
        } else if (alternate) {
            if (pressed) {
                if (state.repeat_remaining != 0)
                    effect.jump = JumpRequest::repeat_delay;
                else if (inputs.jump_contact != 0xff)
                    effect.jump = JumpRequest::contact;
                else
                    test_terrain = true;
            }
        } else if (pressed) {
            if ((event_actor.flags & 0x1800U) != 0)
                effect.jump = JumpRequest::jump_or_airborne;
            else if ((actor.terrain_flags & 0x00400000U) != 0)
                effect.jump = JumpRequest::counter_terrain;
            else if (inputs.jump_contact != 0xff)
                effect.jump = JumpRequest::contact;
            else
                test_terrain = true;
        }
        if (test_terrain) {
            effect.calls[effect.call_count++] = ControlCall::terrain;
            effect.terrain_call = alternate ? TerrainCall::alternate : TerrainCall::normal;
            if (terrain_jump_result(event_actor.flags, actor.terrain_flags) != 0) {
                effect.jump = JumpRequest::terrain_mask;
            } else {
                effect.jump = held_retry ? JumpRequest::held_retry : JumpRequest::pressed;
                event_actor.flags |= 0x800U;
                state.latched_jump_setting = inputs.jump_setting;
                if (alternate) {
                    actor.animation_mode = 0xff;
                    state.repeat_remaining = inputs.repeat_delay;
                }
            }
        }
        if (alternate && state.repeat_remaining != 0)
            --state.repeat_remaining;
        const auto &table = inputs.direction_tables[inputs.alternate_directions != 0 ? 1 : 0];
        auto direction = table[(inputs.held_buttons >> 12U) ^ 15U];
        if ((direction & 0x8000U) == 0) {
            const auto rotated = static_cast<std::int32_t>(direction) - inputs.camera_angle;
            direction = static_cast<std::uint16_t>(static_cast<std::uint32_t>(rotated) & 0xfffU);
        }
        actor.direction = direction;
    }
    event_actor.pc = static_cast<std::uint16_t>(event_actor.pc + 1U);
    return effect;
}

ControlEffect execute_control_event(EventContext &context, std::uint8_t opcode, ControlActor &actor,
                                    ControlState &state, const ControlInputs &inputs,
                                    const BattleRequestState &battle) {
    if (context.current_actor == nullptr)
        throw EventError("Control event requires the current event actor");
    const auto pc = context.current_actor->pc;
    if (context.program.byte(pc) != opcode)
        throw EventError("Dispatched control opcode disagrees with the working PC");
    if (opcode != 0xa7 && opcode != 0x0c)
        throw UnsupportedInstruction(pc, opcode);
    const auto effect = request_field_control(context, actor, state, inputs, battle);
    if (opcode == 0x0c) {
        context.control.break_requested = 1;
        context.current_actor->pc = pc;
    }
    return effect;
}

} // namespace xem::reconstruction::field
