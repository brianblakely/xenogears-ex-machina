// Invented state exercises the mode dispatcher's resumable steps, the
// graphics setup calls and the battle renderer setup. It describes no original content or
// observation.
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

// Battle memory from 800c3000 to 800d3500 and invented scene data; enemies
// in slots 3-7 count, slot 8 is off the field and slot 9 absent. A heap of
// one free block takes both allocations; the wave bank is the only one.
game::Program renderer_program() {
    game::Program program;
    auto &resident = program.resident;
    program.battle.emplace();
    auto &memory = *program.battle;
    memory.regions.emplace(0x800c3000, std::vector<std::uint8_t>(0x10500));
    constexpr std::uint32_t scene = 0x80140000;
    memory.regions.emplace(scene, std::vector<std::uint8_t>(0x500));
    resident.battle_scene_data = scene;
    for (std::uint32_t i = 0; i < 12; i += 2)
        memory.put16(scene + 0x47c + i, 0x100 + i);
    for (std::uint32_t slot = 3; slot < 10; ++slot) {
        memory.put8(0x800c3eb4 + slot * 0x1c + 2, slot == 8 ? 0x11 : 0x10);
        memory.put8(0x800c3eb4 + slot * 0x1c + 4, slot == 9 ? 0 : 1);
    }
    memory.put32(0x800ccb00, 0x800c4a20);
    for (std::uint32_t i = 0; i < 4; ++i)
        memory.put8(0x800c4a38 + i, i + 1);
    auto &heap = resident.heap;
    heap.head = 0x80100008;
    heap.headers = {{0x80100000, {0x8010c008, 0x84000000}},
                    {0x8010c000, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80100008, std::vector<std::uint8_t>(0xbff8, 0x11)}};
    auto &gpu = resident.gpu;
    gpu.services = 0x80056888;
    gpu.functions[4] = 0x80046560;
    gpu.functions[11] = 0x80045d5c;
    gpu.otc_registers = {0x1f8010e0, 0x1f8010e4, 0x1f8010e8, 0x1f8010f0};
    resident.io[0xf0] = 0x21;
    resident.platform = {{game::PlatformInput::Kind::read, 0x80045de4, 0}};
    auto &tasks = resident.sprite_tasks;
    tasks.serial = 7;
    tasks.head = 0x80130000;
    tasks.primary_count = 4;
    tasks.creation_flags = 1;
    resident.null_owner_generation = 0xe0000005;
    // Wave bank 80120010 (SPU block 1010, table entry 2) behind the pool head.
    auto &sound = resident.sound;
    constexpr std::uint32_t head = 0x80120000, wave = 0x80120110;
    sound.pool = head;
    sound.pool_headers = {{head, {0x8000, 0, 0x80120100, wave - 0x10}},
                          {wave - 0x10, {2, 0, 0x80120200, 0}}};
    sound.objects[wave].resize(0x40);
    sound.objects[wave][0x29] = 0x10;
    sound.objects[wave][0x28] = 0x10;
    sound.wave_banks = wave;
    sound.spu_blocks[2] = 2;
    sound.spu_blocks[16 * 2] = 1;
    sound.spu_blocks[16 * 2 + 4] = 0x10;
    sound.spu_blocks[16 * 2 + 5] = 0x10;
    resident.battle_wave = wave;
    return program;
}

void renderer() {
    auto program = renderer_program();
    auto &resident = program.resident;
    program.battle_renderer_setup(0x801a0000);
    const auto &memory = *program.battle;
    check(memory.u32(0x800c3d58) == 5 && memory.u32(0x800ccc5c) == 1 &&
              resident.sprite.rate_control == 0 && resident.sprite_models.depth_shift == 2,
          "Five enemies on the field give the rate 1");
    check(memory.u32(0x800ccb04) == 0x800c8b00 && memory.u32(0x800ccb00) == 0x800c8a90 &&
              memory.u32(0x800c8b00 + 0x3ffc) == 0x0c8b00 + 0x3ff8 &&
              memory.u32(0x800c8b00) == 0x05698c && resident.platform.empty(),
          "The second buffer's ordering table is cleared");
    check(memory.u8(0x800c8aa8) == 1 && memory.u8(0x800c8aab) == 4 && memory.u32(0x800ccb34) == 1 &&
              memory.u32(0x800c3cbc) == 1 && memory.u32(0x800c3674) == 0x200 &&
              memory.u32(0x800c3678) == 0xffffffffU,
          "The draw modes are copied and the display globals set");
    check(resident.sprite.platform_mode == 1 && resident.sprite.platform_argument == 0x2000 &&
              resident.sprite_arena_bytes == 0x5000 &&
              resident.sprite_arenas[1] == resident.sprite_arenas[0] + 0x5000 &&
              resident.heap_contents.contains(resident.sprite_arenas[0]),
          "Both 5000h sprite arenas share one block");
    const auto &tasks = resident.sprite_tasks;
    check(tasks.nodes.size() == 1 && tasks.head == tasks.nodes[0].address &&
              tasks.primary_count == 1 && tasks.auxiliary_count == 0 && tasks.serial == 8 &&
              tasks.allocation_mode == 0 && tasks.creation_flags == 0,
          "The setup module's task is the only one");
    const auto word = [&](std::uint32_t at) {
        const auto &bytes = tasks.nodes[0].bytes;
        return static_cast<std::uint32_t>(bytes[at]) |
               static_cast<std::uint32_t>(bytes[at + 1]) << 8U |
               static_cast<std::uint32_t>(bytes[at + 2]) << 16U |
               static_cast<std::uint32_t>(bytes[at + 3]) << 24U;
    };
    check(word(0) == 0 && word(4) == 0 && word(8) == 0x801e6fec && word(0xc) == 0x8001ce44 &&
              word(0x10) == 7 && word(0x14) == 5 && word(0x18) == 0 && word(0x20) == 0x801a0000,
          "The task keeps its callbacks, generation and argument");
    check(resident.sound.wave_banks == 0 && resident.sound.spu_blocks[16 * 2] == 0,
          "The wave bank is released");
    check(memory.u16(0x800d30a0) == 0x106 && memory.u16(0x800d3358) == 0x10a &&
              memory.u16(0x800d30a8) == 0x100 && memory.u16(0x800d3360) == 0x104,
          "The light vectors come from the scene data");
    const auto &gpu = resident.gpu;
    check(gpu.control[3] == 0 && gpu.commands.back().value == 0x03000000U,
          "The display is enabled");

    auto pending = renderer_program();
    pending.battle->put32(0x800c3684, 0x80150000);
    missing([&] { pending.battle_renderer_setup(0); }, "A pending camera callback stops");
    auto masked = renderer_program();
    masked.set_display_mask(0);
    check(masked.resident.gpu.commands.back().value == 0x03000001U &&
              masked.resident.gpu.display_environment[0] == 0xff,
          "Masking the display disables it and forgets the last environment");
}
} // namespace

int main() {
    try {
        reinit();
        graphics();
        renderer();
        std::cout << "Mode dispatch: three groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
