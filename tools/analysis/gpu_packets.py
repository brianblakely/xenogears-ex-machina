"""Decode and summarize the GPU packets a PS1 program leaves in RAM.

The PS1 GPU consumes ordering tables: linked lists in RAM whose nodes carry a
24-bit next address and a word count, followed by that many GP0 command words.
This tool walks such a list in a RAM image (a capture snapshot or a RAM file),
decodes every GP0 command and summarizes the rendering state the program asks
for: primitive classes, semi-transparency modes, dithering, texture pages,
palettes, texture windows, drawing areas, offsets, mask settings and vertex
ranges. It also decodes libgpu DRAWENV/DISPENV structures and the GTE
projection and depth-cue control registers recorded with each snapshot.

Command encodings follow general PS1 GPU documentation (psx-spx); nothing here
is game-specific. Which list heads and environment structures to read is given
on the command line as address expressions (see `evaluate`).
"""

from __future__ import annotations

import argparse
import collections
import json
import re
import struct
import sys
from pathlib import Path

RAM_BYTES = 0x200000
TERMINATOR = 0x00FFFFFF
MAX_NODES = 1 << 20

# GTE control registers (index within the 32 control registers).
GTE_CONTROLS = {
    "RFC": 21,
    "GFC": 22,
    "BFC": 23,
    "OFX": 24,
    "OFY": 25,
    "H": 26,
    "DQA": 27,
    "DQB": 28,
    "ZSF3": 29,
    "ZSF4": 30,
}
REGISTERS = (
    "zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 "
    "s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra"
).split()


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s11(value: int) -> int:
    value &= 0x7FF
    return value - 0x800 if value & 0x400 else value


def s32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


def word(ram: bytes, address: int) -> int:
    offset = address & 0x1FFFFF
    if offset + 4 > len(ram):
        raise ValueError(f"Address {address:08x} is outside the RAM image")
    return struct.unpack_from("<I", ram, offset)[0]


def evaluate(expression: str, ram: bytes, registers: dict[str, int] | None = None) -> int:
    """Evaluate an address expression: hex/decimal numbers, register names of
    the snapshot record (`a0`), `[e]` for the word at address e, and + / -."""
    tokens = re.findall(r"0x[0-9a-fA-F]+|\d+|[a-z][a-z0-9]*|[\[\]+-]", expression)
    if "".join(tokens) != re.sub(r"\s+", "", expression):
        raise ValueError(f"Unsupported address expression: {expression}")
    position = 0

    def term() -> int:
        nonlocal position
        if position >= len(tokens):
            raise ValueError(f"Incomplete address expression: {expression}")
        token = tokens[position]
        position += 1
        if token == "[":
            value = word(ram, total())
            if position >= len(tokens) or tokens[position] != "]":
                raise ValueError(f"Unbalanced brackets: {expression}")
            position += 1
            return value
        if token[0].isdigit():
            return int(token, 0)
        if token in REGISTERS:
            if registers is None or token not in registers:
                raise ValueError(f"Register {token} is not available here")
            return registers[token]
        raise ValueError(f"Unknown token {token!r} in {expression}")

    def total() -> int:
        nonlocal position
        value = term()
        while position < len(tokens) and tokens[position] in "+-":
            sign = tokens[position]
            position += 1
            value = value + term() if sign == "+" else value - term()
        return value & 0xFFFFFFFF

    value = total()
    if position != len(tokens):
        raise ValueError(f"Trailing tokens in {expression}")
    return value


def texpage(value: int) -> dict:
    """A texture-page attribute (polygon texpage halfword or E1 bits 0-8)."""
    return {
        "x": (value & 0xF) * 64,
        "y": ((value >> 4) & 1) * 256,
        "semi": (value >> 5) & 3,
        "depth": (4, 8, 15, 15)[(value >> 7) & 3],
    }


