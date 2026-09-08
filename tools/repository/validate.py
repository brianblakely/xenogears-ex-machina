"""Executable schemas and integrity gates for evidence, scope and source separation."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
from pathlib import Path

if __package__:
    from .agent_contract import validate as validate_agent_contract
    from .matrix import ROOT, build_matrix, generated_files
    from .phase1_slice import validate_evidence as validate_slice_evidence
    from .phase1_slice import validate_structure as validate_slice_structure
    from .projections import validate_projections
    from .source_archive import source_files
else:
    from agent_contract import validate as validate_agent_contract
    from matrix import ROOT, build_matrix, generated_files
    from phase1_slice import validate_evidence as validate_slice_evidence
    from phase1_slice import validate_structure as validate_slice_structure
    from projections import validate_projections
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


def validate_plan_completion(plan: str, rows: list[dict]) -> None:
    phase, task = -1, 0
    by_task = {}
    for row in rows:
        by_task.setdefault(row["source_id"], []).append(row)
    for line in plan.splitlines():
        if match := re.match(r"## Phase (\d+) —", line):
            phase, task = int(match[1]), 0
        if match := re.match(r"- \[([ x])\] ", line):
            task += 1
            if match[1] == "x":
                key = f"P{phase:02}-T{task:02}"
                require(
                    key in by_task and all(row["status"] == "passed" for row in by_task[key]),
                    f"Checked plan todo lacks complete facet evidence: {key}",
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


def validate_result(row: dict, registry: dict[str, dict]) -> None:
    require(
        row["status"] in {"defined", "passed", "failed", "blocked"}, "Invalid matrix result status"
    )
    if row["status"] == "defined":
        require(not row["evidence"], "A definition is not executed evidence")
        return
    require(bool(row["evidence"]), "Executed/blocked results require evidence")
    require(set(row["evidence"]) <= registry.keys(), "Result references absent evidence")
    if row["status"] != "passed":
        return
    covered_targets = set()
    for evidence_id in row["evidence"]:
        record = registry[evidence_id]
        require(row["id"] in record.get("covers", []), "Evidence does not claim this facet")
        covered_targets.update(record.get("targets", []))
        if row["phase"] > 0:
            require(
                record.get("evidence_kind") not in {"synthetic_tooling", "infrastructure_only"},
                "Synthetic/infrastructure evidence cannot pass gameplay phases",
            )
    require(
        set(row["targets"]) <= covered_targets, "A pass lacks evidence for every declared target"
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


def validate_physical_inventory(root: Path, profiles: dict, findings: dict) -> None:
    for name in ("sector-survey", "source-index"):
        data = load(root, f"analysis/coverage/{name}.json")
        require(
            data["content_catalog_complete"] is False,
            "A physical inventory cannot establish game-content completeness",
        )
        mapped = {row["source_profile"]: row for row in data["profiles"]}
        require(
            len(mapped) == len(data["profiles"]) and mapped.keys() == profiles.keys(),
            "Physical inventory must uniquely cover the selected original profiles",
        )
        for key, row in mapped.items():
            source = profiles[key]["measurement"]["source"]["raw_track"]
            sectors = source["size"] // 2352
            require(
                row["raw_track_sha256"] == source["sha256"],
                "Physical inventory is bound to a different original track",
            )
            require(
                bool(row["evidence"]) and set(row["evidence"]) <= findings.keys(),
                "Physical inventory requires original findings",
            )
            if name == "sector-survey":
                require(
                    row["sector_bytes"] == 2352
                    and row["raw_track_bytes"] == source["size"]
                    and row["surveyed_sectors"] == sectors,
                    "Sector survey has inconsistent source dimensions",
                )
                for counts in (row["header_class_counts"], row["xa_signature_counts"]):
                    require(
                        all(type(value) is int and value > 0 for value in counts.values())
                        and sum(counts.values()) == sectors,
                        "Sector header counts do not account for the complete track",
                    )
                cursor, classes = 0, {}
                for span in row["coarse_header_spans"]:
                    end, kind = span["end_lba_exclusive"], span["header_class"]
                    require(
                        span["start_lba"] == cursor and cursor < end <= sectors,
                        "Sector spans overlap, omit sectors or exceed the track",
                    )
                    classes[kind] = classes.get(kind, 0) + end - cursor
                    cursor = end
                expected = {}
                for kind, count in row["header_class_counts"].items():
                    if kind.endswith(("_audio", "_video")):
                        kind = "declared_audio_or_video"
                    expected[kind] = expected.get(kind, 0) + count
                require(
                    cursor == sectors and classes == expected,
                    "Coarse spans disagree with the complete sector counts",
                )
                continue

            columns = [
                "slot",
                "table_byte_offset",
                "source_lba",
                "signed_length_candidate",
                "kind",
            ]
            require(row["records_columns"] == columns, "Unknown source-index column contract")
            table, terminal = row["table"], row["terminal"]
            require(table["record_bytes_hypothesis"] == 7, "Unsupported source-index hypothesis")
            require(
                terminal["slot"] == len(row["records"])
                and terminal["table_byte_offset"] == 7 * terminal["slot"]
                and terminal["lba_value"] == 0xFFFFFF,
                "Source-index terminal disagrees with its records",
            )
            reconstructed = bytearray()
            positive, headers = 0, {}
            for index, record in enumerate(row["records"]):
                require(len(record) == len(columns), "Incomplete source-index record")
                slot, offset, lba, size, kind = record
                require(
                    slot == index and offset == 7 * slot and 0 <= lba < sectors,
                    "Source-index record has inconsistent coordinates",
                )
                expected_kind = "resource_candidate"
                if size < 0:
                    expected_kind = "negative_count_header_candidate"
                    headers[slot] = -size
                elif not size:
                    expected_kind = (
                        "zero_length_marker_unknown" if lba else "zero_slot_unknown_or_padding"
                    )
                else:
                    positive += 1
                require(kind == expected_kind, "Source-index record classification changed")
                reconstructed.extend(lba.to_bytes(3, "little"))
                reconstructed.extend(size.to_bytes(4, "little", signed=True))
            reconstructed.extend(b"\xff\xff\xff\x00\x00\x00\x00")
            require(
                len(reconstructed) <= table["logical_bytes_surveyed"],
                "Source index exceeds its measured table",
            )
            reconstructed.extend(bytes(table["logical_bytes_surveyed"] - len(reconstructed)))
            require(
                hashlib.sha256(reconstructed).hexdigest() == table["sha256"],
                "Published source-index records do not reproduce the measured table hash",
            )
            groups = {group["header_slot"]: group for group in row["groups"]}
            require(
                len(groups) == len(row["groups"]) and groups.keys() == headers.keys(),
                "Source-index groups omit or duplicate a measured negative header",
            )
            for slot, count in headers.items():
                group = groups[slot]
                require(
                    group["positive_child_count"] == count
                    and group["first_child_slot"] == slot + 1
                    and group["end_child_slot_exclusive"] == slot + count + 1
                    and group["source_lba"] == row["records"][slot][2],
                    "Source-index group disagrees with measured records",
                )
            checks = row["crosschecks"]
            require(
                positive == row["positive_record_count"]
                and checks["negative_headers_matching_immediate_children"] == len(groups)
                and not checks["unmatched_xa_eof_slots"],
                "Source-index summary contradicts its structural cross-checks",
            )
            for label, slots in checks["known_iso_records"].items():
                known = profiles[key]["measurement"]["boot"][label]
                actual = [
                    record[0]
                    for record in row["records"]
                    if record[2:4] == [known["lba"], known["size"]]
                ]
                require(
                    slots == actual and len(slots) == 1,
                    "Source index disagrees with independent ISO boot metadata",
                )
            require(
                row["source_tag"] == profiles[key]["source_disc_tag"],
                "Disc role lacks its measured internal-label correspondence",
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


def validate_traceability(matrix: dict) -> None:
    source, cross = matrix["source_snapshot"], matrix["crosscutting"]
    require(
        [row["source"] for row in cross["boundaries"]] == source["boundaries"],
        "Unmapped/changed project boundary",
    )
    require(
        [row["source"] for row in cross["execution_rules"]] == source["execution_rules"],
        "Unmapped/changed execution rule",
    )
    require(
        cross["foundational_requirement"]["source"] == source["foundational_requirement"]
        and bool(source["foundational_requirement"]),
        "Unmapped/changed foundational requirement",
    )
    for table in ("feature_summary", "keyboard_defaults", "release_checkpoints", "agent_contract"):
        require(
            [row["source_row"] for row in cross[table]] == source["tables"][table],
            f"Unmapped/changed {table}",
        )
    exits = {f"P{row['phase']:02}": row["source"] for row in cross["phase_exits"]}
    require(exits == source["exits"], "Unmapped/changed phase exit criterion")
    rows = {row["id"]: row for row in matrix["requirements"]}
    for collection in cross.values():
        if isinstance(collection, dict):
            collection = [collection]
        if not isinstance(collection, list):
            continue
        for item in collection:
            require(
                item["status"] in {"defined", "passed", "failed", "blocked"}, "Invalid gate status"
            )
            if "tasks" in item:
                require(
                    bool(item["tasks"]) and set(item["tasks"]) <= source["tasks"].keys(),
                    "Invalid crosscutting task references",
                )
            if "facets" in item:
                require(
                    bool(item["facets"]) and set(item["facets"]) <= rows.keys(),
                    "Invalid default facet references",
                )
            if item["status"] == "passed":
                require(bool(item["evidence"]), "Gate pass requires executed evidence")
    validate_slice_structure(matrix)
    for gate in cross["phase_exits"]:
        if gate["phase"] == 1:
            continue
        require(gate["requires_all_phase_facets"] is True, "Phase gate must require all facets")
        if gate["status"] == "passed":
            require(
                all(
                    row["status"] == "passed"
                    for row in rows.values()
                    if row["phase"] == gate["phase"]
                ),
                "Phase exit cannot bypass incomplete facets",
            )
    for gate in cross["release_checkpoints"]:
        require(
            gate["requires_phase_exit_evidence"] is True,
            "Release gate must require phase exit evidence",
        )
        if gate["status"] == "passed":
            require(
                all(
                    item["status"] == "passed"
                    for item in cross["phase_exits"]
                    if item["phase"] in gate["required_phases"]
                ),
                "Release checkpoint cannot bypass incomplete phases",
            )


def validate(root: Path = ROOT) -> dict:
    matrix = build_matrix(root)
    for path, expected in generated_files(root).items():
        require(
            path.exists() and path.read_text() == expected, f"Stale generated file: {path.name}"
        )
    validate_traceability(matrix)
    validate_plan_completion((root / "plan.md").read_text(), matrix["requirements"])
    rows = unique(matrix["requirements"], "requirement matrix")
    targets = unique(load(root, "docs/platforms.json")["targets"], "platform targets")
    for row in rows.values():
        require(set(row["targets"]) <= targets.keys(), "Requirement uses an undeclared target")
    defaults = load(root, "docs/defaults.json")
    for default in defaults["prescribed"]:
        require(
            all(default["source"] + "-" + suffix in rows for suffix in default["facets"]),
            "Default has missing facet coverage",
        )

    profiles = unique(
        load(root, "analysis/reference-profiles.json")["profiles"], "reference profiles"
    )
    require(
        {row["intended_disc"] for row in profiles.values()} == {1, 2},
        "Both intended original discs must be selected",
    )
    for row in profiles.values():
        measurement = row["measurement"]
        for label in ("chd", "raw_track"):
            digest(measurement["source"][label]["sha256"], f"{row['id']} {label}")
            require(measurement["source"][label]["size"] > 0, "Empty reference source")
        digest(measurement["boot"]["executable"]["sha256"], "boot executable")
        require(
            measurement["container_verification"]["exit_code"] == 0,
            "Reference CHD integrity not verified",
        )
        require(
            row["revision"]["match_policy"] == "full_raw_track_sha256",
            "Revision must be content-addressed, not inferred from a name/serial",
        )
        require(
            bool(row["region"]["evidence"]) and bool(row["region"]["observed_marker"]),
            "Region needs original evidence",
        )
        if row["disc_sequence"] is not None:
            require(
                bool(row["disc_sequence_evidence"]),
                "Disc sequence cannot be inferred from filenames",
            )

    findings = {}
    for path in sorted((root / "analysis/findings").glob("*.json")):
        finding = load(root, path.relative_to(root).as_posix())
        validate_finding(finding, set(profiles))
        require(finding["id"] not in findings, "Duplicate finding ID")
        findings[finding["id"]] = finding
    registry = dict(findings)
    for path in sorted((root / "docs/verification").glob("*.json")):
        record = load(root, path.relative_to(root).as_posix())
        require(record["id"] not in registry, "Duplicate evidence ID")
        registry[record["id"]] = record
    for row in profiles.values():
        require(
            set(row["findings"]) <= findings.keys() and row["region"]["evidence"] in findings,
            "Reference profile lacks its original finding",
        )
    for row in rows.values():
        validate_result(row, registry)

    decisions = {path.stem for path in (root / "analysis/decisions").glob("*.json")}
    subsystems = load(root, "analysis/subsystems.json")
    require(subsystems["stages"] == STAGES, "Maturity ladder changed without schema review")
    for row in unique(subsystems["subsystems"], "subsystems").values():
        validate_subsystem(row, set(registry), decisions)
    inventory = load(root, "analysis/coverage/inventory.json")
    validate_inventory(inventory, set(profiles))
    validate_physical_inventory(root, profiles, findings)
    validate_projections(root, profiles, findings)
    validate_observed_entrypoints(
        load(root, "analysis/coverage/observed-entrypoints.json"),
        inventory,
        set(profiles),
        findings,
    )
    validate_baseline_inventory(
        root, inventory, findings, load(root, "analysis/coverage/observed-entrypoints.json")
    )
    if any(row["source_id"] == "P00-T03" and row["status"] == "passed" for row in rows.values()):
        require(
            inventory["baseline_inventory_ready"],
            "Phase 0 inventory facets require the original baseline inventory gate",
        )
    checkpoints = unique(
        load(root, "analysis/coverage/checkpoints.json")["checkpoints"], "checkpoints"
    )
    for disc in (1, 2):
        category_union = set().union(
            *(
                set(row["categories"])
                for row in checkpoints.values()
                if disc in row["intended_discs"]
            )
        )
        require(category_union == CATEGORIES, "Checkpoint definitions omit a disc/category")
    require(
        any(
            set(row["intended_discs"]) == {1, 2} and "CROSS-DISC" in row["id"]
            for row in checkpoints.values()
        ),
        "Cross-disc progression checkpoint omitted",
    )
    for row in checkpoints.values():
        require(
            bool(row["preconditions"])
            and bool(row["input_procedure"])
            and bool(row["capture_marks"])
            and bool(row["observables"]),
            "Incomplete checkpoint definition",
        )
        require(set(row["profiles"]) <= profiles.keys(), "Checkpoint refers to unknown source")
        if row["status"] == "defined_uncaptured":
            require(
                not row["evidence"] and row["oracle"]["expected_outcome"] is None,
                "Uncaptured checkpoint cannot claim observed results",
            )
    dependencies = load(root, "docs/dependencies.json")
    for item in dependencies["dependencies"]:
        require(
            all(
                item.get(key)
                for key in (
                    "name",
                    "version",
                    "source",
                    "pin",
                    "license_expression",
                    "purpose",
                    "scope",
                    "decision",
                    "redistribution",
                    "transitive_review",
                    "review",
                )
            ),
            "Incomplete dependency provenance/license review",
        )

    agent_contract = validate_agent_contract(root, matrix)
    files = source_files(root)
    names = {path.relative_to(root).as_posix() for path in files}
    validate_slice_evidence(root, matrix, findings, registry, set(profiles), names)
    if (root / ".git").exists():
        output = subprocess.check_output(
            ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=root
        )
        repository_files = {name for name in output.decode().split("\0") if name}
        require(
            repository_files <= names,
            "Unreviewed repository files absent from source allowlist: "
            f"{sorted(repository_files - names)}",
        )
    require(
        {path.name for path in (root / "nix").iterdir()}
        == {"flake.nix", "flake.lock", "reference-trace.h", "reference-trace-patch.py"},
        "Nix input boundary contains unexpected files",
    )
    require(
        "../" not in (root / "nix/flake.nix").read_text(),
        "Development flake must not import the repository/data parent",
    )
    return {
        "source_tasks": len(matrix["source_snapshot"]["tasks"]),
        "test_facets": len(rows),
        "phase_exits": len(matrix["source_snapshot"]["exits"]),
        "release_checkpoints": len(matrix["crosscutting"]["release_checkpoints"]),
        "source_profiles": len(profiles),
        "baseline_inventory_ready": inventory["baseline_inventory_ready"],
        "content_catalog_complete": inventory["catalog_complete"],
        "checkpoint_definitions": len(checkpoints),
        "public_sources": len(files),
        **agent_contract,
    }


def main() -> None:
    try:
        print(json.dumps(validate(), indent=2))
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f"Repository validation failed: {error}") from error


if __name__ == "__main__":
    main()
