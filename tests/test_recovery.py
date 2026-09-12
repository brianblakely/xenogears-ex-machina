"""Inventory faults cannot disappear, become no-ops or grant full coverage."""

from __future__ import annotations

import copy
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.repository.recovery import ROOT, diagnose, validate


def fixture():
    """Invented dispatch targets and gaps; no original game bytes are fixtures."""

    def table(namespace, opcodes):
        values = [0x80010000] * len(opcodes)  # Aliases must retain independent statuses.
        return {
            "namespace": namespace,
            "table_sha256" if namespace == "sprite" else "sha256": hashlib.sha256(
                struct.pack(f"<{len(values)}I", *values)
            ).hexdigest(),
            "rows": [
                {
                    "opcode": f"0x{opcode:02x}",
                    "handler": f"0x{target:08x}",
                    "analysis_status": "unresolved",
                    "native_status": "unimplemented",
                }
                for opcode, target in zip(opcodes, values, strict=True)
            ],
        }

    return {
        "schema_version": 1,
        "coverage_complete": False,
        "scope": "Invented inventory for failure-boundary tests",
        "reconstruction_status_rule": "Observed subsets do not establish complete behavior",
        "unknown_instruction_policy": "Unknown instructions must fail explicitly",
        "unresolved_symbols": [],
        "unknown_formats": [],
        "unverified_behavior": [],
        "event_dispatch_tables": [table("primary", range(256)), table("extended", range(256))],
        "sprite_command_dispatch": table("sprite", range(0x8A, 0xFD)),
    }


class RecoveryInventoryTests(unittest.TestCase):
    def setUp(self):
        self.data = fixture()

    def test_injected_symbol_format_and_instruction_block_the_declared_coverage(self):
        self.data["unresolved_symbols"].append(
            {"id": "invented-loader", "next_experiment": "Trace the original loader return"}
        )
        self.data["unknown_formats"].append(
            {"id": "invented-record", "next_experiment": "Measure the original record bounds"}
        )
        expected = ["symbol:invented-loader", "format:invented-record", "instruction:primary:0x03"]
        old = copy.deepcopy(self.data)
        result = diagnose(self.data, expected, "TEST-INVENTED-COVERAGE")
        self.assertEqual(result["status"], "blocked")
        self.assertEqual([b["dependency"] for b in result["blockers"]], expected)
        self.assertTrue(
            all(b["blocked_coverage"] == "TEST-INVENTED-COVERAGE" for b in result["blockers"])
        )
        self.assertIn("loader return", result["blockers"][0]["detail"])
        self.assertIn("record bounds", result["blockers"][1]["detail"])
        self.assertEqual(result["blockers"][2]["table_value"], "0x80010000")
        self.assertEqual(self.data, old)

    def test_partial_library_does_not_promote_shared_targets_or_extended_namespace(self):
        row = self.data["event_dispatch_tables"][0]["rows"][0]
        row.update(
            analysis_status="observed_subset",
            native_status="authored_cpp_library",
            evidence=["INVENTED"],
            cpp_reconstruction="src/invented.cpp",
        )
        result = diagnose(
            self.data,
            ["instruction:primary:0x00", "instruction:primary:0x01", "instruction:extended:0x00"],
            "FULL-INSTRUCTION-BEHAVIOR",
        )
        self.assertEqual(
            [b["analysis_status"] for b in result["blockers"]],
            ["observed_subset", "unresolved", "unresolved"],
        )
        self.assertEqual(result["status"], "blocked")
        with self.assertRaisesRegex(ValueError, "unlisted source"):
            validate(self.data, allowed_sources=set())
        with self.assertRaisesRegex(ValueError, "unknown evidence"):
            validate(self.data, known_evidence=set())

    def test_unlisted_dependencies_and_sprite_opcodes_remain_blocked(self):
        refs = [
            "symbol:unlisted",
            "format:unlisted",
            "instruction:sprite:0xfd",
            "instruction:unlisted:0x00",
        ]
        result = diagnose(self.data, refs, "REQUESTED-COVERAGE")
        self.assertEqual([b["analysis_status"] for b in result["blockers"]], ["unlisted"] * 4)
        self.assertEqual(result["status"], "blocked")

    def test_missing_duplicate_and_reordered_opcodes_are_rejected(self):
        for mutation in (lambda rows: rows.pop(3), lambda rows: rows.__setitem__(3, rows[2])):
            data = copy.deepcopy(self.data)
            mutation(data["event_dispatch_tables"][0]["rows"])
            with self.assertRaisesRegex(
                ValueError, "table has gaps|missing, duplicated or reordered"
            ):
                validate(data)
        self.data["event_dispatch_tables"][1]["namespace"] = "primary"
        with self.assertRaisesRegex(ValueError, "namespaces must remain distinct"):
            validate(self.data)

    def test_changed_target_fingerprint_and_unsupported_completion_fail(self):
        self.data["event_dispatch_tables"][0]["rows"][3]["handler"] = "0x00020001"
        with self.assertRaisesRegex(ValueError, "fingerprint differs"):
            validate(self.data)
        self.data = fixture()
        self.data["coverage_complete"] = True
        with self.assertRaisesRegex(ValueError, "incomplete entries"):
            validate(self.data)
        self.data["coverage_complete"] = False
        self.data["event_dispatch_tables"][0]["rows"][3]["analysis_status"] = "noop"
        with self.assertRaisesRegex(ValueError, "unreviewed analysis status"):
            validate(self.data)

    def test_inventory_gap_cannot_lose_its_identity_or_next_experiment(self):
        self.data["unknown_formats"] = [{"id": "invented", "next_experiment": " "}]
        with self.assertRaisesRegex(ValueError, "format:invented lacks its next experiment"):
            validate(self.data)
        self.data["unknown_formats"][0]["next_experiment"] = "Measure source records"
        self.data["unknown_formats"].append(dict(self.data["unknown_formats"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate format:invented"):
            validate(self.data)

    def test_missing_dependencies_cannot_turn_an_empty_request_into_a_pass(self):
        for dependencies in ([], ["primary:0x00"], ["instruction:primary:0x00"] * 2):
            with self.assertRaises(ValueError):
                diagnose(self.data, dependencies, "REQUESTED-COVERAGE")

    def test_cli_reports_blocked_coverage_and_returns_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "inventory.json"
            path.write_text(json.dumps(self.data))
            result = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools/repository/recovery.py"),
                    "--inventory",
                    str(path),
                    "--coverage",
                    "TEST-INVENTED-COVERAGE",
                    "--dependency",
                    "instruction:primary:0x03",
                ],
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(
            json.loads(result.stdout)["blockers"][0]["blocked_coverage"], "TEST-INVENTED-COVERAGE"
        )


if __name__ == "__main__":
    unittest.main()
