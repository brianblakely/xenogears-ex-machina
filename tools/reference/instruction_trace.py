"""Bounded, guarded instruction-address observations in the external interpreter.

The optional Nix core extension passes registers and RAM read-only to this host
collector. Game addresses and code guards are supplied by a source-qualified
specification. There is no game interpretation or emulated state mutation here.
"""

from __future__ import annotations

import ctypes as ct
import hashlib
import json
import re
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
ScratchpadCallback = ct.CFUNCTYPE(
    None,
    *([ct.c_uint32] * 6),
    ct.POINTER(ct.c_uint32),
    ct.POINTER(ct.c_uint8),
    ct.POINTER(ct.c_uint8),
)


def scratchpad_pointer_offset(pointer: int) -> int | None:
    """The pinned core's three scratchpad aliases, bounded to the first 1 KiB.

    Adjacent hardware registers are deliberately unavailable: observing a bus
    read could have side effects, unlike reading the scratchpad backing bytes.
    """
    for base in (0x1F800000, 0x9F800000, 0xBF800000):
        if base <= pointer < base + 1024:
            return pointer - base
    return None


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
        keys(hook, {"name", "pc", "guard", "ranges"}, {"digests"}, "Trace hook")
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
        if "digests" in hook:
            validate_digests(hook["digests"], budget)
    return spec


def validate_digests(value: object, budget: int) -> None:
    """Bound hashing work independently from emitted payload bytes.

    Fixed-length regions follow a register or one RAM pointer. Variable-length
    regions use an ordered register pair. No game-specific address expressions
    or callback writes are accepted.
    """
    if not isinstance(value, list) or not 1 <= len(value) <= 8:
        raise ValueError("Instruction digests need 1..8 regions")
    names, total = set(), 0
    for item in value:
        keys(
            item,
            {"name", "max_bytes"},
            {"register", "pointer_offset", "size", "end_register"},
            "Instruction digest",
        )
        name = item["name"]
        if (
            not isinstance(name, str)
            or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,79}", name)
            or name in names
        ):
            raise ValueError("Instruction digest names must be unique identifiers")
        names.add(name)
        limit = integer(item["max_bytes"], "Digest byte bound", 1, MEMORY_LIMIT)
        total += limit
        if ("register" in item) == ("pointer_offset" in item):
            raise ValueError("Instruction digest needs exactly one start pointer")
        if "register" in item:
            integer(item["register"], "Digest register", 0, 33)
        else:
            integer(item["pointer_offset"], "Digest pointer offset", 0, MEMORY_LIMIT - 4)
        if ("size" in item) == ("end_register" in item):
            raise ValueError("Instruction digest needs exactly one length source")
        if "size" in item:
            integer(item["size"], "Digest size", 0, limit)
        else:
            integer(item["end_register"], "Digest end register", 0, 33)
    if total * budget > 64 * 1024 * 1024:
        raise ValueError("Instruction digests exceed the 64-MiB hashing budget")


def read_digests(spec: list[dict], gpr: list[int], memory: bytes) -> list[dict]:
    result = []
    for item in spec:
        row = {"name": item["name"]}
        result.append(row)
        if "register" in item:
            pointer = gpr[item["register"]]
        else:
            start = item["pointer_offset"]
            if start + 4 > len(memory):
                row["unavailable"] = "pointer_source_outside_exposed_ram"
                continue
            pointer = int.from_bytes(memory[start : start + 4], "little")
        row["pointer_value"] = pointer
        offset = ram_pointer_offset(pointer)
        size = item.get("size")
        if size is None:
            end = gpr[item["end_register"]]
            row["end_pointer_value"] = end
            # Subtract original addresses: mixing different RAM aliases fails
            # the bound, rather than accidentally accepting a wrapped region.
            size = end - pointer
        row["size"] = size
        if pointer == 0:
            row["unavailable"] = "null_pointer"
        elif offset is None:
            row["unavailable"] = "pointer_outside_system_ram"
        elif size < 0 or size > item["max_bytes"]:
            row["unavailable"] = "length_outside_digest_bound"
        elif offset + size > len(memory):
            row["unavailable"] = "range_outside_exposed_ram"
        else:
            row["resolved_offset"] = offset
            row["sha256"] = hashlib.sha256(memory[offset : offset + size]).hexdigest()
    return result


