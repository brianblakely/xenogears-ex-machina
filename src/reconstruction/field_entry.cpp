// The field entry 80078d44 that the main loop 80077e88 runs when the field
// mode starts (after a battle, a menu or a load): the screen saved and faded,
// the map read and loaded (80070cc8), its image stream, the return's actor
// adjustments, the music and the fade-in frames. Original addresses name
// correlations only; owned records are Program state.
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("The field entry requires field state");
    return *program.field;
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return std::bit_cast<std::uint32_t>(value); }
void unrecovered(std::string_view operation, std::uint32_t address, const char *id,
                 const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}
} // namespace

// 800777dc: read the map's data ahead (8001b484 of map * 2 in slot 0) until
// it is the data read ahead. A call that finds the disc busy starts nothing;
// the interrupts that end the earlier read arrive before the next.
void Program::read_map_ahead() {
    disc_wait(0);
    for (;;) {
        const auto id = (resident.field_map & 0xfffU) << 1U;
        if (preload_field(id, 0) == 0)
            return;
        const bool started = resident.preload_slot == 0 && resident.preload_id == id;
        if (!started && !deliver_interrupt())
            unrecovered("read_map_ahead", 0x80077804, "interrupt:disc-read-completion",
                        "Reading the map ahead needs the interrupt arrivals");
    }
}

// 80071fb0: the display setup: the geometry defaults (80048bc4) and offset
// (8004a12c), both draw blocks' environments (draw at +0 and +5c, display at
// +b8), their areas (80071f64) and screen rectangles (80086d8c), black
// backgrounds, then the second block shown and the renderer's limits
// (8002dff0).
void Program::init_display(FrameServices &services) {
    resident.sprite.rate_control = 1; // 80059198
    draw_sync(services);
    vertical_sync(services);
    init_geometry(0x80071fe8);       // 80048bc4 InitGeom
    set_geometry_offset(0xa0, 0x70); // 8004a12c SetGeomOffset
    constexpr std::uint32_t first = 0x800b249c, second = 0x800ba590;
    set_default_draw_environment(first, 0, 0, 0x140, 0xe0);
    set_default_draw_environment(second, 0, 0x100, 0x140, 0xe0);
    set_default_draw_environment(first + 0x5c, 0, 0, 0x140, 0xe0);
    set_default_draw_environment(second + 0x5c, 0, 0x100, 0x140, 0xe0);
    set_default_display_environment(first + 0xb8, 0, 0x100, 0x140, 0xe0);
    set_default_display_environment(second + 0xb8, 0, 0, 0x140, 0xe0);
    for (const auto block : {first, second}) { // 80071f64(0, 0, 140, e0)
        set_memory(block, 0, 2);
        set_memory(block + 2, block == first ? 0 : 0x100, 2);
        set_memory(block + 4, 0x140, 2);
        set_memory(block + 6, 0xe0, 2);
    }
    for (const auto block : {first, second}) { // 80086d8c
        const auto screen = block + 0xb8 + 8;
        set_memory(screen, 0, 2);
        set_memory(screen + 2, 10, 2);
        set_memory(screen + 4, 0x100, 2);
        set_memory(screen + 6, 0xd8, 2);
    }
    for (const auto block : {first, second}) {
        for (std::uint32_t c = 0x19; c < 0x1c; ++c)
            set_memory(block + c, 0, 1);
        set_memory(block + 0x16, 1, 1);
    }
    vertical_sync(services);
    put_disp_env(second + 0xb8);
    put_draw_env(services, second);
    resident.sprite_models.x_limit = 0x140; // 8002dff0(140, f0)
    resident.sprite_models.y_limit = (0xf0U - 1U) << 16U;
}

// 800ad898: with the party reassigned (800b2268), members of party mode 1
// are shown by their actors (+0 bit 200, not 100 or 400).
void Program::show_reassigned_party() {
    auto &state = loaded(*this);
    if (state.party_reassignment == 0)
        return;
    for (std::size_t slot = 0; slot < 3; ++slot) {
        const auto index = state.party_indices[slot];
        if (index == 0xff || resident.party_modes()[slot] != 1)
            continue;
        const auto actor = memory(state.reload.descriptor_table + 0x5cU * u32(index) + 0x4c);
        set_memory(actor, memory(actor) | 0x200U);
        set_memory(actor, memory(actor) & ~0x500U);
    }
}

