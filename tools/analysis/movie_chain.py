"""Compare the field movie player 800a7c58 as a chain of stage boundaries.

One import at the player's entry (or at its loop's end) runs the recovered
player through its stages; every interrupt the original served in between is
executed by the recovered handlers from recorded platform inputs (hardware
loads, controller buffers, the drive's sectors), and each stage boundary's
exported state is compared exactly with the original snapshot there. Nothing
observed enters the run except the declared platform inputs and service
results (vertical-blank counters, libgpu alarm polls and VRAM read-backs).

Arrival placement: an interrupt the original served between two main-thread
records is delivered by the recovered code where it reaches the later one:
a movie position hook (the call site or entry named by `MOVIE_POSITIONS`) or
a delivering read (a wait loop's recorded load). Between field frame steps
the field chain's convention applies (`ARRIVAL_POINTS` of memory_case).
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from tools.analysis.memory_case import (
    ARRIVAL_POINTS,
    LOOP_READS,
    SPU_TRANSFER_READS,
    DATASYNC_READ,
    KERNEL_SAVE,
    LOAD_PREFIX,
    LOADS,
    ROOT,
    SECTOR_HOOK,
    STACK_BELOW_ENTRY,
    BatchRunner,
    Sources,
    compare,
    file_sha256,
    gte_words,
    header_sector,
    image_map,
    require,
    service_line,
    u32,
    visible_registers,
)
from tools.reference.host_slots import slot as host_slot
from tools.reference.instruction_trace import SnapshotReader, snapshot_path

# Movie positions: hook name -> the point the recovered code delivers at
# (the hook's PC: a call site in the player or library, or an entry).
MOVIE_POSITION_PCS = (
    # 800a7c58 stages.
    0x800A7C58, 0x800A7CA8, 0x800A7CC0, 0x800A7CDC, 0x800A7CF0, 0x800A7D6C, 0x800A7D74,
    0x800A7D7C, 0x800A7D9C, 0x800A7E0C, 0x800A7E2C, 0x800A7E44, 0x800A7E4C, 0x800A7E5C,
    0x800A7E64, 0x800A7E88, 0x800A7EDC, 0x800A7EE4, 0x800A7EF4, 0x800A7F04, 0x800A7F0C,
    0x800A7F14, 0x800A7F1C, 0x800A7F2C, 0x800A7F3C, 0x800A7F44, 0x800A7FD4, 0x800A8014,
    0x800A8034, 0x800A8040, 0x800A80B4, 0x800A80BC, 0x800A80C4, 0x800A80DC, 0x800A80EC,
    0x800A80F4, 0x800A80FC, 0x800A8104, 0x800A8148, 0x800A8150, 0x800A8158, 0x800A8174,
    0x800A8184, 0x800A81A8, 0x800A81CC, 0x800A81DC, 0x800A8204, 0x800A820C, 0x800A8214,
    0x800A822C, 0x800A823C, 0x800A82A8, 0x800A82B0,
    # Helpers: 800a7394, 800775f8, 800a73e8, 800a74f8, 800a708c, 800a7218,
    # 800a732c, 800a77c4.
    0x800A739C, 0x800A73AC, 0x800A73D0, 0x80077600, 0x80077608, 0x800A7468, 0x800A7470,
    0x800A7490, 0x800A7498, 0x800A74C0, 0x800A74D0, 0x800A7688, 0x800A76A4, 0x800A76E8,
    0x800A76F0, 0x800A7710, 0x800A7718, 0x800A70F4, 0x800A72FC, 0x800A733C, 0x800A7360,
    0x800A7368, 0x800A77EC, 0x800A77FC, 0x800A782C, 0x800A7834, 0x800A78EC, 0x800A78F4,
    0x800A790C, 0x800A7914,
    # Library (801d3000): poll, decode, restart, stop, close, start, 801d586c.
    0x801D3FD0, 0x801D4014, 0x801D4050, 0x801D4090, 0x801D40D4, 0x801D3DFC, 0x801D3E54,
    0x801D3E98, 0x801D3F08, 0x801D3F60, 0x801D41E0, 0x801D41F0, 0x801D42C4, 0x801D42D4,
    0x801D4324, 0x801D4370, 0x801D4380, 0x801D4390, 0x801D4398, 0x801D43B8, 0x801D3930,
    0x801D3AC4, 0x801D5888, 0x801D58E4,
)
MOVIE_POSITIONS = {f"mv-{pc:08x}": pc for pc in MOVIE_POSITION_PCS} | {"mv-exit": 0x800A8308}
# The player's PutDrawEnv calls (positions at their jal).
DRAW_ENVIRONMENT_CALLS = {
    0x800A7F04, 0x800A7F3C, 0x800A80EC, 0x800A8184, 0x800A81DC, 0x800A823C
}
# The 800a7394 frame loop's call of 80077dac: arrivals after it (during its
# VSync(1)) follow the field chain's convention.
FIELD_ENTRY_POINTS = {"mv-800a739c": 0x80077DB4}
# Wait-loop loads that deliver the arrivals recorded before them.
DELIVERING_READS = {
    0x80042274, 0x80042420, 0x80041D28, 0x801D4A34, 0x801D4A90, 0x801D4ACC, 0x801D4B28
}
# Stage boundaries: snapshot hooks.
ENTRY_HOOK, FIRST_HOOK, HEAD_HOOK, END_HOOK, EXIT_HOOK = (
    "mv-800a7c58",
    "mv-800a7e5c",
    "mv-800a7e88",
    "mv-800a80b4",
    "mv-exit",
)
# StoreImage read-backs: the hook after the transfer, the register naming
# the destination there and the byte count.
VRAM_READBACKS = {
    "sv-800a7da4": (16, 0x18000),  # s0: the library through VRAM
    "sv-800a76f8": (None, 0x14000),  # *8005a418
    "sv-800a7720": (None, 0x14000),  # *8005a41c
    "sv-800a783c": (20, 0xA800),  # s4: 800a77c4's column block
}
PARKED = {"sv-800a76f8": 0x8005A418, "sv-800a7720": 0x8005A41C}
PAD_BUFFERS = 0x625FC
# The MDEC output a slice's completion loads (the callback's LoadImage call,
# 801d3350, A1 the slice buffer): the DMA1-delivered bytes.
MDEC_HOOK = "mdec-slice"
# Its last 2.5 KiB, at LoadImage's entry (80044894) right after it.
MDEC_TAIL_HOOK = "mdec-slice-tail"
# Load opcodes: memory_case's, plus lwr (26h), whose hook follows the lwr
# completing an unaligned word (801d600c).
CHAIN_LOADS = {**LOADS, 0x26: 4}


def rows_of(capture: Path) -> list[dict]:
    return [json.loads(line) for line in (capture / "instruction-trace.jsonl").open()]


def platform_lines(
    rows: list[dict], ram: bytes, io: bytes, library: bytes
) -> tuple[list[str], dict, list[int]]:
    """Platform input lines of a chain (see the module docstring). Load
    instructions are read from the entry image, or for the movie library
    (loaded during the chain) from `library`, an image holding it."""
    dma3 = u32(ram, 0x800567B4) - 0x1F801000
    idle = int.from_bytes(io[dma3 : dma3 + 4], "little") & 0x1000000
    lines, pending, block = [], [], None
    last = None
    slice_head = None
    counts = collections.Counter()
    sectors = []

    # Visits of each movie position since the last line: arrivals at a point
    # that such a visit passed without taking them are separated from it by
    # `pass` lines, one per visit.
    visits = collections.Counter()
    visits_at = -1

    def emit(point_of):
        nonlocal pending
        for previous, kind, head, reads in pending:
            point = point_of(previous)
            require(point >= 0, f"An arrival after {previous} has no delivery point")
            if visits_at == len(lines) and visits[point]:
                lines.extend([f"pass {point:x}"] * visits[point])
                counts["passes"] += visits[point]
                visits.clear()
            if kind == "tick":
                lines.append(f"tick {point:x} {head:x}")
            else:
                lines.append(f"arrival {point:x}")
                lines.extend(head)
            lines.extend(reads)
        pending = []

    for row in rows:
        hook = row["hook"]
        if hook in ("dispatch-entry", "tick-entry"):
            require(block is None, "Nested interrupt records")
            if hook == "tick-entry":
                block = ["tick", visible_registers(row)[2], []]
                counts["tick_arrivals"] += 1
            else:
                pads = bytes.fromhex(next(r["hex"] for r in row["ranges"] if r["name"] == "pads"))
                block = ["arrival", [f"pad {i:x} {b:x}" for i, b in enumerate(pads)], []]
                counts["interrupt_arrivals"] += 1
        elif hook in ("dispatch-exit", "tick-exit"):
            require(block is not None, "Interrupt exit without its entry")
            pending.append((last, *block))
            block = None
        elif block is not None:
            if hook == MDEC_HOOK:
                slice_head = row
            elif hook == MDEC_TAIL_HOOK and slice_head is not None:
                parts = sorted(slice_head["ranges"] + row["ranges"], key=lambda r: r["name"])
                data = "".join(r["hex"] for r in parts if r["name"].startswith("slice-"))
                block[2].append(f"mdec {data}")
                counts["mdec_outputs"] += 1
                slice_head = None
            elif hook == SECTOR_HOOK:
                sectors.append(header_sector(row))
            elif hook.startswith(LOAD_PREFIX):
                site = int(hook[len(LOAD_PREFIX) :], 16)
                code = u32(library if site >= 0x801D3000 else ram, site)
                require(code >> 26 in CHAIN_LOADS, "Load hook does not follow a load instruction")
                value = visible_registers(row)[(code >> 16) & 31]
                if site == DATASYNC_READ:
                    continue
                # Tick blocks carry their reads after the tick line.
                target = block[2]
                target.append(f"read {site:08x} {value:08x}")
                counts["interrupt_reads"] += 1
        elif hook in MOVIE_POSITIONS or hook in ARRIVAL_POINTS:
            if hook in MOVIE_POSITIONS:
                emit(lambda previous, point=MOVIE_POSITIONS[hook]: point)
                if visits_at != len(lines):
                    visits.clear()
                    visits_at = len(lines)
                visits[MOVIE_POSITIONS[hook]] += 1
                if MOVIE_POSITIONS[hook] in DRAW_ENVIRONMENT_CALLS:
                    visits[MOVIE_POSITIONS[hook] + 4] += 1
            else:
                emit(lambda previous: ARRIVAL_POINTS.get(previous, FIELD_ENTRY_POINTS.get(previous, -1)))
            last = hook
        elif hook == "alarm" and pending and MOVIE_POSITIONS.get(last) in DRAW_ENVIRONMENT_CALLS:
            # Arrivals inside PutDrawEnv before its first queue step (its
            # alarm) precede that step: the reconstruction delivers them at
            # the call's delay slot, after reading the argument.
            point = MOVIE_POSITIONS[last] + 4
            visits[point] -= 1
            emit(lambda previous: point)
            visits.clear()
            visits_at = len(lines)
            visits[point] = 1
        elif hook.startswith(LOAD_PREFIX):
            site = int(hook[len(LOAD_PREFIX) :], 16)
            code = u32(library if site >= 0x801D3000 else ram, site)
            require(code >> 26 in CHAIN_LOADS, "Load hook does not follow a load instruction")
            value = visible_registers(row)[(code >> 16) & 31]
            if site == DATASYNC_READ:
                require(value & 0x1000000 == idle, "DMA3 busy differs from the imported I/O page")
                continue
            if site in DELIVERING_READS:
                emit(lambda previous: 0)
                last = hook
            elif site not in LOOP_READS and site not in SPU_TRANSFER_READS:
                # Main-thread libgpu reads are frame services (queue-busy,
                # dws-return), not platform reads.
                continue
            lines.append(f"read {site:08x} {value:08x}")
            counts["main_reads"] += 1
    require(block is None, "The chain ends inside interrupt code")
    require(not pending, "Arrivals after the chain's last position")
    if sectors:
        lines.insert(0, f"drive {sectors[0]}")
    return lines, dict(counts), sectors


def slice_records(rows: list[dict]) -> list[tuple[int, int, bytes]]:
    """(row index, buffer, bytes) of each recorded MDEC output slice."""
    records, head = [], None
    for index, row in enumerate(rows):
        if row["hook"] == MDEC_HOOK:
            head = row
        elif row["hook"] == MDEC_TAIL_HOOK and head is not None:
            parts = sorted(head["ranges"] + row["ranges"], key=lambda r: r["name"])
            data = bytes.fromhex("".join(r["hex"] for r in parts if r["name"].startswith("slice-")))
            records.append((index, visible_registers(head)[5], data))
            head = None
    return records


def without_in_flight_slice(
    rows: list[dict], records: list[tuple[int, int, bytes]], index: int, image: bytes
) -> tuple[bytes, int]:
    """The boundary image with an MDEC output transfer that was in flight at
    row `index` undone. The reconstruction stores a transfer's bytes when it
    completes; DMA1 had already written part of them. A transfer is in
    flight when the slice completing next was started by the previous
    slice's completion (not by the next frame's first MDEC_out, 801d3e54).
    Only bytes equal to the transfer's own data are restored, to the
    buffer's previous slice, and their count is reported."""
    following = next((r for r in records if r[0] > index), None)
    if following is None:
        return image, 0
    if any(rows[i]["hook"] == "mv-801d3e54" for i in range(index, following[0])):
        return image, 0
    previous = next((r for r in reversed(records) if r[0] < index and r[1] == following[1]), None)
    if previous is None:
        return image, 0
    at = following[1] & 0x1FFFFF
    patched = bytearray(image)
    restored = 0
    for i, (new, old) in enumerate(zip(following[2], previous[2], strict=True)):
        if patched[at + i] == new and new != old:
            patched[at + i] = old
            restored += 1
    return bytes(patched), restored


