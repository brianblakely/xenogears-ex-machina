"""Reproduce the bounded original animation, facing replay and matrix comparisons."""

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

    from tools.analysis.animation_trace import (
        WINDOWS,
        OriginalResources,
        calls,
        compare_effect,
        read_captured,
        specification,
    )
    from tools.analysis.original_trace import capture_records, require
    from tools.analysis.party_sprites import compare_loader, sprite_sources
    from tools.analysis.replay_trace import compare_replay
    from tools.analysis.sprite_animation import (
        apply_header,
        bind_current,
        field_animation,
        select_animation,
    )
    from tools.analysis.sprite_matrix import sprite_matrix
    from tools.analysis.sprite_state import u32
    from tools.analysis.sweep_trace import qualified_ranges
    from tools.analysis.verify_collision_math import source_bytes
    from tools.analysis.verify_field import file_sha, private_output, verify
    from tools.analysis.verify_movement_sweep import qualify_control
else:
    from .animation_trace import (
        WINDOWS,
        OriginalResources,
        calls,
        compare_effect,
        read_captured,
        specification,
    )
    from .original_trace import capture_records, require
    from .party_sprites import compare_loader, sprite_sources
    from .replay_trace import compare_replay
    from .sprite_animation import (
        apply_header,
        bind_current,
        field_animation,
        select_animation,
    )
    from .sprite_matrix import sprite_matrix
    from .sprite_state import u32
    from .sweep_trace import qualified_ranges
    from .verify_collision_math import source_bytes
    from .verify_field import file_sha, private_output, verify
    from .verify_movement_sweep import qualify_control


TOOL_SOURCES = (
    "tools/analysis/verify_sprite_animation.py",
    "tools/analysis/animation_trace.py",
    "tools/analysis/replay_trace.py",
    "tools/analysis/sprite_animation.py",
    "tools/analysis/sprite_matrix.py",
    "tools/analysis/sprite_replay.py",
    "tools/analysis/sprite_state.py",
    "tools/analysis/jump_physics.py",
    "tools/analysis/party_sprites.py",
    "tools/analysis/planar_motion.py",
    "tools/analysis/verify_movement_sweep.py",
    "tools/analysis/sweep_trace.py",
    "tools/analysis/original_trace.py",
    "tools/analysis/verify_collision_math.py",
    "tools/analysis/collision_math.py",
    "tools/analysis/arithmetic.py",
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
    "nix/ghidra/flake.nix",
    "nix/ghidra/flake.lock",
    "tools/analysis/ghidra_project.py",
    "tools/analysis/ghidra/PrepareOriginalProgram.java",
    "tools/analysis/ghidra/ExportOriginalAnalysis.java",
)
GHIDRA_FUNCTIONS = {
    "animation": (
        (0x80022000, 0x80022038),
        (0x80022090, 0x80022224),
        (0x80022224, 0x800222BC),
        (0x800222BC, 0x800223B0),
        (0x800223B0, 0x80022660),
        (0x80022660, 0x80022974),
        (0x800234AC, 0x80023538),
        (0x80023538, 0x80023804),
        (0x800245D8, 0x80024730),
        (0x800821F4, 0x8008237C),
        (0x8008492C, 0x80084A40),
    ),
    "matrix": (
        (0x8003F738, 0x8003F8B0),
        (0x8004974C, 0x80049870),
        (0x80049DCC, 0x80049EF0),
        (0x8004920C, 0x80049318),
    ),
    "replay": (
        (0x80022660, 0x80022974),
        (0x8001D2B0, 0x8001D3F4),
        (0x80022D44, 0x80022DF4),
        (0x80021CF8, 0x80021D3C),
        (0x8001F8E8, 0x8001FAB4),
        (0x800234AC, 0x80023538),
    ),
}


