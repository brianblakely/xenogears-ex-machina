"""Synthetic scripts exercise the recovered overlay script decoders."""

import struct
import unittest

from tools.analysis.overlay_scripts import (
    ARENA,
    BASE,
    WORLDMAP,
    ScriptError,
    UnknownOpcode,
    decode,
    disassemble,
    expected_sha256,
    sweep,
)


def halfwords(*values: int) -> bytes:
    return struct.pack(f"<{len(values)}h", *values)


def image(size: int, **placed: bytes) -> bytearray:
    data = bytearray(size)
    for name, raw in placed.items():
        offset = int(name[2:], 16) - BASE
        data[offset : offset + len(raw)] = raw
    return data


def handler_table() -> bytes:
    return b"".join(
        int(spec.handler[5:], 16).to_bytes(4, "little") for spec in WORLDMAP.opcodes.values()
    )


class WorldmapScriptTests(unittest.TestCase):
    def test_lengths_signed_operands_and_stop(self):
        data = halfwords(1, 10, 2, 0, 13, 0, 3, 4985, -288, 19242, 9, 40, 0, 0)
        listing = disassemble(WORLDMAP, data, BASE)
        self.assertEqual([i.opcode for i in listing], [1, 2, 3, 9, 0])
        self.assertEqual([i.size for i in listing], [4, 8, 8, 4, 4])
        self.assertEqual([i.address - BASE for i in listing], [0, 4, 12, 20, 24])
        self.assertEqual(listing[2].operands, (4985, -288, 19242))
        self.assertEqual(listing[1].text(), "send_actor actor=0, argument=13")
        self.assertEqual(listing[-1].spec.flow, "stop")

    def test_unused_halfwords_are_read_and_shown_when_set(self):
        instruction = decode(WORLDMAP, halfwords(11, 2, 8, 7), BASE)
        self.assertEqual(instruction.text(), "set_fade rate=2, step=8, unused=7")

    def test_opcode_is_the_unsigned_low_halfword_of_the_word(self):
        with self.assertRaises(UnknownOpcode) as caught:
            decode(WORLDMAP, halfwords(-1, 0), BASE)
        self.assertEqual(caught.exception.opcode, 0xFFFF)

    def test_unknown_opcode_misalignment_and_truncation_fail(self):
        with self.assertRaises(UnknownOpcode) as caught:
            disassemble(WORLDMAP, halfwords(1, 5, 12, 0), BASE)
        self.assertEqual((caught.exception.address, caught.exception.opcode), (BASE + 4, 12))
        with self.assertRaises(ScriptError):
            disassemble(WORLDMAP, halfwords(3, 1, 2), BASE)
        with self.assertRaises(ScriptError):
            decode(WORLDMAP, halfwords(0, 0, 0, 0), BASE + 2)

    def test_sweep_checks_the_dispatch_table_and_reads_both_scripts(self):
        first = halfwords(1, 60, 5, 44, 8, 0, 240, 0, 0, 0)
        second = halfwords(12, 0)
        data = image(0x2C0C6, D_8009A3C0=handler_table(), D_8009A758=first, D_8009AC60=second)
        result = sweep(WORLDMAP, bytes(data))
        self.assertEqual((result.scripts, result.instructions), (2, 4))
        self.assertEqual(result.uses, {1: 1, 5: 1, 8: 1, 0: 1})
        self.assertEqual(result.failures[0][:2], ("func_800838E8", 0x8009AC60))
        data[0x8009A3C0 - BASE] ^= 4
        with self.assertRaises(ScriptError):
            sweep(WORLDMAP, bytes(data))


class ArenaScriptTests(unittest.TestCase):
    def test_one_to_three_byte_commands_until_end(self):
        data = bytes([0x02, 0x12, 0x2B, 0x15, 0x07, 0x40, 0x1A, 0x01, 0xFF, 0x00, 0x99])
        listing = disassemble(ARENA, data, BASE)
        self.assertEqual([i.size for i in listing], [1, 2, 3, 1, 2, 1])
        self.assertEqual(listing[2].text(), "show_marker x/2=7, y=64")
        self.assertEqual(listing[4].operands, (255,))
        self.assertEqual(listing[-1].spec.mnemonic, "end")

    def test_values_without_a_case_fail_and_spinning_cases_end_the_listing(self):
        with self.assertRaises(UnknownOpcode) as caught:
            disassemble(ARENA, bytes([0x02, 0x23]), BASE)
        self.assertEqual((caught.exception.address, caught.exception.opcode), (BASE + 1, 0x23))
        for code in (16, 17):
            self.assertEqual(disassemble(ARENA, bytes([code, 0x12]), BASE)[-1].spec.flow, "hang")
        with self.assertRaises(ScriptError):
            disassemble(ARENA, bytes([0x15, 0x07]), BASE)

    def test_sweep_follows_the_scene_table_and_direct_starts(self):
        scene = bytes([0x03, 0x0E, 0x2D, 0x19, 0x00])
        table = struct.pack("<10I", *([0x80090F40] * 9 + [0x80090F48]))
        data = image(0x22E69, D_8009105C=table, D_80090F40=scene, D_80090F48=b"\x24")
        result = sweep(ARENA, bytes(data))
        self.assertEqual((result.scripts, result.instructions), (4, 6))
        self.assertEqual(result.uses, {0: 3, 3: 1, 14: 1, 25: 1})
        self.assertEqual(result.failures[0][:2], ("D_8009105C[9]", 0x80090F48))


class TargetTests(unittest.TestCase):
    def test_overlays_name_their_recovered_images(self):
        for machine in (WORLDMAP, ARENA):
            self.assertRegex(expected_sha256(machine.overlay), "^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
