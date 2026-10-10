#!/usr/bin/env python3
"""Rename splat's placeholder names across the repository, by scope.

    names.py inventory                                    # .local/names/*.tsv
    names.py check MAPPING                                # validate a mapping
    names.py apply MAPPING [--overrides FILE] [--dry-run]

Run from the repository root in the matching shell once every target has
linked (make -C decomp all-verify): the 26 ELFs and link maps give each
image's symbols, and each name a link takes from another image (its linker
scripts' names) is bound to the image defining it as tools/cross_image.py
binds it: by name, by view, by address, else inside an object; where images
overlap, to the one the importer takes its other names from. The resident's
two executables are one image (decomp/src/resident).

Placeholders are splat's func_/D_/jtbl_ names (each image's own symbols, and
the names a link takes for another image's address that the image itself
does not define: views, aliases and parts of objects, listed under the image
holding the address), the C and .s files named by an address or by their
image number, and parameters named argN or aN. `inventory` writes one TSV per
image and all.tsv: kind, name, address, size, binding, defining unit and
file, the images importing it and the first line of its comment.

A mapping is a TSV of `image kind old new unit confidence evidence` rows
(docs/matching.md, Names). Kinds: func, data and jtbl (a symbol), unit (a C
file: old its path, new its file name), asm (a .s file named by its
function, which INCLUDE_ASM ties to it: it follows the function), param (old
FUNCTION.argN, new the parameter's name) and prefix (an image's declared
prefix, new ending in "_"; a resident subsystem's row names its header as
unit). `check` validates the rows against the inventory and the convention;
`apply` checks, then renames in every tracked text file but prompt.md and
plan.md.

A token in a file the build reads (a C unit, its headers and included
assembly, a target's .mk, yaml, symbol files, linker fragments and
classification) maps to what each target reading the file binds it to; they
must agree. Elsewhere (docs, tools, tests) a token maps only where one image
holds the name or where it lies in a path naming the image; each other
occurrence is reported with file:line, and an overrides file
(`file line old image`; line `*` for the whole file, image `-` to keep the
token) decides them. Each renamed definition's address is written into its
comment (a function's leading one, a variable's or label's trailing one)
unless it is there, and each name a link takes from splat's lists is written
to a symbol file its split reads (the image's own; for another image's name,
the resident's for the resident's own symbols, else the importer's), so that
a fresh split names the address so. Units and .s files move with git mv,
with their per-unit .mk settings, yaml subsegments and INCLUDE_ASM paths.
Applying twice changes nothing more.

Two names a split could not keep are refused: a label splat writes inside
another symbol's generated file (alabel), which a name of its own would split
out into a file no INCLUDE_ASM includes, and an alias given its definer's
name where one unit sees both names.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cross_image  # noqa: E402
from matching_coverage import (  # noqa: E402
    SHN_ABS,
    STB_LOCAL,
    STT_FILE,
    STT_FUNC,
    STT_SECTION,
    classification,
    map_sections,
    read_elf,
)

# splat's names give an address (cross_image.ADDRESS_NAME); in text:
PLACEHOLDER = re.compile(r"\b(?:func|D|jtbl)_[0-9A-Fa-f]{8}\w*")
# Parameters named by position (m2c's argN) or by register (aN).
PARAMETER = re.compile(r"arg\d+|a\d")
ADDRESS = re.compile(r"[0-9A-Fa-f]{8}")
GAME_NAME = re.compile(r"[a-z][a-z0-9]*(?:_[a-z0-9]+)*")
LIBRARY_NAME = re.compile(r"lib[a-z0-9]+_[a-z0-9]+(?:_[a-z0-9]+)*")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
BANNED_WORDS = {"unknown", "unk", "maybe", "misc", "stuff", "helper", "helpers"}
BARE_VERBS = {"do", "handle", "process"}
LONG_NAME = 48
KINDS = ("func", "data", "jtbl", "unit", "asm", "param", "prefix")
SYMBOL_KINDS = ("func", "data", "jtbl")
OWN_BINDINGS = ("global", "local", "weak", "script")
NEVER_TOUCHED = {"prompt.md", "plan.md"}
# The build's rules: per-unit settings and the preprocessor's include paths.
MAKEFILE = Path(__file__).resolve().parents[1] / "decomp/Makefile"
SOURCE_FILES = Path("packaging/source-files.txt")
INCLUDE_MACROS = set(
    "INCLUDE_ASM INCLUDE_RODATA INCLUDE_ORIGINAL INCLUDE_ORIGINAL_UNALIGNED INCLUDE_ASSET".split()
)
C_KEYWORDS = set(
    """auto break case char const continue default do double else enum extern float for goto
    if inline int long register return short signed sizeof static struct switch typedef union
    unsigned void volatile while asm typeof __asm__ __attribute__ __inline__ __typeof__
    __extension__ __const __volatile__ __signed__""".split()
)
ATTRIBUTES = {"__attribute__", "__asm__", "asm"}
CONDITIONAL = re.compile(r"\s*#\s*(?:if|ifdef|ifndef|else|elif|endif)\b")
ASM_LABEL = re.compile(r"^\s*(?:glabel|alabel|dlabel|jlabel)\s+(\w+)|^\s*(\w+):")
# A label splat writes inside another symbol's generated file (data in text).
ASM_INNER = re.compile(r"^\s*alabel\s+(\w+)", re.M)
ASM_MACRO = re.compile(r"^\s*\.macro\s+(\w+)", re.M)
ASM_INCLUDE = re.compile(r'^\s*\.include\s+"([^"]+)"', re.M)
INCLUDE_CALL = re.compile(r'\b(INCLUDE_ASM|INCLUDE_RODATA)\(\s*"([^"]*)"\s*,\s*(\w+)\s*\)')
LD_ASSIGNMENT = re.compile(r"^\s*(?:PROVIDE(?:_HIDDEN)?\s*\(\s*)?([A-Za-z_.$][\w.$]*)\s*=(?!=)")
OBJECT = re.compile(r"^ *([^ ]*\.o)\(", re.M)
SYMBOL_LINE = re.compile(r"^\s*([A-Za-z_.$][\w.$]*)\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)\s*;(.*)$")
UNIT_SETTING = re.compile(r"\$\((\w+)_\$\(basename")
PATH_RUN = re.compile(r"[\w./-]+")


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


# --- C structure -------------------------------------------------------------

TOKEN = re.compile(
    r"(?P<comment>/\*.*?\*/|//[^\n]*)"
    r"|(?P<pp>^[ \t]*#(?:\\\n|[^\n])*)"
    r"|(?P<string>\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*')"
    r"|(?P<ident>[A-Za-z_]\w*)"
    r"|(?P<number>\.?\d(?:[eEpP][+-]|[\w.])*)"
    r"|(?P<punct>->|[^\s])",
    re.S | re.M,
)


@dataclass
class Token:
    kind: str
    text: str
    start: int
    end: int


@dataclass
class Function:
    name: int  # token index of the name
    open: int  # the parameter list's parentheses
    close: int
    body: tuple[int, int] | None  # a definition's braces
    start: int  # the declaration's first token


@dataclass
class Source:
    """A C file's tokens and top-level structure."""

    text: str
    tokens: list[Token]
    code: list[int]  # the tokens other than comments and preprocessor lines
    functions: list[Function]
    variables: list[tuple[int, int]]  # each name a top-level definition (not extern)
    # declares, and the statement's first token
    includes: list[tuple[int, list[int]]]  # INCLUDE_* invocations: macro, its arguments
    members: set[int]  # identifiers naming a structure member
    typedefs: list[int]  # the names typedefs declare
    position: dict[int, int]  # token index -> its index in code


def parse_c(text: str) -> Source:
    tokens = [Token(m.lastgroup, m.group(), m.start(), m.end()) for m in TOKEN.finditer(text)]
    code = [i for i, t in enumerate(tokens) if t.kind not in ("comment", "pp")]
    position = {i: k for k, i in enumerate(code)}
    match: dict[int, int] = {}
    stack: list[int] = []
    for i in code:
        if tokens[i].text in ("(", "{", "["):
            stack.append(i)
        elif tokens[i].text in (")", "}", "]") and stack:
            j = stack.pop()
            match[i], match[j] = j, i
    members: set[int] = set()
    record = [False]
    for k, i in enumerate(code):
        text_ = tokens[i].text
        if text_ == "{":
            record.append(
                any(tokens[code[j]].text in ("struct", "union") for j in range(max(0, k - 2), k))
            )
        elif text_ == "}" and len(record) > 1:
            record.pop()
        elif tokens[i].kind == "ident" and (
            record[-1] or k and tokens[code[k - 1]].text in (".", "->")
        ):
            members.add(i)

    def following(i: int) -> Token | None:
        k = position[i] + 1
        return tokens[code[k]] if k < len(code) else None

    functions: list[Function] = []
    variables: list[tuple[int, int]] = []
    includes: list[tuple[int, list[int]]] = []
    typedefs: list[int] = []
    k, start = 0, None
    while k < len(code):
        i = code[k]
        t = tokens[i]
        if start is None:
            start = i
        if t.text == "{":
            k = position[match.get(i, i)] + 1
            continue
        if t.text == ";":
            statement = code[position[start] : k]
            if statement and tokens[statement[0]].text == "typedef":
                typedefs += declarators(tokens, statement[1:], match)
            elif statement and tokens[statement[0]].text != "extern":
                variables += [(name, start) for name in declarators(tokens, statement, match)]
            k, start = k + 1, None
            continue
        nxt = following(i)
        if t.kind == "ident" and nxt is not None and nxt.text == "(" and code[k + 1] in match:
            open_ = code[k + 1]
            close = match[open_]
            if t.text in INCLUDE_MACROS:
                includes.append((i, code[k + 2 : position[close]]))
            elif t.text not in C_KEYWORDS:
                body = definition_body(tokens, code, position, match, open_, close)
                if body is not None:
                    functions.append(Function(i, open_, close, body, start))
                    k, start = position[body[1]] + 1, None
                    continue
                after = following(close)
                if after is not None and after.text in (";", ","):
                    functions.append(Function(i, open_, close, None, start))
            k = position[close] + 1
            continue
        k += 1
    return Source(text, tokens, code, functions, variables, includes, members, typedefs, position)


