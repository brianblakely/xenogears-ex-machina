"""Keep public field availability and projection hashes bound to measured source slots."""

from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def validate_projections(root: Path, profiles: dict, findings: dict) -> None:
    documents = {}
    for name in ("source-index", "source-fingerprints", "field-pairs"):
        data = json.loads((root / f"analysis/coverage/{name}.json").read_text())
        require(data["schema_version"] == 1, "Unknown projection inventory schema")
        require(
            data["content_catalog_complete"] is False,
            "Source projections cannot prove game completeness",
        )
        mapped = {row["source_profile"]: row for row in data["profiles"]}
        require(
            len(mapped) == len(data["profiles"]) and mapped.keys() == profiles.keys(),
            "Projection profiles omit or duplicate an original source",
        )
        documents[name] = data, mapped
    data, projections = documents["source-fingerprints"]
    availability, pairs = documents["field-pairs"]
    _, indices = documents["source-index"]
    inventory = json.loads((root / "analysis/coverage/inventory.json").read_text())
    require(
        bool(data["evidence"])
        and set(data["evidence"]) <= findings.keys()
        and set(availability["physical_evidence"]) <= findings.keys()
        and set(availability["mapping_evidence"]) <= findings.keys(),
        "Source projection availability needs original findings",
    )
    columns = [
        "slot",
        "source_lba",
        "source_sector_count",
        "projected_bytes",
        "projection_unit",
        "projection_sha256",
        "all_projected_bytes_zero",
        "following_sector_gap",
    ]
    distinct = {}
    for profile, row in projections.items():
        require(row["records_columns"] == columns, "Unknown or private projection column")
        index, pair_data = indices[profile], pairs[profile]
        require(
            any(
                findings[key].get("validation", {}).get("result") == "passed"
                and any(
                    correlation["source_profile"] == profile
                    and correlation["fat_sha256"] == index["table"]["sha256"]
                    and correlation["field_pair_first_slot"] == pair_data["first_child_slot"]
                    for correlation in findings[key].get("source_correlations", [])
                )
                for key in availability["mapping_evidence"]
            ),
            "Field selector mapping lacks matching original loader evidence",
        )
        raw = profiles[profile]["measurement"]["source"]["raw_track"]
        for record in (row, pair_data):
            require(
                record["raw_track_sha256"] == raw["sha256"]
                and record["source_index_table_sha256"] == index["table"]["sha256"],
                "Projection source/table identity changed",
            )
        originals = [record for record in index["records"] if record[4] == "resource_candidate"]
        require(
            len(row["records"]) == len(originals),
            "Projection inventory omits original source slots",
        )
        by_slot, keys = {}, set()
        for position, (record, source) in enumerate(zip(row["records"], originals, strict=True)):
            require(len(record) == len(columns), "Incomplete source projection")
            slot, lba, count, size, unit, sha, zero, gap = record
            require(
                [slot, lba, size] == [source[0], source[2], source[3]],
                "Projection contradicts its original source-index slot",
            )
            require(
                unit in (2048, 2336) and count == (size + unit - 1) // unit and type(zero) is bool,
                "Invalid projection dimensions or zero classification",
            )
            next_lba = (
                originals[position + 1][2] if position + 1 < len(originals) else raw["size"] // 2352
            )
            require(
                gap == next_lba - lba - count and gap >= 0,
                "Projection overlaps or misstates its next boundary",
            )
            require(
                isinstance(sha, str) and re.fullmatch(r"[a-f0-9]{64}", sha) is not None,
                "Invalid source projection SHA256",
            )
            by_slot[slot] = dict(zip(columns, record, strict=True))
            keys.add((unit, size, sha))
        require(
            row["summary"]["positive_source_slots"] == len(by_slot)
            and row["summary"]["distinct_projection_keys"] == len(keys),
            "Projection summary confuses original slots and distinct hashes",
        )
        distinct[profile] = keys
        group = next(
            group
            for group in index["groups"]
            if group["header_slot"] == pair_data["group_header_slot"]
        )
        require(
            pair_data["first_child_slot"] == group["first_child_slot"]
            and len(pair_data["pairs"]) == pair_data["pair_count"]
            and 2 * pair_data["pair_count"] == group["positive_child_count"],
            "Field pairs do not cover their measured original group",
        )
        dummy_count = 0
        for number, pair in enumerate(pair_data["pairs"]):
            slots = [pair_data["first_child_slot"] + number * 2 + offset for offset in (0, 1)]
            sources = [by_slot[slot] for slot in slots]
            markers = [
                all(
                    source[field] == availability["dummy_marker"][field]
                    for field in ("projection_unit", "projected_bytes", "projection_sha256")
                )
                for source in sources
            ]
            require(
                pair["candidate_map_index"] == number and pair["source_slots"] == slots,
                "Field-pair index does not identify its original source slots",
            )
            for field, source_field in (
                ("source_lbas", "source_lba"),
                ("projected_bytes", "projected_bytes"),
                ("projection_sha256", "projection_sha256"),
            ):
                require(
                    pair[field] == [source[source_field] for source in sources],
                    "Field pair contradicts original projection metadata",
                )
            require(
                pair["exact_cdmake_dummy"] == markers,
                "Field pair contradicts exact original dummy markers",
            )
            require(
                not any(markers) or all(markers),
                "Mixed dummy pair requires an explicit new availability state",
            )
            expected = "exact_dummy_source_pair" if all(markers) else "non_dummy_source_pair"
            require(
                pair["availability"] == expected,
                "Field-pair availability misclassifies original bytes",
            )
            dummy_count += all(markers)
        require(
            pair_data["exact_dummy_source_pairs"] == dummy_count
            and pair_data["non_dummy_source_pairs"] == pair_data["pair_count"] - dummy_count,
            "Field-pair availability counts are inconsistent",
        )
        field = next(
            item
            for item in inventory["content"]
            if item["profile"] == profile and item["category"] == "fields"
        )
        available_ids = [
            item["candidate_map_index"]
            for item in pair_data["pairs"]
            if item["availability"] == "non_dummy_source_pair"
        ]
        dummy_ids = [
            item["candidate_map_index"]
            for item in pair_data["pairs"]
            if item["availability"] == "exact_dummy_source_pair"
        ]
        require(
            field["catalogued_original_content_ids"] == available_ids
            and field["catalogued_count"] == len(available_ids)
            and field["excluded_dummy_original_ids"] == dummy_ids
            and set(field["original_content_ids"]) <= set(available_ids),
            "Field catalog confuses recovered source IDs, dummy data or observed scenes",
        )
    shared = set.intersection(*distinct.values())
    require(
        data["comparison"]["distinct_projection_keys_shared_between_sources"] == len(shared),
        "Shared-projection count contradicts actual hashes",
    )
    for row in data["comparison"]["profiles"]:
        keys = distinct[row["source_profile"]]
        require(
            row["distinct_projections"] == len(keys)
            and row["distinct_projections_absent_on_other_source"] == len(keys - shared),
            "Compared distinct-projection counts disagree",
        )
    validate_media_catalog(root, projections, inventory, findings)


