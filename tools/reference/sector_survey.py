"""Survey raw-CD sector headers and XA-declared spans, without game-format inference."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
from typing import BinaryIO

if __package__:
    from .inspect_disc import RAW_SECTOR_SIZE, SYNC
else:
    from inspect_disc import RAW_SECTOR_SIZE, SYNC

ROOT = Path(__file__).resolve().parents[2]


def classify(raw: bytes) -> tuple[str, str]:
    if len(raw) != RAW_SECTOR_SIZE:
        raise ValueError("Truncated raw sector")
    if raw[:12] != SYNC:
        return "no_data_sync", "unclassified"
    mode = raw[15]
    if mode == 1:
        return "mode1", "mode1"
    if mode != 2:
        return f"mode{mode}_unclassified", f"mode{mode}"
    if raw[16:20] != raw[20:24]:
        raise ValueError("Mode 2 subheader copies disagree")
    file, channel, submode, coding = raw[16:20]
    form = "form2" if submode & 0x20 else "form1"
    declared = []
    for bit, name in ((0x02, "video"), (0x04, "audio"), (0x08, "data")):
        if submode & bit:
            declared.append(name)
    role = f"mode2_{form}_" + ("+".join(declared) or "untyped")
    signature = f"file={file},channel={channel},submode=0x{submode:02x},coding=0x{coding:02x}"
    return role, signature


def survey(stream: BinaryIO, expected_size: int) -> dict:
    if expected_size <= 0 or expected_size % RAW_SECTOR_SIZE:
        raise ValueError("Expected a nonempty whole raw track")
    total_sectors = expected_size // RAW_SECTOR_SIZE
    source_hash = hashlib.sha256()
    roles: Counter[str] = Counter()
    signatures: Counter[str] = Counter()
    spans = []
    record_markers = []
    lba = 0
    while block := stream.read(RAW_SECTOR_SIZE * 256):
        if len(block) % RAW_SECTOR_SIZE or lba + len(block) // RAW_SECTOR_SIZE > total_sectors:
            raise ValueError("Raw track length changed or ended in a partial sector")
        source_hash.update(block)
        for offset in range(0, len(block), RAW_SECTOR_SIZE):
            raw = block[offset : offset + RAW_SECTOR_SIZE]
            role, signature = classify(raw)
            roles[role] += 1
            signatures[signature] += 1
            if spans and spans[-1]["header_class"] == role:
                spans[-1]["end_lba_exclusive"] = lba + 1
            else:
                spans.append({"start_lba": lba, "end_lba_exclusive": lba + 1, "header_class": role})
            if raw[:12] == SYNC and raw[15] == 2 and raw[18] & 0x81:
                record_markers.append(
                    {
                        "lba": lba,
                        "xa_file": raw[16],
                        "xa_channel": raw[17],
                        "end_of_record": bool(raw[18] & 0x01),
                        "end_of_file": bool(raw[18] & 0x80),
                    }
                )
            lba += 1
    if lba != total_sectors:
        raise ValueError("Raw track shorter than its declared byte count")
    return {
        "raw_track_sha256": source_hash.hexdigest(),
        "raw_track_bytes": expected_size,
        "sector_bytes": RAW_SECTOR_SIZE,
        "surveyed_sectors": lba,
        "header_class_counts": dict(sorted(roles.items())),
        "xa_signature_counts": dict(sorted(signatures.items())),
        "contiguous_header_spans": spans,
        "xa_record_markers": record_markers,
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
        parser.error("Write initial survey measurements only under ignored .local/")
    profiles = json.loads((ROOT / "analysis/reference-profiles.json").read_text())["profiles"]
    profile = next((row for row in profiles if row["id"] == args.profile), None)
    if profile is None:
        parser.error("Unknown source profile")
    expected = profile["measurement"]["source"]["raw_track"]
    with args.raw.open("rb") as stream:
        result = survey(stream, expected["size"])
    if result["raw_track_sha256"] != expected["sha256"]:
        parser.error("Measured raw track does not match the exact selected profile")
    report = {
        "schema_version": 1,
        "source_profile": args.profile,
        "kind": "original_static_sector_header_survey",
        "tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "nix_lock_sha256": hashlib.sha256((ROOT / "nix/flake.lock").read_bytes()).hexdigest(),
        **result,
        "limitations": [
            "Header flags describe declared sector types, not independently decoded media.",
            "A contiguous header span is not necessarily one resource, audio track or FMV.",
            "Interleaving, padding and reused streams prevent deriving content counts here.",
            "Fields, event IDs, battle/minigame logic and archive tables remain unidentified.",
            "This survey does not verify CD ECC/EDC or original hardware behavior.",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(
        f"Surveyed {result['surveyed_sectors']} sectors; "
        f"{len(result['contiguous_header_spans'])} header spans; exact source hash confirmed"
    )


if __name__ == "__main__":
    main()
