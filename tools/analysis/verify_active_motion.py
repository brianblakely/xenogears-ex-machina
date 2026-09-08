"""Reproduce full party movement updates from qualified original call entries."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path

from .active_motion_spec import SEGMENTS, WINDOWS, specification
from .active_motion_trace import compare_motion
from .animation_trace import OriginalResources
from .original_trace import capture_records, require
from .party_sprites import compare_loader, sprite_sources
from .sweep_trace import tables
from .verify_collision_math import source_bytes
from .verify_field import file_sha, private_output, verify
from .verify_movement_sweep import TOOL_SOURCES as SWEEP_TOOLS
from .verify_movement_sweep import qualify_control
from .verify_sprite_animation import TOOL_SOURCES as SPRITE_TOOLS

ROOT = Path(__file__).resolve().parents[2]
TOOL_SOURCES = tuple(
    dict.fromkeys(
        SWEEP_TOOLS
        + SPRITE_TOOLS
        + (
            "tools/analysis/active_motion.py",
            "tools/analysis/active_motion_spec.py",
            "tools/analysis/active_motion_trace.py",
            "tools/analysis/verify_active_motion.py",
        )
    )
)
GHIDRA_FUNCTIONS = ((0x80082BB8, 0x80083178), (0x80082494, 0x800825AC), (0x8008492C, 0x80084A40))
EXPECTED = {
    "control": {
        "records": 6435,
        "active": 470,
        "inhibited": 470,
        "predicate-calls": 874,
        "sweeps": 156,
        "animation-call": 24,
        "excluded": {},
    },
    "traversal-a": {
        "records": 9604,
        "active": 300,
        "inhibited": 300,
        "predicate-calls": 1468,
        "sweeps": 276,
        "animation-call": 5,
        "excluded": {19: 166, 14: 140, 15: 128},
    },
    "traversal-b": {
        "records": 9401,
        "active": 300,
        "inhibited": 300,
        "predicate-calls": 1438,
        "sweeps": 266,
        "animation-call": 9,
        "excluded": {14: 54, 15: 26, 19: 112, 16: 162, 20: 65},
    },
    "traversal-c": {
        "records": 11324,
        "active": 348,
        "inhibited": 348,
        "predicate-calls": 2088,
        "sweeps": 31,
        "animation-call": 2,
        "excluded": {16: 348, 19: 348},
    },
}


def qualify_ghidra(sources, path):
    report = json.loads(path.read_text())
    overlay_hash = hashlib.sha256(sources["overlay"]).hexdigest()
    require(
        report["profile"] == sources["profile"]["id"]
        and report["overlay_sha256"] == overlay_hash
        and report["language"] == "PSX:LE:32:default"
        and report["loader"] == "PSX Executables Loader"
        and report["ghidra_version"] == "12.1.2",
        "Unqualified original motion Ghidra environment",
    )
    functions = report["decompilation"]
    require(len(functions) == len(GHIDRA_FUNCTIONS), "Changed original motion Ghidra function set")
    count = 0
    for function, (begin, end) in zip(functions, GHIDRA_FUNCTIONS, strict=True):
        prefix = "field_" + overlay_hash[:16] + "::"
        require(
            function["entry"] == prefix + f"{begin:08x}"
            and function["complete"]
            and not function["error"]
            and bool(function.get("c"))
            and function["signature"].startswith(("void __stdcall", "int __stdcall"))
            and len(function["instructions"]) == (end - begin) // 4,
            "Incomplete original motion decompilation or signature",
        )
        for index, instruction in enumerate(function["instructions"]):
            at = begin + 4 * index
            require(
                instruction["address"] == prefix + f"{at:08x}"
                and bytes.fromhex(instruction["bytes"]) == source_bytes(sources, at, at + 4)
                and isinstance(instruction["pcode"], list),
                "Ghidra motion instruction differs from source",
            )
            count += 1
    return {
        "path": str(path),
        "sha256": file_sha(path),
        "functions": len(functions),
        "instructions": count,
    }


def compare(sources, route, control, loader, captures, ghidra_path):
    require(route in ("control", "traversal"), "Unqualified original motion route")
    segments = ["control"] if route == "control" else ["traversal-a", "traversal-b", "traversal-c"]
    require(len(captures) == len(segments), "Original motion recording segment set differs")
    ghidra = qualify_ghidra(sources, ghidra_path)
    controls = qualify_control(sources, route, control, tuple(captures))
    loader_report, party = compare_loader(sources, loader)
    lookup = tables(sources)
    reports = {}
    totals, modes, stops, excluded = Counter(), Counter(), Counter(), Counter()
    for segment, capture in zip(segments, captures, strict=True):
        spec = specification(sources, (capture / "final.ram").read_bytes(), segment)
        ram, rows, observation = capture_records(capture, sources, spec, WINDOWS)
        resources = OriginalResources(sources, ram, party)
        proof = compare_motion(rows, spec, sources, resources, lookup)
        expected = EXPECTED[segment]
        require(
            len(rows) == expected["records"]
            and proof["excluded_actor_calls"] == expected["excluded"]
            and all(
                proof["counts"].get(key, 0) == expected[key]
                for key in ("active", "inhibited", "predicate-calls", "animation-call")
            )
            and proof["counts"].get("after-sweep", 0) == expected["sweeps"],
            "Original party motion coverage differs",
        )
        reports[segment] = {
            "capture": str(capture),
            "records": len(rows),
            "observation_sha256": file_sha(capture / "observation.json"),
            "trace_sha256": observation["instruction_trace"]["trace_sha256"],
            "specification_sha256": observation["instruction_trace"]["specification_sha256"],
            **proof,
        }
        totals.update(proof["counts"])
        modes.update(proof["modes"])
        stops.update(proof["stops"])
        excluded.update(proof["excluded_actor_calls"])
    return {
        "schema_version": 1,
        "kind": "original_complete_party_motion_comparison",
        "result": "passed",
        "source_profile": sources["profile"]["id"],
        "raw_track_sha256": sources["raw_sha256"],
        "map": sources["map"],
        "route": route,
        "ghidra": ghidra,
        "controls": controls,
        "party_loader": loader_report,
        "segments": reports,
        "counts": dict(totals),
        "modes": dict(modes),
        "stops": dict(stops),
        "excluded_actor_calls": dict(excluded),
        "tool_sources": {name: file_sha(ROOT / name) for name in TOOL_SOURCES},
        "source_windows": [
            {
                "begin": f"0x{begin:08x}",
                "end_exclusive": f"0x{end:08x}",
                "sha256": hashlib.sha256(source_bytes(sources, begin, end)).hexdigest(),
            }
            for begin, end in WINDOWS
        ],
        "scope": "Every selected party motion call is computed from function entry through return. "
        "Traversal uses three contiguous observation windows on the same original route. "
        "Other actor motion calls are excluded; their idle predicates compare independently. "
        "Bounds, alternate resources and later position/layer integration remain separate. "
        "No observed callee effects, native execution or hardware timing are supplied or claimed.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--segment", choices=tuple(SEGMENTS))
    parser.add_argument("--route", choices=("control", "traversal"))
    parser.add_argument("--ram", type=Path)
    parser.add_argument("--control-capture", type=Path)
    parser.add_argument("--loader-capture", type=Path)
    parser.add_argument("--motion-captures", type=Path, nargs="+")
    parser.add_argument("--ghidra", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = sprite_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            require(
                args.ram is not None and args.segment is not None,
                "Prepare needs original RAM and segment",
            )
            verify(args.raw, args.ram, args.profile, args.map)
            result = specification(sources, args.ram.read_bytes(), args.segment)
        else:
            require(
                all(
                    (
                        args.route,
                        args.control_capture,
                        args.loader_capture,
                        args.motion_captures,
                        args.ghidra,
                    )
                ),
                "Compare needs route, control, loader, motion captures and reviewed Ghidra export",
            )
            result = compare(
                sources,
                args.route,
                args.control_capture,
                args.loader_capture,
                args.motion_captures,
                args.ghidra,
            )
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"Original party motion {args.mode} passed: {output}")
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        parser.exit(1, f"Original party motion {args.mode} failed: {error}\n")


if __name__ == "__main__":
    main()
