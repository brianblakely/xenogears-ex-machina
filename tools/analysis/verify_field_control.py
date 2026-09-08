"""Compare recovered field control requests with original execution.

Inputs, opaque preserved bytes, nested calls, source bytecode, code identity and
original pointer/stack relationships are checked independently of output values.
This is local original-data analysis, not native movement or hardware timing.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not __package__:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.field import event_package, field_components
    from tools.analysis.field_control import (
        ControlActor,
        ControlInputs,
        ControlState,
        EncounterGates,
        encounter_early_exit,
        request_control,
        terrain_jump_result,
    )
    from tools.analysis.verify_collision_math import math_sources, source_bytes
    from tools.analysis.verify_field import file_sha, private_output, verify
    from tools.reference.instruction_trace import validate_instruction_trace
    from tools.reference.memory_sampler import ram_pointer_offset
else:
    from ..reference.instruction_trace import validate_instruction_trace
    from ..reference.memory_sampler import ram_pointer_offset
    from .field import event_package, field_components
    from .field_control import (
        ControlActor,
        ControlInputs,
        ControlState,
        EncounterGates,
        encounter_early_exit,
        request_control,
        terrain_jump_result,
    )
    from .verify_collision_math import math_sources, source_bytes
    from .verify_field import file_sha, private_output, verify

HOOKS = (
    ("wrapper-before", 0x8009F5A8),
    ("control-before", 0x8009F5F4),
    ("control-after", 0x8009F9F8),
    ("wrapper-after", 0x8009F5EC),
    ("encounter-before", 0x80079288),
    ("encounter-after", 0x80079544),
    ("terrain-before", 0x80081F5C),
    ("terrain-after", 0x80081F78),
)
WINDOWS = (
    (0x8009F5A8, 0x8009FA00),
    (0x80081F5C, 0x80081F80),
    (0x80079288, 0x80079338),
    (0x80079530, 0x8007954C),
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def u16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def s16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def control_sources(raw: Path, profile: str, map_id: int) -> dict:
    sources = math_sources(raw, profile, map_id)
    sources["events"] = event_package(field_components(sources["field_source"])[5].logical_data)
    sources["directions"] = source_bytes(sources, 0x800ADF68, 0x800ADFA8)
    return sources


def specification(sources: dict, start: int, end: int, name: str) -> dict:
    def direct(label, offset, size):
        return {"name": label, "offset": offset, "size": size}

    common = [
        direct("actor-pointer", 0xB0078, 4),
        {"name": "actor", "pointer_offset": 0xB0078, "relative_offset": 0, "size": 312},
        direct("field-controls", 0xADB00, 256),
        direct("event-controls", 0xB00B0, 32),
        direct("parameters", 0xB2170, 528),
        direct("held-inputs", 0xAFE98, 8),
        direct("pressed-inputs", 0xC2690, 8),
        direct("direction-tables", 0xADF68, 64),
        direct("camera-angle", 0xAF98C, 2),
        direct("field", 0x4F34C, 4),
        direct("actor-index", 0xAFD1C, 4),
        direct("music-result", 0x4F308, 4),
        direct("script-pointer", 0xADC00, 4),
    ] + [direct(f"dialogue-{i}", 0xC2A14 + i * 0x498, 2) for i in range(4)]
    bytecode_size = len(sources["events"].bytecode)
    hooks = [
        {
            "name": label,
            "pc": pc,
            "guard": {
                "offset": pc - 16 - 0x80000000,
                "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
            },
            "ranges": common,
            "digests": [
                {
                    "name": "bytecode",
                    "pointer_offset": 0xADC00,
                    "size": bytecode_size,
                    "max_bytes": bytecode_size,
                }
            ],
        }
        for label, pc in HOOKS
    ]
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": name,
            "source_profile": sources["profile"]["id"],
            "start_frame": start,
            "end_frame": end,
            "max_callbacks": min(18000, 64 * 1024 * 1024 // max(bytecode_size, 1)),
            "hooks": hooks,
        }
    )


def read_semantics(payload: dict[str, bytes]) -> tuple[ControlActor, ControlState, ControlInputs]:
    a, f, p = (payload[k] for k in ("actor", "field-controls", "parameters"))
    actor = ControlActor(
        u32(a),
        u32(a, 0x14),
        tuple(s16(a, offset) for offset in (0x22, 0x26, 0x2A)),
        struct.unpack_from("<3h", a, 0x68),
        u16(a, 0x104),
        u16(a, 0xE8),
        u16(a, 0xCC),
    )
    state = ControlState(
        u16(f, 2), u16(p, 0x1D2), u32(f, 0x28), u32(f, 0x68), u32(payload["event-controls"], 0x10)
    )
    gates = EncounterGates(
        u32(f, 0xDC),
        u32(f, 0xE4),
        u32(f, 0xEC),
        u32(payload["music-result"]),
        u32(p, 0x128),
        s16(p, 6),
        u32(f, 0x2C),
        f[4],
    )
    inputs = ControlInputs(
        u16(payload["held-inputs"], 4),
        u16(payload["pressed-inputs"], 4),
        tuple(s16(payload[f"dialogue-{i}"]) for i in range(4)),
        p[0x5E],
        u32(f, 0x64),
        s16(p, 0x1D4),
        u16(p, 0x1D0),
        u32(p, 0x1F0),
        p[0x1E4],
        tuple(struct.unpack_from("<16H", payload["direction-tables"], i * 32) for i in range(2)),
        s16(payload["camera-angle"]),
        gates,
    )
    return actor, state, inputs


def correlate(
    payload: dict[str, bytes], actor: ControlActor, state: ControlState
) -> dict[str, bytes]:
    """Original memory codec only: preserve all unrelated captured bytes."""
    output = {name: bytearray(value) for name, value in payload.items()}
    a, f, p = (output[k] for k in ("actor", "field-controls", "parameters"))
    struct.pack_into("<I", a, 0, actor.flags)
    for offset, value in ((0x104, actor.direction), (0xE8, actor.animation_mode), (0xCC, actor.pc)):
        struct.pack_into("<H", a, offset, value)
    struct.pack_into("<H", f, 2, state.stationary_counter)
    struct.pack_into("<H", p, 0x1D2, state.repeat_remaining)
    struct.pack_into("<I", f, 0x28, state.latched_jump_setting)
    struct.pack_into("<I", f, 0x68, state.updated)
    struct.pack_into("<I", output["event-controls"], 0x10, state.break_requested)
    return {name: bytes(value) for name, value in output.items()}


def qualified(row: dict, hook: dict, sources: dict) -> dict[str, bytes]:
    require(row["pc"] == hook["pc"], "control trace PC differs from specification")
    registers = row["gpr_u32"]
    require(
        len(registers) == 34 and all(type(v) is int and 0 <= v <= 0xFFFFFFFF for v in registers),
        "control trace has invalid original registers",
    )
    require(
        row["code"] == u32(source_bytes(sources, row["pc"], row["pc"] + 4)),
        "control instruction differs from original source",
    )
    require(len(row["ranges"]) == len(hook["ranges"]), "incomplete control ranges")
    payload = {r["name"]: bytes.fromhex(r["hex"]) for r in row["ranges"] if "hex" in r}
    require(len(payload) == len(row["ranges"]), "duplicate or unavailable control ranges")
    for actual, expected in zip(row["ranges"], hook["ranges"], strict=True):
        require(
            actual["name"] == expected["name"]
            and actual["size"] == expected["size"]
            and len(payload[actual["name"]]) == expected["size"]
            and "unavailable" not in actual,
            "substituted or incomplete control range",
        )
        if "pointer_offset" in expected:
            pointer = u32(payload["actor-pointer"])
            require(
                actual.get("pointer_value") == pointer
                and actual.get("pointer_offset") == expected["pointer_offset"]
                and actual.get("relative_offset") == 0
                and "register" not in actual,
                "control actor reference differs from captured current actor",
            )
            offset = ram_pointer_offset(pointer)
            require(offset is not None, "control actor pointer outside RAM")
        else:
            offset = expected["offset"]
            require(
                "pointer_offset" not in actual and "register" not in actual,
                "substituted direct control range",
            )
        require(
            actual.get("resolved_space", "ram") == "ram"
            and actual["resolved_offset"] == offset
            and 0 <= offset <= 0x200000 - expected["size"],
            "control range resolved outside expected RAM",
        )
    require(u32(payload["field"]) == sources["map"], "control trace belongs to a different field")
    require(
        u32(payload["actor-index"]) < len(sources["events"].entries),
        "control actor index outside source",
    )
    require(
        payload["direction-tables"] == sources["directions"], "original direction tables changed"
    )
    pointer = u32(payload["script-pointer"])
    offset = ram_pointer_offset(pointer)
    bytecode = sources["events"].bytecode
    require(
        offset is not None and offset + len(bytecode) <= 0x200000,
        "invalid control bytecode pointer",
    )
    require(
        row.get("digests")
        == [
            {
                "name": "bytecode",
                "pointer_value": pointer,
                "size": len(bytecode),
                "resolved_offset": offset,
                "sha256": hashlib.sha256(bytecode).hexdigest(),
            }
        ],
        "control bytecode digest or pointer differs",
    )
    return payload


def equal_payload(actual: dict, expected: dict, context: str) -> None:
    require(actual.keys() == expected.keys(), f"{context}: changed range set")
    for name in actual:
        require(actual[name] == expected[name], f"{context}: original {name} differs")


def same_call(after: dict, before: dict, stack_delta: int = 0) -> None:
    require(
        after["frontend_run"] == before["frontend_run"], "control call crosses frontend boundary"
    )
    require(
        after["gpr_u32"][29] == before["gpr_u32"][29] + stack_delta, "control call stack differs"
    )
    require(after["gpr_u32"][31] == before["gpr_u32"][31], "control return address differs")


def compare_records(rows, sources: dict, spec: dict) -> dict:
    hooks = {h["name"]: h for h in spec["hooks"]}
    pending = {}
    counts, outcomes, jumps, polls, directions, actors = (Counter() for _ in range(6))
    jump_events = []
    actor_pointers = {}
    previous_run = -1
    for number, row in enumerate(rows):
        require(
            type(row["event"]) is int and row["event"] == number, "non-contiguous control trace"
        )
        require(
            type(row["frontend_run"]) is int
            and spec["start_frame"] <= row["frontend_run"] < spec["end_frame"]
            and row["frontend_run"] >= previous_run,
            "invalid control frontend ordering",
        )
        previous_run = row["frontend_run"]
        name, registers = row["hook"], row["gpr_u32"]
        require(name in hooks, "unexpected control hook")
        payload = qualified(row, hooks[name], sources)
        counts[name] += 1
        actor, state, inputs = read_semantics(payload)
        if name == "wrapper-before":
            require(not pending, "nested control wrapper")
            require(
                registers[31] == 0x800A1F78, "wrapper did not come from original primary dispatch"
            )
            require(
                sources["events"].bytecode[actor.pc] == 0x0C, "wrapper PC is not original opcode 0c"
            )
            pending["wrapper"] = [row, payload, request_control(0x0C, actor, state, inputs), False]
        elif name == "control-before":
            require("control" not in pending and "service" not in pending, "nested control call")
            if "wrapper" in pending:
                wrapper = pending["wrapper"]
                require(not wrapper[3], "duplicate inner control call")
                equal_payload(payload, wrapper[1], "wrapper to control")
                require(
                    registers[29] == wrapper[0]["gpr_u32"][29] - 24 and registers[31] == 0x8009F5C8,
                    "wrapper call relationship differs",
                )
            else:
                require(
                    sources["events"].bytecode[actor.pc] == 0xA7,
                    "control PC is not original opcode a7",
                )
                require(
                    registers[31] == 0x800A1F78,
                    "control did not come from original primary dispatch",
                )
            effect = request_control(0xA7, actor, state, inputs)
            pending["control"] = [row, payload, effect, []]
            index, pointer = u32(payload["actor-index"]), u32(payload["actor-pointer"])
            require(
                actor_pointers.setdefault(index, pointer) == pointer,
                "control actor changed allocation",
            )
            actors[str(index)] += 1
        elif name in ("encounter-before", "terrain-before"):
            require(
                "control" in pending and "service" not in pending,
                "orphan or nested control service",
            )
            before, old, effect, calls = pending["control"]
            service = name.split("-")[0]
            require(
                len(calls) < len(effect.calls) and effect.calls[len(calls)] == service,
                "unexpected control service order",
            )
            require(
                registers[29] == before["gpr_u32"][29] - 96
                and row["frontend_run"] == before["frontend_run"],
                "control service stack differs",
            )
            if service == "encounter":
                require(registers[31] == 0x8009F694, "encounter caller differs")
                equal_payload(payload, old, "encounter entry")
                reason = encounter_early_exit(inputs.encounter)
                require(reason is not None, "active encounter selection remains unrecovered")
                polls[reason] += 1
            else:
                old_actor, old_state, _ = read_semantics(old)
                intermediate = replace(
                    old_state, stationary_counter=effect.state.stationary_counter, updated=1
                )
                equal_payload(payload, correlate(old, old_actor, intermediate), "terrain entry")
                alternate = effect.terrain_call_variant == "alternate"
                require(
                    registers[31] == (0x8009F898 if alternate else 0x8009F80C)
                    and registers[4] == u32(payload["actor-pointer"]),
                    "terrain call reference differs",
                )
            pending["service"] = [row, payload, service]
        elif name in ("encounter-after", "terrain-after"):
            require(
                "control" in pending and "service" in pending, "unpaired control service return"
            )
            before, old, service = pending.pop("service")
            require(name == service + "-after", "wrong control service return")
            same_call(row, before)
            equal_payload(payload, old, "control service return")
            if service == "terrain":
                # Hook is before JR's delay slot, which negates this boolean.
                require(
                    registers[2] == -terrain_jump_result(actor),
                    "original terrain predicate differs",
                )
            pending["control"][3].append(service)
        elif name == "control-after":
            require("control" in pending and "service" not in pending, "unpaired control return")
            before, old, effect, calls = pending.pop("control")
            require(tuple(calls) == effect.calls, "missing original control service call")
            same_call(row, before, -96)
            equal_payload(payload, correlate(old, effect.actor, effect.state), "control return")
            outcomes[effect.eligibility] += 1
            jumps[effect.jump] += 1
            directions[f"0x{effect.actor.direction:04x}"] += 1
            if effect.jump in ("pressed", "held_retry", "jump_or_airborne"):
                jump_events.append(
                    {
                        "frontend_run": row["frontend_run"],
                        "actor": u32(payload["actor-index"]),
                        "outcome": effect.jump,
                    }
                )
            if "wrapper" in pending:
                pending["wrapper"][3] = True
        else:
            require(
                name == "wrapper-after" and set(pending) == {"wrapper"}, "unpaired wrapper return"
            )
            before, old, effect, finished = pending.pop("wrapper")
            require(finished, "wrapper missing inner control call")
            same_call(row, before)
            equal_payload(payload, correlate(old, effect.actor, effect.state), "wrapper return")
    require(not pending, "incomplete control trace at end")
    require(counts["control-before"] > 0, "no original control calls compared")
    return {
        "result": "passed",
        "records": sum(counts.values()),
        "counts": dict(counts),
        "actors": dict(actors),
        "eligibility": dict(outcomes),
        "jump_decisions": dict(jumps),
        "encounter_early_exits": dict(polls),
        "directions": dict(directions),
        "jump_events": jump_events,
    }


def compare(sources: dict, capture: Path, spec: dict) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    metadata = observation["instruction_trace"]
    require(
        observation["source_profile"] == sources["profile"]["id"]
        and observation["content_sha256"]
        == sources["profile"]["measurement"]["source"]["chd"]["sha256"]
        and observation["scenario"]["complete"]
        and metadata["core_extension_api_version"] == 2
        and metadata["spec"] == spec
        and not metadata["failed"]
        and not metadata["budget_reached"]
        and not metadata["unavailable_ranges"]
        and not any(metadata["guard_mismatches_by_hook"].values()),
        "incomplete or unqualified original control capture",
    )
    require(
        metadata["tool_sha256"] == file_sha(ROOT / "tools/reference/instruction_trace.py")
        and metadata["memory_helpers_sha256"]
        == file_sha(ROOT / "tools/reference/memory_sampler.py")
        and metadata["validation_helpers_sha256"]
        == file_sha(ROOT / "tools/reference/scenario_program.py")
        and all(
            digest == file_sha(ROOT / "nix" / name)
            for name, digest in metadata["core_extension_inputs"].items()
        ),
        "original control collector sources changed",
    )
    trace, spec_path = capture / "instruction-trace.jsonl", capture / "instruction-trace-spec.json"
    require(
        file_sha(trace) == metadata["trace_sha256"]
        and file_sha(spec_path) == metadata["specification_sha256"]
        and json.loads(spec_path.read_text()) == spec,
        "original control trace fingerprint differs",
    )
    ram_path = capture / "final.ram"
    ram = ram_path.read_bytes()
    require(
        len(ram) == 0x200000
        and observation["captures"][-1]["frame"] == observation["frames"]
        and file_sha(ram_path) == observation["captures"][-1]["ram_sha256"],
        "control final RAM differs",
    )
    for begin, end in WINDOWS + ((0x800AE2A0, 0x800AE6A0),):
        require(
            ram[begin - 0x80000000 : end - 0x80000000] == source_bytes(sources, begin, end),
            "control original instructions or dispatch table differ",
        )
    for opcode, target in ((0x0C, 0x8009F5A8), (0xA7, 0x8009F5F4)):
        require(
            u32(ram, 0xAE2A0 + opcode * 4) == target, "original control dispatch target differs"
        )
    with trace.open() as stream:
        result = compare_records((json.loads(line) for line in stream), sources, spec)
    require(
        result["records"] == metadata["records"] == metadata["candidate_callbacks"]
        and result["records"] < spec["max_callbacks"],
        "control record count or budget differs",
    )
    return {
        "schema_version": 1,
        "kind": "original_field_control_comparison",
        **result,
        "source_profile": sources["profile"]["id"],
        "map": sources["map"],
        "raw_track_sha256": sources["raw_sha256"],
        "frontend_window": [spec["start_frame"], spec["end_frame"]],
        "trace": {"path": str(trace), "sha256": file_sha(trace)},
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "code_windows": [
            {
                "begin": f"0x{begin:08x}",
                "end_exclusive": f"0x{end:08x}",
                "sha256": hashlib.sha256(source_bytes(sources, begin, end)).hexdigest(),
            }
            for begin, end in WINDOWS
        ],
        "direction_tables_sha256": hashlib.sha256(sources["directions"]).hexdigest(),
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in (
                "tools/analysis/field_control.py",
                "tools/analysis/verify_field_control.py",
                "tools/analysis/verify_collision_math.py",
                "tools/analysis/verify_field.py",
            )
        },
        "scope": (
            "Captured control requests and encounter early returns; movement integration "
            "and active encounter selection remain unresolved."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--start", type=int, required=True)
    parser.add_argument("--end", type=int, required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = control_sources(args.raw, args.profile, args.map)
        spec = specification(sources, args.start, args.end, args.name)
        if args.mode == "prepare":
            result = spec
        else:
            if args.capture is None:
                raise ValueError("control comparison needs an original capture")
            source_check = verify(args.raw, args.capture / "final.ram", args.profile, args.map)
            result = {**compare(sources, args.capture, spec), "source_check": source_check}
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output}")
    except (ValueError, KeyError, OSError, struct.error) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
