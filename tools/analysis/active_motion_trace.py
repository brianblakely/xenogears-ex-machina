"""Source specifications and entry-to-return comparisons for party field motion.

NPC motion is enumerated as excluded, never accepted as an unchanged/no-op path.
Every modeled stage is derived from a captured function entry and original data.
"""

import struct
from collections import Counter
from functools import partial

from .active_motion import MotionControls, active_motion, idle_predicate
from .animation_trace import address, read_captured
from .original_trace import exact_payload, ordered_rows, require
from .sprite_state import u32


def compare_motion(rows, spec, source, resources, lookup):
    counts, modes, stops, callers, excluded = Counter(), Counter(), Counter(), Counter(), Counter()
    context, predicate_call = None, None
    table = (
        next(r["offset"] for r in spec["hooks"][0]["ranges"] if r["name"] == "player-descriptor")
        - 3 * 92
        + 0x80000000
    )

    def controls(p):
        return MotionControls(
            u32(p["held-inputs"], 4),
            u32(p["movement-controls"], 4),
            u32(p["collision-controls"]),
            u32(p["collision-controls"], 0x74),
            u32(p["motion-globals"]),
            struct.unpack("<2I", p["party-controls"]),
            {3: u32(p["player-actor"]), 5: u32(p["companion-actor"])},
            *struct.unpack_from("<2h", p["motion-globals"], 0xD8),
            p["query-controls"][4],
            struct.unpack_from("<i", p["sprite-controls"], 8)[0],
            p["sprite-controls"][0x1D],
        )

    def expected_payload(before, old, row, payload, stores):
        expected = {}
        for name in payload:
            pointer = address(row, name)
            if name == "saved-registers":
                initial = struct.pack(
                    "<8I", *(before["gpr_u32"][i] for i in (16, 17, 18, 19, 20, 21, 22, 31))
                )
            else:
                initial = read_captured(before, old, pointer, len(payload[name]))
            data = bytearray(initial)
            for at, value in stores:
                lo, hi = max(pointer, at), min(pointer + len(data), at + len(value))
                if lo < hi:
                    data[lo - pointer : hi - pointer] = value[lo - at : hi - at]
            expected[name] = bytes(data)
        return expected

    def check_stage(c, stage, row, payload):
        entry, old = c["entry"], c["old"]
        g = entry["gpr_u32"]
        stores = [
            (g[6], stage.actor),
            (u32(old["descriptor"], 4), stage.sprite),
            (address(entry, "motion-locals"), stage.locals),
            (0x80065B08, struct.pack("<I", g[4])),
        ]
        exact_payload(
            payload,
            expected_payload(entry, old, row, payload, stores),
            "whole original motion " + row["hook"] + " event " + str(row["event"]),
        )
        require(
            row["frontend_run"] == entry["frontend_run"], "Original motion crossed frontend run"
        )
        require(row["gpr_u32"][29] == g[29] - 0x58, "Original main stack lineage differs")

    for row, p in ordered_rows(rows, spec):
        name, g = row["hook"], row["gpr_u32"]
        require(
            u32(p["field"]) == 23 and u32(p["descriptor-table"]) == table,
            "Original motion field or descriptor table changed",
        )
        if name == "motion-before":
            require(context is None and predicate_call is None, "Nested original main motion")
            require(
                g[31] == 0x80081288
                and g[5] == table + g[4] * 92
                and g[6] == u32(p["descriptor"], 0x4C),
                "Main descriptor/actor lineage differs",
            )
            if g[4] not in (3, 5):
                context = {"entry": row, "old": p, "excluded": True}
                excluded[g[4]] += 1
                continue
            sprite = read_captured(row, p, u32(p["descriptor"], 4), 512)
            result = active_motion(
                g[4],
                p["descriptor"],
                p["actor"],
                sprite,
                p["motion-locals"],
                controls(p),
                source["component"],
                lookup,
                resources.read,
                partial(read_captured, row, p),
            )
            context = {
                "entry": row,
                "old": p,
                "result": result,
                "stages": list(result.stages),
                "next": 0,
            }
            callers[hex(g[31])] += 1
            continue
        if name in ("predicate-before", "predicate-after"):
            if name.endswith("before"):
                require(
                    predicate_call is None and g[31] in (0x80082CC0, 0x80084B20),
                    "Unexpected predicate caller",
                )
                value = idle_predicate(
                    p["actor"], u32(p["collision-controls"]), u32(p["collision-controls"], 0x74)
                )
                predicate_call = row, p, value
                if context and not context.get("excluded"):
                    require(g[31] == 0x80082CC0, "Wrong predicate parent")
                    stage = context["stages"][context["next"]]
                    require(stage.name == "after-predicate", "Unexpected predicate stage")
                    check_stage(context, stage, row, p)
            else:
                require(predicate_call is not None, "Orphan predicate return")
                before, old, value = predicate_call
                require(
                    g[2] == value & 0xFFFFFFFF
                    and g[4] == before["gpr_u32"][4]
                    and g[29] == before["gpr_u32"][29]
                    and g[31] == before["gpr_u32"][31]
                    and row["frontend_run"] == before["frontend_run"],
                    "Predicate return differs",
                )
                exact_payload(p, old, "Original predicate unchanged memory")
                counts["predicate-calls"] += 1
                predicate_call = None
            continue
        if context and context.get("excluded"):
            require(
                row["frontend_run"] == context["entry"]["frontend_run"],
                "Excluded original motion crossed frontend run",
            )
            if name == "motion-after":
                require(
                    g[29] == context["entry"]["gpr_u32"][29] - 0x58,
                    "Excluded original motion stack differs",
                )
                context = None
            continue
        if name == "bounds-call":
            require(context and context["next"] > 0, "Orphan bounds call")
            stage = context["stages"][context["next"] - 1]
            require(
                stage.name == "before-bounds"
                and g[31] == 0x80082DA0
                and g[4] == address(context["entry"], "motion-locals")
                and g[5] == context["entry"]["gpr_u32"][6],
                "Bounds argument lineage differs",
            )
            check_stage(context, stage, row, p)
            counts["bounds-call"] += 1
            continue
        if name == "animation-call" and context is None:
            require(g[31] in (0x80081914, 0x80081998), "Unexpected external animation caller")
            counts["external-animation-boundaries"] += 1
            continue
        require(
            context is not None and context["next"] < len(context["stages"]), "Orphan motion stage"
        )
        c = context
        stage = c["stages"][c["next"]]
        require(stage.name == name, "Expected " + stage.name + " got " + name)
        check_stage(c, stage, row, p)
        entry = c["entry"]["gpr_u32"]
        if name not in ("animation-call", "sweep-ordinary-before", "sweep-special-before"):
            require(
                g[16] == entry[6]
                and g[19] == u32(c["old"]["descriptor"], 4)
                and g[21] == entry[4]
                and g[22] == entry[5],
                "Main saved argument registers differ",
            )
        if name in ("after-predicate", "after-bounds", "after-sweep"):
            require(
                g[2] == stage.values["result"] & 0xFFFFFFFF,
                "Computed original callee return differs",
            )
        if name == "after-predicate":
            require(g[17] == stage.values["extra"], "Original additive motion bits differ")
        if name in ("motion-mode", "animation-choice"):
            require(g[20] == stage.mode & 0xFFFFFFFF, "Original selected animation mode differs")
        if name == "motion-mode":
            require(g[18] == stage.direction & 0xFFFFFFFF, "Original requested direction differs")
        if name == "after-sweep":
            require(
                g[4] == stage.values["result"] & 0xFFFFFFFF, "Original copied sweep result differs"
            )
        if name.startswith("sweep-"):
            require(
                g[4] == address(c["entry"], "motion-locals")
                and g[5] == entry[6]
                and g[6] == g[4] + 16
                and g[7] == stage.direction & 0xFFFFFFFF
                and g[31] == (0x80082F10 if name == "sweep-ordinary-before" else 0x80082F2C),
                "Original sweep arguments differ",
            )
        if name == "animation-call":
            require(
                g[4] == u32(c["old"]["descriptor"], 4)
                and g[5] == stage.mode & 0xFFFFFFFF
                and g[6] == entry[5]
                and g[31] == 0x800830F4,
                "Original animation dispatch differs",
            )
        counts[name] += 1
        c["next"] += 1
        if name == "motion-after":
            require(c["next"] == len(c["stages"]), "Unconsumed main stages")
            result = c["result"]
            counts["inhibited" if result.inhibited else "active"] += 1
            if not result.inhibited:
                modes[result.stages[-1].mode] += 1
                stops[result.stop_reason or "moving"] += 1
            context = None
    require(context is None and predicate_call is None, "Incomplete original call")
    return {
        "counts": dict(counts),
        "modes": dict(modes),
        "stops": dict(stops),
        "callers": dict(callers),
        "excluded_actor_calls": dict(excluded),
        "resource_reads": sum(resources.reads.values()),
    }
