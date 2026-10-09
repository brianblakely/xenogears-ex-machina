"""Disassembly of the scripts and cue timelines of the menu, world map and field overlays.

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
  0, loads the next state and wait from two parallel u16 tables embedded in
  the overlay's data (the step index is the instruction pointer); every other case
  runs one cue and stores state 1 (or 0). A cue that clears D_8009D554 ends
  the world-map loop (func_800712D0) after that frame, so the director never
  runs again.
* Arena move frame events, func_80074678 (decomp/src/menu/menu3.c). A gear
  model file (directory 0x30/1) holds per animation a list of FrameEvent
  records {first, last, spec} ending in first 0xFF; each record whose frame
  range holds the frame runs the HitSpec at header + spec through a switch on
  its kind byte (cases 0-5), and kinds 0 and 2 dispatch again on its type.
* Field movie sound timelines, func_80085678 (decomp/src/field/
  field_800854D0.c). u16 (frame, sound) pairs, one run per movie sound-effect
  bank, each ended by frame 0xFFFF; func_80085788 seeks the bank's run and the
  player plays its entries in order as the movie's frames reach them.
* World map terrain texture animations, func_80074F2C and func_80075104
  (decomp/src/worldmap/worldmap_80072238.c). Runs of (image, duration) frames
  ended by a negative duration, one per slot of D_8009A1E8 and D_8009A250; each
  update steps a slot's frame when its timer runs out and uploads the image.

None of the machines has jumps: a script runs straight to its stop. The
scripts and cue tables embedded in the overlays stay user-supplied: the units
link them from the user's image (INCLUDE_ASSET, `asset` ranges in the
targets' classification files), so the decoders read them from each disc.
Sizes and flow follow each handler's advance; the C comments of the handlers
give each opcode's effect. `--sweep` decodes every script of every machine
from each disc's own files (the packed overlays, model files and sound banks)
and prints aggregate counts only; `--list` prints the disassembly of the
user's discs to stdout.
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

from tools.analysis.disc_index import Disc
from tools.analysis.packed import PackedError, PackedTruncated, decode_block
from tools.extraction.overlays import OVERLAYS, image

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x8006FAF0  # load address of the three overlays (decomp/targets/overlays/*.yaml)


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
    entries: int  # the state table's length (its INCLUDE_ASSET size / 2)


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


# Arena move frame events -----------------------------------------------------
#
# Layouts from decomp/src/menu/menu.h, as (offset, size, signed).
FRAME_EVENT = {"first": (0, 1, False), "last": (1, 1, False), "spec": (2, 2, True)}
HIT_SPEC = {
    "unk0": (0, 1, False),  # the kind
    "type": (1, 1, False),
    "part_a": (2, 1, False),
    "part_b": (3, 1, False),
    "vertex_a": (4, 2, True),
    "vertex_b": (6, 2, True),
}
EVENT_END = 0xFF  # a record whose first frame is 0xFF ends the list
EVENT_TABLE = 0x34  # header + 0x34: an s16 event list offset per animation (func_80084C88)
MODEL_DIRECTORY = (0x30, 1)  # func_8008509C: model id n is file n + 2


@dataclass(frozen=True)
class EventKind:
    mnemonic: str
    handler: str  # func_80074678's case and the function it calls
    fields: tuple[tuple[str, str], ...]  # (HitSpec field the case reads, its role)
    runs: str  # which of a call's frames run it

    def size(self) -> int:
        """HitSpec bytes the case reads, the kind byte included."""
        return max([1] + [HIT_SPEC[name][0] + HIT_SPEC[name][1] for name, _ in self.fields])


POINTS = (
    ("type", "form"),
    ("part_a", "part_a"),
    ("vertex_a", "vertex_a"),
    ("part_b", "part_b"),
    ("vertex_b", "vertex_b"),
)
EVENT_KINDS = {
    0: EventKind(
        "hit",
        "func_80074678 case 0: func_800740E4",
        POINTS,
        "every frame of the range; it is live (actor flag 0x4000000) from the first frame"
        " until the last or until one of its trails connects (func_80075B50)",
    ),
    1: EventKind(
        "sounds",
        "func_80074678 case 1: func_8008EB88",
        (("part_a", "sound_a"), ("part_b", "sound_b")),
        "only the first kind-1 event of a call",
    ),
    2: EventKind(
        "effect",
        "func_80074678 case 2: func_80073F34",
        POINTS,
        "once per HitSpec in a call, up to 20 (the count is never initialised)",
    ),
    3: EventKind("return_home", "func_80074678 case 3: func_80078154", (), "every frame"),
    4: EventKind("hide_part", "func_80074678 case 4", (("type", "part"),), "every frame"),
    5: EventKind("show_part", "func_80074678 case 5", (("type", "part"),), "every frame"),
}


def hit_form(type_: int) -> str:
    """func_800740E4's use of a hit's type while the hit is live; bit 0x40
    drops the sparkle trail (func_8007C880 / func_8007CD44) of every frame."""
    if type_ == 0x20:
        return "charged_shot"  # func_80073424 kind 0 if the charge is taken, else sparkle 9
    if type_ == 4:
        return "shot_1_at_opponent"  # func_80073424 kind 1, b unused
    if 0x21 <= type_ <= 0x26:
        return f"shot_{type_ - 0x20}"  # func_80073424, away from b when the points differ
    return "trail_no_sparkle" if type_ & 0x40 else "trail"  # func_80073CEC


SPARKLE_SIZES = 3  # D_80091228, indexed by type - 0x20


def effect_form(type_: int) -> str:
    """func_80073F34's dispatch on an effect's type; "none" has no case in the
    callee and "past_table" indexes past D_80091228."""
    if 0x10 <= type_ < 0x20:  # func_8007D25C: between the two points
        if type_ == 0x10:
            return "line"  # func_8007CD44
        return f"bolt_{type_ - 0x11}" if type_ <= 0x13 else "none"  # func_8007D65C
    if type_ >= 0x20:  # func_8007C880 at a or the midpoint
        return f"sparkle_trail_{type_ - 0x20}" if type_ - 0x20 < SPARKLE_SIZES else "past_table"
    if type_ <= 4:
        return f"sparkle_{type_}"  # func_8007D190
    if 8 <= type_ <= 12:
        return f"sparkle_{type_ - 8}_jittered"
    return "none"


@dataclass(frozen=True)
class FrameEvent:
    offset: int  # of the record, from the header
    first: int
    last: int
    spec: int  # of the HitSpec, from the header
    kind: int
    values: tuple[int, ...]  # the kind's fields

    @property
    def form(self) -> str | None:
        if self.kind == 0:
            return hit_form(self.values[0])
        if self.kind == 2:
            return effect_form(self.values[0])
        return None

    def text(self) -> str:
        spec = EVENT_KINDS[self.kind]
        values = [
            f"{role}={value}"
            for (_, role), value in zip(spec.fields, self.values, strict=True)
            if role != "form"
        ]
        name = spec.mnemonic + (f" {self.form}" if self.form else "")
        return f"frames {self.first}-{self.last} {name} {', '.join(values)}".rstrip()


class UnknownKind(ScriptError):
    def __init__(self, offset: int, kind: int):
        self.offset, self.kind = offset, kind
        super().__init__(f"HitSpec +0x{offset:x}: kind {kind} has no case (the loop stalls)")


def field_value(data: bytes, offset: int, layout: tuple[int, int, bool]) -> int:
    start, size, signed = layout
    if offset < 0 or offset + start + size > len(data):
        raise ScriptError(f"read at +0x{offset + start:x} lies outside the decoded file")
    return int.from_bytes(data[offset + start : offset + start + size], "little", signed=signed)


def frame_events(data: bytes, header: int, offset: int) -> list[FrameEvent]:
    """The FrameEvent list at header + offset through its 0xFF record, each
    with the HitSpec fields its kind's case reads."""
    events = []
    position = header + offset
    while (first := field_value(data, position, FRAME_EVENT["first"])) != EVENT_END:
        last = field_value(data, position, FRAME_EVENT["last"])
        spec = field_value(data, position, FRAME_EVENT["spec"])
        kind = field_value(data, header + spec, HIT_SPEC["unk0"])
        if kind not in EVENT_KINDS:
            raise UnknownKind(spec, kind)
        values = tuple(
            field_value(data, header + spec, HIT_SPEC[name]) for name, _ in EVENT_KINDS[kind].fields
        )
        events.append(FrameEvent(position - header, first, last, spec, kind, values))
        position += 4
    return events


