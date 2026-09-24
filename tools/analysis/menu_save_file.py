"""Compare the C++ save file block with the card file the original wrote.

The C++ process receives only the original entry image of the save payload
build (801cba4c). It serializes, seals (801cc424) and composes the file 801cbd90
writes: the header template, then the payload. The expectation is the card
image the same capture saved, located through the card directory (general
PS1 memory-card layout: 128 KiB, 16 blocks of 8 KiB, block 0 holding 128-byte
directory frames whose +0a is the file name).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from tools.analysis.memory_case import (
    ROOT,
    file_sha256,
    gte_controls,
    pairs,
    require,
    visible_registers,
)
from tools.reference.instruction_trace import SnapshotReader, snapshot_path

BLOCK = 0x2000
FRAME = 0x80
CARD = 0x20000


def card_file(card: bytes, name: bytes) -> tuple[int, bytes]:
    """Directory frame index and first data block of the file `name`."""
    require(len(card) == CARD and card[:2] == b"MC", "Not a formatted 128 KiB card image")
    found = []
    for index in range(1, 16):
        frame = card[index * FRAME : (index + 1) * FRAME]
        state = int.from_bytes(frame[0:4], "little")
        if state == 0x51 and frame[0xA : 0xA + 21].split(b"\0", 1)[0] == name:
            found.append(index)
    require(len(found) == 1, "The card does not hold exactly one such file")
    return found[0], card[found[0] * BLOCK : (found[0] + 1) * BLOCK]


def run(args: argparse.Namespace) -> int:
    capture = args.capture
    snapshots = SnapshotReader(snapshot_path(capture / "instruction-trace.jsonl"))
    interrupts = (("tick-entry", "tick-exit"), ("dispatch-entry", "dispatch-exit"))
    calls = pairs(capture, "serialize-entry", "serialize-exit", interrupts)
    require(len(calls) == 1, "Expected one payload build in the capture")
    entry_row = calls[0][0]
    entry, scratch, io = snapshots.read(entry_row)
    registers = visible_registers(entry_row)
    digit = registers[6] & 0xFF
    card_port = registers[5]
    name = b"BASLUS-00664" + bytes([(ord("0") + digit) & 0xFF])
    card = args.card.read_bytes()
    frame, expected = card_file(card, name)
    with tempfile.TemporaryDirectory(dir=ROOT / ".local") as directory:
        work = Path(directory)
        (work / "ram.bin").write_bytes(entry)
        (work / "scratch.bin").write_bytes(scratch)
        (work / "io.bin").write_bytes(io)
        for empty in ("field.bin", "overlay.bin", "resources.txt"):
            (work / empty).write_bytes(b"")
        process = subprocess.run(
            [
                str(args.runner),
                "menu_save_file",
                "1000000",
                "",
                str(work / "ram.bin"),
                str(work / "scratch.bin"),
                str(work / "field.bin"),
                str(work / "overlay.bin"),
                str(work / "resources.txt"),
                ",".join(f"{value:x}" for value in gte_controls(entry_row)),
                ",".join(f"{value:x}" for value in registers),
                str(work / "io.bin"),
            ],
            capture_output=True,
            timeout=60,
            check=False,
        )
    report = json.loads(process.stdout)
    require(report["status"] == "completed_boundary", f"Runner stopped: {report['reason']}")
    computed = bytes.fromhex(report["entry_result"]["block"])
    differing = [i for i in range(BLOCK) if i >= len(computed) or computed[i] != expected[i]]
    summary = {
        "capture": str(capture),
        "card": str(args.card),
        "card_sha256": hashlib.sha256(card).hexdigest(),
        "runner_sha256": file_sha256(args.runner),
        "entry_frontend_run": entry_row["frontend_run"],
        "card_port": card_port,
        "file_name": name.decode(),
        "directory_frame": frame,
        "computed_sha256": hashlib.sha256(computed).hexdigest(),
        "original_sha256": hashlib.sha256(expected).hexdigest(),
        "bytes": BLOCK,
        "differing_bytes": len(differing),
        "first_difference": hex(differing[0]) if differing else None,
        "tolerance": "exact",
    }
    text = json.dumps(summary, indent=1)
    if args.report:
        require(not args.report.exists(), "Reports are never overwritten")
        args.report.write_text(text + "\n")
    print(text)
    return 0 if not differing and len(computed) == BLOCK else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--card", type=Path, required=True, help="Card image the capture saved")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/debug/xem-memory-runner")
    parser.add_argument("--report", type=Path)
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
