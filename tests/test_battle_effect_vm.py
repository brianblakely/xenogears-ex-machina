"""Invented effect scripts exercise the effect VM's lengths, flow and containers."""

import struct
import unittest

from tools.analysis.battle_effect_vm import (
    BATTLE,
    MODEL_VIEWER,
    EffectError,
    decode,
    disassemble,
    file_tables,
    script_file,
)


def words(*values):
    return struct.pack(f"<{len(values)}H", *(v & 0xFFFF for v in values))


def command(op, arg=0, *params):
    return words(op | arg << 8, *params)


class DecodeTests(unittest.TestCase):
    def test_parameter_words_follow_the_command(self):
        insn = decode(command(0x1A, 1, 1, 2, 3, 4, 5, 6), 0)
        self.assertEqual((insn.opcode, insn.argument, insn.length), (0x1A, 1, 14))
        self.assertEqual(insn.operands, (1, 2, 3, 4, 5, 6))
        self.assertEqual(insn.successors, (14,))

    def test_jumps_count_bytes_from_the_command_start(self):
        code = command(0x0C) + command(0x32, 0, -2)
        self.assertEqual(decode(code, 2).successors, (0,))
        branch = decode(command(0x35, 0, 8), 0)
        self.assertEqual(branch.successors, (4, 8))

    def test_end_and_returns_stop_the_path(self):
        self.assertEqual(decode(command(0x00), 0).successors, ())
        self.assertEqual(decode(command(0x17), 0).successors, ())
        self.assertEqual(decode(command(0x14, 0xFD, 0x0100), 0).successors, ())
        self.assertEqual(decode(command(0x14, 0xFE, 0x0100), 0).successors, (4,))

    def test_deferred_jumps_need_a_nonzero_argument(self):
        self.assertEqual(decode(command(0x36, 1, 30, 12), 0).successors, (6, 12))
        self.assertEqual(decode(command(0x36, 0, 30, 12), 0).successors, (6,))

    def test_loop_returns_past_its_counter_command(self):
        code = command(0x30, 3, 0) + command(0x0C) + command(0x31, 0, -6) + command(0x00)
        listing = disassemble(code, [0])
        self.assertEqual(listing.errors, [])
        self.assertEqual(listing.instructions[6].successors, (10, 4))
        self.assertEqual(sorted(listing.instructions), [0, 4, 6, 10])

    def test_object_events_are_skipped_as_data(self):
        code = command(0x63, 1, 10) + bytes(6) + command(0x00)
        listing = disassemble(code, [0])
        self.assertEqual(listing.errors, [])
        self.assertEqual(listing.instructions[0].data, (4, 10))
        self.assertEqual(sorted(listing.instructions), [0, 10])

    def test_unknown_opcodes_and_overlaps_are_reported(self):
        self.assertNotIn(0x76, BATTLE)
        with self.assertRaises(EffectError):
            decode(command(0x76), 0)
        reached = disassemble(command(0x0C) + command(0x80), [0])
        self.assertEqual(reached.errors, ["unknown battle opcode 0x80 at 0x2"])
        skipped = command(0x32, 0, 6) + command(0x76) + command(0x00)
        self.assertEqual(disassemble(skipped, [0]).errors, [])
        clash = command(0x35, 0, 6) + command(0x01, 0, 0x32) + command(0x00)
        (error,) = disassemble(clash, [0]).errors  # a jump into a parameter word
        self.assertIn("0x4", error)
        self.assertIn("0x6", error)
        self.assertTrue(disassemble(command(0x01, 0, 1), [0]).errors)  # runs off the end

    def test_dialects_differ_in_parameter_words(self):
        self.assertEqual(decode(command(0x61, 0, 4), 0).length, 4)
        self.assertEqual(decode(command(0x61, 0, 4), 0, "ovl2143").length, 2)
        self.assertEqual(decode(command(0x33, 0, 4), 0, "ovl2143").successors, (4,))
        self.assertEqual(len(MODEL_VIEWER), 0x71)
        with self.assertRaises(EffectError):
            decode(command(0x71, 0, 1), 0, "ovl2143")


class ContainerTests(unittest.TestCase):
    def script_file_bytes(self):
        code = command(0x0C) + command(0x00)
        table = struct.pack("<IIII", 3, 0x10, 0x10, 0)  # animations, a script, an empty id
        return struct.pack("<III", 2, 0x0C, 0) + table + code

    def test_object_script_file(self):
        data = self.script_file_bytes()
        animations, scripts = script_file(data)
        self.assertEqual((animations, scripts), (0x1C, [0x1C, 0]))
        self.assertEqual(sorted(disassemble(data, [0x1C]).instructions), [0x1C, 0x1E])

    def test_enemy_set_model_entries(self):
        inner = self.script_file_bytes()
        header = bytes([2, 0, 0, 0]) + struct.pack("<I", 32 + len(inner))
        entries = struct.pack("<IIB3x", 32, 8, 1) + struct.pack("<IIB3x", 0, 3, 1)
        data = header + entries + inner
        tables = list(file_tables("enemy", data))
        self.assertEqual([(label, table[1]) for label, table in tables], [(".0", [32 + 0x1C, 0])])

    def test_scene_motion_block(self):
        data = bytearray(4 + 0x518)
        struct.pack_into("<i", data, 4 + 0x514, 0x518)
        data += struct.pack("<III", 2, 8, 12) + command(0x00)
        self.assertEqual(
            list(file_tables("scene", bytes(data))), [("", (4 + 0x518 + 8, [4 + 0x518 + 12]))]
        )


if __name__ == "__main__":
    unittest.main()
