"""Bounded read-only RAM samples at external-emulator frontend boundaries.

No game addresses or instruction-trace claims belong in this module. A separate,
source-qualified specification supplies direct ranges or one RAM pointer lookup.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

if __package__:
    from .scenario_program import MEMORY_LIMIT, integer, keys
else:
    from scenario_program import MEMORY_LIMIT, integer, keys

MAX_SPEC_BYTES = 65536


def load_sampling(path: Path) -> tuple[MemorySampler, bytes]:
    with path.open("rb") as stream:
        data = stream.read(MAX_SPEC_BYTES + 1)
    if len(data) > MAX_SPEC_BYTES:
        raise ValueError("Memory-sampling specification exceeds 64 KiB")
    return MemorySampler(json.loads(data)), data


def validate_sampling(value: object) -> dict:
    spec = keys(
        value,
        {
            "schema_version",
            "name",
            "source_profile",
            "start_frame",
            "every_frames",
            "max_samples",
            "ranges",
        },
        set(),
        "Memory sampling",
    )
    if type(spec["schema_version"]) is not int or spec["schema_version"] != 1:
        raise ValueError("Unsupported memory-sampling schema")
    if not isinstance(spec["name"], str) or not re.fullmatch(
        r"[a-z0-9][a-z0-9_-]{0,79}", spec["name"]
    ):
        raise ValueError("Invalid memory-sampling name")
    if not isinstance(spec["source_profile"], str) or not spec["source_profile"]:
        raise ValueError("Memory sampling needs an exact source profile")
    integer(spec["start_frame"], "Sampling start", 0, 36000)
    integer(spec["every_frames"], "Sampling interval", 1, 36000)
    budget = integer(spec["max_samples"], "Sampling budget", 1, 36001)
    ranges = spec["ranges"]
    if not isinstance(ranges, list) or not 1 <= len(ranges) <= 32:
        raise ValueError("Memory sampling needs 1..32 ranges")
    names = set()
    total_size = 0
    for item in ranges:
        keys(
            item,
            {"name", "size"},
            {"offset", "pointer_offset", "relative_offset"},
            "Sampling range",
        )
        name = item["name"]
        if (
            not isinstance(name, str)
            or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,79}", name)
            or name in names
        ):
            raise ValueError("Sampling range names must be unique identifiers")
        names.add(name)
        size = integer(item["size"], "Sampling range size", 1, 1024)
        total_size += size
        if "offset" in item:
            if set(item) != {"name", "size", "offset"}:
                raise ValueError("Direct sampling cannot also follow a pointer")
            integer(item["offset"], "Sampling offset", 0, MEMORY_LIMIT - size)
        else:
            if set(item) != {"name", "size", "pointer_offset", "relative_offset"}:
                raise ValueError("Pointer sampling needs pointer_offset and relative_offset")
            integer(item["pointer_offset"], "Pointer offset", 0, MEMORY_LIMIT - 4)
            integer(
                item["relative_offset"],
                "Pointer-relative offset",
                1 - MEMORY_LIMIT,
                MEMORY_LIMIT - 1,
            )
    if total_size > 8192 or total_size * budget > 64 * 1024 * 1024:
        raise ValueError("Sampling exceeds the 8192-byte/sample or 64-MiB payload budget")
    if len(ranges) * budget > 1_000_000:
        raise ValueError("Sampling exceeds the one-million range-record budget")
    return spec


def ram_pointer_offset(pointer: int) -> int | None:
    """Accept only physical RAM and its first KSEG0/KSEG1 aliases, without wrapping."""
    for base in (0, 0x80000000, 0xA0000000):
        if base <= pointer < base + MEMORY_LIMIT:
            return pointer - base
    return None


def read_sampling_ranges(ranges: list[dict], memory: bytes) -> list[dict]:
    if not 0 < len(memory) <= MEMORY_LIMIT:
        raise ValueError("Memory sampling needs bounded PS1 system RAM")
    results = []
    for item in ranges:
        result = {"name": item["name"], "size": item["size"]}
        offset = item.get("offset")
        if offset is None:
            pointer_offset = item["pointer_offset"]
            result["pointer_offset"] = pointer_offset
            result["relative_offset"] = item["relative_offset"]
            if pointer_offset + 4 > len(memory):
                result["unavailable"] = "pointer_source_outside_exposed_ram"
            else:
                pointer = int.from_bytes(memory[pointer_offset : pointer_offset + 4], "little")
                result["pointer_value"] = pointer
                base = ram_pointer_offset(pointer)
                if pointer == 0:
                    result["unavailable"] = "null_pointer"
                elif base is None:
                    result["unavailable"] = "pointer_outside_system_ram"
                else:
                    offset = base + item["relative_offset"]
        if "unavailable" not in result:
            result["resolved_offset"] = offset
            if offset < 0 or offset + item["size"] > len(memory):
                result["unavailable"] = "range_outside_exposed_ram"
            else:
                result["hex"] = memory[offset : offset + item["size"]].hex()
        results.append(result)
    return results


class MemorySampler:
    def __init__(self, spec: dict):
        self.spec = validate_sampling(spec)
        self.samples = 0
        self.first_frame = None
        self.last_frame = None
        self.payload_bytes = 0
        self.unavailable = {item["name"]: 0 for item in self.spec["ranges"]}

    def due(self, frame: int) -> bool:
        integer(frame, "Sampling boundary", 0, 36000)
        return (
            self.samples < self.spec["max_samples"]
            and frame >= self.spec["start_frame"]
            and (frame - self.spec["start_frame"]) % self.spec["every_frames"] == 0
        )

    def sample(self, frame: int, memory: bytes) -> dict | None:
        if not self.due(frame):
            return None
        if self.last_frame is not None and frame <= self.last_frame:
            raise ValueError("Memory sampling boundaries must increase")
        ranges = read_sampling_ranges(self.spec["ranges"], memory)
        for result in ranges:
            self.payload_bytes += len(result.get("hex", "")) // 2
            if "unavailable" in result:
                self.unavailable[result["name"]] += 1
        self.samples += 1
        if self.first_frame is None:
            self.first_frame = frame
        self.last_frame = frame
        return {"frame": frame, "ranges": ranges}

    def status(self) -> dict:
        return {
            "samples": self.samples,
            "first_frame": self.first_frame,
            "last_frame": self.last_frame,
            "budget_reached": self.samples == self.spec["max_samples"],
            "payload_bytes": self.payload_bytes,
            "unavailable_by_range": self.unavailable.copy(),
        }
