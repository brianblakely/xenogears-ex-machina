#!/usr/bin/env python3
"""Triage aid: score each NON_MATCHING draft of a target against its original function.

    nonmatching_score.py decomp/targets/overlays/field.mk [func_8007XXXX ...]

Compiles every source file holding a selected draft with -DNON_MATCHING into a side
build (.local/decomp/nmscore/<target>/), then prints, per function,
`target address size name SCORE KIND file sizes original/compiled`, lowest first.
SCORE is the edit distance between mnemonic+register sequences plus 2 for every
aligned word that differs outside a relocation. KIND is `frame` when only $sp
offsets differ, `exact` when every non-relocated word agrees.

Blind spot: words carrying a relocation in the object (hi/lo halves, jal targets)
are compared without their immediates, so a wrong struct offset or symbol+offset
scores as equal. Check near misses in the real image (remove the guard, `make
verify`, tools/matching_diff.py -f). Run inside the matching shell from the
repository root (a worktree root works too).
"""

from __future__ import annotations

import argparse
import difflib
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import rabbitizer

ROOT = Path.cwd() if (Path.cwd() / "decomp/Makefile").exists() else Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from matching_coverage import INCLUDE_ASM, NON_MATCHING  # noqa: E402
from matching_diff import config, functions, load_offset  # noqa: E402

IMMEDIATE = re.compile(r"(-?0x[0-9A-Fa-f]+|\b-?\d+\b|%\w+\([^)]*\)|\b(func|D|jtbl|L)_?[0-9A-Fa-f_]+\w*)")


def words(blob: bytes) -> list[int]:
    return [int.from_bytes(blob[i : i + 4], "little") for i in range(0, len(blob) - 3, 4)]


def shape(blob: bytes) -> list[str]:
    return [IMMEDIATE.sub("I", rabbitizer.Instruction(w).disassemble()) for w in words(blob)]


def object_functions(obj: Path) -> tuple[dict[str, tuple[int, int]], bytes, set[int]]:
    symbols = subprocess.run(["psx-readelf", "-sW", str(obj)], capture_output=True, text=True).stdout
    found = {}
    for line in symbols.splitlines():
        p = line.split()
        if len(p) == 8 and p[3] == "FUNC" and p[6] != "UND":
            found[p[7]] = (int(p[1], 16), int(p[2], 0))
    text = subprocess.run(["psx-objcopy", "-O", "binary", "-j", ".text", str(obj), "/dev/stdout"],
                          capture_output=True).stdout
    relocations, in_text = set(), False
    for line in subprocess.run(["psx-readelf", "-rW", str(obj)], capture_output=True, text=True).stdout.splitlines():
        if line.startswith("Relocation section"):
            in_text = "'.rel.text'" in line
        elif in_text and line[:1] in "0123456789abcdef" and line.split():
            try:
                relocations.add(int(line.split()[0], 16))
            except ValueError:
                pass
    return found, text, relocations


def score(original: bytes, rebuilt: bytes, offset: int, relocations: set[int]) -> tuple[int, str]:
    a, b = shape(original), shape(rebuilt)
    matcher = difflib.SequenceMatcher(None, a, b, autojunk=False)
    blocks = matcher.get_matching_blocks()
    edits = len(a) + len(b) - 2 * sum(block.size for block in blocks)
    differing = []
    for block in blocks:
        for k in range(block.size):
            i, j = block.a + k, block.b + k
            x, y = original[4 * i : 4 * i + 4], rebuilt[4 * j : 4 * j + 4]
            if x != y and offset + 4 * j not in relocations:
                edits += 2
                differing.append(x)
    kind = "other"
    if len(original) == len(rebuilt) and a == b:
        sp = all("$sp" in rabbitizer.Instruction(int.from_bytes(x, "little")).disassemble() for x in differing)
        kind = "exact" if not differing else ("frame" if sp else "imm")
    return edits, kind


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("config", type=Path)
    parser.add_argument("functions", nargs="*")
    args = parser.parse_args()

    config_path = args.config if args.config.is_absolute() else ROOT / args.config
    values = config(config_path)
    target = config_path.stem
    elf = ROOT / (values["IMAGE"] + ".elf")
    original_image = (ROOT / values["ORIGINAL"]).read_bytes()
    delta = load_offset(elf)
    symbols = {name: (address, size) for address, size, name in functions(elf)}

    by_file: dict[Path, list[str]] = defaultdict(list)
    for directory in values["SOURCE_DIRS"].split():
        for source in sorted((ROOT / directory).rglob("*.c")):
            for _, fallback in NON_MATCHING.findall(source.read_text()):
                for name in INCLUDE_ASM.findall(fallback):
                    if not args.functions or name in args.functions:
                        by_file[source].append(name)

    side = f".local/decomp/nmscore/{target}"
    rows = []
    for source, names in by_file.items():
        obj = ROOT / side / source.relative_to(ROOT).with_suffix(".o")
        build = subprocess.run(
            ["make", "-s", "-C", str(ROOT / "decomp"), f"CONFIG={config_path.relative_to(ROOT / 'decomp')}",
             f"BUILD={side}", "TARGET_CPPFLAGS=-DNON_MATCHING", str(obj)],
            capture_output=True, text=True)
        if build.returncode:
            reason = (build.stderr.strip().splitlines() or ["?"])[-1][:100]
            rows += [(10**9, f"{target} {name} ERR {source.name} {reason}") for name in names]
            continue
        compiled, text, relocations = object_functions(obj)
        for name in names:
            address, size = symbols[name]
            if name not in compiled:
                rows.append((10**9, f"{target} {address:08x} {size:6d} {name} MISSING {source.name}"))
                continue
            offset, compiled_size = compiled[name]
            original = original_image[address + delta : address + delta + size]
            edits, kind = score(original, text[offset : offset + compiled_size], offset, relocations)
            rows.append((edits, f"{target} {address:08x} {size:6d} {name} {edits} {kind} {source.name} "
                                f"sizes {size}/{compiled_size}"))
    for _, row in sorted(rows):
        print(row)


if __name__ == "__main__":
    main()
