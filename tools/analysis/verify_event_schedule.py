"""Compare original interpreter batch decisions, without simulating handlers.

Unknown handlers remain opaque original execution. This validates the control
policy around them, never their gameplay effects or instruction semantics.
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
    from .event_schedule import DispatchGate, after_handler, begin_batch, dispatch_allowed
    from .event_state import InterpreterControl
    from .packed import decode_block
    from .verify_event_state import record_ranges
    from .verify_field import file_sha, load_sources, private_output, verify
else:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.event_schedule import (
        DispatchGate,
        after_handler,
        begin_batch,
        dispatch_allowed,
    )
    from tools.analysis.event_state import InterpreterControl
    from tools.analysis.packed import decode_block
    from tools.analysis.verify_event_state import record_ranges
    from tools.analysis.verify_field import file_sha, load_sources, private_output, verify
    from tools.reference.instruction_trace import validate_instruction_trace

BASE = 0x8006FAF0
WINDOWS = {
    "batch-entry": (0x800A1EC8, 0x800A1EF8),
    "loop-check": (0x800A1EF8, 0x800A1F38),
    "handler-return": (0x800A1F78, 0x800A2018),
    "return": (0x800A2018, 0x800A2030),
}


def specification(sources: dict, start: int, end: int) -> dict:
    overlay = decode_block(sources["overlay_packed"]).data
    ranges = [
        {"name": "budget-mode", "offset": 0xAFFEC, "size": 4},
        {"name": "break-requested", "offset": 0xB00C0, "size": 4},
        {"name": "budget", "offset": 0xAFC7C, "size": 4},
        {"name": "gate-enabled", "offset": 0xADB1C, "size": 4},
        {"name": "gate-values", "offset": 0xADBE0, "size": 16},
        {"name": "field", "offset": 0x4F34C, "size": 4},
        {"name": "actor-index", "offset": 0xAFD1C, "size": 4},
    ]
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"event-batch-policy-{start}-{end}",
            "source_profile": sources["profile"]["id"],
            "start_frame": start,
            "end_frame": end,
            "max_callbacks": 140000,
            "hooks": [
                {
                    "name": name,
                    "pc": begin,
                    "guard": {
                        "offset": begin - 0x80000000,
                        "expected": overlay[begin - BASE : finish - BASE].hex(),
                    },
                    "ranges": ranges,
                }
                for name, (begin, finish) in WINDOWS.items()
            ],
        }
    )


def compare_records(records, spec: dict, map_id: int) -> dict:
    hooks = {hook["name"]: hook for hook in spec["hooks"]}
    stack = []
    total, last_frame, maximum_depth = 0, -1, 0
    counts, reasons, entry_budgets, cases = Counter(), Counter(), Counter(), Counter()
    for record in records:
        if record["event"] != total or total >= spec["max_callbacks"]:
            raise ValueError("Noncontiguous or over-budget original batch trace")
        frame = record["frontend_run"]
        if not spec["start_frame"] <= frame < spec["end_frame"] or frame < last_frame:
            raise ValueError("Original batch trace has invalid frontend ordering")
        total, last_frame = total + 1, frame
        stage = record["hook"]
        ranges = record_ranges(record, hooks[stage])
        if int.from_bytes(ranges["field"], "little") != map_id:
            raise ValueError("Original batch trace enters a different field")
        control = InterpreterControl(
            int.from_bytes(ranges["budget-mode"], "little"),
            int.from_bytes(ranges["break-requested"], "little"),
        )
        budget = int.from_bytes(ranges["budget"], "little")
        gate = DispatchGate(
            int.from_bytes(ranges["gate-enabled"], "little"),
            tuple(int.from_bytes(ranges["gate-values"][i : i + 4], "little") for i in (0, 4, 12)),
        )
        if stage == "batch-entry":
            if stack and stack[-1]["next_stage"] != "handler-return":
                raise ValueError("Nested original batch outside a handler")
            requested = record["gpr_u32"][4]
            step = begin_batch(requested, control)
            stack.append(
                {"counter": step.counter, "next_stage": step.next_stage, "reason": step.reason}
            )
            entry_budgets[(requested, control.budget_mode)] += 1
            maximum_depth = max(maximum_depth, len(stack))
        else:
            if not stack or stage != stack[-1]["next_stage"]:
                raise ValueError(f"Unexpected original batch stage {stage}")
            active = stack[-1]
            if record["gpr_u32"][16] != active["counter"]:
                raise ValueError("Original batch loop counter differs from reconstruction")
            if stage != "handler-return" and ranges != active["expected_ranges"]:
                raise ValueError("Original batch controls or opaque captured globals differ")
            if stage == "return":
                reasons[active["reason"]] += 1
                counts["batches"] += 1
                stack.pop()
                continue
            if stage == "loop-check":
                if dispatch_allowed(active["counter"]):
                    active["next_stage"] = "handler-return"
                else:
                    active.update(
                        next_stage="return", reason="hard-dispatch-limit", expected_ranges=ranges
                    )
                counts["loop_checks"] += 1
                continue
            # Handler effects are supplied by original execution, explicitly
            # opaque to this policy comparison. No unknown opcode is executed
            # by the reconstruction or accepted as a no-op.
            step = after_handler(active["counter"], budget, control, gate)
            cases[
                (
                    control.budget_mode,
                    control.break_requested,
                    gate.enabled,
                    gate.stops_batch,
                    step.reason,
                )
            ] += 1
            counts["handler_returns"] += 1
        expected = dict(ranges)
        expected["budget"] = step.budget_u32.to_bytes(4, "little")
        expected["break-requested"] = step.control.break_requested.to_bytes(4, "little")
        stack[-1].update(
            counter=step.counter,
            next_stage=step.next_stage,
            reason=step.reason,
            expected_ranges=expected,
        )
    if stack or not counts["batches"]:
        raise ValueError("Original batch trace is empty or ends inside a batch")
    return {
        "records": total,
        "counts": dict(counts),
        "maximum_nesting": maximum_depth,
        "return_reasons": dict(sorted(reasons.items())),
        "entry_budgets": [
            {"budget_u32": budget, "budget_mode": mode, "count": count}
            for (budget, mode), count in sorted(entry_budgets.items())
        ],
        "observed_cases": [{"case": case, "count": count} for case, count in sorted(cases.items())],
    }


def compare(sources: dict, map_id: int, capture: Path, start: int, end: int) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    spec = specification(sources, start, end)
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
        raise ValueError("Incomplete, unqualified or failed original batch capture")
    spec_path, trace = capture / "instruction-trace-spec.json", capture / "instruction-trace.jsonl"
    if (
        file_sha(trace) != metadata["trace_sha256"]
        or file_sha(spec_path) != metadata["specification_sha256"]
        or json.loads(spec_path.read_text()) != spec
    ):
        raise ValueError("Original batch trace or specification hash mismatch")
    overlay = decode_block(sources["overlay_packed"]).data
    ram = (capture / "final.ram").read_bytes()
    if len(ram) != 0x200000:
        raise ValueError("Original batch comparison needs exactly 2 MiB RAM")
    if overlay[0x800A1EC8 - BASE : 0x800A2030 - BASE] != ram[0xA1EC8:0xA2030]:
        raise ValueError("Original interpreter instructions differ from source")
    with trace.open() as stream:
        result = compare_records((json.loads(line) for line in stream), spec, map_id)
    if (
        result["records"] != metadata["records"]
        or metadata["candidate_callbacks"] != metadata["records"]
    ):
        raise ValueError("Original batch trace count differs from observation metadata")
    return {
        "schema_version": 1,
        "kind": "original_field_interpreter_batch_comparison",
        "source_profile": sources["profile"]["id"],
        "map": map_id,
        **result,
        "raw_track_sha256": sources["raw_sha256"],
        "trace": {"path": str(trace), "sha256": file_sha(trace)},
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in (
                "tools/analysis/event_schedule.py",
                "tools/analysis/verify_event_schedule.py",
                "tools/analysis/verify_event_state.py",
            )
        },
        "result": "passed",
        "scope": "Exact original dispatch-loop decisions, counter and "
        "40 captured global bytes. Handlers run only in the original game and their effects "
        "are opaque inputs; this does not validate unknown handlers or player readiness.",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "compare"])
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--start", type=int, required=True)
    parser.add_argument("--end", type=int, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = load_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            result = specification(sources, args.start, args.end)
        else:
            if args.capture is None:
                raise ValueError("compare requires --capture")
            structure = verify(args.raw, args.capture / "final.ram", args.profile, args.map)
            result = compare(sources, args.map, args.capture, args.start, args.end)
            result["field_structure_validation"] = structure
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output.relative_to(ROOT)}")
    except (ValueError, KeyError, IndexError, StopIteration, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