def command_words(command: int, words: list[int], at: int) -> int:
    """The number of words the GP0 command at words[at] occupies."""
    if 0x20 <= command < 0x40:
        vertices = 4 if command & 0x08 else 3
        textured = 1 if command & 0x04 else 0
        gouraud = 1 if command & 0x10 else 0
        return 1 + vertices * (1 + textured) + (vertices - 1) * gouraud
    if 0x40 <= command < 0x60:
        gouraud = 1 if command & 0x10 else 0
        if not command & 0x08:
            return 3 + gouraud
        # Polyline: vertices until a terminator word (5xxx5xxx).
        count = 1
        while at + count < len(words):
            if count > 1 and (words[at + count] & 0xF000F000) == 0x50005000:
                return count + 1
            count += 1
        raise ValueError("Polyline without terminator")
    if 0x60 <= command < 0x80:
        size = (command >> 3) & 3
        return 2 + (1 if command & 0x04 else 0) + (1 if size == 0 else 0)
    if command == 0x02:
        return 3
    if 0x80 <= command < 0xA0:
        return 4
    if 0xA0 <= command < 0xC0:
        width = words[at + 2] & 0xFFFF if at + 2 < len(words) else 0
        height = words[at + 2] >> 16 if at + 2 < len(words) else 0
        return 3 + ((width * height + 1) // 2)
    if 0xC0 <= command < 0xE0:
        return 3
    return 1


def primitive_class(command: int) -> str:
    """A readable class name: shape, shading, texture, semi-transparency."""
    if 0x20 <= command < 0x40:
        parts = ["poly4" if command & 0x08 else "poly3"]
        parts.append("gouraud" if command & 0x10 else "flat")
        if command & 0x04:
            parts.append("textured-raw" if command & 0x01 else "textured-modulated")
        if command & 0x02:
            parts.append("semi")
        return "-".join(parts)
    if 0x40 <= command < 0x60:
        parts = ["polyline" if command & 0x08 else "line"]
        parts.append("gouraud" if command & 0x10 else "flat")
        if command & 0x02:
            parts.append("semi")
        return "-".join(parts)
    if 0x60 <= command < 0x80:
        size = ("variable", "1x1", "8x8", "16x16")[(command >> 3) & 3]
        parts = [f"rect-{size}"]
        if command & 0x04:
            parts.append("textured-raw" if command & 0x01 else "textured-modulated")
        if command & 0x02:
            parts.append("semi")
        return "-".join(parts)
    names = {
        0x00: "nop",
        0x01: "clear-cache",
        0x02: "fill",
        0x1F: "irq",
        0xE1: "draw-mode",
        0xE2: "texture-window",
        0xE3: "clip-top-left",
        0xE4: "clip-bottom-right",
        0xE5: "draw-offset",
        0xE6: "mask",
    }
    if command in names:
        return names[command]
    if 0x80 <= command < 0xA0:
        return "vram-copy"
    if 0xA0 <= command < 0xC0:
        return "cpu-to-vram"
    if 0xC0 <= command < 0xE0:
        return "vram-to-cpu"
    return f"unknown-{command:02x}"


def decode_command(words: list[int]) -> dict:
    """Decode one GP0 command (its complete words)."""
    command = words[0] >> 24
    item: dict = {"command": command, "class": primitive_class(command)}
    if 0x20 <= command < 0x40:
        count = 4 if command & 0x08 else 3
        textured = bool(command & 0x04)
        gouraud = bool(command & 0x10)
        colors, points, uvs = [words[0] & 0xFFFFFF], [], []
        at = 1
        for vertex in range(count):
            if gouraud and vertex > 0:
                colors.append(words[at] & 0xFFFFFF)
                at += 1
            points.append((s11(words[at]), s11(words[at] >> 16)))
            at += 1
            if textured:
                uvs.append(words[at])
                at += 1
        item["points"] = points
        item["colors"] = colors
        if textured:
            item["uv"] = [(v & 0xFF, (v >> 8) & 0xFF) for v in uvs]
            item["clut"] = ((uvs[0] >> 16) & 0x3F) * 16, (uvs[0] >> 22) & 0x1FF
            item["texpage"] = (uvs[1] >> 16) & 0xFFFF
    elif 0x40 <= command < 0x60:
        gouraud = bool(command & 0x10)
        points, colors, at = [], [words[0] & 0xFFFFFF], 1
        while at < len(words):
            if (words[at] & 0xF000F000) == 0x50005000 and command & 0x08 and len(points) >= 2:
                break
            if gouraud and points:
                colors.append(words[at] & 0xFFFFFF)
                at += 1
            points.append((s11(words[at]), s11(words[at] >> 16)))
            at += 1
        item["points"] = points
        item["colors"] = colors
    elif 0x60 <= command < 0x80:
        item["colors"] = [words[0] & 0xFFFFFF]
        x, y = s11(words[1]), s11(words[1] >> 16)
        at = 2
        if command & 0x04:
            item["uv"] = [(words[2] & 0xFF, (words[2] >> 8) & 0xFF)]
            item["clut"] = ((words[2] >> 16) & 0x3F) * 16, (words[2] >> 22) & 0x1FF
            at = 3
        size = (command >> 3) & 3
        if size == 0:
            w, h = words[at] & 0xFFFF, words[at] >> 16
        else:
            w = h = (1, 8, 16)[size - 1]
        item["points"] = [(x, y), (x + w, y + h)]
    elif command == 0x02:
        item["colors"] = [words[0] & 0xFFFFFF]
        item["rect"] = (
            words[1] & 0xFFFF,
            words[1] >> 16,
            words[2] & 0xFFFF,
            words[2] >> 16,
        )
    elif command == 0xE1:
        value = words[0]
        item["texpage"] = value & 0x1FF
        item["dither"] = (value >> 9) & 1
        item["draw_to_display"] = (value >> 10) & 1
        item["texture_disable"] = (value >> 11) & 1
    elif command == 0xE2:
        value = words[0]
        item["window"] = {
            "mask_x": (value & 0x1F) * 8,
            "mask_y": ((value >> 5) & 0x1F) * 8,
            "offset_x": ((value >> 10) & 0x1F) * 8,
            "offset_y": ((value >> 15) & 0x1F) * 8,
        }
    elif command in (0xE3, 0xE4):
        item["point"] = (words[0] & 0x3FF, (words[0] >> 10) & 0x3FF)
    elif command == 0xE5:
        item["offset"] = (s11(words[0]), s11(words[0] >> 11))
    elif command == 0xE6:
        item["set_mask"] = words[0] & 1
        item["check_mask"] = (words[0] >> 1) & 1
    return item


def decode_words(words: list[int]) -> list[dict]:
    """Decode a packet body: consecutive GP0 commands."""
    result, at = [], 0
    while at < len(words):
        command = words[at] >> 24
        size = command_words(command, words, at)
        if at + size > len(words):
            raise ValueError(f"GP0 command {command:02x} is truncated in its packet")
        result.append(decode_command(words[at : at + size]))
        at += size
    return result


def walk(ram: bytes, head: int) -> tuple[list[dict], dict]:
    """Walk an ordering table from `head`; returns the decoded commands in
    draw order (each with its packet address) and list statistics."""
    commands, address, nodes, empty = [], head & 0xFFFFFF, 0, 0
    seen = set()
    while address != TERMINATOR:
        if address in seen:
            raise ValueError(f"Ordering table loops at {address:06x}")
        if len(seen) >= MAX_NODES:
            raise ValueError("Ordering table exceeds the node limit")
        seen.add(address)
        tag = word(ram, address)
        count = tag >> 24
        nodes += 1
        if count == 0:
            empty += 1
        else:
            body = [word(ram, address + 4 + 4 * i) for i in range(count)]
            for item in decode_words(body):
                item["packet"] = 0x80000000 | address
                commands.append(item)
        address = tag & 0xFFFFFF
    return commands, {"nodes": nodes, "empty_nodes": empty, "packets": nodes - empty}


def draw_environment(ram: bytes, address: int) -> dict:
    """libgpu DRAWENV (clip, offset, texture window, tpage, dtd, dfe, isbg,
    background color) plus the GP0 words of its dr_env packet."""
    base = address & 0x1FFFFF
    if base + 0x5C > len(ram):
        raise ValueError("DRAWENV is outside the RAM image")
    h = struct.unpack_from("<10h", ram, base)
    tag = word(ram, address + 0x1C)
    count = min(tag >> 24, 15)
    body = [word(ram, address + 0x20 + 4 * i) for i in range(count)]
    return {
        "clip": h[0:4],
        "offset": h[4:6],
        "texture_window": h[6:10],
        "tpage": struct.unpack_from("<H", ram, base + 0x14)[0],
        "dtd": ram[base + 0x16],
        "dfe": ram[base + 0x17],
        "isbg": ram[base + 0x18],
        "background": tuple(ram[base + 0x19 : base + 0x1C]),
        "packet": [summarize_state(item) for item in decode_words(body)],
    }


def display_environment(ram: bytes, address: int) -> dict:
    """libgpu DISPENV: display area, screen area, interlace and 24-bit flags."""
    base = address & 0x1FFFFF
    if base + 0x14 > len(ram):
        raise ValueError("DISPENV is outside the RAM image")
    h = struct.unpack_from("<8h", ram, base)
    return {
        "disp": h[0:4],
        "screen": h[4:8],
        "isinter": ram[base + 0x10],
        "isrgb24": ram[base + 0x11],
    }


def summarize_state(item: dict) -> str:
    """A compact text form of a state command (E1-E6, fill)."""
    command = item["command"]
    if command == 0xE1:
        page = texpage(item["texpage"])
        return (
            f"E1 page=({page['x']},{page['y']}) semi={page['semi']} depth={page['depth']} "
            f"dither={item['dither']} draw_to_display={item['draw_to_display']}"
        )
    if command == 0xE2:
        w = item["window"]
        return f"E2 mask=({w['mask_x']},{w['mask_y']}) offset=({w['offset_x']},{w['offset_y']})"
    if command in (0xE3, 0xE4):
        return f"{command:02X} {item['point']}"
    if command == 0xE5:
        return f"E5 {item['offset']}"
    if command == 0xE6:
        return f"E6 set={item['set_mask']} check={item['check_mask']}"
    if command == 0x02:
        return f"fill {item['rect']} color={item['colors'][0]:06x}"
    return item["class"]


def oversized_triangles(points: list[tuple[int, int]]) -> list[str]:
    """The triangles (vertices 0-1-2, and 1-2-3 of a quad) whose vertices lie
    more than 1023 pixels apart horizontally or 511 vertically; the GPU does
    not draw those (psx-spx)."""
    triangles = [(0, 1, 2)] + ([(1, 2, 3)] if len(points) == 4 else [])
    result = []
    for triangle in triangles:
        xs = [points[i][0] for i in triangle]
        ys = [points[i][1] for i in triangle]
        if max(xs) - min(xs) > 1023 or max(ys) - min(ys) > 511:
            result.append("-".join(map(str, triangle)))
    return result


class Census:
    """Counts over decoded commands, tracking the E1 state in draw order."""

    def __init__(self) -> None:
        self.classes: collections.Counter = collections.Counter()
        self.states: collections.Counter = collections.Counter()
        self.semi: collections.Counter = collections.Counter()
        self.dither: collections.Counter = collections.Counter()
        self.pages: collections.Counter = collections.Counter()
        self.cluts: collections.Counter = collections.Counter()
        self.modulation: collections.Counter = collections.Counter()
        self.limits: collections.Counter = collections.Counter()
        self.x = [None, None]
        self.y = [None, None]
        self.lists: collections.Counter = collections.Counter()

    def add(self, commands: list[dict], mode: dict | None) -> None:
        """`mode` is the E1 state before the list (from the draw environment),
        or None when unknown."""
        state = dict(mode) if mode is not None else None
        for item in commands:
            name = item["class"]
            self.classes[name] += 1
            command = item["command"]
            if command >= 0xE1 or command == 0x02:
                self.states[summarize_state(item)] += 1
            if command == 0xE1:
                state = {"texpage": item["texpage"], "dither": item["dither"]}
            if "points" in item:
                for x, y in item["points"]:
                    self.x[0] = x if self.x[0] is None else min(self.x[0], x)
                    self.x[1] = x if self.x[1] is None else max(self.x[1], x)
                    self.y[0] = y if self.y[0] is None else min(self.y[0], y)
                    self.y[1] = y if self.y[1] is None else max(self.y[1], y)
            if not 0x20 <= command < 0x80:
                continue
            polygon = command < 0x40
            textured = bool(command & 0x04) and command not in range(0x40, 0x60)
            if polygon and textured:
                # A textured polygon's texpage attribute replaces E1 bits 0-8.
                if state is not None:
                    state = {"texpage": item["texpage"], "dither": state["dither"]}
                page = texpage(item["texpage"])
            elif state is not None:
                page = texpage(state["texpage"])
            else:
                page = None
            if textured:
                key = f"page=({page['x']},{page['y']}) depth={page['depth']}" if page else "?"
                self.pages[key] += 1
                if page is None or page["depth"] != 15:
                    self.cluts[f"{item['clut'][0]},{item['clut'][1]}"] += 1
                raw = bool(command & 0x01)
                if raw:
                    self.modulation["raw"] += 1
                else:
                    neutral = all(c == 0x808080 for c in item["colors"])
                    self.modulation["modulated-neutral" if neutral else "modulated"] += 1
            if command & 0x02:
                self.semi[f"{name} mode={page['semi'] if page else '?'}"] += 1
            gouraud = bool(command & 0x10) and command < 0x60
            blended = textured and not command & 0x01
            if gouraud or blended:
                bit = state["dither"] if state is not None else "?"
                self.dither[f"{name} dither={bit}"] += 1
            if polygon:
                for triangle in oversized_triangles(item["points"]):
                    self.limits[f"triangle-over-size-limit {triangle}"] += 1

    def add_list(self, stats: dict) -> None:
        for key, value in stats.items():
            self.lists[key] += value

    def result(self) -> dict:
        return {
            "classes": dict(sorted(self.classes.items())),
            "state_commands": dict(sorted(self.states.items())),
            "semi_transparency": dict(sorted(self.semi.items())),
            "dither": dict(sorted(self.dither.items())),
            "texture_pages": dict(sorted(self.pages.items())),
            "cluts": len(self.cluts),
            "clut_rows": sorted({int(k.split(",")[1]) for k in self.cluts}),
            "modulation": dict(sorted(self.modulation.items())),
            "hardware_limits": dict(self.limits),
            "vertex_x": self.x,
            "vertex_y": self.y,
            "lists": dict(self.lists),
        }


def gte_controls(cop2: list[int]) -> dict:
    """GTE projection and depth-cue controls from a record's 64 COP2 words."""
    if len(cop2) != 64:
        raise ValueError("A COP2 record needs 32 data and 32 control words")
    control = cop2[32:]
    return {
        "H": control[26] & 0xFFFF,
        "OFX": s32(control[24]) / 65536,
        "OFY": s32(control[25]) / 65536,
        "DQA": s16(control[27]),
        "DQB": s32(control[28]),
        "ZSF3": s16(control[29]),
        "ZSF4": s16(control[30]),
        "far_color": [s32(control[i]) >> 4 for i in (21, 22, 23)],
    }


def initial_mode(env: dict | None) -> dict | None:
    if env is None:
        return None
    return {"texpage": env["tpage"] & 0x1FF, "dither": 1 if env["dtd"] else 0}


def trace_rows(capture: Path):
    for line in (capture / "instruction-trace.jsonl").open():
        row = json.loads(line)
        if "hook" in row:
            yield row


def census_capture(args: argparse.Namespace) -> dict:
    from tools.reference.instruction_trace import SnapshotReader, snapshot_path

    capture = Path(args.capture)
    trace = capture / "instruction-trace.jsonl"
    snapshots = SnapshotReader(snapshot_path(trace))
    total = Census()
    records, environments, gte = [], collections.Counter(), collections.Counter()
    errors: list[dict] = []
    first, last = args.frames
    matching = 0
    for row in trace_rows(capture):
        if row["hook"] != args.hook or "snapshot" not in row:
            continue
        if not first <= row["frontend_run"] <= last:
            continue
        matching += 1
        if (matching - 1) % args.stride:
            continue
        if len(records) >= args.limit:
            break
        ram = snapshots.read(row)[0]
        registers = dict(zip(REGISTERS, row["gpr_u32"][:32], strict=True))
        try:
            result = census_ram(ram, args, registers)
        except ValueError as error:
            # A list that is not (yet) a complete submitted list, e.g. a
            # buffer a program has not built before its first frame.
            errors.append({"frontend_run": row["frontend_run"], "error": str(error)})
            continue
        result["frontend_run"] = row["frontend_run"]
        result["gte"] = gte_controls(row["cop2_u32"])
        gte[json.dumps(result["gte"], sort_keys=True)] += 1
        for key in ("draw_environment", "display_environment"):
            if key in result:
                environments[json.dumps({key: result[key]}, sort_keys=True)] += 1
        total.add(result.pop("_commands"), result.pop("_mode"))
        total.add_list(result["lists"])
        records.append(result)
    if not records:
        raise ValueError("No snapshot record of that hook in the frame range")
    return {
        "capture": str(capture),
        "hook": args.hook,
        "ot": args.ot,
        "records": len(records),
        "unreadable": errors,
        "frontend_runs": [records[0]["frontend_run"], records[-1]["frontend_run"]],
        "census": total.result(),
        "environments": {k: v for k, v in environments.items()},
        "gte": {k: v for k, v in gte.items()},
        "per_record": [
            {
                "frontend_run": r["frontend_run"],
                "heads": r["heads"],
                "lists": r["lists"],
                "classes": sum(r["census"]["classes"].values()),
            }
            for r in records
        ]
        if args.per_record
        else None,
    }


def census_ram(ram: bytes, args: argparse.Namespace, registers: dict | None = None) -> dict:
    result: dict = {}
    env = None
    if args.drawenv:
        env = draw_environment(ram, evaluate(args.drawenv, ram, registers))
        result["draw_environment"] = env
    if args.dispenv:
        result["display_environment"] = display_environment(
            ram, evaluate(args.dispenv, ram, registers)
        )
    census = Census()
    commands, heads, stats = [], [], collections.Counter()
    for expression in args.ot:
        head = evaluate(expression, ram, registers)
        items, list_stats = walk(ram, head)
        heads.append(f"{head:08x}")
        commands += items
        stats.update(list_stats)
    mode = initial_mode(env)
    census.add(commands, mode)
    census.add_list(stats)
    result["heads"] = heads
    result["lists"] = dict(stats)
    result["census"] = census.result()
    result["_commands"] = commands
    result["_mode"] = mode
    return result


def frames(text: str) -> tuple[int, int]:
    first, _, last = text.partition(":")
    return int(first), int(last or first)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ot", action="append", default=[], help="List head expression")
    parser.add_argument("--drawenv", help="DRAWENV address expression")
    parser.add_argument("--dispenv", help="DISPENV address expression")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--capture", help="Capture directory with instruction-trace.jsonl")
    source.add_argument("--ram", help="Raw 2 MiB RAM image")
    parser.add_argument("--hook", help="Snapshot hook name (with --capture)")
    parser.add_argument("--frames", type=frames, default=(0, 1 << 31), help="FIRST:LAST runs")
    parser.add_argument("--limit", type=int, default=64, help="Maximum snapshot records")
    parser.add_argument("--stride", type=int, default=1, help="Use every Nth matching record")
    parser.add_argument("--per-record", action="store_true", help="List each record")
    args = parser.parse_args(argv)
    if args.capture:
        if not args.hook:
            parser.error("--capture needs --hook")
        result = census_capture(args)
    else:
        ram = Path(args.ram).read_bytes()
        if len(ram) != RAM_BYTES:
            parser.error("A RAM image is exactly 2 MiB")
        result = census_ram(ram, args)
        del result["_commands"], result["_mode"]
    json.dump(result, sys.stdout, indent=1)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
