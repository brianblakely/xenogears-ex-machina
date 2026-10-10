#!/usr/bin/env python3
"""A direct agent that walks the field by the game's own state.

    python3 tools/field_agent.py OUT_DIR --scenario SCENARIO.json --until STEP
        --path CAPTURE_DIR --then FIRST_STEP [--face-actor N] [--card2 unformatted]

It runs the scenario's program (compiled as tools/reference/scenario.py does)
through step STEP on `xem-headless --control`, then walks the path the
original took in the capture CAPTURE_DIR (an observe.py --sample-memory trace
of the party leader's position, `fei`, and the field map, `map`): it reads
the leader's position through the schema (`field_view.components.descriptors[N].matrix.t`),
learns which way each direction moves him under the map's camera, and holds
the direction that points best at the next waypoint, whatever the frame.
Battles on the way are fought (Cross, as tools/native_scenario.py does).
At the last waypoint it walks into actor N (--face-actor: a save point), then
plays the program's steps from FIRST_STEP on (the save point's menu inputs).
Nothing is written to game memory but the program's own setup writes; the
agent only presses buttons.
"""

import argparse
import json
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "reference"))
import native_scenario as ns  # noqa: E402
from scenario import compile_scenario  # noqa: E402

DIRECTIONS = {
    ("up",): None,
    ("down",): None,
    ("left",): None,
    ("right",): None,
    ("up", "left"): None,
    ("up", "right"): None,
    ("down", "left"): None,
    ("down", "right"): None,
}
REACHED = 48  # units from a waypoint that count as there
STUCK_FRAMES = 90


class Agent:
    def __init__(self, headless, out):
        self.h, self.out, self.frames, self.battles = headless, out, 0, []

    def frame(self, buttons):
        self.h({"cmd": "pad", "port": 0, "buttons": list(buttons)})
        self.h({"cmd": "frames", "count": 1})
        self.frames += 1
        if self.h({"cmd": "readiness"}).get("mode") == ns.MODE_BATTLE:
            self.fight()

    def fight(self):
        started, settle = self.frames, 0
        while settle < ns.BATTLE_SETTLE_FRAMES:
            mode = self.h({"cmd": "readiness"}).get("mode")
            settle = settle + 1 if mode == ns.MODE_FIELD else 0
            cross = ["cross"] if self.frames % 64 < 8 and mode == ns.MODE_BATTLE else []
            self.h({"cmd": "pad", "port": 0, "buttons": cross})
            self.h({"cmd": "frames", "count": 1})
            self.frames += 1
        self.battles.append({"started": started, "ended": self.frames})

    def leader(self):
        index = self.h({"cmd": "inspect", "path": "mode_party_actors[0]", "depth": 0})
        x, _, z = self.h(
            {
                "cmd": "inspect",
                "path": f"field_view.components.descriptors[{index}].matrix.t",
                "depth": 1,
            }
        )
        return x, z

    def learn_directions(self):
        """How far each direction moves the leader in 8 frames, under this camera
        (a message still open is closed with Cross first)."""
        for _ in range(20):
            before = self.leader()
            for _ in range(8):
                self.frame(("right",))
            if self.leader() != before:
                break
            for keys in (("cross",),) + ((),) * 30:
                self.frame(keys)
        moves = {}
        for keys in DIRECTIONS:
            before = self.leader()
            for _ in range(8):
                self.frame(keys)
            for _ in range(4):
                self.frame(())
            after = self.leader()
            moves[keys] = (after[0] - before[0], after[1] - before[1])
        return moves

    def walk(self, waypoints, moves):
        """Hold the direction pointing best at each waypoint until it is reached.
        Each direction's movement is relearned from what it does (the camera
        can change within a map)."""
        target, best_distance, since_best = 0, math.inf, 0
        log = []
        position = self.leader()
        while target < len(waypoints):
            x, z = position
            tx, tz = waypoints[target]
            dx, dz = tx - x, tz - z
            distance = math.hypot(dx, dz)
            if distance < REACHED or since_best > STUCK_FRAMES:
                log.append(
                    {
                        "waypoint": target,
                        "reached": distance < REACHED,
                        "at": position,
                        "frame": self.frames,
                    }
                )
                target, best_distance, since_best = target + 1, math.inf, 0
                continue
            if distance < best_distance - 4:
                best_distance, since_best = distance, 0
            else:
                since_best += 1
            keys = max(moves, key=lambda k: alignment(moves[k], dx, dz))
            self.frame(keys)
            after = self.leader()
            step = (after[0] - x, after[1] - z)
            if step != (0, 0):
                mx, mz = moves[keys]
                moves[keys] = (0.7 * mx + 0.3 * step[0] * 8, 0.7 * mz + 0.3 * step[1] * 8)
            position = after
        for _ in range(30):
            self.frame(())
        return log


