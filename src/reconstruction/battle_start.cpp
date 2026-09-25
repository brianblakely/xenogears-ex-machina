// The battle's start as one flow: battle 80070f40 from its entry up to its
// first call of the turn procedure 800723e0, through the load 800b8098 and
// its intro swirl 800b7870. The steps are the reconstructions in
// battle_setup.cpp; this file holds the control flow that connects them.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_heap.hpp"
#include <algorithm>

namespace xem::reconstruction {
namespace {
// 80070f40, 800b8098 and 800b7870 frame sizes: the setup phases run with the
// stack pointer 800b7870 leaves.
constexpr std::uint32_t start_frame = 0x30;
constexpr std::uint32_t load_frame = 0x20;
constexpr std::uint32_t swirl_frame = 0x38;
constexpr std::uint32_t draw_a = 0x800c4a20; // the two draw environments
constexpr std::uint32_t draw_b = 0x800c8a90;
constexpr std::uint32_t draw_current = 0x800ccb00;
constexpr std::uint32_t ordering_current = 0x800ccb04;
constexpr std::uint32_t buffer_index = 0x800ccb34;
constexpr std::uint32_t buffer_stride = 0x4070; // 800c3eb0 + index * 4070h

void observed(const ProgramObserver &observe, const Program &program, std::string_view operation,
              std::uint32_t address) {
    if (observe)
        observe(program, {operation, address, {}, {}}, true);
}
} // namespace

// 800b7870-800b79b0 and 800b79b8-800b79f8: the other draw environment
// becomes the current one and its ordering table is cleared.
void Program::swap_battle_draw_buffer() {
    auto &data = *battle;
    const auto draw = data.u32(draw_current) == draw_a ? draw_b : draw_a;
    data.put32(draw_current, draw);
    data.put32(ordering_current, draw + 0x70);
    clear_ordering_table(draw + 0x70, 0x1000);
}

// 800b7870: the intro swirl. Each frame the setup steps advance once the
// disc is idle (800286cc): phase 0 (801e5840), the scene files (8001bb0c),
// phases 1 and 2; the swirl ends once all have run and 86 frames passed.
// Then the swirl block is released (800b7330), the display is disabled
// (SetDispMask(0)), the disc wait ends the reads and phase 3 runs.
void Program::battle_swirl(FrameServices &services, std::uint32_t stack,
                           const BattleStartPresent &present) {
    auto &data = *battle;
    auto &tasks = resident.sprite_tasks;
    tasks.head = 0; // 8001c944
    tasks.pending_head = 0;
    tasks.primary_count = 0;
    tasks.auxiliary_count = 0;
    tasks.wait_count = 0;
    // The screen passes through a 30000h block: StoreImage of the display
    // (0,0 320x224), bit 15 set on each pixel, LoadImage to 704,256.
    auto capture = resident::heap_allocate(resident.heap, 0x30000, 1, 0x800b78a4);
    if (!capture)
        throw battle::BattleError("A quiet null allocation in 800b7870");
    present(BattleStartPresentation::swirl_capture);
    if (resident::heap_release(resident.heap, *capture, 0x800b7938) != 0)
        throw battle::BattleError("The swirl's screen block was not released");
    swap_battle_draw_buffer();
    data.put32(buffer_index, 0);
    data.put32(draw_current, draw_a);
    for (const auto environment : {draw_a, draw_b}) {
        data.put8(environment + 0x18, 1); // dither
        for (std::uint32_t i = 0x19; i < 0x1c; ++i)
            data.put8(environment + i, 0xff); // background colour
    }
    // 800b73ec: the swirl's block (10f7ch), its geometry set up by 800b7424.
    auto block = resident::heap_allocate(resident.heap, 0x10f7c, 1, 0x800b73fc);
    if (!block)
        throw battle::BattleError("A quiet null allocation in 800b73ec");
    const auto swirl = block->address;
    data.regions.emplace(swirl, std::move(block->bytes));
    present(BattleStartPresentation::swirl_open);
    std::uint32_t frames = 86;
    std::uint32_t step = 1;
    std::uint32_t phase = 0;
    while (frames != 0 || step != 5) {
        if (frames > 0)
            --frames;
        swap_battle_draw_buffer();
        data.put32(buffer_index, 1U - data.u32(buffer_index));
        data.put32(0x800c3cb4, data.u32(ordering_current));
        if (disc_busy() == 0 && step < 5) {
            if (step == 2)
                battle_scene_files();
            else
                setup_battle_phase(phase++, services, stack);
            ++step;
        }
        // 80019ca0: the soft reset buttons.
        if (resident.input_queue.current[0] == 0x90c) // 80059570
            throw MissingDependency({"battle_swirl", 0x80019cb8, {}, {}}, "symbol:soft-reset",
                                    false, "The soft reset 80019cd0 is not reconstructed");
        // 800b6f0c and 800b7160 draw the swirl into the ordering table.
        present(BattleStartPresentation::swirl_draw);
        draw_sync(services);
        vertical_sync(services); // VSync(2)
        // 80021ad8(x, -12) on the background colour of the displayed buffer.
        const auto colour = draw_a + 0x19 + data.u32(buffer_index) * buffer_stride;
        for (std::uint32_t i = 0; i < 3; ++i)
            data.put8(colour + i, static_cast<std::uint8_t>(
                                      std::max(0, static_cast<int>(data.u8(colour + i)) - 12)));
        // PutDrawEnv, PutDispEnv and DrawOTag of the current buffer.
        present(BattleStartPresentation::swirl_show);
    }
    // 800b7330: DrawSync, then the swirl block's release.
    draw_sync(services);
    run_battle([&](battle::Battle &context) { release_battle_block(context, swirl, 0x800b7348); });
    set_display_mask(0);
    disc_wait(0);
    setup_battle_phase(3, services, stack);
}

// 800b8098: the load. 8001bbac's files, the swirl (modes 0, 5 and above 5),
// the effect lists after the disc wait, the stage (801e7210) and its result.
void Program::battle_load(std::uint32_t mode, FrameServices &services, std::uint32_t stack,
                          const BattleStartPresent &present, const ProgramObserver &observe) {
    battle_load_prologue(mode);
    battle_setup_files();
    observed(observe, *this, "battle_setup_files", 0x800b80bc);
    // 80070a10: modes 1-4 run setup module functions instead of the swirl.
    if (mode >= 1 && mode <= 4)
        throw MissingDependency({"battle_load", 0x800b80dc, {}, {}},
                                "symbol:battle-load-mode-module", false,
                                "800b8098's modes 1-4 (801e8588, 801e91e8, 801e9594, 801e893c) "
                                "are not reconstructed");
    battle_swirl(services, stack - load_frame - swirl_frame, present);
    observed(observe, *this, "battle_swirl", 0x800b814c);
    battle_effect_lists();
    observed(observe, *this, "battle_effect_lists", 0x800b8158);
    battle_after_scene(battle_stage_setup(services));
    observed(observe, *this, "battle_after_scene", 0x800b81b4);
}

// 801e7210: the stage. Its result is 800c4a38's.
std::uint32_t Program::battle_stage_setup(FrameServices &) {
    throw MissingDependency({"battle_stage_setup", 0x801e7210, {}, {}}, "symbol:battle-stage-setup",
                            false, "The stage setup 801e7210 is not reconstructed");
}

// 80071188..80071278: the opening (fades, 80077990, the camera, 8007819c)
// and the battle frames 800716d8 until the setup task sets 800ccc58.
void Program::battle_setup_frames(FrameServices &, const BattleStartPresent &,
                                  const ProgramObserver &) {
    throw MissingDependency({"battle_setup_frames", 0x8007118c, {}, {}},
                            "symbol:battle-setup-frames", false,
                            "The opening and the setup frames of 80070f40 are not reconstructed");
}

// 80070f40 up to its first call of 800723e0. `stack` is the stack pointer
// at 80070f40.
void Program::battle_start(FrameServices &services, std::uint32_t stack,
                           const BattleStartPresent &present, const ProgramObserver &observe) {
    battle_prologue();
    observed(observe, *this, "battle_prologue", 0x80071160);
    battle_load(resident.battle_request.mode, services, stack - start_frame, present, observe);
    battle_after_load();
    observed(observe, *this, "battle_after_load", 0x80071184);
    battle_renderer_setup(battle->u32(0x800c3dec));
    observed(observe, *this, "battle_renderer_setup", 0x80071188);
    battle_setup_frames(services, present, observe);
    observed(observe, *this, "battle_setup_frames", 0x80071278);
    battle_release_setup();
    observed(observe, *this, "battle_release_setup", 0x80071308);
    battle_adjust_party();
    observed(observe, *this, "battle_adjust_party", 0x80071310);
    battle_place_party();
    observed(observe, *this, "battle_place_party", 0x800723e0);
}

} // namespace xem::reconstruction