def definition_body(
    tokens: list[Token],
    code: list[int],
    position: dict[int, int],
    match: dict[int, int],
    open_: int,
    close: int,
) -> tuple[int, int] | None:
    """The braces of a function definition whose parameter list is open_..close."""
    k = position[close] + 1
    if k >= len(code):
        return None
    if tokens[code[k]].text == "{":
        return (code[k], match[code[k]]) if code[k] in match else None
    inner = [tokens[j] for j in code[position[open_] + 1 : position[close]]]
    if (
        tokens[code[k]].kind != "ident"
        or not inner
        or any(t.kind != "ident" and t.text != "," for t in inner)
    ):
        return None  # only a K&R identifier list is followed by its declarations
    while k < len(code):
        text = tokens[code[k]].text
        if text == "{":
            return (code[k], match[code[k]]) if code[k] in match else None
        if text in ("(", ")", "=", "}"):
            return None
        k += 1
    return None


def declarators(tokens: list[Token], statement: list[int], match: dict[int, int]) -> list[int]:
    """The names a top-level declaration statement declares, unless it
    declares a function."""
    names: list[int] = []
    skip: set[int] = set()
    for n, i in enumerate(statement):
        if i in skip:
            continue
        t = tokens[i]
        nxt = tokens[statement[n + 1]].text if n + 1 < len(statement) else ";"
        if t.text == "{" and i in match or t.text in ATTRIBUTES and nxt == "(":
            group = i if t.text == "{" else statement[n + 1]
            skip |= {j for j in statement if i < j <= match.get(group, group)}
            continue
        if t.kind != "ident" or t.text in C_KEYWORDS:
            continue
        previous = tokens[statement[n - 1]].text if n else ""
        if nxt == "(":
            after = tokens[statement[n + 2]].text if n + 2 < len(statement) else ""
            if after != "*":
                return []  # a function's declaration
            continue
        if nxt in ("=", ",", ";", "[") or nxt == ")" and previous == "*":
            names.append(i)
            if nxt == "=":  # its initializer, up to the next declarator
                depth = 0
                for j in statement[n + 1 :]:
                    text = tokens[j].text
                    depth += 1 if text in ("(", "{", "[") else -1 if text in (")", "}", "]") else 0
                    if text == "," and depth == 0:
                        break
                    skip.add(j)
    return names


def parameters(source: Source, function: Function) -> list[int | None]:
    """Each parameter's name token in a function's parameter list (for an
    unnamed one, its last identifier, which names a type)."""
    tokens = source.tokens
    params: list[list[int]] = [[]]
    depth = 0
    position = source.position
    for i in source.code[position[function.open] + 1 : position[function.close]]:
        text = tokens[i].text
        if text == "," and depth == 0:
            params.append([])
            continue
        depth += 1 if text in ("(", "[") else -1 if text in (")", "]") else 0
        params[-1].append(i)
    names: list[int | None] = []
    for param in params:
        texts = [tokens[i].text for i in param]
        pointer = next(
            (
                param[n + 2]
                for n in range(len(param) - 2)
                if texts[n] == "(" and texts[n + 1] == "*" and tokens[param[n + 2]].kind == "ident"
            ),
            None,
        )
        idents = [
            i for i in param if tokens[i].kind == "ident" and tokens[i].text not in C_KEYWORDS
        ]
        names.append(pointer if pointer is not None else idents[-1] if idents else None)
    return names


def leading_comment(source: Source, start: int) -> int | None:
    """The first comment of the run directly above the declaration starting
    at token `start` (no blank line between; preprocessor conditionals between
    them are skipped)."""
    tokens, text = source.tokens, source.text

    def alone(t: Token) -> bool:  # nothing before it on its line
        return not text[text.rfind("\n", 0, t.start) + 1 : t.start].strip()

    i = start - 1
    while i >= 0 and tokens[i].kind == "pp" and CONDITIONAL.match(tokens[i].text):
        i -= 1
    if (
        i < 0
        or tokens[i].kind != "comment"
        or not alone(tokens[i])
        or text.count("\n", tokens[i].end, tokens[i + 1].start) > 1
    ):
        return None
    while (
        i > 0
        and tokens[i - 1].kind == "comment"
        and alone(tokens[i - 1])
        and text.count("\n", tokens[i - 1].end, tokens[i].start) == 1
    ):
        i -= 1
    return i


def comment_line(comment: str) -> str:
    """The first line of a comment, without its markers."""
    for line in comment.splitlines():
        line = re.sub(r"^\s*(?:/\*+|//+|\*+(?!/)|#+)", "", line)
        line = re.sub(r"\*+/\s*$", "", line).strip()
        if line:
            return line
    return ""


@dataclass
class Site:
    """Where a name is defined, for its address comment."""

    name: str
    leading: bool  # a function's leading comment, else the trailing one on the name's line
    position: int  # the declaration's start (leading) or the name (trailing)
    comment: tuple[int, int] | None  # the existing comment's span
    marker: str  # the comment syntax: "c", "asm" (#) or "ld" (/* */ only)
    note: tuple[int, int] | None = None  # the comment describing it, where not `comment`


def line_span(text: str, position: int) -> tuple[int, int]:
    start = text.rfind("\n", 0, position) + 1
    end = text.find("\n", position)
    return start, len(text) if end < 0 else end


def c_sites(source: Source) -> list[Site]:
    tokens, sites = source.tokens, []

    def trailing(i: int, start: int | None = None) -> Site:
        _start, end = line_span(source.text, tokens[i].start)
        comment, j = None, i + 1
        while j < len(tokens) and tokens[j].start < end:
            if tokens[j].kind == "comment":
                comment = (tokens[j].start, tokens[j].end)
                break
            j += 1
        first = leading_comment(source, start) if start is not None and comment is None else None
        note = (tokens[first].start, tokens[first].end) if first is not None else None
        return Site(tokens[i].text, False, tokens[i].start, comment, "c", note)

    def leading(name: str, start: int) -> Site:
        first = leading_comment(source, start)
        comment = (tokens[first].start, tokens[first].end) if first is not None else None
        return Site(name, True, tokens[start].start, comment, "c")

    for f in source.functions:
        if f.body is not None:
            sites.append(leading(tokens[f.name].text, f.start))
    for macro, args in source.includes:
        names = [i for i in args if tokens[i].kind == "ident"]
        if not names:
            continue
        if tokens[macro].text == "INCLUDE_ASM":
            sites.append(leading(tokens[names[-1]].text, macro))
        else:
            sites.append(
                trailing(names[-1] if tokens[macro].text == "INCLUDE_RODATA" else names[0])
            )
    sites += [trailing(i, start) for i, start in source.variables]
    return sorted(sites, key=lambda s: s.position)


def asm_sites(text: str, macros: set[str]) -> list[Site]:
    """Labels an authored .s file defines: glabel and the like, `name:` and
    the arguments of a macro of its own (`ot_link NAME, 7`)."""
    sites, offset, block = [], 0, None  # block: the first line of the comments above
    for line in text.splitlines(keepends=True):
        code = line.split("#", 1)[0]
        if not code.strip():
            if line.strip():
                block = block or (offset, offset + len(line.rstrip("\n")))
            else:
                block = None
            offset += len(line)
            continue
        names = []
        label = ASM_LABEL.match(code)
        if label:
            names = [label.group(1) or label.group(2)]
        else:
            words = code.split(None, 1)
            if len(words) == 2 and words[0] in macros:
                names = [a.strip() for a in words[1].split(",") if IDENTIFIER.fullmatch(a.strip())]
        hash_ = line.find("#")
        comment = (offset + hash_, offset + len(line.rstrip("\n"))) if hash_ >= 0 else None
        for name in names:
            sites.append(Site(name, False, offset + code.index(name), comment, "asm", block))
        offset, block = offset + len(line), None
    return sites


def ld_sites(text: str) -> list[Site]:
    sites, offset = [], 0
    for line in text.splitlines(keepends=True):
        found = LD_ASSIGNMENT.match(line)
        if found and not line.lstrip().startswith(("/*", "*")):
            opening = line.find("/*", found.end())
            closing = line.find("*/", opening)
            comment = (
                (offset + opening, offset + closing + 2) if opening >= 0 and closing >= 0 else None
            )
            sites.append(Site(found.group(1), False, offset + found.start(1), comment, "ld"))
        offset += len(line)
    return sites


