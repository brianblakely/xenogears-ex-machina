// The field movie player 800a7c58 of field overlay 38a1ce82..., which the
// main loop runs for a movie request (extended 60) and which returns to it
// afterwards. Stages, each an observable boundary:
//
//   prepare (800a7c58..800a7e58): read the movie library (file a9) and
//     move it to 801d3000, park VRAM the movie overwrites, stop the field's
//     effects, open the library (800a708c) and start the stream (800a7218);
//   first frame (800a7e5c..800a7e7c): VSync and decode until the frame
//     callback 800a7120 reports the first frame;
//   passes (800a7e88..800a80b0): present by the request's presentation mode,
//     decode, then the loop's decision (media.cpp);
//   finish (800a80b4..800a8308): close the library, restore VRAM, the
//     display environments and the field stream, and clear the request.
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("The movie player requires loaded field state");
    return *program.field;
}
[[noreturn]] void unrecovered(std::string_view operation, std::uint32_t address, const char *id,
                              const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}
void observed(const ProgramObserver &observe, const Program &program, std::string_view operation,
              std::uint32_t address) {
    if (observe)
        observe(program, {operation, address, {}, {}}, true);
}
// The two draw-buffer blocks (800b249c + 80f4 * index): the draw environment
// at +0, the display environment at +b8.
constexpr std::uint32_t draw_blocks = 0x800b249c;
constexpr std::uint32_t draw_block_bytes = 0x80f4;
// The movie's display-environment pair used after the movie (800ba590).
constexpr std::uint32_t movie_environment = 0x800ba590;
// VRAM the movie overwrites is parked in two heap blocks (8005a418, 8005a41c).
constexpr std::uint32_t parked_vram = 0x8005a418;
// 800adb6c: zero once the library opened; -1 when no movie plays.
constexpr std::uint32_t start_failed = 0x800adb6c;
// 800a77c4's read and write positions (800c3904, 800c390c).
constexpr std::uint32_t read_cursor = 0x800c3904;
constexpr std::uint32_t write_cursor = 0x800c390c;
} // namespace

// 800a7120, the library's frame callback (interrupt context): record the
// frame, clear the first-frame wait, select the draw block of the display
// buffer the frame went to (y 0: block 1) and, for 24-bit movies shown
// full screen, mark that block's display environment 24-bit.
void Program::movie_frame_ready(std::uint32_t callback, std::uint32_t frame, std::uint32_t,
                                std::uint32_t y) {
    if (callback == 0x800768d8 && movie_mode_memory) {
        movie_mode_frame_ready(frame, y);
        return;
    }
    if (callback != 0x800a7120)
        unrecovered("movie_frame_ready", 0x801d3480, "symbol:movie-frame-callback",
                    "A movie frame callback other than the field's 800a7120 is not reconstructed");
    auto &state = loaded(*this);
    state.movie.frame = frame & 0xffffU;
    state.movie_frame_pending = 0;
    state.movie_display = (y & 0xffffU) == 0 ? 1 : 0;
    if (state.single_actor_mode != 0 || state.movie_overlay_active != 0)
        return;
    draw_sync_polled(); // DrawSync(0) inside the callback
    state.draw_block = draw_blocks + draw_block_bytes * state.movie_display;
    if (static_cast<std::int16_t>(state.movie.request.buffer_mark) == 1)
        set_memory(draw_blocks + draw_block_bytes * (state.movie_display & 1U) + 0xc9, 1, 1);
}

// 800a7394: run field frames until the disc and the draw buffer are idle,
// then wait for the CD DMA (80041410 -> 8004293c CD_datasync(0)).
void Program::movie_wait_disc(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    for (;;) {
        deliver_arrivals(0x800a739c);
        field_pre_frame(services); // 80077dac
        field_frame(services, observe); // 8007554c
        deliver_arrivals(0x800a73ac);
        if (disc_busy() == 0 && state.draw_buffer == 0)
            break;
    }
    deliver_arrivals(0x800a73d0);
    cd_datasync_wait();
}

