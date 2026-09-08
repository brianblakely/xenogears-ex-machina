"""Original intermediate arithmetic, tied to independently captured sweep calls."""

from __future__ import annotations

import struct
from collections import Counter

from .arithmetic import signed16, signed32
from .collision_math import normalize
from .original_trace import exact_payload, require
from .sweep_math import atan_angle, fixed_sqrt, planar_length
from .sweep_trace import (
    call,
    correlate,
    edge_points,
    ordered_rows,
    s16,
    sweep_result,
    tables,
    u32,
    vector,
    write_edge,
)
from .verify_collision_math import source_bytes


def compare_math(rows, sources: dict, spec: dict, ram: bytes, sweep_rows: list) -> dict:
    boundaries = [row for row in sweep_rows if "-sweep-" in row["hook"]]
    lookup = tables(sources)
    sqrt_bytes = source_bytes(sources, 0x80056A00, 0x80056B80)
    atan_bytes = source_bytes(sources, 0x80057030, 0x80057832)
    normal_bytes = source_bytes(sources, 0x80056B14, 0x80056D14)
    pending, sweep, boundary = {}, None, 0
    counts, cases = Counter(), Counter()
    for row, payload in ordered_rows(rows, spec, sources, ram):
        name, gpr = row["hook"], row["gpr_u32"]
        counts[name] += 1
        if "-sweep-" in name:
            require(
                boundary < len(boundaries) and not pending,
                "additional or interrupted original sweep boundary",
            )
            correlate(row, boundaries[boundary])
            boundary += 1
            if name.endswith("before"):
                require(sweep is None, "nested original math sweep")
                expected = sweep_result(row, payload, sources, lookup)
                edge = payload["edge"]
                for query in expected.queries:
                    if query.stage == "floor":
                        break
                    edge = write_edge(edge, query.result.edge)
                sweep = {
                    "before": row,
                    "old": payload,
                    "expected": expected,
                    "edge": edge,
                    "atan": 0,
                    "edge-normal": 0,
                    "edge-length": 0,
                    "slope-normal": 0,
                    "slope-length": 0,
                }
            else:
                require(sweep is not None, "orphan original math sweep return")
                expected = sweep["expected"]
                projected = expected.projection is not None
                slid = projected and expected.projection.branch == "slide"
                require(
                    sweep["atan"] == int(projected)
                    and sweep["edge-normal"] == sweep["edge-length"] == int(slid)
                    and sweep["slope-normal"]
                    == sweep["slope-length"]
                    == int(expected.slope is not None),
                    "original sweep intermediate stages omitted or added",
                )
                sweep = None
        elif name == "length-before":
            require(not pending, "nested or interrupted original length")
            pending["length"] = {"before": row, "square": 0, "sqrt": 0}
            if gpr[31] in (0x8007BD5C, 0x8007B7C4):
                require(sweep is not None, "original movement length outside sweep")
                expected = sweep["expected"]
                if gpr[31] == 0x8007BD5C:
                    require(
                        expected.slope is not None
                        and sweep["slope-normal"] == 1
                        and sweep["slope-length"] == 0,
                        "original slope length order differs",
                    )
                    values, result = expected.slope.length_input, expected.slope.length
                    frame, key = 0xA8, "slope-length"
                else:
                    require(
                        expected.projection is not None
                        and expected.projection.branch == "slide"
                        and sweep["edge-normal"] == 1
                        and sweep["edge-length"] == 0,
                        "original edge length order differs",
                    )
                    velocity = vector(sweep["old"]["velocity"])
                    values = velocity[0] >> 12, velocity[2] >> 12
                    result = expected.projection.length
                    frame = (
                        0xA8 if sweep["before"]["hook"].startswith("ordinary") else 0x78
                    ) + 0x48
                    key = "edge-length"
                call(row, sweep["before"], frame)
                require(
                    tuple(signed32(v) for v in gpr[4:6]) == values
                    and planar_length(*values, lookup.square_root) == result,
                    "original movement length inputs differ from sweep model",
                )
                sweep[key] += 1
            else:
                require(
                    sweep is None and gpr[31] in (0x80073AA4, 0x80074268),
                    "unqualified original length caller",
                )
        elif name == "length-square-after":
            require("length" in pending and "sqrt" not in pending, "orphan original length square")
            current = pending["length"]
            before = current["before"]
            values = signed32(before["gpr_u32"][4]), signed32(before["gpr_u32"][5]), 0
            call(row, before, 0x38, row["pc"])
            require(
                current["square"] == current["sqrt"] == 0
                and gpr[4] == gpr[29] + 0x10
                and gpr[5] == gpr[29] + 0x20
                and gpr[2] == gpr[5]
                and payload["input"] == struct.pack("<3i", *values)
                and payload["output"] == struct.pack("<3i", *(signed16(v) ** 2 for v in values)),
                "original signed-halfword square inputs, outputs or order differs",
            )
            current["square"] = 1
        elif name == "sqrt-before":
            require("sqrt" not in pending, "nested original square root")
            if "length" in pending:
                require(
                    pending["length"]["square"] == 1 and pending["length"]["sqrt"] == 0,
                    "original length square-root order differs",
                )
                call(row, pending["length"]["before"], 0x38, 0x80099A7C)
            else:
                require(
                    sweep is None and gpr[31] == 0x80099A3C,
                    "unqualified original square-root caller",
                )
            pending["sqrt"] = {
                "before": row,
                "old": payload,
                "expected": fixed_sqrt(gpr[4], lookup.square_root),
                "lookup": False,
            }
        elif name == "sqrt-lookup":
            require("sqrt" in pending, "orphan original square-root lookup")
            current = pending["sqrt"]
            expected = current["expected"]
            call(row, current["before"], 0, current["before"]["gpr_u32"][31])
            require(
                not current["lookup"]
                and expected.leading_zeroes != 32
                and payload["sqrt-table"] == sqrt_bytes
                and gpr[4] == current["before"]["gpr_u32"][4]
                and gpr[10] == expected.leading_zeroes & ~1
                and gpr[9] == expected.scale_shift
                and gpr[12] == expected.table_index + 64
                and gpr[2] == expected.leading_zeroes,
                "original square-root lookup table, index or scaling differs",
            )
            current["lookup"] = True
        elif name in ("sqrt-after", "sqrt-zero-tail"):
            require("sqrt" in pending, "orphan original square-root return")
            current = pending.pop("sqrt")
            expected, before = current["expected"], current["before"]
            call(row, before, 0, before["gpr_u32"][31])
            exact_payload(payload, current["old"], "square-root shared state")
            if name == "sqrt-after":
                require(
                    current["lookup"]
                    and gpr[2] == expected.value
                    and gpr[12] == expected.table_index * 2
                    and gpr[13] == expected.shifted_word,
                    "original square-root shifted word or result differs",
                )
            else:
                require(
                    not current["lookup"]
                    and before["gpr_u32"][4] == 0
                    and gpr[2] == 32
                    and "length" in pending,
                    "unqualified original square-root zero tail",
                )
            cases[(name, f"0x{before['gpr_u32'][31]:08x}")] += 1
            if "length" in pending:
                pending["length"].update(
                    {"sqrt": 1, "sqrt-input": before["gpr_u32"][4], "sqrt-value": expected.value}
                )
        elif name == "length-after":
            require(
                "length" in pending and "sqrt" not in pending,
                "orphan or premature original length return",
            )
            current = pending.pop("length")
            before = current["before"]
            values = signed32(before["gpr_u32"][4]), signed32(before["gpr_u32"][5]), 0
            squares = tuple(signed16(v) ** 2 for v in values)
            call(row, before, 0x38)
            require(
                current["square"] == current["sqrt"] == 1
                and current["sqrt-input"] == sum(squares) & 0xFFFFFFFF
                and gpr[2]
                == current["sqrt-value"]
                == planar_length(*values[:2], lookup.square_root)
                and u32(payload["length-frame"], 0x30) == before["gpr_u32"][31]
                and payload["length-frame"][0x10:0x1C] == struct.pack("<3i", *values)
                and payload["length-frame"][0x20:0x2C] == struct.pack("<3i", *squares),
                "original length inputs, nested result, caller or output differs",
            )
            cases[
                ("length", f"0x{before['gpr_u32'][31]:08x}", "zero" if gpr[2] == 0 else "nonzero")
            ] += 1
        elif name == "projection-atan-after":
            require(
                sweep is not None
                and not pending
                and sweep["expected"].projection is not None
                and sweep["atan"] == 0,
                "original projection atan order differs",
            )
            first, second = edge_points(sweep["edge"])
            expected = atan_angle(second[2] - first[2], second[0] - first[0], lookup.angle)
            frame = (0xA8 if sweep["before"]["hook"].startswith("ordinary") else 0x78) + 0x48
            call(row, sweep["before"], frame, row["pc"])
            require(
                payload["edge"] == sweep["edge"]
                and payload["velocity"] == sweep["old"]["velocity"]
                and 0 <= gpr[4] <= 1024
                and gpr[4] == expected.table_index
                and gpr[1] == 0x80050000 + gpr[4] * 2
                and payload["atan-word"] == atan_bytes[gpr[4] * 2 : gpr[4] * 2 + 2]
                and gpr[2] == expected.angle & 0xFFFFFFFF,
                "original edge atan inputs, lookup address, word or result differs",
            )
            sweep["atan"] += 1
            cases[("atan", expected.angle)] += 1
        elif name in ("edge-normal-after", "slope-normal-after"):
            require(
                sweep is not None and not pending and payload["normal-table"] == normal_bytes,
                "unqualified original sweep normal",
            )
            expected = sweep["expected"]
            if name == "edge-normal-after":
                require(
                    expected.projection is not None
                    and expected.projection.branch == "slide"
                    and sweep["atan"] == 1
                    and sweep["edge-normal"] == 0,
                    "original edge normal order differs",
                )
                result = expected.projection
                frame = (0xA8 if sweep["before"]["hook"].startswith("ordinary") else 0x78) + 0x48
                require(
                    payload["edge"] == sweep["edge"]
                    and payload["velocity"] == sweep["old"]["velocity"],
                    "original edge normal source differs",
                )
                key = "edge-normal"
            else:
                require(
                    expected.slope is not None and sweep["slope-normal"] == 0,
                    "original slope normal order differs",
                )
                result, frame, key = expected.slope, 0xA8, "slope-normal"
                floor_query = next(query for query in expected.queries if query.stage == "floor")
                require(
                    vector(payload["sweep-frame"][0x20:0x2C]) == floor_query.candidate
                    and s16(payload["sweep-frame"], 0x42) == floor_query.result.point[1]
                    and payload["actor"] == sweep["old"]["actor"],
                    "original slope input candidate, floor or actor differs",
                )
            call(row, sweep["before"], frame, row["pc"])
            require(
                vector(payload["input"]) == result.normal_input
                and vector(payload["output"]) == result.normal
                and result.normal == normalize(result.normal_input, lookup.reciprocal),
                "original sweep normal inputs or result differs",
            )
            sweep[key] += 1
            cases[(name, "zero" if result.normal == (0, 0, 0) else "nonzero")] += 1
        else:
            raise ValueError("unexpected original sweep-math stage")
    require(
        not pending and sweep is None and boundary == len(boundaries),
        "incomplete original sweep math",
    )
    return {
        "counts": dict(counts),
        "sweep_boundaries_correlated": boundary,
        "cases": [list(key) + [value] for key, value in sorted(cases.items())],
    }
