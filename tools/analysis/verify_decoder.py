"""Generate private decoder observations and compare every field output byte.

The external original program supplies output hashes at its return instruction.
The reconstruction supplies candidate hashes. Source bytes, source input ends,
output extents, code guards and capture identity must all agree independently.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if __package__:
    from ..reference.instruction_trace import validate_instruction_trace
    from .field import field_components
    from .packed import decode_block
    from .verify_field import file_sha, load_sources, private_output, sha
else:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.field import field_components
    from tools.analysis.packed import decode_block
    from tools.analysis.verify_field import file_sha, load_sources, private_output, sha
    from tools.reference.instruction_trace import validate_instruction_trace


def specification(sources: dict, map_id: int) -> dict:
    parts = field_components(sources["field_source"])
    overlay = decode_block(sources["overlay_packed"]).data
    offset = 0x8007008C - 0x8006FAF0
    source_size = sources["field_record"]["projected_bytes"]
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"field-{map_id}-decoder-outputs",
            "source_profile": sources["profile"]["id"],
            "start_frame": 0,
            "end_frame": 5000,
            "max_callbacks": 16,
            "hooks": [
                {
                    "name": "field-decoder-return",
                    "pc": 0x800700A0,
                    "guard": {"offset": 0x7008C, "expected": overlay[offset : offset + 36].hex()},
                    "ranges": [
                        {"name": "decoder-code", "offset": 0x32EB4, "size": 160},
                        {"name": "field-selector", "offset": 0x4F34C, "size": 4},
                        {
                            "name": "source-boundary",
                            "pointer_offset": 0x5A4E0,
                            "relative_offset": source_size - 32,
                            "size": 96,
                        },
                    ],
                    "digests": [
                        {
                            "name": "original-output",
                            "register": 14,
                            "end_register": 5,
                            "max_bytes": max(len(part.decoded.data) for part in parts),
                        },
                        {
                            "name": "original-source",
                            "pointer_offset": 0x5A4E0,
                            "size": source_size,
                            "max_bytes": source_size,
                        },
                    ],
                }
            ],
        }
    )


def compare(sources: dict, map_id: int, capture: Path) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    profile = sources["profile"]
    if (
        observation["source_profile"] != profile["id"]
        or observation["content_sha256"] != profile["measurement"]["source"]["chd"]["sha256"]
        or not observation["scenario"]["complete"]
    ):
        raise ValueError("Original capture identity or scenario completion mismatch")
    metadata = observation["instruction_trace"]
    expected_spec = specification(sources, map_id)
    if (
        metadata["spec"] != expected_spec
        or metadata["specification"] != "instruction-trace-spec.json"
        or metadata["trace"] != "instruction-trace.jsonl"
        or metadata["failed"]
        or metadata["budget_reached"]
        or metadata["unavailable_ranges"]
        or any(metadata["guard_mismatches_by_hook"].values())
    ):
        raise ValueError("Incomplete, failed, unavailable or differently guarded decoder trace")
    spec_path = capture / "instruction-trace-spec.json"
    trace_path = capture / "instruction-trace.jsonl"
    if (
        file_sha(spec_path) != metadata["specification_sha256"]
        or json.loads(spec_path.read_text()) != expected_spec
        or file_sha(trace_path) != metadata["trace_sha256"]
        or trace_path.stat().st_size > 512 * 1024
    ):
        raise ValueError("Original trace/specification fingerprint or size mismatch")
    decoder_offset = 0x800 + 0x80032EB4 - 0x80010000
    decoder_code = sources["exe"][decoder_offset : decoder_offset + 160]
    source_digest = sources["field_record"]["projection_sha256"]
    physical_parts = field_components(sources["field_source"])
    source_size = sources["field_record"]["projected_bytes"]
    results, seen = [], set()
    for event, line in enumerate(trace_path.read_text().splitlines()):
        record = json.loads(line)
        if event >= 16 or record["event"] != event or record["hook"] != "field-decoder-return":
            raise ValueError("Unexpected or noncontiguous decoder return records")
        ranges = {item["name"]: bytes.fromhex(item["hex"]) for item in record["ranges"]}
        if (
            ranges["decoder-code"] != decoder_code
            or int.from_bytes(ranges["field-selector"], "little") != map_id
        ):
            raise ValueError("Original decoder code or active field identity differs")
        digests = {item["name"]: item for item in record["digests"]}
        source, output = digests["original-source"], digests["original-output"]
        if source["sha256"] != source_digest:
            raise ValueError("Original loaded field bytes differ from the entire source slot")
        boundary_record = next(r for r in record["ranges"] if r["name"] == "source-boundary")
        boundary = ranges["source-boundary"]
        if (
            boundary_record["pointer_value"] != source["pointer_value"]
            or len(boundary) != 96
            or boundary[:32] != sources["field_source"][source_size - 32 : source_size]
        ):
            raise ValueError("Original source boundary is not anchored to the measured source file")
        # This is an original input measurement, not a fix-up of candidate output.
        # Retain the separate disc-context comparison below, including differences.
        original_memory = sources["field_source"][:source_size] + boundary[32:]
        parts = field_components(original_memory)
        registers = record["gpr_u32"]
        candidates = [
            part
            for part in parts
            if source["pointer_value"] + part.source_offset + part.decoded.source_bytes_read
            == registers[4]
        ]
        if len(candidates) != 1 or candidates[0].index in seen:
            raise ValueError("Original input end does not uniquely match a remaining component")
        part = candidates[0]
        physical_part = physical_parts[part.index]
        if part.logical_data != physical_part.logical_data:
            raise ValueError("Logical component data depends on bytes outside the source file")
        if (
            output["size"] != len(part.decoded.data)
            or output["sha256"] != sha(part.decoded.data)
            or not registers[2] == registers[6] == registers[14] == output["pointer_value"]
            or registers[5] - registers[14] != output["size"]
        ):
            raise ValueError(f"Original component {part.index} output/extent/return mismatch")
        seen.add(part.index)
        results.append(
            {
                "component": part.index,
                "frontend_run": record["frontend_run"],
                "source_offset": part.source_offset,
                "source_bytes_read": part.decoded.source_bytes_read,
                "logical_bytes": part.logical_size,
                "original_output_bytes": output["size"],
                "original_output_sha256": output["sha256"],
                "disc_context_output_sha256": sha(physical_part.decoded.data),
                "disc_context_output_equal": part.decoded.data == physical_part.decoded.data,
                "bytes_read_past_source_file": max(
                    0, part.source_offset + part.decoded.source_bytes_read - source_size
                ),
                "source_boundary_sha256": sha(boundary),
                "equal": True,
            }
        )
    if seen != set(range(9)) or metadata["records"] != 9 or metadata["candidate_callbacks"] != 9:
        raise ValueError("Expected exactly one original decode for every field component")
    return {
        "schema_version": 1,
        "kind": "original_field_decoder_output_comparison",
        "source_profile": profile["id"],
        "map": map_id,
        "raw_track_sha256": sources["raw_sha256"],
        "field_source": sources["field_record"],
        "field_overlay_source": sources["overlay_record"],
        "decoded_overlay_sha256": sha(decode_block(sources["overlay_packed"]).data),
        "decoder_address": "0x80032eb4",
        "decoder_code_sha256": sha(decoder_code),
        "components_in_original_call_order": results,
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "trace": {"path": str(trace_path), "sha256": file_sha(trace_path)},
        "nix_lock_sha256": file_sha(ROOT / "nix/flake.lock"),
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in (
                "tools/analysis/verify_decoder.py",
                "tools/analysis/verify_field.py",
                "tools/analysis/field.py",
                "tools/analysis/packed.py",
            )
        },
        "result": "passed",
        "scope": (
            "All original decoder output bytes, including padding, and original input end "
            "pointers for these nine components. No general format or gameplay equivalence."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "compare"])
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = load_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            if args.capture:
                raise ValueError("prepare does not consume a capture")
            result = specification(sources, args.map)
        else:
            if not args.capture:
                raise ValueError("compare requires --capture")
            result = compare(sources, args.map, args.capture)
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: original field {args.map}, {output.relative_to(ROOT)}")
    except (ValueError, KeyError, StopIteration, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
