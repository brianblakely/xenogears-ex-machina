"""The game's text in other languages: the English of the user's discs for
translators, and the checked translations the runtime ships.

The texts are the entries of the text tables tools.analysis.text_control
reads (docs/scripts/text-control.md). A translation names each text by the
FNV-1a 64 hash of its original bytes through its closing 00, which the port
(src/port/locale.c) computes for every text a window takes; the runtime
installs the translations of the language the player chose (docs/runtime.md,
Languages). Nothing English is committed: `extract` writes the English to
.local/locale/ (batches for translators beside it), and the committed files,
src/runtime/crates/xem-core/locale/<language>/*.txt, hold one line per text:

    <where> <hash> <translation>

<where> names the first table entry holding the text (f012-0003: field map
12's message 3; s22-0005: system data resource 22's entry 5; and so on, see
GROUPS), <hash> the original's 16 hex digits, and the translation is markup:
characters, which the font of the language turns into glyphs
(xem_core::locale, the extra glyphs it composes from the game's own), and
controls in braces, the codes window_reveal_text interprets ({n} a new line,
{p} a page, {wait 30}, {name 128}, ...; CONTROLS below).

    python3 -m tools.translations extract        # .local/locale/en.json, .local/locale/batches/
    python3 -m tools.translations merge pt-BR    # .local/locale/pt-BR/*.txt -> src/runtime/crates/xem-core/locale/pt-BR/
    python3 -m tools.translations check pt-BR    # the committed translation against the discs

`merge` and `check` hold each translation to what its windows take: the
controls of the original (new lines move freely; a page may be added only
where the player's Confirm already ends the text; a choice keeps its last
page's lines), and each line and page within its window (`limits`). A
translation that is the original's own bytes (a name, a cry, a developer's
label) is not shipped, as the original shows where a language has no
translation: `same.txt` lists those texts by <where> and <hash> alone, so
`check` tells them from texts left untranslated.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".local/locale"
SHIPPED = ROOT / "src/runtime/crates/xem-core/locale"

THRESHOLD = 0xFE  # text_font_two_byte_threshold: font halfword 2 on both discs
FIRST_GLYPH = 0x10  # font halfword 6: the first one-byte glyph
NARROW_GLYPHS = 0x51  # font halfword 4: codes 0x10-0x60 advance 2 columns, others 3

# The one-byte glyphs' characters, read from the font (directory (0, 1) file
# 6, glyphs from byte 14, 22 bytes each): the narrow ones are ASCII, the wide
# ones full-width forms and symbols. 0x82 and 0x83 repeat 0x6B and 0x6C, and
# 0x93-0xFD and the two-byte glyphs are kana, used only by Japanese
# placeholders; markup names those as {hex}.
NARROW = " +,-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ[]=abcdefghijklmnopqrstuvwxyz!\"#%&'():?"
WIDE = "０１２３４５６７８９％＆☆○□△※→←↑↓．：－／‥“」＋−×＜＞" + "\0\0" + "『』＃、。・？！＿～…”（）「"
CHARS: dict[int, str] = {FIRST_GLYPH + i: c for i, c in enumerate(NARROW)}
CHARS.update({0x61 + i: c for i, c in enumerate(WIDE) if c != "\0"})
CODES = {c: code for code, c in CHARS.items()}
assert len(NARROW) == NARROW_GLYPHS and max(CHARS) == 0x92

# Controls as markup: (first byte, 0F sub-code or None, name, operand count).
CONTROLS = {
    "n": (0x01, None, 0),
    "p": (0x02, None, 0),
    "pause": (0x03, None, 0),
    "wait": (0x0F, 0x00, 1),
    "speed": (0x0F, 0x01, 1),
    "done": (0x0F, 0x02, 1),
    "res": (0x0F, 0x03, 2),
    "item": (0x0F, 0x04, 0),
    "name": (0x0F, 0x05, 1),
    "res23": (0x0F, 0x06, 1),
    "res24": (0x0F, 0x07, 1),
    "res25": (0x0F, 0x08, 1),
    "num": (0x0F, 0x09, 1),
    "num1": (0x0F, 0x0A, 1),
    "set6d": (0x0F, 0x0B, 1),
    "signed": (0x0F, 0x0C, 1),
    "skip": (0x0F, 0x0D, 1),
    "slow": (0x0F, 0x0E, 1),
    "button": (0x0F, 0x0F, 1),
}
SUBCODES = {sub: name for name, (first, sub, _) in CONTROLS.items() if first == 0x0F}
# Columns (4 pixels) a control's insertion is taken to fill when a line is
# measured: a party name, an item's name, a number.
INSERT_COLUMNS = {"name": 16, "item": 36, "num": 12, "num1": 12, "signed": 14, "res": 36,
                  "res23": 36, "res24": 36, "res25": 36, "button": 16}
# Controls whose order and count a translation keeps (the reveal's timing and
# the window's waits), and those it keeps as a set (insertions).
TIMING = ("pause", "wait", "speed", "done", "skip", "slow", "set6d")
ENDS_WITHOUT_CONFIRM = ("done", "skip")  # the text ends on a timed wait, not Confirm


class MarkupError(ValueError):
    pass


def fnv1a64(data: bytes) -> int:
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def text_bytes(data: bytes, offset: int) -> bytes:
    """A text's bytes through its closing 00, as window_reveal_text walks it
    (src/port/locale.c `xem_text_length` walks it alike)."""
    at = offset
    while True:
        byte = data[at]
        if byte == 0:
            return data[offset : at + 1]
        if byte in (1, 2, 3):
            at += 1
        elif byte == 0x0F:
            sub = data[at + 1]
            at += 2 if sub in (0x04, 0x0B) else 4 if sub == 0x03 else 3
        elif byte < THRESHOLD:
            at += 1
        else:
            at += 2


def render(raw: bytes) -> str:
    """Markup of a text's bytes (without its closing 00)."""
    out, at = [], 0
    while at < len(raw) - 1:
        byte = raw[at]
        if byte == 1:
            out.append("{n}")
            at += 1
        elif byte == 2:
            out.append("{p}")
            at += 2 if raw[at + 1] == 1 else 1  # a 01 after a page is skipped
        elif byte == 3:
            out.append("{pause}")
            at += 1
        elif byte == 0x0F:
            sub = raw[at + 1]
            name = SUBCODES[sub]
            count = CONTROLS[name][2]
            if sub == 0x0B:  # set6d leaves the pointer on its operand
                out.append("{set6d}")
                at += 2
                continue
            operands = raw[at + 2 : at + 2 + count]
            out.append("{" + " ".join([name, *map(str, operands)]) + "}")
            at += 2 + count
        elif byte < THRESHOLD:
            out.append(CHARS.get(byte, f"{{{byte:02X}}}"))
            at += 1
        else:
            out.append(f"{{{byte:02X}{raw[at + 1]:02X}}}")
            at += 2
    return "".join(out)


