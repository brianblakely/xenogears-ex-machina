"""Battle event-script VM (ovl3087): opcode table, decoder and both-disc sweep.

ovl3087 (decomp/src/ovl3087/battle_event_script_vm.c, matching) runs a battle's event
script when the formation sets 800c3d48. 801e879c dispatches code[pc] (u8)
through a 76-case switch (00-4b) to one handler per opcode; each returns the
instruction length, or 0 while it waits. Operands are little-endian: 801e57f8
decodes 16-bit operands from byte 1 on, each an immediate or a variable
offset (by a mask byte, or bit 15 in the signed form). Jump targets are
absolute offsets into the bytecode. Opcodes without a case leave the previous
length in place; they are reported, never skipped.

Script archive (file 2 of directory 0x20, beside ovl3087 as file 1): a
relocatable table of packed blocks (80032e88), a script and its message data
per set. A script holds 0x40 bytes, the thread count (u32), 16 bytes per
thread (eight u16 entry offsets: 0 start, 1 idle, others requested by 03-05),
then the bytecode.
"""

from __future__ import annotations

import argparse
import struct
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from tools.analysis.battle_effect_vm import Disc
from tools.analysis.packed import decode_block

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = 0x20 + 0  # directory of ovl3087: file 2 is the script archive

NEXT, END, JUMP, BRANCH = "next", "end", "jump", "branch"

# Operand kinds: u8/u16 immediates, var (a variable offset), masked (801e57f8
# with the mask byte at insn[5]), signed (801e57f8 signed form), target.
U8, U16, VAR, MASKED, SIGNED, TARGET = "u8", "u16", "var", "masked", "signed", "target"


@dataclass(frozen=True)
class EventOp:
    name: str
    handler: str
    length: int
    operands: tuple[tuple[str, int, str], ...] = ()  # (name, byte offset, kind)
    flow: str = NEXT
    effect: str = ""


def _op(name, handler, length, operands=(), flow=NEXT, effect=""):
    return EventOp(name, handler, length, tuple(operands), flow, effect)


def _signed(*names):
    return [(n, 1 + 2 * i, SIGNED) for i, n in enumerate(names)]


VAR_VALUE = [("var", 1, MASKED), ("value", 3, MASKED), ("mask", 5, U8)]

