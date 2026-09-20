#include "xem/reconstruction/prepared_field_event_pass.hpp"

#include <iomanip>
#include <sstream>

namespace xem::reconstruction::field {
namespace {
struct Stop {
    std::string kind;
    std::string dependency;
};

template <typename T> T &required(std::optional<T> &value, const char *name) {
    if (!value)
        throw Stop{"blocked", name};
    return *value;
}

std::string instruction(const char *space, std::uint8_t opcode) {
    std::ostringstream out;
    out << "instruction:" << space << ":0x" << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<unsigned>(opcode);
    return out.str();
}
} // namespace

ConnectedStop run_connected_events(PreparedFieldEventPass &state, std::uint32_t instruction_limit) {
    ConnectedStop result;
    result.kind = "error";
    if (state.attempted) {
        result.detail = "Prepare a new run; a stopped scheduler pass cannot be restarted";
        return result;
    }
    if (instruction_limit == 0 || instruction_limit > 1000000 || state.actors.empty() ||
        state.actors.size() > 4096 || state.actors.size() != state.events.entries.size() ||
        state.actors.size() != state.descriptor_flags.size()) {
        result.detail = "Invalid instruction bound or inconsistent prepared actor storage";
        return result;
    }
    if (state.variables.unsigned_bitmap != state.events.variable_unsigned_bits) {
        result.detail = "Prepared variable type map disagrees with the event package";
        return result;
    }
    // Construct views after owned storage is final. Moving/copying a prepared
    // PreparedFieldEventPass does not retain stale pointers into the previous object.
    std::vector<EventDescriptor> descriptors;
    descriptors.reserve(state.actors.size());
    for (std::size_t i = 0; i < state.actors.size(); ++i)
        descriptors.push_back({state.descriptor_flags[i], &state.actors[i]});
    EventContext context{state.events.program(), &state.variables, state.control, state.pass};
    SchedulerState scheduler{descriptors, static_cast<std::int32_t>(state.actors.size()),
                             state.single_actor_mode, state.party_processing_mode,
                             state.party_indices};
    state.attempted = true;
    const EventDispatch extended = [&](EventContext &ctx, std::uint8_t opcode) {
        result.opcode = opcode;
        switch (opcode) {
        case 0x7f:
            wait_battle_request_extended(ctx, required(state.battle, "input:battle-state"));
            break;
        case 0xa2:
            wait_music_load_extended(ctx, required(state.music_result, "input:music-result"));
            break;
        default:
            throw UnsupportedExtendedInstruction(ctx.current_actor->pc, opcode);
        }
    };
    const EventDispatch dispatch = [&](EventContext &ctx, std::uint8_t opcode) {
        result.opcode = opcode;
        if (result.completed_handlers == instruction_limit)
            throw Stop{"budget", ""};
        switch (opcode) {
        case 0x05:
        case 0x06:
        case 0x0d:
            if (execute_event_call(ctx, opcode))
                ++result.diagnostic_requests;
            break;
        case 0x71: {
            // Do not synthesize missing producer state. Resolve arguments in a
            // deterministic order; the recovered handler owns its actual gates.
            auto &battle = required(state.battle, "input:battle-state");
            const auto music = required(state.music_result, "input:music-result");
            const auto mode = required(state.battle_mode_source, "input:battle-mode-source");
            (void)execute_battle_request(ctx, battle, music, mode);
            break;
        }
        case 0x86:
            branch_battle_continuation(ctx);
            break;
        case 0xfe:
            run_extended_event(ctx, extended);
            break;
        default:
            execute_core_event(ctx, opcode);
            break;
        }
        ++result.completed_handlers;
    };
    try {
        const auto scheduled = schedule_actor_events(context, scheduler, dispatch);
        result.diagnostic_requests += scheduled.diagnostic_requests;
        result.event_pass_completed = true;
        result.kind = "blocked";
        result.dependency = "integration:field-update-tail";
        result.detail = "Prepared event pass returned; motion/contact and enclosing update remain "
                        "unconnected. Do not repeat events as an invented game tick.";
        result.opcode = -1;
    } catch (const UnsupportedExtendedInstruction &error) {
        result.kind = "blocked";
        result.dependency = instruction("extended", error.opcode);
    } catch (const UnsupportedInstruction &error) {
        result.kind = "blocked";
        result.dependency = instruction("primary", error.opcode);
    } catch (const Stop &stop) {
        result.kind = stop.kind;
        result.dependency = stop.dependency;
    } catch (const EventError &error) {
        // Invalid input or a reconstruction defect is not a recovered dependency.
        result.kind = "error";
        result.detail = error.what();
    }
    // Preserve all committed writes, including FE's prefix increment and writes
    // preceding a malformed operand. Do not roll back or commit an unfinished slot.
    state.control = context.control;
    state.pass = context.pass;
    if (context.current_actor != nullptr) {
        result.actor = context.current_actor_index;
        result.slot = context.current_actor->selected_slot;
        result.pc = context.current_actor->pc;
    }
    return result;
}

} // namespace xem::reconstruction::field
