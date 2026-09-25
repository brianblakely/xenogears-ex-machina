// Geometry coprocessor model from general PS1 hardware documentation: register
// conversions, 44-bit MAC overflow checks, saturation flags and the commands.
#include "xem/reconstruction/gte.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace xem::reconstruction {
namespace {
constexpr std::int64_t mac_limit = std::int64_t{1} << 43;
std::int16_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t pack(std::int16_t low, std::int16_t high) {
    return static_cast<std::uint16_t>(low) |
           (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16U);
}
std::uint32_t extend(std::int16_t value) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
}
// FLAG bits.
constexpr std::uint32_t flag_mac_positive = 1U << 30; // MAC1; MAC2 29, MAC3 28
constexpr std::uint32_t flag_mac_negative = 1U << 27; // MAC1; MAC2 26, MAC3 25
constexpr std::uint32_t flag_ir = 1U << 24;           // IR1; IR2 23, IR3 22
constexpr std::uint32_t flag_color = 1U << 21;        // R; G 20, B 19
constexpr std::uint32_t flag_otz = 1U << 18;
constexpr std::uint32_t flag_divide = 1U << 17;
constexpr std::uint32_t flag_mac0_positive = 1U << 16;
constexpr std::uint32_t flag_mac0_negative = 1U << 15;
constexpr std::uint32_t flag_sx = 1U << 14;
constexpr std::uint32_t flag_sy = 1U << 13;
constexpr std::uint32_t flag_ir0 = 1U << 12;
constexpr std::uint32_t flag_error_bits = 0x7f87e000U;
} // namespace

std::uint32_t Gte::sxy(std::size_t index) const { return pack(sx_[index], sy_[index]); }

std::uint32_t Gte::data(std::uint32_t index) const {
    switch (index) {
    case 0:
    case 2:
    case 4:
        return pack(v_[index / 2][0], v_[index / 2][1]);
    case 1:
    case 3:
    case 5:
        return extend(v_[index / 2][2]);
    case 6:
        return rgbc_;
    case 7:
        return otz_;
    case 8:
    case 9:
    case 10:
    case 11:
        return extend(ir_[index - 8]);
    case 12:
    case 13:
    case 14:
        return sxy(index - 12);
    case 15:
        return sxy(2);
    case 16:
    case 17:
    case 18:
    case 19:
        return sz_[index - 16];
    case 20:
    case 21:
    case 22:
        return rgb_[index - 20];
    case 23:
        return res1_;
    case 24:
    case 25:
    case 26:
    case 27:
        return static_cast<std::uint32_t>(mac_[index - 24]);
    case 28:
    case 29: {
        const auto component = [&](std::size_t i) {
            return static_cast<std::uint32_t>(std::clamp(ir_[i] >> 7, 0, 0x1f));
        };
        return component(1) | (component(2) << 5U) | (component(3) << 10U);
    }
    case 30:
        return lzcs_;
    case 31:
        return lzcr_;
    default:
        throw std::out_of_range("GTE data register index");
    }
}

void Gte::set_data(std::uint32_t index, std::uint32_t value) {
    switch (index) {
    case 0:
    case 2:
    case 4:
        v_[index / 2][0] = s16(value);
        v_[index / 2][1] = s16(value >> 16U);
        return;
    case 1:
    case 3:
    case 5:
        v_[index / 2][2] = s16(value);
        return;
    case 6:
        rgbc_ = value;
        return;
    case 7:
        otz_ = static_cast<std::uint16_t>(value);
        return;
    case 8:
    case 9:
    case 10:
    case 11:
        ir_[index - 8] = s16(value);
        return;
    case 12:
    case 13:
    case 14:
        sx_[index - 12] = s16(value);
        sy_[index - 12] = s16(value >> 16U);
        return;
    case 15: // SXYP pushes the screen FIFO.
        sx_ = {sx_[1], sx_[2], s16(value)};
        sy_ = {sy_[1], sy_[2], s16(value >> 16U)};
        return;
    case 16:
    case 17:
    case 18:
    case 19:
        sz_[index - 16] = static_cast<std::uint16_t>(value);
        return;
    case 20:
    case 21:
    case 22:
        rgb_[index - 20] = value;
        return;
    case 23:
        res1_ = value;
        return;
    case 24:
    case 25:
    case 26:
    case 27:
        mac_[index - 24] = static_cast<std::int32_t>(value);
        return;
    case 28:
        ir_[1] = static_cast<std::int16_t>((value & 0x1fU) << 7U);
        ir_[2] = static_cast<std::int16_t>(((value >> 5U) & 0x1fU) << 7U);
        ir_[3] = static_cast<std::int16_t>(((value >> 10U) & 0x1fU) << 7U);
        return;
    case 29:
        return; // ORGB is read-only.
    case 30:
        lzcs_ = value;
        lzcr_ = static_cast<std::uint32_t>(static_cast<std::int32_t>(value) < 0
                                               ? std::countl_one(value)
                                               : std::countl_zero(value));
        return;
    case 31:
        return; // LZCR is read-only.
    default:
        throw std::out_of_range("GTE data register index");
    }
}

