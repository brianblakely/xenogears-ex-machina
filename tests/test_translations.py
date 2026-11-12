"""tools/translations.py: markup, line wrapping and the fit of a translation
in its original's windows (docs/runtime.md, Languages). The texts are made
up here; none is original."""
import unittest

from tools import translations as tr

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


if __name__ == "__main__":
    unittest.main()
