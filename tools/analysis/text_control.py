"""Message text and its control codes, from the recovered window code.

func_80033DF0 (decomp/src/resident/main2.c) reveals a window's text. A byte
below the font's two-byte threshold (D_8005934C, font halfword 2, installed by
func_80033558) is a one-byte glyph and a byte at or above it starts a two-byte
glyph, unless it is one of the controls 00, 01, 02, 03 or 0F; 0F takes a
sub-code dispatched through the 16-entry jump table at 0x80018A7C (switch cases
0-15). A sub-code of 16 or more matches no case and leaves the pointer on the
0F, so the window never advances: such text is undecodable. Controls that
insert text (0F 03-08, 09/0A/0C, 0F) set the window's resume pointer to their
last operand; the inserted text's 00 returns to the byte after it.

Text lives in tables read by func_80033728: entry n starts at the u16 offset
at byte 4 + 2n. Every located table also holds its count at +0, 0 at +2, count
+ 1 offsets (the last ends the text) and count (columns, rows) byte pairs
after them (func_8003373C/func_80033760 read the pairs). Windows get text only
through func_80034714 (queue), func_80034EAC (one-line layout) and the
insertions; their callers pass entries of these tables, or text built at run
time from character codes through the byte pairs of system resource 27
(func_80033ABC/func_80033B34: numbers, names, name entry). The sweep reads
every table those callers use, classifies the pairs and decodes the initial
names (func_8001B970).

    python3 -m tools.analysis.text_control --sweep    # both discs, aggregate only
    python3 -m tools.analysis.text_control --list field --item 3 --disc 2

`--list GROUP` prints one group's tables (`--item`: one table, a field map's
number, an archive entry or a file as the sweep names them) to stdout; keep
listings under .local/. Each entry shows its offset, (columns, rows) and
tokens: one-byte glyphs as two hex digits, two-byte glyphs as four, controls
by mnemonic in brackets; text no entry reaches follows as `unreached`.
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
        "func_80033DF0 byte 0",
        "end of text: return after an inserted text's control; else state 1 (unk6B), flag 8 "
        "(wait) and unk6C, so once the wait ends the window moves on to its next queued "
        "text; the pointer stays on the 00",
    ),
    0x01: Control(
        0x01,
        "newline",
        (),
        1,
        "func_80033DF0 byte 1",
        "x = 100 and end this step, so the next step starts a new line",
    ),
    0x02: Control(
        0x02,
        "page",
        (),
        1,
        "func_80033DF0 byte 2",
        "state 2, flags 0x48: wait, then clear the window; a following 01 is skipped",
    ),
    0x03: Control(
        0x03, "pause", (), 1, "func_80033DF0 byte 3", "state 3, flag 8: wait, keeping the lines"
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
        "insert the name of the button assigned to `action` (D_80050238) from resource 49",
    ),
)
for _sub, _name, _operands, _length, _effect in _EXTENDED:
    CONTROLS[0x0F00 | _sub] = Control(
        0x0F00 | _sub, _name, _operands, _length, f"func_80033DF0 0F case {_sub}", _effect
    )
EXTENDED_TABLE = 0x80018A7C  # jump table of the 0F sub-codes
INTERPRETER = (0x80033DF0, 0x800345E0)  # func_80033DF0 up to func_800345E0


def check_jump_table(exe: bytes) -> list[str]:
    """Differences between the boot program's 0F jump table and the 16 cases."""
    base = struct.unpack_from("<I", exe, 0x18)[0]  # PS-X EXE text address, after a 0x800 header
    offset = EXTENDED_TABLE - base + 0x800
    targets = struct.unpack_from("<17I", exe, offset)
    problems = [
        f"0F case {sub} target {target:08x} outside func_80033DF0"
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
    """D_8005934C: the font block's halfword 2 (func_80033558)."""
    return struct.unpack_from("<H", font, 4)[0]


# Text built at run time (numbers, names, name entry) comes from character
# codes through the byte pairs of system resource 27; func_80033BAC searches
# its first 0x144 pairs.
PAIR_CODES = 0x144
NAME_SLOTS = 31  # D_8006D634.names: twenty bytes each, decoded by func_8001B970


def code_text(pairs: bytes, codes) -> bytes:
    """func_80033B34: the text of character codes (a pair with first byte 0
    gives its second byte alone), then 00."""
    out = bytearray()
    for code in codes:
        if not 0 <= code < PAIR_CODES:
            raise TextError(0, f"character code 0x{code:x} outside the 0x144 pairs")
        first, second = pairs[2 * code], pairs[2 * code + 1]
        out += bytes((first, second)) if first else bytes((second,))
    return bytes(out) + b"\0"


def pair_kind(first: int, second: int, threshold: int) -> str:
    """What func_80033DF0 reads in the text bytes of one character pair."""
    if first:
        return "two-byte glyph" if first >= threshold else "two separate tokens"
    if second == 0:
        return "00 (ends the text)"
    if second in (1, 2, 3, 0x0F):
        return "control"
    return "one-byte glyph" if second < threshold else "two-byte glyph lead alone"


def initial_names(game: bytes, pairs: bytes) -> list[bytes]:
    """The name texts func_8001B970 makes from the game data file: up to ten
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
    """ "MES SYSDATA", installed by func_800335F4 (D_80059360[n] = entry n)."""
    return unpack(disc.sectors(disc.slot(0, 1, 7)))


def text_tables(disc: Disc):
    """(group, item, table) for every text table the decompiled loaders read
    on `disc`; table is None for a placeholder map file and a TextError when
    it cannot be unpacked."""
    system = system_data(disc)
    for index in range(struct.unpack_from("<I", system, 0)[0]):
        if index != 27:  # D_80059360[27]: character code pairs (func_80033ABC), not text
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
        try:  # component 7 (BUNDLE_8, D_800ADBF0), unpacked by 80070cc8
            messages, past_end = unpack_logical(sectors[offset:], size)
        except TextError as error:
            yield "field messages", field_number, error
            continue
        group = "field messages" + (" (packed stream reads past its file)" if past_end else "")
        yield group, field_number, messages
    # Packed archive entries may read their last flag byte past the file's
    # declared size: unpack them from whole sectors.
    menu = disc.sectors(disc.slot(0x10, 0, 1))  # D_8005945C; labels = files[3] (801c65f4)
    yield "menu labels", 3, archive_entry(menu, 3, packed=True)
    world = disc.sectors(disc.slot(0x24, 0, 0x26))  # the world map's D_8005945C (80071fec)
    yield "menu labels (world map file 0x26)", 3, archive_entry(world, 3, packed=True)
    data = disc.sectors(disc.slot(0x10, 0, 2))  # 801c72bc MenuDataArchive +3C/+40/+54/+58/+D4..;
    for index in (14, 15, 20, 21, 52, 53, 54, 55, 0x27, 0x28, 0x29, *range(0x2C, 0x34)):
        yield "menu data", index, archive_entry(data, index, packed=True)  # shops 801c6828/801c6a54
    mode = unpack(disc.sectors(disc.slot(0x30, 0, 3)))  # menu mode file 3: D_80092880 = entry 0
    yield "menu mode", 0, archive_entry(mode, 0)
    battle = disc.sectors(disc.slot(12, 0, 3))  # D_800595A8 (8001bbac)
    for index in (0x0F, 0x25):  # archive[0x10] D_800D329C, archive[0x26] D_800D39F0 (ovl2615)
        yield "battle archive", index, archive_entry(battle, index, packed=True)
    marker = disc.entries.get(disc.slot(12, 1, 1))  # enemy data: file 2n + 2 (D_800C3DD0)
    for file in range(2, 2 + (-marker["size"] if marker else 0), 2):
        enemy = disc.data(disc.slot(12, 1, file))
        yield "enemy data", file, enemy[struct.unpack_from("<H", enemy, 0x30)[0] :]  # D_800C3DDC
    for file in (1, 2, 3):  # 8007fd38/8007fe3c after 8008ab94: D_800D367C, D_800C3DE8
        yield "battle menu", file, disc.data(disc.slot(0x20, 3, file))
    scripts = disc.sectors(disc.slot(0x20, 0, 2))  # ovl3087 801e5160: ScriptSet n data
    for index in range(1, struct.unpack_from("<I", scripts, 0)[0], 2):
        yield "battle events", index // 2, archive_entry(scripts, index, packed=True)
    for file in sorted(set(WORLD_AREA_FILES)):  # 80071b9c/80073530: area file + 1, section +1C
        area = unpack(disc.sectors(disc.slot(0x24, 0, file + 1)))
        yield "world areas", file, area[struct.unpack_from("<I", area, 0x1C)[0] :]


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


def render(tokens: tuple[Token, ...]) -> str:
    """Glyphs as hex codes (two digits for one byte, four for two), controls
    by mnemonic and operands in brackets."""
    parts = []
    for token in tokens:
        if token.mnemonic == "glyph":
            parts.append(f"{token.code:0{2 * token.length}x}")
        else:
            operands = "".join(f" {name}={value}" for name, value in token.operands)
            parts.append(f"[{token.mnemonic}{operands}]")
    return " ".join(parts)


def table_listing(data: bytes, threshold: int) -> list[str]:
    """Each entry of a text table: number, offset, (columns, rows) and its
    tokens; then the texts in bytes no entry reaches."""
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
        lines.append(f"  {number:4d} +0x{offset:04x} {size}: {render(tokens)}")
    _, texts = unreached_texts(data, covered, min(offsets), min(end, len(data)), threshold)
    for offset, tokens in texts:
        text = f"undecodable, {tokens}" if isinstance(tokens, TextError) else render(tokens)
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


def listing(disc: Disc, name: str, item: int | None = None) -> list[str]:
    """The tables of one group (GROUPS), or its table `item` (a field map's
    number, an archive entry or a file, as the sweep names them)."""
    threshold = font_threshold(unpack(disc.sectors(disc.slot(0, 1, 6))))
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
                body = table_listing(data, threshold)
            except TextError as error:
                body = [f"  not a text table: {error}"]
            lines += [f"{head}: {len(data)} bytes", *body]
    if not lines:
        raise SystemExit(f"disc {disc.number} has no {name} table {item}")
    return lines


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
        try:  # directory 0x10 file 3, read by func_8001B970
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
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--sweep", action="store_true", help="aggregate decode of both discs")
    parser.add_argument("--list", choices=sorted(GROUPS), help="print one group's tables")
    parser.add_argument("--item", type=int, help="only this table of --list (field: map number)")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    args = parser.parse_args(argv)
    if not (args.sweep or args.list):
        parser.error("choose --sweep or --list GROUP")
    if args.list:
        print("\n".join(listing(discs()[args.disc - 1], args.list, args.item)))
    if not args.sweep:
        return 0
    result = sweep()
    print(report(result))
    return 1 if result.unknown or result.table_problems else 0


if __name__ == "__main__":
    sys.exit(main())