std::uint32_t Gte::control(std::uint32_t index) const {
    const auto matrix_word = [](const std::array<std::int16_t, 9> &m, std::uint32_t word) {
        return word == 4 ? extend(m[8]) : pack(m[word * 2], m[word * 2 + 1]);
    };
    if (index < 5)
        return matrix_word(transform.r, index);
    if (index < 8)
        return static_cast<std::uint32_t>(transform.t[index - 5]);
    if (index < 13)
        return matrix_word(light_, index - 8);
    if (index < 16)
        return static_cast<std::uint32_t>(back_[index - 13]);
    if (index < 21)
        return matrix_word(color_, index - 16);
    if (index < 24)
        return static_cast<std::uint32_t>(far_[index - 21]);
    switch (index) {
    case 24:
        return static_cast<std::uint32_t>(screen.offset_x);
    case 25:
        return static_cast<std::uint32_t>(screen.offset_y);
    case 26: // H reads sign-extended.
        return extend(static_cast<std::int16_t>(screen.h));
    case 27:
        return extend(dqa_);
    case 28:
        return static_cast<std::uint32_t>(dqb_);
    case 29:
        return extend(zsf3_);
    case 30:
        return extend(zsf4_);
    case 31:
        return flag_;
    default:
        throw std::out_of_range("GTE control register index");
    }
}

void Gte::set_control(std::uint32_t index, std::uint32_t value) {
    const auto matrix_word = [&](std::array<std::int16_t, 9> &m, std::uint32_t word) {
        if (word == 4) {
            m[8] = s16(value);
        } else {
            m[word * 2] = s16(value);
            m[word * 2 + 1] = s16(value >> 16U);
        }
    };
    if (index < 5)
        return matrix_word(transform.r, index);
    if (index < 8) {
        transform.t[index - 5] = static_cast<std::int32_t>(value);
        return;
    }
    if (index < 13)
        return matrix_word(light_, index - 8);
    if (index < 16) {
        back_[index - 13] = static_cast<std::int32_t>(value);
        return;
    }
    if (index < 21)
        return matrix_word(color_, index - 16);
    if (index < 24) {
        far_[index - 21] = static_cast<std::int32_t>(value);
        return;
    }
    switch (index) {
    case 24:
        screen.offset_x = static_cast<std::int32_t>(value);
        return;
    case 25:
        screen.offset_y = static_cast<std::int32_t>(value);
        return;
    case 26:
        screen.h = static_cast<std::uint16_t>(value);
        return;
    case 27:
        dqa_ = s16(value);
        return;
    case 28:
        dqb_ = static_cast<std::int32_t>(value);
        return;
    case 29:
        zsf3_ = s16(value);
        return;
    case 30:
        zsf4_ = s16(value);
        return;
    case 31:
        flag_ = value & 0x7ffff000U;
        if ((flag_ & flag_error_bits) != 0)
            flag_ |= 0x80000000U;
        return;
    default:
        throw std::out_of_range("GTE control register index");
    }
}

// A 44-bit accumulator step: sets the overflow flag and wraps to 44 bits.
std::int64_t Gte::check_mac(std::size_t index, std::int64_t value) {
    if (value >= mac_limit)
        flag_ |= flag_mac_positive >> (index - 1);
    else if (value < -mac_limit)
        flag_ |= flag_mac_negative >> (index - 1);
    return (value << 20) >> 20;
}

void Gte::check_mac0(std::int64_t value) {
    if (value > INT32_MAX)
        flag_ |= flag_mac0_positive;
    else if (value < INT32_MIN)
        flag_ |= flag_mac0_negative;
}

