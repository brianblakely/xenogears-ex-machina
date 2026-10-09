#!/usr/bin/env python3
"""The menu's assembler front end (menu.mk sets MASPSX to it).

Runs maspsx with the arguments decomp/Makefile passes, then applies the menu
assembler's small-data allocation. maspsx appends a unit's uninitialized
variables, the .lcomm and .comm GCC emits after its code, as one .bss block
in declaration order; its -G stays the unit's 0, so the code addresses them
absolutely, as the original does. The menu's assembler put the objects of up
to 8 bytes in .sbss and the larger ones in .bss (menu.mk), so this moves each
object of that block whose .space is at most 8 to an .sbss block, keeping the
order within each. decomp/Makefile's slot filter then treats both blocks
alike.
"""

from __future__ import annotations

import subprocess
import sys

SMALL_DATA = 8  # bytes: the menu assembler's .sbss threshold


def split(lines: list[str]) -> list[str]:
    """maspsx output with its trailing .bss block divided into .sbss and .bss."""
    if ".section .bss" not in lines:
        return lines
    start = len(lines) - 1 - lines[::-1].index(".section .bss")  # maspsx appends it last
    small: list[str] = []
    large: list[str] = []
    pending: list[str] = []
    for line in lines[start + 1 :]:
        pending.append(line)
        fields = line.split()
        if fields and fields[0] == ".space":
            (small if int(fields[1]) <= SMALL_DATA else large).extend(pending)
            pending = []
    out = lines[:start]
    if small:
        out += [".section .sbss", *small]
    if large:
        out += [".section .bss", *large]
    return out + pending


def main() -> int:
    result = subprocess.run(["maspsx", *sys.argv[1:]], stdin=sys.stdin, stdout=subprocess.PIPE, text=True)
    if result.returncode:
        return result.returncode
    sys.stdout.write("".join(line + "\n" for line in split(result.stdout.splitlines())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
