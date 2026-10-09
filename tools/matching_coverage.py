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

With ``--map`` (the GNU ld map of the same link), every loaded data byte is
attributed by the object that supplied its input section: .rodata*, .data*
and .sdata*, and the .bss/.sbss that lies in a loaded (PROGBITS) output
section, where an image holds its uninitialized variables as zeros. NOLOAD
.bss is not in the image and not counted. The classes:

* ``c``            emitted by the compiler from a decomp/src C unit
* ``bss``          uninitialized variables a decomp/src C unit defines in loaded
                   .bss/.sbss (zero in the file)
* ``included``     original bytes INCLUDE_RODATA'd (strings) or INCLUDE_ORIGINAL'd
                   (.data objects) in a C unit: objects whose padding holds
                   stray assembler bytes (docs/matching.md)
* ``handwritten``  an authored assembly unit under decomp/src
* ``sdk``/``asset``/``handwritten``  generated data inside a classified range
                   (``asset``: user-supplied game data/bytecode, not source)
* ``placeholder``  generated data or loaded .bss from the original image
                   (remaining work)

Bytes outside every input section (alignment gaps, a packer's zero tail) are
not counted. Each INCLUDE_RODATA, INCLUDE_ORIGINAL and INCLUDE_ASSET name in
the sources (.c and .h; a macro wrapping one is rejected) must resolve to
exactly one sized, section-relative ELF symbol (a linker-script assignment
would make it absolute), and an INCLUDE_ASSET object must lie inside an
``asset`` range; otherwise the report fails rather than count original bytes
as C. Instruction counts are static MIPS words (four bytes each) in the same
ELF function ranges as the byte totals, including nops and branch delay
slots. Paths in the map are relative to the working directory, the
repository root.
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
INCLUDE_ASSET = re.compile(r"INCLUDE_ASSET\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*,")
WRAPPER = re.compile(r"^[ \t]*#[ \t]*define\b.*\bINCLUDE_(RODATA|ORIGINAL|ASSET)\b", re.M)
DATA_SECTION = re.compile(r"\.(rodata|data|sdata|sbss|bss)\b")
BSS_SECTION = re.compile(r"\.s?bss\b")
NON_MATCHING = re.compile(r"#ifdef\s+NON_MATCHING(.*?)#else(.*?)#endif", re.DOTALL)


def symbols(elf: Path) -> list[tuple[int, int, str, str, str]]:
    """(address, size, ELF type, section index, name) of every defined symbol."""
    out = subprocess.run(
        ["psx-readelf", "-sW", str(elf)], check=True, capture_output=True, text=True
    ).stdout
    result = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 8 and parts[0][:-1].isdigit() and parts[6] != "UND":
            result.append((int(parts[1], 16), int(parts[2], 0), parts[3], parts[6], parts[7]))
    return result


def loaded_ranges(elf: Path) -> list[tuple[int, int]]:
    """[start, end) of every allocated PROGBITS section: the bytes of the image."""
    out = subprocess.run(
        ["psx-readelf", "-SW", str(elf)], check=True, capture_output=True, text=True
    ).stdout
    result = []
    for line in out.splitlines():
        match = re.search(r"\]\s+\S+\s+PROGBITS\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\S+\s+(\S*A\S*)", line)
        if match:
            start = int(match.group(1), 16)
            result.append((start, start + int(match.group(2), 16)))
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
    """(section, address, size, object) of every nonempty data or .bss input section."""
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
    """Bytes per data class; generated data is split at classified range edges.

    `sections` holds loaded input sections only (filter NOLOAD .bss first)."""
    totals: dict[str, int] = {}

    def add(cls: str, count: int) -> None:
        if count:
            totals[cls] = totals.get(cls, 0) + count

    for name, start, size, obj in sections:
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
            if not authored.with_suffix(".c").exists():
                kind = "handwritten"
            else:
                kind = "bss" if BSS_SECTION.match(name) else "c"
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


def source_names(roots: list[Path]) -> tuple[set[str], set[str], set[str], set[str]]:
    """Names linked as assembly and reviewed nonmatching candidates in the C units
    under roots, and the original data objects of their C units and headers:
    INCLUDE_RODATA/INCLUDE_ORIGINAL (`included`) and INCLUDE_ASSET (assets)."""
    asm_names: set[str] = set()
    nonmatching: set[str] = set()
    included: set[str] = set()
    assets: set[str] = set()
    for root in roots:
        for source in sorted(root.rglob("*.[ch]")):
            text = source.read_text()
            if WRAPPER.search(text.replace("\\\n", " ")):
                raise SystemExit(f"{source}: a macro wrapping INCLUDE_RODATA/ORIGINAL/ASSET hides its names")
            if source.suffix == ".c":
                for _block, fallback in NON_MATCHING.findall(text):
                    nonmatching.update(INCLUDE_ASM.findall(fallback))
                asm_names.update(INCLUDE_ASM.findall(text))
            included.update(INCLUDE_RODATA.findall(text))
            included.update(INCLUDE_ORIGINAL.findall(text))
            assets.update(INCLUDE_ASSET.findall(text))
    return asm_names, nonmatching, included, assets


def original_objects(
    table: list[tuple[int, int, str, str, str]],
    included_names: set[str],
    asset_names: set[str],
    ranges: list[tuple[int, int, str, str]],
) -> list[tuple[int, int]]:
    """[start, end) of each included object; fails on a name that cannot be trusted."""
    found: dict[str, list[tuple[int, int, str]]] = {}
    for address, size, _kind, section, name in table:
        if name in included_names or name in asset_names:
            found.setdefault(name, []).append((address, size, section))

    def bounds(name: str) -> tuple[int, int]:
        entries = found.get(name, [])
        if len(entries) != 1:
            raise SystemExit(f"{name}: {'no' if not entries else len(entries)} ELF symbols, expected one")
        address, size, section = entries[0]
        if section == "ABS":
            raise SystemExit(f"{name}: an absolute symbol (a linker-script assignment shadows it)")
        if not size:
            raise SystemExit(f"{name}: the ELF symbol has no size")
        return address, address + size

    for name in sorted(asset_names):
        start, end = bounds(name)
        if not any(s <= start and end <= e for s, e, kind, _ in ranges if kind == "asset"):
            raise SystemExit(f"{name}: INCLUDE_ASSET object {start:08x}-{end:08x} outside every asset range")
    return [bounds(name) for name in sorted(included_names)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--src", type=Path, action="append", default=[])
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--map", type=Path, help="GNU ld map of the same link (data coverage)")
    parser.add_argument("--list", choices=["c", "nonmatching", "sdk", "handwritten", "asm"])
    args = parser.parse_args()

    asm_names, nonmatching, included_names, asset_names = source_names(args.src)
    ranges = classification(args.classification)

    table = symbols(args.elf)
    bounds = {name: address for address, _, _, _, name in table}
    # splat's section-boundary symbols; data inside the same output section is excluded.
    text = [
        (bounds[name], bounds[name.replace("_START", "_END")])
        for name in bounds
        if name.endswith("_TEXT_START") and name.replace("_START", "_END") in bounds
    ]
    totals: dict[str, list[int]] = {}
    listing = []
    for address, size, kind, _section, name in table:
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
        included = original_objects(table, included_names, asset_names, ranges)
        loaded = loaded_ranges(args.elf)
        sections = [
            s for s in map_sections(args.map)
            if not BSS_SECTION.match(s[0]) or any(lo <= s[1] and s[1] + s[2] <= hi for lo, hi in loaded)
        ]
        data = data_coverage(sections, included, ranges, Path.cwd())
        report["data_bytes"] = sum(data.values())
        report["data_classes"] = dict(sorted(data.items()))
        report["remaining_data_placeholder_bytes"] = data.get("placeholder", 0)
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