def qualify_ghidra(sources: dict, paths: dict[str, Path]) -> dict:
    result = {}
    overlay_hash = hashlib.sha256(sources["overlay"]).hexdigest()
    for kind, definitions in GHIDRA_FUNCTIONS.items():
        path = paths[kind]
        report = json.loads(path.read_text())
        require(
            report["profile"] == sources["profile"]["id"]
            and report["overlay_sha256"] == overlay_hash
            and report["language"] == "PSX:LE:32:default"
            and report["loader"] == "PSX Executables Loader"
            and report["ghidra_version"] == "12.1.2",
            "unqualified original Ghidra environment or source identity",
        )
        functions = report["decompilation"]
        require(len(functions) == len(definitions), "Ghidra reviewed function set differs")
        instructions = 0
        for function, (begin, end) in zip(functions, definitions, strict=True):
            prefix = "field_" + overlay_hash[:16] + "::" if begin >= 0x8006FAF0 else ""
            require(
                function["entry"] == prefix + f"{begin:08x}"
                and function["complete"]
                and not function["error"]
                and bool(function.get("c"))
                and len(function["instructions"]) == (end - begin) // 4,
                "incomplete Ghidra function export or changed original boundary",
            )
            for index, instruction in enumerate(function["instructions"]):
                address = begin + index * 4
                require(
                    instruction["address"] == prefix + f"{address:08x}"
                    and bytes.fromhex(instruction["bytes"])
                    == source_bytes(sources, address, address + 4)
                    and isinstance(instruction["pcode"], list)
                    and isinstance(instruction["text"], str),
                    "Ghidra instruction listing differs from original source",
                )
                instructions += 1
        result[kind] = {
            "path": str(path),
            "sha256": file_sha(path),
            "functions": len(functions),
            "source_instructions": instructions,
        }
    return result


def correlate(animation_rows, animation_spec, replay_rows, replay_spec):
    names = {"orientation-before", "orientation-after", "replay-before", "replay-after"}
    left = [r for r in animation_rows if r["hook"] in names]
    right = [r for r in replay_rows if r["hook"] in names]
    require(len(left) == len(right) == 3088, "original independent sprite call coverage differs")
    a_hooks = {h["name"]: h for h in animation_spec["hooks"]}
    b_hooks = {h["name"]: h for h in replay_spec["hooks"]}
    mapping = {}
    for before, after in zip(left, right, strict=True):
        require(
            all(before[k] == after[k] for k in ("frontend_run", "pc", "gpr_u32", "hook"))
            and qualified_ranges(before, a_hooks[before["hook"]])
            == qualified_ranges(after, b_hooks[after["hook"]]),
            "original independent sprite call inputs or effects differ",
        )
        if before["hook"].endswith("-before"):
            mapping[before["event"]] = after["event"]
    return mapping


def compare_animation(rows, spec, resources, trig, correlation, replay_effects):
    counts, callers = Counter(), Counter()
    for call in calls(rows, spec, "animation"):
        gpr, old, kind = call.before["gpr_u32"], call.old, call.kind
        address, mode = gpr[4], gpr[5]
        rate = struct.unpack_from("<i", old["sprite-controls"], 8)[0]
        platform = old["sprite-controls"][0x1D]
        if kind in ("orientation", "replay"):
            require(
                call.before["event"] in correlation, "missing independently qualified sprite call"
            )
            stores = replay_effects[correlation[call.before["event"]]]
        elif kind == "matrix":
            stores = [(address, sprite_matrix(old["sprite"], address, trig))]
        elif kind == "bind":
            stores = [(address, bind_current(old["sprite"], mode, platform))]
        elif kind == "header":
            require(
                old["header"] == resources.read(mode, 32),
                "original animation header differs from source",
            )
            stores = [
                (
                    address,
                    apply_header(
                        old["sprite"],
                        address,
                        mode,
                        struct.unpack_from("<3H", old["header"]),
                        rate,
                        platform,
                        trig,
                    ),
                )
            ]
        elif kind == "select":
            stores = [
                (
                    address,
                    select_animation(
                        old["sprite"], address, mode, rate, platform, resources.read, trig
                    ),
                )
            ]
        elif kind == "field-animation":
            actor_pointer = u32(old["descriptor"], 0x4C)
            actor = read_captured(call.before, old, actor_pointer, 312)
            sprite, actor = field_animation(
                old["sprite"],
                address,
                mode,
                old["descriptor"],
                actor,
                *struct.unpack_from("<2h", old["motion-globals"], 0xD8),
                rate,
                platform,
                resources.read,
                trig,
            )
            stores = [(address, sprite), (actor_pointer, actor)]
        else:
            raise ValueError("Unreconstructed original animation hook " + kind)
        compare_effect(call, stores)
        counts[kind] += 1
        callers[(kind, hex(gpr[31]))] += 1
    require(
        counts
        == {
            "matrix": 2889,
            "bind": 69,
            "header": 69,
            "select": 69,
            "field-animation": 69,
            "orientation": 1479,
            "replay": 65,
        },
        "original animation route coverage differs",
    )
    return {"counts": dict(counts), "callers": [list(k) + [v] for k, v in sorted(callers.items())]}


