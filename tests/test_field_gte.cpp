// Invented tables and matrices exercise the reconstructed arithmetic and
// saturation boundaries. They describe no original content or observation.
#include "xem/reconstruction/field_gte.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void pair(std::vector<std::uint8_t> &table, std::size_t angle, std::int16_t sine,
          std::int16_t cosine) {
    const auto s = static_cast<std::uint16_t>(sine), c = static_cast<std::uint16_t>(cosine);
    table[angle * 4] = static_cast<std::uint8_t>(s);
    table[angle * 4 + 1] = static_cast<std::uint8_t>(s >> 8U);
    table[angle * 4 + 2] = static_cast<std::uint8_t>(c);
    table[angle * 4 + 3] = static_cast<std::uint8_t>(c >> 8U);
}
std::vector<std::uint8_t> table() {
    std::vector<std::uint8_t> result(0x4000);
    for (std::size_t angle = 0; angle < 4096; ++angle)
        pair(result, angle, 0, 4096);
    pair(result, 1024, 4096, 0);
    pair(result, 3, 100, -200);
    return result;
}

void rotations() {
    const auto trig = table();
    const auto identity = field::rotation_matrix({0, 0, 0}, trig);
    check(identity.r == std::array<std::int16_t, 9>{4096, 0, 0, 0, 4096, 0, 0, 0, 4096},
          "Zero angles give the fixed-point identity");
    const field::GteMatrix kept{{}, {1, 2, 3}};
    check(field::rotation_matrix({0, 0, 0}, trig, kept).t == kept.t,
          "RotMatrix leaves translation words untouched");
    // Z rotation by a quarter turn: sin 1, cos 0.
    const auto z = field::rotation_matrix({0, 0, 1024}, trig);
    check(z.r == std::array<std::int16_t, 9>{0, -4096, 0, 4096, 0, 0, 0, 0, 4096},
          "Z rotation places sine and negated sine in the original entries");
    // The angle's high bits are masked to twelve bits.
    check(field::rotation_matrix({0, 0, static_cast<std::int16_t>(1024 + 4096)}, trig).r == z.r,
          "Angles wrap at 4096");
    // Invented small sine/cosine exercise the product shifts on every axis.
    const auto mixed = field::rotation_matrix({3, 3, 3}, trig);
    check(mixed.r[2] == 100 && mixed.r[8] == ((-200 * -200) >> 12),
          "Y sine and X/Y cosine products follow the original entry order");
    // RotMatrixYXZ: X by a quarter turn either way; a negative angle takes
    // the table at its magnitude with the sine negated.
    check(field::rotation_matrix_yxz({0, 0, 0}, trig) == identity.r,
          "RotMatrixYXZ of zero angles is the identity");
    check(field::rotation_matrix_yxz({1024, 0, 0}, trig) ==
              std::array<std::int16_t, 9>{4096, 0, 0, 0, 0, -4096, 0, 4096, 0},
          "RotMatrixYXZ places the X sine in the original entries");
    check(field::rotation_matrix_yxz({-1024, 0, 0}, trig) ==
              std::array<std::int16_t, 9>{4096, 0, 0, 0, 0, 4096, 0, -4096, 0},
          "RotMatrixYXZ negates the sine of a negative angle");
    const auto yxz = field::rotation_matrix_yxz({0, 3, 1024}, trig);
    check(yxz[0] == ((-200 * 0) >> 12) + 0 && yxz[1] == -((-200 * 4096) >> 12) && yxz[2] == 100 &&
              yxz[3] == 4096 && yxz[8] == -200,
          "RotMatrixYXZ combines Y and Z in the original entry order");
}

void matrix_products() {
    const field::GteMatrix quarter{{0, -4096, 0, 4096, 0, 0, 0, 0, 4096}, {10, 20, 30}};
    field::GteMatrix right{{4096, 0, 0, 0, 4096, 0, 0, 0, 4096}, {0x10005, -7, 0x7fffffff}};
    auto product = right;
    field::multiply_rotation(quarter, product);
    check(product.r == quarter.r && product.t == right.t,
          "MulMatrix2 rotates columns and keeps the right translation");
    auto negative = right;
    negative.pad = 0x1234;
    negative.r[8] = -4096;
    field::multiply_rotation(quarter, negative);
    check(negative.r[8] == -4096 && negative.pad == -1,
          "The last entry is a sign-extended word store, overwriting pad");
    const auto composed = field::compose_matrix(quarter, right);
    // Only low halfwords of the right translation reach the GTE: 5, -7, -1.
    check(composed.t == std::array<std::int32_t, 3>{10 + 7, 20 + 5, 30 - 1},
          "CompMatrix truncates translation input and adds the left words");
    const field::GteMatrix large{{0x7fff, 0x7fff, 0x7fff, 0, 0, 0, -0x8000, -0x8000, -0x8000},
                                 {0, 0, 0}};
    const auto saturated = field::rotate_translate(large, {0x7fff, 0x7fff, 0x7fff});
    check(saturated[0] == 0x7fff && saturated[2] == -0x8000,
          "IR saturates to signed 16 bits while MAC keeps the 32-bit value");
    const auto raw = field::gte_rotate(large, {0x7fff, 0x7fff, 0x7fff}, false);
    check(raw.mac[0] == static_cast<std::int32_t>((3LL * 0x7fff * 0x7fff) >> 12),
          "MAC retains the shifted sum");
    const field::GteMatrix far{{4096, 0, 0, 0, 4096, 0, 0, 0, 4096}, {0x7fffffff, 0, 0}};
    bool rejected = false;
    try {
        static_cast<void>(field::rotate_translate(far, {0x7fff, 0, 0}));
    } catch (const std::domain_error &) {
        rejected = true;
    }
    check(rejected, "An accumulator beyond 44 bits is not silently accepted");
}