@dataclass(frozen=True)
class ModelFile:
    header: int  # file offset of the model header (pointer +0x10)
    lists: tuple[int, ...]  # per animation of the table at +0x08: list offset from the header


WORD = (0, 4, False)


def model_file(data: bytes) -> ModelFile:
    """The pointers func_8008AF6C relocates (absolute against the build address
    at +0x1C) and the event list offsets at header + 0x34 (func_80084C88), one
    per animation of the table at +0x08 (0: none)."""
    base = field_value(data, 0x1C, WORD)
    header = field_value(data, 0x10, WORD) - base
    table = field_value(data, 0x08, WORD)
    count = field_value(data, table - base, WORD) if table else 0
    lists = tuple(
        field_value(data, header + EVENT_TABLE + 2 * i, (0, 2, True)) for i in range(count)
    )
    return ModelFile(header, lists)


def loaded(disc: Disc, slot: int) -> bytes:
    """The bytes func_800891C0 reads into memory: the file's size rounded up
    to words (func_800288EC); the rest of its last sector is not copied."""
    return disc.sectors(slot)[: (disc.entries[slot]["size"] + 3) & ~3]


def unpack(source: bytes) -> tuple[bytes, int]:
    """80032E88's output for `source` and how many of its declared bytes are
    left out. A stream that reads past the source depends on memory after the
    file from there on, so only the output decoded before that read is kept."""
    try:
        return decode_block(source).data, 0
    except PackedTruncated as error:
        return error.output, int.from_bytes(source[:4], "little") - len(error.output)


