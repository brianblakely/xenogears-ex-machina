"""Compare recovered C++ field entries against original full-memory boundaries.

Each case starts from an original entry snapshot captured by the guarded trace
collector. The C++ process receives only that image plus separately qualified
disc components. The matching exit snapshot is the independent expectation:
every byte owned by the reconstruction must equal it, and every byte changed by
the original outside the declared scratch/stack exclusions must be owned.
"""

from __future__ import annotations

import argparse
import bisect
import collections
import hashlib
import json
import re
import selectors
import struct
import subprocess
import tempfile
from pathlib import Path

from tools.analysis.field import field_components
from tools.analysis.packed import decode_block
from tools.analysis.party_sprites import sprite_sources
from tools.reference.host_slots import slot as host_slot
from tools.reference.inspect_disc import RawCd
from tools.reference.instruction_trace import SnapshotReader, snapshot_path

ROOT = Path(__file__).resolve().parents[2]
PROFILE = "na-slus-00664-39c547a9afc6"
OVERLAY = "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc"
RAM = 0x200000
# BIOS exception save areas observed to change only in windows containing an
# interrupt (the field code never accesses BIOS RAM). Attributed only there.
# The ranges are the pinned core's HLE BIOS layout (its TCB and exception
# stack); a capture with another BIOS reports them as unowned writes.
# The last range is the two 0x22-byte controller buffers resident 80036288
# registers with InitPAD (80040828: 800625fc, 8006261e); general PS1 BIOS
# documentation: the BIOS vertical-blank handler fills them before the
# observed event dispatch.
KERNEL_SAVE = ((0x859C, 0x85D0), (0xE0CC, 0xE15A), (0x625FC, 0x62640))
# The entry frame's caller stack below the entry SP is transient callee
# storage; observed field-update frames reach 0x250 bytes below it.
STACK_BELOW_ENTRY = 0x800
# Excluded, not compared: the 1 KiB scratchpad. Field code allocates balanced
# temporary frames there (8007cd3c/8007cd60) that this call tree writes before
# reading; no scratchpad value is Program state.


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def u32(ram: bytes, address: int) -> int:
    return struct.unpack_from("<I", ram, address & 0x1FFFFF)[0]


FIELD_MAP = 0x8004F34C  # The field map; primary 98 stores a requested map here.
IO_BASE = 0x1F801000  # The recorded hardware I/O page.
SPU_PAGE = (0xC00, 0xE00)  # SPU registers inside it.


# Entries that need no loaded field or field source.
RESIDENT_ENTRIES = (
    "heap_allocate",
    "heap_release",
    "music_stop",
    "battle_commit",
    "battle_apply",
    "battle_alive",
    "battle_rewards",
    "battle_reward_totals",
    "battle_drops",
    "battle_results_step",
    "battle_atb",
    "battle_reload",
    "disc_read_file",
    "disc_read_files",
    "disc_read_stream",
    "decode_block",
    "battle_ai",
    "menu_item_effect",
    "menu_item_use",
    "menu_equip_swap",
    "menu_equip_bonus",
    "menu_equip_stats",
    "menu_save_serialize",
    "menu_save_seal",
    "menu_save_store",
    "menu_names_decode",
    "menu_load_check",
    "menu_load_restore",
    "menu_load_apply",
    "menu_load_slot_valid",
    "menu_load_find_slot",
    "menu_text_encode",
    "menu_text_decode",
    "sound_set_mode",
    "sound_set_master",
    "sound_set_cd",
    "sound_update_voices",
    "set_next_mode",
    "battle_mode_exit",
    "field_exit",
    "interrupt_dispatch",
    "sound_tick",
    "battle_turn_select",
    "battle_turn_begin",
    "battle_turn_actions_begin",
    "battle_turn_actions",
    "battle_turn_actions_resume",
    "battle_turn_prepare",
    "battle_turn_order",
    "battle_turn_settle",
    "battle_turn_finish",
    "battle_turn_decode",
    "battle_turn_menu",
    "battle_turn_menu_presented",
    "battle_turn_attack_resume",
    "battle_turn_view_resume",
    "battle_turn_combo_resume",
    "battle_turn_confirm_resume",
)
# Field entries besides the update and move phases.
FIELD_ENTRIES = ("field_event_extended", "movie_decision")
# The field reload 800a5c40 and the steps around each field frame. Their
# platform results are the service records inside the call (see
# `call_services`).
RELOAD_ENTRIES = (
    "field_pre_frame",
    "field_post_frame",
    "field_reload_draw",
    "field_reload_shade",
    "field_reload_fade_in",
    "field_reload_fade_frame",
    "field_reload_finish",
    "field_reload_teardown",
    "field_reload",
    "field_load",
)
# 80077dac in the reload's fade-in loop: the loop head before it and the
# return after it. Its VSync(1) result goes straight to 800adb9c, which the
# return's image holds.
STEP_ENTRY, STEP_EXIT = "f-head", "f-7dac"
FRAME_START_HCOUNT = 0x800ADB9C
# StoreImage read-backs without a service hook: the hook after the transfer
# completes, the word naming the destination and the byte count. 800a915c
# saves 40h x 100h of VRAM into the block at 800afc70 before t-b.
VRAM_READS = {"t-b": [(0x800AFC70, 0x8000)]}
# 800a5884's five columns (800a5774) are read into one heap block, just below
# the read-ahead block (8005a4e0), each read overwriting the last: the image
# after them holds the fifth (bit 15 set, which 800a5774 sets again). The
# first four are overwritten before any image and are supplied with it.
COLUMN_READS = ("r-5884", 5, 0x7000)

# Platform inputs. A hook named `load-SITE` sits on the instruction after the
# original hardware load at SITE (hex); the loaded value is that load's target
# register with pending loads committed. A hook named `sector` sits on the
# sector-position conversion 80041534 with a four-byte range at A0: the header
# (BCD minute, second, sector) of the sector the drive delivered.
LOAD_PREFIX = "load-"
SECTOR_HOOK = "sector"
LOADS = {0x20: 1, 0x21: 2, 0x23: 4, 0x24: 1, 0x25: 2}


def file_sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def gte_controls(row: dict) -> list[int]:
    """The 32 GTE control registers of a trace record."""
    require("cop2_u32" in row, "Capture records lack GTE registers; recapture required")
    return row["cop2_u32"][32:64]


def gte_words(row: dict) -> list[int]:
    """Program-owned GTE state: control registers 0-30 (FLAG is transient)."""
    return gte_controls(row)[0:31]


# Platform results a field frame consumes (see FrameServices). Each service
# hook records the value at the original service's return: VSync(1) results,
# the VSync(0) globals, each libgpu alarm's VSync(-1), the alarm polls counted
# by DrawSync/LoadImage waits, ClearImage's GPUSTAT read, the queue's DMA busy
# check, SetIntrMask(0)'s previous mask and GPU information reads (GPUREAD).
def service_line(row: dict) -> str | None:
    hook = row["hook"]
    registers = visible_registers(row)

    def range_words(name: str) -> list[int]:
        data = bytes.fromhex(next(item["hex"] for item in row["ranges"] if item["name"] == name))
        return list(struct.unpack(f"<{len(data) // 4}I", data))

    if hook in ("vsync1-a", "vsync1-b"):
        return f"hblank {registers[2]:x}"
    if hook == "vsync0-return":
        hcount, counter = range_words("vsync-state")
        return f"vblank_wait {hcount:x} {counter:x}"
    if hook == "vsync0-wait":
        # Inside VSync (8004b674) after a wait: 80057848 is stored and V1
        # holds root counter 1, stored next at 80057844.
        return f"vblank_wait {registers[3]:x} {range_words('vsync-state')[1]:x}"
    if hook == "alarm":
        return f"vblank {registers[2]:x}"
    if hook in ("sync-return", "dws-return"):
        return f"alarm_polls {range_words('alarm-state')[1]:x}"
    if hook in ("clear-status-a", "clear-status-b"):
        return f"gpu_status {registers[4]:x}"
    if hook == "queue-busy":
        return f"dma_busy {registers[2]:x}"
    if hook == "intr-mask":
        return f"interrupt_mask {registers[2]:x}"
    if hook == "gpu-info":
        return f"gpu_info {registers[2]:x}"
    return None


