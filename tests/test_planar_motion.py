"""Invented signed/rounding/branch boundaries; no original game table is embedded."""

import struct
import unittest

from tools.analysis.planar_motion import (
    FieldVector,
    SpriteVector,
    animation_speed,
    command_pc_store,
    field_party_velocity,
    motion_mode,
    sprite_bundle_offsets,
    sprite_velocity,
    trig_pair,
)


class PlanarMotionTests(unittest.TestCase):
    def test_inhibition_precedes_owner_and_air_mode_selection(self):
        self.assertIsNone(motion_mode(0x01005800, 0x40, 1, 2))
        self.assertEqual(motion_mode(0x4000, 0x40, 1, 0), 2)
        for flags, buttons, updated in ((0, 0x40, 1), (0x4000, 0, 1), (0x4000, 0x40, 2)):
            self.assertEqual(motion_mode(flags, buttons, updated, 2), 1)

    def test_air_mode_retains_only_signed_walk_or_run(self):
        for flag in (0x800, 0x1000, 0x1800):
            self.assertEqual(motion_mode(flag, 0, 0, 2), 2)
            self.assertEqual(motion_mode(flag | 0x4000, 0x40, 1, 0x10001), 1)
            for old in (0, 3, 0xFFFF):
                self.assertEqual(motion_mode(flag | 0x4000, 0x40, 1, old), 2)
        self.assertEqual(motion_mode(0, 0, 0, 2), 1)

    def test_speed_signed_operand_and_truncation_toward_zero(self):
        self.assertEqual(animation_speed(255, 4097, 0), -4096)
        self.assertEqual(animation_speed(1, 4097, 0), 4096)
        self.assertEqual(animation_speed(255, 255, 0), 0)
        self.assertEqual(animation_speed(128, 4096, 0), -524288)

    def test_speed_signed_scale_and_rate_product_wrap(self):
        self.assertEqual(animation_speed(3, 0xF000, 0), -12288)
        self.assertEqual(animation_speed(127, 4096, 0x7FFFFFFF), 0)
        self.assertEqual(animation_speed(99, 123, 0xFFFFFFFF), 0)
        # 6,724,836,944 wraps to -1,865,097,648; truncation gives -455,346.
        self.assertEqual(animation_speed(127, 32767, 100), -116568576)

    def test_lookup_masks_angle_and_reads_signed_pair(self):
        table = bytearray(16384)
        struct.pack_into("<hh", table, 4095 * 4, -32768, 32767)
        for angle in (-1, 4095, 8191, 0xFFFFFFFF):
            self.assertEqual(trig_pair(table, angle), (-32768, 32767))
        with self.assertRaises(ValueError):
            trig_pair(bytes(16383), 0)

    def test_vector_integer_shifts_floor_before_division(self):
        self.assertEqual(sprite_velocity(-1, 3 << 7, 0, 256), SpriteVector(-85, -85, 0))
        self.assertEqual(sprite_velocity(15, 3 << 7, 0, 256), SpriteVector(0, 0, 0))

    def test_vector_negates_before_final_shift(self):
        self.assertEqual(sprite_velocity(16, 256 << 7, 4, -1), SpriteVector(1, -1, -1))
        self.assertEqual(sprite_velocity(16, 256 << 7, -1, 4), SpriteVector(1, 0, 0))

    def test_vector_multiply_and_negation_wrap(self):
        effect = sprite_velocity(1 << 27, 1 << 7, 8, 8)
        self.assertEqual(effect, SpriteVector(-2147483648, 0, 0))
        effect = sprite_velocity(1 << 26, 1 << 7, 8, 8)
        self.assertEqual(effect, SpriteVector(1073741824, -33554432, -33554432))

    def test_vector_divisor_masks_flags_and_rejects_zero(self):
        expected = sprite_velocity(32, 256 << 7, 256, 256)
        self.assertEqual(sprite_velocity(32, (256 << 7) | 0xF800007F, 256, 256), expected)
        with self.assertRaises(ValueError):
            sprite_velocity(0, 0xF800007F, 0, 0)

    def test_field_quantizes_positive_and_negative_components_down(self):
        table = bytearray(16384)
        struct.pack_into("<hh", table, 28, 4, -4)
        effect = field_party_velocity(7, 99, 0x40, 0, 16, 256 << 7, table)
        self.assertEqual(effect, FieldVector(7, -4096, -4096, SpriteVector(1, -1, -1)))
        struct.pack_into("<hh", table, 28, -32768, 32767)
        effect = field_party_velocity(7, 99, 0x40, 0, 8192, 256 << 7, table)
        self.assertEqual((effect.x, effect.z), (61440, 65536))

    def test_stop_short_circuits_actor_lookup_and_zero_divisor(self):
        effect = field_party_velocity(0x8001, 0xFFFF, 0x40, None, 99, 0, b"")
        self.assertEqual(effect, FieldVector(-1, 0, 0, None))
        effect = field_party_velocity(0x8000, 123, 0x40, 0x82000, 99, 0, b"")
        self.assertEqual(effect.angle, 123)

    def test_unreconstructed_field_paths_are_explicit(self):
        with self.assertRaisesRegex(ValueError, "ratio"):
            field_party_velocity(0x8000, 0, 0, 0, 0, 0, b"")
        for flags in (None, 0x2000, 0x80000, 0x82000):
            with self.assertRaisesRegex(ValueError, "alternate"):
                field_party_velocity(0, 0, 0x40, flags, 0, 0, b"")

    def test_pc_store_uses_current_pointer_and_unsigned_width_with_wrap(self):
        widths = bytearray(256)
        widths[163] = 255
        self.assertEqual(command_pc_store(0xFFFFFFFE, 163, widths), 253)
        self.assertEqual(command_pc_store(123, 163, widths), 378)
        self.assertEqual(command_pc_store(123, 162, widths), 123)

    def test_pc_store_rejects_unqualified_width_access(self):
        for opcode, widths in ((256, bytes(256)), (-1, bytes(256)), (0, bytes(255))):
            with self.assertRaises(ValueError):
                command_pc_store(0, opcode, widths)

    def test_field_sprite_index_preserves_source_relative_offsets(self):
        data = struct.pack("<III", 2, 12, 17) + bytes(9)
        self.assertEqual(sprite_bundle_offsets(data), (12, 17))

    def test_field_sprite_index_rejects_bad_counts_and_overlapping_bounds(self):
        cases = [b"", struct.pack("<I", 0), struct.pack("<I", 0xFFFFFFFF)]
        cases += [
            struct.pack("<III", 2, begin, end) + bytes(8)
            for begin, end in ((4, 16), (12, 12), (16, 12), (12, 20), (12, 99))
        ]
        for data in cases:
            with self.assertRaises(ValueError):
                sprite_bundle_offsets(data)


if __name__ == "__main__":
    unittest.main()