def model_slots(disc: Disc) -> list[tuple[int, int]]:
    """(file, slot) of every model file: the directory's own index entry
    (file 1) holds minus the number of files that follow it."""
    group, index = MODEL_DIRECTORY
    count = -disc.entries[disc.slot(group, index, 1)]["size"]
    return [(file, disc.slot(group, index, file)) for file in range(2, count + 2)]


@dataclass
class EventSweep:
    files: int = 0
    short: list = field(default_factory=list)  # output bytes left out, per file that has any
    animations: int = 0
    with_list: int = 0
    lists: int = 0  # distinct lists
    events: int = 0
    uses: Counter = field(default_factory=Counter)
    forms: Counter = field(default_factory=Counter)  # (kind, form)
    types: Counter = field(default_factory=Counter)  # (kind, type) of the kinds with forms
    never: int = 0  # first > last: no frame runs it
    tails: int = 0  # hit HitSpecs followed by two bytes no event reads
    unread: int = 0  # other HitSpec-area bytes (before the first list) no event reads
    unlisted: int = 0  # bytes between lists that no animation reaches
    inert: list = field(default_factory=list)  # forms with no effect or past a table
    failures: list = field(default_factory=list)


def event_sweep_file(result: EventSweep, name: str, data: bytes) -> None:
    model = model_file(data)
    result.animations += len(model.lists)
    lists = {}
    for number, offset in enumerate(model.lists):
        if offset:
            result.with_list += 1
            lists.setdefault(offset, number)
    read, spans, hits = set(), set(), set()
    for offset, number in sorted(lists.items()):
        try:
            events = frame_events(data, model.header, offset)
        except ScriptError as error:
            result.failures.append((name, number, str(error)))
            continue
        result.lists += 1
        result.events += len(events)
        spans.update(range(offset, offset + 4 * len(events) + 4))  # with the 0xFF record
        for event in events:
            result.uses[event.kind] += 1
            read.update(range(event.spec, event.spec + EVENT_KINDS[event.kind].size()))
            if event.kind == 0:
                hits.add(event.spec)
            if event.form is not None:
                result.forms[event.kind, event.form] += 1
                result.types[event.kind, event.values[0]] += 1
                if event.form in ("none", "past_table"):
                    result.inert.append((name, number, event.text()))
            if event.first > event.last:
                result.never += 1
    if spans:
        start = EVENT_TABLE + 2 * len(model.lists)
        unread = set(range(start, min(spans))) - read
        tails = sum(1 for spec in hits if {spec + 8, spec + 9} <= unread)
        result.tails += tails
        result.unread += len(unread) - 2 * tails
        result.unlisted += len(set(range(min(spans), max(spans))) - spans)