def frame_services(
    capture: Path, entry_hook: str, exit_hook: str, interrupts: tuple[tuple[str, str], ...]
) -> dict[str, tuple[int, list[tuple[int, str]]]]:
    """Entry cycle and (cycle, service line) pairs per call, keyed by the entry
    snapshot's RAM digest.

    The key lets a capture of the identical execution with other hooks supply
    the results; records inside interrupt brackets are not the call's.
    """
    starts = {entry for entry, _ in interrupts}
    ends = {exit for _, exit in interrupts}
    result, lines, key, depth, start = {}, None, None, 0, 0
    for line in (capture / "instruction-trace.jsonl").read_text().splitlines():
        row = json.loads(line)
        hook = row["hook"]
        if hook in starts:
            depth += 1
        elif hook in ends:
            depth -= 1
        elif hook == entry_hook:
            key, lines, start = row["snapshot"]["ram_sha256"], [], row["cycle_u32"]
        elif hook == exit_hook and lines is not None:
            require(key not in result, "Two calls start from the same image")
            result[key] = (start, lines)
            lines = None
        elif lines is not None and depth == 0:
            item = service_line(row)
            if item is not None:
                lines.append((row["cycle_u32"], item))
    # A frame the window cut short keeps the results it recorded; a later
    # result it would need is reported as not supplied.
    if lines is not None:
        result[key] = (start, lines)
    return result


def call_services(
    rows: list[dict], snapshots, entry_row: dict, exit_row: dict, interrupts
) -> list[str]:
    """Service results of one call, in order, outside interrupt brackets.

    The VSync(1) of 80077dac has no service hook: it is the value the step
    stores at 800adb9c, read from the image at the step's return. StoreImage
    read-backs are the destination's bytes in the image of the first hook
    after the transfer (VRAM_READS).
    """
    starts = {entry for entry, _ in interrupts}
    ends = {exit for _, exit in interrupts}
    clears = otc_alarms(rows, entry_row, exit_row, interrupts)
    lines, depth = [], 0
    for index, row in enumerate(rows):
        if not entry_row["event"] <= row["event"] <= exit_row["event"]:
            continue
        hook = row["hook"]
        if hook in VRAM_READS and row["event"] > entry_row["event"]:
            ram = snapshots.read(row)[0]
            for pointer, size in VRAM_READS[hook]:
                at = u32(ram, pointer) & 0x1FFFFF
                lines.append(f"vram_read {size // 4:x} {ram[at : at + size].hex()}")
        if hook == COLUMN_READS[0] and row["event"] > entry_row["event"]:
            ram = snapshots.read(row)[0]
            _, count, size = COLUMN_READS
            at = (u32(ram, 0x8005A4E0) - 8 - size) & 0x1FFFFF
            lines += [f"vram_read {size // 4:x} {ram[at : at + size].hex()}"] * count
        if row["event"] == exit_row["event"]:
            continue
        if hook in starts:
            depth += 1
        elif hook in ends:
            depth -= 1
        elif depth == 0:
            if hook == STEP_ENTRY:
                after = next((r for r in rows[index + 1 :] if r["hook"] == STEP_EXIT), None)
                require(after is not None, "A pre-frame step has no recorded return")
                ram = snapshots.read(after)[0]
                lines.append(f"hblank {u32(ram, FRAME_START_HCOUNT & 0x1FFFFF):x}")
            # ClearOTagR's alarm reads VSync(-1) itself.
            if row["event"] in clears:
                continue
            item = service_line(row)
            if item is not None:
                lines.append(item)
    return lines


def otc_alarms(rows: list[dict], entry_row: dict, exit_row: dict, interrupts) -> set[int]:
    """Events of the call's libgpu alarms that belong to ClearOTagR (80044ad8).

    Each draw-buffer swap (80073fe0) clears its tables back to back: first
    from 80073f50, then from 80073fe0 itself, a frame of 18h higher, with no
    other service between them. The alarm hook records no caller, so these
    runs of consecutive alarms identify the clears.
    """
    starts = {entry for entry, _ in interrupts}
    ends = {exit for _, exit in interrupts}
    services, depth = [], 0
    for row in rows:
        if not entry_row["event"] <= row["event"] < exit_row["event"]:
            continue
        if row["hook"] in starts:
            depth += 1
        elif row["hook"] in ends:
            depth -= 1
        elif depth == 0 and service_line(row) is not None:
            services.append(row)
    clears = set()
    for k in range(len(services) - 1):
        low, high = services[k], services[k + 1]
        if not (
            low["hook"] == high["hook"] == "alarm"
            and visible_registers(high)[29] == visible_registers(low)[29] + 0x18
        ):
            continue
        clears.update({low["event"], high["event"]})
        # A third clear (the second model table) repeats the frame before it.
        if (
            k + 2 < len(services)
            and services[k + 2]["hook"] == "alarm"
            and visible_registers(services[k + 2])[29] == visible_registers(high)[29]
        ):
            clears.add(services[k + 2]["event"])
    return clears


def assumed_otc_reads(rows: list[dict], entry_row: dict, exit_row: dict, interrupts) -> list[str]:
    """One idle DMA6 busy read (80045de4) per ClearOTagR of the call.

    The shared reload capture records no hardware loads; with
    --assume-idle-otc each ordering-table clear's first busy poll is supplied
    as idle, which the zero poll count after every clear agrees with.
    """
    return ["read 80045de4 00000000"] * len(otc_alarms(rows, entry_row, exit_row, interrupts))


def visible_registers(row: dict) -> list[int]:
    """CPU registers with pending loads committed.

    A load issued just before a hook (for example in a caller's delay slot) is
    still pending in the interpreter's load-delay slots; the code at the hook
    sees it from the next instruction. Slots commit in order, the selected one
    first.
    """
    require("load_delay" in row, "Capture records lack load-delay state; recapture required")
    registers = list(row["gpr_u32"])
    delay = row["load_delay"]
    pending = [slot for slot in (delay["select"], delay["select"] ^ 1) if delay["registers"][slot]]
    require(len(pending) <= 1, "More than one load is pending at a hook")
    for slot in pending:
        target = delay["registers"][slot]
        # The hooked instruction must not name the pending register in any
        # register field; otherwise its own view of it is ambiguous here.
        code = row["code"]
        require(
            target not in ((code >> 21) & 31, (code >> 16) & 31, (code >> 11) & 31),
            "The hooked instruction names a register with a pending load",
        )
        registers[target] = delay["values"][slot]
    return registers


def pairs(
    capture: Path,
    entry_hook: str,
    exit_hook: str,
    interrupts: tuple[tuple[str, str], ...] = (),
    outcomes: tuple[str, ...] = (),
    presentation: tuple[tuple[str, str], ...] = (),
    entry_repeats: bool = False,
    arrival: str | None = None,
) -> list[tuple[dict, dict, list, list]]:
    """Adjacent entry/exit records of one call, in original order.

    Interrupt pairs name the entry and return hooks of interrupt-context code
    (a dispatcher or callback). Each call lists the complete interrupt
    invocations observed between its entry and exit, and its platform-input
    records (load and sector hooks, and `arrival` hook records) outside those
    interrupts, in order. Outcome hooks close a call like the exit hook; they
    mark the branch a decision took.

    Presentation pairs bracket calls the reconstruction does not run (camera,
    text windows). They are attributed like interrupt code; a bracket that
    overlaps another (an interrupt inside a camera call) merges into one span
    that is presentation.

    With `entry_repeats` the entry hook is a loop head: its repeated records
    inside a call belong to that call. An entry hook that is also the exit
    hook heads a loop whose passes are the calls; an outcome hook ends the
    last pass.
    """
    interrupts = tuple(interrupts) + tuple(presentation)
    presented = {entry for entry, _ in presentation}
    exits = {exit_hook, *outcomes}
    rows = [
        json.loads(line) for line in (capture / "instruction-trace.jsonl").read_text().splitlines()
    ]
    starts = {entry: exit for entry, exit in interrupts}
    ends = {exit: entry for entry, exit in interrupts}
    # Different interrupt paths may overlap (a BIOS event callback can run
    # while the dispatcher's context is active); one path never nests itself.
    result, pending, handlers, open_handlers, platform = [], None, [], {}, []
    for row in rows:
        platform_row = row["hook"] != entry_hook and (
            row["hook"].startswith(LOAD_PREFIX) or row["hook"] in (SECTOR_HOOK, arrival)
        )
        if platform_row:
            if pending is not None and not open_handlers:
                platform.append(row)
        elif row["hook"] in starts:
            require(row["hook"] not in open_handlers, "Nested interrupt records")
            open_handlers[row["hook"]] = row
        elif row["hook"] in ends:
            entry = open_handlers.pop(ends[row["hook"]], None)
            require(entry is not None, "Interrupt return without its entry")
            if pending is not None:
                handlers.append((entry, row))
        elif row["hook"] == entry_hook:
            # An entry hook that is also the exit hook heads a loop: each
            # pass closes the previous one.
            if entry_hook == exit_hook and pending is not None:
                require(not open_handlers, "Call exit inside interrupt code")
                result.append((pending, row, merge_brackets(handlers, presented), platform))
                pending = None
            if entry_repeats and pending is not None:
                continue
            require(pending is None, "Nested or unmatched original entry record")
            require(not open_handlers, "Call entry inside interrupt code")
            pending, handlers, platform = row, [], []
        elif row["hook"] in exits and pending is not None:
            require(not open_handlers, "Call exit inside interrupt code")
            result.append((pending, row, merge_brackets(handlers, presented), platform))
            pending = None
    return result


