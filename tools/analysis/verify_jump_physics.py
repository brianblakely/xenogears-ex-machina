"""Compare original party resource loading and bounded jump-physics stages.

Every expected arithmetic result is computed from captured inputs. Source bytes,
pointer relationships, opaque preserved memory and nested call order are checked
separately. Complete animation, collision and control semantics remain outside
this comparison; other sprite-command returns are explicitly counted as opaque.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not __package__:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.field import collision_package
    from tools.analysis.jump_physics import (
        SpriteTiming,
        VerticalState,
        gravity_prefix,
        sprite_impulse,
        terrain_attribute,
        vertical_step,
    )
    from tools.analysis.original_trace import capture_records, exact_payload, ordered_rows, require
    from tools.analysis.party_sprites import (
        LOADER_WINDOWS,
        compare_loader,
        digest,
        loader_specification,
        sprite_sources,
        u32,
    )
    from tools.analysis.verify_collision_math import source_bytes
    from tools.analysis.verify_field import file_sha, private_output, verify
    from tools.reference.instruction_trace import validate_instruction_trace
else:
    from ..reference.instruction_trace import validate_instruction_trace
    from .field import collision_package
    from .jump_physics import (
        SpriteTiming,
        VerticalState,
        gravity_prefix,
        sprite_impulse,
        terrain_attribute,
        vertical_step,
    )
    from .original_trace import capture_records, exact_payload, ordered_rows, require
    from .party_sprites import (
        LOADER_WINDOWS,
        compare_loader,
        digest,
        loader_specification,
        sprite_sources,
        u32,
    )
    from .verify_collision_math import source_bytes
    from .verify_field import file_sha, private_output, verify

PHYSICS_WINDOWS = (
    (0x800183D8, 0x800185A4),
    (0x8001FBE4, 0x8001FC34),
    (0x800219AC, 0x80021A44),
    (0x80021AB8, 0x80021AD8),
    (0x80022224, 0x800223B0),
    (0x80023538, 0x80023658),
    (0x800245D8, 0x8002470C),
    (0x80024EAC, 0x80024F00),
    (0x80080968, 0x800809D0),
    (0x800821F4, 0x8008237C),
    (0x8008505C, 0x8008515C),
)


def s16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def physics_specification(sources: dict, ram: bytes) -> dict:
    table = u32(ram, 0xAFB10)
    descriptor = table - 0x80000000 + 3 * 92
    require(0 <= descriptor <= 0x200000 - 92, "original player descriptor outside RAM")

    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def reg(name, index, size, offset=0):
        return {"name": name, "register": index, "relative_offset": offset, "size": size}

    common = [
        direct("field", 0x4F34C, 4),
        direct("descriptor-table", 0xAFB10, 4),
        direct("player-descriptor", descriptor, 92),
        {
            "name": "player-actor",
            "pointer_offset": descriptor + 0x4C,
            "relative_offset": 0,
            "size": 312,
        },
        {
            "name": "player-sprite",
            "pointer_offset": descriptor + 4,
            "relative_offset": 0,
            "size": 180,
        },
    ]
    hooks = []

    def hook(name, pc, ranges):
        hooks.append(
            {
                "name": name,
                "pc": pc,
                "guard": {
                    "offset": pc - 16 - 0x80000000,
                    "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
                },
                "ranges": common + ranges,
            }
        )

    for name, pc in [
        ("vertical-before", 0x8008505C),
        ("vertical-terrain", 0x80085074),
        ("vertical-after", 0x8008515C),
    ]:
        hook(
            name,
            pc,
            [reg("actor", 17, 312), reg("sprite", 21, 180), direct("mesh-globals", 0xAFB18, 64)],
        )
    for name, pc, sprite, header in [
        ("gravity-before", 0x80023538, 4, 5),
        ("gravity-after", 0x80023658, 16, 17),
    ]:
        hook(
            name,
            pc,
            [
                reg("sprite", sprite, 180),
                reg("header", header, 8),
                direct("sprite-globals", 0x59190, 32),
            ],
        )
    for name, pc in [
        ("impulse-before", 0x800219AC),
        ("impulse-store", 0x80021A40),
        ("sprite-command-after", 0x80021AB8),
    ]:
        ranges = [reg("sprite", 19, 180)]
        if name != "sprite-command-after":
            ranges.append(reg("operand", 17, 8, -1))
        hook(name, pc, ranges + [direct("sprite-globals", 0x59190, 32)])
    hook("animation-before", 0x800821F4, [reg("sprite", 4, 180), reg("descriptor", 6, 92)])
    hook("animation-after", 0x80082360, [reg("descriptor", 17, 92)])
    hook(
        "impulse-reference",
        0x800219C8,
        [
            reg("sprite", 19, 180),
            reg("operand", 17, 8, -1),
            direct("sprite-globals", 0x59190, 32),
            reg("referenced-velocity", 2, 4),
        ],
    )
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": "field23-jump-physics-v3",
            "source_profile": sources["profile"]["id"],
            "start_frame": 4300,
            "end_frame": 4921,
            "max_callbacks": 20000,
            "hooks": hooks,
        }
    )


def timing(payload: dict) -> SpriteTiming:
    return SpriteTiming(
        s16(payload["sprite"], 0x82),
        u32(payload["sprite-globals"], 8),
        u32(payload["sprite"], 0xAC),
    )


def changed(payload: dict, sprite_pointer: int, actor_pointer: int | None, updates: list) -> dict:
    """Memory correlation codec; preserve all bytes outside semantic stores."""
    result = {k: bytearray(v) for k, v in payload.items()}
    aliases = {"sprite": "player-sprite", "actor": "player-actor"}
    pointers = {"sprite": sprite_pointer, "actor": actor_pointer}
    player_pointers = {
        "sprite": u32(payload["player-descriptor"], 4),
        "actor": u32(payload["player-descriptor"], 0x4C),
    }
    for name, offset, value in updates:
        struct.pack_into("<I", result[name], offset, value & 0xFFFFFFFF)
        if pointers[name] == player_pointers[name]:
            struct.pack_into("<I", result[aliases[name]], offset, value & 0xFFFFFFFF)
    return {k: bytes(v) for k, v in result.items()}


def source_region(resources: dict, payload: dict, pointer: int, data: bytes) -> int:
    base = u32(payload["sprite"], 0x44)
    require(
        base in resources and u32(payload["sprite"], 0x48) == base,
        "original sprite does not select a qualified party resource",
    )
    offset = pointer - base
    require(
        0 <= offset <= len(resources[base]) - len(data)
        and resources[base][offset : offset + len(data)] == data,
        "original sprite command or header differs from qualified source",
    )
    return base


def same_call(row: dict, before: dict, delta: int = 0) -> None:
    require(
        row["frontend_run"] == before["frontend_run"]
        and row["gpr_u32"][29] == before["gpr_u32"][29] + delta
        and row["gpr_u32"][31] == before["gpr_u32"][31],
        "original physics call relationship differs",
    )


def compare_physics_records(rows, sources: dict, spec: dict, ram: bytes, resources: dict) -> dict:
    mesh = collision_package(sources["component"])
    mesh_globals = ram[0xAFB18:0xAFB58]
    component = u32(mesh_globals)
    require(
        ram[component - 0x80000000 : component - 0x80000000 + len(sources["component"])]
        == sources["component"],
        "original collision component differs from source",
    )
    require(
        u32(mesh_globals, 8) == component + u32(sources["component"], 0x14),
        "original terrain attribute table pointer differs",
    )
    for index, layer in enumerate(mesh.layers):
        require(
            u32(mesh_globals, 12 + 4 * index)
            == component + u32(sources["component"], 0x18 + 8 * index)
            and u32(mesh_globals, 28 + 4 * index)
            == component + u32(sources["component"], 0x1C + 8 * index)
            and u32(mesh_globals, 44 + 4 * index) == len(layer.triangles),
            "original collision layer pointers or counts differ",
        )
    for pointer, data in resources.items():
        require(
            ram[pointer - 0x80000000 : pointer - 0x80000000 + len(data)] == data,
            "party source differs between loader and physics captures",
        )
    counts, outcomes, header_modes = Counter(), Counter(), Counter()
    impulses, vertical, pending = [], [], {}
    player_descriptor = next(
        r["offset"] for r in spec["hooks"][0]["ranges"] if r["name"] == "player-descriptor"
    )
    for row, payload in ordered_rows(rows, spec):
        name, gpr = row["hook"], row["gpr_u32"]
        counts[name] += 1
        require(
            u32(payload["field"]) == sources["map"]
            and u32(payload["descriptor-table"]) + 3 * 92 == player_descriptor + 0x80000000,
            "original field or player descriptor identity differs",
        )
        if name == "animation-before":
            require(not pending, "nested original field animation selection")
            require(
                gpr[4] == u32(payload["descriptor"], 4)
                and gpr[6] >= u32(payload["descriptor-table"])
                and (gpr[6] - u32(payload["descriptor-table"])) % 92 == 0,
                "original animation descriptor/sprite relationship differs",
            )
            pending["animation"] = [row, payload, False]
        elif name == "gravity-before":
            require(
                set(pending) == {"animation"} and not pending["animation"][2],
                "orphan or repeated original gravity setup",
            )
            animation, old, _ = pending["animation"]
            require(
                gpr[4] == animation["gpr_u32"][4]
                and gpr[29] == animation["gpr_u32"][29] - 64
                and gpr[31] == 0x8002470C
                and row["frontend_run"] == animation["frontend_run"],
                "gravity is not nested in its original animation selection",
            )
            base = source_region(resources, payload, gpr[5], payload["header"])
            mode = animation["gpr_u32"][5]
            require(0 <= mode <= 3, "unobserved animation selection mode")
            data = resources[base]
            table = u32(data, 4)
            header = table + struct.unpack_from("<H", data, table + 2 + mode * 2)[0]
            require(gpr[5] == base + header, "animation mode selected a different source header")
            header_modes[f"0x{base:08x}:mode{mode}"] += 1
            effect = gravity_prefix(
                struct.unpack_from("<3H", payload["header"]),
                gpr[5],
                u32(payload["sprite"], 0xA8),
                timing(payload),
            )
            updates = [
                ("sprite", o, v)
                for o, v in [
                    (0x1C, effect.gravity),
                    (0xA8, effect.sprite_flags),
                    (0x58, effect.header_pointer),
                    (0x64, effect.command_pointer),
                    (0x54, effect.frame_pointer),
                ]
            ]
            pending["gravity"] = (row, payload, changed(payload, gpr[4], None, updates))
        elif name == "gravity-after":
            require(set(pending) == {"animation", "gravity"}, "unpaired original gravity return")
            before, old, expected = pending.pop("gravity")
            same_call(row, before, -32)
            require(
                gpr[16] == before["gpr_u32"][4] and gpr[17] == before["gpr_u32"][5],
                "original gravity arguments changed",
            )
            exact_payload(payload, expected, "gravity prefix")
            pending["animation"][2] = True
            outcomes["gravity_prefixes"] += 1
        elif name == "animation-after":
            require(set(pending) == {"animation"}, "unpaired original animation return")
            before, old, completed = pending.pop("animation")
            require(
                completed
                and gpr[17] == before["gpr_u32"][6]
                and gpr[29] == before["gpr_u32"][29] - 32
                and gpr[31] == 0x800822D0
                and row["frontend_run"] == before["frontend_run"]
                and payload["descriptor"] == old["descriptor"],
                "original animation call correlation differs",
            )
            outcomes["animation_calls_correlated_not_fully_reconstructed"] += 1
        elif name == "impulse-before":
            require(not pending, "nested original sprite impulse")
            require(
                gpr[31] == 0x80024EDC and gpr[5] == 0xA1 and payload["operand"][0] == 0xA1,
                "original impulse did not originate at sprite A1 dispatch",
            )
            source_region(resources, payload, gpr[17] - 1, payload["operand"])
            pending["impulse"] = [row, payload, None, None, False]
        elif name == "impulse-reference":
            require(set(pending) == {"impulse"}, "orphan original impulse reference")
            before, old, _, effect, seen = pending["impulse"]
            require(
                not seen
                and effect is None
                and u32(old["sprite"], 0xA8) & 1
                and gpr[2] == u32(old["sprite"], 0x7C),
                "original impulse reference relationship differs",
            )
            same_call(row, before)
            exact_payload(
                {k: v for k, v in payload.items() if k != "referenced-velocity"},
                old,
                "impulse reference",
            )
            pending["impulse"][2] = u32(payload["referenced-velocity"])
            pending["impulse"][4] = True
        elif name == "impulse-store":
            require(set(pending) == {"impulse"}, "orphan original impulse store")
            before, old, reference, effect, seen = pending["impulse"]
            require(
                effect is None and seen == bool(u32(old["sprite"], 0xA8) & 1),
                "missing or repeated original impulse reference",
            )
            same_call(row, before)
            effect = sprite_impulse(
                old["operand"][1], timing(old), u32(old["sprite"], 0xA8), reference
            )
            require(
                gpr[19] == before["gpr_u32"][19]
                and gpr[17] == before["gpr_u32"][17]
                and gpr[2] == effect.velocity & 0xFFFFFFFF,
                "original impulse quotient or arguments differ",
            )
            exact_payload(
                payload,
                changed(old, gpr[19], None, [("sprite", 0x10, effect.division_numerator)]),
                "impulse intermediate",
            )
            pending["impulse"][3] = effect
        elif name == "sprite-command-after":
            if "impulse" not in pending:
                require(not pending, "unexpected sprite command inside compared stage")
                outcomes["other_sprite_commands_opaque"] += 1
                continue
            before, old, reference, effect, seen = pending.pop("impulse")
            require(
                effect is not None and gpr[19] == before["gpr_u32"][19],
                "missing original impulse store",
            )
            same_call(row, before)
            expected = changed(old, gpr[19], None, [("sprite", 0x10, effect.velocity)])
            expected.pop("operand")
            exact_payload(payload, expected, "impulse final")
            outcomes["impulses"] += 1
            impulses.append(
                {
                    "frontend_run": row["frontend_run"],
                    "sprite_pointer": f"0x{gpr[19]:08x}",
                    "source_base": f"0x{u32(old['sprite'], 0x44):08x}",
                    "velocity": effect.velocity,
                    "division_numerator": effect.division_numerator,
                    "used_reference": effect.used_reference,
                    "reference_value": reference,
                }
            )
        elif name == "vertical-before":
            require(
                not pending
                and gpr[17] == u32(payload["player-descriptor"], 0x4C)
                and gpr[21] == u32(payload["player-descriptor"], 4)
                and payload["mesh-globals"] == mesh_globals,
                "unqualified original vertical actor or mesh",
            )
            actor, sprite = payload["actor"], payload["sprite"]
            terrain = terrain_attribute(
                u32(actor, 4), s16(actor, 0x10), struct.unpack_from("<4h", actor, 8), mesh
            )
            effect = vertical_step(
                VerticalState(u32(actor, 0x24), u32(sprite, 0x10), u32(actor), u32(actor, 0xF0)),
                u32(sprite, 0x1C),
                s16(sprite, 0x84),
                s16(actor, 0x10),
                gpr[30],
                terrain,
            )
            pending["vertical"] = [row, payload, effect, terrain, False]
        elif name == "vertical-terrain":
            require(set(pending) == {"vertical"}, "orphan original vertical terrain result")
            before, old, effect, terrain, seen = pending["vertical"]
            require(
                not seen
                and gpr[2] == terrain
                and gpr[31] == 0x80085074
                and gpr[29] == before["gpr_u32"][29]
                and gpr[30] == before["gpr_u32"][30]
                and row["frontend_run"] == before["frontend_run"],
                "original vertical terrain result differs",
            )
            exact_payload(
                payload,
                changed(old, gpr[21], gpr[17], [("actor", 0x24, effect.integrated_y)]),
                "vertical integration",
            )
            pending["vertical"][4] = True
        elif name == "vertical-after":
            require(set(pending) == {"vertical"}, "orphan original vertical-stage return")
            before, old, effect, terrain, seen = pending.pop("vertical")
            require(
                seen
                and gpr[17] == before["gpr_u32"][17]
                and gpr[21] == before["gpr_u32"][21]
                and gpr[30] == before["gpr_u32"][30]
                and gpr[31] == 0x80085074
                and gpr[29] == before["gpr_u32"][29]
                and row["frontend_run"] == before["frontend_run"],
                "original vertical call or previous-layer register differs",
            )
            state = effect.state
            expected = changed(
                old,
                gpr[21],
                gpr[17],
                [
                    ("actor", 0x24, state.y),
                    ("actor", 0, state.flags),
                    ("actor", 0xF0, state.vertical_marker),
                    ("sprite", 0x10, state.velocity),
                ],
            )
            exact_payload(payload, expected, "vertical-stage result")
            outcomes["vertical_" + effect.branch] += 1
            vertical.append(
                {
                    "frontend_run": row["frontend_run"],
                    "branch": effect.branch,
                    "y": state.y,
                    "velocity": state.velocity,
                    "flags": state.flags,
                    "marker": state.vertical_marker,
                }
            )
        else:
            raise ValueError("unimplemented original jump observation")
    require(
        not pending
        and outcomes["gravity_prefixes"]
        and outcomes["impulses"]
        and outcomes["vertical_airborne"]
        and outcomes["vertical_floor"],
        "incomplete original jump comparison",
    )
    return {
        "result": "passed",
        "records": sum(counts.values()),
        "counts": dict(counts),
        "outcomes": dict(outcomes),
        "header_modes": dict(header_modes),
        "impulses": impulses,
        "vertical_updates": vertical,
    }


def compare(sources: dict, loader_capture: Path, physics_capture: Path) -> dict:
    loader, resources = compare_loader(sources, loader_capture)
    spec = physics_specification(sources, (physics_capture / "final.ram").read_bytes())
    ram, rows, observation = capture_records(physics_capture, sources, spec, PHYSICS_WINDOWS)
    require(
        u32(ram, 0x183D8 + (0xA1 - 0x8A) * 4) == 0x800219AC,
        "original sprite A1 dispatch table differs",
    )
    physics = compare_physics_records(rows, sources, spec, ram, resources)
    paths = (
        "tools/analysis/jump_physics.py",
        "tools/analysis/party_sprites.py",
        "tools/analysis/original_trace.py",
        "tools/analysis/verify_jump_physics.py",
        "tools/analysis/arithmetic.py",
        "tools/analysis/field.py",
        "tools/analysis/packed.py",
        "tools/analysis/verify_collision_math.py",
        "tools/analysis/verify_field.py",
    )
    return {
        "schema_version": 1,
        "kind": "original_jump_physics_comparison",
        "result": "passed",
        "source_profile": sources["profile"]["id"],
        "map": sources["map"],
        "raw_track_sha256": sources["raw_sha256"],
        "loader": loader,
        "physics": physics,
        "trace": {
            "path": str(physics_capture / "instruction-trace.jsonl"),
            "sha256": observation["instruction_trace"]["trace_sha256"],
        },
        "observation": {
            "path": str(physics_capture / "observation.json"),
            "sha256": file_sha(physics_capture / "observation.json"),
        },
        "tool_sources": {path: file_sha(ROOT / path) for path in paths},
        "code_windows": [
            {
                "begin": f"0x{begin:08x}",
                "end_exclusive": f"0x{end:08x}",
                "sha256": digest(source_bytes(sources, begin, end)),
            }
            for begin, end in LOADER_WINDOWS + PHYSICS_WINDOWS
        ],
        "scope": (
            "Two party decodes, observed directory/index selections, gravity prefix, A1 impulse "
            "and bounded vertical stage. Animation calls are correlated; other sprite commands "
            "and complete collision/control behavior remain unreconstructed."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare-loader", "prepare-physics", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--ram", type=Path)
    parser.add_argument("--loader-capture", type=Path)
    parser.add_argument("--physics-capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = sprite_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare-loader":
            result = loader_specification(sources)
        elif args.mode == "prepare-physics":
            require(args.ram is not None, "physics preparation needs qualified original RAM")
            verify(args.raw, args.ram, args.profile, args.map)
            result = physics_specification(sources, args.ram.read_bytes())
        else:
            require(
                args.loader_capture is not None and args.physics_capture is not None,
                "comparison needs original loader and physics captures",
            )
            result = compare(sources, args.loader_capture, args.physics_capture)
            result["source_check"] = verify(
                args.raw, args.physics_capture / "final.ram", args.profile, args.map
            )
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output}")
    except (ValueError, KeyError, IndexError, OSError, struct.error) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
