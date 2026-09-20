#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <iostream>

namespace game = xem::reconstruction;
namespace field = game::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}

game::Program events(std::vector<std::uint8_t> code, std::size_t count = 1) {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    auto &state = *program.field;
    state.event_package.bytecode = std::move(code);
    state.event_package.entries.resize(count);
    state.actors.resize(count);
    state.event_control.gate_values = {1, 1, 1};
    state.event_control.post_initialization = 1;
    for (auto &actor : state.actors) {
        put(actor.descriptor, 0x58, 0x200);
        for (std::size_t i = 1; i < 8; ++i)
            put(actor.storage, 0x90 + i * 8, 15U << 18U);
    }
    return program;
}

void state_continuity_and_moves() {
    auto program = events({0x35, 0, 0, 5, 0, 0x40, 0x26, 2, 0x80, 0x38, 0, 0, 3, 0, 0x40, 0});
    program.resident.random_seed = 0x12345678;
    program.resident.disc_stream.expected_sequence = 41;
    static_cast<void>(program.event_pass());
    auto moved = std::move(program);
    check(!program.field && moved.resident.variables.words[0] == 5,
          "Program move transfers owned mode storage and preserves resident variables");
    for (int pass = 0; pass < 3; ++pass)
        static_cast<void>(moved.event_pass());
    check(moved.resident.variables.words[0] == 8 && moved.field->actors[0].events().pc == 15,
          "Four passes compute the variable and wait continuation in one shared program");
    check(moved.field->actors[0].events().slots[0].resume_pc == 15,
          "Scheduler commits working PC to its owned selected slot");
    check(moved.resident.random_seed == 0x12345678 &&
              moved.resident.disc_stream.expected_sequence == 41,
          "Unrelated resident services survive event passes and a program move");
}

void connected_handlers_and_partial_state() {
    auto program = events({0x71, 0, 0x80, 0xfe, 0x7f, 0x86, 32, 0x80, 11, 0, 0, 0x75});
    program.field->battle_mode_source = 5;
    program.resident.battle_request.field_active = 0xffffffffU;
    program.resident.variables.words[0] = 27;
    static_cast<void>(program.event_pass());
    check(program.resident.battle_request.pending == 1 && program.resident.battle_request.mode == 5,
          "Real primary71 publishes resident transition request through shared dispatcher");
    check(program.field->actors[0].events().pc == 3,
          "A request returns at its entry boundary; it is not battle execution");
    static_cast<void>(program.event_pass());
    check(program.field->actors[0].events().pc == 3,
          "Request closes the scheduler gate; another pass does not bypass the transition");
    // Explicit synthetic return-service inputs, not a reconstruction of combat.
    program.field->event_control.gate_values[0] = 1;
    static_cast<void>(program.event_pass());
    check(program.field->actors[0].events().pc == 3,
          "Pending wait keeps its original prefix PC and does not invent elapsed time");
    program.resident.battle_request.pending = 0;
    static_cast<void>(program.event_pass());
    check(program.field->actors[0].events().pc == 5, "Ready wait computes the next batch PC");
    try {
        static_cast<void>(program.event_pass());
        check(false, "Unknown music handler must stop");
    } catch (const field::UnsupportedInstruction &error) {
        check(error.pc == 11 && error.opcode == 0x75,
              "Computed battle branch encounters its actual next instruction");
    }
    check(program.field->actors[0].events().pc == 11 &&
              program.field->actors[0].events().slots[0].resume_pc == 5,
          "Committed handler state survives; interrupted scheduler has not committed resume PC");
}

void scheduler_observation_continuity() {
    auto program = events({0x26, 0, 0x80, 0}, 2);
    bool second_actor = false;
    static_cast<void>(program.event_pass([&](const auto &active, auto point, bool completed) {
        if (!completed && point.actor == 1) {
            second_actor = true;
            check(active.field->actors[0].events().slots[0].resume_pc == 3,
                  "Second actor observers see the first actor's scheduler commit");
        }
    }));
    check(second_actor, "Both actors ran in the same scheduler pass");
}

void extended_error_retains_prefix() {
    auto program = events({0xfe, 0xff});
    try {
        static_cast<void>(program.event_pass());
        check(false, "Unknown extended handler must stop");
    } catch (const field::UnsupportedExtendedInstruction &error) {
        check(error.pc == 1 && error.opcode == 0xff, "Extended PC is distinct from prefix PC");
    }
    check(program.field->actors[0].events().pc == 1,
          "FE's already-issued increment remains committed after interruption");
}

void original_snapshot_ownership() {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    program.resident.field_return_mode = 1;
    program.resident.variables.unsigned_bitmap[0] = 3;
    program.resident.random_seed = 93;
    field::original::FieldCaptureInput capture{};
    capture.descriptor_count = 71;
    capture.variables[0] = 27;
    put(capture.globals.field_state, 0x116, 0xfffc, 2);
    put(capture.globals.field_state, 0x1f0, 0);
    put(capture.globals.field_state, 0x154, 7, 1);
    put(capture.globals.field_state, 0x2de, 5, 1);
    const std::vector<std::uint8_t> previous(14340, 0xa5);
    program.resident.field_snapshot =
        field::original::capture_field_return(capture, previous).storage;
    const auto immutable = program.resident.field_snapshot;
    program.field->sprite_gate = 123;
    program.field->party_reassignment = 1;
    program.restore_field({}, {}); // Original zero-actor branch returns normally.
    check(program.field->descriptor_count == 71 && program.field->sprite_gate == -4 &&
              program.field->party_reassignment == 0 && program.field->battle_mode_source == 5 &&
              program.field->party_processing_mode == 7,
          "Return globals produce represented mode gates instead of prepared results");
    check(program.resident.variables.words[0] == 27 &&
              program.resident.variables.unsigned_bitmap[0] == 3 &&
              program.resident.random_seed == 93,
          "Snapshot restores variable values, preserves loaded types and unrelated RNG owner");
    check(program.resident.field_snapshot == immutable,
          "Original snapshot stays immutable and is not a native persistence image");
    program.resident.field_snapshot.resize(10);
    try {
        program.restore_field_data();
        check(false, "Malformed snapshot must fail");
    } catch (const field::original::ReturnFormatError &) {
    }
    check(program.resident.variables.words[0] == 27,
          "Invalid input preserves previous resident state");
}

void control_shares_pass_and_owner() {
    auto program = events({0x0c});
    put(program.field->actors[0].storage, 0, 0x4000);
    program.field->control_inputs.held_buttons = 0x40;
    program.field->control_inputs.dialogue_status.fill(-1);
    // Source early encounter return is computed from inactive field, not a fake service result.
    static_cast<void>(program.event_pass());
    check(program.field->pass.input_updated == 1 && program.field->actors[0].events().pc == 0,
          "Control handler and scheduler share input_updated and preserve wrapper PC");
}
} // namespace

int main() {
    try {
        state_continuity_and_moves();
        connected_handlers_and_partial_state();
        scheduler_observation_continuity();
        extended_error_retains_prefix();
        original_snapshot_ownership();
        control_shares_pass_and_owner();
        std::cout << "6 connected program regression groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