def event_sweep(disc: Disc) -> EventSweep:
    result = EventSweep()
    for file, slot in model_slots(disc):
        result.files += 1
        name = f"file {file} (slot {slot})"
        try:
            data, short = unpack(loaded(disc, slot))
            event_sweep_file(result, name, data)
        except (PackedError, ScriptError) as error:
            result.failures.append((name, None, str(error)))
            continue
        if short:
            result.short.append(short)
    return result


def spans_text(values: list[int]) -> str:
    """Sorted values as runs, e.g. 0x00-0x04, 0x10."""
    runs = []
    for value in values:
        if runs and value == runs[-1][1] + 1:
            runs[-1][1] = value
        else:
            runs.append([value, value])
    return ", ".join(f"{a:#04x}" if a == b else f"{a:#04x}-{b:#04x}" for a, b in runs)


def event_report(root: Path = ROOT) -> int:
    group, index = MODEL_DIRECTORY
    print(
        f"arena-events: interpreter func_80074678, {len(EVENT_KINDS)} kinds (switch),"
        f" model files of directory {group:#x}/{index}"
    )
    results = {}
    for number in (1, 2):
        results[number] = result = event_sweep(Disc(number, root))
        short = sorted(result.short)
        short = f"{len(short)} decode {short[0]}-{short[-1]} bytes short" if short else "all whole"
        print(
            f"  disc {number}: {result.files} files ({short}), {result.animations} animations,"
            f" {result.with_list} with a list ({result.lists} distinct), {result.events} events,"
            f" {len(result.failures)} undecodable"
        )
        for name, animation, error in result.failures:
            print(f"    {name} animation {animation}: {error}")
        print(
            f"    ranges with first > last: {result.never}; forms without effect or past a"
            f" table: {len(result.inert)}; bytes between lists that no animation reaches:"
            f" {result.unlisted}"
        )
        print(
            f"    HitSpec-area bytes no event reads: {2 * result.tails} in the two-byte tails"
            f" of {result.tails} hits, {result.unread} others"
        )
        for name, animation, text in result.inert:
            print(f"    {name} animation {animation}: {text}")
    print("  kind uses (disc 1, disc 2):")
    for kind, spec in EVENT_KINDS.items():
        counts = " ".join(f"{result.uses[kind]:5d}" for result in results.values())
        print(f"    {kind:3d} {spec.mnemonic:<22} {counts}")
        forms = sorted(
            {form for result in results.values() for k, form in result.forms if k == kind}
        )
        for form in forms:
            counts = " ".join(f"{result.forms[kind, form]:5d}" for result in results.values())
            print(f"          {form:<20} {counts}")
        types = {t for result in results.values() for k, t in result.types if k == kind}
        if types:
            print(f"          types used: {spans_text(sorted(types))}")
    for number, result in results.items():
        used = sum(1 for kind in EVENT_KINDS if result.uses[kind])
        print(f"  disc {number} kinds used: {used} of {len(EVENT_KINDS)}")
    return sum(len(result.failures) for result in results.values())


def event_listing(number: int, root: Path = ROOT) -> None:
    disc = Disc(number, root)
    for file, slot in model_slots(disc):
        data, _ = unpack(loaded(disc, slot))
        model = model_file(data)
        print(f"file {file} (slot {slot}, model {file - 2}): header +0x{model.header:x}")
        shown = {}
        for animation, offset in enumerate(model.lists):
            if not offset:
                continue
            if offset in shown:
                print(f"  animation {animation}: list +0x{offset:x} (as animation {shown[offset]})")
                continue
            shown[offset] = animation
            print(f"  animation {animation}: list +0x{offset:x}")
            for event in frame_events(data, model.header, offset):
                print(f"    +0x{event.offset:x} spec +0x{event.spec:x}: {event.text()}")


