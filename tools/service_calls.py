#!/usr/bin/env python3
"""Inventory the platform services the recovered game code uses (port boundary).

    service_calls.py SIGNATURES CONFIG.mk [CONFIG.mk ...] [--detail]

For every game function of each target (not SDK, not hand-written), this
statically scans the linked image's instructions and reports:

* calls (`jal`) into PsyQ SDK library functions, grouped by library and
  function, with their game callers (the services a port must provide);
* direct loads/stores to the I/O register window 0x1F801000-0x1F802FFF and to
  the scratchpad 0x1F800000-0x1F8003FF, found by tracking `lui`/`addiu`/`ori`
  base registers through the function (`constant_bases`, also used by
  tools/data_users.py: a linear static scan, not an execution trace);
* hardware pointer globals: data words outside function ranges whose value
  lies in the I/O window (e.g. the sound driver's SPU register base), and the
  game functions that load them;
* functions using the GTE (cop2 instructions, including GTE commands).

SDK functions are the FUNC symbols inside each target's `sdk` classification
ranges (the resident's for the overlays, which call it at fixed addresses);
their library comes from the PsyQ object signatures that cover them
(tools/psyq_signatures.py; SIGNATURES is the pinned psx_psyq_signatures data).
Indirect calls (`jalr`) are counted, not resolved. Run inside the matching shell.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import rabbitizer

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from matching_coverage import classification  # noqa: E402
from matching_diff import config, functions, load_offset  # noqa: E402
from psyq_signatures import scan  # noqa: E402

IO_WINDOW = (0x1F801000, 0x1F803000)
SCRATCHPAD = (0x1F800000, 0x1F800400)
LOADS_STORES = {
    "lb", "lbu", "lh", "lhu", "lw", "lwl", "lwr", "sb", "sh", "sw", "swl", "swr", "lwc2", "swc2",
}
VOLATILE = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31)  # at, v, a, t, ra


def constant_bases(blob: bytes, address: int, gp: int | None = None, indexed: bool = False):
    """Yield (instruction, opcode, target) for each word of a function: target
    is the address a load/store reaches, or an `addiu`/`ori` forms, from a
    register holding a constant (`lui`, then `addiu`/`ori`), else None.

    One linear pass: a register keeps its constant across branches and jumps
    until another write, or a call, whose caller-saved registers go after its
    delay slot. With `gp`, $gp holds that value (setting it forms no address);
    with `indexed`, `addu` of a constant base and another register keeps the
    base (an indexed access into the same object)."""
    regs: dict[int, int] = {} if gp is None else {28: gp}
    clobber = 0
    for offset in range(0, len(blob) - 3, 4):
        word = int.from_bytes(blob[offset : offset + 4], "little")
        ins = rabbitizer.Instruction(word, vram=address + offset)
        name = ins.getOpcodeName()
        if clobber:
            clobber -= 1
            if not clobber:
                for reg in VOLATILE:
                    regs.pop(reg, None)
        target = None
        if name in LOADS_STORES and ins.rs.value in regs:
            target = (regs[ins.rs.value] + ins.getProcessedImmediate()) & 0xFFFFFFFF
        if name == "lui":
            regs[ins.rt.value] = (ins.getProcessedImmediate() << 16) & 0xFFFFFFFF
        elif name in ("addiu", "ori") and ins.rs.value in regs:
            value, immediate = regs[ins.rs.value], ins.getProcessedImmediate()
            regs[ins.rt.value] = (value + immediate if name == "addiu" else value | immediate) & 0xFFFFFFFF
            if gp is None or ins.rt.value != 28:
                target = regs[ins.rt.value]
        elif indexed and name == "addu" and (ins.rs.value in regs) != (ins.rt.value in regs):
            regs[ins.rd.value] = regs.get(ins.rs.value, regs.get(ins.rt.value))
        else:
            if ins.modifiesRt():
                regs.pop(ins.rt.value, None)
            if ins.modifiesRd():
                regs.pop(ins.rd.value, None)
            if name in ("jal", "jalr"):
                clobber = 2
            if gp is not None:
                regs[28] = gp
        yield ins, name, target


def target(config_path: Path) -> dict:
    values = config(config_path)
    image = ROOT / values["IMAGE"]
    elf = Path(str(image) + ".elf")
    ranges = classification(ROOT / values["CLASSIFICATION"]) if values.get("CLASSIFICATION") else []
    return {
        "name": config_path.stem,
        "elf": elf,
        "data": image.read_bytes(),
        "delta": load_offset(elf),
        "functions": functions(elf),
        "ranges": ranges,
    }


def sdk_functions(t: dict, signatures: Path) -> dict[int, tuple[str, str]]:
    """address -> (name, library) for the FUNC symbols inside the target's sdk ranges."""
    result = {}
    for start, end, kind, _ in t["ranges"]:
        if kind != "sdk":
            continue
        blob = t["data"][start + t["delta"] : end + t["delta"]]
        objects = scan(signatures, blob, start)
        for address, size, name in t["functions"]:
            if not start <= address < end:
                continue
            libraries = sorted({
                hit.split()[1] for (s, e), hits in objects.items() if s <= address < e for hit in hits
            })
            result[address] = (name, "/".join(libraries) or "unattributed")
    return result


