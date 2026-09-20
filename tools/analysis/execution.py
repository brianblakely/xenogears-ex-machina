"""Run the shared C++ reconstruction; compare separate, independent observations.

This module owns prepared cases, host limits, transport and reporting. It does
not implement gameplay. Original case qualification is performed by return_case;
an arbitrary JSON file with original identities is not original evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import subprocess
import tempfile
from pathlib import Path

from tools.repository.recovery import entries as recovery_entries

ROOT = Path(__file__).resolve().parents[2]
ENTRIES = ("field_return", "field_return_data", "event_pass", "event_batch")
EXE = "dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119"
OVERLAY = "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc"
INPUT_KEYS = {
    "snapshot",
    "event_actor_count",
    "actors",
    "environment",
    "field_sprite_base",
    "party_resources",
    "allocations",
    "resources",
    "frame_list",
    "trig",
    "widths",
    "event_component",
    "variables",
    "event_control",
    "battle_request",
    "music_gate",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_digest(value: dict) -> str:
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def uint(value, maximum: int = 0xFFFFFFFF) -> int:
    require(type(value) is int and 0 <= value <= maximum, f"Expected integer in 0..{maximum}")
    return value


def word(value) -> bytes:
    return struct.pack("<I", uint(value))


def blob(value: str, size: int | None = None, maximum: int = 0x200000) -> bytes:
    require(
        isinstance(value, str) and len(value) <= maximum * 2, "Invalid/oversized hex byte range"
    )
    data = bytes.fromhex(value)
    require(len(data) * 2 == len(value), "Byte ranges must use contiguous hex")
    require(size is None or len(data) == size, f"Expected {size} bytes, got {len(data)}")
    return word(len(data)) + data


def words(values, count: int) -> bytes:
    require(isinstance(values, list) and len(values) == count, f"Expected {count} words")
    return b"".join(word(v) for v in values)


def resources(values, *, allocated: bool = False) -> bytes:
    require(isinstance(values, list) and len(values) <= 65536, "Invalid resource list")
    result = word(len(values))
    for row in values:
        keys = {"address", "bytes", "mode"} if allocated else {"address", "bytes"}
        require(isinstance(row, dict) and set(row) == keys, "Invalid resource fields")
        if allocated:
            result += word(row["mode"])
        result += word(row["address"]) + blob(row["bytes"])
    return result


def encode_case(
    case: dict, *, budget: int, repeats: int = 1, actor: int = 0, batch_limit: int = 8
) -> bytes:
    """Serialize inputs only. Expected checkpoints never reach the C++ process."""
    require(isinstance(case, dict) and case.get("schema_version") == 1, "Unsupported case schema")
    require(
        set(case) <= {"schema_version", "entry", "source", "input", "provenance"},
        "Unknown case fields (expectations must be in a separate file)",
    )
    require(case.get("entry") in ENTRIES, "Unsupported selected entry")
    source = case.get("source")
    require(isinstance(source, dict), "Missing source identity")
    if source.get("kind") != "synthetic":
        require(
            source.get("executable") == EXE and source.get("overlay") == OVERLAY,
            "Unsupported executable/overlay identity",
        )
        require(
            source.get("profile") == "na-slus-00664-39c547a9afc6", "Unsupported original profile"
        )
    state = case.get("input")
    require(isinstance(state, dict) and set(state) <= INPUT_KEYS, "Unknown or missing input fields")
    actors = state.get("actors")
    require(isinstance(actors, list) and len(actors) <= 255, "Missing or oversized actor storage")
    require(state.get("event_actor_count", len(actors)) == len(actors), "Event actor count differs")
    environment = state.get("environment", [0] * 16)
    result = b"XEMRUN01" + word(ENTRIES.index(case["entry"]))
    result += word(uint(budget, 1000000)) + word(uint(repeats, 10000)) + word(uint(actor, 255))
    require(
        type(batch_limit) is int and -(1 << 31) <= batch_limit < (1 << 31), "Invalid batch limit"
    )
    result += word(batch_limit & 0xFFFFFFFF) + blob(state.get("snapshot", "")) + word(len(actors))
    for item in actors:
        require(
            isinstance(item, dict) and set(item) == {"actor", "descriptor"}, "Invalid actor fields"
        )
        result += blob(item["actor"], 312) + blob(item["descriptor"], 92)
    result += words(environment, 16) + word(state.get("field_sprite_base", 0))
    # B2268 is reconstructed from snapshot globals, never supplied by a case.
    result += word(0)
    party = state.get("party_resources", [])
    result += word(len(party)) + words(party, len(party))
    result += resources(state.get("allocations", []), allocated=True)
    result += resources(state.get("resources", [])) + resources(state.get("frame_list", []))
    result += blob(state.get("trig", ""), maximum=0x4000)
    result += blob(state.get("widths", ""), maximum=256)
    result += blob(state.get("event_component", ""))
    result += blob(state.get("variables", "00" * 2048), 2048)
    result += words(state.get("event_control", [0] * 8), 8)
    result += words(state.get("battle_request", [0] * 8), 8)
    result += word(state.get("music_gate", 0))
    require(len(result) <= 64 * 1024 * 1024, "Case exceeds 64 MiB transport budget")
    return result


def first_difference(actual, expected, path: str = "") -> dict | None:
    """Exact named projection: every expected field is required; extra diagnostics are allowed."""
    if isinstance(expected, dict):
        if not isinstance(actual, dict):
            return {"path": path, "expected": expected, "actual": actual}
        for key, value in expected.items():
            if key not in actual:
                return {"path": path + "." + key, "expected": value, "actual": "<missing>"}
            difference = first_difference(actual[key], value, path + "." + key)
            if difference:
                return difference
        return None
    if type(actual) is not type(expected) or actual != expected:
        if isinstance(actual, list) and isinstance(expected, list) and len(actual) == len(expected):
            for i, (left, right) in enumerate(zip(actual, expected, strict=True)):
                difference = first_difference(left, right, f"{path}[{i}]")
                if difference:
                    return difference
        return {"path": path, "expected": expected, "actual": actual}
    return None


def compare(report: dict, expected: dict) -> dict:
    require(expected.get("schema_version") == 1, "Unsupported comparison schema")
    require(expected.get("entry") == report.get("entry"), "Comparison entry differs")
    require(expected.get("source") == report.get("source"), "Comparison original identity differs")
    if "case_sha256" in expected:
        require(
            expected["case_sha256"] == report.get("case_sha256"),
            "Comparison starting input differs",
        )
    checkpoints = expected.get("checkpoints")
    require(isinstance(checkpoints, list), "Missing comparison checkpoints")
    actual = report.get("checkpoints", [])
    for index, checkpoint in enumerate(checkpoints):
        difference = first_difference(
            actual[index] if index < len(actual) else None, checkpoint, f"checkpoints[{index}]"
        )
        if difference:
            return {
                "status": "behavioral_divergence",
                "matched_checkpoints": index,
                "first_difference": difference,
            }
    if len(actual) != len(checkpoints):
        return {
            "status": "behavioral_divergence",
            "matched_checkpoints": len(checkpoints),
            "first_difference": {
                "path": "checkpoint_count",
                "expected": len(checkpoints),
                "actual": len(actual),
            },
        }
    stop = expected.get("stop")
    if stop:
        observed = {
            "actor_index": report.get("location", {}).get("actor"),
            "operation": report.get("location", {}).get("operation"),
            "opcode": report.get("opcode"),
            "sprite_bytecode_pc": report.get("sprite_bytecode_pc"),
            "status": report.get("execution_status", report["status"]),
        }
        projected = {k: v for k, v in stop.items() if k in observed}
        if stop.get("operation") == "ordinary_sprite_command":
            projected["status"] = "dependency_needs_recovery"
        difference = first_difference(observed, projected, "stop")
        if difference:
            return {
                "status": "behavioral_divergence",
                "matched_checkpoints": len(checkpoints),
                "first_difference": difference,
            }
    if "partial_actor" in expected:
        index = uint(expected["partial_actor"]["index"], 254)
        partial = report.get("partial_state") or {}
        sprites = partial.get("sprites", [])
        observed = sprites[index] if index < len(sprites) else None
        projected = {k: v for k, v in expected["partial_actor"].items() if k != "index"}
        difference = first_difference(observed, projected, f"partial_state.sprites[{index}]")
        if difference:
            return {
                "status": "behavioral_divergence",
                "matched_checkpoints": len(checkpoints),
                "first_difference": difference,
            }
    return {
        "status": "matched",
        "matched_checkpoints": len(checkpoints),
        "tolerance": "exact; only the explicitly listed state projection",
    }


def enrich(report: dict) -> None:
    location = report.get("location")
    if location is not None:
        machine = location.get("machine_address", 0)
        location["code_identity"] = (
            EXE
            if 0x80010000 <= machine < 0x8006FAF0
            else OVERLAY
            if 0x8006FAF0 <= machine < 0x800C0000
            else None
        )
        location["address_space"] = "PS1 KSEG0 machine code" if machine else None
    dependency = report.get("dependency")
    if not dependency:
        return
    inventory = json.loads((ROOT / "analysis/recovery.json").read_text())
    row = recovery_entries(inventory).get(dependency)
    if row:
        report["recovery"] = row
        report["recovery"]["inventory"] = "analysis/recovery.json"
        if row.get("table_value"):
            report["location"]["handler_machine_address"] = int(row["table_value"], 16)
        if (
            row.get("native_status") == "authored_cpp_library"
            and report["status"] == "dependency_needs_recovery"
        ):
            report["status"] = "dependency_not_connected"
        for table in [*inventory["event_dispatch_tables"], inventory["sprite_command_dispatch"]]:
            for original in table["rows"]:
                if dependency == f"instruction:{table['namespace']}:{original['opcode']}":
                    report["recovery"].update(
                        {
                            k: original[k]
                            for k in ("cpp_reconstruction", "next_experiment")
                            if k in original
                        }
                    )


def run(
    case: dict,
    runner: Path,
    *,
    budget: int = 10000,
    timeout: float = 30,
    repeats: int = 1,
    actor: int = 0,
    batch_limit: int = 8,
) -> dict:
    require(math.isfinite(timeout) and timeout > 0, "Timeout must be positive and finite")
    data = encode_case(case, budget=budget, repeats=repeats, actor=actor, batch_limit=batch_limit)
    with tempfile.TemporaryDirectory(prefix="xem-execution-") as directory:
        path = Path(directory) / "input.bin"
        path.write_bytes(data)
        try:
            result = subprocess.run(
                [str(runner.resolve()), str(path)],
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired:
            return {
                "entry": case["entry"],
                "source": case["source"],
                "status": "host_timeout",
                "reason": "Wall-clock watchdog expired; subprocess terminated",
                "continuation": "restart_from_immutable_input",
                "checkpoints": [],
                "partial_state": None,
                "partial_state_limit": "A killed process cannot publish its final state",
            }
    try:
        report = json.loads(result.stdout)
    except (ValueError, TypeError) as error:
        raise RuntimeError(
            f"Reconstruction process did not produce a report (exit {result.returncode}): "
            f"{result.stderr}"
        ) from error
    require(isinstance(report, dict), "Invalid reconstruction report")
    if result.returncode not in (0, 1):
        report.update(
            status="reconstruction_error", reason=result.stderr, process_exit=result.returncode
        )
    report.update(
        source=case["source"],
        case_sha256=canonical_digest(case),
        runner_sha256=digest(runner),
        limits={"operations": budget, "timeout_seconds": timeout},
        invocation={"repeats": repeats, "actor": actor, "batch_limit": batch_limit},
        evidence_scope=case.get("provenance", {"kind": "synthetic; no original-game claim"}),
    )
    enrich(report)
    return report


def summary(report: dict) -> dict:
    return {
        key: report[key]
        for key in (
            "entry",
            "status",
            "execution_status",
            "reason",
            "location",
            "dependency",
            "opcode",
            "sprite_bytecode_pc",
            "operations",
            "completed_entries",
            "entry_result",
            "comparison",
            "recovery",
            "report",
        )
        if key in report
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    execute = sub.add_parser("run")
    execute.add_argument("--case", type=Path, required=True)
    execute.add_argument("--runner", type=Path, default=ROOT / "build/debug/xem-analysis-runner")
    execute.add_argument("--expected", type=Path)
    execute.add_argument("--report", type=Path, required=True)
    execute.add_argument("--max-operations", type=int, default=10000)
    execute.add_argument("--timeout", type=float, default=30)
    execute.add_argument("--repeat", type=int, default=1)
    execute.add_argument("--actor", type=int, default=0)
    execute.add_argument("--batch-limit", type=int, default=8)
    comparison = sub.add_parser("compare")
    comparison.add_argument("--report", type=Path, required=True)
    comparison.add_argument("--expected", type=Path, required=True)
    args = parser.parse_args()
    report = {"status": "invalid_input", "entry": "unknown"}
    save_report = False
    try:
        if args.command == "compare":
            report = json.loads(args.report.read_text())
            result = compare(report, json.loads(args.expected.read_text()))
            print(json.dumps(result, indent=2))
            raise SystemExit(0 if result["status"] == "matched" else 1)
        require(
            not args.report.exists(), "Report path must be new; preserve prior execution evidence"
        )
        save_report = True
        report = run(
            json.loads(args.case.read_text()),
            args.runner,
            budget=args.max_operations,
            timeout=args.timeout,
            repeats=args.repeat,
            actor=args.actor,
            batch_limit=args.batch_limit,
        )
        if args.expected:
            result = compare(report, json.loads(args.expected.read_text()))
            report["comparison"] = result
            if result["status"] != "matched":
                report["execution_status"] = report["status"]
                report["status"] = "behavioral_divergence"
    except (OSError, ValueError, KeyError, TypeError) as error:
        if "partial_state" in report:
            report["execution_status"] = report["status"]
        report.update(status="invalid_input", reason=str(error))
    except RuntimeError as error:
        report.update(status="reconstruction_error", reason=str(error))
    if save_report:
        try:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            with args.report.open("x") as stream:
                json.dump(report, stream, indent=2)
                stream.write("\n")
            report["report"] = str(args.report)
        except OSError as error:
            report.update(
                status="invalid_input", reason=f"Cannot preserve execution report: {error}"
            )
    print(json.dumps(summary(report), indent=2))
    raise SystemExit(0 if report["status"] == "completed_boundary" else 1)


if __name__ == "__main__":
    main()
