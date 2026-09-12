"""Reject false coverage claims and data leakage with adversarial original fixtures."""

from __future__ import annotations

import copy
import io
import tarfile
import tempfile
import unittest
from pathlib import Path

from tools.repository.matrix import build_matrix, facet_specs, markdown, plan_sources, target_scope
from tools.repository.source_archive import archive_bytes, validate_path
from tools.repository.validate import (
    CATEGORIES,
    STAGES,
    validate_finding,
    validate_inventory,
    validate_observed_entrypoints,
    validate_plan_completion,
    validate_result,
    validate_subsystem,
    validate_traceability,
)


class EvidenceWorkflowTests(unittest.TestCase):
    def test_lettered_phase_and_duplicate_contract_tables_remain_distinct(self) -> None:
        plan = (
            "## Foundational native-agent contract\n"
            "| Capability | Required behavior |\n|---|---|\n| Play | Native play |\n"
            "## Foundational agent-authoring contract\n"
            "| Capability | Required behavior |\n|---|---|\n| Build | Source build |\n"
            "## Phase 2 — Runtime\n- [ ] Runtime task.\n"
            "**Exit criterion:** Runtime ready.\n"
            "## Phase 2A — Authoring\n- [x] Authored task.\n"
            "**Early exit criterion, required by Phase 3:** Small bridge ready.\n"
            "## Phase 3 — Native slice\n- [ ] Slice task.\n"
        )
        source = plan_sources(plan)
        self.assertEqual(source["phases"], ["P02", "P02A", "P03"])
        self.assertEqual(list(source["tasks"]), ["P02-T01", "P02A-T01", "P03-T01"])
        self.assertEqual(source["tables"]["agent_contract"], [["Play", "Native play"]])
        self.assertEqual(source["tables"]["authoring_contract"], [["Build", "Source build"]])
        self.assertEqual(source["exits"], {"P02": "Runtime ready."})
        self.assertIn("P02A", source["early_exits"])
        rows = [{"source_id": "P02A-T01", "status": "defined"}]
        with self.assertRaisesRegex(ValueError, "P02A-T01"):
            validate_plan_completion(plan, rows)
        rows[0]["status"] = "passed"
        validate_plan_completion(plan, rows)

    def test_authoring_matrix_keeps_arch_preview_and_complete_sdk_distinct(self) -> None:
        matrix = build_matrix()
        authoring = [row for row in matrix["requirements"] if row["phase"] == "2A"]
        self.assertEqual(len({row["source_id"] for row in authoring}), 41)
        self.assertTrue(
            all(row["status"] == "defined" and not row["evidence"] for row in authoring)
        )
        self.assertEqual(target_scope("P02A-T35"), ["arch-vulkan", "arch-headless"])
        self.assertIn("windows-headless", target_scope("P02A-T07"))
        self.assertIn("macos-metal", target_scope("P13-T19"))
        rendered = markdown(matrix)
        self.assertIn("## Phase 2A\n", rendered)
        self.assertLess(rendered.index("## Phase 2\n"), rendered.index("## Phase 2A\n"))
        self.assertLess(rendered.index("## Phase 2A\n"), rendered.index("## Phase 3\n"))

    def test_authoring_source_tables_prose_and_early_gate_cannot_drift(self) -> None:
        for table in ("authoring_contract", "authoring_stack"):
            matrix = build_matrix()
            matrix["crosscutting"][table].pop()
            with self.subTest(table=table), self.assertRaisesRegex(ValueError, table):
                validate_traceability(matrix)
        matrix = build_matrix()
        matrix["crosscutting"]["foundational_authoring_requirement"]["source"] = "lost"
        with self.assertRaisesRegex(ValueError, "foundational authoring requirement"):
            validate_traceability(matrix)
        matrix = build_matrix()
        matrix["crosscutting"]["early_exits"][0]["source"] = "lost"
        with self.assertRaisesRegex(ValueError, "early exit criterion"):
            validate_traceability(matrix)

    def test_early_bridge_pass_cannot_close_broader_authoring_checklist(self) -> None:
        matrix = build_matrix()
        early = matrix["crosscutting"]["early_exits"][0]
        early.update(status="passed", evidence=["synthetic-gate-test-only"])
        with self.assertRaisesRegex(ValueError, "Early exit cannot bypass incomplete facets"):
            validate_traceability(matrix)
        for row in matrix["requirements"]:
            if row["source_id"] in early["tasks"]:
                row["status"] = "passed"
        validate_traceability(matrix)
        phase7 = next(gate for gate in matrix["crosscutting"]["phase_exits"] if gate["phase"] == 7)
        phase7.update(status="passed", evidence=["synthetic-gate-test-only"])
        for row in matrix["requirements"]:
            if row["phase"] == 7:
                row["status"] = "passed"
        with self.assertRaisesRegex(ValueError, "incomplete authoring checklist"):
            validate_traceability(matrix)

    def test_phase3_cannot_pass_without_authored_bridge(self) -> None:
        matrix = build_matrix()
        phase3 = next(gate for gate in matrix["crosscutting"]["phase_exits"] if gate["phase"] == 3)
        phase3.update(status="passed", evidence=["synthetic-gate-test-only"])
        for row in matrix["requirements"]:
            if row["phase"] == 3:
                row["status"] = "passed"
        with self.assertRaisesRegex(ValueError, "incomplete early bridge"):
            validate_traceability(matrix)
        phase3["requires_early_exits"] = []
        with self.assertRaisesRegex(ValueError, "must require the authored early bridge"):
            validate_traceability(matrix)

    def test_release_and_early_gate_cannot_omit_required_scope(self) -> None:
        matrix = build_matrix()
        matrix["crosscutting"]["early_exits"][0]["tasks"].pop()
        with self.assertRaisesRegex(ValueError, "protected bridge acceptance"):
            validate_traceability(matrix)
        matrix = build_matrix()
        preview = next(
            gate
            for gate in matrix["crosscutting"]["release_checkpoints"]
            if gate["source_row"][0] == "Agent-authoring preview"
        )
        preview["required_early_exits"] = []
        with self.assertRaisesRegex(ValueError, "cannot omit the authored early bridge"):
            validate_traceability(matrix)
        matrix = build_matrix()
        matrix["crosscutting"]["release_checkpoints"][0]["required_phases"] = [99]
        with self.assertRaisesRegex(ValueError, "unknown phases"):
            validate_traceability(matrix)

    def test_checked_todo_requires_every_facet_to_have_passed(self) -> None:
        plan = "## Phase 0 — Example\n- [x] A compound requirement.\n"
        rows = [
            {"source_id": "P00-T01", "status": "passed"},
            {"source_id": "P00-T01", "status": "defined"},
        ]
        with self.assertRaisesRegex(ValueError, "complete facet evidence"):
            validate_plan_completion(plan, rows)
        rows[1]["status"] = "passed"
        validate_plan_completion(plan, rows)

    def test_finding_requires_coordinates_and_qualified_addresses(self) -> None:
        finding = {
            "id": "SYNTHETIC-RECORD",
            "kind": "original_static_metadata",
            "source_profiles": ["fixture-profile"],
            "locations": [
                {
                    "profile": "fixture-profile",
                    "file": None,
                    "file_offset": None,
                    "track_lba": 16,
                    "sector_bytes": 2352,
                    "user_data_offset": 24,
                    "executable": None,
                    "overlay": None,
                    "address": None,
                    "address_space": None,
                    "not_applicable": "Synthetic static metadata case.",
                }
            ],
            "observation": "Synthetic record for schema validation only.",
            "interpretation": "No original-game evidence established.",
            "confidence": "unknown",
            "validation": {
                "preconditions": ["Invented fixture"],
                "commands": ["synthetic procedure"],
                "expected": "Unobserved",
                "actual": "Not run",
                "result": "not_run",
                "tolerance": "Not established",
                "artifacts": [],
            },
            "limitations": ["Not a real original-data finding."],
            "author": "Original synthetic test",
            "reviewers": [],
        }
        validate_finding(finding, {"fixture-profile"})
        no_location = copy.deepcopy(finding)
        no_location["locations"][0]["track_lba"] = None
        with self.assertRaisesRegex(ValueError, "no byte/sector location"):
            validate_finding(no_location, {"fixture-profile"})
        unqualified = copy.deepcopy(finding)
        unqualified["locations"][0]["address"] = "0x80010000"
        with self.assertRaisesRegex(ValueError, "executable and address-space"):
            validate_finding(unqualified, {"fixture-profile"})

    def test_crosscutting_drift_is_rejected(self) -> None:
        matrix = build_matrix()
        validate_traceability(matrix)
        matrix["crosscutting"]["keyboard_defaults"].pop()
        with self.assertRaisesRegex(ValueError, "keyboard_defaults"):
            validate_traceability(matrix)

    def test_duplicate_or_empty_facets_are_rejected(self) -> None:
        for text in (
            "P00-T01 | a; a | check",
            "P00-T01 | a; | check",
            "P00-T01 | a | check\nP00-T01 | b | check",
        ):
            with self.assertRaises(ValueError):
                facet_specs(text)

    def test_phase_exit_cannot_override_incomplete_facets(self) -> None:
        matrix = build_matrix()
        next(row for row in matrix["requirements"] if row["phase"] == 2)["status"] = "defined"
        matrix["crosscutting"]["phase_exits"][2].update(status="passed", evidence=["invented"])
        with self.assertRaisesRegex(ValueError, "incomplete facets"):
            validate_traceability(matrix)

    def test_pass_needs_facet_and_every_platform_evidence(self) -> None:
        row = {
            "id": "R",
            "phase": 3,
            "status": "passed",
            "targets": ["arch-vulkan", "windows-d3d12"],
            "evidence": ["E"],
        }
        record = {
            "E": {"covers": ["R"], "targets": ["arch-vulkan"], "evidence_kind": "native_behavior"}
        }
        with self.assertRaisesRegex(ValueError, "every declared target"):
            validate_result(row, record)
        record["E"]["targets"].append("windows-d3d12")
        validate_result(row, record)
        record["E"]["covers"] = []
        with self.assertRaisesRegex(ValueError, "does not claim"):
            validate_result(row, record)

    def test_synthetic_evidence_cannot_pass_gameplay(self) -> None:
        row = {
            "id": "R",
            "phase": 3,
            "status": "passed",
            "targets": ["arch-vulkan"],
            "evidence": ["E"],
        }
        for kind in ("synthetic_tooling", "infrastructure_only"):
            with self.assertRaisesRegex(ValueError, "cannot pass gameplay"):
                validate_result(
                    row, {"E": {"covers": ["R"], "targets": ["arch-vulkan"], "evidence_kind": kind}}
                )
        row["phase"] = "2A"
        with self.assertRaisesRegex(ValueError, "cannot pass gameplay"):
            validate_result(
                row,
                {
                    "E": {
                        "covers": ["R"],
                        "targets": ["arch-vulkan"],
                        "evidence_kind": "infrastructure_only",
                    }
                },
            )

    def test_unknown_subsystem_cannot_skip_evidence_or_become_changed(self) -> None:
        original = {
            "id": "synthetic-unidentified-subsystem",
            "stage": "unidentified",
            "stage_evidence": {stage: [] for stage in STAGES},
            "behavior_status": "unknown",
            "intentional_change_records": [],
            "unknowns": ["No original behavior has been observed."],
        }
        validate_subsystem(original, set(), set())
        promoted = copy.deepcopy(original)
        promoted["stage"] = "decompiled"
        with self.assertRaisesRegex(ValueError, "every preceding stage"):
            validate_subsystem(promoted, set(), set())
        changed = copy.deepcopy(original)
        changed["behavior_status"] = "intentionally_changed"
        with self.assertRaisesRegex(ValueError, "decision records"):
            validate_subsystem(changed, set(), set())

    def test_unknown_catalog_cannot_claim_completion_or_invent_counts(self) -> None:
        profiles = {"synthetic-profile"}
        original = {
            "catalog_complete": False,
            "categories": sorted(CATEGORIES),
            "content": [
                {
                    "id": category,
                    "category": category,
                    "profile": "synthetic-profile",
                    "status": "unidentified",
                    "original_content_ids": [],
                    "observed_count": 0,
                    "total_count": None,
                    "gap": "No original observations.",
                }
                for category in sorted(CATEGORIES)
            ],
        }
        validate_inventory(original, profiles)
        complete = copy.deepcopy(original)
        complete["catalog_complete"] = True
        with self.assertRaisesRegex(ValueError, "cannot claim catalog"):
            validate_inventory(complete, profiles)
        invented = copy.deepcopy(original)
        invented["content"][0]["total_count"] = 1
        with self.assertRaisesRegex(ValueError, "cannot invent"):
            validate_inventory(invented, profiles)

    def test_observation_requires_image_provenance_and_matching_category(self) -> None:
        data = {
            "catalog_complete": False,
            "entries": [
                {
                    "id": "OBS-SYNTHETIC",
                    "source_profile": "fixture",
                    "category": "menus",
                    "status": "observed_original_execution",
                    "evidence": ["E"],
                    "image_sha256": "a" * 64,
                }
            ],
        }
        inventory = {
            "content": [
                {
                    "profile": "fixture",
                    "category": "menus",
                    "observed_entrypoint_ids": ["OBS-SYNTHETIC"],
                    "observed_entrypoint_count": 1,
                }
            ]
        }
        findings = {
            "E": {
                "source_profiles": ["fixture"],
                "validation": {"result": "passed", "artifacts": [{"sha256": "a" * 64}]},
            }
        }
        validate_observed_entrypoints(data, inventory, {"fixture"}, findings)
        wrong_image = copy.deepcopy(data)
        wrong_image["entries"][0]["image_sha256"] = "b" * 64
        with self.assertRaisesRegex(ValueError, "lacks a passed finding"):
            validate_observed_entrypoints(wrong_image, inventory, {"fixture"}, findings)
        wrong_inventory = copy.deepcopy(inventory)
        wrong_inventory["content"][0]["category"] = "battles"
        with self.assertRaisesRegex(ValueError, "references or counts"):
            validate_observed_entrypoints(data, wrong_inventory, {"fixture"}, findings)


