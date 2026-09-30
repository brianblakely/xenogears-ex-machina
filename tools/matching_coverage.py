#!/usr/bin/env python3
"""Report source coverage of one linked target, separately from binary matching.

Every function symbol in the linked ELF is attributed to exactly one class:

* ``c``            compiled from reviewed C under decomp/src (no INCLUDE_ASM)
* ``nonmatching``  still linked as assembly, but a reviewed C candidate exists in
                   a ``#ifdef NON_MATCHING`` block (does not satisfy the exit)
* ``sdk``          assembly inside a range classified as PsyQ SDK library code
* ``handwritten``  assembly inside a range classified as original hand-written asm
* ``asm``          unrecovered original assembly (remaining work)

The classification file lists ``START END CLASS NOTE...`` lines (hex VRAM,
END exclusive). Data sections are reported by size only. This tool never
reads or asserts binary agreement; run the exact comparison separately.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path

INCLUDE_ASM = re.compile(r"INCLUDE_ASM\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*\)")
NON_MATCHING = re.compile(r"#ifdef\s+NON_MATCHING(.*?)#else(.*?)#endif", re.DOTALL)


def symbols(elf: Path) -> list[tuple[int, int, str, str]]:
    """(address, size, ELF type, name) of every defined symbol."""
    out = subprocess.run(
        ["psx-readelf", "-sW", str(elf)], check=True, capture_output=True, text=True
    ).stdout
    result = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 8 and parts[0][:-1].isdigit() and parts[6] != "UND":
            result.append((int(parts[1], 16), int(parts[2], 0), parts[3], parts[7]))
    return result


def classification(path: Path | None) -> list[tuple[int, int, str, str]]:
    if path is None:
        return []
    ranges = []
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            start, end, kind, *note = line.split()
            if kind not in ("sdk", "handwritten"):
                raise SystemExit(f"unknown classification {kind!r}")
            ranges.append((int(start, 16), int(end, 16), kind, " ".join(note)))
    return ranges


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--src", type=Path, action="append", default=[])
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--list", choices=["c", "nonmatching", "sdk", "handwritten", "asm"])
    args = parser.parse_args()

    asm_names: set[str] = set()
    nonmatching: set[str] = set()
    for root in args.src:
        for source in root.rglob("*.c"):
            text = source.read_text()
            for block, fallback in NON_MATCHING.findall(text):
                nonmatching.update(INCLUDE_ASM.findall(fallback))
            asm_names.update(INCLUDE_ASM.findall(text))
    ranges = classification(args.classification)

    table = symbols(args.elf)
    bounds = {name: address for address, _, _, name in table}
    # splat's section-boundary symbols; data inside the same output section is excluded.
    text = [
        (bounds[name], bounds[name.replace("_START", "_END")])
        for name in bounds
        if name.endswith("_TEXT_START") and name.replace("_START", "_END") in bounds
    ]
    totals: dict[str, list[int]] = {}
    listing = []
    for address, size, kind, name in table:
        if kind != "FUNC" or name.startswith("__maspsx_include_asm_hack"):
            continue
        if not any(s <= address < e for s, e in text):
            continue
        if name in nonmatching:
            cls = "nonmatching"
        elif name in asm_names:
            cls = next((k for s, e, k, _ in ranges if s <= address < e), "asm")
        else:
            cls = "c"
        entry = totals.setdefault(cls, [0, 0])
        entry[0] += 1
        entry[1] += size
        if cls == args.list:
            listing.append(f"{address:08x} {size:6d} {name}")
    if args.list:
        print("\n".join(listing))
        return
    text_bytes = sum(v[1] for v in totals.values())
    report = {
        "claim": "source_coverage_only",
        "binary_agreement": "not_measured",
        "text_bytes": text_bytes,
        "classes": {k: {"functions": v[0], "bytes": v[1]} for k, v in sorted(totals.items())},
        "remaining_asm_bytes": totals.get("asm", [0, 0])[1] + totals.get("nonmatching", [0, 0])[1],
    }
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
