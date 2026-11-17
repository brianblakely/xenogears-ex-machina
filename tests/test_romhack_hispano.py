"""tools/romhack_hispano.py: Romhack Hispano's font read code by code, its
bytes as the language's markup, and the menu pictures' words held to their
windows (docs/runtime.md, Languages). The fonts and texts are made up here;
none is the original's or Romhack Hispano's."""
import unittest
from unittest import mock

from tools import romhack_hispano as rh
from tools import translations as tr

EXTRA = tr.language_characters(rh.LANGUAGE)


def image(n: int) -> bytes:
    return bytes([n]) * 22


class Characters(unittest.TestCase):
    def characters(self, old: dict[int, bytes], new: dict[int, bytes]):
        report: list[str] = []
        with mock.patch.object(rh, "font_glyphs", side_effect=[old, new]):
            chars, unnamed = rh.characters(object(), mock.Mock(number=1), report)
        return chars, unnamed, report

    def test_letters_moved_and_blanked_glyphs_and_unnamed_ones(self):
        blank = bytes(22)
        old = {0x10: blank, 0x62: image(1), 0x6E: image(2), 0x74: image(3), 0x7D: image(4), 0x89: image(5), 0xD9: image(6), 0xE0: image(7)}
        # á drawn over １; ○ kept for the button; ↑ moved to D9; ・ blanked; E0 drawn anew and unnamed.
        new = {**old, 0x62: image(9), 0x6E: image(10), 0x74: image(11), 0xD9: image(3), 0x89: blank, 0xE0: image(12)}
        chars, unnamed, report = self.characters(old, new)
        self.assertEqual((chars[0x62], chars[0x6E], chars[0xD9], chars[0x89]), ("á", "○", "↑", " "))
        self.assertNotIn(0x74, chars)  # ↑ moved away: its old code is now something no rule names
        self.assertEqual(unnamed, {0x74, 0xE0})
        self.assertEqual(report, ["disc 1: the patch's font draws 2 code(s) no rule names: 74 E0"])


class Markup(unittest.TestCase):
    def test_the_patchs_letters_become_the_languages_glyphs_and_an_unnamed_code_is_refused(self):
        chars = {**tr.CHARS, 0x62: "á", 0x6C: "¡", 0x73: "¿", 0x79: "ñ"}
        raw = bytes([0x73, 0x2B, 0x62, 0x60, 0x01, 0x6C, 0x2D, 0x79, 0x57, 0x00])  # "¿Lá?{n}¡Nñ!"
        marked = rh.markup(raw, b"\x20\x00", chars, set())
        self.assertEqual(marked, "¿Lá?{n}¡Nñ!")
        self.assertEqual(tr.encode(marked, EXTRA)[-1], 0)
        refused = rh.markup(bytes([0x20, 0xE0, 0x00]), b"\x20\x00", {**tr.CHARS}, {0xE0})
        self.assertEqual(refused, "A{ES E0}")
        self.assertTrue(tr.problems(tr.Text("f001-0000", "0" * 16, "A", 52, 4, "field messages", [], None), refused, EXTRA))

    def test_a_development_text_in_kana_keeps_the_originals_reading(self):
        raw = bytes([0xB2, 0xBF, 0x00])
        self.assertEqual(rh.markup(raw, raw, {0xB2: "−", 0xBF: "！"}, set()), tr.render(raw))

    def test_every_menu_pictures_word_fits_the_small_font_where_it_must(self):
        for where, label in rh.LABELS.items():
            tr.encode(label, EXTRA)  # every character is a glyph of the language
            if where.startswith(("mc22", "mc23", "mc24")):
                self.assertRegex(label, tr.SMALL["word"], where)
                self.assertLessEqual(len(label.rstrip("%")), 8, where)


if __name__ == "__main__":
    unittest.main()
