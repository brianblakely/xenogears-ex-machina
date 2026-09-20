"""Synthetic analysis host tests; original agreement has separate qualified evidence."""

from __future__ import annotations

import copy
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.analysis.execution import (  # noqa: E402
    canonical_digest,
    compare,
    encode_case,
    enrich,
    run,
)

RUNNER = None


def case(code: bytes, entry: str = "event_pass") -> dict:
    actor = bytearray(312)
    for i in range(1, 8):
        struct.pack_into("<I", actor, 0x90 + i * 8, 15 << 18)
    descriptor = bytearray(92)
    struct.pack_into("<I", descriptor, 0x58, 0x200)
    component = bytes(128) + struct.pack("<I", 1) + bytes(64) + code
    return {
        "schema_version": 1,
        "entry": entry,
        "source": {"kind": "synthetic"},
        "input": {
            "actors": [{"actor": actor.hex(), "descriptor": descriptor.hex()}],
            "event_component": component.hex(),
            "event_control": [1, 0, 0, 1, 1, 1, 1, 0],
        },
    }


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.report = {
            "entry": "event_pass",
            "source": {"kind": "synthetic"},
            "status": "dependency_needs_recovery",
            "opcode": 0x75,
            "location": {"actor": 0},
            "checkpoints": [{"operation": "event_instruction", "state": {"value": 5}}],
        }
        self.expected = {
            "schema_version": 1,
            "entry": "event_pass",
            "source": {"kind": "synthetic"},
            "checkpoints": [{"operation": "event_instruction", "state": {"value": 5}}],
            "stop": {"actor_index": 0, "opcode": 0x75},
        }

    def test_independent_expectation_finds_first_divergence(self):
        self.assertEqual(compare(self.report, self.expected)["status"], "matched")
        self.report["checkpoints"][0]["state"]["value"] = 4
        result = compare(self.report, self.expected)
        self.assertEqual(result["status"], "behavioral_divergence")
        self.assertEqual(result["first_difference"]["path"], "checkpoints[0].state.value")
        self.assertEqual(result["matched_checkpoints"], 0)

    def test_missing_extra_changed_identity_and_immutable_start_reject(self):
        self.report["checkpoints"] = []
        self.assertEqual(compare(self.report, self.expected)["status"], "behavioral_divergence")
        self.report["source"] = {"kind": "different"}
        with self.assertRaisesRegex(ValueError, "identity"):
            compare(self.report, self.expected)
        self.report["source"] = self.expected["source"]
        self.expected["case_sha256"] = "different"
        with self.assertRaisesRegex(ValueError, "starting input"):
            compare(self.report, self.expected)

    def test_recovered_but_not_connected_is_distinct(self):
        self.report.update(dependency="instruction:primary:0x71")
        enrich(self.report)
        self.assertEqual(self.report["status"], "dependency_not_connected")
        self.assertIn("EVID-REF-031", self.report["recovery"]["evidence"])

    def test_expected_results_cannot_enter_reconstruction_transport(self):
        source = case(b"\x00")
        encoded = encode_case(source, budget=10)
        source["expected"] = {"result": "pass"}
        with self.assertRaisesRegex(ValueError, "separate"):
            encode_case(source, budget=10)
        source.pop("expected")
        source["input"]["next_pc"] = 5
        with self.assertRaisesRegex(ValueError, "input fields"):
            encode_case(source, budget=10)
        self.assertTrue(encoded.startswith(b"XEMRUN01"))

    def test_malformed_case_values_reject_before_execution(self):
        for value in (-1, True, 1000001):
            with self.subTest(value=value), self.assertRaises(ValueError):
                encode_case(case(b"\x00"), budget=value)
        source = case(b"\x00")
        source["input"]["actors"][0]["actor"] = "00"
        with self.assertRaises(ValueError):
            encode_case(source, budget=1)

    def test_watchdog_is_separate_from_game_time(self):
        source = case(b"\x00")
        with patch(
            "tools.analysis.execution.subprocess.run",
            side_effect=subprocess.TimeoutExpired("runner", 1),
        ):
            report = run(source, Path("unused"), timeout=1)
        self.assertEqual(report["status"], "host_timeout")
        self.assertEqual(report["continuation"], "restart_from_immutable_input")
        self.assertIsNone(report["partial_state"])


class RunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if RUNNER is None:
            raise unittest.SkipTest("Executable checks run in CTest analysis-execution")

    def test_normal_return_and_no_invented_dependency(self):
        report = run(case(b"\x00"), RUNNER)
        self.assertEqual(report["status"], "completed_boundary")
        self.assertEqual(report["completed_entries"], 1)
        self.assertEqual(report["dependency"], "")
        self.assertEqual(report["partial_state"]["event_control"][1], 1)

    def test_computed_variable_then_actual_dependency(self):
        source = case(bytes((0x35, 0, 0, 5, 0, 0x40, 0x75)))
        unchanged = copy.deepcopy(source)
        report = run(source, RUNNER)
        self.assertEqual(source, unchanged)
        self.assertEqual(report["status"], "dependency_needs_recovery")
        self.assertEqual(report["location"]["event_pc"], 6)
        self.assertEqual(report["opcode"], 0x75)
        self.assertEqual(report["partial_state"]["variables"], "0500" + "00" * 2046)
        expected = {
            "schema_version": 1,
            "entry": source["entry"],
            "source": source["source"],
            "case_sha256": canonical_digest(source),
            "checkpoints": [
                {
                    "operation": "event_instruction",
                    "actor": 0,
                    "event_pc": 0,
                    "state": {"variables": "0500" + "00" * 2046},
                }
            ],
            "stop": {"actor_index": 0, "opcode": 0x75},
        }
        self.assertEqual(compare(report, expected)["status"], "matched")
        expected["checkpoints"][0]["state"]["variables"] = "0600" + "00" * 2046
        self.assertEqual(compare(report, expected)["status"], "behavioral_divergence")

    def test_limits_retain_partial_state_and_restart_is_explicit(self):
        source = case(bytes((0x35, 0, 0, 5, 0, 0x40, 0x75)))
        limited = run(source, RUNNER, budget=1)
        self.assertEqual(limited["status"], "host_budget_exhausted")
        self.assertEqual(limited["dependency"], "")
        self.assertEqual(limited["partial_state"]["variables"][:4], "0500")
        self.assertEqual(limited["continuation"], "restart_from_immutable_input")
        complete = run(source, RUNNER)
        self.assertEqual(complete["status"], "dependency_needs_recovery")
        self.assertEqual(complete["partial_state"], limited["partial_state"])

    def test_no_host_work_occurs_with_zero_budget(self):
        report = run(case(b"\x00"), RUNNER, budget=0)
        self.assertEqual(report["status"], "host_budget_exhausted")
        self.assertEqual(report["operations"], 0)
        self.assertEqual(report["checkpoints"], [])

    def test_waits_count_passes_not_frames(self):
        source = case(bytes((0x26, 2, 0x80, 0)))
        for repeats, count in ((1, 2), (2, 1), (3, 0)):
            report = run(source, RUNNER, repeats=repeats)
            actor = bytes.fromhex(report["partial_state"]["actors"][0]["actor"])
            self.assertEqual(actor[0x8E], count)
            self.assertEqual(report["completed_entries"], repeats)

    def test_truncated_transport_and_snapshot_report_invalid_input(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.bin"
            path.write_bytes(b"XEMRUN01\x00")
            result = subprocess.run(
                [str(RUNNER), str(path)], capture_output=True, text=True, check=False
            )
        self.assertEqual(json.loads(result.stdout)["status"], "invalid_input")
        source = case(b"\x00", "field_return_data")
        source["input"]["snapshot"] = "00"
        self.assertEqual(run(source, RUNNER)["status"], "invalid_input")

    def test_invalid_bytecode_access_is_reconstruction_error(self):
        report = run(case(b"\x35"), RUNNER)
        self.assertEqual(report["status"], "reconstruction_error")
        self.assertIn("outside", report["reason"])

    def test_original_safeguard_is_a_normal_return_not_a_host_limit(self):
        source = case(bytes((1, 0, 0)), "event_batch")
        source["input"]["event_control"][0] = 0
        report = run(source, RUNNER)
        self.assertEqual(report["status"], "completed_boundary")
        self.assertEqual(
            report["entry_result"],
            {
                "batch_exit": "safeguard",
                "dispatched": 1025,
                "diagnostic_requested": True,
            },
        )
        report = run(source, RUNNER, budget=10)
        self.assertEqual(report["status"], "host_budget_exhausted")
        self.assertEqual(report["completed_entries"], 0)

    def test_invalid_expectations_preserve_execution_and_do_not_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "case.json"
            expected = Path(directory) / "expected.json"
            report = Path(directory) / "report.json"
            source.write_text(json.dumps(case(b"\x00")))
            expected.write_text("{}")
            command = [
                sys.executable,
                "-m",
                "tools.analysis.execution",
                "run",
                "--case",
                str(source),
                "--expected",
                str(expected),
                "--runner",
                str(RUNNER),
                "--report",
                str(report),
            ]
            root = Path(__file__).resolve().parents[1]
            first = subprocess.run(command, cwd=root, capture_output=True, text=True, check=False)
            self.assertEqual(first.returncode, 1)
            before = report.read_bytes()
            result = json.loads(before)
            self.assertEqual(result["status"], "invalid_input")
            self.assertEqual(result["execution_status"], "completed_boundary")
            self.assertEqual(len(result["checkpoints"]), 1)
            subprocess.run(command, cwd=root, capture_output=True, text=True, check=False)
            self.assertEqual(report.read_bytes(), before)


if __name__ == "__main__":
    RUNNER = Path(sys.argv.pop(1)).resolve()
    unittest.main()