TOKEN_RE = re.compile(r"\{([^{}]*)\}|([^{}])")


@dataclass
class Token:
    kind: str  # "glyph", a control name
    operands: tuple[int, ...] = ()
    code: int = 0  # a glyph's code (two-byte: first << 8 | second)
    char: str = ""


def tokens(markup: str, extra: dict[str, int] | None = None) -> list[Token]:
    """Markup as tokens; `extra` maps a language's added characters to their
    two-byte codes (EXTRA_GLYPHS on)."""
    out = []
    pos = 0
    for match in TOKEN_RE.finditer(markup):
        if match.start() != pos:
            raise MarkupError(f"unbalanced brace at {pos}: {markup[pos:pos + 20]!r}")
        pos = match.end()
        tag, char = match.groups()
        if char is not None:
            if char in CODES:
                out.append(Token("glyph", code=CODES[char], char=char))
            elif extra and char in extra:
                out.append(Token("glyph", code=extra[char], char=char))
            else:
                raise MarkupError(f"no glyph for {char!r}")
            continue
        parts = tag.split()
        if not parts:
            raise MarkupError("empty braces")
        if re.fullmatch(r"[0-9A-F]{2}", parts[0]) and len(parts) == 1:
            out.append(Token("glyph", code=int(parts[0], 16), char=match.group(0)))
            continue
        if re.fullmatch(r"[0-9A-F]{4}", parts[0]) and len(parts) == 1:
            out.append(Token("glyph", code=int(parts[0], 16), char=match.group(0)))
            continue
        name = parts[0]
        if name not in CONTROLS:
            raise MarkupError(f"unknown control {{{tag}}}")
        count = 0 if name == "set6d" else CONTROLS[name][2]
        if len(parts) != 1 + count or not all(p.isdigit() and int(p) < 256 for p in parts[1:]):
            raise MarkupError(f"{{{tag}}} takes {count} operand(s) of 0-255")
        out.append(Token(name, tuple(int(p) for p in parts[1:])))
    if pos != len(markup):
        raise MarkupError(f"unbalanced brace at {pos}: {markup[pos:pos + 20]!r}")
    return out