# Field movie sound timelines -------------------------------------------------
#
# D_800AE060 (decomp/src/field/field_800854D0.c) holds u16 (frame, sound)
# pairs: a leading end, then one run per movie sound-effect bank, each ended
# by an entry whose frame is 0xFFFF. func_80085788 loads the movie's bank,
# file 0x115 + bank of directory (0x1C, 0), and leaves D_800C3A64 past bank + 1
# ends. Once per movie frame func_80085678 then plays every entry from there
# whose frame plus the movie's sound start (FIELD_MOVIE.sound_start) the movie
# frame D_800B06A0 has reached: the bank's effect in the low byte on the voice
# pair in bits 8-10 (func_80039EC4 gets pair * 2). Neither tests the run's end
# itself: frame 0xFFFF lies past every movie. Event fe a0 (func_8008EA58)
# names the bank in operand 9; 0xFF (FIELD_MOVIE.sound_bank's reset value)
# loads none and plays nothing.

MOVIE_SOUNDS = 0x800AE060
MOVIE_SOUND_ENTRIES = 0x60  # the INCLUDE_ASSET size 0x180, four bytes per entry
MOVIE_SOUND_END = 0xFFFF
MOVIE_SOUND_DIRECTORY = (0x1C, 0)  # func_80028470(0x1C, 0) before the bank is read
MOVIE_SOUND_FILE = 0x115  # + bank
MOVIE_SOUND_NONE = 0xFF
MOVIE_SOUND_REQUEST = (0xFE, 0xA0, 9)  # event fe a0, its bank operand's offset


@dataclass(frozen=True)
class MovieSound:
    index: int  # table entry
    frame: int  # movie frames after the sound start
    sound: int

    @property
    def effect(self) -> int:
        return self.sound & 0xFF

    @property
    def pair(self) -> int:
        return (self.sound >> 8) & 7

    @property
    def unread(self) -> int:
        """Bits 11-15, which func_80085678 does not read."""
        return self.sound >> 11

    def text(self) -> str:
        return f"frame {self.frame:4d}: effect {self.effect:#04x} voice pair {self.pair}"


def movie_sound_table(data: bytes, base: int = BASE) -> list[tuple[int, int]]:
    """The (frame, sound) entries of D_800AE060 in a field image."""
    values = halfwords(data, MOVIE_SOUNDS, 2 * MOVIE_SOUND_ENTRIES, base)
    return list(zip(values[0::2], values[1::2], strict=True))


def movie_sound_seek(table: list[tuple[int, int]], bank: int) -> int:
    """The position func_80085788 leaves in D_800C3A64: past bank + 1 ends."""
    position = 0
    for _ in range(bank + 1):
        while position < len(table) and table[position][0] != MOVIE_SOUND_END:
            position += 1
        if position >= len(table):
            raise ScriptError(
                f"movie sound bank {bank}: the table holds fewer than {bank + 1} ends"
            )
        position += 1
    return position


def movie_sound_run(table: list[tuple[int, int]], bank: int) -> list[MovieSound]:
    """Bank `bank`'s entries in play order, up to its end."""
    run = []
    position = movie_sound_seek(table, bank)
    while position < len(table) and table[position][0] != MOVIE_SOUND_END:
        run.append(MovieSound(position, *table[position]))
        position += 1
    if position >= len(table):
        raise ScriptError(f"movie sound bank {bank}: its run has no end inside the table")
    return run


def movie_sound_banks(table: list[tuple[int, int]]) -> int:
    """The banks the table holds a run for: one per end after the leading one."""
    return max(0, sum(1 for frame, _ in table if frame == MOVIE_SOUND_END) - 1)


def movie_sound_step(
    table: list[tuple[int, int]], position: int, frame: int, start: int
) -> tuple[list[MovieSound], int]:
    """One call of func_80085678 at movie frame `frame`: the entries it plays
    from `position` (D_800C3A64) and the position it leaves."""
    played = []
    while True:
        if position >= len(table):
            raise ScriptError(f"movie sound position {position} lies past the table")
        if frame < table[position][0] + start:
            return played, position
        played.append(MovieSound(position, *table[position]))
        position += 1