// 800775f8: DrawSync(0), then VSync(0).
void Program::draw_and_vertical_sync(FrameServices &services) {
    deliver_arrivals(0x80077600);
    draw_sync(services);
    deliver_arrivals(0x80077608);
    vertical_sync(services);
}

// 800a73e8: load the two VRAM areas parked in the kept blocks 8005a418 and
// 8005a41c back to (200, 0) and (200, 80), then release both blocks.
// Presentation 2 only releases them. `frame` is 800a73e8's stack frame.
void Program::movie_release_parked(FrameServices &services, std::uint32_t frame) {
    auto &state = loaded(*this);
    if (state.single_actor_mode != 2) {
        const std::array<std::array<std::uint32_t, 3>, 2> areas{{
            {0, 0x800a7468, 0x800a7470},
            {0x80, 0x800a7490, 0x800a7498},
        }};
        for (std::uint32_t k = 0; k < 2; ++k) {
            std::array<std::int16_t, 4> rect{0x200, static_cast<std::int16_t>(areas[k][0]), 0x140,
                                             0x80};
            deliver_arrivals(areas[k][1]);
            static_cast<void>(load_image(rect, frame + 0x10, memory(parked_vram + 4 * k), &services));
            deliver_arrivals(areas[k][2]);
            draw_sync(services);
        }
    }
    resident::heap_set_keep(resident.heap, memory(parked_vram), false); // 800320b8
    resident::heap_set_keep(resident.heap, memory(parked_vram + 4), false);
    for (const auto &[k, site] : {std::pair{0U, 0x800a74c0U}, {1U, 0x800a74d0U}}) {
        const auto block = memory(parked_vram + 4 * k);
        // A block the field loaded as a sprite resource stops being one.
        std::erase_if(state.resources, [&](const field::SpriteAllocation &resource) {
            return resource.address == block;
        });
        deliver_arrivals(site);
        static_cast<void>(release_owned_block(block, site));
    }
}

// 800a74f8: the counterpart after the movie: take the two kept blocks again
// and store the VRAM areas into them (presentation 2 decodes the party
// sprite files instead). `frame` is 800a74f8's stack frame.
void Program::movie_restore_parked(FrameServices &services, std::uint32_t frame) {
    auto &state = loaded(*this);
    if (state.single_actor_mode == 2)
        unrecovered("movie_restore_parked", 0x800a7524, "symbol:field-movie-party-reload",
                    "Reloading the party sprites after a presentation-2 movie is not reconstructed");
    deliver_arrivals(0x800a7688);
    set_memory(parked_vram, load_block(0x14000, 0, 0x800a7688));
    deliver_arrivals(0x800a76a4);
    set_memory(parked_vram + 4, load_block(0x14000, 0, 0x800a76a4));
    resident::heap_set_keep(resident.heap, memory(parked_vram), true); // 800320a4
    resident::heap_set_keep(resident.heap, memory(parked_vram + 4), true);
    const std::array<std::array<std::uint32_t, 3>, 2> areas{{
        {0, 0x800a76e8, 0x800a76f0},
        {0x80, 0x800a7710, 0x800a7718},
    }};
    for (std::uint32_t k = 0; k < 2; ++k) {
        std::array<std::int16_t, 4> rect{0x200, static_cast<std::int16_t>(areas[k][0]), 0x140, 0x80};
        const auto block = memory(parked_vram + 4 * k);
        deliver_arrivals(areas[k][1]);
        store_image(services, rect, frame + 0x10, block, owned_span(block).first(0x14000));
        deliver_arrivals(areas[k][2]);
        draw_sync(services);
    }
}