def encode(markup: str, extra: dict[str, int] | None = None) -> bytes:
    out = bytearray()
    for token in tokens(markup, extra):
        if token.kind == "glyph":
            out += bytes([token.code]) if token.code < 0x100 else token.code.to_bytes(2, "big")
            continue
        first, sub, _ = CONTROLS[token.kind]
        out.append(first)
        if sub is not None:
            out.append(sub)
        out += bytes(token.operands)
    return bytes(out) + b"\0"


EXTRA_GLYPHS = 0xFF20  # a language's added letters: 0xFF20 on (src/port/locale.c)


def glyph_columns(code: int) -> int:
    """text_get_glyph_width: 2 for a narrow glyph, else 3; a language's added
    glyphs are narrow letters."""
    if code < 0x100:
        return 2 if code - FIRST_GLYPH < NARROW_GLYPHS else 3
    return 2 if EXTRA_GLYPHS <= code < 0xFFFF else 3


def pages(toks: list[Token]) -> list[list[int]]:
    """Each page's lines' widths in columns (insertions at INSERT_COLUMNS)."""
    result, lines, width = [], [], 0
    for token in toks:
        if token.kind == "glyph":
            width += glyph_columns(token.code)
        elif token.kind in INSERT_COLUMNS:
            width += INSERT_COLUMNS[token.kind]
        elif token.kind == "n":
            lines.append(width)
            width = 0
        elif token.kind == "p":
            lines.append(width)
            result.append(lines)
            lines, width = [], 0
    lines.append(width)
    result.append(lines)
    return result


# Groups: the prefix of <where>, the text_control group names it covers.
GROUPS = {
    "f": "field messages",
    "s": "system data",
    "ml": "menu labels",
    "mw": "menu labels (world map file 0x26)",
    "md": "menu data",
    "mm": "menu mode",
    "ba": "battle archive",
    "en": "enemy data",
    "bm": "battle menu",
    "be": "battle events",
    "wa": "world areas",
    "mc": "menu commands",
}
PREFIX = {name: prefix for prefix, name in GROUPS.items()}
PREFIX["field messages (packed stream reads past its file)"] = "f"

# A field message's window (field_event_open_message_window, field_dialogue_open_window):
# the table's (columns, rows), with a portrait at least 0x18 columns plus 0x11
# for the portrait and 4 rows; the window stays on the 320-pixel screen up to
# about 70 columns, so a translation's lines may take FIELD_COLUMNS when its
# table's are fewer. The fixed box (03 message_box) is 0x48 x 4.
FIELD_COLUMNS = 52
FIELD_ROWS = 4
BOX = (0x48, 4)


@dataclass
class Text:
    where: str
    hash: str
    markup: str
    columns: int  # the widest line a translation may have
    rows: int  # the most lines a page may have
    group: str
    places: list[str] = field(default_factory=list)
    choice: tuple[int, int] | None = None  # field choice lines (first, last)
    fixed: bool = False  # the script sets the window's size
    original_columns: int = 0
    original_rows: int = 0


