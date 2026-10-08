"""Synthetic text bytes exercise the recovered staff-roll line reader; they
describe no original text."""

import re
import struct
import unittest
from pathlib import Path

from tools.analysis.staff_roll import (
    DIRECTORY,
    FONT_COUNT,
    FONT_FILE,
    FONT_FIRST,
    LINE_CODES,
    TEXT_FILE,
    Line,
    StaffRollError,
    count,
    decode,
    font_cell,
    glyph,
    inside,
    report,
    tim_image,
)

ROOT = Path(__file__).resolve().parents[1]


def codes(*values: int) -> bytes:
    return struct.pack(f">{len(values)}H", *values)


def tim(width: int, height: int, *, clut: bool = True) -> bytes:
    """A TIM of zero pixels: 8-bit with a 256-colour CLUT, or 16-bit."""
    out = struct.pack("<II", 0x10, 9 if clut else 2)
    if clut:
        out += struct.pack("<I4H", 12 + 2 * 256, 0, 0x1E0, 256, 1) + bytes(2 * 256)
    pixels = 2 * width * height
    return out + struct.pack("<I4H", 12 + pixels, 0, 0, width, height) + bytes(pixels)


def function(path: str, head: str) -> str:
    text = (ROOT / path).read_text()
    body = text[text.index(head) :]
    return body[: body.index("\n}\n")]


class LineTests(unittest.TestCase):
    def test_lines_end_at_a_cr_and_codes_are_big_endian(self):
        lines = decode(b"\r" + codes(0x8140, 0x8541) + b"\r" + codes(0x889F) + b"\r")
        self.assertEqual([line.codes for line in lines], [(), (0x8140, 0x8541), (0x889F,)])
        self.assertEqual([(line.offset, line.length) for line in lines], [(0, 1), (1, 5), (6, 3)])
        self.assertTrue(all(line.cr for line in lines))

    def test_a_cr_as_second_byte_belongs_to_its_code(self):
        self.assertEqual(decode(b"\x81\r\r"), [Line(0, (0x810D,), 3, True)])

    def test_a_line_of_28_codes_leaves_its_cr_to_the_next(self):
        full = codes(*range(0x8140, 0x8140 + LINE_CODES))
        lines = decode(full + b"\r" + codes(0x8540) + b"\r")
        self.assertEqual(
            [(len(line.codes), line.length, line.cr) for line in lines],
            [(28, 56, False), (0, 1, True), (1, 3, True)],
        )
        self.assertLessEqual(2 * LINE_CODES + 1, 0x40)  # func_800AC0F0 copies 0x40 bytes

    def test_reading_stops_when_no_bytes_are_left(self):
        self.assertEqual(decode(b""), [])
        self.assertEqual([line.length for line in decode(b"\r\r")], [1, 1])

    def test_a_line_past_the_end_of_the_text_fails(self):
        for data, offset in (
            (codes(0x8140), 0),
            (b"\r\x81", 1),
            (b"\r" + codes(0x8140) + b"\x82", 3),
        ):
            with self.assertRaises(StaffRollError) as caught:
                decode(data)
            self.assertEqual(caught.exception.offset, offset)


class GlyphTests(unittest.TestCase):
    def test_font_codes_and_rom_codes(self):
        self.assertEqual(glyph(0x853F), ("rom", 0x853F))
        self.assertEqual(glyph(0x8540), ("font", 0))
        self.assertEqual(glyph(0x887F), ("font", 0x33F))
        self.assertEqual(glyph(0x8880), ("rom", 0x8880))
        self.assertEqual(glyph(0x0041), ("rom", 0x0041))

    def test_font_cells_fill_rows_of_seven(self):
        self.assertEqual(font_cell(0), (0x380, 0x100))
        self.assertEqual(font_cell(6), (0x380 + 54, 0x100))
        self.assertEqual(font_cell(7), (0x380, 0x110))
        self.assertEqual([c for c in range(FONT_COUNT) if inside(c, 64, 256)], list(range(112)))
        self.assertEqual(sum(inside(c, 62, 32) for c in range(FONT_COUNT)), 12)

    def test_tim_image(self):
        self.assertEqual(tim_image(tim(64, 256)), (0, 0, 64, 256))
        self.assertEqual(tim_image(tim(8, 4, clut=False)), (0, 0, 8, 4))
        with self.assertRaises(StaffRollError):
            tim_image(bytes(8))


class SweepTests(unittest.TestCase):
    def test_counts_and_cells_outside_the_image(self):
        result = count(b"\r" + codes(0x8140, 0x8540, 0x85B0) + b"\r", tim(64, 256))
        self.assertEqual(
            (result.lines, result.cr, result.full, result.empty, result.longest), (2, 2, 0, 1, 3)
        )
        self.assertEqual((result.font, result.rom), ({0: 1, 112: 1}, {0x8140: 1}))
        self.assertEqual(result.outside, ["cell 112 (code 85b0)"])
        self.assertIn("used cells outside it: 1", report({1: result}, {b""}))

    def test_undecodable_text_is_reported(self):
        result = count(b"\x81", tim(64, 256))
        self.assertEqual(
            result.unknown, ["file 0xab +0x0: glyph code runs past the end of the text"]
        )
        self.assertIn("unknown/undecodable: 1", report({1: result}, {b""}))


class SourceTests(unittest.TestCase):
    """The reader follows the recovered field code."""

    def test_constants_follow_field_glyph_h(self):
        header = (ROOT / "decomp/src/field/field_glyph.h").read_text()
        defines = dict(re.findall(r"#define (GLYPH_\w+) (0x[0-9A-F]+|\d+)\n", header))
        self.assertEqual(int(defines["GLYPH_OWN_FIRST"], 0), FONT_FIRST)
        self.assertEqual(int(defines["GLYPH_OWN_COUNT"], 0), FONT_COUNT)
        self.assertEqual(int(defines["GLYPH_LINE_CELLS"], 0), LINE_CODES)

    def test_lines_codes_and_cells_follow_the_c(self):
        path = "decomp/src/field/field_800A9274.c"
        line = function(path, "u8 *func_800AC0F0(")
        self.assertRegex(line, r"if \(D_800AF780 <= 0\) \{\s+count = 0;")
        self.assertRegex(line, r"count < GLYPH_LINE_CELLS; count\+\+")
        self.assertRegex(line, r"if \(line\[used\] == '\\r'\) \{\s+used\+\+;\s+break;")
        self.assertRegex(line, r"glyph = func_800ABFDC\(&line\[used\], &own\);\s+used \+= 2;")
        self.assertIn("source.x = glyph % 7 * 9 + 0x380;", line)
        self.assertIn("source.y = glyph / 7 * 16 + 0x100;", line)
        self.assertIn("D_800AF780 -= used;", line)
        self.assertIn("code = text[1] | (text[0] << 8);", function(path, "s32 func_800ABFDC("))
        loader = function(path, "void func_800AC308(")
        self.assertIn(f"func_80028470({DIRECTORY[0]}, {DIRECTORY[1]});", loader)
        self.assertIn(f"D_800AF780 = func_80028738(0x{TEXT_FILE:X});", loader)
        self.assertIn(f"func_800295D8(0x{FONT_FILE:X}, D_800AF784, 0, 0x80);", loader)
        self.assertIn(
            "func_80070340(D_800AF784, 0x380, 0x100, 0, 0x1FF, 0, 0);",
            function(path, "void func_800ACB90("),
        )


if __name__ == "__main__":
    unittest.main()