// 800a708c: open the library for a 320 x 224 movie (scale 80h, 16-pixel
// slices, a 20h-sector ring, 800h halfwords per decode call) in the
// request's color mode, with heap tag 4 while it allocates.
void Program::movie_open_display() {
    auto &state = loaded(*this);
    resident::heap_select_tag(resident.heap, 4, 0);
    set_memory(movie::split_display, state.single_actor_mode == 2 ? 1U : 0U);
    deliver_arrivals(0x800a70f4);
    static_cast<void>(movie_open(0x140, 0xe0, 0x80, 0x10, 0x20, 0x800,
                                 state.movie.request.buffer_mark));
    set_memory(start_failed, 0);
    resident::heap_select_tag(resident.heap, 8, 0);
}

// 800a7218: start the requested movie: file movie + 2 of directory 18h + 1
// from the request's sector, frames and display area, the XA audio of
// channel 1 unless the request turns it off, the frame callback 800a7120
// and at most e0h rows per slice.
void Program::movie_start_request() {
    auto &state = loaded(*this);
    state.movie.frame = 0;
    resident::heap_select_tag(resident.heap, 4, 0);
    if (memory(start_failed) == 0) {
        static_cast<void>(select_directory(0x18, 1));
        const auto &request = state.movie.request;
        std::uint32_t select = 1;
        if (static_cast<std::int16_t>(request.start_select) != 0xff ||
            (state.movie.flags & 0x40U) != 0)
            select = 3;
        MovieStart start{};
        start.file = static_cast<std::uint32_t>(static_cast<std::int16_t>(request.movie) + 2);
        start.sector = request.start_a1;
        start.first_frame = request.start_a2;
        start.last_frame = request.end_frame;
        start.channel = 1;
        start.select = select;
        start.hold = request.hold;
        start.area = {request.area[0], request.area[1], request.area[2], request.area[3]};
        start.rows = 0xe0;
        start.callback = 0x800a7120;
        deliver_arrivals(0x800a72fc);
        movie_start(start);
        static_cast<void>(select_directory(4, 0));
    }
    resident::heap_select_tag(resident.heap, 8, 0);
}

// 800a732c(count): the reset check (80019ca0), then `count` decode steps,
// each followed by the movie's sound-effect step (80085678).
void Program::movie_decode_steps(std::uint32_t count, movie::MdecCodec &codec) {
    static_cast<void>(loaded(*this));
    deliver_arrivals(0x800a733c);
    if (resident.input_queue.current[0] == 0x90c)
        unrecovered("soft_reset", 0x80019cb8, "symbol:soft-reset-80019cd0",
                    "The soft reset (80019cd0) is not recovered");
    if (memory(start_failed) != 0)
        return;
    for (std::uint32_t i = 0; s32(i) < s32(count); ++i) {
        deliver_arrivals(0x800a7360);
        movie_poll(codec); // 801d3f7c
        deliver_arrivals(0x800a7368);
        movie_sound_step(); // 80085678
    }
}

