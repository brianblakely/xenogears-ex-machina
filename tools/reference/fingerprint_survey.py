"""Fingerprint explicit source-byte projections without decoding game resources."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
from typing import BinaryIO

if __package__:
    from .index_survey import measure as measure_index
    from .inspect_disc import RAW_SECTOR_SIZE, SYNC, RawCd, file_hash
else:
    from index_survey import measure as measure_index
    from inspect_disc import RAW_SECTOR_SIZE, SYNC, RawCd, file_hash

ROOT = Path(__file__).resolve().parents[2]


def fingerprint(stream: BinaryIO, lba: int, size: int, unit: int, sectors: int) -> dict:
    if unit not in (2048, 2336) or size <= 0 or lba < 0:
        raise ValueError("Invalid fingerprint projection")
    count = (size + unit - 1) // unit
    if lba + count > sectors:
        raise ValueError("Fingerprint extent exceeds original track")
    cd = RawCd(stream, sectors * RAW_SECTOR_SIZE)
    digest, prefix, remaining, zero = hashlib.sha256(), bytearray(), size, True
    for source_lba in range(lba, lba + count):
        if unit == 2048:
            data = cd.read_sector(source_lba)
        else:
            stream.seek(source_lba * RAW_SECTOR_SIZE)
            raw = stream.read(RAW_SECTOR_SIZE)
            if (
                len(raw) != RAW_SECTOR_SIZE
                or raw[:12] != SYNC
                or raw[15] != 2
                or raw[16:20] != raw[20:24]
            ):
                raise ValueError("Invalid Mode 2 sector in 2336-byte projection")
            data = raw[16:]
        data = data[:remaining]
        digest.update(data)
        prefix.extend(data[: max(0, 64 - len(prefix))])
        zero = zero and not any(data)
        remaining -= len(data)
    if remaining:
        raise ValueError("Truncated fingerprint projection")
    return {
        "source_lba": lba,
        "source_sector_count": count,
        "projected_bytes": size,
        "projection_unit": unit,
        "projection_sha256": digest.hexdigest(),
        "all_projected_bytes_zero": zero,
        "private_prefix64_hex": prefix.hex(),
    }


def projection_key(row: dict) -> tuple:
    return row["projection_unit"], row["projected_bytes"], row["projection_sha256"]


def scan(raw: Path, profile: dict) -> dict:
    index = measure_index(raw, profile)
    sectors = profile["measurement"]["source"]["raw_track"]["size"] // RAW_SECTOR_SIZE
    candidates = [row for row in index["records"] if row["kind"] == "resource_candidate"]
    records = []
    with raw.open("rb") as stream:
        for position, candidate in enumerate(candidates):
            unit = 2048 if candidate["xa_eof_matches"]["2048"] else 2336
            row = fingerprint(
                stream,
                candidate["source_lba"],
                candidate["signed_length_candidate"],
                unit,
                sectors,
            )
            next_start = (
                candidates[position + 1]["source_lba"]
                if position + 1 < len(candidates)
                else sectors
            )
            end = row["source_lba"] + row["source_sector_count"]
            if end > next_start:
                raise ValueError("Selected projection overlaps the following candidate extent")
            records.append(
                {
                    "slot": candidate["slot"],
                    **row,
                    "following_sector_gap": next_start - end,
                }
            )
    by_slot = {row["slot"]: row for row in records}
    for key, slots in index["crosschecks"]["known_iso_records"].items():
        known = profile["measurement"]["boot"][key]
        if by_slot[slots[0]]["projection_sha256"] != known["sha256"]:
            raise ValueError(f"Projection disagrees with independent ISO {key} hash")
    return {
        "schema_version": 1,
        "kind": "original_source_projection_fingerprints",
        "source_profile": profile["id"],
        "raw_track_sha256": index["raw_track_sha256"],
        "source_index_table_sha256": index["table"]["sha256"],
        "tool_sha256": file_hash(Path(__file__)),
        "index_tool_sha256": index["tool_sha256"],
        "nix_lock_sha256": file_hash(ROOT / "nix/flake.lock"),
        "projection_policy": {
            "2048": "Bounded Mode 1/Mode 2 Form 1 logical user-data bytes, "
            "clipped to the candidate length.",
            "2336": "Bytes 16..2351 of each complete validated Mode 2 sector, "
            "clipped to candidate length; includes XA subheaders and redundant/check bytes.",
            "selection": "Use 2048 when its candidate final sector has the measured XA EOF; "
            "otherwise use the corroborated 2336 candidate. Both must fit before the next start. "
            "This is an explicit byte projection, not a recovered game-loader size convention.",
        },
        "records": records,
        "groups": index["groups"],
        "summary": {
            "positive_source_slots": len(records),
            "distinct_projection_keys": len({projection_key(row) for row in records}),
            "all_zero_projection_slots": sum(row["all_projected_bytes_zero"] for row in records),
            "projection_unit_counts": dict(
                sorted(Counter(row["projection_unit"] for row in records).items())
            ),
            "gaps_after_slots": [
                {"slot": row["slot"], "sectors": row["following_sector_gap"]}
                for row in records
                if row["following_sector_gap"]
            ],
        },
        "limitations": [
            "Equal unit/length/SHA256 means equal selected source-byte projections, "
            "not equal gameplay behavior or unique original content IDs.",
            "Duplicate and zero-byte projections remain separate source slots; "
            "their loader/game meaning is unresolved.",
            "The 2336-byte projection is not an audio/video decoder or media asset count.",
            "Private prefixes aid direct inspection and must not enter public metadata "
            "or distributed assets.",
            "No native extractor, decoder, loader or game implementation is provided.",
        ],
    }


def compare(reports: list[dict]) -> dict:
    if len(reports) != 2 or len({report["source_profile"] for report in reports}) != 2:
        raise ValueError("Compare exactly two distinct original reference profiles")
    projections = [{projection_key(row) for row in report["records"]} for report in reports]
    groups = []
    for index, report in enumerate(reports):
        by_slot = {row["slot"]: row for row in report["records"]}
        if len(by_slot) != len(report["records"]):
            raise ValueError("Fingerprint report duplicates a source slot")
        summaries = []
        for group in report["groups"]:
            children = [
                by_slot[slot]
                for slot in range(group["first_child_slot"], group["end_child_slot_exclusive"])
            ]
            summaries.append(
                {
                    "header_slot": group["header_slot"],
                    "source_child_count": len(children),
                    "slots_matching_some_projection_on_other_source": sum(
                        projection_key(row) in projections[1 - index] for row in children
                    ),
                }
            )
        groups.append(
            {
                "source_profile": report["source_profile"],
                "raw_track_sha256": report["raw_track_sha256"],
                "distinct_projections": len(projections[index]),
                "distinct_projections_absent_on_other_source": len(
                    projections[index] - projections[1 - index]
                ),
                "group_summaries": summaries,
            }
        )
    return {
        "schema_version": 1,
        "kind": "source_projection_hash_comparison",
        "tool_sha256": file_hash(Path(__file__)),
        "nix_lock_sha256": file_hash(ROOT / "nix/flake.lock"),
        "distinct_projection_keys_shared_between_sources": len(projections[0] & projections[1]),
        "profiles": groups,
        "content_catalog_complete": False,
        "interpretation": "Equality of the explicit unit/length/SHA256 projection key only. "
        "These are physical-byte relationships; they neither count unique gameplay content "
        "nor establish equivalent behavior.",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action", required=True)
    scan_parser = actions.add_parser("scan")
    scan_parser.add_argument("--raw", type=Path, required=True)
    scan_parser.add_argument("--profile", required=True)
    compare_parser = actions.add_parser("compare")
    compare_parser.add_argument("reports", type=Path, nargs=2)
    for subparser in (scan_parser, compare_parser):
        subparser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if Path.cwd().resolve() != ROOT:
        parser.error("Run from the repository root")
    if not args.output.resolve().is_relative_to((ROOT / ".local").resolve()):
        parser.error("Initial fingerprint measurements must stay under ignored .local/")
    if args.output.exists():
        parser.error("Output path must be new")
    if args.action == "scan":
        profiles = json.loads((ROOT / "analysis/reference-profiles.json").read_text())["profiles"]
        profile = next((row for row in profiles if row["id"] == args.profile), None)
        if profile is None:
            parser.error("Unknown original reference profile")
        report = scan(args.raw, profile)
    else:
        inputs = [json.loads(path.read_text()) for path in args.reports]
        if any(row.get("kind") != "original_source_projection_fingerprints" for row in inputs):
            parser.error("Expected measured source-projection fingerprint reports")
        report = compare(inputs)
        report["input_reports"] = [
            {"path": str(path), "sha256": file_hash(path)} for path in args.reports
        ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(
        json.dumps(
            report.get(
                "summary",
                {
                    "shared_projection_keys": report.get(
                        "distinct_projection_keys_shared_between_sources"
                    )
                },
            )
        )
    )


if __name__ == "__main__":
    main()
