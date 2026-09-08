#include "xem/reconstruction/field_events.hpp"

#include <algorithm>
#include <string>

namespace xem::reconstruction::field {
namespace {
constexpr std::int32_t signed_half(std::uint16_t value) noexcept {
    return static_cast<std::int32_t>(value) - ((value & 0x8000U) != 0 ? 65536 : 0);
}

std::size_t variable_index(std::uint16_t reference) {
    if (reference >= 2048) {
        throw EventError("Variable reference outside the recovered 1024-word bank");
    }
    return reference >> 1U; // Odd byte references alias the preceding even reference.
}

EventActor &actor(EventContext &context) {
    if (context.current_actor == nullptr || context.current_actor->selected_slot >= 8) {
        throw EventError("Current event actor or selected slot is unavailable");
    }
    return *context.current_actor;
}

EventVariables &variables(EventContext &context) {
    if (context.variables == nullptr) {
        throw EventError("Instruction requires the original field variable bank");
    }
    return *context.variables;
}

bool compare(std::uint8_t operation, std::int32_t left, std::int32_t right) noexcept {
    const auto lhs = static_cast<std::uint32_t>(left);
    const auto rhs = static_cast<std::uint32_t>(right);
    switch (operation) {
    case 0:
        return left == right;
    case 1:
    case 7:
        return left != right;
    case 2:
        return left > right;
    case 3:
        return left < right;
    case 4:
        return left >= right;
    case 5:
        return left <= right;
    case 6:
    case 9:
        return (lhs & rhs) != 0;
    case 8:
        return (lhs | rhs) != 0;
    case 10:
        return (~lhs & rhs) != 0;
    default:
        return false; // Original switch default; no invented comparison.
    }
}

void conditional_branch(EventContext &context) {
    auto &current = actor(context);
    const auto pc = static_cast<std::uint32_t>(current.pc);
    const auto selector = context.program.byte(pc + 5);
    const auto mode = selector & 0xf0U;
    std::int32_t left = 0, right = 0;
    const auto operand = [&](std::uint32_t offset) { return context.program.word(pc + offset); };
    if (mode == 0 || mode == 0x40U) {
        const auto reference = operand(1);
        auto &bank = variables(context);
        left = bank.read(reference);
        right = mode == 0x40U ? signed_half(operand(3)) : bank.read(operand(3));
        right = bank.is_unsigned(reference) ? static_cast<std::uint16_t>(right)
                                            : signed_half(static_cast<std::uint16_t>(right));
    } else if (mode == 0x80U) {
        const auto reference = operand(3);
        auto &bank = variables(context);
        left = signed_half(operand(1));
        right = bank.read(reference);
        if (bank.is_unsigned(reference)) {
            left = static_cast<std::uint16_t>(left);
        }
    } else if (mode == 0xc0U) {
        left = signed_half(operand(1));
        right = signed_half(operand(3));
    }
    // Other selector nibbles leave both original initialized operands at zero.
    current.pc =
        compare(selector & 15U, left, right) ? static_cast<std::uint16_t>(pc + 8) : operand(6);
}
} // namespace

std::uint32_t EventSlot::priority() const noexcept { return (control_bits >> 18U) & 15U; }

std::uint8_t EventProgram::byte(std::uint32_t offset) const {
    if (bytecode.size() > 65536 || offset >= bytecode.size()) {
        throw EventError("Event byte read outside the supplied bytecode");
    }
    return bytecode[offset];
}

std::uint16_t EventProgram::word(std::uint32_t offset) const {
    const auto low = byte(offset);
    const auto high = byte(offset + 1);
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(low) |
                                      (static_cast<std::uint32_t>(high) << 8U));
}

std::uint16_t EventProgram::entry(std::int32_t index, std::uint32_t event) const {
    if (index < 0 || static_cast<std::size_t>(index) >= entries.size() || event >= 32) {
        throw EventError("Event entry outside the supplied actor table");
    }
    return entries[static_cast<std::size_t>(index)][event];
}

bool EventVariables::is_unsigned(std::uint16_t reference) const {
    const auto index = variable_index(reference);
    return (unsigned_bitmap[index >> 3U] & (1U << (index & 7U))) != 0;
}

std::int32_t EventVariables::read(std::uint16_t reference) const {
    const auto value = words[variable_index(reference)];
    return is_unsigned(reference) ? static_cast<std::int32_t>(value) : signed_half(value);
}

void EventVariables::write(std::uint16_t reference, std::int32_t value) {
    words[variable_index(reference)] = static_cast<std::uint16_t>(value);
}

bool EventControl::blocked() const noexcept {
    return post_initialization != 0 && std::any_of(gate_values.begin(), gate_values.end(),
                                                   [](auto value) { return value == 0; });
}

UnsupportedInstruction::UnsupportedInstruction(std::uint16_t at, std::uint8_t value)
    : EventError("Unreconstructed primary opcode " + std::to_string(value) + " at PC " +
                 std::to_string(at)),
      pc(at), opcode(value) {}