def field_scripts(disc) -> dict[int, dict]:
    """Per map: messages offered as a choice (message -> (first, last)),
    messages a script shows in a window it sized (message -> (columns,
    rows)) and the messages each window mode shows (`boxed`: d3 and 03, whose
    window is 0x48 x 4 whatever the table says), from the event scripts in
    PC order: a9 offers the lines of the message the running script showed
    last, cf/d0 set the layout its next speaker or centred messages (d2, d4,
    fc, f5) take until f4 resets it."""
    from tools.analysis import events

    extract = ROOT / f".local/extract/disc{disc.number}"
    result = {}
    for map_id, path in events.map_files(extract, disc.track):
        if not path.exists():
            continue
        try:
            package = events.map_events(path.read_bytes())
        except Exception:  # noqa: BLE001 - a placeholder or unreadable map has no scripts
            continue
        if package is None:
            continue
        starts, _ = events.script_entries(package)
        found = events.walk(package.bytecode, [pc for _, _, pc in starts])
        choices, fixed, boxed, speaker = {}, {}, set(), set()
        last, layout = None, None
        for pc in sorted(found.instructions):
            ins = found.instructions[pc]
            if ins.extended is not None:
                continue
            op = ins.opcode
            if op in (0x03, 0xD2, 0xD3, 0xF5):
                last = ins.operands[0]
            elif op in (0xD4, 0xFC):
                last = ins.operands[1]
            else:
                if op == 0xA9 and last is not None:
                    value = ins.operands[0]
                    choices[last] = (value >> 4, value & 15)
                elif op == 0xCF:
                    layout = (ins.operands[2], ins.operands[3])
                elif op == 0xD0:
                    columns, rows = ins.operands[2], ins.operands[3]
                    layout = (columns & 0x7FFF if columns & 0x8000 else 0xFF,
                              rows & 0x7FFF if rows & 0x8000 else 0xFF)
                elif op == 0xF4 and ins.operands[0] != 0:
                    layout = None
                continue
            if op in (0x03, 0xD3):
                boxed.add(last)
                continue
            speaker.add(last)
            if layout is not None:
                fixed[last] = layout
        result[map_id] = {"choices": choices, "fixed": fixed, "boxed": boxed, "speaker": speaker}
    return result


# The menus' command labels (src/port/locale.c): words drawn in the menu's
# textures, sheet entries 0x123-0x149 of its sprite sheet (directory (0x10, 0)
# file 1, archive entry 2), named by the hash of each entry's bytes (its part
# count, then 0x1C bytes a part). Their English is a picture, not text; a
# translation is one line the port draws with the game's text renderer, at
# most LABEL_COLUMNS columns.
LABELS = range(0x123, 0x14A)
LABEL_COLUMNS = 59


def menu_labels(disc) -> list[tuple[int, bytes]]:
    from tools.analysis import text_control as tc

    sheet = tc.archive_entry(disc.sectors(disc.slot(0x10, 0, 1)), 2, packed=True)
    out = []
    for label in LABELS:
        offset = struct.unpack_from("<H", sheet, 4 + 2 * label)[0]
        parts = struct.unpack_from("<h", sheet, offset)[0]
        out.append((label, sheet[offset : offset + 4 + 0x1C * parts]))
    return out


