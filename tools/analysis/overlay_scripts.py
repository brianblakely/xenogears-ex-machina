"""Disassembly of the script machines of the menu and world map overlays.

Every table is read from the recovered interpreter, not inferred from the
data:

* World map actor scripts, func_80076B34 (decomp/src/worldmap/
  worldmap_80072238.c). The script is a stream of signed halfwords. The
  interpreter reads the 32-bit word at the actor's script position: its low
  halfword indexes the twelve handlers of D_8009A3C0 (unchecked) and its high
  halfword and the next two halfwords are the handler's three arguments. The
  handler returns the halfwords to advance; 0 yields until the actor's next
  update. Only func_800827C8 (D_8009A758) and func_800838E8 (D_8009AC60) give
  an actor a script.
* Arena scene scripts, func_8007107C (decomp/src/menu/menu2.c). Bytes; switch
  cases 1-34 take one to three bytes, except 16 and 17, which never advance
  and never return. 0 and every value without a case return without
  advancing. func_80070F80 starts the scripts: D_8009105C[scene]
  (func_8007191C, scenes 0-9), the opening D_80090F38 (func_800719F0) and the
  setup script D_800910C4 (func_800720D4).
* World map scene directors, the update handlers func_8007A9F8,
  func_8007C3B8, func_8007DE98, func_80080370 and func_800811C0 of the
  scripted world-map modes 14, 12, 15, 13 and 16. Each switches on its
  actor's state. Case 1 counts the actor's wait down and, once it drops below
  0, loads the next state and wait from two parallel u16 tables compiled into
  the overlay (the step index is the instruction pointer); every other case
  runs one cue and stores state 1 (or 0). A cue that clears D_8009D554 ends
  the world-map loop (func_800712D0) after that frame, so the director never
  runs again.

None of the machines has jumps: a script runs straight to its stop.
Sizes and flow follow each handler's advance; the C comments of the handlers
give each opcode's effect. `--sweep` decodes every script of every machine
from each disc's own packed overlay container and prints aggregate counts
only; `--list` prints the disassembly of the user's discs to stdout.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mmap
import re
import struct
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from tools.extraction.overlays import OVERLAYS, image

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x8006FAF0  # load address of both overlays (decomp/targets/overlays/*.yaml)


class ScriptError(ValueError):
    """Bytes that the recovered interpreter cannot run as a script."""


class UnknownOpcode(ScriptError):
    def __init__(self, machine: str, address: int, opcode: int):
        self.machine, self.address, self.opcode = machine, address, opcode
        super().__init__(f"{machine}: opcode {opcode} at 0x{address:08x} has no handler")


@dataclass(frozen=True)
class Opcode:
    mnemonic: str
    handler: str  # function or switch case the entry was read from
    operands: tuple[str | None, ...] = ()  # stream order; None is read but unused
    flow: str = "next"  # next | wait (may yield, then falls through) | stop | hang


@dataclass(frozen=True)
class Machine:
    name: str
    overlay: str  # target and tools.extraction.overlays name
    interpreter: str
    table: str  # dispatch table or switch
    unit: int  # bytes of the opcode and of each operand
    signed: bool  # operand signedness
    align: int  # script positions the interpreter can read
    opcodes: dict[int, Opcode]

    def size(self, opcode: Opcode) -> int:
        return self.unit * (1 + len(opcode.operands))


@dataclass(frozen=True)
class Instruction:
    address: int
    opcode: int
    spec: Opcode
    operands: tuple[int, ...]
    size: int

    def text(self) -> str:
        values = [
            f"{name or 'unused'}={value}"
            for name, value in zip(self.spec.operands, self.operands, strict=True)
            if name is not None or value
        ]
        return f"{self.spec.mnemonic} {', '.join(values)}".rstrip()


# Handlers of D_8009A3C0 in decomp/src/worldmap/worldmap_80072238.c. Each
# operand is a signed halfword; sizes are the halfwords each handler returns
# (exit_worldmap never advances: its word is the one the interpreter reads).
WORLDMAP = Machine(
    name="worldmap",
    overlay="worldmap",
    interpreter="func_80076B34",
    table="D_8009A3C0",
    unit=2,
    signed=True,
    align=4,  # the opcode word is one lw
    opcodes={
        0: Opcode("exit_worldmap", "func_80076BC4", (None,), "stop"),
        1: Opcode("wait", "func_80076BDC", ("frames",), "wait"),
        2: Opcode("send_actor", "func_80076C18", ("actor", "argument", None)),
        3: Opcode("place_player", "func_80076C3C", ("x", "y", "z")),
        4: Opcode("set_script_vector", "func_80076C68", ("x", "y", "z")),
        5: Opcode("start_emitters", "func_80076C88", ("group",)),
        6: Opcode("stop_emitters", "func_80076CB4", ("group",)),
        7: Opcode("stop_effects", "func_80076CD4", ("group",)),
        8: Opcode("fade_music", "func_80076CF4", ("level", "frames", None)),
        9: Opcode("play_sound", "func_80076D1C", ("sound",)),
        10: Opcode("slide_sound_volume", "func_80076D50", ("sound", "volume", "frames")),
        11: Opcode("set_fade", "func_80076D8C", ("rate", "step", None)),
    },
)

# Switch cases of func_8007107C in decomp/src/menu/menu2.c. Operands are
# unsigned bytes. Headings are relative to the facing toward the opponent.
ARENA = Machine(
    name="arena",
    overlay="menu",
    interpreter="func_8007107C",
    table="switch",
    unit=1,
    signed=False,
    align=1,
    opcodes={
        0: Opcode("end", "case 0", (), "stop"),
        1: Opcode("wait", "case 1", ("frames",), "wait"),
        2: Opcode("drive_first", "case 2"),
        3: Opcode("drive_second", "case 3"),
        4: Opcode("clear_inputs", "case 4"),
        5: Opcode("queue_input_1", "case 5"),
        6: Opcode("queue_input_2", "case 6"),
        7: Opcode("queue_input_4", "case 7"),
        8: Opcode("queue_input_3", "case 8"),
        9: Opcode("queue_input_5", "case 9"),
        10: Opcode("queue_input_5", "case 10"),
        11: Opcode("approach", "case 11", (), "wait"),
        12: Opcode("back_off", "case 12", (), "wait"),
        13: Opcode("back_off_far", "case 13", (), "wait"),
        14: Opcode("strafe_minus", "case 14", ("frames",), "wait"),
        15: Opcode("strafe_plus", "case 15", ("frames",), "wait"),
        16: Opcode("spin", "case 16", (), "hang"),
        17: Opcode("spin", "case 17", (), "hang"),
        18: Opcode("message", "case 18", ("message",)),
        19: Opcode("resume_window", "case 19"),
        20: Opcode("strafe_minus_flagged", "case 20", ("frames",), "wait"),
        21: Opcode("show_marker", "case 21", ("x/2", "y")),
        22: Opcode("callback", "case 22", ("command",)),
        23: Opcode("camera_view", "case 23", ("view",)),
        24: Opcode("bout_end_step", "case 24", ("step",)),
        25: Opcode("wait_message", "case 25", (), "wait"),
        26: Opcode("wait_page", "case 26", (), "wait"),
        27: Opcode("hide_marker", "case 27"),
        28: Opcode("restore_hp", "case 28", ("opponent",)),
        29: Opcode("set_hp_one", "case 29", ("opponent",)),
        30: Opcode("close_choice", "case 30"),
        31: Opcode("set_charge", "case 31", ("charge/16",)),
        32: Opcode("hold_charge", "case 32", ("charge/16",)),
        33: Opcode("layout", "case 33", ("layout",)),
        34: Opcode("guard", "case 34", ("on",)),
    },
)

MACHINES = {machine.name: machine for machine in (WORLDMAP, ARENA)}


def read(data: bytes, offset: int, unit: int, signed: bool) -> int:
    if offset < 0 or offset + unit > len(data):
        raise ScriptError(f"script runs past the image at +0x{offset:x}")
    return int.from_bytes(data[offset : offset + unit], "little", signed=signed)


def decode(machine: Machine, data: bytes, address: int, base: int = BASE) -> Instruction:
    """One instruction at `address` of an image loaded at `base`."""
    offset = address - base
    if address % machine.align:
        raise ScriptError(f"{machine.name}: misaligned script position 0x{address:08x}")
    code = read(data, offset, machine.unit, False)
    spec = machine.opcodes.get(code)
    if spec is None:
        raise UnknownOpcode(machine.name, address, code)
    operands = tuple(
        read(data, offset + machine.unit * (i + 1), machine.unit, machine.signed)
        for i in range(len(spec.operands))
    )
    return Instruction(address, code, spec, operands, machine.size(spec))


def disassemble(machine: Machine, data: bytes, address: int, base: int = BASE) -> list[Instruction]:
    """The straight-line script from `address` through its stop instruction."""
    out = []
    while True:
        instruction = decode(machine, data, address, base)
        out.append(instruction)
        if instruction.spec.flow in ("stop", "hang"):
            return out
        address += instruction.size


def words(data: bytes, address: int, count: int, base: int = BASE) -> list[int]:
    offset = address - base
    if offset < 0 or offset + 4 * count > len(data):
        raise ScriptError(f"table 0x{address:08x} lies outside the image")
    return list(struct.unpack_from(f"<{count}I", data, offset))


def halfwords(data: bytes, address: int, count: int, base: int = BASE) -> list[int]:
    offset = address - base
    if offset < 0 or offset + 2 * count > len(data):
        raise ScriptError(f"table 0x{address:08x} lies outside the image")
    return list(struct.unpack_from(f"<{count}H", data, offset))


def scripts(machine: Machine, data: bytes, base: int = BASE) -> list[tuple[str, int]]:
    """The scripts the interpreter can be given, as (reference, address)."""
    if machine is WORLDMAP:
        handlers = [int(spec.handler[5:], 16) for spec in WORLDMAP.opcodes.values()]
        if words(data, 0x8009A3C0, len(handlers), base) != handlers:
            raise ScriptError("D_8009A3C0 does not hold the recovered handlers")
        return [("func_800827C8", 0x8009A758), ("func_800838E8", 0x8009AC60)]
    table = words(data, 0x8009105C, 10, base)
    found = [("func_800719F0", 0x80090F38), ("func_800720D4", 0x800910C4)]
    return found + [(f"D_8009105C[{scene}]", address) for scene, address in enumerate(table)]


# Script-shaped data that nothing starts (menu2.c); decoded and reported apart.
UNREFERENCED = {ARENA.name: [("D_80091050", 0x80091050)], WORLDMAP.name: []}


@dataclass
class Sweep:
    scripts: int = 0
    instructions: int = 0
    uses: Counter = field(default_factory=Counter)
    failures: list = field(default_factory=list)


def sweep(machine: Machine, data: bytes, base: int = BASE) -> Sweep:
    """Decode each distinct script; failures keep their location."""
    result = Sweep()
    seen = {}
    for reference, address in scripts(machine, data, base):
        seen.setdefault(address, reference)
    for address, reference in seen.items():
        result.scripts += 1
        try:
            listing = disassemble(machine, data, address, base)
        except ScriptError as error:
            result.failures.append((reference, address, str(error)))
            continue
        result.instructions += len(listing)
        result.uses.update(instruction.opcode for instruction in listing)
    return result


def expected_sha256(overlay: str) -> str:
    text = (ROOT / f"decomp/targets/overlays/{overlay}.mk").read_text()
    return re.search(r"^ORIGINAL_SHA256 := ([0-9a-f]{64})$", text, re.M).group(1)


def disc_image(overlay: str, disc: int) -> bytes:
    """The overlay decoded from this disc's own container, checked against the
    image the addresses were recovered from."""
    slot = OVERLAYS[overlay][disc - 1]
    manifest = json.loads((ROOT / f".local/extract/disc{disc}/manifest.json").read_text())
    entry = next(entry for entry in manifest["files"] if entry["slot"] == slot)
    with open(ROOT / f".local/discs/disc{disc}.bin", "rb") as handle:
        with mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as raw:
            data = image(raw, entry, OVERLAYS[overlay][2])
    if hashlib.sha256(data).hexdigest() != expected_sha256(overlay):
        raise SystemExit(f"Disc {disc} {overlay}: not the image the scripts were recovered from")
    return data


def report(machine: Machine) -> int:
    print(
        f"{machine.name}: interpreter {machine.interpreter}, {len(machine.opcodes)} opcodes"
        f" ({machine.table}), overlay {machine.overlay}"
    )
    results = {}
    for disc in (1, 2):
        data = disc_image(machine.overlay, disc)
        results[disc] = result = sweep(machine, data)
        print(
            f"  disc {disc}: {result.scripts} scripts, {result.instructions} instructions,"
            f" {len(result.failures)} undecodable"
        )
        for reference, address, error in result.failures:
            print(f"    {reference} 0x{address:08x}: {error}")
        for name, address in UNREFERENCED[machine.name]:
            listing = disassemble(machine, data, address)
            print(f"    unreferenced {name}: {len(listing)} instructions (not counted)")
    print("  opcode uses (disc 1, disc 2):")
    for code, spec in machine.opcodes.items():
        counts = " ".join(f"{result.uses[code]:4d}" for result in results.values())
        print(f"    {code:3d} {spec.mnemonic:<22} {counts}")
    for disc, result in results.items():
        used = sum(1 for code in machine.opcodes if result.uses[code])
        print(f"  disc {disc} opcodes used: {used} of {len(machine.opcodes)}")
    return sum(len(result.failures) for result in results.values())


def listing(machine: Machine, disc: int) -> None:
    data = disc_image(machine.overlay, disc)
    for reference, address in scripts(machine, data) + UNREFERENCED[machine.name]:
        print(f"{reference} -> 0x{address:08x}")
        for instruction in disassemble(machine, data, address):
            print(f"  {instruction.address:08x}: {instruction.text()}")


# World map scene directors ---------------------------------------------------
#
# A director's switch cases are its opcodes: the state fetched from the state
# table, with the wait fetched from the duration table as the operand. The
# actions of each case are its calls and stores in source order:
#   ("request", s, n)    func_80097770(s, n): command 1 with argument n to
#                        actor slot s, dropped while that actor has one
#                        pending (Director.slots; slot 0 is the screen fade,
#                        func_800925A0: 13 fades out, 12 fades in)
#   ("sound", n)         func_80039E60: area-bank effect n on two free voices
#   ("sound_12", n)      func_80039E18: area-bank effect n on voices 12-13
#   ("ambient", k)       func_80039E60 of D_8009A5A0[D_8009D3D4][k]
#   ("emitters", g)      func_80089160(g, NULL, NULL): start emitter group g
#                        at position 0 unless one of its emitters is live
#   ("emitters_at_target", g)  the same at the camera target's x, z (y 0)
#   ("stop_effects", g)  func_80089514(g): stop group g's live effects
#   ("music_fade", a, b) func_8003A89C(D_80062528, a, b): music to level a
#                        over b frames
#   ("fade_rate", v)     D_8009CCA4 = v: the fade quad's semi-transparency
#                        rate (1 adds it, fading to white; 2 subtracts it)
#   ("fade_step", v)     D_8009D3CC = v: its brightness step per frame
#   ("exit_worldmap",)   D_8009D554 = D_8009D7CC = 0: the world-map loop ends
#                        after this frame with exit 0
#   ("fetch",)           case 1: --wait < 0 loads state and wait of the step
#                        index and advances it


@dataclass(frozen=True)
class Cue:
    actions: tuple[tuple, ...] = ()
    state: int | None = 1  # the state the case stores; None: it stores none

    @property
    def flow(self) -> str:
        """wait (case 1), exit (ends the world-map loop), next (back to state
        1) or idle (the director stops stepping)."""
        if ("fetch",) in self.actions:
            return "wait"
        if ("exit_worldmap",) in self.actions:
            return "exit"
        return "next" if self.state == 1 else "idle"

    @property
    def mnemonic(self) -> str:
        if self.flow == "wait":
            return "wait"
        names = []
        for verb, *args in self.actions:
            if verb == "request" and args[0] == 0 and args[1] in (12, 13):
                name = "fade_out" if args[1] == 13 else "fade_in"
            elif verb in ("fade_rate", "fade_step"):
                continue
            else:
                name = {
                    "request": "requests",
                    "sound": "sounds",
                    "sound_12": "sounds",
                    "ambient": "sounds",
                    "emitters_at_target": "emitters",
                }.get(verb, verb)
            if name not in names:
                names.append(name)
        return "+".join(names) or "idle"

    def text(self, slots: tuple[str, ...] = ()) -> str:
        """The actions, naming each request's target by its slot's handler."""
        out = []
        for verb, *args in self.actions:
            if verb == "request":
                owner = f" ({slots[args[0]]})" if args[0] < len(slots) else ""
                out.append(f"slot {args[0]}{owner} request {args[1]}")
            elif verb != "fetch":
                out.append(" ".join([verb, *(f"{value:#x}" for value in args)]))
        if self.state == 0 and self.flow != "idle":
            out.append("state 0")
        return ", ".join(out)


IDLE = Cue((), None)  # case 0: nothing
FETCH = Cue((("fetch",),), None)  # case 1


def _cue(*actions: tuple, state: int = 1) -> Cue:
    return Cue(tuple(actions), state)


def _requests(*pairs: tuple[int, int]) -> tuple[tuple, ...]:
    return tuple(("request", s, n) for s, n in pairs)


def _sounds(*numbers: int) -> tuple[tuple, ...]:
    return tuple(("sound", n) for n in numbers)


EXIT = ("exit_worldmap",)
FLAMES = ((4, 3), (5, 3), (6, 3), (7, 3), (8, 3))  # func_8007DE98's flame slots 4-8
FADE = "func_800925A0"  # slot 0, the screen fade


@dataclass(frozen=True)
class Sequence:
    name: str
    states: int  # u16 state table (the starter reads entry 0 too)
    durations: int  # u16 wait table, same index
    entries: int  # the state table's length in its C definition


@dataclass(frozen=True)
class Director:
    interpreter: str  # the update handler holding the switch
    source: str
    starter: str  # its start handler, which loads entry 0
    advances: bool  # the starter steps past entry 0 (else entry 0 is fetched again)
    mode: int  # D_8009A058 mode whose setup registers it
    setup: str
    slots: tuple[str, ...]  # update handler per actor slot, in the setup's order
    sequences: tuple[Sequence, ...]
    cues: dict[int, Cue]
    picker: int | None = None  # Sequence table the starter indexes by D_8009D3D4


DIRECTORS = (
    Director(
        "func_8007A9F8",
        "decomp/src/worldmap/worldmap_8007A9F8.c",
        "func_8007A9B4",
        False,
        14,
        "func_8007A5DC",
        (FADE, "func_8007A9F8", "func_8007ADD4", "func_8007B394", "func_8007B798", "func_8007BA10")
        + ("func_8007BBEC", "func_80078950"),
        (Sequence("D_8009A450", 0x8009A450, 0x8009A46C, 14),),
        {
            0: IDLE,
            1: FETCH,
            2: _cue(("emitters", 0x11), ("sound", 1)),
            3: _cue(("request", 2, 2), ("emitters", 0xF), ("emitters", 0x10), *_sounds(4, 5, 6)),
            4: _cue(*_requests((2, 3), (3, 1))),
            5: _cue(*_requests((2, 6), (4, 1), (5, 1)), *_sounds(7, 8, 9)),
            6: _cue(("request", 2, 4), ("sound", 0xA), ("sound_12", 0xB), ("sound_12", 0xC)),
            7: _cue(("request", 2, 5)),
            8: _cue(("request", 2, 1)),
            9: _cue(*_requests((6, 1), (2, 7))),
            10: _cue(("request", 0, 0xD), ("fade_step", 4)),
            11: _cue(EXIT),
        },
    ),
    Director(
        "func_8007C3B8",
        "decomp/src/worldmap/worldmap_8007C3B8.c",
        "func_8007C36C",
        True,
        12,
        "func_8007BF50",
        (FADE, "func_8007C3B8", "func_8007C7D8", "func_8007CD20", "func_8007CF18", "func_8007D110")
        + ("func_8007D2B8", "func_8007D4A4", "func_8007D690", "func_8007D7FC", "func_80078950"),
        (Sequence("D_8009A4D8", 0x8009A4D8, 0x8009A4E8, 8),),
        {
            0: IDLE,
            1: FETCH,
            2: _cue(("request", 2, 1), *_sounds(0xD, 0xE, 0xF)),
            0x10: _cue(("emitters_at_target", 0x14), *_sounds(0x10, 0x11, 0x12)),
            0x11: _cue(
                *_requests((2, 2), (0, 0xD)),
                ("fade_rate", 1),
                ("fade_step", 0x40),
                *_sounds(0x13, 0x14, 0x15),
            ),
            0x12: _cue(
                *_requests((2, 3), (3, 1), (4, 1), (5, 1), (6, 1), (7, 1), (0, 0xC)),
                ("fade_rate", 1),
                ("fade_step", 1),
            ),
            0x13: _cue(*_requests((2, 4), (9, 1))),
            0x14: _cue(*_requests((2, 6), (3, 2), (9, 2))),
            0x16: _cue(("request", 0, 0xD), ("fade_rate", 2), ("fade_step", 4)),
            0x40: _cue(EXIT),
        },
    ),
    Director(
        "func_8007DE98",
        "decomp/src/worldmap/worldmap_8007DE98.c",
        "func_8007DE14",
        True,
        15,
        "func_8007D918",
        (FADE, "func_8007DE98", "func_8007E4E4", "func_8007EE34", *["func_8007F968"] * 5)
        + ("func_8007FD30", "func_80078950"),
        (
            Sequence("D_8009A5D4", 0x8009A5D4, 0x8009A5F0, 14),
            Sequence("D_8009A60C", 0x8009A60C, 0x8009A620, 9),
            Sequence("D_8009A634", 0x8009A634, 0x8009A648, 9),
        ),
        {
            0: IDLE,
            1: FETCH,
            2: _cue(*_sounds(0x1C, 0x1D, 0x1E), *_requests((2, 2), (3, 2), *FLAMES)),
            3: _cue(
                ("stop_effects", 0x22),
                ("stop_effects", 0x23),
                ("stop_effects", 0x24),
                *_requests((2, 3), (3, 3), *FLAMES),
            ),
            4: _cue(*_requests((2, 4), (3, 4), *FLAMES)),
            5: _cue(("request", 2, 5)),
            6: _cue(("request", 0, 0xD), ("fade_rate", 1), ("fade_step", 0x40)),
            7: _cue(
                *_requests((3, 5), (4, 4), (5, 4), (6, 4), (7, 4), (8, 4), (0, 0xC)),
                ("fade_rate", 1),
                ("fade_step", 0x40),
            ),
            8: _cue(("request", 2, 6)),
            9: _cue(("request", 0, 0xD), ("fade_rate", 1), ("fade_step", 0x80)),
            10: _cue(
                ("request", 0, 0xC),
                ("fade_rate", 1),
                ("fade_step", 0x80),
                *_requests((9, 1), (2, 7)),
            ),
            16: _cue(*_requests((2, 4), (3, 0x10), *FLAMES)),
            17: _cue(("request", 2, 0x10)),
            18: _cue(("request", 2, 0x11)),
            24: _cue(*_requests((2, 4), (3, 0x18), *FLAMES)),
            25: _cue(("request", 2, 0x18)),
            61: _cue(("ambient", 0), ("ambient", 1), ("ambient", 2)),
            62: _cue(*_sounds(0x19, 0x1A, 0x1B)),
            63: _cue(("request", 0, 0xD), ("fade_rate", 2), ("fade_step", 4)),
            64: _cue(EXIT, state=0),
        },
        picker=0x8009A65C,
    ),
    Director(
        "func_80080370",
        "decomp/src/worldmap/worldmap_80080370.c",
        "func_8008032C",
        False,
        13,
        "func_8007FF70",
        (FADE, "func_80080370", "func_80080600", "func_80080944", "func_80080AC4", "func_80076A1C"),
        (Sequence("D_8009A698", 0x8009A698, 0x8009A6AC, 9),),
        {
            0: IDLE,
            1: FETCH,
            2: _cue(("request", 3, 1)),
            3: _cue(("request", 2, 2)),
            4: _cue(("request", 0, 0xD), ("fade_rate", 1), ("fade_step", 0x80)),
            5: _cue(*_requests((0, 0xC), (4, 1)), ("fade_rate", 1), ("fade_step", 0x80)),
            6: _cue(("request", 2, 3)),
            7: _cue(("request", 0, 0xD), ("fade_rate", 2), ("fade_step", 4)),
            8: _cue(*_sounds(0x16, 0x17, 0x18)),
            0x40: _cue(EXIT, state=0),
        },
    ),
    Director(
        "func_800811C0",
        "decomp/src/worldmap/worldmap_800811C0.c",
        "func_80081174",
        True,
        16,
        "func_80080D00",
        (FADE, "func_800811C0", "func_80081470", "func_80081868", "func_80081B24", "func_80081D80")
        + ("func_80081FD8", "func_80078950"),
        (Sequence("D_8009A6C0", 0x8009A6C0, 0x8009A70C, 37),),
        {
            0: IDLE,
            1: FETCH,
            2: _cue(("request", 6, 1)),
            3: _cue(("request", 6, 0)),
            4: _cue(("request", 6, 2)),
            5: _cue(("request", 6, 3)),
            6: _cue(("request", 6, 4)),
            7: _cue(("request", 6, 5)),
            8: _cue(*_sounds(0x2E, 0x2F, 0x30)),
            0x10: _cue(*_requests((3, 1), (4, 1))),
            0x11: _cue(("request", 3, 2)),
            0x3F: _cue(
                ("music_fade", 0, 0xF0),
                ("request", 0, 0xD),
                ("fade_rate", 2),
                ("fade_step", 4),
            ),
            0x40: _cue(EXIT, state=0),
        },
    ),
)


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def sequence_tables(director: Director, data: bytes, base: int = BASE):
    """Each sequence's (states, durations) as the director reads them; the
    picker table must hold the recovered sequences."""
    if director.picker is not None:
        pairs = words(data, director.picker, 2 * len(director.sequences), base)
        expected = [address for s in director.sequences for address in (s.states, s.durations)]
        if pairs != expected:
            raise ScriptError(f"0x{director.picker:08x} does not hold the recovered sequences")
    return [
        (
            sequence,
            halfwords(data, sequence.states, sequence.entries, base),
            halfwords(data, sequence.durations, sequence.entries, base),
        )
        for sequence in director.sequences
    ]


@dataclass(frozen=True)
class Step:
    index: int  # table entry
    state: int
    wait: int
    frame: int  # the director update on which its state first runs


@dataclass(frozen=True)
class Run:
    steps: tuple[Step, ...]
    stop: str  # exit | idle | no case | past table
    frame: int  # the update on which it stops
    next: int  # the entry a further fetch would read

    def reached(self) -> set[int]:
        return {step.index for step in self.steps}


def run(director: Director, states: list[int], durations: list[int], limit: int = 1 << 20) -> Run:
    """Follow a director from its starter (update 0; the actor pass runs it
    once after the setup) to where it stops. Fetching an entry takes wait + 1
    updates of state 1; a cue runs on the update after its fetch."""
    index = 1 if director.advances else 0
    state, wait = s16(states[0]), s16(durations[0])
    steps = [Step(0, state, wait, 1)]
    frame = 0
    while frame < limit:
        frame += 1
        if state != 1:
            cue = director.cues.get(state)
            if cue is None:
                return Run(tuple(steps), "no case", frame, index)
            if cue.flow in ("exit", "idle"):
                return Run(tuple(steps), cue.flow, frame, index)
            state = cue.state
            continue
        wait = s16(wait - 1)
        if wait >= 0:
            continue
        if index >= len(states):
            return Run(tuple(steps), "past table", frame, index)
        state, wait = s16(states[index]), s16(durations[index])
        steps.append(Step(index, state, wait, frame + 1))
        index += 1
    raise ScriptError(f"{director.interpreter}: no stop within {limit} updates")


@dataclass
class SceneSweep:
    sequences: int = 0
    instructions: int = 0  # distinct entries reached
    uses: Counter = field(default_factory=Counter)  # (interpreter, state)
    notes: list = field(default_factory=list)
    failures: list = field(default_factory=list)


def scene_sweep(data: bytes, base: int = BASE) -> SceneSweep:
    result = SceneSweep()
    for director in DIRECTORS:
        for sequence, states, durations in sequence_tables(director, data, base):
            result.sequences += 1
            outcome = run(director, states, durations)
            reached = sorted(outcome.reached())
            result.instructions += len(reached)
            result.uses.update((director.interpreter, s16(states[i])) for i in reached)
            where = f"{director.interpreter} {sequence.name}"
            if outcome.stop in ("no case", "past table"):
                result.failures.append((where, outcome.stop, outcome.next))
                continue
            note = (
                f"{where}: {len(reached)} of {sequence.entries} entries, {outcome.stop} on"
                f" update {outcome.frame}"
            )
            last = director.cues[s16(states[outcome.steps[-1].index])]
            if outcome.stop == "exit" and last.state == 1:
                beyond = "past the table" if outcome.next >= sequence.entries else ""
                note += f"; a further update would fetch entry {outcome.next} {beyond}".rstrip()
            result.notes.append(note)
    return result


def scene_report() -> int:
    cases = sum(len(director.cues) for director in DIRECTORS)
    print(
        f"worldmap-scene: {len(DIRECTORS)} directors (state switches), {cases} cases,"
        " overlay worldmap"
    )
    results = {}
    for disc in (1, 2):
        results[disc] = result = scene_sweep(disc_image("worldmap", disc))
        print(
            f"  disc {disc}: {result.sequences} sequences, {result.instructions} entries reached,"
            f" {len(result.failures)} undecodable"
        )
        for where, stop, entry in result.failures:
            print(f"    {where}: {stop} at entry {entry}")
        for note in result.notes:
            print(f"    {note}")
    print("  reached entries per case (disc 1, disc 2):")
    for director in DIRECTORS:
        print(f"    {director.interpreter} (mode {director.mode}, starter {director.starter})")
        for state, cue in director.cues.items():
            counts = " ".join(
                f"{result.uses[director.interpreter, state]:4d}" for result in results.values()
            )
            print(f"    {state:5d} {cue.mnemonic:<26} {counts}")
    for disc, result in results.items():
        used = sum(
            1
            for director in DIRECTORS
            for state in director.cues
            if result.uses[director.interpreter, state]
        )
        print(f"  disc {disc} cases an entry holds: {used} of {cases}")
    return sum(len(result.failures) for result in results.values())


def scene_listing(disc: int) -> None:
    data = disc_image("worldmap", disc)
    for director in DIRECTORS:
        for sequence, states, durations in sequence_tables(director, data):
            outcome = run(director, states, durations)
            print(
                f"{director.interpreter} {sequence.name} (mode {director.mode},"
                f" starter {director.starter}): {outcome.stop} on update {outcome.frame}"
            )
            for step in outcome.steps:
                cue = director.cues.get(step.state)
                name = cue.mnemonic if cue else "(no case)"
                text = cue.text(director.slots) if cue else ""
                print(
                    f"  update {step.frame:5d} entry {step.index:2d}: {step.state:3d} {name:<24}"
                    f" wait={step.wait:<4d} {text}".rstrip()
                )
            for index in sorted(set(range(sequence.entries)) - outcome.reached()):
                state, wait = states[index], durations[index]
                print(f"  never fetched entry {index}: state {state} wait={wait}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="aggregate decode of both discs")
    parser.add_argument(
        "--list",
        choices=sorted([*MACHINES, "worldmap-scene"]),
        help="print one machine's scripts",
    )
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    if args.list == "worldmap-scene":
        scene_listing(args.disc)
    elif args.list:
        listing(MACHINES[args.list], args.disc)
    if args.sweep:
        failures = sum(report(machine) for machine in MACHINES.values())
        failures += scene_report()
        if failures:
            raise SystemExit(f"{failures} undecodable scripts")
    if not (args.sweep or args.list):
        parser.error("choose --sweep or --list")


if __name__ == "__main__":
    main()
