#include "xem/reconstruction/prepared_field_event_pass.hpp"

#include <stdexcept>
#include <utility>

using namespace xem::reconstruction::field;
namespace {
void check(bool value) {
    if (!value)
        throw std::runtime_error("Connected ownership regression");
}
PreparedFieldEventPass prepared() {
    PreparedFieldEventPass state;
    state.events.bytecode = {0x36, 0, 0, 0};
    state.events.entries.resize(1);
    state.actors.resize(1);
    for (auto &slot : state.actors[0].slots)
        slot.control_bits = 15U << 18U;
    state.actors[0].slots[0].control_bits = 0;
    state.descriptor_flags = {0x100};
    state.control.post_initialization = 1;
    state.control.gate_values = {1, 1, 1};
    return state;
}
} // namespace

int main() {
    auto original = prepared();
    auto copy = original;
    auto moved = std::move(copy);
    const auto stop = run_connected_events(moved, 100);
    check(stop.kind == "blocked" && stop.event_pass_completed);
    check(moved.variables.words[0] == 1 && original.variables.words[0] == 0);
    check(moved.actors[0].pc == 3 && original.actors[0].pc == 0);
    check(run_connected_events(moved, 100).kind == "error");
    check(run_connected_events(original, 100).event_pass_completed);

    auto partial = prepared();
    check(run_connected_events(partial, 1).kind == "budget");
    check(partial.variables.words[0] == 1 && partial.actors[0].slots[0].resume_pc == 0);
    check(run_connected_events(partial, 100).kind == "error");

    auto malformed = prepared();
    malformed.descriptor_flags.clear();
    check(run_connected_events(malformed, 100).kind == "error");
    check(!malformed.attempted);
    malformed = prepared();
    malformed.variables.unsigned_bitmap[0] = 1;
    check(run_connected_events(malformed, 100).kind == "error");
    check(!malformed.attempted);
    return 0;
}
