#include "xem/reconstruction/program.hpp"

#include <iostream>
#include <stdexcept>

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

game::Program loaded_field(std::vector<std::uint8_t> code, std::size_t count) {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    auto &state = *program.field;
    state.event_package.bytecode = std::move(code);
    state.event_package.entries.resize(count);
    state.actors.resize(count);
    state.event_control.gate_values = {1, 1, 1};
    for (auto &actor : state.actors)
        put(actor.descriptor, 0x58, 0x200);
    return program;
}

void transition_shade() {
    auto program = loaded_field({0}, 1);
    auto &state = *program.field;
    // The five transition records at 800b1274, 50h apart: a 28h packet per buffer.
    program.resident.heap_contents[0x800b1274] = std::vector<std::uint8_t>(5 * 0x50);
    state.draw_buffer = 0;
    program.reload_transition_shade(0x1c3);
    const auto &bytes = program.resident.heap_contents.at(0x800b1274);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto next = 0x50 * i + 0x28;
        check(bytes[next + 4] == 0xc3 && bytes[next + 5] == 0xc3 && bytes[next + 6] == 0xc3,
              "800a5600 sets the next buffer's quad color to the shade's low byte");
        check(bytes[0x50 * i + 4] == 0, "800a5600 leaves the drawn buffer's quad alone");
    }
}

void teardown_dependencies() {
    game::FrameServices services;
    auto program = loaded_field({0}, 1);
    program.resident.w_59394 = 0x80120000;
    program.resident.w_593a0 = 1;
    std::uint32_t address = 0;
    try {
        program.field_reload_teardown(services);
    } catch (const game::MissingDependency &error) {
        address = error.point.machine_address;
    }
    check(address == 0x800374a0 && program.resident.w_593a0 == 1,
          "Releasing the block 80059394 names (8003748c) is a named dependency");
    program.resident.w_59394 = 0;
    program.field->particle_slots[3] = 1;
    address = 0;
    try {
        program.field_reload_teardown(services);
    } catch (const game::MissingDependency &error) {
        address = error.point.machine_address;
    }
    check(address == 0x800a92dc && program.resident.w_593a0 == 0 &&
              program.field->particle_slots[0] == 0,
          "Stopping a running particle emitter (800a92ac) is a named dependency");
}

void idle_map_change() {
    auto program = loaded_field({0}, 1);
    auto &state = *program.field;
    state.event_control.gate_values[2] = 1;
    game::FrameServices services;
    program.field_map_change_step(services);
    check(program.resident.preload_slot == 0xffffffffU &&
              program.resident.heap.headers.empty(),
          "No map change is due: the step neither reads ahead nor reloads");
}

} // namespace

int main() {
    try {
        transition_shade();
        teardown_dependencies();
        idle_map_change();
        std::cout << "3 field reload groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
