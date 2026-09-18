#include "xem/reconstruction/field_battle.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

template <typename Operation> void rejects(Operation operation, const char *message) {
    bool failed = false;
    try {
        operation();
    } catch (const field::EventError &) {
        failed = true;
    }
    check(failed, message);
}

// Invented source-boundary cases. Original captures are replayed separately and
// are neither embedded in this source nor replaced by these expected values.
struct Scenario {
    std::vector<std::uint8_t> code{0x71, 0, 0x80};
    field::EventVariables variables;
    field::EventActor actor;
    field::EventContext context;
    field::BattleRequestState request{17, 0, 0, 0xffffffffU, 91, 37, 255};
    std::uint32_t music{};
    std::uint8_t mode{5};
    Scenario() {
        context.current_actor = &actor;
        context.control.break_requested = 7;
        context.control.budget_mode = 1;
        context.control.batch_limit = 19;
        context.control.post_initialization = 1;
        context.control.gate_values = {-9, 2, -2147483647 - 1};
        context.control.diagnostic_suppression = 3;
        actor.flags = 0x12345678U;
        actor.layer_flags = 0x87654321U;
        actor.selected_slot = 255; // This handler never reads the selected slot.
    }
    field::BattleRequestResult run() {
        context.program = {code, {}};
        return field::execute_battle_request(context, request, music, mode);
    }
    void operand(std::uint16_t value) {
        code = {0x71, static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8U)};
    }
};

void check_untouched_control(const field::EventControl &before, const field::EventControl &after,
                             bool accepted) {
    check(after.budget_mode == before.budget_mode && after.batch_limit == before.batch_limit &&
              after.post_initialization == before.post_initialization &&
              after.diagnostic_suppression == before.diagnostic_suppression &&
              after.gate_values[1] == before.gate_values[1] &&
              after.gate_values[2] == before.gate_values[2] &&
              after.gate_values[0] == (accepted ? 0 : before.gate_values[0]),
          "Request must retain unrelated interpreter state and gates");
}

void ordered_lazy_gates() {
    const std::array reasons{
        field::BattleRequestRetry::field_inactive, field::BattleRequestRetry::gate_e4_clear,
        field::BattleRequestRetry::gate_ec_clear,  field::BattleRequestRetry::menu_gate_set,
        field::BattleRequestRetry::music_pending,  field::BattleRequestRetry::gate_90_set};
    for (std::size_t first = 0; first < reasons.size(); ++first) {
        Scenario s;
        s.code.resize(1); // Every retry precedes unavailable operand/variable reads.
        if (first == 0)
            s.request.field_active = 0;
        if (first <= 1)
            s.context.control.gate_values[1] = 0;
        if (first <= 2)
            s.context.control.gate_values[2] = 0;
        if (first <= 3)
            s.request.menu_gate = 0x80000000U;
        if (first <= 4)
            s.music = 0xffffffffU;
        s.request.gate_90 = 0x80000000U;
        const auto before = s.request;
        const auto control = s.context.control;
        const auto result = s.run();
        check(!result.accepted() && result.retry == reasons[first] && !result.resolution,
              "Retry must report the first blocked source gate");
        check(s.request == before && s.actor.pc == 0 && s.context.control.break_requested == 1,
              "Retry changes only the interpreter break request");
        check_untouched_control(control, s.context.control, false);
    }
    for (auto music : {0U, 1U, 0x80000000U, 0xfffffffeU}) {
        Scenario s;
        s.music = music;
        check(s.run().accepted(), "Only exactly ffffffff is the music retry sentinel");
    }
}

