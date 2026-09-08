"""Source guards, memory provenance and call relations for original sprite traces."""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass

from ..reference.instruction_trace import validate_instruction_trace
from .field import field_components
from .original_trace import exact_payload, require
from .planar_motion import sprite_bundle_offsets
from .sprite_state import u32
from .sweep_trace import qualified_ranges
from .verify_collision_math import source_bytes

WINDOWS = (
    (0x8001D2B0, 0x8001D3F4),
    (0x8001F750, 0x8001FAB4),
    (0x80021CF8, 0x80021D3C),
    (0x80022000, 0x80022974),
    (0x80022D44, 0x80022DF4),
    (0x800234AC, 0x80023804),
    (0x800245D8, 0x80024C04),
    (0x8003F738, 0x8003F8B0),
    (0x8004974C, 0x80049870),
    (0x80049DCC, 0x80049EF0),
    (0x8004FC40, 0x8004FD40),
    (0x800523F0, 0x800563F0),
    (0x80073D00, 0x80073E00),
    (0x80081700, 0x80081A00),
    (0x800821F4, 0x8008237C),
    (0x80082BB8, 0x80083178),
)
FRAME_BYTES = {
    "field-animation": 32,
    "select": 32,
    "bind": 56,
    "orientation": 32,
    "matrix": 144,
    "replay": 48,
    "header": 32,
    "resource": 0,
    "frame": 32,
    "lookup": 24,
    "previous-frame": 64,
    "clear": 0,
}
RETURN_OFFSET = {
    "field-animation": 28,
    "select": 24,
    "bind": 52,
    "orientation": 24,
    "matrix": 140,
    "replay": 40,
    "header": 24,
    "frame": 24,
    "lookup": 16,
    "previous-frame": 56,
}
INSIDE_HOOKS = {
    "replay-step": "replay",
    "frame-list-step": "frame",
    "frame-insert": "frame",
    "previous-frame-part": "previous-frame",
}
CALLERS = {
    "field-animation": (0x800830F4, 0x80081998, 0x80081914),
    "select": (0x800822D0,),
    "bind": (0x80024698,),
    "header": (0x8002470C,),
    "matrix": (0x800236B8, 0x80022068),
    "orientation": (0x80073DB4, 0x80024718),
    "replay": (0x80022614,),
    "lookup": (0x80024990, 0x80022724),
    "frame": (0x80022DE4, 0x800249B8, 0x8002274C),
    "clear": (0x8001D320,),
    "previous-frame": (0x8001D398,),
    "resource": (),
}
PARENTS = {
    "animation": {
        "field-animation": (None,),
        "select": ("field-animation",),
        "bind": ("select",),
        "header": ("select",),
        "matrix": (None, "header"),
        "orientation": (None, "select"),
        "replay": ("orientation",),
        "resource": ("bind",),
    },
    "replay": {
        "orientation": (None,),
        "replay": ("orientation",),
        "lookup": (None, "replay"),
        "frame": (None, "lookup", "replay"),
        "clear": ("frame",),
        "previous-frame": ("frame",),
    },
}


