"""Invented streams distinguish ordinary sprite time from facing-only replay."""

import unittest

from tests.sprite_fixtures import ADDRESS, RESOURCE, forbidden_read, invented_sprite, reader
from tools.analysis.sprite_state import put, u16, u32
from tools.analysis.sprite_vm import (
    SpriteEnvironment,
    UnsupportedSpriteCommand,
    advance_sprite_timer,
    execute_sprite_commands,
)


class SpriteVmTests(unittest.TestCase):
    def setUp(self):
        self.sprite, self.resource, _ = invented_sprite()
        self.start = RESOURCE + 0x1A0
        put(self.sprite, 0x64, self.start)
        self.widths = bytearray(256)
        self.widths[0xB3] = 2
        self.environment = SpriteEnvironment(0, 0, 0)

    def execute(self, **kwargs):
        return execute_sprite_commands(
            self.sprite,
            ADDRESS,
            self.environment,
            self.widths,
            reader(RESOURCE, self.resource),
            forbidden_read,
            **kwargs,
        )

    def advance(self, **kwargs):
        return advance_sprite_timer(
            self.sprite,
            ADDRESS,
            self.environment,
            self.widths,
            reader(RESOURCE, self.resource),
            forbidden_read,
            **kwargs,
        )

    def test_expiring_timer_executes_and_scales_the_new_duration(self):
        self.resource[0x1A0] = 0x33
        put(self.sprite, 0x9E, 1, 2)
        put(self.sprite, 0xAC, 128 << 7)
        out = self.advance()
        self.assertEqual((u16(out.sprite, 0x9E), u32(out.sprite, 0x64)), (2, self.start + 1))
        self.assertEqual(out.commands, 1)
        self.assertEqual(u16(self.sprite, 0x9E), 1)

    def test_zero_negative_and_disabled_timers_preserve_original_width_rules(self):
        for rate, initial, expected in ((0, 0, 0), (0, 0x8000, 0x7FFF), (-1, 1, 1)):
            put(self.sprite, 0x9E, initial, 2)
            out = advance_sprite_timer(
                self.sprite,
                ADDRESS,
                SpriteEnvironment(rate, 1, 17),
                self.widths,
                forbidden_read,
                forbidden_read,
            )
            self.assertEqual((u16(out.sprite, 0x9E), out.commands), (expected, 0))
            self.assertEqual(out.environment.frame_head, 17)

    def test_rate_one_can_expire_two_consecutive_waits_in_one_timer_call(self):
        self.environment = SpriteEnvironment(1, 0, 0)
        self.resource[0x1A0:0x1A2] = bytes((0x30, 0x3F))
        put(self.sprite, 0x9E, 1, 2)
        out = self.advance()
        self.assertEqual((u16(out.sprite, 0x9E), out.commands), (16, 2))
        self.assertEqual(u32(out.sprite, 0x64), self.start + 2)

    def test_jump_and_index_store_feed_actual_frame_lookup_with_saturated_step(self):
        self.resource[0x1A0:0x1A8] = bytes((0xE1, 5, 0, 0xFE, 0x80, 0xB3, 0xFF, 0x13))
        put(self.sprite, 0x54, RESOURCE + 0x160)
        put(self.sprite, 0xA8, 63 << 22)
        out = self.execute()
        self.assertEqual(out.commands, 3)
        self.assertEqual((u16(out.sprite, 0x34), u16(out.sprite, 0x9E)), (1, 4))
        self.assertEqual((u32(out.sprite, 0xA8) >> 11) & 63, 0)
        self.assertEqual((u32(out.sprite, 0xA8) >> 22) & 63, 63)
        self.assertEqual(
            (u32(out.sprite, 0x64), out.environment.frame_head), (self.start + 8, ADDRESS)
        )

    def test_frame_increment_and_decrement_preserve_halfword_wrap(self):
        for opcode, frame, expected in ((0x00, 65535, 0), (0x20, 0, 65535)):
            self.resource[0x1A0] = opcode
            put(self.sprite, 0x34, frame, 2)
            out = self.execute()
            self.assertEqual((u16(out.sprite, 0x34), u16(out.sprite, 0x9E)), (expected, 1))

    def test_uninitialized_duration_is_required_and_signed_scaling_is_explicit(self):
        self.resource[0x1A0] = 0x40
        with self.assertRaisesRegex(ValueError, "incoming S3"):
            self.execute()
        out = self.execute(incoming_duration=-257)
        self.assertEqual(u16(out.sprite, 0x9E), 65536 - 257)
        put(self.sprite, 0xAC, 0)
        self.assertEqual(u16(self.execute(incoming_duration=0).sprite, 0x9E), 1)

    def test_backward_jump_uses_signed_operand_relative_to_current_pc(self):
        self.resource[0x1A0:0x1A6] = bytes((0x32, 0x80, 0x80, 0xE1, 0xFD, 0xFF))
        put(self.sprite, 0x64, self.start + 3)
        out = self.execute()
        self.assertEqual(
            (out.commands, u16(out.sprite, 0x9E), u32(out.sprite, 0x64)), (2, 3, self.start + 1)
        )

    def test_unknown_command_reports_exact_pc_and_never_mutates_caller_state(self):
        self.resource[0x1A0:0x1A3] = bytes((0xB3, 17, 0x90))
        old = bytes(self.sprite)
        with self.assertRaises(UnsupportedSpriteCommand) as caught:
            self.execute()
        self.assertEqual(
            (caught.exception.pointer, caught.exception.opcode), (self.start + 2, 0x90)
        )
        self.assertEqual(self.sprite, old)

    def test_zero_width_and_unreachable_timer_rates_fail_instead_of_yielding(self):
        self.resource[0x1A0:0x1A2] = bytes((0xB3, 17))
        self.widths[0xB3] = 0
        with self.assertRaisesRegex(ValueError, "command execution exceeded"):
            self.execute(inspection_limit=3)
        self.environment = SpriteEnvironment(-2, 0, 0)
        with self.assertRaisesRegex(ValueError, "timer exceeded"):
            self.advance(inspection_limit=3)


if __name__ == "__main__":
    unittest.main()
