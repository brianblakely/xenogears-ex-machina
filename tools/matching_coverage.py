#!/usr/bin/env python3
"""Report source coverage of one linked target, separately from binary matching.

Every function symbol in a .text input section of the linked ELF counts in one
class:

* ``c``            cc1 emitted it (``.ent NAME``) in its decomp/src unit
* ``nonmatching``  assembly a C unit includes, named as the INCLUDE_ASM fallback
                   of a reviewed ``#ifdef NON_MATCHING`` candidate (found by the
                   only source scan; does not satisfy the exit)
* ``sdk``          other assembly inside a range classified as PsyQ SDK code
* ``handwritten``  other assembly inside a range classified as original
                   hand-written asm, or in an authored .s unit under decomp/src
* ``asm``          other assembly (remaining work)

The classification file lists ``START END CLASS NOTE...`` lines (hex VRAM,
END exclusive). This tool never reads or asserts binary agreement; run the
exact comparison separately.

Every loaded data byte counts in one class: each .rodata/.data/.sdata input
section of the GNU ld map, and the .bss/.sbss that lies in a loaded (PROGBITS)
output section, where an image holds its uninitialized variables as zeros.
NOLOAD .bss is not in the image and not counted.

* ``c``            emitted by cc1 in a data section of its decomp/src unit
* ``bss``          the same in .bss/.sbss (uninitialized, zero in the file)
* ``included``     original bytes a C unit links with INCLUDE_RODATA (a .s file)
                   or INCLUDE_ORIGINAL/INCLUDE_ASSET (``.incbin`` of the image)
* ``nonmatching``/``asm``/``sdk``/``handwritten``  the other bytes an
                   INCLUDE_ASM'd file puts in a C unit (splat moves rodata only
                   one function uses into its file), under that function's class
* ``handwritten``  an authored assembly unit under decomp/src
* ``sdk``/``asset``/``handwritten``  inside a classified range, which takes
                   precedence (``asset``: user-supplied game data/bytecode)
* ``placeholder``  generated data or loaded .bss from the original image
                   (remaining work)

A C unit's bytes are attributed by where GAS put them, not by source
spellings. ``make coverage`` compiles each C unit again with a label line at
both ends of the text of every asm statement (``--mark-asm`` on the
preprocessed unit, so macros, token pasting and included files are already
expanded) and assembles it with each label recording its position in each of
.text, .rodata, .data, .sdata, .sbss and .bss and the number of GAS macro
expansions so far (``--mark-gas``). The report fails unless, without the
labels, the unit's cc1 output, its GAS input (maspsx numbers its division-check
labels by input line) and its sections and relocations are exactly the built
object's, and the recorded positions tile every section. The bytes between a
statement's two labels, in every section, are that statement's, whatever its
text includes, incbins, expands or redefines; every other byte was assembled
from cc1's own lines, which may expand no GAS macro. A statement is classified
by its exact text in the cc1 output, which must be one of include_asm.h's:

* INCLUDE_ASM: the ``.include`` inside ``__maspsx_include_asm_hack_NAME``; its
  functions are assembly, its other bytes take the class of the function NAME
  (``nonmatching`` or ``asm`` where the file defines none, as for the
  resident's SDK data tag in .text)
* INCLUDE_RODATA: its functions are assembly, its other bytes ``included``
* INCLUDE_ASSET/INCLUDE_ORIGINAL: ``included`` (or a range's class) outside
  .text
* the macro.inc include, which emits nothing

or any other asm statement written inside a compiled function, which may emit
only code inside that function (the PsyQ GTE macros, ``break``) with no GAS
macro and no ``.include``/``.incbin`` in its text; that code counts with the
function, so only review keeps it to the original-style macros
(docs/matching.md). The report fails on every other byte instead of counting
it: a function in a C unit that cc1 did not emit and no INCLUDE_ASM/
INCLUDE_RODATA file defines, a compiled function holding a statement's other
bytes, inline asm data, original bytes in .text, an input section it does not
track. The .text bytes outside every function (padding and data words of
included files, data a unit places in .text) are attributed the same way but
not counted. Bytes outside every input section (alignment gaps, a packer's
zero tail) are not counted.

Instruction counts are static MIPS words (four bytes each) in the same ELF
function ranges as the byte totals, including nops and branch delay slots.
Paths in the map are relative to the working directory, the repository root.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from dataclasses import dataclass
from itertools import pairwise
from pathlib import Path

# Reviewed NON_MATCHING candidates (shared with nonmatching_score.py): each
# INCLUDE_ASM in the fallback of an `#ifdef NON_MATCHING ... #else ... #endif`.
INCLUDE_ASM = re.compile(r"INCLUDE_ASM\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*\)")
NON_MATCHING = re.compile(r"#ifdef\s+NON_MATCHING(.*?)#else(.*?)#endif", re.DOTALL)
COMMENT_OR_LITERAL = re.compile(
    r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'|/\*.*?\*/|//[^\n]*", re.S
)

# The sections a C unit's bytes are attributed in, with each one's key in the
# position records.
TRACKED = {
    ".text": "text", ".rodata": "rodata", ".data": "data",
    ".sdata": "sdata", ".sbss": "sbss", ".bss": "bss",
}
DATA_SECTION = re.compile(r"\.(rodata|data|sdata|sbss|bss)\b")
BSS_SECTION = re.compile(r"\.s?bss\b")

# The coverage build's label lines. maspsx passes them through and its
# lookahead skips them; in an include-asm hack function it keeps only lines
# marked `# maspsx-keep`.
MARK = re.compile(r"^\s*Lcov([be])_(\d+):(?: # maspsx-keep)?\s*$")
# maspsx names its division-check labels after the input line number.
MASPSX_LINE_LABEL = re.compile(r"\.L_(NOT_DIV_BY_ZERO|DIV_BY_POSITIVE_SIGN)_(\d+)")
C_TOKEN = re.compile(
    r"""(?P<space>^[ \t]*\#[^\n]*|\s+)
      | (?P<string>L?"(?:\\.|[^"\\\n])*")
      | (?P<char>L?'(?:\\.|[^'\\\n])*')
      | (?P<name>[A-Za-z_$][\w$]*)
      | (?P<punct>\.?\d(?:[eEpP][+-]|[\w.])*|->|\+\+|--|<<=|>>=|\.\.\.
                  |[-+*/%&|^<>=!]=|<<|>>|&&|\|\||.)""",
    re.M | re.S | re.X,
)
ASM_KEYWORDS = {"asm", "__asm", "__asm__"}
# An asm keyword starts a statement after one of these tokens, after the `)` of
# a condition, or when qualifiers (`volatile`, `const`) follow it; anywhere else
# it gives a declarator's assembler name (`register int v asm("$14")`).
STATEMENT_AFTER = {"", ";", "{", "}", ":", "else", "do", "__extension__"}
CONDITIONS = {"if", "while", "for", "switch"}
# The exact text of include_asm.h's statements in the cc1 output (each line
# stripped).
TEMPLATES = {
    "asm": re.compile(
        r"\.text # maspsx-keep\n\.align\s2 # maspsx-keep\n\.set noreorder # maspsx-keep\n"
        r"\.set noat # maspsx-keep\n\.include \"(?P<path>[^\"\n]+)\" # maspsx-keep\n"
        r"\.set reorder # maspsx-keep\n\.set at # maspsx-keep\n"
    ),
    "rodata": re.compile(r"\.section \.rodata\n\.include \"[^\"\n]+\"\n\.section \.text"),
    "asset": re.compile(
        r"\.section \S+\n\.align 2\n\.globl (?P<name>\w+)\n(?P=name):\n"
        r"\.incbin \"[^\"\n]+\", \w+ - \w+, (?P<size>\w+)\n\.size (?P=name), (?P=size)\n"
        r"\.previous"
    ),
    "macros": re.compile(r"\.include \"macro\.inc\"\n"),
}
FILE_DIRECTIVE = re.compile(r"\.(include|incbin)\b", re.I)

SHT_PROGBITS, SHT_SYMTAB, SHT_NOBITS, SHT_REL = 1, 2, 8, 9
SHF_ALLOC = 2
STT_FUNC, STT_SECTION = 2, 3


def mark_asm(text: str) -> str:
    """Preprocessed C with a label line at both ends of each asm statement's text."""
    tokens = [match for match in C_TOKEN.finditer(text) if match.lastgroup != "space"]
    opener, stack = {}, []  # each `)` token's `(`
    for index, token in enumerate(tokens):
        if token.group() == "(":
            stack.append(index)
        elif token.group() == ")" and stack:
            opener[index] = stack.pop()
    out, last, number, i = [], 0, 0, 0
    while i < len(tokens):
        if tokens[i].lastgroup != "name" or tokens[i].group() not in ASM_KEYWORDS:
            i += 1
            continue
        j = i + 1
        while j < len(tokens) and tokens[j].lastgroup == "name":
            j += 1
        k = j + 1
        while k < len(tokens) and tokens[k].lastgroup == "string":
            k += 1
        if j == len(tokens) or tokens[j].group() != "(" or k == j + 1:
            i += 1
            continue
        before = tokens[i - 1].group() if i else ""
        if before == ")" and opener.get(i - 1):
            statement = tokens[opener[i - 1] - 1].group() in CONDITIONS
        else:
            statement = before in STATEMENT_AFTER
        if statement or j > i + 1:
            first, final = tokens[j + 1], tokens[k - 1]
            keep = " # maspsx-keep" if "# maspsx-keep" in text[first.start():final.end()] else ""
            out += [text[last:first.start()], f'"Lcovb_{number}:{keep}\\n\\t" ',
                    text[first.start():final.end()], f' "\\nLcove_{number}:{keep}"']
            last, number = final.end(), number + 1
        i = k
    return "".join(out) + text[last:]


