#!/usr/bin/env python3
"""Debugging aid: list functions whose rebuilt bytes differ, and show one side by side.

    matching_diff.py CONFIG.mk            # every differing function
    matching_diff.py CONFIG.mk -f NAME    # instruction diff of one function
    matching_diff.py CONFIG.mk -f NAME --original-address 0xADDR --original-size 0xSIZE

Reads ORIGINAL/IMAGE from the target fragment and function symbols from the
linked ELF. By default, both images use the rebuilt function's address and size.
For a relocated or resized function, supply its original bounds from the original
disassembly. Comparing function by function only locates a difference; the
exact whole-image comparison (make verify) remains the acceptance check.
"""

from __future__ import annotations

import argparse
import re
import subprocess
from itertools import zip_longest
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


def function_bytes(image: bytes, address: int, size: int, delta: int, label: str) -> bytes:
    start = address + delta
    if address % 4 or size <= 0 or size % 4:
        raise SystemExit(f"{label}: function address and positive size must be word-aligned")
    if start < 0 or start + size > len(image):
        raise SystemExit(f"{label}: function {address:#x}+{size:#x} is outside the image")
    return image[start : start + size]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    parser.add_argument("-f", "--function")
    parser.add_argument("--original-address", type=lambda value: int(value, 0))
    parser.add_argument("--original-size", type=lambda value: int(value, 0))
    args = parser.parse_args()
    bounds = args.original_address is not None or args.original_size is not None
    if bounds and (not args.function or args.original_address is None or args.original_size is None):
        parser.error("original bounds require --function, --original-address and --original-size")
    values = config(args.config)
    image = ROOT / values["IMAGE"]
    elf = Path(str(image) + ".elf")
    original = (ROOT / values["ORIGINAL"]).read_bytes()
    rebuilt = image.read_bytes()
    delta = load_offset(elf)
    for address, size, name in functions(elf):
        if args.function:
            if name != args.function:
                continue
            original_address = args.original_address if bounds else address
            original_size = args.original_size if bounds else size
            a = function_bytes(original, original_address, original_size, delta, "original")
            b = function_bytes(rebuilt, address, size, delta, "rebuilt")
            print(f"{name}: original {original_address:08x}+{original_size:#x}; rebuilt {address:08x}+{size:#x}")
            left, right = text(a, original_address), text(b, address)
            for i, (x, y) in enumerate(zip_longest(left, right, fillvalue="<absent>")):
                equal = a[i * 4 : i * 4 + 4] == b[i * 4 : i * 4 + 4]
                print(f"{'  ' if equal else '!!'} {x:<52} | {y}")
            return
        if size == 0:
            continue
        a = function_bytes(original, address, size, delta, "original")
        b = function_bytes(rebuilt, address, size, delta, "rebuilt")
        if a != b:
            first = next(i for i in range(size) if a[i] != b[i])
            print(f"{address:08x} {size:6d} {name} first difference +{first:#x}")
    if args.function:
        parser.error(f"function not found: {args.function}")


if __name__ == "__main__":
    main()
