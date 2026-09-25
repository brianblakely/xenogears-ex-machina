// Invented state exercises the mode dispatcher's resumable steps and the
// graphics setup calls. It describes no original content or observation.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

#include <iostream>

namespace game = xem::reconstruction;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call> void missing(Call call, const char *message) {
    bool stopped = false;
    try {
        call();
    } catch (const game::MissingDependency &) {
        stopped = true;
    }
    check(stopped, message);
}

// A heap of one free block from 80100008 to the end block at 80100300,
// with two kept tag-6 blocks above it.
game::Program heap_program() {
    game::Program program;
    auto &heap = program.resident.heap;
    heap.head = 0x80100008;
    heap.headers = {{0x80100000, {0x80100308, 0x84000000}},
                    {0x80100300, {0x80100408, 6U << 21U | game::resident::heap_keep}},
                    {0x80100400, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80100008, std::vector<std::uint8_t>(0x2f8, 0x11)}};
    return program;
}

void reinit() {
    auto program = heap_program();
    auto &resident = program.resident;
    resident.next_mode = 2; // battle: BSS end 800d39f0 in the mode table
    resident.mode_loaded = 2;
    resident.heap.dirty = 1;
    resident.input_queue.count = 3;
    resident.w_59334 = 7;
    // The RAM between the new start (800d39f4) and the heap is outside it.
    missing(
        [&] {
            game::FrameServices services;
            auto copy = heap_program();
            copy.resident.next_mode = 9;
            static_cast<void>(copy.mode_dispatch(services, game::DispatchStep::reinit));
        },
        "An unknown mode stops at the mode table");
    // A restart below the heap needs the RAM there: here the invented heap
    // lies above 800d39f4, so the RAM from there is supplied outside it.
    resident.heap_outside.emplace(0x800d39f4,
                                  std::vector<std::uint8_t>(0x80100000 - 0x800d39f4, 0x22));
    game::FrameServices services;
    const auto function = program.mode_dispatch(services, game::DispatchStep::reinit);
    check(function == 0x8001b6c4, "The battle row names 8001b6c4");
    check(resident.heap.head == 0x800d39fc && resident.heap_outside.empty() &&
              resident.heap.headers.at(0x800d39f4)[0] == 0x80100308,
          "The heap restarts after the BSS end");
    check(resident.heap.allocation_class == 0x20 && resident.heap.tag == 10 &&
              resident.w_59334 == 0 && resident.heap.tag_words[10] == 0,
          "80031a30 resets the heap class and tag");
    check(resident.input_queue.count == 0 && resident.input_queue.w50200 == 1,
          "The input queue is reset");
    check(resident.next_mode == 0 && resident.mode_loaded == 0xffffffffU,
          "The next mode becomes 0");
}

void graphics() {
    game::Program program;
    program.battle.emplace();
    program.battle->regions.emplace(0x800c4a00, std::vector<std::uint8_t>(0x100, 0xee));
    program.resident.video_mode = 0;
    program.set_default_draw_environment(0x800c4a20, 0, 0xe0, 0x140, 0xe0);
    program.set_battle_draw_modes(0x800c4a20);
    const auto &memory = *program.battle;
    check(memory.u16(0x800c4a22) == 0xe0 && memory.u16(0x800c4a24) == 0x140 &&
              memory.u16(0x800c4a2a) == 0xe0 && memory.u16(0x800c4a34) == 10 &&
              memory.u8(0x800c4a37) == 1 && memory.u8(0x800c4a38) == 1 &&
              memory.u8(0x800c4a39) == 0x3c && memory.u8(0x800c4a3b) == 0x78,
          "Draw environments land in battle memory");
    program.set_default_display_environment(0x800c4a7c, 0, 0xe0, 0x140, 0xe0);
    check(memory.u16(0x800c4a7e) == 0xe0 && memory.u16(0x800c4a82) == 0xe0 &&
              memory.u8(0x800c4a8c) == 0,
          "Display environments land in battle memory");
    program.init_geometry(0x8001b89c);
    program.set_geometry_offset(0xa0, 0xb4);
    program.set_geometry_screen(0x200);
    const auto &gte = program.resident.gte;
    check(gte.control(24) == 0xa00000 && gte.control(25) == 0xb40000 && gte.control(26) == 0x200 &&
              program.resident.geometry_return == 0x8001b89c,
          "The geometry offset and screen distance follow InitGeom");
}
} // namespace

int main() {
    try {
        reinit();
        graphics();
        std::cout << "Mode dispatch: two groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