def movie_sound_bank_file(disc: Disc, bank: int) -> tuple[int, int]:
    """(slot, effect count) of bank `bank`'s file, which func_80038428 opens
    in place: func_8003F614 wants magic "seds", a zero word sum and version
    0x101 at +0xC; the effect count is at +0x12 (sound_sequence.parse_bank)."""
    from tools.analysis.sound_sequence import SequenceError, parse_bank, word_sum

    slot = disc.slot(*MOVIE_SOUND_DIRECTORY, MOVIE_SOUND_FILE + bank)
    entry = disc.entries.get(slot)
    if entry is None or entry["size"] <= 0:
        raise ScriptError(f"movie sound bank {bank}: no file at slot {slot}")
    data = disc.data(slot)
    try:
        parse_bank(data)
    except SequenceError as error:
        raise ScriptError(f"movie sound bank {bank} (slot {slot}): {error}") from error
    if struct.unpack_from("<H", data, 0xC)[0] != 0x101 or word_sum(data, len(data)):
        raise ScriptError(f"movie sound bank {bank} (slot {slot}): func_8003F614 rejects it")
    return slot, struct.unpack_from("<H", data, 0x12)[0]


def movie_sound_requests(number: int, root: Path = ROOT) -> Counter:
    """The bank operand of every reachable event fe a0 in the disc's field
    maps (tools.analysis.events): its value when immediate, "variable" when
    the operand names a variable."""
    from tools.analysis import events

    prefix, extended, offset = MOVIE_SOUND_REQUEST
    found = Counter()
    extract, raw = root / f".local/extract/disc{number}", root / f".local/discs/disc{number}.bin"
    for _, path in events.map_files(extract, raw):
        package = events.map_events(path.read_bytes())
        if package is None:
            continue
        starts, _ = events.script_entries(package)
        result = events.walk(package.bytecode, [pc for _, _, pc in starts])
        for instruction in result.instructions.values():
            if instruction.opcode != prefix or instruction.extended != extended:
                continue
            (index,) = [i for i, o in enumerate(instruction.spec.operands) if o.offset == offset]
            immediate = instruction.immediate[index]
            found[instruction.operands[index] if immediate else "variable"] += 1
    return found


@dataclass
class MovieSoundSweep:
    banks: int = 0
    entries: int = 0
    pairs: Counter = field(default_factory=Counter)
    unread: int = 0  # entries with bits 11-15 set
    unordered: list = field(default_factory=list)  # (bank, entry): an earlier frame than the last
    # (bank, slot, effect count, effects it never plays, effects it plays twice or more)
    files: list = field(default_factory=list)
    requests: Counter = field(default_factory=Counter)
    failures: list = field(default_factory=list)


def movie_sound_sweep(data: bytes, disc: Disc | None = None, requests: Counter | None = None):
    """Decode every bank's run; with a disc, check each bank's file and the
    effects its run plays; with requests, the banks the events ask for."""
    result = MovieSoundSweep()
    table = movie_sound_table(data)
    result.banks = movie_sound_banks(table)
    if table[-1][0] != MOVIE_SOUND_END:
        result.failures.append(("table", "entries after the last end belong to no run"))
    for bank in range(result.banks):
        try:
            run = movie_sound_run(table, bank)
        except ScriptError as error:
            result.failures.append((f"bank {bank}", str(error)))
            continue
        result.entries += len(run)
        result.pairs.update(sound.pair for sound in run)
        result.unread += sum(1 for sound in run if sound.unread)
        result.unordered += [
            (bank, b.index) for a, b in zip(run, run[1:], strict=False) if b.frame < a.frame
        ]
        if disc is None:
            continue
        try:
            slot, effects = movie_sound_bank_file(disc, bank)
        except ScriptError as error:
            result.failures.append((f"bank {bank}", str(error)))
            continue
        played = Counter(sound.effect for sound in run)
        past = sorted(effect for effect in played if effect >= effects)
        if past:
            result.failures.append((f"bank {bank}", f"effects {past} past its {effects}"))
        unplayed = sorted(set(range(effects)) - set(played))
        repeated = sorted(effect for effect, count in played.items() if count > 1)
        result.files.append((bank, slot, effects, unplayed, repeated))
    if requests is not None:
        result.requests = requests
        for bank in requests:
            if bank not in ("variable", MOVIE_SOUND_NONE) and bank >= result.banks:
                result.failures.append((f"bank {bank}", "requested by fe a0, but has no run"))
    return result


