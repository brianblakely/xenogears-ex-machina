#include "xem/reconstruction/field_gte.hpp"

#include "xem/reconstruction/field_motion.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <stdexcept>

namespace xem::reconstruction::field {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
// MULT/MFLO: the low word of a signed product.
std::int32_t low(std::int32_t a, std::int32_t b) {
    return s32(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int16_t half(std::int32_t value) { return static_cast<std::int16_t>(value); }
} // namespace

GteProduct gte_rotate(const GteMatrix &matrix, const GteVector &vector, bool translate,
                      bool shift) {
    GteProduct result{};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int64_t sum = translate ? static_cast<std::int64_t>(matrix.t[row]) * 4096 : 0;
        for (std::size_t column = 0; column < 3; ++column)
            sum += static_cast<std::int64_t>(matrix.r[row * 3 + column]) * vector[column];
        // The 44-bit accumulator never wraps for these routines' operand ranges;
        // wider sums would need the unreconstructed overflow behavior.
        if (sum >= (std::int64_t{1} << 43) || sum < -(std::int64_t{1} << 43))
            throw std::domain_error("GTE MVMVA accumulator exceeds 44 bits");
        const auto mac = shift ? sum >> 12 : sum;
        result.mac[row] = s32(static_cast<std::uint32_t>(mac));
        result.ir[row] = static_cast<std::int16_t>(std::clamp<std::int64_t>(mac, -0x8000, 0x7fff));
    }
    return result;
}

GteMatrix rotation_matrix(const GteVector &angles, std::span<const std::uint8_t> trigonometry,
                          GteMatrix result) {
    const auto x = planar_trigonometry(trigonometry, static_cast<std::uint16_t>(angles[0]));
    const auto y = planar_trigonometry(trigonometry, static_cast<std::uint16_t>(angles[1]));
    const auto z = planar_trigonometry(trigonometry, static_cast<std::uint16_t>(angles[2]));
    const std::int32_t sx = x.sine, cx = x.cosine, sy = y.sine, cy = y.cosine;
    const std::int32_t sz = z.sine, cz = z.cosine;
    const auto a = low(cz, -sy) >> 12; // cos(z) * -sin(y)
    const auto b = low(sz, -sy) >> 12; // sin(z) * -sin(y)
    auto &r = result.r;
    r[0] = half(low(cz, cy) >> 12);
    r[1] = half(s32(0U - static_cast<std::uint32_t>(low(sz, cy))) >> 12);
    r[2] = half(sy);
    r[3] = half((low(sz, cx) >> 12) - (low(a, sx) >> 12));
    r[4] = half((low(cz, cx) >> 12) + (low(b, sx) >> 12));
    r[5] = half(s32(0U - static_cast<std::uint32_t>(low(cy, sx))) >> 12);
    r[6] = half((low(a, cx) >> 12) + (low(sz, sx) >> 12));
    r[7] = half((low(cz, sx) >> 12) - (low(b, cx) >> 12));
    r[8] = half(low(cy, cx) >> 12);
    return result;
}

std::array<std::int16_t, 9> rotation_matrix_yxz(const GteVector &angles,
                                                std::span<const std::uint8_t> trigonometry) {
    const auto at = [&](std::int16_t angle) {
        if (angle >= 0)
            return planar_trigonometry(trigonometry, static_cast<std::uint16_t>(angle));
        auto magnitude =
            planar_trigonometry(trigonometry, static_cast<std::uint32_t>(-angle) & 0xfffU);
        magnitude.sine = static_cast<std::int16_t>(-magnitude.sine);
        return magnitude;
    };
    const auto x = at(angles[0]);
    const auto y = at(angles[1]);
    const auto z = at(angles[2]);
    const std::int32_t sx = x.sine, cx = x.cosine, sy = y.sine, cy = y.cosine;
    const std::int32_t sz = z.sine, cz = z.cosine;
    const auto sysx = low(sy, sx) >> 12;
    const auto cysx = low(cy, sx) >> 12;
    return {half((low(cy, cz) >> 12) + (low(sysx, sz) >> 12)),
            half((low(sysx, cz) >> 12) - (low(cy, sz) >> 12)),
            half(low(sy, cx) >> 12),
            half(low(sz, cx) >> 12),
            half(low(cz, cx) >> 12),
            half(-sx),
            half((low(cysx, sz) >> 12) - (low(sy, cz) >> 12)),
            half((low(sy, sz) >> 12) + (low(cysx, cz) >> 12)),
            half(low(cy, cx) >> 12)};
}

std::array<std::int16_t, 9> rotation_matrix_zyx(const GteVector &angles,
                                                std::span<const std::uint8_t> trigonometry) {
    const auto at = [&](std::int16_t angle) {
        if (angle >= 0)
            return planar_trigonometry(trigonometry, static_cast<std::uint16_t>(angle));
        auto magnitude =
            planar_trigonometry(trigonometry, static_cast<std::uint32_t>(-angle) & 0xfffU);
        magnitude.sine = static_cast<std::int16_t>(-magnitude.sine);
        return magnitude;
    };
    const auto x = at(angles[0]);
    const auto y = at(angles[1]);
    const auto z = at(angles[2]);
    const std::int32_t sx = x.sine, cx = x.cosine, sy = y.sine, cy = y.cosine;
    const std::int32_t sz = z.sine, cz = z.cosine;
    const auto sxsy = low(sx, sy) >> 12;
    const auto cxsy = low(sy, cx) >> 12;
    return {half(low(cy, cz) >> 12),
            half((low(sxsy, cz) >> 12) - (low(sz, cx) >> 12)),
            half((low(cxsy, cz) >> 12) + (low(sx, sz) >> 12)),
            half(low(sz, cy) >> 12),
            half((low(sxsy, sz) >> 12) + (low(cx, cz) >> 12)),
            half((low(cxsy, sz) >> 12) - (low(sx, cz) >> 12)),
            half(-sy),
            half(low(sx, cy) >> 12),
            half(low(cx, cy) >> 12)};
}

void multiply_rotation(const GteMatrix &left, GteMatrix &right) {
    std::array<GteVector, 3> columns{};
    for (std::size_t column = 0; column < 3; ++column)
        columns[column] =
            gte_rotate(left, {right.r[column], right.r[3 + column], right.r[6 + column]}, false).ir;
    for (std::size_t column = 0; column < 3; ++column)
        for (std::size_t row = 0; row < 3; ++row)
            right.r[row * 3 + column] = columns[column][row];
    // The last entry is stored with SWC2 of IR3: a sign-extended full word.
    right.pad = right.r[8] < 0 ? -1 : 0;
}

GteMatrix compose_matrix(const GteMatrix &left, const GteMatrix &right) {
    auto result = right;
    multiply_rotation(left, result);
    // The translation vector loads only the low halfword of each word.
    const auto moved =
        gte_rotate(left, {half(right.t[0]), half(right.t[1]), half(right.t[2])}, false);
    for (std::size_t i = 0; i < 3; ++i) {
        // The original uses a trapping ADD; overflow would raise an exception.
        const auto sum = static_cast<std::int64_t>(moved.mac[i]) + left.t[i];
        if (sum > INT32_MAX || sum < INT32_MIN)
            throw std::domain_error("CompMatrix translation ADD overflow traps");
        result.t[i] = static_cast<std::int32_t>(sum);
    }
    return result;
}

GteVector rotate_translate(const GteMatrix &matrix, const GteVector &vector) {
    return gte_rotate(matrix, vector, true).ir;
}

std::array<std::uint32_t, 8> gte_words(const GteMatrix &loaded) {
    const auto pair = [&](std::size_t low, std::size_t high) {
        return static_cast<std::uint16_t>(loaded.r[low]) |
               static_cast<std::uint32_t>(static_cast<std::uint16_t>(loaded.r[high])) << 16U;
    };
    return {pair(0, 1),
            pair(2, 3),
            pair(4, 5),
            pair(6, 7),
            static_cast<std::uint32_t>(static_cast<std::int32_t>(loaded.r[8])),
            static_cast<std::uint32_t>(loaded.t[0]),
            static_cast<std::uint32_t>(loaded.t[1]),
            static_cast<std::uint32_t>(loaded.t[2])};
}

GteMatrix gte_from_words(const std::array<std::uint32_t, 8> &words) {
    GteMatrix result{};
    for (std::size_t i = 0; i < 4; ++i) {
        result.r[i * 2] = half(s32(words[i]));
        result.r[i * 2 + 1] = half(s32(words[i] >> 16U));
    }
    result.r[8] = half(s32(words[4])); // CTC2 keeps R33's low halfword, sign-extended.
    for (std::size_t i = 0; i < 3; ++i)
        result.t[i] = s32(words[5 + i]);
    return result;
}

GteLong rot_trans(const GteMatrix &loaded, const GteVector &vector) {
    return gte_rotate(loaded, vector, true).mac;
}

GteLong apply_matrix(const GteMatrix &matrix, const GteVector &vector) {
    return gte_rotate(matrix, vector, false).mac;
}

GteLong apply_matrix_lv(const GteMatrix &matrix, const GteLong &vector) {
    GteVector high{}, low{};
    for (std::size_t i = 0; i < 3; ++i) {
        // Negative inputs are split by magnitude, so both parts keep the sign.
        const auto value = vector[i];
        const auto magnitude =
            value < 0 ? 0U - static_cast<std::uint32_t>(value) : static_cast<std::uint32_t>(value);
        auto upper = s32(magnitude) >> 15;
        auto lower = s32(magnitude & 0x7fffU);
        if (value < 0) {
            upper = s32(0U - static_cast<std::uint32_t>(upper));
            lower = -lower;
        }
        // MTC2 to IR keeps the low halfword.
        high[i] = half(upper);
        low[i] = half(lower);
    }
    const auto upper = gte_rotate(matrix, high, false, false).mac;
    const auto lower = gte_rotate(matrix, low, false).mac;
    GteLong result{};
    for (std::size_t i = 0; i < 3; ++i)
        result[i] = s32(static_cast<std::uint32_t>(lower[i]) +
                        (static_cast<std::uint32_t>(upper[i]) << 3U));
    return result;
}

void scale_matrix(GteMatrix &matrix, const GteLong &scale) {
    for (std::size_t i = 0; i < 9; ++i) {
        const auto value = low(matrix.r[i], scale[i % 3]) >> 12;
        matrix.r[i] = half(value);
        if (i == 8)
            matrix.pad = half(value >> 16);
    }
}

GteLong outer_product12(const GteLong &a, const GteLong &b) {
    GteLong result{};
    const auto x = [](const GteLong &v, std::size_t i) {
        return static_cast<std::int64_t>(half(v[i]));
    };
    for (std::size_t i = 0; i < 3; ++i) {
        const auto j = (i + 1) % 3, k = (i + 2) % 3;
        result[i] = s32(static_cast<std::uint32_t>((x(a, j) * x(b, k) - x(a, k) * x(b, j)) >> 12));
    }
    return result;
}

std::uint32_t gte_divide(std::uint32_t h, std::uint32_t sz3) {
    if (h >= sz3 * 2)
        return 0x1ffff;
    const auto shift =
        static_cast<std::uint32_t>(std::countl_zero(static_cast<std::uint16_t>(sz3)));
    const std::uint64_t n = static_cast<std::uint64_t>(h) << shift;
    std::uint64_t d = static_cast<std::uint64_t>(sz3) << shift;
    const auto index = static_cast<std::uint32_t>((d - 0x7fc0) >> 7);
    const auto table = std::max<std::int64_t>(
        0, (0x40000 / (static_cast<std::int64_t>(index) + 0x100) + 1) / 2 - 0x101);
    const auto u = static_cast<std::uint64_t>(table) + 0x101;
    d = ((0x2000080 - d * u) & 0xffffffffffffULL) >> 8;
    d = ((0x0000080 + d * u) & 0xffffffffffffULL) >> 8;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(0x1ffff, (n * d + 0x8000) >> 16));
}

