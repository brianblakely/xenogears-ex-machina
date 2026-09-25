// Invented menu-mode memory exercises the menu overlay's framework (memory,
// callee frames, heap, C library and libgpu helpers, arrival ordering) and
// some of its functions and their explicit unrecovered paths. It describes
// no original content.
#include "xem/reconstruction/menu_overlay.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace game = xem::reconstruction;
namespace menu = game::menu;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Body> void rejects(Body body, const char *message) {
    bool rejected = false;
    try {
        body();
    } catch (const Error &) {
        rejected = true;
    }
    check(rejected, message);
}

constexpr std::uint32_t state = 0x80100000;
constexpr std::uint32_t party = 0x80101000;
constexpr std::uint32_t card = 0x80102000;
constexpr std::uint32_t entry_sp = 0x801ffe00;

// A Program in menu mode: a menu state block, its party and card blocks,
// the game data, and a heap with one free block (80180008..80180fff).
game::Program sample() {
    game::Program program;
    auto &resident = program.resident;
    resident.game_state = 0x8006d634;
    resident.game_data.assign(game::game_data_bytes, 0);
    auto &regions = program.menu.emplace().regions;
    regions[menu::state_pointer] = {0x00, 0x00, 0x10, 0x80};
    regions[menu::overlay_base].resize(0x2e4a0); // the overlay image and statics
    regions[state].resize(0x2000);
    regions[party].resize(0x6c);
    regions[card].resize(0x5034);
    auto put32 = [&](std::uint32_t address, std::uint32_t value) {
        auto &bytes = regions[state];
        for (std::uint32_t i = 0; i < 4; ++i)
            bytes[address - state + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    put32(state + menu::state_party, party);
    put32(state + menu::state_card, card);
    auto &heap = resident.heap;
    heap.head = 0x80180008;
    heap.tag = 10;
    heap.headers = {{0x80180000, {0x80181008, 0}}, {0x80181000, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80180008, std::vector<std::uint8_t>(0xff8, 0xee)}};
    return program;
}

void memory_and_frames() {
    auto program = sample();
    game::FrameServices services;
    {
        menu::Overlay overlay(program, services, entry_sp);
        check(overlay.state() == state && overlay.at(0x33c) == state + 0x33c, "state pointer");
        // Game data is menu memory while the overlay runs.
        overlay.put16(0x8006d634 + 2, 0xbeef);
        check(overlay.u16(0x8006d636) == 0xbeef && overlay.s16(0x8006d636) == -0x4111,
              "game data access");
        // The stack keeps what callees left; frames follow the original SP.
        {
            const auto outer = overlay.enter(0x20);
            const auto locals = overlay.frame(0x20);
            check(locals.base == entry_sp - 0x20, "outer frame base");
            overlay.put32(locals[0x10], 0x12345678);
            {
                const auto inner = overlay.enter(0x18);
                check(overlay.frame(0x18).base == entry_sp - 0x38, "inner frame base");
                rejects<menu::MenuError>([&] { static_cast<void>(overlay.frame(0x20)); },
                                         "locals beyond the entered frame");
            }
            check(overlay.u32(locals[0x10]) == 0x12345678, "stack contents");
        }
        rejects<menu::MenuError>([&] { overlay.put8(0x80400000, 0); }, "address beyond RAM");
    }
    check(program.resident.game_data[2] == 0xef && program.resident.game_data[3] == 0xbe,
          "game data returns to the resident state");
    check(program.menu->stack.empty(), "the stack is released with the overlay");
}

void heap_and_library() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    const auto block = overlay.allocate(0x20, 0, 0x801c5f44);
    check(block == 0x80180008 && overlay.u8(block) == 0xee, "allocation hands the held bytes");
    check(overlay.bzero(block, 0x20) == block && overlay.u32(block + 0x1c) == 0, "bzero");
    check(overlay.bzero(block, 0) == 0 && overlay.bzero(0, 4) == 0, "bzero without bytes");
    overlay.put32(block, 0x00636261); // "abc"
    check(overlay.strlen(block) == 3, "strlen");
    check(overlay.strcpy(block + 8, block) == block + 8 && overlay.u32(block + 8) == 0x00636261,
          "strcpy");
    check(overlay.strcat(block + 8, block) == block + 8 && overlay.strlen(block + 8) == 6,
          "strcat");
    check(overlay.strcat(block, block) == 0, "strcat onto itself");
    check(overlay.memmove(block + 1, block, 3) == 0x61 && overlay.u8(block + 3) == 0x63,
          "memmove backward copy returns the last byte");
    check(overlay.memcpy(block + 0x10, block, 2) == block + 0x10, "memcpy");
    overlay.release(block, 0x801c60fc);
    check(!program.menu->regions.contains(block), "a released block leaves menu memory");
    rejects<menu::MenuError>([&] { overlay.release(0x80170000, 0x801c60fc); },
                             "releasing a block the menu does not own");
    // rand (8003fa38) through 8001bd40.
    program.resident.random_seed = 1;
    check(overlay.random_range(0xff, 3) == 0xff && overlay.random_range(2, 0) == 0 &&
              overlay.random_range(7, 7) == 7,
          "random range special cases");
    const auto value = overlay.random_range(0, 0xff);
    check(program.resident.random_seed == 1U * 0x41c64e6dU + 12345U &&
              value == ((program.resident.random_seed >> 16U) & 0xffU),
          "random byte");
}

void gpu_helpers() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    const auto packet = state + 0x100;
    const auto table = state + 0x200;
    overlay.put32(table, 0x00ffffff);
    overlay.set_poly_ft4(packet);
    check(overlay.u8(packet + 3) == 9 && overlay.u8(packet + 7) == 0x2c, "SetPolyFT4");
    overlay.set_semi_trans(packet, 1);
    overlay.set_shade_tex(packet, 1);
    check(overlay.u8(packet + 7) == 0x2f, "semi-transparent raw texture");
    overlay.add_prim(table, packet);
    check((overlay.u32(table) & 0xffffffU) == (packet & 0xffffffU) &&
              (overlay.u32(packet) & 0xffffffU) == 0xffffffU,
          "AddPrim links the packet first");
    overlay.set_line_f3(packet + 0x40);
    check(overlay.u32(packet + 0x54) == 0x55555555U, "SetLineF3 terminator");
    check(menu::Overlay::get_clut(0x100, 0x1d1) == ((0x1d1U << 6U) | 0x10U), "GetClut");
    // 80035734: controller kind by the receive buffer.
    program.resident.pad.buffers[0][0] = 0;
    program.resident.pad.buffers[0][1] = 0x41;
    check(overlay.pad_kind(0) == 1, "digital pad");
    program.resident.pad.buffers[0][1] = 0x12;
    check(overlay.pad_kind(0) == 2, "mouse");
    program.resident.pad.buffers[0][1] = 0x23;
    check(overlay.pad_kind(0) == 0xffffffffU, "unknown kind");
    program.resident.pad.buffers[0][0] = 0xff;
    check(overlay.pad_kind(0) == 0, "no controller");
}

