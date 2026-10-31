"""The port passes windows the active language's translations and draws its letters."""
import ctypes
import struct
import tempfile
import unittest
from pathlib import Path

from tests import port_native
from tools import translations

# The translations' region (xem_core::locale::BASE) and the RAM the test's
# texts, table, font and the original's special glyph rows lie in.
LOCALE = 0x20000000
LOCALE_BYTES = 0x10000
TEXT = 0x80010000
TABLE = 0x80011000
FONT = 0x80012000
SPECIAL = 0x800501D0
FIRST = 0x10

# A text with a control of each length and a two-byte glyph: "Hi" 0F 03 r e
# 0F 04 FE 3C 0F 00 30 01 "!" 00.
ORIGINAL = bytes([0x27, 0x45, 0x0F, 0x03, 2, 5, 0x0F, 0x04, 0xFE, 0x3C, 0x0F, 0x00, 0x30, 0x01, 0x57, 0x00])
TRANSLATION = bytes([0x32, 0x45, 0x49, 0xFF, 0x20, 0x00])  # "Sim" + the first added letter


def _map_locale():
    libc = ctypes.CDLL(None, use_errno=True)
    libc.mmap.restype = ctypes.c_void_p
    libc.mmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_long]
    got = libc.mmap(LOCALE, LOCALE_BYTES, 3, 0x02 | 0x20 | 0x100000, -1, 0)
    if got != LOCALE:
        raise OSError(ctypes.get_errno(), "cannot map the translations' region")


def region(active, translations, glyphs):
    """The host's image (xem_core::locale::build): a header, one language,
    the glyphs' recipes, the language's records ascending by hash, then the
    texts. `translations`: (hash, bytes, columns, rows); `glyphs`: (base,
    drop, clear, accent rows)."""
    glyphs_at = 32 + 8
    records = glyphs_at + 28 * len(glyphs)
    text = records + 16 * len(translations)
    out = bytearray(text)
    struct.pack_into("<IIIII", out, 0, 0x31434C58, active, 1, LOCALE + glyphs_at, len(glyphs))
    struct.pack_into("<II", out, 32, LOCALE + records, len(translations))
    for n, (base, drop, clear, accent) in enumerate(glyphs):
        struct.pack_into("<BBHH11H", out, glyphs_at + 28 * n, base, 0, drop, clear, *accent)
    for n, (digest, data, columns, rows) in enumerate(sorted(translations)):
        struct.pack_into("<IIIBBH", out, records + 16 * n, digest & 0xFFFFFFFF, digest >> 32, LOCALE + len(out), columns, rows, 0)
        out += data
    return bytes(out)


