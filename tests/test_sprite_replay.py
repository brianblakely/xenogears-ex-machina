"""Invented command streams and linked objects exercise recovered replay rules."""

import unittest

from tests.sprite_fixtures import ADDRESS, RESOURCE, forbidden_read, invented_sprite, reader
from tools.analysis.sprite_replay import command_replay, frame_change, lookup_frame, previous_frame
from tools.analysis.sprite_state import put, u16, u32


class SpriteReplayTests(unittest.TestCase):
    def test_frame_insertion_prepends_and_clears_pending_auxiliary_bytes(self):
        sprite, _, _ = invented_sprite()
        put(sprite, 0x40, 0x100004)
        sprite[0x140:0x180] = b"\xaa" * 64
        out, head = frame_change(
            sprite, ADDRESS, 0x10002, 0x80002000, forbidden_read, forbidden_read
        )
        self.assertEqual((head, u32(out, 0xEC)), (ADDRESS, 0x80002000))
        self.assertEqual((u16(out, 0x34), u32(out, 0x40)), (2, 0x20004))
        self.assertEqual(out[0x140:0x180], bytes(64))

    def test_existing_node_traversal_keeps_list_order_and_skips_disabled_metadata(self):
        sprite, _, _ = invented_sprite()
        put(sprite, 0x40, 0xA0000)
        other = bytearray(512)
        put(other, 0x20, 0x800020B4)
        put(other, 0xEC, ADDRESS)
        visits = []
        out, head = frame_change(
            sprite,
            ADDRESS,
            9,
            0x80002000,
            forbidden_read,
            reader(0x80002000, other),
            lambda node, _: visits.append(node),
        )
        self.assertEqual(visits, [0x80002000, ADDRESS])
        self.assertEqual((head, u16(out, 0x34)), (0x80002000, 9))
        self.assertEqual(out[0xEC:0xF0], sprite[0xEC:0xF0])

    def test_stale_queued_flag_inserts_again_and_cyclic_lists_are_rejected(self):
        sprite, _, _ = invented_sprite()
        put(sprite, 0x40, 0x20000)
        other = bytearray(512)
        put(other, 0x20, 0x800020B4)
        out, head = frame_change(
            sprite, ADDRESS, 4, 0x80002000, forbidden_read, reader(0x80002000, other)
        )
        self.assertEqual((head, u32(out, 0xEC)), (ADDRESS, 0x80002000))
        put(other, 0xEC, 0x80002000)
        with self.assertRaisesRegex(ValueError, "cyclic"):
            frame_change(sprite, ADDRESS, 4, 0x80002000, forbidden_read, reader(0x80002000, other))

    def test_non_sprite_mode_only_resets_frame_without_touching_list(self):
        sprite, _, _ = invented_sprite()
        put(sprite, 0x3C, 2)
        put(sprite, 0x34, 100, 2)
        expected = bytearray(sprite)
        put(expected, 0x34, 0, 2)
        out, head = frame_change(sprite, ADDRESS, 22, 99, forbidden_read, forbidden_read)
        self.assertEqual((out, head), (expected, 99))

    def test_previous_frame_metadata_parses_skips_coordinates_and_scaled_halfword(self):
        sprite, resource, _ = invented_sprite()
        sprite[0x140:0x180] = b"\xaa" * 64
        resource[0x30] = 0x81
        resource[0x3A:0x46] = bytes((0x83, 4, 5, 0xF2, 253, 255, 16, 0xC1, 7, 8, 9, 10))
        visits = []
        out = previous_frame(
            sprite,
            ADDRESS,
            1,
            ADDRESS + 0x110,
            reader(RESOURCE, resource),
            lambda pointer, *_: visits.append(pointer),
        )
        self.assertEqual(visits, [RESOURCE + x for x in (0x3A, 0x3D, 0x41, 0x42)])
        self.assertEqual(out[0x150:0x152], b"\xfd\xff")
        self.assertEqual(u16(out, 0x156), 256)
        self.assertEqual(u16(out, 0x14E), 0)
        self.assertEqual(out[0x152:0x156], b"\xaa" * 4)

    def test_previous_frame_range_gate_and_unreconstructed_formats_or_allocation(self):
        sprite, resource, _ = invented_sprite()
        put(resource, 0x20, 0x8003, 2)
        self.assertEqual(
            previous_frame(sprite, ADDRESS, 4, ADDRESS + 0x110, reader(RESOURCE, resource)), sprite
        )
        with self.assertRaisesRegex(ValueError, "frame format"):
            previous_frame(sprite, ADDRESS, 1, ADDRESS + 0x110, reader(RESOURCE, resource))
        put(resource, 0x20, 3, 2)
        resource[0x30], resource[0x3A] = 1, 0xC0
        put(sprite, 0xE8, 0)
        with self.assertRaisesRegex(ValueError, "allocation"):
            previous_frame(sprite, ADDRESS, 1, ADDRESS + 0x110, reader(RESOURCE, resource))

    def test_lookup_masks_index_and_combines_frame_flip_with_orientation_flip(self):
        sprite, resource, _ = invented_sprite()
        put(sprite, 0x54, RESOURCE + 0x100)
        put(sprite, 0xA8, 63 << 11)
        put(sprite, 0xAC, (256 << 7) | 4)
        put(resource, 0x17E, 0x209, 2)
        out, head = lookup_frame(sprite, ADDRESS, 0, reader(RESOURCE, resource), forbidden_read)
        self.assertEqual((u16(out, 0x34), head), (9, ADDRESS))
        self.assertEqual((u32(out, 0xAC) & 12, u32(out, 0x3C) & 8), (12, 0))

    def test_replay_duration_wraps_timer_and_saturates_step_counter_at_sixty_three(self):
        sprite, resource, _ = invented_sprite()
        start = RESOURCE + 0x1A0
        put(sprite, 0x64, start)
        put(sprite, 0xA8, 63 << 22)
        put(sprite, 0x9E, 0xFFF8, 2)
        resource[0x1A0:0x1A3] = bytes((0x30, 0x3F, 0x80))
        out, head = command_replay(
            sprite,
            ADDRESS,
            start + 2,
            63,
            0,
            bytes(256),
            reader(RESOURCE, resource),
            forbidden_read,
        )
        self.assertEqual((u16(out, 0x9E), (u32(out, 0xA8) >> 22) & 63, head), (9, 63, 0))
        self.assertEqual(u32(out, 0x64), start + 2)

    def test_replay_skips_by_source_width_and_handles_increment_decrement_and_explicit_frame(self):
        sprite, resource, _ = invented_sprite()
        start = RESOURCE + 0x1A0
        put(sprite, 0x64, start)
        put(sprite, 0x40, 0x80000)
        put(sprite, 0x34, 1, 2)
        resource[0x1A0:0x1A9] = bytes((0xA0, 99, 88, 0x05, 0x23, 0xBE, 3, 0x32, 0x80))
        widths = bytearray(256)
        widths[0xA0] = widths[0xBE] = 3
        out, head = command_replay(
            sprite, ADDRESS, start + 8, 2, 0, widths, reader(RESOURCE, resource), forbidden_read
        )
        self.assertEqual((u16(out, 0x34), u16(out, 0x9E), head), (3, 17, ADDRESS))
        self.assertEqual((u32(out, 0xAC) & 8, u32(out, 0x3C) & 8), (8, 8))

    def test_replay_frame_index_wrap_and_signed_relative_call_with_packed_return_address(self):
        sprite, resource, _ = invented_sprite()
        start = RESOURCE + 0x1A0
        put(sprite, 0x64, start)
        put(sprite, 0x54, RESOURCE + 0x160)
        resource[0x1A0:0x1A5] = bytes((0xB3, 0xFF, 0x15, 0x80, 0))
        widths = bytearray(256)
        widths[0xB3] = 2
        out, _ = command_replay(
            sprite, ADDRESS, start + 3, 1, 0, widths, reader(RESOURCE, resource), forbidden_read
        )
        self.assertEqual(((u32(out, 0xA8) >> 11) & 63, u16(out, 0x34)), (0, 1))
        put(sprite, 0x64, start + 6)
        resource[0x1A0:0x1A9] = bytes((0x30, 0x80, 0, 0, 0, 0, 0xE2, 0xFA, 0xFF))
        out, _ = command_replay(
            sprite, ADDRESS, start + 1, 1, 0, widths, reader(RESOURCE, resource), forbidden_read
        )
        self.assertEqual(out[0x8C], 13)
        self.assertEqual(out[0x9B:0x9E], ((start + 9) & 0xFFFFFF).to_bytes(3, "little"))
        self.assertEqual(u16(out, 0x9E), 1)

    def test_replay_stops_and_unresolved_duration_or_width_are_explicit(self):
        sprite, resource, _ = invented_sprite()
        start = RESOURCE + 0x1A0
        put(sprite, 0x64, start)
        for opcode in (0x80, 0x81, 0x82, 0x86, 0x87, 0x97):
            resource[0x1A0] = opcode
            out, head = command_replay(
                sprite,
                ADDRESS,
                start,
                1,
                123,
                bytes(256),
                reader(RESOURCE, resource),
                forbidden_read,
            )
            self.assertEqual((out, head), (sprite, 123))
        resource[0x1A0] = 0x40
        with self.assertRaisesRegex(ValueError, "incoming S3"):
            command_replay(
                sprite,
                ADDRESS,
                start + 1,
                1,
                0,
                bytes(256),
                reader(RESOURCE, resource),
                forbidden_read,
            )
        resource[0x1A0] = 0x83
        with self.assertRaisesRegex(ValueError, "zero-width"):
            command_replay(
                sprite,
                ADDRESS,
                start + 1,
                1,
                0,
                bytes(256),
                reader(RESOURCE, resource),
                forbidden_read,
            )

    def test_replay_reuses_incoming_or_preceding_duration_for_high_frame_commands(self):
        sprite, resource, _ = invented_sprite()
        start = RESOURCE + 0x1A0
        put(sprite, 0x64, start)
        resource[0x1A0:0x1A4] = bytes((0x40, 0x32, 0x7F, 0x80))
        out, _ = command_replay(
            sprite,
            ADDRESS,
            start + 3,
            3,
            0,
            bytes(256),
            reader(RESOURCE, resource),
            forbidden_read,
            initial_duration=-2,
        )
        self.assertEqual(u16(out, 0x9E), 4)
        self.assertEqual((u32(out, 0xA8) >> 22) & 63, 3)

    def test_packed_return_stack_reloads_aliased_cursor_and_command_pointer(self):
        sprite, resource, _ = invented_sprite()
        start = RESOURCE + 0x1A0
        put(sprite, 0x64, start)
        put(sprite, 0x8C, 1, 1)
        resource[0x1A0:0x1A7] = bytes((0xE2, 3, 0, 0x80, 0, 0, 0x80))
        out, _ = command_replay(
            sprite, ADDRESS, start + 3, 0, 0, bytes(256), reader(RESOURCE, resource), forbidden_read
        )
        self.assertEqual(out[0x8C], 0xA3)
        self.assertEqual(out[0x32:0x34], b"\x01\x01")
        put(sprite, 0x8C, 0xD9, 1)
        out, _ = command_replay(
            sprite, ADDRESS, start + 6, 0, 0, bytes(256), reader(RESOURCE, resource), forbidden_read
        )
        self.assertEqual(u32(out, 0x64), start + 6)


if __name__ == "__main__":
    unittest.main()
