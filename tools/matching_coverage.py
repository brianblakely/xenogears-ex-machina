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
                   stray assembler bytes, or rodata several assembly functions
                   share (docs/matching.md)
* ``nonmatching``/``asm``/``sdk``/``handwritten``  data that an INCLUDE_ASM'd
                   .s file defines (splat moves rodata only one function uses,
                   its jump tables and strings, into the function's file),
                   under that function's class
* ``handwritten``  an authored assembly unit under decomp/src
* ``sdk``/``asset``/``handwritten``  generated data inside a classified range
                   (``asset``: user-supplied game data/bytecode, not source)
* ``placeholder``  generated data or loaded .bss from the original image
                   (remaining work)

Within a C unit's input section a classified range takes precedence, then
the original objects the unit links (the classes above), then the unit's own
class; each byte counts once. Bytes outside every input section (alignment
gaps, a packer's zero tail) are not counted.

Original data reaches a C unit through the INCLUDE_* macros, and the report
fails rather than count it as C where it cannot attribute it: each
INCLUDE_ASM, INCLUDE_RODATA, INCLUDE_ORIGINAL and INCLUDE_ASSET token of the
sources (.c and .h, outside comments) must be spelled as the patterns read
it, ``INCLUDE_X("...", NAME...)``, and not be wrapped in a macro; each data
object of an included .s file (a ``dlabel``...``enddlabel`` pair in a
section other than .text, where any other line fails) and each
INCLUDE_RODATA, INCLUDE_ORIGINAL and INCLUDE_ASSET name must resolve to
exactly one sized, section-relative ELF symbol (a linker-script assignment
would make it absolute); an INCLUDE_ASSET object must lie inside an
``asset`` range. A .s file's objects in one section are assembled back to
back, so the alignment padding between them counts with them; the padding
its first ``.align`` puts after the unit's preceding data is no object's and
counts with the section (field's 7 zero bytes ahead of jtbl_8006FD30, as C).
Inline ``__asm__`` data outside these macros is not checked; no unit has any.

Instruction counts are static MIPS words (four bytes each) in the same ELF
function ranges as the byte totals, including nops and branch delay slots.
Paths in the map and the INCLUDE_ASM/INCLUDE_RODATA folders are relative to
the working directory, the repository root.
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
STRICT = {"ASM": INCLUDE_ASM, "RODATA": INCLUDE_RODATA, "ORIGINAL": INCLUDE_ORIGINAL, "ASSET": INCLUDE_ASSET}
INCLUDE_TOKEN = re.compile(r"\bINCLUDE_(ASM|RODATA|ORIGINAL|ASSET)\b")
# The .s file an INCLUDE_ASM or INCLUDE_RODATA links: (macro, folder, name).
INCLUDED_FILE = re.compile(r"INCLUDE_(ASM|RODATA)\(\s*\"([^\"]*)\"\s*,\s*(\w+)\s*\)")
WRAPPER = re.compile(r"^[ \t]*#[ \t]*define\b.*\bINCLUDE_(ASM|RODATA|ORIGINAL|ASSET)\b", re.M)
COMMENT_OR_LITERAL = re.compile(r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'|/\*.*?\*/|//[^\n]*", re.S)
DATA_SECTION = re.compile(r"\.(rodata|data|sdata|sbss|bss)\b")
BSS_SECTION = re.compile(r"\.s?bss\b")
NON_MATCHING = re.compile(r"#ifdef\s+NON_MATCHING(.*?)#else(.*?)#endif", re.DOTALL)
# Lines of a data section in an included .s file (splat's output): an object
# opens with `dlabel NAME` and closes with `enddlabel NAME` (its ELF size);
# alignment and splat's `nonmatching` marker emit no object bytes.
ASM_COMMENT = re.compile(r"/\*.*?\*/")
ASM_SECTION = {".text", ".data", ".rdata", ".sdata", ".bss", ".sbss"}
ASM_NEUTRAL = {".align", ".balign", ".p2align", "nonmatching"}
ASM_DATA = {
    ".byte", ".half", ".hword", ".short", ".2byte", ".word", ".4byte", ".int", ".long",
    ".ascii", ".asciz", ".string", ".space", ".skip", ".zero", ".fill", ".incbin", ".float", ".double",
}


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
    objects: list[tuple[int, int, str]],
    ranges: list[tuple[int, int, str, str]],
    root: Path,
) -> dict[str, int]:
    """Bytes per data class; generated data is split at classified range edges.

    `sections` holds loaded input sections only (filter NOLOAD .bss first);
    `objects` the (start, end, class) of the original data C units link
    (original_objects, assembly_data). In a decomp/src unit's section the
    classified ranges come first (INCLUDE_ASSET bytes count as `asset`), then
    the objects, then the unit's own class; each byte counts once."""
    totals: dict[str, int] = {}

    def add(cls: str, count: int) -> None:
        if count:
            totals[cls] = totals.get(cls, 0) + count

    for name, start, size, obj in sections:
        end = start + size
        if "/decomp/src/" in obj:
            unit = Path(obj.split("/decomp/src/", 1)[1]).with_suffix("")
            if not (root / "decomp/src" / unit).with_suffix(".c").exists():
                kind = "handwritten"
            else:
                kind = "bss" if BSS_SECTION.match(name) else "c"
            free = [(start, end)]
            for lo, hi, cls in [(s, e, k) for s, e, k, _note in ranges] + objects:
                left = []
                for a, b in free:
                    cut_lo, cut_hi = max(a, lo), min(b, hi)
                    if cut_lo >= cut_hi:
                        left.append((a, b))
                        continue
                    add(cls, cut_hi - cut_lo)
                    left += [part for part in ((a, cut_lo), (cut_hi, b)) if part[0] < part[1]]
                free = left
            add(kind, sum(b - a for a, b in free))
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


def strip_comments(text: str) -> str:
    """C text without its comments (each becomes a space or its line breaks)."""

    def blank(match: re.Match[str]) -> str:
        found = match.group(0)
        return found if found[0] in "\"'" else "\n" * found.count("\n") or " "

    return COMMENT_OR_LITERAL.sub(blank, text)


def source_names(
    roots: list[Path],
) -> tuple[set[str], set[str], set[str], set[str], list[tuple[str, Path, str]]]:
    """Names linked as assembly and reviewed nonmatching candidates in the C units
    and headers under roots, their original data objects: INCLUDE_RODATA/
    INCLUDE_ORIGINAL (`included`) and INCLUDE_ASSET (assets), and the (macro,
    path, name) of each .s file an INCLUDE_ASM or INCLUDE_RODATA links. Fails
    on a use the patterns would miss: a wrapping macro or another spelling."""
    asm_names: set[str] = set()
    nonmatching: set[str] = set()
    included: set[str] = set()
    assets: set[str] = set()
    files: list[tuple[str, Path, str]] = []
    for root in roots:
        for source in sorted(root.rglob("*.[ch]")):
            text = strip_comments(source.read_text())
            if WRAPPER.search(text.replace("\\\n", " ")):
                raise SystemExit(f"{source}: a macro wrapping INCLUDE_ASM/RODATA/ORIGINAL/ASSET hides its names")
            for token in INCLUDE_TOKEN.finditer(text):
                if not STRICT[token.group(1)].match(text, token.start()):
                    macro = token.group(0)
                    line = text.count("\n", 0, token.start()) + 1
                    raise SystemExit(f'{source}:{line}: {macro} is not spelled {macro}("...", NAME...) as the report reads it')
            for _block, fallback in NON_MATCHING.findall(text):
                nonmatching.update(INCLUDE_ASM.findall(fallback))
            asm_names.update(INCLUDE_ASM.findall(text))
            included.update(INCLUDE_RODATA.findall(text))
            included.update(INCLUDE_ORIGINAL.findall(text))
            assets.update(INCLUDE_ASSET.findall(text))
            files += [(macro, Path(folder) / f"{name}.s", name) for macro, folder, name in INCLUDED_FILE.findall(text)]
    return asm_names, nonmatching, included, assets, files


def asm_data_objects(path: Path, section: str) -> list[tuple[str, str]]:
    """(section, name) of each data object an included .s file defines, read from
    `section` on (the macro's: .text for INCLUDE_ASM, .rodata for INCLUDE_RODATA).
    Every line of a section other than .text must be alignment, splat's
    `nonmatching` marker, or an object's `dlabel`, data and `enddlabel`."""
    objects: list[tuple[str, str]] = []
    current = None
    for number, line in enumerate(path.read_text().splitlines(), 1):
        words = ASM_COMMENT.sub(" ", line).split("#", 1)[0].replace(",", " ").split()
        if not words:
            continue
        head, where = words[0], f"{path}:{number}"
        if head == ".section" or head in ASM_SECTION:
            if current:
                raise SystemExit(f"{where}: {current} has no enddlabel before the section changes")
            section = words[1] if head == ".section" and len(words) > 1 else head
        elif head in (".previous", ".pushsection", ".popsection", ".subsection"):
            raise SystemExit(f"{where}: {head} is not followed by the coverage report")
        elif section.startswith(".text"):
            continue
        elif head == "dlabel" and len(words) == 2 and not current:
            current = words[1]
            objects.append((section, current))
        elif head == "enddlabel" and words[1:] == [current]:
            current = None
        elif not (head in ASM_NEUTRAL or head in ASM_DATA and current):
            raise SystemExit(f"{where}: {line.strip()!r} in {section} is no labelled object's data")
    if current:
        raise SystemExit(f"{path}: {current} has no enddlabel")
    return objects


def resolve(table: list[tuple[int, int, str, str, str]], names: set[str]) -> dict[str, tuple[int, int]]:
    """{name: (start, end)} of each name's one sized, section-relative ELF symbol;
    fails on a name that cannot be trusted."""
    found: dict[str, list[tuple[int, int, str]]] = {}
    for address, size, _kind, section, name in table:
        if name in names:
            found.setdefault(name, []).append((address, size, section))
    result = {}
    for name in sorted(names):
        entries = found.get(name, [])
        if len(entries) != 1:
            raise SystemExit(f"{name}: {'no' if not entries else len(entries)} ELF symbols, expected one")
        address, size, section = entries[0]
        if section == "ABS":
            raise SystemExit(f"{name}: an absolute symbol (a linker-script assignment shadows it)")
        if not size:
            raise SystemExit(f"{name}: the ELF symbol has no size")
        result[name] = (address, address + size)
    return result


def original_objects(
    table: list[tuple[int, int, str, str, str]],
    included_names: set[str],
    asset_names: set[str],
    ranges: list[tuple[int, int, str, str]],
) -> list[tuple[int, int]]:
    """[start, end) of each included object; fails on a name that cannot be trusted."""
    for name, (start, end) in resolve(table, asset_names).items():
        if not any(s <= start and end <= e for s, e, kind, _ in ranges if kind == "asset"):
            raise SystemExit(f"{name}: INCLUDE_ASSET object {start:08x}-{end:08x} outside every asset range")
    return list(resolve(table, included_names).values())


def assembly_data(
    table: list[tuple[int, int, str, str, str]],
    files: list[tuple[str, Path, str]],
    classes: dict[str, str],
) -> list[tuple[int, int, str]]:
    """(start, end, class) of the data each included .s file defines, per section
    from its first object to its last: `included` for an INCLUDE_RODATA file,
    the class of the function (`classes`) for an INCLUDE_ASM file."""
    result = []
    for macro, path, name in files:
        objects = asm_data_objects(path, ".text" if macro == "ASM" else ".rodata")
        if not objects:
            continue
        cls = "included" if macro == "RODATA" else classes.get(name)
        if cls is None:
            raise SystemExit(f"{path}: defines data, but {name} is no function of the image")
        bounds = resolve(table, {label for _section, label in objects})
        for section in dict.fromkeys(s for s, _label in objects):
            spans = [bounds[label] for s, label in objects if s == section]
            result.append((min(a for a, _ in spans), max(b for _, b in spans), cls))
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--src", type=Path, action="append", default=[])
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--map", type=Path, help="GNU ld map of the same link (data coverage)")
    parser.add_argument("--list", choices=["c", "nonmatching", "sdk", "handwritten", "asm"])
    args = parser.parse_args()

    asm_names, nonmatching, included_names, asset_names, files = source_names(args.src)
    ranges = classification(args.classification)

    table = symbols(args.elf)
    bounds = {name: address for address, _, _, _, name in table}
    # splat's section-boundary symbols; data inside the same output section is excluded.
    text = [
        (bounds[name], bounds[name.replace("_START", "_END")])
        for name in bounds
        if name.endswith("_TEXT_START") and name.replace("_START", "_END") in bounds
    ]
    classes = {}
    for address, _size, kind, _section, name in table:
        if kind == "FUNC":
            if name in nonmatching:
                classes[name] = "nonmatching"
            elif name in asm_names:
                classes[name] = next((k for s, e, k, _ in ranges if s <= address < e), "asm")
    totals: dict[str, list[int]] = {}
    listing = []
    for address, size, kind, _section, name in table:
        if kind != "FUNC" or name.startswith("__maspsx_include_asm_hack"):
            continue
        if not any(s <= address < e for s, e in text):
            continue
        if address % 4 or size % 4:
            raise SystemExit(f"unaligned MIPS function range: {name} at {address:08x}, size {size}")
        cls = classes.get(name, "c")
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
        objects = [(s, e, "included") for s, e in original_objects(table, included_names, asset_names, ranges)]
        objects += assembly_data(table, files, classes)
        loaded = loaded_ranges(args.elf)
        sections = [
            s for s in map_sections(args.map)
            if not BSS_SECTION.match(s[0]) or any(lo <= s[1] and s[1] + s[2] <= hi for lo, hi in loaded)
        ]
        data = data_coverage(sections, objects, ranges, Path.cwd())
        report["data_bytes"] = sum(data.values())
        report["data_classes"] = dict(sorted(data.items()))
        report["remaining_data_placeholder_bytes"] = data.get("placeholder", 0)
        # Data that unrecovered or nonmatching assembly carries: the compiler
        # emits it once the function is C.
        report["remaining_data_asm_bytes"] = data.get("asm", 0) + data.get("nonmatching", 0)
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
