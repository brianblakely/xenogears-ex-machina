#!/usr/bin/env python3
"""Explore actual player input with block feedback and replayable failures.

Run in nix/runtime with a --coverage game build and its xem-headless binary.
Only the seed may set up a scenario; exploration uses pad input and the
existing assisted-walking controls (which also drive the pad).
Results distinguish legal play from scenario starts. No run proves all paths.
All states, logs and reports must stay in an ignored local output directory.
"""

import argparse
import hashlib
import json
import os
import random
import select
import selectors
import struct
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import game_frontier  # noqa: E402
from game_coverage import HEADER  # noqa: E402


class Failure(RuntimeError):
    def __init__(self, kind, detail, command):
        super().__init__(detail)
        self.kind, self.detail, self.command = kind, detail, command

    def record(self):
        return {"kind": self.kind, "detail": self.detail, "command": self.command}


class Client:
    """One isolated process; watchdog covers even a loop within one frame."""

    def __init__(self, command, stderr, timeout=30):
        self.timeout = timeout
        self.process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr, bufsize=0
        )
        os.set_blocking(self.process.stdin.fileno(), False)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)
        self.buffer = b""

    def __call__(self, command):
        if command.get("cmd") == "assert_memory_hash":
            actual = self({"cmd": "memory_hash"})
            if actual != command["expected"]:
                raise Failure("seed_hash", f"expected {command['expected']}, got {actual}", command)
            return actual
        deadline = time.monotonic() + self.timeout
        try:
            data = (json.dumps(command) + "\n").encode()
            # FileIO.write may write only part of a long command.
            while data:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not select.select([], [self.process.stdin], [], remaining)[1]:
                    raise Failure("timeout", f"command not sent within {self.timeout}s", command)
                n = self.process.stdin.write(data)
                if n:
                    data = data[n:]
        except (BrokenPipeError, OSError) as error:
            raise Failure(
                "process_exit", f"{error}; exit={self.process.poll()}", command
            ) from error
        while b"\n" not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise Failure("timeout", f"no reply within {self.timeout}s", command)
            chunk = os.read(self.process.stdout.fileno(), 65536)
            if not chunk:
                raise Failure("process_exit", f"stdout closed; exit={self.process.poll()}", command)
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        try:
            reply = json.loads(line)
        except ValueError as error:
            raise Failure("protocol", line[:500].decode(errors="replace"), command) from error
        if isinstance(reply, dict):
            if reply.get("error"):
                raise Failure("command_error", str(reply["error"]), command)
            status = reply.get("status", reply)
            for key in ("query_trap", "stopped"):
                if status.get(key):
                    raise Failure(key, str(status[key]), command)
            if reply.get("blocked"):
                raise Failure("blocked", "native frame did not finish", command)
            if command.get("cmd") == "run_until" and reply.get("met") is False:
                raise Failure("condition_not_met", "run_until exhausted its frame budget", command)
        return reply

    def close(self):
        self.selector.close()
        self.process.stdin.close()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdout.close()


def save_json(path, value):
    temporary = path.with_suffix(path.suffix + ".new")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def read_coverage(client, manifest, clear=False):
    address, count = manifest["address"], len(manifest["blocks"])
    signature = client({"cmd": "read_memory", "address": address, "length": HEADER})
    if signature != manifest["signature"]:
        raise ValueError(
            "coverage map does not match the running module; "
            "rebuild xem-headless with XEM_GAME_WASM"
        )
    hits = set()
    for offset in range(0, count, 16384):
        length = min(16384, count - offset)
        at = address + HEADER + offset
        data = bytes.fromhex(client({"cmd": "read_memory", "address": at, "length": length}))
        if len(data) != length or any(value not in (0, 1) for value in data):
            raise ValueError("invalid coverage bytes (wrong map or overwritten instrumentation)")
        hits.update(offset + i for i, value in enumerate(data) if value)
        if clear:
            client({"cmd": "write_memory", "address": at, "hex": "00" * length})
    return hits


SCRIPT_FEATURE = 1 << 63


