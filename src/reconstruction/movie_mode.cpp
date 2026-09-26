// Mode 6, the movie mode: the overlay at 8006faf0 plays one movie of the
// movie directory with the movie library (801d3000), then selects the next
// mode. At boot it shows the opening logo movie. The source is
// project-authored from the decoded mode 6 overlay and the resident
// executable; request bytes 8004fe44..47 name the movie (44: 7f the
// kind, 80 a flag; 45: the movie; 46: the next mode; 47: 1 keeps Circle
// and Start from ending it).
//
// Positions: each call site in the overlay is a delivery point for the
// interrupt arrivals recorded before it (deliver_arrivals).
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_heap.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int32_t s16(std::uint32_t value) { return static_cast<std::int16_t>(value); }
[[noreturn]] void unrecovered(std::string_view operation, std::uint32_t address, const char *id,
                              const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}

// The overlay's statics.
constexpr std::uint32_t repeat_count = 0x80076f04; // frames a held button repeated
constexpr std::uint32_t repeat_delay = 0x80076f08; // 5a, then 1 while held
constexpr std::uint32_t shown_frame = 0x80077010;  // the frame callback's last frame
constexpr std::uint32_t ending = 0x80077014;       // 1: stop; 2..5: frames until then
constexpr std::uint32_t draw_index = 0x80077018;   // environment pair the frame went to
constexpr std::uint32_t display_index = 0x8007701c;
constexpr std::uint32_t no_decode = 0x80077020;
constexpr std::uint32_t display_y = 0x80077024;    // the first display buffer's y
constexpr std::uint32_t keep_playing = 0x80077028; // 8004fe47
constexpr std::uint32_t movie_index = 0x8007711c;  // 8004fe45
constexpr std::uint32_t current_env = 0x80077120;
// Two environment pairs, 138h apart: DRAWENV (5ch) then DISPENV.
constexpr std::uint32_t environments = 0x80077124;
constexpr std::uint32_t environment_bytes = 0x138;
constexpr std::uint32_t channel = 0x80077398;
constexpr std::uint32_t last_frame = 0x8007739c;
constexpr std::uint32_t rows = 0x800773a0;
constexpr std::uint32_t first_frame = 0x800773a4;
constexpr std::uint32_t start_sector = 0x800773a8;
constexpr std::uint32_t buttons = 0x800773ac;
constexpr std::uint32_t kind_0 = 0x800773b0;
constexpr std::uint32_t previous_buttons = 0x800773b4;
constexpr std::uint32_t vsync_table = 0x800773b8; // pairs of VSync(1) around each decode
constexpr std::uint32_t split = 0x80077438;
constexpr std::uint32_t mode_43c = 0x8007743c;
constexpr std::uint32_t request_seen = 0x80077440;
constexpr std::uint32_t mode_444 = 0x80077444;
constexpr std::uint32_t kind = 0x80077448; // 8004fe44 & 7f
constexpr std::uint32_t mode_44c = 0x8007744c;
constexpr std::uint32_t host = 0x80077450;
constexpr std::uint32_t color = 0x80077454; // the library's output mode
constexpr std::uint32_t clear_rect = 0x800704e0;
constexpr std::uint32_t request = 0x8004fe44;
constexpr std::uint32_t frame_callback = 0x800768d8;
} // namespace