// 800a22ac(entry): run actor 0's script `entry` (800a3090) as one
// initialization batch (800a1ec8), its record saved to a heap block and put
// back after; every slot's wait state is reset first.
void Program::run_actor0_script(std::uint32_t entry, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &actor = state.actors.at(0);
    state.published_actor = 0;
    const auto saved = load_block(0x138, 1, 0x800a22e0);
    std::ranges::copy(actor.storage, resident.heap_contents.at(saved).begin());
    auto &a = actor.storage;
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        const auto at = 8 * slot;
        a[0x8e + at] = 0;
        a[0x8f + at] = 0xff;
        a[0x8c + at] = 0xff;
        a[0x8d + at] = 0xff;
        std::uint32_t control = 0;
        for (std::uint32_t i = 0; i < 4; ++i)
            control |= static_cast<std::uint32_t>(a[0x90 + at + i]) << (8U * i);
        control = (control & 0xfffcffffU) | 0x3c0000U;
        control &= 0xffbfffffU;
        control = (control & 0xffff0000U) | 0xffffU;
        control &= 0xfe7fffffU;
        for (std::uint32_t i = 0; i < 4; ++i)
            a[0x90 + at + i] = static_cast<std::uint8_t>(control >> (8U * i));
    }
    state.event_control.post_initialization = 0;
    state.event_control.budget_mode = 0;
    const auto pc = state.event_package.entries.at(0).at(entry);
    a[0xcc] = static_cast<std::uint8_t>(pc);
    a[0xcd] = static_cast<std::uint8_t>(pc >> 8U);
    static_cast<void>(event_batch(0, 0xffff, observe));
    state.event_control.post_initialization = 1;
    std::ranges::copy(resident.heap_contents.at(saved), actor.storage.begin());
    static_cast<void>(release_owned_block(saved, 0x800a2468));
}

// 800a24c4: after a return (8004f30c): actor 0's script 2 (800a22ac),
// reassigned party members shown (800ad898), the controlled actor 8 higher
// unless its +74 is ff, and the pieces (800b21d2 mode 0 or 1) moved by their
// accumulated drift (800b21bc).
void Program::adjust_after_return(const ProgramObserver &observe) {
    auto &state = loaded(*this);
    if (resident.w_4f30c == 0)
        return;
    run_actor0_script(2, observe);
    show_reassigned_party();
    if (s32(state.w_b2264) > 0)
        unrecovered("adjust_after_return", 0x800a251c, "symbol:field-801e8330",
                    "The 801e7fd4 module's return (801e8330) is not recovered");
    const auto table = state.reload.descriptor_table;
    const auto leader = memory(table + 0x5cU * u32(state.controlled_actor) + 0x4c);
    if (memory(leader + 0x74, 1) != 0xff)
        set_memory(leader + 0x24, memory(leader + 0x24) - 8);
    const auto mode = state.piece_drift_mode & 0x7fU;
    const auto drift = [&](std::uint32_t descriptor) {
        for (std::uint32_t axis = 0; axis < 3; ++axis)
            set_memory(descriptor + 0x20 + 4 * axis,
                       memory(descriptor + 0x20 + 4 * axis) + u32(state.piece_drift_total[axis]));
    };
    for (std::uint32_t i = 0; s32(i) < s32(state.descriptor_count); ++i) {
        const auto descriptor = table + 0x5c * i;
        if (s32(i) < s32(state.reload.event_actors)) {
            if ((memory(memory(descriptor + 0x4c) + 0x12c) & 3U) != 0)
                continue;
        } else if (mode == 0) {
            drift(descriptor);
        }
        if (mode == 1)
            drift(descriptor);
    }
}

