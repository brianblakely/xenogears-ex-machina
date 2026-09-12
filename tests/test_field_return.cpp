#include "xem/reconstruction/field_return.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

namespace original = xem::reconstruction::field::original;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Function> void rejects(Function &&function, const char *message) {
    try {
        function();
    } catch (const original::ReturnFormatError &) {
        return;
    }
    throw std::runtime_error(message);
}
void store(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8U));
}

void return_preserves_resource_ownership_and_event_state() {
    std::array<original::ActorCaptureInput, 2> actors{};
    original::FieldCaptureInput input{};
    input.descriptor_count = 0x102; // Original truncation is distinct from event count.
    input.actors = actors;
    input.party_modes = {1, 255, 2};
    input.variables[0] = 0x8000;
    input.variables[1023] = 0xcafe;
    input.globals.collision_attributes[1023] = 91;
    actors[0].descriptor_auxiliary.fill(0x25);
    actors[0].descriptor_flags = 0x12345678;
    store(actors[0].actor, 0x118, 0x80102030);
    store(actors[0].actor, 0x8c, 0x0302abcd);
    store(actors[0].actor, 0x90, 5U << 18U);
    store(actors[0].actor, 0xcc, 0x07061234);
    actors[0].sprite.sprite[0xaf] = 0xff;
    actors[0].sprite.sprite[0xb0] = 0x80;
    actors[0].sprite.sequencer.fill(0x42);
    actors[1].actor.fill(0x33);
    store(actors[1].actor, 0x134, 0);
    store(actors[1].actor, 0x12c, 0);
    original::Bytes previous(6000, 0xa5);
    const auto captured = original::capture_field_return(input, previous);
    check(captured.bytes_used == 4 + 0x958 + 2 * 0x174 + 0x800,
          "Copied field-return extent must include the full variable bank");
    check(captured.storage[0] == 2 && captured.storage[1] == 0xa5 && captured.storage[3] == 0xa5 &&
              captured.storage.back() == 0xa5,
          "Only the descriptor-count byte and defined extent are written");
    check(captured.saved_party_modes == std::array<std::uint32_t, 3>{1, 255, 2},
          "Party mode bytes widen into separate resident words");
    check(std::all_of(previous.begin(), previous.end(), [](auto x) { return x == 0xa5; }),
          "Capture never mutates the supplied previous image");
    const auto parsed = original::parse_field_return(captured.storage, 2);
    const auto &sprite = parsed.actors[0].sprite_checkpoint;
    check(sprite[0xc] == 0xa5 && sprite[0xf] == 0xa5 && sprite[0x1a] == 0xa5 &&
              sprite[0x2f] == 0xa5 && sprite[0x14] == 255 && sprite[0x15] == 255 &&
              sprite[0x16] == 128 && sprite[0x17] == 255,
          "Reserved sprite bytes survive and animation bytes are sign extended");
    const auto event = parsed.actors[0].event_state();
    check(event.slots[0].resume_pc == 0xabcd && event.slots[0].countdown == 2 &&
              event.slots[0].event_tag == 3 && event.slots[0].priority() == 5 &&
              event.pc == 0x1234 && event.selected_slot == 6,
          "Return records retain semantic event slots and the working PC");
    std::array<original::ActorRestoreTarget, 2> fresh{};
    fresh[0].descriptor_flags = 0xfedc0000;
    store(fresh[0].actor, 0x118, 0x80001234);
    const auto restored = original::restore_field_return_data(parsed, fresh);
    check(restored.actors[0].descriptor_flags == 0xfedc5678 &&
              restored.actors[0].actor[0x118] == 0x34 && restored.actors[0].actor[0x119] == 0x12 &&
              restored.actors[0].actor[0x11a] == 0 && restored.actors[0].actor[0x11b] == 0x80,
          "Restore preserves fresh +118 ownership and high descriptor flags");
    check(restored.variables == input.variables && restored.globals == input.globals &&
              restored.pending_sprite_checkpoints.size() == 2 &&
              restored.pending_sprite_checkpoints[0] == sprite,
          "Data restoration retains explicit pending sprite rebind/replay work");
}

