"""Qualify and compare original field movement sweeps on two bounded routes.

The original component, tables, source guards and independent control artifacts
are mandatory. Queries and intermediate arithmetic are reconstructed directly;
trace results are never substituted for opaque calls in the sweep model.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not __package__:
    sys.path.insert(0, str(ROOT))

    from tools.analysis.arithmetic import signed32
    from tools.analysis.original_trace import capture_records, exact_payload, require
    from tools.analysis.query_trace import area_pairs, caller_effect, compare_queries
    from tools.analysis.sweep_math_trace import compare_math
    from tools.analysis.sweep_trace import (
        WINDOWS,
        call,
        edge_points,
        ordered_rows,
        read_captured,
        selected,
        specification,
        sweep_result,
        tables,
        u32,
        vector,
        write_edge,
    )
    from tools.analysis.verify_collision_math import math_sources, source_bytes
    from tools.analysis.verify_field import file_sha, private_output, verify
    from tools.reference.scenario import compile_scenario
else:
    from ..reference.scenario import compile_scenario
    from .arithmetic import signed32
    from .original_trace import capture_records, exact_payload, require
    from .query_trace import area_pairs, caller_effect, compare_queries
    from .sweep_math_trace import compare_math
    from .sweep_trace import (
        WINDOWS,
        call,
        edge_points,
        ordered_rows,
        read_captured,
        selected,
        specification,
        sweep_result,
        tables,
        u32,
        vector,
        write_edge,
    )
    from .verify_collision_math import math_sources, source_bytes
    from .verify_field import file_sha, private_output, verify


ORDINARY_CALLERS = {
    "probe-left": 0x8007BB6C,
    "probe-right": 0x8007BBC8,
    "probe-center": 0x8007BC2C,
    "floor": 0x8007BCB0,
    "slope-floor": 0x8007BDD4,
}
SPECIAL_CALLERS = {
    "probe-center": 0x8007B8A8,
    "probe-left": 0x8007B914,
    "probe-right": 0x8007B96C,
    "floor": 0x8007B9E8,
}
TOOL_SOURCES = (
    "tools/analysis/verify_movement_sweep.py",
    "tools/analysis/sweep_trace.py",
    "tools/analysis/query_trace.py",
    "tools/analysis/sweep_math_trace.py",
    "tools/analysis/movement_sweep.py",
    "tools/analysis/collision_query.py",
    "tools/analysis/sweep_math.py",
    "tools/analysis/planar_motion.py",
    "tools/analysis/collision_math.py",
    "tools/analysis/arithmetic.py",
    "tools/analysis/original_trace.py",
    "tools/analysis/verify_collision_math.py",
    "tools/analysis/verify_field.py",
    "tools/analysis/field.py",
    "tools/analysis/packed.py",
    "tools/reference/scenario.py",
    "tools/reference/scenario_program.py",
    "tools/reference/instruction_trace.py",
    "tools/reference/memory_sampler.py",
    "tools/reference/observe.py",
    "analysis/coverage/field-pairs.json",
    "nix/flake.lock",
    "nix/flake.nix",
    "nix/reference-trace.h",
    "nix/reference-trace-patch.py",
)


def compare_sweeps(rows, sources: dict, spec: dict, ram: bytes) -> dict:
    lookup, current, query_pending, projection_before = tables(sources), None, None, None
    counts, cases, projections = Counter(), Counter(), Counter()
    ignored = {
        "triangle-loop",
        "triangle-decision",
        "special-triangle-loop",
        "special-triangle-decision",
        "height-before",
        "height-after",
    }
    for row, payload in ordered_rows(rows, spec, sources, ram):
        name, gpr = row["hook"], row["gpr_u32"]
        if name.endswith("sweep-before"):
            require(
                current is None and query_pending is None and projection_before is None,
                "nested original sweep",
            )
            ordinary = name.startswith("ordinary")
            require(
                gpr[31] == (0x80082F10 if ordinary else 0x80082F2C)
                and gpr[4] == gpr[29] + 0x10
                and gpr[6] == gpr[29] + 0x20
                and gpr[5] == u32(payload["player-descriptor"], 0x4C),
                "unqualified original sweep caller or actor",
            )
            result = sweep_result(row, payload, sources, lookup)
            current = {
                "before": row,
                "old": payload,
                "result": result,
                "ordinary": ordinary,
                "index": 0,
                "edge": payload["edge"],
                "projection": 0,
            }
        elif name.endswith("query-before"):
            require(
                current is not None
                and query_pending is None
                and projection_before is None
                and current["index"] < len(current["result"].queries),
                "additional or interrupted original sweep query",
            )
            expected = current["result"].queries[current["index"]]
            original = current["before"]["gpr_u32"]
            ordinary = current["ordinary"]
            call(
                row,
                current["before"],
                0xA8 if ordinary else 0x78,
                (ORDINARY_CALLERS if ordinary else SPECIAL_CALLERS)[expected.stage],
            )
            require(
                name.startswith("ordinary") == ordinary
                and gpr[4] == gpr[29] + (0x20 if ordinary else 0x18)
                and gpr[5] == original[5] + 0x20
                and gpr[6] == original[5]
                and gpr[7] == original[6]
                and u32(payload["query-caller"], 16) == gpr[29] + (0x40 if ordinary else 0x28)
                and (not ordinary or u32(payload["query-caller"], 24) == gpr[29] + 0x70)
                and payload["actor"] == current["old"]["actor"]
                and payload["edge"] == current["edge"]
                and all(
                    value is None or value == vector(payload["candidate"])[i]
                    for i, value in enumerate(expected.candidate)
                )
                and signed32(u32(payload["query-caller"], 20)) == expected.mode,
                "original sweep query inputs, outputs or candidate differs",
            )
            if expected.stage == "floor" and current["result"].projection is not None:
                require(current["projection"] == 2, "original floor query precedes edge projection")
            query_pending = row, payload, expected
        elif name.endswith("query-after"):
            require(
                current is not None and query_pending is not None,
                "orphan original sweep query return",
            )
            before, old, expected = query_pending
            effect = expected.result
            call(row, before, 0x90 if current["ordinary"] else 0x80)
            require(
                name.split("-")[0] == before["hook"].split("-")[0]
                and gpr[2] == effect.value & 0xFFFFFFFF,
                "original sweep query returned a different result",
            )
            current["edge"] = write_edge(current["edge"], effect.edge)
            require(
                payload["edge"] == current["edge"]
                and read_captured(row, before["gpr_u32"][29], 176)
                == caller_effect(before, old, effect),
                "original nested sweep query memory effects differ",
            )
            exact_payload(selected(payload), selected(old), "nested sweep query shared state")
            current["index"] += 1
            query_pending = None
            counts["queries"] += 1
        elif name == "edge-projection-before":
            require(
                current is not None
                and query_pending is None
                and projection_before is None
                and current["projection"] == 0
                and current["result"].projection is not None
                and current["index"] > 0,
                "unexpected original edge projection",
            )
            previous = current["result"].queries[current["index"] - 1]
            ordinary = current["ordinary"]
            call(
                row,
                current["before"],
                0xA8 if ordinary else 0x78,
                0x8007BC60 if ordinary else 0x8007B9A0,
            )
            require(
                previous.result.value == -1
                and previous.stage.startswith("probe-")
                and signed32(gpr[4]) == signed32(current["before"]["gpr_u32"][7])
                and gpr[5] == current["before"]["gpr_u32"][6]
                and gpr[6] == gpr[29] + (0x20 if ordinary else 0x18)
                and payload["edge"] == current["edge"]
                and payload["velocity"] == current["old"]["velocity"],
                "original edge projection inputs differ",
            )
            projection_before = row, payload
            current["projection"] = 1
        elif name == "edge-projection-after":
            require(
                current is not None
                and projection_before is not None
                and current["projection"] == 1,
                "orphan original edge projection return",
            )
            before, old = projection_before
            call(row, before, 0x48)
            effect = current["result"].projection
            require(
                gpr[16] == before["gpr_u32"][5]
                and gpr[18] == before["gpr_u32"][6]
                and gpr[2] == effect.direction
                and vector(payload["velocity"]) == effect.velocity
                and payload["edge"] == current["edge"],
                "original edge projection result differs",
            )
            exact_payload(selected(payload), selected(old), "edge projection shared state")
            current["projection"] = 2
            projection_before = None
            counts["projections"] += 1
            projections[(effect.branch, effect.direction)] += 1
        elif name.endswith("sweep-after"):
            require(
                current is not None and query_pending is None and projection_before is None,
                "orphan or premature original sweep return",
            )
            before, old, effect = current["before"], current["old"], current["result"]
            original, ordinary = before["gpr_u32"], current["ordinary"]
            call(row, before, 0xA8 if ordinary else 0x78)
            require(
                name.split("-")[0] == before["hook"].split("-")[0]
                and current["index"] == len(effect.queries)
                and current["projection"] == (2 if effect.projection is not None else 0)
                and gpr[2] == effect.value & 0xFFFFFFFF
                and gpr[17] == original[4]
                and gpr[18 if ordinary else 19] == original[5]
                and gpr[23 if ordinary else 20] == original[6],
                "original sweep stages, returned pointers or result differs",
            )
            actor = bytearray(old["actor"])
            struct.pack_into("<I", actor, 0, effect.actor.query.flags)
            struct.pack_into("<h", actor, 0x72, effect.actor.floor)
            velocity = struct.pack("<3i", *effect.velocity)
            require(
                payload["actor"] == actor
                and payload["velocity"] == velocity
                and edge_points(payload["edge"]) == effect.edge
                and payload["edge"] == current["edge"],
                "original sweep complete actor, velocity or edge bytes differ",
            )
            shared = selected(old)
            shared["player-actor"] = bytes(actor)
            exact_payload(selected(payload), shared, "sweep shared state")
            caller = bytearray(old["caller-stack"])
            for pointer, data in ((original[4], velocity), (original[6], current["edge"])):
                at = pointer - original[29]
                require(
                    0 <= at <= len(caller) - len(data),
                    "original sweep output outside caller capture",
                )
                caller[at : at + len(data)] = data
            require(
                payload["caller-stack"] == caller,
                "original sweep caller writes or preserved bytes differ",
            )
            counts["sweeps"] += 1
            cases[(name, effect.value, effect.slope_reason or "none")] += 1
            current = None
        elif name not in ignored:
            raise ValueError("unexpected original sweep stage")
    require(
        current is None and query_pending is None and projection_before is None,
        "incomplete original sweep",
    )
    return {
        "counts": dict(counts),
        "cases": [list(k) + [v] for k, v in sorted(cases.items())],
        "projections": [list(k) + [v] for k, v in sorted(projections.items())],
    }


def qualify_control(sources: dict, route: str, control: Path, captures: tuple[Path, ...]) -> dict:
    scenario_path = (
        ROOT
        / "tests/reference-inputs"
        / (
            "field23-control-observation.json"
            if route == "control"
            else "field23-ramp-observation.json"
        )
    )
    program, _ = compile_scenario(
        json.loads(scenario_path.read_text()),
        json.loads((ROOT / "analysis/coverage/field-pairs.json").read_text()),
    )
    expected_frames, expected_files = (5241, 66) if route == "control" else (6197, 53)
    baseline = json.loads((control / "observation.json").read_text())
    require("instruction_trace" not in baseline, "original control is instrumented")
    files = None
    observations = []
    for capture in (control,) + captures:
        observation = json.loads((capture / "observation.json").read_text())
        require(
            observation["scenario"]["program"] == program
            and observation["scenario"]["complete"]
            and observation["scenario"]["cold_boot"]
            and observation["frames"] == expected_frames
            and observation["initial_reference_state_sha256"] is None
            and observation["inputs"] == []
            and observation["source_profile"] == sources["profile"]["id"]
            and observation["content_sha256"]
            == sources["profile"]["measurement"]["source"]["chd"]["sha256"]
            and observation["tool_sha256"] == file_sha(ROOT / "tools/reference/observe.py")
            and observation["nix_lock_sha256"] == file_sha(ROOT / "nix/flake.lock")
            and observation["scenario"]["executor_sha256"]
            == file_sha(ROOT / "tools/reference/scenario_program.py")
            and observation["core_sha256"] == file_sha(Path(observation["core_path"])),
            "unqualified original control route, collector or core",
        )
        for key in (
            "captures",
            "scenario",
            "effective_options",
            "bios_sha256",
            "audio_frames",
            "audio_sha256",
            "final_state_sha256",
        ):
            require(observation[key] == baseline[key], f"original control {key} differs")
        if capture != control:
            require(
                observation["core_sha256"] != baseline["core_sha256"],
                "original control and instrumented core are identical",
            )
            metadata = observation["instruction_trace"]
            require(
                set(metadata["core_extension_inputs"])
                == {"flake.nix", "reference-trace.h", "reference-trace-patch.py"}
                and set(metadata["guard_mismatches_by_hook"])
                == {h["name"] for h in metadata["spec"]["hooks"]},
                "original extension provenance or hook coverage incomplete",
            )
        names = {
            p.name
            for p in capture.iterdir()
            if p.suffix in (".png", ".wav", ".ram", ".state", ".bin")
        }
        if route == "control":
            names.add("memory-trace.jsonl")
            sampling = observation["memory_sampling"]
            require(
                not sampling["budget_reached"]
                and not any(sampling["unavailable_by_range"].values())
                and sampling["trace_sha256"] == file_sha(capture / "memory-trace.jsonl")
                and sampling == baseline["memory_sampling"],
                "original control memory sampling differs",
            )
        actual = {name: file_sha(capture / name) for name in sorted(names)}
        if files is None:
            files = actual
        require(
            len(actual) == expected_files and actual == files,
            "original control artifact set or bytes differ",
        )
        observations.append(
            {
                "capture": str(capture),
                "observation_sha256": file_sha(capture / "observation.json"),
                "core_sha256": observation["core_sha256"],
            }
        )
    return {
        "scenario": {
            "path": str(scenario_path.relative_to(ROOT)),
            "sha256": file_sha(scenario_path),
        },
        "frontend_runs": expected_frames,
        "files": [{"name": k, "sha256": v} for k, v in files.items()],
        "observations": observations,
    }


def compare(sources: dict, route: str, control: Path, sweep: Path, area: Path, math: Path) -> dict:
    require(
        sources["map"] == 23 and route in ("control", "traversal"),
        "unqualified original movement route",
    )
    controls = qualify_control(sources, route, control, (sweep, area, math))
    loaded, manifests = {}, {}
    for kind, capture in (("sweep", sweep), ("area", area), ("math", math)):
        spec = specification(sources, (capture / "final.ram").read_bytes(), route, kind)
        ram, rows, observation = capture_records(capture, sources, spec, WINDOWS)
        loaded[kind] = ram, rows, spec
        manifests[kind] = {
            "capture": str(capture),
            "records": len(rows),
            "trace_sha256": observation["instruction_trace"]["trace_sha256"],
            "observation_sha256": file_sha(capture / "observation.json"),
            "specification_sha256": observation["instruction_trace"]["specification_sha256"],
        }
    ram, rows, spec = loaded["area"]
    pairs, areas = area_pairs(rows, sources, spec, ram)
    ram, rows, spec = loaded["sweep"]
    queries = compare_queries(rows, sources, spec, ram, pairs)
    sweeps = compare_sweeps(rows, sources, spec, ram)
    math_ram, math_rows, math_spec = loaded["math"]
    arithmetic = compare_math(math_rows, sources, math_spec, math_ram, rows)
    require(
        (sweeps["counts"]["sweeps"], queries["queries"], queries["all_height_pairs"])
        == ((156, 702, 1450) if route == "control" else (573, 2528, 4144)),
        "original route coverage differs from qualified observation",
    )
    return {
        "schema_version": 1,
        "kind": "original_movement_sweep_comparison",
        "result": "passed",
        "source_profile": sources["profile"]["id"],
        "raw_track_sha256": sources["raw_sha256"],
        "map": sources["map"],
        "route": route,
        "captures": manifests,
        "controls": controls,
        "areas": areas,
        "queries": queries,
        "sweeps": sweeps,
        "arithmetic": arithmetic,
        "tool_sources": {name: file_sha(ROOT / name) for name in TOOL_SOURCES},
        "source_windows": [
            {
                "begin": f"0x{begin:08x}",
                "end_exclusive": f"0x{end:08x}",
                "sha256": hashlib.sha256(source_bytes(sources, begin, end)).hexdigest(),
            }
            for begin, end in WINDOWS
        ],
        "scope": (
            "Source-derived sweep/query output and intermediate arithmetic on two bounded "
            "original routes. Later position integration, scratchpad copy lineage, GTE side "
            "effects and hardware timing remain separate."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--route", choices=("control", "traversal"), required=True)
    parser.add_argument("--kind", choices=("sweep", "area", "math"))
    parser.add_argument("--ram", type=Path)
    for name in ("control", "sweep", "area", "math"):
        parser.add_argument(f"--{name}-capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = math_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            require(
                args.ram is not None and args.kind is not None,
                "preparation needs original RAM and capture kind",
            )
            verify(args.raw, args.ram, args.profile, args.map)
            result = specification(sources, args.ram.read_bytes(), args.route, args.kind)
        else:
            require(
                all(
                    getattr(args, f"{kind}_capture") is not None
                    for kind in ("control", "sweep", "area", "math")
                ),
                "comparison needs three original traces and the uninstrumented control",
            )
            source_check = verify(
                args.raw, args.control_capture / "final.ram", args.profile, args.map
            )
            result = compare(
                sources,
                args.route,
                args.control_capture,
                args.sweep_capture,
                args.area_capture,
                args.math_capture,
            )
            result["source_check"] = source_check
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output}")
    except (ValueError, KeyError, IndexError, OSError, struct.error, TypeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
