"""Bounded, fingerprinted execution steps for original-game scenario testing.

This module has no game addresses. Recovered adapters supply those separately.
The caller exposes the external emulator's RAM and executes one frontend frame
between calls to tick. Every mutation is guarded and recorded.
"""

from __future__ import annotations

import re
from collections.abc import Callable

BUTTONS = {
    "cross",
    "square",
    "select",
    "start",
    "up",
    "down",
    "left",
    "right",
    "circle",
    "triangle",
    "l1",
    "r1",
    "l2",
    "r2",
}
MEMORY_LIMIT = 2 * 1024 * 1024


def integer(value: object, label: str, minimum: int, maximum: int) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{label} must be an integer in {minimum}..{maximum}")
    return value


def keys(value: object, required: set[str], optional: set[str], label: str) -> dict:
    if not isinstance(value, dict) or not required <= value.keys():
        raise ValueError(f"{label} requires {sorted(required)}")
    if value.keys() - required - optional:
        raise ValueError(f"{label} has unknown fields")
    return value


def hex_bytes(value: object, label: str) -> bytes:
    if not isinstance(value, str) or not re.fullmatch(r"(?:[0-9a-f]{2}){1,256}", value):
        raise ValueError(f"{label} must contain 1..256 bytes of lowercase hexadecimal")
    return bytes.fromhex(value)


def validate_program(value: object) -> dict:
    program = keys(
        value, {"schema_version", "name", "source_profile", "kind", "steps"}, set(), "Program"
    )
    if type(program["schema_version"]) is not int or program["schema_version"] != 1:
        raise ValueError("Unsupported scenario-program schema")
    if not isinstance(program["name"], str) or not re.fullmatch(
        r"[a-z0-9][a-z0-9_-]{0,79}", program["name"]
    ):
        raise ValueError("Invalid scenario-program name")
    if not isinstance(program["source_profile"], str) or not program["source_profile"]:
        raise ValueError("Scenario program needs an exact source profile")
    if program["kind"] not in ("analysis_probe", "recovered_scenario"):
        raise ValueError("Unknown scenario-program kind")
    steps = program["steps"]
    if not isinstance(steps, list) or not 1 <= len(steps) <= 256:
        raise ValueError("Scenario program needs 1..256 ordered steps")
    names = set()
    for step in steps:
        keys(
            step,
            {"name", "timeout_frames"},
            {"when", "stable_frames", "writes", "run_frames", "buttons", "capture", "evidence"},
            "Step",
        )
        name = step["name"]
        if (
            not isinstance(name, str)
            or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,79}", name)
            or name in names
        ):
            raise ValueError("Step names must be unique identifiers")
        names.add(name)
        integer(step["timeout_frames"], "Step timeout", 1, 216000)
        integer(step.get("stable_frames", 1), "Stable frames", 1, 36000)
        integer(step.get("run_frames", 0), "Run frames", 0, 216000)
        if "capture" in step and type(step["capture"]) is not bool:
            raise ValueError("Capture must be boolean")
        buttons = step.get("buttons", [])
        if (
            not isinstance(buttons, list)
            or any(not isinstance(button, str) or button not in BUTTONS for button in buttons)
            or len(buttons) != len(set(buttons))
        ):
            raise ValueError("Invalid or duplicate scenario buttons")
        if buttons and not step.get("run_frames"):
            raise ValueError("Held buttons require run_frames")
        evidence = step.get("evidence", [])
        if not isinstance(evidence, list) or any(
            not isinstance(item, str) or not item for item in evidence
        ):
            raise ValueError("Evidence must be nonempty reference strings")
        conditions = step.get("when", [])
        if not isinstance(conditions, list) or len(conditions) > 256:
            raise ValueError("Step conditions must be a bounded list")
        for condition in conditions:
            keys(condition, {"offset", "width", "operator", "value"}, {"mask"}, "Condition")
            width = condition["width"]
            if type(width) is not int or width not in (1, 2, 4):
                raise ValueError("Condition width must be 1, 2, or 4")
            integer(condition["offset"], "Condition offset", 0, MEMORY_LIMIT - width)
            integer(condition["value"], "Condition value", 0, (1 << (width * 8)) - 1)
            integer(
                condition.get("mask", (1 << (width * 8)) - 1),
                "Condition mask",
                0,
                (1 << (width * 8)) - 1,
            )
            if condition["operator"] not in ("eq", "ne", "lt", "le", "gt", "ge"):
                raise ValueError("Unknown condition operator")
        writes = step.get("writes", [])
        if not isinstance(writes, list) or len(writes) > 256:
            raise ValueError("Step writes must be a bounded list")
        occupied = set()
        for write in writes:
            keys(write, {"offset", "expected", "value", "reason"}, set(), "Write")
            expected = hex_bytes(write["expected"], "Expected bytes")
            replacement = hex_bytes(write["value"], "Replacement bytes")
            if len(expected) != len(replacement):
                raise ValueError("Write must preserve byte length")
            offset = integer(write["offset"], "Write offset", 0, MEMORY_LIMIT - len(expected))
            positions = set(range(offset, offset + len(expected)))
            if occupied & positions:
                raise ValueError("Overlapping writes in a single step")
            occupied |= positions
            if not isinstance(write["reason"], str) or not write["reason"].strip():
                raise ValueError("Every write needs a reason")
        if program["kind"] == "recovered_scenario" and writes and not evidence:
            raise ValueError("Recovered scenario mutations need evidence references")
    return program


