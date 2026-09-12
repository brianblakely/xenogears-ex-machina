"""Invented checkpoints exercise source ordering, ownership and failed readiness."""

import unittest

from tests.sprite_fixtures import ADDRESS, RESOURCE, forbidden_read, invented_sprite, reader
from tools.analysis.sprite_return import restore_sprite_checkpoint, sprite_return_policy
from tools.analysis.sprite_state import put, u16, u32
from tools.analysis.sprite_vm import SpriteEnvironment


class SpriteReturnTests(unittest.TestCase):
    def setUp(self):
        self.sprite, self.resource, self.trig = invented_sprite()
        put(self.sprite, 0x7C, ADDRESS + 0xF4)
        self.checkpoint = bytearray(48)
        for offset, value in (
            (0, 0x12345678),
            (4, 0xFFFFFFF0),
            (8, 0x11223344),
            (0x1C, 0xDEADBEEF),
            (0x20, 0x01234567),
        ):
            put(self.checkpoint, offset, value)
        for offset in (0x24, 0x26, 0x28, 0x2A, 0x2C):
            put(self.checkpoint, offset, 4096, 2)
        self.environment = SpriteEnvironment(5, 0, 0)

    def restore(self, **kwargs):
        return restore_sprite_checkpoint(
            self.sprite,
            ADDRESS,
            self.checkpoint,
            self.environment,
            bytes(256),
            reader(RESOURCE, self.resource),
            forbidden_read,
            self.trig,
            **kwargs,
        )

    def test_skipped_actor_still_updates_animation_and_consumes_both_extensions(self):
        actor = bytearray(312)
        put(actor, 4, 0x1000000)
        put(actor, 0xEA, 0xFFFE, 2)
        put(actor, 0x134, 0x80)
        put(actor, 0x12C, 0x1000)
        old = bytes(self.checkpoint)
        decision = sprite_return_policy(actor, self.checkpoint, (0, 1, 2), (0, 1, 2), 0)
        self.assertFalse(decision.restore)
        self.assertEqual((u16(decision.checkpoint, 0x14), decision.record_bytes), (0xFFFE, 400))
        self.assertEqual(self.checkpoint, old)

    def test_party_mode_gate_compares_saved_words_to_current_bytes(self):
        actor = bytearray(312)
        put(actor, 0, 0x600)
        for gate, saved, current, expected in (
            (1, 256, 0, False),
            (1, 0, 256, True),
            (0, 256, 0, True),
        ):
            result = sprite_return_policy(
                actor, self.checkpoint, (saved, 2, 3), (current, 2, 3), gate
            )
            self.assertEqual(result.restore, expected)
        put(actor, 0, 0)
        self.assertTrue(
            sprite_return_policy(actor, self.checkpoint, (1, 2, 3), (0, 2, 3), 1).restore
        )

    def test_actor_sentinel_and_255_animation_each_preserve_saved_animation(self):
        actor = bytearray(312)
        put(self.checkpoint, 0x14, 7, 2)
        for marker, animation in ((65535, 9), (0, 255)):
            put(actor, 0x124, marker, 2)
            put(actor, 0xEA, animation, 2)
            result = sprite_return_policy(actor, self.checkpoint, (0, 0, 0), (0, 0, 0), 0)
            self.assertEqual(u16(result.checkpoint, 0x14), 7)

    def test_connected_replay_uses_temporary_rate_then_restores_positions_and_timers(self):
        self.resource[0x180] = 0x13
        put(self.checkpoint, 0x18, 1, 2)
        old, saved = bytes(self.sprite), bytes(self.checkpoint)
        out = self.restore()
        self.assertEqual(out.commands, 1)
        self.assertEqual(out.environment, SpriteEnvironment(5, 0, ADDRESS))
        self.assertEqual(out.sprite[:12], self.checkpoint[:12])
        self.assertEqual(out.sprite[0xF4:0xFC], self.checkpoint[0x1C:0x24])
        self.assertEqual((u16(out.sprite, 0x9E), u32(out.sprite, 0x10)), (4, 16384))
        self.assertEqual((u32(out.sprite, 0xA8) >> 22) & 63, 1)
        self.assertEqual((self.sprite, self.checkpoint), (old, saved))

    def test_zero_target_does_not_execute_unsupported_first_command(self):
        # The fixture's first command is 80. Selection resets the step to zero;
        # the original return helper reaches its target without a VM call.
        out = self.restore()
        self.assertEqual(out.commands, 0)
        self.assertEqual(out.environment, self.environment)
        self.assertEqual(u16(out.sprite, 0x9E), 1)

    def test_animation_is_loaded_after_aliased_renderer_stores(self):
        # The second renderer store aliases AF and changes the signed animation.
        # Reading the checkpoint byte again would incorrectly select animation 0.
        put(self.sprite, 0x20, ADDRESS + 0xA7)
        put(self.checkpoint, 0x26, 255, 2)
        with self.assertRaisesRegex(ValueError, "negative animation"):
            self.restore()

    def test_bad_sizes_unreachable_step_and_negative_animation_fail_explicitly(self):
        for target in (64, 65535):
            put(self.checkpoint, 0x18, target, 2)
            with self.assertRaisesRegex(ValueError, "unreachable six-bit step"):
                self.restore()
        put(self.checkpoint, 0x18, 0, 2)
        self.checkpoint[0x14] = 255
        with self.assertRaisesRegex(ValueError, "negative animation"):
            self.restore()
        with self.assertRaisesRegex(ValueError, "actor or checkpoint"):
            sprite_return_policy(bytes(311), self.checkpoint, (0, 0, 0), (0, 0, 0), 0)
        with self.assertRaisesRegex(ValueError, "three saved/current"):
            sprite_return_policy(bytes(312), self.checkpoint, (), (0, 0, 0), 0)


if __name__ == "__main__":
    unittest.main()
