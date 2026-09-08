"""Bounded, guarded instruction-address observations in the external interpreter.

The optional Nix core extension passes registers and RAM read-only to this host
collector. Game addresses and code guards are supplied by a source-qualified
specification. There is no game interpretation or emulated state mutation here.
"""

from __future__ import annotations

import ctypes as ct
import hashlib
import json
from pathlib import Path

if __package__:
    from .memory_sampler import (
        MAX_SPEC_BYTES,
        ram_pointer_offset,
        read_sampling_ranges,
        validate_sampling,
    )
    from .scenario_program import MEMORY_LIMIT, hex_bytes, integer, keys
else:
    from memory_sampler import (
        MAX_SPEC_BYTES,
        ram_pointer_offset,
        read_sampling_ranges,
        validate_sampling,
    )
    from scenario_program import MEMORY_LIMIT, hex_bytes, integer, keys

Callback = ct.CFUNCTYPE(
    None,
    *([ct.c_uint32] * 6),
    ct.POINTER(ct.c_uint32),
    ct.POINTER(ct.c_uint8),
)


def load_instruction_trace(path: Path) -> tuple[dict, bytes]:
    with path.open("rb") as stream:
        data = stream.read(MAX_SPEC_BYTES + 1)
    if len(data) > MAX_SPEC_BYTES:
        raise ValueError("Instruction-trace specification exceeds 64 KiB")
    return validate_instruction_trace(json.loads(data)), data


def validate_instruction_trace(value: object) -> dict:
    spec = keys(
        value,
        {
            "schema_version",
            "name",
            "source_profile",
            "start_frame",
            "end_frame",
            "max_callbacks",
            "hooks",
        },
        set(),
        "Instruction trace",
    )
    start = integer(spec["start_frame"], "Trace start", 0, 35999)
    integer(spec["end_frame"], "Trace end", start + 1, 36000)
    budget = integer(spec["max_callbacks"], "Trace callback budget", 1, 1_000_000)
    hooks = spec["hooks"]
    if not isinstance(hooks, list) or not 1 <= len(hooks) <= 16:
        raise ValueError("Instruction trace needs 1..16 hooks")
    names, pcs = set(), set()
    for hook in hooks:
        keys(hook, {"name", "pc", "guard", "ranges"}, set(), "Trace hook")
        pc = integer(hook["pc"], "Hook PC", 0, 0xFFFFFFFF)
        offset = ram_pointer_offset(pc)
        if offset is None or pc % 4 or pc in pcs:
            raise ValueError("Hook PCs must be distinct aligned system-RAM addresses")
        pcs.add(pc)
        guard = keys(hook["guard"], {"offset", "expected"}, set(), "Trace code guard")
        expected = hex_bytes(guard["expected"], "Trace expected code")
        begin = integer(guard["offset"], "Trace guard offset", 0, MEMORY_LIMIT - len(expected))
        if not begin <= offset or offset + 4 > begin + len(expected):
            raise ValueError("Trace code guard must contain the entire watched instruction")
        ranges = hook["ranges"]
        if not isinstance(ranges, list):
            raise ValueError("Trace ranges must be a bounded list")
        ordinary = []
        for item in ranges:
            if isinstance(item, dict) and "register" in item:
                keys(item, {"name", "size", "register", "relative_offset"}, set(), "Register range")
                integer(item["register"], "Register index", 0, 33)
                integer(
                    item["relative_offset"],
                    "Register-relative offset",
                    1 - MEMORY_LIMIT,
                    MEMORY_LIMIT - 1,
                )
                ordinary.append({"name": item["name"], "size": item["size"], "offset": 0})
            else:
                ordinary.append(item)
        # Share the strict name, profile, range, size and schema checks with RAM sampling.
        validate_sampling(
            {
                "schema_version": spec["schema_version"],
                "name": spec["name"],
                "source_profile": spec["source_profile"],
                "start_frame": 0,
                "every_frames": 1,
                "max_samples": 1,
                "ranges": ordinary,
            }
        )
        validate_sampling(
            {
                "schema_version": 1,
                "name": hook["name"],
                "source_profile": spec["source_profile"],
                "start_frame": 0,
                "every_frames": 1,
                "max_samples": 1,
                "ranges": [{"name": "name-validation", "offset": 0, "size": 1}],
            }
        )
        if hook["name"] in names:
            raise ValueError("Trace hook names must be unique")
        names.add(hook["name"])
        payload = 34 * 4 + 32 + sum(item["size"] for item in ranges)
        if payload * budget > 64 * 1024 * 1024 or len(ranges) * budget > 1_000_000:
            raise ValueError("Instruction trace exceeds payload or range-record budget")
    return spec