class Locale(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "locale_support.c"
        # Stand-ins for the originals that keep what they were passed, a
        # font block at FONT and an original text table at TABLE.
        support.write_text('''#include "common.h"
#include "resident/window.h"
#include "xem/memory.h"
u32 test_message;
u32 test_text;
u32 test_glyph[2];
u16 test_rows[11];
s16 xem_original_window_queue_message(Window *window, s32 message) { test_message = (u32)message; return 1; }
s32 xem_original_window_render_text_line(u8 *text, void *image, s16 width, s32 flags) { test_text = (u32)(XemUintptr)text; return 8; }
void xem_original_window_insert_text(Window *window, u8 *text) { test_text = (u32)(XemUintptr)text; }
u8 xem_original_text_get_message_columns(u16 *table, s32 index) { return 40; }
u8 xem_original_text_get_message_rows(u16 *table, s32 index) { return 3; }
s32 xem_original_text_get_glyph_width(u16 first, u16 second) { return 3; }
extern u16 text_special_glyph_rows[11];
void xem_original_text_draw_glyph(s32 first, u16 second, u16 *image, s16 stride, s32 plane) {
    s32 row;
    test_glyph[0] = first;
    test_glyph[1] = second;
    for (row = 0; row < 11; row++) test_rows[row] = text_special_glyph_rows[row];
}
u16 *text_get_font(void) { return (u16 *)(XemUintptr)0x80012000u; }
u8 *text_get_resource_entry(u8 *resource, s32 index) { return resource + ((u16 *)resource)[index + 2]; }
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/locale.c", str(support)],
                                    symbols={"text_special_glyph_rows": SPECIAL})
        _map_locale()
        lib = cls.lib
        lib.xem_text_length.restype = ctypes.c_uint
        lib.xem_text_length.argtypes = [ctypes.c_void_p]
        lib.window_queue_message.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.window_render_text_line.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_short, ctypes.c_int]
        lib.window_insert_text.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        lib.text_get_message_columns.restype = ctypes.c_ubyte
        lib.text_get_message_columns.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.text_get_message_rows.restype = ctypes.c_ubyte
        lib.text_get_message_rows.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.text_get_glyph_width.argtypes = [ctypes.c_ushort, ctypes.c_ushort]
        lib.text_draw_glyph.argtypes = [ctypes.c_int, ctypes.c_ushort, ctypes.c_void_p, ctypes.c_short, ctypes.c_int]

    def word(self, name):
        return ctypes.c_uint.in_dll(self.lib, name).value

    def setUp(self):
        port_native.clear()
        ctypes.memset(LOCALE, 0, LOCALE_BYTES)
        port_native.write(TEXT, ORIGINAL)
        # A table of one entry (count, 0, offsets, then the (columns, rows) pairs): ORIGINAL.
        port_native.write(TABLE, struct.pack("<HHHH", 1, 0, 10, 10 + len(ORIGINAL)) + bytes([40, 3]) + ORIGINAL)
        # The font block: halfword 1 the glyphs' offset (14), 6 the first code;
        # glyph 0x3D ("a") rows 3-9 drawn, one bit each.
        font = bytearray(14 + 22 * 0x50)
        struct.pack_into("<7H", font, 0, 0, 14, 0xFE, 0, 0x51, 0, FIRST)
        for row in range(3, 10):
            struct.pack_into("<H", font, 14 + 22 * (0x3D - FIRST) + 2 * row, 0x0100 | row)
        port_native.write(FONT, bytes(font))
        port_native.write(SPECIAL, struct.pack("<11H", *range(0xA0, 0xAB)))

    def install(self, active, columns=50, rows=5, glyphs=((0x3D, 1 << 4, 1 << 9, (0x10, 0x20) + (0,) * 9),)):
        digest = translations.fnv1a64(ORIGINAL)
        data = region(active, [(digest, TRANSLATION, columns, rows)], glyphs)
        ctypes.memmove(LOCALE, data, len(data))

    def translated(self):
        return LOCALE + 40 + 28 + 16

    def test_a_text_is_walked_as_the_window_walks_it_and_named_as_the_tools_name_it(self):
        self.assertEqual(self.lib.xem_text_length(TEXT), len(ORIGINAL))
        self.assertEqual(translations.text_bytes(ORIGINAL + b"\x99", 0), ORIGINAL)
        # Without its 00 before the end of RAM, or outside RAM: no name.
        port_native.write(0x801FFFFE, b"\x27\x45")
        self.assertEqual(self.lib.xem_text_length(0x801FFFFE), 0)
        self.assertEqual(self.lib.xem_text_length(LOCALE), 0)
        port_native.fill(0x80100000, 5000, 0x27)
        self.assertEqual(self.lib.xem_text_length(0x80100000), 0, "longer than 4 KiB")

    def test_without_an_active_language_every_function_is_the_originals(self):
        for active in (None, 0, 2):
            if active is not None:
                self.install(active)
            self.lib.window_queue_message(None, TEXT)
            self.lib.window_render_text_line(TEXT, None, 36, 0)
            self.assertEqual((self.word("test_message"), self.word("test_text")), (TEXT, TEXT), active)
            self.assertEqual((self.lib.text_get_message_columns(TABLE, 0), self.lib.text_get_message_rows(TABLE, 0)), (40, 3))

    def test_the_active_language_passes_its_translation_and_the_original_where_it_has_none(self):
        self.install(1)
        self.lib.window_queue_message(None, TEXT)
        self.assertEqual(self.word("test_message"), self.translated())
        self.assertEqual(ctypes.string_at(self.translated(), len(TRANSLATION)), TRANSLATION)
        for call in (lambda t: self.lib.window_render_text_line(t, None, 36, 0), lambda t: self.lib.window_insert_text(None, t)):
            call(TEXT)
            self.assertEqual(self.word("test_text"), self.translated())
            port_native.write(TEXT + 0x100, ORIGINAL[:-2] + b"\x58\x00")  # another text
            call(TEXT + 0x100)
            self.assertEqual(self.word("test_text"), TEXT + 0x100)
        self.lib.window_queue_message(None, 0)
        self.assertEqual(self.word("test_message"), 0, "no text")

    def test_a_field_messages_window_takes_what_its_translation_needs_beyond_the_tables(self):
        self.install(1, columns=50, rows=5)
        self.assertEqual((self.lib.text_get_message_columns(TABLE, 0), self.lib.text_get_message_rows(TABLE, 0)), (50, 5))
        self.install(1, columns=30, rows=2)
        self.assertEqual((self.lib.text_get_message_columns(TABLE, 0), self.lib.text_get_message_rows(TABLE, 0)), (40, 3))

    def test_an_added_letter_is_narrow_and_drawn_from_its_base_with_its_accent_through_the_special_glyph(self):
        self.install(0)  # installed letters draw whichever language is active
        self.assertEqual(self.lib.text_get_glyph_width(0xFF, 0x20), 2)
        self.assertEqual(self.lib.text_get_glyph_width(0xFF, 0x21), 3, "past the glyphs installed")
        self.assertEqual(self.lib.text_get_glyph_width(0xFF, 0xFF), 3, "the original's special glyph")
        self.assertEqual(self.lib.text_get_glyph_width(0xFE, 0x20), 3)
        before = port_native.read(SPECIAL, 22)
        self.lib.text_draw_glyph(0xFF, 0x20, None, 8, 0)
        self.assertEqual(tuple((ctypes.c_uint * 2).in_dll(self.lib, "test_glyph")), (0xFF, 0xFF))
        rows = list((ctypes.c_ushort * 11).in_dll(self.lib, "test_rows"))
        # "a" rows 3-9 with row 4 taken out: 3 moves to 4, 0-2 down to 1-3,
        # row 0 blank; row 9 blanked; the accent over rows 0 and 1.
        base = [0, 0, 0] + [0x0100 | r for r in range(3, 10)] + [0]
        expected = [0] + base[:4] + base[5:]
        expected[9] = 0
        expected[0] |= 0x10
        expected[1] |= 0x20
        self.assertEqual(rows, expected)
        self.assertEqual(port_native.read(SPECIAL, 22), before, "the special glyph's rows put back")
        self.lib.text_draw_glyph(0x30, 0x41, None, 8, 0)
        self.assertEqual(self.word("test_glyph"), 0x30, "any other glyph is the original's")