def originals() -> dict[str, Text]:
    """Every distinct text of both discs, by hash, with its limits."""
    from tools.analysis import text_control as tc
    from tools.analysis.disc_index import Disc

    texts: dict[str, Text] = {}
    table_columns: dict[tuple[str, int], int] = {}
    entries = []
    for number in (1, 2):
        disc = Disc(number)
        scripts = field_scripts(disc)
        for label, data in menu_labels(disc):
            digest = f"{fnv1a64(data):016x}"
            text = texts.setdefault(digest, Text(f"mc{label:03d}-0000", digest, "", LABEL_COLUMNS, 1, GROUPS["mc"]))
            text.places.append(f"mc{label:03d}-0000@{number}")
        for group, item, data in tc.text_tables(disc):
            if data is None or isinstance(data, Exception):
                continue
            prefix = PREFIX[group]
            offsets, _ = tc.text_table(data)
            widest = 0
            for index, offset in enumerate(offsets):
                raw = text_bytes(data, offset)
                where = f"{prefix}{item:03d}-{index:04d}"
                markup = render(raw)
                shape = pages(tokens(markup))
                widest = max(widest, *(max(p) for p in shape))
                pair = None
                if prefix == "f":
                    pair = struct.unpack_from("<BB", data, 4 + 2 * (len(offsets) + 1) + 2 * index)
                entries.append((number, prefix, item, index, where, raw, markup, shape, pair, scripts.get(item)))
            key = (prefix, item)
            table_columns[key] = max(table_columns.get(key, 0), widest)
    for number, prefix, item, index, where, raw, markup, shape, pair, script in entries:
        digest = f"{fnv1a64(raw):016x}"
        lines = max(len(p) for p in shape)
        widest = max(max(p) for p in shape)
        place = f"{where}@{number}"
        if prefix == "f":
            script = script or {}
            choice = script.get("choices", {}).get(index)
            fixed = script.get("fixed", {}).get(index)
            # Each way the scripts show it gives a window; the tightest
            # holds (a message no reachable script shows: a speaker's).
            windows = []
            if index in script.get("boxed", ()):
                windows.append(BOX)
            if fixed is not None:
                columns, rows = fixed
                windows.append((pair[0] if columns == 0xFF else columns, pair[1] if rows == 0xFF else rows))
            elif index in script.get("speaker", ()) or not windows:
                windows.append((max(pair[0], FIELD_COLUMNS),
                                max(pair[1], FIELD_ROWS) if lines <= FIELD_ROWS else max(pair[1], lines)))
            columns, rows = min(w[0] for w in windows), min(w[1] for w in windows)
            if choice is not None:
                rows = lines
            limits = (max(columns, widest), max(rows, lines))
        elif prefix == "be":
            limits = (widest, lines)  # the script sizes each message's window
        else:
            limits = (table_columns[(prefix, item)], lines)
        text = texts.get(digest)
        if text is None:
            text = texts[digest] = Text(where, digest, markup, limits[0], limits[1], GROUPS[prefix],
                                        original_columns=widest, original_rows=lines)
        else:  # the tightest window any of its places gives
            text.columns = min(text.columns, limits[0])
            text.rows = min(text.rows, limits[1])
        text.places.append(place)
        if prefix == "f":
            if choice is not None:
                text.choice = choice
            if fixed is not None:
                text.fixed = True
    return texts


def kana(markup: str) -> bool:
    """A Japanese placeholder: kana glyphs, which no English text uses."""
    return any(0x93 <= int(code, 16) <= 0xFD or len(code) == 4 and code != "FFFF"
               for code in re.findall(r"\{([0-9A-F]{2,4})\}", markup))


def translatable(text: Text) -> bool:
    """Texts worth a translation: some letter, and no kana."""
    return bool(re.search(r"[A-Za-z]", re.sub(r"\{[^}]*\}", "", text.markup))) and not kana(text.markup)


BATCH_CHARACTERS = 36000


def extract(args) -> int:
    texts = originals()
    LOCAL.mkdir(parents=True, exist_ok=True)
    ordered = sorted(texts.values(), key=lambda t: (list(GROUPS).index(re.match(r"[a-z]+", t.where)[0]), t.where))
    (LOCAL / "en.json").write_text(json.dumps([asdict(t) for t in ordered], ensure_ascii=False, indent=0))
    wanted = [t for t in ordered if translatable(t)]
    # The menus and system texts first (they settle the glossary), then the
    # battles' event messages, then the field maps in batches of whole maps.
    batches = [[t for t in wanted if t.group not in ("field messages", "battle events")],
               [t for t in wanted if t.group == "battle events"]]
    current, size = [], 0
    last_table = None
    for text in (t for t in wanted if t.group == "field messages"):
        table = text.where.split("-")[0]
        if current and size > BATCH_CHARACTERS and table != last_table:
            batches.append(current)
            current, size = [], 0
        current.append(text)
        size += len(text.markup)
        last_table = table
    if current:
        batches.append(current)
    out = LOCAL / "batches"
    out.mkdir(exist_ok=True)
    for old in out.glob("*.txt"):
        old.unlink()
    for number, batch in enumerate(batches):
        lines = []
        for text in batch:
            flags = [f"w{text.columns // 2}", f"r{text.rows}"]
            if text.choice:
                flags.append(f"choice{text.choice[0]}-{text.choice[1]}")
            if text.fixed:
                flags.append("fixed")
            if any(t.kind in ENDS_WITHOUT_CONFIRM for t in tokens(text.markup)):
                flags.append("timed")
            lines.append(f"{text.where} [{' '.join(flags)}] {text.markup}")
        (out / f"{number:03d}.txt").write_text("\n".join(lines) + "\n")
    by_group = Counter()
    for text in wanted:
        by_group[text.group] += len(text.markup)
    print(f"{len(texts)} distinct texts, {len(wanted)} to translate in {len(batches)} batches under {out}")
    for group, size in by_group.most_common():
        print(f"  {group}: {size} characters of markup")
    return 0


