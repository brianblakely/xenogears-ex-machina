"""Message text and its control codes, from the recovered window code.

window_reveal_text (decomp/src/resident/main2.c) reveals a window's text. A byte
below the font's two-byte threshold (text_font_two_byte_threshold, font halfword 2, installed by
text_install_font) is a one-byte glyph and a byte at or above it starts a two-byte
glyph, unless it is one of the controls 00, 01, 02, 03 or 0F; 0F takes a
sub-code dispatched through the 16-entry jump table at 0x80018A7C (switch cases
0-15). A sub-code of 16 or more matches no case and leaves the pointer on the
0F, so the window never advances: such text is undecodable. Controls that
insert text (0F 03-08, 09/0A/0C, 0F) set the window's resume pointer to their
last operand; the inserted text's 00 returns to the byte after it.

Text lives in tables read by text_get_resource_entry: entry n starts at the u16 offset
at byte 4 + 2n. Every located table also holds its count at +0, 0 at +2, count
+ 1 offsets (the last ends the text) and count (columns, rows) byte pairs
after them (text_get_message_columns/text_get_message_rows read the pairs). Windows get text only
through window_queue_message (queue), window_render_text_line (one-line layout) and the
insertions; their callers pass entries of these tables, or text built at run
time from character codes through the byte pairs of system resource 27
(text_decode_codes_to_buffer/text_decode_codes: numbers, names, name entry). The sweep reads
every table those callers use, classifies the pairs and decodes the initial
names (mode_load_initial_game_data).

    python3 -m tools.analysis.text_control --sweep    # both discs, aggregate only
    python3 -m tools.analysis.text_control --list field --item 3 --disc 2

`--list GROUP` prints one group's tables (`--item`: one table, a field map's
number, an archive entry or a file as the sweep names them) to stdout; keep
listings under .local/. Each entry shows its offset, (columns, rows) and
tokens: one-byte glyphs as two hex digits, two-byte glyphs as four, controls
by mnemonic in brackets; text no entry reaches follows as `unreached`.

`--chars` prints the glyphs whose characters the disc's own data gives as
those characters and every other glyph as {hex} (character_map): the digits,
signs and blank of the number code, the letters of the memory card title
lines that recur as whole texts, and the other letters of the name entry
grid's two alphabet runs, whose places those letters confirm. There is no
font map in this module; the limits are in docs/scripts/text-control.md.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field

from tools.analysis.disc_index import Disc, discs
from tools.analysis.packed import PackedError, decode_block
from tools.extraction.overlays import OVERLAYS


@dataclass(frozen=True)
class Control:
    code: int  # first byte, or 0x0F00 | sub-code
    mnemonic: str
    operands: tuple[str, ...]
    length: int  # bytes the window pointer passes, the code included
    source: str
    effect: str


CONTROLS = {
    0x00: Control(
        0x00,
        "end",
        (),
        1,
        "window_reveal_text byte 0",
        "end of text: return after an inserted text's control; else state 1 (unk6B), flag 8 "
        "(wait) and unk6C, so once the wait ends the window moves on to its next queued "
        "text; the pointer stays on the 00",
    ),
    0x01: Control(
        0x01,
        "newline",
        (),
        1,
        "window_reveal_text byte 1",
        "x = 100 and end this step, so the next step starts a new line",
    ),
    0x02: Control(
        0x02,
        "page",
        (),
        1,
        "window_reveal_text byte 2",
        "state 2, flags 0x48: wait, then clear the window; a following 01 is skipped",
    ),
    0x03: Control(
        0x03, "pause", (), 1, "window_reveal_text byte 3", "state 3, flag 8: wait, keeping the lines"
    ),
}
_EXTENDED = (
    (0x00, "wait", ("frames",), 3, "wait `frames` (unk84) and end this step"),
    (
        0x01,
        "speed",
        ("speed",),
        3,
        "glyphs per step = speed, saving the old speed; 0 restores the saved speed",
    ),
    (
        0x02,
        "wait_done",
        ("frames",),
        3,
        "wait `frames`; the following step only clears window flag 4 (unk6C), so the window "
        "takes its next queued text",
    ),
    (
        0x03,
        "insert",
        ("resource", "entry"),
        4,
        "insert entry `entry` of system resource `resource`",
    ),
    (
        0x04,
        "insert_selection",
        (),
        2,
        "insert entry selection & 0xFF of resource 22, 23, 17, 51 or 50 for selection kinds "
        "0x000-0x400 (another kind: the 04 is read as text)",
    ),
    (
        0x05,
        "insert_name",
        ("name",),
        3,
        "insert character name `name` (0x80 and up through D_8006F2E8; slot 0xFF: "
        "resource 26 entry 0)",
    ),
    (0x06, "insert_23", ("entry",), 3, "insert entry `entry` of system resource 23"),
    (0x07, "insert_24", ("entry",), 3, "insert entry `entry` of system resource 24"),
    (0x08, "insert_25", ("entry",), 3, "insert entry `entry` of system resource 25"),
    (0x09, "insert_number", ("value",), 3, "insert window value `value` in decimal, palette 0"),
    (0x0A, "insert_number_1", ("value",), 3, "insert window value `value` in decimal, palette 1"),
    (
        0x0B,
        "set_6d",
        ("value",),
        2,
        "window byte 0x6D = value; the pointer stops on the operand, which is read again as text",
    ),
    (
        0x0C,
        "insert_signed",
        ("value",),
        3,
        "insert window value `value` as signed decimal, palette 1",
    ),
    (
        0x0D,
        "wait_done_skippable",
        ("frames",),
        3,
        "as wait_done, also setting window flag 0x200 (800345e0 then drops the wait and the "
        "pending clear)",
    ),
    (
        0x0E,
        "slow",
        ("frames",),
        3,
        "one glyph per step every `frames` frames (unk86/unk88), saving the speed",
    ),
    (
        0x0F,
        "insert_button",
        ("action",),
        3,
        "insert the name of the button assigned to `action` (pad_button_assignment) from resource 49",
    ),
)
for _sub, _name, _operands, _length, _effect in _EXTENDED:
    CONTROLS[0x0F00 | _sub] = Control(
        0x0F00 | _sub, _name, _operands, _length, f"window_reveal_text 0F case {_sub}", _effect
    )
EXTENDED_TABLE = 0x80018A7C  # jump table of the 0F sub-codes
INTERPRETER = (0x80033DF0, 0x800345E0)  # window_reveal_text up to window_end_wait


def check_jump_table(exe: bytes) -> list[str]:
    """Differences between the boot program's 0F jump table and the 16 cases."""
    base = struct.unpack_from("<I", exe, 0x18)[0]  # PS-X EXE text address, after a 0x800 header
    offset = EXTENDED_TABLE - base + 0x800
    targets = struct.unpack_from("<17I", exe, offset)
    problems = [
        f"0F case {sub} target {target:08x} outside window_reveal_text"
        for sub, target in enumerate(targets[:16])
        if not INTERPRETER[0] <= target < INTERPRETER[1]
    ]
    if INTERPRETER[0] <= targets[16] < INTERPRETER[1]:
        problems.append("the jump table has more than 16 entries")
    return problems