class SourceBoundaryTests(unittest.TestCase):
    def test_private_binary_symlink_and_escape_paths_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "safe.md").write_text("Original authored fixture\n")
            (root / "bad.md").write_bytes(b"secret\x00bytes")
            (root / "image.chd").write_bytes(b"not even a real disc")
            (root / "link.md").symlink_to(root / "safe.md")
            (root / ".local").mkdir()
            (root / ".local/private.md").write_text("private")
            for name in (
                "../safe.md",
                "/safe.md",
                "bad.md",
                "image.chd",
                "link.md",
                ".local/private.md",
            ):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    validate_path(root, name)

    def test_allowlist_ignores_extra_payload_and_archive_is_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "packaging").mkdir()
            (root / "packaging/source-files.txt").write_text("README.md\n")
            (root / "README.md").write_text("Original public fixture\n")
            (root / "secret.chd").write_bytes(b"private fixture not to package")
            first, second = archive_bytes(root), archive_bytes(root)
            self.assertEqual(first, second)
            with tarfile.open(fileobj=io.BytesIO(first), mode="r:gz") as archive:
                self.assertEqual(archive.getnames(), ["xenogears-ex-machina/README.md"])
                self.assertEqual(
                    archive.extractfile(archive.getmembers()[0]).read(),
                    b"Original public fixture\n",
                )


if __name__ == "__main__":
    unittest.main()
