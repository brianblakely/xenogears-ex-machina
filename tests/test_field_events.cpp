#include "xem/reconstruction/field_events.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

struct Scenario {
    std::array<std::uint8_t, 32> code{};
    std::array<std::array<std::uint16_t, 32>, 2> entries{};
    field::EventVariables variables;
    std::array<field::EventActor, 2> actors{};
    std::array<field::EventDescriptor, 2> descriptors{};
    field::EventContext context;
    field::SchedulerState scheduler;
    Scenario() {
        for (std::size_t i = 0; i < 2; ++i) {
            for (auto &slot : actors[i].slots)
                slot.control_bits = 15U << 18U;
            descriptors[i] = {0x200, &actors[i]};
            entries[i][1] = 20;
        }
        context.program = {code, entries};
        context.variables = &variables;
        context.current_actor = &actors[0];
        context.control.gate_values = {1, 1, 1};
        scheduler.descriptors = descriptors;
        scheduler.event_actor_count = 1;
    }
};

void connected_wait_and_variable_program() {
    Scenario s;
    const std::array<std::uint8_t, 16> code{0x35, 0,    0, 5, 0, 0x40, 0x26, 2,
                                            0x80, 0x38, 0, 0, 3, 0,    0x40, 0};
    std::copy(code.begin(), code.end(), s.code.begin());
    s.actors[0].slots[0].control_bits = 2U << 18U;
    for (const auto expected : {2, 1, 0}) {
        const auto result =
            field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event);
        check(result.dispatched_actors == 1, "One eligible actor must dispatch");
        check(s.actors[0].slots[0].countdown == expected,
              "Wait must count calls and preserve its PC");
        check(s.variables.read(0) == 5, "Assignment must precede the wait");
    }
    check(s.actors[0].slots[0].resume_pc == 9, "Completed wait must commit its following PC");
    const auto finished =
        field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event);
    check(finished.dispatched_actors == 1 && s.variables.read(0) == 8,
          "Resumed arithmetic must execute before end");
    check(s.actors[0].slots[0].priority() == 15 && s.actors[0].slots[0].event_tag == 255,
          "End must terminate only the selected slot");
    check(s.actors[0].pc == 15 && s.actors[0].slots[0].resume_pc == 15,
          "End keeps and commits the working PC");
}

void scheduling_boundaries() {
    Scenario s;
    s.actors[0].selected_slot = 255; // Selection overwrites the previous value before using it.
    s.actors[0].slots[2].control_bits = 4U << 18U;
    s.actors[0].slots[6].control_bits = 4U << 18U;
    s.actors[0].slots[6].resume_pc = 7;
    s.actors[0].flags = 0x1000001;
    s.context.pass.input_updated = 7;
    s.context.pass.unknown_c4268 = 9;
    auto result = field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event);
    check(result.dispatched_actors == 0 && s.actors[0].selected_slot == 6 && s.actors[0].pc == 7,
          "Ties choose the last slot even when actor bit zero suppresses dispatch");
    check(s.actors[0].flags == 1 && s.context.pass.input_updated == 0 &&
              s.context.pass.unknown_c4268 == 0,
          "Pass and per-actor clear order must be retained");
    s.scheduler.party_processing_mode = 1;
    s.scheduler.party_indices[0] = 0;
    s.actors[0].flags = 0x1000000;
    s.actors[0].pc = 19;
    result = field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event);
    check(result.dispatched_actors == 0 && s.actors[0].flags == 0 && s.actors[0].pc == 19,
          "Party skip follows flag/context publication and precedes slot selection");
    s.context.control.post_initialization = 1;
    s.context.control.gate_values[1] = 0;
    s.actors[0].flags = 0x1000000;
    result = field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event);
    check(result.gate_stopped_pass && s.actors[0].flags == 0x1000000,
          "Closed gate stops the pass before publishing actor changes");
}