void publication_order_and_widths() {
    for (const auto operand : {0x8000U, 0x80ffU, 0x8100U, 0xffffU}) {
        Scenario s;
        s.operand(static_cast<std::uint16_t>(operand));
        s.mode = 255;
        const auto before = s.request;
        const auto control = s.context.control;
        const auto result = s.run();
        check(result.accepted() && result.resolution.has_value(),
              "Accepted request must expose its resolved-selector boundary");
        auto latched = before;
        latched.mode = 255;
        const auto &resolution = *result.resolution;
        check(resolution.state == latched && resolution.pc == 0 &&
                  resolution.selector_value == static_cast<std::int32_t>(operand & 0x7fffU) &&
                  resolution.control.break_requested == 7,
              "Mode alone is published before the resolved selector boundary");
        check_untouched_control(control, resolution.control, false);
        check(s.request == field::BattleRequestState{0, 0, 0, 1, static_cast<std::uint8_t>(operand),
                                                     255, 0} &&
                  s.actor.pc == 3 && s.context.control.break_requested == 1,
              "Accepted request must publish exact byte truncation and continuation PC");
        check_untouched_control(control, s.context.control, true);
        check(s.actor.flags == 0x12345678U && s.actor.layer_flags == 0x87654321U &&
                  s.actor.selected_slot == 255,
              "Request must not require or alter an event slot");
    }
    for (const bool is_unsigned : {false, true}) {
        for (const auto reference : {2046U, 2047U}) {
            Scenario s;
            s.operand(static_cast<std::uint16_t>(reference));
            s.variables.words[1023] = 0xff80;
            s.variables.unsigned_bitmap[127] = is_unsigned ? 0x80 : 0;
            s.context.variables = &s.variables;
            const auto result = s.run();
            check(result.resolution->selector_value == (is_unsigned ? 65408 : -128) &&
                      s.request.selector == 128,
                  "Typed variable selectors retain sign extension and odd-reference aliasing");
            check(s.variables.words[1023] == 0xff80 &&
                      s.variables.unsigned_bitmap[127] == (is_unsigned ? 0x80 : 0),
                  "Selector resolution must not mutate the variable bank");
        }
    }
}

void pc_and_unavailable_inputs() {
    Scenario s;
    s.code.assign(65536, 0);
    s.code[65533] = 0x71;
    s.code[65535] = 0x80;
    s.actor.pc = 65533;
    check(s.run().accepted() && s.actor.pc == 0, "Final PC must wrap at its halfword store");

    s = Scenario{};
    s.context.current_actor = &s.actor;
    s.code.assign(65536, 0);
    s.code[65535] = 0x71;
    s.code[1] = 0x80;
    s.actor.pc = 65535;
    const auto before = s.request;
    rejects([&] { static_cast<void>(s.run()); }, "Operand addresses must not wrap to PC zero");
    auto latched = before;
    latched.mode = s.mode;
    check(s.request == latched && s.actor.pc == 65535 && s.context.control.break_requested == 7,
          "Bounded operand failure retains the earlier mode latch without request publication");
    s.request.field_active = 0;
    check(!s.run().accepted(), "Retry at PC ffff still requires no operand");

    for (auto reference : {0U, 2048U, 0x7fffU}) {
        Scenario bad;
        bad.operand(static_cast<std::uint16_t>(reference));
        if (reference != 0)
            bad.context.variables = &bad.variables;
        rejects([&] { static_cast<void>(bad.run()); },
                "Missing bank and out-of-bank selectors must fail explicitly");
        check(bad.request.mode == bad.mode && bad.request.pending == 0xffffffffU &&
                  bad.actor.pc == 0,
              "Variable errors must preserve source store order");
    }
    Scenario bad;
    bad.code[0] = 0x70;
    rejects([&] { static_cast<void>(bad.run()); }, "Another opcode must not run this handler");
    bad.code.assign(65537, 0x71);
    rejects([&] { static_cast<void>(bad.run()); }, "Oversized bytecode must be rejected");
    bad.context.current_actor = nullptr;
    rejects([&] { static_cast<void>(bad.run()); }, "Missing actor must be rejected");
}

