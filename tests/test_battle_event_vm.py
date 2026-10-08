"""Invented battle event scripts exercise operand forms, lengths and flow."""

import struct
import unittest

from tools.analysis.battle_event_vm import (
    OPCODES,
    EventError,
    archive_scripts,
    decode,
    disassemble,
    entry_points,
    parse_script,
)


def literal_block(data: bytes) -> bytes:
    """A packed block of literal tokens only (80032eb4's format)."""
    data += bytes(-len(data) % 8)
    groups = b"".join(b"\0" + data[i : i + 8] for i in range(0, len(data), 8))
    return struct.pack("<I", len(data)) + groups + b"\0"


class DecodeTests(unittest.TestCase):
    def test_masked_operands(self):
        insn = decode(bytes([0x06, 0x10, 0x00, 0x05, 0x00, 0x40]), 0)
        self.assertEqual((insn.name, insn.length), ("set", 6))
        self.assertEqual(insn.operands, (("var", "var[10]"), ("value", "#5"), ("mask", "40")))
        insn = decode(bytes([0x09, 0x10, 0x00, 0x22, 0x00, 0x00]), 0)
        self.assertEqual(insn.operands[1], ("value", "var[22]"))

    def test_signed_form_operands(self):
        self.assertEqual(decode(bytes([0x1E, 0x14, 0x80]), 0).operands, (("frames", "#14"),))
        self.assertEqual(decode(bytes([0x1E, 0x21, 0x00]), 0).operands, (("frames", "var[20]"),))

    def test_flow(self):
        self.assertEqual(decode(bytes([0x00]), 0).successors, ())
        self.assertEqual(decode(bytes([0x01, 0x34, 0x12]), 0).successors, (0x1234,))
        branch = bytes([0x02, 1, 0, 2, 0, 0xC0, 0x20, 0])
        self.assertEqual(decode(branch, 0).successors, (8, 0x20))
        self.assertEqual(decode(bytes([0x18, 1, 0, 0]), 0).successors, (4,))

    def test_every_handler_has_a_length(self):
        self.assertEqual(sorted(OPCODES), list(range(0x4C)))
        self.assertTrue(all(op.length >= 1 for op in OPCODES.values()))

    def test_unknown_and_truncated_instructions(self):
        with self.assertRaises(EventError):
            decode(bytes([0x4C]), 0)
        with self.assertRaises(EventError):
            decode(bytes([0x31, 0, 0x80]), 0)
        listing = disassemble(bytes([0x0D, 0, 0, 0x4C]), [0])
        self.assertEqual(listing.errors, ["unknown opcode 0x4c at 0x3"])

    def test_jump_into_an_instruction_is_reported(self):
        code = bytes([0x01, 0x04, 0x00, 0x2B, 0x05, 0x80, 0x00])
        listing = disassemble(code, [0, 3])
        self.assertEqual(len(listing.errors), 1)


class ScriptTests(unittest.TestCase):
    def test_archive_script_threads_and_entries(self):
        code = bytes([0x1B, 0x00, 0x00, 0x22, 0x00])
        threads = struct.pack("<8H", 0, 2, 3, 0, 0, 0, 0, 0)
        script = bytes(0x40) + struct.pack("<I", 1) + threads + code
        archive = struct.pack("<III", 2, 12, 12 + len(literal_block(script)))
        archive += literal_block(script) + literal_block(b"messages")
        ((number, raw),) = list(archive_scripts(archive))
        parsed = parse_script(raw)
        self.assertEqual((number, parsed.threads[0][:3]), (0, (0, 2, 3)))
        self.assertEqual(parsed.code[: len(code)], code)
        listing = disassemble(parsed.code, entry_points(parsed))
        self.assertEqual(listing.errors, [])
        self.assertEqual(
            [i.name for _, i in sorted(listing.instructions.items())],
            ["speaker", "end", "last_pass", "end"],
        )


if __name__ == "__main__":
    unittest.main()
