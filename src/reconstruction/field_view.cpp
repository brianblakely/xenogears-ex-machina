// Field view helpers shared by the move phase and the frame's compass.
#include "xem/reconstruction/field_view.hpp"

#include "xem/reconstruction/field_actor.hpp"

#include <bit>

namespace xem::reconstruction::field {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t add(std::int32_t a, std::int32_t b) { return s32(u32(a) + u32(b)); }
std::int32_t subtract(std::int32_t a, std::int32_t b) { return s32(u32(a) - u32(b)); }
// The high halfword of a 16.16 word, as LH of its upper half reads it.
std::int32_t high(std::int32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(u32(value) >> 16U));
}
} // namespace

std::uint32_t turn_toward(std::int32_t current, std::int32_t target, std::int32_t speed) {
    auto value = current;
    if ((u32(subtract(value, target)) & 0xfffU) < 0x800) {
        value = subtract(value, speed);
        if ((u32(subtract(value, target)) & 0xfffU) >= 0x800)
            value = target;
    } else {
        value = add(value, speed);
        if ((u32(subtract(value, target)) & 0xfffU) < 0x800)
            value = target;
    }
    return u32(value) & 0xfffU;
}

void build_view(Gte &gte, std::span<const std::int16_t> reciprocal, GteMatrix &m,
                const GteLong &eye, const GteLong &target, const GteLong &up) {
    GteLong direction{}, above{};
    for (std::size_t i = 0; i < 3; ++i) {
        direction[i] = subtract(target[i], eye[i]) >> 16;
        above[i] = up[i] >> 16;
    }
    const auto z = normalize_field_vector(direction, reciprocal);
    const auto x = normalize_field_vector(outer_product12(above, z), reciprocal);
    const auto y = normalize_field_vector(outer_product12(z, x), reciprocal);
    for (std::size_t i = 0; i < 3; ++i) {
        m.r[i] = static_cast<std::int16_t>(x[i]);
        m.r[3 + i] = static_cast<std::int16_t>(y[i]);
        m.r[6 + i] = static_cast<std::int16_t>(z[i]);
    }
    GteVector scaled{};
    for (std::size_t i = 0; i < 3; ++i)
        scaled[i] = static_cast<std::int16_t>(high(eye[i]) * 3);
    const auto moved = apply_matrix(m, scaled);
    gte.transform.r = m.r; // ApplyMatrix loads the rotation only.
    for (std::size_t i = 0; i < 3; ++i)
        m.t[i] = s32(0U - u32(moved[i]));
}

} // namespace xem::reconstruction::field