void connected_request_and_continuation() {
    Scenario s;
    s.actor.selected_slot = 0;
    s.code = {0x71, 0, 0x80, 0x35, 0, 0, 9, 0, 0x40, 0};
    s.context.program = {s.code, {}};
    s.context.variables = &s.variables;
    s.music = 0xffffffffU;
    std::optional<field::BattleRequestResult> effect;
    const auto dispatch = [&](field::EventContext &context, std::uint8_t opcode) {
        if (opcode == 0x71)
            effect = field::execute_battle_request(context, s.request, s.music, s.mode);
        else
            field::execute_core_event(context, opcode);
    };
    const auto retry = field::run_event_batch(s.context, 8, dispatch);
    check(retry.reason == field::BatchExit::handler_break && retry.dispatched == 1 &&
              s.actor.pc == 0 && effect && !effect->accepted(),
          "Real batch must yield a music retry at the unchanged request PC");
    s.music = 0;
    const auto accepted = field::run_event_batch(s.context, 8, dispatch);
    check(accepted.reason == field::BatchExit::control_gate && accepted.dispatched == 1 &&
              s.actor.pc == 3 && effect->accepted() && s.variables.read(0) == 0,
          "Accepted request clears the shared gate before batch break evaluation");

    // Synthetic later caller input, not a reconstruction of battle return producers.
    s.context.control.gate_values[0] = 1;
    const auto continuation = field::run_event_batch(s.context, 8, dispatch);
    check(continuation.reason == field::BatchExit::handler_break && continuation.dispatched == 2 &&
              s.variables.read(0) == 9 && s.actor.pc == 9,
          "The saved continuation executes the real assignment and end handlers");

    Scenario initializing;
    initializing.actor.selected_slot = 0;
    initializing.code.push_back(0x70);
    initializing.context.program = {initializing.code, {}};
    initializing.context.control.post_initialization = 0;
    initializing.context.control.budget_mode = 0;
    bool unsupported = false;
    try {
        static_cast<void>(field::run_event_batch(
            initializing.context, 8, [&](field::EventContext &context, std::uint8_t opcode) {
                if (opcode == 0x71)
                    static_cast<void>(field::execute_battle_request(
                        context, initializing.request, initializing.music, initializing.mode));
                else
                    field::execute_core_event(context, opcode);
            }));
    } catch (const field::UnsupportedInstruction &error) {
        unsupported = error.opcode == 0x70 && error.pc == 3;
    }
    check(unsupported && initializing.context.control.batch_limit == 65535,
          "Mode-zero continuation must still reject an unreconstructed next instruction");
}

void continuation_wait_and_branch_boundaries() {
    for (const auto pending : {0U, 1U, 2U, 0x80000000U, 0xffffffffU}) {
        Scenario s;
        s.code.assign(65536, 0);
        s.code[65535] = 0xfe;
        s.code[0] = 0x7f;
        s.actor.pc = 65535;
        s.context.program = {s.code, {}};
        s.request.pending = pending;
        const auto before = s.request;
        field::run_extended_event(s.context, [&](auto &context, auto opcode) {
            check(opcode == 0x7f && context.current_actor->pc == 0,
                  "Extended dispatch wraps its own PC store before the wait");
            field::wait_battle_request_extended(context, s.request);
        });
        check(s.actor.pc == (pending == 0 ? 1 : 65535) && s.context.control.break_requested == 1 &&
                  s.context.control.budget_mode == 1 && s.request == before,
              "All nonzero pending words retry; the wait never clears shared state");
    }
    for (const bool zero_unsigned : {false, true}) {
        for (const bool operand_unsigned : {false, true}) {
            Scenario s;
            s.code = {0x86, 0xff, 7, 0xff, 0xff}; // Odd reference aliases final variable.
            s.context.program = {s.code, {}};
            s.context.variables = &s.variables;
            s.variables.words[0] = s.variables.words[1023] = 0x8000;
            s.variables.unsigned_bitmap[0] = zero_unsigned ? 1 : 0;
            s.variables.unsigned_bitmap[127] = operand_unsigned ? 0x80 : 0;
            field::branch_battle_continuation(s.context);
            check(s.actor.pc == (zero_unsigned == operand_unsigned ? 5 : 65535) &&
                      s.context.control.break_requested == 7 && s.variables.words[0] == 0x8000 &&
                      s.variables.words[1023] == 0x8000,
                  "Both variables retain independent signedness without a write or break");
        }
    }
    Scenario s;
    s.code = {0x86, 0xff, 0xff};
    s.context.program = {s.code, {}};
    s.context.variables = &s.variables;
    s.variables.words[0] = 32767;
    field::branch_battle_continuation(s.context);
    check(s.actor.pc == 5, "Tagged immediate is unsigned 15-bit; equal branch never reads target");
    s.actor.pc = 0;
    s.variables.words[0] = 0xffff;
    rejects([&] { field::branch_battle_continuation(s.context); },
            "Unequal branch must reject its unavailable target");
    check(s.actor.pc == 0 && s.context.control.break_requested == 7,
          "Missing branch target must not publish a new PC or break");
    s.code.assign(65536, 0);
    s.code[65533] = 0x86;
    s.code[65534] = 0xff;
    s.code[65535] = 0xff;
    s.actor.pc = 65533;
    s.variables.words[0] = 32767;
    s.context.program = {s.code, {}};
    field::branch_battle_continuation(s.context);
    check(s.actor.pc == 2, "Equality wraps final PC independently of operand addresses");
    s.code[65535] = 0x86;
    s.actor.pc = 65535;
    rejects([&] { field::branch_battle_continuation(s.context); },
            "Operand pointers do not wrap with the event PC");
    s.code = {0x86, 0, 8, 0, 0};
    s.context.program = {s.code, {}};
    s.actor.pc = 0;
    rejects([&] { field::branch_battle_continuation(s.context); },
            "Out-of-bank operand reference must fail");
    s.context.variables = nullptr;
    rejects([&] { field::branch_battle_continuation(s.context); },
            "Branch always needs typed variable zero");
    s.context.current_actor = nullptr;
    rejects([&] { field::wait_battle_request_extended(s.context, s.request); },
            "Wait needs a current actor");
}

