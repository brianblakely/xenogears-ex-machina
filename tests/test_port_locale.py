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
NAMES = 0x80058000  # game_data: its names first, 31 of 20 bytes
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


def region(active, translations, glyphs, entries=()):
    """The host's image (xem_core::locale::build): a header, one language,
    the glyphs' recipes, the language's records ascending by hash, its
    records of single entries ascending by name, then the texts.
    `translations` and `entries`: (hash or name, bytes, columns, rows);
    `glyphs`: (base, drop, clear, accent rows[, wide])."""
    glyphs_at = 32 + 16
    records = glyphs_at + 28 * len(glyphs)
    alone = records + 16 * len(translations)
    out = bytearray(alone + 16 * len(entries))
    struct.pack_into("<IIIII", out, 0, 0x32434C58, active, 1, LOCALE + glyphs_at, len(glyphs))
    struct.pack_into("<IIII", out, 32, LOCALE + records, len(translations), LOCALE + alone, len(entries))
    for n, (base, drop, clear, accent, *wide) in enumerate(glyphs):
        struct.pack_into("<BBHH11H", out, glyphs_at + 28 * n, base, *(wide or [0]), drop, clear, *accent)
    for n, (digest, data, columns, rows) in enumerate([*sorted(translations), *sorted(entries)]):
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
s32 xem_original_window_render_text_line(u8 *text, void *image, s16 width, s32 flags) {
    test_text = (u32)(XemUintptr)text;
    if (image != NULL) ((u16 *)image)[0] = 0x1111; /* a mark where the line starts */
    return 8 * 4 * 5; /* five narrow glyphs' width in pixels */
}
void xem_original_window_insert_text(Window *window, u8 *text) { test_text = (u32)(XemUintptr)text; }
u8 *xem_original_text_get_resource_entry(u8 *resource, s32 index) { return resource + ((u16 *)resource)[index + 2]; }
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
u32 test_loads;
u32 test_rects[16][4];
u32 test_syncs;
u32 test_frees;
void *heap_alloc(s32 size, s32 mode) { return (void *)(XemUintptr)0x80040000u; }
s32 heap_free(void *data) { test_frees++; return 0; }
int LoadImage(RECT *rect, u_long *p) {
    if (test_loads < 16) {
        test_rects[test_loads][0] = rect->x; test_rects[test_loads][1] = rect->y;
        test_rects[test_loads][2] = rect->w; test_rects[test_loads][3] = rect->h;
    }
    test_loads++;
    return 0;
}
int DrawSync(int mode) { test_syncs++; return 0; }
void *menu_state_current;
void xem_original_menu_load_resources(void) {}
void xem_original_item_shop_load_resources(void) {}
void xem_original_gear_shop_load_resources(void) {}
void xem_original_member_change_load_resources(void) {}
void xem_original_name_entry_load_resources(void) {}
s32 menu_field_block_part_images[20];
s32 menu_field_block_part_x_table[20];
s32 menu_detail_part_images[48];
s32 menu_detail_part_x_table[48];
u16 gear_shop_gear_values_frame_images[14];
u16 gear_shop_gear_values_frame_x_table[14];
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/locale.c", str(support)],
                                    symbols={"text_special_glyph_rows": SPECIAL, "game_data": NAMES})
        _map_locale()
        lib = cls.lib
        lib.xem_text_length.restype = ctypes.c_uint
        lib.xem_text_length.argtypes = [ctypes.c_void_p]
        lib.window_queue_message.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.window_render_text_line.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_short, ctypes.c_int]
        lib.window_insert_text.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        lib.text_get_resource_entry.restype = ctypes.c_void_p
        lib.text_get_resource_entry.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.text_get_message_columns.restype = ctypes.c_ubyte
        lib.text_get_message_columns.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.text_get_message_rows.restype = ctypes.c_ubyte
        lib.text_get_message_rows.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.text_get_glyph_width.argtypes = [ctypes.c_ushort, ctypes.c_ushort]
        lib.text_draw_glyph.argtypes = [ctypes.c_int, ctypes.c_ushort, ctypes.c_void_p, ctypes.c_short, ctypes.c_int]
        lib.xem_locale_menu_labels.argtypes = [ctypes.c_void_p]
        lib.xem_locale_menu_words.argtypes = [ctypes.c_void_p]

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
        return LOCALE + 48 + 28 + 16

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

    def test_a_table_entry_the_language_translates_alone_is_taken_translated_and_its_text_elsewhere_as_everywhere(self):
        """A table of three entries, the first two the same text: the language
        translates the text, and its second entry alone otherwise (named by
        the text's hash carried on over the entry's index, offset and the
        table's count, as the tools name it)."""
        other = ORIGINAL[:-2] + b"\x58\x00"
        offsets = [18, 18 + len(ORIGINAL), 18 + 2 * len(ORIGINAL)]
        table = struct.pack("<HHHHHH", 3, 0, *offsets, offsets[2] + len(other)) + bytes([40, 3] * 3) + ORIGINAL * 2 + other
        self.assertEqual(table[offsets[1] : offsets[1] + len(ORIGINAL)], ORIGINAL)
        port_native.write(TABLE, table)
        alone = bytes([0x2E, 0x4B, 0x00])  # "No"
        name = translations.place_key(ORIGINAL, 1, offsets[1], 3)
        self.assertEqual(name, translations.fnv1a64(ORIGINAL + struct.pack("<HHH", 1, offsets[1], 3)))
        everywhere = [(translations.fnv1a64(ORIGINAL), TRANSLATION, 50, 5)]
        data = region(1, everywhere, (), [(name, alone, 60, 6)])
        ctypes.memmove(LOCALE, data, len(data))
        there = LOCALE + 48 + 16 + 16 + len(TRANSLATION)
        self.assertEqual(ctypes.string_at(there, len(alone)), alone)
        self.assertEqual(self.lib.text_get_resource_entry(TABLE, 1), there, "the entry translated alone")
        self.assertEqual(self.lib.text_get_resource_entry(TABLE, 0), TABLE + offsets[0], "the same text in another entry")
        self.assertEqual(self.lib.text_get_resource_entry(TABLE, 2), TABLE + offsets[2])
        # The windows pass the entry's translation on as it is, and translate the text of the other entry.
        self.lib.window_queue_message(None, there)
        self.assertEqual(self.word("test_message"), there)
        self.lib.window_queue_message(None, TABLE + offsets[0])
        self.assertEqual(self.word("test_message"), LOCALE + 48 + 16 + 16)
        # Each window takes what its own translation needs.
        sizes = [(self.lib.text_get_message_columns(TABLE, n), self.lib.text_get_message_rows(TABLE, n)) for n in range(3)]
        self.assertEqual(sizes, [(50, 5), (60, 6), (40, 3)])
        # A language that translates no entry alone, and the original's text: every entry as it is.
        for data in (region(1, everywhere, ()), region(0, everywhere, (), [(name, alone, 60, 6)])):
            ctypes.memset(LOCALE, 0, LOCALE_BYTES)
            ctypes.memmove(LOCALE, data, len(data))
            self.assertEqual([self.lib.text_get_resource_entry(TABLE, n) for n in range(3)], [TABLE + offset for offset in offsets])

    def test_a_name_of_the_game_data_is_an_entry_by_its_slot(self):
        """The party's and the Gears' names are text in the game data, 20
        bytes a slot: slot n is entry n at 20 n of a table of 31, so the
        language respells one slot's name and leaves the same name of another."""
        name = bytes([0x32, 0x51, 0x45, 0x41, 0x4F, 0x00])  # "Stier"
        for slot in (17, 25):
            port_native.write(NAMES + 20 * slot, name)
        respelled = bytes([0x24, 0x48, 0x10]) + name  # "El Stier"
        data = region(1, [], (), [(translations.place_key(name, 25, 20 * 25, 31), respelled, 16, 1)])
        ctypes.memmove(LOCALE, data, len(data))
        there = LOCALE + 48 + 16
        for call in (lambda t: self.lib.window_insert_text(None, t), lambda t: self.lib.window_render_text_line(t, None, 36, 0)):
            call(NAMES + 20 * 25)
            self.assertEqual(self.word("test_text"), there)
            call(NAMES + 20 * 17)
            self.assertEqual(self.word("test_text"), NAMES + 20 * 17, "the same name in another slot")
            port_native.write(TEXT + 0x200, name)
            call(TEXT + 0x200)
            self.assertEqual(self.word("test_text"), TEXT + 0x200, "the same text outside the names")
        # A name the player changed is another text.
        port_native.write(NAMES + 20 * 25, bytes([0x32, 0x51, 0x45, 0x00]))
        self.lib.window_insert_text(None, NAMES + 20 * 25)
        self.assertEqual(self.word("test_text"), NAMES + 20 * 25)

    def test_an_added_letter_is_narrow_and_drawn_from_its_base_with_its_accent_through_the_special_glyph(self):
        self.install(0, glyphs=((0x3D, 1 << 4, 1 << 9, (0x10, 0x20) + (0,) * 9), (0x13, 0, 0, (0,) * 11, 1)))
        self.assertEqual(self.lib.text_get_glyph_width(0xFF, 0x21), 3, "a glyph its recipe makes wide")
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

    def test_a_menus_command_labels_the_language_translates_are_drawn_into_the_free_page_and_their_entries_show_them(self):
        """Sheet entries 0x123-0x149 (the command labels), by the hash of the
        entry's bytes: a translated one drawn with the text renderer into
        page (448, 256), its entry made one part showing it with the text
        palette, two rows up; an untranslated one left as it was."""
        sheet = 0x80050000
        count = 0x150
        offsets = 4 + 2 * count
        entry = struct.pack("<hh", 2, 0) + struct.pack("<HhHHhhxxxxhhhHHBB", 0, 148, 32, 9, -55, -39, 0, 272, 509, 448, 148, 0, 0) \
            + struct.pack("<HhHHhhxxxxhhhHHBB", 0x20, 148, 8, 9, -60, -39, 0, 272, 509, 448, 148, 0, 0)
        other = struct.pack("<hh", 1, 0) + struct.pack("<HhHHhhxxxxhhhHHBB", 0x40, 157, 19, 9, 10, 20, 0, 272, 509, 448, 148, 0, 0)
        table = bytearray(struct.pack("<HH", count, 0) + bytes(2 * count))
        for label in range(count):
            struct.pack_into("<H", table, 4 + 2 * label, offsets + (len(entry) if label == 0x12C else 0))
        port_native.write(sheet, bytes(table) + entry + other)
        digest = translations.fnv1a64(entry)
        data = region(1, [(digest, TRANSLATION, 10, 1)], ())
        ctypes.memmove(LOCALE, data, len(data))
        for name in ("test_loads", "test_syncs", "test_frees"):
            ctypes.c_uint.in_dll(self.lib, name).value = 0
        self.lib.xem_locale_menu_labels(sheet)
        # Every entry but 0x12C names the first entry, which is translated: each is drawn and patched once.
        self.assertGreater(self.word("test_loads"), 0)
        self.assertEqual(self.word("test_syncs"), self.word("test_loads"), "each transfer waited for")
        self.assertEqual(self.word("test_frees"), 1, "the line image freed")
        rect = tuple((ctypes.c_uint * 4).in_dll(self.lib, "test_rects"))
        self.assertEqual(rect, (448, 256, 40, 13), "the page's first line, 160 pixels wide")
        patched = port_native.read(sheet + offsets, 4 + 0x1C)
        parts, _, u, v, w, h, x, y, mode, clut_x, clut_y, page_x, page_y, flip_x, flip_y = struct.unpack("<hhHhHHhhxxxxhhhHHBB", patched)
        self.assertEqual((parts, u, v, w, h, x, y), (1, 0, 0, 160, 13, -60, -41))
        self.assertEqual((mode, clut_x, clut_y, page_x, page_y, flip_x, flip_y), (0, 0, 0x1D1, 448, 256, 0, 0))
        self.assertEqual(port_native.read(sheet + offsets + len(entry), len(other)), other, "the untranslated entry unchanged")
        # Without the active language nothing is drawn or changed.
        port_native.write(sheet + offsets, entry)
        ctypes.memmove(LOCALE + 4, struct.pack("<I", 0), 4)
        ctypes.c_uint.in_dll(self.lib, "test_loads").value = 0
        self.lib.xem_locale_menu_labels(sheet)
        self.assertEqual(self.word("test_loads"), 0)
        self.assertEqual(port_native.read(sheet + offsets, len(entry)), entry)

    def test_a_stat_panels_word_is_spelled_anew_with_the_small_fonts_letters_ending_where_it_ended(self):
        """Sheet entries 0xDE-0xF8 compose a word of the small font's letter
        cells (capitals from 0x0A, small letters from 0x24, the full stop
        0x91): a translation takes those cells, a part a letter, its last
        ending where the original's last ended; one with more letters than
        parts or a letter the font lacks leaves the word as it was."""
        sheet = 0x80060000
        count = 0x150
        part = lambda u, v, x, y: struct.pack("<HhHHhhxxxxhhhHHBB", u, v, 8, 8, x, y, 0, 304, 509, 479, 120, 0, 0)
        cells = {0x0A: part(0x10, 128, 0, 0), 0x25: part(0x20, 136, 0, 0), 0x91: part(0x30, 144, 0, 0)}
        word = struct.pack("<hh", 4, 0) + b"".join(part(0x40 + 8 * k, 152, x, 3) for k, x in enumerate((-12, -5, 1, 7)))
        table = bytearray(struct.pack("<HH", count, 0) + bytes(2 * count))
        body = bytearray()
        offsets = 4 + 2 * count
        for label in range(count):
            struct.pack_into("<H", table, 4 + 2 * label, offsets + len(body) if label in cells or label == 0xDE else offsets)
            if label in cells:
                body += struct.pack("<hh", 1, 0) + cells[label]
            elif label == 0xDE:
                at_word = offsets + len(body)
                body += word
        port_native.write(sheet, bytes(table) + body)
        digest = translations.fnv1a64(word)
        for text, expected in ((bytes([0x20, 0x3E, 0x14, 0]), "spelled"), (bytes([0x20] * 5 + [0]), "too long"),
                               (bytes([0x20, 0xFF, 0x20, 0]), "a letter the font lacks")):
            port_native.write(sheet + at_word, word)
            data = region(1, [(digest, text, 10, 1)], ())
            ctypes.memmove(LOCALE, data, len(data))
            self.lib.xem_locale_menu_words(sheet)
            after = port_native.read(sheet + at_word, len(word))
            if expected != "spelled":
                self.assertEqual(after, word, expected)
                continue
            parts = struct.unpack_from("<h", after)[0]
            spelled = [struct.unpack_from("<HhHHhh", after, 4 + 0x1C * k) for k in range(parts)]
            # A 7, b 6, . 3: 16 pixels ending at 7 + 6 = 13.
            self.assertEqual(parts, 3)
            self.assertEqual([(u, v, x, y) for u, v, _, _, x, y in spelled], [(0x10, 128, -3, 3), (0x20, 136, 4, 3), (0x30, 144, 10, 3)])
        # Without the active language the word stays.
        port_native.write(sheet + at_word, word)
        ctypes.memmove(LOCALE + 4, struct.pack("<I", 0), 4)
        self.lib.xem_locale_menu_words(sheet)
        self.assertEqual(port_native.read(sheet + at_word, len(word)), word)

    def test_a_stat_panels_word_ending_in_percent_keeps_its_percent(self):
        sheet = 0x80070000
        count = 0x150
        part = lambda u, v, x, y: struct.pack("<HhHHhhxxxxhhhHHBB", u, v, 8, 8, x, y, 0, 304, 509, 479, 120, 0, 0)
        cells = {0x0A: part(0x10, 128, 0, 0), 0x25: part(0x20, 136, 0, 0)}
        # "Hit%": its % (the rightmost part) first in memory, at x 19.
        percent = part(0x70, 200, 19, 2)
        word = struct.pack("<hh", 4, 0) + percent + part(0x40, 152, 2, 2) + part(0x48, 152, 7, 2) + part(0x50, 152, 12, 2)
        table = bytearray(struct.pack("<HH", count, 0) + bytes(2 * count))
        body = bytearray()
        offsets = 4 + 2 * count
        for label in range(count):
            struct.pack_into("<H", table, 4 + 2 * label, offsets + len(body) if label in cells or label == 0xDF else offsets)
            if label in cells:
                body += struct.pack("<hh", 1, 0) + cells[label]
            elif label == 0xDF:
                at_word = offsets + len(body)
                body += word
        port_native.write(sheet, bytes(table) + body)
        data = region(1, [(translations.fnv1a64(word), bytes([0x20, 0x3E, 0x5A, 0]), 8, 1)], ())
        ctypes.memmove(LOCALE, data, len(data))
        self.lib.xem_locale_menu_words(sheet)
        after = port_native.read(sheet + at_word, 4 + 0x1C * 3)
        self.assertEqual(struct.unpack_from("<h", after)[0], 3)
        spelled = [struct.unpack_from("<HhHHhh", after, 4 + 0x1C * k) for k in range(3)]
        # A 7 and b 6 end where the % begins (19); the % as it was.
        self.assertEqual([(u, v, x, y) for u, v, _, _, x, y in spelled], [(0x10, 128, 6, 2), (0x20, 136, 13, 2), (0x70, 200, 19, 2)])

    def test_a_menu_rows_letters_are_spelled_anew_from_its_first_x_its_last_letter_filling_the_slots_left(self):
        """The gear shop's FUEL and WEIGHT and the menus' Next LV are rows of
        single letters in their overlays' tables, a small font cell and an x a
        slot (src/decomp/src/ovl2602/gear_shop.c, slot39/menu_framework.c),
        named by the hash of the slots' cells: a translated row takes the
        cells from its first x, 8 pixels a letter (i and l 5, a letter before
        one 6), a space as wide, and the slots it leaves repeat its last
        letter in place; the loaders spell each row after the original's."""
        ctypes.c_uint.in_dll(self.lib, "menu_state_current").value = 0
        images = (ctypes.c_uint16 * 14).in_dll(self.lib, "gear_shop_gear_values_frame_images")
        xs = (ctypes.c_uint16 * 14).in_dll(self.lib, "gear_shop_gear_values_frame_x_table")
        codes = [0x11, 0x19, 0x3E, 0x0F, 0x1E, 0x0E, 0x15, 0x3E, 0x20, 0x0E, 0x12, 0x10, 0x11, 0x1D]
        places = [210, 218, 250, 210, 218, 226, 234, 250, 210, 218, 226, 234, 242, 250]
        fuel = struct.pack("<4H", *codes[3:7])
        weight = struct.pack("<6H", *codes[8:14])
        # "Al i" for FUEL (a letter before a narrow one, a space), "PESO" for WEIGHT.
        rows = [(translations.fnv1a64(fuel), bytes([0x20, 0x48, 0x10, 0x45, 0]), 8, 1),
                (translations.fnv1a64(weight), bytes([0x2F, 0x24, 0x32, 0x2E, 0]), 12, 1)]
        for active in (1, 0):
            images[:], xs[:] = codes, places
            data = region(active, rows, ())
            ctypes.memmove(LOCALE, data, len(data))
            self.lib.gear_shop_load_resources()
            if not active:
                self.assertEqual((list(images), list(xs)), (codes, places), "without the active language")
                continue
            # A 6 (before l), l 5, the space 8: A 210, l 216, i 229 and again; P E S O 8 apart and O again.
            self.assertEqual(list(images), codes[:3] + [0x0A, 0x2F, 0x2C, 0x2C] + codes[7:8] + [0x19, 0x0E, 0x1C, 0x18, 0x18, 0x18])
            self.assertEqual(list(xs), places[:3] + [210, 216, 229, 229] + places[7:8] + [210, 218, 226, 234, 234, 234])
        # The menus' three Next LV rows (s32 slots) share one translation;
        # one longer than its slots or with a letter the font lacks is left.
        block = (ctypes.c_int32 * 20).in_dll(self.lib, "menu_field_block_part_images")
        block_xs = (ctypes.c_int32 * 20).in_dll(self.lib, "menu_field_block_part_x_table")
        detail = (ctypes.c_int32 * 48).in_dll(self.lib, "menu_detail_part_images")
        detail_xs = (ctypes.c_int32 * 48).in_dll(self.lib, "menu_detail_part_x_table")
        next_lv = [0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F, 0x1F]
        digest = translations.fnv1a64(struct.pack("<8i", *next_lv))
        for text, expected in ((bytes([0x25, 0x3D, 0x48, 0x50, 0x3D, 0x49, 0]), "spelled"),
                               (bytes([0x25] * 9 + [0]), "too long"), (bytes([0x25, 0x5A, 0]), "a letter the font lacks")):
            block[:] = [0] * 10 + next_lv + [0, 0]
            block_xs[:] = [0] * 10 + [0x80, 0x88, 0x90, 0x98, 0x98, 0xA8, 0xB0, 0xB0] + [0, 0]
            detail[:] = [0] * 5 + next_lv + [0] * 16 + next_lv + [0] * 11
            detail_xs[:] = [0] * 5 + [0xB0] * 8 + [0] * 16 + [0x98] * 8 + [0] * 11
            data = region(1, [(digest, text, 16, 1)], ())
            ctypes.memmove(LOCALE, data, len(data))
            self.lib.menu_load_resources()
            rows = [(list(block[10:18]), block_xs[10:18]), (list(detail[5:13]), detail_xs[5:13]),
                    (list(detail[29:37]), detail_xs[29:37])]
            if expected != "spelled":
                self.assertEqual([cells for cells, _ in rows], [next_lv] * 3, expected)
                continue
            # "Faltam": F 8, a 6 (before l), l 5, t a m 8, m again twice.
            spelled = [0x0F, 0x24, 0x2F, 0x37, 0x24, 0x30, 0x30, 0x30]
            for (cells, at), first in zip(rows, (0x80, 0xB0, 0x98)):
                self.assertEqual(cells, spelled)
                self.assertEqual(list(at), [first + d for d in (0, 8, 14, 19, 27, 35, 35, 35)])
