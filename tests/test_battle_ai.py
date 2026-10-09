"""Invented enemy AI scripts exercise decoding, operand layouts and rule flow."""

import contextlib
import io
import json
import struct
import tempfile
import unittest
from pathlib import Path

from tools.analysis import battle_ai as ai


def ins(*values):
    return bytes(values) + bytes(4 - len(values))


def table(*offsets):
    return struct.pack("<4H", *offsets)


def enemy_file(block0, other=None):
    """A header for ids 0-7 (ids 1-7 share `other`), then the name table."""
    other = other if other is not None else table(8, 0xFFFF, 0xFFFF, 0xFFFF) + ins(0xFD)
    first = 0x32
    second = first + len(block0)
    names = second + len(other)
    header = struct.pack("<25H", first, *[second] * 7, *[0] * 16, names)
    return header + block0 + other + b"\x00\x00\x00\x00"


class OpcodeTableTests(unittest.TestCase):
    def test_tables_follow_the_two_dispatch_switches(self):
        actions = [op for op in range(0x01, 0x75) if op not in (0x6E, 0x6F)]
        self.assertEqual(sorted(ai.ACTIONS), actions)
        self.assertEqual(sorted(ai.CONDITIONS), list(range(0x80, 0x9C)))
        for op in (*ai.ACTIONS.values(), *ai.CONDITIONS.values()):
            with self.subTest(op=op.mnemonic):
                self.assertTrue(op.handler and op.effect)
                positions = [p for _, used in op.operands for p in used]
                self.assertEqual(len(positions), len(set(positions)))
                self.assertTrue(all(1 <= p <= 3 for p in positions))
                self.assertTrue(all(kind in ai.FORMATS for kind, _ in op.operands))

    def test_every_byte_value_has_a_runner_class(self):
        expected = {
            0x00: "unknown-action",
            0x01: "action",
            0x62: "action",
            0x6E: "unknown-action",
            0x6F: "unknown-action",
            0x74: "action",
            0x75: "unknown-action",
            0x7F: "unknown-action",
            0x80: "condition",
            0x99: "condition",
            0x9B: "condition",
            0x9C: "unknown-condition",
            0xFC: "unknown-condition",
            0xFD: "end",
            0xFE: "unknown-condition",
            0xFF: "end",
        }
        for opcode, kind in expected.items():
            self.assertEqual(ai.lookup(opcode)[0], kind, hex(opcode))
        self.assertEqual({ai.lookup(op)[0] for op in range(256)}, set(expected.values()))


class DecodeTests(unittest.TestCase):
    def test_operands_come_from_the_handler_layouts(self):
        cases = {
            ins(0x05, 1, 0x34, 0x12): "set_v v1, 0x1234",
            ins(0x01, 0, 0x0E): "list_set +0, 0x0e",
            ins(0x52, 6, 1): "list_set_v +6, v1",
            ins(0x18, 1, 2, 3): "add_bb b1, b2, b3",
            ins(0x39, 0x34, 0x12): "set_experience 0x1234",
            ins(0x2B, 5, 2, 0): "put_attr8 b5, attr2, v0",
            ins(0x5C, 2, 3, 4): "pick_party_attr attr2, v3, v4",
            ins(0x82, 6, 0, 1): "eq_v v6, 0x0100",
            ins(0x80, 9, 9, 9): "always",
            ins(0x62): "mark",
            ins(0x6E, 1, 2, 3): "raw 0x01, 0x02, 0x03",
            ins(0xFE, 1): "raw_cond 0x01, 0x00, 0x00",
            ins(0xFD, 0xFF, 0xFF, 0xFF): "end",
        }
        for raw, text in cases.items():
            self.assertEqual(ai.decode(raw, 0).text(), text)

    def test_instructions_are_four_bytes(self):
        code = ins(0x04, 1, 2) + ins(0xFD)
        self.assertEqual([i.pc for i in ai.analyse_script(code, 0, 8).instructions], [0, 4])
        with self.assertRaises(ValueError):
            ai.decode(b"\x01\x02\x03", 0)
        with self.assertRaises(ValueError):
            ai.analyse_script(code + b"\xfd", 0, 9)