def mark_gas(text: str) -> str:
    """GAS input with each label line replaced by a record of every tracked
    section's position and the number of GAS macro expansions so far, as
    symbols no other line may define (`.equiv`)."""
    lines = ["\t.macro __cov_mark name"]
    for section, key in TRACKED.items():
        lines += [f"\t.pushsection {section}", f"\t.equiv \\name\\()_{key}, .", "\t.popsection"]
    lines += ["\t.equiv \\name\\()_macros, \\@", "\t.endm", "\t__cov_mark __cov_start"]
    seen: dict[str, int] = {}
    for line in text.splitlines():
        match = MARK.match(line)
        if match:
            tag = f"{match.group(1)}_{match.group(2)}"
            seen[tag] = seen.get(tag, -1) + 1
            line = f"\t__cov_mark __cov_{tag}_{seen[tag]}"
        lines.append(line)
    return "\n".join(lines + ["\t__cov_mark __cov_end", ""])


@dataclass
class Section:
    name: str
    type: int
    flags: int
    address: int
    size: int
    data: bytes
    info: int


@dataclass
class Symbol:
    name: str
    value: int
    size: int
    kind: int
    section: int


def read_elf(path: Path) -> tuple[list[Section], list[Symbol]]:
    """The sections and the symbol table of a 32-bit little-endian ELF file."""
    data = path.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise SystemExit(f"{path}: not a 32-bit little-endian ELF file")
    offset = struct.unpack_from("<I", data, 0x20)[0]
    size, count, names = struct.unpack_from("<HHH", data, 0x2E)
    headers = [struct.unpack_from("<10I", data, offset + i * size) for i in range(count)]

    def string(table: int, start: int) -> str:
        start += headers[table][4]
        return data[start:data.index(b"\0", start)].decode("latin-1")

    sections = [
        Section(string(names, h[0]), h[1], h[2], h[3], h[5],
                b"" if h[1] == SHT_NOBITS else data[h[4]:h[4] + h[5]], h[7])
        for h in headers
    ]
    symbols = [
        Symbol(string(h[6], name), value, size, info & 0xF, index)
        for h in headers if h[1] == SHT_SYMTAB
        for name, value, size, info, _other, index
        in struct.iter_unpack("<IIIBBH", data[h[4]:h[4] + h[5]])
    ]
    return sections, symbols


