"""Compare recovered slot selection and handler effects with original execution.

This is private-data analysis, not a native VM or complete scheduler. Expected
states are reconstructed from original before snapshots and source bytecode;
after snapshots are independently captured from the original MIPS handlers.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if __package__:
    from ..reference.instruction_trace import validate_instruction_trace
    from ..reference.memory_sampler import ram_pointer_offset
    from .event_state import (
        EventEffect,
        InterpreterControl,
        correlate_actor_scripts,
        execute,
        read_actor_scripts,
        select_slot,
    )
    from .events import Variables, decode_instruction
    from .field import event_package, field_components
    from .packed import decode_block
    from .verify_field import file_sha, load_sources, private_output, verify
else:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.event_state import (
        EventEffect,
        InterpreterControl,
        correlate_actor_scripts,
        execute,
        read_actor_scripts,
        select_slot,
    )
    from tools.analysis.events import Variables, decode_instruction
    from tools.analysis.field import event_package, field_components
    from tools.analysis.packed import decode_block
    from tools.analysis.verify_field import file_sha, load_sources, private_output, verify
    from tools.reference.instruction_trace import validate_instruction_trace
    from tools.reference.memory_sampler import ram_pointer_offset

BASE = 0x8006FAF0
# Entry, after all semantic stores, exclusive function end. Addresses belong to
# the qualified shared field overlay, not to every resident of this RAM region.
HANDLERS = {
    0x00: (0x800A1B70, 0x800A1BC8, 0x800A1BD0),
    0x01: (0x800A1E74, 0x800A1E94, 0x800A1E9C),
    0x04: (0x800A1A8C, 0x800A1B54, 0x800A1B70),
    0x26: (0x8009DD34, 0x8009DDDC, 0x8009DDEC),
    0x35: (0x8009D9A4, 0x8009DA08, 0x8009DA1C),
    0x36: (0x8009D960, 0x8009D99C, 0x8009D9A4),
    0x37: (0x8009D91C, 0x8009D958, 0x8009D960),
    0x38: (0x8009D890, 0x8009D908, 0x8009D91C),
    0x39: (0x8009D804, 0x8009D87C, 0x8009D890),
}
GROUPS = {
    "slot-control": ((0x00, 0x01, 0x04), 65000),
    "wait-variables": ((0x26, 0x35, 0x36, 0x37, 0x38, 0x39), 12000),
    "slot-selection": ((), 140000),
}
SELECTION = (0x800A2194, 0x800A2218, 0x800A221C)


def specification(sources: dict, group: str, start: int, end: int) -> dict:
    overlay = decode_block(sources["overlay_packed"]).data
    opcodes, budget = GROUPS[group]
    common = [
        {"name": "actor", "pointer_offset": 0xB0078, "relative_offset": 0, "size": 256},
        {"name": "field", "offset": 0x4F34C, "size": 4},
        {"name": "actor-index", "offset": 0xAFD1C, "size": 4},
        {"name": "budget-mode", "offset": 0xAFFEC, "size": 4},
        {"name": "break-requested", "offset": 0xB00C0, "size": 4},
    ]
    if group == "wait-variables":
        common += [
            {"name": "variables-low", "offset": 0xC3A68, "size": 1024},
            {"name": "variables-high", "offset": 0xC3E68, "size": 1024},
            {
                "name": "variable-types",
                "pointer_offset": 0xADBF8,
                "relative_offset": 0,
                "size": 128,
            },
        ]
    windows = [(f"op-{opcode:02x}", HANDLERS[opcode]) for opcode in opcodes]
    if group == "slot-selection":
        windows = [("select", SELECTION)]
    hooks = []
    for name, (before, after, finish) in windows:
        guard = {
            "offset": before - 0x80000000,
            "expected": overlay[before - BASE : finish - BASE].hex(),
        }
        for side, address in (("before", before), ("after", after)):
            hooks.append(
                {"name": f"{name}-{side}", "pc": address, "guard": guard, "ranges": common}
            )
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"event-{group}-{start}-{end}",
            "source_profile": sources["profile"]["id"],
            "start_frame": start,
            "end_frame": end,
            "max_callbacks": budget,
            "hooks": hooks,
        }
    )


def record_ranges(record: dict, hook: dict) -> dict[str, bytes]:
    """Reject incomplete, relocated or substituted input snapshots."""
    if record["pc"] != hook["pc"]:
        raise ValueError("Original hook PC differs from the guarded specification")
    guard = hook["guard"]
    code = bytes.fromhex(guard["expected"])
    offset = hook["pc"] - 0x80000000 - guard["offset"]
    if record["code"] != int.from_bytes(code[offset : offset + 4], "little"):
        raise ValueError("Original hook instruction differs from the source")
    if len(record["ranges"]) != len(hook["ranges"]):
        raise ValueError("Original trace is missing a specified range")
    result = {}
    for actual, expected in zip(record["ranges"], hook["ranges"], strict=True):
        if actual["name"] != expected["name"] or actual["size"] != expected["size"]:
            raise ValueError("Original range name or size differs from specification")
        if "pointer_offset" in expected:
            if (
                actual["pointer_offset"] != expected["pointer_offset"]
                or actual["relative_offset"] != expected["relative_offset"]
            ):
                raise ValueError("Original pointer source differs from specification")
            resolved = ram_pointer_offset(actual["pointer_value"])
            if resolved is None:
                raise ValueError("Original pointer is outside system RAM")
            resolved += expected["relative_offset"]
        else:
            resolved = expected["offset"]
        data = bytes.fromhex(actual["hex"])
        if actual["resolved_offset"] != resolved or len(data) != expected["size"]:
            raise ValueError("Original range address or payload is incomplete")
        result[actual["name"]] = data
    return result


def compare_records(records, spec: dict, package, map_id: int) -> dict:
    hooks = {hook["name"]: hook for hook in spec["hooks"]}
    pending = None
    counts, cases, sites = Counter(), Counter(), set()
    total, last_frame = 0, -1
    for record in records:
        if record["event"] != total or total >= spec["max_callbacks"]:
            raise ValueError("Noncontiguous or over-budget original event trace")
        frame = record["frontend_run"]
        if not spec["start_frame"] <= frame < spec["end_frame"] or frame < last_frame:
            raise ValueError("Original event trace has invalid frontend ordering")
        total, last_frame = total + 1, frame
        hook = hooks[record["hook"]]
        ranges = record_ranges(record, hook)
        actor_index = int.from_bytes(ranges["actor-index"], "little")
        if int.from_bytes(ranges["field"], "little") != map_id:
            raise ValueError("Original event trace enters a different field")
        idle_entry = package.entry(actor_index, 1)
        actor = read_actor_scripts(ranges["actor"])
        control = InterpreterControl(
            int.from_bytes(ranges["budget-mode"], "little"),
            int.from_bytes(ranges["break-requested"], "little"),
        )
        values = None
        if "variable-types" in ranges:
            if ranges["variable-types"] != package.variable_unsigned_bits:
                raise ValueError("Original event variable types differ from the source package")
            values = Variables(
                ranges["variables-low"] + ranges["variables-high"], ranges["variable-types"]
            )
        identity = (actor_index, record["ranges"][0]["pointer_value"])
        name, side = hook["name"].rsplit("-", 1)
        if side == "before":
            if pending is not None:
                raise ValueError("Nested or missing original event return")
            if name == "select":
                if record["gpr_u32"][6] != 15 or record["gpr_u32"][16] != actor_index:
                    raise ValueError(
                        "Original selector entry registers differ from recovered caller"
                    )
                effect = EventEffect(select_slot(actor, idle_entry), control, values)
                priorities = [slot.priority for slot in actor.slots]
                case = (name, min(priorities), priorities.count(min(priorities)))
            else:
                instruction = decode_instruction(package.bytecode, actor.pc)
                if name != f"op-{instruction.opcode:02x}":
                    raise ValueError("Original handler differs from the source event opcode")
                effect = execute(instruction, actor, control, values, idle_entry=idle_entry)
                if instruction.opcode == 0x26:
                    case = (
                        name,
                        actor.slots[actor.selected_slot].countdown,
                        effect.actor.slots[actor.selected_slot].countdown,
                        bool(instruction.operands[0] & 0x8000),
                        control.budget_mode,
                    )
                elif instruction.opcode in (0x35, 0x38, 0x39):
                    case = (name, instruction.operands[2], control.budget_mode)
                else:
                    case = (name, control.budget_mode)
                sites.add((actor_index, actor.pc, instruction.opcode))
            expected_actor = correlate_actor_scripts(ranges["actor"], effect.actor)
            pending = (name, identity, expected_actor, effect, case)
        elif side == "after":
            if pending is None or (name, identity) != pending[:2]:
                raise ValueError("Unpaired original event return or different actor")
            _, _, expected_actor, effect, case = pending
            if ranges["actor"] != expected_actor:
                differences = [
                    i
                    for i, (a, b) in enumerate(zip(expected_actor, ranges["actor"], strict=True))
                    if a != b
                ]
                raise ValueError(f"Original {name} actor bytes differ at offsets {differences}")
            if control != effect.control or values != effect.variables:
                raise ValueError(f"Original {name} interpreter control or variable bank differs")
            counts[name] += 1
            cases[case] += 1
            pending = None
        else:
            raise ValueError("Unexpected original hook side")
    if pending is not None or not counts:
        raise ValueError("Original event trace is empty or ends before a handler return")
    return {
        "records": total,
        "original_transitions": sum(counts.values()),
        "counts": dict(sorted(counts.items())),
        "actor_pc_opcode_sites": sorted(sites),
        "observed_cases": [{"case": case, "count": count} for case, count in sorted(cases.items())],
    }


def compare(sources: dict, map_id: int, capture: Path, group: str, start: int, end: int) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    spec = specification(sources, group, start, end)
    metadata = observation["instruction_trace"]
    if (
        observation["source_profile"] != sources["profile"]["id"]
        or observation["content_sha256"]
        != sources["profile"]["measurement"]["source"]["chd"]["sha256"]
        or not observation["scenario"]["complete"]
        or metadata["spec"] != spec
        or metadata["failed"]
        or metadata["budget_reached"]
        or metadata["unavailable_ranges"]
        or any(metadata["guard_mismatches_by_hook"].values())
    ):
        raise ValueError("Incomplete, unqualified or failed original event capture")
    spec_path, trace = capture / "instruction-trace-spec.json", capture / "instruction-trace.jsonl"
    if (
        file_sha(trace) != metadata["trace_sha256"]
        or file_sha(spec_path) != metadata["specification_sha256"]
        or json.loads(spec_path.read_text()) != spec
    ):
        raise ValueError("Original event trace or specification hash mismatch")
    overlay = decode_block(sources["overlay_packed"]).data
    ram = (capture / "final.ram").read_bytes()
    if len(ram) != 0x200000:
        raise ValueError("Original event comparison needs exactly 2 MiB RAM")
    windows = [(before, finish - before) for before, _, finish in HANDLERS.values()]
    windows += [
        (0x800A2030, 0x278),
        (0x800A2FE0, 0xD4),
        (0x800ACD7C, 0xA8),
        (0x8009CFBC, 0x44),
        (0x800AE2A0, 0x400),
    ]
    for address, size in windows:
        if (
            overlay[address - BASE : address - BASE + size]
            != ram[address - 0x80000000 : address - 0x80000000 + size]
        ):
            raise ValueError(
                f"Original handler/helper/table code differs from source at {address:#x}"
            )
    for opcode, (address, _, _) in HANDLERS.items():
        offset = 0x800AE2A0 - BASE + 4 * opcode
        if int.from_bytes(overlay[offset : offset + 4], "little") != address:
            raise ValueError("Original opcode dispatch target differs from recovered handler")
    package = event_package(field_components(sources["field_source"])[5].logical_data)
    with trace.open() as stream:
        compared = compare_records((json.loads(line) for line in stream), spec, package, map_id)
    if (
        compared["records"] != metadata["records"]
        or metadata["candidate_callbacks"] != metadata["records"]
    ):
        raise ValueError("Original event trace count differs from observation metadata")
    return {
        "schema_version": 1,
        "kind": "original_field_event_state_comparison",
        "source_profile": sources["profile"]["id"],
        "map": map_id,
        "group": group,
        "raw_track_sha256": sources["raw_sha256"],
        **compared,
        "trace": {"path": str(trace), "sha256": file_sha(trace)},
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in (
                "tools/analysis/events.py",
                "tools/analysis/event_state.py",
                "tools/analysis/verify_event_state.py",
            )
        },
        "result": "passed",
        "scope": "Exact 256-byte actor prefix and two interpreter controls; "
        "wait/variable group also compares all 2048 variable bytes and 128 type bytes. "
        "Only observed paths are validated; selection is one stage of the scheduler.",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "compare"])
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--group", choices=GROUPS, required=True)
    parser.add_argument("--start", type=int, default=0)
    parser.add_argument("--end", type=int, default=5000)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = load_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            result = specification(sources, args.group, args.start, args.end)
        else:
            if args.capture is None:
                raise ValueError("compare requires --capture")
            structure = verify(args.raw, args.capture / "final.ram", args.profile, args.map)
            result = compare(sources, args.map, args.capture, args.group, args.start, args.end)
            result["field_structure_validation"] = structure
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output.relative_to(ROOT)}")
    except (ValueError, KeyError, IndexError, StopIteration, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