def merge_brackets(handlers: list, presented: set[str]) -> list:
    """Time-ordered brackets; overlapping spans merge when one is presentation.

    Each item is (entry row, return row); a merged span keeps the earliest
    entry and the latest return and counts as presentation.
    """
    spans = sorted(handlers, key=lambda pair: pair[0]["event"])
    merged: list = []
    for before, after in spans:
        if merged and before["event"] < merged[-1][1]["event"]:
            last_before, last_after = merged[-1]
            require(
                before["hook"] in presented or last_before["hook"] in presented,
                "Interrupt brackets overlap",
            )
            if after["event"] > last_after["event"]:
                last_after = after
            if last_before["hook"] not in presented:
                last_before = dict(last_before, hook=before["hook"])
            merged[-1] = (last_before, last_after)
        else:
            merged.append((before, after))
    return merged


def header_sector(row: dict) -> int:
    """LBA of a delivered sector from its recorded BCD header."""
    require(row["ranges"] and row["ranges"][0]["size"] == 4, "Sector hook lacks its header range")
    header = bytes.fromhex(row["ranges"][0]["hex"])

    def decimal(value: int) -> int:
        return (value >> 4) * 10 + (value & 15)

    return (decimal(header[0]) * 60 + decimal(header[1])) * 75 + decimal(header[2]) - 150


def platform_inputs(rows: list[dict], ram: bytes, arrival: str | None) -> tuple[str, list[int]]:
    """The runner's platform input file and the recorded delivered sectors.

    Load values come from the original registers after each load; nothing is
    taken from an exit image.
    """
    lines, sectors = [], []
    for row in rows:
        hook = row["hook"]
        if hook == arrival:
            lines.append("interrupt")
        elif hook == SECTOR_HOOK:
            sectors.append(header_sector(row))
        else:
            site = int(hook[len(LOAD_PREFIX) :], 16)
            require(row["pc"] == site + 4, "Load hook is not on the instruction after its load")
            code = u32(ram, site)
            require(code >> 26 in LOADS, "Load hook does not follow a load instruction")
            value = visible_registers(row)[(code >> 16) & 31]
            lines.append(f"read {site:08x} {value:08x}")
    if sectors:
        lines.insert(0, f"drive {sectors[0]}")
    return "".join(line + "\n" for line in lines), sectors


PARTY_RESOURCES = 0x8005A414
PARTY_SLOTS = 0x8005A444
FIELD_SPRITES = 0x800AFB1C
FIELD_GEOMETRY = 0x800AFB14  # Component 2: models, including collision models.
# Resident read-only tables: sprite tables used by the recovered sprite code
# (qualified by the EVID-REF-040 return case) and the primitive routine table
# 8004fe50 of the model renderer, 17 rows of 28 bytes, which contains the two
# sprite tables at 8004ff30 and 80050070.
RESIDENT_TABLES = (
    (0x800B1F78, 256),
    (0x8004FD40, 12),
    (0x8004FE50, 17 * 0x28),
    (0x8004FAB8, 0x20),  # sprite texture origins (8001d53c)
    (0x8004FAF8, 0x10),  # sprite group masks (8001e3d8)
    (0x800ADF04, 0x28),  # field overlay: dialogue prompt cursor frames (8007e1c0)
)


def slot_bytes(raw: Path, slot: int) -> bytes:
    """The fingerprint-qualified physical bytes of one catalog source slot."""
    catalog = json.loads((ROOT / "analysis/coverage/source-fingerprints.json").read_text())
    profile = next(p for p in catalog["profiles"] if p["source_profile"] == PROFILE)
    records = [dict(zip(profile["records_columns"], r, strict=True)) for r in profile["records"]]
    record = next((r for r in records if r["slot"] == slot), None)
    require(record is not None and record["projection_unit"] == 2048, "Unknown field slot")
    with raw.open("rb") as stream:
        data = RawCd(stream, raw.stat().st_size).read_extent(
            record["source_lba"], record["source_sector_count"] * 2048
        )
    require(
        hashlib.sha256(data[: record["projected_bytes"]]).hexdigest()
        == record["projection_sha256"],
        "Field slot projection differs from its record",
    )
    return data


def image_map(ram: bytes) -> int:
    """The field map an image names (bits 0-13 of 8004f34c, as 80092f44 reads it)."""
    return u32(ram, FIELD_MAP) & 0x3FFF


class Sources:
    """Fingerprint-qualified field source, field overlay and party sprite files.

    The map is the field loaded in the capture (the selector at 8004f34c). A
    field that is not the map's own file pair (the New Game prologue's) names
    its catalog slot instead; import still requires its components to equal
    the loaded ones.
    """

    def __init__(self, raw: Path, map_id: int, field_slot: int | None = None):
        sources = sprite_sources(raw, PROFILE, map_id)
        self.field = sources["field_source"] if field_slot is None else slot_bytes(raw, field_slot)
        # Source slot 36: the packed field overlay; its decoded block (from
        # decode_block in math_sources) is the image loaded at 8006faf0.
        record = sources["overlay_record"]
        packed = sources["overlay_packed"]
        require(record["slot"] == 36, "Field overlay record is not source slot 36")
        require(
            hashlib.sha256(packed[: record["projected_bytes"]]).hexdigest()
            == record["projection_sha256"],
            "Field overlay projection differs from its record",
        )
        self.overlay = sources["overlay"]
        require(
            hashlib.sha256(self.overlay).hexdigest() == OVERLAY,
            "Decoded field overlay differs from the qualified image",
        )
        components = field_components(self.field)
        self.sprite_bundle = components[3].logical_size
        self.geometry = components[2].logical_size
        self.party = [
            decode_block(item["physical"]).data for item in sources["party_resources"].values()
        ]

    def manifest(self, ram: bytes) -> str:
        """Resource extents from source sizes; bytes stay the image's live state."""
        rows = list(RESIDENT_TABLES) + [
            (u32(ram, FIELD_SPRITES), self.sprite_bundle),
            (u32(ram, FIELD_GEOMETRY), self.geometry),
        ]
        for slot in range(3):
            pointer = u32(ram, PARTY_RESOURCES + 4 * slot)
            if pointer == 0 or u32(ram, PARTY_SLOTS + 4 * slot) == 0xFF:
                continue  # Unoccupied party slot: its buffer is not a loaded resource.
            offset = pointer & 0x1FFFFF
            matches = [d for d in self.party if ram[offset : offset + 64] == d[:64]]
            require(len(matches) == 1, "Party resource pointer does not identify one source file")
            rows.append((pointer, len(matches[0])))
        return "".join(f"{address} {size}\n" for address, size in rows)