class TextError(ValueError):
    def __init__(self, offset: int, reason: str):
        self.offset, self.reason = offset, reason
        super().__init__(f"+0x{offset:x}: {reason}")


@dataclass(frozen=True)
class Token:
    offset: int
    code: int  # glyph: its code ((first << 8) | second for two bytes); control: Control.code
    mnemonic: str  # "glyph" or the control's
    operands: tuple[tuple[str, int], ...]
    length: int  # bytes the window pointer passes


def decode_token(data: bytes, offset: int, threshold: int) -> Token:
    if offset >= len(data):
        raise TextError(offset, "text runs past the end of its table")
    byte = data[offset]
    if byte in (0, 1, 3):
        return Token(offset, byte, CONTROLS[byte].mnemonic, (), 1)
    if byte == 2:
        length = 2 if offset + 1 < len(data) and data[offset + 1] == 1 else 1
        return Token(offset, 2, "page", (), length)
    if byte == 0x0F:
        if offset + 1 >= len(data):
            raise TextError(offset, "0F without a sub-code")
        sub = data[offset + 1]
        control = CONTROLS.get(0x0F00 | sub)
        if control is None:
            raise TextError(offset, f"0F sub-code 0x{sub:02x} matches no case (the window stalls)")
        if offset + 2 + len(control.operands) > len(data):
            raise TextError(offset, f"0F {sub:02x} runs past the end of its table")
        operand_bytes = data[offset + 2 : offset + 2 + len(control.operands)]
        values = tuple(zip(control.operands, operand_bytes, strict=True))
        return Token(offset, control.code, control.mnemonic, values, control.length)
    if byte < threshold:
        return Token(offset, byte, "glyph", (), 1)
    if offset + 1 >= len(data):
        raise TextError(offset, "two-byte glyph runs past the end of its table")
    return Token(offset, byte << 8 | data[offset + 1], "glyph", (), 2)


def decode_text(data: bytes, offset: int, threshold: int) -> tuple[Token, ...]:
    """Tokens from `offset` through its terminating 00."""
    out = []
    while True:
        token = decode_token(data, offset, threshold)
        out.append(token)
        if token.mnemonic == "end":
            return tuple(out)
        offset += token.length


def text_table(data: bytes) -> tuple[tuple[int, ...], int]:
    """Entry offsets of a text table and the offset ending its text; a count
    of 0xFFFF marks a field without messages."""
    if len(data) < 4:
        raise TextError(0, "text table shorter than its header")
    count, zero = struct.unpack_from("<HH", data, 0)
    if count == 0xFFFF:
        return (), 4
    if zero != 0 or 4 + 2 * (count + 1) + 2 * count > len(data):
        raise TextError(0, "not a text table (count, 0, offsets, sizes)")
    offsets = struct.unpack_from(f"<{count + 1}H", data, 4)
    if any(o > len(data) for o in offsets):
        raise TextError(4, "text offset outside its table")
    return offsets[:count], offsets[count]


def font_threshold(font: bytes) -> int:
    """text_font_two_byte_threshold: the font block's halfword 2 (text_install_font)."""
    return struct.unpack_from("<H", font, 4)[0]


# Text built at run time (numbers, names, name entry) comes from character
# codes through the byte pairs of system resource 27; text_find_char_code searches
# its first 0x144 pairs.
PAIR_CODES = 0x144
NAME_SLOTS = 31  # game_data.names: twenty bytes each, decoded by mode_load_initial_game_data


