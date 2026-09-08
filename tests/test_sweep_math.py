"""Authored integer and geometry fixtures, using deliberately artificial tables."""

import unittest

from tools.analysis.sweep_math import (
    atan_angle,
    edge_projection,
    fixed_sqrt,
    planar_length,
    slope_projection,
)


class SweepMathTests(unittest.TestCase):
    reciprocal = (4096,) * 192
    roots = (4096,) * 192
    angles = tuple(index // 2 for index in range(1025))

    def test_atan_axes_quadrants_and_zero(self):
        for z, x, angle in (
            (0, 0, 0),
            (0, 1, 0),
            (1, 0, 1024),
            (0, -1, 2048),
            (-1, 0, -1024),
            (1, 2, 256),
            (1, -2, 1792),
            (-1, -2, -1792),
            (2, 1, 768),
        ):
            with self.subTest(z=z, x=x):
                self.assertEqual(atan_angle(z, x, self.angles).angle, angle)
        self.assertIsNone(atan_angle(0, 0, self.angles).table_index)

    def test_atan_uses_both_division_paths_and_supplied_coefficients(self):
        shifted = atan_angle(1, 2, self.angles)
        reduced = atan_angle(0x200000, 0x400000, self.angles)
        self.assertEqual((shifted.table_index, reduced.table_index), (512, 512))
        self.assertEqual(shifted.division_path, "z-smaller-shifted")
        self.assertEqual(reduced.division_path, "z-smaller-reduced")
        table = list(self.angles)
        table[512] = 123
        self.assertEqual(atan_angle(1, 2, table).angle, 123)

    def test_atan_wrapped_absolute_value_can_reach_original_break(self):
        with self.assertRaisesRegex(ValueError, "BREAK 7"):
            atan_angle(-0x80000000, 0, self.angles)
        with self.assertRaisesRegex(ValueError, "1025"):
            atan_angle(1, 2, self.angles[:-1])

    def test_sqrt_uses_lookup_bins_instead_of_host_square_root(self):
        result = fixed_sqrt(25, self.roots)
        self.assertEqual((result.value, result.leading_zeroes, result.table_index), (4, 27, 36))
        table = list(self.roots)
        table[36] = 5120
        self.assertEqual(fixed_sqrt(25, table).value, 5)
        self.assertEqual(fixed_sqrt(0, table).value, 0)
        self.assertIsNone(fixed_sqrt(0, table).table_index)

    def test_sqrt_sign_extends_the_coefficient_then_shifts_logically(self):
        table = list(self.roots)
        table[0] = -1
        result = fixed_sqrt(64, table)
        self.assertEqual(result.shifted_word, 0xFFFFFFF8)
        self.assertEqual(result.value, 0xFFFFF)

    def test_length_squares_signed_halfwords_and_rejects_unqualified_sum(self):
        self.assertEqual(planar_length(0x10010, 0, self.roots), 16)
        self.assertEqual(planar_length(-16, 0, self.roots), 16)
        self.assertEqual(planar_length(0, 0, self.roots), 0)
        with self.assertRaisesRegex(ValueError, "negative"):
            planar_length(-32768, -32768, self.roots)
        with self.assertRaisesRegex(ValueError, "192"):
            fixed_sqrt(1, ())

    def test_edge_gate_includes_both_endpoints_and_clears_vertical_motion(self):
        edge = ((0, 50, 0), (16, 70, 0))
        for relative, branch, x in (
            (127, "stop", 0),
            (128, "slide", -65536),
            (3968, "slide", 65536),
            (3969, "stop", 0),
        ):
            with self.subTest(relative=relative):
                result = edge_projection(
                    (0xC00 - relative) & 0xFFF,
                    edge,
                    (65536, -700, 0),
                    self.reciprocal,
                    self.roots,
                    self.angles,
                )
                self.assertEqual((result.branch, result.velocity), (branch, (x, 0, 0)))

    def test_edge_projection_can_take_slide_branch_with_zero_quantized_length(self):
        result = edge_projection(
            1024,
            ((0, 0, 0), (16, 0, 0)),
            (1, 20, 1),
            self.reciprocal,
            self.roots,
            self.angles,
        )
        self.assertEqual((result.branch, result.length, result.velocity), ("slide", 0, (0, 0, 0)))
        self.assertEqual(result.normal, (4096, 0, 0))

    def test_slope_uses_selected_floor_and_replaces_input_vertical_velocity(self):
        flat = slope_projection((65536, 123, 0), 0, 0, self.reciprocal, self.roots)
        slope = slope_projection((65536, 123, 0), 0, -1, self.reciprocal, self.roots)
        self.assertEqual(flat.velocity, (65536, 0, 0))
        self.assertEqual(slope.normal_input, (-256, -256, 0))
        self.assertEqual(slope.velocity, (65536, -65536, 0))
        self.assertEqual(slope.length_input, (256, 0))


if __name__ == "__main__":
    unittest.main()