// 800737ec: load the library, set up the display, play the request's movie
// (800763bc), release the library and select the next mode.
void Program::movie_mode(FrameServices &services, movie::MdecCodec &codec, std::uint32_t frame,
                         const ProgramObserver &observe) {
    deliver_arrivals(0x80073834);
    resident::heap_select_tag(resident.heap, 4, 0);
    deliver_arrivals(0x80073840);
    static_cast<void>(select_directory(0x18, 0));
    deliver_arrivals(0x8007384c);
    resident::set_cd_volume(resident.sound, 0, 0);
    deliver_arrivals(0x80073854);
    draw_sync(services);
    deliver_arrivals(0x8007385c);
    vertical_sync(services);
    deliver_arrivals(0x80073864);
    set_disp_mask(0);
    // A four-byte block from the top bounds the library's: the rest of the
    // heap below it from 801d3000.
    deliver_arrivals(0x80073870);
    const auto top = load_block(4, 1, 0x80073870);
    deliver_arrivals(0x80073894);
    const auto library = load_block((top & 0xffffffU) + 0xffe2cff8U, 1, 0x80073894);
    deliver_arrivals(0x800738a0);
    static_cast<void>(release_owned_block(top, 0x800738a0));
    deliver_arrivals(0x800738b4);
    static_cast<void>(read_file(1, library, 0, 0));
    deliver_arrivals(0x800738bc);
    disc_wait(0);
    deliver_arrivals(0x800738e8);
    static_cast<void>(movie_open(0x140, 0x100, 0x80, 0x10, 0x20, 0x800, 3));
    deliver_arrivals(0x800738f0);
    set_memory(host, resident.disc_stream.host_file_table != 0 ? 1 : 0); // 8002c3d8
    set_memory(rows, 0xffffffffU);
    set_memory(last_frame, 0xc80);
    set_memory(0x80077394, 0);
    set_memory(kind_0, 0);
    set_memory(color, 1);
    set_memory(kind, 1);
    set_memory(movie_index, 0);
    set_memory(first_frame, 1);
    set_memory(channel, 1);
    set_memory(mode_444, 2);
    set_memory(mode_43c, 0);
    set_memory(start_sector, 0);
    set_memory(split, 0);
    const auto draw0 = environments;
    const auto disp0 = environments + 0x5c;
    const auto draw1 = environments + environment_bytes;
    const auto disp1 = environments + environment_bytes + 0x5c;
    deliver_arrivals(0x80073990);
    set_default_draw_environment(draw0, 0, 0, 0x140, 0xf0);
    deliver_arrivals(0x800739a8);
    set_default_display_environment(disp0, 0, 0xf0, 0x140, 0xf0);
    deliver_arrivals(0x800739c0);
    set_default_draw_environment(draw1, 0, 0xf0, 0x140, 0xf0);
    deliver_arrivals(0x800739d8);
    set_default_display_environment(disp1, 0, 0, 0x140, 0xf0);
    for (const auto e : {draw0, draw1}) {
        set_memory(e + 0x16, 1, 1); // dithering
        set_memory(e + 0x18, 1, 1); // background clear, black
        for (const auto offset : {0x19U, 0x1aU, 0x1bU})
            set_memory(e + offset, 0, 1);
    }
    for (const auto d : {disp0, disp1}) {
        set_memory(d + 0x10, 0, 1); // no interlace
        for (const auto [offset, value] :
             {std::pair{8U, 0U}, {0xaU, 0xaU}, {0xcU, 0x100U}, {0xeU, 0xd8U}})
            set_memory(d + offset, value, 2); // the screen range
    }
    set_memory(current_env, draw0);
    set_memory(mode_44c, 0);
    deliver_arrivals(0x80073aa4);
    deliver_arrivals(0x80073aa8);
    put_draw_env(services, draw0);
    deliver_arrivals(0x80073ab4);
    put_disp_env(memory(current_env) + 0x5c);
    if (memory(host) != 0)
        unrecovered("movie_mode", 0x80073acc, "symbol:movie-mode-host",
                    "The movie mode's host-file path is not reconstructed");
    set_memory(buttons, 0);
    const auto requested = (memory(request) & 0xffU);
    if (requested == 0xff || (memory(buttons) & 0x100U) != 0)
        unrecovered("movie_mode", 0x80073bb4, "symbol:movie-mode-menu",
                    "The movie mode without a request (its selection screen) is not reconstructed");
    set_memory(request_seen, 0);
    set_memory(channel, 1);
    set_memory(first_frame, 1);
    set_memory(kind, requested & 0x7fU);
    set_memory(movie_index, ((memory(request) >> (8U * 1U)) & 0xffU));
    if ((requested & 0x80U) != 0)
        unrecovered("movie_mode", 0x80073b6c, "symbol:movie-mode-request-flag",
                    "A movie request with bit 80 is not reconstructed");
    set_memory(last_frame, 0xe9);
    deliver_arrivals(0x80073b84);
    movie_mode_play(services, codec, ((memory(request) >> (8U * 3U)) & 0xffU), frame, observe);
    deliver_arrivals(0x80073b8c);
    movie_close();
    deliver_arrivals(0x80073b94);
    static_cast<void>(release_owned_block(library, 0x80073b94));
    deliver_arrivals(0x80073ba4);
    set_next_mode(((memory(request) >> (8U * 2U)) & 0xffU)); // 8001996c
    // 80019acc(0), the mode dispatcher, follows.
    deliver_arrivals(0x80073bac);
}

