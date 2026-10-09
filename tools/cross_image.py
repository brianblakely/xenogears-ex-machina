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
that hold that address (their own symbols, not names they take themselves):

* by name: each such target whose ELF defines the name defines it there;
* by address, where none defines the name: one of them has a symbol there;
* otherwise the address must lie inside an input section one of them places:
  a member or part of an object that no symbol names, which this check
  cannot tie to a particular object.

A value outside every target (a size, a constant, a hardware or kernel
address) is no image's address and is not checked.

The resident's mode table (MODE_TABLE in its configuration: the ModeEntry
records of decomp/src/resident/mode.h, an entry, the BSS bounds and a decode
flag) must hold at each mode overlay's index (MODE in the overlay's
configuration) the overlay's entry symbol (MODE_ENTRY) and the bounds of its
uninitialized data as its rebuilt link places them. The dispatcher (main.c
func_80019ACC) clears the words after bss_start through bss_end
(func_80019560), so bss_start + 4 and bss_end + 4 must be the start and end
of the overlay's .sbss/.bss input sections. A mode whose entry is resident
code (battle) declares only MODE.

The report claims only that agreement; run it after the targets link.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from matching_coverage import (  # noqa: E402
    BSS_SECTION,
    SHF_ALLOC,
    SHT_PROGBITS,
    STT_FILE,
    STT_SECTION,
    Section,
    extent,
    map_sections,
    read_elf,
    script_assignments,
    script_names,
)
from matching_diff import config  # noqa: E402

# ModeEntry (decomp/src/resident/mode.h): entry, bss_start, bss_end, loaded.
MODE_ENTRY_SIZE = 16


@dataclass
class Target:
    name: str
    values: dict[str, str]
    sections: list[Section]
    lo: int
    end: int
    value: dict[str, int]  # every defined name's value in the link
    names: dict[str, set[int]]  # its own symbols (inside its range): their values
    at: dict[int, set[str]]  # and their names by value
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
    bounds = {name for name, _expression, _provide
              in script_assignments(Path(values["LINKER_SCRIPT"]))}
    value, names, at = {}, {}, {}
    for symbol in symbols:
        if symbol.name and symbol.section and symbol.kind not in (STT_SECTION, STT_FILE):
            value[symbol.name] = symbol.value
            if lo <= symbol.value < end and symbol.name not in bounds:
                names.setdefault(symbol.name, set()).add(symbol.value)
                at.setdefault(symbol.value, set()).add(symbol.name)
    inputs = map_sections(map_path)
    bss = [(a, a + size) for name, a, size, _obj in inputs if BSS_SECTION.match(name)]
    scripts = [Path(values["BUILD"]) / f"{script[:-4]}.ld" if script.endswith("_auto.txt")
               else Path(script) for script in values.get("LINKER_EXTRA", "").split()]
    return Target(path.stem, values, sections, lo, end, value, names, at,
                  sorted((a, a + size) for _name, a, size, _obj in inputs),
                  (min(a for a, _ in bss), max(b for _, b in bss)) if bss else None,
                  script_names(map_path, scripts))


def word(target: Target, address: int) -> int:
    for s in target.sections:
        if (s.type == SHT_PROGBITS and s.flags & SHF_ALLOC
                and s.address <= address and address + 4 <= s.address + s.size):
            return struct.unpack_from("<I", s.data, address - s.address)[0]
    raise SystemExit(f"{target.name}: no loaded word at {address:08x}")


def check(targets: list[Target]) -> tuple[list[str], dict[str, int]]:
    """The disagreements, and what agreed."""
    errors: list[str] = []
    counts = {"names": 0, "by_name": 0, "by_address": 0, "inside_object": 0,
              "mode_entries": 0}
    for t in targets:
        for name, (script, _expression) in sorted(t.imports.items()):
            value = t.value.get(name)
            if value is None or t.lo <= value < t.end:
                continue
            holders = [d for d in targets if d is not t and d.lo <= value < d.end]
            if not holders:
                continue
            counts["names"] += 1
            where = f"{t.name}: {name} = {value:08x} ({script})"
            named = [d for d in holders if name in d.names]
            if named:
                counts["by_name"] += 1
                for d in named:
                    if value not in d.names[name]:
                        found = ", ".join(f"{v:08x}" for v in sorted(d.names[name]))
                        errors.append(f"{where}, but {d.name} defines {name} at {found}")
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


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("configs", type=Path, nargs="+", help="every target's configuration")
    args = parser.parse_args()
    targets = [load(path) for path in args.configs]
    errors, counts = check(targets)
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    if errors:
        sys.exit(1)
    print(json.dumps({"claim": "cross_image_agreement", "targets": len(targets), **counts}))


if __name__ == "__main__":
    main()