def movie_sound_report(root: Path = ROOT) -> int:
    print(
        f"movie-sounds: player func_80085678, seek func_80085788, table D_800AE060"
        f" ({MOVIE_SOUND_ENTRIES} entries), overlay field"
    )
    failures = 0
    for number in (1, 2):
        disc = Disc(number, root)
        result = movie_sound_sweep(
            disc_image("field", number), disc, movie_sound_requests(number, root)
        )
        failures += len(result.failures)
        print(
            f"  disc {number}: {result.banks} banks, {result.entries} entries,"
            f" {len(result.failures)} undecodable; frames out of order: {len(result.unordered)},"
            f" entries with bits 11-15 set: {result.unread}"
        )
        for where, error in result.failures:
            print(f"    {where}: {error}")
        for bank, slot, effects, unplayed, repeated in result.files:
            print(
                f"    bank {bank}: file {MOVIE_SOUND_FILE + bank:#x} (slot {slot}),"
                f" {effects} effects; never played: {unplayed or 'none'},"
                f" played twice or more: {repeated or 'none'}"
            )
        pairs = " ".join(f"{pair}:{count}" for pair, count in sorted(result.pairs.items()))
        print(f"    voice pairs (pair:entries): {pairs}")
        named = sorted(bank for bank in result.requests if isinstance(bank, int))
        print(
            "    event fe a0 bank operands: "
            + ", ".join(f"{bank:#x} x{result.requests[bank]}" for bank in named)
            + f", from a variable x{result.requests['variable']}"
        )
    return failures


def movie_sound_listing(number: int) -> None:
    table = movie_sound_table(disc_image("field", number))
    for bank in range(movie_sound_banks(table)):
        run = movie_sound_run(table, bank)
        print(f"bank {bank} (file {MOVIE_SOUND_FILE + bank:#x}): {len(run)} entries")
        for sound in run:
            extra = f" (bits 11-15: {sound.unread:#x})" if sound.unread else ""
            print(f"  entry {sound.index:2d}: {sound.text()}{extra}")


# World map terrain texture animations ----------------------------------------
#
# D_8009A1E8[2] and D_8009A250[3] (decomp/src/worldmap/worldmap_80072238.c)
# are TexAnimSlot rows {RECT rect; s32; TexAnimFrame *frames}; a TexAnimFrame
# run {s16 image; s16 duration} ends with a negative duration. func_80074E58
# and func_80075030 give animation i of the area file's two animation sections
# (+0x20 and +0x24: a count, then image offsets) slot i, frame 0 and timer 1.
# Each update func_80074F2C and func_80075104 count the timer down; at 0 they
# step to the next frame and take its duration, restart at frame 0 with that
# frame's duration when it is negative, and upload the frame's image into the
# slot's rect (the first set's images are 16 bytes, the second's w * h * 2).

TEXTURE_SLOTS = (
    ("D_8009A1E8", 0x8009A1E8, 2, "func_80074F2C"),
    ("D_8009A250", 0x8009A250, 3, "func_80075104"),
)
TEXTURE_SLOT_BYTES = 16  # RECT, s32, frames pointer


@dataclass(frozen=True)
class TextureSlot:
    table: str
    index: int
    rect: tuple[int, int, int, int]  # x, y, w, h
    frames: int  # address of its run
    stepper: str


def texture_slots(data: bytes, base: int = BASE) -> list[TextureSlot]:
    slots = []
    for table, address, count, stepper in TEXTURE_SLOTS:
        for index in range(count):
            offset = address + TEXTURE_SLOT_BYTES * index - base
            if offset < 0 or offset + TEXTURE_SLOT_BYTES > len(data):
                raise ScriptError(f"{table}[{index}] lies outside the image")
            x, y, w, h, _, frames = struct.unpack_from("<4hiI", data, offset)
            slots.append(TextureSlot(table, index, (x, y, w, h), frames, stepper))
    return slots


