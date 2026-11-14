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
    <where> <hash>@<entry> <translation>

<where> names the first table entry holding the text (f012-0003: field map
12's message 3; s22-0005: system data resource 22's entry 5; and so on, see
GROUPS), <hash> the original's 16 hex digits, and the translation is markup:
characters, which the font of the language turns into glyphs
(xem_core::locale, the extra glyphs it composes from the game's own), and
controls in braces, the codes window_reveal_text interprets ({n} a new line,
{p} a page, {wait 30}, {name 128}, ...; CONTROLS below). A line without a
translation translates its text to nothing.

The first form translates a text wherever a window takes it. The second
translates it in one table entry alone, the one <where> names: <entry> is
that entry's name (`place_key`: the text's hash carried on over the entry's
index, its offset in its table and the table's count), which the port
computes where the game takes an entry from a table (text_get_resource_entry).
A language uses it where one original text reads differently from place to
place (Perfect Works titles each tune of a jukebox whose lists all read
"track00", "track01" ...).

    python3 -m tools.translations extract        # .local/locale/en.json, .local/locale/batches/
    python3 -m tools.translations merge pt-BR    # .local/locale/pt-BR/*.txt -> src/runtime/crates/xem-core/locale/pt-BR/
    python3 -m tools.translations check pt-BR    # the committed translation against the discs

`merge` and `check` hold each translation to what its windows take: the
controls of the original (new lines move freely; a page may be added only
where the player's Confirm already ends the text; a choice keeps its last
page's lines), and each line and page within its window (`limits`). A
language that brings its own layout (OWN_LAYOUT: Perfect Works, each text as
its build shows it on the console, tools/perfect_works.py) sets its own
waits, insertions and pages, and its lines may take the screen's columns in
the field and the line images' in the menus and the battle (`Place.own`); a
choice still keeps its cursor's lines. A translation that is the
original's own bytes (a name, a cry, a developer's label) is not shipped, as
the original shows where a language has no translation: `same.txt` lists
those texts by <where> and <hash> alone, so `check` tells them from texts
left untranslated.
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


def render(raw: bytes, chars: dict[int, str] = CHARS) -> str:
    """Markup of a text's bytes (without its closing 00); `chars` names the
    one-byte glyphs of the font the text was written for."""
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
            out.append(chars.get(byte, f"{{{byte:02X}}}"))
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
    columns: int = 0  # a glyph's advance


class Characters(dict):
    """The characters a language adds to the font and their two-byte codes;
    `wide` holds those the game draws 3 columns wide."""

    def __init__(self, codes=(), wide=()):
        super().__init__(codes)
        self.wide = frozenset(wide)


def glyph(code: int, char: str, columns: int = 0) -> Token:
    return Token("glyph", code=code, char=char, columns=columns or glyph_columns(code))


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
                out.append(glyph(CODES[char], char))
            elif extra and char in extra:
                out.append(glyph(extra[char], char, 3 if char in getattr(extra, "wide", ()) else 2))
            else:
                raise MarkupError(f"no glyph for {char!r}")
            continue
        parts = tag.split()
        if not parts:
            raise MarkupError("empty braces")
        if re.fullmatch(r"[0-9A-F]{2}", parts[0]) and len(parts) == 1:
            out.append(glyph(int(parts[0], 16), match.group(0)))
            continue
        if re.fullmatch(r"[0-9A-F]{4}", parts[0]) and len(parts) == 1:
            out.append(glyph(int(parts[0], 16), match.group(0)))
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
    glyphs are narrow letters but for those its glyphs.txt marks wide
    (`Characters.wide`, which `tokens` reads)."""
    if code < 0x100:
        return 2 if code - FIRST_GLYPH < NARROW_GLYPHS else 3
    return 2 if EXTRA_GLYPHS <= code < 0xFFFF else 3


def pages(toks: list[Token]) -> list[list[int]]:
    """Each page's lines' widths in columns (insertions at INSERT_COLUMNS)."""
    result, lines, width = [], [], 0
    for token in toks:
        if token.kind == "glyph":
            width += token.columns
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
    "mr": "menu letter rows",
    "nm": "names",
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
# Languages that bring their own layout (module notes), and the most such a
# language's text may take where the original's own text is no measure of it
# (`Place.own`). A field window's lines, where no script sizes it: the
# screen's columns (field_event_open_message_window keeps left >= 0xC with
# left + 4 * columns <= 0x124), or the fixed box's. Such a window's pages
# keep their own lines: it takes its longest page's rows
# (text_get_message_rows), and one with more lines than rows turns them over
# as the original's long scrolls do (window_reveal_text). And the line
# images the menus and the battle render a table's entries into
# (window_render_text_line's 0x39 columns; where a menu's image is narrower
# the language's build shows what fits it). A window a script sizes, in the
# field or the battle, holds every language to what it shows.
OWN_LAYOUT = {"en-x-pw"}
SCREEN_COLUMNS = 70
ANY_ROWS = 255
LINE_COLUMNS = 0x39


def place_key(raw: bytes, index: int, offset: int, count: int) -> int:
    """A table entry's name (src/port/locale.c `xem_locale_place`): the hash
    of its text carried on over the entry's index, its offset in its table
    and the table's count of entries, two bytes each."""
    return fnv1a64(raw + struct.pack("<HHH", index & 0xFFFF, offset, count))


@dataclass
class Place:
    """One table entry holding a text, and its window there."""
    where: str
    disc: int
    key: str = ""  # place_key, 16 hex digits (none for a menu's label or letter row)
    columns: int = 0  # the widest line a translation may have there
    rows: int = 0  # the most lines a page may have there
    choice: tuple[int, int] | None = None
    fixed: bool = False
    own: tuple[int, int] = (0, 0)  # the columns and rows a language with its own layout may take there, if more


@dataclass
class Text:
    where: str
    hash: str
    markup: str
    columns: int  # the widest line a translation may have
    rows: int  # the most lines a page may have
    group: str
    places: list[Place] = field(default_factory=list)
    choice: tuple[int, int] | None = None  # field choice lines (first, last)
    fixed: bool = False  # the script sets the window's size
    original_columns: int = 0
    original_rows: int = 0
    small: str = ""  # "word" or "row": spelled with the menus' small font (SMALL)
    own: tuple[int, int] = (0, 0)  # the least of its places' `Place.own`

    def at(self, place: Place) -> "Text":
        """The text as one of its entries shows it: that entry's window."""
        return Text(place.where, self.hash, self.markup, place.columns, place.rows, self.group, [place], place.choice,
                    place.fixed, self.original_columns, self.original_rows, self.small, place.own)


# A field script places and sizes the running actor's windows
# (field_event_set_window_layout and field_event_set_window_layout_operands,
# cf and d0): a left, a top, a width of 3 columns a unit and rows, each left
# as the game computes it where the operand is 0; d0 gives the windows' style
# too. The actor's speaker and centred messages take them
# (field_event_open_message_window modes 0 and 3) until it sets others or
# clears them (f4 with a non-zero byte); they outlast the script that set
# them. A window given a width shows a speaker with a portrait 0x11 columns
# fewer (the text begins past the portrait in a frame no wider), and one
# given rows shows a portrait's 4. An actor has a portrait once it takes a
# character (fe 0d; fc takes the actor's it speaks as), unless style bit 2,
# the message's own where its style byte is not 0, shows none.
UNSIZED = (0, 0)
VARIABLE = -1  # a size an event variable gives
PORTRAIT_COLUMNS = 0x11
NO_PORTRAIT = 2
NO_CHARACTER = 0xFC  # fe 0d's operand for none (field_event_resolve_character)
SPEAKER_MESSAGES = {0xD2: (0, 1), 0xF5: (0, 1), 0xD4: (1, 2), 0xFC: (1, 2)}  # opcode: its message and style operands
BOXED_MESSAGES = {0x03: 0, 0xD3: 0}


def window_size(instruction) -> tuple[int, int, int | None] | None:
    """The (columns, rows, style) an instruction gives its actor's windows,
    if it gives any: 0 for a size it leaves to the game, VARIABLE for a
    variable's, and no style where it keeps the one before."""
    if instruction.extended is not None:
        return None
    operands = instruction.operands
    if instruction.opcode == 0xCF:
        return 3 * operands[2], operands[3], None
    if instruction.opcode == 0xD0:
        columns, rows, style = ((value & 0x7FFF) if value & 0x8000 else VARIABLE for value in operands[2:5])
        return 3 * columns if columns > 0 else columns, rows, style & 0xFF if style >= 0 else None
    if instruction.opcode == 0xF4 and operands[0] != 0:
        return 0, 0, 0
    return None


def takes_character(instruction) -> bool | None:
    """Whether an instruction leaves its actor with a character, whose
    portrait its windows show, if it says."""
    if instruction.extended == 0x0D:  # set_character
        value = instruction.operands[0]
        return not (value & 0x8000 and value & 0x7FFF == NO_CHARACTER)
    if instruction.extended is None and instruction.opcode in (0x16, 0xFC):  # become_party_character, message_as_actor
        return True
    return None


def sized_windows(package) -> dict[int, set[tuple[int, int, bool]]]:
    """Per message, the windows its actor's scripts may have sized when a
    speaker or centred instruction shows it: (columns, rows, whether a
    portrait may show), a size 0 where the game computes it (as where no
    script sizes the window, which is among them where that may be so) and
    VARIABLE where a variable gives it. A message no script sizes is absent.

    Each actor's scripts are followed from its events' entries; each
    instruction passes on the sizes, style and character that reach it, a
    call's return whatever its routine may leave. An event starts as the
    actor was made, without a size or a character, or as a script of the
    actor ends (they outlast the script; the scripts clear a size once its
    message is dismissed, so none is taken to stand while another event of
    the actor waits)."""
    from tools.analysis import events

    starts, _ = events.script_entries(package)
    whole = events.walk(package.bytecode, [pc for _, _, pc in starts])
    if all((window_size(instruction) or UNSIZED)[:2] == UNSIZED for instruction in whole.instructions.values()):
        return {}
    entries: dict[int, list[int]] = defaultdict(list)
    for actor, _, pc in starts:
        entries[actor].append(pc)
    sized: dict[int, set[tuple[int, int, bool]]] = defaultdict(set)
    for actor_entries in entries.values():
        found = events.walk(package.bytecode, actor_entries)
        given = {pc: size for pc, instruction in found.instructions.items() if (size := window_size(instruction)) is not None}
        if all(size[:2] == UNSIZED for size in given.values()):
            continue
        faces = {pc: face for pc, instruction in found.instructions.items() if (face := takes_character(instruction)) is not None}
        returns = [pc + instruction.size for pc, instruction in found.instructions.items() if instruction.spec.flow == "call"]
        ends = [pc for pc, instruction in found.instructions.items() if instruction.spec.flow in ("end", "return")]

        def leaving(pc, state):  # the state (columns, rows, style, character) after the instruction
            columns, rows, style, face = state
            if pc in given:  # a cf keeps the style that reached it
                columns, rows = given[pc][:2]
                style = given[pc][2] if given[pc][2] is not None else style
            return columns, rows, style, faces.get(pc, face)

        reaching: dict[int, set] = defaultdict(set)
        standing = {(0, 0, 0, False)}  # what an event may start with
        while True:
            pending = []
            for pc in actor_entries:
                reaching[pc] |= standing
                pending.append(pc)
            while pending:
                pc = pending.pop()
                instruction = found.instructions.get(pc)
                if instruction is None:
                    continue
                after = {leaving(pc, state) for state in reaching[pc]}
                for successor in returns if instruction.spec.flow == "return" else found.edges.get(pc, ()):
                    if not after <= reaching[successor]:
                        reaching[successor] |= after
                        pending.append(successor)
            left = {leaving(pc, state) for pc in ends for state in reaching[pc]}
            if left <= standing:
                break
            standing |= left
        for pc, instruction in found.instructions.items():
            if instruction.extended is not None or instruction.opcode not in SPEAKER_MESSAGES:
                continue
            message, own = (instruction.operands[at] for at in SPEAKER_MESSAGES[instruction.opcode])
            if all(state[:2] == UNSIZED for state in reaching[pc]):
                continue
            for columns, rows, style, face in reaching[pc]:
                face = faces.get(pc, face)  # fc speaks as an actor, with its portrait
                sized[message].add((columns, rows, face and not (own if own else style or 0) & NO_PORTRAIT))
    return dict(sized)


def field_window(pair, widest: int, lines: int, sizes, boxed: bool, speaker: bool) -> tuple[int, int, bool]:
    """The columns and rows a field message's translation may take, and
    whether a script sizes its window: `pair` its table's (columns, rows),
    `widest` and `lines` the original text's own, `sizes` the windows its
    scripts may have sized (`sized_windows`), `boxed` and `speaker` the ways
    they show it.

    Each way gives a window, and the tightest holds (a message no reachable
    script shows: a speaker's). A window its table sizes takes what a
    translation needs. One a script may have sized shows its columns and
    rows, less what a portrait takes where one may show; one a variable
    sizes, no more than the original's own text. A size the original's own
    text passes is not the window's when it shows."""
    table = (max(pair[0], FIELD_COLUMNS), max(pair[1], FIELD_ROWS) if lines <= FIELD_ROWS else max(pair[1], lines))
    windows, sized = [], False
    if boxed:
        windows.append(BOX)
    for columns, rows, portrait in sizes or ():
        if columns > 0 and widest > columns or rows > 0 and lines > rows and not (portrait and lines <= FIELD_ROWS):
            continue
        if (columns, rows) != UNSIZED:
            sized = True
        if columns > 0 and portrait and widest <= columns - PORTRAIT_COLUMNS:
            columns -= PORTRAIT_COLUMNS
        if rows > 0 and portrait:
            rows = min(rows, FIELD_ROWS)
        windows.append((table[0] if columns == 0 else max(columns, 0), table[1] if rows == 0 else max(rows, 0)))
    if not sized and (speaker or not windows):
        windows.append(table)
    return min(w[0] for w in windows), min(w[1] for w in windows), sized


def field_scripts(disc) -> dict[int, dict]:
    """Per map: messages offered as a choice (message -> (first, last)),
    messages a script may show in a window it sized (message -> the sizes,
    `sized_windows`) and the messages each window mode shows (`boxed`: d3 and
    03, whose window is 0x48 x 4 whatever the table says; `speaker`: d2, d4,
    fc and f5, which take the actor's layout), from the event scripts: a9
    offers the lines of the message the running script showed last (in PC
    order)."""
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
        choices, boxed, speaker = {}, set(), set()
        last = None
        for pc in sorted(found.instructions):
            ins = found.instructions[pc]
            if ins.extended is not None:
                continue
            op = ins.opcode
            if op in BOXED_MESSAGES:
                last = ins.operands[BOXED_MESSAGES[op]]
                boxed.add(last)
            elif op in SPEAKER_MESSAGES:
                last = ins.operands[SPEAKER_MESSAGES[op][0]]
                speaker.add(last)
            elif op == 0xA9 and last is not None:
                value = ins.operands[0]
                choices[last] = (value >> 4, value & 15)
        result[map_id] = {"choices": choices, "fixed": sized_windows(package), "boxed": boxed, "speaker": speaker}
    return result


# A battle event script's message (ovl3087 battle_event_script_show_message)
# takes the window its script laid out last (opcode 1a: x, y, width in
# 12-pixel units, rows; 0 the default's 16 and 8), which returns to the
# default once the message is dismissed. The text has 3 columns a unit, and
# the frame shows 4 rows where the layout's are 5 or more.
BATTLE_WINDOW = (16, 8)
BATTLE_ROWS = 4


def battle_windows(disc) -> dict[int, dict[int, tuple[int, int]]]:
    """Per battle event script set, the columns and rows each message's
    window shows: of the layout before it in the bytecode since the message
    before (the tightest where a message is shown twice). A message whose
    layout a variable gives, or whose script does not read, is absent, and
    is held to its own text."""
    from tools.analysis import battle_event_vm as vm
    from tools.analysis import text_control as tc

    archive = disc.sectors(disc.slot(0x20, 0, 2))
    out: dict[int, dict[int, tuple[int, int]]] = {}
    for number in range(struct.unpack_from("<I", archive, 0)[0] // 2):
        try:
            script = vm.parse_script(tc.archive_entry(archive, 2 * number, packed=True))
            listing = vm.disassemble(script.code, vm.entry_points(script))
        except (vm.EventError, tc.TextError):
            continue
        windows: dict[int, tuple[int, int] | None] = {}
        layout: tuple[int, int] | None = BATTLE_WINDOW
        for offset in sorted(listing.instructions):
            instruction = listing.instructions[offset]
            operands = dict(instruction.operands)
            if instruction.name == "window":
                given = [operands[name] for name in ("width", "height")]
                layout = None
                if all(value.startswith("#") for value in given):  # immediates
                    layout = tuple(int(value[1:], 16) or default for value, default in zip(given, BATTLE_WINDOW))
            elif instruction.name in ("message", "message_from"):
                message = int(operands["message"], 16)
                shown = (3 * layout[0], BATTLE_ROWS if layout[1] > BATTLE_ROWS else layout[1]) if layout else None
                if message in windows and (shown is None or windows[message] is None):
                    shown = None
                elif message in windows:
                    shown = (min(shown[0], windows[message][0]), min(shown[1], windows[message][1]))
                windows[message] = shown
                layout = BATTLE_WINDOW
        out[number] = {message: shown for message, shown in windows.items() if shown is not None}
    return out


# The menus' command labels (src/port/locale.c): words drawn in the menu's
# textures, sheet entries 0x123-0x149 of its sprite sheet (directory (0x10, 0)
# file 1, archive entry 2), named by the hash of each entry's bytes (its part
# count, then 0x1C bytes a part). Their English is a picture, not text; a
# translation is one line the port draws with the game's text renderer, at
# most LABEL_COLUMNS columns.
LABELS = range(0x123, 0x14A)
LABEL_COLUMNS = 59
# The words the menus compose of their small font's letter cells, one part a
# letter (Attack, Hit%, Defense, Evade%, Ether, EthDef., Weight, Agility,
# fuel, Response, EthAmp, maxHeat): a translation respells them with the same
# cells, in no more letters than the original has parts, a closing % keeping
# the original's (src/port/locale.c).
WORDS = (0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE4, 0xE6, 0xEB, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8)
# The small font's cells, sheet entries 0x00-0x3E, and what a translation of
# a word or row may hold: its letters and digits, the full stop (0x91), a
# word's closing % and a row's spaces.
SMALL_CELLS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz/"
SMALL = {"word": re.compile(r"[A-Za-z0-9.]+%?"), "row": re.compile(r"[A-Za-z0-9.]+( [A-Za-z0-9.]+)*")}
# The menus' rows of single letters in overlay tables, a sheet image (a small
# font cell) and an x a slot, 8 pixels apart: a translation is spelled from
# the row's first x in no more letters than the row has slots (src/port/locale.c).
OVERLAY_VRAM = 0x801C5000  # slot39's and ovl2602's load address
LETTER_ROWS = (  # overlay, images, their x, entry size, first slot, slots
    ("slot39", 0x801EA34C, 0x801E9A78, 4, 10, 8),  # the field menu's Next LV
    ("slot39", 0x801EA39C, 0x801E9B60, 4, 5, 8),  # the status panel's
    ("slot39", 0x801EA39C, 0x801E9B60, 4, 24 + 5, 8),  # the gear panel's
    ("ovl2602", 0x801D708C, 0x801D70A8, 2, 3, 4),  # the gear shop's FUEL
    ("ovl2602", 0x801D708C, 0x801D70A8, 2, 8, 6),  # and WEIGHT
)


# The party's and the Gears' names (game_data.names, 31 of 20 bytes, which
# mode_load_initial_game_data fills from directory (0x10, 0) file 3 and a save
# brings back): text a window takes from the game data itself, so the port
# names slot n as entry n at 20 n of a table of 31 (src/port/locale.c
# `xem_locale_name`). A name is the player's to change; a language respells
# one only as the game first gives it, in ten glyphs' columns.
NAME_SLOTS = 31
NAME_BYTES = 20
NAME_COLUMNS = 20


def initial_names(disc) -> list[bytes]:
    from tools.analysis import text_control as tc

    pairs = tc.archive_entry(tc.system_data(disc), 27)
    return tc.initial_names(disc.data(disc.slot(0x10, 0, 3)), pairs)


def menu_labels(disc) -> list[tuple[int, bytes]]:
    from tools.analysis import text_control as tc

    sheet = tc.archive_entry(disc.sectors(disc.slot(0x10, 0, 1)), 2, packed=True)
    out = []
    for label in (*LABELS, *WORDS):
        offset = struct.unpack_from("<H", sheet, 4 + 2 * label)[0]
        parts = struct.unpack_from("<h", sheet, offset)[0]
        out.append((label, sheet[offset : offset + 4 + 0x1C * parts]))
    return out


def letter_rows(disc) -> list[tuple[int, bytes, str, int]]:
    """Each letter row's number, its slots' images (the bytes the port names
    it by), its English and its slots."""
    from tools.extraction.overlays import OVERLAYS

    out = []
    for number, (overlay, images, xs, size, first, slots) in enumerate(LETTER_ROWS):
        image = disc.data(OVERLAYS[overlay][disc.number - 1])
        form = "<i" if size == 4 else "<H"

        def entry(table, slot):
            return struct.unpack_from(form, image, table - OVERLAY_VRAM + size * slot)[0]

        start = images - OVERLAY_VRAM + size * first
        placed = sorted({entry(xs, slot): SMALL_CELLS[entry(images, slot)] for slot in range(first, first + slots)}.items())
        english = placed[0][1]
        for (last, _), (x, char) in zip(placed, placed[1:]):
            english += " " * ((x - last) // 8 - 1) + char
        out.append((number, image[start : start + size * slots], english, slots))
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
        battles = battle_windows(disc)
        for label, data in menu_labels(disc):
            digest = f"{fnv1a64(data):016x}"
            columns = LABEL_COLUMNS if label in LABELS else 2 * struct.unpack_from("<h", data, 0)[0]
            text = texts.setdefault(digest, Text(f"mc{label:03d}-0000", digest, "", columns, 1, GROUPS["mc"],
                                                 small="word" if label in WORDS else ""))
            text.places.append(Place(f"mc{label:03d}-0000", number, columns=columns, rows=1))
        for row, data, english, slots in letter_rows(disc):
            digest = f"{fnv1a64(data):016x}"
            text = texts.setdefault(digest, Text(f"mr{row:03d}-0000", digest, english, 2 * slots, 1, GROUPS["mr"],
                                                 small="row"))
            text.places.append(Place(f"mr{row:03d}-0000", number, columns=2 * slots, rows=1))
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
                entry = f"{place_key(raw, index, offset, len(offsets)):016x}"
                script = battles.get(item, {}).get(index) if prefix == "be" else scripts.get(item)
                entries.append((number, prefix, item, index, where, raw, markup, shape, pair, script, entry))
            key = (prefix, item)
            table_columns[key] = max(table_columns.get(key, 0), widest)
        for slot, raw in enumerate(initial_names(disc)):
            where = f"nm000-{slot:04d}"
            markup = render(raw)
            shape = pages(tokens(markup))
            entry = f"{place_key(raw, slot, NAME_BYTES * slot, NAME_SLOTS):016x}"
            entries.append((number, "nm", 0, slot, where, raw, markup, shape, None, None, entry))
        table_columns["nm", 0] = NAME_COLUMNS
    for number, prefix, item, index, where, raw, markup, shape, pair, script, entry in entries:
        digest = f"{fnv1a64(raw):016x}"
        lines = max(len(p) for p in shape)
        widest = max(max(p) for p in shape)
        choice = fixed = None
        own = (0, 0)
        if prefix == "f":
            script = script or {}
            choice = script.get("choices", {}).get(index)
            fixed = script.get("fixed", {}).get(index)
            columns, rows, sized = field_window(pair, widest, lines, fixed, index in script.get("boxed", ()),
                                                index in script.get("speaker", ()))
            fixed = fixed if sized else None
            if choice is not None:
                rows = lines
            window = (max(columns, widest), max(rows, lines))
            if fixed is None:
                own = (BOX[0] if index in script.get("boxed", ()) else SCREEN_COLUMNS, ANY_ROWS)
        elif prefix == "be":  # the script lays out each message's window
            window = (max(script[0], widest), max(script[1], lines)) if script else (widest, lines)
        else:
            window = (table_columns[(prefix, item)], lines)
            if prefix != "nm":
                own = (LINE_COLUMNS, 0)
        text = texts.get(digest)
        if text is None:
            text = texts[digest] = Text(where, digest, markup, window[0], window[1], GROUPS[prefix],
                                        original_columns=widest, original_rows=lines, own=own)
        else:  # the tightest window any of its places gives
            text.columns = min(text.columns, window[0])
            text.rows = min(text.rows, window[1])
            text.own = (min(text.own[0], own[0]), min(text.own[1], own[1]))
        text.places.append(Place(where, number, entry, window[0], window[1], choice, fixed is not None, own))
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
    """Texts worth a translation: some letter, and no kana; a name alone
    (one no table holds) is the player's."""
    return (bool(re.search(r"[A-Za-z]", re.sub(r"\{[^}]*\}", "", text.markup))) and not kana(text.markup)
            and text.group != GROUPS["nm"])


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

# A shipped line, `<where> <hash>[@<entry>] [<translation>]`, and a translator's,
# `<where> [<flags>] [<translation>]`: the flag `here` translates that entry
# alone (the shipped line's @<entry>).
LINE_RE = re.compile(r"^(?P<where>[a-z]+\d{3}-\d{4}) (?P<hash>[0-9a-f]{16})(?:@(?P<entry>[0-9a-f]{16}))?(?: (?P<text>.*))?$")
WORK_RE = re.compile(r"^(?P<where>[a-z]+\d{3}-\d{4})(?: \[(?P<flags>[^\]]*)\])?(?: (?P<text>.*))?$")
HERE = "here"


def language_characters(language: str) -> Characters:
    """The characters a language adds to the font, from its glyphs.txt
    (`<char> <base> <accent> ...` lines): two-byte glyphs 0xFF20 on in file
    order (a second byte past the controls' 00-0F), those marked `wide` 3
    columns wide."""
    path = SHIPPED / language / "glyphs.txt"
    extra, wide = {}, set()
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields and not line.startswith("#") and fields[0] != "accent":
            extra[fields[0]] = EXTRA_GLYPHS + len(extra)
            if "wide" in fields[3:]:
                wide.add(fields[0])
    return Characters(extra, wide)


def limits(original: Text, own_layout: bool = False) -> tuple[int, int]:
    """The widest line and the most lines a page of a translation may have:
    the original's window, or what a language that brings its own layout
    may take there (`Place.own`) if more."""
    if own_layout:
        return max(original.columns, original.own[0]), max(original.rows, original.own[1])
    return original.columns, original.rows


def problems(original: Text, translation: str, extra: dict[str, int], own_layout: bool = False) -> list[str]:
    """Why a translation does not fit its original's windows, if it does not.
    A language with its own layout (OWN_LAYOUT) sets its own waits,
    insertions and pages."""
    try:
        new = tokens(translation, extra)
    except MarkupError as error:
        return [str(error)]
    old = tokens(original.markup)
    found = []

    def sequence(toks, names):
        return [(t.kind, t.operands) for t in toks if t.kind in names]

    old_pages, new_pages = pages(old), pages(new)
    if not own_layout:
        if sequence(new, TIMING) != sequence(old, TIMING):
            found.append(f"timing controls {sequence(old, TIMING)} became {sequence(new, TIMING)}")
        inserts = tuple(INSERT_COLUMNS)
        if sorted(sequence(new, inserts)) != sorted(sequence(old, inserts)):
            found.append(f"insertions {sequence(old, inserts)} became {sequence(new, inserts)}")
        if len(new_pages) != len(old_pages) and not (may_add_pages(original) and len(new_pages) > len(old_pages)):
            found.append(f"{len(old_pages)} page(s) became {len(new_pages)}")
    if original.choice and new_pages and old_pages and len(new_pages[-1]) != len(old_pages[-1]):
        found.append(f"the choice's last page has {len(old_pages[-1])} lines, not {len(new_pages[-1])}")
    columns, rows = limits(original, own_layout)
    for page in new_pages:
        if len(page) > rows:
            found.append(f"a page of {len(page)} lines (at most {rows})")
            break
    if original.small and not SMALL[original.small].fullmatch(translation):
        found.append(f"not spelled with the small font's letters, digits and full stop ({SMALL[original.small].pattern})")
    widest = max(max(p) for p in new_pages)
    if widest > columns:
        found.append(f"a line of {widest} columns (at most {columns}, {columns // 2} letters)")
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
    return sum(t.columns if t.kind == "glyph" else INSERT_COLUMNS.get(t.kind, 0) for t in toks)


def may_add_pages(original: Text) -> bool:
    """A field message the player's Confirm ends, without a choice, in a
    window no script sized."""
    return (original.group == "field messages" and not original.choice and not original.fixed
            and not any(t.kind in ENDS_WITHOUT_CONFIRM or t.kind == "pause" for t in tokens(original.markup)))


# Marks a line never begins with: French sets them after a space ("Quoi ?"),
# which stays with the word before it.
NO_BREAK_BEFORE = "?!:"


def wrap(original: Text, translation: str, extra: dict[str, int], own_layout: bool = False) -> str:
    """The translation with every line too wide for its window broken at its
    spaces (never before one of NO_BREAK_BEFORE, and keeping its indent), and
    every page with more lines than the window's rows split into pages of
    even length where a page may be added."""
    columns, rows = limits(original, own_layout)
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
            if width(line) <= columns:
                lines.append(line)
                continue
            # The line's indent stays: its leading spaces, and a control that
            # shows nothing before them (a speed set ahead of a choice's lines).
            indent, lead = [], 0
            while lead < len(line) and (space(line[lead]) or line[lead].kind != "glyph" and line[lead].kind not in INSERT_COLUMNS):
                lead += 1
            if any(space(token) for token in line[:lead]):
                indent, line = line[:lead], line[lead:]
            words, current = [], []
            for at, token in enumerate(line):
                follows = line[at + 1] if at + 1 < len(line) else None
                if space(token) and not (current and follows is not None and follows.kind == "glyph"
                                         and follows.char in NO_BREAK_BEFORE):
                    if current:
                        words.append(current)
                    current = []
                else:
                    current.append(token)
            if current:
                words.append(current)
            row = list(indent)
            for word in words:
                if row and row != indent and width(row) + 2 + width(word) > columns:
                    lines.append(row)
                    row = []
                if row and row != indent:
                    row.append(glyph(CODES[" "], " "))
                row.extend(word)
            lines.append(row)
        if len(lines) > rows and may_add_pages(original):
            count = -(-len(lines) // rows)
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
    def pair(value):
        return tuple(value) if value else None

    def place(d):
        return Place(**{**d, "choice": pair(d["choice"]), "own": tuple(d["own"])})

    return {t.where: t for t in (Text(**{**d, "choice": pair(d["choice"]), "own": tuple(d["own"]), "places": [place(p) for p in d["places"]]})
                                 for d in json.loads((LOCAL / "en.json").read_text()))}


def entries_by_where(texts) -> dict[str, tuple[Text, list[Place]]]:
    """Every table entry's text and its places (one a disc that holds it)."""
    out: dict[str, tuple[Text, list[Place]]] = {}
    for text in texts:
        for place in text.places:
            if place.key:
                out.setdefault(place.where, (text, []))[1].append(place)
    return out


def at_entry(text: Text, places: list[Place]) -> Text:
    """The text in the tightest of one entry's windows (the discs agree but
    for a script one of them changes)."""
    first = text.at(places[0])
    for place in places[1:]:
        other = text.at(place)
        first.columns, first.rows = min(first.columns, other.columns), min(first.rows, other.rows)
        first.own = (min(first.own[0], other.own[0]), min(first.own[1], other.own[1]))
        first.choice, first.fixed = first.choice or other.choice, first.fixed or other.fixed
    return first


def lint(args) -> int:
    """Check translators' files: each line wrapped as `merge` wraps it, and
    what still does not fit, with the wrapped text."""
    texts = originals_by_where()
    entries = entries_by_where(texts.values())
    extra = language_characters(args.language)
    own = args.language in OWN_LAYOUT
    bad = 0
    batches: dict[str, set[str]] = defaultdict(set)
    for path in args.files:
        work = read_work([Path(path)])
        batch = re.match(r"\d+", Path(path).name)
        if batch:
            batches[batch[0]] |= {where for where, _ in work}
        for (where, here), translation in work.items():
            original = at_entry(*entries[where]) if here and where in entries else None if here else texts.get(where)
            if original is None:
                print(f"{where}: no such {'entry' if here else 'text'}")
                bad += 1
                continue
            try:
                wrapped = wrap(original, translation, extra, own)
            except MarkupError as error:
                print(f"{where}: {error}")
                bad += 1
                continue
            found = problems(original, wrapped, extra, own)
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


def read_work(paths) -> dict[tuple[str, bool], str]:
    """Translators' lines: (where, here) and the translation, `here` for a
    line flagged [here], which translates that entry alone."""
    out = {}
    for path in paths:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if not line.strip():
                continue
            match = WORK_RE.match(line)
            if not match:
                raise SystemExit(f"{path}:{number}: not `<where> <translation>`: {line[:60]!r}")
            out[match["where"], HERE in (match["flags"] or "").split()] = match["text"] or ""
    return out


def shipped_line(where: str, digest: str, translation: str, entry: str = "") -> str:
    return " ".join(part for part in (where, digest + (f"@{entry}" if entry else ""), translation) if part)


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
    return {"s": "system.txt", "ml": "menus.txt", "mw": "menus.txt", "md": "menus.txt", "mm": "menus.txt", "mc": "menus.txt", "mr": "menus.txt",
            "nm": "menus.txt", "ba": "battle.txt", "en": "battle.txt", "bm": "battle.txt", "be": "battle.txt",
            "wa": "world.txt"}[prefix]


def merge(args) -> int:
    texts = originals_by_where()
    entries = entries_by_where(texts.values())
    extra = language_characters(args.language)
    own = args.language in OWN_LAYOUT
    work = read_work([Path(f) for f in args.files] if args.files else sorted((LOCAL / args.language).glob("[0-9]*.txt")))
    bad = 0
    files: dict[str, list[str]] = defaultdict(list)
    alone: dict[str, tuple[str, str]] = {}  # an entry's name: where it was translated alone, and how
    for (where, here), translation in sorted(work.items()):
        original = at_entry(*entries[where]) if here and where in entries else None if here else texts.get(where)
        if original is None:
            print(f"{where}: no such {'entry' if here else 'text'}")
            bad += 1
            continue
        try:
            translation = wrap(original, translation, extra, own)
        except MarkupError:
            pass
        found = problems(original, translation, extra, own)
        if found:
            bad += 1
            print(f"{where}: {'; '.join(found)}")
            if not args.keep_bad:
                continue
        if here:  # the entry alone, by each name the discs give it (one name for the same entry of two tables alike)
            for entry in sorted({place.key for place in entries[where][1]}):
                first, known = alone.setdefault(entry, (where, translation))
                if known != translation:
                    bad += 1
                    print(f"{where}: the same entry as {first}, which reads otherwise")
                elif first == where:
                    files[file_for(where)].append(shipped_line(where, original.hash, translation, entry))
            continue
        if original.markup and encode(translation, extra) == encode(original.markup):
            files[SAME].append(f"{where} {original.hash}")
            continue
        files[file_for(where)].append(shipped_line(where, original.hash, translation))
    missing = [t.where for t in texts.values() if translatable(t) and (t.where, False) not in work]
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
    own = args.language in OWN_LAYOUT
    seen, entries, bad = set(), set(), 0
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
            if match["entry"]:  # the text in one table entry: its window there
                places = [place for place in original.places if place.key == match["entry"] and place.where == match["where"]]
                if not places:
                    print(f"{name}:{number}: {match['where']}: no entry there holds the text as {match['entry']}")
                    bad += 1
                    continue
                if match["entry"] in entries:
                    print(f"{name}:{number}: {match['where']}: entry {match['entry']} again")
                    bad += 1
                entries.add(match["entry"])
                original = at_entry(original, places)
            else:
                if match["hash"] in seen:
                    print(f"{name}:{number}: {match['where']}: hash {match['hash']} again")
                    bad += 1
                seen.add(match["hash"])
            found = problems(original, match["text"] or "", extra, own)
            if found:
                print(f"{name}:{number}: {match['where']}: {'; '.join(found)}")
                bad += 1
    same = same_as_original(args.language)
    for digest in same - set(texts):
        print(f"{SAME}: no text of both discs has hash {digest}")
        bad += 1
    for digest in same & seen:
        print(f"{SAME}: {texts[digest].where} is translated and listed as the same")
        bad += 1
    missing = [t for t in texts.values() if translatable(t) and t.hash not in seen and t.hash not in same]
    print(f"{len(seen)} translations checked, {len(entries)} in one entry alone, {len(same)} the same as the original, "
          f"{bad} problems, {len(missing)} texts untranslated")
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