void batch_limits_and_unknowns() {
    Scenario s;
    s.code[0] = 1; // Original jump to its own PC, not a fake handler.
    s.context.control.budget_mode = 0;
    const auto result = field::run_event_batch(s.context, 8, field::execute_core_event);
    check(result.reason == field::BatchExit::safeguard && result.dispatched == 1025 &&
              result.diagnostic_requested,
          "Initialization mode must retain the 1025-dispatch safeguard");
    check(s.context.control.batch_limit == 65535,
          "Initialization extends the limit after each handler");
    s.context.control.break_requested = 1;
    const auto empty = field::run_event_batch(s.context, -1, field::execute_core_event);
    check(empty.dispatched == 0 && s.context.control.break_requested == 0 &&
              s.context.control.batch_limit == -1,
          "Nonpositive batches still initialize control storage");
    s.code[0] = 0x71;
    bool failed = false;
    try {
        static_cast<void>(field::run_event_batch(s.context, 8, field::execute_core_event));
    } catch (const field::UnsupportedInstruction &error) {
        failed = error.pc == 0 && error.opcode == 0x71;
    }
    check(failed, "Unreconstructed battle handler must fail with its identity");
}

void branch_and_variable_widths() {
    Scenario s;
    s.code[0] = 2;
    s.code[3] = 255;
    s.code[4] = 255;
    s.code[5] = 0x45;
    s.code[6] = 12;
    s.variables.words[0] = 1;
    s.variables.unsigned_bitmap[0] = 1;
    field::execute_core_event(s.context, 2);
    check(s.actors[0].pc == 8, "Unsigned left variable coerces the immediate to unsigned16");
    s.actors[0].pc = 0;
    s.variables.unsigned_bitmap[0] = 0;
    field::execute_core_event(s.context, 2);
    check(s.actors[0].pc == 12, "Signed comparison uses the sign-extended immediate");
    s.actors[0].pc = 0;
    s.code[5] = 0x10;
    field::execute_core_event(s.context, 2);
    check(s.actors[0].pc == 8, "Original unhandled operand nibble leaves zero operands");
    s.actors[0].pc = 0;
    s.code[5] = 0xcf;
    field::execute_core_event(s.context, 2);
    check(s.actors[0].pc == 12, "Original unknown comparison code follows the false successor");
    s.variables.write(1, 65535);
    check(s.variables.read(0) == -1 && s.variables.read(1) == -1,
          "Odd references alias and signed reads extend");
    s.variables.unsigned_bitmap[0] = 1;
    check(s.variables.read(0) == 65535,
          "Type bitmap changes extension without changing stored bits");
}

void idle_fallback_and_malformed_inputs() {
    Scenario s;
    s.actors[0].slots[0].control_bits |= 0x80000000U;
    const auto result =
        field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event);
    check(result.dispatched_actors == 1 && s.actors[0].pc == 20 &&
              (s.actors[0].slots[0].control_bits & 0x80000000U) != 0,
          "Fallback uses entry one and preserves unrelated slot bits");
    s.scheduler.event_actor_count = 3;
    bool failed = false;
    try {
        static_cast<void>(
            field::schedule_actor_events(s.context, s.scheduler, field::execute_core_event));
    } catch (const field::EventError &) {
        failed = true;
    }
    check(failed, "Missing descriptor storage must remain explicit");
    failed = false;
    try {
        static_cast<void>(s.variables.read(2048));
    } catch (const field::EventError &) {
        failed = true;
    }
    check(failed, "Variable bank overflow must be rejected");
}

void connected_event_calls() {
    Scenario s;
    s.code[0] = 5;
    s.code[1] = 8;
    s.code[8] = 6;
    s.code[9] = 16;
    s.code[11] = 0xfe;
    s.code[12] = 0xff; // Skipped bytes of the five-byte call are never operands.
    s.code[13] = 0x0d;
    s.code[16] = 0x0d;
    auto &actor = s.actors[0];
    actor.model_and_bounds_flags = 0xa5a50023;
    actor.return_pcs = {51, 52, 53, 54};
    actor.slots[0].event_tag = 9;
    s.context.control.budget_mode = 1;
    const auto result = field::run_event_batch(s.context, 8, [](auto &context, auto opcode) {
        if (opcode == 5 || opcode == 6 || opcode == 0x0d) {
            check(!field::execute_event_call(context, opcode), "Valid calls need no diagnostic");
        } else {
            field::execute_core_event(context, opcode);
        }
    });
    check(result.reason == field::BatchExit::handler_break && result.dispatched == 5 &&
              actor.pc == 3 && actor.return_pcs == std::array<std::uint16_t, 4>{3, 13, 53, 54} &&
              actor.model_and_bounds_flags == 0xa5a50023,
          "Nested three/five-byte calls return through real handlers and the batch scheduler");
}

