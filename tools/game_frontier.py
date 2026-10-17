"""Input choices and scheduling for the headless gameplay explorer.

All generated actions use the existing pad/assisted-walking controls. Coverage
selects checkpoints, never changes game state or forces a branch outcome.
"""

import argparse
import hashlib
import json
import math
import shutil
import time
from collections import Counter
from pathlib import Path

OPPOSITE = {"Up": "Down", "Down": "Up", "Left": "Right", "Right": "Left"}


def pulse(action, settle=10):
    return [{"cmd": "act", "actions": [action], "frames": 2},
            {"cmd": "frames", "count": settle}]


def menu_actions(targets):
    """Enumerate visible choices, including ring pages and battle attacks.

    Directions are pressed through the game's actual cursor handling. We do
    not write the selection, AP, inventory, battle result, or story flags.
    """
    if not targets:
        return []
    result, selected = [], targets.get("selected")
    steps = targets["steps"]
    for index, entry in enumerate(targets["entries"]):
        commands = []
        if entry.get("press"):
            commands = pulse(entry["press"]) * (2 if entry.get("twice") else 1)
        elif steps["kind"] == "direct" and entry.get("select"):
            commands = pulse(entry["select"])
            if targets["confirm"]:
                commands += pulse("Confirm")
        elif selected is not None and steps["kind"] in ("ring", "grid"):
            if steps["kind"] == "ring":
                count, direction = steps["count"], steps["next"]
                distance = (index - selected) % count
                if distance > count // 2:
                    distance, direction = count - distance, OPPOSITE[direction]
                commands = pulse(direction) * distance
            else:
                columns = steps["columns"]
                dy, dx = index // columns - selected // columns, index % columns - selected % columns
                commands = (pulse("Down" if dy > 0 else "Up") * abs(dy)
                            + pulse("Right" if dx > 0 else "Left") * abs(dx))
            if targets["confirm"]:
                commands += pulse("Confirm")
        else:
            continue  # Compass targets retain the ordinary direction choices below.
        if commands:
            result.append((f"{targets['what']}:{index}", commands + [{"cmd": "frames", "count": 90}]))
    return result


def actions(client, context):
    """Short, repeatable branches from one checkpoint, tried once each."""
    kind = context["kind"]
    if kind == "dialogue" and not context.get("choice"):
        return [("Confirm", pulse("Confirm", 90)),
                ("wait", [{"cmd": "pad", "buttons": []}, {"cmd": "frames", "count": 300}])]
    if kind == "none" or (kind == "event" and not context.get("buttons")):
        return [("wait", [{"cmd": "pad", "buttons": []}, {"cmd": "frames", "count": 300}]),
                ("Confirm", pulse("Confirm", 90)), ("Start", pulse("Start", 90))]
    result = []
    if kind == "walk":
        field = client({"cmd": "field"})
        player = next((a for a in field["actors"] if a["index"] == field["player"]), None)
        if player:
            def distance(actor):
                return sum((actor["position"][axis] - player["position"][axis]) ** 2
                           for axis in ("x", "z"))
            actors = sorted((a for a in field["actors"] if a != player), key=distance)
            for actor in actors[:12]:
                result.append((f"talk:{actor['index']}", [
                    {"cmd": "interact", "actor": actor["index"]},
                    {"cmd": "frames", "count": 240},
                    {"cmd": "assist_stop"}, {"cmd": "pad", "buttons": []},
                    {"cmd": "frames", "count": 2},
                ]))
    if kind in ("dialogue", "menu", "battle"):
        result += menu_actions(client({"cmd": "targets"}))
    if kind in ("walk", "world_map", "arena"):
        directions = [[d] for d in ("Up", "Down", "Left", "Right")]
        directions += [[v, h] for v in ("Up", "Down") for h in ("Left", "Right")]
        for direction in directions:
            result.append(("walk:" + "+".join(direction), [
                {"cmd": "act", "actions": direction, "frames": 90},
                {"cmd": "frames", "count": 92},
            ]))
    for action in ("Confirm", "Cancel", "Menu", "Jump", "Start"):
        result.append((action, pulse(action, 90)))
    if kind == "battle":
        for action in ("AttackWeak", "AttackStrong", "AttackFierce", "AttackEnd",
                       "Up", "Down", "Left", "Right"):
            result.append((action, pulse(action, 90)))
    if kind == "event":
        for bit, action in ((0x10, "Jump"), (0x20, "Cancel"), (0x40, "Confirm"),
                            (0x80, "Menu"), (0x1000, "Up"), (0x2000, "Right"),
                            (0x4000, "Down"), (0x8000, "Left")):
            if context.get("buttons", 0) & bit:
                result.append((f"script:{action}", pulse(action, 30)))
    result.append(("wait", [{"cmd": "pad", "buttons": []}, {"cmd": "frames", "count": 300}]))
    # Different UI entries can resolve to the same ordinary input sequence.
    unique, seen = [], set()
    for label, commands in result:
        key = repr(commands)
        if key not in seen:
            unique.append((label, commands))
            seen.add(key)
    return unique