OPCODES = {
    0x00: _op(
        "end", "801e5c1c", 1, flow=END, effect="drop the level; the thread restarts at entry 1"
    ),
    0x01: _op("jump", "801e5ce4", 3, [("target", 1, TARGET)], JUMP),
    0x02: _op(
        "branch_unless",
        "801e5d24",
        8,
        [("a", 1, MASKED), ("b", 3, MASKED), ("condition", 5, U8), ("target", 6, TARGET)],
        BRANCH,
        "jump to target unless compare(a, b) by condition & 0xf (801e58ec)",
    ),
    0x03: _op(
        "request",
        "801e5dcc",
        3,
        [("thread", 1, U8), ("entry_priority", 2, U8)],
        effect="start entry (low 5 bits) of thread on a free level, priority top 3 bits",
    ),
    0x04: _op(
        "request_wait_start",
        "801e5ef8",
        3,
        [("thread", 1, U8), ("entry_priority", 2, U8)],
        effect="request, then wait until the thread runs the entry",
    ),
    0x05: _op(
        "request_wait_end",
        "801e5f8c",
        3,
        [("thread", 1, U8), ("entry_priority", 2, U8)],
        effect="request, then wait until the entry finished",
    ),
    0x06: _op("set", "801e6084", 6, VAR_VALUE, effect="var = value"),
    0x07: _op("set_one", "801e60e8", 3, [("var", 1, VAR)], effect="var = 1"),
    0x08: _op("set_zero", "801e6118", 3, [("var", 1, VAR)], effect="var = 0"),
    0x09: _op("add", "801e6144", 6, VAR_VALUE, effect="var += value"),
    0x0A: _op("subtract", "801e61b4", 6, VAR_VALUE, effect="var -= value"),
    0x0B: _op("or", "801e6224", 6, VAR_VALUE, effect="var |= value"),
    0x0C: _op("clear_bits", "801e6294", 6, VAR_VALUE, effect="var &= ~value"),
    0x0D: _op("increment", "801e6304", 3, [("var", 1, VAR)], effect="var++"),
    0x0E: _op("decrement", "801e633c", 3, [("var", 1, VAR)], effect="var--"),
    0x0F: _op("and", "801e6374", 6, VAR_VALUE, effect="var &= value"),
    0x10: _op("or_10", "801e63e4", 6, VAR_VALUE, effect="var |= value (as 0b)"),
    0x11: _op("xor", "801e6454", 6, VAR_VALUE, effect="var ^= value"),
    0x12: _op(
        "shift_left", "801e64c4", 5, [("var", 1, VAR), ("count", 3, VAR)], effect="var <<= count"
    ),
    0x13: _op(
        "shift_right", "801e6534", 5, [("var", 1, VAR), ("count", 3, VAR)], effect="var >>= count"
    ),
    0x14: _op("random", "801e65a4", 3, [("var", 1, VAR)], effect="var = random 0..7fff"),
    0x15: _op(
        "random_below",
        "801e65fc",
        5,
        [("limit", 1, U16), ("var", 3, VAR)],
        effect="var = random 0..limit",
    ),
    0x16: _op("multiply", "801e6660", 6, VAR_VALUE, effect="var = a * value (a: the var operand)"),
    0x17: _op("divide", "801e66d8", 6, VAR_VALUE, effect="var = a / value (signed)"),
    0x18: _op(
        "message",
        "801e71d4",
        4,
        [("message", 1, U16), ("flags", 3, U8)],
        effect="show the speaker's message; wait until dismissed",
    ),
    0x19: _op(
        "message_from",
        "801e7230",
        5,
        [("actor", 1, U8), ("message", 2, U16), ("flags", 4, U8)],
        effect="show an actor's message; wait until dismissed",
    ),
    0x1A: _op(
        "window",
        "801e7278",
        11,
        _signed("x", "y", "width", "height", "flags"),
        effect="message window layout (0: default)",
    ),
    0x1B: _op(
        "speaker", "801e7314", 2, [("actor", 1, U8)], effect="the thread's speaker (f3-f5 party)"
    ),
    0x1C: _op("enter_script_mode", "801e7358", 1, effect="battle frame mode (800c3e4c) = 2"),
    0x1D: _op("enter_turn_mode", "801e736c", 1, effect="battle frame mode (800c3e4c) = 1"),
    0x1E: _op(
        "fade_white",
        "801e7380",
        3,
        _signed("frames"),
        effect="fade the screen to white over 2 * frames frames (blend 2, 800b39c0)",
    ),
    0x1F: _op(
        "fade_black", "801e73d4", 3, _signed("frames"), effect="fade the screen to black (as 1e)"
    ),
    0x20: _op("halt_until_battle_end", "801e746c", 1, effect="800c3d44 = 1, halt the script"),
    0x21: _op("skip_result_screens", "801e748c", 1, effect="800d2d50 = 1"),
    0x22: _op(
        "last_pass", "801e74a0", 1, effect="this pass of 801e879c is its last (back to the battle)"
    ),
    0x23: _op(
        "act",
        "801e74e0",
        7,
        _signed("object", "slot", "script"),
        effect="object - f3 acts on slot + 13 with effect script (800aa384); wait until the "
        "effect reports done (its 02/03), finish the loads",
    ),
    0x24: _op(
        "next_battle",
        "801e7700",
        5,
        _signed("formation", "kind"),
        effect="pending formation (8005947c) = formation + 1, battle kind (8005954c) = kind:"
        " the next battle's formation of the same set (resident 8001b6c4, battle 80070f40)",
    ),
    0x25: _op("allow_defeat", "801e775c", 1, effect="800c3d5c = 1"),
    0x26: _op(
        "set_saved_map",
        "801e7770",
        9,
        _signed("a", "b", "c", "d"),
        effect="8006f94e[0..3] = a..d, clear state word 8004f30c (8001ac94)",
    ),
    0x27: _op(
        "request_movie",
        "801e77e4",
        9,
        _signed("a", "b", "c", "d"),
        effect="8004fe44 = a | 0x80, b, 1, c; 800d3338 = 1; 80062514 = d",
    ),
    0x28: _op(
        "fade",
        "801e786c",
        6,
        [("blend", 1, U8), ("r", 2, U8), ("g", 3, U8), ("b", 4, U8), ("frames", 5, U8)],
        effect="fade the screen to a colour in a blend mode over 2 * frames frames (800b39c0)",
    ),
    0x29: _op(
        "quake",
        "801e78a8",
        9,
        _signed("x", "y", "z", "frames"),
        effect="quake the view towards amplitude (x, y, z) over 2 * frames frames (800b3658)",
    ),
    0x2A: _op(
        "model_animation",
        "801e79e0",
        5,
        _signed("slot", "animation"),
        effect="animate slot's loaded model (801e9958)",
    ),
    0x2B: _op(
        "wait",
        "801e7b58",
        3,
        _signed("count"),
        effect="wait until the thread timer (2 * count) runs out",
    ),
    0x2C: _op(
        "run_order",
        "801e7c0c",
        3,
        [("order", 1, U8), ("unused", 2, U8)],
        effect="the thread's order request; fe runs it first at once",
    ),
    0x2D: _op(
        "music", "801e7e14", 3, _signed("music"), effect="start music (file music + 4) at volume 7f"
    ),
    0x2E: _op("music_silent", "801e7e5c", 3, _signed("music"), effect="start music at volume 0"),
    0x2F: _op(
        "music_fade",
        "801e7ea4",
        5,
        _signed("volume", "time"),
        effect="fade the music, keep the volume",
    ),
    0x30: _op(
        "music_volume",
        "801e7f08",
        3,
        _signed("silence"),
        effect="0: restore the stored volume, else silence",
    ),
    0x31: _op(
        "sound",
        "801e7f70",
        9,
        _signed("sound", "volume", "pan", "resident"),
        effect="play a sound of the script (or resident) bank (80039f18)",
    ),
    0x32: _op("nop", "801e8074", 1),
    0x33: _op("stop_music", "801e807c", 1),
    0x34: _op("nop_34", "801e80e8", 1),
    0x35: _op(
        "load_model",
        "801e7914",
        5,
        _signed("slot", "model"),
        effect="create model n of the model archive in slot (thread slot + 13)",
    ),
    0x36: _op("free_model", "801e7b08", 3, _signed("slot")),
    0x37: _op("exit_battle", "801e74b8", 1, effect="800d2fc4 = 1, outcome 800c48ea = 1, halt"),
    0x38: _op(
        "act_nowait",
        "801e75f0",
        7,
        _signed("object", "slot", "script"),
        effect="object - f3 starts effect script on slot + 13 (800aa320)",
    ),
    0x39: _op(
        "slot0_to_gear",
        "801e80f0",
        1,
        effect="party slot 0 changes to its gear (80088490, 800baf48, 800883ac) and battle flags",
    ),
    0x3A: _op("actor_animation", "801e818c", 5, _signed("actor", "animation"), effect="801e9430"),
    0x3B: _op(
        "actor_idle",
        "801e81ec",
        3,
        _signed("actor"),
        effect="back to its idle animation (801e950c)",
    ),
    0x3C: _op(
        "actor_clear",
        "801e823c",
        3,
        _signed("actor"),
        effect="clear 0x9e, 0x34, flags 0x40 bits 2-7",
    ),
    0x3D: _op("actor_clear_9e", "801e828c", 3, _signed("actor"), effect="clear 0x9e"),
    0x3E: _op(
        "actor_move", "801e82dc", 9, _signed("actor", "x", "y", "z"), effect="move (801e95e4); wait"
    ),
    0x3F: _op(
        "actor_action",
        "801e83c0",
        9,
        _signed("actor", "x", "y", "z"),
        effect="action 3 (801e9694); wait",
    ),
    0x40: _op(
        "free_model_camera",
        "801e7b2c",
        3,
        _signed("slot"),
        effect="free the model, reset the script camera (801e9b2c), effects disabled",
    ),
    0x41: _op(
        "sound_volume",
        "801e7ff4",
        7,
        _signed("sound", "volume", "resident"),
        effect="set a playing sound's volume (8003a2e4)",
    ),
    0x42: _op("show_gear_hud", "801e86d0", 1, effect="show member 0's number lists (8007ff14(0))"),
    0x43: _op("leave_member_menu", "801e86f4", 1, effect="leave member 0's menu (800800e8(0))"),
    0x44: _op("clear_objects_35", "801e8718", 1, effect="byte 0x35 = 0 of battle objects 0-10"),
    0x45: _op(
        "attack",
        "801e84a4",
        9,
        _signed("attacker", "target", "animation", "code"),
        effect="scripted attack; wait",
    ),
    0x46: _op(
        "actor_attack",
        "801e8600",
        7,
        _signed("actor", "target", "command"),
        effect="idle with command (801e9700), attack (801e9760)",
    ),
    0x47: _op(
        "finish_loads", "801e86ac", 1, effect="stop the disc read and finish the loads (800b8d7c)"
    ),
    0x48: _op(
        "battle_sound",
        "801e8750",
        5,
        _signed("index", "variant"),
        effect="play battle sound to its end (800b838c)",
    ),
    0x49: _op("set_return_fade", "801e7424", 3, _signed("value")),
    0x4A: _op(
        "set_slot0_attack_level4", "801e7660", 1, effect="slot 0 state 4, timer 6 (8009c0e0(0))"
    ),
    0x4B: _op(
        "suppress_gear_hp_warning",
        "801e7684",
        3,
        _signed("actor"),
        effect="set bit 0 of its record flags (0x36)",
    ),
}