def relocations(
    sections: list[Section], symbols: list[Symbol], target: int
) -> list[tuple[int, int, str]]:
    """(offset, type, symbol) of each relocation of section index `target`; a
    section symbol by its section's name."""
    result = []
    for section in sections:
        if section.type == SHT_REL and section.info == target:
            for offset, info in struct.iter_unpack("<II", section.data):
                symbol = symbols[info >> 8]
                name = sections[symbol.section].name if symbol.kind == STT_SECTION else symbol.name
                result.append((offset, info & 0xFF, name))
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
    """(section, address, size, object) of every nonempty input section the
    link placed (the discarded ones listed first are skipped)."""
    text = path.read_text()
    text = text.split("\nLinker script and memory map\n", 1)[-1]
    result, pending = [], None
    for line in text.splitlines():
        parts = line.split()
        if line.startswith(" .") and len(parts) == 1:
            pending = parts[0]  # GNU ld puts a long section name on a line of its own
            continue
        if pending and len(parts) == 3:
            parts = [pending] + parts
        elif not line.startswith(" ."):
            parts = []
        pending = None
        if len(parts) != 4:
            continue
        name, address, size, obj = parts
        if address.startswith("0x") and size.startswith("0x") and int(size, 16):
            result.append((name, int(address, 16), int(size, 16), obj))
    return result


