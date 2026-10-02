#!/usr/bin/env python3
"""Rewrite placeholder symbol names in decomp sources to the names in symbol files.

    decomp_rename.py SYMBOLS [SYMBOLS...] --src DIR [--src DIR...]

Every `func_XXXXXXXX` / `D_XXXXXXXX` identifier whose address a symbol file
names (splat format, `name = 0xADDR;`) is replaced in the C sources and
headers under the given directories. Resident names apply to every target;
an overlay's own symbol file must only be applied to that overlay's sources,
because overlays sharing a load address are distinct symbol spaces.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ENTRY = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]{8})\s*;")
PLACEHOLDER = re.compile(r"\b(func|D)_([0-9A-F]{8})\b")


def names(paths: list[Path]) -> dict[int, str]:
    result: dict[int, str] = {}
    for path in paths:
        for line in path.read_text().splitlines():
            match = ENTRY.match(line)
            if match and not PLACEHOLDER.fullmatch(match.group(1)):
                result[int(match.group(2), 16)] = match.group(1)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("symbols", type=Path, nargs="+")
    parser.add_argument("--src", type=Path, action="append", required=True)
    args = parser.parse_args()
    table = names(args.symbols)
    for root in args.src:
        for source in sorted([*root.rglob("*.c"), *root.rglob("*.h")]):
            text = source.read_text()
            renamed = PLACEHOLDER.sub(lambda m: table.get(int(m.group(2), 16), m.group(0)), text)
            if renamed != text:
                source.write_text(renamed)
                print(source)


if __name__ == "__main__":
    main()
