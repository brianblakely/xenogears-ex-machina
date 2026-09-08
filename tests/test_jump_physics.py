"""Invented arithmetic and branch fixtures, separate from original-run evidence."""

import struct
import unittest

from tools.analysis.field import CollisionLayer, CollisionPackage, CollisionTriangle
from tools.analysis.jump_physics import (
    SpriteTiming,
    VerticalState,
    gravity_prefix,
    sprite_impulse,
    terrain_attribute,
    vertical_step,
)


class JumpPhysicsTests(unittest.TestCase):
    def test_impulse_sign_extension_and_fraction_truncation(self):
        timing = SpriteTiming(4097, 0, 256 << 7)
        effect = sprite_impulse(255, timing, 0, None)
        self.assertEqual((effect.velocity, effect.division_numerator), (-4096, -1048576))
        self.assertFalse(effect.used_reference)
        self.assertEqual(sprite_impulse(1, timing, 0, None).velocity, 4096)

    def test_impulse_reference_bypasses_operand_scaling_only(self):
        timing = SpriteTiming(123, 987, 7 << 7)
        effect = sprite_impulse(91, timing, 1, -5)
        self.assertEqual((effect.velocity, effect.division_numerator), (-182, -1280))
        self.assertTrue(effect.used_reference)
        with self.assertRaises(ValueError):
            sprite_impulse(91, timing, 1, None)
        self.assertEqual(sprite_impulse(0, timing, 0, -5).velocity, 0)
        self.assertFalse(sprite_impulse(0, timing, 1, 0).used_reference)

    def test_impulse_wraps_multiply_and_sign_extends_scale(self):
        self.assertEqual(
            sprite_impulse(127, SpriteTiming(4096, 0x7FFFFFFF, 128), 0, None).velocity, 0
        )
        timing = SpriteTiming(0xF000, 0, 256 << 7)
        self.assertEqual(sprite_impulse(1, timing, 0, None).velocity, -4096)

    def test_divisor_uses_only_packed_twelve_bits_and_rejects_zero(self):
        timing = SpriteTiming(4096, 0, (256 << 7) | 0xF800007F)
        self.assertEqual(sprite_impulse(1, timing, 0, None).velocity, 4096)
        zero = SpriteTiming(4096, 0, 0xF800007F)
        with self.assertRaises(ValueError):
            sprite_impulse(1, zero, 0, None)
        with self.assertRaises(ValueError):
            gravity_prefix((4, 0, 0), 0, 0, zero)

    def test_gravity_signed_six_bit_coefficient_boundaries(self):
        timing = SpriteTiming(4096, 0, 256 << 7)
        for encoded, expected in ((0, 0), (1, 1024), (31, 31744), (32, -32768), (63, -1024)):
            effect = gravity_prefix(((encoded << 2) | 2, 3, 7), 100, 0xFFFFFFFF, timing)
            self.assertEqual(effect.gravity, expected)
            self.assertEqual(effect.sprite_flags, 0xFFEFFFFF)

    def test_gravity_truncates_factor_before_multiplication(self):
        effect = gravity_prefix((31 << 2, 0, 0), 0, 0, SpriteTiming(2048, 0, 256 << 7))
        self.assertEqual(effect.gravity, 0)
        effect = gravity_prefix((31 << 2, 0, 0), 0, 0, SpriteTiming(4096, 0, 4095 << 7))
        self.assertEqual(effect.gravity, 124)
        effect = gravity_prefix((33 << 2, 0, 0), 0, 0, SpriteTiming(4096, 0, 4095 << 7))
        self.assertEqual(effect.gravity, -124)

    def test_gravity_rate_and_pointer_addition_wrap(self):
        effect = gravity_prefix((124, 65535, 9), 0xFFFFFFFC, 0, SpriteTiming(4096, -1, 128))
        self.assertEqual(
            (effect.gravity, effect.command_pointer, effect.frame_pointer), (0, 65533, 9)
        )

    def test_attribute_uses_low_byte_and_disabled_layer_short_circuit(self):
        layer = CollisionLayer((CollisionTriangle((0, 0, 0), (0, 0, 0), 0xAB01),), ())
        mesh = CollisionPackage((layer,), struct.pack("<II", 11, 22), ())
        self.assertEqual(terrain_attribute(0, 0, (0,), mesh), 22)
        self.assertEqual(terrain_attribute(8, 0, (-1,), mesh), 0)
        self.assertEqual(terrain_attribute(1, -3, (), mesh), 0)

    def test_invalid_terrain_access_is_explicit(self):
        layer = CollisionLayer((CollisionTriangle((0, 0, 0), (0, 0, 0), 2),), ())
        mesh = CollisionPackage((layer,), bytes(4), ())
        for layer_id, triangles in (
            (-1, (0,)),
            (1, (0,)),
            (0, ()),
            (0, (-1,)),
            (0, (1,)),
            (0, (0,)),
        ):
            with self.assertRaises(ValueError):
                terrain_attribute(0, layer_id, triangles, mesh)

    def test_invented_jump_integrates_old_velocity_then_gravity(self):
        state = VerticalState(7 << 16, -229376, 0x400800, 99)
        expected_y = (229376, 65536, -32768, -65536, -32768, 65536, 229376, 458752)
        for index, y in enumerate(expected_y):
            effect = vertical_step(state, 65536, 7, 2, 2, 0)
            self.assertEqual(effect.state.y, y)
            self.assertEqual(effect.branch, "floor" if index == 7 else "airborne")
            state = effect.state
        self.assertEqual(state, VerticalState(458752, 0, 0x800, 0))

    def test_floor_comparison_uses_signed_integer_part(self):
        air = vertical_step(VerticalState(655359, 0, 0, 0), 1, 10, 0, 0, 0)
        self.assertEqual((air.branch, air.state.y, air.state.velocity), ("airborne", 655359, 1))
        floor = vertical_step(VerticalState(655361, 0, 0, 0), 1, 10, 0, 0, 0)
        self.assertEqual((floor.branch, floor.state.y), ("floor", 655360))

    def test_layer_change_clears_forced_floor_before_comparison(self):
        state = VerticalState(-65536, 0, 0x04000000, 91)
        same = vertical_step(state, 7, 0, 2, 2, 0)
        changed = vertical_step(state, 7, 0, 2, 1, 0)
        self.assertEqual(same.state, VerticalState(0, 0, 0, 0))
        self.assertEqual(changed.state, VerticalState(-65536, 7, 0x1000, 7))

    def test_floor_keeps_upward_velocity_and_selected_terrain_marker(self):
        for terrain, marker in ((0, 0), (0x00400000, 123), (0x00020000, 123), (0x1000, 0)):
            effect = vertical_step(VerticalState(100000, -1, 0xFFFFFFFF, 123), 99, 0, 0, 0, terrain)
            self.assertEqual(effect.state, VerticalState(0, -1, 0xFBBFEFFF, marker))
        effect = vertical_step(VerticalState(1, 1, 0, 123), 99, 0, 0, 0, 0)
        self.assertEqual(effect.state, VerticalState(0, 0, 0, 0))

    def test_position_and_gravity_additions_wrap_before_decisions(self):
        effect = vertical_step(VerticalState(0x7FFF0000, 0x20000, 0, 0), 0x7FFFFFFF, 0, 0, 0, 0)
        self.assertEqual(effect.state, VerticalState(-2147418112, -2147352577, 0x1000, -2147352577))


if __name__ == "__main__":
    unittest.main()
