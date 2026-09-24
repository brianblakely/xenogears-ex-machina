#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace xem::reconstruction::field {

// Resident matrix routines of executable dc0b2dd7..., reconstructed from their
// CPU arithmetic and the geometry-coprocessor commands they issue. Rotation
// entries are 4.12 fixed point; translations are 32-bit words.
struct GteMatrix {
    std::array<std::int16_t, 9> r{}; // Row-major, as the original 3x3 short array.
    std::array<std::int32_t, 3> t{};
    // Halfword after r[8] (+0x12). Rotation loads ignore it; ScaleMatrix and
    // whole-record copies write it.
    std::int16_t pad{};
    bool operator==(const GteMatrix &) const = default;
};
using GteVector = std::array<std::int16_t, 3>;
using GteLong = std::array<std::int32_t, 3>;

// MVMVA with the rotation matrix and lm=0, as these routines issue it; sf=1
// unless `shift` is false. mac is the 32-bit MAC register; ir saturates to
// signed 16 bits.
struct GteProduct {
    std::array<std::int32_t, 3> mac;
    GteVector ir;
};
[[nodiscard]] GteProduct gte_rotate(const GteMatrix &matrix, const GteVector &vector,
                                    bool translate, bool shift = true);

// The rotation and translation control registers (words 0-7) as the pinned
// interpreter stores them: R33 is sign-extended. Other GTE registers are not
// program state that survives these routines.
[[nodiscard]] std::array<std::uint32_t, 8> gte_words(const GteMatrix &loaded);
[[nodiscard]] GteMatrix gte_from_words(const std::array<std::uint32_t, 8> &words);

// 8003f738: X/Y/Z rotation from 12-bit angles through the resident sine/cosine
// table (800523f0). Returns the rotation; translation words are left untouched.
[[nodiscard]] GteMatrix rotation_matrix(const GteVector &angles,
                                        std::span<const std::uint8_t> trigonometry,
                                        GteMatrix result = {});
// 80049bdc: right = left.r * right.r; right.t unchanged; pad = sign of r[8].
void multiply_rotation(const GteMatrix &left, GteMatrix &right);
// 8004931c: left.r * right.r, left.t + (left.r * low halfwords of right.t) >> 12.
[[nodiscard]] GteMatrix compose_matrix(const GteMatrix &left, const GteMatrix &right);
// 8004a54c with the matrix loaded by 80049efc/80049f8c: IR of R * v + T.
[[nodiscard]] GteVector rotate_translate(const GteMatrix &matrix, const GteVector &vector);
// 8004a6dc RotTrans with loaded R/T: the MAC words of R * v + T.
[[nodiscard]] GteLong rot_trans(const GteMatrix &loaded, const GteVector &vector);
// 80049cec: loads matrix.r, then the MAC words of R * v.
[[nodiscard]] GteLong apply_matrix(const GteMatrix &matrix, const GteVector &vector);
// 8004947c: loads matrix.r; each word is split into a signed high part (>> 15)
// and a 15-bit low part, multiplied separately and recombined with wrapping.
[[nodiscard]] GteLong apply_matrix_lv(const GteMatrix &matrix, const GteLong &vector);
// 80049dcc: scales columns in place with 32-bit products; the last entry is
// stored as a full word, so pad receives its high half.
void scale_matrix(GteMatrix &matrix, const GteLong &scale);
// 8004a480: GTE OP with the low halfwords of a as the diagonal and of b as IR,
// sf=1. The diagonal registers are restored, so loaded R is unchanged.
[[nodiscard]] GteLong outer_product12(const GteLong &a, const GteLong &b);

// Screen registers used by perspective transforms (control 24-26).
struct GteScreen {
    std::int32_t offset_x{}; // OFX, 16.16
    std::int32_t offset_y{}; // OFY, 16.16
    std::uint16_t h{};       // H: projection plane distance
    bool operator==(const GteScreen &) const = default;
};
// GTE unsigned division H / SZ3 by table-seeded Newton-Raphson; 1ffff when
// H >= 2 * SZ3 (the divide-overflow case).
[[nodiscard]] std::uint32_t gte_divide(std::uint32_t h, std::uint32_t sz3);
// 8004a64c RotTransPers with loaded R/T: GTE RTPS (sf=1, lm=0), returning the
// saturated screen X/Y (SXY2). The perspective quotient uses the hardware's
// reciprocal-table division.
[[nodiscard]] std::array<std::int16_t, 2>
rot_trans_pers(const GteMatrix &loaded, const GteScreen &screen, const GteVector &vector);

// Resident PushMatrix/PopMatrix (8004960c/800496ac): twenty 0x20-byte records
// at 80056d30, depth in bytes at 80056d2c.
struct MatrixStack {
    std::uint32_t depth{};
    std::array<std::uint8_t, 0x280> records{};
};
void push_matrix(MatrixStack &stack, const GteMatrix &loaded);
[[nodiscard]] GteMatrix pop_matrix(MatrixStack &stack);

} // namespace xem::reconstruction::field