def texture_frames(data: bytes, address: int, base: int = BASE) -> list[tuple[int, int]]:
    """The (image, duration) run at `address`, its negative end included."""
    run = []
    while True:
        offset = address + 4 * len(run) - base
        if offset < 0 or offset + 4 > len(data):
            raise ScriptError(f"texture run 0x{address:08x} has no end inside the image")
        image, duration = struct.unpack_from("<2h", data, offset)
        run.append((image, duration))
        if duration < 0:
            return run


def texture_uploads(run: list[tuple[int, int]], updates: int) -> list[tuple[int, int, int]]:
    """(update, frame, image) of every upload in the first `updates` updates
    of a slot, stepped as func_80074F2C does from frame 0 and timer 1."""
    frame, timer, uploads = 0, 1, []
    for update in range(1, updates + 1):
        timer = s16(timer - 1)
        if timer != 0:
            continue
        frame += 1
        timer = run[frame][1]
        if timer < 0:
            frame, timer = 0, run[0][1]
        uploads.append((update, frame, run[frame][0]))
    return uploads


@dataclass
class TextureSweep:
    slots: int = 0
    frames: int = 0  # entries before each run's end
    cycles: list = field(default_factory=list)  # (slot, updates per cycle)
    failures: list = field(default_factory=list)


def texture_sweep(data: bytes, base: int = BASE) -> TextureSweep:
    result = TextureSweep()
    for slot in texture_slots(data, base):
        where = f"{slot.table}[{slot.index}]"
        result.slots += 1
        try:
            run = texture_frames(data, slot.frames, base)
        except ScriptError as error:
            result.failures.append((where, str(error)))
            continue
        frames = run[:-1]
        result.frames += len(frames)
        if not frames or frames[0][1] <= 0:
            result.failures.append((where, "frame 0 has no positive duration to restart with"))
        elif any(duration == 0 for _, duration in frames):
            result.failures.append((where, "a zero duration: the timer would wrap past 0"))
        else:
            result.cycles.append((where, sum(duration for _, duration in frames)))
    return result


def texture_report() -> int:
    print(
        "worldmap-textures: steppers func_80074F2C, func_80075104, slots D_8009A1E8[2],"
        " D_8009A250[3], overlay worldmap"
    )
    failures = 0
    for number in (1, 2):
        result = texture_sweep(disc_image("worldmap", number))
        failures += len(result.failures)
        print(
            f"  disc {number}: {result.slots} slots, {result.frames} frames,"
            f" {len(result.failures)} undecodable"
        )
        for where, error in result.failures:
            print(f"    {where}: {error}")
        for where, updates in result.cycles:
            print(f"    {where}: a cycle of {updates} updates")
    return failures


def texture_listing(number: int) -> None:
    data = disc_image("worldmap", number)
    for slot in texture_slots(data):
        run = texture_frames(data, slot.frames)
        x, y, w, h = slot.rect
        print(
            f"{slot.table}[{slot.index}] (stepped by {slot.stepper}): rect ({x}, {y}, {w}, {h}),"
            f" frames at 0x{slot.frames:08x}"
        )
        for index, (picture, duration) in enumerate(run):
            what = "end: restart at frame 0" if duration < 0 else f"for {duration} updates"
            print(f"  frame {index}: image {picture} {what}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="aggregate decode of both discs")
    listings = {
        "worldmap-scene": scene_listing,
        "arena-events": event_listing,
        "movie-sounds": movie_sound_listing,
        "worldmap-textures": texture_listing,
    }
    parser.add_argument(
        "--list",
        choices=sorted([*MACHINES, *listings]),
        help="print one machine's scripts",
    )
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    if args.list in listings:
        listings[args.list](args.disc)
    elif args.list:
        listing(MACHINES[args.list], args.disc)
    if args.sweep:
        failures = sum(report(machine) for machine in MACHINES.values())
        failures += scene_report() + event_report() + movie_sound_report() + texture_report()
        if failures:
            raise SystemExit(f"{failures} undecodable scripts")
    if not (args.sweep or args.list):
        parser.error("choose --sweep or --list")


if __name__ == "__main__":
    main()
