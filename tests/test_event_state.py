"""Authored fixtures for original slot scheduling and integer state transitions."""

import struct
import unittest
from dataclasses import replace

from tools.analysis.event_state import (
    ActorScripts,
    EventSlot,
    InterpreterControl,
    correlate_actor_scripts,
    execute,
    read_actor_scripts,
    select_slot,
)
from tools.analysis.events import EventError, Variables, decode_instruction


def actor(priorities=(15,) * 8, *, selected=0, pc=0):
    return ActorScripts(
        tuple(
            EventSlot(10 + i, i, 20 + i, (0xA5C12345 & ~0x3C0000) | (priority << 18))
            for i, priority in enumerate(priorities)
        ),
        selected,
        pc,
    )


def variables(first=0, second=0, bits=0):
    return Variables(struct.pack("<HH", first, second) + bytes(2044), bytes([bits]) + bytes(127))


def instruction(opcode, *operands):
    if opcode in (0, 4):
        source = bytes([opcode])
    elif opcode in (1, 0x26, 0x36, 0x37):
        source = struct.pack("<BH", opcode, *operands)
    else:
        source = struct.pack("<BHHB", opcode, *operands)
    return decode_instruction(source + bytes(32), 0)


class EventStateTests(unittest.TestCase):
    def test_last_minimum_priority_slot_wins_without_other_mutation(self):
        before = actor((8, 5, 5, 7, 15, 2, 2, 15), selected=3)
        self.assertEqual(select_slot(before, 99), replace(before, selected_slot=6))

    def test_all_ended_restarts_only_slot_zero_and_preserves_other_bits(self):
        before = actor(selected=6)
        after = select_slot(before, 99)
        self.assertEqual(
            (after.selected_slot, after.slots[0].priority, after.slots[0].resume_pc), (0, 7, 99)
        )
        self.assertEqual(after.slots[1:], before.slots[1:])
        self.assertEqual(
            after.slots[0].control_bits & ~0x3C0000, before.slots[0].control_bits & ~0x3C0000
        )
        self.assertEqual((after.slots[0].countdown, after.slots[0].event_tag), (0, 20))
        self.assertEqual(after.pc, before.pc)

    def test_end_and_reset_have_different_budget_mode_effects(self):
        before = actor((7, 3, 7, 15, 6, 7, 15, 15), selected=2)
        control = InterpreterControl(0, 0)
        ended = execute(instruction(0), before, control)
        reset = execute(instruction(4), before, control, idle_entry=99)
        self.assertEqual(ended.control, InterpreterControl(1, 1))
        self.assertEqual(reset.control, InterpreterControl(0, 1))
        self.assertEqual(ended.actor.pc, before.pc)
        self.assertEqual(ended.actor.slots[2].priority, 15)
        self.assertEqual(ended.actor.slots[2].event_tag, 255)
        self.assertEqual(ended.actor.slots[0], before.slots[0])
        self.assertEqual([s.resume_pc for s in reset.actor.slots], [99, 11, 99, 13, 14, 99, 16, 17])
        self.assertEqual(reset.actor.slots[1], before.slots[1])
        with self.assertRaises(EventError):
            execute(instruction(4), before, control)

    def test_jump_changes_working_pc_not_the_slot_resume_pc(self):
        before = actor()
        result = execute(instruction(1, 17), before, InterpreterControl(0, 0))
        self.assertEqual(result.actor, replace(before, pc=17))
        self.assertEqual(result.control, InterpreterControl(0, 0))

    def test_wait_loads_then_counts_down_and_always_requests_a_break(self):
        before = actor()
        command = instruction(0x26, 0x8002)
        countdowns, pcs = [], []
        for _ in range(3):
            result = execute(command, before, InterpreterControl(1, 0))
            countdowns.append(result.actor.slots[0].countdown)
            pcs.append(result.actor.pc)
            self.assertEqual(result.control, InterpreterControl(1, 1))
            before = result.actor
        self.assertEqual(countdowns, [2, 1, 0])
        self.assertEqual(pcs, [0, 0, 3])

    def test_wait_byte_truncation_and_lazy_variable_read(self):
        control = InterpreterControl(0, 0)
        for value in (0x8000, 0x8100):
            self.assertEqual(execute(instruction(0x26, value), actor(), control).actor.pc, 3)
        result = execute(instruction(0x26, 0), actor(), control, variables(0xFFFF))
        self.assertEqual(result.actor.slots[0].countdown, 255)
        result = execute(instruction(0x26, 0), result.actor, control)
        self.assertEqual(result.actor.slots[0].countdown, 254)
        with self.assertRaises(EventError):
            execute(instruction(0x26, 0), actor(), control)

    def test_variable_store_and_arithmetic_preserve_type_bits_and_other_values(self):
        values = variables(0xFFFF, 0x8000, 2)
        for opcode, expected in [(0x35, 2), (0x38, 1), (0x39, 0xFFFD)]:
            command = instruction(opcode, 1, 2, 0xFF)
            result = execute(command, actor(), InterpreterControl(1, 0), values)
            self.assertEqual(int.from_bytes(result.variables.values[:2], "little"), expected)
            self.assertEqual(result.variables.values[2:], values.values[2:])
            self.assertEqual(result.variables.unsigned_bits, values.unsigned_bits)
            self.assertEqual(result.actor.pc, 6)
        for opcode, expected in [(0x36, 1), (0x37, 0)]:
            result = execute(instruction(opcode, 1), actor(), InterpreterControl(1, 0), values)
            self.assertEqual(result.variables.read(0), expected)
            self.assertEqual(result.actor.pc, 3)

    def test_variable_sources_and_signed_immediates(self):
        values = variables(1, 0x8000, 2)
        for mode in (0, 0xBF):
            result = execute(
                instruction(0x35, 0, 2, mode), actor(), InterpreterControl(1, 0), values
            )
            self.assertEqual(result.variables.read(0), -32768)
        result = execute(
            instruction(0x38, 0, 0xFFFF, 0x40), actor(), InterpreterControl(1, 0), values
        )
        self.assertEqual(result.variables.read(0), 0)

    def test_pc_wraps_at_the_original_halfword_store(self):
        code = bytes(0xFFFD) + struct.pack("<BH", 0x36, 0)
        command = decode_instruction(code, 0xFFFD)
        result = execute(command, actor(pc=0xFFFD), InterpreterControl(1, 0), variables())
        self.assertEqual(result.actor.pc, 0)

    def test_memory_correlation_preserves_all_unrelated_bytes(self):
        opaque = bytearray(range(256))
        opaque[0xCE] = 3
        parsed = read_actor_scripts(opaque)
        self.assertEqual(correlate_actor_scripts(opaque, parsed), opaque)
        changed = replace(parsed, pc=42, selected_slot=1)
        output = correlate_actor_scripts(opaque, changed)
        self.assertEqual(
            [i for i, (a, b) in enumerate(zip(opaque, output, strict=True)) if a != b],
            [0xCC, 0xCD, 0xCE],
        )
        with self.assertRaises(EventError):
            read_actor_scripts(bytes(0xCF))
        opaque[0xCE] = 8
        with self.assertRaises(EventError):
            read_actor_scripts(opaque)


if __name__ == "__main__":
    unittest.main()