def code_text(pairs: bytes, codes) -> bytes:
    """text_decode_codes: the text of character codes (a pair with first byte 0
    gives its second byte alone), then 00."""
    out = bytearray()
    for code in codes:
        if not 0 <= code < PAIR_CODES:
            raise TextError(0, f"character code 0x{code:x} outside the 0x144 pairs")
        first, second = pairs[2 * code], pairs[2 * code + 1]
        out += bytes((first, second)) if first else bytes((second,))
    return bytes(out) + b"\0"


def pair_kind(first: int, second: int, threshold: int) -> str:
    """What window_reveal_text reads in the text bytes of one character pair."""
    if first:
        return "two-byte glyph" if first >= threshold else "two separate tokens"
    if second == 0:
        return "00 (ends the text)"
    if second in (1, 2, 3, 0x0F):
        return "control"
    return "one-byte glyph" if second < threshold else "two-byte glyph lead alone"


def initial_names(game: bytes, pairs: bytes) -> list[bytes]:
    """The name texts mode_load_initial_game_data makes from the game data file: up to ten
    u16 character codes per twenty-byte slot, ending at code 0x000F."""
    names = []
    for slot in range(NAME_SLOTS):
        codes = []
        for code in struct.unpack_from("<10H", game, slot * 20):
            if code == 0x000F:
                break
            codes.append(code)
        names.append(code_text(pairs, codes))
    return names


def unpack(data: bytes) -> bytes:
    return decode_block(data).data


def archive_entry(data: bytes, index: int, *, packed: bool = False) -> bytes:
    """Entry `index` of an offset archive (count, then offsets; 8003342c),
    unpacked from its offset as 80032e88 does when `packed` (a packed stream
    may read its final flag byte from the following entry)."""
    count = struct.unpack_from("<I", data, 0)[0]
    if index >= count:
        raise TextError(0, f"archive has {count} entries, not {index + 1}")
    offsets = struct.unpack_from(f"<{count}I", data, 4) + (len(data),)
    if packed:
        return unpack(data[offsets[index] :])
    return data[offsets[index] : offsets[index + 1]]


def unpack_logical(source: bytes, size: int) -> tuple[bytes, bool]:
    """The first `size` bytes a packed component decodes to, and whether its
    stream reads past `source`. Such a stream is taken only when two different
    continuations give the same `size` bytes (the rest is padding output)."""
    try:
        return decode_block(source, output_limit=size + 16).data[:size], False
    except PackedError:
        outputs = set()
        for fill in (b"\0", b"\xff"):
            try:
                outputs.add(decode_block(source + fill * 64, output_limit=size + 16).data[:size])
            except PackedError as error:
                raise TextError(0, f"packed table: {error}") from None
        if len(outputs) != 1:
            raise TextError(0, "packed table depends on bytes past its file") from None
        return outputs.pop(), True


# D_8009B584[].file (decomp/src/worldmap/worldmap_80094A5C.c): the world map's
# area file sets.
WORLD_AREA_FILES = (
    43,
    54,
    65,
    76,
    87,
    98,
    109,
    120,
    131,
    43,
    142,
    142,
    43,
    164,
    186,
    153,
    175,
    197,
    208,
    219,
)


def system_data(disc: Disc) -> bytes:
    """ "MES SYSDATA", installed by text_install_system_data (text_system_resources[n] = entry n)."""
    return unpack(disc.sectors(disc.slot(0, 1, 7)))


