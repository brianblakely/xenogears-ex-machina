"""Authored connected-execution fixtures, not original-game evidence."""

from __future__ import annotations

import copy
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.analysis import connected  # noqa: E402

BINARY = Path(os.environ["XEM_RECONSTRUCT"]) if "XEM_RECONSTRUCT" in os.environ else None
if __name__ == "__main__" and len(sys.argv) > 1:
    BINARY = Path(sys.argv.pop(1)).resolve()


def artifact(root: Path, name: str, content: bytes) -> dict:
    (root / name).write_bytes(content)
    return {"path": name, "sha256": hashlib.sha256(content).hexdigest()}


def actor_projection(pc: int) -> dict:
    return {"flags": 0, "layer_flags": 0, "pc": pc, "selected_slot": 0,
            "model_and_bounds_flags": 0, "return_pcs": [0] * 4,
            "slots": [[pc, 0, 0, 0]] + [[0, 0, 0, 15 << 18] for _ in range(7)]}


def fixture(root: Path, code: bytes, pcs: list[int]) -> tuple[Path, dict]:
    events = bytes(128) + struct.pack("<I", len(pcs))
    events += b"".join(struct.pack("<32H", *([pc] * 32)) for pc in pcs) + code
    actors = bytearray(0x138 * len(pcs))
    for index, pc in enumerate(pcs):
        base = index * 0x138
        struct.pack_into("<H", actors, base + 0xCC, pc)
        struct.pack_into("<H", actors, base + 0x8C, pc)
        for slot in range(1, 8):
            struct.pack_into("<I", actors, base + 0x90 + slot * 8, 15 << 18)
    case = {"provenance": {"kind": "synthetic", "overlay_sha256": connected.OVERLAY},
            "inputs": {"events": artifact(root, "events.bin", events),
                       "actors": artifact(root, "actors.bin", actors),
                       "variables": artifact(root, "variables.bin", bytes(2048)),
                       "descriptor_flags": [0x100] * len(pcs),
                       "control": [0, 0, 0, 1, 1, 1, 1, 0], "pass": [123, 456],
                       "scheduler": [0, 0, 255, 255, 255],
                       "battle": [1, 0, 0, 0, 99, 8, 1], "music_result": 0,
                       "battle_mode_source": 2},
            "instruction_limit": 1000, "expected": None}
    path = root / "case.json"
    path.write_text(json.dumps(case))
    return path, case


def expected_battle() -> dict:
    # Hand-authored effects: actor 0 writes variable 0, actor 1 consumes it as
    # a battle selector, and its shared gate prevents actor 2's unknown opcode.
    actors = [actor_projection(pc) for pc in (0, 7, 10)]
    actors[0]["pc"] = 6
    actors[0]["slots"][0] = [6, 0, 255, 15 << 18]
    actors[1]["pc"] = 10
    actors[1]["slots"][0][0] = 10
    return {"location": [1, 0, 10],
            "state": {"actors": actors, "descriptor_flags": [0x100] * 3,
                      "variables": [5] + [0] * 1023, "unsigned_bitmap": [0] * 128,
                      "control": [1, 1, 8, 1, 0, 1, 1, 0], "pass": [0, 0],
                      "scheduler": [0, 0, 255, 255, 255],
                      "battle": [0, 0, 0, 1, 5, 2, 0], "music_result": 0,
                      "battle_mode_source": 2}}


class CaseValidation(unittest.TestCase):
    def test_duplicate_json_keys(self):
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            connected.load_json(b'{"value": 1, "value": 2}')

    def test_types_and_complete_comparison(self):
        self.assertIsNotNone(connected.first_difference({"v": 1}, {"v": True}))
        self.assertIsNotNone(connected.first_difference({"v": 1}, {"v": 1, "extra": 0}))
        self.assertEqual(connected.first_difference([1, 2], [1, 3]), "$[1]: value")

    def test_input_hash_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, case = fixture(root, bytes([0]), [0])
            (root / "events.bin").write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                connected.run(path, root / "not-an-executable")

    def test_rejects_defaulted_or_malformed_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, case = fixture(root, bytes([0]), [0])
            for value in (-1, 0x100000000, True):
                invalid = copy.deepcopy(case)
                invalid["inputs"]["music_result"] = value
                with self.assertRaises(ValueError):
                    connected.prepare(invalid, root)
            del case["inputs"]["control"]
            with self.assertRaises(ValueError):
                connected.prepare(case, root)

    def test_expected_output_never_enters_execution_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, case = fixture(root, bytes([0]), [0])
            before = connected.prepare(case, root)
            case["expected"] = {"path": "deliberately-unread.json", "sha256": "not-used"}
            self.assertEqual(before, connected.prepare(case, root))


