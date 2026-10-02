#!/usr/bin/env python3
"""Debugging aid: list functions whose rebuilt bytes differ, and show one side by side.

    matching_diff.py CONFIG.mk            # every differing function
    matching_diff.py CONFIG.mk -f NAME    # instruction diff of one function

Reads ORIGINAL/IMAGE from the target fragment and function symbols from the
linked ELF. Comparing function by function only locates a difference; the
exact whole-image comparison (make verify) remains the acceptance check.
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

import rabbitizer

ROOT = Path(__file__).resolve().parents[1]


def config(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text().splitlines():
        match = re.match(r"(\w+)\s*:=\s*(.*)", line)
        if match:
            values[match.group(1)] = match.group(2).strip()
    return values


def functions(elf: Path) -> list[tuple[int, int, str]]:
    out = subprocess.run(["psx-readelf", "-sW", str(elf)], check=True, capture_output=True, text=True)
    result = []
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 8 and parts[0][:-1].isdigit() and parts[3] == "FUNC" and parts[6] != "UND":
            if not parts[7].startswith("__maspsx_include_asm_hack"):
                result.append((int(parts[1], 16), int(parts[2], 0), parts[7]))
    return sorted(result)


def load_offset(elf: Path) -> int:
    """File offset minus VRAM of the first loaded section (PS-X EXE header aware)."""
    out = subprocess.run(["psx-readelf", "-lW", str(elf)], check=True, capture_output=True, text=True)
    for line in out.stdout.splitlines():
        parts = line.split()
        if parts and parts[0] == "LOAD":
            vaddr, paddr = int(parts[2], 16), int(parts[3], 16)
            if vaddr >= 0x80000000:
                return paddr - vaddr
    raise SystemExit("no loaded segment")


def text(words: bytes, vram: int) -> list[str]:
    lines = []
    for i in range(0, len(words) - 3, 4):
        word = int.from_bytes(words[i : i + 4], "little")
        lines.append(f"{vram + i:08x} {word:08x} {rabbitizer.Instruction(word, vram + i).disassemble()}")
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    parser.add_argument("-f", "--function")
    args = parser.parse_args()
    values = config(args.config)
    image = ROOT / values["IMAGE"]
    elf = Path(str(image) + ".elf")
    original = (ROOT / values["ORIGINAL"]).read_bytes()
    rebuilt = image.read_bytes()
    delta = load_offset(elf)
    for address, size, name in functions(elf):
        start = address + delta
        a, b = original[start : start + size], rebuilt[start : start + size]
        if args.function:
            if name != args.function:
                continue
            left, right = text(a, address), text(b, address)
            for x, y in zip(left, right):
                print(f"{'  ' if x == y else '!!'} {x:<52} | {y[18:]}")
            return
        if a != b:
            first = next(i for i in range(size) if a[i] != b[i])
            print(f"{address:08x} {size:6d} {name} first difference +{first:#x}")


if __name__ == "__main__":
    main()
