"""tools/translations.py: markup, line wrapping and the fit of a translation
in its original's windows (docs/runtime.md, Languages). The texts and the
scripts are made up here; none is original."""
import struct
import unittest

from tools import translations as tr
from tools.analysis import events, field

EXTRA = {c: tr.EXTRA_GLYPHS + n for n, c in enumerate("áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ")}
FRENCH = {c: tr.EXTRA_GLYPHS + n for n, c in enumerate("àâçéèêëîïôùûüÀÂÇÉÈÊËÎÏÔÙÛÜ")}


def text(markup, columns=52, rows=4, group="field messages", choice=None):
    return tr.Text("f001-0000", "0" * 16, markup, columns, rows, group, choice=choice)


class Markup(unittest.TestCase):
    def test_bytes_render_as_markup_and_encode_back(self):
        raw = bytes([0x25, 0x41, 0x45, 0x01, 0x58, 0x20, 0x0F, 0x00, 10, 0x02, 0x01, 0x0F, 0x05, 128, 0x6D, 0xFE, 0x3C, 0x00])
        markup = tr.render(raw)
        self.assertEqual(markup, 'Fei{n}"A{wait 10}{p}{name 128}☆{FE3C}')
        # A page's skipped new line is not written back.
        self.assertEqual(tr.encode(markup), raw[:9] + bytes([0x02]) + raw[11:])
        self.assertEqual(tr.text_bytes(raw + b"\x41", 0), raw)

    def test_the_added_letters_are_narrow_two_byte_glyphs(self):
        self.assertEqual(tr.encode("Ação", EXTRA), bytes([0x20, 0xFF, 0x2B, 0xFF, 0x23, 0x4B, 0]))
        self.assertEqual(tr.width(tr.tokens("Ação", EXTRA)), 8)
        with self.assertRaises(tr.MarkupError):
            tr.tokens("Ação")
        for bad in ("a;b", "{wait}", "{wait 300}", "{bold}", "a}", "{"):
            with self.assertRaises(tr.MarkupError, msg=bad):
                tr.tokens(bad, EXTRA)


