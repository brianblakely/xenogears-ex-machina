// Invented bytecode and field state exercise the movie request (extended 60,
// 8008ec30), the start wait (extended 61, 8008e9f8), the movie loop's
// decisions (800a7f78..800a80b0), the map-change field exit (8007954c) and
// the movie library's slice completion (801d30c4) with a supplied MDEC
// output. They describe no original content or observation.
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/program.hpp"

#include <array>
#include <iostream>
#include <vector>

namespace game = xem::reconstruction;
namespace field = game::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

struct Fixture {
    std::vector<std::uint8_t> code = std::vector<std::uint8_t>(64);
    std::array<std::array<std::uint16_t, 32>, 1> entries{};
    field::EventVariables variables;
    field::EventControl control;
    std::array<std::uint8_t, 0x138> actor{};
    std::array<std::uint8_t, 0x5c> descriptor{};
    std::array<std::uint8_t, 0x40> sprite{};
    std::vector<field::FieldRecord> views;
    std::vector<std::uint8_t> zones = std::vector<std::uint8_t>(24);
    std::array<field::DialogueWindow, 4> dialogue{};
    std::uint16_t flag{};
    field::MathTables math;
    field::CollisionPackage collision;
    field::FieldFade fade;
    field::FieldCamera camera;
    std::int16_t inhibition{};
    std::array<std::uint8_t, 2> flags_b21d0{};
    field::GteScreen screen{};
    std::array<std::uint16_t, 24> tables{};
    field::FieldWorld world;

    Fixture()
        : world{{code, entries}, variables, control, {},       0,      0,
                {255, 255, 255}, 0,         zones,   dialogue, flag,   math,
                collision,       {},        0,       fade,     camera, inhibition,
                flags_b21d0,     screen,    tables} {
        views.push_back({actor, descriptor, {0x80100000U, sprite}});
        world.actors = views;
    }
    std::uint32_t pc() const { return actor[0xcc] | (std::uint32_t{actor[0xcd]} << 8U); }
    // FE 60 at 0 with operands movie, start, end and options (bit 15 marks
    // an immediate); the PC is on the extended byte, as the FE handler leaves it.
    void place(std::uint16_t movie, std::uint16_t start, std::uint16_t end, std::uint16_t options) {
        code[0] = 0xfe;
        code[1] = 0x60;
        std::size_t at = 2;
        for (const auto value : {movie, start, end, options}) {
            const auto immediate = static_cast<std::uint16_t>(value | 0x8000U);
            code[at++] = static_cast<std::uint8_t>(immediate);
            code[at++] = static_cast<std::uint8_t>(immediate >> 8);
        }
        actor[0xcc] = 1;
        actor[0xcd] = 0;
    }
};

void request() {
    for (const auto [options, area, single, mark] :
         {std::tuple{0x80, std::array<std::uint16_t, 4>{0x140, 0, 0x140, 0x100}, 1, 0},
          std::tuple{0x41, std::array<std::uint16_t, 4>{0, 0, 0, 0x100}, 0, 0},
          std::tuple{0x82, std::array<std::uint16_t, 4>{0, 0, 0, 0x100}, 0, 1}}) {
        Fixture f;
        f.place(13, 7, 0x116e, static_cast<std::uint16_t>(options));
        field::MovieState movie;
        movie.request.hold = 5;
        std::int32_t single_actor = 9;
        std::uint32_t requested = 0;
        field::request_movie(f.world, movie, 1, single_actor, requested);
        const auto &r = movie.request;
        check(r.movie == 13 && r.start_a1 == 7 && r.end_frame == 0x116e && r.start_a2 == 1 &&
                  r.layout == (options & 0xf) && r.size[0] == 0x140 && r.size[1] == 0x100 &&
                  r.start_select == 0xff && r.hold == 0,
              "The request stores its operands and fixed arguments");
        check(movie.flags == static_cast<std::uint32_t>(options & 0xc0) && r.area == area &&
                  single_actor == single && r.buffer_mark == mark,
              "The layout selects the area, the actor mode and the buffer mark");
        check(requested == 1 && f.control.break_requested == 1 && f.pc() == 10,
              "The request breaks the batch after its operands");
    }
    Fixture f;
    f.place(1, 2, 3, 4);
    field::MovieState movie;
    std::int32_t single_actor = 9;
    std::uint32_t requested = 0;
    field::request_movie(f.world, movie, 0, single_actor, requested);
    check(f.pc() == 0 && f.control.break_requested == 1 && requested == 0 && single_actor == 9 &&
              movie.request.movie == 0,
          "An inactive field waits on the FE prefix");
}

void start_wait() {
    Fixture f;
    f.actor[0xcc] = 5;
    std::uint32_t started = 0;
    field::wait_movie_started(f.world, started);
    check(f.pc() == 4 && f.control.break_requested == 1,
          "Before the player starts, FE 61 waits on its prefix");
    started = 1;
    f.control.break_requested = 0;
    f.actor[0xcc] = 5;
    field::wait_movie_started(f.world, started);
    check(f.pc() == 6 && started == 0 && f.control.break_requested == 1,
          "After the start FE 61 clears the flag and moves on");
}