# --- translations ---------------------------------------------------------

LINE_RE = re.compile(r"^(?P<where>[a-z]+\d{3}-\d{4}) (?P<hash>[0-9a-f]{16}) (?P<text>.*)$")
WORK_RE = re.compile(r"^(?P<where>[a-z]+\d{3}-\d{4})(?: \[[^\]]*\])? (?P<text>.*)$")


def language_characters(language: str) -> dict[str, int]:
    """The characters a language adds to the font, from its glyphs.txt
    (`<char> <base> <accent> ...` lines): two-byte glyphs 0xFF20 on in file
    order (a second byte past the controls' 00-0F)."""
    path = SHIPPED / language / "glyphs.txt"
    extra = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields and not line.startswith("#") and fields[0] != "accent":
            extra[fields[0]] = EXTRA_GLYPHS + len(extra)
    return extra


def problems(original: Text, translation: str, extra: dict[str, int]) -> list[str]:
    """Why a translation does not fit its original's windows, if it does not."""
    try:
        new = tokens(translation, extra)
    except MarkupError as error:
        return [str(error)]
    old = tokens(original.markup)
    found = []

    def sequence(toks, names):
        return [(t.kind, t.operands) for t in toks if t.kind in names]

    if sequence(new, TIMING) != sequence(old, TIMING):
        found.append(f"timing controls {sequence(old, TIMING)} became {sequence(new, TIMING)}")
    inserts = tuple(INSERT_COLUMNS)
    if sorted(sequence(new, inserts)) != sorted(sequence(old, inserts)):
        found.append(f"insertions {sequence(old, inserts)} became {sequence(new, inserts)}")
    old_pages, new_pages = pages(old), pages(new)
    if len(new_pages) != len(old_pages) and not (may_add_pages(original) and len(new_pages) > len(old_pages)):
        found.append(f"{len(old_pages)} page(s) became {len(new_pages)}")
    if original.choice and new_pages and old_pages and len(new_pages[-1]) != len(old_pages[-1]):
        found.append(f"the choice's last page has {len(old_pages[-1])} lines, not {len(new_pages[-1])}")
    for page in new_pages:
        if len(page) > original.rows:
            found.append(f"a page of {len(page)} lines (at most {original.rows})")
            break
    widest = max(max(p) for p in new_pages)
    if widest > original.columns:
        found.append(f"a line of {widest} columns (at most {original.columns}, {original.columns // 2} letters)")
    return found


def markup(toks: list[Token]) -> str:
    out = []
    for token in toks:
        if token.kind == "glyph":
            out.append(token.char)
        else:
            out.append("{" + " ".join([token.kind, *map(str, token.operands)]) + "}")
    return "".join(out)


def width(toks: list[Token]) -> int:
    return sum(glyph_columns(t.code) if t.kind == "glyph" else INSERT_COLUMNS.get(t.kind, 0) for t in toks)


def may_add_pages(original: Text) -> bool:
    """A field message the player's Confirm ends, without a choice, in a
    window no script sized."""
    return (original.group == "field messages" and not original.choice and not original.fixed
            and not any(t.kind in ENDS_WITHOUT_CONFIRM or t.kind == "pause" for t in tokens(original.markup)))