class EventError(ValueError):
    """An event script does not decode under the recovered handlers."""


@dataclass(frozen=True)
class EventInstruction:
    offset: int
    opcode: int
    name: str
    length: int
    operands: tuple[tuple[str, str], ...]
    successors: tuple[int, ...]

    def text(self) -> str:
        values = ", ".join(f"{n}={v}" for n, v in self.operands)
        return f"{self.offset:04x}: {self.opcode:02x} {self.name} {values}".rstrip()


def operand_text(code: bytes, offset: int, index: int, kind: str, position: int) -> str:
    at = offset + position
    if kind == U8:
        return f"{code[at]:02x}"
    raw = code[at] | code[at + 1] << 8
    if kind in (U16, TARGET):
        return f"{raw:04x}"
    if kind == VAR:
        return f"var[{raw & 0xFFFE:x}]"
    if kind == SIGNED:
        return f"#{raw & 0x7FFF:x}" if raw & 0x8000 else f"var[{raw & 0xFFFE:x}]"
    mask = code[offset + 5]
    return f"#{raw:x}" if (mask << index) & 0x80 else f"var[{raw & 0xFFFE:x}]"


def decode(code: bytes, offset: int) -> EventInstruction:
    if not 0 <= offset < len(code):
        raise EventError(f"pc 0x{offset:x} outside the bytecode")
    op = code[offset]
    if op not in OPCODES:
        raise EventError(f"unknown opcode 0x{op:02x} at 0x{offset:x}")
    spec = OPCODES[op]
    if offset + spec.length > len(code):
        raise EventError(f"opcode 0x{op:02x} at 0x{offset:x} runs past the bytecode")
    operands = []
    masked = 0
    target = None
    for name, position, kind in spec.operands:
        operands.append((name, operand_text(code, offset, masked, kind, position)))
        masked += kind == MASKED
        if kind == TARGET:
            target = code[offset + position] | code[offset + position + 1] << 8
    end = offset + spec.length
    successors = {NEXT: (end,), END: (), JUMP: (target,), BRANCH: (end, target)}[spec.flow]
    return EventInstruction(offset, op, spec.name, spec.length, tuple(operands), successors)


