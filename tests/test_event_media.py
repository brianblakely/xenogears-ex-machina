"""Authored fixtures for extended PC semantics and music-loading waits."""

import unittest
from dataclasses import replace

from tools.analysis.event_media import enter_extended, execute_music_wait, wait_music_load_extended
from tools.analysis.event_state import ActorScripts, EventSlot, InterpreterControl
from tools.analysis.events import (
    EventError,
    UnknownInstruction,
    decode_instruction,
    disassemble_reachable,
)


def actor(pc=0):
    return ActorScripts(
        tuple(EventSlot(100 + i, i + 1, i, 0xA5C00000 | i) for i in range(8)), 3, pc
    )


class EventMediaTests(unittest.TestCase):
    def test_dispatch_increment_changes_only_working_pc(self):
        before = actor(21)
        self.assertEqual(enter_extended(before), replace(before, pc=22))
        self.assertEqual(enter_extended(actor(65535)).pc, 0)

    def test_pending_retries_prefix_and_preserves_slots_and_budget_mode(self):
        before = actor()
        command = decode_instruction(bytes([0xFE, 0xA2, 0]), 0)
        result = execute_music_wait(command, before, InterpreterControl(0, 0), 0xFFFFFFFF)
        self.assertEqual(result.actor, before)
        self.assertEqual(result.control, InterpreterControl(0, 1))

    def test_only_exact_pending_sentinel_retries(self):
        command = decode_instruction(bytes([0xFE, 0xA2, 0]), 0)
        for status in (0, 1, 2, 0x80000000, 0xFFFFFFFE):
            result = execute_music_wait(command, actor(), InterpreterControl(2, 0), status)
            self.assertEqual(result.actor, replace(actor(), pc=2))
            self.assertEqual(result.control, InterpreterControl(2, 1))
        with self.assertRaises(EventError):
            execute_music_wait(command, actor(), InterpreterControl(1, 0), -1)

    def test_extended_pc_wraps_back_to_prefix(self):
        before = actor(0)
        result = wait_music_load_extended(before, InterpreterControl(1, 0), 0xFFFFFFFF)
        self.assertEqual(result.actor.pc, 65535)
        code = bytes([0xA2, 0]) + bytes(65533) + bytes([0xFE])
        command = decode_instruction(code, 65535)
        self.assertEqual(command.successors, (65535, 1))
        result = execute_music_wait(command, actor(65535), InterpreterControl(1, 0), 0)
        self.assertEqual(result.actor.pc, 1)
        self.assertEqual([x.pc for x in disassemble_reachable(code, 65535)], [1, 65535])

    def test_unknown_extended_opcode_reports_its_namespace_and_pc(self):
        with self.assertRaises(UnknownInstruction) as caught:
            decode_instruction(bytes([0xFE, 0xA3, 0]), 0)
        error = caught.exception
        self.assertEqual((error.namespace, error.pc, error.opcode), ("extended", 1, 0xA3))
        with self.assertRaises(ValueError):
            decode_instruction(bytes([0xFE]), 0)
        with self.assertRaises(EventError):
            execute_music_wait(
                decode_instruction(bytes([0]), 0), actor(), InterpreterControl(1, 0), 0
            )


if __name__ == "__main__":
    unittest.main()
