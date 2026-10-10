#!/usr/bin/env python3
"""Run an original-environment scenario program on the native runtime and
compare its state with the original capture.

    python3 tools/native_scenario.py CAPTURE_DIR OUT_DIR [--disc DISC] [--headless BIN]

CAPTURE_DIR is a tools/reference/observe.py capture made with --program: its
observation.json holds the program, the frames each step started and
completed, and a RAM image at every labelled capture. The same executor
(tools/reference/scenario_program.py) runs the same program against
`xem-headless --control`: conditions read game RAM, writes are guarded by the
same fingerprints, buttons are held for the same frames, and RAM is saved at
the same labels.

The one write the compiled program cannot take is a patch of code: the
reference makes main's mode_select_next_mode(6) select the Kernel MENU (mode
0) by patching that call's argument. The runtime makes the same change at the
same point through `boot_mode` (main's mode_dispatch restart, before the
dispatcher reads mode_next_mode).

Reported per capture: the frame (reference/native), whether game_data,
the RNG seed and the field event variables equal the reference's, game_data
again without the name padding the original filled from uninitialized stack
(without_name_padding), and the number of differing RAM bytes. The step
timing (completed frames) is compared for every step. Nothing is written into
game state beyond the program's own writes.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "reference"))
from scenario_program import ScenarioProgram  # noqa: E402

RAM = 0x200000
KSEG0 = 0x80000000
# Code patches the reference applies, with the runtime's equivalent command.
CODE_PATCHES = {(0x19930, "00000434"): {"cmd": "boot_mode", "mode": 0}}
# Original state compared at each capture: (name, schema global, image that
# must be loaded for it to be meaningful or None).
STATE = [
    ("game_data", "game_data", None),
    ("rng", "libc_rand_seed", None),
    ("event_variables", "field_event_variables", "field"),
]


class Headless:
    def __init__(self, command):
        self.process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True
        )

    def __call__(self, command):
        self.process.stdin.write(json.dumps(command) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(f"xem-headless exited at {command}")
        reply = json.loads(line)
        if isinstance(reply, dict) and reply.get("error"):
            raise RuntimeError(f"{command}: {reply['error']}")
        return reply.get("ok", reply) if isinstance(reply, dict) else reply

    def read(self, offset, length):
        data = b""
        while length:
            n = min(length, 0x10000)
            data += bytes.fromhex(
                self({"cmd": "read_memory", "address": KSEG0 + offset, "length": n})
            )
            offset, length = offset + n, length - n
        return data

    def close(self):
        self.process.stdin.close()
        self.process.wait()


class RemoteMemory:
    """Game RAM read on demand: the executor reads its conditions' words."""

    def __init__(self, headless):
        self.headless = headless

    def __len__(self):
        return RAM

    def __getitem__(self, key):
        return self.headless.read(key.start, key.stop - key.start)

    def __iter__(self):
        # The executor copies the whole image after its writes.
        return iter(self.headless.read(0, RAM))


def without_name_padding(ram, offset, length):
    """game_data with the bytes after each name's terminator zeroed.

    mode_load_initial_game_data decodes every name into an uninitialized
    20-byte local and copies all of it: past the terminator the names hold
    whatever the PS1 stack held there, which the port's stack (outside PS1
    RAM) cannot reproduce. The game reads names only up to the terminator.
    """
    data = bytearray(ram[offset : offset + length])
    for slot in range(31):
        name = data[slot * 20 : slot * 20 + 20]
        end = name.find(0)
        if end >= 0:
            data[slot * 20 + end + 1 : slot * 20 + 20] = bytes(19 - end)
    return bytes(data)


def state_regions(schema_path):
    schema = json.loads(Path(schema_path).read_text())
    types = schema["types"]

    def size(index):
        ty = types[index]
        while ty["kind"] == "typedef":
            ty = types[ty["type"]]
        if ty["kind"] == "array":
            return size(ty["of"]) * ty["count"]
        return ty.get("size", 4)

    regions = []
    for label, name, image in STATE:
        entry = schema["globals"][name]
        length = size(entry["type"]) if entry.get("type") is not None else 4
        regions.append((label, entry["address"] - KSEG0, length, image))
    return regions