def address_edit(
    text: str, site: Site, addresses: list[str], renamed: tuple[str, ...] = ()
) -> tuple[int, int, str] | None:
    """The edit writing addresses into a site's comment, or None where its
    comment (or, for a trailing one, its line) holds them already, outside
    the names being renamed."""
    if site.leading:
        region = text[site.comment[0] : site.position] if site.comment else ""
    else:
        region = text[slice(*line_span(text, site.position))]
    for name in renamed:
        region = re.sub(rf"\b{re.escape(name)}\b", "", region)
    missing = [a for a in addresses if a.lower() not in region.lower()]
    if not missing:
        return None
    words = " ".join(missing)
    if site.comment:
        start = site.comment[0]
        opener = re.match(r"/\*+|//+|#+", text[start:])
        at = start + opener.end()
        rest = text[at : site.comment[1]]
        if rest.startswith("\n") or rest.strip() in ("", "*/"):
            return at, at, f" {words}"
        if rest.startswith(" "):
            at += 1
        return at, at, f"{words}: "
    if site.leading:
        start, _end = line_span(text, site.position)
        indent = re.match(r"[ \t]*", text[start:]).group()
        return start, start, f"{indent}/* {words} */\n"
    _start, end = line_span(text, site.position)
    while end > 0 and text[end - 1] in " \t\r":
        end -= 1
    return end, end, f"  # {words}" if site.marker == "asm" else f" /* {words} */"


# --- the repository ----------------------------------------------------------


@dataclass
class Entry:
    image: str
    kind: str
    name: str
    address: int | None
    size: int | None = None
    binding: str = ""
    unit: str = ""
    file: str = ""
    importers: set[str] = field(default_factory=set)
    comment: str = ""


@dataclass
class Repository:
    targets: list[cross_image.Target]
    image: dict[str, str]  # target -> image
    images: dict[str, list[cross_image.Target]]
    symbol_files: dict[str, list[Path]]  # target -> the symbol files its split reads
    auto: dict[str, set[str]]  # target -> the names in its splat lists
    scopes: dict[Path, set[str]]  # file -> the targets whose build reads it
    deps: dict[Path, set[Path]]  # C unit -> what it includes
    sdk: dict[str, list[tuple[int, int]]]  # image -> its sdk ranges
    units: dict[str, list[Path]]  # image -> its linked units
    # (image, name) -> the generated file splat keeps the label in (alabel)
    inner: dict[tuple[str, str], Path] = field(default_factory=dict)
    entries: dict[tuple[str, str], Entry] = field(default_factory=dict)
    holding: dict[str, set[str]] = field(default_factory=dict)  # name -> images
    partners: dict[str, set[str]] = field(default_factory=dict)
    bound: dict[tuple[str, str], tuple[str, str] | None] = field(default_factory=dict)
    sources: dict[Path, Source] = field(default_factory=dict)
    sites: dict[Path, list[Site]] = field(default_factory=dict)
    by_name: dict[str, cross_image.Target] = field(default_factory=dict)

    def c(self, path: Path) -> Source:
        if path not in self.sources:
            self.sources[path] = parse_c(file_text(path))
        return self.sources[path]

    def target(self, name: str) -> cross_image.Target:
        return self.by_name[name]

    def own_file(self, image: str) -> Path:
        return self.symbol_files[self.images[image][0].name][0]


def tracked() -> list[Path]:
    """The tracked text files, but prompt.md and plan.md."""
    names = subprocess.run(
        ["git", "ls-files", "-z"], check=True, capture_output=True, text=True
    ).stdout.split("\0")
    files = []
    for name in names:
        path = Path(name)
        if not name or name in NEVER_TOUCHED or not path.is_file():
            continue
        data = path.read_bytes()
        if b"\0" in data:
            continue
        try:
            data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        files.append(path)
    return files


def yaml_list(path: Path, key: str) -> list[str]:
    """The items of a yaml option `key:` (a block list or one value)."""
    lines = path.read_text().splitlines()
    for n, line in enumerate(lines):
        if re.match(rf"\s*{key}:\s*$", line):
            items = []
            for item in lines[n + 1 :]:
                found = re.match(r"\s*-\s*(\S+)", item)
                if not found:
                    break
                items.append(found.group(1))
            return items
        found = re.match(rf"\s*{key}:\s*(\S+)", line)
        if found:
            return [found.group(1)]
    return []


def source_of(build: str, obj: str) -> Path:
    """The file an object of the link was built from."""
    relative = Path(obj).relative_to(build)
    for suffix in (".c", ".s"):
        if relative.with_suffix(suffix).exists():
            return relative.with_suffix(suffix)
    return relative.with_suffix(".s")


def include_paths() -> list[str]:
    """The -I options of decomp/Makefile's CPPFLAGS, from the repository root."""
    flags = re.search(r"^CPPFLAGS :=((?:.*\\\n)*.*)", MAKEFILE.read_text(), re.M).group(1)
    return [f"-I{path}" for path in re.findall(r"-I\$\(ROOT\)/(\S+)", flags)]


def dependencies(unit: Path, version: str, includes: list[str]) -> set[Path]:
    """What a C unit includes: its headers (psx-cpp -M) and the tracked
    assembly its INCLUDE_ASM/INCLUDE_RODATA statements and .include lines read."""
    command = [f"psx-cpp-{version}", "-M", "-undef", "-nostdinc", "-lang-c", *includes, str(unit)]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        fail(f"{unit}: {' '.join(command)}: {result.stderr.strip()}")
    found = {Path(word) for word in result.stdout.replace("\\\n", " ").split()[1:]}
    pending = [
        Path(folder) / f"{name}.s"
        for _macro, folder, name in INCLUDE_CALL.findall(unit.read_text())
        if not folder.startswith(".local")
    ]
    while pending:
        path = pending.pop()
        if path in found or not path.exists():
            continue
        found.add(path)
        pending += [Path(name) for name in ASM_INCLUDE.findall(path.read_text())]
    return found


def load() -> Repository:
    configs = sorted(Path("decomp/targets").glob("*/*.mk"))
    if not configs:
        fail("no decomp/targets/*/*.mk: run from the repository root")
    targets = [cross_image.load(path) for path in configs]
    image, images, symbol_files, auto = {}, defaultdict(list), {}, {}
    scopes: dict[Path, set[str]] = defaultdict(set)
    deps: dict[Path, set[Path]] = {}
    sdk: dict[str, list[tuple[int, int]]] = defaultdict(list)
    units: dict[str, list[Path]] = defaultdict(list)
    inner: dict[tuple[str, str], Path] = {}
    includes = include_paths()
    for path, t in zip(configs, targets, strict=True):
        values = t.values
        name = Path(values["SOURCE_DIRS"].split()[0]).name
        image[t.name] = name
        images[name].append(t)
        yamls = [Path(values["SPLAT_CONFIG"])] + [
            Path(p) for p in values.get("SPLIT_ALSO", "").split()
        ]
        symbol_files[t.name] = [Path(p) for p in yaml_list(yamls[0], "symbol_addrs_path")]
        files = {path, *yamls, *symbol_files[t.name]}
        auto[t.name] = set()
        for script in values.get("LINKER_EXTRA", "").split() + values.get("LINK_VIEWS", "").split():
            if script.endswith("_auto.txt"):
                if Path(script).exists():
                    auto[t.name] |= {
                        line.split("=")[0].strip()
                        for line in Path(script).read_text().splitlines()
                        if "=" in line
                    }
            else:
                files.add(Path(script))
        if values.get("CLASSIFICATION"):
            files.add(Path(values["CLASSIFICATION"]))
            if t.name == images[name][0].name:
                sdk[name] += [
                    (a, b)
                    for a, b, kind, _note in classification(Path(values["CLASSIFICATION"]))
                    if kind == "sdk"
                ]
        found = sorted(set(OBJECT.findall(Path(values["LINKER_SCRIPT"]).read_text())))
        for unit in (source_of(values["BUILD"], obj) for obj in found):
            if unit not in units[name]:
                units[name].append(unit)
            files.add(unit)
            if unit.suffix == ".c":
                if unit not in deps:
                    deps[unit] = dependencies(unit, values["CC_VERSION"], includes)
                files |= deps[unit]
                for _macro, folder, included in INCLUDE_CALL.findall(file_text(unit)):
                    generated = Path(folder) / f"{included}.s"
                    if folder.startswith(".local") and generated.exists():
                        for label in ASM_INNER.findall(file_text(generated)):
                            inner[(name, label)] = generated
        for file in files:
            scopes[file].add(t.name)
    repo = Repository(
        targets, image, dict(images), symbol_files, auto, dict(scopes), deps, dict(sdk), dict(units)
    )
    repo.by_name = {t.name: t for t in targets}
    repo.inner = inner
    repo.partners = {t.name: partners(repo, t) for t in targets}
    inventory(repo)
    return repo


def partners(repo: Repository, t: cross_image.Target) -> set[str]:
    """The images a target takes names from by name (each name one image's);
    none for the resident, which loads every mode overlay (MODE_TABLE)."""
    found: set[str] = set()
    if t.values.get("MODE_TABLE"):
        return found
    for name in t.imports:
        value = t.value.get(name)
        if value is not None and not t.lo <= value < t.end:
            images = {
                repo.image[d.name] for d in cross_image.definers(repo.targets, t, name, value)
            }
            if len(images) == 1:
                found |= images
    return found