def symbol_names(elf: Path) -> dict[int, str]:
    out = subprocess.run(["psx-readelf", "-sW", str(elf)], check=True, capture_output=True, text=True)
    result = {}
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 8 and parts[0][:-1].isdigit() and parts[6] != "UND" and parts[3] in ("OBJECT", "NOTYPE"):
            result.setdefault(int(parts[1], 16), parts[7])
    return result


def classified(t: dict, address: int) -> str | None:
    return next((k for s, e, k, _ in t["ranges"] if s <= address < e), None)


def hardware_pointers(t: dict) -> dict[int, int]:
    """Data words (outside function ranges) holding an I/O-window address: {address: value}."""
    text = [(a, a + n) for a, n, _ in t["functions"]]
    found = {}
    data = t["data"]
    for offset in range(0, len(data) - 3, 4):
        value = int.from_bytes(data[offset : offset + 4], "little")
        if IO_WINDOW[0] <= value < IO_WINDOW[1]:
            address = offset - t["delta"]
            if address >= 0x80000000 and not any(s <= address < e for s, e in text):
                found[address] = value
    return found


def scan_function(blob: bytes, address: int, pointers: dict[int, int]) -> dict:
    calls, io, scratch, gte, indirect = [], set(), set(), 0, 0
    hardware = set()
    for ins, name, target in constant_bases(blob, address):
        if name == "jal":
            calls.append(ins.getInstrIndexAsVram())
        elif name == "jalr":
            indirect += 1
        if ins.getRaw() >> 26 in (0x12, 0x32, 0x3A):  # COP2 (GTE commands, moves), lwc2, swc2
            gte += 1
        if name in LOADS_STORES and target is not None:
            if name == "lw" and target in pointers:
                hardware.add(target)
            if IO_WINDOW[0] <= target < IO_WINDOW[1]:
                io.add(target)
            elif SCRATCHPAD[0] <= target < SCRATCHPAD[1]:
                scratch.add(target)
    return {
        "calls": calls, "io": sorted(io), "scratchpad": sorted(scratch), "gte": gte,
        "indirect": indirect, "hardware_pointers": sorted(hardware),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("signatures", type=Path, help="psx_psyq_signatures data directory")
    parser.add_argument("configs", type=Path, nargs="+")
    parser.add_argument("--detail", action="store_true", help="list callers and addresses")
    args = parser.parse_args()

    targets = [target(c if c.is_absolute() else ROOT / c) for c in args.configs]
    sdk: dict[int, tuple[str, str]] = {}
    pointers: dict[int, int] = {}
    names: dict[int, str] = {}
    for t in targets:
        sdk.update(sdk_functions(t, args.signatures))
        pointers.update(hardware_pointers(t))
        names.update(symbol_names(t["elf"]))
    pointer_users: dict[str, set[str]] = defaultdict(set)
    services: dict[str, dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
    io: dict[str, list[str]] = {}
    scratchpad: list[str] = []
    gte: list[str] = []
    indirect = 0
    game_functions = 0
    for t in targets:
        for address, size, name in t["functions"]:
            if size == 0 or classified(t, address) in ("sdk", "handwritten"):
                continue
            game_functions += 1
            blob = t["data"][address + t["delta"] : address + t["delta"] + size]
            found = scan_function(blob, address, pointers)
            caller = f"{t['name']}:{name}"
            for callee in found["calls"]:
                if callee in sdk:
                    sdk_name, library = sdk[callee]
                    services[library][sdk_name].add(caller)
            if found["io"]:
                io[caller] = [f"{a:08x}" for a in found["io"]]
            for pointer in found["hardware_pointers"]:
                pointer_users[f"{names.get(pointer, f'{pointer:08x}')}={pointers[pointer]:08x}"].add(caller)
            if found["scratchpad"]:
                scratchpad.append(caller)
            if found["gte"]:
                gte.append(caller)
            indirect += found["indirect"]
    report = {
        "claim": "static_instruction_scan",
        "targets": [t["name"] for t in targets],
        "game_functions": game_functions,
        "sdk_services": {
            library: {
                fn: (sorted(callers) if args.detail else len(callers))
                for fn, callers in sorted(functions_.items())
            }
            for library, functions_ in sorted(services.items())
        },
        "direct_io": io if args.detail else len(io),
        "hardware_pointers": {
            k: (sorted(v) if args.detail else len(v)) for k, v in sorted(pointer_users.items())
        },
        "scratchpad_functions": sorted(scratchpad) if args.detail else len(scratchpad),
        "gte_functions": sorted(gte) if args.detail else len(gte),
        "indirect_calls": indirect,
    }
    print(json.dumps(report, indent=1, sort_keys=True))


if __name__ == "__main__":
    main()