// 800763bc(keep): play the requested movie of kind 1 when the directory
// holds it.
void Program::movie_mode_play(FrameServices &services, movie::MdecCodec &codec,
                              std::uint32_t select, std::uint32_t frame,
                              const ProgramObserver &observe) {
    set_memory(keep_playing, select & 0xffU);
    set_memory(buttons, 0xffffffffU);
    const auto k = memory(kind);
    if (k != 1)
        unrecovered("movie_mode_play", 0x800763e8, "symbol:movie-mode-kind",
                    "Movie kinds other than 1 are not reconstructed");
    deliver_arrivals(0x8007640c);
    static_cast<void>(select_directory(0x18, 1));
    deliver_arrivals(0x80076418);
    const auto count = file_count(1); // 80028928
    if (!(s32(memory(movie_index)) < s16(static_cast<std::uint32_t>(count))))
        return;
    for (const auto e : {environments, environments + environment_bytes}) {
        set_memory(e + 0x18, 0, 1);        // no background clear
        set_memory(e + 0x5c + 0x11, 1, 1); // 24-bit display
    }
    deliver_arrivals(0x80076470);
    movie_mode_run(services, codec, frame, observe);
}

// 80076488: clear the screen, reopen the library, stream the movie and
// present each frame until the frame callback or a button ends it.
void Program::movie_mode_run(FrameServices &services, movie::MdecCodec &codec, std::uint32_t frame,
                             const ProgramObserver &observe) {
    auto &memory_state = movie_mode_memory.value();
    memory_state.frame = {frame, std::vector<std::uint8_t>(0x308)};
    const auto rect = frame + 0xc8;
    for (std::uint32_t i = 0; i < 8; i += 4)
        set_memory(rect + i, memory(clear_rect + i));
    set_memory(ending, 0);
    set_memory(no_decode, 0);
    if (memory(kind) == 0)
        unrecovered("movie_mode_run", 0x800764fc, "symbol:movie-mode-kind",
                    "Movie kind 0 is not reconstructed");
    const auto file = memory(movie_index) + 2;
    deliver_arrivals(0x80076520);
    if (file_size(static_cast<std::int32_t>(file)) == 0x18)
        unrecovered("movie_mode_run", 0x80076534, "symbol:movie-mode-short-file",
                    "A 18h-byte movie entry is not reconstructed");
    deliver_arrivals(0x8007654c);
    vertical_sync(services);
    deliver_arrivals(0x80076560);
    clear_image(services, rect, 0);
    deliver_arrivals(0x80076568);
    draw_sync(services);
    deliver_arrivals(0x80076570);
    vertical_sync(services);
    set_memory(draw_index, 0);
    set_memory(display_index, 0);
    deliver_arrivals(0x80076588);
    movie_close();
    if (s32(memory(rows)) > 0)
        unrecovered("movie_mode_run", 0x800765a4, "symbol:movie-mode-rows",
                    "A row limit for the movie mode is not reconstructed");
    set_memory(display_y, 0);
    deliver_arrivals(0x800765f4);
    static_cast<void>(movie_open(0x140, 0xf0, 0x80, 0x10, 0x20, 0x800, memory(color, 2)));
    set_memory(movie::split_display, 0);
    if (memory(split) != 0)
        unrecovered("movie_mode_run", 0x80076614, "symbol:movie-mode-split",
                    "The split display of the movie mode is not reconstructed");
    MovieStart start{};
    start.file = file;
    start.sector = memory(start_sector);
    start.first_frame = memory(first_frame, 2);
    start.last_frame = memory(last_frame, 2);
    start.channel = memory(channel, 2);
    start.select = 1;
    start.hold = 0;
    const auto y = memory(display_y, 2);
    start.area = {0, y, 0, (y + 0xf0) & 0xffffU};
    start.rows = static_cast<std::uint32_t>(s16(memory(rows, 2)));
    start.callback = frame_callback;
    deliver_arrivals(0x80076698);
    movie_start(start);
    std::uint32_t budget = 30; // tenths of a decode step per frame
    const auto pair = [&](std::uint32_t index) { return environments + environment_bytes * index; };
    // Each argument is read at its call, after the arrivals before it; those
    // inside PutDrawEnv before its queue step come at the delay slot.
    deliver_arrivals(0x800766d0);
    const auto draw = pair(memory(draw_index));
    deliver_arrivals(0x800766d4);
    put_draw_env(services, draw);
    deliver_arrivals(0x800766fc);
    put_disp_env(pair(memory(draw_index)) + 0x5c);
    deliver_arrivals(0x80076704);
    set_disp_mask(1);
    for (;;) {
        // 8007670c: the loop head.
        deliver_arrivals(0x8007670c);
        if (observe)
            observe(*this, {"movie_mode_head", 0x8007670c, {}, {}}, true);
        if (memory(no_decode) == 0) {
            const auto steps = budget / 10;
            for (std::uint32_t i = 0, slot = 1; i < steps; ++i, slot += 2) {
                deliver_arrivals(0x80076750);
                const auto before =
                    take_service(services.hblank_counts, "VSync(1) before a decode");
                deliver_arrivals(0x80076758);
                movie_poll(codec);
                deliver_arrivals(0x80076760);
                const auto after = take_service(services.hblank_counts, "VSync(1) after a decode");
                if (slot < 32) {
                    set_memory(vsync_table + 8 * i, before);
                    set_memory(vsync_table + 8 * i + 4, after);
                }
            }
            budget %= 10;
        }
        budget += 30;
        deliver_arrivals(0x800767b8);
        movie_mode_pad();
        deliver_arrivals(0x800767c0);
        if (resident.input_queue.current[0] == 0x90c)
            unrecovered("soft_reset", 0x80019cb8, "symbol:soft-reset-80019cd0",
                        "The soft reset (80019cd0) is not recovered");
        deliver_arrivals(0x800767c8);
        vertical_sync(services);
        deliver_arrivals(0x800767f0);
        put_disp_env(pair(memory(display_index)) + 0x5c);
        set_memory(display_index, memory(draw_index));
        const auto count = memory(ending);
        if (count == 1)
            break;
        if (s32(count) >= 2)
            set_memory(ending, count - 1);
    }
    deliver_arrivals(0x80076834);
    movie_stop();
    if (memory(display_index) == 0)
        unrecovered("movie_mode_run", 0x80076850, "symbol:movie-mode-last-buffer",
                    "Ending on the first display buffer is not reconstructed");
    memory_state.frame = {};
}

