"""Compare recovered C++ field entries against original full-memory boundaries.

Each case starts from an original entry snapshot captured by the guarded trace
collector. The C++ process receives only that image plus separately qualified
disc components. The matching exit snapshot is the independent expectation:
every byte owned by the reconstruction must equal it, and every byte changed by
the original outside the declared scratch/stack exclusions must be owned.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import struct
import subprocess
import tempfile
from pathlib import Path

from tools.analysis.field import field_components
from tools.analysis.packed import decode_block
from tools.analysis.party_sprites import sprite_sources
from tools.reference.instruction_trace import SnapshotReader, snapshot_path

ROOT = Path(__file__).resolve().parents[2]
PROFILE = "na-slus-00664-39c547a9afc6"
OVERLAY = "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc"
RAM = 0x200000
# BIOS exception save areas observed to change only in windows containing an
# interrupt (the field code never accesses BIOS RAM). Attributed only there.
# The ranges are the pinned core's HLE BIOS layout (its TCB and exception
# stack); a capture with another BIOS reports them as unowned writes.
KERNEL_SAVE = ((0x859C, 0x85D0), (0xE0CC, 0xE15A))
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
    "battle_atb",
    "battle_reload",
    "disc_read_file",
    "battle_ai",
    "interrupt_dispatch",
)

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
    """Program-owned GTE state: rotation/translation (0-7) and OFX, OFY, H (24-26)."""
    controls = gte_controls(row)
    return controls[0:8] + controls[24:27]


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
    arrival: str | None = None,
) -> list[tuple[dict, dict, list, list]]:
    """Adjacent entry/exit records of one call, in original order.

    Interrupt pairs name the entry and return hooks of interrupt-context code
    (a dispatcher or callback). Each call lists the complete interrupt
    invocations observed between its entry and exit, and its platform-input
    records (load and sector hooks, and `arrival` hook records) outside those
    interrupts, in order.
    """
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
            require(pending is None, "Nested or unmatched original entry record")
            require(not open_handlers, "Call entry inside interrupt code")
            pending, handlers, platform = row, [], []
        elif row["hook"] == exit_hook and pending is not None:
            require(not open_handlers, "Call exit inside interrupt code")
            result.append((pending, row, handlers, platform))
            pending = None
    return result


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
# Resident read-only sprite tables used by the recovered sprite code; the same
# extents are qualified by the EVID-REF-040 return case.
RESIDENT_TABLES = ((0x800B1F78, 256), (0x8004FD40, 12), (0x8004FF30, 40), (0x80050070, 40))


class Sources:
    """Fingerprint-qualified field 23 source, field overlay and party sprite files."""

    def __init__(self, raw: Path):
        sources = sprite_sources(raw, PROFILE, 23)
        self.field = sources["field_source"]
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


def compare(
    entry: bytes,
    exit: bytes,
    owned: list[dict],
    sp: int,
    update_changed: set[int] | None = None,
    interrupt_changed: set[int] = frozenset(),
    superseded: dict[int, tuple[int, int]] | None = None,
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
    computed = {}
    names = {}
    for item in owned:
        data = bytes.fromhex(item["hex"])
        base = item["address"] & 0x1FFFFF
        for i, value in enumerate(data):
            require(base + i not in computed, "Overlapping owned ranges")
            computed[base + i] = value
            names[base + i] = (item["name"], item["address"], i)
    low, high = (sp & 0x1FFFFF) - STACK_BELOW_ENTRY, sp & 0x1FFFFF
    changed = unowned_offsets(entry, exit)
    own = update_changed if update_changed is not None else set(changed)
    kernel = {
        o
        for o in set(changed) | own | set(interrupt_changed)
        if interrupt_changed and any(a <= o < b for a, b in KERNEL_SAVE)
    }
    excused = {o for o in interrupt_changed if o not in own} | kernel
    # An owned byte that only interrupt code changed belongs to the interrupt
    # when the C++ left it at its entry value; the Program does not run handlers.
    verified = {o for o, value in computed.items() if o in superseded and superseded[o][0] == value}
    mismatches = [
        (offset, value, exit[offset])
        for offset, value in computed.items()
        if exit[offset] != value
        and not (offset in excused and value == entry[offset])
        and offset not in verified
    ]
    unowned = [
        offset
        for offset in changed
        if offset not in computed and not low <= offset < high and offset not in excused
    ]
    # BIOS save-area bytes also change on exception entry, before the observed
    # dispatch hook; they are machine state, never Program state.
    conflicts = sorted(
        o
        for o in interrupt_changed
        if o not in verified
        and ((o in own and o not in kernel) or (o in computed and computed[o] != entry[o]))
    )
    return {
        "owned_bytes": len(computed),
        "changed_bytes": len([o for o in changed if not low <= o < high]),
        "changed_ranges": sorted({names[o][0] for o in changed if o in names}),
        "mismatches": [
            {
                "address": hex(0x80000000 + offset),
                "range": names[offset][0],
                "range_address": hex(names[offset][1]),
                "offset": hex(names[offset][2]),
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


def unowned_offsets(entry: bytes, exit: bytes) -> list[int]:
    # Fast path: compare 4 KiB pages first.
    result = []
    for page in range(0, RAM, 4096):
        if entry[page : page + 4096] != exit[page : page + 4096]:
            result.extend(page + i for i in range(4096) if entry[page + i] != exit[page + i])
    return result


def run(args: argparse.Namespace) -> int:
    capture = args.capture
    snapshot_file = snapshot_path(capture / "instruction-trace.jsonl")
    snapshots = SnapshotReader(snapshot_file)
    interrupts = tuple(tuple(item.split(":", 1)) for item in args.interrupt)
    require(all(len(item) == 2 for item in interrupts), "Interrupt pairs are ENTRY:EXIT")
    calls = pairs(capture, args.entry_hook, args.exit_hook, interrupts, args.arrival)
    require(calls, "No original entry/exit pairs in the capture")
    selected = calls[args.start : args.start + args.limit]
    # Resident entries (the heap) need no loaded field or field source.
    resident = args.entry in RESIDENT_ENTRIES
    sources = None if resident else Sources(args.raw)
    statuses = collections.Counter()
    dependencies = collections.defaultdict(list)
    divergences = []
    superseded_calls = []  # Matched calls with interrupt-superseded owned bytes.
    matched = 0
    behaviours = collections.Counter()
    opcodes = collections.Counter()  # Event opcodes entered by matched calls only.
    trace = json.loads((capture / "observation.json").read_text())["instruction_trace"]
    require(not trace.get("failed"), "Capture instruction trace failed")
    # The trace and snapshot files must be the ones the capture recorded.
    require(
        file_sha256(capture / "instruction-trace.jsonl") == trace.get("trace_sha256")
        and file_sha256(snapshot_file) == trace.get("snapshot_file_sha256"),
        "Capture trace or snapshot file does not match its recorded digest",
    )
    with tempfile.TemporaryDirectory(dir=ROOT / ".local") as directory:
        work = Path(directory)
        (work / "field.bin").write_bytes(b"" if resident else sources.field)
        (work / "overlay.bin").write_bytes(b"" if resident else sources.overlay)
        for index, (entry_row, exit_row, handlers, inputs) in enumerate(selected, args.start):
            # Read in capture order: entry, interrupt brackets, exit.
            entry, scratch, io = snapshots.read(entry_row)
            brackets = [
                (snapshots.read(before_row)[0], snapshots.read(after_row)[0])
                for before_row, after_row in handlers
            ]
            exit, _, _ = snapshots.read(exit_row)
            (work / "ram.bin").write_bytes(entry)
            (work / "scratch.bin").write_bytes(scratch)
            (work / "io.bin").write_bytes(io)
            (work / "resources.txt").write_text("" if resident else sources.manifest(entry))
            platform, recorded_sectors = platform_inputs(inputs, entry, args.arrival)
            (work / "platform.txt").write_text(platform)
            process = subprocess.run(
                [
                    str(args.runner),
                    args.entry,
                    str(args.budget),
                    args.stop,
                    str(work / "ram.bin"),
                    str(work / "scratch.bin"),
                    str(work / "field.bin"),
                    str(work / "overlay.bin"),
                    str(work / "resources.txt"),
                    # GTE rotation/translation control words at entry.
                    ",".join(f"{value:x}" for value in gte_controls(entry_row)),
                    # CPU registers at entry supply arguments (A0, A1, RA).
                    ",".join(f"{value:x}" for value in visible_registers(entry_row)),
                    str(work / "io.bin"),
                    str(work / "platform.txt"),
                    str(args.raw) if args.raw.exists() else "",
                ],
                capture_output=True,
                timeout=args.timeout,
                check=False,
            )
            try:
                report = json.loads(process.stdout)
            except json.JSONDecodeError:
                report = {
                    "status": "runner_failure",
                    "dependency": "",
                    "reason": process.stderr.decode(errors="replace")[-400:],
                    "location": None,
                }
            if process.returncode not in (0, 1):
                report["status"] = "runner_failure"
                report["reason"] = f"runner exit {process.returncode}: " + report.get("reason", "")
            status = report["status"]
            statuses[status] += 1
            frame = entry_row["frontend_run"]
            if status != "completed_boundary":
                key = report["dependency"] or report["reason"]
                dependencies[key].append(
                    {"call": index, "frontend_run": frame, "location": report["location"]}
                )
                continue
            update_changed, interrupt_changed, superseded = None, set(), {}
            if handlers:
                # Brackets must be disjoint and in time order to alternate.
                events = [entry_row["event"]]
                for before_row, after_row in handlers:
                    events += [before_row["event"], after_row["event"]]
                events.append(exit_row["event"])
                require(events == sorted(events), "Interrupt brackets overlap or are out of order")
                superseded = superseded_bytes(
                    [entry, *(image for pair in brackets for image in pair), exit]
                )
                # Alternate call segments and interrupt segments in order.
                update_changed, previous = set(), entry
                for before, after in brackets:
                    update_changed.update(unowned_offsets(previous, before))
                    interrupt_changed.update(unowned_offsets(before, after))
                    previous = after
                update_changed.update(unowned_offsets(previous, exit))
            result = compare(
                entry,
                exit,
                report["owned"],
                visible_registers(entry_row)[29],
                update_changed,
                interrupt_changed,
                superseded,
            )
            # Exact GTE rotation/translation at exit. Interrupt handlers are not
            # modeled; one that changed these registers would surface here.
            expected_gte = gte_words(exit_row)
            # A returned value must equal the original V0 at the exit hook.
            if (
                report["return_value"] is not None
                and report["return_value"] != visible_registers(exit_row)[2]
            ):
                result["return_mismatch"] = {
                    "computed": report["return_value"],
                    "original": visible_registers(exit_row)[2],
                }
                result["mismatch_count"] += 1
            # Every recorded platform input must be consumed, and the drive
            # must deliver the sectors the original received.
            if report["platform_unconsumed"]:
                result["platform_unconsumed"] = report["platform_unconsumed"]
                result["mismatch_count"] += 1
            if recorded_sectors and report["delivered_sectors"] != recorded_sectors:
                result["sector_mismatch"] = {
                    "computed": report["delivered_sectors"],
                    "recorded": recorded_sectors,
                }
                result["mismatch_count"] += 1
            if report["gte"] != expected_gte:
                result["gte_mismatch"] = {"computed": report["gte"], "original": expected_gte}
                result["mismatch_count"] += 1
            result["interrupts"] = len(handlers)
            if result["mismatch_count"] or result["unowned_count"] or result["interrupt_conflicts"]:
                divergences.append({"call": index, "frontend_run": frame, **result})
                if len(divergences) >= args.max_divergences:
                    break
            else:
                matched += 1
                if result["interrupt_superseded"]:
                    superseded_calls.append(
                        {
                            "call": index,
                            "frontend_run": frame,
                            "bytes": result["interrupt_superseded"],
                        }
                    )
                behaviours[tuple(result["changed_ranges"])] += 1
                opcodes.update(report["executed_opcodes"])
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
        "hooks": {"entry": args.entry_hook, "exit": args.exit_hook, "interrupts": interrupts},
        "matched_behaviours": [
            {"changed_ranges": list(key), "calls": count} for key, count in behaviours.items()
        ],
        "matched_event_opcodes": dict(sorted(opcodes.items())),
        "snapshot_file_sha256": trace["snapshot_file_sha256"],
        "entry": args.entry,
        "stop": args.stop,
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
            *RESIDENT_ENTRIES,
        ),
        required=True,
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
        "--arrival",
        help="Hook whose records inside a call are interrupt arrivals the call's waits deliver",
    )
    parser.add_argument("--start", type=int, default=0)
    parser.add_argument("--limit", type=int, default=1 << 30)
    parser.add_argument("--budget", type=int, default=1_000_000)
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--max-divergences", type=int, default=5)
    parser.add_argument("--report", type=Path)
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
