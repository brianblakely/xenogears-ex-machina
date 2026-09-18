"""Authored event fixtures test boundaries; original traces remain the oracle."""

import struct
import unittest

from tools.analysis.events import (
    EventError,
    UnknownInstruction,
    Variables,
    branch_matches,
    branch_next_pc,
    branch_operands,
    decode_instruction,
    disassemble_reachable,
)
from tools.analysis.field import FieldError


def branch(left: int, right: int, mode: int):
    return decode_instruction(struct.pack("<BHHBH", 2, left, right, mode, 9) + b"\0\0", 0)


def variables(first: int, second: int, bits: int = 0) -> Variables:
    return Variables(struct.pack("<HH", first, second) + bytes(2044), bytes([bits]) + bytes(127))


class EventAnalysisTests(unittest.TestCase):
    def test_variable_width_signedness_and_byte_references(self):
        state = variables(0xFFFF, 0x8000, 1)
        self.assertEqual([state.read(i) for i in range(4)], [65535, 65535, -32768, -32768])
        for reference in (-1, 2048, True):
            with self.subTest(reference=reference), self.assertRaises(EventError):
                state.read(reference)
        with self.assertRaises(EventError):
            Variables(bytes(2047), bytes(128))

    def test_unsigned_type_bits_cross_byte_and_word_boundaries(self):
        bits = bytearray(128)
        for index in (7, 8, 31, 32, 1023):
            bits[index >> 3] |= 1 << (index & 7)
        state = Variables(b"\xff\xff" * 1024, bytes(bits))
        for index in range(1024):
            self.assertEqual(state.read(index * 2), 65535 if index in (7, 8, 31, 32, 1023) else -1)

    def test_branch_type_coercion_is_order_dependent(self):
        self.assertEqual(
            branch_operands(branch(0, 2, 0), variables(0xFFFF, 0x8000, 1)), (65535, 32768)
        )
        self.assertEqual(
            branch_operands(branch(0, 2, 0), variables(0xFFFF, 0x8000, 2)), (-1, -32768)
        )
        self.assertEqual(branch_operands(branch(0, 0xFFFF, 0x40), variables(1, 0, 1)), (1, 65535))
        self.assertEqual(branch_operands(branch(0, 0xFFFF, 0x40), variables(1, 0)), (1, -1))
        self.assertEqual(branch_operands(branch(0xFFFF, 2, 0x80), variables(0, 1, 2)), (65535, 1))
        self.assertEqual(branch_operands(branch(0xFFFF, 2, 0x80), variables(0, 1)), (-1, 1))
        self.assertEqual(
            branch_operands(branch(0x8000, 0xFFFF, 0xC0), variables(0, 0)), (-32768, -1)
        )

    def test_all_comparison_dispatch_targets(self):
        expected = [False, True, False, True, False, True, True, True, True, True, False]
        self.assertEqual([branch_matches(i, -1, 1) for i in range(11)], expected)
        self.assertTrue(branch_matches(10, 0, 1))
        for index in (0, 4, 5):
            self.assertTrue(branch_matches(index, 2, 2))
        for index in (1, 2, 3, 7):
            self.assertFalse(branch_matches(index, 2, 2))
        with self.assertRaises(EventError):
            branch_matches(11, 0, 0)

    def test_true_falls_through_and_false_uses_absolute_target(self):
        instruction = branch(0, 42, 0x40)
        self.assertEqual(branch_next_pc(instruction, variables(42, 0)), 8)
        self.assertEqual(branch_next_pc(instruction, variables(41, 0)), 9)

    def test_cfg_terminates_cycles_and_follows_both_branches(self):
        self.assertEqual(len(disassemble_reachable(b"\x01\0\0", 0)), 1)
        code = struct.pack("<BHHBH", 2, 1, 2, 0xC0, 9) + b"\0\0"
        self.assertEqual([i.pc for i in disassemble_reachable(code, 0)], [0, 8, 9])
        self.assertEqual(decode_instruction(b"\0", 0).successors, ())

    def test_unknown_and_overlapping_instructions_never_become_noops(self):
        with self.assertRaises(UnknownInstruction) as error:
            disassemble_reachable(b"\x01\x03\0\xff", 0)
        self.assertEqual((error.exception.pc, error.exception.opcode), (3, 0xFF))
        with self.assertRaisesRegex(EventError, "overlapping"):
            disassemble_reachable(b"\x01\x01\0\0", 0)

    def test_connected_request_continuation_cfg(self):
        # Invented route: request -> pending wait -> variable-zero branch, with
        # a motion divisor on one path and real end slots on both paths.
        code = bytes((0x71, 0, 0x80, 0xFE, 0x7F, 0x86, 0x20, 0x80, 14, 0, 0x21, 0, 0x81, 0, 0))
        decoded = disassemble_reachable(code, 0)
        self.assertEqual([i.pc for i in decoded], [0, 3, 5, 10, 13, 14])
        self.assertEqual(decoded[0].successors, (0, 3))
        self.assertEqual((decoded[1].name, decoded[1].successors), ("wait_battle_request", (3, 5)))
        self.assertEqual((decoded[2].operands, decoded[2].successors), ((0x8020, 14), (10, 14)))
        self.assertEqual((decoded[3].name, decoded[3].operands), ("set_motion_divisor", (0x8100,)))
        with self.assertRaisesRegex(EventError, "overlapping"):
            disassemble_reachable(bytes((0x86, 0, 0x80, 1, 0, 0)), 0)
        with self.assertRaisesRegex(EventError, "outside bytecode"):
            decode_instruction(bytes((0x86, 0x20, 0x80, 0xFF, 0xFF, 0)), 0)
        with self.assertRaises(FieldError):
            decode_instruction(bytes((0x86, 0x20, 0x80)), 0)

    def test_battle_wait_prefix_wrap_and_unknown_successor(self):
        code = bytes((0x7F, 0)) + bytes(65533) + bytes((0xFE,))
        wait = decode_instruction(code, 65535)
        self.assertEqual((wait.operands, wait.successors), ((0x7F,), (65535, 1)))
        with self.assertRaises(UnknownInstruction) as error:
            disassemble_reachable(bytes((0xFE, 0x7F, 0xFF)), 0)
        self.assertEqual((error.exception.pc, error.exception.opcode), (2, 0xFF))

    def test_truncation_bad_targets_and_unknown_modes_are_explicit(self):
        for data in (b"", b"\x01", b"\x01\0", b"\x02" + bytes(6)):
            with self.subTest(data=data), self.assertRaises(FieldError):
                decode_instruction(data, 0)
        with self.assertRaisesRegex(EventError, "outside bytecode"):
            decode_instruction(b"\x01\xff\xff", 0)
        for mode in (0x10, 0x20, 0x0B, 0xCF):
            with self.subTest(mode=mode), self.assertRaises(EventError):
                branch(0, 0, mode)
        for pc in (-1, 0x10000, True):
            with self.subTest(pc=pc), self.assertRaises(EventError):
                decode_instruction(b"\0", pc)


if __name__ == "__main__":
    unittest.main()