def strip_comments(text: str) -> str:
    """C text without its comments (each becomes a space or its line breaks)."""

    def blank(match: re.Match[str]) -> str:
        found = match.group(0)
        return found if found[0] in "\"'" else "\n" * found.count("\n") or " "

    return COMMENT_OR_LITERAL.sub(blank, text)


def nonmatching_names(roots: list[Path]) -> set[str]:
    """The functions linked as the fallback of a reviewed NON_MATCHING candidate
    in the C units and headers under roots. Only `nonmatching` rather than `asm`
    depends on this scan."""
    names: set[str] = set()
    for root in roots:
        for source in sorted(root.rglob("*.[ch]")):
            for _block, fallback in NON_MATCHING.findall(strip_comments(source.read_text())):
                names.update(INCLUDE_ASM.findall(fallback))
    return names


@dataclass
class Asm:
    """One occurrence of an asm statement's text in a C unit's cc1 output."""

    lines: list[str]
    function: str | None  # the compiled function it is written in
    kind: str = "other"  # or a TEMPLATES key
    name: str = ""  # INCLUDE_ASM's function
    macros: int = 0  # GAS macro expansions inside it


@dataclass
class Unit:
    """A C unit's attribution: per tracked section its (start, end, owner)
    spans, owner None for cc1's own lines, and the functions cc1 emitted."""

    spans: dict[str, list[tuple[int, int, Asm | None]]]
    compiled: set[str]


def renumber(lines: list[str]) -> list[str]:
    """GAS input lines with maspsx's line-numbered labels numbered in order."""
    numbers: dict[str, str] = {}

    def label(match: re.Match[str]) -> str:
        number = numbers.setdefault(match.group(2), str(len(numbers)))
        return f".L_{match.group(1)}_#{number}"

    return [MASPSX_LINE_LABEL.sub(label, line) for line in lines]


def asm_statements(path: str, text: str) -> tuple[list[Asm], set[str], list[tuple[str, int]]]:
    """The asm statement texts of a marked cc1 output in order, the functions cc1
    emitted outside them, and the labels in order."""
    statements, compiled, labels = [], set(), []
    current, function, opened = None, None, False
    for line in text.splitlines():
        match = MARK.match(line)
        if opened and not (match and match.group(1) == "b"):
            raise SystemExit(f"{path}: asm text without labels follows #APP")
        opened = line.strip() == "#APP"
        if match:
            kind, number = match.group(1), int(match.group(2))
            labels.append((kind, number))
            if kind == "b" and current is None:
                current = (number, [])
            elif kind == "e" and current is not None and current[0] == number:
                statements.append(Asm(current[1], function))
                current = None
            else:
                raise SystemExit(f"{path}: unbalanced asm statement labels at Lcov{kind}_{number}")
        elif current is not None:
            current[1].append(line)
        elif entry := re.match(r"\s*\.ent\s+(\S+)", line):
            function = entry.group(1)
            compiled.add(function)
        elif re.match(r"\s*\.end\s", line):
            function = None
    if current is not None:
        raise SystemExit(f"{path}: unbalanced asm statement labels at the end")
    return statements, compiled, labels