def text_tables(disc: Disc):
    """(group, item, table) for every text table the decompiled loaders read
    on `disc`; table is None for a placeholder map file and a TextError when
    it cannot be unpacked."""
    system = system_data(disc)
    for index in range(struct.unpack_from("<I", system, 0)[0]):
        if index != 27:  # text_system_resources[27]: character code pairs (text_decode_codes_to_buffer), not text
            yield "system data", index, archive_entry(system, index)
    marker = disc.entries.get(disc.slot(4, 0, 0xB7))  # sub-directory of the map files
    for file in range(0xB8, 0xB8 + (-marker["size"] if marker else 0), 2):
        slot = disc.slot(4, 0, file)  # 8001b53c: map file 0xb8 + 2 * field
        entry = disc.entries.get(slot)
        field_number = (file - 0xB8) // 2
        if entry is None or entry["size"] < 0x154:  # "CDMAKE Dummy" files
            yield "field messages", field_number, None
            continue
        sectors = disc.sectors(slot)
        size, offset = (struct.unpack_from("<I", sectors, base + 28)[0] for base in (0x10C, 0x130))
        try:  # component 7 (BUNDLE_8, field_message_table), unpacked by 80070cc8
            messages, past_end = unpack_logical(sectors[offset:], size)
        except TextError as error:
            yield "field messages", field_number, error
            continue
        group = "field messages" + (" (packed stream reads past its file)" if past_end else "")
        yield group, field_number, messages
    # Packed archive entries may read their last flag byte past the file's
    # declared size: unpack them from whole sectors.
    menu = disc.sectors(disc.slot(0x10, 0, 1))  # menu_state_resource_file; labels = files[3] (801c65f4)
    yield "menu labels", 3, archive_entry(menu, 3, packed=True)
    world = disc.sectors(disc.slot(0x24, 0, 0x26))  # the world map's menu_state_resource_file (80071fec)
    yield "menu labels (world map file 0x26)", 3, archive_entry(world, 3, packed=True)
    data = disc.sectors(disc.slot(0x10, 0, 2))  # 801c72bc MenuDataArchive +3C/+40/+54/+58/+D4..;
    for index in (14, 15, 20, 21, 52, 53, 54, 55, 0x27, 0x28, 0x29, *range(0x2C, 0x34)):
        yield "menu data", index, archive_entry(data, index, packed=True)  # shops 801c6828/801c6a54
    mode = unpack(disc.sectors(disc.slot(0x30, 0, 3)))  # menu mode file 3: arena_text_message_table = entry 0
    yield "menu mode", 0, archive_entry(mode, 0)
    battle = disc.sectors(disc.slot(12, 0, 3))  # mode_battle_setup_archive (8001bbac)
    for index in (0x0F, 0x25):  # archive[0x10] battle_item_name_table, archive[0x26] battle_message_table (ovl2615)
        yield "battle archive", index, archive_entry(battle, index, packed=True)
    marker = disc.entries.get(disc.slot(12, 1, 1))  # enemy data: file 2n + 2 (battle_enemy_data_file)
    for file in range(2, 2 + (-marker["size"] if marker else 0), 2):
        enemy = disc.data(disc.slot(12, 1, file))
        yield "enemy data", file, enemy[struct.unpack_from("<H", enemy, 0x30)[0] :]  # battle_enemy_name_table
    for file in (1, 2, 3):  # 8007fd38/8007fe3c after 8008ab94: battle_command_menu_module_block, battle_command_menu_file3_block
        yield "battle menu", file, disc.data(disc.slot(0x20, 3, file))
    scripts = disc.sectors(disc.slot(0x20, 0, 2))  # ovl3087 801e5160: ScriptSet n data
    for index in range(1, struct.unpack_from("<I", scripts, 0)[0], 2):
        yield "battle events", index // 2, archive_entry(scripts, index, packed=True)
    for file in sorted(set(WORLD_AREA_FILES)):  # 80071b9c/80073530: area file + 1, section +1C
        area = unpack(disc.sectors(disc.slot(0x24, 0, file + 1)))
        yield "world areas", file, area[struct.unpack_from("<I", area, 0x1C)[0] :]


# Characters for --chars, read from each disc's own data.
#
# The number code: text_format_number writes a number's digits as the character
# codes palette * 16 + digit and its sign as palette * 16 + 10 (negative) or
# + 11, and the window controls pass palettes 0 and 1 (0F 09, 0F 0A, 0F 0C).
# The menus write the blank that replaces a number's leading zeros as code 0xC3
# (slot39 func_801DC3D8, ovl2601 func_801CDD14, ovl2602 func_801D1304), which
# the name entry also enters for an empty cell (ovl2600 func_801CB33C).
# Resource 27 gives each code's glyph.
NUMBER_PALETTES = (0, 1)
SIGN_CODES = {10: "-", 11: "+"}
BLANK_CODE = 0xC3
# The memory card title lines (directory (0x10, 1) file 1): func_801C6400
# skips D_8006EF64 lines, a byte of 0x80 or more taking the next byte with it,
# and copies the next 30 bytes of two-byte Shift-JIS into the save header. The
# menu's func_801E65E4 turns ASCII 0x20-0x7F into Shift-JIS through the
# 96-entry table D_801EA610 of the slot-39 image; its inverse reads the titles.
TITLE_FILE = (0x10, 1, 1)
TITLE_BYTES = 30
SJIS_TABLE = 0x801EA610 - 0x801C5000  # offset of D_801EA610 in the slot-39 image
# The name entry grid (ovl2600 D_801CBEC0): 36 entries of six character codes,
# of which func_801CA558 shows five, in four columns of nine, so screen row r
# shows entries r, r + 9, r + 18 and r + 27 from left to right.
GRID = 0x801CBEC0 - 0x801C5000  # offset of D_801CBEC0 in the ovl2600 image
GRID_ENTRIES, GRID_CODES, GRID_SHOWN, GRID_ROWS = 36, 6, 5, 9
ALPHABETS = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")


def number_glyphs(pairs: bytes, threshold: int) -> dict[int, str]:
    """The one-byte glyphs of the number code's digit, sign and blank codes."""
    codes = {BLANK_CODE: " "}
    for palette in NUMBER_PALETTES:
        codes.update({16 * palette + digit: str(digit) for digit in range(10)})
        codes.update({16 * palette + code: sign for code, sign in SIGN_CODES.items()})
    found: dict[int, set[str]] = {}
    for code, char in codes.items():
        first, second = pairs[2 * code], pairs[2 * code + 1]
        if pair_kind(first, second, threshold) == "one-byte glyph":
            found.setdefault(second, set()).add(char)
    return {glyph: chars.pop() for glyph, chars in found.items() if len(chars) == 1}