// 800a7c58 up to the first-frame wait (800a7e58). `frame` is the player's
// stack frame (its entry SP - 28h), holding the rectangle its image calls
// pass at +10.
void Program::movie_prepare(FrameServices &services, std::uint32_t frame,
                            const ProgramObserver &observe) {
    auto &state = loaded(*this);
    const auto rect_address = frame + 0x10;
    state.movie.signals = 0;
    state.movie_frame_pending = 0;
    state.movie_display = 0;
    // The library file (a9 of directory 4) into a temporary block.
    const auto temporary = load_block(file_words(movie::library_file), 0, 0x800a7c90);
    deliver_arrivals(0x800a7ca8);
    static_cast<void>(read_file(movie::library_file, temporary, 0, 0x80));
    state.movie.frame = 0;
    state.movie_overlay_active = 0;
    deliver_arrivals(0x800a7cc0);
    movie_wait_disc(services, observe);
    observed(observe, *this, "movie_library_read", 0x800a7cc8);
    static_cast<void>(select_directory(0x18, 0));
    deliver_arrivals(0x800a7cdc);
    seek_file(static_cast<std::int16_t>(state.movie.request.movie)); // 8002a2d0
    static_cast<void>(select_directory(4, 0));
    deliver_arrivals(0x800a7cf0);
    movie_wait_disc(services, observe);
    observed(observe, *this, "movie_seek", 0x800a7cf8);
    if (state.w_b2264 != 0)
        unrecovered("movie_prepare", 0x800a7d0c, "symbol:field-movie-gear-mode",
                    "Preparing a movie with 800b2264 set (801e7fd4, 800775f8) is not "
                    "reconstructed");
    stop_field_particles(services); // 800a9460
    auto library = temporary;
    if (state.single_actor_mode != 2) {
        // The library file goes through VRAM (140h, 0; c0h x 100h) so that
        // its block is free before the library's own block is taken.
        std::array<std::int16_t, 4> rect{0x140, 0, 0xc0, 0x100};
        deliver_arrivals(0x800a7d6c);
        static_cast<void>(load_image(rect, rect_address, temporary, &services));
        deliver_arrivals(0x800a7d74);
        draw_sync(services);
        deliver_arrivals(0x800a7d7c);
        static_cast<void>(release_owned_block(temporary, 0x800a7d7c));
        library = load_block(0x18000, 0, 0x800a7d8c);
        deliver_arrivals(0x800a7d9c);
        store_image(services, rect, rect_address, library, owned_span(library).first(0x18000));
    }
    std::uint32_t image = 0;
    if (resident.w_4f370 == 0) {
        // The library's own block runs from 801d3000 up to 800adb30.
        image = load_block((state.heap_limit & 0x00ffffffU) - 0x001d3008U, 1, 0x800a7dd4);
        const auto size = file_words(movie::library_file);
        const auto source = owned_span(library);
        const auto target = owned_span(image);
        if (source.size() < size || target.size() < size)
            throw field::FieldFormatError("The movie library copy overruns its blocks");
        std::copy_n(source.begin(), size, target.begin()); // 8003f968
    } else {
        image = load_block(8, 1, 0x800a7e00);
    }
    if (image != movie::library_address)
        throw field::FieldFormatError("The movie library was not placed at 801d3000");
    state.movie_library = image;
    deliver_arrivals(0x800a7e0c);
    static_cast<void>(release_owned_block(library, 0x800a7e0c));
    movie_sound_load();   // 80085788
    movie_overlay_load(); // 800acc58
    state.movie_frame_pending = 1;
    deliver_arrivals(0x800a7e2c);
    movie_release_parked(services, frame - 0x30); // 800a73e8
    draw_and_vertical_sync(services);             // 8007999c; FlushCache touches no RAM
    resident::heap_coalesce(resident.heap);
    deliver_arrivals(0x800a7e44);
    movie_open_display(); // 800a708c
    deliver_arrivals(0x800a7e4c);
    movie_start_request(); // 800a7218
    observed(observe, *this, "movie_started", 0x800a7e54);
    set_memory(0x800adb7c, 1);
    deliver_arrivals(0x800a7e5c); // Up to the first-frame wait.
}

// 800a7e5c..800a7e7c: VSync(0) and three decode steps until the first
// frame is loaded.
void Program::movie_first_frame(FrameServices &services, movie::MdecCodec &codec) {
    auto &state = loaded(*this);
    do {
        deliver_arrivals(0x800a7e5c);
        vertical_sync(services);
        deliver_arrivals(0x800a7e64);
        movie_decode_steps(3, codec);
    } while (state.movie_frame_pending != 0);
    deliver_arrivals(0x800a7e88); // Up to the loop head.
}