class BatchRunner:
    """One `xem-memory-runner --batch` process that serves a case's calls in turn.

    Each call writes the single-run arguments as one tab-separated line and
    reads that call's report line. Calls share no state in the runner. A call
    that exceeds its time budget, or a runner that exits, yields a
    runner_failure report for that call; the next call starts a new process.
    """

    def __init__(self, path: Path):
        self.path = path
        self.process: subprocess.Popen | None = None
        self.errors = tempfile.TemporaryFile()

    def __enter__(self) -> BatchRunner:
        return self

    def __exit__(self, *_: object) -> None:
        self.stop()
        self.errors.close()

    def stop(self) -> None:
        if self.process is not None:
            self.process.kill()
            self.process.wait()
            self.process = None

    def failure(self, reason: str) -> dict:
        self.errors.seek(0)
        tail = self.errors.read().decode(errors="replace")[-400:]
        self.errors.seek(0)
        self.errors.truncate()
        return {
            "status": "runner_failure",
            "dependency": "",
            "reason": reason + tail,
            "location": None,
        }

    def call(self, arguments: list[str], timeout: float) -> dict:
        require(
            all("\t" not in item and "\n" not in item for item in arguments),
            "Runner arguments cannot contain tabs or newlines",
        )
        if self.process is None:
            self.process = subprocess.Popen(
                [str(self.path), "--batch"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=self.errors,
            )
        self.process.stdin.write(("\t".join(arguments) + "\n").encode())
        self.process.stdin.flush()
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            ready = selector.select(timeout)
        if not ready:
            self.stop()
            return self.failure(f"runner exceeded {timeout} s: ")
        line = self.process.stdout.readline()
        if not line.endswith(b"\n"):
            code = self.process.wait()
            self.process = None
            return self.failure(f"runner exit {code}: ")
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            self.stop()
            return self.failure("runner wrote no report: ")


class OwnedBytes:
    """The runner's owned ranges as sorted, disjoint RAM spans.

    Lookups bisect the span starts; comparisons check whole spans as slices
    and visit single bytes only where a span differs. Behaves as the mapping
    offset -> computed value it replaces.
    """

    def __init__(self, owned: list[dict]):
        spans = sorted(
            (item["address"] & 0x1FFFFF, bytes.fromhex(item["hex"]), item["name"], item["address"])
            for item in owned
        )
        for (base, data, _, _), (following, _, _, _) in zip(spans, spans[1:], strict=False):
            require(base + len(data) <= following, "Overlapping owned ranges")
        self.spans = [span for span in spans if span[1]]
        self.starts = [span[0] for span in self.spans]

    def _span(self, offset: int):
        index = bisect.bisect_right(self.starts, offset) - 1
        if index >= 0:
            span = self.spans[index]
            if offset < span[0] + len(span[1]):
                return span
        return None

    def __contains__(self, offset: int) -> bool:
        return self._span(offset) is not None

    def __len__(self) -> int:
        return sum(len(span[1]) for span in self.spans)

    def get(self, offset: int) -> int | None:
        span = self._span(offset)
        return None if span is None else span[1][offset - span[0]]

    def name(self, offset: int) -> tuple[str, int, int]:
        base, _, name, address = self._span(offset)
        return name, address, offset - base

    def differing(self, image: bytes):
        """(offset, computed value) for every owned byte that differs from image."""
        for base, data, _, _ in self.spans:
            if image[base : base + len(data)] == data:
                continue
            for page in range(0, len(data), 4096):
                chunk = data[page : page + 4096]
                if image[base + page : base + page + len(chunk)] == chunk:
                    continue
                original = image[base + page : base + page + len(chunk)]
                for i in differing_positions(chunk, original):
                    yield base + page + i, chunk[i]


def compare(
    entry: bytes,
    exit: bytes,
    owned: list[dict],
    sp: int,
    update_changed: set[int] | None = None,
    interrupt_changed: set[int] = frozenset(),
    superseded: dict[int, tuple[int, int]] | None = None,
    arrival_stacks: tuple[int, ...] = (),
) -> dict:
    """Exact comparison of owned bytes plus attribution of every other change.

    Without interrupts every changed byte belongs to the call. With observed
    interrupt code, a byte is excused only if it changed inside interrupt code
    and never in the call's own segments; overlap is reported, never hidden.
    `superseded` maps a byte that interrupt code changed after the call's last
    write to its value when that interrupt began and the interrupt's index (see
    `superseded_bytes`); an owned byte equal to it is matched and listed with
    both, not hidden.
    """
    superseded = superseded or {}
    computed = OwnedBytes(owned)
    # Callee stack windows: the call's, and each arrived interrupt's (its
    # handler runs on the stack the exception hook selects). An entry SP of
    # 80200000 (seen inside the mode dispatcher) is the end of RAM, not
    # offset zero.
    windows = [
        (((stack - 1) & 0x1FFFFF) + 1 - STACK_BELOW_ENTRY, ((stack - 1) & 0x1FFFFF) + 1)
        for stack in sorted({sp, *arrival_stacks})
    ]

    def stacked(offset: int) -> bool:
        return any(low <= offset < high for low, high in windows)

    changed = unowned_offsets(entry, exit)
    own = update_changed if update_changed is not None else set(changed)
    kernel = {
        o
        for o in set(changed) | own | set(interrupt_changed)
        if (interrupt_changed or arrival_stacks) and any(a <= o < b for a, b in KERNEL_SAVE)
    }
    excused = {o for o in interrupt_changed if o not in own} | kernel
    # An owned byte that only interrupt code changed belongs to the interrupt
    # when the C++ left it at its entry value; the Program does not run handlers.
    verified = {o for o, (value, _) in superseded.items() if computed.get(o) == value}
    mismatches = [
        (offset, value, exit[offset])
        for offset, value in computed.differing(exit)
        if not (offset in excused and value == entry[offset]) and offset not in verified
    ]
    unowned = [
        offset
        for offset in changed
        if offset not in computed and not stacked(offset) and offset not in excused
    ]
    # BIOS save-area bytes also change on exception entry, before the observed
    # dispatch hook; they are machine state, never Program state.
    conflicts = sorted(
        o
        for o in interrupt_changed
        if o not in verified
        and ((o in own and o not in kernel) or (o in computed and computed.get(o) != entry[o]))
        # The stack below the entry SP is transient for both.
        and not stacked(o)
    )
    return {
        "owned_bytes": len(computed),
        "changed_bytes": len([o for o in changed if not stacked(o)]),
        "changed_ranges": sorted({computed.name(o)[0] for o in changed if o in computed}),
        "mismatches": [
            {
                "address": hex(0x80000000 + offset),
                "range": computed.name(offset)[0],
                "range_address": hex(computed.name(offset)[1]),
                "offset": hex(computed.name(offset)[2]),
                "computed": computed_value,
                "original": original,
            }
            for offset, computed_value, original in mismatches[:64]
        ],
        "mismatch_count": len(mismatches),
        "unowned_writes": [hex(0x80000000 + o) for o in unowned[:64]],
        "unowned_count": len(unowned),
        "interrupt_attributed": len([o for o in changed if o in excused]),
        "interrupt_conflicts": [hex(0x80000000 + o) for o in conflicts[:64]],
        # Only bytes the call also changed; the others are excused above.
        "interrupt_superseded": [
            {
                "address": hex(0x80000000 + o),
                "value": superseded[o][0],
                "interrupt": superseded[o][1],
                "exit": exit[o],
            }
            for o in sorted(verified & own)
        ],
    }


def superseded_bytes(images: list[bytes]) -> dict[int, tuple[int, int]]:
    """Bytes interrupt code changed after the call's last write to them.

    `images` alternate call and interrupt segments: entry, then before and
    after each disjoint, time-ordered interrupt, then exit. A byte's value when
    the first interrupt after the call's last change to it began is the call's
    own final value; each byte maps to that value and the interrupt's index.
    Snapshots show net changes only, so a call store that repeats the value an
    interrupt left is invisible here, as it is to the interrupt-only rule.
    """
    brackets = (len(images) - 2) // 2
    require(len(images) == 2 * brackets + 2, "Interrupt images must alternate with call segments")
    changed = set()
    for k in range(brackets):
        changed.update(unowned_offsets(images[2 * k + 1], images[2 * k + 2]))
    result = {}
    for o in changed:
        # Call segment i runs from images[2i] to images[2i + 1].
        last = max(
            (i for i in range(brackets + 1) if images[2 * i][o] != images[2 * i + 1][o]), default=0
        )
        for k in range(last, brackets):
            if images[2 * k + 1][o] != images[2 * k + 2][o]:
                result[o] = (images[2 * k + 1][o], k)
                break
    return result


def segments(
    entry_row: dict, exit_row: dict, handlers: list, entry: bytes, exit: bytes, brackets: list
) -> tuple[set[int] | None, set[int], dict[int, tuple[int, int]]]:
    """The call's own changed bytes, interrupt-changed bytes and superseded bytes."""
    if not handlers:
        return None, set(), {}
    # Brackets must be disjoint and in time order to alternate.
    events = [entry_row["event"]]
    for before_row, after_row in handlers:
        events += [before_row["event"], after_row["event"]]
    events.append(exit_row["event"])
    require(events == sorted(events), "Interrupt brackets overlap or are out of order")
    superseded = superseded_bytes([entry, *(image for pair in brackets for image in pair), exit])
    # Alternate call segments and interrupt segments in order.
    update_changed, interrupt_changed, previous = set(), set(), entry
    for before, after in brackets:
        update_changed.update(unowned_offsets(previous, before))
        interrupt_changed.update(unowned_offsets(before, after))
        previous = after
    update_changed.update(unowned_offsets(previous, exit))
    return update_changed, interrupt_changed, superseded


NONZERO = re.compile(rb"[^\x00]")


def differing_positions(a: bytes, b: bytes) -> list[int]:
    """Indexes where two equal-length byte strings differ, found in C."""
    difference = int.from_bytes(a, "little") ^ int.from_bytes(b, "little")
    return [match.start() for match in NONZERO.finditer(difference.to_bytes(len(a), "little"))]


def unowned_offsets(entry: bytes, exit: bytes) -> list[int]:
    # Compare 4 KiB pages first; locate bytes only in pages that differ.
    result = []
    for page in range(0, RAM, 4096):
        a, b = entry[page : page + 4096], exit[page : page + 4096]
        if a != b:
            result.extend(page + i for i in differing_positions(a, b))
    return result


def run(args: argparse.Namespace) -> int:
    capture = args.capture
    snapshot_file = snapshot_path(capture / "instruction-trace.jsonl")
    snapshots = SnapshotReader(snapshot_file)
    interrupts = tuple(tuple(item.split(":", 1)) for item in args.interrupt)
    require(all(len(item) == 2 for item in interrupts), "Interrupt pairs are ENTRY:EXIT")
    outcomes = {}
    for item in args.outcome:
        hook, _, value = item.partition("=")
        require(hook and value.isdigit(), "Outcomes are HOOK=VALUE")
        outcomes[hook] = int(value)
    presentation = tuple(tuple(item.split(":", 1)) for item in args.presentation)
    require(all(len(item) == 2 for item in presentation), "Presentation pairs are ENTRY:EXIT")
    presented = {entry for entry, _ in presentation}
    calls = pairs(
        capture,
        args.entry_hook,
        args.exit_hook,
        interrupts,
        tuple(outcomes),
        presentation,
        args.entry_repeats,
        args.arrival,
    )
    require(calls, "No original entry/exit pairs in the capture")
    selected = calls[args.start : args.start + args.limit]
    # Resident entries (the heap) need no loaded field or field source. Field
    # entries use the source of the map each entry image names, unless --map
    # selects one (a requested map is stored before its field is loaded).
    resident = args.entry in RESIDENT_ENTRIES
    loaded_sources: dict[int, Sources] = {}
    statuses = collections.Counter()
    dependencies = collections.defaultdict(list)
    divergences = []
    superseded_calls = []  # Matched calls with interrupt-superseded owned bytes.
    matched = 0
    behaviours = collections.Counter()
    opcodes = collections.Counter()  # Event opcodes entered by matched calls only.
    services, frame_keys = {}, {}
    call_rows = trace_rows(capture) if args.entry in RELOAD_ENTRIES else []
    if args.entry == "field_frame":
        services = frame_services(args.services or capture, "frame-entry", "frame-exit", interrupts)
        # Each call's frame: the latest frame entry at or before its entry record.
        key = None
        for line in (capture / "instruction-trace.jsonl").read_text().splitlines():
            row = json.loads(line)
            if row["hook"] == "frame-entry":
                key = row["snapshot"]["ram_sha256"]
            for call_entry, *_ in calls:
                if call_entry["event"] == row["event"]:
                    frame_keys[id(call_entry)] = key
        # A call in a frame that began before the capture window has no
        # recorded services; it is not comparable.
        selected = [call for call in selected if frame_keys.get(id(call[0])) is not None]
    chains = [[call] for call in selected]
    trace = json.loads((capture / "observation.json").read_text())["instruction_trace"]
    require(not trace.get("failed"), "Capture instruction trace failed")
    # The trace and snapshot files must be the ones the capture recorded.
    require(
        file_sha256(capture / "instruction-trace.jsonl") == trace.get("trace_sha256")
        and file_sha256(snapshot_file) == trace.get("snapshot_file_sha256"),
        "Capture trace or snapshot file does not match its recorded digest",
    )
    with (
        tempfile.TemporaryDirectory(dir=ROOT / ".local") as directory,
        BatchRunner(args.runner) as runner,
    ):
        work = Path(directory)
        maps = collections.Counter()
        index = args.start
        for chain in chains:
            # Read in capture order: entry, interrupt brackets, exit.
            frames = []
            for entry_row, exit_row, handlers, inputs in chain:
                entry, scratch, io = snapshots.read(entry_row)
                brackets = [
                    (snapshots.read(before_row)[0], snapshots.read(after_row)[0])
                    for before_row, after_row in handlers
                ]
                exit, _, exit_io = snapshots.read(exit_row)
                frames.append(
                    {
                        "entry_row": entry_row,
                        "exit_row": exit_row,
                        "handlers": handlers,
                        "inputs": inputs,
                        "entry": entry,
                        "brackets": brackets,
                        "exit": exit,
                        "exit_io": exit_io,
                    }
                )
            first = frames[0]
            entry, scratch, io = snapshots.read(first["entry_row"])
            sources = None
            if not resident:
                map_id = image_map(entry) if args.map is None else args.map
                if map_id not in loaded_sources:
                    loaded_sources[map_id] = Sources(args.raw, map_id, args.field_slot)
                sources = loaded_sources[map_id]
                maps[map_id] += len(frames)
            (work / "field.bin").write_bytes(b"" if resident else sources.field)
            (work / "overlay.bin").write_bytes(b"" if resident else sources.overlay)
            (work / "ram.bin").write_bytes(entry)
            (work / "scratch.bin").write_bytes(scratch)
            (work / "io.bin").write_bytes(io)
            (work / "resources.txt").write_text("" if resident else sources.manifest(entry))
            platform, recorded_sectors = platform_inputs(first["inputs"], entry, args.arrival)
            if args.assume_idle_otc:
                require(args.entry in RELOAD_ENTRIES, "Assumed reads apply to reload entries")
                platform += "".join(
                    line + "\n"
                    for line in assumed_otc_reads(
                        call_rows, first["entry_row"], first["exit_row"], interrupts
                    )
                )
            (work / "platform.txt").write_text(platform)
            lines = []
            for item in frames:
                row = item["entry_row"]
                if args.entry == "field_frame":
                    # Results of the frame containing the entry, from the entry
                    # on: captures of one execution share cycle counts.
                    key = frame_keys[id(row)]
                    require(key in services, "No service results recorded for this frame")
                    start = services[key][0]
                    lines += [
                        line
                        for cycle, line in services[key][1]
                        if (cycle - start) % (1 << 32) >= (row["cycle_u32"] - start) % (1 << 32)
                    ]
                if args.entry in RELOAD_ENTRIES:
                    lines += call_services(
                        call_rows, snapshots, row, item["exit_row"], interrupts
                    )
            (work / "services.txt").write_text("".join(line + "\n" for line in lines))
            entry_row = first["entry_row"]
            report = runner.call(
                [
                    args.entry + (f":{args.frame_from}" if args.frame_from else ""),
                    str(args.budget),
                    args.stop,
                    str(work / "ram.bin"),
                    str(work / "scratch.bin"),
                    str(work / "field.bin"),
                    str(work / "overlay.bin"),
                    str(work / "resources.txt"),
                    # All 64 GTE registers at entry (data, then control).
                    ",".join(f"{value:x}" for value in entry_row["cop2_u32"]),
                    # CPU registers at entry supply arguments (A0, A1, RA).
                    ",".join(f"{value:x}" for value in visible_registers(entry_row)),
                    str(work / "io.bin"),
                    str(work / "platform.txt"),
                    str(args.raw) if args.raw.exists() else "",
                    str(work / "services.txt"),
                ],
                args.timeout * len(frames),
            )
            if report["status"] == "completed_boundary":
                outputs = [report]
            else:
                outputs = []
            stop = False
            for k, item in enumerate(frames):
                entry_row, exit_row = item["entry_row"], item["exit_row"]
                entry, exit = item["entry"], item["exit"]
                frame = entry_row["frontend_run"]
                call = index + k
                if k >= len(outputs):
                    status = report["status"]
                    if status == "completed_boundary":
                        status = "runner_failure"
                    statuses[status] += 1
                    key = report["dependency"] or report["reason"]
                    dependencies[key].append(
                        {"call": call, "frontend_run": frame, "location": report["location"]}
                    )
                    break
                statuses["completed_boundary"] += 1
                output = outputs[k]
                update_changed, interrupt_changed, superseded = segments(
                    entry_row, exit_row, item["handlers"], entry, exit, item["brackets"]
                )
                result = compare(
                    entry,
                    exit,
                    output["owned"],
                    visible_registers(entry_row)[29] + args.frame_above,
                    update_changed,
                    interrupt_changed,
                    superseded,
                    tuple(
                        visible_registers(row)[29]
                        for row in item["inputs"]
                        if row["hook"] == args.arrival
                    ),
                )
                # Exact GTE control registers at exit. Interrupt handlers are
                # not modeled; one that changed these registers would surface here.
                expected_gte = gte_words(exit_row)
                # Presentation (a camera call) may load GTE matrices the call's
                # own code never uses: the reconstruction keeps the entry's
                # registers and every segment of original call code must leave
                # them unchanged.
                handlers = item["handlers"]
                if any(before["hook"] in presented for before, _ in handlers):
                    expected_gte = gte_words(entry_row)
                    bounds = [entry_row, *(row for pair in handlers for row in pair), exit_row]
                    for start_row, end_row in zip(bounds[::2], bounds[1::2], strict=True):
                        if gte_words(start_row) != gte_words(end_row):
                            result["gte_segment_mismatch"] = start_row["frontend_run"]
                            result["mismatch_count"] += 1
                returned = output.get("return_value")
                # A call closed by an outcome hook must return that hook's value.
                if exit_row["hook"] in outcomes:
                    if returned != outcomes[exit_row["hook"]]:
                        result["outcome_mismatch"] = {
                            "computed": returned,
                            "original": exit_row["hook"],
                        }
                        result["mismatch_count"] += 1
                # A returned value must equal the original result register (V0
                # unless the exit hook precedes a delay slot that moves it there).
                elif (
                    returned is not None
                    and returned != visible_registers(exit_row)[args.return_register]
                ):
                    result["return_mismatch"] = {
                        "computed": returned,
                        "original": visible_registers(exit_row)[args.return_register],
                    }
                    result["mismatch_count"] += 1
                # Every recorded platform input must be consumed, and the drive
                # must deliver the sectors the original received.
                if report.get("platform_unconsumed"):
                    result["platform_unconsumed"] = report["platform_unconsumed"]
                    result["mismatch_count"] += 1
                if recorded_sectors and report.get("delivered_sectors") != recorded_sectors:
                    result["sector_mismatch"] = {
                        "computed": report.get("delivered_sectors"),
                        "recorded": recorded_sectors,
                    }
                    result["mismatch_count"] += 1
                if output["gte"] != expected_gte:
                    result["gte_mismatch"] = {"computed": output["gte"], "original": expected_gte}
                    result["mismatch_count"] += 1
                # Each SPU register the call wrote must hold its last written
                # value in the exit I/O page, when the page records the SPU. The
                # pinned core's recorded page does not mirror the SPU registers
                # (they read zero in every snapshot); writes there are counted
                # as unrecorded, not compared. Other registers (DMA, CD ports)
                # do not read back what was written and are not compared here.
                written = {}
                for write in output.get("hardware_writes", []):
                    offset = write["address"] - IO_BASE
                    if SPU_PAGE[0] <= offset < SPU_PAGE[1]:
                        written[offset] = write
                exit_io = item["exit_io"]
                io_mismatches = []
                if any(exit_io[SPU_PAGE[0] : SPU_PAGE[1]]):
                    io_mismatches = [
                        {
                            "address": hex(IO_BASE + offset),
                            "computed": write["value"],
                            "original": value,
                        }
                        for offset, write in sorted(written.items())
                        if (
                            value := int.from_bytes(
                                exit_io[offset : offset + write["width"]], "little"
                            )
                        )
                        != write["value"]
                    ]
                elif written:
                    result["spu_writes_unrecorded"] = len(written)
                result["hardware_writes"] = len(output.get("hardware_writes", []))
                if io_mismatches:
                    result["hardware_mismatch"] = io_mismatches
                    result["mismatch_count"] += len(io_mismatches)
                result["interrupts"] = len(item["handlers"])
                if (
                    result["mismatch_count"]
                    or result["unowned_count"]
                    or result["interrupt_conflicts"]
                ):
                    divergences.append({"call": call, "frontend_run": frame, **result})
                    if len(divergences) >= args.max_divergences:
                        stop = True
                        break
                else:
                    matched += 1
                    if result["interrupt_superseded"]:
                        superseded_calls.append(
                            {
                                "call": call,
                                "frontend_run": frame,
                                "bytes": result["interrupt_superseded"],
                            }
                        )
                    behaviours[tuple(result["changed_ranges"])] += 1
                    if k == 0:
                        opcodes.update(report["executed_opcodes"])
            index += len(frames)
            if stop:
                break
    trace_status = json.loads((capture / "observation.json").read_text())["instruction_trace"]
    summary = {
        "capture": str(capture),
        "capture_trace": {
            key: trace_status.get(key)
            for key in ("trace_sha256", "snapshot_file_sha256", "failed", "budget_reached")
        },
        "runner_sha256": file_sha256(args.runner),
        "tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "source_revision": subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=False
        ).stdout.strip(),
        "hooks": {
            "entry": args.entry_hook,
            "exit": args.exit_hook,
            "interrupts": interrupts,
            "presentation": presentation,
            "return_register": args.return_register,
        },
        "matched_behaviours": [
            {"changed_ranges": list(key), "calls": count} for key, count in behaviours.items()
        ],
        "matched_event_opcodes": dict(sorted(opcodes.items())),
        "snapshot_file_sha256": trace["snapshot_file_sha256"],
        "entry": args.entry,
        "field_maps": {str(k): v for k, v in sorted(maps.items())},
        "field_slot": args.field_slot,
        "outcomes": outcomes,
        "stop": args.stop,
        "assumed_idle_otc_reads": args.assume_idle_otc,
        "calls_available": len(calls),
        "calls_selected": len(selected),
        "matched": matched,
        "statuses": dict(statuses),
        "dependencies": {
            key: {"count": len(rows), "first": rows[0], "last": rows[-1]}
            for key, rows in dependencies.items()
        },
        "divergences": divergences,
        "interrupt_superseded": superseded_calls,
        "tolerance": "exact; owned bytes and every unowned original write",
        "exclusions": {
            "stack_below_entry_sp": STACK_BELOW_ENTRY,
            "callee_frames_above_entry_sp": args.frame_above,
            "scratchpad": "not compared; balanced temporary frames only",
        },
        "interrupt_attribution": (
            "bytes changed only inside observed interrupt-code brackets and never in the "
            "call's own segments, plus the observed BIOS exception save areas in windows "
            "containing such code; overlaps are conflicts, except owned bytes whose "
            "computed value equals the byte when the first interrupt after the call's "
            "last change to it began (listed under interrupt_superseded)"
            if interrupts
            else None
        ),
    }
    text = json.dumps(summary, indent=1)
    if args.report:
        require(not args.report.exists(), "Reports are never overwritten")
        args.report.write_text(text + "\n")
    print(
        json.dumps(
            {
                "calls_selected": summary["calls_selected"],
                "matched": matched,
                "statuses": summary["statuses"],
                "dependencies": {k: v["count"] for k, v in summary["dependencies"].items()},
                "divergent_calls": [
                    [d["frontend_run"], d["mismatch_count"], d["unowned_count"]]
                    for d in divergences
                ],
                "interrupt_superseded": [
                    [c["frontend_run"], len(c["bytes"])] for c in superseded_calls
                ],
            }
        )
    )
    return 0 if matched == len(selected) else 1