class Frontier:
    """Spend work on untried choices near rare coverage and recent gains.

    Frequency is measured across retained checkpoints, not every animation
    frame. Bounded storage evicts redundant, already-explored checkpoints.
    All their input lineages remain available for regression minimization.
    """

    def __init__(self, rng, limit):
        self.rng, self.limit, self.entries = rng, limit, []
        self.preferred = None

    def frequencies(self):
        return Counter(feature for entry in self.entries for feature in entry["features"])

    def weight(self, entry, frequency):
        features = entry["features"]
        rarity = sum(1 / frequency[f] for f in features) / max(1, len(features))
        rate = entry.get("gain_rate", 1)
        return (1 + 4 * rarity + math.log1p(rate)) / (1 + entry["trials"])

    def add(self, entry):
        self.entries.append(entry)
        self.preferred = entry
        if len(self.entries) <= self.limit:
            return None
        frequency = self.frequencies()
        # Do not discard the just-discovered frontier before trying it once.
        victim = min(self.entries[:-1], key=lambda e: self.weight(e, frequency))
        self.entries.remove(victim)
        return victim

    def choose(self):
        candidates = [entry for entry in self.entries if entry["remaining"]]
        if not candidates:
            return None
        # Preserve a productive chain through dialogue/animation waits, while
        # reserving a quarter of choices for branching elsewhere in the corpus.
        if self.preferred in candidates and self.rng.random() < 0.75:
            return self.preferred
        frequency = self.frequencies()
        return self.rng.choices(candidates, [self.weight(e, frequency) for e in candidates])[0]


def minimize(cases):
    """Greedy weighted set cover. Every retained measurement has a witness.

    This reduces repeat regression work; it does not prove paths equivalent.
    Features describe the complete recorded input lineage, including its seed.
    """
    remaining = set().union(*(case["features"] for case in cases))
    chosen = []
    while remaining:
        best = max(cases, key=lambda case: len(remaining.intersection(case["features"]))
                   / max(0.001, case["cost"]))
        covered = remaining.intersection(best["features"])
        if not covered:
            raise ValueError("coverage has no replay witness")
        chosen.append(best)
        remaining -= covered
    return chosen


def replay_seeds(path):
    """Replay input, never copy old coverage IDs into a newly built module."""
    case = json.loads(path.read_text())
    if "cases" in case:
        return [seed for name in case["cases"] for seed in replay_seeds(path.parent / name)]
    if case.get("failure"):
        raise ValueError("reseed from completed witnesses; use --replay to check a failure")
    if case.get("origin") not in ("legal", "scenario") or not case.get("disc"):
        raise ValueError(f"{path}: replay witness lacks its origin/boot disc")
    return [(case["origin"], case["prefix"] + case["commands"], None, Path(case["disc"]))]