def run(program_json, headless, out):
    program = ScenarioProgram(program_json)
    memory = RemoteMemory(headless)
    out.mkdir(parents=True, exist_ok=True)
    captures = {}
    frame = 0
    while True:

        def write(offset, data):
            patch = CODE_PATCHES.get((offset, data.hex()))
            if patch:
                headless(patch)
            else:
                headless({"cmd": "write_memory", "address": KSEG0 + offset, "hex": data.hex()})

        labels = program.tick(frame, memory, write)
        if frame and labels:
            ram = headless.read(0, RAM)
            (out / f"ram-{frame:06d}.bin").write_bytes(ram)
            headless({"cmd": "screenshot", "path": str(out / f"frame-{frame:06d}.png")})
            for label in labels:
                captures[label] = frame
        if program.complete:
            break
        headless({"cmd": "pad", "port": 0, "buttons": program.buttons})
        reply = headless({"cmd": "frames", "count": 1})
        if isinstance(reply, dict) and reply.get("blocked"):
            raise RuntimeError("a native disc read blocked")
        frame += 1
    return program.events, captures


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("capture", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--disc", type=Path, default=ROOT / "discs" / "Xenogears disc 1.chd")
    parser.add_argument(
        "--headless",
        type=Path,
        default=ROOT / "src" / "runtime" / "target" / "release" / "xem-headless",
    )
    parser.add_argument("--schema", type=Path, default=ROOT / "build" / "game" / "schema.json")
    parser.add_argument(
        "--compare-only", action="store_true", help="compare an earlier run in OUT_DIR"
    )
    args = parser.parse_args()

    observation = json.loads((args.capture / "observation.json").read_text())
    reference = observation["scenario"]
    if args.compare_only:
        saved = json.loads((args.out / "run.json").read_text())
        events, captures = saved["events"], saved["captures"]
    else:
        cards = args.out / "cards"
        cards.mkdir(parents=True, exist_ok=True)
        headless = Headless(
            [
                str(args.headless),
                "--disc",
                str(args.disc),
                "--control",
                "--cards",
                str(cards),
                "--schema",
                str(args.schema),
            ]
        )
        try:
            events, captures = run(reference["program"], headless, args.out)
        finally:
            headless.close()
        (args.out / "run.json").write_text(
            json.dumps({"events": events, "captures": captures}) + "\n"
        )

    regions = state_regions(args.schema)
    reference_frames = {}
    for capture in observation["captures"]:
        for label in capture.get("scenario_steps") or []:
            reference_frames[label] = capture["frame"]
    game_data = next((offset, length) for name, offset, length, _ in regions if name == "game_data")
    rows, equal = [], {label: 0 for label, *_ in regions}
    equal["game_data_except_name_padding"] = 0
    for label, frame in captures.items():
        theirs_frame = reference_frames.get(label)
        theirs = (
            (args.capture / f"ram-{theirs_frame:06d}.bin").read_bytes() if theirs_frame else None
        )
        ours = (args.out / f"ram-{frame:06d}.bin").read_bytes()
        row = {"step": label, "frame": frame, "reference_frame": theirs_frame}
        if theirs:
            row["ram_bytes_differing"] = sum(a != b for a, b in zip(ours, theirs, strict=True))
            for name, offset, length, _ in regions:
                same = ours[offset : offset + length] == theirs[offset : offset + length]
                row[name] = same
                equal[name] += same
            same = without_name_padding(ours, *game_data) == without_name_padding(
                theirs, *game_data
            )
            row["game_data_except_name_padding"] = same
            equal["game_data_except_name_padding"] += same
        rows.append(row)
    timing = [
        {
            "step": ours["step"],
            "completed": ours.get("completed_frame"),
            "reference_completed": theirs.get("completed_frame"),
        }
        for ours, theirs in zip(events, reference["events"], strict=False)
    ]
    report = {"captures": rows, "equal_counts": equal, "timing": timing}
    (args.out / "comparison.json").write_text(json.dumps(report, indent=1) + "\n")
    for row in rows:
        names = [name for name, *_ in regions] + ["game_data_except_name_padding"]
        marks = " ".join(f"{name}={'=' if row.get(name) else 'x'}" for name in names)
        print(
            f"{row['step']:24} {row['frame']:6} ref {row['reference_frame']!s:6} "
            f"{marks} ram_diff={row.get('ram_bytes_differing')}"
        )
    late = [t for t in timing if t["completed"] != t["reference_completed"]]
    print(f"{len(rows)} captures; equal: {equal}")
    print(f"steps with different timing: {len(late)}/{len(timing)}")


if __name__ == "__main__":
    main()