def specification(sources: dict, ram: bytes, kind: str) -> dict:
    require(
        kind in PARENTS and sources["map"] == 23 and len(ram) == 0x200000,
        "unqualified original sprite trace kind, field or RAM",
    )
    table = u32(ram, 0xAFB10) - 0x80000000
    require(0 <= table <= len(ram) - 6 * 92, "original descriptor table outside RAM")

    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def register(name, index, size, offset=0):
        return {"name": name, "register": index, "relative_offset": offset, "size": size}

    common = [
        direct("field", 0x4F34C, 4),
        direct("descriptor-table", 0xAFB10, 4),
        direct("sprite-controls", 0x59190, 40),
        direct("collision-controls", 0xADB98, 120),
        direct("motion-globals", 0xB226C, 224),
    ]
    for name, index in (("player", 3), ("companion", 5)):
        descriptor = table + index * 92
        common.append(direct(name + "-descriptor", descriptor, 92))
        for target, offset, size in (("actor", 0x4C, 312), ("sprite", 4, 512)):
            common.append(
                {
                    "name": name + "-" + target,
                    "pointer_offset": descriptor + offset,
                    "relative_offset": 0,
                    "size": size,
                }
            )
    hooks = []

    def hook(name, pc, ranges):
        hooks.append(
            {
                "name": name,
                "pc": pc,
                "guard": {
                    "offset": pc - 16 - 0x80000000,
                    "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
                },
                "ranges": common + ranges,
            }
        )

    if kind == "animation":
        hook(
            "field-animation-before",
            0x800821F4,
            [register("sprite", 4, 512), register("descriptor", 6, 92)],
        )
        hook(
            "field-animation-after",
            0x80082360,
            [register("descriptor", 17, 92), register("frame", 29, 32)],
        )
        definitions = (
            ("select", 0x800245D8, 0x80024718, 16, 32),
            ("bind", 0x800222BC, 0x80022394, 17, 56),
            ("orientation", 0x800223B0, 0x80022648, 17, 32),
            ("matrix", 0x80022090, 0x80022208, 18, 144),
            ("replay", 0x80022660, 0x8002294C, 17, 48),
        )
    else:
        definitions = (
            ("orientation", 0x800223B0, 0x80022648, 17, 32),
            ("replay", 0x80022660, 0x8002294C, 17, 48),
            ("frame", 0x8001D2B0, 0x8001D3DC, 16, 32),
            ("previous-frame", 0x8001F8E8, 0x8001FA8C, 18, 64),
        )
    for name, begin, end, reg, frame in definitions:
        hook(name + "-before", begin, [register("sprite", 4, 512)])
        hook(name + "-after", end, [register("sprite", reg, 512), register("frame", 29, frame)])
    if kind == "animation":
        hook("header-before", 0x80023538, [register("sprite", 4, 512), register("header", 5, 32)])
        hook(
            "header-after",
            0x800237EC,
            [register("sprite", 16, 512), register("header", 17, 32), register("frame", 29, 32)],
        )
        hook(
            "resource-before",
            0x80022224,
            [register("binding", 4, 20), register("resource", 5, 32), register("frame", 29, 24)],
        )
        hook(
            "resource-after",
            0x800222B4,
            [register("binding", 8, 20), register("resource", 5, 32), register("frame", 29, 24)],
        )
    else:
        hook("replay-step", 0x8002268C, [register("sprite", 17, 512), register("frame", 29, 48)])
        hook("lookup-before", 0x80022D44, [register("sprite", 4, 512)])
        hook("lookup-after", 0x80022DE4, [register("frame", 29, 24)])
        hook("clear-before", 0x800234AC, [register("sprite", 4, 512)])
        hook("clear-after", 0x80023530, [register("sprite", 4, 512)])
        hook(
            "frame-list-step",
            0x8001D354,
            [
                register("sprite", 16, 512),
                register("list-sprite", 3, 512),
                register("frame", 29, 32),
            ],
        )
        hook("frame-insert", 0x8001D3B8, [register("sprite", 16, 512), register("frame", 29, 32)])
        hook(
            "previous-frame-part",
            0x8001F978,
            [
                register("sprite", 18, 512),
                register("source-command", 16, 16),
                register("frame", 29, 64),
            ],
        )
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"field23-{kind}-v1",
            "source_profile": sources["profile"]["id"],
            "start_frame": 4300,
            "end_frame": 5241,
            "max_callbacks": 16000 if kind == "animation" else 18000,
            "hooks": hooks,
        }
    )


class OriginalResources:
    """Original loader output plus source-derived field bundles, checked on every read."""

    def __init__(self, sources: dict, ram: bytes, party: dict[int, bytes]):
        require(
            len(ram) == 0x200000 and len(party) == 2, "incomplete original sprite resource inputs"
        )
        self.ram, self.resources, self.reads = ram, dict(party), Counter()
        for base, data in party.items():
            require(
                ram[base - 0x80000000 : base - 0x80000000 + len(data)] == data,
                "original party resource differs from qualified loader output",
            )
        component = field_components(sources["field_source"])[3]
        offsets = sprite_bundle_offsets(component.logical_data)
        base = u32(ram, 0xAFB1C)
        header = component.logical_data[: 4 * (len(offsets) + 1)]
        require(
            ram[base - 0x80000000 : base - 0x80000000 + len(header)] == header,
            "original field sprite bundle directory differs",
        )
        for begin, end in zip(offsets, offsets[1:] + (component.logical_size,), strict=True):
            require(base + begin not in self.resources, "duplicate original sprite resource base")
            self.resources[base + begin] = component.logical_data[begin:end]

    def read(self, pointer: int, size: int) -> bytes:
        matches = [
            (base, data)
            for base, data in self.resources.items()
            if base <= pointer and pointer + size <= base + len(data)
        ]
        require(size >= 0 and len(matches) == 1, "unqualified original sprite resource read")
        base, data = matches[0]
        result = data[pointer - base : pointer - base + size]
        require(
            result == self.ram[pointer - 0x80000000 : pointer - 0x80000000 + size],
            "original resource read differs from disc-derived bytes",
        )
        self.reads[(pointer, size)] += 1
        return result


def address(row: dict, name: str) -> int:
    item = next((r for r in row["ranges"] if r["name"] == name), None)
    require(
        item is not None and item.get("resolved_space", "ram") == "ram",
        "missing original RAM object range",
    )
    return item["resolved_offset"] + 0x80000000


def memory_candidates(row: dict, payload: dict, pointer: int, size: int) -> list[bytes]:
    result = []
    for item in row["ranges"]:
        if item.get("resolved_space", "ram") != "ram":
            continue
        start = item["resolved_offset"] + 0x80000000
        if start <= pointer and pointer + size <= start + item["size"]:
            result.append(payload[item["name"]][pointer - start : pointer - start + size])
    require(all(value == result[0] for value in result), "conflicting original memory aliases")
    return result


def read_captured(row: dict, payload: dict, pointer: int, size: int) -> bytes:
    values = memory_candidates(row, payload, pointer, size)
    require(bool(values), "original pointed storage was not captured")
    return values[0]