def approach(agent, moves, actor):
    """Walk into actor `actor` (its descriptor's position) until stopped by it,
    so the leader faces it."""
    best, since = math.inf, 0
    while since < 20:
        x, z = agent.leader()
        tx, _, tz = agent.h(
            {
                "cmd": "inspect",
                "path": f"field_view.components.descriptors[{actor}].matrix.t",
                "depth": 1,
            }
        )
        dx, dz = tx - x, tz - z
        distance = math.hypot(dx, dz)
        if distance < best - 1:
            best, since = distance, 0
        else:
            since += 1
        agent.frame(max(moves, key=lambda k: alignment(moves[k], dx, dz)))
    for _ in range(20):
        agent.frame(())
    return best


def alignment(move, dx, dz):
    """How much a direction's movement points along (dx, dz)."""
    norm = math.hypot(*move) or 1
    return (move[0] * dx + move[1] * dz) / norm


def waypoints_from_trace(trace, field_map, spacing=96):
    points, final = [], None
    for line in open(trace):
        record = json.loads(line)
        ranges = {r["name"]: r for r in record["ranges"]}
        if ranges["fei"].get("hex") is None or ranges["fei"]["hex"] == "00" * 12:
            continue
        if int.from_bytes(bytes.fromhex(ranges["map"]["hex"]), "little") != field_map:
            continue
        x, _, z = struct.unpack("<iii", bytes.fromhex(ranges["fei"]["hex"]))
        final = (x, z)
        if not points or math.hypot(x - points[-1][0], z - points[-1][1]) >= spacing:
            points.append(final)
    if final and points[-1] != final:
        points.append(final)
    return points


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("out", type=Path)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--until", required=True, help="last program step run before walking")
    parser.add_argument(
        "--path", type=Path, required=True, help="capture with a memory-trace.jsonl"
    )
    parser.add_argument("--map", type=int, default=22)
    parser.add_argument("--then", required=True, help="first program step run at the last waypoint")
    parser.add_argument("--card2")
    parser.add_argument("--log", type=Path, help="write the session log (schedule's commands) here")
    parser.add_argument("--face-actor", type=int, help="walk into this actor before FIRST_STEP")
    args = parser.parse_args()

    availability = json.loads((ROOT / "analysis/coverage/field-pairs.json").read_text())
    program, _ = compile_scenario(json.loads(args.scenario.read_text()), availability)
    names = [step["name"] for step in program["steps"]]
    head = {**program, "steps": program["steps"][: names.index(args.until) + 1]}
    tail = {**program, "steps": program["steps"][names.index(args.then) :]}

    cards = args.out / "cards"
    cards.mkdir(parents=True, exist_ok=True)
    if args.card2:
        image = bytes(0x20000) if args.card2 == "unformatted" else Path(args.card2).read_bytes()
        (cards / "card2.mcd").write_bytes(image)
    headless = ns.Headless(
        [
            str(ns.ROOT / "src/runtime/target/release/xem-headless"),
            "--disc",
            str(ns.ROOT / "discs/Xenogears disc 1.chd"),
            "--control",
            "--cards",
            str(cards),
        ],
        log=args.log is not None,
    )
    agent = Agent(headless, args.out)
    events, _ = ns.run(head, headless, args.out, fight=True)
    agent.frames = events[-1]["completed_frame"] + 1
    print("after the program's head: leader at", agent.leader(), "frame", agent.frames)
    moves = agent.learn_directions()
    print("after learning the directions: leader at", agent.leader(), "frame", agent.frames)
    waypoints = waypoints_from_trace(args.path / "memory-trace.jsonl", args.map)
    walk = agent.walk(waypoints, moves)
    print("walked to", agent.leader(), "last waypoint", waypoints[-1], "battles", agent.battles)
    if args.face_actor is not None:
        print("stopped", approach(agent, moves, args.face_actor), "from actor", args.face_actor)
    tail_events, captures = ns.run(tail, headless, args.out, fight=True)
    headless({"cmd": "screenshot", "path": str(args.out / "end.png")})
    if args.log:
        headless.save_log(args.log)
    headless.close()
    report = {
        "moves": {"+".join(k): v for k, v in moves.items()},
        "waypoints": waypoints,
        "walk": walk,
        "battles": agent.battles,
        "captures": captures,
    }
    (args.out / "agent.json").write_text(json.dumps(report, indent=1) + "\n")
    print(json.dumps(report)[:600])


if __name__ == "__main__":
    main()
