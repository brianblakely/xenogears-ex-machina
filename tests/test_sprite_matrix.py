"""Artificial coefficients expose original matrix rounding and storage widths."""

import struct
import unittest

from tests.sprite_fixtures import ADDRESS, invented_sprite
from tools.analysis.sprite_matrix import fixed_product, rotation, scale_matrix, sprite_matrix
from tools.analysis.sprite_state import put


class SpriteMatrixTests(unittest.TestCase):
    def test_fixed_product_wraps_before_arithmetic_shift(self):
        self.assertEqual(fixed_product(1, -1), -1)
        self.assertEqual(fixed_product(32767, 0x7FFFFFFF), 524280)
        self.assertEqual(fixed_product(0x100000001, 4096), 1)

    def test_rotation_negates_before_rounding_and_masks_table_angles(self):
        table = bytearray(struct.pack("<2h", 0, 4096) * 4096)
        struct.pack_into("<2h", table, 4, 0, 1)
        struct.pack_into("<2h", table, 8, 1, 4096)
        expected = (1, -1, 0, 1, 4096, 0, 0, 0, 1)
        self.assertEqual(rotation((0, 1, 2), table), expected)
        self.assertEqual(rotation((4096, 4097, -4094), table), expected)

    def test_row_and_column_scaling_preserve_translation_and_overwrite_padding(self):
        matrix = struct.pack("<9hH3i", *range(1, 10), 0xABCD, 100, -200, 300)
        for columns, expected in (
            (False, (1, 2, 3, 8, 10, 12, -7, -8, -9)),
            (True, (1, 4, -3, 4, 10, -6, 7, 16, -9)),
        ):
            out = scale_matrix(matrix, (4096, 8192, -4096), columns)
            self.assertEqual(struct.unpack_from("<9h", out), expected)
            self.assertEqual(out[18:20], b"\xff\xff")
            self.assertEqual(out[20:], matrix[20:])

    def test_final_scale_element_is_a_word_while_earlier_elements_are_halfwords(self):
        matrix = struct.pack("<9hH3i", *((30000,) * 9), 0, 1, 2, 3)
        out = scale_matrix(matrix, (8192,) * 3)
        self.assertEqual(struct.unpack_from("<8h", out), (-5536,) * 8)
        self.assertEqual(struct.unpack_from("<i", out, 16)[0], 60000)

    def test_sprite_extra_scale_halves_unsigned_input_and_preserves_other_storage(self):
        sprite, _, table = invented_sprite()
        sprite[0xC0 + 20 : 0xC0 + 32] = bytes(range(12))
        put(sprite, 0x3A, 4097, 2)
        out = sprite_matrix(sprite, ADDRESS, table)
        self.assertEqual(struct.unpack_from("<9h", out, 0xC0), (2048, 0, 0, 0, 2048, 0, 0, 0, 2048))
        self.assertEqual(out[:0xC0], sprite[:0xC0])
        self.assertEqual(out[0xC0 + 20 :], sprite[0xC0 + 20 :])

    def test_unsupported_product_and_missing_storage_are_explicit(self):
        sprite, _, table = invented_sprite()
        put(sprite, 0x40, 1)
        with self.assertRaisesRegex(ValueError, "GTE"):
            sprite_matrix(sprite, ADDRESS, table)
        with self.assertRaises(ValueError):
            rotation((0, 0, 0), table[:-1])
        with self.assertRaises(ValueError):
            sprite_matrix(sprite[:80], ADDRESS, table)


if __name__ == "__main__":
    unittest.main()