def read_scripts(client, manifest, clear=False):
    layout = manifest.get("field_scripts")
    if layout is None:
        return set()
    at = layout["header"]
    header = bytes.fromhex(client({"cmd": "read_memory", "address": at, "length": 12}))
    epoch, count, overflow = struct.unpack("<III", header)
    if overflow or count > layout["capacity"]:
        raise ValueError(f"field telemetry overflow ({overflow}, {count}); shorten input batches")
    hits = set()
    for offset in range(0, count * 8, 16384):
        length = min(16384, count * 8 - offset)
        data = bytes.fromhex(client({"cmd": "read_memory", "address": layout["queue"] + offset,
                                    "length": length}))
        for (key,) in struct.iter_unpack("<Q", data):
            if key >> 45 not in (1, 2) or key & 0x1FFFF > 0x10000:
                raise ValueError("invalid field telemetry key")
            hits.add(key)
    if len(hits) != count:
        raise ValueError("duplicate field telemetry keys")
    if clear:
        if epoch == 0xFFFFFFFF:
            raise ValueError("field telemetry epoch exhausted; restart the worker")
        client({"cmd": "write_memory", "address": at,
                "hex": struct.pack("<III", epoch + 1, 0, 0).hex()})
    return hits


def feedback(client, manifest):
    blocks = read_coverage(client, manifest, clear=True)
    scripts = read_scripts(client, manifest, clear=True)
    return blocks | {SCRIPT_FEATURE | key for key in scripts}


def script_summary(hits):
    groups = defaultdict(lambda: {"instructions": 0, "outcomes": 0})
    for key in hits:
        disc, map_id = key >> 45, key >> 33 & 0xFFF
        group = groups[f"disc{disc}/field{map_id}"]
        group["instructions" if key & 0x1FFFF == 0x10000 else "outcomes"] += 1
    return dict(sorted(groups.items()))


def seed_commands(args):
    if args.field is not None:
        return (
            "scenario",
            [
                {"cmd": "enter_field", "map": args.field},
                {"cmd": "frames", "count": args.seed_frames},
            ],
            None,
        )
    if args.log:
        log = json.loads(args.log.read_text())
        frame = 0
        legal = True
        for at, command in log["commands"]:
            if at < frame or at > log["frames"]:
                raise ValueError("log commands must be ordered within its frame count")
            frame = at
            # A scenario's memory/setup edits cannot count as legal progression.
            legal &= command["cmd"] in {
                "pad", "press", "hold", "act", "connect_pad", "assist", "assist_stop", "shop_quantity"
            }
        # Use the runtime's scheduler: enter_field may itself
        # advance frames, so subtracting timestamps in a second player is wrong.
        ordered = [{"cmd": "schedule", "commands": log["commands"]}]
        for end in range(600, log["frames"] + 600, 600):
            ordered.append({"cmd": "run_until", "max_frames": 600,
                            "until": {"frames": min(end, log["frames"])}})
        return "legal" if legal else "scenario", ordered, log.get("memory_hash")
    # Reuse the qualified new-game input route, including releases.
    inputs = json.loads((ROOT / "tests/reference-inputs/new-game-movie-skip.json").read_text())
    times = sorted(
        {0, args.seed_frames}
        | {t for item in inputs for t in (item["start"], item["end"]) if t < args.seed_frames}
    )
    ordered = []
    for start, end in zip(times[:-1], times[1:], strict=True):
        buttons = sorted(
            {b for item in inputs if item["start"] <= start < item["end"] for b in item["buttons"]}
        )
        ordered += [{"cmd": "pad", "buttons": buttons}, {"cmd": "frames", "count": end - start}]
    return "legal", ordered, None


