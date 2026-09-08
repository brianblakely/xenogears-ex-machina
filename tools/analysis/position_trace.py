"""Source-state and call-lineage checks for original field position observations.

The optional prologue hook captures each sprite before its first read. The
qualified original prologue writes only saved registers and the result flag;
it has no calls and does not alter that sprite. This is a function input,
not an observed result from a dependency. Own layer-query stack scratch is
outside the byte comparison; the parent frame and saved return are checked.
"""

from __future__ import annotations

import struct
from collections import Counter

from . import position as model
from .animation_trace import address, read_captured
from .original_trace import exact_payload, ordered_rows, require
from .sprite_state import u16, u32


def compare_position(source, rows, spec):
    require(bool(rows), "Empty original position comparison")
    first_read = any(h["name"] == "position-inputs" for h in spec["hooks"])
    observed_masks = any(h["name"] == "layer-mask" for h in spec["hooks"])
    calls = []
    call = None
    counts = Counter()
    reasons = Counter()
    verticals = Counter()
    layers = Counter()
    queries = Counter()
    masks = Counter()
    callers = Counter()
    links = Counter()
    for row, p in ordered_rows(rows, spec):
        name = row["hook"]
        counts[name] += 1
        if name == "position-before":
            require(call is None, "Overlapping original position calls")
            call = []
        require(call is not None, "Original position event without entry")
        call.append((row, p))
        if name == "position-after":
            calls.append(call)
            call = None
    require(call is None, "Truncated original position call")
    for call in calls:
        before, old = call[0]
        b = before["gpr_u32"]
        index = b[4]
        sp = b[29] - 0x100
        require(b[31] in (0x80084898, 0x800814B4), "Unqualified original position caller")
        table = u32(old["descriptor-table"])
        descriptor_pointer = table + 92 * index
        descriptor = read_captured(before, old, descriptor_pointer, 92)
        require(u32(descriptor, 0x4C) == b[7], "Position actor descriptor identity")
        if first_read:
            inputs, initial = call[1]
            g = inputs["gpr_u32"]
            require(
                inputs["hook"] == "position-inputs"
                and inputs["frontend_run"] == before["frontend_run"],
                "Original position first-read order",
            )
            require(
                (g[17], g[21], g[29], g[22], g[23]) == (b[7], u32(descriptor, 4), sp, index, b[5]),
                "Original position first-read argument lineage",
            )
            require(
                b[6] == descriptor_pointer
                and old["entry-descriptor"] == descriptor
                and initial["entry-descriptor"] == descriptor,
                "Original caller descriptor argument",
            )
            require(initial["actor"] == old["actor"], "Original prologue changed its actor input")
            sprite = initial["sprite"]
        else:
            require(index in (3, 5), "Original position recording lacks first-read NPC inputs")
            sprite = read_captured(before, old, u32(descriptor, 4), 512)
        sprite_pointer = u32(descriptor, 4)
        ctl = model.PositionControls(
            u32(old["motion-globals"]),
            old["query-controls"][7],
            old["query-controls"][4],
            u32(old["collision-controls"]),
            u32(old["collision-controls"], 0x74),
            u16(old["mesh-globals"], 0x3C),
            u32(old["position-debug"]) if "position-debug" in old else None,
        )
        result = model.position(
            index,
            b[5],
            u32(old["caller-arguments"], 0x10),
            old["actor"],
            sprite,
            descriptor,
            old["position-frame"],
            sp,
            b,
            ctl,
            source["component"],
            source["table"],
            old["history-a"] + old["history-b"] + old["history-c"],
            u32(old["history-index"]),
            u32(old["history-reset"]),
            u16(old["position-result"]),
            observe_first_read=first_read,
        )
        reasons[index, result.value, result.reason] += 1
        callers[b[31]] += 1
        layers[ctl.layer_count] += 1
        links[(ctl.collision_mode, u32(old["caller-arguments"], 0x10), b[5])] += 1
        if result.vertical:
            verticals[result.vertical.branch] += 1
        for q in result.queries:
            queries[q.value, q.reason] += 1
            for step in q.steps:
                masks[step[2]] += 1

        def compare(
            row,
            p,
            stage,
            *,
            b=b,
            sprite_pointer=sprite_pointer,
            descriptor_pointer=descriptor_pointer,
            sp=sp,
            sprite=sprite,
            before=before,
            old=old,
        ):
            stores = [
                (b[7], stage.actor),
                (sprite_pointer, stage.sprite),
                (descriptor_pointer, stage.descriptor),
                (sp, stage.frame),
                (0x800B14F0, stage.ring),
                (0x800B2360, struct.pack("<I", stage.history_index)),
                (0x800C3910, struct.pack("<I", stage.history_reset)),
                (0x800ADB00, struct.pack("<H", stage.position_result)),
            ]
            expected = {}
            for name, data in p.items():
                if name == "layer-frame":
                    continue
                pointer = address(row, name)
                length = len(data)
                if sprite_pointer <= pointer and pointer + length <= sprite_pointer + len(sprite):
                    data = bytearray(
                        sprite[pointer - sprite_pointer : pointer - sprite_pointer + length]
                    )
                else:
                    data = bytearray(read_captured(before, old, pointer, length))
                for at, value in stores:
                    lo, hi = max(pointer, at), min(pointer + length, at + len(value))
                    if lo < hi:
                        data[lo - pointer : hi - pointer] = value[lo - at : hi - at]
                expected[name] = bytes(data)
            exact_payload(
                {k: v for k, v in p.items() if k != "layer-frame"},
                expected,
                "Position " + row["hook"] + " " + str(row["event"]),
            )

        next_stage = 0
        next_query = 0
        query = None
        next_step = 0
        next_mask = 0
        query_before = None
        for row, p in call[1:]:
            name, g = row["hook"], row["gpr_u32"]
            require(
                row["frontend_run"] == before["frontend_run"],
                "Original position run changed within call",
            )
            if name in ("layer-step", "layer-mask"):
                require(query is not None, "Layer loop event without query")
                step = next_step if name == "layer-step" else next_mask
                require(step < len(query.steps), "Extra original layer step/mask")
                require(g[30] == b[7] and g[29] == sp - 0x80, "Layer loop actor/frame lineage")
                require(
                    (g[17], g[23]) == (query.steps[step][0] & 0xFFFFFFFF, query.steps[step][1]),
                    "Computed original layer step",
                )
                if name == "layer-mask":
                    require(
                        next_step == next_mask + 1 and g[16] == query.steps[step][2],
                        "Computed original layer mask",
                    )
                    next_mask += 1
                else:
                    require(
                        not observed_masks or next_step == next_mask, "Missing original layer mask"
                    )
                    next_step += 1
                compare(row, p, query_before)
                continue
            require(next_stage < len(result.stages), "Extra original position stage")
            stage = result.stages[next_stage]
            require(
                name == stage.name,
                "Original position stage order "
                + str(row["event"])
                + " "
                + name
                + " != "
                + stage.name,
            )
            compare(row, p, stage)
            next_stage += 1
            if name == "layer-before":
                layer = stage.values["layer"]
                require(query is None, "Nested layer query")
                require(
                    (g[4], g[5], g[6], g[7], g[29], g[31])
                    == (b[7], layer, sp + 0x18 + 4 * layer, sp + 0x50 + 16 * layer, sp, 0x80084C00),
                    "Position layer argument lineage",
                )
                query = result.queries[next_query]
                next_query += 1
                next_step = 0
                next_mask = 0
                query_before = stage
            elif name == "layer-after":
                require(
                    query is not None
                    and next_step == len(query.steps)
                    and (not observed_masks or next_mask == next_step),
                    "Missing original layer loop records",
                )
                require(
                    g[2] == stage.values["result"] & 0xFFFFFFFF
                    and g[29] == sp - 0x80
                    and g[30] == b[7],
                    "Layer return value/frame",
                )
                require(u32(p["layer-frame"], 0x7C) == 0x80084C00, "Layer saved return lineage")
                query = None
            elif name.startswith("history-"):
                require(g[29] == sp and g[31] == 0x80085498, "Position history call lineage")
                if name == "history-before":
                    require(g[4] == index, "Position history actor lineage")
            else:
                require(
                    g[29] == sp and g[17] == b[7] and g[21] == sprite_pointer,
                    "Position stage frame/actor/sprite lineage",
                )
                if name == "position-after":
                    require(g[2] == result.value & 0xFFFFFFFF, "Position return value")
        require(
            next_stage == len(result.stages)
            and next_query == len(result.queries)
            and query is None,
            "Incomplete computed position sequence",
        )
    return {
        "records": len(rows),
        "counts": dict(counts),
        "position_results": [list(k) + [v] for k, v in reasons.items()],
        "vertical_branches": dict(verticals),
        "layer_count_inputs": dict(layers),
        "layer_results": [list(k) + [v] for k, v in queries.items()],
        "computed_layer_masks": dict(masks),
        "callers": {hex(k): v for k, v in callers.items()},
        "linked_floor_inputs": [[*k, v] for k, v in links.items()],
        "first_read_sprite_inputs": first_read,
        "excluded_position_calls": 0,
    }
