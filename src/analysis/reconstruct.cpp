#include "xem/reconstruction/prepared_field_event_pass.hpp"

#include <charconv>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>

using namespace xem::reconstruction::field;
namespace {
// Internal, versionless decimal transport from tools.analysis.connected. JSON
// parsing, source hashes and expected results stay outside the recovered process.
template <typename T> T number() {
    std::string token;
    if (!(std::cin >> token) || token.size() > 20)
        throw EventError("Truncated or oversized driver input token");
    std::int64_t value{};
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size() ||
        value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
        throw EventError("Driver input integer is invalid or out of range");
    return static_cast<T>(value);
}

template <typename Range> void read_values(Range &values) {
    for (auto &value : values)
        value = number<std::remove_cvref_t<decltype(value)>>();
}

void text(std::string_view value) {
    std::cout << '"';
    constexpr char hex[] = "0123456789abcdef";
    for (const char character : value) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\')
            std::cout << '\\' << static_cast<char>(c);
        else if (c < 32)
            std::cout << "\\u00" << hex[c >> 4U] << hex[c & 15U];
        else
            std::cout << static_cast<char>(c);
    }
    std::cout << '"';
}

template <typename Range> void array(const Range &values) {
    std::cout << '[';
    bool first = true;
    for (const auto value : values) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << +value;
    }
    std::cout << ']';
}

template <typename T> void optional(const std::optional<T> &value) {
    if (value)
        std::cout << +*value;
    else
        std::cout << "null";
}

PreparedFieldEventPass read_state() {
    const auto size = number<std::uint32_t>();
    if (size > 0x200000)
        throw EventError("Driver component exceeds the host allocation bound");
    std::vector<std::uint8_t> component(size);
    read_values(component);
    PreparedFieldEventPass state;
    state.events = parse_event_package(component);
    const auto count = number<std::uint32_t>();
    if (count == 0 || count > 4096 || count != state.events.entries.size())
        throw EventError("Driver actor count disagrees with the event package");
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<std::uint8_t, 0x138> actor{};
        read_values(actor);
        state.actors.push_back(original::read_event_actor(actor));
        state.descriptor_flags.push_back(number<std::uint32_t>());
    }
    read_values(state.variables.words);
    state.variables.unsigned_bitmap = state.events.variable_unsigned_bits;
    auto &control = state.control;
    control.budget_mode = number<std::int32_t>();
    control.break_requested = number<std::int32_t>();
    control.batch_limit = number<std::int32_t>();
    control.post_initialization = number<std::int32_t>();
    read_values(control.gate_values);
    control.diagnostic_suppression = number<std::int32_t>();
    state.pass.input_updated = number<std::uint32_t>();
    state.pass.unknown_c4268 = number<std::uint32_t>();
    state.single_actor_mode = number<std::int32_t>();
    state.party_processing_mode = number<std::uint8_t>();
    read_values(state.party_indices);
    if (number<bool>()) {
        state.battle = BattleRequestState{number<std::uint32_t>(), number<std::uint32_t>(),
                                         number<std::uint32_t>(), number<std::uint32_t>(),
                                         number<std::uint8_t>(), number<std::uint8_t>(),
                                         number<std::uint8_t>()};
    }
    if (number<bool>())
        state.music_result = number<std::uint32_t>();
    if (number<bool>())
        state.battle_mode_source = number<std::uint8_t>();
    return state;
}

void write_state(const PreparedFieldEventPass &state) {
    std::cout << "{\"actors\":[";
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        if (i != 0)
            std::cout << ',';
        const auto &actor = state.actors[i];
        std::cout << "{\"flags\":" << actor.flags << ",\"layer_flags\":" << actor.layer_flags
                  << ",\"pc\":" << actor.pc << ",\"selected_slot\":" << +actor.selected_slot
                  << ",\"model_and_bounds_flags\":" << actor.model_and_bounds_flags
                  << ",\"return_pcs\":";
        array(actor.return_pcs);
        std::cout << ",\"slots\":[";
        for (std::size_t slot = 0; slot < actor.slots.size(); ++slot) {
            if (slot != 0)
                std::cout << ',';
            const auto &value = actor.slots[slot];
            array(std::array<std::uint32_t, 4>{value.resume_pc, value.countdown, value.event_tag,
                                             value.control_bits});
        }
        std::cout << "]}";
    }
    std::cout << "],\"descriptor_flags\":";
    array(state.descriptor_flags);
    std::cout << ",\"variables\":";
    array(state.variables.words);
    std::cout << ",\"unsigned_bitmap\":";
    array(state.variables.unsigned_bitmap);
    std::cout << ",\"control\":";
    const auto &c = state.control;
    array(std::array<std::int32_t, 8>{c.budget_mode, c.break_requested, c.batch_limit,
                                    c.post_initialization, c.gate_values[0], c.gate_values[1],
                                    c.gate_values[2], c.diagnostic_suppression});
    std::cout << ",\"pass\":";
    array(std::array{state.pass.input_updated, state.pass.unknown_c4268});
    std::cout << ",\"scheduler\":";
    array(std::array<std::int32_t, 5>{state.single_actor_mode, state.party_processing_mode,
                                    state.party_indices[0], state.party_indices[1],
                                    state.party_indices[2]});
    std::cout << ",\"battle\":";
    if (state.battle) {
        const auto &b = *state.battle;
        array(std::array<std::uint32_t, 7>{b.field_active, b.menu_gate, b.gate_90, b.pending,
                                         b.selector, b.mode, b.resident_flag});
    } else {
        std::cout << "null";
    }
    std::cout << ",\"music_result\":";
    optional(state.music_result);
    std::cout << ",\"battle_mode_source\":";
    optional(state.battle_mode_source);
    std::cout << '}';
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Prepared field-event analysis driver; use python -m tools.analysis.connected.\n";
        return 0;
    }
    try {
        if (argc != 1)
            throw EventError("Unexpected argument; use --help");
        auto state = read_state();
        const auto limit = number<std::uint32_t>();
        std::string extra;
        if (std::cin >> extra)
            throw EventError("Unexpected trailing driver input");
        const auto stop = run_connected_events(state, limit);
        std::cout << "{\"scope\":\"prepared-event-pass\",\"overlay_sha256\":";
        text(event_source_overlay_sha256);
        std::cout << ",\"stop\":{\"kind\":";
        text(stop.kind);
        std::cout << ",\"dependency\":";
        text(stop.dependency);
        std::cout << ",\"detail\":";
        text(stop.detail);
        std::cout << ",\"completed_handlers\":" << stop.completed_handlers
                  << ",\"diagnostic_requests\":" << stop.diagnostic_requests
                  << ",\"event_pass_completed\":" << (stop.event_pass_completed ? "true" : "false")
                  << ",\"actor\":" << stop.actor << ",\"slot\":" << stop.slot
                  << ",\"pc\":" << stop.pc << ",\"opcode\":" << stop.opcode << "},\"state\":";
        write_state(state);
        std::cout << "}\n";
        return stop.kind == "error" ? 2 : stop.kind == "budget" ? 4 : 1;
    } catch (const std::exception &error) {
        std::cerr << "xem-reconstruct: " << error.what() << '\n';
        return 2;
    }
}
