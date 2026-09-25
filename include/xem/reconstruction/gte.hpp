#pragma once

#include "xem/reconstruction/field_gte.hpp"

#include <array>
#include <cstdint>

namespace xem::reconstruction {

// The PlayStation geometry coprocessor (COP2) as recovered code drives it:
// register transfers (MTC2/MFC2/CTC2/CFC2, LWC2/SWC2) and the command set.
// Arithmetic, saturation and FLAG bits follow general PS1 hardware
// documentation; nothing here is game-specific. The rotation/translation and
// screen registers keep the types the existing matrix helpers use.
class Gte {
  public:
    // Control registers 0-7 and 24-26.
    field::GteMatrix transform{};
    field::GteScreen screen{};

    // Data register access with the hardware read/write conversions
    // (sign/zero extension, SXYP push, IRGB/ORGB, LZCS/LZCR).
    [[nodiscard]] std::uint32_t data(std::uint32_t index) const;
    void set_data(std::uint32_t index, std::uint32_t value);
    [[nodiscard]] std::uint32_t control(std::uint32_t index) const;
    void set_control(std::uint32_t index, std::uint32_t value);

    // Executes one COP2 command word (bits 0-24 of the instruction).
    void execute(std::uint32_t command);

    // Named commands with the instruction fields the original issues.
    void rtps() { execute(0x01U | sf_bit); }
    void rtpt() { execute(0x30U | sf_bit); }
    void nclip() { execute(0x06U); }
    void avsz3() { execute(0x2dU | sf_bit); }
    void avsz4() { execute(0x2eU | sf_bit); }
    // MVMVA: matrix mx (0 rotation, 1 light, 2 color) times vector v (0-2, 3
    // IR) plus cv (0 translation, 1 background, 3 none).
    void mvmva(std::uint32_t mx, std::uint32_t v, std::uint32_t cv) {
        execute(0x12U | sf_bit | mx << 17U | v << 15U | cv << 13U);
    }
    void set_ir(const field::GteVector &vector) {
        ir_[1] = vector[0];
        ir_[2] = vector[1];
        ir_[3] = vector[2];
    }

    // Vector/data accessors used by reconstructed callers.
    [[nodiscard]] std::int32_t mac(std::size_t index) const { return mac_[index]; }
    [[nodiscard]] std::int16_t ir(std::size_t index) const { return ir_[index]; }
    [[nodiscard]] std::uint32_t flag() const { return flag_; }
    [[nodiscard]] std::uint16_t otz() const { return otz_; }
    [[nodiscard]] std::uint32_t sxy(std::size_t index) const;
    [[nodiscard]] std::uint16_t sz(std::size_t index) const { return sz_[index]; }
    [[nodiscard]] std::uint32_t rgb(std::size_t index) const { return rgb_[index]; }
    void set_vector(std::size_t index, const field::GteVector &vector) { v_[index] = vector; }
    [[nodiscard]] const field::GteVector &vector(std::size_t index) const { return v_[index]; }

    bool operator==(const Gte &) const = default;

  private:
    static constexpr std::uint32_t sf_bit = 1U << 19;

    // Data registers.
    std::array<field::GteVector, 3> v_{}; // V0-V2
    std::uint32_t rgbc_{};                // RGBC
    std::uint16_t otz_{};
    std::array<std::int16_t, 4> ir_{}; // IR0-IR3
    std::array<std::int16_t, 3> sx_{}, sy_{};
    std::array<std::uint16_t, 4> sz_{};
    std::array<std::uint32_t, 3> rgb_{}; // RGB0-RGB2
    std::uint32_t res1_{};
    std::array<std::int32_t, 4> mac_{}; // MAC0-MAC3
    std::uint32_t lzcs_{}, lzcr_{};
    // Control registers other than rotation/translation and screen.
    std::array<std::int16_t, 9> light_{}; // L11..L33
    std::array<std::int32_t, 3> back_{};  // RBK, GBK, BBK
    std::array<std::int16_t, 9> color_{}; // LR1..LB3
    std::array<std::int32_t, 3> far_{};   // RFC, GFC, BFC
    std::int16_t dqa_{};
    std::int32_t dqb_{};
    std::int16_t zsf3_{}, zsf4_{};
    std::uint32_t flag_{};

    std::int64_t check_mac(std::size_t index, std::int64_t value);
    void check_mac0(std::int64_t value);
    std::int16_t saturate_ir(std::size_t index, std::int64_t value, bool lm);
    void set_mac_ir(std::size_t index, std::int64_t value, int shift, bool lm);
    void multiply(const std::array<std::int16_t, 9> &matrix, const field::GteVector &vector,
                  const std::array<std::int32_t, 3> &translation, int shift, bool lm);
    void project(std::size_t vector, int shift, bool lm, bool last);
    void depth_cue(bool multiply_ir, bool fifo_color, int shift, bool lm);
    void push_color();
    void mac_to_ir(bool lm);
};

} // namespace xem::reconstruction