@dataclass
class EventListing:
    instructions: dict[int, EventInstruction] = field(default_factory=dict)
    errors: list[str] = field(default_factory=list)


def disassemble(code: bytes, entries) -> EventListing:
    listing = EventListing()
    owner: dict[int, int] = {}
    pending = sorted(set(entries))
    while pending:
        offset = pending.pop()
        if offset in listing.instructions:
            continue
        if offset in owner:
            listing.errors.append(
                f"0x{offset:x} starts inside the instruction at 0x{owner[offset]:x}"
            )
            continue
        try:
            insn = decode(code, offset)
        except EventError as error:
            listing.errors.append(str(error))
            continue
        clash = [b for b in range(offset, offset + insn.length) if b in owner]
        if clash:
            listing.errors.append(f"0x{offset:x} overlaps the instruction at 0x{owner[clash[0]]:x}")
            continue
        for b in range(offset, offset + insn.length):
            owner[b] = offset
        listing.instructions[offset] = insn
        pending.extend(s for s in insn.successors if s not in listing.instructions)
    return listing


@dataclass(frozen=True)
class EventScript:
    threads: tuple[tuple[int, ...], ...]  # eight entry offsets per thread
    code: bytes


def parse_script(script: bytes) -> EventScript:
    if len(script) < 0x44:
        raise EventError("script shorter than its header")
    count = struct.unpack_from("<I", script, 0x40)[0]
    start = 0x44 + 16 * count
    if not 1 <= count <= 16 or start > len(script):
        raise EventError(f"bad thread count {count}")
    threads = tuple(struct.unpack_from("<8H", script, 0x44 + 16 * t) for t in range(count))
    return EventScript(threads, script[start:])


