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

// 80085890: the field's effect bank (file a8 of directory 4) in a block
// kept from the heap's coalescing (8006259c): read from the disc, or copied
// (8003f968) from the copy a battle kept (8005a4bc), which is released; then
// opened in the sound driver (80038428) once the SPU is idle (8003bdfc).
void Program::load_field_effect_bank() {
    static_cast<void>(select_directory(4, 0));
    const auto size = file_words(0xa8);
    const auto bank = load_block(size, 0, 0x800858b8);
    resident.field_effect_bank = bank;
    const auto keep = [&](std::uint32_t address) -> std::uint32_t & {
        const auto header = resident.heap.headers.find(address - 8);
        if (header == resident.heap.headers.end())
            throw field::FieldFormatError("An effect bank block has no heap header");
        return header->second[1];
    };
    keep(bank) |= resident::heap_keep; // 800320a4
    auto &bytes = resident.heap_contents.at(bank);
    if (resident.w_4f32c == 0xffffffffU) {
        static_cast<void>(read_file(0xa8, bank, 0, 0x80)); // 800295d8
        disc_wait(0);
    } else {
        const auto cache = resident.effect_bank_cache;
        for (std::uint32_t i = 0; i < size; ++i)
            bytes.at(i) = static_cast<std::uint8_t>(memory(cache + i, 1));
        keep(cache) &= ~resident::heap_keep; // 800320b8
        static_cast<void>(release_owned_block(cache, 0x8008593c));
    }
    // The sound driver owns the bank from here on.
    resident.sound.objects[bank] = std::move(resident.heap_contents.at(bank));
    resident.heap_contents.erase(bank);
    resident::link_effect_bank(resident.sound, bank); // 80038428
    static_cast<void>(sound_wait(0x10));              // 8003bdfc
    static_cast<void>(select_directory(4, 0));
    resident.w_4f32c = 0xffffffffU;
}

// 80077c88: the three party sprite files' blocks (14000h each, 8005a414),
// kept from coalescing.
void Program::allocate_party_sprites() {
    auto &heap = resident.heap;
    resident::heap_select_tag(heap, 8, 0); // 80032498
    constexpr std::array<std::uint32_t, 3> sites{0x80077ca8, 0x80077cc4, 0x80077cdc};
    for (std::size_t i = 0; i < 3; ++i)
        resident.party_sprite_blocks[i] = load_block(0x14000, 0, sites[i]);
    for (const auto address : resident.party_sprite_blocks)
        heap.headers.at(address - 8)[1] |= resident::heap_keep; // 800320a4
}

// 80071ee8: the pointer (mouse) setup: the pad buffers it reads (8007ad8c),
// its divisors 3 and 4 (8007ae14), its bounds (8007ada4) and both ports'
// start positions (8007ae2c), all scaled by the divisors.
void Program::init_pointer() {
    auto &state = loaded(*this);
    state.pointer_pads = {0x800625fc, 0x800625fc + 0x22};
    state.pointer_divisors = {3, 4};
    const auto bounds = [&](std::int32_t left, std::int32_t right, std::int32_t top,
                            std::int32_t bottom) {
        const auto x = static_cast<std::int32_t>(state.pointer_divisors[0]);
        const auto y = static_cast<std::int32_t>(state.pointer_divisors[1]);
        state.pointer_bounds = {left * x, top * y, right * x, bottom * y};
    };
    bounds(0, 0x140, 0, 0xe0);
    const auto start = [&](std::size_t port, std::int32_t x, std::int32_t y) {
        state.pointer_x[port] = x * static_cast<std::int32_t>(state.pointer_divisors[0]);
        state.pointer_y[port] = y * static_cast<std::int32_t>(state.pointer_divisors[1]);
    };
    start(0, 0x50, 0x64);
    start(1, 0xfa, 0x64);
    bounds(0, 0x12c, 0xa, 0xdc);
}