void long_vectors_and_stack() {
    const field::GteMatrix identity{{4096, 0, 0, 0, 4096, 0, 0, 0, 4096}, {}};
    // ApplyMatrixLV splits each word into a signed high part and 15 low bits,
    // so the identity returns every representable input exactly, including
    // negative values whose parts both carry the sign.
    for (const std::int32_t value : {0, 1, 0x7fff, 0x8000, -1, -0x8000, 0x12345678, -0x12345678})
        check(field::apply_matrix_lv(identity, {value, -value, value}) ==
                  field::GteLong{value, -value, value},
              "ApplyMatrixLV recombines high and low parts");
    const field::GteMatrix half{{2048, 0, 0, 0, 2048, 0, 0, 0, 2048}, {}};
    // (0x10001 >> 15) = 2 contributes 2 * 2048 without a shift (then << 3),
    // the low part 1 contributes (1 * 2048) >> 12 = 0.
    check(field::apply_matrix_lv(half, {0x10001, 0, 0})[0] == 0x8000,
          "ApplyMatrixLV truncates the low product only");
    auto scaled = identity;
    scaled.r[8] = 0x7fff;
    field::scale_matrix(scaled, {2, 3, 0x10000});
    check(scaled.r[0] == 2 && scaled.r[4] == 3,
          "ScaleMatrix multiplies each column by its component");
    // 0x7fff * 0x10000 >> 12 = 0x7fff0: the low half is stored, the high half lands in pad.
    check(scaled.r[8] == static_cast<std::int16_t>(0xfff0) && scaled.pad == 7,
          "ScaleMatrix stores the last entry as a full word");
    check(field::outer_product12({4096, 0, 0}, {0, 4096, 0}) == field::GteLong{0, 0, 4096},
          "OuterProduct12 is the 4.12 cross product");
    check(field::outer_product12({0x10000 + 4096, 0, 0}, {0, 4096, 0}) ==
              field::GteLong{0, 0, 4096},
          "OuterProduct12 reads only the low halfwords");
    field::GteMatrix loaded{{1, 2, 3, 4, 5, 6, 7, 8, -9}, {10, -11, 12}};
    const auto words = field::gte_words(loaded);
    check(words[0] == 0x00020001U && words[4] == 0xfffffff7U && words[6] == 0xfffffff5U,
          "Control words pack pairs and sign-extend R33");
    auto unextended = words;
    unextended[4] = 0x1234fff7U;
    check(field::gte_from_words(unextended) == loaded, "CTC2 keeps only R33's low halfword");
    field::MatrixStack stack;
    field::push_matrix(stack, loaded);
    check(stack.depth == 0x20 && stack.records[16] == 0xf7 && stack.records[19] == 0xff,
          "PushMatrix stores the eight control words");
    check(field::pop_matrix(stack) == loaded && stack.depth == 0,
          "PopMatrix restores the pushed registers");
    bool rejected = false;
    try {
        static_cast<void>(field::pop_matrix(stack));
    } catch (const std::domain_error &) {
        rejected = true;
    }
    check(rejected, "The empty-stack diagnostic path is not silently accepted");
    stack.depth = 0x280;
    rejected = false;
    try {
        field::push_matrix(stack, loaded);
    } catch (const std::domain_error &) {
        rejected = true;
    }
    check(rejected, "The full-stack diagnostic path is not silently accepted");
}

void perspective() {
    const field::GteMatrix view{{4096, 0, 0, 0, 4096, 0, 0, 0, 4096}, {64, -64, 1000}};
    // H / SZ3 = 1/2: 64 * 0x8000 >> 16 = 32 with a half-pixel offset absorbing the
    // reciprocal table's rounding of the quotient.
    const field::GteScreen screen{0x8000, 0x8000, 500};
    const auto point = field::rot_trans_pers(view, screen, {0, 0, 0});
    check(point[0] == 32 && point[1] == -32, "RTPS divides IR by depth and adds the offset");
    const field::GteMatrix near{{4096, 0, 0, 0, 4096, 0, 0, 0, 4096}, {0x7fff, -0x8000, 100}};
    // H >= 2 * SZ3 saturates the quotient at 1ffff; screen X/Y saturate at 3ff/-400.
    const auto clamped = field::rot_trans_pers(near, {0, 0, 500}, {0, 0, 0});
    check(clamped[0] == 0x3ff && clamped[1] == -0x400, "RTPS saturates quotient and screen");
    const field::GteMatrix behind{{4096, 0, 0, 0, 4096, 0, 0, 0, 4096}, {0, 0, -100}};
    check(field::rot_trans_pers(behind, {0, 0, 500}, {0, 0, 0}) ==
              std::array<std::int16_t, 2>{0, 0},
          "Negative depth clamps SZ3 to zero");
}
} // namespace

int main() {
    try {
        rotations();
        matrix_products();
        long_vectors_and_stack();
        perspective();
        std::cout << "Field GTE reconstruction: four source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
