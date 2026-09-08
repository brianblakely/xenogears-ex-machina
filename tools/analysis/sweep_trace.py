"""Source-generated specifications and memory qualification for sweep traces.

This composes the existing RAM and scratchpad qualifiers without changing the
historical RAM-only verifier. Every pointer must resolve to captured storage;
overlapping aliases must agree before any semantic comparison runs.
"""

from __future__ import annotations

import struct

from ..reference.instruction_trace import validate_instruction_trace
from .collision_query import QueryActor
from .movement_sweep import SweepActor, SweepTables, movement_sweep, uses_ordinary_sweep
from .original_trace import qualified_ranges as ram_ranges
from .original_trace import require
from .verify_collision_math import address_region, source_bytes
from .verify_collision_math import qualified_ranges as collision_ranges

WINDOWS = (
    (0x8003F8B0, 0x8003F8E8),
    (0x80048C4C, 0x80048E94),
    (0x8004A414, 0x8004A43C),
    (0x8004A480, 0x8004A4D8),
    (0x8004A70C, 0x8004A730),
    (0x8004B32C, 0x8004B4AC),
    (0x800523F0, 0x800563F0),
    (0x80056A00, 0x80056D14),
    (0x80057030, 0x80057832),
    (0x8006FB8C, 0x8006FBCC),
    (0x8007B07C, 0x8007CD3C),
    (0x80082BB8, 0x80083178),
    (0x80099A4C, 0x80099A8C),
)
COMMON = (
    "field",
    "descriptor-table",
    "player-descriptor",
    "player-actor",
    "player-sprite",
    "mesh-globals",
    "query-controls",
    "collision-mode",
)


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def s16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def vector(data: bytes) -> tuple[int, int, int]:
    return struct.unpack("<3i", data)


def edge_points(data: bytes) -> tuple:
    require(len(data) == 16, "incomplete original edge")
    return struct.unpack_from("<3h", data), struct.unpack_from("<3h", data, 8)


def write_edge(data: bytes, points: tuple | None) -> bytes:
    result = bytearray(data)
    if points is not None:
        for offset, point in zip((0, 8), points, strict=True):
            struct.pack_into("<3h", result, offset, *point)
    return bytes(result)


def selected(payload: dict) -> dict:
    require(all(name in payload for name in COMMON), "missing shared original sweep state")
    return {name: payload[name] for name in COMMON}


def actor_state(data: bytes) -> SweepActor:
    require(len(data) == 312, "incomplete original actor")
    layer = s16(data, 0x10)
    require(0 <= layer < 4, "unqualified original actor layer")
    query = QueryActor(
        u32(data), u32(data, 4), layer, s16(data, 8 + 2 * layer), vector(data[0x20:0x2C])
    )
    return SweepActor(query, u32(data, 0x14), data[0x74], s16(data, 0x72), s16(data, 0xEC))


def tables(sources: dict) -> SweepTables:
    return SweepTables(
        sources["table"],
        struct.unpack("<192h", source_bytes(sources, 0x80056A00, 0x80056B80)),
        struct.unpack("<1025h", source_bytes(sources, 0x80057030, 0x80057832)),
        source_bytes(sources, 0x800523F0, 0x800563F0),
    )


def sweep_result(row: dict, payload: dict, sources: dict, lookup: SweepTables):
    state = actor_state(payload["actor"])
    ordinary = row["hook"].startswith("ordinary")
    require(
        ordinary == uses_ordinary_sweep(state, u32(payload["collision-mode"])),
        "original main caller selected a different sweep",
    )
    return movement_sweep(
        sources["component"],
        lookup,
        state,
        vector(payload["velocity"]),
        edge_points(payload["edge"]),
        row["gpr_u32"][7],
        ordinary=ordinary,
        collision_mode=u32(payload["collision-mode"]),
        attribute_control=payload["query-controls"][4],
    )


