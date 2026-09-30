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

// Original call ABI retained from the initial qualified connected entry.
// Its stack is owned once and evolves under recovered calls; it is never
// supplied again at a menu boundary. Qualification does not establish that
// every intervening callee's stale-stack writes have been recovered.
struct MenuCallState {
    std::uint32_t entry_sp{};
    std::uint32_t stack_base{};
    std::vector<std::uint8_t> stack;
    std::array<std::uint32_t, 8> saved_registers{};
    std::uint32_t frame_pointer{};
    std::uint32_t return_address{};
};

// Original caller registers used by the recovered field-frame and movie
// routes. Addresses correlate owned bytes, never native C++ stack pointers.
struct FrameCallAbi {
    std::uint32_t sp{};
    std::array<std::uint32_t, 8> saved_registers{};
    std::uint32_t return_address{};
};

} // namespace xem::reconstruction