// One pass of the loop 800a7e88..800a80b0. `frame` is the player's stack
// frame.
field::MovieStep Program::movie_pass(FrameServices &services, movie::MdecCodec &codec,
                                     const ProgramObserver &observe) {
    auto &state = loaded(*this);
    switch (state.single_actor_mode) {
    case 1:
        unrecovered("movie_pass", 0x800a7ec4, "symbol:field-movie-presentation-1",
                    "Presentation 1 (80073f50, 80075910) is not reconstructed");
    case 0: {
        // Twice: wait for the GPU and the vertical blank, show the draw
        // block the frame callback selected, decode three steps.
        const std::array<std::array<std::uint32_t, 5>, 2> sites{{
            {0x800a7edc, 0x800a7ee4, 0x800a7ef4, 0x800a7f04, 0x800a7f0c},
            {0x800a7f14, 0x800a7f1c, 0x800a7f2c, 0x800a7f3c, 0x800a7f44},
        }};
        for (const auto &at : sites) {
            deliver_arrivals(at[0]);
            draw_sync(services);
            deliver_arrivals(at[1]);
            vertical_sync(services);
            deliver_arrivals(at[2]);
            put_disp_env(state.draw_block + 0xb8);
            deliver_arrivals(at[3]);
            // PutDrawEnv's argument is read at its call; arrivals inside
            // the call before its first queue step come after (at + 4).
            const auto draw = state.draw_block;
            deliver_arrivals(at[3] + 4);
            put_draw_env(services, draw);
            deliver_arrivals(at[4]);
            movie_decode_steps(3, codec);
        }
        movie_overlay_step(); // 800a7948
        break;
    }
    case 2:
        field_pre_frame(services);      // 80077dac
        field_frame(services, observe); // 8007554c
        movie_decode_steps(9, codec);
        break;
    default:
        break;
    }
    observed(observe, *this, "movie_pass_presented", 0x800a7f78);
    if (movie_pad() == field::MoviePad::drain) {
        deliver_arrivals(state.event_control.diagnostic_suppression == 0 ? 0x800a7fd4 : 0x800a8014);
        drain_pad(); // 80074700
    }
    const auto step = field::movie_step(state.movie, state.single_actor_mode,
                                        state.event_control.diagnostic_suppression,
                                        static_cast<std::uint16_t>(memory(0x800c3900, 2)));
    if (step == field::MovieStep::skip) {
        deliver_arrivals(0x800a8034);
        resident::set_cd_volume(resident.sound, 0, 10);
        for (int wait = 0; wait < 5; ++wait) {
            deliver_arrivals(0x800a8040);
            vertical_sync(services);
        }
    }
    // Up to the next loop head, or to the loop's end.
    deliver_arrivals(step == field::MovieStep::next_frame ? 0x800a7e88 : 0x800a80b4);
    return step;
}

