"""Adversarial mutations of public metadata; no original asset is a test fixture."""

from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

from tools.repository.matrix import ROOT
from tools.repository.projections import validate_projections


class ProjectionRecordTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.documents = {
            name: json.loads((ROOT / f"analysis/coverage/{name}.json").read_text())
            for name in ("source-index", "source-fingerprints", "field-pairs", "inventory", "media")
        }
        cls.profiles = {
            row["id"]: row
            for row in json.loads((ROOT / "analysis/reference-profiles.json").read_text())[
                "profiles"
            ]
        }
        cls.findings = {
            path.stem: json.loads(path.read_text())
            for path in (ROOT / "analysis/findings").glob("*.json")
        }

    def validate_mutation(self, name: str, mutate, expected_error: str) -> None:
        documents = copy.deepcopy(self.documents)
        mutate(documents[name])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "analysis/coverage").mkdir(parents=True)
            for document, data in documents.items():
                (root / f"analysis/coverage/{document}.json").write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, expected_error):
                validate_projections(root, self.profiles, self.findings)

    def test_relocated_projection_cannot_keep_original_source_identity(self) -> None:
        def mutate(data):
            data["profiles"][0]["records"][0][1] += 1

        self.validate_mutation("source-fingerprints", mutate, "original source-index slot")

    def test_missing_field_data_cannot_be_reclassified_as_available(self) -> None:
        def mutate(data):
            pair = next(
                pair
                for profile in data["profiles"]
                for pair in profile["pairs"]
                if pair["availability"] == "exact_dummy_source_pair"
            )
            pair["exact_cdmake_dummy"] = [False, False]
            pair["availability"] = "non_dummy_source_pair"

        self.validate_mutation("field-pairs", mutate, "exact original dummy markers")

    def test_shared_resource_count_cannot_exceed_measured_hash_intersection(self) -> None:
        def mutate(data):
            data["comparison"]["distinct_projection_keys_shared_between_sources"] += 1

        self.validate_mutation("source-fingerprints", mutate, "Shared-projection count")

    def test_unobserved_source_count_and_observed_scene_count_remain_separate(self) -> None:
        def mutate(data):
            field = next(row for row in data["content"] if row["category"] == "fields")
            field["catalogued_count"] = field["observed_count"]

        self.validate_mutation("inventory", mutate, "Field catalog confuses")

    def test_source_mapping_cannot_claim_an_unrelated_original_finding(self) -> None:
        def mutate(data):
            data["mapping_evidence"] = ["EVID-REF-001"]

        self.validate_mutation("field-pairs", mutate, "original loader evidence")

    def test_audio_header_record_cannot_claim_unmatched_source_bytes(self) -> None:
        def mutate(data):
            data["profiles"][0]["audio_headers"][0]["projection_sha256"] = "0" * 64

        self.validate_mutation("media", mutate, "original source projection")

    def test_movie_source_slot_cannot_be_relabelled_as_another_selector(self) -> None:
        def mutate(data):
            data["profiles"][0]["movies"][0]["original_movie_selector"] = 99

        self.validate_mutation("media", mutate, "original selectors")


if __name__ == "__main__":
    unittest.main()