def save_titles(data: bytes, table: tuple[int, ...]) -> list[str]:
    """The title lines as func_801C6400 reads them: 30 bytes from the start of
    the file and after each newline, as text through the inverse of `table`
    (entry n the Shift-JIS of ASCII 0x20 + n) up to the first byte below 0x80,
    without trailing blanks. A line holding a code outside the table is left
    out."""
    ascii_of: dict[int, str] = {}
    for n, code in enumerate(table):
        ascii_of.setdefault(code, chr(0x20 + n))
    starts, i = [0], 0
    while i < len(data):
        if data[i] >= 0x80:
            i += 2
            continue
        if data[i] == 0x0A:
            starts.append(i + 1)
        i += 1
    titles = []
    for start in starts:
        line = data[start : start + TITLE_BYTES]
        chars = []
        for k in range(0, len(line) - 1, 2):
            if line[k] < 0x80:
                break
            chars.append(ascii_of.get(line[k] << 8 | line[k + 1]))
        if None not in chars and "".join(chars).rstrip(" "):
            titles.append("".join(chars).rstrip(" "))
    return titles


def fits(title: str, text: bytes, known: dict[int, str]) -> bool:
    """Whether `text` (one-byte glyphs) can spell `title`: the same length,
    each known glyph at its character, and the other characters at glyphs not
    yet known, equal characters at equal glyphs and different ones apart."""
    if len(title) != len(text):
        return False
    known_chars = set(known.values())
    glyph_of: dict[str, int] = {}
    char_of: dict[int, str] = {}
    for char, glyph in zip(title, text, strict=True):
        if glyph in known:
            if known[glyph] != char:
                return False
        elif char in known_chars:
            return False
        elif glyph_of.setdefault(char, glyph) != glyph or char_of.setdefault(glyph, char) != char:
            return False
    return True


def agreeing(proposals: dict[str, bytes], known: dict[int, str]) -> list[str]:
    """The titles in every largest set of proposals that agree: no glyph they
    add is read as two characters and no character gets two glyphs."""
    names = sorted(proposals)
    added = {
        title: {(g, c) for g, c in zip(proposals[title], title, strict=True) if g not in known}
        for title in names
    }

    def clash(a: str, b: str) -> bool:
        pairs = added[a] | added[b]
        return len({g for g, _ in pairs}) < len(pairs) or len({c for _, c in pairs}) < len(pairs)

    best: list[frozenset[str]] = []

    def grow(chosen: frozenset[str], rest: list[str]) -> None:
        size = len(best[0]) if best else 0
        if len(chosen) + len(rest) < size:
            return
        if not rest:
            if len(chosen) > size:
                best[:] = [chosen]
            elif chosen:
                best.append(chosen)
            return
        first, others = rest[0], rest[1:]
        grow(chosen | {first}, [n for n in others if not clash(first, n)])
        grow(chosen, others)

    grow(frozenset(), names)
    return sorted(frozenset.intersection(*best)) if best else []


def align_titles(
    titles: list[str], texts: set[bytes], known: dict[int, str]
) -> tuple[dict[int, str], list[tuple[str, bytes]]]:
    """Extend `known` (glyph -> character) from the titles that recur as whole
    texts. In each round a title with known and unknown characters proposes the
    text that fits it when exactly one does (fits); the proposals that agree
    (agreeing) add their glyphs. Returns the glyphs and the (title, text) pairs
    used."""
    known = dict(known)
    by_length: dict[int, list[bytes]] = {}
    for text in texts:
        by_length.setdefault(len(text), []).append(text)
    aligned: list[tuple[str, bytes]] = []
    while True:
        chars = set(known.values())
        proposals = {}
        for title in sorted(set(titles)):
            if not set(title) & chars or set(title) <= chars:
                continue
            found = [text for text in by_length.get(len(title), ()) if fits(title, text, known)]
            if len(found) == 1:
                proposals[title] = found[0]
        accepted = agreeing(proposals, known)
        if not accepted:
            return known, aligned
        for title in accepted:
            known.update(zip(proposals[title], title, strict=True))
            aligned.append((title, proposals[title]))