void optional_resources_require_complete_data_and_real_allocation() {
    std::array<original::ActorCaptureInput, 1> actors{};
    original::FieldCaptureInput input{};
    input.actors = actors;
    input.descriptor_count = 3;
    store(actors[0].actor, 0x134, 0x80);
    store(actors[0].actor, 0x12c, 0x1000);
    original::Bytes previous(5000, 0);
    rejects([&] { static_cast<void>(original::capture_field_return(input, previous)); },
            "Missing optional actor storage must not become zero bytes");
    actors[0].extension_110 = original::Block<12>{};
    actors[0].extension_114 = original::Block<16>{};
    actors[0].extension_110->fill(0x51);
    actors[0].extension_114->fill(0x71);
    const auto captured = original::capture_field_return(input, previous);
    const auto parsed = original::parse_field_return(captured.storage, 1);
    check(parsed.descriptor_count == 3 && parsed.actors.size() == 1 &&
              parsed.bytes_used == 4 + 0x958 + 0x174 + 28 + 0x800,
          "Resource event count and flagged extension lengths govern record parsing");
    std::array<original::ActorRestoreTarget, 1> fresh{};
    rejects([&] { static_cast<void>(original::restore_field_return_data(parsed, fresh)); },
            "Optional restore allocations require an implemented service");
    std::size_t called = 0;
    const auto restored = original::restore_field_return_data(
        parsed, fresh, [&](std::size_t actor, std::span<const std::uint8_t> data) {
            check(actor == 0 && data.size() == (called == 0 ? 12 : 16) &&
                      data[0] == (called == 0 ? 0x51 : 0x71),
                  "Allocation contents and call order must follow original ownership flags");
            return static_cast<std::uint32_t>(0x80001000 + called++ * 0x100);
        });
    check(called == 2 && restored.actors[0].actor[0x111] == 0x10 &&
              restored.actors[0].actor[0x115] == 0x11 &&
              restored.actors[0].extension_110 == actors[0].extension_110 &&
              restored.actors[0].extension_114 == actors[0].extension_114,
          "Reallocated original correlations and complete extension data are preserved");
}

void malformed_snapshots_fail_before_mutation_or_services() {
    std::array<original::ActorCaptureInput, 1> actors{};
    original::FieldCaptureInput input{};
    input.actors = actors;
    original::Bytes previous(4 + 0x958 + 0x174 + 0x800, 0);
    const auto captured = original::capture_field_return(input, previous);
    for (auto size : {std::size_t{0}, std::size_t{3}, std::size_t{0x960}, captured.bytes_used - 1})
        rejects(
            [&] {
                static_cast<void>(
                    original::parse_field_return(std::span(captured.storage).first(size), 1));
            },
            "Every truncated required region must fail");
    rejects(
        [&] {
            static_cast<void>(original::parse_field_return(
                captured.storage, std::numeric_limits<std::size_t>::max()));
        },
        "A malicious event count must fail before multiplication or allocation");
    store(actors[0].actor, 0x134, 0x80);
    actors[0].extension_110 = original::Block<12>{};
    rejects([&] { static_cast<void>(original::capture_field_return(input, previous)); },
            "Optional data cannot overrun an otherwise sufficient image");
    auto parsed = original::parse_field_return(captured.storage, 1);
    parsed.actors[0].extension_110 = original::Block<12>{};
    std::array<original::ActorRestoreTarget, 1> fresh{};
    std::size_t called = 0;
    rejects(
        [&] {
            static_cast<void>(original::restore_field_return_data(parsed, fresh, [&](auto, auto) {
                ++called;
                return 0;
            }));
        },
        "Inconsistent parsed ownership must fail before allocator calls");
    check(called == 0, "Invalid state must not invoke allocation services");
}
} // namespace

int main() {
    return_preserves_resource_ownership_and_event_state();
    optional_resources_require_complete_data_and_real_allocation();
    malformed_snapshots_fail_before_mutation_or_services();
    std::cout << "Three connected original field-return format groups passed\n";
}
