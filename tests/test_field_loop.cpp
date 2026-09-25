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
// The battle branch: a battle request with the battle's music already
// loaded takes the first stage and the loop goes on; the second stage
// leaves.
void battle_branch() {
    // Every dialogue window is closed (+3f6 ffff).
    const auto closed = [](game::Program &p) {
        for (auto &window : p.field->dialogue)
            window.set_half(0x3f6, 0xffff);
    };
    auto program = sample();
    closed(program);
    auto &resident = program.resident;
    auto &state = *program.field;
    resident.battle_request.field_active = 0;
    resident.preload_slot = 0xffffffffU;
    resident.music.requested = 0x12;
    resident.music.loaded_sequence = 0;
    state.w_adbd0 = 1;
    state.regions.add("field_raw", 0x800adb18, std::vector<std::uint8_t>(4));
    state.regions.add("field_raw", 0x800adbd4, std::vector<std::uint8_t>(4));
    game::FrameServices services;
    services.hblank_counts = {1};
    check(program.field_between_frames(services), "The first stage keeps the loop running");
    check(state.w_adbd0 == 0 && state.regions.word(0x800adbd4) == 1,
          "The first stage clears 800adbd0 and sets 800adbd4");
    check(state.music_saved && state.saved_music == 0x12,
          "The branch saves the playing music once");

    // The second stage with 800adb18 set: no snapshot, and it leaves.
    auto leave = sample();
    closed(leave);
    leave.resident.preload_slot = 0xffffffffU;
    leave.field->regions.add("field_raw", 0x800adb18, std::vector<std::uint8_t>{1, 0, 0, 0});
    leave.field->regions.add("field_raw", 0x800adbd4, std::vector<std::uint8_t>(4));
    leave.resident.w_4f30c = 5;
    check(leave.field_battle_start(), "The second stage leaves the loop");
    check(leave.resident.w_4f30c == 5, "800adb18 skips the snapshot and its count");
}

// 8007954c(0): the departure, the music to return to and battle mode.
void battle_exit() {
    auto program = sample();
    auto &resident = program.resident;
    auto &data = resident.game_data;
    // 800a30fc copies the bank to +1930 first: variable 1 lands at +1932.
    resident.variables.words[1] = 0x234;
    program.field->saved_music = 0x1d;
    resident.mode_loaded = 1;
    check(program.exit_field(0), "The battle exit calls the dispatcher");
    check(resident.next_mode == 2 && resident.mode_loaded == 0xffffffffU,
          "The battle exit selects battle mode");
    check(resident.music.requested == 0x1d && data[0x2322] == 0x1d && data[0x2323] == 0,
          "The saved music is the one to return to");
    check(data[0x2320] == 0x34 && data[0x2321] == 0x02,
          "The departure's variable 1 is the map entry kept");
    auto kept = sample();
    kept.resident.w_4f370 = 1;
    check(!kept.exit_field(0) && kept.resident.next_mode == 0,
          "8004f370 keeps the field without a mode");
}

// 8003852c: unlink an effect bank and stop its voices.
void effect_bank() {
    game::resident::SoundDriver driver;
    constexpr std::uint32_t first = 0x80100000, bank = 0x80100100, effects = 0x80101000;
    std::vector<std::uint8_t> header(0x20);
    const auto put = [](std::vector<std::uint8_t> &bytes, std::size_t at, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    auto other = header;
    put(other, 0x1c, bank);
    put(header, 0, 0x73646573);
    put(header, 8, 0x20);
    header[0xc] = 1;
    header[0xd] = 1;
    header[0x14] = 7;
    // The words must sum to zero.
    put(header, 0x18, 0U - (0x73646573U + 0x20U + 0x101U + 7U));
    driver.objects[first] = other;
    driver.objects[bank] = header;
    auto block = std::vector<std::uint8_t>(0x94 + 0x158);
    block[0x94] = 1;     // active
    block[0x94 + 6] = 3; // mask bit 3
    block[0x94 + 0xa] = 7;
    block[0x94 + 0x27] = 2; // hardware voice 2
    put(block, 0x48, 0xff);
    driver.objects[effects] = block;
    driver.effect_block = effects;
    driver.effect_banks = first;
    driver.voice_limit = 1;
    driver.voice_owners[2] = effects + 0x94 + 0x30;
    game::resident::unlink_effect_bank(driver, bank);
    const auto &record = driver.objects[effects];
    check(record[0x94] == 0 && record[0x48] == 0xf7, "The bank's voice stops");
    check(driver.voice_owners[2] == 0, "Its hardware voice is released");
    check(driver.objects[first][0x1c] == 0 && driver.objects[first][0x1f] == 0,
          "The previous bank links past it");
    bool refused = false;
    try {
        game::resident::unlink_effect_bank(driver, bank);
    } catch (const game::resident::SoundError &) {
        refused = true;
    }
    check(refused, "An unlisted bank reaches the driver's error handler");
}
} // namespace

int main() {
    try {
        between_frames();
        stops();
        battle_branch();
        battle_exit();
        effect_bank();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "field loop: ok\n";
    return 0;
}