// Original handlers: A1B70, A1E74, A1BD0, A1A8C, 9DD34, 9D804..9DA1C.
void execute_core_event(EventContext &context, std::uint8_t opcode) {
    auto &current = actor(context);
    const auto pc = static_cast<std::uint32_t>(current.pc);
    if (context.program.byte(pc) != opcode) {
        throw EventError("Dispatched opcode disagrees with the working PC");
    }
    const auto operand = [&](std::uint32_t offset) { return context.program.word(pc + offset); };
    switch (opcode) {
    case 0:
    case 4: {
        if (opcode == 4) {
            for (auto &slot : current.slots) {
                if (slot.priority() == 7) {
                    slot.resume_pc = context.program.entry(context.current_actor_index, 1);
                }
            }
        }
        auto &slot = current.slots[current.selected_slot];
        slot.control_bits |= 0x003c0000U;
        slot.event_tag = 255;
        context.control.break_requested = 1;
        if (opcode == 0)
            context.control.budget_mode = 1;
        return;
    }
    case 1:
        current.pc = operand(1);
        return;
    case 2:
        conditional_branch(context);
        return;
    case 0x26: {
        auto &slot = current.slots[current.selected_slot];
        if (slot.countdown != 0) {
            --slot.countdown;
        } else {
            const auto raw = operand(1);
            const auto value = (raw & 0x8000U) != 0 ? static_cast<std::int32_t>(raw & 0x7fffU)
                                                    : variables(context).read(raw);
            slot.countdown = static_cast<std::uint8_t>(value);
        }
        if (slot.countdown == 0)
            current.pc = static_cast<std::uint16_t>(pc + 3);
        context.control.break_requested = 1;
        return;
    }
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39: {
        auto &bank = variables(context);
        const auto destination = operand(1);
        std::int32_t value = opcode == 0x36 ? 1 : 0;
        const bool has_operand = opcode != 0x36 && opcode != 0x37;
        if (has_operand) {
            const auto raw = operand(3);
            value = (context.program.byte(pc + 5) & 0x40U) != 0 ? signed_half(raw) : bank.read(raw);
            if (opcode == 0x38)
                value = bank.read(destination) + value;
            if (opcode == 0x39)
                value = bank.read(destination) - value;
        }
        bank.write(destination, value);
        current.pc = static_cast<std::uint16_t>(pc + (has_operand ? 6U : 3U));
        return;
    }
    default:
        throw UnsupportedInstruction(current.pc, opcode);
    }
}

// Selection belongs to the scheduler at A2194..A2214; it is not a separate PS1 function.
void select_event_slot(EventActor &current, std::uint16_t idle_entry) {
    std::uint32_t best = 15;
    for (std::uint8_t index = 0; index < 8; ++index) {
        const auto priority = current.slots[index].priority();
        if (priority <= best) {
            current.selected_slot = index;
            best = priority;
        }
    }
    if (best == 15) {
        current.selected_slot = 0;
        current.slots[0].resume_pc = idle_entry;
        current.slots[0].control_bits = (current.slots[0].control_bits & 0xffc3ffffU) | 0x001c0000U;
    }
}

// Original A1EC8..A2030. The dispatcher executes real reconstructed handlers.
BatchResult run_event_batch(EventContext &context, std::int32_t requested_limit,
                            const EventDispatch &dispatch) {
    context.control.break_requested = 0;
    context.control.batch_limit = requested_limit;
    if (requested_limit <= 0)
        return {BatchExit::nonpositive_limit, 0, false};
    if (!dispatch)
        throw EventError("Event batch requires a handler implementation");
    for (std::uint32_t count = 0; count < 1025; ++count) {
        dispatch(context, context.program.byte(actor(context).pc));
        if (context.control.budget_mode == 0)
            context.control.batch_limit = 65535;
        if (context.control.blocked())
            return {BatchExit::control_gate, count + 1, false};
        if (context.control.break_requested == 1 && context.control.budget_mode == 1) {
            return {BatchExit::handler_break, count + 1, false};
        }
        if (context.control.batch_limit <= static_cast<std::int32_t>(count + 1)) {
            return {BatchExit::limit, count + 1, false};
        }
    }
    return {BatchExit::safeguard, 1025, context.control.diagnostic_suppression == 0};
}

// Original A2030..A22A8 (exclusive end). Gate producers and other subsystems are separate
// dependencies.
ScheduleResult schedule_actor_events(EventContext &context, SchedulerState &state,
                                     const EventDispatch &dispatch) {
    const auto count = state.single_actor_mode == 1 ? 1 : state.event_actor_count;
    state.unknown_pass_state_a = 0;
    state.unknown_pass_state_b = 0;
    ScheduleResult result;
    for (std::int32_t index = 0; index < count; ++index) {
        if (static_cast<std::size_t>(index) >= state.descriptors.size()) {
            throw EventError("Scheduler actor count exceeds the supplied descriptors");
        }
        ++result.visited;
        auto &descriptor = state.descriptors[static_cast<std::size_t>(index)];
        if ((descriptor.flags & 0xf00U) == 0)
            continue;
        if (descriptor.actor == nullptr)
            throw EventError("Eligible descriptor has no event actor");
        if ((descriptor.actor->layer_flags & 0x00100000U) != 0)
            continue;
        if (context.control.blocked()) {
            result.gate_stopped_pass = true;
            return result;
        }
        context.current_actor = descriptor.actor;
        context.current_actor_index = index;
        context.current_descriptor = &descriptor;
        context.current_actor->flags &= 0xfeffffffU;
        if (state.party_processing_mode != 0 &&
            std::any_of(state.party_indices.begin(), state.party_indices.end(),
                        [index](auto party) { return party != 255 && party == index; }))
            continue;
        auto &current = *context.current_actor;
        const bool needs_idle = std::all_of(current.slots.begin(), current.slots.end(),
                                            [](const auto &slot) { return slot.priority() == 15; });
        select_event_slot(current, needs_idle ? context.program.entry(index, 1) : 0);
        context.control.budget_mode = 1;
        current.pc = current.slots[current.selected_slot].resume_pc;
        if ((current.flags & 1U) == 0) {
            const auto batch = run_event_batch(context, 8, dispatch);
            ++result.dispatched_actors;
            if (batch.diagnostic_requested)
                ++result.diagnostic_requests;
        }
        auto &resumed = actor(context); // Handlers use and may replace the shared current context.
        resumed.slots[resumed.selected_slot].resume_pc = resumed.pc;
    }
    return result;
}

} // namespace xem::reconstruction::field