// 800768d8(frame, x, y), the library's frame callback: the display pair
// the frame went to; the last frame ends the movie.
void Program::movie_mode_frame_ready(std::uint32_t frame, std::uint32_t y) {
    set_memory(shown_frame, frame & 0xffffU);
    if (memory(movie::split_display) == 1)
        unrecovered("movie_mode_frame_ready", 0x800768f8, "symbol:movie-mode-split",
                    "The split display of the movie mode is not reconstructed");
    const auto index = (y & 0xffffU) == memory(display_y) ? 1U : 0U;
    set_memory(draw_index, index);
    set_memory(display_index, index);
    if (s32(frame & 0xffffU) < s32(memory(last_frame)))
        return;
    if (memory(split) == 0)
        set_memory(ending, 1);
}

// 800769a4: controller port 0 with a repeat after 5ah frames held; Circle
// (40) or Start (800) pressed ends the movie in five frames, fading the CD
// volume, unless 8004fe47 keeps it.
void Program::movie_mode_pad() {
    const auto previous = memory(buttons);
    set_memory(previous_buttons, previous);
    deliver_arrivals(0x800769bc);
    const auto now = pad_buttons(0);
    set_memory(buttons, now);
    auto reset_repeat = [&](std::uint32_t delay) {
        set_memory(repeat_delay, delay);
        set_memory(repeat_count, 0);
    };
    if (previous != now || previous == 0) {
        reset_repeat(0x5a);
    } else {
        const auto count = memory(repeat_count) + 1;
        set_memory(repeat_count, count);
        if (s32(memory(repeat_delay)) < s32(count)) {
            set_memory(previous_buttons, 0);
            reset_repeat(1);
        }
    }
    for (const auto button : {0x40U, 0x800U}) {
        if ((memory(previous_buttons) & button) != 0 || (memory(buttons) & button) == 0 ||
            memory(keep_playing) != 0)
            continue;
        deliver_arrivals(0x80076acc);
        resident::set_cd_volume(resident.sound, 0, 10);
        set_memory(ending, 5);
    }
}

} // namespace xem::reconstruction