// 80070340(tim, x, y, clut x, clut y, clut w, clut h): the TIM's image
// (OpenTIM 800471b4, ReadTIM 800471c4) loaded at (x, y) and its CLUT at the
// given place and size; a CLUT y of -1 or a zero size keeps the TIM's.
void Program::load_tim_at(FrameServices &services, std::uint32_t tim, std::int32_t x,
                          std::int32_t y, std::int32_t clut_x, std::int32_t clut_y,
                          std::int32_t clut_w, std::int32_t clut_h) {
    auto &cursor = resident.gpu.tim_cursor;
    cursor = tim;
    auto at = cursor;
    if (memory(at) != 0x10)
        return;
    at += 4;
    const auto flags = memory(at);
    at += 4;
    if (resident.gpu.debug == 2)
        throw MissingDependency({"load_tim_at", 0x80047570, {}, {}}, "symbol:printf-80019964",
                                false, "libgpu TIM messages are not reconstructed");
    std::uint32_t clut_rect = 0, clut_words = 0;
    if ((flags & 8U) != 0) {
        clut_rect = at + 4;
        clut_words = memory(at) >> 2U;
        at += clut_words * 4U;
    }
    const auto image_rect = at + 4;
    cursor += (clut_words + (memory(at) >> 2U) + 2U) * 4U;
    const auto half = [&](std::uint32_t address, std::int32_t value) {
        set_memory(address, static_cast<std::uint16_t>(value), 2);
    };
    if (clut_rect != 0) {
        if (static_cast<std::int16_t>(clut_y) != -1) {
            half(clut_rect, clut_x);
            half(clut_rect + 2, clut_y);
        }
        if (static_cast<std::int16_t>(clut_w) != 0)
            half(clut_rect + 4, clut_w);
        if (static_cast<std::int16_t>(clut_h) != 0)
            half(clut_rect + 6, clut_h);
        load_image_at(services, clut_rect, clut_rect + 8); // 80044894
    }
    half(image_rect, x);
    half(image_rect + 2, y);
    load_image_at(services, image_rect, image_rect + 8);
}

// 80077620: the field's text images (file a7, read once while 8004f344 is
// clear, 8005a4a0): its offset table becomes addresses (8003342c), each of
// its eight TIMs is loaded where the table at 800adc44 places it (80070340),
// then the compass colors are read back from VRAM (0, fb) and the file's
// block released.
void Program::load_text_images(FrameServices &services) {
    auto &state = loaded(*this);
    auto &heap = resident.heap;
    const auto keep = [&](std::uint32_t address) -> std::uint32_t & {
        return heap.headers.at(address - 8)[1];
    };
    if (resident.w_4f344 == 0) {
        const auto size = file_words(0xa7);
        resident.text_images = load_block(size, 1, 0x80077658);
        keep(resident.text_images) |= resident::heap_keep; // 800320a4
        static_cast<void>(read_file(0xa7, resident.text_images, 0, 0x80));
        disc_wait(0);
    }
    const auto file = resident.text_images;
    keep(file) &= ~resident::heap_keep; // 800320b8
    resident.w_4f344 = 0;
    state.w_c2692 = 0;
    state.w_c2690 = 0;
    set_memory(0x800c38fe, 0, 2); // Two halfwords of the dialogue windows' records.
    set_memory(0x800c38fc, 0, 2);
    // 8003342c (resident; kept here until the shared helper merges): the
    // count, then offsets from the file that become addresses.
    const auto count = memory(file);
    for (std::uint32_t i = 1; i <= count; ++i)
        set_memory(file + 4 * i, memory(file + 4 * i) + file);
    for (std::uint32_t i = 0; i < 8; ++i) {
        const auto place = 0x800adc44U + 12 * i;
        const auto at = [&](std::uint32_t offset) {
            // The placement table is overlay data.
            const auto low = overlay_byte(place + offset);
            const auto high = overlay_byte(place + offset + 1);
            return static_cast<std::int32_t>(static_cast<std::int16_t>(low | high << 8U));
        };
        load_tim_at(services, memory(file + 4 + 4 * i), at(0), at(2), at(4), at(6), at(8), at(0xa));
        draw_sync(services);
    }
    state.compass_palette_rect = {0, 0xfb, 0x10, 1};
    auto rect = state.compass_palette_rect;
    store_image(services, rect, 0x800b004c, 0x800afc08); // 800448f8
    state.compass_palette_rect = rect;
    draw_sync(services);
    static_cast<void>(release_owned_block(file, 0x800777ac));
}

