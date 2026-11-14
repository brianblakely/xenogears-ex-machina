"""tools/perfect_works.py: Perfect Works' bytes as the language's markup, a
reading fitted to its entry's window, and a text's entries as translators'
lines (docs/runtime.md, Languages). The texts are made up here; none is the
original's or Perfect Works'."""
import unittest

from tools import perfect_works as pw
from tools import translations as tr

EXTRA = tr.language_characters(pw.LANGUAGE)


def text(where, markup, places, columns=52, rows=4, group="field messages", **more):
    """A text held by entries `places` (where, key[, own]), every window the same."""
    held = [tr.Place(place[0], 1, place[1], columns, rows, more.get("choice"), own=place[2] if len(place) > 2 else (70, tr.ANY_ROWS))
            for place in places]
    return tr.Text(where, f"{abs(hash(markup)) & 0xFFFFFFFFFFFFFFFF:016x}", markup, columns, rows, group, held, more.get("choice"),
                   own=held[0].own if held else (0, 0))


class Markup(unittest.TestCase):
    def test_the_marks_perfect_works_font_draws_become_the_languages_added_glyphs(self):
        # "Wha" 64 "!" 61 62 63, then the El's two halves before "Ab", and the wide blank.
        raw = bytes([0x36, 0x44, 0x3D, 0x64, 0x57, 0x61, 0x62, 0x63, 0x01, 0x3A, 0x3B, 0x20, 0x3E, 0x8C, 0x00])
        changed = {0x14, 0x3A, 0x3B, 0x61, 0x62, 0x63, 0x64, 0x72, 0x8C, 0x93}
        self.assertEqual(pw.markup(raw, changed), "Wha—!*;ö{n}ﭏAb␣")
        self.assertEqual(tr.encode(pw.markup(raw, changed), EXTRA)[-1], 0)
        # The dash, the blank and El advance a wide glyph's 3 columns; the others a letter's 2.
        widths = {token.char: token.columns for token in tr.tokens("—␣ﭏ*;ö", EXTRA)}
        self.assertEqual(widths, {"—": 3, "␣": 3, "ﭏ": 3, "*": 2, ";": 2, "ö": 2})

    def test_a_glyph_its_font_draws_anew_and_no_mark_names_is_refused(self):
        # 0x93 on are its icons in place of kana; a full stop and an arrow it only moved stay themselves.
        raw = bytes([0x20, 0x14, 0x72, 0x93, 0x00])
        marked = pw.markup(raw, {0x14, 0x72, 0x93})
        self.assertEqual(marked, "A.→{PW 93}")
        fitted, why = pw.fit(text("f001-0000", "A", [("f001-0000", "k")]), marked, EXTRA, False)
        self.assertEqual((fitted, why), (None, "unknown control {PW 93}"))
        # A lone half of El is no glyph of the language either.
        self.assertIsNone(pw.fit(text("f001-0000", "A", [("f001-0000", "k")]), pw.markup(bytes([0x3A, 0x20, 0]), {0x3A}), EXTRA, False)[0])


class Fit(unittest.TestCase):
    def test_a_reading_is_taken_as_it_is_where_its_own_layout_fits(self):
        original = text("f001-0000", 'Bob{n}"Hi."{done 30}', [("f001-0000", "k")])
        # Its own waits, pages and a line past the portrait window's 52 columns (31 letters: 62), within the screen's 70.
        reading = 'Bob{n}"' + "Well met, traveller, this day!" + '{wait 10}"{p}And again.'
        self.assertEqual(tr.width(tr.tokens('"Well met, traveller, this day!', EXTRA)), 62)
        self.assertEqual(pw.fit(original, reading, EXTRA, False), (reading, ""))
        # Shown otherwise by Perfect Works' own script, it is held to the original's window: broken anew.
        fitted, why = pw.fit(original, reading, EXTRA, True)
        self.assertEqual(why, "broken anew")
        self.assertTrue(all(tr.width(tr.tokens(line, EXTRA)) <= 52 for page in fitted.split("{p}") for line in page.split("{n}")), fitted)
        # A timed text has no page to add: one that does not fit keeps the original's.
        timed = text("f001-0001", "Go{done 30}", [("f001-0001", "k")], columns=12, rows=1)
        self.assertIsNone(pw.fit(timed, "Go on now, quickly{done 30}", EXTRA, True)[0])

    def test_a_choice_keeps_blank_the_lines_its_cursor_still_walks(self):
        original = text("f001-0000", "Pick{n} One{n} Two{n} Three", [("f001-0000", "k")], choice=(1, 3))
        fitted, why = pw.fit(original, "Choose{n} A{n} B", EXTRA, False)
        self.assertEqual((fitted, why), ("Choose{n} A{n} B{n}", "1 blank line(s) kept for the cursor"))
        self.assertEqual(tr.problems(original, fitted, EXTRA, True), [])
        # A line more than the cursor walks cannot stay.
        self.assertIsNone(pw.fit(original, "Choose{n} A{n} B{n} C{n} D", EXTRA, False)[0])

    def test_a_message_is_shown_otherwise_where_its_maps_scripts_show_it_so(self):
        otherwise = {12: {3, 4}, 60: None}
        shown = [where for where in ("f012-0003", "f012-0005", "f060-0000", "f061-0003", "s012-0003") if pw.shown_otherwise(where, otherwise)]
        self.assertEqual(shown, ["f012-0003", "f060-0000"])