# Multi-frame runs: the field main loop 80077e88 between frames. Each
# interrupt arrival is delivered where the recovered code reaches the point
# that follows the last position hook before it (Program::deliver_arrivals):
# frame steps are hooked at their call sites in the image capture, the loop
# steps in the platform capture.
ARRIVAL_POINTS = {
    "frame-entry": 0x8007555C,  # during the frame's first VSync(1)
    "vsync1-a": 0x800739C0,  # field_move
    "s-86908": 0x80086908,
    "s-71cb4": 0x80071CB4,
    "s-74108": 0x80074108,
    "s-748e8": 0x800748E8,
    "s-752c8": 0x800752C8,
    "s-a9688": 0x800A9688,
    "s-a4dac": 0x80075694,  # up to the VSync(1) after drawing
    "vsync1-b": 0x8004B54C,  # DrawSync, dialogue, VSync(0)
    "vsync0-return": 0x8007554C,  # the rest of the frame and its final wait
    "frame-exit": 0x80077DB4,  # 800a5924 .. the VSync(1) of 80077dac
    "vsync1-loop": 0x80077DCC,  # the ordering tables
    "drain-call": 0x80074700,  # the pad drain (ordering unknown; delivered after it)
    "drain-return": 0x800A31E8,
    "loop-31e8": 0x80077DAC,  # 800a31e8
}
# Hardware reads the code between frames consumes: ClearOTagR's DMA6 busy
# polls. The frame's own results come from its services.
LOOP_READS = (0x80045DE4, 0x80045E18)
# CD_datasync's DMA3 busy read; the recovered disc status (800286cc) takes it
# from the imported I/O page, so every recording must agree with that page.
DATASYNC_READ = 0x80042A6C
PAD_BUFFERS = (0x625FC, 0x44)
MAIN_LOOP_RETURN = 0x800782E4