def grid_letters(
    grid: bytes, pairs: bytes, threshold: int, known: dict[int, str]
) -> dict[int, str]:
    """The letters of the name entry grid's alphabet runs. In screen order
    (GRID notes) each run of 26 ascending character codes whose resource 27
    pairs are one-byte glyphs is an alphabet when the known glyphs among them
    are at least two letters of one case, each at its own place in that
    alphabet; its other glyphs then take the alphabet's other letters, unless
    one of them is known or one of those letters has a glyph already. A run
    that fails any of this adds nothing."""
    codes = [
        code
        for row in range(GRID_ROWS)
        for column in range(GRID_ENTRIES // GRID_ROWS)
        for code in grid[(row + GRID_ROWS * column) * GRID_CODES :][:GRID_SHOWN]
    ]
    runs, run = [], codes[:1]
    for code in codes[1:]:
        if code == run[-1] + 1:
            run.append(code)
        else:
            runs.append(run)
            run = [code]
    runs.append(run)
    letters: dict[int, str] = {}
    for run in runs:
        if len(run) != len(ALPHABETS[0]) or 2 * run[-1] + 1 >= len(pairs):
            continue
        if any(
            pair_kind(pairs[2 * code], pairs[2 * code + 1], threshold) != "one-byte glyph"
            for code in run
        ):
            continue
        glyphs = [pairs[2 * code + 1] for code in run]
        placed = [(place, known[glyph]) for place, glyph in enumerate(glyphs) if glyph in known]
        cases = [alphabet for alphabet in ALPHABETS if all(c in alphabet for _, c in placed)]
        if len(placed) < 2 or len(cases) != 1:
            continue
        alphabet = cases[0]
        if any(alphabet[place] != char for place, char in placed) or len(set(glyphs)) != 26:
            continue
        added = {glyph: alphabet[place] for place, glyph in enumerate(glyphs) if glyph not in known}
        if set(added.values()) & set(known.values()):
            continue
        letters.update(added)
    return letters


@dataclass(frozen=True)
class CharacterMap:
    glyphs: dict[int, str]  # one-byte glyph -> character
    numbers: int  # glyphs from the number code
    titles: tuple[tuple[str, bytes], ...]  # (title, text) pairs that gave letters
    grid: int = 0  # glyphs from the name entry grid's alphabet runs

    def summary(self) -> str:
        letters = "".join(sorted(c for c in self.glyphs.values() if c.isalpha()))
        from_titles = len(self.glyphs) - self.numbers - self.grid
        return (
            f"{len(self.glyphs)} glyphs: {self.numbers} from the number code, "
            f"{from_titles} from {len(self.titles)} memory card titles "
            f"({', '.join(title for title, _ in self.titles)}), {self.grid} from the name "
            f"entry grid's alphabets; letters {letters}"
        )


def whole_texts(disc: Disc, threshold: int) -> set[bytes]:
    """Every entry's text that holds only one-byte glyphs, without its 00."""
    texts = set()
    for _, _, data in text_tables(disc):
        if not isinstance(data, bytes):
            continue
        try:
            offsets, _ = text_table(data)
        except TextError:
            continue
        for offset in set(offsets):
            try:
                tokens = decode_text(data, offset, threshold)
            except TextError:
                continue
            if all(t.mnemonic == "glyph" and t.length == 1 for t in tokens[:-1]):
                texts.add(bytes(t.code for t in tokens[:-1]))
    return texts


def character_map(disc: Disc, threshold: int) -> CharacterMap:
    """The glyphs whose characters `disc`'s own data gives (module notes)."""
    pairs = archive_entry(system_data(disc), 27)
    known = number_glyphs(pairs, threshold)
    slot, other, _ = OVERLAYS["slot39"]  # the unpacked image, directory (0x10, 0) file 5
    image = disc.data((slot, other)[disc.number - 1])
    table = struct.unpack_from("<96H", image, SJIS_TABLE)
    titles = save_titles(disc.data(disc.slot(*TITLE_FILE)), table)
    glyphs, aligned = align_titles(titles, whole_texts(disc, threshold), known)
    slot, other, _ = OVERLAYS["ovl2600"]
    grid = disc.data((slot, other)[disc.number - 1])[GRID : GRID + GRID_ENTRIES * GRID_CODES]
    letters = grid_letters(grid, pairs, threshold, glyphs)
    return CharacterMap({**glyphs, **letters}, len(known), tuple(aligned), len(letters))


@dataclass
class Sweep:
    tables: Counter = field(default_factory=Counter)
    texts: int = 0
    tokens: int = 0
    uses: Counter = field(default_factory=Counter)
    unknown: list = field(default_factory=list)
    thresholds: set = field(default_factory=set)
    table_problems: list = field(default_factory=list)
    unreferenced: int = 0
    unreferenced_tables: int = 0
    unreferenced_texts: int = 0
    unreferenced_errors: list = field(default_factory=list)
    digests: set = field(default_factory=set)
    pairs: dict = field(default_factory=dict)  # disc -> Counter of pair_kind
    names: int = 0
    name_uses: Counter = field(default_factory=Counter)
    disc_codes: dict = field(default_factory=dict)  # disc -> controls its tables use
    characters: dict = field(default_factory=dict)  # disc -> CharacterMap

    def add(self, where: str, data: bytes, threshold: int) -> set:
        """Decode the table's texts into the counts; returns the controls
        they use."""
        codes = set()
        try:
            offsets, end = text_table(data)
        except TextError as error:
            self.unknown.append(f"{where}: {error}")
            return codes
        covered = bytearray(len(data))
        for offset in sorted(set(offsets)):
            try:
                tokens = decode_text(data, offset, threshold)
            except TextError as error:
                self.unknown.append(f"{where} text +0x{offset:x}: {error}")
                continue
            self.texts += 1
            self.tokens += len(tokens)
            last = tokens[-1].offset + tokens[-1].length
            covered[offset:last] = b"\1" * (last - offset)
            for token in tokens:
                if token.mnemonic == "glyph":
                    self.uses["glyph (2 bytes)" if token.length == 2 else "glyph (1 byte)"] += 1
                else:
                    self.uses[token.code] += 1
                    codes.add(token.code)
        if offsets:
            self.unreached(where, data, covered, min(offsets), min(end, len(data)), threshold)
        return codes

    def unreached(
        self, where: str, data: bytes, covered: bytearray, start: int, stop: int, threshold: int
    ) -> None:
        """Count the nonzero text bytes no entry reaches and decode each run
        of them as texts of its own (text no entry shows, kept out of the use
        counts)."""
        dead, texts = unreached_texts(data, covered, start, stop, threshold)
        for _, tokens in texts:
            if isinstance(tokens, TextError):
                self.unreferenced_errors.append(f"{where}: {tokens}")
            else:
                self.unreferenced_texts += 1
        self.unreferenced += dead
        self.unreferenced_tables += dead > 0


def unreached_texts(
    data: bytes, covered: bytearray, start: int, stop: int, threshold: int
) -> tuple[int, list[tuple[int, tuple[Token, ...] | TextError]]]:
    """The nonzero bytes in [start, stop) that no entry reaches, and each run
    of them decoded as texts of its own: (offset, tokens), or (offset, error)
    for the rest of a run that does not decode."""
    position, dead, texts = start, 0, []
    while position < stop:
        if covered[position] or not data[position]:
            position += 1
            continue
        end = position
        while end < stop and not covered[end]:
            end += 1
        dead += sum(1 for byte in data[position:end] if byte)
        while position < end:
            if not data[position]:
                position += 1
                continue
            try:
                tokens = decode_text(data, position, threshold)
            except TextError as error:
                texts.append((position, error))
                break
            texts.append((position, tokens))
            position = tokens[-1].offset + tokens[-1].length
        position = max(position, end)
    return dead, texts


def render(tokens: tuple[Token, ...], chars: dict[int, str] | None = None) -> str:
    """Glyphs as hex codes (two digits for one byte, four for two), controls
    by mnemonic and operands in brackets. With `chars` (one-byte glyph ->
    character) the tokens join without spaces: a glyph it holds prints as its
    character (after a backslash when it is one of \\ { } [ ]), any other
    glyph as its hex code in braces."""
    parts = []
    for token in tokens:
        if token.mnemonic != "glyph":
            operands = "".join(f" {name}={value}" for name, value in token.operands)
            parts.append(f"[{token.mnemonic}{operands}]")
        elif chars is None:
            parts.append(f"{token.code:0{2 * token.length}x}")
        elif token.length == 1 and token.code in chars:
            char = chars[token.code]
            parts.append("\\" + char if char in "\\{}[]" else char)
        else:
            parts.append(f"{{{token.code:0{2 * token.length}x}}}")
    return " ".join(parts) if chars is None else "".join(parts)


def table_listing(data: bytes, threshold: int, chars: dict[int, str] | None = None) -> list[str]:
    """Each entry of a text table: number, offset, (columns, rows) and its
    tokens (render); then the texts in bytes no entry reaches."""
    offsets, end = text_table(data)
    if not offsets:
        return ["  no messages (count 0xFFFF)"]
    pairs = 4 + 2 * (len(offsets) + 1)
    lines, covered = [], bytearray(len(data))
    for number, offset in enumerate(offsets):
        size = f"{data[pairs + 2 * number]}x{data[pairs + 2 * number + 1]}"
        try:
            tokens = decode_text(data, offset, threshold)
        except TextError as error:
            lines.append(f"  {number:4d} +0x{offset:04x} {size}: undecodable, {error}")
            continue
        last = tokens[-1].offset + tokens[-1].length
        covered[offset:last] = b"\1" * (last - offset)
        lines.append(f"  {number:4d} +0x{offset:04x} {size}: {render(tokens, chars)}")
    _, texts = unreached_texts(data, covered, min(offsets), min(end, len(data)), threshold)
    for offset, tokens in texts:
        text = f"undecodable, {tokens}" if isinstance(tokens, TextError) else render(tokens, chars)
        lines.append(f"  unreached +0x{offset:04x}: {text}")
    return lines


# --list names of the groups text_tables yields.
GROUPS = {
    "system": ("system data",),
    "field": ("field messages", "field messages (packed stream reads past its file)"),
    "menu-labels": ("menu labels",),
    "worldmap-labels": ("menu labels (world map file 0x26)",),
    "menu-data": ("menu data",),
    "menu-mode": ("menu mode",),
    "battle-archive": ("battle archive",),
    "enemy": ("enemy data",),
    "battle-menu": ("battle menu",),
    "battle-events": ("battle events",),
    "world-areas": ("world areas",),
}


def listing(disc: Disc, name: str, item: int | None = None, *, chars: bool = False) -> list[str]:
    """The tables of one group (GROUPS), or its table `item` (a field map's
    number, an archive entry or a file, as the sweep names them); with `chars`
    the glyphs of character_map print as characters, after a line naming them."""
    threshold = font_threshold(unpack(disc.sectors(disc.slot(0, 1, 6))))
    glyphs, head_lines = None, []
    if chars:
        characters = character_map(disc, threshold)
        glyphs = characters.glyphs
        head_lines.append(f"disc {disc.number} characters: {characters.summary()}")
    lines = []
    for group, number, data in text_tables(disc):
        if group not in GROUPS[name] or item is not None and number != item:
            continue
        head = f"disc {disc.number} {group} {number}"
        if data is None:
            lines.append(f"{head}: placeholder map file")
        elif isinstance(data, TextError):
            lines.append(f"{head}: {data}")
        else:
            try:
                body = table_listing(data, threshold, glyphs)
            except TextError as error:
                body = [f"  not a text table: {error}"]
            lines += [f"{head}: {len(data)} bytes", *body]
    if not lines:
        raise SystemExit(f"disc {disc.number} has no {name} table {item}")
    return head_lines + lines


def sweep() -> Sweep:
    result = Sweep()
    for disc in discs():
        result.table_problems += [f"disc {disc.number}: {p}" for p in check_jump_table(disc.boot)]
        threshold = font_threshold(unpack(disc.sectors(disc.slot(0, 1, 6))))  # "MES FONT"
        result.thresholds.add(threshold)
        for group, item, data in text_tables(disc):
            if data is None:
                result.tables["placeholder map files"] += 1
            elif isinstance(data, TextError):
                result.unknown.append(f"disc{disc.number} {group} {item}: {data}")
            else:
                result.tables[group] += 1
                result.digests.add(hashlib.sha256(data).digest())
                result.disc_codes.setdefault(disc.number, set()).update(
                    result.add(f"disc{disc.number} {group} {item}", data, threshold)
                )
        pairs = archive_entry(system_data(disc), 27)
        result.pairs[disc.number] = Counter(
            pair_kind(pairs[2 * code], pairs[2 * code + 1], threshold) for code in range(PAIR_CODES)
        )
        try:  # directory 0x10 file 3, read by mode_load_initial_game_data
            names = initial_names(disc.data(disc.slot(0x10, 0, 3)), pairs)
        except TextError as error:
            result.unknown.append(f"disc{disc.number} initial names: {error}")
            names = []
        for slot, name in enumerate(names):
            try:
                tokens = decode_text(name, 0, threshold)
            except TextError as error:
                result.unknown.append(f"disc{disc.number} initial name {slot}: {error}")
                continue
            result.names += 1
            result.name_uses.update(token.mnemonic for token in tokens)
        result.characters[disc.number] = character_map(disc, threshold)
    return result


def control_name(code: int) -> str:
    return f"0F {code & 0xFF:02x}" if code >= 0x0F00 else f"{code:02x}"


def report(result: Sweep) -> str:
    thresholds = ", ".join(f"0x{t:02x}" for t in sorted(result.thresholds))
    lines = [
        "text control sweep (both discs)",
        "  0F jump table (0x80018A7C) vs the 16 cases: "
        + ("match" if not result.table_problems else f"{len(result.table_problems)} differences"),
        *(f"    {p}" for p in result.table_problems),
        f"  two-byte glyph threshold (font halfword 2): {thresholds}",
    ]
    decoded = sum(
        count for group, count in result.tables.items() if group != "placeholder map files"
    )
    lines.append(f"  tables decoded: {decoded} ({len(result.digests)} distinct)")
    lines.append(
        "    " + ", ".join(f"{group} {count}" for group, count in sorted(result.tables.items()))
    )
    lines.append(f"  texts decoded: {result.texts}, tokens: {result.tokens}")
    for name in ("glyph (1 byte)", "glyph (2 bytes)"):
        lines.append(f"    {name}: {result.uses[name]}")
    used = sorted(code for code in result.uses if isinstance(code, int))
    lines.append(f"  controls defined: {len(CONTROLS)}; used: {len(used)}")
    lines.append(
        "    used per disc: "
        + ", ".join(
            f"disc {disc} {len(codes)}" for disc, codes in sorted(result.disc_codes.items())
        )
    )
    for code in used:
        lines.append(f"    {control_name(code)} {CONTROLS[code].mnemonic}: {result.uses[code]}")
    unused = [control_name(code) for code in sorted(CONTROLS) if code not in result.uses]
    lines.append(f"  defined but unused: {', '.join(unused)}")
    lines.append(f"  unknown/undecodable: {len(result.unknown)}")
    lines += [f"    {u}" for u in result.unknown]
    if result.unreferenced:
        lines.append(
            f"  nonzero text bytes no entry reaches: {result.unreferenced} in "
            f"{result.unreferenced_tables} tables, decoding as {result.unreferenced_texts} "
            "whole texts"
        )
    if result.unreferenced_errors:
        lines.append(f"  unreached bytes that do not decode: {len(result.unreferenced_errors)}")
        lines += [f"    {u}" for u in result.unreferenced_errors]
    for disc, kinds in sorted(result.pairs.items()):
        lines.append(
            f"  disc {disc} character pairs (resource 27, 0x144 codes; run-time text): "
            + ", ".join(f"{kind} {count}" for kind, count in sorted(kinds.items()))
        )
    lines.append(
        f"  initial names (directory 0x10 file 3 via the pairs): {result.names} decoded; "
        + ", ".join(f"{name} {count}" for name, count in sorted(result.name_uses.items()))
    )
    for disc, characters in sorted(result.characters.items()):
        lines.append(f"  disc {disc} characters (--chars): {characters.summary()}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--sweep", action="store_true", help="aggregate decode of both discs")
    parser.add_argument("--list", choices=sorted(GROUPS), help="print one group's tables")
    parser.add_argument("--item", type=int, help="only this table of --list (field: map number)")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    parser.add_argument(
        "--chars",
        action="store_true",
        help="with --list: glyphs the disc's data names as characters, others as {hex}",
    )
    args = parser.parse_args(argv)
    if not (args.sweep or args.list):
        parser.error("choose --sweep or --list GROUP")
    if args.chars and not args.list:
        parser.error("--chars needs --list GROUP")
    if args.list:
        print("\n".join(listing(discs()[args.disc - 1], args.list, args.item, chars=args.chars)))
    if not args.sweep:
        return 0
    result = sweep()
    print(report(result))
    return 1 if result.unknown or result.table_problems else 0


if __name__ == "__main__":
    sys.exit(main())