std::int16_t Gte::saturate_ir(std::size_t index, std::int64_t value, bool lm) {
    const std::int64_t low = lm ? 0 : -0x8000;
    if (value < low || value > 0x7fff) {
        flag_ |= flag_ir >> (index - 1);
        return static_cast<std::int16_t>(std::clamp<std::int64_t>(value, low, 0x7fff));
    }
    return static_cast<std::int16_t>(value);
}

void Gte::set_mac_ir(std::size_t index, std::int64_t value, int shift, bool lm) {
    value = check_mac(index, value);
    mac_[index] = static_cast<std::int32_t>(value >> shift);
    ir_[index] = saturate_ir(index, mac_[index], lm);
}

void Gte::multiply(const std::array<std::int16_t, 9> &m, const field::GteVector &v,
                   const std::array<std::int32_t, 3> &t, int shift, bool lm) {
    for (std::size_t i = 0; i < 3; ++i) {
        auto sum = check_mac(i + 1, (static_cast<std::int64_t>(t[i]) << 12) + m[i * 3] * v[0]);
        sum = check_mac(i + 1, sum + m[i * 3 + 1] * v[1]);
        sum = check_mac(i + 1, sum + m[i * 3 + 2] * v[2]);
        mac_[i + 1] = static_cast<std::int32_t>(sum >> shift);
    }
    for (std::size_t i = 1; i < 4; ++i)
        ir_[i] = saturate_ir(i, mac_[i], lm);
}

// RTPS/RTPT for one vector. IR3's flag uses the unshifted-by-sf depth.
void Gte::project(std::size_t index, int shift, bool lm, bool last) {
    const auto &v = v_[index];
    const auto &m = transform.r;
    std::array<std::int64_t, 3> sums{};
    for (std::size_t i = 0; i < 3; ++i) {
        auto sum =
            check_mac(i + 1, (static_cast<std::int64_t>(transform.t[i]) << 12) + m[i * 3] * v[0]);
        sum = check_mac(i + 1, sum + m[i * 3 + 1] * v[1]);
        sums[i] = check_mac(i + 1, sum + m[i * 3 + 2] * v[2]);
        mac_[i + 1] = static_cast<std::int32_t>(sums[i] >> shift);
    }
    ir_[1] = saturate_ir(1, mac_[1], lm);
    ir_[2] = saturate_ir(2, mac_[2], lm);
    const auto depth = sums[2] >> 12;
    if (depth < -0x8000 || depth > 0x7fff)
        flag_ |= flag_ir >> 2;
    ir_[3] = static_cast<std::int16_t>(std::clamp<std::int64_t>(mac_[3], lm ? 0 : -0x8000, 0x7fff));
    if (depth < 0 || depth > 0xffff)
        flag_ |= flag_otz;
    sz_ = {sz_[1], sz_[2], sz_[3],
           static_cast<std::uint16_t>(std::clamp<std::int64_t>(depth, 0, 0xffff))};
    if (screen.h >= static_cast<std::uint32_t>(sz_[3]) * 2)
        flag_ |= flag_divide;
    const auto q = static_cast<std::int64_t>(field::gte_divide(screen.h, sz_[3]));
    const auto x = q * ir_[1] + screen.offset_x;
    check_mac0(x);
    const auto y = q * ir_[2] + screen.offset_y;
    check_mac0(y);
    mac_[0] = static_cast<std::int32_t>(y);
    const auto screen_value = [&](std::int64_t value, std::uint32_t bit) {
        value >>= 16;
        if (value < -0x400 || value > 0x3ff) {
            flag_ |= bit;
            value = std::clamp<std::int64_t>(value, -0x400, 0x3ff);
        }
        return static_cast<std::int16_t>(value);
    };
    sx_ = {sx_[1], sx_[2], screen_value(x, flag_sx)};
    sy_ = {sy_[1], sy_[2], screen_value(y, flag_sy)};
    if (last) {
        const auto depth_cue = q * dqa_ + dqb_;
        check_mac0(depth_cue);
        mac_[0] = static_cast<std::int32_t>(depth_cue);
        const auto ir0 = depth_cue >> 12;
        if (ir0 < 0 || ir0 > 0x1000)
            flag_ |= flag_ir0;
        ir_[0] = static_cast<std::int16_t>(std::clamp<std::int64_t>(ir0, 0, 0x1000));
    }
}