def run(args):
    # Shared protocol/watchdog and seed semantics with the random-input control
    # experiment. Keep imports local so the command-line entry is also usable.
    import random

    import game_explore as ge

    manifest = json.loads(args.coverage.read_text())
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "cases").mkdir()
    rng = random.Random(args.seed)
    frontier = Frontier(rng, args.max_corpus)
    hits, places, cases = set(), set(), []
    attempts = Counter()
    started, frames, attempted, failure = time.monotonic(), 0, 0, None
    completed, imported = False, 0
    stderr = (args.out / "stderr.log").open("w")
    client = ge.Client(ge.command_line(args), stderr, args.timeout)
    root = {"state": "root.xemstate", "prefix": [], "commands": [], "origin": "legal",
            "disc": str(args.disc.resolve()), "features": set(), "cost": 0}
    current, place = root, None
    ge.save_json(args.out / "run.json", {
        "module_sha256": manifest["module_sha256"], "coverage_signature": manifest["signature"],
        "headless_sha256": hashlib.sha256(args.headless.read_bytes()).hexdigest(),
        "disc": root["disc"], "strategy": "frontier", "seed": args.seed,
        "options": {k: str(v) if isinstance(v, Path) else [str(p) for p in v]
                    if isinstance(v, list) else v for k, v in vars(args).items()},
    })

    def checkpoint(prefix, origin, disc, features, cost, gain, place, context):
        index = len(cases)
        state = f"state-{index:06d}.xemstate"
        client({"cmd": "state_save", "path": str((args.out / state).resolve())})
        entry = {"id": index, "state": state, "prefix": prefix, "commands": [],
                 "origin": origin, "disc": disc, "features": set(features), "cost": cost,
                 "gain_rate": gain, "place": place, "context": context, "trials": 0,
                 "remaining": actions(client, context)}
        ge.save_json((args.out / state).with_suffix(".json"),
                     {**entry, "features": sorted(features)})
        evicted = frontier.add(entry)
        if evicted:
            (args.out / evicted["state"]).unlink()
            (args.out / evicted["state"]).with_suffix(".json").unlink()
        return entry

    def witness(entry, prefix, features, cost, label):
        # Every completed branch has its full input lineage even if its fast
        # snapshot is later evicted. Regression replay always starts at boot.
        case = {"id": len(cases), "prefix": prefix, "commands": [],
                "origin": entry["origin"], "disc": entry["disc"], "label": label,
                "features": sorted(features), "cost": cost, "requires_from_boot": True}
        ge.save_json(args.out / "cases" / f"{case['id']:06d}.json", case)
        cases.append(case)

    try:
        ge.feedback(client, manifest)  # validates signature and starts an epoch
        client({"cmd": "state_save", "path": str((args.out / root["state"]).resolve())})
        seeds = []
        if args.field is not None or args.log or not (args.add_field or args.add_log or args.resume or args.add_replay):
            seeds.append(ge.seed_commands(args))
        for field in args.add_field:
            seeds.append(ge.seed_commands(argparse.Namespace(**{**vars(args), "field": field, "log": None})))
        for log in args.add_log:
            seeds.append(ge.seed_commands(argparse.Namespace(**{**vars(args), "field": None, "log": log})))
        seeds = [(*seed, args.disc) for seed in seeds]
        for path in args.add_replay:
            seeds += replay_seeds(path)
        for kind, commands, expected, disc in seeds:
            if str(disc.resolve()) != root["disc"]:
                client.close()
                client = ge.Client(ge.command_line(argparse.Namespace(**{**vars(args), "disc": disc})),
                                   stderr, args.timeout)
                ge.feedback(client, manifest)
                client({"cmd": "state_save", "path": str((args.out / root["state"]).resolve())})
                root["disc"] = str(disc.resolve())
            client({"cmd": "state_load", "path": str((args.out / root["state"]).resolve())})
            ge.feedback(client, manifest)
            if expected:
                commands.append({"cmd": "assert_memory_hash", "expected": expected})
            current = {**root, "features": [], "origin": kind, "commands": commands}
            ge.save_json(args.out / "pending.json", current)
            began, reached = time.monotonic(), set()
            for command in commands:
                client(command)
                reached |= ge.feedback(client, manifest)
            cost = time.monotonic() - began
            place, context = ge.progress_key(client)
            new = len(reached - hits)
            entry = checkpoint(commands, kind, root["disc"], reached, cost,
                               new / max(cost, 0.001), place, context)
            witness(entry, commands, reached, cost, "seed")
            hits |= reached
            places.add(place)

        for directory in args.resume:
            previous = json.loads((directory / "run.json").read_text())
            if previous.get("strategy") != "frontier":
                raise ValueError(f"{directory}: --resume needs a frontier run with replay witnesses")
            if previous["coverage_signature"] != manifest["signature"]:
                raise ValueError(f"{directory}: checkpoint coverage build differs")
            # Import the measured union and its replay witnesses, not guesses
            # from the checkpoint's current PC. Copy so this run is standalone.
            for path in sorted((directory / "cases").glob("*.json")):
                case = json.loads(path.read_text())
                case["id"] = len(cases)
                ge.save_json(args.out / "cases" / f"{case['id']:06d}.json", case)
                cases.append(case)
                hits.update(case["features"])
            for old_state in sorted(directory.glob("*.xemstate")):
                if old_state.name == "root.xemstate":
                    continue
                path = old_state.with_suffix(".json")
                entry = json.loads(path.read_text())
                if old_state.name != entry["state"]:
                    raise ValueError(f"checkpoint metadata names a different state: {path}")
                entry["features"] = set(entry["features"])
                name = f"state-import-{imported:06d}.xemstate"
                imported += 1
                shutil.copyfile(old_state, args.out / name)
                entry["state"] = name
                ge.save_json((args.out / name).with_suffix(".json"),
                             {**entry, "features": sorted(entry["features"])})
                evicted = frontier.add(entry)
                if evicted:
                    (args.out / evicted["state"]).unlink()
                    (args.out / evicted["state"]).with_suffix(".json").unlink()
                places.add(entry["place"])
        if not frontier.entries:
            raise ValueError("no seed or retained frontier")

        deadline = time.monotonic() + args.seconds if args.seconds else float("inf")
        for iteration in range(args.iterations):
            if time.monotonic() >= deadline:
                break
            entry = frontier.choose()
            if entry is None:
                break
            client({"cmd": "state_load", "path": str((args.out / entry["state"]).resolve())})
            ge.feedback(client, manifest)  # state files deliberately omit telemetry
            location = json.loads(entry["place"])
            scope = (entry["disc"], location.get("map"), location.get("story"))
            if entry["context"]["kind"] == "walk":
                # Moving one cell must not make the same nearby NPC a fresh
                # target again. Try neglected actors/directions before retries.
                entry["remaining"].sort(key=lambda item: attempts[scope, item[0]])
            label, commands = entry["remaining"].pop(0)
            attempts[scope, label] += 1
            entry["trials"] += 1
            ge.save_json((args.out / entry["state"]).with_suffix(".json"),
                         {**entry, "features": sorted(entry["features"])})
            current = {k: entry[k] for k in ("state", "prefix", "origin", "disc")}
            current["commands"] = commands
            ge.save_json(args.out / "pending.json", current)
            began = time.monotonic()
            for command in commands:
                client(command)
            reached = ge.feedback(client, manifest)
            # Advance a bounded piece of dialogue/cutscene without saving and
            # restoring a multi-megabyte snapshot at every text box. Stop at a
            # real choice, player control, or a script that tests buttons.
            settled = 0
            while settled < args.settle_frames:
                ui = client({"cmd": "context"})
                if ui["kind"] == "dialogue" and not ui.get("choice"):
                    extra = pulse("Confirm", min(90, args.settle_frames - settled))
                elif ui["kind"] == "none" or (ui["kind"] == "event" and not ui.get("buttons")):
                    extra = [{"cmd": "pad", "buttons": []},
                             {"cmd": "frames", "count": min(300, args.settle_frames - settled)}]
                else:
                    break
                commands += extra
                ge.save_json(args.out / "pending.json", current)
                for command in extra:
                    client(command)
                settled += sum(c.get("count", 0) for c in extra if c["cmd"] == "frames")
                reached |= ge.feedback(client, manifest)
            place, context = ge.progress_key(client)
            elapsed = time.monotonic() - began
            new = len(reached - hits)
            entry["gain_rate"] = 0.75 * entry["gain_rate"] + 0.25 * new / max(elapsed, 0.001)
            prefix, features = entry["prefix"] + commands, entry["features"] | reached
            cost = entry["cost"] + elapsed
            witness(entry, prefix, features, cost, label)
            frames += sum(c.get("count", 0) for c in commands if c["cmd"] == "frames")
            attempted += 1
            # A long unskippable event must keep advancing through a plateau.
            # Its continuation is a search hint, not reported as new coverage.
            continuation = context["kind"] in ("none", "event", "dialogue")
            if new or place not in places or (continuation and label in ("wait", "Confirm")):
                checkpoint(prefix, entry["origin"], entry["disc"], features, cost,
                           new / max(elapsed, 0.001), place, context)
            hits |= reached
            places.add(place)
            with (args.out / "steps.jsonl").open("a") as stream:
                stream.write(json.dumps({"iteration": iteration, "checkpoint": entry["state"],
                                         "action": label, "new_features": new,
                                         "seconds": elapsed, "frames": frames}) + "\n")
            if attempted % 25 == 0:
                blocks = sum(f < ge.SCRIPT_FEATURE for f in hits)
                print(f"{attempted} branches, {frames} frames, {blocks} blocks, "
                      f"{len(hits) - blocks} script observations, "
                      f"{len(frontier.entries)} checkpoints", flush=True)
        completed = True
    except (ge.Failure, ValueError) as error:
        failure = (error if isinstance(error, ge.Failure) else
                   ge.Failure("measurement", str(error), {"cmd": "coverage"}))
        current = {k: v for k, v in current.items() if k != "features"}
        current["failure"] = failure.record()
        if failure.command not in current["commands"]:
            current["commands"].append(failure.command)
        ge.save_json(args.out / "failure.json", current)
        print(f"{failure.kind}: {failure.detail}", flush=True)
    finally:
        client.close()
        stderr.close()
        blocks = {f for f in hits if f < ge.SCRIPT_FEATURE}
        scripts = {f ^ ge.SCRIPT_FEATURE for f in hits if f >= ge.SCRIPT_FEATURE}
        ge.save_json(args.out / "coverage-hits.json", sorted(blocks))
        ge.save_json(args.out / "coverage-script-hits.json", sorted(scripts))
        selected = minimize(cases) if cases else []
        # Paths resolve relative to this file, and each case names its boot disc.
        ge.save_json(args.out / "replay-suite.json", {
            "cases": [f"cases/{c['id']:06d}.json" for c in selected],
            "features": len(set().union(*(c["features"] for c in selected))),
            "measured_cases": len(cases), "selected_cases": len(selected),
            "estimated_all_replay_seconds": sum(c["cost"] for c in cases),
            "estimated_selected_replay_seconds": sum(c["cost"] for c in selected),
        })
        ge.save_json(args.out / "summary.json", {
            "strategy": "frontier", "completed": completed, "inputs": attempted,
            "exploration_frames": frames, "seconds": time.monotonic() - started,
            "failures": [failure.record()] if failure else [],
            "blocks_hit": len(blocks), "blocks_total": len(manifest["blocks"]),
            "field_scripts": ge.script_summary(scripts), "places": len(places),
            "last_place": json.loads(place) if place else None,
            "images": ge.coverage_summary(manifest, blocks),
        })
    return 1 if failure else 0
