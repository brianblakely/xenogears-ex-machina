"""Strict RAM-only trace qualification for original-source analysis comparisons."""

from __future__ import annotations

import json
from pathlib import Path

from ..reference.memory_sampler import ram_pointer_offset
from .verify_collision_math import source_bytes
from .verify_field import file_sha

ROOT = Path(__file__).resolve().parents[2]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def qualified_ranges(row: dict, hook: dict) -> dict[str, bytes]:
    gpr = row["gpr_u32"]
    require(
        len(gpr) == 34 and all(type(v) is int and 0 <= v <= 0xFFFFFFFF for v in gpr),
        "invalid original registers",
    )
    require(row["pc"] == hook["pc"], "substituted original hook PC")
    offset = row["pc"] - 0x80000000 - hook["guard"]["offset"]
    code = bytes.fromhex(hook["guard"]["expected"])[offset : offset + 4]
    require(row["code"] == int.from_bytes(code, "little"), "changed original instruction")
    require(len(row["ranges"]) == len(hook["ranges"]), "changed original range count")
    payload, regions, pointers = {}, [], []
    for actual, expected in zip(row["ranges"], hook["ranges"], strict=True):
        name = expected["name"]
        require(
            name not in payload
            and actual["name"] == name
            and actual["size"] == expected["size"]
            and "unavailable" not in actual
            and actual.get("resolved_space", "ram") == "ram",
            "substituted or unavailable original range",
        )
        data = bytes.fromhex(actual["hex"])
        require(len(data) == expected["size"], "truncated original range")
        if "register" in expected:
            require(
                "pointer_offset" not in actual
                and "offset" not in actual
                and all(actual.get(k) == expected[k] for k in ("register", "relative_offset"))
                and actual.get("register_value") == gpr[expected["register"]],
                "substituted original register range",
            )
            base = ram_pointer_offset(gpr[expected["register"]])
            require(base is not None, "original register outside RAM")
            resolved = base + expected["relative_offset"]
        elif "pointer_offset" in expected:
            require(
                "register" not in actual
                and "offset" not in actual
                and all(
                    actual.get(k) == expected[k] for k in ("pointer_offset", "relative_offset")
                ),
                "substituted original pointer range",
            )
            base = ram_pointer_offset(actual["pointer_value"])
            require(base is not None, "original pointer outside RAM")
            resolved = base + expected["relative_offset"]
            pointers.append((expected["pointer_offset"], actual["pointer_value"]))
        else:
            require(
                "register" not in actual
                and "pointer_offset" not in actual
                and "relative_offset" not in actual,
                "substituted direct original range",
            )
            resolved = expected["offset"]
        require(
            actual["resolved_offset"] == resolved and 0 <= resolved <= 0x200000 - len(data),
            "original range resolved to a different address",
        )
        for start, other in regions:
            begin, end = max(start, resolved), min(start + len(other), resolved + len(data))
            require(
                begin >= end
                or other[begin - start : end - start] == data[begin - resolved : end - resolved],
                "overlapping original ranges disagree",
            )
        payload[name] = data
        regions.append((resolved, data))
    for pointer_offset, pointer in pointers:
        copies = [
            data[pointer_offset - start : pointer_offset - start + 4]
            for start, data in regions
            if start <= pointer_offset and pointer_offset + 4 <= start + len(data)
        ]
        require(
            copies and all(int.from_bytes(data, "little") == pointer for data in copies),
            "original pointer differs from captured pointer storage",
        )
    return payload


def capture_records(capture: Path, sources: dict, spec: dict, windows: tuple) -> tuple:
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
        "incomplete or unqualified original capture",
    )
    for key, path in [
        ("tool_sha256", "tools/reference/instruction_trace.py"),
        ("memory_helpers_sha256", "tools/reference/memory_sampler.py"),
        ("validation_helpers_sha256", "tools/reference/scenario_program.py"),
    ]:
        require(metadata[key] == file_sha(ROOT / path), "original trace collector changed")
    require(
        all(
            digest == file_sha(ROOT / "nix" / name)
            for name, digest in metadata["core_extension_inputs"].items()
        ),
        "original core extension sources changed",
    )
    require(
        file_sha(capture / "instruction-trace.jsonl") == metadata["trace_sha256"]
        and file_sha(capture / "instruction-trace-spec.json") == metadata["specification_sha256"]
        and json.loads((capture / "instruction-trace-spec.json").read_text()) == spec,
        "original trace or specification fingerprint differs",
    )
    ram = (capture / "final.ram").read_bytes()
    require(
        len(ram) == 0x200000
        and observation["captures"][-1]["frame"] == observation["frames"]
        and file_sha(capture / "final.ram") == observation["captures"][-1]["ram_sha256"],
        "incomplete or altered original final RAM",
    )
    for begin, end in windows:
        require(
            ram[begin - 0x80000000 : end - 0x80000000] == source_bytes(sources, begin, end),
            "original code window differs from source",
        )
    rows = [json.loads(line) for line in (capture / "instruction-trace.jsonl").open()]
    require(
        len(rows) == metadata["records"] == metadata["candidate_callbacks"]
        and 0 < len(rows) < spec["max_callbacks"],
        "incomplete original trace record count",
    )
    return ram, rows, observation


def ordered_rows(rows, spec: dict):
    hooks = {h["name"]: h for h in spec["hooks"]}
    previous = -1
    for index, row in enumerate(rows):
        run = row["frontend_run"]
        require(
            type(row["event"]) is int
            and row["event"] == index
            and type(run) is int
            and spec["start_frame"] <= run < spec["end_frame"]
            and run >= previous,
            "invalid original event ordering",
        )
        require(row["hook"] in hooks, "unknown original hook")
        previous = run
        yield row, qualified_ranges(row, hooks[row["hook"]])


def exact_payload(actual: dict, expected: dict, context: str) -> None:
    require(actual.keys() == expected.keys(), f"{context}: range set differs")
    for name in actual:
        require(actual[name] == expected[name], f"{context}: original {name} differs")
