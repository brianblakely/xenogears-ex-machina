"""Small public integrity gate; source and tests, not generated requirement facets."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if __package__:
    from .source_archive import source_files
else:
    from source_archive import source_files

STAGES = ["identified", "analyzed", "decompiled", "natively_implemented", "behaviorally_validated"]

CONFIDENCE = {"unknown", "tentative", "supported", "confirmed"}

CATEGORIES = {
    "fields",
    "world_map",
    "battles",
    "menus",
    "event_sequences",
    "audio",
    "fmvs",
    "saves",
    "optional_activities",
    "minigames",
}

def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)

def digest(value: str, label: str) -> None:
    require(
        isinstance(value, str) and bool(re.fullmatch(r"[a-f0-9]{64}", value)),
        f"Invalid SHA256: {label}",
    )

def unique(rows: list[dict], label: str) -> dict:
    mapped = {row["id"]: row for row in rows}
    require(len(mapped) == len(rows), f"Duplicate IDs in {label}")
    return mapped

def load(root: Path, name: str) -> dict:
    value = json.loads((root / name).read_text())
    require(value.get("schema_version") == 1, f"Unsupported schema: {name}")
    return value

def validate_finding(finding: dict, profiles: set[str]) -> None:
    required = {
        "id",
        "kind",
        "source_profiles",
        "locations",
        "observation",
        "interpretation",
        "confidence",
        "validation",
        "limitations",
        "author",
        "reviewers",
    }
    require(required <= finding.keys(), "Finding missing required evidence fields")
    require(
        all(finding.get(key) for key in ("kind", "observation", "interpretation", "author")),
        "Finding must separate nonempty observation, interpretation and authorship",
    )
    require(finding["confidence"] in CONFIDENCE, "Invalid finding confidence")
    require(
        bool(finding["source_profiles"]) and set(finding["source_profiles"]) <= profiles,
        "Finding requires known source profiles",
    )
    require(bool(finding["locations"]), "Finding needs source coordinates")
    for loc in finding["locations"]:
        require(
            {
                "profile",
                "file",
                "file_offset",
                "track_lba",
                "sector_bytes",
                "user_data_offset",
                "executable",
                "overlay",
                "address",
                "address_space",
                "not_applicable",
            }
            <= loc.keys(),
            "Incomplete finding location",
        )
        require(loc["profile"] in finding["source_profiles"], "Finding location/profile mismatch")
        require(
            loc["track_lba"] is not None or (loc["file"] and loc["file_offset"] is not None),
            "Finding has no byte/sector location",
        )
        for key in ("file_offset", "track_lba", "user_data_offset"):
            require(
                loc[key] is None or isinstance(loc[key], int) and loc[key] >= 0,
                f"Invalid location {key}",
            )
        if loc["track_lba"] is not None:
            require(
                loc["sector_bytes"] in {2048, 2352, 2448},
                "Sector location requires an explicit supported coordinate unit",
            )
        if loc["address"] is not None:
            require(
                bool(loc["executable"]) and bool(loc["address_space"]),
                "Address requires executable and address-space identity",
            )
            require(
                bool(loc["overlay"]) or bool(loc["not_applicable"]),
                "Address needs overlay identity or explicit applicability reason",
            )
        require(
            all(
                loc[key] is not None
                for key in ("file", "file_offset", "executable", "overlay", "address")
            )
            or bool(loc["not_applicable"]),
            "Missing coordinates need an applicability explanation",
        )
    validation = finding["validation"]
    require(
        {"preconditions", "commands", "expected", "actual", "result", "tolerance", "artifacts"}
        <= validation.keys(),
        "Incomplete reproducibility procedure",
    )
    require(
        bool(validation["commands"]) and bool(validation["preconditions"]),
        "Finding needs commands and preconditions",
    )
    require(
        validation["result"] in {"not_run", "passed", "failed", "blocked"}, "Invalid finding result"
    )
    if validation["result"] == "passed":
        require(
            bool(validation["artifacts"]) and bool(validation["actual"]),
            "Passed finding requires measured artifacts and actual output",
        )
    for artifact in validation["artifacts"]:
        digest(artifact["sha256"], "finding artifact")
        require(
            artifact["distribution"] in {"private_local_only", "public_authored_metadata"},
            "Artifact distribution scope missing",
        )
    if finding["confidence"] == "confirmed":
        require(
            validation["result"] == "passed" and bool(finding["reviewers"]),
            "Confirmed finding needs reproduced and reviewed evidence",
        )

def validate_subsystem(row: dict, evidence: set[str], decisions: set[str]) -> None:
    require(row["stage"] in ["unidentified", *STAGES], "Invalid subsystem stage")
    require(set(row["stage_evidence"]) == set(STAGES), "Subsystem must track every maturity stage")
    index = -1 if row["stage"] == "unidentified" else STAGES.index(row["stage"])
    for number, stage in enumerate(STAGES):
        references = row["stage_evidence"][stage]
        require(set(references) <= evidence, "Subsystem references missing evidence")
        require(
            number > index or bool(references),
            "Subsystem cannot advance without every preceding stage's evidence",
        )
        require(
            number <= index or not references,
            "Evidence above declared maturity needs an explicit transition review",
        )
    require(
        row["behavior_status"]
        in {"unknown", "partially_understood", "preserved", "intentionally_changed", "mixed"},
        "Invalid behavior classification",
    )
    changes = row["intentional_change_records"]
    require(set(changes) <= decisions, "Subsystem references a missing intentional-change record")
    if row["behavior_status"] in {"intentionally_changed", "mixed"}:
        require(bool(changes), "Intentional changes need independent decision records")
    if row["behavior_status"] == "unknown":
        require(
            bool(row["unknowns"]) and row["stage"] != "behaviorally_validated",
            "Unknown behavior cannot be claimed validated",
        )

def validate_inventory(data: dict, profiles: set[str]) -> None:
    require(set(data["categories"]) == CATEGORIES, "Original-content category omitted")
    entries = unique(data["content"], "content inventory")
    require(
        len(entries) == len(profiles) * len(CATEGORIES)
        and len({(row["profile"], row["category"]) for row in entries.values()}) == len(entries),
        "Each source/category needs exactly one inventory row",
    )
    for profile in profiles:
        require(
            {row["category"] for row in entries.values() if row["profile"] == profile}
            == CATEGORIES,
            "Each reference needs all original-content categories",
        )
    for row in entries.values():
        require(
            row["profile"] in profiles and row["category"] in CATEGORIES,
            "Invalid content profile/category",
        )
        require(
            row["status"]
            in {"unidentified", "partially_enumerated", "enumerated", "not_applicable"},
            "Invalid inventory status",
        )
        require(
            row["observed_count"] == len(row["original_content_ids"]),
            "Observed count must match actual content IDs",
        )
        require(
            len(set(row["original_content_ids"])) == len(row["original_content_ids"]),
            "Original content IDs cannot be counted twice",
        )
        if row["status"] == "unidentified":
            require(
                row["total_count"] is None and not row["original_content_ids"] and bool(row["gap"]),
                "Unidentified content cannot invent counts or IDs",
            )
        if row["status"] in {"enumerated", "not_applicable"}:
            require(
                bool(row["evidence"]) and row["total_count"] == row["observed_count"],
                "Completed content category needs exact counts and original evidence",
            )
    if data["catalog_complete"]:
        require(
            all(row["status"] in {"enumerated", "not_applicable"} for row in entries.values()),
            "A taxonomy/partial inventory cannot claim catalog completeness",
        )

def validate_observed_entrypoints(
    data: dict, inventory: dict, profiles: set[str], findings: dict
) -> None:
    require(
        data["catalog_complete"] is False,
        "Observed entry points do not prove a complete original catalog",
    )
    entries = unique(data["entries"], "observed entry points")
    for entry in entries.values():
        require(
            entry["source_profile"] in profiles and entry["category"] in CATEGORIES,
            "Observed entry point has an unknown source or category",
        )
        require(
            entry["status"] == "observed_original_execution"
            and bool(entry["evidence"])
            and set(entry["evidence"]) <= findings.keys(),
            "Observed entry point requires original-execution evidence",
        )
        digest(entry["image_sha256"], "observed entry point image")
        require(
            any(
                entry["source_profile"] in findings[key]["source_profiles"]
                and findings[key]["validation"]["result"] == "passed"
                and any(
                    artifact["sha256"] == entry["image_sha256"]
                    for artifact in findings[key]["validation"]["artifacts"]
                )
                for key in entry["evidence"]
            ),
            "Observed image lacks a passed finding for that original source",
        )
    for row in inventory["content"]:
        expected = {
            entry["id"]
            for entry in entries.values()
            if entry["source_profile"] == row["profile"] and entry["category"] == row["category"]
        }
        references = row["observed_entrypoint_ids"]
        require(
            len(references) == len(set(references))
            and set(references) == expected
            and row["observed_entrypoint_count"] == len(expected),
            "Inventory observed-entrypoint references or counts are inconsistent",
        )



def validate_baseline_inventory(
    root: Path, inventory: dict, findings: dict, observations: dict
) -> None:
    """Require original anchors without mistaking a baseline for exhaustive recovery."""
    require(
        isinstance(inventory["baseline_inventory_ready"], bool)
        and bool(inventory["baseline_scope"]),
        "Baseline inventory needs an explicit status and scope",
    )
    sources = {
        profile["source_profile"]: {
            record[0]: dict(zip(profile["records_columns"], record, strict=True))
            for record in profile["records"]
        }
        for profile in load(root, "analysis/coverage/source-fingerprints.json")["profiles"]
    }
    observed = unique(observations["entries"], "observed entry points")
    catalog_paths = {
        "fields": "analysis/coverage/field-pairs.json",
        "fmvs": "analysis/coverage/media.json",
        "audio": "analysis/coverage/media.json",
    }
    for row in inventory["content"]:
        anchors = row.get("baseline_anchors", [])
        if inventory["baseline_inventory_ready"]:
            require(
                row["status"] != "unidentified" and bool(anchors),
                "Baseline inventory cannot pass a taxonomy without original anchors",
            )
        if row["status"] == "partially_enumerated":
            require(
                bool(row["gap"]) and bool(row["next_evidence"]),
                "Partial original inventory must retain unknowns and next evidence",
            )
        for anchor in anchors:
            evidence = anchor["evidence"]
            require(
                bool(anchor["relationship"])
                and bool(evidence)
                and set(evidence) <= set(row["evidence"]),
                "Baseline anchor needs an explicit category relationship and evidence",
            )
            require(
                all(
                    key in findings
                    and row["profile"] in findings[key]["source_profiles"]
                    and findings[key]["validation"]["result"] == "passed"
                    for key in evidence
                ),
                "Baseline anchor needs passed original evidence for this source",
            )
            kind = anchor["kind"]
            if kind == "observed_entrypoint":
                entry = observed.get(anchor["entrypoint"])
                require(
                    entry is not None
                    and entry["source_profile"] == row["profile"]
                    and bool(set(entry["evidence"]) & set(evidence))
                    and (
                        entry["category"] == row["category"]
                        or (entry["category"], row["category"])
                        == ("minigames", "optional_activities")
                    ),
                    "Baseline observation has an unrelated source or category",
                )
            elif kind == "source_resource":
                resource = anchor["source"]
                require(
                    resource == sources[row["profile"]].get(resource["slot"]),
                    "Baseline resource disagrees with the measured original projection",
                )
                require(
                    any(
                        loc["profile"] == row["profile"]
                        and isinstance(loc["overlay"], dict)
                        and loc["overlay"].get("source_slot") == resource["slot"]
                        and resource["source_lba"]
                        <= loc["track_lba"]
                        < resource["source_lba"] + resource["source_sector_count"]
                        and resource["projection_sha256"] in loc["overlay"].values()
                        for key in evidence
                        for loc in findings[key]["locations"]
                    ),
                    "Baseline resource lacks its original finding coordinate and hash",
                )
            elif kind == "source_catalog":
                require(
                    anchor["path"] == catalog_paths.get(row["category"])
                    and row["catalogued_count"] > 0,
                    "Baseline catalog needs an attributed content selector/signature inventory",
                )
                catalog = load(root, anchor["path"])
                catalog_evidence = (
                    catalog["mapping_evidence"]
                    if row["category"] == "fields"
                    else catalog["evidence"]
                )
                require(
                    bool(set(evidence) & set(catalog_evidence)),
                    "Baseline catalog lacks its original mapping evidence",
                )
            else:
                raise ValueError("Unknown baseline inventory anchor kind")


def validate(root: Path = ROOT) -> None:
    files = source_files(root)
    for path in files:
        if path.suffix == '.json':
            json.loads(path.read_text())
    # Actual historical evidence remains tested by the focused validator tests.
    # Do not reinterpret its old phase/facet labels as the current checklist.
    print(f"Source boundary and JSON integrity passed: {len(files)} authored files")


if __name__ == '__main__':
    validate()
