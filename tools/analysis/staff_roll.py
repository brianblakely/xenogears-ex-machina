"""The staff-roll text of field file 0xAB, as the recovered field code reads it.

Field extended event BE (field_event_enable_movie_overlay, decomp/src/field/field_event.c) is
the only store of 1 to mode_staff_roll_enabled; no reachable field script uses it (the
tools.analysis.events sweep). With it set, the field movie player loads
directory (4, 0) files 0xAB (the text) and 0xAC (a font image) (field_staff_roll_start,
field_staff_roll_load_files) and draws a text roll over movie frames 0x687-0x18E1
(field_staff_roll_run_over_movie, decomp/src/field/field_screen.c). Of that loop's passes,
field_staff_roll_advance draws the next line on every 16th, at x 0x300 into VRAM row
n & 15 from row 15, and field_staff_roll_draw scrolls the sixteen rows up a pixel on
each (decomp/src/field/field_effect.c).

field_staff_roll_write_line reads a line as up to 28 (GLYPH_LINE_CELLS) big-endian two-byte
glyph codes ending at a CR byte (0x0D), which the line consumes. The CR test
comes before each code, so a line of 28 codes stops before its CR and the next
line is that CR alone, an empty line. field_staff_roll_get_glyph takes codes 8540-887F
(GLYPH_OWN_FIRST, GLYPH_OWN_COUNT) as cell code - 8540 of the font image
(file 0xAC, uploaded to VRAM (380, 100) by field_staff_roll_upload_font; seven 9x16 cells a
row) and passes any other code to the BIOS kanji ROM lookup Krom2RawAdd, whose
PsyQ prototype names its argument a Shift-JIS code. CR is the only control. A
line is read only while the bytes left (field_staff_roll_bytes_left: file 0xAB's size less what
earlier lines used) are above 0; every line after that is blank.

    python3 -m tools.analysis.staff_roll --sweep            # both discs, aggregate only
    python3 -m tools.analysis.staff_roll --list [--disc N]  # one disc's lines, codes in hex
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from collections import Counter
from dataclasses import dataclass, field

from tools.analysis.disc_index import Disc, discs

DIRECTORY = (4, 0)  # 80028470(4, 0) in field_staff_roll_load_files
TEXT_FILE = 0xAB
FONT_FILE = 0xAC
CR = 0x0D  # field_staff_roll_write_line: line[used] == '\r'
LINE_CODES = 28  # GLYPH_LINE_CELLS (decomp/src/field/field_glyph.h)
FONT_FIRST = 0x8540  # GLYPH_OWN_FIRST
FONT_COUNT = 0x340  # GLYPH_OWN_COUNT
FONT_ORIGIN = (0x380, 0x100)  # field_staff_roll_write_line's MoveImage source; field_staff_roll_upload_font's upload
FONT_COLUMNS = 7  # glyph % 7, glyph / 7
CELL = (9, 16)  # VRAM halfwords (18 8-bit pixels) by rows


class StaffRollError(ValueError):
    def __init__(self, offset: int, reason: str):
        self.offset, self.reason = offset, reason
        super().__init__(f"+0x{offset:x}: {reason}")


@dataclass(frozen=True)
class Line:
    offset: int
    codes: tuple[int, ...]  # glyph codes, (first byte << 8) | second byte
    length: int  # bytes the line consumes: two per code, plus its CR when read
    cr: bool  # False for a line of 28 codes, which leaves its CR to the next line


def decode_line(data: bytes, offset: int) -> Line:
    """field_staff_roll_write_line: up to 28 codes from `offset`, ending at a CR it consumes."""
    codes = []
    position = offset
    while len(codes) < LINE_CODES:
        if position >= len(data):
            raise StaffRollError(offset, "line runs past the end of the text")
        if data[position] == CR:
            return Line(offset, tuple(codes), position + 1 - offset, True)
        if position + 1 >= len(data):
            raise StaffRollError(position, "glyph code runs past the end of the text")
        codes.append(data[position] << 8 | data[position + 1])
        position += 2
    return Line(offset, tuple(codes), position - offset, False)


def decode(data: bytes) -> list[Line]:
    """The lines field_staff_roll_advance draws from a text of len(data) bytes: one per
    call of field_staff_roll_write_line until the bytes left (field_staff_roll_bytes_left) are 0 or less."""
    lines, offset = [], 0
    while len(data) - offset > 0:
        line = decode_line(data, offset)
        lines.append(line)
        offset += line.length
    return lines


def glyph(code: int) -> tuple[str, int]:
    """field_staff_roll_get_glyph: ("font", cell) for codes 8540-887F, else ("rom", code),
    the code passed to Krom2RawAdd."""
    if 0 <= code - FONT_FIRST < FONT_COUNT:
        return "font", code - FONT_FIRST
    return "rom", code


def font_cell(cell: int) -> tuple[int, int]:
    """The VRAM (x, y) field_staff_roll_write_line copies font cell `cell` from."""
    return (
        FONT_ORIGIN[0] + cell % FONT_COLUMNS * CELL[0],
        FONT_ORIGIN[1] + cell // FONT_COLUMNS * CELL[1],
    )


def tim_image(tim: bytes) -> tuple[int, int, int, int]:
    """The image rectangle (x, y, w, h) of a TIM (OpenTIM/ReadTIM in
    field_load_tim_at): word 0x10, a flags word (bit 3: a CLUT block first), then
    blocks of a length word and a u16 (x, y, w, h) rectangle."""
    if len(tim) < 8 or struct.unpack_from("<I", tim, 0)[0] != 0x10:
        raise StaffRollError(0, "font file is not a TIM")
    offset = 8
    if struct.unpack_from("<I", tim, 4)[0] & 8:
        offset += struct.unpack_from("<I", tim, offset)[0]
    if offset + 12 > len(tim):
        raise StaffRollError(offset, "TIM image block runs past the end of the file")
    return struct.unpack_from("<4H", tim, offset + 4)


def inside(cell: int, width: int, height: int) -> bool:
    """Whether font cell `cell` lies inside a width x height image at the font
    origin (field_load_tim_at keeps the TIM's size and moves it there)."""
    x, y = font_cell(cell)
    return x + CELL[0] <= FONT_ORIGIN[0] + width and y + CELL[1] <= FONT_ORIGIN[1] + height


@dataclass
class TextCount:
    """One disc's text: its counts and problems."""

    size: int = 0
    lines: int = 0
    cr: int = 0
    full: int = 0  # lines of 28 codes, ending without their CR
    empty: int = 0
    longest: int = 0
    font: Counter = field(default_factory=Counter)  # cell -> uses
    rom: Counter = field(default_factory=Counter)  # code -> uses
    image: tuple[int, int] = (0, 0)  # file 0xAC's image size (w, h)
    outside: list = field(default_factory=list)  # font cells outside that image
    unknown: list = field(default_factory=list)  # undecodable text


def count(text: bytes, tim: bytes) -> TextCount:
    """Decode one disc's text and check its font cells against its font image."""
    result = TextCount(size=len(text))
    try:
        result.image = tim_image(tim)[2:]
    except StaffRollError as error:
        result.unknown.append(f"file 0xac {error}")
    try:
        lines = decode(text)
    except StaffRollError as error:
        result.unknown.append(f"file 0xab {error}")
        return result
    for line in lines:
        result.lines += 1
        result.cr += line.cr
        result.full += not line.cr
        result.empty += not line.codes
        result.longest = max(result.longest, len(line.codes))
        for code in line.codes:
            kind, value = glyph(code)
            (result.font if kind == "font" else result.rom)[value] += 1
    for cell in sorted(result.font):
        if not inside(cell, *result.image):
            result.outside.append(f"cell {cell} (code {FONT_FIRST + cell:04x})")
    return result


def disc_files(disc: Disc) -> tuple[bytes, bytes]:
    """Files 0xAB and 0xAC of directory (4, 0), at their index sizes (cd_get_file_size)."""
    return tuple(disc.data(disc.slot(*DIRECTORY, file)) for file in (TEXT_FILE, FONT_FILE))


def report(results: dict[int, TextCount], digests: set[bytes]) -> str:
    lines = [
        "staff-roll text: field_staff_roll_write_line, directory (4, 0) file 0xab; font image file 0xac",
        f"  texts decoded: {len(results)} ({len(digests)} distinct)",
    ]
    for disc, r in sorted(results.items()):
        lines.append(
            f"  disc {disc}: {r.size} bytes, {r.lines} lines ({r.cr} end at a CR, {r.full} at "
            f"{LINE_CODES} codes before it), {r.empty} empty, longest {r.longest} codes"
        )
        cells = sum(inside(cell, *r.image) for cell in range(FONT_COUNT))
        lines.append(
            f"    glyph codes: {sum(r.font.values()) + sum(r.rom.values())}; font cells "
            f"{sum(r.font.values())} ({len(r.font)} distinct, highest "
            f"{max(r.font, default='none')}), kanji ROM codes {sum(r.rom.values())} "
            f"({len(r.rom)} distinct)"
        )
        lines.append(
            f"    file 0xac image {r.image[0]}x{r.image[1]} at (380, 100) holds {cells} of the "
            f"{FONT_COUNT} cells; used cells outside it: {len(r.outside)}"
        )
        lines += [f"      {cell}" for cell in r.outside]
    unknown = [f"disc {disc} {u}" for disc, r in sorted(results.items()) for u in r.unknown]
    lines.append(f"  unknown/undecodable: {len(unknown)}")
    lines += [f"    {u}" for u in unknown]
    return "\n".join(lines)


def listing(disc: Disc) -> None:
    """Print one disc's lines: offset, then each code (font cells as f:cell)."""
    text, _ = disc_files(disc)
    for line in decode(text):
        codes = " ".join(
            f"f:{value}" if kind == "font" else f"{value:04x}"
            for kind, value in map(glyph, line.codes)
        )
        print(f"+{line.offset:04x}: {codes}" + ("" if line.cr else " (no CR)"))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--sweep", action="store_true", help="aggregate decode of both discs")
    parser.add_argument("--list", action="store_true", help="print one disc's lines")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    args = parser.parse_args(argv)
    if not (args.sweep or args.list):
        parser.error("choose --sweep or --list")
    if args.list:
        listing(discs()[args.disc - 1])
    if not args.sweep:
        return 0
    results, digests = {}, set()
    for disc in discs():
        text, tim = disc_files(disc)
        digests.add(hashlib.sha256(text).digest())
        results[disc.number] = count(text, tim)
    print(report(results, digests))
    return 1 if any(r.unknown or r.outside for r in results.values()) else 0


if __name__ == "__main__":
    raise SystemExit(main())