def trace_rows(capture: Path) -> list[dict]:
    return [json.loads(line) for line in (capture / "instruction-trace.jsonl").open()]


def loop_inputs(
    image_rows: list[dict],
    loop_rows: list[dict],
    pads: dict[int, bytes],
    ram: bytes,
    io: bytes,
) -> tuple[list[str], dict, list[int], set[int]]:
    """Platform input lines of a chain, their accounting, sectors and arrival stacks.

    Rows are one execution's records between the chain's first frame entry
    and last frame exit, merged by cycle.
    """
    base = loop_rows[0]["cycle_u32"]
    merged = sorted(
        image_rows + loop_rows,
        key=lambda row: ((row["cycle_u32"] - base) % (1 << 32), row["subcycle_u32"]),
    )
    dma3 = u32(ram, 0x800567B4) - IO_BASE
    idle = struct.unpack_from("<I", io, dma3)[0] & 0x1000000
    lines, pending, block, last = [], [], None, None
    counts = collections.Counter()
    sectors, stacks = [], set()
    for row in merged:
        hook = row["hook"]
        if hook in ARRIVAL_POINTS:
            require(block is None, "A position hook inside interrupt code")
            lines += [line for item in pending for line in item]
            pending, last = [], hook
        elif hook in ("dispatch-entry", "tick-entry"):
            require(block is None and last is not None, "Arrival outside the chain's positions")
            point = ARRIVAL_POINTS[last]
            stacks.add(visible_registers(row)[29])
            if hook == "tick-entry":
                block = [f"tick {point:x} {visible_registers(row)[2]:x}"]
                counts["tick_arrivals"] += 1
            else:
                pad = pads[row["cycle_u32"]]
                block = [f"arrival {point:x}"] + [f"pad {i:x} {b:x}" for i, b in enumerate(pad)]
                counts["interrupt_arrivals"] += 1
                counts["pad_bytes"] += len(pad)
            if last == "drain-call":
                counts["arrivals_during_drain"] += 1
        elif hook in ("dispatch-exit", "tick-exit"):
            require(block is not None, "Interrupt exit without its arrival")
            pending.append(block)
            block = None
        elif hook == SECTOR_HOOK:
            require(block is not None, "Sector delivered outside interrupt code")
            sectors.append(header_sector(row))
        elif hook.startswith(LOAD_PREFIX):
            site = int(hook[len(LOAD_PREFIX) :], 16)
            require(row["pc"] == site + 4, "Load hook is not on the instruction after its load")
            code = u32(ram, site)
            require(code >> 26 in LOADS, "Load hook does not follow a load instruction")
            value = visible_registers(row)[(code >> 16) & 31]
            if block is not None:
                block.append(f"read {site:08x} {value:08x}")
                counts["interrupt_reads"] += 1
            elif site in LOOP_READS:
                lines.append(f"read {site:08x} {value:08x}")
                counts["loop_reads"] += 1
            elif site == DATASYNC_READ:
                require(value & 0x1000000 == idle, "DMA3 busy differs from the imported I/O page")
                counts["datasync_reads_checked"] += 1
    require(block is None, "The chain ends inside interrupt code")
    lines += [line for item in pending for line in item]
    if sectors:
        lines.insert(0, f"drive {sectors[0]}")
    return lines, dict(counts), sectors, stacks


