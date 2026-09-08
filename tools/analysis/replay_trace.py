"""Compare complete source-calculated replay effects and original nested steps."""

from __future__ import annotations

from collections import Counter

from .animation_trace import OriginalCall, OriginalResources, calls, compare_effect, list_memory
from .original_trace import require
from .sprite_animation import select_orientation
from .sprite_replay import (
    clear_auxiliary,
    command_replay,
    frame_change,
    lookup_frame,
    previous_frame,
)
from .sprite_state import put, u32


def required_next(iterator, label: str):
    value = next(iterator, None)
    require(value is not None, "missing original " + label)
    return value


def completed(iterator, label: str) -> None:
    require(next(iterator, None) is None, "additional original " + label)


def evaluate(call: OriginalCall, resources: OriginalResources, widths: bytes, steps: Counter):
    gpr, old, kind = call.before["gpr_u32"], call.old, call.kind
    address, sprite, head = gpr[4], old["sprite"], u32(old["sprite-controls"])

    def read_memory(pointer, size):
        return list_memory(call, pointer, size)

    def children(name):
        return iter((row, payload) for row, payload in call.children if row["hook"] == name)

    if kind == "clear":
        result = clear_auxiliary(sprite, address)
    elif kind == "previous-frame":
        parts = children("previous-frame-part")

        def part(pointer, index, count, flags, out):
            row, payload = required_next(parts, "previous-frame part")
            registers = row["gpr_u32"]
            require(
                (pointer, index, count, flags)
                == (registers[16], registers[19], registers[20], registers[21])
                and out == payload["sprite"],
                "original frame metadata traversal differs",
            )
            require(
                payload["source-command"] == resources.read(pointer, 16),
                "original frame metadata source differs",
            )
            steps["previous-frame-part"] += 1

        result = previous_frame(sprite, address, gpr[5], gpr[6], resources.read, part)
        completed(parts, "previous-frame part")
    elif kind == "frame":
        links, previous = children("frame-list-step"), children("previous-frame-before")

        def link(node, out):
            row, payload = required_next(links, "frame list step")
            require(
                node == row["gpr_u32"][3] and out == payload["sprite"],
                "original frame list traversal differs",
            )
            steps["frame-list-step"] += 1

        def prior(out, frame, binding):
            row, payload = required_next(previous, "previous-frame call")
            require(
                [address, frame, binding] == row["gpr_u32"][4:7] and out == payload["sprite"],
                "original previous-frame call inputs differ",
            )
            steps["frame-previous-call"] += 1

        result, head = frame_change(
            sprite, address, gpr[5], head, resources.read, read_memory, link, prior
        )
        completed(links, "frame list step")
        completed(previous, "previous-frame call")
        inserted = [
            (row, payload) for row, payload in call.children if row["hook"] == "frame-insert"
        ]
        # A changed list head proves insertion; an already equal head can also be
        # rewritten when its queued flag is clear. Original source tests that flag first.
        insertion_expected = bool(
            u32(sprite, 0x3C) & 3 == 1
            and (
                not u32(sprite, 0x40) & 0x20000
                or not any(
                    row["gpr_u32"][3] == address
                    for row, _ in call.children
                    if row["hook"] == "frame-list-step"
                )
            )
        )
        require(len(inserted) == int(insertion_expected), "original frame insertion branch differs")
        if inserted:
            row, payload = inserted[0]
            expected = bytearray(sprite)
            if u32(sprite, 0x40) & 0x100000:
                put(expected, 0x40, u32(expected, 0x40) & ~0x100000)
                renderer = u32(expected, 0x20) - address
                if u32(expected, renderer + 0x34):
                    expected = bytearray(clear_auxiliary(expected, address))
            require(
                payload["sprite"] == expected and row["gpr_u32"][17] == gpr[5],
                "original frame insertion input differs",
            )
            steps["frame-insert"] += 1
    elif kind == "lookup":
        frames = children("frame-before")

        def selected(out, frame):
            row, payload = required_next(frames, "lookup frame call")
            require(
                row["gpr_u32"][4:6] == [address, frame] and payload["sprite"] == out,
                "original lookup frame inputs differ",
            )
            steps["lookup-frame-call"] += 1

        result, head = lookup_frame(sprite, address, head, resources.read, read_memory, selected)
        completed(frames, "lookup frame call")
    elif kind in ("replay", "orientation"):
        replay_steps, entries = children("replay-step"), children("replay-before")

        def step(out, current_head):
            _, payload = required_next(replay_steps, "replay step")
            require(
                out == payload["sprite"] and current_head == u32(payload["sprite-controls"]),
                "original replay step effect differs",
            )
            steps[kind + "-replay-step"] += 1

        def replay(out, target, target_step):
            nonlocal head
            row, payload = required_next(entries, "orientation replay call")
            require(
                row["gpr_u32"][4:7] == [address, target, target_step] and out == payload["sprite"],
                "original orientation replay reset differs",
            )
            out, head = command_replay(
                out,
                address,
                target,
                target_step,
                head,
                widths,
                resources.read,
                read_memory,
                step,
                gpr[19],
            )
            return out

        if kind == "replay":
            result, head = command_replay(
                sprite,
                address,
                gpr[5],
                gpr[6],
                head,
                widths,
                resources.read,
                read_memory,
                step,
                gpr[19],
            )
        else:
            result = select_orientation(sprite, gpr[5], resources.read, replay)
        completed(replay_steps, "replay step")
        completed(entries, "orientation replay call")
    else:
        raise ValueError("Unreconstructed original replay helper " + kind)
    stores = [(address, result), (gpr[28] + 0x20, head.to_bytes(4, "little"))]
    compare_effect(call, stores)
    return stores


def compare_replay(rows, spec: dict, resources: OriginalResources, widths: bytes):
    counts, callers, steps, effects = Counter(), Counter(), Counter(), {}
    for call in calls(rows, spec, "replay"):
        stores = evaluate(call, resources, widths, steps)
        if call.kind in ("orientation", "replay"):
            # This cache contains calculated source-model outputs. No observed
            # return payload is used to stand in for an unreconstructed callee.
            effects[call.before["event"]] = stores
        counts[call.kind] += 1
        callers[(call.kind, hex(call.before["gpr_u32"][31]))] += 1
    require(
        counts
        == {
            "orientation": 1479,
            "lookup": 119,
            "frame": 198,
            "clear": 69,
            "replay": 65,
            "previous-frame": 33,
        },
        "original replay route coverage differs",
    )
    return {
        "counts": dict(counts),
        "steps": dict(steps),
        "callers": [list(k) + [v] for k, v in sorted(callers.items())],
    }, effects
