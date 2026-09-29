// Invented external results prove request observation and boundary validation,
// not original BIOS or game fidelity.
#include "../src/analysis/recorded_card.hpp"

#include <cstdlib>
#include <iostream>

namespace {
using Card = xem::analysis::RecordedCardBios;
void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
template <class Error> void rejects(auto call, const char *message) {
    try {
        call();
    } catch (const Error &) {
        return;
    }
    check(false, message);
}
void request_order_and_payload() {
    Card card;
    card.add("open", 3, {});
    card.add("info", 1, {});
    card.add("write", 2, {});
    std::string name = "bu10:invented";
    check(card.open(0x801ff000, name, 2) == 3, "Open returns its external result");
    name[0] = 'x';
    check(card.card_info(0x10) == 1, "Info returns its independent external result");
    std::array<std::uint8_t, 3> bytes{0, 0xff, 0x7a};
    check(card.write(3, 0x801c1000, bytes) == 2, "A partial external write result is retained");
    bytes.fill(0x55);
    card.close(3);
    const auto &requests = card.requests();
    check(requests.size() == 4 && requests[0].kind == "open" && requests[1].kind == "info" &&
              requests[2].kind == "write" && requests[3].kind == "close",
          "The transcript retains global call order across per-kind queues");
    check(requests[0].arguments == std::vector<std::uint32_t>{0x801ff000, 2} &&
              requests[0].names == std::vector<std::string>{"bu10:invented"} &&
              requests[2].arguments == std::vector<std::uint32_t>{3, 0x801c1000, 3} &&
              requests[2].payload == std::vector<std::uint8_t>{0, 0xff, 0x7a},
          "Arguments, filename and all requested write bytes are owned observations");
    check(requests[0].result_source == "recorded" &&
              requests[3].result_source == "adapter_constant" && card.unconsumed() == 0,
          "Discarded unrecorded return values remain distinguishable from recorded inputs");
    Card reversed;
    reversed.add("open", 3, {});
    reversed.add("info", 1, {});
    reversed.card_info(0x10);
    reversed.open(0x801ff000, "bu10:invented", 2);
    check(reversed.unconsumed() == 0 && reversed.requests()[0].kind != requests[0].kind,
          "Exact per-kind consumption cannot hide a global request-order difference");
}
void offsets_and_unknowns() {
    Card card;
    card.add("open", 2, {});
    card.add("read", 2, {0x10, 0x20});
    card.add("read", 0xffffffff, {});
    card.add("write", 1, {});
    card.open(0x80010000, "bu00:invented", 3);
    auto data = card.read(2, 0x80011000, 3);
    check(data.result == 2 && data.bytes == std::vector<std::uint8_t>{0x10, 0x20},
          "Read response stays external and returns only the recorded bytes");
    card.read(2, 0x80011002, 1);
    card.write(2, 0x80011002, std::array<std::uint8_t, 1>{0x77});
    check(card.requests()[1].offset == 0 && card.requests()[2].offset == 2 &&
              card.requests()[3].offset == 2,
          "Observation-only synchronous prefix advances by successful bytes, not request size");
    card.close(2);
    card.add("read", 0, {});
    card.read(2, 0x80011000, 1);
    check(!card.requests().back().offset, "An unknown descriptor has no inferred offset");
    card.add("open", 3, {});
    card.add("write", 0, {});
    card.open(0x80010000, "bu10:invented", 0x8002);
    card.write(3, 0x80011000, std::array<std::uint8_t, 1>{0x44});
    check(!card.requests().back().offset, "Async offsets are explicitly unavailable");
}
void event_and_directory_observations() {
    Card card;
    card.add("wait", 0, {});
    check(card.test_event(0x1234) == 0 && card.test_event(0x5678) == 0 &&
              card.test_event(0x9abc) == 1,
          "Recorded wait retains existing new/error/done polling semantics");
    check(card.requests().size() == 4 && card.requests()[2].arguments[0] == 0x9abc &&
              card.requests()[2].result_source == "derived_wait" &&
              card.requests()[3].kind == "wait" && card.requests()[3].arguments[0] == 0,
          "Aggregate wait observations do not pretend individual polls were captured");
    card.add("first", 0x801f0010, std::vector<std::uint8_t>(40, 0x66));
    card.add("next", 0, {});
    check(card.first_file(0x801f0038, "bu00:", 0x801f0010).has_value() &&
              !card.next_file(0x801f0010),
          "Directory pointer and exact response bytes remain visible");
    check(card.requests()[4].arguments == std::vector<std::uint32_t>{0x801f0038, 0x801f0010} &&
              card.requests()[4].response == std::vector<std::uint8_t>(40, 0x66),
          "The native directory destination is independent of returned bytes");
    card.bu_init();
    card.start_card();
    card.enter_critical_section();
    card.exit_critical_section();
    check(card.requests().back().kind == "exit_critical_section" &&
              card.requests().back().result_source == "void",
          "Calls with discarded or void results are still observed");
}
void malformed_and_missing_results() {
    Card card;
    rejects<Card::InputError>([&] { card.add("guessed", 0, {}); }, "Reject unknown result kinds");
    rejects<Card::InputError>([&] { card.add("wait", 4, {}); }, "Reject impossible wait indices");
    rejects<Card::InputError>([&] { card.add("read", 2, {1}); }, "Reject truncated read input");
    rejects<Card::InputError>([&] { card.add("read", 0xffffffff, {1}); },
                              "Reject bytes on a failed read");
    rejects<Card::InputError>([&] { card.add("first", 1, std::vector<std::uint8_t>(39)); },
                              "Reject a truncated directory entry");
    rejects<Card::InputError>([&] { card.add("next", 0, {1}); },
                              "Reject bytes with a null directory result");
    rejects<Card::InputError>([&] { card.add("init", 0, std::vector<std::uint8_t>(19)); },
                              "Reject an incomplete card patch");
    rejects<Card::InputError>([&] { card.add("info", 1, {1}); },
                              "Reject unexpected bytes on a scalar result");
    rejects<Card::InputError>([&] { card.add("rom", 0x10000, {}); },
                              "Reject an impossible ROM halfword");
    card.add("read", 2, {1, 2});
    rejects<Card::InputError>([&] { card.read(3, 0x80010000, 1); },
                              "Reject a response exceeding the computed request bounds");
    check(card.requests().back().arguments == std::vector<std::uint32_t>{3, 0x80010000, 1},
          "A malformed result retains its independently computed request for diagnosis");
    rejects<xem::reconstruction::ServiceUnavailable>([&] { card.open(1, "invented", 1); },
                                                     "Missing external results stop explicitly");
    check(card.requests().back().result_source == "unavailable" && !card.requests().back().result,
          "No result is invented for an unavailable call");
}
} // namespace
int main() {
    request_order_and_payload();
    offsets_and_unknowns();
    event_and_directory_observations();
    malformed_and_missing_results();
}
