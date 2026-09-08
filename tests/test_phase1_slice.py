"""Reject missing Phase 1 proof domains and false deferred-work completion."""

from __future__ import annotations

import unittest

from tools.repository.matrix import ROOT, build_matrix, target_scope
from tools.repository.phase1_slice import original_evidence, validate_structure
from tools.repository.validate import validate_plan_completion, validate_traceability


class Phase1SliceGateTests(unittest.TestCase):
    def test_manifest_retains_all_required_work(self) -> None:
        matrix = build_matrix()
        validate_structure(matrix)
        self.assertEqual(len(matrix["phase1_slice"]["proofs"]), 9)

    def test_missing_required_proof_is_rejected_before_exit(self) -> None:
        matrix = build_matrix()
        matrix["phase1_slice"]["proofs"].pop(0)
        with self.assertRaisesRegex(ValueError, "missing or changed required proof"):
            validate_structure(matrix)

    def test_dropped_broad_facet_is_rejected(self) -> None:
        matrix = build_matrix()
        matrix["phase1_slice"]["facet_dispositions"].pop()
        with self.assertRaisesRegex(ValueError, "dropped or unknown broad facet"):
            validate_structure(matrix)

    def test_deferred_facet_cannot_be_passed(self) -> None:
        matrix = build_matrix()
        deferred = next(
            d["facet"]
            for d in matrix["phase1_slice"]["facet_dispositions"]
            if d["disposition"] == "deferred"
        )
        next(r for r in matrix["requirements"] if r["id"] == deferred)["status"] = "passed"
        with self.assertRaisesRegex(ValueError, "deferred facet cannot be passed"):
            validate_structure(matrix)

    def test_deferred_task_cannot_be_checked(self) -> None:
        matrix = build_matrix()
        deferred = next(
            d["facet"]
            for d in matrix["phase1_slice"]["facet_dispositions"]
            if d["disposition"] == "deferred"
        )
        task = deferred.rsplit("-", 1)[0]
        for row in matrix["requirements"]:
            if row["source_id"] == task and row["id"] != deferred:
                row["status"] = "passed"
        text = (ROOT / "plan.md").read_text()
        original = matrix["source_snapshot"]["tasks"][task]
        text = text.replace("- [ ] " + original, "- [x] " + original)
        with self.assertRaisesRegex(ValueError, "complete facet evidence"):
            validate_plan_completion(text, matrix["requirements"])

    def test_exit_cannot_bypass_required_proofs(self) -> None:
        matrix = build_matrix()
        matrix["crosscutting"]["phase_exits"][1].update(status="passed", evidence=["invented"])
        with self.assertRaisesRegex(ValueError, "incomplete required proofs"):
            validate_traceability(matrix)

    def test_deferred_work_cannot_be_dropped_from_later_scope(self) -> None:
        matrix = build_matrix()
        next(b for b in matrix["phase1_slice"]["backlog"] if b["kind"] == "outside_slice")[
            "later_facets"
        ] = []
        with self.assertRaisesRegex(ValueError, "linked to later facets"):
            validate_structure(matrix)

    def test_synthetic_behavior_cannot_become_original_proof(self) -> None:
        synthetic = {
            "covers": ["P01-SLICE-FIELD-BEHAVIOR"],
            "targets": ["original-reference-analysis"],
            "evidence_kind": "synthetic_tooling",
            "validation": {"result": "passed", "artifacts": ["synthetic"]},
        }
        with self.assertRaisesRegex(ValueError, "confirmed original source/execution"):
            original_evidence(synthetic, "P01-SLICE-FIELD-BEHAVIOR")

    def test_later_native_targets_are_preserved(self) -> None:
        self.assertEqual(target_scope("P01-T05"), ["original-reference-analysis"])
        self.assertEqual(target_scope("P02-T04"), ["arch-vulkan"])
        self.assertEqual(target_scope("P06-T03"), ["windows-d3d12"])
        self.assertEqual(target_scope("P13-T03"), ["macos-metal"])
        self.assertEqual(target_scope("P04-T01"), ["arch-vulkan", "windows-d3d12", "macos-metal"])


if __name__ == "__main__":
    unittest.main()
