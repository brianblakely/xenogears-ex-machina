"""Authored geometric fixtures and arithmetic boundaries; no original assets."""

import unittest

from tools.analysis.collision_math import (
    edge_area,
    height_and_normal,
    locate,
    normalize,
    packed_coordinate,
)
from tools.analysis.field import CollisionLayer, CollisionTriangle


class CollisionMathTests(unittest.TestCase):
    # Deliberately artificial coefficients make these fixtures independent of
    # the original table. Original coefficients are tested only in local runs.
    table = (4096,) * 192

    def test_zero_axes_and_input_halfword_conversion(self):
        for vector, expected in (
            ((0, 0, 0), (0, 0, 0)),
            ((16, 0, 0), (4096, 0, 0)),
            ((0, -16, 0), (0, -4096, 0)),
            ((0, 0, 0x10010), (0, 0, 4096)),
        ):
            with self.subTest(vector=vector):
                self.assertEqual(normalize(vector, self.table), expected)

    def test_lookup_bin_and_negative_arithmetic_shift(self):
        table = list(self.table)
        table[1] = 4080
        table[168] = 4095
        self.assertEqual(normalize((8, 1, 0), table), (4080, 510, 0))
        self.assertEqual(normalize((7, -3, 0), table), (7166, -3072, 0))

    def test_unqualified_magnitude_overflow_and_missing_coefficients_fail(self):
        with self.assertRaisesRegex(ValueError, "overflow"):
            normalize((-32768, -32768, -32768), self.table)
        with self.assertRaisesRegex(ValueError, "outside supplied"):
            normalize((8, 1, 0), (4096,))

    def test_flat_and_sloped_height_do_not_renormalize_the_cross_product(self):
        flat = ((0, 7, 0), (16, 7, 0), (0, 7, 16))
        slope = ((0, 10, 0), (16, 26, 0), (0, 10, 16))
        self.assertEqual(height_and_normal(flat, 3, 5, self.table), (7, (0, -4096, 0)))
        self.assertEqual(height_and_normal(slope, -3, 5, self.table), (7, (4096, -4096, 0)))

    def test_vertical_plane_returns_zero_height(self):
        vertical = ((0, 3, 0), (0, 19, 0), (0, 3, 16))
        self.assertEqual(height_and_normal(vertical, 0, 4, self.table), (0, (4096, 0, 0)))

    def test_height_division_truncates_toward_zero_before_halfword_store(self):
        plane = ((0, 32767, 0), (16, 32759, 0), (0, 32767, 16))
        self.assertEqual(height_and_normal(plane, -3, 0, self.table)[0], -32768)
        self.assertEqual(height_and_normal(plane, 3, 0, self.table)[0], 32766)

    def test_packed_negative_coordinate_borrows_from_the_other_halfword(self):
        self.assertEqual(packed_coordinate(4, -1), (-1, 3))
        self.assertEqual(packed_coordinate(-32768, -1), (-1, 32767))
        self.assertEqual(edge_area((0, 0, 0), (0, 0, 16), 2, 3), 32)

    def test_first_containing_face_including_edges_and_explicit_no_match(self):
        vertices = tuple((x, y, z, 0) for y in (7, 12) for x, z in ((0, 0), (0, 16), (16, 0)))
        triangles = tuple(CollisionTriangle(v, (0xFFFF,) * 3, 0) for v in ((0, 1, 2), (3, 4, 5)))
        layer = CollisionLayer(triangles, vertices)
        self.assertEqual(locate(layer, 2, 3, self.table), (0, (2, 7, 3), (0, 4096, 0)))
        self.assertEqual(locate(layer, 0, 0, self.table)[0], 0)
        self.assertIsNone(locate(layer, 20, 20, self.table))


if __name__ == "__main__":
    unittest.main()
