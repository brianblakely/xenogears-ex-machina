#include "xem/reconstruction/field_events.hpp"

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

struct Scenario {
    std::vector<std::uint8_t> code{0xfe, 0xa2, 0x35, 0, 0, 9, 0, 0x40, 0};
    field::EventActor actor;
    field::EventVariables variables;
    field::EventContext context;
    std::uint32_t music_result{0xffffffffU};
    Scenario() {
        context.current_actor = &actor;
        context.variables = &variables;
        context.program = {code, {}};
        context.control.budget_mode = 1;
        context.control.gate_values = {1, 1, 1};
        actor.flags = 0xa1b2c3d4;
        actor.layer_flags = 0xf0e1d2c3;
        for (std::uint16_t i = 0; i < 8; ++i)
            actor.slots[i] = {static_cast<std::uint16_t>(100U + i),
                              static_cast<std::uint8_t>(i + 1U), static_cast<std::uint8_t>(i),
                              0xa5c00000U | i};
    }
    void extended(field::EventContext &current, std::uint8_t opcode) {
        if (opcode != 0xa2)
            throw field::UnsupportedExtendedInstruction(current.current_actor->pc, opcode);
        field::wait_music_load_extended(current, music_result);
    }
    void primary(field::EventContext &current, std::uint8_t opcode) {
        if (opcode == 0xfe)
            field::run_extended_event(current, [&](auto &ctx, auto op) { extended(ctx, op); });
        else
            field::execute_core_event(current, opcode);
    }
    field::BatchResult batch() {
        context.program = {code, {}};
        return field::run_event_batch(context, 8, [&](auto &ctx, auto op) { primary(ctx, op); });
    }
};

void pending_ready_and_shared_batch() {
    Scenario s;
    const auto before = s.actor;
    const auto pending = s.batch();
    check(pending.dispatched == 1 && pending.reason == field::BatchExit::handler_break &&
              s.actor.pc == 0 && s.context.control.break_requested == 1 &&
              s.context.control.budget_mode == 1 && s.variables.read(0) == 0,
          "Pending music wait retries FE and breaks the real batch without running continuation");
    check(s.actor.flags == before.flags && s.actor.layer_flags == before.layer_flags &&
              s.actor.selected_slot == before.selected_slot,
          "Music wait must preserve actor flags and selected slot");
    for (std::size_t i = 0; i < 8; ++i)
        check(s.actor.slots[i].resume_pc == before.slots[i].resume_pc &&
                  s.actor.slots[i].countdown == before.slots[i].countdown &&
                  s.actor.slots[i].event_tag == before.slots[i].event_tag &&
                  s.actor.slots[i].control_bits == before.slots[i].control_bits,
              "Prefix/music wait must preserve every event slot field");
    s.music_result = 0;
    const auto ready = s.batch();
    check(ready.dispatched == 1 && ready.reason == field::BatchExit::handler_break &&
              s.actor.pc == 2 && s.variables.read(0) == 0,
          "Ready music wait advances beyond A2 but still breaks this batch");
    const auto resumed = s.batch();
    check(resumed.dispatched == 2 && resumed.reason == field::BatchExit::handler_break &&
              s.variables.read(0) == 9 && s.actor.pc == 8,
          "Next batch executes the actual assignment/end continuation");
    for (const auto status : {0U, 1U, 2U, 0x80000000U, 0xfffffffeU}) {
        Scenario nonpending;
        nonpending.music_result = status;
        (void)nonpending.batch();
        check(nonpending.actor.pc == 2, "Only exactly ffffffff may retry the prefix");
    }
}

void initialization_safeguard_and_wrap() {
    Scenario s;
    s.context.control.budget_mode = 0;
    const auto pending = s.batch();
    check(pending.reason == field::BatchExit::safeguard && pending.dispatched == 1025 &&
              pending.diagnostic_requested && s.actor.pc == 0 &&
              s.context.control.batch_limit == 65535 && s.context.control.budget_mode == 0 &&
              s.context.control.break_requested == 1,
          "Pending wait must preserve initialization mode and reach its real 1025-dispatch "
          "safeguard");
    s.context.control.diagnostic_suppression = 7;
    check(!s.batch().diagnostic_requested, "Diagnostic suppression survives pending wait loops");
    s.context.control.budget_mode = 1;
    s.context.control.post_initialization = 1;
    s.context.control.gate_values[1] = 0;
    check(s.batch().reason == field::BatchExit::control_gate,
          "Shared gate evaluation precedes music handler's break request");

    Scenario wrapped;
    wrapped.code.assign(65536, 0);
    wrapped.code[65535] = 0xfe;
    wrapped.code[0] = 0xa2;
    wrapped.actor.pc = 65535;
    (void)wrapped.batch();
    check(wrapped.actor.pc == 65535, "Prefix increment and pending decrement both wrap as u16");
    wrapped.music_result = 0;
    (void)wrapped.batch();
    check(wrapped.actor.pc == 1, "Ready wait after a wrapping FE prefix advances to PC one");
    wrapped.actor.pc = 65535;
    field::wait_music_load_extended(wrapped.context, 0);
    check(wrapped.actor.pc == 0, "The extended handler's ready increment also wraps as u16");
}

void real_dispatch_and_failures() {
    Scenario s;
    s.code[1] = 0xa3;
    bool unknown = false;
    try {
        (void)s.batch();
    } catch (const field::UnsupportedExtendedInstruction &error) {
        unknown = error.pc == 1 && error.opcode == 0xa3;
    }
    check(unknown && s.actor.pc == 1,
          "Unknown extended opcode must retain its namespace, advanced PC and identity");
    s.actor.pc = 0;
    s.code.resize(1);
    s.context.program = {s.code, {}};
    rejects<field::EventError>([&] { (void)s.batch(); }, "Truncated extended operand must fail");
    check(s.actor.pc == 1, "A bounded extended lookup error follows the original PC increment");
    s.actor.pc = 0;
    rejects<field::EventError>([&] { field::run_extended_event(s.context, {}); },
                               "Missing extended dispatcher must not silently succeed");
    s.actor.pc = 0;
    s.code[0] = 0xfd;
    rejects<field::EventError>(
        [&] {
            field::run_extended_event(s.context,
                                      [&](auto &ctx, auto opcode) { s.extended(ctx, opcode); });
        },
        "A different primary opcode must not enter extended dispatch");
    check(s.actor.pc == 0, "Wrong prefix fails before mutating PC");
    s.context.current_actor = nullptr;
    rejects<field::EventError>([&] { field::wait_music_load_extended(s.context, 0); },
                               "Music wait requires an actor");

    // This authored replacement dispatcher proves shared actor ownership follows
    // the real called handler; it is not original execution evidence.
    Scenario replace;
    field::EventActor other;
    other.pc = 19;
    field::run_extended_event(replace.context, [&](auto &ctx, auto opcode) {
        check(opcode == 0xa2 && ctx.current_actor->pc == 1, "Dispatcher sees the extended PC");
        ctx.current_actor = &other;
    });
    check(replace.actor.pc == 1 && replace.context.current_actor == &other && other.pc == 19,
          "The wrapper must not restore an actor pointer replaced by its handler");
}
} // namespace

int main() {
    try {
        pending_ready_and_shared_batch();
        initialization_safeguard_and_wrap();
        real_dispatch_and_failures();
        std::cout << "Extended dispatch/music wait: connected source boundaries passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