// 800a80b4..800a8308. `frame` is the player's stack frame.
void Program::movie_finish(FrameServices &services, std::uint32_t frame,
                           const ProgramObserver &observe) {
    auto &state = loaded(*this);
    const auto present = [&](std::uint32_t environment, std::uint32_t display_site,
                             std::uint32_t draw_site) {
        deliver_arrivals(display_site);
        put_disp_env(environment + 0xb8);
        deliver_arrivals(draw_site);
        deliver_arrivals(draw_site + 4);
        put_draw_env(services, environment);
    };
    const auto wait = [&](std::uint32_t site) {
        deliver_arrivals(site);
        vertical_sync(services);
    };
    const auto sync = [&](std::uint32_t site) {
        deliver_arrivals(site);
        draw_sync(services);
    };
    wait(0x800a80b4);
    sync(0x800a80bc);
    deliver_arrivals(0x800a80c4);
    movie_close(); // 801d43b0
    draw_and_vertical_sync(services); // 8007999c
    present(state.draw_block, 0x800a80dc, 0x800a80ec);
    wait(0x800a80f4);
    sync(0x800a80fc);
    deliver_arrivals(0x800a8104);
    static_cast<void>(release_owned_block(state.movie_library, 0x800a8104));
    state.movie_library = 0;
    resident::heap_coalesce(resident.heap);
    movie_restore_parked(services, frame - 0x50); // 800a74f8
    if (state.w_b2264 != 0)
        unrecovered("movie_finish", 0x800778ac, "symbol:field-movie-gear-mode",
                    "Finishing a movie with 800b2264 set (80077884) is not reconstructed");
    observed(observe, *this, "movie_closed", 0x800a811c);
    // Display buffer 800adb78 holds the last frame; the move copies the
    // area onto itself.
    const auto y = static_cast<std::int16_t>(state.movie_display << 8U);
    deliver_arrivals(0x800a8148);
    move_image(services, {0, y, 0x1e0, 0xe0}, 0, y);
    sync(0x800a8150);
    wait(0x800a8158);
    state.draw_block = movie_environment;
    present(movie_environment, 0x800a8174, 0x800a8184);
    if (state.single_actor_mode != 2)
        movie_last_frame_to_15bit(services, frame - 0x38); // 800a77c4(0)
    wait(0x800a81a8);
    set_memory(draw_blocks + 0xc9, 0, 1);
    state.draw_block = draw_blocks;
    present(draw_blocks, 0x800a81cc, 0x800a81dc);
    deliver_arrivals(0x800a8204);
    move_image(services, {0, 0x100, 0x140, 0xe0}, 0, 0);
    sync(0x800a820c);
    wait(0x800a8214);
    set_memory(movie_environment + 0xc9, 0, 1);
    state.draw_block = movie_environment;
    present(movie_environment, 0x800a822c, 0x800a823c);
    state.w_adb50 = memory(0x800afe84) != 0 ? 1 : 0;
    if (state.w_b2264 != 0)
        unrecovered("movie_finish", 0x80077af4, "symbol:field-movie-gear-mode",
                    "Finishing a movie with 800b2264 set (80077ab4) is not reconstructed");
    observed(observe, *this, "movie_screen_restored", 0x800a8278);
    if (state.single_actor_mode != 2) {
        static_cast<void>(select_directory(4, 0));
        resident::heap_select_tag(resident.heap, 8, 0);
        state.reload.stream_pending = 0;
        deliver_arrivals(0x800a82a8);
        start_field_stream(); // 80070488
        deliver_arrivals(0x800a82b0);
        finish_field_stream(services); // 80070508
        set_memory(0x800adb3c, 0x20);
        state.transition = 1;
    }
    movie_sound_release(); // 80085738
    state.movie.request.start_select = 0xff;
    state.single_actor_mode = 0;
    set_memory(start_failed, 0xffffffffU);
    deliver_arrivals(0x800a8308); // The return.
}

// The whole player 800a7c58: the stages above until the loop ends.
void Program::play_movie(FrameServices &services, std::uint32_t frame, movie::MdecCodec &codec,
                         const ProgramObserver &observe) {
    movie_prepare(services, frame, observe);
    movie_first_frame(services, codec);
    while (movie_pass(services, codec, observe) == field::MovieStep::next_frame) {
    }
    movie_finish(services, frame, observe);
}