std::array<std::int16_t, 2> rot_trans_pers(const GteMatrix &loaded, const GteScreen &screen,
                                           const GteVector &vector) {
    const auto product = gte_rotate(loaded, vector, true);
    const auto sz3 =
        static_cast<std::uint32_t>(std::clamp<std::int32_t>(product.mac[2], 0, 0xffff));
    const auto quotient = static_cast<std::int64_t>(gte_divide(screen.h, sz3));
    const auto project = [&](std::int32_t offset, std::int16_t ir) {
        const auto value = (static_cast<std::int64_t>(offset) + ir * quotient) >> 16;
        return static_cast<std::int16_t>(std::clamp<std::int64_t>(value, -0x400, 0x3ff));
    };
    return {project(screen.offset_x, product.ir[0]), project(screen.offset_y, product.ir[1])};
}

void push_matrix(MatrixStack &stack, const GteMatrix &loaded) {
    // The full-stack path prints a diagnostic and pushes nothing.
    if (stack.depth > 0x27f)
        throw std::domain_error("PushMatrix stack overflow path is not reconstructed");
    const auto words = gte_words(loaded);
    for (std::size_t i = 0; i < words.size(); ++i)
        for (std::size_t byte = 0; byte < 4; ++byte)
            stack.records.at(stack.depth + i * 4 + byte) =
                static_cast<std::uint8_t>(words[i] >> (byte * 8U));
    stack.depth += 0x20;
}

GteMatrix pop_matrix(MatrixStack &stack) {
    if (stack.depth < 1 || stack.depth > stack.records.size())
        throw std::domain_error("PopMatrix empty-stack path is not reconstructed");
    stack.depth -= 0x20;
    std::array<std::uint32_t, 8> words{};
    for (std::size_t i = 0; i < words.size(); ++i)
        for (std::size_t byte = 0; byte < 4; ++byte)
            words[i] |= static_cast<std::uint32_t>(stack.records.at(stack.depth + i * 4 + byte))
                        << (byte * 8U);
    return gte_from_words(words);
}

} // namespace xem::reconstruction::field
