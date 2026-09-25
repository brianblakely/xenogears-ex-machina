// Resident perspective helpers of executable dc0b2dd7... (see
// resident_gte.hpp).
#include "xem/reconstruction/resident_gte.hpp"

#include <array>

namespace xem::reconstruction::resident {
namespace {
// LWC2 of an SVECTOR: VXY from its first word, VZ from the low half of the
// second (the pad halfword is ignored).
field::GteVector load_vector(Memory &memory, std::uint32_t address) {
    const auto xy = memory.read(address, 4);
    return {static_cast<std::int16_t>(xy), static_cast<std::int16_t>(xy >> 16U),
            static_cast<std::int16_t>(memory.read(address + 4, 4))};
}
} // namespace

// 8004a73c: RotTransPers4 on SVECTORs in memory; every output is a word store.
std::uint32_t rot_trans_pers4(Memory &memory, Gte &gte, std::uint32_t v0, std::uint32_t v1,
                              std::uint32_t v2, std::uint32_t v3, std::uint32_t sxy0,
                              std::uint32_t sxy1, std::uint32_t sxy2, std::uint32_t sxy3,
                              std::uint32_t depth, std::uint32_t flag) {
    const std::array<std::uint32_t, 4> corners{v0, v1, v2, v3};
    const std::array<std::uint32_t, 6> outputs{sxy0, sxy1, sxy2, sxy3, depth, flag};
    return rot_trans_pers4(
        gte, [&](std::uint32_t i) { return load_vector(memory, corners.at(i)); },
        [&](std::uint32_t i, std::uint32_t value) { memory.write(outputs.at(i), value, 4); });
}

// 8004a67c: RTPT on three SVECTORs; stores SXY0-2, IR0 and FLAG in that order.
std::uint32_t rot_trans_pers3(Memory &memory, Gte &gte, std::uint32_t v0, std::uint32_t v1,
                              std::uint32_t v2, std::uint32_t sxy0, std::uint32_t sxy1,
                              std::uint32_t sxy2, std::uint32_t depth, std::uint32_t flag) {
    gte.set_vector(0, load_vector(memory, v0));
    gte.set_vector(1, load_vector(memory, v1));
    gte.set_vector(2, load_vector(memory, v2));
    gte.rtpt();
    memory.write(sxy0, gte.sxy(0), 4);
    memory.write(sxy1, gte.sxy(1), 4);
    memory.write(sxy2, gte.sxy(2), 4);
    memory.write(depth, gte.data(8), 4);
    memory.write(flag, gte.flag(), 4);
    return gte.data(19) >> 2U;
}

} // namespace xem::reconstruction::resident