void functions() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    // 801c7f34: 1 hour, 23 minutes, 45 seconds of VSyncs.
    overlay.split_play_time(((1 * 60 + 23) * 60 + 45) * 60);
    check(overlay.u32(state + 0x2f4) == 1 && overlay.u32(state + 0x2f8) == 2 &&
              overlay.u32(state + 0x2fc) == 3 && overlay.u32(state + 0x300) == 4 &&
              overlay.u32(state + 0x304) == 5 && overlay.u32(state + 0x2ec) == 0,
          "play time digits");
    // 801d22c4 hides both cursors.
    overlay.put8(party + 3, 1);
    overlay.put8(party + 4, 1);
    overlay.hide_cursors();
    check(overlay.u8(party + 3) == 0 && overlay.u8(party + 4) == 0, "hide cursors");
    // 801c7d78 decodes the queue's first menu button: pressed 4000 (code 1,
    // sound 1 with menu sounds off).
    auto &queue = program.resident.input_queue;
    program.resident.pad.buffers[0] = {};
    queue.count = 1;
    queue.ring[4][0] = 0x4000;
    overlay.decode_input();
    check(overlay.u8(state + 0x325) == 1, "input code");
    overlay.decode_input();
    check(overlay.u8(state + 0x325) == 8, "no input");
    // Unrecovered paths stop explicitly.
    program.resident.pad.buffers[0][0] = 0xff;
    rejects<game::MissingDependency>([&] { overlay.decode_input(); }, "controller wait");
    overlay.put8(state + 0x336, 2);
    rejects<game::MissingDependency>([&] { static_cast<void>(overlay.run_command(0)); },
                                     "menu command 2");
    rejects<game::MissingDependency>([&] { static_cast<void>(overlay.call(0x801c5000, {})); },
                                     "an entry that is not a function");
    check(overlay.call(0x801d22c4, {}) == 0, "dispatch by entry address");
}

void arrivals() {
    auto program = sample();
    game::FrameServices services;
    services.vblank_waits = {{1, 2}, {3, 4}};
    // One interrupt arrival after two events; the dispatcher is not set up,
    // so delivering it fails: nothing may deliver it earlier.
    program.resident.platform.push_back({game::PlatformInput::Kind::interrupt, 2, 0});
    menu::Overlay overlay(program, services, entry_sp);
    overlay.pass_position();
    check(overlay.events() == 1 && program.resident.platform.size() == 1, "arrival waits");
    overlay.vsync();
    check(overlay.events() == 2 && program.resident.platform.size() == 1,
          "a service result counts once consumed");
    rejects<std::exception>([&] { overlay.pass_position(); }, "the arrival is due");
}
} // namespace

int main() {
    try {
        memory_and_frames();
        heap_and_library();
        gpu_helpers();
        functions();
        arrivals();
    } catch (const std::exception &error) {
        std::cerr << "menu overlay test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "menu overlay tests passed\n";
    return 0;
}