class Fit(unittest.TestCase):
    def test_long_lines_wrap_at_spaces_and_long_pages_split_where_confirm_ends_the_text(self):
        original = text('Fei{n}"Hello there."', columns=20, rows=3)
        wrapped = tr.wrap(original, 'Fei{n}"Olá, tudo bem com você hoje? Que bom te ver de novo por aqui."', EXTRA)
        pages = [page.split("{n}") for page in wrapped.split("{p}")]
        self.assertEqual(pages[0][0], "Fei", "the speaker's line kept")
        self.assertTrue(all(len(line) <= 10 for page in pages for line in page), wrapped)
        self.assertTrue(all(len(page) <= 3 for page in pages), wrapped)
        self.assertEqual(tr.problems(original, wrapped, EXTRA), [])

    def test_a_line_never_begins_with_the_mark_french_sets_after_a_space(self):
        original = text("Hello", columns=16, rows=8)
        wrapped = tr.wrap(original, "Tu viens ? Oui ! Va : vite", FRENCH)
        # Broken at every space, "Tu viens" would fill the first line and "?" begin the next.
        self.assertEqual(wrapped.split("{n}"), ["Tu", "viens ?", "Oui !", "Va :", "vite"])
        self.assertEqual(tr.problems(original, wrapped, FRENCH), [])

    def test_a_broken_line_keeps_its_indent_past_a_control_that_shows_nothing(self):
        original = text("{speed 255}    One{n}    Two", columns=16, rows=3)
        wrapped = tr.wrap(original, "{speed 255}    First of all{n}    Two", EXTRA)
        self.assertEqual(wrapped.split("{n}"), ["{speed 255}    First", "of all", "    Two"])
        # A control ahead of a word with no spaces between is the word's.
        self.assertEqual(tr.wrap(original, "{speed 255}First of them all", EXTRA).split("{n}"), ["{speed 255}First of", "them all"])

    def test_a_timed_text_a_choice_and_a_menu_text_keep_their_pages(self):
        for original in (text("One{wait 5} two{done 30}", columns=12, rows=1),
                         text("Go?{n} Yes{n} No", columns=12, rows=3, choice=(1, 2)),
                         text("Potion", columns=12, rows=1, group="system data")):
            long = "Uma frase longa demais para a janela inteira"
            if "{done" in original.markup:
                long += "{wait 5}{done 30}"
            wrapped = tr.wrap(original, long, EXTRA)
            self.assertNotIn("{p}", wrapped, original.markup)
            self.assertTrue(tr.problems(original, wrapped, EXTRA), original.markup)

    def test_controls_keep_their_timing_order_and_insertions(self):
        original = text("{name 128} got {item}!{wait 10} Wow{done 20}", columns=80)
        self.assertEqual(tr.problems(original, "{item} para {name 128}!{wait 10} Uau{done 20}", EXTRA), [])
        self.assertIn("timing", tr.problems(original, "{item} {name 128}! Uau{wait 10}{wait 10}{done 20}", EXTRA)[0])
        self.assertIn("insertions", tr.problems(original, "{item}!{wait 10} Uau{done 20}", EXTRA)[0])
        self.assertIn("page(s) became 2", tr.problems(original, "{name 128} {item}!{wait 10}{p}Uau{done 20}", EXTRA)[0])

    def test_a_choice_keeps_the_lines_of_its_last_page(self):
        original = text("Hi{p}Go?{n}  Yes{n}  No", rows=3, choice=(1, 2))
        self.assertEqual(tr.problems(original, "Oi{p}Vamos?{n}  Sim{n}  Não", EXTRA), [])
        self.assertIn("choice", tr.problems(original, "Oi{p}Vamos? Sim{n}  Não", EXTRA)[0])

    def test_a_menu_word_or_row_takes_the_small_fonts_letters_alone_in_its_slots(self):
        word = tr.Text("mc223-0000", "0" * 16, "", 8, 1, "menu commands", small="word")
        self.assertEqual(tr.problems(word, "Pre%", EXTRA), [])
        self.assertEqual(tr.problems(word, "Agi.", EXTRA), [])
        self.assertIn("small font", tr.problems(word, "Pré%", EXTRA)[0])
        self.assertIn("small font", tr.problems(word, "A%b", EXTRA)[0])
        self.assertIn("at most 8", tr.problems(word, "Ataque", EXTRA)[0])
        row = tr.Text("mr000-0000", "0" * 16, "Go on", 12, 1, "menu letter rows", small="row")
        self.assertEqual(tr.problems(row, "Va em", EXTRA), [])
        self.assertIn("small font", tr.problems(row, "Vá", EXTRA)[0])
        self.assertIn("small font", tr.problems(row, "Va%", EXTRA)[0])