def choose_input(context, rng):
    """Favor useful input for the current interface, still try unusual input."""
    directions = [[name] for name in ("up", "down", "left", "right")]
    kind = context["kind"]
    if kind in ("walk", "world_map", "arena"):
        choices = (
            directions * 3
            + [d + ["cross"] for d in directions]
            + [[], ["cross"], ["circle"], ["square"], ["start"], ["l1"], ["r1"]]
        )
        duration = rng.choice([1, 2, 8, 30, 60, 120])
    elif kind == "dialogue":
        choices = [[], ["cross"], ["cross"], ["circle"]]
        if context.get("choice"):
            choices += directions * 2
        duration = rng.choice([1, 2, 8, 30, 90])
    elif kind == "event":
        # The script's own pad test mask, plus ordinary controls.
        bits = {
            0x10: "triangle",
            0x20: "circle",
            0x40: "cross",
            0x80: "square",
            0x1000: "up",
            0x2000: "right",
            0x4000: "down",
            0x8000: "left",
        }
        choices = [[], ["cross"], ["circle"]] + [
            [name] for bit, name in bits.items() if context.get("buttons", 0) & bit
        ]
        duration = rng.choice([2, 8, 30, 120, 300])
    else:
        choices = [
            [],
            ["cross"],
            ["cross"],
            ["circle"],
            ["square"],
            ["triangle"],
            ["start"],
            ["l1"],
            ["r1"],
        ] + directions
        duration = rng.choice([1, 2, 8, 30, 120])
    buttons = rng.choice(choices)
    if rng.randrange(10) == 0:
        buttons = rng.sample(
            [
                "up",
                "down",
                "left",
                "right",
                "cross",
                "circle",
                "square",
                "triangle",
                "start",
                "select",
                "l1",
                "r1",
                "l2",
                "r2",
            ],
            rng.randrange(4),
        )
    return [
        {"cmd": "pad", "buttons": buttons},
        {"cmd": "frames", "count": duration},
        {"cmd": "pad", "buttons": []},
        {"cmd": "frames", "count": 2},
    ]


def coverage_summary(manifest, hits):
    groups = defaultdict(
        lambda: {
            "blocks_hit": 0,
            "blocks_total": 0,
            "functions_hit": set(),
            "functions_total": set(),
        }
    )
    for i, block in enumerate(manifest["blocks"]):
        group = groups[block["image"]]
        key = (block["source"], block["function"])
        group["blocks_total"] += 1
        group["functions_total"].add(key)
        if i in hits:
            group["blocks_hit"] += 1
            group["functions_hit"].add(key)
    return {
        image: {
            key: len(value) if isinstance(value, set) else value for key, value in group.items()
        }
        for image, group in sorted(groups.items())
    }


