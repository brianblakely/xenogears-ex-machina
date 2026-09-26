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
    check(program.resident.preload_slot == 0xffffffffU && program.resident.heap.headers.empty(),
          "No map change is due: the step neither reads ahead nor reloads");
}

std::uint32_t get(const std::vector<std::uint8_t> &bytes, std::size_t at, std::size_t width = 4) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes.at(at + i)) << (8U * i);
    return value;
}

void display_environments() {
    auto program = loaded_field({0}, 1);
    auto &env = program.resident.heap_contents[0x800b249c] = std::vector<std::uint8_t>(0x100, 0xaa);
    program.set_default_draw_environment(0x800b249c, 0, 0x100, 0x140, 0xe0);
    check(get(env, 0, 2) == 0 && get(env, 2, 2) == 0x100 && get(env, 4, 2) == 0x140 &&
              get(env, 6, 2) == 0xe0 && get(env, 8, 2) == 0 && get(env, 0xa, 2) == 0x100,
          "SetDefDrawEnv sets the area and the offset");
    check(get(env, 0xc, 4) == 0 && get(env, 0x10, 4) == 0 && get(env, 0x14, 2) == 10 &&
              env[0x16] == 1 && env[0x17] == 1 && env[0x18] == 0 && env[0x19] == 0 &&
              env[0x1a] == 0 && env[0x1b] == 0,
          "SetDefDrawEnv: no texture window, page 10, dithering under 257 lines, black");
    program.resident.video_mode = 1;
    program.set_default_draw_environment(0x800b249c, 0, 0, 0x140, 0x120);
    check(env[0x17] == 1, "A PAL draw environment dithers under 289 lines");
    program.set_default_display_environment(0x800b249c + 0xb8, 0, 0x100, 0x140, 0xe0);
    check(get(env, 0xb8 + 2, 2) == 0x100 && get(env, 0xb8 + 8, 4) == 0 && env[0xb8 + 0x10] == 0 &&
              env[0xb8 + 0x13] == 0 && get(env, 0xb8 + 6, 2) == 0xe0,
          "SetDefDispEnv sets the area and clears the screen rectangle and modes");
}

void reassigned_party() {
    auto program = loaded_field({0}, 2);
    auto &state = *program.field;
    state.reload.descriptor_table = 0x80120000;
    auto &table = program.resident.heap_contents[0x80120000] = std::vector<std::uint8_t>(2 * 0x5c);
    put(table, 0x5c + 0x4c, 0x80130000);
    auto &actor = program.resident.heap_contents[0x80130000] = std::vector<std::uint8_t>(0x138);
    put(actor, 0, 0x501);
    state.party_indices = {1, 255, 255};
    program.resident.game_data.assign(game::game_data_bytes, 0);
    program.resident.game_data[0x22b1] = 1;
    program.show_reassigned_party();
    check(get(actor, 0) == 0x501, "Without a reassignment 800ad898 changes nothing");
    state.party_reassignment = 1;
    program.show_reassigned_party();
    check(get(actor, 0) == 0x201, "A reassigned member of mode 1 is shown (200, not 100 or 400)");
}

void pointer_setup() {
    auto program = loaded_field({0}, 1);
    program.init_pointer();
    const auto &state = *program.field;
    check(state.pointer_pads[0] == 0x800625fc && state.pointer_pads[1] == 0x8006261e &&
              state.pointer_divisors[0] == 3 && state.pointer_divisors[1] == 4,
          "80071ee8 reads both pad buffers with divisors 3 and 4");
    check(state.pointer_bounds[0] == 0 && state.pointer_bounds[1] == 0xa * 4 &&
              state.pointer_bounds[2] == 0x12c * 3 && state.pointer_bounds[3] == 0xdc * 4,
          "The last bounds (0..12c, a..dc) stay, scaled by the divisors");
    check(state.pointer_x[0] == 0x50 * 3 && state.pointer_y[0] == 0x64 * 4 &&
              state.pointer_x[1] == 0xfa * 3 && state.pointer_y[1] == 0x64 * 4,
          "Both ports start at their scaled positions");
}

void recorded_positions() {
    auto program = loaded_field({0}, 1);
    using Kind = game::PlatformInput::Kind;
    auto &inputs = program.resident.platform;
    inputs = {{Kind::end, 0x80078d64, 0}, {Kind::read, 0x80045de4, 2}};
    program.reach_position(0x80078d64);
    check(inputs.size() == 1 && inputs.front().kind == Kind::read,
          "A stage consumes the end recorded at its return address");
    inputs = {{Kind::read, 0x80045de4, 2}, {Kind::end, 0x80078d6c, 0}};
    program.reach_position(0x80078d6c);
    check(inputs.size() == 2, "Input recorded before a stage's end keeps it for later");
    inputs = {{Kind::end, 0x80078d6c, 0}};
    program.reach_position(0x80078d64);
    check(inputs.size() == 1, "Another stage's end stays for that stage");
}

} // namespace

int main() {
    try {
        transition_shade();
        teardown_dependencies();
        idle_map_change();
        display_environments();
        reassigned_party();
        recorded_positions();
        pointer_setup();
        std::cout << "7 field reload groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
