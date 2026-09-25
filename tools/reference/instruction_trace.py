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
import struct
import zlib
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

ScratchpadCallback = ct.CFUNCTYPE(
    None,
    *([ct.c_uint32] * 6),
    ct.POINTER(ct.c_uint32),
    ct.POINTER(ct.c_uint8),
    ct.POINTER(ct.c_uint8),
    ct.POINTER(ct.c_uint32),
    ct.POINTER(ct.c_uint32),
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


MAX_SNAPSHOTS = 8192
# Hooks the trace extension watches (nix/reference-trace.h XEM_TRACE_HOOKS)
# and the bound on one hook's record payload times the callback budget.
MAX_HOOKS = 256
MAX_PAYLOAD_BYTES = 1024 * 1024 * 1024
# The 4 KiB hardware I/O register page (1f801000) stored with each snapshot.
IO_PAGE = 0x1000
# A snapshot image is RAM, then the 1 KiB scratchpad, then the I/O page.
SNAPSHOT_BYTES = MEMORY_LIMIT + 1024 + IO_PAGE
# Snapshot file: magic, then page size, image size and keyframe interval
# (little-endian u32), then one length-prefixed zlib chunk per snapshot. A
# keyframe holds the complete image; every other chunk holds only the pages
# that differ from the previous snapshot (u32 count, u32 page indexes, pages).
SNAPSHOT_MAGIC = b"XEMSNAP2"
SNAPSHOT_PAGE = 256
SNAPSHOT_KEY_INTERVAL = 256
SNAPSHOT_BLOCK = 0x10000  # Coarse comparison before scanning pages
SNAPSHOT_HEADER = SNAPSHOT_MAGIC + struct.pack(
    "<III", SNAPSHOT_PAGE, SNAPSHOT_BYTES, SNAPSHOT_KEY_INTERVAL
)


def snapshot_path(trace: Path) -> Path:
    """Complete RAM, scratchpad and I/O images live beside the JSONL trace."""
    return trace.with_name(trace.stem + "-snapshots.bin")


class SnapshotWriter:
    """Append snapshots as keyframes and page deltas; returns each sequence."""

    def __init__(self, stream):
        self.stream = stream
        self.digest = hashlib.sha256()
        self.previous = bytearray(SNAPSHOT_BYTES)
        self.count = 0
        self.emit(SNAPSHOT_HEADER)

    def emit(self, data: bytes) -> None:
        self.stream.write(data)
        self.digest.update(data)

    def write(self, image: bytes) -> int:
        if len(image) != SNAPSHOT_BYTES:
            raise ValueError("Snapshot image has the wrong size")
        if self.count % SNAPSHOT_KEY_INTERVAL == 0:
            payload = image
        else:
            previous = self.previous
            pages = [
                page
                for block in range(0, SNAPSHOT_BYTES, SNAPSHOT_BLOCK)
                if image[block : block + SNAPSHOT_BLOCK] != previous[block : block + SNAPSHOT_BLOCK]
                for page in range(block, min(block + SNAPSHOT_BLOCK, SNAPSHOT_BYTES), SNAPSHOT_PAGE)
                if image[page : page + SNAPSHOT_PAGE] != previous[page : page + SNAPSHOT_PAGE]
            ]
            payload = b"".join(
                [
                    struct.pack(
                        f"<I{len(pages)}I", len(pages), *(p // SNAPSHOT_PAGE for p in pages)
                    ),
                    *(image[p : p + SNAPSHOT_PAGE] for p in pages),
                ]
            )
        data = zlib.compress(payload, 6)
        self.emit(struct.pack("<I", len(data)) + data)
        self.previous[:] = image
        self.count += 1
        return self.count - 1


class SnapshotReader:
    """Reconstruct recorded snapshots; reading in capture order replays forward."""

    def __init__(self, path: Path):
        data = path.read_bytes()
        if not data.startswith(SNAPSHOT_HEADER):
            raise ValueError("Snapshot file has an unsupported header")
        self.chunks = []
        at = len(SNAPSHOT_HEADER)
        while at < len(data):
            if at + 4 > len(data):
                raise ValueError("Snapshot file ends inside a chunk length")
            (size,) = struct.unpack_from("<I", data, at)
            if at + 4 + size > len(data):
                raise ValueError("Snapshot file ends inside a chunk")
            self.chunks.append(memoryview(data)[at + 4 : at + 4 + size])
            at += 4 + size
        self.image = bytearray(SNAPSHOT_BYTES)
        self.current = -1

    def apply(self, sequence: int) -> None:
        # Invalidate first: a failure below leaves no partially applied image.
        self.current = -1
        stream = zlib.decompressobj()
        payload = stream.decompress(self.chunks[sequence])
        if not stream.eof or stream.unused_data:
            raise ValueError("Snapshot chunk is not exactly one compressed stream")
        if sequence % SNAPSHOT_KEY_INTERVAL == 0:
            if len(payload) != SNAPSHOT_BYTES:
                raise ValueError("Snapshot keyframe has the wrong size")
            self.image[:] = payload
        else:
            if len(payload) < 4:
                raise ValueError("Snapshot delta has the wrong size")
            (count,) = struct.unpack_from("<I", payload)
            at = 4 + 4 * count
            if len(payload) != at + count * SNAPSHOT_PAGE:
                raise ValueError("Snapshot delta has the wrong size")
            indexes = struct.unpack_from(f"<{count}I", payload, 4)
            if any(index >= SNAPSHOT_BYTES // SNAPSHOT_PAGE for index in indexes):
                raise ValueError("Snapshot delta page is outside the image")
            for index in indexes:
                start = index * SNAPSHOT_PAGE
                self.image[start : start + SNAPSHOT_PAGE] = payload[at : at + SNAPSHOT_PAGE]
                at += SNAPSHOT_PAGE
        self.current = sequence

    def read(self, record: dict) -> tuple[bytes, bytes, bytes]:
        """Return the exact RAM, scratchpad and I/O page bytes of one record."""
        item = record["snapshot"]
        sequence = item["sequence"]
        if not 0 <= sequence < len(self.chunks):
            raise ValueError("Snapshot sequence is outside the file")
        key = sequence - sequence % SNAPSHOT_KEY_INTERVAL
        start = self.current + 1 if key <= self.current <= sequence else key
        for step in range(start, sequence + 1):
            self.apply(step)
        ram = bytes(self.image[:MEMORY_LIMIT])
        scratchpad = bytes(self.image[MEMORY_LIMIT : MEMORY_LIMIT + 1024])
        io = bytes(self.image[MEMORY_LIMIT + 1024 :])
        if (
            hashlib.sha256(ram).hexdigest() != item["ram_sha256"]
            or hashlib.sha256(scratchpad).hexdigest() != item["scratchpad_sha256"]
            or hashlib.sha256(io).hexdigest() != item["io_sha256"]
        ):
            raise ValueError("Instruction snapshot does not match its recorded digests")
        return ram, scratchpad, io


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
        {"max_snapshots"},
        "Instruction trace",
    )
    start = integer(spec["start_frame"], "Trace start", 0, 35999)
    integer(spec["end_frame"], "Trace end", start + 1, 36000)
    budget = integer(spec["max_callbacks"], "Trace callback budget", 1, 1_000_000)
    hooks = spec["hooks"]
    if not isinstance(hooks, list) or not 1 <= len(hooks) <= MAX_HOOKS:
        raise ValueError(f"Instruction trace needs 1..{MAX_HOOKS} hooks")
    names, pcs = set(), set()
    for hook in hooks:
        keys(hook, {"name", "pc", "guard", "ranges"}, {"digests", "snapshot"}, "Trace hook")
        if hook.get("snapshot", True) is not True:
            raise ValueError("Trace hook snapshot must be true when present")
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
        if payload * budget > MAX_PAYLOAD_BYTES or len(ranges) * budget > 1_000_000:
            raise ValueError("Instruction trace exceeds payload or range-record budget")
        if "digests" in hook:
            validate_digests(hook["digests"], budget)
    snapshots = any(hook.get("snapshot") for hook in hooks)
    if snapshots != ("max_snapshots" in spec):
        raise ValueError("Snapshot hooks and max_snapshots must be declared together")
    if snapshots:
        integer(spec["max_snapshots"], "Snapshot budget", 1, MAX_SNAPSHOTS)
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
            "retro_xem_trace_enable": (None, [ct.c_uint32]),
            "retro_xem_trace_count": (ct.c_uint32, []),
        }
        for name, (restype, argtypes) in prototypes.items():
            function = getattr(core, name, None)
            if function is None:
                raise ValueError("Instruction tracing requires the pinned observation-trace shell")
            function.restype, function.argtypes = restype, argtypes
        configure = getattr(core, "retro_xem_trace_configure", None)
        if configure is None:
            raise ValueError("External core is missing trace configuration")
        configure.restype = ct.c_int
        configure.argtypes = [ct.POINTER(ct.c_uint32), ct.c_uint32, ct.c_uint32, ScratchpadCallback]
        self.frame = 0
        self.records = 0
        self.guard_mismatches = {hook["name"]: 0 for hook in spec["hooks"]}
        self.first_guard_mismatch = {}
        self.unavailable = 0
        self.digest_bytes = 0
        self.digest = hashlib.sha256()
        self.failed = False
        self.callback = ScratchpadCallback(self.accept)
        addresses = (ct.c_uint32 * len(spec["hooks"]))(*(hook["pc"] for hook in spec["hooks"]))
        if (
            core.retro_xem_trace_configure(
                addresses, len(addresses), spec["max_callbacks"], self.callback
            )
            != 1
        ):
            raise ValueError("External core rejected the bounded trace configuration")
        self.snapshots = None
        self.stream = output.open("xb")
        if "max_snapshots" in self.spec:
            self.snapshots = SnapshotWriter(snapshot_path(output).open("xb"))

    def start_run(self, frame: int):
        self.frame = frame
        self.core.retro_xem_trace_enable(
            not self.failed and self.spec["start_frame"] <= frame < self.spec["end_frame"]
        )

    def accept(
        self, index, pc, code, cycle, subcycle, path, registers, ram, scratchpad, cop2, load_delay
    ):
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
            if not scratchpad:
                raise ValueError("External core supplied a null scratchpad")
            if not cop2:
                raise ValueError("External core supplied null GTE registers")
            if not load_delay:
                raise ValueError("External core supplied null load-delay state")
            # The core's hardware backing: scratchpad first, the I/O registers at +1000.
            hardware = (
                memoryview(
                    ct.cast(scratchpad, ct.POINTER(ct.c_uint8 * (0x1000 + IO_PAGE))).contents
                )
                .cast("B")
                .toreadonly()
            )
            scratch = hardware[:1024]
            record = guarded_record(hook, pc, code, gpr, memory, scratch)
            if record is None:
                self.guard_mismatches[hook["name"]] += 1
                self.first_guard_mismatch.setdefault(
                    hook["name"], {"frontend_run": self.frame, "pc": pc, "code": code}
                )
                return
            record.update(
                {
                    # Raw GTE storage: 32 data words, then 32 control words.
                    "cop2_u32": [cop2[i] for i in range(64)],
                    # Interpreter load-delay slots: loads not yet in gpr_u32.
                    "load_delay": {
                        "select": load_delay[0],
                        "registers": [load_delay[1], load_delay[2]],
                        "values": [load_delay[3], load_delay[4]],
                    },
                    "event": self.records,
                    "frontend_run": self.frame,
                    "pc": pc,
                    "code": code,
                    "cycle_u32": cycle,
                    "subcycle_u32": subcycle,
                    "dispatch_path": path,
                }
            )
            if hook.get("snapshot"):
                if self.snapshots.count >= self.spec["max_snapshots"]:
                    raise ValueError("Instruction snapshot budget exhausted")
                image = b"".join((memory, scratch, hardware[0x1000:]))
                record["snapshot"] = {
                    "sequence": self.snapshots.write(image),
                    "ram_sha256": hashlib.sha256(memory).hexdigest(),
                    "scratchpad_sha256": hashlib.sha256(scratch).hexdigest(),
                    "io_sha256": hashlib.sha256(hardware[0x1000:]).hexdigest(),
                }
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
        snapshots = {}
        if self.snapshots:
            self.snapshots.stream.close()
            snapshots = {
                "snapshots": self.snapshots.count,
                "snapshot_file": snapshot_path(Path(self.stream.name)).name,
                "snapshot_file_sha256": self.snapshots.digest.hexdigest(),
            }
        return {
            **snapshots,
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


# Coverage per declared frame window, one entry per aligned RAM word: the
# executed bitmap, the changed bitmap (a later dispatch fetched a different
# instruction word), then the first and last fetched words (little-endian u32).
COVERAGE_WORDS = MEMORY_LIMIT // 4
COVERAGE_BITMAP = COVERAGE_WORDS // 8
COVERAGE_BYTES = 2 * COVERAGE_BITMAP + 8 * COVERAGE_WORDS
COVERAGE_MAGIC = b"XEMCOV01"
MAX_COVERAGE_WINDOWS = 1024
MAX_COVERAGE_RANGES = 16
IDENTIFIER = r"[a-z0-9][a-z0-9_-]{0,79}"


def load_coverage(path: Path) -> tuple[dict, bytes]:
    with path.open("rb") as stream:
        data = stream.read(MAX_SPEC_BYTES + 1)
    if len(data) > MAX_SPEC_BYTES:
        raise ValueError("Coverage specification exceeds 64 KiB")
    return validate_coverage(json.loads(data)), data


def validate_coverage(value: object) -> dict:
    """Frame windows and the RAM ranges hashed at each window's start and end.

    Windows are ordered, disjoint and use frontend frame indices (start
    inclusive, end exclusive). Ranges identify loaded code; they are hashed,
    never interpreted.
    """
    spec = keys(
        value,
        {"schema_version", "name", "source_profile", "windows", "code_ranges"},
        set(),
        "Coverage",
    )
    if type(spec["schema_version"]) is not int or spec["schema_version"] != 1:
        raise ValueError("Unsupported coverage schema")
    if not isinstance(spec["name"], str) or not re.fullmatch(IDENTIFIER, spec["name"]):
        raise ValueError("Invalid coverage name")
    if not isinstance(spec["source_profile"], str) or not spec["source_profile"]:
        raise ValueError("Coverage needs an exact source profile")
    for field, label, required, limit in (
        ("windows", "window", {"name", "start_frame", "end_frame"}, MAX_COVERAGE_WINDOWS),
        ("code_ranges", "code range", {"name", "offset", "size"}, MAX_COVERAGE_RANGES),
    ):
        items = spec[field]
        if not isinstance(items, list) or not 1 <= len(items) <= limit:
            raise ValueError(f"Coverage needs 1..{limit} {label}s")
        names = set()
        for item in items:
            keys(item, required, set(), f"Coverage {label}")
            if not isinstance(item["name"], str) or not re.fullmatch(IDENTIFIER, item["name"]):
                raise ValueError(f"Coverage {label} names must be identifiers")
            if item["name"] in names:
                raise ValueError(f"Coverage {label} names must be unique")
            names.add(item["name"])
    end = 0
    for window in spec["windows"]:
        start = integer(window["start_frame"], "Coverage window start", end, 35999)
        end = integer(window["end_frame"], "Coverage window end", start + 1, 36000)
    for item in spec["code_ranges"]:
        offset = integer(item["offset"], "Coverage range offset", 0, MEMORY_LIMIT - 4)
        integer(item["size"], "Coverage range size", 4, MEMORY_LIMIT - offset)
        if offset % 4 or item["size"] % 4:
            raise ValueError("Coverage code ranges must be word aligned")
    return spec


def coverage_path(output: Path) -> Path:
    return output / "coverage.bin"


def read_coverage_records(path: Path) -> list[bytes]:
    """Return each recorded window's coverage record in window order."""
    data = path.read_bytes()
    if not data.startswith(COVERAGE_MAGIC):
        raise ValueError("Coverage file has an unsupported header")
    records, at = [], len(COVERAGE_MAGIC)
    while at < len(data):
        if at + 4 > len(data):
            raise ValueError("Coverage file ends inside a chunk length")
        (size,) = struct.unpack_from("<I", data, at)
        if at + 4 + size > len(data):
            raise ValueError("Coverage file ends inside a chunk")
        stream = zlib.decompressobj()
        record = stream.decompress(data[at + 4 : at + 4 + size])
        if not stream.eof or stream.unused_data or len(record) != COVERAGE_BYTES:
            raise ValueError("Coverage chunk is not one complete record")
        records.append(record)
        at += 4 + size
    return records


def split_coverage(record: bytes) -> tuple[bytes, bytes, memoryview, memoryview]:
    """Executed bitmap, changed bitmap, first and last instruction words."""
    words = memoryview(record)[2 * COVERAGE_BITMAP :].cast("I")
    return (
        record[:COVERAGE_BITMAP],
        record[COVERAGE_BITMAP : 2 * COVERAGE_BITMAP],
        words[:COVERAGE_WORDS],
        words[COVERAGE_WORDS:],
    )


def executed_offsets(bitmap: bytes) -> list[int]:
    """RAM byte offsets of the words set in one coverage bitmap."""
    result = []
    for index, byte in enumerate(bitmap[:COVERAGE_BITMAP]):
        while byte:
            low = byte & -byte
            result.append((index * 8 + low.bit_length() - 1) * 4)
            byte ^= low
    return result


class CoverageTrace:
    """Executed RAM words and their fetched instructions at the interpreter's dispatch point.

    The fetched words attribute each executed address to an exact code image
    even when an overlay is replaced inside a window. The declared code ranges
    are hashed at each window's first and last frame boundary and each distinct
    range content is kept privately. Nothing here writes emulated state.
    """

    def __init__(self, core, spec: dict, output: Path):
        self.spec = validate_coverage(spec)
        self.core = core
        prototypes = {
            "retro_xem_coverage_enable": (None, [ct.c_uint32]),
            "retro_xem_coverage_take": (ct.c_int, [ct.POINTER(ct.c_uint8), ct.c_uint32]),
        }
        for name, (restype, argtypes) in prototypes.items():
            function = getattr(core, name, None)
            if function is None:
                raise ValueError("Coverage requires the pinned observation-trace shell")
            function.restype, function.argtypes = restype, argtypes
        self.buffer = (ct.c_uint8 * COVERAGE_BYTES)()
        self.ranges = output / "coverage-ranges"
        self.ranges.mkdir()
        self.stream = coverage_path(output).open("xb")
        self.digest = hashlib.sha256()
        self.emit(COVERAGE_MAGIC)
        self.index = 0
        self.active = None
        self.records = []
        self.core.retro_xem_coverage_enable(0)

    def emit(self, data: bytes) -> None:
        self.stream.write(data)
        self.digest.update(data)

    def hash_ranges(self, memory: bytes) -> dict:
        result = {}
        for item in self.spec["code_ranges"]:
            data = memory[item["offset"] : item["offset"] + item["size"]]
            digest = hashlib.sha256(data).hexdigest()
            path = self.ranges / f"{digest}.bin"
            if not path.exists():
                path.write_bytes(data)
            result[item["name"]] = digest
        return result

    def take(self) -> bytes:
        if self.core.retro_xem_coverage_take(self.buffer, COVERAGE_BYTES) != 1:
            raise ValueError("External core rejected the coverage transfer")
        return bytes(self.buffer)

    def due(self, frame: int) -> bool:
        """Whether `frame` is a window boundary, which needs a RAM view."""
        windows = self.spec["windows"]
        return (self.active is not None and frame == self.active["end_frame"]) or (
            self.index < len(windows) and frame == windows[self.index]["start_frame"]
        )

    def boundary(self, frame: int, memory: bytes | None) -> None:
        """Called after the previous retro_run and before the next one."""
        windows = self.spec["windows"]
        closed = self.active is not None and frame == self.active["end_frame"]
        if closed:
            self.close(self.take(), memory, complete=True)
        if (
            self.active is None
            and self.index < len(windows)
            and frame == windows[self.index]["start_frame"]
        ):
            if not closed:
                self.take()  # Discard words executed outside every window.
            self.active = {**windows[self.index], "start_ranges": self.hash_ranges(memory)}
        self.core.retro_xem_coverage_enable(self.active is not None)

    def close(self, coverage: bytes, memory: bytes | None, complete: bool) -> None:
        data = zlib.compress(coverage, 6)
        self.emit(struct.pack("<I", len(data)) + data)
        executed, changed, _, _ = split_coverage(coverage)
        record = {
            **self.active,
            "executed_words": int.from_bytes(executed, "little").bit_count(),
            "changed_words": int.from_bytes(changed, "little").bit_count(),
            "coverage_sha256": hashlib.sha256(coverage).hexdigest(),
            "complete": complete,
        }
        if memory is not None:
            record["end_ranges"] = self.hash_ranges(memory)
        self.records.append(record)
        self.active = None
        self.index += 1

    def finish(self, memory: bytes | None = None) -> dict:
        """Close an interrupted window as incomplete and report every window."""
        self.core.retro_xem_coverage_enable(0)
        if self.active is not None:
            self.close(self.take(), memory, complete=False)
        if not self.stream.closed:
            self.stream.close()
        return {
            "records": coverage_path(Path(self.stream.name).parent).name,
            "records_sha256": self.digest.hexdigest(),
            "ranges_directory": self.ranges.name,
            "windows": self.records,
        }
