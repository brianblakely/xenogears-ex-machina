#pragma once

#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/disc_stream.hpp"
#include "xem/reconstruction/field_control.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_movie.hpp"
#include "xem/reconstruction/field_return.hpp"
#include "xem/reconstruction/field_script.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/field_sprite_model.hpp"
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/gte.hpp"
#include "xem/reconstruction/interrupts.hpp"
#include "xem/reconstruction/menu.hpp"
#include "xem/reconstruction/menu_save.hpp"
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>


namespace xem::reconstruction {

// Resident pad-input queue (80035c0c enqueue, 80035cdc dequeue): a 16-entry ring
// of six halfwords. 80035db0 clears it along with other input halfwords whose
// readers are not recovered; those keep their original addresses as names.
struct InputQueue {
    std::uint32_t count{};    // 8005937c
    std::uint32_t write{};    // 80059380
    std::uint32_t read{};     // 80059384
    std::uint32_t overflow{}; // 80050208, set when a seventeenth entry is refused
    std::uint32_t w50200{};   // 80050200, set to 1 by the reset
    // 80059570, 80059574, 8005948c, 80059490, 800594a4, 800594a8: last dequeued entry.
    std::array<std::uint16_t, 6> current{};
    // 800594dc, 800594e0, 800594e8, 800594ec, 800595c8, 800595cc.
    std::array<std::uint16_t, 6> other{};
    // The ring: 16 entries of each field in its own array (8005a0fc + 20 *
    // field), indexed by the low 4 bits of the counters.
    std::array<std::array<std::uint16_t, 16>, 6> ring{};
    // 80035db0: clear the counters and entries; the ring keeps its contents.
    void reset() {
        const auto kept = ring;
        *this = {.w50200 = 1};
        ring = kept;
    }
    // 80035cdc: move the oldest entry into `current`; false when empty.
    bool dequeue();
};

// Per-VSync controller and clock state (VSync callback 8003634c and callees
// 800358bc, 80035e44, 80036220). The pad buffers are the BIOS pad driver's
// receive buffers: platform input, read only.
struct PadState {
    std::uint32_t vsyncs{};   // 80059488: VSync callbacks run; the play counter saves keep
    std::uint32_t hook{};     // 800501fc: optional per-VSync call
    std::uint32_t debugger{}; // 80059390
    std::uint8_t type{};      // 80059388: last controller type
    std::array<std::array<std::uint8_t, 34>, 2> buffers{}; // 800625fc, 8006261e
    std::array<std::uint32_t, 2> held{};                   // 80059374: last buttons per port
    std::array<std::uint32_t, 2> repeat_delay{};           // 8005022c: VSyncs since a new press
    // Analog bytes per port: 80059444, 8005944c, 80059430, 80059438 and
    // 80059448, 80059450, 80059434, 8005943c.
    std::array<std::array<std::uint8_t, 4>, 2> analog{};
    std::array<std::uint16_t, 8> remap_bits{};  // 800501e8
    std::array<std::uint8_t, 8> remap_index{};  // 80050238
    std::array<std::uint8_t, 16> direction_x{}; // 8005020c: by d-pad nibble
    std::array<std::uint8_t, 16> direction_y{}; // 8005021c
    std::uint8_t clock_stopped{};               // 800501f8
    // Play clock: 80059370 frames, 80059418 seconds, 80059420 minutes, 80059484 hours.
    std::array<std::uint8_t, 4> clock{};
    std::array<std::array<std::uint8_t, 8>, 2> actuators{}; // 8005a1bc
};

} // namespace xem::reconstruction