class FlowTests(unittest.TestCase):
    def test_false_rule_skips_its_actions_and_closing_end(self):
        code = (
            ins(0x81, 0, 1) + ins(0x04, 1, 2) + ins(0xFD)
            + ins(0x81, 0, 2) + ins(0x04, 1, 3) + ins(0xFD)
            + ins(0x80) + ins(0x04, 1, 4) + ins(0xFD)
            + ins(0xFF)
        )  # fmt: skip
        script = ai.analyse_script(code, 0, len(code))
        self.assertEqual(script.branches, {0: 12, 12: 24, 24: None})
        self.assertEqual(sorted(script.reachable), list(range(0, 36, 4)))
        self.assertEqual((script.escapes, script.runs_off), ([], []))

    def test_consecutive_conditions_must_all_hold(self):
        code = ins(0x81, 0, 1) + ins(0x83, 1, 2) + ins(0x04) + ins(0xFD) + ins(0x80) + ins(0xFD)
        script = ai.analyse_script(code, 0, len(code))
        self.assertEqual(script.branches, {0: 16, 4: 16, 16: None})

    def test_or_chain_takes_conditions_up_to_the_next_action(self):
        code = ins(0x99) + ins(0x81) + ins(0x83) + ins(0x04) + ins(0xFD) + ins(0x80) + ins(0xFD)
        script = ai.analyse_script(code, 0, len(code))
        self.assertEqual(script.branches, {0: 20, 20: None})
        self.assertEqual(script.chained, {4, 8})
        always = ins(0x99) + ins(0x81) + ins(0x9A) + ins(0x04) + ins(0xFD)
        self.assertEqual(ai.analyse_script(always, 0, len(always)).branches, {0: None})

    def test_or_chain_consumes_end_opcodes(self):
        code = ins(0x99) + ins(0x81) + ins(0xFD) + ins(0x04) + ins(0xFD)
        script = ai.analyse_script(code, 0, len(code))
        self.assertEqual(script.ends_in_chain, [8])
        self.assertEqual(script.branches, {0: 20})
        self.assertEqual(script.escapes, [(0, 20)])

    def test_failed_last_rule_runs_past_the_terminator(self):
        code = ins(0x85, 2, 0x20) + ins(0x04) + ins(0xFD) + ins(0xFF)
        script = ai.analyse_script(code + ins(0x08) + ins(0x80), 0, 16)
        self.assertEqual(script.escapes, [(0, 20)])
        self.assertEqual(sorted(script.reachable), [0, 4, 8])

    def test_flow_off_the_end_without_terminator(self):
        script = ai.analyse_script(ins(0x04) + ins(0x80), 0, 8)
        self.assertEqual(script.runs_off, [4])


class EnemyFileTests(unittest.TestCase):
    def test_script_table_roles_and_regions(self):
        block = table(8, 0xFFFF, 0x10, 0x10) + ins(0x80) + ins(0xFD) + ins(0xFD)
        blocks = ai.enemy_blocks(enemy_file(block))
        self.assertEqual(len(blocks), 8)
        scripts = [(s.roles, s.entry - 0x32, s.end - 0x32) for s in blocks[0].scripts]
        self.assertEqual(scripts, [(["turn"], 8, 0x10), (["reaction", "targeted"], 0x10, 0x14)])
        self.assertEqual(blocks[1].offset, blocks[7].offset)

    def test_table_pointing_into_itself_is_not_a_script_table(self):
        placeholder = table(0, 0, 0xBB, 0) + bytes(0xC0)
        blocks = ai.enemy_blocks(
            enemy_file(table(8, 0xFFFF, 0xFFFF, 0xFFFF) + ins(0xFD), placeholder)
        )
        self.assertIsNone(blocks[0].malformed)
        self.assertIn("0x0", blocks[1].malformed)
        self.assertEqual(blocks[1].scripts, [])

    def test_header_ids_beyond_seven_are_rejected(self):
        data = bytearray(enemy_file(table(8, 0xFFFF, 0xFFFF, 0xFFFF) + ins(0xFD)))
        data[16:18] = b"\x01\x00"
        with self.assertRaises(ValueError):
            ai.enemy_blocks(bytes(data))


class SweepTests(unittest.TestCase):
    def build(self, root: Path, files: dict[int, bytes]) -> None:
        directory = 5  # slot of the directory (12, 1) entry
        for disc in (1, 2):
            sectors = bytearray(41 * ai.SECTOR)
            entries = [0xFFFF] * 61
            entries[13] = directory + 1
            start = 40 * ai.SECTOR + ai.USER
            sectors[start : start + 0x7A] = struct.pack("<61H", *entries)
            (root / "discs").mkdir(exist_ok=True)
            (root / "discs" / f"disc{disc}.bin").write_bytes(sectors)
            extract = root / "extract" / f"disc{disc}"
            (extract / "files").mkdir(parents=True)
            manifest = [{"slot": directory, "lba": 0, "size": -4}]
            for n, data in files.items():
                for slot, content in (
                    (2 * n + 2 + directory - 1, data),
                    (2 * n + 3 + directory - 1, b"m"),
                ):
                    manifest.append({"slot": slot, "lba": 0, "size": len(content)})
                    (extract / "files" / f"{slot:04d}.bin").write_bytes(content)
            (extract / "manifest.json").write_text(json.dumps({"files": manifest}))

    def test_sweep_reports_aggregates_and_unknown_locations(self):
        clean = enemy_file(
            table(8, 0xFFFF, 0xFFFF, 0xFFFF) + ins(0x80) + ins(0x01, 0, 4) + ins(0xFD)
        )
        unknown = enemy_file(table(8, 0xFFFF, 0xFFFF, 0xFFFF) + ins(0x80) + ins(0x6E) + ins(0xFD))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.build(root, {0: clean, 1: unknown})
            results = ai.sweep(root)
            lines = ai.report(results)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = ai.main(["--sweep", "--root", str(root)])
        self.assertEqual(status, 1)
        self.assertEqual([t.files for _, t in results], [2, 2])
        self.assertEqual(results[0][1].scripts["turn"], 16)
        self.assertIn("  unknown opcodes: 1", lines)
        self.assertTrue(any("battle 1 id 0" in line and "6e" in line for line in lines))
        self.assertIn("enemy files identical on both discs: yes", lines)
        self.assertTrue(any(line.startswith("  04 event ") for line in lines))


if __name__ == "__main__":
    unittest.main()