def compare(
    sources: dict, control: Path, loader: Path, animation: Path, replay: Path, ghidra_paths: dict
) -> dict:
    require(sources["map"] == 23, "unqualified original sprite comparison field")
    ghidra = qualify_ghidra(sources, ghidra_paths)
    controls = qualify_control(sources, "control", control, (animation, replay))
    loader_report, party = compare_loader(sources, loader)
    loaded, manifests = {}, {}
    for kind, capture in (("animation", animation), ("replay", replay)):
        spec = specification(sources, (capture / "final.ram").read_bytes(), kind)
        ram, rows, observation = capture_records(capture, sources, spec, WINDOWS)
        loaded[kind] = rows, spec, OriginalResources(sources, ram, party)
        manifests[kind] = {
            "capture": str(capture),
            "records": len(rows),
            "observation_sha256": file_sha(capture / "observation.json"),
            "trace_sha256": observation["instruction_trace"]["trace_sha256"],
            "specification_sha256": observation["instruction_trace"]["specification_sha256"],
        }
    a_rows, a_spec, a_resources = loaded["animation"]
    r_rows, r_spec, r_resources = loaded["replay"]
    correlation = correlate(a_rows, a_spec, r_rows, r_spec)
    replay_report, replay_effects = compare_replay(
        r_rows, r_spec, r_resources, source_bytes(sources, 0x8004FC40, 0x8004FD40)
    )
    animation_report = compare_animation(
        a_rows,
        a_spec,
        a_resources,
        source_bytes(sources, 0x800523F0, 0x800563F0),
        correlation,
        replay_effects,
    )
    return {
        "schema_version": 1,
        "kind": "original_sprite_animation_comparison",
        "result": "passed",
        "source_profile": sources["profile"]["id"],
        "raw_track_sha256": sources["raw_sha256"],
        "map": sources["map"],
        "ghidra": ghidra,
        "captures": manifests,
        "controls": controls,
        "party_loader": loader_report,
        "animation": animation_report,
        "replay": replay_report,
        "exact_independent_records": 3088,
        "resource_reads": {
            kind: {"reads": sum(resource.reads.values()), "distinct_windows": len(resource.reads)}
            for kind, (_, _, resource) in loaded.items()
        },
        "source_windows": [
            {
                "begin": f"0x{begin:08x}",
                "end_exclusive": f"0x{end:08x}",
                "sha256": hashlib.sha256(source_bytes(sources, begin, end)).hexdigest(),
            }
            for begin, end in WINDOWS
        ],
        "tool_sources": {name: file_sha(ROOT / name) for name in TOOL_SOURCES},
        "scope": (
            "Complete observed animation changes, orientation/replay, frame scheduling/metadata "
            "and sprite matrices on one qualified control route. Shared calls are correlated "
            "across two captures, not counted as independent scenarios. Alternate resources, "
            "platforms, allocation, frame formats and GTE product remain explicit; "
            "no broad or native completion claim."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--kind", choices=("animation", "replay"))
    parser.add_argument("--ram", type=Path)
    for name in ("control", "loader", "animation", "replay"):
        parser.add_argument("--" + name + "-capture", type=Path)
    for name in GHIDRA_FUNCTIONS:
        parser.add_argument("--ghidra-" + name, type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = sprite_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            require(
                args.ram is not None and args.kind is not None,
                "prepare needs original RAM and a trace kind",
            )
            verify(args.raw, args.ram, args.profile, args.map)
            result = specification(sources, args.ram.read_bytes(), args.kind)
        else:
            captures = [
                getattr(args, name + "_capture")
                for name in ("control", "loader", "animation", "replay")
            ]
            ghidra = {name: getattr(args, "ghidra_" + name) for name in GHIDRA_FUNCTIONS}
            require(
                all(captures) and all(ghidra.values()),
                "compare needs all original captures and reviewed Ghidra exports",
            )
            result = compare(sources, *captures, ghidra)
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"Original sprite {args.mode} passed: {output}")
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        parser.exit(1, f"Original sprite {args.mode} failed: {error}\n")


if __name__ == "__main__":
    main()
