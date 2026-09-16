#include "xem/reconstruction/field_control.hpp"

#include <array>
#include <iostream>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Operation>
void rejects(Operation operation, const char *message) {
    try {
        operation();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error(message);
}

// Invented tables and inputs. Original control traces are compared separately.
struct Scenario {
    field::EventActor event_actor;
    field::ControlActor actor{0, {12, -5, 31}, {12, -5, 31}, 0x8123, 7};
    field::ControlState state{0, 0, 987, 0};
    field::ControlInputs inputs;
    field::BattleRequestState battle{1, 0, 0, 0, 0, 0, 0};
    field::EventContext context;
    Scenario() {
        event_actor.flags = 0x4000;
        event_actor.pc = 123;
        event_actor.selected_slot = 255; // These source handlers never read a selected slot.
        context.current_actor = &event_actor;
        context.control.gate_values = {1, 1, 1};
        context.control.break_requested = 9;
        inputs.dialogue_status = {-1, -1, -1, -1};
        inputs.jump_contact = 255;
        inputs.repeat_delay = 8;
        inputs.jump_setting = 42;
        inputs.encounter.enabled_byte = 1;
        for (std::uint16_t i = 0; i < 16; ++i) {
            inputs.direction_tables[0][i] = static_cast<std::uint16_t>(i * 256U);
            inputs.direction_tables[1][i] = static_cast<std::uint16_t>(0x8000U + i);
        }
    }
    field::ControlEffect run() {
        return field::request_field_control(context, actor, state, inputs, battle);
    }
};

void ownership_and_gates() {
    for (const auto preserve : {std::uint8_t{0}, std::uint8_t{2}}) {
        Scenario s;
        s.event_actor.flags = 0xa0040080;
        s.event_actor.pc = 65535;
        s.inputs.preserve_nonplayer_motion = preserve;
        s.inputs.held_buttons = 0x1080;
        s.inputs.pressed_buttons = 0x80;
        s.inputs.encounter.encounter_gate = 1; // Would be unsupported if reached.
        const auto before = s.actor;
        const auto state = s.state;
        const auto effect = s.run();
        check(s.event_actor.flags == (preserve == 0 ? 0xa1040080U : 0xa0040080U) &&
                  s.event_actor.pc == 0 && s.actor == before && s.state == state &&
                  effect.eligibility == field::ControlEligibility::not_control_owner &&
                  effect.call_count == 0 && s.context.control.break_requested == 9,
              "Non-owner path preserves control data and wraps PC after optional motion flag");
    }
    for (std::size_t blocked = 0; blocked < 4; ++blocked) {
        Scenario s;
        s.inputs.dialogue_status[blocked] = 0;
        s.inputs.held_buttons = 0x1080;
        s.inputs.encounter.encounter_gate = 1;
        const auto before = s.state;
        const auto effect = s.run();
        check(effect.eligibility == field::ControlEligibility::dialogue && effect.call_count == 0 &&
                  s.actor.direction == 0x8000 && s.state == before && s.event_actor.pc == 124,
              "Each of four zero dialogue statuses blocks before nested calls");
    }
    for (const auto inhibition : std::array<std::int16_t, 4>{-32768, -1, 1, 32767}) {
        Scenario s;
        s.inputs.encounter.inhibition = inhibition;
        check(s.run().eligibility == field::ControlEligibility::inhibited &&
                  s.actor.direction == 0x8000,
              "Any nonzero control inhibition blocks ownership");
    }
    for (unsigned first = 0; first < 8; ++first) {
        Scenario s;
        const std::array reasons{
            field::EncounterExit::field_inactive,       field::EncounterExit::gate_e4_clear,
            field::EncounterExit::gate_ec_clear,        field::EncounterExit::music_pending,
            field::EncounterExit::encounter_gate_clear, field::EncounterExit::inhibited,
            field::EncounterExit::menu_gate_one,        field::EncounterExit::enabled_byte_clear};
        s.inputs.encounter.encounter_gate = 1;
        if (first == 0)
            s.battle.field_active = 0;
        if (first <= 1)
            s.context.control.gate_values[1] = 0;
        if (first <= 2)
            s.context.control.gate_values[2] = 0;
        if (first <= 3)
            s.inputs.encounter.music_result = 0xffffffffU;
        if (first <= 4)
            s.inputs.encounter.encounter_gate = 0;
        if (first <= 5)
            s.inputs.encounter.inhibition = -1;
        if (first <= 6)
            s.battle.menu_gate = 1;
        s.inputs.encounter.enabled_byte = 0;
        check(field::encounter_early_exit(s.battle, s.context.control, s.inputs.encounter) ==
                  reasons[first],
              "Encounter returns must test the original ordered gates");
    }
    Scenario active;
    active.inputs.encounter.encounter_gate = 1;
    active.inputs.encounter.music_result = 0xfffffffeU;
    active.battle.menu_gate = 2;
    check(field::encounter_early_exit(active.battle, active.context.control,
                                      active.inputs.encounter) == field::EncounterExit::none,
          "Only exact menu-one and music-minus-one values block the encounter poll");
    active.inputs.held_buttons = 0x1000;
    const auto before = active.actor;
    const auto state = active.state;
    rejects<field::UnrecoveredEncounter>([&] { (void)active.run(); },
                                         "Active encounter selection must fail explicitly");
    check(active.actor == before && active.state == state && active.event_actor.pc == 123 &&
              active.context.control.break_requested == 9,
          "Unrecovered encounter must not publish downstream control changes");
    active.inputs.held_buttons = 0;
    check(active.run().call_count == 0, "No direction request means no encounter poll");
}

void normal_jump_and_terrain() {
    Scenario s;
    s.inputs.pressed_buttons = 0x80;
    s.inputs.held_buttons = 0x1000;
    const auto effect = s.run();
    check(s.event_actor.flags == 0x4800 && s.state.latched_jump_setting == 42 &&
              s.state.updated == 1 && effect.jump == field::JumpRequest::pressed &&
              effect.call_count == 2 && effect.calls[0] == field::ControlCall::encounter &&
              effect.calls[1] == field::ControlCall::terrain &&
              effect.terrain_call == field::TerrainCall::normal,
          "Pressed jump follows the real encounter early return and terrain predicate");
    Scenario held;
    held.inputs.held_buttons = 0x80;
    check(held.run().jump == field::JumpRequest::not_pressed && held.event_actor.flags == 0x4000 &&
              held.state.latched_jump_setting == 987,
          "Held Triangle is not a newly pressed normal jump");
    for (unsigned test = 0; test < 5; ++test) {
        Scenario blocked;
        blocked.inputs.pressed_buttons = 0x80;
        const std::array reasons{field::JumpRequest::jump_or_airborne,
                                 field::JumpRequest::jump_or_airborne,
                                 field::JumpRequest::counter_terrain, field::JumpRequest::contact,
                                 field::JumpRequest::terrain_mask};
        if (test < 2)
            blocked.event_actor.flags |= test == 0 ? 0x800U : 0x1000U;
        if (test == 2)
            blocked.actor.terrain_flags = 0x400000;
        if (test == 3)
            blocked.inputs.jump_contact = 0x100ff;
        if (test == 4) {
            blocked.event_actor.flags |= 0x200;
            blocked.actor.terrain_flags = 8;
        }
        const auto flags = blocked.event_actor.flags;
        check(blocked.run().jump == reasons[test] && blocked.event_actor.flags == flags &&
                  blocked.state.latched_jump_setting == 987,
              "Normal jump blockers must preserve flags and latched settings");
    }
    for (std::uint32_t actor_bits = 0; actor_bits < 4; ++actor_bits)
        for (std::uint32_t terrain_bits = 0; terrain_bits < 4; ++terrain_bits)
            check(field::terrain_jump_result(0xfffff9ffU | (actor_bits << 9U),
                                             0xffffffe7U | (terrain_bits << 3U)) ==
                      ((actor_bits & terrain_bits) != 0 ? -1 : 0),
                  "Terrain result must intersect exactly the two corresponding bits");
}

void counter_and_alternate_jump() {
    Scenario s;
    s.actor.terrain_flags = 0x400000;
    s.state.stationary_counter = 31;
    s.inputs.held_buttons = 0x80;
    check(s.run().jump == field::JumpRequest::not_pressed && s.state.stationary_counter == 32,
          "Stationary count equal to 32 must not enable held retry");
    check(s.run().jump == field::JumpRequest::held_retry && s.state.stationary_counter == 32,
          "Stationary count above 32 clamps and enables held retry");
    for (const auto counter :
         {std::uint16_t{0x7fff}, std::uint16_t{0xfffe}, std::uint16_t{0xffff}}) {
        Scenario wrapped;
        wrapped.actor.terrain_flags = 0x400000;
        wrapped.state.stationary_counter = counter;
        wrapped.inputs.held_buttons = 0x80;
        check(wrapped.run().jump == field::JumpRequest::not_pressed &&
                  wrapped.state.stationary_counter == static_cast<std::uint16_t>(counter + 1U),
              "Counter halfword wrap precedes signed threshold comparison");
    }
    Scenario moved;
    moved.actor.terrain_flags = 0x400000;
    moved.actor.integer_position[0] = 13;
    moved.state.stationary_counter = 17;
    (void)moved.run();
    check(moved.state.stationary_counter == 17, "Movement on counter terrain retains its counter");
    moved.actor.terrain_flags = 0;
    (void)moved.run();
    check(moved.state.stationary_counter == 0, "Other terrain resets the stationary counter");

    Scenario alternate;
    alternate.inputs.jump_mode = -1;
    alternate.inputs.pressed_buttons = 0x80;
    alternate.inputs.repeat_delay = 2;
    alternate.event_actor.flags = 0x5800;
    auto effect = alternate.run();
    check(effect.jump == field::JumpRequest::pressed && alternate.actor.animation_mode == 255 &&
              alternate.state.repeat_remaining == 1 &&
              effect.terrain_call == field::TerrainCall::alternate,
          "Alternate jump skips airborne gates, reloads cooldown and decrements immediately");
    effect = alternate.run();
    check(effect.jump == field::JumpRequest::repeat_delay && effect.call_count == 0 &&
              alternate.state.repeat_remaining == 0,
          "Existing alternate cooldown blocks this press before decrementing");
    alternate.inputs.repeat_delay = 0;
    (void)alternate.run();
    check(alternate.state.repeat_remaining == 0, "Reloading zero must not decrement to ffff");
    for (const std::uint32_t terrain : {0x400000U, 0x400008U}) {
        Scenario retry;
        retry.actor.terrain_flags = terrain;
        retry.event_actor.flags = 0x4200;
        retry.state = {32, 7, 987, 0};
        retry.inputs.jump_mode = 1;
        retry.inputs.held_buttons = 0x80;
        effect = retry.run();
        check(effect.terrain_call == field::TerrainCall::normal &&
                  effect.jump == (terrain == 0x400000 ? field::JumpRequest::held_retry
                                                      : field::JumpRequest::terrain_mask) &&
                  retry.state.repeat_remaining == 7 && retry.actor.animation_mode == 7,
              "Held retry bypasses alternate cooldown even when terrain rejects the jump");
    }
}

void directions_and_connected_events() {
    for (unsigned nibble = 0; nibble < 16; ++nibble) {
        Scenario s;
        s.inputs.held_buttons = static_cast<std::uint16_t>(nibble << 12U);
        s.inputs.camera_angle = 18;
        (void)s.run();
        check(s.actor.direction == (((15U - nibble) * 256U - 18U) & 0xfffU),
              "All direction nibbles use XOR 15 and signed camera subtraction");
        s.inputs.camera_angle = -32768;
        s.inputs.alternate_directions = 2;
        (void)s.run();
        check(s.actor.direction == 0x800fU - nibble,
              "Direction sentinel retains every bit and skips camera rotation");
    }
    Scenario s;
    s.event_actor.pc = 0;
    s.event_actor.selected_slot = 0;
    s.context.control.budget_mode = 1;
    field::EventVariables variables;
    s.context.variables = &variables;
    const std::array<std::uint8_t, 8> code{0xa7, 0x35, 0, 0, 9, 0, 0x40, 0};
    s.context.program = {code, {}};
    const auto dispatch = [&](field::EventContext &context, std::uint8_t opcode) {
        if (opcode == 0xa7 || opcode == 0x0c)
            (void)field::execute_control_event(context, opcode, s.actor, s.state, s.inputs,
                                               s.battle);
        else
            field::execute_core_event(context, opcode);
    };
    const auto batch = field::run_event_batch(s.context, 8, dispatch);
    check(batch.dispatched == 3 && batch.reason == field::BatchExit::handler_break &&
              variables.read(0) == 9 && s.event_actor.pc == 7 && s.state.updated == 1,
          "A7 must compose with assignment/end in the real shared event batch");
    std::vector<std::uint8_t> wrapper_code(65536);
    wrapper_code[65535] = 0x0c;
    s.context.program = {wrapper_code, {}};
    s.event_actor.pc = 65535;
    s.event_actor.flags = 0; // Blocked wrapper must still yield without advancing.
    const auto wrapper = field::run_event_batch(s.context, 8, dispatch);
    check(wrapper.dispatched == 1 && wrapper.reason == field::BatchExit::handler_break &&
              s.event_actor.pc == 65535 && s.context.control.break_requested == 1,
          "0C restores the saved PC and requests a break even when blocked");
    wrapper_code[65535] = 0x0b;
    rejects<field::UnsupportedInstruction>(
        [&] {
            (void)field::execute_control_event(s.context, 0x0b, s.actor, s.state, s.inputs,
                                               s.battle);
        },
        "Unknown control opcode must not be treated as a request");
    rejects<field::EventError>(
        [&] {
            (void)field::execute_control_event(s.context, 0xa7, s.actor, s.state, s.inputs,
                                               s.battle);
        },
        "Dispatched control opcode must agree with bytecode");
    s.context.current_actor = nullptr;
    rejects<field::EventError>([&] { (void)s.run(); }, "Missing actor must fail explicitly");
}

void original_actor_import() {
    std::array<std::uint8_t, 0x138> bytes{};
    const auto word = [&](std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
    };
    word(0x14, 0xff08);
    word(0x16, 0x8040);
    word(0x20, 0x1234); // Fractional position halfwords must not become integer coordinates.
    word(0x22, 0x8000);
    word(0x24, 0x5678);
    word(0x26, 0x7fff);
    word(0x28, 0x9abc);
    word(0x2a, 0xffff);
    word(0x68, 0x7fff);
    word(0x6a, 0x8000);
    word(0x6c, 0xfffe);
    word(0x104, 0x8123);
    word(0xe8, 0xff56);
    const auto state = field::original::read_control_actor(bytes);
    check(state.terrain_flags == 0x8040ff08 &&
              state.integer_position == std::array<std::int16_t, 3>{-32768, 32767, -1} &&
              state.cached_position == std::array<std::int16_t, 3>{32767, -32768, -2} &&
              state.direction == 0x8123 && state.animation_mode == 0xff56,
          "Original actor import must read integer high halves and signed cached coordinates");
}
} // namespace

int main() {
    try {
        ownership_and_gates();
        normal_jump_and_terrain();
        counter_and_alternate_jump();
        directions_and_connected_events();
        original_actor_import();
        std::cout << "Field control: source boundaries and connected event tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
