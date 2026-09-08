"""Compare reconstructed conditional branches with original instructions and state.

Only opcode 0x02 is exercised by these hooks. This does not validate the rest of
the event VM, dialogue, encounter setup, scheduler or complete slice route.
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
    from .arithmetic import signed32
    from .events import Variables, branch_next_pc, branch_operands, decode_instruction
    from .field import event_package, field_components
    from .packed import decode_block
    from .verify_field import file_sha, load_sources, private_output, verify
else:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.arithmetic import signed32
    from tools.analysis.events import Variables, branch_next_pc, branch_operands, decode_instruction
    from tools.analysis.field import event_package, field_components
    from tools.analysis.packed import decode_block
    from tools.analysis.verify_field import file_sha, load_sources, private_output, verify
    from tools.reference.instruction_trace import validate_instruction_trace


def specification(sources: dict, start: int, end: int) -> dict:
    overlay = decode_block(sources["overlay_packed"]).data
    code = overlay[0x800A1BD0 - 0x8006FAF0 : 0x800A1E74 - 0x8006FAF0]
    common = [
        {"name": "actor", "pointer_offset": 0xB0078, "relative_offset": 0, "size": 256},
        {"name": "field", "offset": 0x4F34C, "size": 4},
        {"name": "actor-index", "offset": 0xAFD1C, "size": 4},
    ]
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"conditional-branches-{start}-{end}",
            "source_profile": sources["profile"]["id"],
            "start_frame": start,
            "end_frame": end,
            "max_callbacks": 25000,
            "hooks": [
                {
                    "name": "branch-before",
                    "pc": 0x800A1BD0,
                    "guard": {"offset": 0xA1BD0, "expected": code[:96].hex()},
                    "ranges": common
                    + [
                        {"name": "variables-low", "offset": 0xC3A68, "size": 1024},
                        {"name": "variables-high", "offset": 0xC3E68, "size": 1024},
                        {
                            "name": "variable-types",
                            "pointer_offset": 0xADBF8,
                            "relative_offset": 0,
                            "size": 128,
                        },
                    ],
                },
                {
                    "name": "branch-after",
                    "pc": 0x800A1E5C,
                    "guard": {"offset": 0xA1E20, "expected": code[0x250:].hex()},
                    "ranges": common,
                },
            ],
        }
    )


def compare(sources: dict, map_id: int, capture: Path, start: int, end: int) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    expected_spec = specification(sources, start, end)
    metadata = observation["instruction_trace"]
    if (
        observation["source_profile"] != sources["profile"]["id"]
        or observation["content_sha256"]
        != sources["profile"]["measurement"]["source"]["chd"]["sha256"]
        or not observation["scenario"]["complete"]
        or metadata["spec"] != expected_spec
        or metadata["failed"]
        or metadata["budget_reached"]
        or metadata["unavailable_ranges"]
        or any(metadata["guard_mismatches_by_hook"].values())
    ):
        raise ValueError("Incomplete, unqualified or failed original branch capture")
    spec_path, trace = capture / "instruction-trace-spec.json", capture / "instruction-trace.jsonl"
    if (
        file_sha(trace) != metadata["trace_sha256"]
        or file_sha(spec_path) != metadata["specification_sha256"]
        or json.loads(spec_path.read_text()) != expected_spec
    ):
        raise ValueError("Original branch trace or specification hash mismatch")
    package = event_package(field_components(sources["field_source"])[5].logical_data)
    overlay = decode_block(sources["overlay_packed"]).data
    original_ram = (capture / "final.ram").read_bytes()
    for address, size in (
        (0x800A1BD0, 0x2A4),
        (0x800A2FE0, 0x94),
        (0x800ACD7C, 0x70),
        (0x8006FD58, 44),
        (0x800AE2A0, 12),
    ):
        offset = address - 0x8006FAF0
        if (
            overlay[offset : offset + size]
            != original_ram[address - 0x80000000 : address - 0x80000000 + size]
        ):
            raise ValueError(f"Original branch/helper code differs from source at {address:#x}")
    # The caller checks the same immutable package against original final RAM.
    pending = None
    counts, sites = Counter(), set()
    records = 0
    for line in trace.open():
        record = json.loads(line)
        if record["event"] != records or records >= 25000:
            raise ValueError("Noncontiguous or over-budget original branch trace")
        records += 1
        ranges = {item["name"]: bytes.fromhex(item["hex"]) for item in record["ranges"]}
        if int.from_bytes(ranges["field"], "little") != map_id:
            raise ValueError("Trace enters a different field")
        actor = int.from_bytes(ranges["actor-index"], "little")
        if actor >= len(package.entries):
            raise ValueError("Original actor index exceeds the recovered event table")
        state = ranges["actor"]
        pc = int.from_bytes(state[0xCC:0xCE], "little")
        pointer = next(
            item["pointer_value"] for item in record["ranges"] if item["name"] == "actor"
        )
        if record["hook"] == "branch-before":
            if pending is not None or ranges["variable-types"] != package.variable_unsigned_bits:
                raise ValueError("Nested/incomplete branch or modified original variable types")
            instruction = decode_instruction(package.bytecode, pc)
            if instruction.opcode != 2:
                raise ValueError("Original branch handler does not correspond to source opcode 2")
            values = Variables(
                ranges["variables-low"] + ranges["variables-high"], ranges["variable-types"]
            )
            pending = (actor, pointer, instruction, values)
        elif record["hook"] == "branch-after":
            if pending is None or (actor, pointer) != pending[:2]:
                raise ValueError("Unpaired branch return or different actor state")
            instruction, values = pending[2:]
            operands = branch_operands(instruction, values)
            original_operands = (signed32(record["gpr_u32"][17]), signed32(record["gpr_u32"][16]))
            if operands != original_operands or branch_next_pc(instruction, values) != pc:
                raise ValueError(
                    f"Original branch operands or successor differ at +0x{instruction.pc:04x}"
                )
            mode, comparison = instruction.operands[2:]
            counts[(mode, comparison, pc == instruction.successors[0])] += 1
            sites.add((actor, instruction.pc))
            pending = None
        else:
            raise ValueError("Unexpected instruction hook in branch trace")
    if pending is not None or not counts or records != metadata["records"]:
        raise ValueError("Original branch trace is empty or incomplete")
    return {
        "schema_version": 1,
        "kind": "original_conditional_branch_comparison",
        "source_profile": sources["profile"]["id"],
        "map": map_id,
        "raw_track_sha256": sources["raw_sha256"],
        "original_branches": sum(counts.values()),
        "actor_pc_sites": sorted(sites),
        "observed_cases": [
            {"operand_mode": mode, "comparison": comparison, "fallthrough": fall, "count": count}
            for (mode, comparison, fall), count in sorted(counts.items())
        ],
        "trace": {"path": str(trace), "sha256": file_sha(trace)},
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in ("tools/analysis/events.py", "tools/analysis/verify_events.py")
        },
        "result": "passed",
        "scope": "Exact operands and successor PCs for observed opcode 0x02 calls only.",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "compare"])
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--start", type=int, required=True)
    parser.add_argument("--end", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = load_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            result = specification(sources, args.start, args.end)
        else:
            if not args.capture:
                raise ValueError("compare requires --capture")
            structure = verify(args.raw, args.capture / "final.ram", args.profile, args.map)
            result = compare(sources, args.map, args.capture, args.start, args.end)
            result["field_structure_validation"] = structure
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output.relative_to(ROOT)}")
    except (ValueError, KeyError, StopIteration, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