void Gte::push_color() {
    const auto component = [&](std::int32_t value, std::uint32_t bit) {
        value >>= 4;
        if (value < 0 || value > 0xff) {
            flag_ |= bit;
            value = std::clamp(value, 0, 0xff);
        }
        return static_cast<std::uint32_t>(value);
    };
    rgb_[0] = rgb_[1];
    rgb_[1] = rgb_[2];
    rgb_[2] = component(mac_[1], flag_color) | (component(mac_[2], flag_color >> 1) << 8U) |
              (component(mac_[3], flag_color >> 2) << 16U) | (rgbc_ & 0xff000000U);
}

void Gte::mac_to_ir(bool lm) {
    for (std::size_t i = 1; i < 4; ++i)
        ir_[i] = saturate_ir(i, mac_[i], lm);
}

// MAC = color * IR << 4 (or color << 16), then MAC + (FC - MAC) * IR0.
void Gte::depth_cue(bool multiply_ir, bool fifo_color, int shift, bool lm) {
    const auto color = fifo_color ? rgb_[0] : rgbc_;
    const std::array<std::int16_t, 3> in{ir_[1], ir_[2], ir_[3]};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::int64_t c = static_cast<std::int64_t>((color >> (8U * i)) & 0xffU) << 4;
        const std::int64_t base = multiply_ir ? c * in[i] : c << 12;
        const auto difference =
            check_mac(i + 1, (static_cast<std::int64_t>(far_[i]) << 12) - base) >> shift;
        mac_[i + 1] = static_cast<std::int32_t>(difference);
        const auto step = saturate_ir(i + 1, mac_[i + 1], false);
        mac_[i + 1] = static_cast<std::int32_t>(
            check_mac(i + 1, base + static_cast<std::int64_t>(ir_[0]) * step) >> shift);
    }
    push_color();
    mac_to_ir(lm);
}