// 80078d44 with its own stack frame `frame` (the main loop's SP - 30h).
void Program::field_entry(FrameServices &services, std::uint32_t frame,
                          const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    const auto done = [&](std::string_view operation, std::uint32_t address) {
        reach_position(address);
        if (observe)
            observe(*this, {operation, address, {}, {}}, true);
    };
    deliver_stage_arrivals();
    load_text_palette(services, frame); // 80077544
    done("entry_text_palette", 0x80078d64);
    save_particle_vram(services); // 800a915c
    done("entry_save_vram", 0x80078d6c);
    static_cast<void>(select_directory(4, 0));
    read_map_ahead(); // 800777dc
    done("entry_map_ahead", 0x80078d80);
    draw_sync(services); // 800775f8
    vertical_sync(services);
    done("entry_sync", 0x80078d88);
    if (resident.w_4f2f8 == 0)
        unrecovered("field_entry", 0x80078d9c, "symbol:field-800a77c4",
                    "The first entry's screen setup (800a77c4, then a screen move) is not "
                    "recovered");
    init_display(services); // 80071fb0
    done("entry_display", 0x80078dac);
    state.draw_buffer = 1;
    copy_screen(services, 0x2c0, 0x100); // 800a4748
    copy_screen(services, 0, 0x100);     // 800a476c
    draw_sync(services);
    swap_draw_buffer();  // 80073fe0
    draw_sync(services); // 800775f8
    vertical_sync(services);
    const bool quick = resident.departure_594d0 == 1; // 800594d0
    const bool cut = resident.b_5942c == 1;           // 8005942c
    if (quick || cut)
        unrecovered("field_entry", 0x80078e4c, "symbol:field-entry-without-transition",
                    "An entry without the transition quads (800a5884(0, 0)) is not recovered");
    reload_screen_fade(services, frame); // 800a5884(1, 1)
    done("entry_screen_fade", 0x80078e64);
    resident.w_4f2f8 = 1;
    disc_wait(0);
    done("entry_disc_wait", 0x80078e74);
    static_cast<void>(select_directory(4, 0));
    load_field(services, frame - 0xa0, observe); // 80070cc8
    done("entry_load", 0x80078e88);
    start_field_stream(); // 80070488
    done("entry_stream", 0x80078e90);
    state.dialogue_gate_afd04 = 1;
    if (state.w_b2264 != 0)
        unrecovered("field_entry", 0x80078ea8, "symbol:field-801e7378",
                    "The 801e7fd4 module's entry (801e7378) is not recovered");
    constexpr std::int32_t zoom_step = 0x20; // Zero for the refused entries.
    if (reload.stream_pending == 1) {
        // The transition quads, zooming in, until the stream is read.
        do {
            swap_draw_buffer();
            reload_transition_draw();
            reload_present(services);
            if (reload.zoom < 0x22c0)
                reload.zoom += zoom_step;
        } while (disc_busy() != 0);
        draw_sync(services);
        release_music_buffer(reload.stream_ring, 0x80078f80);
        reload.stream_pending = 0;
        brighten_text_strip(services, frame - 0x20); // 80078c5c
    }
    done("entry_stream_read", 0x80078f98);
    if (resident.w_4f304 != 0)
        unrecovered("field_entry", 0x80079028, "symbol:field-entry-sound-restore",
                    "Restoring the saved sound state (80039c4c, 800399d4, 80038310) is not "
                    "recovered");
    adjust_after_return(observe); // 800a24c4
    done("entry_return", 0x80079068);
    resident.w_4f310 = 0;
    resident.w_4f30c = 0;
    draw_sync(services); // 800775f8
    vertical_sync(services);
    resident.input_queue.reset(); // 80035db0
    auto &music = resident.music;
    music.gate = 0;
    if (music.loaded_sequence != music.requested) {
        stop_music(); // 8001b66c
        music.gate = 0xffffffffU;
        if (music.cached_sequence != 0)
            music.reuse_sequence = 1;
        load_music(music.requested); // 80085b20
    } else {
        release_cached_sequence(); // 80085eec
    }
    done("entry_music", 0x80079118);
    record_play_state(); // 800a31e8
    done("entry_play_state", 0x80079120);
    field::begin_fade_out(state.fade, 0x20); // 80071e58(20)
    std::int32_t shade = 0x800000;
    for (std::uint32_t n = 0; n < 32; ++n) {
        field_pre_frame(services); // 80077dac
        reload_transition_draw();  // 800a6408
        field_frame(services, observe);
        field_post_frame(); // 80078b5c
        reload_transition_shade(u32(shade >> 16));
        shade = std::max(shade - 0x40000, 0);
        if (reload.zoom < 0x22c0)
            reload.zoom += zoom_step;
    }
    deliver_arrivals(0x80077db4); // Since the last fade frame's exit.
    done("entry_fade_in", 0x80079220);
    restore_particle_vram(services); // 800a91f0
    done("entry_restore_vram", 0x80079244);
    resident::heap_coalesce(resident.heap); // 80031ff8
    release_named_block();                  // 8003748c
    load_text_palette(services, frame);     // 80077544
    state.dialogue_gate_afd04 = 0;
    done("field_entry", 0x80079280);
}

// The main loop after its entry (8007815c): encounters enabled (800adb04)
// and s5 (the saved music flag) clear, then its top up to the first frame.
void Program::field_loop_start(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    state.music_saved = false;
    state.control_inputs.encounter.enabled_byte = 1;
    field_loop_top(services, observe);
}

} // namespace xem::reconstruction