// The library image (801d3000) and a four by two slice into buffer 80100000
// that ends its frame, with loading into VRAM disabled.
game::Program slice_program() {
    namespace movie = game::movie;
    game::Program program;
    auto &library = program.resident.heap_contents[movie::library_address];
    library.assign(movie::library_bytes, 0);
    const auto put = [&](std::uint32_t address, std::uint32_t value, std::size_t width) {
        for (std::size_t i = 0; i < width; ++i)
            library[address - movie::library_address + i] =
                static_cast<std::uint8_t>(value >> (8 * i));
    };
    put(movie::slice_rects + 4, 4, 2); // width
    put(movie::slice_rects + 6, 2, 2); // rows
    put(movie::row_limit, 0xffff, 2);
    put(movie::slice_buffers, 0x80100000, 4);
    put(movie::display_buffers + 4, 4, 2); // the frame's last column
    put(movie::loaded_frame, 7, 4);
    program.resident.heap_contents[0x80100000] = std::vector<std::uint8_t>(16, 0xee);
    return program;
}

void slice_output() {
    namespace movie = game::movie;
    auto program = slice_program();
    std::vector<std::uint8_t> output(16);
    for (std::size_t i = 0; i < output.size(); ++i)
        output[i] = static_cast<std::uint8_t>(i + 1);
    program.resident.mdec_output.push_back(output);
    program.movie_slice_decoded();
    const auto &library = program.resident.heap_contents.at(movie::library_address);
    const auto at = [&](std::uint32_t address) { return library[address - movie::library_address]; };
    check(program.resident.heap_contents.at(0x80100000) == output &&
              program.resident.mdec_output.empty(),
          "The supplied MDEC output lands in the slice buffer");
    check(at(movie::mdec_idle) == 1 && at(movie::shown_frame) == 7 &&
              at(movie::load_display) == 1 && at(movie::slice_rects) == 4,
          "The frame's last slice marks the MDEC idle and the frame shown");
    for (const auto size : {std::size_t{0}, std::size_t{12}}) {
        auto missing = slice_program();
        if (size != 0)
            missing.resident.mdec_output.emplace_back(size);
        bool refused = false;
        try {
            missing.movie_slice_decoded();
        } catch (const game::PlatformInputError &) {
            refused = true;
        }
        check(refused, "A missing or mis-sized MDEC output is refused");
    }
}

void decisions() {
    field::MovieState movie;
    movie.request.end_frame = 100;
    movie.frame = 99;
    movie.flags = 0x80;
    std::uint16_t buttons = 0;
    check(field::movie_pad(movie, 0, 1) == field::MoviePad::drain &&
              field::movie_step(movie, 0, 1, buttons) == field::MovieStep::next_frame,
          "A skippable movie drains and plays on");
    buttons = 0x20;
    check(field::movie_step(movie, 0, 1, buttons) == field::MovieStep::skip, "Cross skips it");
    movie.flags = 0;
    check(field::movie_pad(movie, 0, 1) == field::MoviePad::keep &&
              field::movie_step(movie, 0, 1, buttons) == field::MovieStep::next_frame,
          "Cross does not end an unskippable movie");
    movie.frame = 100;
    check(field::movie_step(movie, 0, 1, buttons) == field::MovieStep::end,
          "The end frame ends it");
    movie.request.hold = 1;
    check(field::movie_step(movie, 0, 1, buttons) == field::MovieStep::next_frame,
          "A hold keeps the loop running past the end frame");
    movie.request.hold = 0;
    movie.frame = 0;
    check(field::movie_pad(movie, 1, 0) == field::MoviePad::drain &&
              field::movie_step(movie, 1, 0, buttons) == field::MovieStep::end,
          "Without the marker Cross ends any movie at once");
    buttons = 0x80;
    check(field::movie_pad(movie, 2, 0) == field::MoviePad::keep &&
              field::movie_step(movie, 2, 0, buttons) == field::MovieStep::end,
          "Presentation 2 ends on its own button");
    buttons = 0;
    movie.signals = 1;
    check(field::movie_step(movie, 2, 1, buttons) == field::MovieStep::end,
          "A signal ends presentation 2");
}

struct Waits final : field::MovieServices {
    int count = 0;
    void wait_vertical_blank() override { ++count; }
};

void program_paths() {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    auto &state = *program.field;
    state.event_control.diagnostic_suppression = 1;
    state.movie.flags = 0x80;
    state.dialogue[3].bytes[0x488] = 0x20; // 800c3900
    state.movie.request.end_frame = 10;
    program.resident.sound.cd_level = 0x7fff0000;
    check(program.movie_pad() == field::MoviePad::drain, "The skippable movie drains");
    Waits waits;
    check(program.movie_decision(waits) == field::MovieStep::skip && waits.count == 5 &&
              program.resident.sound.cd_frames == 10 && program.resident.sound.cd_target == 0,
          "A skip fades the CD volume and waits five vertical blanks");

    state.exit_mode = 0x01;
    program.resident.mode_loaded = 1;
    program.resident.w_4f30c = 3;
    check(program.exit_field(3) && program.resident.next_mode == 1 &&
              program.resident.w_4f30c == 0 && program.resident.mode_loaded == 1,
          "A map change selects the field mode for the dispatcher");
    program.resident.w_4f370 = 1;
    program.resident.next_mode = 0;
    check(!program.exit_field(3) && program.resident.next_mode == 0, "8004f370 keeps the field");
    program.resident.w_4f370 = 0;
    state.exit_mode = 0x81;
    bool stopped = false;
    try {
        static_cast<void>(program.exit_field(3));
    } catch (const game::MissingDependency &error) {
        stopped = error.point.machine_address == 0x8001bb50;
    }
    check(stopped, "Exit mode bit 80 stops at 8001bb50");
}
} // namespace

int main() {
    try {
        request();
        start_wait();
        slice_output();
        decisions();
        program_paths();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
