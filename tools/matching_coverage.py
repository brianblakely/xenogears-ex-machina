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
END exclusive). This tool never reads or asserts binary agreement; run the
exact comparison separately.

With ``--map`` (the GNU ld map of the same link), loaded data input sections
(.rodata*, .data*, .sdata*) are attributed by the object that supplied them:

* ``c``            emitted by the compiler from a decomp/src C unit
* ``included``     original bytes INCLUDE_RODATA'd (strings) or INCLUDE_ORIGINAL'd
                   (.data objects) in a C unit: objects whose padding holds
                   stray assembler bytes (docs/matching.md)
* ``handwritten``  an authored assembly unit under decomp/src
* ``sdk``/``asset``/``handwritten``  generated data inside a classified range
                   (``asset``: user-supplied game data/bytecode, not source)
* ``placeholder``  generated data from the original image (remaining work)
Instruction counts are static MIPS words (four bytes each) in the same ELF
function ranges as the byte totals, including nops and branch delay slots.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path

INCLUDE_ASM = re.compile(r"INCLUDE_ASM\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*\)")
INCLUDE_RODATA = re.compile(r"INCLUDE_RODATA\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*\)")
INCLUDE_ORIGINAL = re.compile(r"INCLUDE_ORIGINAL\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*,")
DATA_SECTION = re.compile(r"\.(rodata|data|sdata)\b")
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
            if kind not in ("sdk", "handwritten", "asset"):
                raise SystemExit(f"unknown classification {kind!r}")
            ranges.append((int(start, 16), int(end, 16), kind, " ".join(note)))
    return ranges


def map_sections(path: Path) -> list[tuple[str, int, int, str]]:
    """(section, address, size, object) of every nonempty loaded data input section."""
    result, pending = [], None
    for line in path.read_text().splitlines():
        parts = line.split()
        if line.startswith(" .") and len(parts) == 1:
            pending = parts[0]  # GNU ld puts a long section name on a line of its own
            continue
        if pending and len(parts) == 3:
            parts = [pending] + parts
        elif not line.startswith(" ."):
            parts = []
        pending = None
        if len(parts) != 4 or not DATA_SECTION.match(parts[0]):
            continue
        name, address, size, obj = parts
        if address.startswith("0x") and size.startswith("0x") and int(size, 16):
            result.append((name, int(address, 16), int(size, 16), obj))
    return result


def data_coverage(
    sections: list[tuple[str, int, int, str]],
    included: list[tuple[int, int]],
    ranges: list[tuple[int, int, str, str]],
    root: Path,
) -> dict[str, int]:
    """Bytes per data class; generated data is split at classified range edges."""
    totals: dict[str, int] = {}

    def add(cls: str, count: int) -> None:
        if count:
            totals[cls] = totals.get(cls, 0) + count

    for _name, start, size, obj in sections:
        end = start + size
        if "/decomp/src/" in obj:
            unit = Path(obj.split("/decomp/src/", 1)[1]).with_suffix("")
            authored = root / "decomp/src" / unit
            inside = sum(max(0, min(e, end) - max(s, start)) for s, e in included)
            # INCLUDE_ASSET bytes inside an authored unit count under their class.
            classified = 0
            for s, e, cls, _note in ranges:
                overlap = max(0, min(e, end) - max(s, start))
                if overlap:
                    add(cls, overlap)
                    classified += overlap
            kind = "c" if authored.with_suffix(".c").exists() else "handwritten"
            add(kind, size - inside - classified)
            add("included", inside)
            continue
        cursor = start
        for s, e, kind, _note in sorted(ranges):
            lo, hi = max(s, cursor), min(e, end)
            if lo < hi:
                add("placeholder", lo - cursor)
                add(kind, hi - lo)
                cursor = hi
        add("placeholder", end - cursor)
    return totals


def source_names(roots: list[Path]) -> tuple[set[str], set[str], set[str]]:
    """Names linked as assembly, reviewed nonmatching candidates and original
    data objects (INCLUDE_RODATA, INCLUDE_ORIGINAL) in the C units under roots."""
    asm_names: set[str] = set()
    nonmatching: set[str] = set()
    included: set[str] = set()
    for root in roots:
        for source in root.rglob("*.c"):
            text = source.read_text()
            for _block, fallback in NON_MATCHING.findall(text):
                nonmatching.update(INCLUDE_ASM.findall(fallback))
            asm_names.update(INCLUDE_ASM.findall(text))
            included.update(INCLUDE_RODATA.findall(text))
            included.update(INCLUDE_ORIGINAL.findall(text))
    return asm_names, nonmatching, included


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--src", type=Path, action="append", default=[])
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--map", type=Path, help="GNU ld map of the same link (data coverage)")
    parser.add_argument("--list", choices=["c", "nonmatching", "sdk", "handwritten", "asm"])
    args = parser.parse_args()

    asm_names, nonmatching, included_names = source_names(args.src)
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
        if address % 4 or size % 4:
            raise SystemExit(f"unaligned MIPS function range: {name} at {address:08x}, size {size}")
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
    remaining = [
        sum(totals.get(cls, [0, 0])[i] for cls in ("asm", "nonmatching"))
        for i in (0, 1)
    ]
    report = {
        "claim": "source_coverage_only",
        "binary_agreement": "not_measured",
        "text_bytes": text_bytes,
        "text_instructions": text_bytes // 4,
        "classes": {
            k: {"functions": v[0], "bytes": v[1], "instructions": v[1] // 4}
            for k, v in sorted(totals.items())
        },
        "remaining_asm_functions": remaining[0],
        "remaining_asm_bytes": remaining[1],
        "remaining_asm_instructions": remaining[1] // 4,
    }
    if args.map:
        included = [(a, a + n) for a, n, _, name in table if name in included_names]
        root = Path(__file__).resolve().parents[1]
        data = data_coverage(map_sections(args.map), included, ranges, root)
        report["data_bytes"] = sum(data.values())
        report["data_classes"] = dict(sorted(data.items()))
        report["remaining_data_placeholder_bytes"] = data.get("placeholder", 0)
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