def services_of(rows: list[dict], snapshots: SnapshotReader) -> tuple[list[str], dict]:
    """Service results outside interrupt code, in order. The ordering-table
    clears (ClearOTagR) of 80077dac's buffer swap, between its VSync(1) and
    its pad drain, read VSync(-1) themselves: their alarms are not services."""
    lines, depth = [], 0
    counts = collections.Counter()
    swapping = False
    for row in rows:
        if row["hook"] == "vsync1-loop":
            swapping = True
        elif row["hook"] == "drain-call":
            swapping = False
        if swapping and row["hook"] == "alarm" and depth == 0:
            continue
        hook = row["hook"]
        if hook in ("dispatch-entry", "tick-entry"):
            depth += 1
            continue
        if hook in ("dispatch-exit", "tick-exit"):
            depth -= 1
            continue
        if depth:
            continue
        if hook in VRAM_READBACKS:
            register, size = VRAM_READBACKS[hook]
            ram = snapshots.read(row)[0]
            pointer = (
                visible_registers(row)[register] if register is not None else u32(ram, PARKED[hook])
            )
            at = pointer & 0x1FFFFF
            lines.append(f"vram_read {size // 4:x} {ram[at : at + size].hex()}")
            counts["vram_read"] += 1
            continue
        if hook == "vsync1-loop":
            lines.append(f"hblank {visible_registers(row)[2]:x}")
            counts["hblank"] += 1
            continue
        item = service_line(row)
        if item is not None:
            lines.append(item)
            counts[item.split()[0]] += 1
    return lines, dict(counts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--raw", type=Path, default=ROOT / ".local/references/source-1/disc.bin")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/debug/xem-memory-runner")
    parser.add_argument("--from-end", action="store_true", help="Import at the loop's end 800a80b4")
    parser.add_argument(
        "--field-slot", type=int, help="Catalog slot of a field loaded outside its map pair"
    )
    parser.add_argument("--passes", type=int, default=1 << 30, help="Stop after this many passes")
    parser.add_argument("--budget", type=int, default=100_000_000)
    parser.add_argument("--timeout", type=int, default=3600)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    capture = args.capture
    trace = json.loads((capture / "observation.json").read_text())["instruction_trace"]
    require(not trace.get("failed") and not trace.get("budget_reached"), "Capture trace failed")
    require(
        file_sha256(capture / "instruction-trace.jsonl") == trace.get("trace_sha256"),
        "Capture trace does not match its recorded digest",
    )
    rows = rows_of(capture)
    snapshots = SnapshotReader(snapshot_path(capture / "instruction-trace.jsonl"))
    entry_hook = END_HOOK if args.from_end else ENTRY_HOOK
    start = next(i for i, row in enumerate(rows) if row["hook"] == entry_hook and "snapshot" in row)
    entry_row = rows[start]
    # Boundaries, in the runner's order.
    boundaries = []
    if not args.from_end:
        first = next(i for i in range(start + 1, len(rows)) if rows[i]["hook"] == FIRST_HOOK)
        boundaries.append(("prepared", first))
        heads = [i for i in range(first, len(rows)) if rows[i]["hook"] == HEAD_HOOK]
        end = next((i for i in range(first, len(rows)) if rows[i]["hook"] == END_HOOK), None)
        heads = [i for i in heads if end is None or i < end]
        require(heads, "No loop head recorded after the first-frame wait")
        boundaries.append(("first_frame", heads[0]))
        for i in heads[1 : args.passes + 1]:
            boundaries.append(("pass", i))
        if end is not None and len(heads) - 1 <= args.passes:
            boundaries.append(("ended", end))
    finished = next((i for i in range(start + 1, len(rows)) if rows[i]["hook"] == EXIT_HOOK), None)
    if (args.from_end or boundaries[-1][0] == "ended") and finished is not None:
        boundaries.append(("finished", finished))
    last = boundaries[-1][1]
    entry, scratch, io = snapshots.read(entry_row)
    chain_rows = rows[start + 1 : last + 1]
    # An image with the movie library loaded: the loop's first head, or the
    # entry itself when the chain starts at the loop's end.
    library = snapshots.read(rows[boundaries[0][1]] if args.from_end else rows[
        next(i for i in range(start, len(rows)) if rows[i]["hook"] == HEAD_HOOK)
    ])[0]
    snapshots = SnapshotReader(snapshot_path(capture / "instruction-trace.jsonl"))
    platform, counts, sectors = platform_lines(chain_rows, entry, io, library)
    services, service_counts = services_of(chain_rows, snapshots)
    sources = Sources(args.raw, image_map(entry), args.field_slot)
    registers = visible_registers(entry_row)
    with (
        host_slot("compare"),
        tempfile.TemporaryDirectory(dir=ROOT / ".local") as directory,
        BatchRunner(args.runner) as runner,
    ):
        work = Path(directory)
        (work / "field.bin").write_bytes(sources.field)
        (work / "overlay.bin").write_bytes(sources.overlay)
        (work / "ram.bin").write_bytes(entry)
        (work / "scratch.bin").write_bytes(scratch)
        (work / "io.bin").write_bytes(io)
        (work / "resources.txt").write_text(sources.manifest(entry))
        (work / "platform.txt").write_text("".join(line + "\n" for line in platform))
        (work / "services.txt").write_text("".join(line + "\n" for line in services))
        passes = sum(1 for kind, _ in boundaries if kind == "pass")
        truncated = not args.from_end and boundaries[-1][0] in ("pass", "first_frame")
        report = runner.call(
            [
                "movie_finish_chain" if args.from_end else "movie_chain",
                str(args.budget),
                f"passes={passes + 1}" if truncated else "",
                str(work / "ram.bin"),
                str(work / "scratch.bin"),
                str(work / "field.bin"),
                str(work / "overlay.bin"),
                str(work / "resources.txt"),
                ",".join(f"{value:x}" for value in entry_row["cop2_u32"]),
                ",".join(f"{value:x}" for value in registers),
                str(work / "io.bin"),
                str(work / "platform.txt"),
                str(args.raw) if args.raw.exists() else "",
                str(work / "services.txt"),
            ],
            args.timeout,
        )
    outputs = report.get("frames", [])
    stacks = tuple(
        visible_registers(row)[29] for row in chain_rows if row["hook"] == "dispatch-entry"
    )
    sp = registers[29]
    compared, divergence = [], None
    slices = slice_records(rows)
    for (kind, index), output in zip(boundaries, outputs, strict=False):
        row = rows[index]
        require(output["boundary"] == kind, f"Runner boundary {output['boundary']} is not {kind}")
        image, in_flight = without_in_flight_slice(rows, slices, index, snapshots.read(row)[0])
        result = compare(entry, image, output["owned"], sp, arrival_stacks=stacks)
        result["mdec_in_flight_bytes"] = in_flight
        if output["gte"] != gte_words(row):
            result["gte_mismatch"] = True
            result["mismatch_count"] += 1
        ok = not (result["mismatch_count"] or result["unowned_count"])
        compared.append(
            {
                "boundary": kind,
                "frontend_run": row["frontend_run"],
                "matched": ok,
                "changed_bytes": result["changed_bytes"],
                "owned_bytes": result["owned_bytes"],
                "mismatch_count": result["mismatch_count"],
                "unowned_count": result["unowned_count"],
                "mdec_in_flight_bytes": in_flight,
            }
        )
        if not ok:
            divergence = {"boundary": kind, "frontend_run": row["frontend_run"], **result}
            break
    matched = sum(1 for item in compared if item["matched"])
    summary = {
        "capture": str(capture),
        "trace_sha256": trace["trace_sha256"],
        "runner_sha256": file_sha256(args.runner),
        "tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "source_revision": subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=False
        ).stdout.strip(),
        "entry": "movie_finish_chain" if args.from_end else "movie_chain",
        "entry_frame": entry_row["frontend_run"],
        "field_slot": args.field_slot,
        "boundaries": len(boundaries),
        "boundaries_completed": len(outputs),
        "boundaries_matched": matched,
        "status": report.get("status"),
        "stopped_at": {
            "dependency": report.get("dependency"),
            "reason": report.get("reason"),
            "location": report.get("location"),
        },
        "first_divergence": divergence,
        "platform_inputs": counts,
        "platform_unconsumed": report.get("platform_unconsumed"),
        "service_results": service_counts,
        "recorded_sectors": len(sectors),
        "delivered_sectors": len(report.get("delivered_sectors", [])),
        "tolerance": "exact at every stage boundary: owned bytes and every unowned original "
        "write since the import",
        "exclusions": {
            "stack_below_entry_sp": STACK_BELOW_ENTRY,
            "stack_below_arrival_sp": STACK_BELOW_ENTRY,
            "bios_save_areas_and_pad_buffers": [[hex(a), hex(b)] for a, b in KERNEL_SAVE],
            "scratchpad": "not compared",
        },
        "boundary_results": compared,
    }
    text = json.dumps(summary, indent=1)
    if args.report:
        require(not args.report.exists(), "Reports are never overwritten")
        args.report.write_text(text + "\n")
    print(
        json.dumps(
            {
                k: summary[k]
                for k in (
                    "boundaries",
                    "boundaries_completed",
                    "boundaries_matched",
                    "status",
                    "stopped_at",
                    "platform_unconsumed",
                )
            }
        )
    )
    if divergence:
        print(json.dumps({k: v for k, v in divergence.items() if k != "owned"})[:4000])
    return 0 if matched == len(boundaries) and divergence is None else 1


if __name__ == "__main__":
    raise SystemExit(main())
