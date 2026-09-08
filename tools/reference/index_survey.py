"""Measure a directly observed candidate source index; gameplay meanings remain unknown.

The 7-byte layout hypothesis was derived independently from selected original
sectors and checked against ISO boot extents and XA end markers. This read-only
inventory probe is not an asset extractor or a completed loader decompilation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

if __package__:
    from .inspect_disc import BLOCK_SIZE, RAW_SECTOR_SIZE, SYNC, RawCd, file_hash
else:
    from inspect_disc import BLOCK_SIZE, RAW_SECTOR_SIZE, SYNC, RawCd, file_hash

ROOT = Path(__file__).resolve().parents[2]
INDEX_LBA = 24
INDEX_BYTES = 15 * BLOCK_SIZE


def parse_candidates(data: bytes, track_sectors: int) -> dict:
    records = []
    terminal = None
    for offset in range(0, len(data) - 6, 7):
        lba = int.from_bytes(data[offset : offset + 3], "little")
        size = int.from_bytes(data[offset + 3 : offset + 7], "little", signed=True)
        index = offset // 7
        if lba == 0xFFFFFF and size == 0:
            if any(data[offset + 7 :]):
                raise ValueError("Nonzero bytes after candidate terminal marker")
            terminal = {"slot": index, "table_byte_offset": offset, "lba_value": lba}
            break
        if not 0 <= lba < track_sectors:
            raise ValueError(f"Candidate source LBA outside track at slot {index}")
        if size > track_sectors * 2336:
            raise ValueError(f"Candidate length exceeds whole track at slot {index}")
        kind = "resource_candidate"
        if lba == 0 and size == 0:
            kind = "zero_slot_unknown_or_padding"
        elif size == 0:
            kind = "zero_length_marker_unknown"
        elif size < 0:
            kind = "negative_count_header_candidate"
        records.append(
            {
                "slot": index,
                "table_byte_offset": offset,
                "source_lba": lba,
                "signed_length_candidate": size,
                "kind": kind,
            }
        )
    if terminal is None:
        raise ValueError("Candidate terminal marker absent or truncated")
    groups = []
    for row in records:
        count = -row["signed_length_candidate"]
        if count <= 0:
            continue
        first, end = row["slot"] + 1, row["slot"] + 1 + count
        if end > len(records) or any(
            child["kind"] != "resource_candidate" for child in records[first:end]
        ):
            raise ValueError(
                f"Negative count does not match immediate positive children at slot {row['slot']}"
            )
        if records[first]["source_lba"] != row["source_lba"]:
            raise ValueError("Candidate group header and first child disagree on start LBA")
        groups.append(
            {
                "header_slot": row["slot"],
                "table_byte_offset": row["table_byte_offset"],
                "source_lba": row["source_lba"],
                "positive_child_count": count,
                "first_child_slot": first,
                "end_child_slot_exclusive": end,
                "gameplay_category": "unknown",
            }
        )
    return {"records": records, "groups": groups, "terminal": terminal}


def end_marker(stream, lba: int, track_sectors: int) -> bool:
    if not 0 <= lba < track_sectors:
        return False
    stream.seek(lba * RAW_SECTOR_SIZE)
    header = stream.read(24)
    return (
        len(header) == 24
        and header[:12] == SYNC
        and header[15] == 2
        and header[16:20] == header[20:24]
        and bool(header[18] & 0x80)
    )


def measure(raw: Path, profile: dict) -> dict:
    expected = profile["measurement"]["source"]["raw_track"]
    if raw.stat().st_size != expected["size"] or file_hash(raw) != expected["sha256"]:
        raise ValueError("Raw track does not match exact selected profile")
    with raw.open("rb") as stream:
        cd = RawCd(stream, expected["size"])
        tag_data = cd.read_sector(23)
        tag = tag_data.split(b"\x00", 1)[0].decode("ascii", errors="strict")
        tag_padding_zero = not any(tag_data[len(tag) :])
        table = cd.read_extent(INDEX_LBA, INDEX_BYTES)
        candidates = parse_candidates(table, cd.sectors)
        auxiliary = cd.read_sector(40)
        unmatched = []
        for row in candidates["records"]:
            if row["kind"] != "resource_candidate":
                continue
            row["xa_eof_matches"] = {}
            for unit in (2048, 2336):
                end = row["source_lba"] + (row["signed_length_candidate"] + unit - 1) // unit
                row["xa_eof_matches"][str(unit)] = end_marker(stream, end - 1, cd.sectors)
            if not any(row["xa_eof_matches"].values()):
                unmatched.append(row["slot"])
    known = {}
    for key in ("system_cnf", "executable"):
        item = profile["measurement"]["boot"][key]
        known[key] = [
            row["slot"]
            for row in candidates["records"]
            if row["source_lba"] == item["lba"] and row["signed_length_candidate"] == item["size"]
        ]
        if len(known[key]) != 1:
            raise ValueError(f"Candidate table disagrees with independent ISO {key} extent")
    if unmatched:
        raise ValueError(f"Candidate extents lack an XA EOF under either tested unit: {unmatched}")
    return {
        "schema_version": 1,
        "kind": "original_static_candidate_index_survey",
        "source_profile": profile["id"],
        "raw_track_sha256": expected["sha256"],
        "tool_sha256": file_hash(Path(__file__)),
        "nix_lock_sha256": file_hash(ROOT / "nix/flake.lock"),
        "source_tag": {
            "lba": 23,
            "user_data_offset": 24,
            "text": tag,
            "remaining_logical_block_zero": tag_padding_zero,
        },
        "table": {
            "lba": INDEX_LBA,
            "logical_bytes_surveyed": INDEX_BYTES,
            "record_bytes_hypothesis": 7,
            "sha256": hashlib.sha256(table).hexdigest(),
        },
        "crosschecks": {
            "known_iso_records": known,
            "unmatched_xa_eof_slots": unmatched,
            "negative_headers_matching_immediate_children": len(candidates["groups"]),
        },
        **candidates,
        "auxiliary_sample": {
            "lba": 40,
            "whole_block_sha256": hashlib.sha256(auxiliary).hexdigest(),
            "first_64_u16le_values": [
                int.from_bytes(auxiliary[i : i + 2], "little") for i in range(0, 128, 2)
            ],
            "semantics": "unknown; sample is not a declared complete directory-ID table",
        },
        "limitations": [
            "Slots are source coordinates/project keys, not recovered runtime content IDs.",
            "Negative counts match consecutive entries; directory meaning needs loader evidence.",
            "EOF checks corroborate boundaries; size units remain ambiguous for small files.",
            "Zero slots/length markers retain unknown meaning and are never executed as no-ops.",
            "No field, battle, menu, audio, FMV, save or minigame meaning is assigned here.",
            "This read-only inventory probe is not a game decoder, extractor or native loader.",
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if Path.cwd().resolve() != ROOT:
        parser.error("Run from the repository root")
    if not args.output.resolve().is_relative_to((ROOT / ".local").resolve()):
        parser.error("Write initial measurements under ignored .local/")
    profiles = json.loads((ROOT / "analysis/reference-profiles.json").read_text())["profiles"]
    profile = next((row for row in profiles if row["id"] == args.profile), None)
    if profile is None:
        parser.error("Unknown reference profile")
    report = measure(args.raw, profile)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    counts = {
        kind: sum(row["kind"] == kind for row in report["records"])
        for kind in sorted({row["kind"] for row in report["records"]})
    }
    print(
        json.dumps(
            {
                "source_tag": report["source_tag"],
                "counts": counts,
                "crosschecks": report["crosschecks"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
