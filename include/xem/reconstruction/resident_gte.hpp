#pragma once

#include "xem/reconstruction/gte.hpp"
#include "xem/reconstruction/resident_memory.hpp"

#include <cstdint>

// Resident perspective helpers of executable dc0b2dd7... that issue geometry
// coprocessor commands on the loaded rotation, translation and screen
// registers (general PS1 GTE semantics; see gte.hpp).
namespace xem::reconstruction::resident {

// 8004a73c RotTransPers4: corners 0-2 by RTPT, then corner 3 by RTPS. Stores,
// in the original order, the screen points of corners 0-2 (SXY0-2), corner
// 3's screen point (SXY2 after the RTPS), the depth cue IR0 and the OR of the
// two FLAG readings; returns SZ3 / 4. `corner(i)` supplies corner i when the
// original loads it (corner 3 after the first three stores); `store(i,
// value)` receives output i: 0-3 the screen points, 4 the depth cue, 5 the
// flag.
template <class Corner, class Store>
std::uint32_t rot_trans_pers4(Gte &gte, Corner corner, Store store) {
    for (std::uint32_t i = 0; i < 3; ++i)
        gte.set_vector(i, corner(i));
    gte.rtpt();
    for (std::uint32_t i = 0; i < 3; ++i)
        store(i, gte.sxy(i));
    const auto first_flag = gte.flag();
    gte.set_vector(0, corner(3));
    gte.rtps();
    store(3, gte.sxy(2));
    store(4, gte.data(8));
    store(5, gte.flag() | first_flag);
    return gte.data(19) >> 2U; // SZ3 is 16-bit unsigned, so SRA equals SRL.
}

// 8004a73c with the original arguments: SVECTOR addresses v0-v3 (A0-A3), then
// the stack words: the four screen-point destinations, the depth-cue and the
// flag destinations.
std::uint32_t rot_trans_pers4(Memory &memory, Gte &gte, std::uint32_t v0, std::uint32_t v1,
                              std::uint32_t v2, std::uint32_t v3, std::uint32_t sxy0,
                              std::uint32_t sxy1, std::uint32_t sxy2, std::uint32_t sxy3,
                              std::uint32_t depth, std::uint32_t flag);

// 8004a67c RotTransPers3: SVECTORs v0-v2 (A0-A2) by RTPT; stores SXY0 to
// `sxy0` (A3), then the stack words: SXY1, SXY2, the depth cue IR0 and FLAG.
// Returns SZ3 / 4.
std::uint32_t rot_trans_pers3(Memory &memory, Gte &gte, std::uint32_t v0, std::uint32_t v1,
                              std::uint32_t v2, std::uint32_t sxy0, std::uint32_t sxy1,
                              std::uint32_t sxy2, std::uint32_t depth, std::uint32_t flag);

} // namespace xem::reconstruction::resident