@unittest.skipUnless(BINARY, "Native integration is run separately by CTest with xem-reconstruct")
class NativeConnection(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def execute(self, code, pcs=(0,), change=None):
        path, case = fixture(self.root, bytes(code), list(pcs))
        if change:
            change(case)
        path.write_text(json.dumps(case))
        return connected.run(path, BINARY)

    def test_shared_state_across_actors_and_battle_gate(self):
        expected = artifact(self.root, "expected.json", json.dumps(expected_battle()).encode())
        report, code = self.execute([0x35, 0, 0, 5, 0, 0x40, 0, 0x71, 0, 0, 3], (0, 7, 10),
                                    lambda case: case.update(expected=expected))
        self.assertEqual(code, 1)  # Matching a boundary does not mean the game runs.
        self.assertEqual(report["comparison"]["status"], "matched-projection")
        stop = report["result"]["stop"]
        self.assertEqual(stop["completed_handlers"], 3)
        self.assertTrue(stop["event_pass_completed"])
        self.assertEqual(stop["dependency"], "integration:field-update-tail")

    def test_wrong_model_or_input_is_not_blessed(self):
        expected = artifact(self.root, "expected.json", json.dumps(expected_battle()).encode())
        report, code = self.execute([0x35, 0, 0, 6, 0, 0x40, 0, 0x71, 0, 0, 3], (0, 7, 10),
                                    lambda case: case.update(expected=expected))
        self.assertEqual(code, 3)
        self.assertEqual(report["comparison"]["status"], "mismatch")
        self.assertEqual((self.root / "expected.json").read_bytes(),
                         json.dumps(expected_battle()).encode())

    def test_unknown_preserves_previous_writes_without_committing_partial_slot(self):
        report, code = self.execute([0x35, 0, 0, 9, 0, 0x40, 3])
        result = report["result"]
        self.assertEqual(code, 1)
        self.assertEqual(result["stop"]["dependency"], "instruction:primary:0x03")
        self.assertEqual(result["stop"]["pc"], 6)
        self.assertEqual(result["state"]["variables"][0], 9)
        self.assertEqual(result["state"]["actors"][0]["slots"][0][0], 0)

    def test_extended_failure_keeps_prefix_increment(self):
        report, code = self.execute([0xFE, 0x03])
        self.assertEqual(code, 1)
        self.assertEqual(report["result"]["stop"]["dependency"], "instruction:extended:0x03")
        self.assertEqual(report["result"]["stop"]["pc"], 1)

    def test_missing_input_is_not_a_guessed_service(self):
        report, code = self.execute(
            [0xFE, 0xA2], change=lambda c: c["inputs"].update(music_result=None)
        )
        self.assertEqual(code, 1)
        self.assertEqual(report["result"]["stop"]["dependency"], "input:music-result")
        self.assertEqual(report["result"]["stop"]["pc"], 1)

    def test_budget_is_not_a_recovery_blocker(self):
        report, code = self.execute([0x35, 0, 0, 9, 0, 0x40, 0],
                                    change=lambda c: c.update(instruction_limit=1))
        self.assertEqual(code, 4)
        self.assertEqual(report["result"]["stop"]["kind"], "budget")
        self.assertEqual(report["result"]["stop"]["dependency"], "")
        self.assertEqual(report["result"]["state"]["variables"][0], 9)

    def test_malformed_operand_is_error_not_missing_game_code(self):
        report, code = self.execute([0x35, 0, 8, 9, 0, 0x40, 0])
        self.assertEqual(code, 2)
        self.assertEqual(report["result"]["stop"]["kind"], "error")
        self.assertEqual(report["result"]["stop"]["dependency"], "")

    def test_known_extended_and_return_handlers_compose(self):
        report, code = self.execute([0xFE, 0x7F, 0])
        self.assertEqual(code, 1)
        self.assertEqual(report["result"]["state"]["actors"][0]["pc"], 2)
        report, code = self.execute([5, 4, 0, 0, 0x36, 0, 0, 0x0D])
        self.assertEqual(code, 1)
        actor = report["result"]["state"]["actors"][0]
        self.assertEqual(actor["pc"], 3)
        self.assertEqual(actor["return_pcs"][0], 3)
        self.assertEqual(report["result"]["state"]["variables"][0], 1)

    def test_transport_truncation_and_trailing_data(self):
        _, case = fixture(self.root, bytes([0]), [0])
        wire = connected.prepare(case, self.root)
        for malformed in (wire[:100], wire + b" 99", b"-1", b"999999999999999999999"):
            result = subprocess.run(
                [str(BINARY)], input=malformed, capture_output=True, check=False
            )
            self.assertEqual(result.returncode, 2)
            self.assertFalse(result.stdout)

    def test_deterministic_reexecution_from_prepared_boundary(self):
        first, _ = self.execute([0x36, 0, 0, 0])
        second, _ = self.execute([0x36, 0, 0, 0])
        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