def guarded_record(
    hook: dict, pc: int, code: int, gpr: list[int], memory: bytes, scratchpad: bytes | None = None
) -> dict | None:
    if scratchpad is not None and len(scratchpad) != 1024:
        raise ValueError("Instruction scratchpad view must contain exactly 1 KiB")
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
        buffer = memory
        if offset is None and scratchpad is not None:
            offset = scratchpad_pointer_offset(pointer)
            if offset is not None:
                result["resolved_space"] = "scratchpad"
                buffer = scratchpad
        if pointer == 0:
            result["unavailable"] = "null_pointer"
        elif offset is None:
            result["unavailable"] = "pointer_outside_system_ram"
        else:
            offset += item["relative_offset"]
            result["resolved_offset"] = offset
            if offset < 0 or offset + item["size"] > len(buffer):
                result["unavailable"] = (
                    "range_outside_scratchpad"
                    if result.get("resolved_space") == "scratchpad"
                    else "range_outside_exposed_ram"
                )
            else:
                result["hex"] = buffer[offset : offset + item["size"]].hex()
        ranges.append(result)
    result = {"hook": hook["name"], "gpr_u32": list(gpr), "ranges": ranges}
    if "digests" in hook:
        result["digests"] = read_digests(hook["digests"], gpr, memory)
    return result


class InstructionTrace:
    def __init__(self, core, spec: dict, output: Path, errors: list[str]):
        self.spec = validate_instruction_trace(spec)
        self.core, self.errors = core, errors
        prototypes = {
            "retro_xem_trace_version": (ct.c_uint32, []),
            "retro_xem_trace_enable": (None, [ct.c_uint32]),
            "retro_xem_trace_count": (ct.c_uint32, []),
        }
        for name, (restype, argtypes) in prototypes.items():
            function = getattr(core, name, None)
            if function is None:
                raise ValueError("Instruction tracing requires the pinned observation-trace shell")
            function.restype, function.argtypes = restype, argtypes
        self.api_version = core.retro_xem_trace_version()
        if self.api_version not in (1, 2):
            raise ValueError("Unsupported external instruction-trace API")
        callback_type = Callback if self.api_version == 1 else ScratchpadCallback
        configure = getattr(core, "retro_xem_trace_configure", None)
        if configure is None:
            raise ValueError("External core is missing trace configuration")
        configure.restype = ct.c_int
        configure.argtypes = [ct.POINTER(ct.c_uint32), ct.c_uint32, ct.c_uint32, callback_type]
        self.frame = 0
        self.records = 0
        self.guard_mismatches = {hook["name"]: 0 for hook in spec["hooks"]}
        self.first_guard_mismatch = {}
        self.unavailable = 0
        self.digest_bytes = 0
        self.digest = hashlib.sha256()
        self.failed = False
        self.callback = callback_type(self.accept)
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

    def accept(self, index, pc, code, cycle, subcycle, path, registers, ram, scratchpad=None):
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
            scratch = None
            if self.api_version == 2:
                if not scratchpad:
                    raise ValueError("External core supplied a null scratchpad")
                scratch = (
                    memoryview(ct.cast(scratchpad, ct.POINTER(ct.c_uint8 * 1024)).contents)
                    .cast("B")
                    .toreadonly()
                )
            record = guarded_record(hook, pc, code, gpr, memory, scratch)
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
            self.unavailable += sum("unavailable" in item for item in record.get("digests", []))
            self.digest_bytes += sum(
                item["size"] for item in record.get("digests", []) if "sha256" in item
            )
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
            "digest_bytes": self.digest_bytes,
            "records": self.records,
            "candidate_callbacks": candidates,
            "budget_reached": candidates == self.spec["max_callbacks"],
            "guard_mismatches_by_hook": self.guard_mismatches,
            "first_guard_mismatch": self.first_guard_mismatch,
            "unavailable_ranges": self.unavailable,
            "failed": self.failed,
            "trace_sha256": self.digest.hexdigest(),
        }
