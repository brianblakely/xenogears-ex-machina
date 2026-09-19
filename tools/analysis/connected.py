"""Run recovered C++ from one prepared boundary; compare only after execution.

No gameplay model, expected-state injection, automatic blessing or evidence
promotion lives here. See docs/connected-decomp.md for the current narrow scope.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OVERLAY = "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def load_json(data: bytes):
    def unique(items):
        result = {}
        for key, value in items:
            require(key not in result, f"Duplicate JSON key: {key}")
            result[key] = value
        return result

    return json.loads(data, object_pairs_hook=unique)


def checked_file(base: Path, record: dict, limit: int = 4 * 1024 * 1024) -> bytes:
    require(isinstance(record, dict) and set(record) == {"path", "sha256"}, "Expected path/hash")
    require(isinstance(record["path"], str), "Invalid file path")
    with (base / record["path"]).open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, "Case artifact exceeds the host allocation bound")
    require(hashlib.sha256(data).hexdigest() == record["sha256"], "Case artifact hash mismatch")
    return data


def integers(values, widths: list[tuple[int, int]], label: str) -> list[int]:
    require(isinstance(values, list) and len(values) == len(widths), f"Invalid {label} length")
    for value, (low, high) in zip(values, widths, strict=True):
        require(type(value) is int and low <= value <= high, f"Invalid {label} integer")
    return values


U8, U32, S32 = (0, 255), (0, 0xFFFFFFFF), (-0x80000000, 0x7FFFFFFF)


def prepare(case: dict, base: Path) -> bytes:
    require(
        isinstance(case, dict)
        and set(case) == {"provenance", "inputs", "instruction_limit", "expected"},
        "Unexpected or missing case fields",
    )
    provenance = case["provenance"]
    require(isinstance(provenance, dict), "Missing provenance")
    require(provenance.get("overlay_sha256") == OVERLAY, "Unsupported source overlay")
    kind = provenance.get("kind")
    require(kind in {"synthetic", "original-projection"}, "Missing input provenance kind")
    if kind == "original-projection":
        require(
            isinstance(provenance.get("profile"), str)
            and isinstance(provenance.get("finding"), str)
            and bool(provenance["profile"].strip())
            and bool(provenance["finding"].strip()),
            "Original projections need their existing source profile and finding",
        )
        checked_file(base, provenance["capture_manifest"])
    source = case["inputs"]
    require(
        isinstance(source, dict)
        and set(source)
        == {"events", "actors", "variables", "descriptor_flags", "control", "pass", "scheduler",
            "battle", "music_result", "battle_mode_source"},
        "Unexpected or missing prepared-state fields",
    )
    events = checked_file(base, source["events"], 0x200000)
    actors = checked_file(base, source["actors"])
    variables = checked_file(base, source["variables"], 2048)
    require(len(actors) % 0x138 == 0 and 0 < len(actors) // 0x138 <= 4096, "Invalid actors")
    require(len(variables) == 2048, "A complete variable bank is required")
    count = len(actors) // 0x138
    flags = integers(source["descriptor_flags"], [U32] * count, "descriptor flags")
    values = [len(events), *events, count]
    for i in range(count):
        values.extend(actors[i * 0x138 : (i + 1) * 0x138])
        values.append(flags[i])
    values.extend(struct.unpack("<1024H", variables))
    values.extend(integers(source["control"], [S32] * 8, "control"))
    values.extend(integers(source["pass"], [U32] * 2, "pass"))
    values.extend(integers(source["scheduler"], [S32, U8, S32, S32, S32], "scheduler"))
    for key, widths in (("battle", [U32] * 4 + [U8] * 3), ("music_result", [U32]),
                        ("battle_mode_source", [U8])):
        value = source[key]
        values.append(int(value is not None))
        if value is not None:
            values.extend(integers(value if key == "battle" else [value], widths, key))
    values.extend(integers([case["instruction_limit"]], [(1, 1000000)], "instruction limit"))
    return (" ".join(map(str, values)) + "\n").encode("ascii")


def first_difference(expected, actual, path: str = "$") -> str | None:
    if type(expected) is not type(actual):
        return path + ": type"
    if isinstance(expected, dict):
        if expected.keys() != actual.keys():
            return path + ": keys"
        for key in expected:
            difference = first_difference(expected[key], actual[key], f"{path}.{key}")
            if difference:
                return difference
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            return path + ": length"
        for index, (left, right) in enumerate(zip(expected, actual, strict=True)):
            difference = first_difference(left, right, f"{path}[{index}]")
            if difference:
                return difference
    elif expected != actual:
        return path + ": value"
    return None


def projection(result: dict) -> dict:
    stop = result["stop"]
    return {"state": result["state"], "location": [stop["actor"], stop["slot"], stop["pc"]]}


def next_action(dependency: str) -> dict:
    if dependency.startswith("input:"):
        return {"action": "qualify-missing-input", "detail": "Supply the actual prepared boundary "
                "value; do not substitute a guessed service result."}
    if dependency == "integration:field-update-tail":
        return {"action": "connect-next-stage", "source": "analysis/formats/field-lifecycle.md",
                "detail": "Connect the recovered field-update tail and its owned state. Do not "
                "repeat event passes in place of motion, contact, media or timing."}
    path = ROOT / "analysis/recovery.json"
    if dependency.startswith("instruction:") and path.exists():
        _, namespace, opcode = dependency.split(":")
        inventory = load_json(path.read_bytes())
        for table in inventory["event_dispatch_tables"]:
            if table["namespace"] == namespace:
                for row in table["rows"]:
                    if row["opcode"] == opcode:
                        return {"action": "integrate-existing-source" if
                                row["native_status"] == "authored_cpp_library" else "recover",
                                "inventory": row}
    return {
        "action": "investigate",
        "detail": "No status is inferred from a missing inventory row.",
    }


def run(case_path: Path, executable: Path) -> tuple[dict, int]:
    case_bytes = case_path.read_bytes()
    require(len(case_bytes) <= 4 * 1024 * 1024, "Case JSON is too large")
    case = load_json(case_bytes)
    wire = prepare(case, case_path.parent)
    expected = None
    if case["expected"] is not None:
        expected = load_json(checked_file(case_path.parent, case["expected"]))
        require(isinstance(expected, dict) and set(expected) == {"state", "location"},
                "Expected output must specify the complete semantic state and stop location")
    executable = executable.resolve(strict=True)
    binary_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    # Only prepared inputs enter C++; the independently supplied oracle does not.
    process = subprocess.run([str(executable)], input=wire, capture_output=True, timeout=60,
                             check=False)
    require(process.returncode in {1, 2, 4} and bool(process.stdout),
            "Reconstruction failed: " + process.stderr.decode("utf-8", errors="replace")[:1000])
    result = load_json(process.stdout)
    require(result["overlay_sha256"] == OVERLAY, "Driver source identity mismatch")
    comparison = {"status": "not-requested"}
    exit_code = process.returncode
    if expected is not None:
        difference = first_difference(expected, projection(result))
        comparison = {"status": "mismatch" if difference else "matched-projection",
                      "first_difference": difference}
        if difference:
            exit_code = 3
    report = {"case_sha256": hashlib.sha256(case_bytes).hexdigest(),
              "executable_sha256": binary_hash, "provenance": case["provenance"],
              "inputs": case["inputs"], "expected": case["expected"], "result": result,
              "comparison": comparison, "next": next_action(result["stop"]["dependency"]),
              "qualification": "Comparison only; original-source qualification and independent "
              "review remain in the existing evidence workflow. No Phase 1 gate is promoted."}
    return report, exit_code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output", type=Path, help="New private report file; never overwrite")
    args = parser.parse_args()
    try:
        report, code = run(args.case, args.executable)
        text = json.dumps(report, indent=2) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8") as stream:
                stream.write(text)
        else:
            print(text, end="")
        return code
    except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        print(f"connected reconstruction: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
