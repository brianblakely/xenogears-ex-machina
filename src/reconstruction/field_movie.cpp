#include "xem/reconstruction/field_movie.hpp"

namespace xem::reconstruction::field {
namespace {

std::uint32_t pc(const FieldWorld &world) {
    const auto &actor = world.actors[world.current].actor;
    return static_cast<std::uint32_t>(actor[0xcc] | (actor[0xcd] << 8));
}
void set_pc(FieldWorld &world, std::uint32_t value) {
    auto &actor = world.actors[world.current].actor;
    actor[0xcc] = static_cast<std::uint8_t>(value);
    actor[0xcd] = static_cast<std::uint8_t>(value >> 8);
}

} // namespace

void request_movie(FieldWorld &world, MovieState &movie, std::uint32_t field_active,
                   std::int32_t &single_actor_mode, std::uint32_t &movie_requested) {
    if (field_active == 0) {
        world.control.break_requested = 1;
        set_pc(world, pc(world) - 1);
        return;
    }
    const auto operand = [&](std::uint32_t offset) {
        return static_cast<std::uint16_t>(read_immediate15_or_variable(world, offset));
    };
    auto &request = movie.request;
    request.movie = operand(1);
    request.start_a1 = operand(3);
    request.end_frame = operand(5);
    const auto options = static_cast<std::uint32_t>(operand(7));
    movie.flags = options & 0xc0U;
    request.size = {0x140, 0x100};
    request.layout = static_cast<std::uint16_t>(options & 0xfU);
    request.start_a2 = 1;
    switch (request.layout) {
    case 0:
        request.area = {0x140, 0, 0x140, 0x100};
        single_actor_mode = 1;
        request.buffer_mark = 0;
        break;
    case 1:
    case 2:
        request.area = {0, 0, 0, 0x100};
        single_actor_mode = 0;
        request.buffer_mark = request.layout == 2 ? 1 : 0;
        break;
    default: // Other layouts leave the area, the mode and the mark as they were.
        break;
    }
    request.start_select = 0xff;
    request.hold = 0;
    movie_requested = 1;
    world.control.break_requested = 1;
    set_pc(world, pc(world) + 9);
}

// 8008e9f8 (extended 61): wait on the prefix until the player has started
// the requested movie (800adb7c), then clear that flag and move on.
void wait_movie_started(FieldWorld &world, std::uint32_t &started) {
    world.control.break_requested = 1;
    if (started == 0) {
        set_pc(world, pc(world) - 1);
        return;
    }
    started = 0;
    set_pc(world, pc(world) + 1);
}

MoviePad movie_pad(const MovieState &movie, std::int32_t presentation, std::int32_t marker) {
    if (marker == 0)
        return presentation == 2 ? MoviePad::keep : MoviePad::drain;
    return (movie.flags & 0x80U) != 0 ? MoviePad::drain : MoviePad::keep;
}

MovieStep movie_step(const MovieState &movie, std::int32_t presentation, std::int32_t marker,
                     std::uint16_t buttons) {
    if (marker == 0) {
        if (presentation == 2) {
            if ((buttons & 0x80U) != 0 || movie.signals != 0)
                return MovieStep::end;
        } else if ((buttons & 0x20U) != 0) {
            return MovieStep::end;
        }
    } else if ((movie.flags & 0x80U) != 0 && (buttons & 0x20U) != 0) {
        return MovieStep::skip;
    }
    if (presentation == 2 && movie.signals != 0)
        return MovieStep::end;
    if (static_cast<std::int16_t>(movie.request.hold) != 0)
        return MovieStep::next_frame;
    return static_cast<std::int32_t>(movie.frame) < movie.request.end_frame ? MovieStep::next_frame
                                                                            : MovieStep::end;
}

} // namespace xem::reconstruction::field