void call_stack_error_policy() {
    Scenario s;
    auto &actor = s.actors[0];
    actor.model_and_bounds_flags = 0x80000107;
    actor.return_pcs = {1, 2, 3, 4};
    actor.selected_slot = 255; // The full-stack branch never reads a slot.
    s.code[0] = 5;
    s.context.program.bytecode = std::span(s.code).first(1);
    s.context.control.budget_mode = 7;
    check(field::execute_event_call(s.context, 5) && s.context.control.break_requested == 1 &&
              s.context.control.budget_mode == 7 && actor.pc == 0 &&
              actor.model_and_bounds_flags == 0x80000107 &&
              actor.return_pcs == std::array<std::uint16_t, 4>{1, 2, 3, 4},
          "Full stack diagnoses and breaks without reading the absent target or changing depth");
    s.context.control.diagnostic_suppression = -1;
    check(!field::execute_event_call(s.context, 5), "Any nonzero suppression hides diagnostic");
    s.code[0] = 0x0d;
    actor.model_and_bounds_flags = 0x80000007;
    actor.selected_slot = 3;
    actor.slots[3] = {1234, 81, 19, 0x80400055};
    check(!field::execute_event_call(s.context, 0x0d) && actor.pc == 0 &&
              actor.model_and_bounds_flags == 0x80000007 && actor.slots[3].resume_pc == 1234 &&
              actor.slots[3].countdown == 81 && actor.slots[3].event_tag == 255 &&
              actor.slots[3].control_bits == 0x807c0055 && s.context.control.budget_mode == 1 &&
              s.context.control.break_requested == 1,
          "Empty return releases selected slot priority/tag and changes budget without a PC write");
}

void call_widths_and_partial_failure() {
    Scenario s;
    auto &actor = s.actors[0];
    std::vector<std::uint8_t> code(65536);
    s.context.program.bytecode = code;
    actor.pc = 65533;
    code[65533] = 5;
    code[65534] = 1;
    check(!field::execute_event_call(s.context, 5) && actor.pc == 1 && actor.return_pcs[0] == 0 &&
              actor.model_and_bounds_flags == 0x40,
          "Saved continuation wraps independently from the bytewise raw target read");
    actor.pc = 65534;
    actor.model_and_bounds_flags = 0;
    code[65534] = 6;
    bool failed = false;
    try {
        static_cast<void>(field::execute_event_call(s.context, 6));
    } catch (const field::EventError &) {
        failed = true;
    }
    check(failed && actor.return_pcs[0] == 3 && actor.pc == 65534 &&
              actor.model_and_bounds_flags == 0,
          "Return-address store precedes a malformed target read; PC/depth stores follow it");
    actor.pc = 0;
    code[0] = 0x0d;
    actor.model_and_bounds_flags = 6U << 6U;
    failed = false;
    try {
        static_cast<void>(field::execute_event_call(s.context, 0x0d));
    } catch (const field::EventError &) {
        failed = true;
    }
    check(failed && actor.pc == 0 && actor.model_and_bounds_flags == (5U << 6U),
          "Unrecovered adjacent stack storage rejects after the original decrement store");
}
} // namespace

int main() {
    try {
        connected_wait_and_variable_program();
        scheduling_boundaries();
        batch_limits_and_unknowns();
        branch_and_variable_widths();
        idle_fallback_and_malformed_inputs();
        connected_event_calls();
        call_stack_error_policy();
        call_widths_and_partial_failure();
        std::cout << "Field event reconstruction: eight connected source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