def resolve(repo: Repository, t: cross_image.Target, name: str) -> tuple[str, str] | None:
    """What target t binds a name to: (image, name), or None where its link
    has no such name; ("", name) for an address outside every image and ("?",
    name) where the images holding it stay ambiguous."""
    key = (t.name, name)
    if key in repo.bound:
        return repo.bound[key]
    value = t.value.get(name)
    if value is None:
        result = None
    elif t.lo <= value < t.end:
        result = (repo.image[t.name], name)
    else:
        result = ("", name)
        holders = [d for d in repo.targets if d is not t and d.lo <= value < d.end]
        if holders:
            named = cross_image.definers(repo.targets, t, name, value)
            expression = t.imports.get(name, (None, ""))[1]
            view = cross_image.VIEW.fullmatch(expression.strip())
            base = view.group(1) if view and view.group(1) != name else None
            bases = (
                cross_image.definers(repo.targets, t, base, t.value[base])
                if base is not None and base in t.value
                else []
            )
            at = [d for d in holders if value in d.at]
            inside = [d for d in holders if any(a <= value < b for a, b in d.inputs)]
            found = {repo.image[d.name] for d in (named or bases or at or inside or holders)}
            if len(found) > 1 and len(found & repo.partners[t.name]) == 1:
                found &= repo.partners[t.name]
            result = (found.pop(), name) if len(found) == 1 else ("?", name)
    repo.bound[key] = result
    return result


def address_named(image: str, unit: Path) -> bool:
    """A file named by an address or by its image's number."""
    return bool(ADDRESS.search(unit.stem)) or bool(re.search(r"\d", image)) and image in unit.stem


def inventory(repo: Repository) -> None:
    """Every placeholder of every image (repo.entries)."""
    entries = repo.entries
    functions: dict[str, set[str]] = {}
    for t in repo.targets:
        img = repo.image[t.name]
        _sections, symbols = read_elf(Path(f"{t.values['IMAGE']}.elf"))
        functions[t.name] = {s.name for s in symbols if s.kind == STT_FUNC}
        inputs = map_sections(Path(f"{t.values['IMAGE']}.map"))
        for s in symbols:
            if (
                not s.name
                or not s.section
                or s.kind in (STT_SECTION, STT_FILE)
                or not cross_image.ADDRESS_NAME.fullmatch(s.name)
                or not t.lo <= s.value < t.end
                or (img, s.name) in entries
            ):
                continue
            if s.section == SHN_ABS:
                binding, unit = "script", str(t.imports.get(s.name, ("", ""))[0])
            else:
                binding = "local" if s.bind == STB_LOCAL else "global" if s.bind == 1 else "weak"
                unit = next(
                    (
                        str(source_of(t.values["BUILD"], obj))
                        for _section, a, size, obj in inputs
                        if a <= s.value < a + size
                    ),
                    "",
                )
            kind = (
                "jtbl"
                if s.name.startswith("jtbl_")
                else "func"
                if s.kind == STT_FUNC or s.name.startswith("func_")
                else "data"
            )
            entries[(img, s.name)] = Entry(img, kind, s.name, s.value, s.size, binding, unit, unit)
    for t in repo.targets:
        for name, (script, expression) in t.imports.items():
            key = resolve(repo, t, name)
            if (
                not key
                or key[0] in ("", "?", repo.image[t.name])
                or not cross_image.ADDRESS_NAME.fullmatch(name)
            ):
                continue
            if key not in entries:
                value = t.value[name]
                holder = next(d for d in repo.images[key[0]] if d.lo <= value < d.end)
                defined = sorted(n for n in holder.at.get(value, ()) if n in holder.names)
                view = cross_image.VIEW.fullmatch(expression.strip())
                kind = "data"
                if defined:
                    binding = f"alias:{defined[0]}"
                    kind = "func" if defined[0] in functions[holder.name] else "data"
                elif view and view.group(1) != name and view.group(2):
                    binding = f"view:{' '.join(expression.split())}"
                else:
                    binding = "member"
                listed = Path(script)  # a splat list the build turned into a script
                for d in repo.targets:
                    if listed.is_relative_to(d.values["BUILD"]) and listed.suffix == ".ld":
                        listed = listed.relative_to(d.values["BUILD"]).with_suffix(".txt")
                entries[key] = Entry(key[0], kind, name, value, None, binding, "", str(listed))
            entries[key].importers.add(repo.image[t.name])
    for img, units in repo.units.items():
        for unit in units:
            if unit.suffix == ".c" and address_named(img, unit):
                found = ADDRESS.search(unit.stem)
                entries[(img, str(unit))] = Entry(
                    img,
                    "unit",
                    str(unit),
                    int(found.group(), 16) if found else None,
                    None,
                    "file",
                    str(unit),
                    str(unit),
                )
    for path in sorted(repo.scopes):
        if (
            path.suffix == ".s"
            and PLACEHOLDER.fullmatch(path.stem)
            and path.exists()
            and not str(path).startswith(".local")
        ):
            for img in sorted({repo.image[t] for t in repo.scopes[path]}):
                entries[(img, str(path))] = Entry(
                    img,
                    "asm",
                    str(path),
                    int(ADDRESS.search(path.stem).group(), 16),
                    None,
                    f"follows:{path.stem}",
                    str(path),
                    str(path),
                )
    for key, generated in repo.inner.items():
        if key in entries:
            entries[key].binding, entries[key].file = "label", str(generated)
    describe(repo)
    for (img, name), entry in list(entries.items()):
        if entry.kind in SYMBOL_KINDS:
            repo.holding.setdefault(name, set()).add(img)
    for path in sorted(p for p in repo.scopes if p.suffix in (".c", ".h") and p.exists()):
        source = repo.c(path)
        for f in source.functions:
            fname = source.tokens[f.name].text
            names = [
                i
                for i in parameters(source, f)
                if i is not None and PARAMETER.fullmatch(source.tokens[i].text)
            ]
            if not names:
                continue
            key, _how, _candidates = bind(repo, path, fname, source.tokens[f.name].start)
            img = key[0] if key and key[0] not in ("", "?") else image_of_path(repo, path)
            function = entries.get((img, fname))
            address = next(
                (t.value[fname] for t in repo.images.get(img, []) if fname in t.value), None
            )
            for i in names:
                name = f"{fname}.{source.tokens[i].text}"
                if (img, name) not in entries or f.body is not None:
                    entries[(img, name)] = Entry(
                        img,
                        "param",
                        name,
                        address,
                        None,
                        "definition" if f.body is not None else "prototype",
                        str(path),
                        str(path),
                        set(),
                        function.comment if function else "",
                    )


def image_of_path(repo: Repository, path: Path) -> str:
    names = sorted({repo.image[t] for t in repo.scopes.get(path, ())})
    return names[0] if len(names) == 1 else "?"


def sites_of(repo: Repository, path: Path) -> list[Site]:
    if path not in repo.sites:
        sites: list[Site] = []
        if path.suffix in (".c", ".h"):
            sites = c_sites(repo.c(path))
        elif path.suffix == ".s":
            text = file_text(path)
            macros = set(ASM_MACRO.findall(text))
            for name in ASM_INCLUDE.findall(text):
                if Path(name).exists():
                    macros |= set(ASM_MACRO.findall(file_text(Path(name))))
            sites = asm_sites(text, macros)
        elif path.suffix == ".ld":
            sites = ld_sites(file_text(path))
        repo.sites[path] = sites
    return repo.sites[path]


def describe(repo: Repository) -> None:
    """Each entry's defining file and the first line of its comment."""
    for entry in repo.entries.values():
        if entry.kind in ("unit", "asm"):
            path = Path(entry.file)
            first = (
                next((t.text for t in repo.c(path).tokens if t.kind == "comment"), "")
                if path.suffix == ".c"
                else next(
                    (
                        line
                        for line in file_text(path).splitlines()
                        if line.lstrip().startswith("#")
                    ),
                    "",
                )
            )
            entry.comment = comment_line(first)
            continue
        unit = Path(entry.unit) if entry.unit else None
        candidates = [unit] if unit and unit.exists() else []
        if unit in repo.deps:
            candidates += sorted(repo.deps[unit] - {unit})
        if not candidates and entry.file and Path(entry.file).exists():
            candidates = [Path(entry.file)]
        found = False
        for path in candidates:  # the first site defines it; a later one may describe it
            site = next((s for s in sites_of(repo, path) if s.name == entry.name), None)
            if site is None:
                continue
            note = site.comment or site.note
            if not found:
                entry.file, found = str(path), True
            if note:
                entry.comment = comment_line(file_text(path)[note[0] : note[1]])
                break
        if found and not entry.comment and entry.kind == "data":
            entry.comment = declared_comment(repo, candidates, entry.name)


def declared_comment(repo: Repository, paths: list[Path], name: str) -> str:
    """The trailing comment of an extern declaration of a name in the files."""
    pattern = re.compile(rf"\bextern\b[^;{{}}]*\b{re.escape(name)}\b[^;]*;[ \t]*/\*(.*?)\*/", re.S)
    for path in paths:
        if path.suffix == ".h":
            found = pattern.search(file_text(path))
            if found:
                return comment_line(found.group(1))
    return ""


# --- binding tokens in files -------------------------------------------------


