#!/usr/bin/env python3
"""Compare the addresses each target's link takes from other targets with
those targets' rebuilt links, once every target has linked (decomp/Makefile:
all-verify runs it last; `make -C decomp cross-image` runs it alone).

Each target links alone. A name one of its linker scripts assigns (splat's
undefined_syms_auto.txt and undefined_funcs_auto.txt, read as PROVIDE where
the link used them, and fragments such as the *.resident.ld files and
debug595.field.ld) whose value lies outside the target's own image and
uninitialized data but inside another target's is an address copied from
the original, which no link places. Each must agree with the rebuilt targets
(their own symbols, not names they take themselves). A name that gives an
address (splat's D_, func_ and jtbl_ names) must hold that address; then

* by name: every other target that defines the name defines it there, each
  target whose ELF exports it (a global or weak symbol) wherever the value
  points, and a target holding the value also by a local symbol;
* by view, where none defines the name: the script gives it as another name
  plus or minus a constant (``game_data_party_state = game_data + 0x1D30``), that name
  agrees by name, and the value lies in the object that holds that name in
  each target defining it (up to the next symbol its link places in a
  section, or its end): a member of a named object;
* by address, where neither applies: a target holding the value has a
  symbol there;
* otherwise the address must lie inside an input section a target holding it
  places: a member or part of an object that no symbol names.

By address and inside an object, the check ties the value only to the
address its name gives, not to a particular object or, where targets
overlap, to a particular target. A value outside every target (a size, a
constant, a hardware or kernel address) is no image's address and is not
checked, nor is an address that C spells as a number: ``--numbers`` lists
those instead, each other target's address a link holds without a
relocation (a lui and the instruction completing it, or a data word),
outside asset and included bytes and the mode table (docs/matching.md).

The resident's mode table (MODE_TABLE in its configuration: the ModeEntry
records of decomp/include/resident/mode.h, an entry, the BSS bounds and a decode
flag) must hold at each mode overlay's index (MODE in the overlay's
configuration) the overlay's entry symbol (MODE_ENTRY) and the bounds of its
uninitialized data as its rebuilt link places them. The dispatcher (main.c
mode_dispatch) clears the words after bss_start through bss_end
(boot_clear_bss_range), so bss_start + 4 and bss_end + 4 must be the start and end
of the overlay's .sbss/.bss input sections. A mode whose entry is resident
code (battle) declares only MODE.

The report claims only that agreement; run it after the targets link.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from bisect import bisect_right
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from matching_coverage import (  # noqa: E402
    BSS_SECTION,
    R_MIPS_32,
    R_MIPS_HI16,
    SHF_ALLOC,
    SHN_ABS,
    SHT_PROGBITS,
    SHT_REL,
    STB_LOCAL,
    STT_FILE,
    STT_FUNC,
    STT_SECTION,
    Section,
    classification,
    extent,
    map_sections,
    read_elf,
    script_assigned_names,
    script_names,
)
from matching_diff import config  # noqa: E402

# ModeEntry (decomp/include/resident/mode.h): entry, bss_start, bss_end, loaded.
MODE_ENTRY_SIZE = 16
# A name that gives its address (splat's names for addresses it found used).
ADDRESS_NAME = re.compile(r"(?:D|func|jtbl)_([0-9A-Fa-f]{8})(?:_\w+)?")
# A script expression that is another name plus or minus a constant.
VIEW = re.compile(r"([A-Za-z_.$][\w.$]*)\s*(?:([+-])\s*(0[xX][0-9a-fA-F]+|\d+))?")
R_MIPS_LO16 = 6
# The instructions that complete a lui's address: addiu, ori, loads and stores.
LOW_HALF = {0x09, 0x0D, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0x2B,
            0x2E, 0x32, 0x3A}


@dataclass
class Target:
    name: str
    values: dict[str, str]
    sections: list[Section]
    lo: int
    end: int
    value: dict[str, int]  # every defined name's value in the link
    names: dict[str, set[int]]  # its own symbols (inside its range): their values
    exported: set[str]  # those of them that a global or weak symbol defines
    at: dict[int, set[str]]  # and their names by value
    starts: list[int]  # the values of its own symbols in a section (objects)
    inputs: list[tuple[int, int]]  # the input sections the link placed
    bss: tuple[int, int] | None  # its .sbss/.bss input sections' start and end
    imports: dict[str, tuple[Path, str]]  # the names its scripts assign


def load(path: Path) -> Target:
    values = config(path)
    image = Path(values["IMAGE"])
    elf, map_path = Path(f"{image}.elf"), Path(f"{image}.map")
    for built in (elf, map_path):
        if not built.exists():
            raise SystemExit(f"{built}: missing; link the target first"
                             f" (make -C decomp CONFIG={path} verify)")
    sections, symbols = read_elf(elf)
    bss_end = int(values["BSS_END"], 0) if values.get("BSS_END") else None
    lo, _hi, end = extent(sections, bss_end)
    # splat's main script names segment and section bounds, not objects.
    bounds = script_assigned_names(Path(values["LINKER_SCRIPT"]))
    value, names, exported, at, starts = {}, {}, set(), {}, set()
    for symbol in symbols:
        if symbol.name and symbol.section and symbol.kind not in (STT_SECTION, STT_FILE):
            value[symbol.name] = symbol.value
            if lo <= symbol.value < end and symbol.name not in bounds:
                names.setdefault(symbol.name, set()).add(symbol.value)
                at.setdefault(symbol.value, set()).add(symbol.name)
                if symbol.bind != STB_LOCAL:
                    exported.add(symbol.name)
                if symbol.section != SHN_ABS:
                    starts.add(symbol.value)
    inputs = map_sections(map_path)
    bss = [(a, a + size) for name, a, size, _obj in inputs if BSS_SECTION.match(name)]
    scripts = [Path(values["BUILD"]) / f"{script[:-4]}.ld" if script.endswith("_auto.txt")
               else Path(script) for script in values.get("LINKER_EXTRA", "").split()]
    return Target(path.stem, values, sections, lo, end, value, names, exported, at,
                  sorted(starts), sorted((a, a + size) for _name, a, size, _obj in inputs),
                  (min(a for a, _ in bss), max(b for _, b in bss)) if bss else None,
                  script_names(map_path, scripts))


def word(target: Target, address: int) -> int:
    for s in target.sections:
        if (s.type == SHT_PROGBITS and s.flags & SHF_ALLOC
                and s.address <= address and address + 4 <= s.address + s.size):
            return struct.unpack_from("<I", s.data, address - s.address)[0]
    raise SystemExit(f"{target.name}: no loaded word at {address:08x}")


def definers(targets: list[Target], t: Target, name: str, value: int) -> list[Target]:
    """The targets other than t that define `name`: each that exports it,
    and one whose range holds `value` also by a local symbol."""
    return [d for d in targets if d is not t and name in d.names
            and (name in d.exported or d.lo <= value < d.end)]


def holding(d: Target, address: int) -> tuple[int, int]:
    """The extent of the object that holds `address` in d: from the last
    symbol its link places in a section at or before it to the next one (or
    d's end)."""
    i = bisect_right(d.starts, address)
    return (d.starts[i - 1] if i else d.lo), (d.starts[i] if i < len(d.starts) else d.end)


def check(targets: list[Target]) -> tuple[list[str], dict[str, int]]:
    """The disagreements, and what agreed."""
    errors: list[str] = []
    counts = {"names": 0, "by_name": 0, "by_view": 0, "by_address": 0, "inside_object": 0,
              "mode_entries": 0}

    def disagree(where: str, d: Target, name: str) -> None:
        found = ", ".join(f"{v:08x}" for v in sorted(d.names[name]))
        errors.append(f"{where}, but {d.name} defines {name} at {found}")

    for t in targets:
        for name, (script, expression) in sorted(t.imports.items()):
            value = t.value.get(name)
            if value is None or t.lo <= value < t.end:
                continue
            holders = [d for d in targets if d is not t and d.lo <= value < d.end]
            if not holders:
                continue
            counts["names"] += 1
            where = f"{t.name}: {name} = {value:08x} ({script})"
            given = ADDRESS_NAME.fullmatch(name)
            if given and int(given.group(1), 16) != value:
                errors.append(f"{where}, but its name gives {int(given.group(1), 16):08x}")
                continue
            named = definers(targets, t, name, value)
            view = VIEW.fullmatch(expression.strip())
            base = view.group(1) if view and view.group(1) != name else None
            bases = (definers(targets, t, base, t.value[base])
                     if base is not None and base in t.value else [])
            if named:
                counts["by_name"] += 1
                for d in named:
                    if value not in d.names[name]:
                        disagree(where, d, name)
            elif bases:
                counts["by_view"] += 1
                for d in bases:
                    if t.value[base] not in d.names[base]:
                        disagree(f"{where} is {expression}", d, base)
                        continue
                    start, stop = holding(d, t.value[base])
                    if not start <= value < stop:
                        errors.append(f"{where} is {expression}, outside the object holding"
                                      f" {base} in {d.name} ({start:08x}-{stop:08x})")
            elif any(value in d.at for d in holders):
                counts["by_address"] += 1
            elif any(a <= value < b for d in holders for a, b in d.inputs):
                counts["inside_object"] += 1
            else:
                errors.append(f"{where} lies in {', '.join(d.name for d in holders)} but in"
                              " no input section they place")
    tables = [t for t in targets if t.values.get("MODE_TABLE")]
    modes = [t for t in targets if t.values.get("MODE")]
    if modes and not tables:
        errors.append("mode overlays (MODE) but no target with the mode table (MODE_TABLE)")
    for r in tables:
        symbol = r.values["MODE_TABLE"]
        if len(r.names.get(symbol, ())) != 1:
            errors.append(f"{r.name}: no single mode table {symbol}")
            continue
        (base,) = r.names[symbol]
        for o in modes:
            index = int(o.values["MODE"], 0)
            entry, start, stop = (word(r, base + MODE_ENTRY_SIZE * index + 4 * i)
                                  for i in range(3))
            where = f"{r.name}: {symbol}[{index}]"
            function = o.values.get("MODE_ENTRY")
            if function and entry not in o.names.get(function, ()):
                found = ", ".join(f"{v:08x}" for v in sorted(o.names.get(function, ())))
                errors.append(f"{where} enters {entry:08x}, not {o.name}'s {function}"
                              f" ({found or 'undefined'})")
            if o.bss is None:
                errors.append(f"{where}: {o.name} links no uninitialized data")
            elif (start + 4, stop + 4) != o.bss:
                errors.append(f"{where} clears {start + 4:08x}-{stop + 4:08x}, but {o.name}"
                              f" links its uninitialized data at {o.bss[0]:08x}-{o.bss[1]:08x}")
            counts["mode_entries"] += 1
    return errors, counts


def numbers(targets: list[Target]) -> list[str]:
    """Each address of another target that a link holds as a number, which
    neither check() nor the relocation scan (a target's own range) sees: a lui
    without R_MIPS_HI16 with the next instruction that completes it from the
    same register without R_MIPS_LO16, and a word without R_MIPS_32 in a
    loaded data section or in .text outside every function. Bytes classified
    asset or included (original data) and the mode table, which check()
    compares, are left out. A listing for review, not a check."""
    found = []
    count = max((int(o.values["MODE"], 0) + 1 for o in targets if o.values.get("MODE")),
                default=0)
    for t in targets:
        image = Path(t.values["IMAGE"])
        _sections, symbols = read_elf(Path(f"{image}.elf"))
        spans = sorted((s.value, s.value + s.size) for s in symbols
                       if s.kind == STT_FUNC and s.section and s.size)
        starts = [a for a, _ in spans]
        left = [(a, b) for a, b, kind, _note in
                (classification(Path(t.values["CLASSIFICATION"]))
                 if t.values.get("CLASSIFICATION") else []) if kind in ("asset", "included")]
        if t.values.get("MODE_TABLE"):
            left += [(v, v + MODE_ENTRY_SIZE * count) for v in t.names[t.values["MODE_TABLE"]]]
        loaded = [s for s in t.sections if s.type == SHT_PROGBITS and s.flags & SHF_ALLOC]

        def held(address: int, t: Target = t) -> list[str]:
            return [d.name for d in targets
                    if d is not t and d.lo <= address < d.end and not t.lo <= address < t.end]

        objects: dict[str, list[Section]] = {}
        for name, address, size, obj in map_sections(Path(f"{image}.map")):
            if not any(s.address <= address < s.address + s.size for s in loaded):
                continue
            sections = objects.setdefault(obj, read_elf(Path(obj))[0])
            (index,) = [i for i, s in enumerate(sections) if s.name == name]
            kinds: dict[int, set[int]] = {}
            for section in sections:
                if section.type == SHT_REL and section.info == index:
                    for offset, info in struct.iter_unpack("<II", section.data):
                        kinds.setdefault(address + offset, set()).add(info & 0xFF)
            words = list(range((address + 3) & ~3, address + size - 3, 4))
            for i, a in enumerate(words):
                value = word(t, a)
                if any(lo <= a < hi for lo, hi in left):
                    continue
                k = bisect_right(starts, a) - 1
                if name == ".text" and k >= 0 and a < spans[k][1]:
                    if value >> 26 != 0x0F or R_MIPS_HI16 in kinds.get(a, ()):
                        continue
                    target, register = (value & 0xFFFF) << 16, (value >> 16) & 31
                    for b in words[i + 1:i + 12]:
                        following = word(t, b)
                        if (following >> 26 in LOW_HALF and (following >> 21) & 31 == register
                                and R_MIPS_LO16 not in kinds.get(b, ())):
                            low = following & 0xFFFF
                            target = (target | low if following >> 26 == 0x0D else
                                      (target + low - (low & 0x8000) * 2) & 0xFFFFFFFF)
                            break
                    if held(target):
                        found.append(f"{t.name} {a:08x} lui {target:08x} ({obj}):"
                                     f" {', '.join(held(target))}")
                elif R_MIPS_32 not in kinds.get(a, ()) and held(value):
                    found.append(f"{t.name} {a:08x} word {value:08x} ({name} of {obj}):"
                                 f" {', '.join(held(value))}")
    return found


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("configs", type=Path, nargs="+", help="every target's configuration")
    parser.add_argument("--numbers", action="store_true",
                        help="list the other targets' addresses each link holds as numbers")
    args = parser.parse_args()
    targets = [load(path) for path in args.configs]
    if args.numbers:
        print("\n".join(numbers(targets)))
        return
    errors, counts = check(targets)
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    if errors:
        sys.exit(1)
    print(json.dumps({"claim": "cross_image_agreement", "targets": len(targets), **counts}))


if __name__ == "__main__":
    main()
