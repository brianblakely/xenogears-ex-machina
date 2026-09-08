"""Authored animation headers and bitfields test source-visible branch boundaries."""

import struct
import unittest

from tests.sprite_fixtures import ADDRESS, HEADER, RESOURCE, forbidden_read, invented_sprite, reader
from tools.analysis.sprite_animation import (
    apply_header,
    bind_current,
    field_animation,
    select_animation,
    select_orientation,
)
from tools.analysis.sprite_state import put, read_exact, u16, u32


class SpriteAnimationTests(unittest.TestCase):
    def test_header_reset_preserves_facing_group_and_clears_required_storage(self):
        sprite, _, table = invented_sprite()
        put(sprite, 0xA8, 0xFFFFFFFF)
        put(sprite, 0x7C, ADDRESS + 0xF4)
        sprite[0xF4:0x102] = b"\xaa" * 14
        sprite[0x140:0x180] = b"\xbb" * 64
        struct.pack_into("<4i", sprite, 0x0C, 1, -2, 3, 4)
        struct.pack_into("<3h", sprite, 0xB4, 3, 4, 5)
        out = apply_header(sprite, ADDRESS, HEADER, (0x42, 20, 30), 0, 0, table)
        self.assertEqual((u32(out, 0x64), u32(out, 0x54)), (HEADER + 22, HEADER + 34))
        self.assertEqual((u32(out, 0xA8) >> 17) & 7, 7)
        self.assertEqual((u32(out, 0xA8) >> 20) & 3, 2)
        self.assertEqual(out[0x0C:0x1C], bytes(16))
        self.assertEqual(out[0xB4:0xBA], bytes(6))
        self.assertEqual(out[0x140:0x180], bytes(64))
        self.assertEqual(out[0xF4:0xFC], bytes(8))
        self.assertEqual(out[0xFC:0x100], b"\xaa" * 4)
        self.assertEqual((out[0x8C], u16(out, 0x30), u16(out, 0x9E)), (16, 0, 1))

    def test_header_preservation_bits_skip_motion_rotation_and_pending_auxiliary_clear(self):
        sprite, _, table = invented_sprite()
        put(sprite, 0x40, 0x100000)
        sprite[0x0C:0x1C] = bytes(range(16))
        sprite[0xB4:0xBA] = bytes(range(6))
        sprite[0x140:0x180] = b"\xdd" * 64
        out = apply_header(sprite, ADDRESS, HEADER, (0x1842, 0, 0), 0, 0, table)
        for lo, hi in ((0x0C, 0x1C), (0xB4, 0xBA), (0x140, 0x180)):
            self.assertEqual(out[lo:hi], sprite[lo:hi])

    def test_mode_zero_flip_boundary_and_null_resource_early_return(self):
        sprite, resource, _ = invented_sprite()
        put(sprite, 0xA8, 0)
        for angle, flip in ((1024, 0), (1025, 4), (-1024, 0)):
            out = select_orientation(sprite, angle, reader(RESOURCE, resource))
            self.assertEqual(u32(out, 0xAC) & 4, flip)
            self.assertEqual(u32(out, 0x5C), HEADER + 6)
        put(sprite, 0x48, 0)
        put(sprite, 0x3C, 0x12345678)
        out = select_orientation(sprite, 0x10001, forbidden_read)
        self.assertEqual(u16(out, 0x80), 1)
        self.assertEqual(u32(out, 0xAC) & 4, 4)
        self.assertEqual(u32(out, 0x3C), 0x12345678)

    def test_mode_one_mirrors_group_three_through_group_one_frames(self):
        sprite, resource, _ = invented_sprite()
        put(sprite, 0xA8, (1 << 20) | (3 << 17))
        out = select_orientation(sprite, 1536, reader(RESOURCE, resource))
        self.assertEqual((u32(out, 0xA8) >> 17) & 7, 3)
        self.assertEqual(u32(out, 0x54), RESOURCE + 0x168)
        self.assertEqual(u32(out, 0xAC) & 4, 4)

    def test_mode_two_mirrored_mapping_and_replay_timer_restoration(self):
        sprite, resource, _ = invented_sprite()
        put(sprite, 0xA8, (2 << 20) | (0 << 17) | (13 << 22))
        put(sprite, 0x9E, 0xFFFD, 2)
        put(sprite, 0x64, RESOURCE + 0x190)
        calls = []

        def replay(out, target, step):
            calls.append((out, target, step))
            result = bytearray(out)
            put(result, 0x9E, 1000, 2)
            put(result, 0x34, 77, 2)
            return bytes(result)

        out = select_orientation(sprite, 1280, reader(RESOURCE, resource), replay)
        self.assertEqual(len(calls), 1)
        initial, target, step = calls[0]
        self.assertEqual((target, step), (RESOURCE + 0x190, 13))
        self.assertEqual(u32(initial, 0x64), RESOURCE + 0x180)
        self.assertEqual((u32(initial, 0xA8) >> 22) & 63, 0)
        self.assertEqual((u32(initial, 0xA8) >> 11) & 63, 63)
        self.assertEqual((u32(out, 0xA8) >> 17) & 7, 3)
        self.assertEqual(u32(out, 0x54), RESOURCE + 0x178)
        self.assertEqual((u16(out, 0x9E), u16(out, 0x34)), (0xFFFD, 77))

    def test_selection_uses_original_relative_directory_and_marks_pending_frame_clear(self):
        sprite, resource, table = invented_sprite()
        out = select_animation(sprite, ADDRESS, 0, 0, 0, reader(RESOURCE, resource), table)
        self.assertEqual(u32(out, 0x58), HEADER)
        self.assertEqual(u32(out, 0x64), RESOURCE + 0x180)
        self.assertEqual(u32(out, 0x40) & 0x100000, 0x100000)
        self.assertEqual((out[0xAF], u16(out, 0x9E)), (0, 1))
        put(sprite, 0x120, RESOURCE + 0x82)
        with self.assertRaisesRegex(ValueError, "directory pointer"):
            select_animation(sprite, ADDRESS, 0, 0, 0, reader(RESOURCE, resource), table)

    def test_field_flags_gate_dispatch_but_still_clear_jump_state(self):
        sprite, _, table = invented_sprite()
        actor, descriptor = bytearray(312), bytearray(92)
        put(actor, 0, 0x12340800)
        put(actor, 4, 0x1000000)
        put(descriptor, 0x58, 0x40, 2)
        out, state = field_animation(
            sprite, ADDRESS, 255, descriptor, actor, 1, 0, 0, 0, forbidden_read, table
        )
        self.assertEqual(out, sprite)
        self.assertEqual(u32(state), 0x12340800)
        _, state = field_animation(
            sprite, ADDRESS, 2, descriptor, actor, 0, 2, 0, 0, forbidden_read, table
        )
        self.assertEqual(u32(state), 0x12340000)
        put(actor, 4, 0x2000)
        with self.assertRaisesRegex(ValueError, "model animation"):
            field_animation(
                sprite, ADDRESS, 2, descriptor, actor, 0, 2, 0, 0, forbidden_read, table
            )

    def test_unknown_binding_replay_platform_and_truncated_reads_fail_explicitly(self):
        sprite, resource, table = invented_sprite()
        self.assertEqual(bind_current(sprite, 0, 0), sprite)
        with self.assertRaisesRegex(ValueError, "rebinding"):
            bind_current(sprite, RESOURCE + 4, 0)
        with self.assertRaisesRegex(ValueError, "platform"):
            apply_header(sprite, ADDRESS, HEADER, (0x42, 0, 0), 0, 1, table)
        with self.assertRaisesRegex(ValueError, "replay required"):
            select_orientation(sprite, 1280, reader(RESOURCE, resource))
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            read_exact(lambda pointer, size: b"", 10, 2)


if __name__ == "__main__":
    unittest.main()