def specification(sources: dict, ram: bytes, route: str, kind: str) -> dict:
    require(
        route in ("control", "traversal") and kind in ("sweep", "area", "math"),
        "unknown original sweep route or capture kind",
    )
    require(len(ram) == 0x200000, "incomplete original specification RAM")
    descriptor = u32(ram, 0xAFB10) - 0x80000000 + 3 * 92
    require(0 <= descriptor <= len(ram) - 92, "original descriptor outside RAM")

    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def reg(name, index, size, relative=0):
        return {"name": name, "register": index, "relative_offset": relative, "size": size}

    common = [
        direct("field", 0x4F34C, 4),
        direct("descriptor-table", 0xAFB10, 4),
        direct("player-descriptor", descriptor, 92),
        {
            "name": "player-actor",
            "pointer_offset": descriptor + 0x4C,
            "relative_offset": 0,
            "size": 312,
        },
        {
            "name": "player-sprite",
            "pointer_offset": descriptor + 4,
            "relative_offset": 0,
            "size": 180,
        },
        direct("mesh-globals", 0xAFB18, 64),
        direct("query-controls", 0xB21C8, 8),
        direct("collision-mode", 0xADB98, 4),
    ]
    hooks = []

    def hook(name, pc, ranges, shared=common):
        hooks.append(
            {
                "name": name,
                "pc": pc,
                "guard": {
                    "offset": pc - 16 - 0x80000000,
                    "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
                },
                "ranges": shared + ranges,
            }
        )

    for name, pc in [("ordinary-sweep-before", 0x8007BAC0), ("special-sweep-before", 0x8007B814)]:
        hook(
            name,
            pc,
            [
                reg("velocity", 4, 12),
                reg("actor", 5, 312),
                reg("edge", 6, 16),
                reg("caller-stack", 29, 48),
            ],
        )
    hook(
        "ordinary-sweep-after",
        0x8007BEC0,
        [
            reg("velocity", 17, 12),
            reg("actor", 18, 312),
            reg("edge", 23, 16),
            reg("caller-stack", 29, 48, 0xA8),
        ],
    )
    hook(
        "special-sweep-after",
        0x8007BA90,
        [
            reg("velocity", 17, 12),
            reg("actor", 19, 312),
            reg("edge", 20, 16),
            reg("caller-stack", 29, 48, 0x78),
        ],
    )
    if kind == "math":
        field = [direct("field", 0x4F34C, 4)]
        definitions = [
            ("length-before", 0x80099A4C, [reg("caller-stack", 29, 32)]),
            ("length-after", 0x80099A7C, [reg("length-frame", 29, 56)]),
            ("sqrt-before", 0x80048C4C, []),
            ("sqrt-lookup", 0x80048CA0, [direct("sqrt-table", 0x56A00, 384)]),
            ("sqrt-after", 0x80048CC4, []),
            ("sqrt-zero-tail", 0x80048CCC, []),
            (
                "projection-atan-after",
                0x8007B714,
                [reg("edge", 16, 16), reg("velocity", 18, 12), reg("atan-word", 1, 2, 0x7030)],
            ),
            (
                "edge-normal-after",
                0x8007B7B0,
                [
                    reg("input", 29, 12, 0x10),
                    reg("output", 29, 12, 0x20),
                    reg("edge", 16, 16),
                    reg("velocity", 18, 12),
                    direct("normal-table", 0x56B14, 512),
                ],
            ),
            (
                "slope-normal-after",
                0x8007BD48,
                [
                    reg("input", 29, 12, 0x50),
                    reg("output", 29, 12, 0x60),
                    reg("sweep-frame", 29, 128),
                    reg("actor", 18, 312),
                    direct("normal-table", 0x56B14, 512),
                ],
            ),
            ("length-square-after", 0x80099A6C, [reg("input", 4, 12), reg("output", 5, 12)]),
        ]
        for name, pc, ranges in definitions:
            hook(name, pc, ranges, field)
        name, budget = f"field23-{route}-sweep-math-v2", 45000
    else:
        for name, pc in [
            ("ordinary-query-before", 0x8007BEF4),
            ("special-query-before", 0x8007C694),
        ]:
            hook(
                name,
                pc,
                [
                    reg("candidate", 4, 12),
                    reg("position", 5, 12),
                    reg("actor", 6, 312),
                    reg("edge", 7, 16),
                    reg("query-caller", 29, 176),
                ],
            )
        hook("ordinary-query-after", 0x8007C63C, [reg("edge", 23, 16), reg("query-frame", 29, 320)])
        hook("special-query-after", 0x8007CD08, [reg("edge", 23, 16), reg("query-frame", 29, 304)])
        if kind == "area":
            hooks = hooks[4:]
            locations = [
                (0x8007C0F0, 0x8007C104, 0x8007C11C, 0x8007C1A0, 0x8007C1DC, 0x8007C20C),
                (0x8007C82C, 0x8007C840, 0x8007C858, 0x8007C8DC, 0x8007C918, 0x8007C948),
            ]
            for prefix, pcs in zip(("ordinary", "special"), locations, strict=True):
                for index, pc in enumerate(pcs):
                    hook(f"{prefix}-area-{index}", pc, [])
            name = (
                "field23-query-signed-areas-v1"
                if route == "control"
                else "field23-east-ramp-query-areas-v1"
            )
            budget = 16000 if route == "control" else 45000
        else:
            hook("edge-projection-before", 0x8007B6C4, [reg("edge", 5, 16), reg("velocity", 6, 12)])
            hook(
                "edge-projection-after", 0x8007B7F4, [reg("edge", 16, 16), reg("velocity", 18, 12)]
            )
            hook(
                "height-before",
                0x8007B07C,
                [
                    reg("a", 4, 8),
                    reg("b", 5, 8),
                    reg("c", 6, 8),
                    reg("point", 7, 6),
                    reg("normal-pointer", 29, 4, 16),
                ],
            )
            hook("height-after", 0x8007B1A0, [reg("point", 19, 6), reg("normal", 20, 12)])
            for prefix, frame, table, pcs in [
                ("", 320, 0x6FB8C, (0x8007C07C, 0x8007C12C)),
                ("special-", 304, 0x6FBAC, (0x8007C7B8, 0x8007C868)),
            ]:
                for suffix, pc in zip(("loop", "decision"), pcs, strict=True):
                    hook(
                        f"{prefix}triangle-{suffix}",
                        pc,
                        [reg("query-frame", 29, frame), direct("triangle-dispatch", table, 32)],
                    )
            name = (
                "field23-sweeps-and-queries-v2"
                if route == "control"
                else "field23-east-ramp-sweeps-v1"
            )
            budget = 20000 if route == "control" else 45000
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": name,
            "source_profile": sources["profile"]["id"],
            "start_frame": 4300,
            "end_frame": 5241 if route == "control" else 6200,
            "max_callbacks": budget,
            "hooks": hooks,
        }
    )


