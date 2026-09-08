"""Validate recovered field structures against private original sources and RAM.

This command publishes no original payload. The report must live under .local.
Expected bytes come from independently captured original execution, never from
another decoder's output or a generated synthetic oracle.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if __package__:
    from ..reference.inspect_disc import RawCd
    from .field import collision_package, event_package, field_components
    from .packed import decode_block
else:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.field import collision_package, event_package, field_components
    from tools.analysis.packed import decode_block
    from tools.reference.inspect_disc import RawCd


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def private_output(path: Path) -> Path:
    resolved = path.resolve()
    if not resolved.is_relative_to((ROOT / ".local").resolve()):
        raise ValueError("Original-data reports must remain under ignored .local")
    if resolved.exists():
        raise ValueError("Evidence output must be a new file")
    resolved.parent.mkdir(parents=True, exist_ok=True)
    return resolved


def load_sources(raw: Path, profile_id: str, map_id: int) -> dict:
    """Bind every source read to an exact revision and measured slot identity."""
    if type(map_id) is not int or not 0 <= map_id < 730:
        raise ValueError("Map selector outside catalog range")
    profiles = json.loads((ROOT / "analysis/reference-profiles.json").read_text())["profiles"]
    profile = next(p for p in profiles if p["id"] == profile_id)
    disc = profile["disc_sequence"]
    if disc not in (1, 2):
        raise ValueError("No recovered source-slot mapping for this disc")
    raw_sha = file_sha(raw)
    if raw_sha != profile["measurement"]["source"]["raw_track"]["sha256"]:
        raise ValueError("Original raw-track fingerprint mismatch")
    catalog = json.loads((ROOT / "analysis/coverage/source-fingerprints.json").read_text())
    source = next(p for p in catalog["profiles"] if p["source_profile"] == profile_id)
    rows = {r[0]: dict(zip(source["records_columns"], r, strict=True)) for r in source["records"]}
    pair_start = 606 if disc == 1 else 601
    slot = pair_start + 2 * map_id
    overlay_slot = 36 if disc == 1 else 31
    with raw.open("rb") as stream:
        cd = RawCd(stream, raw.stat().st_size)

        def physical(record: dict) -> bytes:
            if record["projection_unit"] != 2048:
                raise ValueError("Field probe requires measured 2048-byte logical sectors")
            data = cd.read_extent(record["source_lba"], record["source_sector_count"] * 2048)
            if sha(data[: record["projected_bytes"]]) != record["projection_sha256"]:
                raise ValueError("Source slot projection fingerprint mismatch")
            return data

        boot = profile["measurement"]["boot"]["executable"]
        exe = cd.read_extent(boot["lba"], boot["size"])
        if sha(exe) != boot["sha256"]:
            raise ValueError("Original executable fingerprint mismatch")
        field_source = physical(rows[slot])
        overlay_packed = physical(rows[overlay_slot])
    return {
        "profile": profile,
        "raw_sha256": raw_sha,
        "field_record": rows[slot],
        "overlay_record": rows[overlay_slot],
        "exe": exe,
        "field_source": field_source,
        "overlay_packed": overlay_packed,
    }


def verify(raw: Path, ram_path: Path, profile_id: str, map_id: int) -> dict:
    sources = load_sources(raw, profile_id, map_id)
    exe, field_source = sources["exe"], sources["field_source"]
    overlay_packed = sources["overlay_packed"]
    ram = ram_path.read_bytes()
    if len(ram) != 0x200000:
        raise ValueError("Expected exactly 2 MiB original RAM")
    if struct.unpack_from("<I", ram, 0x4F34C)[0] != map_id:
        raise ValueError("Original captured field selector differs from selected map")

    def original_ram(address: int, size: int) -> bytes:
        if size < 0 or not 0x80000000 <= address <= 0x80200000 or size > 0x80200000 - address:
            raise ValueError(f"Original RAM pointer outside selected address space: {address:#x}")
        offset = address - 0x80000000
        return ram[offset : offset + size]

    def pointer(address: int) -> int:
        return int.from_bytes(original_ram(address, 4), "little")

    overlay = decode_block(overlay_packed)
    decoder_source = exe[0x800 + 0x80032EB4 - 0x80010000 : 0x800 + 0x80032F54 - 0x80010000]
    if decoder_source != original_ram(0x80032EB4, len(decoder_source)):
        raise ValueError("Original decoder instructions differ between source and captured RAM")
    windows = []
    for address, length in (
        (0x8007008C, 36),
        (0x8007104C, 160),
        (0x800711A8, 216),
        (0x8007B1C4, 692),
        (0x80093568, 252),
        (0x800A1B70, 96),
        (0x800A1EC8, 360),
        (0x800A2030, 632),
        (0x800A3018, 156),
        (0x800ACDB8, 108),
    ):
        offset = address - 0x8006FAF0
        code = overlay.data[offset : offset + length]
        if len(code) != length or code != original_ram(address, length):
            raise ValueError(f"Original field instruction window mismatch at {address:#x}")
        windows.append(
            {
                "address": f"0x{address:08x}",
                "decoded_offset": offset,
                "bytes": length,
                "sha256": sha(code),
            }
        )
    components = field_components(field_source)
    component_rows = []
    immutable = {1: 0x800AFB18, 5: 0x800ADBF8, 7: 0x800ADBF0}
    for part in components:
        row = {
            "index": part.index,
            "source_offset": part.source_offset,
            "logical_bytes": part.logical_size,
            "decoder_output_bytes": len(part.decoded.data),
            "output_padding_bytes": len(part.decoded.data) - part.logical_size,
            "token_bytes": part.decoded.token_bytes,
            "source_bytes_read_including_final_prefetch": part.decoded.source_bytes_read,
            "logical_sha256": sha(part.logical_data),
            "decoder_output_sha256": sha(part.decoded.data),
        }
        if part.index in immutable:
            address = pointer(immutable[part.index])
            original = original_ram(address, part.logical_size)
            if original != part.logical_data:
                raise ValueError(f"Original component {part.index} differs from captured RAM")
            row["original_ram_comparison"] = {
                "pointer_global": f"0x{immutable[part.index]:08x}",
                "address": f"0x{address:08x}",
                "bytes": len(original),
                "sha256": sha(original),
                "equal": True,
            }
        component_rows.append(row)
    scripts = event_package(components[5].logical_data)
    collision = collision_package(components[1].logical_data)
    if pointer(0x800ADBFC) != len(scripts.entries):
        raise ValueError("Original actor count differs from parsed actor table")
    original_script_base = pointer(0x800ADBF8) + scripts.bytecode_offset
    if pointer(0x800ADC00) != original_script_base:
        raise ValueError("Original bytecode base differs from recovered actor-table arithmetic")
    formation = components[6].logical_data
    if original_ram(0x800658DC, len(formation)) != formation:
        raise ValueError("Original formation data differs from captured field table")
    return {
        "schema_version": 1,
        "kind": "private_original_field_structure_validation",
        "source_profile": profile_id,
        "raw_track_sha256": sources["raw_sha256"],
        "boot_executable_sha256": sha(exe),
        "map_selector": map_id,
        "field_source": sources["field_record"],
        "field_physical_sha256": sha(field_source),
        "field_overlay_source": sources["overlay_record"],
        "decoded_field_overlay_sha256": sha(overlay.data),
        "decoder_source_sha256": sha(decoder_source),
        "instruction_windows": windows,
        "components": component_rows,
        "event_actor_count": len(scripts.entries),
        "event_entries_per_actor": 32,
        "event_bytecode_bytes": len(scripts.bytecode),
        "event_bytecode_base": f"0x{original_script_base:08x}",
        "collision_layers": [
            {"triangles": len(layer.triangles), "vertices": len(layer.vertices)}
            for layer in collision.layers
        ],
        "collision_attribute_bytes": len(collision.attributes_raw),
        "formation_original_ram_bytes_equal": len(formation),
        "ram": {"path": str(ram_path), "sha256": sha(ram)},
        "nix_lock_sha256": file_sha(ROOT / "nix/flake.lock"),
        "tool_sources": {
            str(path.relative_to(ROOT)): file_sha(path)
            for path in (
                Path(__file__).resolve(),
                ROOT / "tools/analysis/field.py",
                ROOT / "tools/analysis/packed.py",
            )
        },
        "result": "passed",
        "scope": (
            "Exact source/runtime instruction windows and immutable decoded components; "
            "bounded structure fields only. This does not establish traversal, complete "
            "script semantics, native behavior, or arbitrary maps."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--ram", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if not 0 <= args.map < 730:
            raise ValueError("Map selector outside catalog range")
        output = private_output(args.output)
        report = verify(args.raw.resolve(), args.ram.resolve(), args.profile, args.map)
        with output.open("x") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
        print(
            f"Verified original field {args.map}: {len(report['components'])} components, "
            f"{report['event_actor_count']} actors"
        )
    except (ValueError, StopIteration, KeyError, OSError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
