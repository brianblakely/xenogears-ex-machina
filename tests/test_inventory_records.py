"""Adversarial public-metadata checks; no original game bytes are test fixtures."""

from __future__ import annotations

import copy
import json
import unittest

from tools.repository.matrix import ROOT
from tools.repository.validate import validate_baseline_inventory, validate_inventory


class InventoryRecordTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.inventory = json.loads((ROOT / "analysis/coverage/inventory.json").read_text())
        cls.observations = json.loads(
            (ROOT / "analysis/coverage/observed-entrypoints.json").read_text()
        )
        cls.findings = {
            path.stem: json.loads(path.read_text())
            for path in (ROOT / "analysis/findings").glob("*.json")
        }
        cls.profiles = {row["profile"] for row in cls.inventory["content"]}

    def validate(self, inventory: dict, findings: dict | None = None) -> None:
        validate_inventory(inventory, self.profiles)
        validate_baseline_inventory(ROOT, inventory, findings or self.findings, self.observations)

    def test_grounded_baseline_does_not_require_exhaustive_catalog(self) -> None:
        self.assertTrue(self.inventory["baseline_inventory_ready"])
        self.assertFalse(self.inventory["catalog_complete"])
        self.validate(self.inventory)
        changed = copy.deepcopy(self.inventory)
        changed["catalog_complete"] = True
        with self.assertRaisesRegex(ValueError, "cannot claim catalog completeness"):
            self.validate(changed)

    def test_taxonomy_cannot_replace_original_anchors(self) -> None:
        changed = copy.deepcopy(self.inventory)
        changed["content"][0]["baseline_anchors"] = []
        with self.assertRaisesRegex(ValueError, "taxonomy without original anchors"):
            self.validate(changed)

    def test_unverified_finding_cannot_ground_a_source_family(self) -> None:
        findings = copy.deepcopy(self.findings)
        findings["EVID-REF-012"]["validation"]["result"] = "not_run"
        with self.assertRaisesRegex(ValueError, "passed original evidence"):
            self.validate(self.inventory, findings)

    def test_source_relocation_cannot_preserve_anchor_identity(self) -> None:
        changed = copy.deepcopy(self.inventory)
        anchor = next(
            anchor
            for row in changed["content"]
            for anchor in row["baseline_anchors"]
            if anchor["kind"] == "source_resource"
        )
        anchor["source"]["source_lba"] += 1
        with self.assertRaisesRegex(ValueError, "measured original projection"):
            self.validate(changed)

    def test_other_disc_observation_cannot_satisfy_this_disc(self) -> None:
        changed = copy.deepcopy(self.inventory)
        row = next(row for row in changed["content"] if row["category"] == "world_map")
        anchor = next(a for a in row["baseline_anchors"] if a["kind"] == "observed_entrypoint")
        anchor["entrypoint"] = next(
            entry["id"]
            for entry in self.observations["entries"]
            if entry["category"] == row["category"] and entry["source_profile"] != row["profile"]
        )
        with self.assertRaisesRegex(ValueError, "unrelated source or category"):
            self.validate(changed)

    def test_duplicate_category_cannot_replace_an_uncovered_category(self) -> None:
        changed = copy.deepcopy(self.inventory)
        changed["content"][1]["category"] = changed["content"][0]["category"]
        with self.assertRaisesRegex(ValueError, "exactly one inventory row"):
            self.validate(changed)

    def test_baseline_cannot_erase_unresolved_content_work(self) -> None:
        changed = copy.deepcopy(self.inventory)
        changed["content"][0]["gap"] = ""
        with self.assertRaisesRegex(ValueError, "retain unknowns and next evidence"):
            self.validate(changed)


if __name__ == "__main__":
    unittest.main()