class Entries(unittest.TestCase):
    def test_an_entry_is_named_by_its_texts_hash_carried_on_over_its_place(self):
        raw = bytes([0x25, 0x41, 0x45, 0x00])
        self.assertEqual(tr.place_key(raw, 3, 0x1234, 9), tr.fnv1a64(raw + bytes([3, 0, 0x34, 0x12, 9, 0])))
        self.assertNotEqual(tr.place_key(raw, 3, 0x1234, 9), tr.place_key(raw, 4, 0x1234, 9))
        self.assertNotEqual(tr.place_key(raw, 3, 0x1234, 9), tr.fnv1a64(raw))

    def test_a_line_translates_a_text_everywhere_or_in_one_entry_and_may_translate_it_to_nothing(self):
        line = tr.LINE_RE.match("f222-0304 0123456789abcdef@fedcba9876543210 Second Tune")
        self.assertEqual((line["where"], line["hash"], line["entry"], line["text"]), ("f222-0304", "0123456789abcdef", "fedcba9876543210", "Second Tune"))
        everywhere = tr.LINE_RE.match("f222-0293 0123456789abcdef First Tune")
        self.assertEqual((everywhere["entry"], everywhere["text"]), (None, "First Tune"))
        for nothing in ("md053-0406 0123456789abcdef", "md053-0407 0123456789abcdef@fedcba9876543210"):
            self.assertIsNone(tr.LINE_RE.match(nothing)["text"])
        self.assertIsNone(tr.LINE_RE.match("f222-0304 0123456789abcdef@12 Tune"))
        self.assertEqual(tr.shipped_line("f222-0304", "0" * 16, "Tune", "1" * 16), "f222-0304 " + "0" * 16 + "@" + "1" * 16 + " Tune")
        self.assertEqual(tr.shipped_line("md053-0406", "0" * 16, ""), "md053-0406 " + "0" * 16)
        # A translator's line: its flags in brackets, `here` for the entry alone.
        work = [tr.WORK_RE.match(line) for line in ("f222-0304 [here] Second Tune", "f222-0293 [w26 r4] First Tune", "f222-0293 First", "md053-0406 [here]")]
        self.assertEqual([(m["where"], tr.HERE in (m["flags"] or "").split(), m["text"]) for m in work],
                         [("f222-0304", True, "Second Tune"), ("f222-0293", False, "First Tune"), ("f222-0293", False, "First"), ("md053-0406", True, None)])

    def test_an_entrys_text_takes_that_entrys_window(self):
        tight, roomy = tr.Place("md021-0019", 1, "a" * 16, 16, 1), tr.Place("f012-0003", 1, "b" * 16, 52, 4, own=(70, tr.ANY_ROWS))
        shared = tr.Text("f012-0003", "0" * 16, "Cancel", 16, 1, "field messages", [roomy, tight])
        entries = tr.entries_by_where([shared])
        self.assertEqual(sorted(entries), ["f012-0003", "md021-0019"])
        there = tr.at_entry(*entries["f012-0003"])
        self.assertEqual((there.where, there.columns, there.rows, there.own), ("f012-0003", 52, 4, (70, tr.ANY_ROWS)))
        self.assertEqual(tr.problems(there, "Take it all back", EXTRA), [])
        self.assertIn("at most 16", tr.problems(shared, "Take it all back", EXTRA)[0], "the text everywhere fits its tightest window")
        # The discs' two places of one entry: the tighter of the two.
        both = tr.at_entry(shared, [roomy, tr.Place("f012-0003", 2, "b" * 16, 40, 3)])
        self.assertEqual((both.columns, both.rows, both.own), (40, 3, (0, 0)))


class OwnLayout(unittest.TestCase):
    """A language that brings its own layout (tr.OWN_LAYOUT)."""

    def test_it_sets_its_own_waits_insertions_and_pages_and_takes_the_screen(self):
        original = tr.Text("f001-0000", "0" * 16, 'Bob{n}"Hi."{done 30}', 52, 4, "field messages", own=(tr.SCREEN_COLUMNS, tr.ANY_ROWS))
        reading = 'Bob{n}"Well met, {name 128}, on this day!{wait 10}"{p}One{n}two{n}three{n}four{n}five.'
        self.assertEqual(len(tr.problems(original, reading, EXTRA)), 5, "as a translation: timing, insertions, pages, lines, width")
        self.assertEqual(tr.problems(original, reading, EXTRA, True), [])
        self.assertEqual(tr.limits(original, True), (70, tr.ANY_ROWS))
        self.assertEqual(tr.limits(original), (52, 4))
        # Past the screen's columns a line is still too wide, and is broken as any other.
        wide = "A line of many words that runs on and on."
        self.assertIn("at most 70", tr.problems(original, wide, EXTRA, True)[0])
        self.assertEqual(tr.problems(original, tr.wrap(original, wide, EXTRA, True), EXTRA, True), [])
        self.assertIn("en-x-pw", tr.OWN_LAYOUT)

    def test_it_keeps_a_choices_lines_a_boxs_columns_and_a_window_a_script_gave_no_room(self):
        choice = tr.Text("f001-0000", "0" * 16, "Go?{n} Yes{n} No", 52, 3, "field messages", choice=(1, 2), own=(tr.SCREEN_COLUMNS, tr.ANY_ROWS))
        self.assertIn("choice", tr.problems(choice, "Shall we?{n} Yes", EXTRA, True)[0])
        self.assertEqual(tr.problems(choice, "Shall we go at once?{n} Aye{n} Nay", EXTRA, True), [])
        battle = tr.Text("be001-0000", "0" * 16, "No!", 24, 2, "battle events")
        self.assertIn("at most 24", tr.problems(battle, "No, not this again!", EXTRA, True)[0])
        name = tr.Text("s022-0005", "0" * 16, "Potion", 20, 1, "system data", own=(tr.LINE_COLUMNS, 0))
        self.assertEqual(tr.problems(name, "A Potion of some length", EXTRA, True), [])
        self.assertIn("at most 1", tr.problems(name, "Two{n}lines", EXTRA, True)[0])

    def test_a_glyph_a_language_marks_wide_takes_three_columns(self):
        marks = tr.Characters({"—": tr.EXTRA_GLYPHS, ";": tr.EXTRA_GLYPHS + 1}, wide="—")
        self.assertEqual([token.columns for token in tr.tokens("a—;", marks)], [2, 3, 2])
        self.assertEqual(tr.width(tr.tokens("Wha—!?", marks)), 13)
        self.assertEqual(tr.pages(tr.tokens("ab—{n}c", marks)), [[7, 2]])