def bind(
    repo: Repository, path: Path, name: str, offset: int, overrides: dict | None = None
) -> tuple[tuple[str, str] | None, str, set[tuple[str, str]]]:
    """What a token at `offset` of a file stands for: (image, name) or None,
    how that was decided and the candidates. "scope": every target reading
    the file that binds the name agrees ("conflict" where they differ);
    "override"/"kept": the overrides file, where no target binds it or the
    images holding it stay ambiguous; "image": one image reading the file
    holds it; "path": the path it lies in names the image; "unique": one
    image holds it; else "ambiguous" or "unknown"."""
    scope = repo.scopes.get(path, set())
    keys = {resolve(repo, repo.target(t), name) for t in scope} - {None}
    holding = repo.holding.get(name, set())
    candidates = {(image, name) for image in holding}
    if len(keys) > 1:
        return None, "conflict", keys
    if keys and next(iter(keys))[0] != "?":
        key = keys.pop()
        return (key if key[0] else None), "scope", {key}
    if overrides:
        line = repo_line(path, offset)
        for where in (line, "*"):
            choice = overrides.get((str(path), where, name))
            if choice is not None:
                return (
                    (None, "kept", candidates)
                    if choice == "-"
                    else ((choice, name), "override", candidates)
                )
    if keys:  # the images holding it stay ambiguous for the targets reading it
        return None, "ambiguous", candidates
    if scope:
        here = {repo.image[t] for t in scope} & holding
        if len(here) == 1:
            return (here.pop(), name), "image", candidates
    text = file_text(path)
    run = next(
        (
            m
            for m in PATH_RUN.finditer(text, max(0, offset - 200), offset + len(name) + 200)
            if m.start() <= offset < m.end()
        ),
        None,
    )
    if run:
        named = [
            p
            for p in text[run.start() : offset].split("/")[:-1]
            if p in repo.images and p in holding
        ]
        if named:
            return (named[-1], name), "path", candidates
    if len(holding) == 1:
        return (next(iter(holding)), name), "unique", candidates
    return None, "ambiguous" if holding else "unknown", candidates


_texts: dict[Path, str] = {}


def file_text(path: Path) -> str:
    if path not in _texts:
        _texts[path] = path.read_text()
    return _texts[path]


def repo_line(path: Path, offset: int) -> int:
    return file_text(path).count("\n", 0, offset) + 1


# --- the inventory's files ---------------------------------------------------

COLUMNS = "image kind name address size binding unit file importers comment".split()


def entry_columns(e: Entry) -> list[str]:
    return [
        e.image,
        e.kind,
        e.name,
        f"{e.address:08X}" if e.address is not None else "",
        f"0x{e.size:X}" if e.size else "",
        e.binding,
        e.unit,
        e.file,
        ",".join(sorted(e.importers)),
        e.comment.replace("\t", " "),
    ]