def guarded_record(hook: dict, pc: int, code: int, gpr: list[int], memory: bytes) -> dict | None:
    guard = hook["guard"]
    expected = bytes.fromhex(guard["expected"])
    begin = guard["offset"]
    pc_offset = ram_pointer_offset(pc)
    if (
        pc != hook["pc"]
        or memory[begin : begin + len(expected)] != expected
        or code != int.from_bytes(expected[pc_offset - begin : pc_offset - begin + 4], "little")
    ):
        return None
    ranges = []
    for item in hook["ranges"]:
        if "register" not in item:
            ranges.extend(read_sampling_ranges([item], memory))
            continue
        pointer = gpr[item["register"]]
        offset = ram_pointer_offset(pointer)
        result = {**item, "register_value": pointer}
        if pointer == 0:
            result["unavailable"] = "null_pointer"
        elif offset is None:
            result["unavailable"] = "pointer_outside_system_ram"
        else:
            offset += item["relative_offset"]
            result["resolved_offset"] = offset
            if offset < 0 or offset + item["size"] > len(memory):
                result["unavailable"] = "range_outside_exposed_ram"
            else:
                result["hex"] = memory[offset : offset + item["size"]].hex()
        ranges.append(result)
    return {"hook": hook["name"], "gpr_u32": list(gpr), "ranges": ranges}


class InstructionTrace:
    def __init__(self, core, spec: dict, output: Path, errors: list[str]):
        self.spec = validate_instruction_trace(spec)
        self.core, self.errors = core, errors
        prototypes = {
            "retro_xem_trace_version": (ct.c_uint32, []),
            "retro_xem_trace_configure": (
                ct.c_int,
                [ct.POINTER(ct.c_uint32), ct.c_uint32, ct.c_uint32, Callback],
            ),
            "retro_xem_trace_enable": (None, [ct.c_uint32]),
            "retro_xem_trace_count": (ct.c_uint32, []),
        }
        for name, (restype, argtypes) in prototypes.items():
            function = getattr(core, name, None)
            if function is None:
                raise ValueError("Instruction tracing requires the pinned observation-trace shell")
            function.restype, function.argtypes = restype, argtypes
        if core.retro_xem_trace_version() != 1:
            raise ValueError("Unsupported external instruction-trace API")
        self.frame = 0
        self.records = 0
        self.guard_mismatches = {hook["name"]: 0 for hook in spec["hooks"]}
        self.first_guard_mismatch = {}
        self.unavailable = 0
        self.digest = hashlib.sha256()
        self.failed = False
        self.callback = Callback(self.accept)
        addresses = (ct.c_uint32 * len(spec["hooks"]))(*(hook["pc"] for hook in spec["hooks"]))
        if (
            core.retro_xem_trace_configure(
                addresses, len(addresses), spec["max_callbacks"], self.callback
            )
            != 1
        ):
            raise ValueError("External core rejected the bounded trace configuration")
        self.stream = output.open("xb")

    def start_run(self, frame: int):
        self.frame = frame
        self.core.retro_xem_trace_enable(
            not self.failed and self.spec["start_frame"] <= frame < self.spec["end_frame"]
        )

    def accept(self, index, pc, code, cycle, subcycle, path, registers, ram):
        try:
            if not ram or not registers or not 0 <= index < len(self.spec["hooks"]):
                raise ValueError("External core supplied invalid trace arguments")
            memory = (
                memoryview(ct.cast(ram, ct.POINTER(ct.c_uint8 * MEMORY_LIMIT)).contents)
                .cast("B")
                .toreadonly()
            )
            gpr = [registers[index] for index in range(34)]
            hook = self.spec["hooks"][index]
            record = guarded_record(hook, pc, code, gpr, memory)
            if record is None:
                self.guard_mismatches[hook["name"]] += 1
                self.first_guard_mismatch.setdefault(
                    hook["name"], {"frontend_run": self.frame, "pc": pc, "code": code}
                )
                return
            record.update(
                {
                    "event": self.records,
                    "frontend_run": self.frame,
                    "pc": pc,
                    "code": code,
                    "cycle_u32": cycle,
                    "subcycle_u32": subcycle,
                    "dispatch_path": path,
                }
            )
            self.unavailable += sum("unavailable" in item for item in record["ranges"])
            data = (json.dumps(record, separators=(",", ":")) + "\n").encode()
            self.stream.write(data)
            self.digest.update(data)
            self.records += 1
        except Exception as error:
            self.failed = True
            self.core.retro_xem_trace_enable(0)
            self.errors.append(f"Instruction trace: {error}")

    def finish(self) -> dict:
        self.core.retro_xem_trace_enable(0)
        self.stream.close()
        candidates = self.core.retro_xem_trace_count()
        return {
            "records": self.records,
            "candidate_callbacks": candidates,
            "budget_reached": candidates == self.spec["max_callbacks"],
            "guard_mismatches_by_hook": self.guard_mismatches,
            "first_guard_mismatch": self.first_guard_mismatch,
            "unavailable_ranges": self.unavailable,
            "failed": self.failed,
            "trace_sha256": self.digest.hexdigest(),
        }