def package(code: bytes, entries: list[list[int]]):
    """An event component of invented scripts (tests/test_events.py)."""
    rows = b"".join(struct.pack("<32H", *row, *[0] * (32 - len(row))) for row in entries)
    return field.event_package(bytes(0x80) + struct.pack("<I", len(entries)) + rows + code)


def immediate(value: int) -> bytes:
    return struct.pack("<H", 0x8000 | value)


class Windows(unittest.TestCase):
    """The windows a field script sizes (cf, d0, f4) and a battle script lays out (1a)."""

    def test_a_layout_sizes_only_what_it_gives_and_its_actors_next_messages_until_it_is_cleared(self):
        code = bytearray(b"\x00")  # +0000 end_slot: every event the rows leave at 0
        code += b"\xd0" + immediate(100) + immediate(80) + immediate(10) + immediate(0) + immediate(2)  # +0001: 30 columns, no portrait
        code += b"\xd2" + struct.pack("<HB", 0, 0)  # +000c message 0
        code += b"\xf4\x01"  # +0010 clears the layout
        code += b"\xd2" + struct.pack("<HB", 1, 0)  # +0012 message 1
        code += b"\xfe\x0d" + immediate(5)  # +0016 the actor takes a character
        code += b"\xcf" + bytes([0, 0, 0x10, 3])  # +001a: 48 columns, 3 rows
        code += b"\xd2" + struct.pack("<HB", 2, 0)  # +001f message 2
        code += b"\xd0" + immediate(60) + immediate(40) + immediate(0) + immediate(0) + immediate(0)  # +0023 places the window only
        code += b"\xd2" + struct.pack("<HB", 3, 0)  # +002e message 3
        code += b"\x00"  # +0032 end_slot
        # A second actor's messages are its own: no layout of the first reaches them.
        second = len(code)
        code += b"\xd2" + struct.pack("<HB", 4, 0) + b"\x00"
        scripts = package(bytes(code), [[1], [second]])
        self.assertEqual([tr.window_size(events.decode_instruction(scripts.bytecode, pc)) for pc in (1, 0x10, 0x1A, 0x23)],
                         [(30, 0, 2), (0, 0, 0), (48, 3, None), (0, 0, 0)])
        self.assertEqual(tr.sized_windows(scripts), {0: {(30, 0, False)}, 2: {(48, 3, True)}})
        # No layout that sizes a window: nothing to follow.
        self.assertEqual(tr.sized_windows(package(bytes(code[second:]), [[0]])), {})

    def test_a_size_a_script_ends_with_stands_when_the_actors_next_event_starts(self):
        code = bytearray(b"\x00")
        talk = len(code)  # event 2: a message, before any layout of its own
        code += b"\xd2" + struct.pack("<HB", 7, 0) + b"\x00"
        setup = len(code)  # event 0: sizes the window and ends
        code += b"\xcf" + bytes([0, 0, 8, 2]) + b"\x00"
        self.assertEqual(tr.sized_windows(package(bytes(code), [[setup, 0, talk]])), {7: {(0, 0, False), (24, 2, False)}})

    def test_a_field_messages_window_is_its_tables_unless_a_script_sizes_it(self):
        pair = (40, 3)
        # Its table's: a translation's lines may take 52 columns and 4 rows; the fixed box 72 by 4.
        self.assertEqual(tr.field_window(pair, 40, 3, None, False, True), (52, 4, False))
        self.assertEqual(tr.field_window(pair, 40, 3, None, True, False), (72, 4, False))
        self.assertEqual(tr.field_window(pair, 40, 3, {(0, 0, False)}, False, True), (52, 4, False), "a layout that only places it")
        # A script's 48 by 3: all of it without a portrait, 0x11 columns fewer and a portrait's 4 rows with one.
        self.assertEqual(tr.field_window(pair, 30, 3, {(48, 3, False)}, False, True), (48, 3, True))
        self.assertEqual(tr.field_window(pair, 30, 3, {(48, 3, True)}, False, True), (31, 3, True))
        self.assertEqual(tr.field_window(pair, 30, 2, {(48, 6, True)}, False, True), (31, 4, True))
        # The original's own text shows more than the portrait would leave: there is none.
        self.assertEqual(tr.field_window(pair, 40, 3, {(48, 3, True)}, False, True), (48, 3, True))
        # A size the original's text passes is not the window's; one of several that may stand is the tightest.
        self.assertEqual(tr.field_window(pair, 50, 3, {(48, 3, False)}, False, True), (52, 4, False))
        self.assertEqual(tr.field_window(pair, 20, 2, {(0, 0, False), (48, 3, False), (30, 0, False)}, False, True), (30, 3, True))
        self.assertEqual(tr.field_window(pair, 20, 2, {(tr.VARIABLE, 2, False)}, False, True), (0, 2, True), "a variable's: the original's own text")

    def test_a_battle_messages_window_is_the_layout_its_script_set_before_it(self):
        def packed(data: bytes) -> bytes:  # literal groups, as tests/test_events.py packs a component
            padded = data + bytes(-len(data) % 8)
            out = bytearray(struct.pack("<I", len(padded)))
            for start in range(0, len(padded), 8):
                out += b"\x00" + padded[start : start + 8]
            return bytes(out + b"\x00")

        def layout(width: int, rows: int) -> bytes:
            return b"\x1a" + immediate(0) + immediate(0) + immediate(width) + immediate(rows) + immediate(0)

        code = bytearray()
        code += b"\x18" + struct.pack("<HB", 0, 0)  # message 0 in the default 16 units by 8 rows
        code += layout(10, 2) + b"\x18" + struct.pack("<HB", 1, 0)  # 30 columns, 2 rows
        code += b"\x18" + struct.pack("<HB", 2, 0)  # the default again
        code += layout(8, 0) + b"\x1b\x03" + b"\x19" + struct.pack("<BHB", 3, 3, 0)  # 24 columns for a named actor's
        code += layout(20, 3) + b"\x18" + struct.pack("<HB", 1, 0)  # message 1 again, in a roomier window
        code += b"\x1a" + immediate(0) + immediate(0) + struct.pack("<H", 0x0010) + immediate(0) + immediate(0) + b"\x18" + struct.pack("<HB", 4, 0)  # a variable's
        code += b"\x00"
        script = bytes(0x40) + struct.pack("<I", 1) + struct.pack("<8H", *[0] * 8) + bytes(code)
        entries = [packed(script), packed(b"\x00")]
        archive = struct.pack("<I", 2) + struct.pack("<2I", 12, 12 + len(entries[0])) + b"".join(entries)

        class Disc:
            def slot(self, *place):
                return place

            def sectors(self, slot):
                return archive + bytes(64)

        self.assertEqual(tr.battle_windows(Disc()), {0: {0: (48, 4), 1: (30, 2), 2: (48, 4), 3: (24, 4)}})


if __name__ == "__main__":
    unittest.main()