@dataclass
class OriginalCall:
    kind: str
    before: dict
    old: dict
    after: dict
    new: dict
    children: list[tuple[dict, dict]]


def calls(rows, spec: dict, kind: str):
    hooks = {h["name"]: h for h in spec["hooks"]}
    table = (
        next(r["offset"] for r in spec["hooks"][0]["ranges"] if r["name"] == "player-descriptor")
        - 3 * 92
        + 0x80000000
    )
    stack, previous = [], -1
    for index, row in enumerate(rows):
        run, name = row["frontend_run"], row["hook"]
        require(
            type(row["event"]) is int
            and row["event"] == index
            and type(run) is int
            and spec["start_frame"] <= run < spec["end_frame"]
            and run >= previous,
            "invalid original sprite event ordering",
        )
        require(name in hooks, "unexpected original sprite hook")
        previous = run
        payload = qualified_ranges(row, hooks[name])
        require(
            u32(payload["field"]) == 23 and u32(payload["descriptor-table"]) == table,
            "original sprite field or descriptor table changed",
        )
        gpr = row["gpr_u32"]
        if name in INSIDE_HOOKS:
            require(stack and stack[-1].kind == INSIDE_HOOKS[name], "orphan original helper step")
            before = stack[-1].before
            require(
                run == before["frontend_run"]
                and address(row, "sprite") == before["gpr_u32"][4]
                and gpr[29] == before["gpr_u32"][29] - FRAME_BYTES[stack[-1].kind],
                "original helper step changed its caller, sprite or stack",
            )
            if name == "replay-step":
                require(
                    gpr[20:22] == before["gpr_u32"][5:7], "original replay target registers differ"
                )
            for entry in stack:
                entry.children.append((row, payload))
            continue
        call_kind, boundary = name.rsplit("-", 1)
        if boundary == "before":
            require(
                call_kind in CALLERS and gpr[31] in CALLERS[call_kind],
                "unqualified original sprite caller",
            )
            require(
                (stack[-1].kind if stack else None) in PARENTS[kind][call_kind],
                "original sprite call nesting differs",
            )
            if kind == "replay":
                require(gpr[28] == 0x80059170, "original sprite GP context differs")
            if call_kind == "field-animation":
                require(
                    u32(payload["descriptor"], 4) == gpr[4],
                    "original descriptor selects another sprite",
                )
            for entry in stack:
                entry.children.append((row, payload))
            stack.append(OriginalCall(call_kind, row, payload, {}, {}, []))
            continue
        require(
            boundary == "after" and stack and stack[-1].kind == call_kind,
            "orphan or reordered original sprite return",
        )
        call = stack.pop()
        original = call.before["gpr_u32"]
        require(
            run == call.before["frontend_run"] and gpr[29] == original[29] - FRAME_BYTES[call_kind],
            "original sprite return run or frame differs",
        )
        require(
            (
                gpr[31]
                if call_kind in ("clear", "resource")
                else u32(payload["frame"], RETURN_OFFSET[call_kind])
            )
            == original[31],
            "original saved return address differs",
        )
        for name in payload.keys() & call.old.keys() - {"frame"}:
            require(
                address(row, name) == address(call.before, name),
                "original object storage changed across call",
            )
        if call_kind == "field-animation":
            require(
                address(row, "descriptor") == original[6],
                "original descriptor return lineage differs",
            )
        elif call_kind != "lookup":
            require(address(row, "sprite") == original[4], "original sprite return lineage differs")
        if call_kind == "header":
            require(
                address(row, "header") == original[5], "original animation header lineage differs"
            )
        call.after, call.new = row, payload
        for entry in stack:
            entry.children.append((row, payload))
        yield call
    require(not stack, "incomplete original sprite call")


def expected_payload(before: dict, old: dict, after: dict, stores: list[tuple[int, bytes]]) -> dict:
    result = {}
    for name in after.keys() - {"frame"}:
        require(name in old, "original output range lacks its input storage")
        start = address(before, name)
        data = bytearray(old[name])
        for pointer, payload in stores:
            lo, hi = max(start, pointer), min(start + len(data), pointer + len(payload))
            if lo < hi:
                data[lo - start : hi - start] = payload[lo - pointer : hi - pointer]
        result[name] = bytes(data)
    return result


def compare_effect(call: OriginalCall, stores: list[tuple[int, bytes]]) -> None:
    expected = expected_payload(call.before, call.old, call.new, stores)
    exact_payload(
        {k: v for k, v in call.new.items() if k != "frame"}, expected, "original " + call.kind
    )


def list_memory(call: OriginalCall, pointer: int, size: int) -> bytes:
    values = memory_candidates(call.before, call.old, pointer, size)
    if values:
        return values[0]
    for row, payload in call.children:
        if row["hook"] == "frame-list-step":
            start = address(row, "list-sprite")
            if start <= pointer and pointer + size <= start + len(payload["list-sprite"]):
                values.append(payload["list-sprite"][pointer - start : pointer - start + size])
    require(
        values and all(value == values[0] for value in values),
        "uncaptured or changing original frame list node",
    )
    return values[0]