def read_conditions(memory: bytes, conditions: list[dict]) -> tuple[bool, list[dict]]:
    results = []
    for condition in conditions:
        offset, width = condition["offset"], condition["width"]
        if offset + width > len(memory):
            raise ValueError("Condition extends past actual emulator RAM")
        actual = int.from_bytes(memory[offset : offset + width], "little")
        masked = actual & condition.get("mask", (1 << (8 * width)) - 1)
        wanted = condition["value"]
        matched = {
            "eq": masked == wanted,
            "ne": masked != wanted,
            "lt": masked < wanted,
            "le": masked <= wanted,
            "gt": masked > wanted,
            "ge": masked >= wanted,
        }[condition["operator"]]
        results.append({**condition, "actual": actual, "masked_actual": masked, "matched": matched})
    return all(item["matched"] for item in results), results


class ScenarioProgram:
    def __init__(self, program: dict):
        self.program = validate_program(program)
        self.index = 0
        self.entered_frame = 0
        self.started_frame = None
        self.stable_count = 0
        self.last_frame = -1
        self.events = []
        self.observed = []

    @property
    def complete(self) -> bool:
        return self.index == len(self.program["steps"])

    @property
    def buttons(self) -> list[str]:
        if self.complete or self.started_frame is None:
            return []
        return self.program["steps"][self.index].get("buttons", [])

    def tick(
        self, frame: int, memory: bytes, write_memory: Callable[[int, bytes], None]
    ) -> list[str]:
        """Advance at a boundary before the next frontend frame; return capture labels."""
        if frame != self.last_frame + 1:
            raise ValueError("Scenario ticks must start at zero and advance one frame at a time")
        self.last_frame = frame
        captures = []
        while not self.complete:
            step = self.program["steps"][self.index]
            if self.started_frame is None:
                matched, self.observed = read_conditions(memory, step.get("when", []))
                self.stable_count = self.stable_count + 1 if matched else 0
                if self.stable_count < step.get("stable_frames", 1):
                    if frame - self.entered_frame >= step["timeout_frames"]:
                        raise TimeoutError(
                            f"Scenario step {step['name']} timed out: {self.observed}"
                        )
                    return captures
                changes = []
                # Check every fingerprint before making any change.
                for write in step.get("writes", []):
                    offset = write["offset"]
                    expected = bytes.fromhex(write["expected"])
                    actual = memory[offset : offset + len(expected)]
                    if actual != expected:
                        raise ValueError(
                            f"Scenario fingerprint mismatch at {offset:#x}: "
                            f"{actual.hex()} != {expected.hex()}"
                        )
                    changes.append({**write, "before": actual.hex()})
                for change in changes:
                    write_memory(change["offset"], bytes.fromhex(change["value"]))
                if changes:
                    updated = bytearray(memory)
                    for change in changes:
                        data = bytes.fromhex(change["value"])
                        updated[change["offset"] : change["offset"] + len(data)] = data
                    memory = bytes(updated)
                self.started_frame = frame
                self.events.append(
                    {
                        "step": step["name"],
                        "started_frame": frame,
                        "conditions": self.observed,
                        "writes": changes,
                    }
                )
            if frame - self.started_frame < step.get("run_frames", 0):
                return captures
            self.events[-1]["completed_frame"] = frame
            if step.get("capture", False):
                captures.append(step["name"])
            self.index += 1
            self.entered_frame = frame
            self.started_frame = None
            self.stable_count = 0
        return captures