def wrap(original: Text, translation: str, extra: dict[str, int]) -> str:
    """The translation with every line too wide for its window broken at its
    spaces, and every page with more lines than the window's rows split
    into pages of even length where a page may be added."""
    toks = tokens(translation, extra)
    split: list[list[list[Token]]] = [[[]]]
    for token in toks:
        if token.kind == "p":
            split.append([[]])
        elif token.kind == "n":
            split[-1].append([])
        else:
            split[-1][-1].append(token)
    space = lambda t: t.kind == "glyph" and t.code == CODES[" "]  # noqa: E731
    out_pages = []
    for page in split:
        lines = []
        for line in page:
            if width(line) <= original.columns:
                lines.append(line)
                continue
            indent = []
            while line and space(line[0]):
                indent.append(line.pop(0))
            words, current = [], []
            for token in line:
                if space(token):
                    if current:
                        words.append(current)
                    current = []
                else:
                    current.append(token)
            if current:
                words.append(current)
            row = list(indent)
            for word in words:
                if row and row != indent and width(row) + 2 + width(word) > original.columns:
                    lines.append(row)
                    row = []
                if row and row != indent:
                    row.append(Token("glyph", code=CODES[" "], char=" "))
                row.extend(word)
            lines.append(row)
        if len(lines) > original.rows and may_add_pages(original):
            count = -(-len(lines) // original.rows)
            size = -(-len(lines) // count)
            for start in range(0, len(lines), size):
                out_pages.append(lines[start : start + size])
        else:
            out_pages.append(lines)
    joined = []
    for number, page in enumerate(out_pages):
        if number:
            joined.append(Token("p"))
        for index, line in enumerate(page):
            if index:
                joined.append(Token("n"))
            joined.extend(line)
    return markup(joined)


def originals_by_where() -> dict[str, Text]:
    return {t.where: t for t in (Text(**{**d, "choice": tuple(d["choice"]) if d["choice"] else None})
                                 for d in json.loads((LOCAL / "en.json").read_text()))}


def lint(args) -> int:
    """Check translators' files: each line wrapped as `merge` wraps it, and
    what still does not fit, with the wrapped text."""
    texts = originals_by_where()
    extra = language_characters(args.language)
    bad = 0
    batches: dict[str, set[str]] = defaultdict(set)
    for path in args.files:
        work = read_work([Path(path)])
        batch = re.match(r"\d+", Path(path).name)
        if batch:
            batches[batch[0]] |= set(work)
        for where, translation in work.items():
            original = texts.get(where)
            if original is None:
                print(f"{where}: no such text")
                bad += 1
                continue
            try:
                wrapped = wrap(original, translation, extra)
            except MarkupError as error:
                print(f"{where}: {error}")
                bad += 1
                continue
            found = problems(original, wrapped, extra)
            if found:
                bad += 1
                print(f"{where} [w{original.columns // 2} r{original.rows}]: {'; '.join(found)}\n    wrapped: {wrapped}")
    for batch, done in batches.items():
        source = LOCAL / "batches" / f"{batch}.txt"
        expected = {line.split()[0] for line in source.read_text().splitlines() if line.strip()} if source.exists() else set()
        for where in sorted(expected - done):
            print(f"{where}: missing (batch {batch})")
            bad += 1
    print(f"{bad} problem(s)")
    return 1 if bad else 0


def read_work(paths) -> dict[str, str]:
    out = {}
    for path in paths:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if not line.strip():
                continue
            match = WORK_RE.match(line)
            if not match:
                raise SystemExit(f"{path}:{number}: not `<where> <translation>`: {line[:60]!r}")
            out[match["where"]] = match["text"]
    return out


SAME = "same.txt"  # texts whose translation is the original (<where> <hash>)


def shipped_files(language: str) -> dict[str, list[str]]:
    return {path.name: path.read_text().splitlines() for path in sorted((SHIPPED / language).glob("*.txt"))
            if path.name not in ("glyphs.txt", SAME)}


def same_as_original(language: str) -> set[str]:
    path = SHIPPED / language / SAME
    return {line.split()[1] for line in path.read_text().splitlines() if line.strip()} if path.exists() else set()


# The files a language ships (xem_core::locale embeds each by name).
FILES = ["system.txt", "menus.txt", "battle.txt", "world.txt", *(f"field-{n}.txt" for n in range(8))]


def file_for(where: str) -> str:
    prefix = re.match(r"[a-z]+", where)[0]
    if prefix == "f":
        number = int(where[1:4])
        return f"field-{number // 100}.txt"
    return {"s": "system.txt", "ml": "menus.txt", "mw": "menus.txt", "md": "menus.txt", "mm": "menus.txt", "mc": "menus.txt",
            "ba": "battle.txt", "en": "battle.txt", "bm": "battle.txt", "be": "battle.txt",
            "wa": "world.txt"}[prefix]


def merge(args) -> int:
    texts = originals_by_where()
    extra = language_characters(args.language)
    work = read_work([Path(f) for f in args.files] if args.files else sorted((LOCAL / args.language).glob("[0-9]*.txt")))
    bad = 0
    files: dict[str, list[str]] = defaultdict(list)
    for where, translation in sorted(work.items()):
        original = texts.get(where)
        if original is None:
            print(f"{where}: no such text")
            bad += 1
            continue
        try:
            translation = wrap(original, translation, extra)
        except MarkupError:
            pass
        found = problems(original, translation, extra)
        if found:
            bad += 1
            print(f"{where}: {'; '.join(found)}")
            if not args.keep_bad:
                continue
        if original.markup and encode(translation, extra) == encode(original.markup):
            files[SAME].append(f"{where} {original.hash}")
            continue
        files[file_for(where)].append(f"{where} {original.hash} {translation}")
    missing = [t.where for t in texts.values() if translatable(t) and t.where not in work]
    out = SHIPPED / args.language
    out.mkdir(parents=True, exist_ok=True)
    for name in [*FILES, SAME]:  # every file xem_core::locale embeds, empty or not, and the list
        lines = sorted(files.get(name, []))
        (out / name).write_text("".join(line + "\n" for line in lines))
    shipped = sum(len(lines) for name, lines in files.items() if name != SAME)
    print(f"{shipped} translations written, {len(files.get(SAME, []))} the same as the original, "
          f"{bad} refused, {len(missing)} texts untranslated")
    for where in missing[:20]:
        print(f"  untranslated: {where}")
    return 1 if bad or missing else 0


def check(args) -> int:
    texts = {t.hash: t for t in originals().values()}
    extra = language_characters(args.language)
    seen, bad = set(), 0
    for name, lines in shipped_files(args.language).items():
        for number, line in enumerate(lines, 1):
            match = LINE_RE.match(line)
            if not match:
                print(f"{name}:{number}: not `<where> <hash> <translation>`")
                bad += 1
                continue
            original = texts.get(match["hash"])
            if original is None:
                print(f"{name}:{number}: {match['where']}: no text of both discs has hash {match['hash']}")
                bad += 1
                continue
            if match["hash"] in seen:
                print(f"{name}:{number}: {match['where']}: hash {match['hash']} again")
                bad += 1
            seen.add(match["hash"])
            found = problems(original, match["text"], extra)
            if found:
                print(f"{name}:{number}: {match['where']}: {'; '.join(found)}")
                bad += 1
    same = same_as_original(args.language)
    for digest in same - set(texts):
        print(f"{SAME}: no text of both discs has hash {digest}")
        bad += 1
    missing = [t for t in texts.values() if translatable(t) and t.hash not in seen and t.hash not in same]
    print(f"{len(seen)} translations checked, {len(same)} the same as the original, {bad} problems, "
          f"{len(missing)} texts untranslated")
    for text in missing[:20]:
        print(f"  untranslated: {text.where}")
    return 1 if bad or missing else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("extract").set_defaults(run=extract)
    command = sub.add_parser("lint")
    command.add_argument("language")
    command.add_argument("files", nargs="+")
    command.set_defaults(run=lint)
    for name, run in (("merge", merge), ("check", check)):
        command = sub.add_parser(name)
        command.add_argument("language")
        command.set_defaults(run=run)
        if name == "merge":
            command.add_argument("--keep-bad", action="store_true", help="write refused translations too")
            command.add_argument("files", nargs="*", help="translators' files (default: every .local/locale/<language>/NNN*.txt)")
    args = parser.parse_args(argv)
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
