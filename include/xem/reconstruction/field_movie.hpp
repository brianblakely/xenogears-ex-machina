#pragma once

#include "xem/reconstruction/field_script.hpp"

#include <array>
#include <cstdint>

namespace xem::reconstruction::field {

// Movie request of extended event 60 (field 8008ec30): the parameters the
// field movie player 800a7c58 passes to the movie library it loads at
// 801d3000 (init 801d3538, start 801d37cc). Roles not visible in field code
// are named after the library argument they become.
struct MovieRequest {
    std::uint16_t movie{};               // 800c3a20: operand 1; the library starts file movie + 2
    std::array<std::uint16_t, 4> area{}; // 800c3a22..800c3a28: start stack arguments 8-11
    std::uint16_t start_a1{};            // 800c3a2a: operand 3; start argument A1
    std::uint16_t start_a2{};            // 800c3a2c: 1; start argument A2
    std::uint16_t end_frame{};           // 800c3a2e: operand 5; the loop ends at this frame
    std::uint16_t layout{};              // 800c3a30: operand 7 bits 0-3
    std::array<std::uint16_t, 2> size{}; // 800c3a32, 800c3a34: 140, 100
    std::uint16_t buffer_mark{};         // 800c3a36: 1 for layout 2; init stack argument 7
    std::uint16_t start_select{};        // 800c3a38: ff; ff selects start stack argument 6 = 1
    std::uint16_t hold{}; // 800c3a3a: 0; start stack argument 7; nonzero keeps the loop running
};

// Field state the movie player's loop 800a7e88..800a80b4 decides with.
struct MovieState {
    MovieRequest request;
    std::uint32_t flags{};   // 800adb80: operand 7 bits 6-7; 80 lets Cross skip, 40 start select
    std::uint32_t signals{}; // 800adb84: counted by the event handler 800a0eb0
    // 800b06a0: the frame number the movie library last passed to the frame
    // callback 800a7120 (its A0). A recorded platform input, never computed.
    std::uint32_t frame{};
};

// Extended 60 (field 8008ec30). While the field is inactive (800adbdc zero)
// the instruction waits: the PC returns to its FE prefix. Otherwise it stores
// the request from its four 15-bit operands, sets the scheduler's single-actor
// mode (800adb74) to 1 for layout 0 and 0 otherwise, requests the movie
// (800adb70) and breaks the batch, advancing past its eight operand bytes.
void request_movie(FieldWorld &world, MovieState &movie, std::uint32_t field_active,
                   std::int32_t &single_actor_mode, std::uint32_t &movie_requested);

// Extended 61 (field 8008e9f8): wait until the requested movie has played.
// The movie player sets `started` (800adb7c) once it has started the stream;
// the field frames it runs before (800a7394) see it clear, so the
// instruction repeats there (the PC returns to its FE prefix) and completes
// after the player returns: the flag clears and the PC passes the opcode.
// The batch breaks either way.
void wait_movie_started(FieldWorld &world, std::uint32_t &started);

// One decision of the movie loop, 800a7f78..800a80b0. `presentation` is
// 800adb74 (the loop draws differently for 0, 1 and 2); `marker` is 800c268c,
// 1 when the resident word 80010000 is ffffffff (field entry 80077e88).
enum class MoviePad {
    keep,  // No drain this frame: the decision sees last frame's buttons.
    drain, // The loop calls the field pad drain 80074700 before deciding.
};
enum class MovieStep {
    next_frame,
    end,
    // Cross skipped a skippable movie: the loop fades the CD volume to zero
    // over 10 ticks (80038d18), waits five vertical blanks and ends.
    skip,
};
[[nodiscard]] MoviePad movie_pad(const MovieState &movie, std::int32_t presentation,
                                 std::int32_t marker);
// The decision once the drain movie_pad selects has run. The drain is field
// input code outside the movie player; `buttons` is the halfword 800c3900 it
// collects (bit 20 Cross; bit 80 ends presentation 2).
[[nodiscard]] MovieStep movie_step(const MovieState &movie, std::int32_t presentation,
                                   std::int32_t marker, std::uint16_t buttons);

// Platform service the movie loop waits on. Pure virtual: no default result.
class MovieServices {
  public:
    virtual ~MovieServices() = default;
    // libetc VSync(0) (resident 8004b54c): block until the next vertical
    // blank. It returns nothing the loop uses.
    virtual void wait_vertical_blank() = 0;
};

} // namespace xem::reconstruction::field