// 800a77c4(0): convert the last movie frame (24-bit, 1e0h x e0h VRAM units
// from 0, 0) to 15-bit colors at (0, 100): five columns of 60h units (40h
// pixels) through a StoreImage block and a converted block. `frame` is
// 800a77c4's stack frame.
void Program::movie_last_frame_to_15bit(FrameServices &services, std::uint32_t frame) {
    deliver_arrivals(0x800a77ec);
    const auto source = load_block(0xa800, 0, 0x800a77ec);
    deliver_arrivals(0x800a77fc);
    const auto target = load_block(0x7000, 0, 0x800a77fc);
    for (std::uint32_t column = 0; column < 5; ++column) {
        std::array<std::int16_t, 4> rect{static_cast<std::int16_t>(column * 0x60), 0, 0x60, 0xe0};
        deliver_arrivals(0x800a782c);
        store_image(services, rect, frame + 0x10, source, owned_span(source).first(0xa800));
        deliver_arrivals(0x800a7834);
        draw_sync(services);
        auto &state = loaded(*this);
        set_memory(read_cursor, source);
        set_memory(write_cursor, target);
        state.movie_bits_read = 0;
        for (std::uint32_t word = 0; word < 0x1c00; ++word) {
            // Two 15-bit pixels per word from six color bytes.
            std::uint32_t pair = 0;
            for (const auto shift : {0U, 5U, 10U, 16U, 21U, 26U})
                pair |= movie_next_component() << shift; // 800a7744
            set_memory(memory(write_cursor), pair);
            set_memory(write_cursor, memory(write_cursor) + 4);
        }
        std::array<std::int16_t, 4> area{static_cast<std::int16_t>(column << 6U), 0x100, 0x40,
                                         0xe0};
        deliver_arrivals(0x800a78ec);
        static_cast<void>(load_image(area, frame + 0x10, target, &services));
        deliver_arrivals(0x800a78f4);
        draw_sync(services);
    }
    deliver_arrivals(0x800a790c);
    static_cast<void>(release_owned_block(source, 0x800a790c));
    deliver_arrivals(0x800a7914);
    static_cast<void>(release_owned_block(target, 0x800a7914));
}

// 80085678, 80085788, 80085738: a movie with a sound-effect bank (800c3a38
// not ff) loads it and plays its effects at listed frames. No such movie is
// reconstructed.
void Program::movie_sound_step() {
    if (static_cast<std::int16_t>(loaded(*this).movie.request.start_select) != 0xff)
        unrecovered("movie_sound_step", 0x80085698, "symbol:field-movie-effects",
                    "A movie's sound-effect bank (80085788, 80085678, 80085738) is not "
                    "reconstructed");
}

// 800a7948: presentation 0 overlays field drawing on the movie between
// frames 687h and 18e2h when 8004f300 is set; otherwise nothing.
void Program::movie_overlay_step() {
    if (resident.w_4f300 != 0)
        unrecovered("movie_overlay_step", 0x800a7968, "symbol:field-movie-overlay",
                    "The field drawing over a movie (800a7948 with 8004f300 set) is not "
                    "reconstructed");
}

// 800a7744: the next 8-bit color component as a 5-bit one; a nonzero
// component never becomes zero.
std::uint32_t Program::movie_next_component() {
    auto &state = loaded(*this);
    if ((state.movie_bits_read & 3U) == 0) {
        state.movie_component_word = memory(memory(read_cursor));
        set_memory(read_cursor, memory(read_cursor) + 4);
    }
    const auto component = state.movie_component_word & 0xffU;
    ++state.movie_bits_read;
    state.movie_component_word >>= 8U;
    if (component == 0)
        return 0;
    return (component >> 3U) != 0 ? component >> 3U : 1U;
}

// 8008578c..80085878 (80085788): a movie without a sound-effect bank loads
// nothing.
void Program::movie_sound_load() { movie_sound_step(); }
void Program::movie_sound_release() { movie_sound_step(); }

// 800acc58: the field overlay drawing's setup runs only with 8004f300 set.
void Program::movie_overlay_load() {
    if (resident.w_4f300 != 0)
        unrecovered("movie_overlay_load", 0x800acc6c, "symbol:field-movie-overlay",
                    "The field drawing over a movie (800acc58 with 8004f300 set) is not "
                    "reconstructed");
}

// 80070488: start the field's own stream (the map's file 185 + 2 * map in
// directory 4) into a four-sector ring (8002a260), unless one runs.
void Program::start_field_stream() {
    auto &reload = loaded(*this).reload;
    if (reload.stream_pending != 0)
        return;
    reload.stream_pending = 1;
    reload.stream_ring = allocate_stream_ring(4, 1);
    const auto file = static_cast<std::int32_t>((resident.field_map & 0xfffU) * 2U + 0xb9U);
    static_cast<void>(read_stream(file, reload.stream_ring, 0, {}));
}

