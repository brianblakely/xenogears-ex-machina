"""Inventory original movie packets and audio headers without decoding media payloads."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path

if __package__:
    from .index_survey import measure as measure_index
    from .inspect_disc import RAW_SECTOR_SIZE, SYNC, RawCd, file_hash
else:
    from index_survey import measure as measure_index
    from inspect_disc import RAW_SECTOR_SIZE, SYNC, RawCd, file_hash

ROOT = Path(__file__).resolve().parents[2]
AUDIO_SIGNATURES = (b"wds ", b"smds", b"seds")


def audio_header(data: bytes) -> dict:
    if len(data) < 32 or data[:4] not in AUDIO_SIGNATURES:
        raise ValueError("Unsupported or truncated audio header")
    size = struct.unpack_from("<I", data, 8)[0]
    aligned = (size + 3) & ~3
    if not 16 <= aligned <= len(data):
        raise ValueError("Audio header-declared checksum span exceeds source bounds")
    record = {
        "signature": data[:4].decode("ascii"),
        "header_version_u16": struct.unpack_from("<H", data, 12)[0],
        "header_declared_checksum_bytes": size,
        "aligned_checksum_bytes": aligned,
        "header_word_sum_u32": sum(value[0] for value in struct.iter_unpack("<I", data[:aligned]))
        & 0xFFFFFFFF,
        "declared_header_span_sha256": hashlib.sha256(data[:aligned]).hexdigest(),
    }
    if data[:4] == b"wds ":
        header_size, payload_size, payload_offset = struct.unpack_from("<III", data, 16)
        record["wave_region_candidate"] = {
            "header_bytes": header_size,
            "payload_bytes": payload_size,
            "payload_offset": payload_offset,
            "fits_source": payload_offset + payload_size <= len(data),
            "matches_full_source_extent": payload_offset + payload_size == len(data),
        }
    if data[:4] == b"smds":
        record["explicit_movie_dummy_music_label"] = b"Movie dammy music" in data[:256]
    return record


class MovieHeaders:
    """Measure packet relationships; unknown or inconsistent metadata stays visible."""

    def __init__(self) -> None:
        self.frame_chunks: dict[tuple, set] = defaultdict(set)
        self.frame_headers: dict[tuple, tuple] = {}
        self.roles: Counter = Counter()
        self.xa_pairs: Counter = Counter()
        self.magics: Counter = Counter()
        self.dimensions: Counter = Counter()
        self.duplicates: list[int] = []
        self.disagreements: list[int] = []
        self.invalid_chunks: list[int] = []
        self.digest = hashlib.sha256()

    def consume(self, lba: int, raw: bytes) -> None:
        if (
            not 0 <= lba <= 0xFFFFFFFF
            or len(raw) != RAW_SECTOR_SIZE
            or raw[:12] != SYNC
            or raw[15] != 2
            or raw[16:20] != raw[20:24]
        ):
            raise ValueError("Invalid original movie sector")
        self.digest.update(lba.to_bytes(4, "little") + raw[:56])
        self.xa_pairs[(raw[16], raw[17])] += 1
        role = "video" if raw[18] & 2 else "audio" if raw[18] & 4 else "other"
        self.roles[role] += 1
        if role != "video":
            return
        magic, chunk, count, frame, size, width, height = struct.unpack_from("<IHHIIHH", raw, 24)
        self.magics[f"{magic:08x}"] += 1
        self.dimensions[(width, height)] += 1
        key, value = (raw[16], raw[17], frame), (count, size, width, height)
        if not 0 <= chunk < count:
            self.invalid_chunks.append(lba)
        if key in self.frame_headers and self.frame_headers[key] != value:
            self.disagreements.append(lba)
        self.frame_headers[key] = value
        if chunk in self.frame_chunks[key]:
            self.duplicates.append(lba)
        self.frame_chunks[key].add(chunk)

    def summary(self) -> dict:
        missing = [
            {
                "xa_file": key[0],
                "xa_channel": key[1],
                "frame": key[2],
                "expected_chunks": self.frame_headers[key][0],
                "observed_chunks": sorted(parts),
            }
            for key, parts in self.frame_chunks.items()
            if len(parts) != self.frame_headers[key][0]
            or (parts and (min(parts) != 0 or max(parts) != len(parts) - 1))
        ]
        numbers = sorted({key[2] for key in self.frame_chunks})
        return {
            "header_scan_sha256": self.digest.hexdigest(),
            "xa_role_counts": dict(self.roles),
            "xa_file_channel_counts": [
                {"file": key[0], "channel": key[1], "sectors": count}
                for key, count in sorted(self.xa_pairs.items())
            ],
            "video_magic_counts": dict(self.magics),
            "dimension_candidates": [
                {"width": key[0], "height": key[1], "video_sectors": count}
                for key, count in sorted(self.dimensions.items())
            ],
            "video_frame_header_count": len(self.frame_chunks),
            "frame_number_min": min(numbers) if numbers else None,
            "frame_number_max": max(numbers) if numbers else None,
            "frame_numbers_contiguous": bool(numbers)
            and len(numbers) == numbers[-1] - numbers[0] + 1,
            "missing_or_incomplete_frames": missing,
            "duplicate_video_chunk_lbas": self.duplicates,
            "disagreeing_frame_header_lbas": self.disagreements,
            "invalid_chunk_lbas": self.invalid_chunks,
        }


def measure(raw: Path, profile: dict) -> dict:
    index = measure_index(raw, profile)
    movies, audio = [], []
    group = next(group for group in index["groups"] if group["header_slot"] == 0)
    with raw.open("rb") as stream:
        cd = RawCd(stream, raw.stat().st_size)
        directory = struct.unpack("<64H", cd.read_sector(40)[:128])
        if directory[25] != 1:
            raise ValueError("Movie directory differs from the recovered original mapping")
        for source in index["records"]:
            if source["kind"] != "resource_candidate":
                continue
            lba, size = source["source_lba"], source["signed_length_candidate"]
            unit = 2048 if source["xa_eof_matches"]["2048"] else 2336
            count = (size + unit - 1) // unit
            record = {
                "slot": source["slot"],
                "source_lba": lba,
                "source_sector_count": count,
                "projected_bytes": size,
                "projection_unit": unit,
            }
            if unit == 2048:
                header = cd.read_extent(lba, min(size, 64))
                if header[:4] not in AUDIO_SIGNATURES:
                    continue
                data = cd.read_extent(lba, size)
                audio.append(
                    {
                        **record,
                        "projection_sha256": hashlib.sha256(data).hexdigest(),
                        **audio_header(data),
                    }
                )
                continue
            if not group["first_child_slot"] <= source["slot"] < group["end_child_slot_exclusive"]:
                raise ValueError("Unreviewed streamed resource outside the original movie group")
            packets, digest, remaining = MovieHeaders(), hashlib.sha256(), size
            for sector_lba in range(lba, lba + count):
                stream.seek(sector_lba * RAW_SECTOR_SIZE)
                sector = stream.read(RAW_SECTOR_SIZE)
                packets.consume(sector_lba, sector)
                payload = sector[16 : 16 + min(remaining, 2336)]
                digest.update(payload)
                remaining -= len(payload)
            if remaining:
                raise ValueError("Truncated original movie projection")
            movies.append(
                {
                    **record,
                    "projection_sha256": digest.hexdigest(),
                    "original_movie_selector": source["slot"] - 1,
                    **packets.summary(),
                }
            )
    if len(movies) != group["positive_child_count"]:
        raise ValueError("Movie packet catalog omits part of the original counted group")
    signatures = Counter(row["signature"] for row in audio)
    return {
        "schema_version": 1,
        "kind": "original_media_header_inventory",
        "source_profile": profile["id"],
        "raw_track_sha256": index["raw_track_sha256"],
        "source_index_table_sha256": index["table"]["sha256"],
        "tool_sha256": file_hash(Path(__file__)),
        "index_tool_sha256": index["tool_sha256"],
        "nix_lock_sha256": file_hash(ROOT / "nix/flake.lock"),
        "movie_source_group": 0,
        "movie_directory_halfword_index": 25,
        "movie_directory_value": directory[25],
        "movies": movies,
        "audio_headers": audio,
        "summary": {
            "original_movie_selectors": len(movies),
            "audio_signature_counts": dict(signatures),
            "audio_distinct_projection_counts": {
                tag: len({row["projection_sha256"] for row in audio if row["signature"] == tag})
                for tag in signatures
            },
            "audio_nonzero_word_sum_slots": [
                row["slot"] for row in audio if row["header_word_sum_u32"]
            ],
            "audio_versions": sorted({row["header_version_u16"] for row in audio}),
            "explicit_movie_dummy_music_slots": [
                row["slot"] for row in audio if row.get("explicit_movie_dummy_music_label")
            ],
            "incomplete_movie_slots": [
                row["slot"]
                for row in movies
                if row["missing_or_incomplete_frames"]
                or row["duplicate_video_chunk_lbas"]
                or row["disagreeing_frame_header_lbas"]
                or row["invalid_chunk_lbas"]
            ],
        },
        "limitations": [
            "Movie selectors follow the separately recovered original loader path; "
            "source presence and complete packet headers do not prove decoded playback.",
            "Audio signature counts preserve source slots and duplicates; they are not "
            "song, instrument, sound-effect or audible-event totals.",
            "The original additive-word helper is applied as a header diagnostic; "
            "older version semantics remain unknown and nonzero sums do not imply bad discs.",
            "Dimensions are header candidates; no picture/audio payload is decoded here.",
            "No native extractor, media player, sequence interpreter or runtime is implemented.",
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    profiles = json.loads((ROOT / "analysis/reference-profiles.json").read_text())["profiles"]
    profile = next((row for row in profiles if row["id"] == args.profile), None)
    output = args.output.resolve()
    if profile is None or not output.is_relative_to(ROOT / ".local") or output.exists():
        parser.error("Select a known source profile and a new ignored .local output path")
    report = measure(args.raw, profile)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"output": str(output), **report["summary"]}, indent=2))


if __name__ == "__main__":
    main()