// 80077e88..80078154: the field mode's start: the release build (the word
// at 80010000 is -1) suppresses the diagnostics, the music's shared wave
// and the wave bank (800595ac) carried over, the effect bank and party sprite blocks, the loop's
// gates, the pointer, the map, entry and music of the departure the game
// data holds (8006f94e), the text images, the party sprites and the exit
// block (800adb30, released by the exit to a battle); the field entry
// (80078d44) follows.
void Program::field_mode_start(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &music = resident.music;
    auto &heap = resident.heap;
    const auto release_build = resident.debug_word == 0xffffffffU;
    state.event_control.diagnostic_suppression = release_build ? 1 : 0;
    draw_and_vertical_sync(services); // 8007999c; FlushCache touches no RAM
    if (!release_build)
        unrecovered("field_mode_start", 0x80077ef8, "symbol:field-debug-800444d8",
                    "The diagnostic build's setup (800444d8, 80280000) is not recovered");
    music.active_shared_wave = music.shared_wave;
    resident.w_62524 = resident.battle_wave; // 800595ac
    resident::heap_select_tag(heap, 8, 0);   // 80032498
    if (resident.w_4f30c == 0)
        for (const std::uint32_t at : {0x8006fac4U, 0x8006fac0U, 0x8006fabcU})
            set_memory(at, 0xff);
    load_field_effect_bank(); // 80085890
    allocate_party_sprites(); // 80077c88
    state.gate_adbe8 = 0xffffffffU;
    state.event_control.gate_values[1] = -1; // 800adbe4
    state.event_control.gate_values[0] = -1; // 800adbe0
    resident.battle_request.field_active = 0xffffffffU;
    state.gate_adbd8 = 0xffffffffU;
    music.sequence_pending = 0;
    music.wave_pending = 0;
    state.w_adc10 = 0;
    state.reload.stream_pending = 0;
    state.particles_paused = 0;
    state.w_adb7c = 0;
    state.fade.mode = 2;
    // 800775c0.
    resident::heap_select_tag(heap, 8, 0); // 80032498
    static_cast<void>(select_directory(4, 0));
    init_pointer(); // 80071ee8
    resident.game_state = 0x8006d634;
    auto &data = resident.game_data;
    if (data.size() != game_data_bytes)
        throw field::FieldFormatError("The field mode's start requires the game data");
    const auto half = [&](std::size_t at) {
        return static_cast<std::uint32_t>(data[at] | data[at + 1] << 8U);
    };
    const auto put_half = [&](std::size_t at, std::uint32_t value) {
        data[at] = static_cast<std::uint8_t>(value);
        data[at + 1] = static_cast<std::uint8_t>(value >> 8U);
    };
    resident.field_map = half(0x231a);
    put_half(0x1932, half(0x2320));
    put_half(0x1938, half(0x231c) >> 9U);
    if (resident.w_4f2f8 == 0) {
        resident.departure_594d0 = 0;
        music.requested = 0xff;
    } else {
        music.requested = half(0x2322);
    }
    if (release_build) {
        resident.variables.write(0x50, 1); // 800a3074(50, 1)
        put_half(0x1980, 1);
    }
    load_text_images(services); // 80077620
    resident.party_sprite_load.needed = 0;
    prepare_party_sprites(); // 8001b044
    decode_party_sprites();  // 8001b3a8
    state.w_adb30 = load_block(4, 1, 0x80078108);
    reach_position(0x80078154);
    if (observe)
        observe(*this, {"field_mode_start", 0x80078154, {}, {}}, true);
}

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