void Gte::execute(std::uint32_t command) {
    const int shift = (command & (1U << 19)) != 0 ? 12 : 0;
    const bool lm = (command & (1U << 10)) != 0;
    flag_ = 0;
    const std::array<std::int32_t, 3> none{};
    const auto ir_vector = [&]() { return field::GteVector{ir_[1], ir_[2], ir_[3]}; };
    const auto color_times_ir = [&]() {
        for (std::size_t i = 0; i < 3; ++i) {
            const std::int64_t c = static_cast<std::int64_t>((rgbc_ >> (8U * i)) & 0xffU) << 4;
            mac_[i + 1] = static_cast<std::int32_t>(check_mac(i + 1, c * ir_[i + 1]) >> shift);
        }
    };
    switch (command & 0x3fU) {
    case 0x01: // RTPS
        project(0, shift, lm, true);
        break;
    case 0x30: // RTPT
        project(0, shift, lm, false);
        project(1, shift, lm, false);
        project(2, shift, lm, true);
        break;
    case 0x06: { // NCLIP
        const auto value = static_cast<std::int64_t>(sx_[0]) * (sy_[1] - sy_[2]) +
                           static_cast<std::int64_t>(sx_[1]) * (sy_[2] - sy_[0]) +
                           static_cast<std::int64_t>(sx_[2]) * (sy_[0] - sy_[1]);
        check_mac0(value);
        mac_[0] = static_cast<std::int32_t>(value);
        break;
    }
    case 0x2d:   // AVSZ3
    case 0x2e: { // AVSZ4
        const bool four = (command & 0x3fU) == 0x2e;
        const std::int64_t sum = four ? static_cast<std::int64_t>(sz_[0]) + sz_[1] + sz_[2] + sz_[3]
                                      : static_cast<std::int64_t>(sz_[1]) + sz_[2] + sz_[3];
        const auto value = (four ? zsf4_ : zsf3_) * sum;
        check_mac0(value);
        mac_[0] = static_cast<std::int32_t>(value);
        const auto z = value >> 12;
        if (z < 0 || z > 0xffff)
            flag_ |= flag_otz;
        otz_ = static_cast<std::uint16_t>(std::clamp<std::int64_t>(z, 0, 0xffff));
        break;
    }
    case 0x12: { // MVMVA
        const auto mx = (command >> 17U) & 3U;
        const auto vi = (command >> 15U) & 3U;
        const auto cv = (command >> 13U) & 3U;
        if (mx == 3 || cv == 2)
            throw std::domain_error("GTE MVMVA garbage-matrix and far-color forms are not modeled");
        const auto &m = mx == 0 ? transform.r : mx == 1 ? light_ : color_;
        const auto v = vi == 3 ? ir_vector() : v_[vi];
        const auto &t = cv == 0 ? transform.t : cv == 1 ? back_ : none;
        multiply(m, v, t, shift, lm);
        break;
    }
    case 0x28: // SQR
        for (std::size_t i = 1; i < 4; ++i)
            mac_[i] = static_cast<std::int32_t>(
                check_mac(i, static_cast<std::int64_t>(ir_[i]) * ir_[i]) >> shift);
        mac_to_ir(lm);
        break;
    case 0x0c: { // OP
        const auto &r = transform.r;
        const std::array<std::int64_t, 3> values{
            static_cast<std::int64_t>(r[4]) * ir_[3] - static_cast<std::int64_t>(r[8]) * ir_[2],
            static_cast<std::int64_t>(r[8]) * ir_[1] - static_cast<std::int64_t>(r[0]) * ir_[3],
            static_cast<std::int64_t>(r[0]) * ir_[2] - static_cast<std::int64_t>(r[4]) * ir_[1]};
        for (std::size_t i = 0; i < 3; ++i)
            mac_[i + 1] = static_cast<std::int32_t>(check_mac(i + 1, values[i]) >> shift);
        mac_to_ir(lm);
        break;
    }
    case 0x10: // DPCS
        depth_cue(false, false, shift, lm);
        break;
    case 0x2a: // DPCT
        for (int i = 0; i < 3; ++i)
            depth_cue(false, true, shift, lm);
        break;
    case 0x29: // DCPL
        depth_cue(true, false, shift, lm);
        break;
    case 0x11: { // INTPL
        const std::array<std::int16_t, 3> in{ir_[1], ir_[2], ir_[3]};
        for (std::size_t i = 0; i < 3; ++i) {
            const std::int64_t base = static_cast<std::int64_t>(in[i]) << 12;
            mac_[i + 1] = static_cast<std::int32_t>(
                check_mac(i + 1, (static_cast<std::int64_t>(far_[i]) << 12) - base) >> shift);
            const auto step = saturate_ir(i + 1, mac_[i + 1], false);
            mac_[i + 1] = static_cast<std::int32_t>(
                check_mac(i + 1, base + static_cast<std::int64_t>(ir_[0]) * step) >> shift);
        }
        push_color();
        mac_to_ir(lm);
        break;
    }
    case 0x13:   // NCDS
    case 0x16:   // NCDT
    case 0x1b:   // NCCS
    case 0x3f:   // NCCT
    case 0x1e:   // NCS
    case 0x20: { // NCT
        const auto op = command & 0x3fU;
        const bool triple = op == 0x16 || op == 0x3f || op == 0x20;
        for (std::size_t n = 0; n < (triple ? 3U : 1U); ++n) {
            multiply(light_, v_[n], none, shift, lm);
            multiply(color_, ir_vector(), back_, shift, lm);
            if (op == 0x13 || op == 0x16) {
                depth_cue(true, false, shift, lm);
            } else if (op == 0x1b || op == 0x3f) {
                color_times_ir();
                push_color();
                mac_to_ir(lm);
            } else {
                push_color();
                mac_to_ir(lm);
            }
        }
        break;
    }
    case 0x1c: // CC
        multiply(color_, ir_vector(), back_, shift, lm);
        color_times_ir();
        push_color();
        mac_to_ir(lm);
        break;
    case 0x14: // CDP
        multiply(color_, ir_vector(), back_, shift, lm);
        depth_cue(true, false, shift, lm);
        break;
    case 0x3d: // GPF
        for (std::size_t i = 1; i < 4; ++i)
            mac_[i] = static_cast<std::int32_t>(
                check_mac(i, static_cast<std::int64_t>(ir_[i]) * ir_[0]) >> shift);
        push_color();
        mac_to_ir(lm);
        break;
    case 0x3e: // GPL
        for (std::size_t i = 1; i < 4; ++i)
            mac_[i] = static_cast<std::int32_t>(
                check_mac(i, (static_cast<std::int64_t>(mac_[i]) << shift) +
                                 static_cast<std::int64_t>(ir_[i]) * ir_[0]) >>
                shift);
        push_color();
        mac_to_ir(lm);
        break;
    default:
        throw std::domain_error("Unknown GTE command");
    }
    if ((flag_ & flag_error_bits) != 0)
        flag_ |= 0x80000000U;
}

} // namespace xem::reconstruction