def run_frames(args: argparse.Namespace) -> int:
    """Consecutive field main-loop iterations from one imported frame entry.

    The image capture (--capture) holds frame-entry and frame-exit snapshots
    plus the frame-step, dispatch and tick hooks; --services the frames'
    service results; --platform the loop hooks, arrivals and hardware reads.
    All three record one execution. Every frame boundary is compared exactly;
    nothing observed enters the run except the declared platform inputs.
    """
    capture = args.capture
    require(
        args.entry == "field_frame" and args.platform and args.services,
        "Multi-frame runs are field_frame chains with --platform and --services",
    )
    snapshots = SnapshotReader(snapshot_path(capture / "instruction-trace.jsonl"))
    for path in (capture, args.platform, args.services):
        trace = json.loads((path / "observation.json").read_text())["instruction_trace"]
        require(
            not trace.get("failed") and not trace.get("budget_reached"),
            f"Capture trace of {path} failed or reached its budget",
        )
        require(
            file_sha256(path / "instruction-trace.jsonl") == trace.get("trace_sha256"),
            f"Capture trace of {path} does not match its recorded digest",
        )
    calls = pairs(capture, "frame-entry", "frame-exit")
    image_rows = trace_rows(capture)
    loop_rows = trace_rows(args.platform)
    services = frame_services(args.services, "frame-entry", "frame-exit", ())
    positions = {hook for hook in ARRIVAL_POINTS if hook not in ("frame-entry", "frame-exit")}
    # A main-loop iteration's frame returns to 800782e4. Frames other code
    # calls (the entry fade-in 80078d44 returns to 80079178) split chains.
    selected = calls[args.start : args.start + args.limit]
    callers = collections.Counter(
        hex(visible_registers(entry)[31])
        for entry, *_ in selected
        if visible_registers(entry)[31] != MAIN_LOOP_RETURN
    )
    runs, chains, results = [[]], [], []
    for call in selected:
        if visible_registers(call[0])[31] == MAIN_LOOP_RETURN:
            runs[-1].append(call)
        elif runs[-1]:
            runs.append([])
    for run in runs:
        chains += [run[i : i + args.frames] for i in range(0, len(run), args.frames)]
    loaded_sources: dict[int, Sources] = {}
    with (
        tempfile.TemporaryDirectory(dir=ROOT / ".local") as directory,
        BatchRunner(args.runner) as runner,
    ):
        work = Path(directory)
        for chain in chains:
            entry_row, last_exit = chain[0][0], chain[-1][1]
            entry, scratch, io = snapshots.read(entry_row)
            start, end = entry_row["cycle_u32"], last_exit["cycle_u32"]

            first_run, last_run = entry_row["frontend_run"], last_exit["frontend_run"]

            # Cycle counts wrap; frontend frames place a row in the chain's span.
            def inside(row: dict, start=start, end=end, runs=(first_run, last_run)) -> bool:
                return runs[0] <= row["frontend_run"] <= runs[1] and (row["cycle_u32"] - start) % (
                    1 << 32
                ) <= (end - start) % (1 << 32)

            chain_loop = [row for row in loop_rows if inside(row)]
            chain_image = [row for row in image_rows if inside(row) and row["hook"] in positions]
            pads = {
                row["cycle_u32"]: snapshots.read(row)[0][PAD_BUFFERS[0] : sum(PAD_BUFFERS)]
                for row in image_rows
                if inside(row) and row["hook"] == "dispatch-entry"
            }
            platform, counts, sectors, stacks = loop_inputs(
                chain_image, chain_loop, pads, entry, io
            )
            # Services: each frame's results; before each later frame, the
            # VSync(1) of 80077dac.
            loop_vsyncs = [
                visible_registers(row)[2] for row in chain_loop if row["hook"] == "vsync1-loop"
            ]
            lines, service_counts = [], collections.Counter()
            for k, (frame_entry, _, _, _) in enumerate(chain):
                key = frame_entry["snapshot"]["ram_sha256"]
                require(key in services, "No service results recorded for a chain frame")
                if k:
                    require(len(loop_vsyncs) >= k, "No VSync(1) recorded between frames")
                    lines += ["frame", f"hblank {loop_vsyncs[k - 1]:x}"]
                    service_counts["hblank"] += 1
                for _, line in services[key][1]:
                    lines.append(line)
                    service_counts[line.split()[0]] += 1
            map_id = image_map(entry) if args.map is None else args.map
            if map_id not in loaded_sources:
                loaded_sources[map_id] = Sources(args.raw, map_id, args.field_slot)
            sources = loaded_sources[map_id]
            (work / "field.bin").write_bytes(sources.field)
            (work / "overlay.bin").write_bytes(sources.overlay)
            (work / "ram.bin").write_bytes(entry)
            (work / "scratch.bin").write_bytes(scratch)
            (work / "io.bin").write_bytes(io)
            (work / "resources.txt").write_text(sources.manifest(entry))
            (work / "platform.txt").write_text("".join(line + "\n" for line in platform))
            (work / "services.txt").write_text("".join(line + "\n" for line in lines))
            report = runner.call(
                [
                    "field_frames",
                    str(args.budget * len(chain)),
                    "",
                    str(work / "ram.bin"),
                    str(work / "scratch.bin"),
                    str(work / "field.bin"),
                    str(work / "overlay.bin"),
                    str(work / "resources.txt"),
                    ",".join(f"{value:x}" for value in entry_row["cop2_u32"]),
                    ",".join(f"{value:x}" for value in visible_registers(entry_row)),
                    str(work / "io.bin"),
                    str(work / "platform.txt"),
                    str(args.raw) if args.raw.exists() else "",
                    str(work / "services.txt"),
                ],
                args.timeout * len(chain),
            )
            outputs = report.get("frames", [])
            sp = visible_registers(entry_row)[29] + args.frame_above
            boundaries = []
            for k, (frame_entry, frame_exit, _, _) in enumerate(chain):
                boundaries += [] if k == 0 else [("entry", frame_entry)]
                boundaries.append(("exit", frame_exit))
            compared, first_divergence = [], None
            for (kind, row), output in zip(boundaries, outputs, strict=False):
                require(output["boundary"] == kind, "Runner boundaries out of order")
                image = snapshots.read(row)[0]
                result = compare(entry, image, output["owned"], sp, arrival_stacks=tuple(stacks))
                if output["gte"] != gte_words(row):
                    result["gte_mismatch"] = {"computed": output["gte"], "original": gte_words(row)}
                    result["mismatch_count"] += 1
                ok = not (result["mismatch_count"] or result["unowned_count"])
                compared.append(
                    {
                        "boundary": kind,
                        "frontend_run": row["frontend_run"],
                        "matched": ok,
                        "changed_bytes": result["changed_bytes"],
                        "owned_bytes": result["owned_bytes"],
                        "interrupt_attributed": result["interrupt_attributed"],
                        "mismatch_count": result["mismatch_count"],
                        "unowned_count": result["unowned_count"],
                    }
                )
                if not ok:
                    first_divergence = {
                        "boundary": kind,
                        "frontend_run": row["frontend_run"],
                        **result,
                    }
                    break
            matched = 0
            for item in compared:
                if not item["matched"]:
                    break
                matched += 1
            complete = len(outputs) == len(boundaries)
            if complete and first_divergence is None:
                if report.get("platform_unconsumed"):
                    first_divergence = {"platform_unconsumed": report["platform_unconsumed"]}
                elif sectors and report.get("delivered_sectors") != sectors:
                    first_divergence = {
                        "sector_mismatch": {
                            "computed": report.get("delivered_sectors"),
                            "recorded": sectors,
                        }
                    }
            # Frames whose exit matched with every earlier boundary.
            frames_matched = sum(1 for item in compared[:matched] if item["boundary"] == "exit")
            results.append(
                {
                    "first_frame": entry_row["frontend_run"],
                    "frames": len(chain),
                    "boundaries": len(boundaries),
                    "boundaries_completed": len(outputs),
                    "boundaries_matched": matched,
                    "frames_matched": frames_matched,
                    "status": report["status"],
                    "stopped_at": None
                    if complete
                    else {
                        "dependency": report.get("dependency"),
                        "reason": report.get("reason"),
                        "location": report.get("location"),
                    },
                    "first_divergence": first_divergence,
                    "platform_inputs": counts,
                    "service_results": dict(service_counts),
                    "supplied_state_bytes": 0,
                    "boundary_results": compared,
                }
            )
    summary = {
        "captures": {
            name: {
                "path": str(path),
                "trace_sha256": json.loads((path / "observation.json").read_text())[
                    "instruction_trace"
                ]["trace_sha256"],
            }
            for name, path in (
                ("images", capture),
                ("platform", args.platform),
                ("services", args.services),
            )
        },
        "runner_sha256": file_sha256(args.runner),
        "tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "source_revision": subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=False
        ).stdout.strip(),
        "entry": "field_frames",
        "tolerance": "exact at every frame entry and exit; owned bytes and every unowned "
        "original write since the import",
        "exclusions": {
            "stack_below_entry_sp": STACK_BELOW_ENTRY,
            "stack_below_arrival_sp": STACK_BELOW_ENTRY,
            "bios_save_areas_and_pad_buffers": [[hex(a), hex(b)] for a, b in KERNEL_SAVE],
            "scratchpad": "not compared",
        },
        "frames_of_other_callers": dict(callers),
        "chains": results,
    }
    text = json.dumps(summary, indent=1)
    if args.report:
        require(not args.report.exists(), "Reports are never overwritten")
        args.report.write_text(text + "\n")
    print(
        json.dumps(
            [
                [
                    c["first_frame"],
                    c["frames"],
                    c["frames_matched"],
                    c["boundaries_matched"],
                    c["status"],
                    (c["stopped_at"] or {}).get("dependency"),
                    bool(c["first_divergence"]),
                ]
                for c in results
            ]
        )
    )
    return (
        0
        if all(c["frames_matched"] == c["frames"] and not c["first_divergence"] for c in results)
        else 1
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--raw", type=Path, default=ROOT / ".local/references/source-1/disc.bin")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/debug/xem-memory-runner")
    parser.add_argument(
        "--entry",
        choices=(
            "field_event_pass",
            "field_update",
            "field_move",
            "field_checkpoints",
            "field_save",
            "field_preload",
            "field_map_change_step",
            "field_map_change_start",
            "field_frame",
            *RESIDENT_ENTRIES,
            *FIELD_ENTRIES,
            *RELOAD_ENTRIES,
        ),
        required=True,
    )
    parser.add_argument(
        "--services",
        type=Path,
        help="Capture of the same execution whose service hooks supply field_frame results",
    )
    parser.add_argument(
        "--frame-from", help="field_frame step to resume at (the entry hook's call site)"
    )
    parser.add_argument("--entry-hook", default="update-entry")
    parser.add_argument("--exit-hook", default="update-return")
    parser.add_argument("--stop", default="", help="Completed library boundary to stop at")
    parser.add_argument(
        "--interrupt",
        action="append",
        default=[],
        help="ENTRY:EXIT hook names bracketing interrupt-context code (repeatable)",
    )
    parser.add_argument(
        "--entry-repeats",
        action="store_true",
        help="The entry hook heads a loop; repeats inside a call belong to it",
    )
    parser.add_argument(
        "--presentation",
        action="append",
        default=[],
        help="ENTRY:EXIT hook names bracketing presentation calls the reconstruction skips",
    )
    parser.add_argument(
        "--return-register",
        type=int,
        choices=range(32),
        default=2,
        help="Register holding the original result at the exit hook",
    )
    parser.add_argument(
        "--outcome",
        action="append",
        default=[],
        help="HOOK=VALUE: another exit hook; calls it closes must return VALUE (repeatable)",
    )
    parser.add_argument(
        "--field-slot", type=int, help="Catalog slot of a field loaded outside its map pair"
    )
    parser.add_argument("--map", type=int, help="Field source map (default: the image's map)")
    parser.add_argument(
        "--arrival",
        help="Hook whose records inside a call are interrupt arrivals the call's waits deliver",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=1,
        help="Run this many consecutive field main-loop iterations from one import "
        "(field_frame only; needs --platform)",
    )
    parser.add_argument(
        "--platform",
        type=Path,
        help="Capture of the same execution with the main-loop hooks, interrupt arrivals and "
        "their hardware reads (multi-frame runs)",
    )
    parser.add_argument("--start", type=int, default=0)
    parser.add_argument("--limit", type=int, default=1 << 30)
    parser.add_argument("--budget", type=int, default=1_000_000)
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--max-divergences", type=int, default=5)
    parser.add_argument(
        "--frame-above",
        type=int,
        default=0,
        help="Bytes of callee frames a resumed entry lies inside; its stack exclusion ends there",
    )
    parser.add_argument(
        "--assume-idle-otc",
        action="store_true",
        help="Supply each ClearOTagR's unrecorded DMA6 busy read as idle (reload entries)",
    )
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    with host_slot("compare"):
        return run_frames(args) if args.frames > 1 else run(args)


if __name__ == "__main__":
    raise SystemExit(main())
