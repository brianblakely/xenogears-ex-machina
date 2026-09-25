#pragma once

#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/gte.hpp"

#include <cstdint>
#include <span>

namespace xem::reconstruction::field {

// Field 80073930: turn a 12-bit angle toward a target by speed without overshoot.
[[nodiscard]] std::uint32_t turn_toward(std::int32_t current, std::int32_t target,
                                        std::int32_t speed);

// Field 80073750: a look-at matrix from eye to target with an up vector. Its
// ApplyMatrix leaves the rotation loaded in `gte`.
void build_view(Gte &gte, std::span<const std::int16_t> reciprocal, GteMatrix &m,
                const GteLong &eye, const GteLong &target, const GteLong &up);

} // namespace xem::reconstruction::field