// 80070508: wait for the field stream's read, release its ring, then
// 80078c5c.
void Program::finish_field_stream(FrameServices &services) {
    auto &state = loaded(*this);
    if (state.reload.stream_pending == 1) {
        deliver_arrivals(0x80070520);
        disc_wait(0); // 80028a60
        deliver_arrivals(0x80070528);
        draw_sync(services);
        release_music_buffer(state.reload.stream_ring, 0x8007053c); // 800320e8
        state.reload.stream_pending = 0;
    }
    // 80078c5c: with 800b2344 set, brighten the 1e0h x 20h area at (0,
    // 100h) through a StoreImage block.
    if (state.control_inputs.jump_mode != 0)
        unrecovered("finish_field_stream", 0x80078c74, "symbol:field-80078c5c",
                    "Brightening the area at (0, 100h) with 800b2344 set is not reconstructed");
}

void add_movie_globals(std::vector<OriginalGlobal> &table) {
    const auto field_state = [](Program &program) -> FieldState & {
        if (!program.field)
            throw field::FieldFormatError("Original field globals require field state");
        return *program.field;
    };
    const auto add = [&](std::string name, std::uint32_t address, std::size_t width, auto access) {
        table.push_back({std::move(name), address, width, false,
                         [access, field_state](const Program &program) {
                             return static_cast<std::uint32_t>(
                                 access(field_state(const_cast<Program &>(program))));
                         },
                         [access, field_state](Program &program, std::uint32_t raw) {
                             auto &value = access(field_state(program));
                             value = static_cast<std::remove_reference_t<decltype(value)>>(raw);
                         }});
    };
    add("movie_frame_pending", 0x800b00e4, 4,
        [](FieldState &s) -> auto & { return s.movie_frame_pending; });
    add("movie_display", 0x800adb78, 4, [](FieldState &s) -> auto & { return s.movie_display; });
    add("movie_overlay_active", 0x800afe74, 4,
        [](FieldState &s) -> auto & { return s.movie_overlay_active; });
    add("heap_limit", 0x800adb30, 4, [](FieldState &s) -> auto & { return s.heap_limit; });
    add("movie_component_word", 0x800c2688, 4,
        [](FieldState &s) -> auto & { return s.movie_component_word; });
    add("movie_bits_read", 0x800b14a8, 4,
        [](FieldState &s) -> auto & { return s.movie_bits_read; });
    // Resident globals the movie library and player reach.
    const auto add_resident = [&](std::string name, std::uint32_t address, std::size_t width,
                                  auto access) {
        table.push_back({std::move(name), address, width, true,
                         [access](const Program &program) {
                             return static_cast<std::uint32_t>(
                                 access(const_cast<Program &>(program).resident));
                         },
                         [access](Program &program, std::uint32_t raw) {
                             auto &value = access(program.resident);
                             value = static_cast<std::remove_reference_t<decltype(value)>>(raw);
                         }});
    };
    add_resident("movie_overlay_enabled", 0x8004f300, 4,
                 [](ResidentState &r) -> auto & { return r.w_4f300; });
    add_resident("movie_request", 0x8004fe44, 4,
                 [](ResidentState &r) -> auto & { return r.movie_request; });
    add_resident("cd_564cc", 0x800564cc, 4, [](ResidentState &r) -> auto & { return r.cd.w_564cc; });
    add_resident("stream_header_mode", 0x8005a470, 4,
                 [](ResidentState &r) -> auto & { return r.cd.stream_header_mode; });
    add_resident("stall_frame", 0x8005a4b8, 2,
                 [](ResidentState &r) -> auto & { return r.disc_read.h_5a4b8; });
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        add_resident("party_block", 0x8005a414 + 4 * slot, 4,
                     [slot](ResidentState &r) -> auto & { return r.party_blocks[slot]; });
}

} // namespace xem::reconstruction