class Lines(unittest.TestCase):
    def lines(self, texts, found, otherwise=None):
        report = []
        out, tally = pw.lines(texts, found, otherwise or {}, report)
        return out, tally, report

    def test_a_text_read_alike_in_all_its_entries_is_one_line_and_one_left_alone_is_listed_as_the_same(self):
        hello = text("f001-0000", "Hello.", [("f001-0000", "a"), ("f002-0007", "b")])
        other = text("f001-0001", "Bye.", [("f001-0001", "c")])
        out, tally, _ = self.lines([hello, other], {"f001-0000": "Greetings.", "f002-0007": "Greetings."})
        self.assertEqual(out, ["f001-0000 [text] Greetings.", "f001-0001 [text] Bye."])
        self.assertEqual((tally["texts read anew wherever they stand"], tally["entries the same as the original"]), (1, 1))

    def test_an_entry_read_otherwise_than_its_texts_other_entries_is_translated_alone(self):
        # Three lists' first entries all read "track00"; each gets its own title, the most common the text's.
        track = text("f222-0293", "track00", [("f222-0293", "a"), ("f222-0304", "b"), ("f222-0308", "c")])
        out, tally, _ = self.lines([track], {"f222-0293": "First Tune", "f222-0304": "Second Tune", "f222-0308": "First Tune"})
        self.assertEqual(out, ["f222-0293 [text] First Tune", "f222-0304 [here] Second Tune"])
        self.assertEqual(tally["entries read anew alone"], 1)
        # Where an entry keeps the original's, the original is the text's and each new reading stands alone.
        cancel = text("s018-0011", "Cancel", [("s018-0011", "a"), ("ml003-0012", "b"), ("md054-0195", "c")], group="system data")
        out, _, _ = self.lines([cancel], {"ml003-0012": "Delete"})
        self.assertEqual(out, ["s018-0011 [text] Cancel", "ml003-0012 [here] Delete"])
        # A reading of nothing is a line without text.
        by = text("md053-0406", "by", [("md053-0406", "a")], group="menu data")
        self.assertEqual(self.lines([by], {"md053-0406": ""})[0], ["md053-0406 [text]"])

    def test_a_name_no_table_holds_is_respelled_in_its_slot_alone(self):
        gear = text("nm000-0014", "Heimdal", [("nm000-0014", "a", (0, 0)), ("nm000-0027", "b", (0, 0))], columns=20, rows=1, group="names")
        out, _, _ = self.lines([gear], {"nm000-0014": "Heimdall", "nm000-0027": "Heimdall"})
        self.assertEqual(out, ["nm000-0014 [here] Heimdall", "nm000-0027 [here] Heimdall"])
        # A name an enemy's table holds too is that text's wherever it stands.
        boss = text("en086-0000", "Seibzehn", [("en086-0000", "a", (0, 0)), ("nm000-0019", "b", (0, 0))], columns=20, rows=1, group="enemy data")
        self.assertEqual(self.lines([boss], {"en086-0000": "Siebzehn", "nm000-0019": "Siebzehn"})[0], ["en086-0000 [text] Siebzehn"])

    def test_two_tables_alike_give_an_entry_one_name_and_one_line(self):
        # The world map's copy of the menu's labels: the same entry of both has one name.
        label = text("ml003-0012", "Cancel", [("ml003-0012", "same"), ("mw003-0012", "same"), ("s018-0011", "other")], group="menu labels")
        out, _, _ = self.lines([label], {"ml003-0012": "Delete", "mw003-0012": "Delete"})
        self.assertEqual(out, ["ml003-0012 [text] Cancel", "ml003-0012 [here] Delete"])

    def test_a_reading_that_fits_no_window_keeps_the_original_and_is_reported(self):
        tight = text("be006-0009", "......", [("be006-0009", "a", (0, 0))], columns=12, rows=1, group="battle events")
        out, tally, report = self.lines([tight], {"be006-0009": 'Bob{n}"............"'})
        self.assertEqual(out, [])
        self.assertEqual(sum(count for name, count in tally.items() if name.startswith("left")), 1)
        self.assertIn("be006-0009: left as the original", report[0])


if __name__ == "__main__":
    unittest.main()
