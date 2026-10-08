"""Disassembly of the two small script machines embedded in mode overlays.

Both opcode tables are read from the recovered interpreters, not inferred from
the data:

* World map actor scripts, func_80076B34 (decomp/src/worldmap/
  worldmap_80072238.c). The script is a stream of signed halfwords. The
  interpreter reads the 32-bit word at the actor's script position: its low
  halfword indexes the twelve handlers of D_8009A3C0 (unchecked) and its high
  halfword and the next two halfwords are the handler's three arguments. The
  handler returns the halfwords to advance; 0 yields until the actor's next
  update. Only func_800827C8 (D_8009A758) and func_800838E8 (D_8009AC60) give
  an actor a script.
* Arena scene scripts, func_8007107C (decomp/src/menu/menu2.c). Bytes; switch
  cases 1-34 take one to three bytes, 0 and every value without a case return
  without advancing. func_80070F80 starts the scripts: D_8009105C[scene]
  (func_8007191C, scenes 0-9), the opening D_80090F38 (func_800719F0) and the
  setup script D_800910C4 (func_800720D4).

Neither machine has jumps: a script runs straight to its stop instruction.
`--sweep` decodes every script of both machines from each disc's own packed
overlay container and prints aggregate counts only; `--list` prints the
disassembly of the user's discs to stdout.
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="aggregate decode of both discs")
    parser.add_argument("--list", choices=sorted(MACHINES), help="print one machine's scripts")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    if args.list:
        listing(MACHINES[args.list], args.disc)
    if args.sweep:
        failures = sum(report(machine) for machine in MACHINES.values())
        if failures:
            raise SystemExit(f"{failures} undecodable scripts")
    if not (args.sweep or args.list):
        parser.error("choose --sweep or --list")


if __name__ == "__main__":
    main()
