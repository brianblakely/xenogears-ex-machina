// Invented field state, controller bytes and hardware reads exercise the code
// the field main loop runs between frames. They describe no original content
// or observation.
#include "xem/reconstruction/program.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace game = xem::reconstruction;
namespace {
using Input = game::PlatformInput;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string stop(const std::function<void()> &call) {
    try {
        call();
    } catch (const game::MissingDependency &error) {
        return error.dependency;
    }
    return {};
}
Input read(std::uint32_t site, std::uint32_t value) { return {Input::Kind::read, site, value}; }

constexpr std::uint32_t draw_blocks = 0x800b249c;
constexpr std::uint32_t block_bytes = 0x80f4;

// A field between frames: no exit, battle, map change or menu is due, draw
// buffer 1 is the one just shown, and a VSync pass arrives before the drain.
game::Program sample() {
    game::Program program;
    auto &resident = program.resident;
    program.field = std::make_unique<game::FieldState>();
    auto &state = *program.field;
    state.event_control.diagnostic_suppression = 1;
    state.event_control.gate_values = {-1, -1, -1};
    state.gate_adbd8 = 0xffffffff;
    state.gate_adbe8 = 0xffffffff;
    state.gate_adbc4 = 0xff;
    resident.battle_request.field_active = 0xffffffff;
    state.pass.input_updated = 1;
    state.draw_buffer = 1;
    state.input_mask = 0xffff;
    state.position_result = 0xffff;
    state.pointer_pads = {0x800625fc, 0x8006261e};
    state.pointer_divisors = {3, 4};
    state.pointer_x = {0, 0x2ee};
    state.pointer_y = {0, -0x190};
    state.regions.add("draw_blocks", draw_blocks, std::vector<std::uint8_t>(2 * block_bytes));
    state.actors.resize(1);
    // The descriptor (800afb10 table) names its actor (+4c).
    state.reload.descriptor_table = 0x80120000;
    state.actors[0].address = 0x80130000;
    state.actors[0].descriptor_address = 0x80120000;
    state.actors[0].descriptor[0x4c] = 0x00;
    state.actors[0].descriptor[0x4e] = 0x13;
    state.actors[0].descriptor[0x4f] = 0x80;
    auto &actor = state.actors[0].storage;
    actor[0x22] = 0x34;
    actor[0x23] = 0x12;
    actor[0x2a] = 0xfe;
    actor[0x2b] = 0xff;
    state.party_characters = {2, 5, 7};
    resident.game_data.assign(game::game_data_bytes, 0);
    resident.random_seed = 1;
    resident.cd_dma_register = 0x1f8010b8;
    resident.pad.clock = {10, 20, 30, 4};
    resident.debug_word = 0xffffffff;
    // libgpu: the ordering-table service and its DMA registers.
    auto &gpu = resident.gpu;
    gpu.services = 0x80056888;
    gpu.functions[11] = 0x80045d5c;
    gpu.otc_registers = {0x1f8010e0, 0x1f8010e4, 0x1f8010e8, 0x1f8010f0};
    resident.io[0xf0] = 0x21;
    // A dispatcher serving VSync; the controller maps its low byte through.
    auto &irq = resident.interrupts;
    irq.initialized = 1;
    irq.mask = 1;
    irq.registers = {0x1f801070, 0x1f801074, 0x1f8010f0};
    irq.handlers[0] = 0x8004bf78;
    irq.vsync_callbacks[0] = 0x8003634c;
    resident.io[0x74] = 1;
    for (std::uint16_t bit = 0; bit < 8; ++bit) {
        resident.pad.remap_bits[bit] = static_cast<std::uint16_t>(1U << bit);
        resident.pad.remap_index[bit] = static_cast<std::uint8_t>(bit);
    }
    // Port 2 holds a digital controller, not a mouse.
    resident.pad.buffers[1] = {0, 0x41, 0xff, 0xff, 0xf6, 0x05};
    // The small table, the model table (never busy), then the arrival.
    auto &platform = resident.platform;
    platform = {read(0x80045de4, 0), read(0x80045de4, 0x01000000), read(0x80045e18, 0)};
    platform.push_back({Input::Kind::interrupt, 0x80077dcc, 0});
    // Port 1: status 0, digital, triangle (bit 4 of byte 3) pressed.
    for (const auto [index, value] : {std::pair{0U, 0U}, {1U, 0x41U}, {2U, 0xffU}, {3U, 0xefU}})
        platform.push_back({Input::Kind::pad, index, value});
    platform.push_back(read(0x8004ba34, 1));
    platform.push_back(read(0x8004bac8, 0));
    platform.push_back(read(0x8004baf0, 0));
    return program;
}

void between_frames() {
    auto program = sample();
    auto &resident = program.resident;
    auto &state = *program.field;
    resident.w_4f318 = 30;
    resident.variables.write(10, 60);
    game::FrameServices services;
    services.hblank_counts = {0x123};
    program.field_between_frames(services);
    check(resident.platform.empty() && services.hblank_counts.empty(),
          "The code between frames consumes its reads, arrival and VSync(1)");
    check(state.frame_start_hcount == 0x123, "80077dac stores its VSync(1)");
    check(state.draw_buffer == 0 && state.draw_block == draw_blocks,
          "The draw buffer swaps to buffer 0");
    const auto word = [&](std::uint32_t address) { return state.regions.word(address); };
    check(word(draw_blocks + 0x80d4) == 0x5698c && word(draw_blocks + 0x80d8) == 0x0ba570 &&
              word(draw_blocks + 0x80f0) == 0x0ba588,
          "The small table is reverse-linked from the terminator packet");
    check(word(draw_blocks + 0xcc) == 0x5698c && word(draw_blocks + 0x40c8) == 0x0b6560,
          "The model table is reverse-linked");
    check(resident.hardware_writes.size() == 11 &&
              resident.hardware_writes[0].address == 0x1f8010f0 &&
              resident.hardware_writes[0].value == 0x08000021U &&
              resident.hardware_writes[4].value == 0x11000002U,
          "Each table starts one ordering-table DMA");
    const auto &inputs = state.control_inputs;
    check(inputs.held_buttons == 0x10 && inputs.pressed_buttons == 0x10 &&
              state.dialogue[3].half(0x488) == 0x10 && state.held_buttons_2 == 0,
          "The drain collects the VSync's triangle press");
    check(resident.input_queue.count == 0 && resident.input_queue.w50200 == 1,
          "The drain resets the queue");
    check(resident.pointer == std::array<std::int32_t, 5>{0xfa, -0x64, -0x100, -10, 5},
          "Port 2's pointer record divides its positions");
    check(resident.random_seed == 1U * 0x41c64e6dU + 12345U, "80078b5c advances rand");
    const auto &variables = resident.variables;
    check(variables.read(0x50) == 1, "A suppressed field sets variable 50");
    check(variables.read(10) == 0x100 && resident.w_4f318 == 0,
          "Every 31st record steps variable 10 past 60");
    check(variables.read(12) == (30 << 8 | 20) && variables.read(14) == 4,
          "The play clock enters variables 12 and 14");
    check(variables.read(0x1e) == 0x1234 && variables.read(0x20) == -2,
          "The controlled actor's position enters variables 1e and 20");
    check(resident.game_data[0x1d34] == 2 && resident.game_data[0x1d36] == 7,
          "The party enters the game data");
    check(state.held_history == 0x10, "800afc6c keeps the held buttons");
    check(inputs.jump_contact == 0, "No menu request without an earlier triangle");

    // The drained triangle asks for the menu on the next pass.
    auto next = sample();
    next.field->dialogue[3].set_half(0x488, 0x10);
    next.field->control_inputs.jump_contact = 0xff;
    next.field->script_flag_b236c = 0x42;
    game::FrameServices more;
    more.hblank_counts = {1};
    next.field_between_frames(more);
    check(next.field->control_inputs.jump_contact == 0x80 && next.resident.b_59171 == 0x42,
          "Triangle records a menu request");

    // A requested music load (8004f308 -1) whose sequence is already ready:
    // with the disc idle the poll 80085c90 finishes and 0 is committed.
    auto music = sample();
    auto &load = music.resident.music;
    load.gate = 0xffffffffU;
    load.requested = 3;
    auto &overlay = music.field->overlay;
    const auto row = 0x800adfccU - game::field_overlay_base + 6;
    overlay.assign(row + 2, 0);
    overlay[row] = 2;
    overlay[row + 1] = 1; // No shared wave bank
    music.field->overlay_verified.assign(overlay.size(), true);
    game::FrameServices once;
    once.hblank_counts = {1};
    music.field_between_frames(once);
    check(load.gate == 0 && load.completed == 1 && load.start_parameter == 0xffffffffU,
          "The main loop commits the finished music poll");
}

void stops() {
    game::FrameServices none;
    auto transition = sample();
    transition.field->transition = 1;
    check(stop([&] { transition.field_between_frames(none); }) ==
              "symbol:field-transition-800a5924",
          "A transition stops at 800a5924");
    auto menu = sample();
    menu.field->control_inputs.jump_contact = 0x80;
    menu.field->draw_buffer = 0;
    check(stop([&] { menu.field_between_frames(none); }) == "symbol:field-menu-800799d4",
          "A pending menu request stops at 800799d4");
    auto mouse = sample();
    mouse.resident.pad.buffers[1] = {0, 0x12};
    game::FrameServices one;
    one.hblank_counts = {1};
    check(stop([&] { mouse.field_between_frames(one); }) == "symbol:field-mouse-pointer",
          "A mouse on port 2 stops at 8007af74");
    auto early = sample();
    early.resident.platform.push_front({Input::Kind::interrupt, 0x80074700, 0});
    game::FrameServices two;
    two.hblank_counts = {1};
    try {
        early.field_between_frames(two);
        check(false, "An arrival for a later point blocks the reads before it");
    } catch (const game::PlatformInputError &) {
    }
}
} // namespace

int main() {
    try {
        between_frames();
        stops();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "field loop: ok\n";
    return 0;
}