def progress_key(client):
    """Preserve travel/story progress even when it uses already-covered code.

    These are exploration hints, never reported as code or script coverage.
    Whole-RAM hashes would retain every ticking animation/RNG state forever.
    """
    context = client({"cmd": "context"})
    mode = client({"cmd": "readiness"})["mode"]
    place = {"mode": mode, "context": context}
    if mode == 1:
        place["map"] = client({"cmd": "inspect", "path": "mode_field_map_id", "depth": 0})
        place["story"] = client({"cmd": "inspect", "path": "field_event_variables[0]", "depth": 0})
        if context["kind"] == "walk":
            field = client({"cmd": "field"})
            player = next((a for a in field["actors"] if a["index"] == field["player"]), None)
            if player:
                place["cell"] = [int(player["position"][axis] // 128) for axis in ("x", "z")]
    return json.dumps(place, sort_keys=True), context


def command_line(args):
    command = [str(args.headless.resolve()), "--disc", str(args.disc.resolve()), "--control"]
    if args.stubs:
        command += ["--stubs", str(args.stubs.resolve())]
    return command


def replay(args):
    case = json.loads(args.replay.read_text())
    if "cases" in case:
        results = []
        for path in case["cases"]:
            child = argparse.Namespace(**{**vars(args), "replay": args.replay.parent / path,
                                          "from_boot": True})
            results.append(replay(child))
        print(json.dumps({"cases": len(results), "passed": results.count(0)}))
        return 0 if all(result == 0 for result in results) else 2
    if case.get("requires_from_boot") and not args.from_boot:
        raise ValueError("regression witnesses need --from-boot (fast snapshots may be evicted)")
    if case.get("disc"):
        args = argparse.Namespace(**{**vars(args), "disc": Path(case["disc"])})
    commands = case["commands"]
    if args.from_boot:
        commands = case["prefix"] + commands
    else:
        state = args.replay.parent / case["state"]
        commands = [{"cmd": "state_load", "path": str(state.resolve())}] + commands
    client = Client(command_line(args), sys.stderr, args.timeout)
    try:
        for command in commands:
            client(command)
        print(json.dumps({"reproduced": False, "completed": True,
                          "memory_hash": client({"cmd": "memory_hash"})}))
        return 0
    except Failure as error:
        expected = case.get("failure", {})
        same = error.kind == expected.get("kind") and error.detail == expected.get("detail")
        print(json.dumps({"reproduced": same, "failure": error.record()}))
        return 1 if same else 2
    finally:
        client.close()


def explore(args):
    manifest = json.loads(args.coverage.read_text())
    if manifest.get("format") != 1:
        raise ValueError("unsupported coverage map")
    kind, seed, expected_hash = seed_commands(args)
    if expected_hash:
        seed.append({"cmd": "assert_memory_hash", "expected": expected_hash})
    # Reusing an output could overwrite the only reproducer of an old failure.
    args.out.mkdir(parents=True, exist_ok=False)
    rng, hits, corpus, places = random.Random(args.seed), set(), [], set()
    scripts = set()
    began, frames, cases, failures = time.monotonic(), 0, 0, []
    stderr = (args.out / "stderr.log").open("w")
    client = Client(command_line(args), stderr, args.timeout)
    save_json(
        args.out / "run.json",
        {
            "module_sha256": manifest["module_sha256"],
            "coverage_signature": manifest["signature"],
            "headless_sha256": hashlib.sha256(args.headless.read_bytes()).hexdigest(),
            "disc": str(args.disc.resolve()),
            "seed": args.seed,
            "origin": kind,
            "seed_commands": seed,
            "options": {
                key: str(value) if isinstance(value, Path) else value
                for key, value in vars(args).items()
            },
        },
    )
    current = {"state": "root.xemstate", "prefix": [], "commands": [], "origin": kind}
    failure, completed = None, False
    try:
        # Fail early on a stale/wrong binary before executing the seed.
        read_coverage(client, manifest)
        read_scripts(client, manifest, clear=True)
        client({"cmd": "state_save", "path": str((args.out / current["state"]).resolve())})
        current["commands"] = seed
        save_json(args.out / "pending.json", current)
        for command in seed:
            client(command)
        hits |= read_coverage(client, manifest, clear=True)
        scripts |= read_scripts(client, manifest, clear=True)
        place, context = progress_key(client)
        places.add(place)
        prefix = list(seed)
        stale, novel = 0, True
        deadline = time.monotonic() + args.seconds if args.seconds else float("inf")
        for iteration in range(args.iterations):
            if time.monotonic() >= deadline:
                break
            if novel:
                # Keep the entire legal/scenario lineage as well as the fast
                # snapshot replay. Replaying from boot also works on a normal build.
                state = f"state-{iteration:06d}.xemstate"
                client({"cmd": "state_save", "path": str((args.out / state).resolve())})
                entry = {"state": state, "prefix": list(prefix), "commands": [], "origin": kind}
                save_json(args.out / f"state-{iteration:06d}.json", entry)
                corpus.append(entry)
                if len(corpus) > args.max_corpus:
                    # Keep the root; evict a random other frontier, bounded disk.
                    evicted = corpus.pop(rng.randrange(1, len(corpus) - 1))
                    (args.out / evicted["state"]).unlink()
                    (args.out / evicted["state"]).with_suffix(".json").unlink()
            elif stale >= args.stall and context["kind"] in (
                "walk",
                "menu",
                "world_map",
                "arena",
                "battle",
            ):
                entry = rng.choice(corpus)
                client({"cmd": "state_load", "path": str((args.out / entry["state"]).resolve())})
                prefix = list(entry["prefix"])
                place, context = progress_key(client)
                stale = 0
            # Retain all input since this checkpoint, including no-new-code steps.
            current = {**entry, "commands": list(prefix[len(entry["prefix"]) :])}
            commands = choose_input(context, rng)
            current["commands"] += commands
            save_json(args.out / "pending.json", current)
            for command in commands:
                client(command)
            prefix += commands
            frames += sum(
                command.get("count", 0) for command in commands if command["cmd"] == "frames"
            )
            reached = read_coverage(client, manifest, clear=True)
            scripts |= read_scripts(client, manifest, clear=True)
            place, context = progress_key(client)
            novel = bool(reached - hits) or place not in places
            hits |= reached
            places.add(place)
            stale = 0 if novel else stale + 1
            cases += 1
            if cases % 25 == 0:
                print(
                    f"{cases} inputs, {frames} exploration frames, "
                    f"{len(hits)}/{len(manifest['blocks'])} blocks, {len(corpus)} checkpoints",
                    flush=True,
                )
        completed = True
    except Failure as error:
        failure = error
        if error.kind not in ("timeout", "process_exit", "protocol"):
            try:
                hits |= read_coverage(client, manifest)
            except (Failure, ValueError):
                pass  # The saved input and original failure remain authoritative.
        current["failure"] = error.record()
        if error.command not in current["commands"]:
            current["commands"].append(error.command)
        save_json(args.out / "failure.json", current)
        failures.append(error.record())
        print(f"{kind} {error.kind}: {error.detail}", file=sys.stderr, flush=True)
    finally:
        client.close()
        stderr.close()
        save_json(args.out / "coverage-hits.json", sorted(hits))
        save_json(args.out / "coverage-script-hits.json", sorted(scripts))
        save_json(
            args.out / "summary.json",
            {
                "origin": kind,
                "completed": completed,
                "inputs": cases,
                "exploration_frames": frames,
                "seconds": time.monotonic() - began,
                "failures": failures,
                "metric": manifest["metric"],
                "blocks_hit": len(hits),
                "blocks_total": len(manifest["blocks"]),
                "places": len(places),
                "last_place": json.loads(place) if places else None,
                "images": coverage_summary(manifest, hits),
                "field_scripts": script_summary(scripts),
            },
        )
    return 1 if failure else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--headless", type=Path, required=True)
    parser.add_argument("--disc", type=Path, required=True)
    parser.add_argument("--stubs", type=Path)
    parser.add_argument("--coverage", type=Path, default=ROOT / "build/game-coverage/coverage.json")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--iterations", type=int, default=1000)
    parser.add_argument("--seed-frames", type=int, default=4500)
    parser.add_argument("--max-corpus", type=int, default=64)
    parser.add_argument("--stall", type=int, default=50)
    parser.add_argument("--strategy", choices=("frontier", "random"), default="frontier",
                        help="targeted checkpoint branches, or a random-input control experiment")
    parser.add_argument("--add-field", type=int, action="append", default=[],
                        help="add another field scenario to the shared frontier")
    parser.add_argument("--add-log", type=Path, action="append", default=[],
                        help="add another retained input log to the shared frontier")
    parser.add_argument("--resume", type=Path, action="append", default=[],
                        help="import retained frontier checkpoints and measured replay witnesses")
    parser.add_argument("--add-replay", type=Path, action="append", default=[],
                        help="reseed from a replay witness/suite, remeasuring it on this build")
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--seconds", type=float, help="exploration time budget, after seed setup")
    parser.add_argument("--settle-frames", type=int, default=900,
                        help="advance non-choice dialogue/events within each frontier branch")
    start = parser.add_mutually_exclusive_group()
    start.add_argument(
        "--field", type=int, help="debug scenario start; never reported as legal progression"
    )
    start.add_argument(
        "--log", type=Path, help="existing session log (scenario classification is automatic)"
    )
    start.add_argument("--replay", type=Path, help="a saved failure.json or pending.json")
    parser.add_argument(
        "--from-boot",
        action="store_true",
        help="replay full input lineage; also works with an uninstrumented binary",
    )
    args = parser.parse_args()
    if (
        args.iterations < 1
        or args.max_corpus < 2
        or args.stall < 1
        or args.timeout <= 0
        or args.seed_frames < 0
        or (args.seconds is not None and args.seconds <= 0)
        or args.settle_frames < 0
    ):
        parser.error("positive iteration/stall/timeout, max-corpus >= 2, seed-frames >= 0 required")
    if args.replay:
        return replay(args)
    if args.strategy == "random" and (args.add_field or args.add_log or args.resume or args.add_replay):
        parser.error("the random control experiment takes one seed; use --strategy frontier")
    if args.out is None:
        parser.error("--out is required for exploration")
    # Private artifacts are not source: constrain outputs to ignored local roots.
    if not any(
        args.out.resolve().is_relative_to(ROOT / root) for root in ("build", ".local", "captures")
    ):
        parser.error("--out must be under build/, .local/ or captures/")
    return game_frontier.run(args) if args.strategy == "frontier" else explore(args)


if __name__ == "__main__":
    raise SystemExit(main())