def validate_media_catalog(root: Path, projections: dict, inventory: dict, findings: dict) -> None:
    data = json.loads((root / "analysis/coverage/media.json").read_text())
    require(
        data["schema_version"] == 1
        and data["content_catalog_complete"] is False
        and bool(data["evidence"])
        and set(data["evidence"]) <= findings.keys(),
        "Media catalog lacks original evidence or overstates completeness",
    )
    sources = {row["source_profile"]: row for row in data["profiles"]}
    require(
        len(sources) == len(data["profiles"]) and sources.keys() == projections.keys(),
        "Media catalog omits or duplicates a selected source",
    )
    audio_by_profile = []
    for profile, source in sources.items():
        original = projections[profile]
        require(
            source["raw_track_sha256"] == original["raw_track_sha256"]
            and source["source_index_table_sha256"] == original["source_index_table_sha256"],
            "Media catalog source identity changed",
        )
        by_slot = {
            row[0]: dict(zip(original["records_columns"], row, strict=True))
            for row in original["records"]
        }
        seen = set()
        for row in source["movies"] + source["audio_headers"]:
            require(row["slot"] not in seen, "Media catalog repeats an original source slot")
            seen.add(row["slot"])
            require(
                row["slot"] in by_slot
                and all(
                    row[key] == by_slot[row["slot"]][key]
                    for key in (
                        "source_lba",
                        "source_sector_count",
                        "projected_bytes",
                        "projection_unit",
                        "projection_sha256",
                    )
                ),
                "Media entry contradicts its measured original source projection",
            )
        require(
            [row["original_movie_selector"] for row in source["movies"]]
            == list(range(source["summary"]["original_movie_selectors"]))
            and {row["slot"] for row in source["movies"]}
            == {row["slot"] for row in by_slot.values() if row["projection_unit"] == 2336},
            "Movie catalog omits or invents counted original selectors",
        )
        for row in source["movies"]:
            require(
                row["slot"] == row["original_movie_selector"] + 1
                and sum(row["xa_role_counts"].values()) == row["source_sector_count"]
                and sum(row["video_magic_counts"].values()) == row["xa_role_counts"]["video"],
                "Movie selector or packet accounting disagrees with original source extent",
            )
        tags = Counter(row["signature"] for row in source["audio_headers"])
        require(
            set(tags) <= {"wds ", "smds", "seds"}
            and dict(tags) == source["summary"]["audio_signature_counts"],
            "Audio signature counts disagree with catalogued source entries",
        )
        for tag in tags:
            require(
                len(
                    {
                        row["projection_sha256"]
                        for row in source["audio_headers"]
                        if row["signature"] == tag
                    }
                )
                == source["summary"]["audio_distinct_projection_counts"][tag],
                "Audio catalog confuses source slots and distinct byte projections",
            )
        require(
            [row["slot"] for row in source["audio_headers"] if row["header_word_sum_u32"]]
            == source["summary"]["audio_nonzero_word_sum_slots"]
            and sorted({row["header_version_u16"] for row in source["audio_headers"]})
            == source["summary"]["audio_versions"],
            "Audio catalog silently hides original header variants",
        )
        for category, rows, id_field in (
            ("fmvs", source["movies"], "original_movie_selector"),
            ("audio", source["audio_headers"], "slot"),
        ):
            content = next(
                row
                for row in inventory["content"]
                if row["profile"] == profile and row["category"] == category
            )
            require(
                content["catalogued_original_content_ids"] == [row[id_field] for row in rows]
                and content["catalogued_count"] == len(rows),
                "Media inventory counts or IDs disagree with the original source catalog",
            )
        audio_by_profile.append({row["slot"]: row for row in source["audio_headers"]})
    pairs = data["cross_disc_audio_correspondence"]["source_slot_pairs"]
    require(
        len(pairs) == len(audio_by_profile[0]) == len(audio_by_profile[1])
        and {row["disc1_slot"] for row in pairs} == audio_by_profile[0].keys()
        and {row["disc2_slot"] for row in pairs} == audio_by_profile[1].keys(),
        "Audio source-pair comparison omits or repeats original entries",
    )
    for pair in pairs:
        originals = [audio_by_profile[i][pair[f"disc{i + 1}_slot"]] for i in (0, 1)]
        require(
            all(
                all(row[key] == pair[key] for key in ("projection_sha256", "signature"))
                for row in originals
            ),
            "Claimed shared audio data differs between original sources",
        )