def write_table(path: Path, entries: list[Entry]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["\t".join(COLUMNS)] + ["\t".join(entry_columns(e)) for e in entries]
    path.write_text("\n".join(lines) + "\n")


def ordered(entries) -> list[Entry]:
    return sorted(entries, key=lambda e: (e.image, KINDS.index(e.kind), e.address or 0, e.name))


# --- names the sources use (collisions) --------------------------------------


@dataclass
class Names:
    code: dict[str, set[str]]  # identifier -> the decomp files using it (not as a member)
    members: dict[str, set[str]]  # identifier -> the files using it as a structure member
    macros: set[str]
    typedefs: set[str]
    symbols: dict[str, set[tuple[str, int]]]  # every link's symbols: (image, value)
    psyq: set[str]  # the identifiers decomp/include/psyq declares


def source_names(repo: Repository, files: list[Path]) -> Names:
    code: dict[str, set[str]] = defaultdict(set)
    members: dict[str, set[str]] = defaultdict(set)
    macros: set[str] = set()
    typedefs: set[str] = set()
    psyq: set[str] = set()
    for path in files:
        if path.parts[0] != "decomp" or path.suffix == ".md":
            continue
        if path.suffix in (".c", ".h"):
            source = repo.c(path)
            for i in source.code:
                t = source.tokens[i]
                if t.kind == "ident":
                    (members if i in source.members else code)[t.text].add(str(path))
            for t in source.tokens:
                if t.kind == "pp":
                    define = re.match(r"\s*#\s*define\s+(\w+)", t.text)
                    if define:
                        macros.add(define.group(1))
                    for word in IDENTIFIER.findall(t.text):
                        code[word].add(str(path))
            typedefs |= {source.tokens[i].text for i in source.typedefs}
            if "psyq" in path.parts:
                psyq |= {
                    source.tokens[i].text for i in source.code if source.tokens[i].kind == "ident"
                }
        else:
            text = file_text(path)
            if path.suffix in (".s", ".mk", ".yaml", ".inc"):
                text = re.sub(r"#[^\n]*", "", text)
            elif path.suffix == ".ld":
                text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
            elif path.suffix == ".txt":
                text = re.sub(r"//[^\n]*|#[^\n]*", "", text)
            for word in IDENTIFIER.findall(text):
                code[word].add(str(path))
    symbols: dict[str, set[tuple[str, int]]] = defaultdict(set)
    for t in repo.targets:
        for name, value in t.value.items():
            symbols[name].add((repo.image[t.name], value))
    return Names(dict(code), dict(members), macros, typedefs, dict(symbols), psyq)


# --- the mapping and its check -----------------------------------------------


@dataclass
class Row:
    line: int
    image: str
    kind: str
    old: str
    new: str
    unit: str
    confidence: str
    evidence: str


@dataclass
class Plan:
    renames: dict[tuple[str, str], str] = field(default_factory=dict)  # symbols
    names: dict[tuple[str, str], str] = field(default_factory=dict)  # also those applied
    units: dict[Path, Path] = field(default_factory=dict)  # C units and .s files
    params: dict[tuple[str, str], str] = field(default_factory=dict)
    prefixes: dict[str, list[str]] = field(default_factory=dict)
    owner: dict[str, str] = field(default_factory=dict)  # prefix -> image
    headers: dict[str, list[str]] = field(default_factory=dict)  # header -> its prefixes
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    unnamed: list[Entry] = field(default_factory=list)
    applied: int = 0


def read_rows(path: Path, errors: list[str]) -> list[Row]:
    rows = []
    for n, line in enumerate(path.read_text().splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        columns = [c.strip() for c in line.split("\t")]
        if columns[:4] == ["image", "kind", "old", "new"]:
            continue
        if len(columns) != 7:
            errors.append(
                f"{path}:{n}: {len(columns)} columns, not the seven of image kind old"
                " new unit confidence evidence"
            )
            continue
        rows.append(Row(n, *columns))
    return rows


def owning_prefix(plan: Plan, name: str) -> str | None:
    """The longest declared prefix a name starts with: its owner's."""
    return max((p for p in plan.owner if name.startswith(p) and name != p), key=len, default=None)


def check_words(plan: Plan, where: str, name: str) -> None:
    words = name.split("_")
    banned = sorted(BANNED_WORDS & set(words))
    if banned:
        plan.errors.append(f"{where}: {name}: says nothing ({', '.join(banned)})")
    if words[-1] in BARE_VERBS:
        plan.errors.append(f"{where}: {name}: a bare '{words[-1]}' says nothing")
    if len(name) > LONG_NAME:
        plan.warnings.append(
            f"{where}: {name}: {len(name)} characters (keep under about {LONG_NAME})"
        )


def check_prefix(plan: Plan, where: str, image: str, name: str) -> None:
    owner = owning_prefix(plan, name)
    if not plan.prefixes.get(image):
        plan.errors.append(f"{where}: {image} declares no prefix (a prefix row)")
    elif owner is None or plan.owner[owner] != image:
        plan.errors.append(
            f"{where}: {name} does not start with a prefix of {image}"
            f" ({', '.join(plan.prefixes[image])})"
            + (f": {owner} is {plan.owner[owner]}'s" if owner else "")
        )


def function_identifiers(source: Source, f: Function) -> set[str]:
    end = f.body[1] if f.body else f.close
    return {
        source.tokens[i].text
        for i in source.code
        if f.open < i < end and source.tokens[i].kind == "ident" and i not in source.members
    }


def check(repo: Repository, mapping: Path, files: list[Path]) -> Plan:
    plan = Plan()
    error, warning = plan.errors.append, plan.warnings.append
    seen: set[tuple[str, str, str]] = set()
    valid = []
    for row in read_rows(mapping, plan.errors):
        where = f"{mapping}:{row.line}"
        if row.kind not in KINDS:
            error(f"{where}: kind {row.kind!r} is none of {', '.join(KINDS)}")
        elif row.image not in repo.images:
            error(f"{where}: no image {row.image!r} (one of {', '.join(sorted(repo.images))})")
        elif (row.image, row.kind, row.old) in seen and row.kind != "prefix":
            error(f"{where}: {row.image} {row.kind} {row.old} is named twice")
        elif row.kind == "prefix":
            if not re.fullmatch(r"[a-z][a-z0-9]*(?:_[a-z0-9]+)*_", row.new) or re.fullmatch(
                r"lib[a-z0-9]*_", row.new
            ):
                error(
                    f"{where}: prefix {row.new!r} is not lower snake_case ending in _"
                    " (lib<name>_ is the SDK's)"
                )
            elif plan.owner.get(row.new, row.image) != row.image:
                error(f"{where}: prefix {row.new} is also {plan.owner[row.new]}'s")
            elif row.new not in plan.prefixes.get(row.image, []):
                plan.owner[row.new] = row.image
                plan.prefixes.setdefault(row.image, []).append(row.new)
                if row.unit:
                    plan.headers.setdefault(row.unit, []).append(row.new)
        else:
            seen.add((row.image, row.kind, row.old))
            if not row.confidence or not row.evidence:
                warning(f"{where}: {row.old}: no confidence or evidence")
            valid.append(row)
    names = source_names(repo, files)
    written = {
        name
        for files_ in repo.symbol_files.values()
        for f in files_
        for name, _a, _x in symbol_lines(file_text(f))
    } | {
        site.name
        for t in repo.targets
        for script in t.values.get("LINKER_EXTRA", "").split()
        if not script.endswith("_auto.txt") and Path(script).exists()
        for site in ld_sites(file_text(Path(script)))
    }
    given: dict[str, list[tuple[Row, Entry]]] = defaultdict(list)

    def applied(row: Row) -> bool:
        """Renamed already: no file binds the old name to the image, and a
        symbol file or fragment names the new one (not yet split) or a link
        binds it there (split and linked)."""
        if any(
            bind(repo, Path(f), row.old, 0)[0] == (row.image, row.old)
            for f in names.code.get(row.old, ())
        ):
            return False
        return row.new in written or (row.image, row.new) in {
            resolve(repo, t, row.new) for t in repo.targets
        }

    for row in valid:
        where = f"{mapping}:{row.line}"
        if row.kind in SYMBOL_KINDS:
            plan.names[(row.image, row.old)] = row.new
            entry = repo.entries.get((row.image, row.old))
            if entry is None or entry.kind not in SYMBOL_KINDS:
                if applied(row):
                    plan.applied += 1
                else:
                    error(
                        f"{where}: {row.old} is no placeholder of {row.image} (names.py inventory)"
                    )
                continue
            if entry.kind != row.kind:
                error(f"{where}: {row.old} is a {entry.kind}, not a {row.kind}")
            check_symbol(repo, plan, names, where, row, entry)
            plan.renames[(row.image, row.old)] = row.new
            given[row.new].append((row, entry))
    for row in valid:
        where = f"{mapping}:{row.line}"
        if row.kind == "unit":
            check_unit(repo, plan, where, row)
        elif row.kind == "param":
            check_param(repo, plan, names, files, where, row)
    for new, uses in given.items():
        if len({(r.image, e.address) for r, e in uses}) > 1:
            error(
                f"{new} is given to {len(uses)} symbols: "
                + ", ".join(f"{r.image} {r.old}" for r, _e in uses)
            )
        row, entry = uses[0]
        where = f"{mapping}:{row.line}"
        existing = names.symbols.get(new, set())
        if existing and not existing <= {(row.image, entry.address)}:
            error(
                f"{where}: {new} is already a symbol of "
                + ", ".join(sorted({image for image, _value in existing}))
            )
        if names.code.get(new) and not existing and not applied(row):
            error(f"{where}: {new} is already a name in " + ", ".join(sorted(names.code[new])[:3]))
        if names.members.get(new):
            warning(
                f"{where}: {new} also names a structure member in "
                + ", ".join(sorted(names.members[new])[:3])
            )
    for alias, entry in (u for uses in given.values() for u in uses):
        if entry.binding.startswith("alias:"):
            check_alias(repo, plan, names, f"{mapping}:{alias.line}", alias, entry)
    for row in valid:
        if row.kind == "asm":
            check_asm(repo, plan, f"{mapping}:{row.line}", row)
    for (image, _name), entry in repo.entries.items():
        function = entry.binding[len("follows:") :]
        if entry.kind == "asm" and (image, function) in plan.renames:
            path = Path(entry.name)
            plan.units[path] = path.with_name(f"{plan.renames[(image, function)]}.s")
    for key, entry in repo.entries.items():
        if entry.kind in ("unit", "asm"):
            named = Path(entry.name) in plan.units
        else:
            named = key in plan.renames or key in plan.params
        if not named:
            plan.unnamed.append(entry)
    return plan


def check_alias(
    repo: Repository, plan: Plan, names: Names, where: str, row: Row, entry: Entry
) -> None:
    """An alias (another image's name for an address its image names) takes
    that name, unless a unit sees both, where one name would declare both."""
    definer = entry.binding[len("alias:") :]
    final = plan.names.get((row.image, definer), definer)
    alias_files, definer_files = names.code.get(row.old, set()), names.code.get(definer, set())
    meet = sorted(
        str(unit)
        for unit, included in repo.deps.items()
        if {str(unit), *map(str, included)} & alias_files
        and {str(unit), *map(str, included)} & definer_files
    )
    if row.new == final and meet:
        plan.errors.append(
            f"{where}: {row.old} and {definer} ({row.image}) meet in {meet[0]}: one name"
            " would declare both; give the alias a name of its own"
        )
    elif row.new != final and not meet:
        plan.warnings.append(
            f"{where}: {row.old} is {row.image}'s {definer}: one name per address ({final})"
            " where no unit sees both"
        )


def check_symbol(
    repo: Repository, plan: Plan, names: Names, where: str, row: Row, entry: Entry
) -> None:
    new = row.new
    if not IDENTIFIER.fullmatch(new) or new in C_KEYWORDS:
        plan.errors.append(f"{where}: {new!r} is no C identifier")
        return
    if cross_image.ADDRESS_NAME.fullmatch(new):
        plan.errors.append(f"{where}: {new} is still a placeholder")
        return
    if entry.binding == "label":
        plan.errors.append(
            f"{where}: {row.old} is a label splat keeps inside {entry.file}: naming it would"
            " split it out of that file, which no INCLUDE_ASM includes"
        )
        return
    if entry.binding == f"alias:{new}":
        return  # the name its image already gives the address
    sdk = entry.address is not None and any(
        a <= entry.address < b for a, b in repo.sdk.get(row.image, [])
    )
    if sdk:
        if LIBRARY_NAME.fullmatch(new) or new in names.psyq:
            return
        if GAME_NAME.fullmatch(new):
            plan.errors.append(
                f"{where}: {new}: an SDK member keeps its PsyQ name or takes"
                " its library's prefix (libgte_..., libcd_...)"
            )
        else:
            plan.warnings.append(
                f"{where}: {new}: a PsyQ spelling that decomp/include/psyq"
                " does not declare (the evidence should cite the routine)"
            )
        return
    if not GAME_NAME.fullmatch(new):
        plan.errors.append(f"{where}: {new} is not lower snake_case")
        return
    check_prefix(plan, where, row.image, new)
    check_words(plan, where, new)
    headers = sorted(h for h in names.code.get(row.old, ()) if h in plan.headers)
    expected = sorted({p for h in headers for p in plan.headers[h]})
    if expected and not any(new.startswith(p) for p in expected):
        plan.warnings.append(
            f"{where}: {new}: declared in {', '.join(headers)}, whose prefix is"
            f" {', '.join(expected)}"
        )


def unit_entry(repo: Repository, image: str, old: str, kind: str) -> Entry | None:
    for entry in repo.entries.values():
        if (
            entry.image == image
            and entry.kind == kind
            and old in (entry.name, Path(entry.name).name, Path(entry.name).stem)
        ):
            return entry
    return None


def check_unit(repo: Repository, plan: Plan, where: str, row: Row) -> None:
    entry = unit_entry(repo, row.image, row.old, "unit")
    stem = row.new[:-2] if row.new.endswith(".c") else row.new
    if entry is None:
        old = Path(row.old) if "/" in row.old else Path("decomp/src") / row.image / row.old
        if not old.exists() and old.with_name(f"{stem}.c").exists():
            plan.applied += 1  # already moved
        else:
            plan.errors.append(
                f"{where}: {row.old} is no unit of {row.image} named by an"
                " address or its image number"
            )
        return
    old = Path(entry.name)
    new = old.with_name(f"{stem}.c")
    if not GAME_NAME.fullmatch(stem):
        plan.errors.append(f"{where}: {row.new} is not a lower snake_case file name")
        return
    check_prefix(plan, where, row.image, stem)
    check_words(plan, where, stem)
    stems = {u.stem for units in repo.units.values() for u in units}
    if new.exists() or stem in stems or stem in repo.images or new in plan.units.values():
        plan.errors.append(f"{where}: {new} is already a file, a unit or an image")
        return
    plan.units[old] = new


def check_asm(repo: Repository, plan: Plan, where: str, row: Row) -> None:
    entry = unit_entry(repo, row.image, row.old, "asm")
    stem = row.new[:-2] if row.new.endswith(".s") else row.new
    if entry is None:
        old = Path(row.old) if "/" in row.old else Path("decomp/src") / row.image / row.old
        if not old.exists() and old.with_name(f"{stem}.s").exists():
            plan.applied += 1  # moved already
        else:
            plan.errors.append(
                f"{where}: {row.old} is no .s file of {row.image} named by its function"
            )
        return
    function = entry.binding[len("follows:") :]
    if (row.image, function) not in plan.renames:
        plan.errors.append(
            f"{where}: {entry.name} follows its function {function}, after which"
            " INCLUDE_ASM names it: rename the function"
        )
    elif plan.renames[(row.image, function)] != stem:
        plan.errors.append(
            f"{where}: {entry.name} follows its function: "
            f"{plan.renames[(row.image, function)]}.s, not {stem}.s"
        )


def check_param(
    repo: Repository, plan: Plan, names: Names, files: list[Path], where: str, row: Row
) -> None:
    entry = repo.entries.get((row.image, row.old))
    function = row.old.rpartition(".")[0]
    new = row.new
    if entry is None or entry.kind != "param":
        functions = {function, plan.names.get((row.image, function))}
        if any(
            source.tokens[i].text == new
            for path in files
            if path.suffix in (".c", ".h") and path.parts[0] == "decomp"
            for source in [repo.c(path)]
            for f in source.functions
            if source.tokens[f.name].text in functions
            for i in parameters(source, f)
            if i is not None
        ):
            plan.applied += 1  # renamed already
        else:
            plan.errors.append(f"{where}: {row.old} is no placeholder parameter of {row.image}")
        return
    if not GAME_NAME.fullmatch(new) or new in C_KEYWORDS:
        plan.errors.append(f"{where}: parameter {new!r} is not a lower snake_case identifier")
        return
    if new in names.macros or new in names.typedefs:
        plan.errors.append(f"{where}: parameter {new} is a macro or a type")
        return
    for path in files:
        if path.suffix not in (".c", ".h") or str(path) not in names.code.get(function, ()):
            continue
        source = repo.c(path)
        for f in source.functions:
            if source.tokens[f.name].text == function and new in function_identifiers(source, f):
                plan.errors.append(
                    f"{where}: parameter {new} is already a name in {function} ({path})"
                )
                return
    plan.params[(row.image, row.old)] = new


# --- apply -------------------------------------------------------------------


@dataclass
class Change:
    path: Path
    line: int
    old: str
    new: str
    how: str


SYMBOL_FILE_HEADER = "// Names tools/names.py gave these addresses (docs/matching.md, Names)."


def unit_settings() -> list[str]:
    """The per-unit variables decomp/Makefile reads (CC_<file>, GP_<file>, ...)."""
    return sorted(set(UNIT_SETTING.findall(MAKEFILE.read_text())))


def apply_edits(text: str, edits: list[tuple[int, int, str]]) -> str:
    out, at = [], 0
    for start, end, new in sorted(set(edits)):
        if start < at:
            fail(f"overlapping edits at offset {start}: {new!r}")
        out += [text[at:start], new]
        at = end
    return "".join(out + [text[at:]])


def symbol_lines(text: str) -> list[tuple[str, int, str]]:
    """(name, address, attributes) of each line of a splat symbol file."""
    found = []
    for line in text.splitlines():
        match = SYMBOL_LINE.match(line)
        if match and not line.lstrip().startswith("//"):
            found.append((match.group(1), int(match.group(2), 0), match.group(3)))
    return found


def read_overrides(path: Path | None) -> dict[tuple[str, int | str, str], str]:
    overrides: dict[tuple[str, int | str, str], str] = {}
    if path is None:
        return overrides
    for n, line in enumerate(path.read_text().splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        columns = [c.strip() for c in line.split("\t")]
        if len(columns) != 4 or not (columns[1] == "*" or columns[1].isdigit()):
            fail(f"{path}:{n}: not `file line old image` (line a number or *)")
        file, where, old, image = columns
        overrides[(file, "*" if where == "*" else int(where), old)] = image
    return overrides


def apply(repo: Repository, plan: Plan, files: list[Path], overrides: dict, dry_run: bool) -> int:
    edits: dict[Path, list[tuple[int, int, str]]] = defaultdict(list)
    changes: list[Change] = []
    problems: list[str] = []
    ambiguous: list[str] = []
    uses: dict[tuple[str, str], set[str]] = defaultdict(set)
    renamed = {name for _image, name in plan.renames}
    for path in files:
        text = file_text(path)
        for m in PLACEHOLDER.finditer(text):
            name = m.group()
            if name not in renamed:
                continue
            key, how, candidates = bind(repo, path, name, m.start(), overrides)
            line = repo_line(path, m.start())
            if key in plan.renames:
                edits[path].append((m.start(), m.end(), plan.renames[key]))
                changes.append(Change(path, line, name, plan.renames[key], how))
                uses[key] |= {
                    t
                    for t in repo.scopes.get(path, ())
                    if resolve(repo, repo.target(t), name) == key or name in repo.auto[t]
                }
            elif how == "conflict" and any(k in plan.renames for k in candidates):
                problems.append(
                    f"{path}:{line}: {name}: the targets reading it bind it to "
                    + ", ".join(f"{k[0] or 'no image'}'s" for k in sorted(candidates))
                )
            elif how == "ambiguous" and any(k in plan.renames for k in candidates):
                ambiguous.append(
                    f"{path}:{line}: {name}: "
                    + ", ".join(sorted(k[0] for k in candidates))
                    + " hold it"
                )
    rename_units(repo, plan, files, edits, changes)
    rename_params(repo, plan, files, edits, changes)
    write_addresses(repo, plan, files, edits, changes)
    add_symbols(repo, plan, uses, edits, changes, problems)
    moves = [(old, new) for old, new in sorted(plan.units.items()) if old.exists()]
    for old, new in moves:
        changes.append(Change(old, 0, str(old), str(new), "move"))
    report = Path(".local/names/apply.tsv")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(
        "file\tline\told\tnew\thow\n"
        + "".join(f"{c.path}\t{c.line}\t{c.old}\t{c.new}\t{c.how}\n" for c in changes)
    )
    for message in ambiguous:
        print(f"ambiguous: {message}", file=sys.stderr)
    for message in problems:
        print(f"error: {message}", file=sys.stderr)
    if problems:
        return 1
    count: dict[str, int] = defaultdict(int)
    for c in changes:
        count[c.how] += 1
    summary = (
        f"{len([p for p in edits if edits[p]])} files"
        + "".join(f", {n} {how}" for how, n in sorted(count.items()))
        + f"; {len(ambiguous)} ambiguous left (report {report})"
    )
    if dry_run:
        print(f"apply --dry-run: would change {summary}")
        return 0
    for path, changed in edits.items():
        if changed:
            path.write_text(apply_edits(file_text(path), changed))
    for old, new in moves:
        subprocess.run(["git", "mv", str(old), str(new)], check=True)
    names = [line for line in SOURCE_FILES.read_text().splitlines() if line]
    if names != sorted(names):
        SOURCE_FILES.write_text("\n".join(sorted(names)) + "\n")
    print(f"apply: changed {summary}")
    return 0


def rename_units(
    repo: Repository,
    plan: Plan,
    files: list[Path],
    edits: dict[Path, list[tuple[int, int, str]]],
    changes: list[Change],
) -> None:
    """A renamed unit's name in paths, per-unit settings, yaml subsegments,
    INCLUDE_ASM folders and prose; its overview comment keeps its address."""
    settings = "|".join(unit_settings())
    stems: dict[str, int] = defaultdict(int)
    for units in repo.units.values():
        for unit in units:
            stems[unit.stem] += 1
    for old, new in sorted(plan.units.items()):
        if old.suffix != ".c" or not old.exists():
            continue
        a, b = re.escape(old.stem), new.stem
        image = next(img for img, units in repo.units.items() if old in units)
        patterns = [re.compile(rf"\b(?:{settings})_({a})\b")]
        if old.stem not in repo.images and stems[old.stem] == 1:
            patterns.append(re.compile(rf"\b({a})\b"))
            subsegments = []
        else:  # a unit named like its image: only where it names the unit
            patterns += [re.compile(rf"\b({a})\.[co]\b"), re.compile(rf"(?<=nonmatchings/)({a})\b")]
            subsegments = [
                re.compile(
                    rf"^\s*-\s*\[\s*0x[0-9A-Fa-f]+\s*,\s*[\w.]+\s*,\s*({a})(?=\s*[,\]])", re.M
                ),
                re.compile(rf"^(?=[^\n]*\btype:)[^\n]*\bname:\s*({a})(?=\s*[,}}])", re.M),
                re.compile(rf"^[ \t]+name:[ \t]*({a})[ \t]*$", re.M),
            ]
        yamls = {Path(t.values["SPLAT_CONFIG"]) for t in repo.images[image]}
        for path in files:
            text = file_text(path)
            if old.stem not in text:
                continue
            spans = {m.span(1) for pattern in patterns for m in pattern.finditer(text)}
            if path in yamls:
                spans |= {m.span(1) for pattern in subsegments for m in pattern.finditer(text)}
            for start, end in sorted(spans):
                edits[path].append((start, end, b))
                changes.append(Change(path, repo_line(path, start), old.stem, b, "unit"))
        address = ADDRESS.search(old.stem)
        if address:
            source = repo.c(old)
            first = next((t for t in source.tokens if t.kind == "comment"), None)
            site = Site(
                old.stem,
                True,
                first.end if first else 0,
                (first.start, first.end) if first else None,
                "c",
            )
            edit = address_edit(source.text, site, [address.group()], (old.stem,))
            if edit:
                edits[old].append(edit)
                changes.append(Change(old, repo_line(old, edit[0]), "", address.group(), "address"))


def rename_params(
    repo: Repository,
    plan: Plan,
    files: list[Path],
    edits: dict[Path, list[tuple[int, int, str]]],
    changes: list[Change],
) -> None:
    """Parameters, only inside their own function: its parameter list, a
    definition's declarations, body and leading comment."""
    functions = {old.rpartition(".")[0] for _image, old in plan.params}
    for path in files:
        if path.suffix not in (".c", ".h") or path not in repo.scopes:
            continue
        source = repo.c(path)
        for f in source.functions:
            fname = source.tokens[f.name].text
            if fname not in functions:
                continue
            key, _how, _candidates = bind(repo, path, fname, source.tokens[f.name].start)
            renames = {
                old.rpartition(".")[2]: new
                for (image, old), new in plan.params.items()
                if key and image == key[0] and old.rpartition(".")[0] == fname
            }
            if not renames:
                continue
            end = f.body[1] if f.body else f.close
            first = leading_comment(source, f.start) if f.body else None
            span = range(first if first is not None else f.open, end)
            word = re.compile(r"\b(" + "|".join(map(re.escape, renames)) + r")\b")
            for i in span:
                t = source.tokens[i]
                inside = f.open < i
                if t.kind == "ident" and inside and t.text in renames and i not in source.members:
                    hits = [(t.start, t.end, t.text)]
                elif t.kind == "comment":
                    hits = [
                        (t.start + m.start(), t.start + m.end(), m.group())
                        for m in word.finditer(t.text)
                    ]
                else:
                    continue
                for start, stop, old in hits:
                    edits[path].append((start, stop, renames[old]))
                    changes.append(
                        Change(
                            path, repo_line(path, start), f"{fname}.{old}", renames[old], "param"
                        )
                    )


def write_addresses(
    repo: Repository,
    plan: Plan,
    files: list[Path],
    edits: dict[Path, list[tuple[int, int, str]]],
    changes: list[Change],
) -> None:
    """Each renamed definition's address in its comment: in the file defining
    it, and on each linker-script line assigning it."""
    renamed = {name for _image, name in plan.renames}
    for path in files:
        if path.suffix not in (".c", ".h", ".s", ".ld"):
            continue
        text = file_text(path)
        done: set[str] = set()
        groups: dict[tuple[bool, int], list[tuple[Site, str]]] = defaultdict(list)
        for site in sites_of(repo, path):
            if site.name not in renamed:
                continue
            key, _how, _candidates = bind(repo, path, site.name, site.position)
            if key not in plan.renames:
                continue
            if path.suffix != ".ld":
                if str(path) != repo.entries[key].file or site.name in done:
                    continue
                done.add(site.name)
            address = ADDRESS.search(site.name).group()
            group = (
                (True, site.comment[0] if site.comment else site.position)
                if site.leading
                else (False, line_span(text, site.position)[0])
            )
            groups[group].append((site, address))
        for items in groups.values():
            edit = address_edit(
                text,
                items[0][0],
                [address for _site, address in items],
                tuple(site.name for site, _address in items),
            )
            if edit:
                edits[path].append(edit)
                changes.append(
                    Change(
                        path,
                        repo_line(path, edit[0]),
                        "",
                        " ".join(a for _s, a in items),
                        "address",
                    )
                )


def add_symbols(
    repo: Repository,
    plan: Plan,
    uses: dict[tuple[str, str], set[str]],
    edits: dict[Path, list[tuple[int, int, str]]],
    changes: list[Change],
    problems: list[str],
) -> None:
    """Each new name in a symbol file the splits read: an image's own symbols
    in its own file; another image's name where a link takes it from splat's
    lists, in the resident's file for the resident's own symbols, else in the
    importer's own file (unless a fragment or symbol file there names it)."""
    readable = {f for files in repo.symbol_files.values() for f in files}
    after = {f: symbol_lines(apply_edits(file_text(f), edits.get(f, []))) for f in readable}
    declared = {f: {name for name, _a, _x in lines} for f, lines in after.items()}
    at: dict[int, list[tuple[Path, str, str]]] = defaultdict(list)
    for f, lines in after.items():
        for name, address, attributes in lines:
            at[address].append((f, name, attributes))
    together = {
        f: {g for files in repo.symbol_files.values() if f in files for g in files}
        for f in readable
    }  # the files a split reads with f
    assigned = {
        t.name: {
            site.name
            for script in t.values.get("LINKER_EXTRA", "").split()
            if not script.endswith("_auto.txt") and Path(script).exists()
            for site in ld_sites(apply_edits(file_text(Path(script)), edits.get(Path(script), [])))
        }
        for t in repo.targets
    }
    additions: dict[Path, list[tuple[int, str, str]]] = defaultdict(list)
    for key, new in sorted(plan.renames.items()):
        entry = repo.entries[key]
        own = entry.binding in OWN_BINDINGS
        wanted = [repo.own_file(key[0])] if own else []
        # the targets using it: in the files they read, or only in their
        # generated assembly (splat's lists)
        users = uses[key] | {
            t.name
            for t in repo.targets
            if entry.name in repo.auto[t.name] and resolve(repo, t, entry.name) == key
        }
        for t in sorted(users):
            files = repo.symbol_files[t]
            if repo.image[t] == key[0] or own and repo.own_file(key[0]) in files:
                continue
            if any(new in declared[f] for f in files) or new in assigned[t]:
                continue
            wanted.append(files[0])
        for f in dict.fromkeys(wanted):
            if new in declared[f]:
                continue
            declared[f].add(new)
            attributes = ["type:func"] if entry.kind == "func" else []
            others = sorted(
                {(g, n, x) for g, n, x in at[entry.address] if g in together[f] and n != new}
            )
            if others:
                if all("allow_duplicated:True" in x for _g, _n, x in others):
                    attributes.append("allow_duplicated:True")
                else:
                    problems.append(
                        f"{f}: {new} = 0x{entry.address:08X}: "
                        + ", ".join(f"{g} names it {n}" for g, n, _x in others)
                        + "; both lines need allow_duplicated:True"
                    )
            line = f"{new} = 0x{entry.address:08X};" + (
                f" // {' '.join(attributes)}" if attributes else ""
            )
            additions[f].append((entry.address, new, line))
    for f, lines in additions.items():
        text = file_text(f)
        block = "" if not text or text.endswith("\n") else "\n"
        if SYMBOL_FILE_HEADER not in text:
            block += SYMBOL_FILE_HEADER + "\n"
        block += "".join(f"{line}\n" for _a, _n, line in sorted(lines))
        edits[f].append((len(text), len(text), block))
        for _a, _new, line in sorted(lines):
            changes.append(Change(f, 0, "", line, "symbol-file"))


# --- the command line --------------------------------------------------------


def report_check(plan: Plan, rows: int) -> None:
    for message in plan.warnings:
        print(f"warning: {message}", file=sys.stderr)
    for message in plan.errors:
        print(f"error: {message}", file=sys.stderr)
    unnamed = Path(".local/names/unnamed.tsv")
    write_table(unnamed, ordered(plan.unnamed))
    counts: dict[tuple[str, str], int] = defaultdict(int)
    for e in plan.unnamed:
        counts[(e.image, e.kind)] += 1
    print(
        f"check: {rows} rows ({len(plan.renames)} symbols, "
        f"{len([u for u in plan.units if u.suffix == '.c'])} units, "
        f"{len([u for u in plan.units if u.suffix == '.s'])} .s files, {len(plan.params)}"
        f" parameters, {sum(map(len, plan.prefixes.values()))} prefixes"
        + (f", {plan.applied} applied already" if plan.applied else "")
        + f"); {len(plan.errors)} errors, {len(plan.warnings)} warnings"
    )
    print(
        f"unnamed: {len(plan.unnamed)} placeholders ({unnamed}): "
        + ", ".join(f"{image} {kind} {n}" for (image, kind), n in sorted(counts.items()))
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("inventory", help="list every placeholder under .local/names/")
    checking = commands.add_parser("check", help="validate a mapping")
    checking.add_argument("mapping", type=Path)
    applying = commands.add_parser("apply", help="check a mapping, then rename")
    applying.add_argument("mapping", type=Path)
    applying.add_argument(
        "--overrides", type=Path, help="TSV of file, line (or *), old, image (or - to keep)"
    )
    applying.add_argument("--dry-run", action="store_true", help="report, change nothing")
    args = parser.parse_args()
    repo = load()
    if args.command == "inventory":
        entries = ordered(repo.entries.values())
        out = Path(".local/names")
        counts: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
        for e in entries:
            counts[e.image][e.kind] += 1
        for image in counts:
            write_table(
                out / f"{image.replace('?', 'unresolved')}.tsv",
                [e for e in entries if e.image == image],
            )
        write_table(out / "all.tsv", entries)
        for image, kinds in sorted(counts.items()):
            print(
                f"{image}: "
                + ", ".join(f"{kind} {kinds[kind]}" for kind in KINDS if kinds.get(kind))
            )
        print(f"inventory: {len(entries)} placeholders in {len(counts)} images ({out}/)")
        return
    files = tracked()
    plan = check(repo, args.mapping, files)
    report_check(plan, len(read_rows(args.mapping, [])))
    if plan.errors:
        sys.exit(1)
    if args.command == "apply":
        sys.exit(apply(repo, plan, files, read_overrides(args.overrides), args.dry_run))


if __name__ == "__main__":
    main()