def qualified_ranges(row: dict, hook: dict) -> dict:
    require(len(row["ranges"]) == len(hook["ranges"]), "original sweep range count changed")
    groups = {"ram": ([], []), "scratchpad": ([], [])}
    for actual, expected in zip(row["ranges"], hook["ranges"], strict=True):
        space = (
            address_region(row["gpr_u32"][expected["register"]])[0]
            if "register" in expected
            else "ram"
        )
        if space == "scratchpad":
            require(
                "pointer_offset" not in actual and "offset" not in actual,
                "substituted original scratchpad reference",
            )
        groups[space][0].append(actual)
        groups[space][1].append(expected)
    result, regions = {}, []
    for space, (actual, expected) in groups.items():
        if expected:
            values = (ram_ranges if space == "ram" else collision_ranges)(
                {**row, "ranges": actual}, {**hook, "ranges": expected}
            )
            require(not result.keys() & values.keys(), "duplicate original range name")
            result.update(values)
    for item in row["ranges"]:
        space, start, data = (
            item.get("resolved_space", "ram"),
            item["resolved_offset"],
            result[item["name"]],
        )
        for other_space, other_start, other in regions:
            begin, end = max(start, other_start), min(start + len(data), other_start + len(other))
            require(
                space != other_space
                or begin >= end
                or data[begin - start : end - start]
                == other[begin - other_start : end - other_start],
                "overlapping original sweep ranges disagree",
            )
        regions.append((space, start, data))
    return result


def mesh_state(sources: dict, ram: bytes) -> bytes:
    mesh = ram[0xAFB18:0xAFB58]
    base = u32(mesh)
    space, offset = address_region(base)
    require(
        space == "ram" and ram[offset : offset + len(sources["component"])] == sources["component"],
        "original sweep collision component differs from source",
    )
    require(
        u32(mesh, 8) == base + u32(sources["component"], 0x14),
        "original sweep attribute pointer differs from source",
    )
    for index, layer in enumerate(sources["layers"]):
        triangle, vertex = struct.unpack_from("<II", sources["component"], 0x18 + index * 8)
        require(
            tuple(u32(mesh, 12 + index * 4 + n) for n in (0, 16, 32))
            == (base + triangle, base + vertex, len(layer.triangles)),
            "original sweep layer pointers/counts differ from source",
        )
    return mesh


def ordered_rows(rows, spec: dict, sources: dict, ram: bytes):
    hooks = {h["name"]: h for h in spec["hooks"]}
    mesh, previous = mesh_state(sources, ram), -1
    for index, row in enumerate(rows):
        run = row["frontend_run"]
        require(
            type(row["event"]) is int
            and row["event"] == index
            and type(run) is int
            and spec["start_frame"] <= run < spec["end_frame"]
            and run >= previous,
            "original sweep event order differs",
        )
        require(row["hook"] in hooks, "unknown original sweep hook")
        payload = qualified_ranges(row, hooks[row["hook"]])
        require(u32(payload["field"]) == sources["map"], "original sweep enters another field")
        if "mesh-globals" in payload:
            require(payload["mesh-globals"] == mesh, "original collision globals changed")
        if "triangle-dispatch" in payload:
            begin = 0x8006FBAC if row["hook"].startswith("special") else 0x8006FB8C
            require(
                payload["triangle-dispatch"] == source_bytes(sources, begin, begin + 32),
                "original triangle dispatch differs from source",
            )
        previous = run
        yield row, payload


def read_captured(row: dict, pointer: int, size: int) -> bytes:
    space, start = address_region(pointer)
    matches = []
    for item in row["ranges"]:
        at = item["resolved_offset"]
        if (
            item.get("resolved_space", "ram") == space
            and at <= start
            and start + size <= at + item["size"]
        ):
            matches.append(bytes.fromhex(item["hex"])[start - at : start - at + size])
    require(
        matches and all(value == matches[0] for value in matches),
        "original requested bytes absent or inconsistent",
    )
    return matches[0]


def call(row: dict, before: dict, frame: int, return_address: int | None = None) -> None:
    require(
        row["frontend_run"] == before["frontend_run"]
        and row["gpr_u32"][29] == before["gpr_u32"][29] - frame
        and (return_address is None or row["gpr_u32"][31] == return_address),
        "original sweep call stack/run/return differs",
    )


def correlate(actual: dict, expected: dict) -> None:
    require(
        {k: v for k, v in actual.items() if k != "event"}
        == {k: v for k, v in expected.items() if k != "event"},
        "original boundaries differ between independent recordings",
    )