void connected_pending_wait_and_result_branch() {
    Scenario s;
    s.actor.selected_slot = 0;
    // Invented composition of real request/wait/branch/assignment/end handlers.
    s.code = {0x71, 0, 0x80, 0xfe, 0x7f, 0x86, 0x20, 0x80, 16, 0, 0x35, 2, 0, 9, 0, 0x40, 0};
    s.context.program = {s.code, {}};
    s.context.variables = &s.variables;
    const auto dispatch = [&](field::EventContext &context, std::uint8_t opcode) {
        if (opcode == 0x71)
            static_cast<void>(field::execute_battle_request(context, s.request, s.music, s.mode));
        else if (opcode == 0xfe)
            field::run_extended_event(context, [&](auto &extended, auto operation) {
                if (operation != 0x7f)
                    throw field::UnsupportedExtendedInstruction(extended.current_actor->pc,
                                                                operation);
                field::wait_battle_request_extended(extended, s.request);
            });
        else if (opcode == 0x86)
            field::branch_battle_continuation(context);
        else
            field::execute_core_event(context, opcode);
    };
    check(field::run_event_batch(s.context, 8, dispatch).reason == field::BatchExit::control_gate &&
              s.actor.pc == 3 && s.request.pending == 1,
          "Accepted request stops the real batch before the continuation");
    // Explicit caller inputs exercise the continuation; these are not recovered
    // combat results, return ownership, or producers of the restored gates.
    s.context.control.gate_values[0] = 1;
    const auto waiting = field::run_event_batch(s.context, 8, dispatch);
    check(waiting.dispatched == 1 && waiting.reason == field::BatchExit::handler_break &&
              s.actor.pc == 3 && s.request.pending == 1,
          "Pending request retries through the real FE dispatcher");
    s.request.pending = 0;
    const auto ready = field::run_event_batch(s.context, 8, dispatch);
    check(ready.dispatched == 1 && s.actor.pc == 5 && s.variables.read(2) == 0,
          "Cleared pending still yields before the branch");
    s.variables.words[0] = 32;
    const auto equal = field::run_event_batch(s.context, 8, dispatch);
    check(equal.dispatched == 3 && s.variables.read(2) == 9 && s.actor.pc == 16,
          "Equal variable-zero comparison executes the real continuation effects");
    s.actor.pc = 5;
    s.variables.words[0] = 27;
    s.variables.words[1] = 0;
    const auto unequal = field::run_event_batch(s.context, 8, dispatch);
    check(unequal.dispatched == 2 && s.variables.read(2) == 0 && s.actor.pc == 16,
          "Unequal variable-zero comparison jumps past the assignment");
}
} // namespace

int main() {
    try {
        ordered_lazy_gates();
        publication_order_and_widths();
        pc_and_unavailable_inputs();
        connected_request_and_continuation();
        continuation_wait_and_branch_boundaries();
        connected_pending_wait_and_result_branch();
        std::cout << "Field battle request/continuation: six source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