def archive_scripts(archive: bytes):
    """Yield (set, script bytes) for each script set of the archive (its packed
    message block is not needed to decode the bytecode)."""
    count = struct.unpack_from("<I", archive, 0)[0]
    offsets = struct.unpack_from(f"<{count}I", archive, 4)
    for n in range(count // 2):
        yield n, decode_block(archive[offsets[2 * n] :]).data


def entry_points(script: EventScript):
    return [entry for thread in script.threads for entry in thread]


@dataclass
class Unreached:
    """Bytecode that no thread reaches: runs before the last reached instruction
    (decoded on their own, kept out of the use counts) and the nonzero bytes
    after it."""

    runs: int = 0
    instructions: int = 0
    errors: list[str] = field(default_factory=list)
    tail_bytes: int = 0


def unreached(code: bytes, listing: EventListing) -> Unreached:
    result = Unreached()
    covered = bytearray(len(code))
    for insn in listing.instructions.values():
        covered[insn.offset : insn.offset + insn.length] = b"\1" * insn.length
    last = max((i.offset + i.length for i in listing.instructions.values()), default=0)
    position = 0
    while position < last:
        if covered[position] or not code[position]:
            position += 1
            continue
        run = disassemble(code, [position])
        if run.errors:
            result.errors.append(f"0x{position:x}: {run.errors[0]}")
            while position < last and not covered[position]:
                position += 1
            continue
        fresh = [i for i in run.instructions.values() if not covered[i.offset]]
        result.runs += 1
        result.instructions += len(fresh)
        for insn in fresh:
            covered[insn.offset : insn.offset + insn.length] = b"\1" * insn.length
    result.tail_bytes = sum(1 for byte in code[last:] if byte)
    return result


@dataclass
class EventSweep:
    identical: bool = False  # both discs hold the same archive
    sets: int = 0
    threads: int = 0
    instructions: int = 0
    opcodes: Counter = field(default_factory=Counter)
    errors: list[str] = field(default_factory=list)
    unreached: Unreached = field(default_factory=Unreached)
    tail_sets: int = 0  # sets with nonzero bytes after their last instruction


def sweep(root: Path = ROOT) -> EventSweep:
    result = EventSweep()
    archives = [Disc(root, name).read(SCRIPTS, 2) for name in ("disc1", "disc2")]
    result.identical = archives[0] == archives[1]
    for name, archive in zip(("disc1", "disc2"), archives, strict=True):
        for n, raw in archive_scripts(archive):
            script = parse_script(raw)
            listing = disassemble(script.code, entry_points(script))
            result.sets += 1
            result.threads += len(script.threads)
            result.instructions += len(listing.instructions)
            result.opcodes.update(i.opcode for i in listing.instructions.values())
            result.errors.extend(f"{name} set {n}: {e}" for e in listing.errors)
            extra = unreached(script.code, listing)
            result.unreached.runs += extra.runs
            result.unreached.instructions += extra.instructions
            result.unreached.errors.extend(f"{name} set {n}: {e}" for e in extra.errors)
            result.unreached.tail_bytes += extra.tail_bytes
            result.tail_sets += extra.tail_bytes > 0
            for insn in listing.instructions.values():
                if insn.opcode in (0x03, 0x04, 0x05):
                    thread, request = (
                        int(insn.operands[0][1], 16),
                        int(insn.operands[1][1], 16) & 0x1F,
                    )
                    if thread >= len(script.threads) or request >= 8:
                        result.errors.append(
                            f"{name} set {n}: request of thread {thread} entry {request} "
                            f"at 0x{insn.offset:x} outside the entry table"
                        )
    return result


def print_listing(archive: bytes, only: int | None) -> None:
    for n, raw in archive_scripts(archive):
        if only is not None and n != only:
            continue
        script = parse_script(raw)
        print(f"set {n}: {len(script.threads)} threads")
        names = {}
        for t, thread in enumerate(script.threads):
            for e, entry in enumerate(thread):
                names.setdefault(entry, []).append(f"thread{t}.{e}")
        listing = disassemble(script.code, list(names))
        for offset in sorted(listing.instructions):
            if offset in names:
                print(f"{' '.join(names[offset])}:")
            print(f"  {listing.instructions[offset].text()}")
        for error in listing.errors:
            print(f"  error: {error}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--sweep", action="store_true", help="decode every battle event script of both discs"
    )
    parser.add_argument(
        "--list", type=Path, help="disassemble an extracted script archive (prints to stdout)"
    )
    parser.add_argument("--set", type=int, help="only this script set of --list")
    args = parser.parse_args()
    if args.list:
        print_listing(args.list.read_bytes(), args.set)
        return
    if not args.sweep:
        parser.error("choose --sweep or --list")
    result = sweep()
    used = sorted(result.opcodes)
    print(
        f"both discs ({'identical' if result.identical else 'different'} archives), counts summed:"
    )
    print(
        f"battle events (801e879c): {result.sets} script sets, {result.threads} threads, "
        f"{result.instructions} instructions"
    )
    print(f"  opcodes defined {len(OPCODES)}, used {len(used)}")
    print("  " + " ".join(f"{op:02x}:{result.opcodes[op]}" for op in used))
    print(f"  unused: {' '.join(f'{op:02x}' for op in sorted(set(OPCODES) - set(used)))}")
    print(f"  unknown/undecodable: {len(result.errors)}")
    for error in result.errors:
        print(f"    {error}")
    dead = result.unreached
    print(
        f"  no thread reaches: {dead.runs} runs decoding as {dead.instructions} instructions; "
        f"{dead.tail_bytes} nonzero bytes after the last instruction of {result.tail_sets} sets"
    )
    print(f"  unreached bytes that do not decode: {len(dead.errors)}")
    for error in dead.errors:
        print(f"    {error}")


if __name__ == "__main__":
    main()