def classify(statement: Asm) -> None:
    text = "\n".join(line.strip() for line in statement.lines)
    for kind, template in TEMPLATES.items():
        match = template.fullmatch(text)
        if match:
            if kind == "asm":
                path = Path(match.group("path"))
                hack = f"__maspsx_include_asm_hack_{path.stem}"
                if path.suffix != ".s" or statement.function != hack:
                    return
                statement.name = path.stem
            statement.kind = kind
            return


def coverage_build(obj: str) -> Unit:
    """Attribute a C unit's object by its coverage build (decomp/Makefile)."""
    side = obj[:-2] + ".cov.o"
    try:
        cc1, gas, marked_cc1, marked_gas = (
            Path(path).read_text(encoding="latin-1")
            for path in (obj + ".cc1.s", obj + ".s", side + ".cc1.s", side + ".s")
        )
    except FileNotFoundError as error:
        raise SystemExit(
            f"{obj}: no coverage build ({error.filename}); `make coverage` builds it"
        ) from None
    if [line for line in marked_cc1.splitlines() if not MARK.match(line)] != cc1.splitlines():
        raise SystemExit(f"{side}.cc1.s: without its labels, not the unit's cc1 output {obj}.cc1.s")
    unmarked = [line for line in marked_gas.splitlines() if not MARK.match(line)]
    if renumber(unmarked) != renumber(gas.splitlines()):
        raise SystemExit(f"{side}.s: without its labels, not the unit's GAS input {obj}.s")
    statements, compiled, labels = asm_statements(side + ".cc1.s", marked_cc1)
    matches = [MARK.match(line) for line in marked_gas.splitlines()]
    if labels != [(match.group(1), int(match.group(2))) for match in matches if match]:
        raise SystemExit(f"{side}.s: its labels are not those of {side}.cc1.s")

    built = read_elf(Path(obj))
    sections, symbols = read_elf(Path(side))
    sizes = {}
    for name in TRACKED:
        found = []
        for table, table_symbols in (built, (sections, symbols)):
            index = next((i for i, s in enumerate(table) if s.name == name), None)
            found.append((0, b"", []) if index is None else (
                table[index].size, table[index].data, relocations(table, table_symbols, index)
            ))
        if found[0] != found[1]:
            raise SystemExit(f"{side}: its {name} differs from the built object's")
        sizes[name] = found[0][0]

    seen: dict[str, int] = {}
    marks = ["__cov_start"]
    for kind, number in labels:
        tag = f"{kind}_{number}"
        seen[tag] = seen.get(tag, -1) + 1
        marks.append(f"__cov_{tag}_{seen[tag]}")
    marks.append("__cov_end")
    values = {symbol.name: symbol for symbol in symbols if symbol.name.startswith("__cov_")}

    def record(mark: str, key: str, section: str | None) -> int:
        symbol = values.get(f"{mark}_{key}")
        if symbol is None or section is not None and sections[symbol.section].name != section:
            raise SystemExit(f"{side}: no position record {mark}_{key}")
        return symbol.value

    macros = [record(mark, "macros", None) for mark in marks]
    spans: dict[str, list[tuple[int, int, Asm | None]]] = {}
    for section, key in TRACKED.items():
        positions = [record(mark, key, section) for mark in marks]
        if positions[0] or positions[-1] != sizes[section] or positions != sorted(positions):
            raise SystemExit(f"{side}: the position records do not tile {section}")
        spans[section] = [
            (start, end, statements[(i - 1) // 2] if i % 2 else None)
            for i, (start, end) in enumerate(pairwise(positions)) if end > start
        ]
    for i, (before, after) in enumerate(pairwise(macros)):
        if i % 2:
            statements[(i - 1) // 2].macros = after - before - 1
        elif after - before - 1:
            raise SystemExit(f"{obj}: cc1's lines expanded a GAS macro (before {marks[i + 1]})")

    sections_of: dict[int, set[str]] = {}
    for name, rows in spans.items():
        for _start, _end, owner in rows:
            if owner is not None:
                sections_of.setdefault(id(owner), set()).add(name)
    for statement in statements:
        classify(statement)
        emitted = sections_of.get(id(statement), set())
        text = " / ".join(statement.lines)[:120]
        where = f"{obj}: asm statement in {statement.function or 'file scope'} ({text})"
        if statement.kind == "macros" and emitted:
            raise SystemExit(f"{where}: the macro.inc include emits bytes")
        if statement.kind == "asset" and ".text" in emitted:
            raise SystemExit(
                f"{where}: INCLUDE_ASSET/INCLUDE_ORIGINAL links original bytes into .text"
            )
        if statement.kind == "other" and emitted:
            if statement.function is None or emitted != {".text"}:
                raise SystemExit(f"{where}: inline asm emits {', '.join(sorted(emitted))} bytes"
                                 " outside a compiled function's code")
            if statement.macros or any(FILE_DIRECTIVE.search(line) for line in statement.lines):
                raise SystemExit(f"{where}: inline asm in a compiled function expands a GAS macro"
                                 " or reads a file")
    return Unit(spans, compiled)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("elf", type=Path, nargs="?")
    parser.add_argument("--map", type=Path, help="GNU ld map of the same link")
    parser.add_argument("--src", type=Path, action="append", default=[])
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--list", choices=["c", "nonmatching", "sdk", "handwritten", "asm"])
    parser.add_argument("--mark-asm", action="store_true", help=(
        "coverage build: label each asm statement of preprocessed C (stdin to stdout)"))
    parser.add_argument("--mark-gas", action="store_true", help=(
        "coverage build: turn the labels of GAS input into position records (stdin to stdout)"))
    args = parser.parse_args()
    if args.mark_asm or args.mark_gas:
        text = sys.stdin.buffer.read().decode("latin-1")
        marked = mark_asm(text) if args.mark_asm else mark_gas(text)
        sys.stdout.buffer.write(marked.encode("latin-1"))
        return
    if args.elf is None or args.map is None:
        parser.error("the ELF and --map are required")

    nonmatching = nonmatching_names(args.src)
    ranges = classification(args.classification)

    def ranged(address: int) -> str | None:
        return next((k for s, e, k, _ in ranges if s <= address < e), None)

    elf_sections, elf_symbols = read_elf(args.elf)
    inputs = map_sections(args.map)
    units: dict[str, Unit] = {}

    def unit_of(obj: str) -> tuple[str, Unit | None]:
        """("c", attribution) for a C unit; ("handwritten", None) for an authored
        assembly unit under decomp/src; ("generated", None) otherwise."""
        if "/decomp/src/" not in obj:
            return "generated", None
        source = Path("decomp/src/" + obj.split("/decomp/src/", 1)[1]).with_suffix(".c")
        if not source.exists():
            return "handwritten", None
        if obj not in units:
            units[obj] = coverage_build(obj)
        return "c", units[obj]

    texts = [(address, address + size, obj) for name, address, size, obj in inputs
             if name == ".text"]
    totals: dict[str, list[int]] = {}
    listing = []
    functions: dict[str, list[tuple[int, int]]] = {}  # per C unit: its function ranges in .text
    owned: dict[int, dict[str, str]] = {}  # INCLUDE_ASM statement: its functions' classes
    for symbol in sorted(elf_symbols, key=lambda s: (s.value, s.name)):
        if symbol.kind != STT_FUNC or symbol.section == 0:
            continue
        text = next(((lo, obj) for lo, hi, obj in texts if lo <= symbol.value < hi), None)
        if text is None:
            continue
        address, size, name = symbol.value, symbol.size, symbol.name
        if address % 4 or size % 4:
            raise SystemExit(f"unaligned MIPS function range: {name} at {address:08x}, size {size}")
        kind, unit = unit_of(text[1])
        if unit is None:
            cls = ranged(address) or ("handwritten" if kind == "handwritten" else "asm")
        else:
            lo = address - text[0]
            hi = max(lo + size, lo + 1)
            functions.setdefault(text[1], []).append((lo, lo + size))
            owners = [owner for start, end, owner in unit.spans[".text"] if start < hi and end > lo]
            if len(owners) == 1 and owners[0] is not None and owners[0].kind in ("asm", "rodata"):
                cls = "nonmatching" if name in nonmatching else ranged(address) or "asm"
                owned.setdefault(id(owners[0]), {})[name] = cls
            elif name in unit.compiled and all(
                owner is None or owner.kind == "other" and owner.function == name
                for owner in owners
            ):
                cls = "c"
            else:
                raise SystemExit(
                    f"{name} ({address:08x}, {text[1]}): a function that cc1 did not emit and no"
                    " INCLUDE_ASM/INCLUDE_RODATA file defines, or one holding included bytes"
                )
        entry = totals.setdefault(cls, [0, 0])
        entry[0] += 1
        entry[1] += size
        if cls == args.list:
            listing.append(f"{address:08x} {size:6d} {name}")

    def statement_class(statement: Asm) -> str:
        if statement.kind != "asm":
            return "included"
        fallback = "nonmatching" if statement.name in nonmatching else "asm"
        return owned.get(id(statement), {}).get(statement.name, fallback)

    # The .text bytes outside every function: cc1's (data placed in .text) and an
    # INCLUDE_ASM/INCLUDE_RODATA file's (padding, data words) are not counted.
    for name, address, size, obj in inputs:
        kind, unit = unit_of(obj) if name == ".text" else ("", None)
        if unit is None:
            continue
        if size != sum(end - start for start, end, _owner in unit.spans[".text"]):
            raise SystemExit(f"{obj}: input section .text is not the attributed one")
        merged: list[list[int]] = []
        for lo, hi in sorted(functions.get(obj, [])):
            if merged and lo <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], hi)
            else:
                merged.append([lo, hi])
        for start, end, owner in unit.spans[".text"]:
            inside = sum(max(0, min(end, hi) - max(start, lo)) for lo, hi in merged)
            if inside < end - start and owner is not None and owner.kind not in ("asm", "rodata"):
                raise SystemExit(f"{obj}: .text bytes {address + start:08x}-{address + end:08x}"
                                 " of an asm statement lie outside every function")

    data: dict[str, int] = {}

    def add(cls: str, count: int) -> None:
        if count:
            data[cls] = data.get(cls, 0) + count

    def split(lo: int, hi: int, cls: str) -> None:
        """Count [lo, hi) as cls outside the classified ranges, which take precedence."""
        for s, e, kind, _note in sorted(ranges):
            cut_lo, cut_hi = max(lo, s), min(hi, e)
            if cut_lo < cut_hi:
                add(cls, cut_lo - lo)
                add(kind, cut_hi - cut_lo)
                lo = cut_hi
        add(cls, max(0, hi - lo))

    loaded = [(s.address, s.address + s.size) for s in elf_sections
              if s.type == SHT_PROGBITS and s.flags & SHF_ALLOC]
    for name, address, size, obj in inputs:
        if name == ".text" or not DATA_SECTION.match(name):
            if name != ".text" and unit_of(obj)[0] == "c":
                raise SystemExit(f"{obj}: input section {name} is not attributed")
            continue
        if BSS_SECTION.match(name) and not any(lo <= address and address + size <= hi
                                               for lo, hi in loaded):
            continue
        kind, unit = unit_of(obj)
        if unit is None:
            own = "handwritten" if kind == "handwritten" else "placeholder"
            split(address, address + size, own)
            continue
        rows = unit.spans.get(name)
        if rows is None or size != sum(end - start for start, end, _owner in rows):
            raise SystemExit(f"{obj}: input section {name} is not the attributed one")
        for start, end, owner in rows:
            if owner is None:
                own = "bss" if BSS_SECTION.match(name) else "c"
            else:
                own = statement_class(owner)
            split(address + start, address + end, own)

    if args.list:
        print("\n".join(listing))
        return
    text_bytes = sum(v[1] for v in totals.values())
    remaining = [sum(totals.get(cls, [0, 0])[i] for cls in ("asm", "nonmatching")) for i in (0, 1)]
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
        "data_bytes": sum(data.values()),
        "data_classes": dict(sorted(data.items())),
        "remaining_data_placeholder_bytes": data.get("placeholder", 0),
        # Data that unrecovered or nonmatching assembly carries: the compiler
        # emits it once the function is C.
        "remaining_data_asm_bytes": data.get("asm", 0) + data.get("nonmatching", 0),
    }
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
