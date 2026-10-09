"""Synthetic message bytes exercise the recovered text controls; they
describe no original text."""

import struct
import unittest

from tools.analysis.text_control import (
    BLANK_CODE,
    CONTROLS,
    PAIR_CODES,
    TITLE_BYTES,
    Sweep,
    TextError,
    agreeing,
    align_titles,
    archive_entry,
    code_text,
    decode_text,
    decode_token,
    fits,
    initial_names,
    number_glyphs,
    pair_kind,
    render,
    save_titles,
    table_listing,
    text_table,
    unpack_logical,
)

THRESHOLD = 0xFE


def table(texts: list[bytes]) -> bytes:
    """count, 0, count + 1 offsets, (columns, rows) pairs, then the texts."""
    head = 4 + 2 * (len(texts) + 1) + 2 * len(texts)
    head += -head % 4
    offsets, body = [], bytearray()
    for text in texts:
        offsets.append(head + len(body))
        body += text
    offsets.append(head + len(body))
    out = bytearray(struct.pack(f"<HH{len(offsets)}H", len(texts), 0, *offsets))
    out += bytes([0x20, 0x03]) * len(texts)
    out += bytes(head - len(out))
    return bytes(out + body)


class ControlTests(unittest.TestCase):
    def test_table_has_every_case(self):
        self.assertEqual(
            sorted(c for c in CONTROLS if c >= 0x0F00), [0x0F00 | n for n in range(16)]
        )
        self.assertEqual({CONTROLS[c].length for c in (0, 1, 2, 3)}, {1})

    def test_glyphs_and_controls(self):
        text = bytes(
            [
                0x41,
                0xFE,
                0x12,
                0x01,
                0x02,
                0x01,
                0x03,
                0x0F,
                0x00,
                0x1E,
                0x0F,
                0x03,
                0x18,
                0x05,
                0x0F,
                0x04,
                0x0F,
                0x05,
                0x80,
                0x00,
            ]
        )
        tokens = decode_text(text, 0, THRESHOLD)
        self.assertEqual(
            [(t.mnemonic, t.length) for t in tokens],
            [
                ("glyph", 1),
                ("glyph", 2),
                ("newline", 1),
                ("page", 2),
                ("pause", 1),
                ("wait", 3),
                ("insert", 4),
                ("insert_selection", 2),
                ("insert_name", 3),
                ("end", 1),
            ],
        )
        self.assertEqual(tokens[1].code, 0xFE12)
        self.assertEqual(tokens[6].operands, (("resource", 0x18), ("entry", 5)))
        self.assertEqual(sum(t.length for t in tokens), len(text))

    def test_operand_read_again_as_text(self):
        tokens = decode_text(bytes([0x0F, 0x0B, 0x41, 0x00]), 0, THRESHOLD)
        self.assertEqual(
            [(t.offset, t.mnemonic) for t in tokens], [(0, "set_6d"), (2, "glyph"), (3, "end")]
        )
        self.assertEqual(tokens[0].operands, (("value", 0x41),))

    def test_threshold_splits_glyphs(self):
        self.assertEqual(decode_token(bytes([0x80, 0x10]), 0, 0x80).length, 2)
        self.assertEqual(decode_token(bytes([0x7F, 0x10]), 0, 0x80).length, 1)

    def test_errors(self):
        with self.assertRaises(TextError) as caught:
            decode_text(bytes([0x41, 0x0F, 0x10, 0x00]), 0, THRESHOLD)
        self.assertEqual(caught.exception.offset, 1)
        with self.assertRaises(TextError):
            decode_text(bytes([0x41, 0x42]), 0, THRESHOLD)
        with self.assertRaises(TextError):
            decode_text(bytes([0x0F, 0x03, 0x01]), 0, THRESHOLD)


class TableTests(unittest.TestCase):
    def test_text_table(self):
        data = table([bytes([0x41, 0x00]), bytes([0x0F, 0x01, 0x02, 0x42, 0x00])])
        offsets, end = text_table(data)
        self.assertEqual(end, len(data))
        self.assertEqual([len(decode_text(data, o, THRESHOLD)) for o in offsets], [2, 3])
        self.assertEqual(text_table(struct.pack("<HHI", 0xFFFF, 0, 0)), ((), 4))
        with self.assertRaises(TextError):
            text_table(struct.pack("<HHHH", 1, 5, 8, 8))

    def test_unreached_text_decodes_on_its_own(self):
        # The entry ends at its first 00; 42 43 00 and 0F 10 00 follow it.
        data = table([b"\x41\x00\x42\x43\x00\x0f\x10\x00"])
        result = Sweep()
        result.add("table", data, THRESHOLD)
        self.assertEqual((result.texts, result.tokens), (1, 2))
        self.assertEqual((result.unreferenced, result.unreferenced_texts), (4, 1))
        self.assertEqual(len(result.unreferenced_errors), 1)
        self.assertEqual(result.unknown, [])

    def test_listing_shows_entries_tokens_and_unreached_text(self):
        data = table([b"\x41\xfe\x42\x0f\x00\x05\x01\x00\x42\x43\x00\x0f\x10\x00"])
        self.assertEqual(
            table_listing(data, THRESHOLD),
            [
                "     0 +0x000c 32x3: 41 fe42 [wait frames=5] [newline] [end]",
                "  unreached +0x0014: 42 43 [end]",
                "  unreached +0x0017: undecodable, +0x17: 0F sub-code 0x10 matches no case"
                " (the window stalls)",
            ],
        )
        self.assertEqual(
            table_listing(struct.pack("<HHI", 0xFFFF, 0, 0), THRESHOLD),
            ["  no messages (count 0xFFFF)"],
        )

    def test_archive_entries(self):
        text = table([b"\x41\x42\x43\x00"])
        first = b"\x01\x02\x03\x04"
        # The decoder reads one flag byte past a complete stream (here padding).
        archive = struct.pack("<III", 2, 12, 16) + first + literals(text) + b"\0"
        self.assertEqual(archive_entry(archive, 0), first)
        self.assertEqual(archive_entry(archive, 1, packed=True), text)
        with self.assertRaises(TextError):
            archive_entry(archive, 2)

    def test_stream_reading_past_its_file(self):
        data = bytes(range(16))
        self.assertEqual(unpack_logical(literals(data) + b"\0", 16), (data, False))
        self.assertEqual(unpack_logical(literals(data), 16), (data, True))
        with self.assertRaises(TextError):
            unpack_logical(literals(data)[:-8], 16)


def literals(data: bytes) -> bytes:
    """A packed stream of literal groups (80032eb4 format) for 8n bytes."""
    out = bytearray(len(data).to_bytes(4, "little"))
    for start in range(0, len(data), 8):
        out += b"\0" + data[start : start + 8]
    return bytes(out)


class CharacterCodeTests(unittest.TestCase):
    # Codes 0-3: one byte 41, one byte 01, empty, two bytes FE 12.
    PAIRS = bytes([0, 0x41, 0, 0x01, 0, 0, 0xFE, 0x12]) + bytes(2 * (PAIR_CODES - 4))

    def test_code_text(self):
        self.assertEqual(code_text(self.PAIRS, [0, 3, 0]), b"\x41\xfe\x12\x41\x00")
        self.assertEqual(code_text(self.PAIRS, []), b"\x00")
        with self.assertRaises(TextError):
            code_text(self.PAIRS, [PAIR_CODES])

    def test_pair_kinds(self):
        kinds = [pair_kind(*self.PAIRS[2 * c : 2 * c + 2], THRESHOLD) for c in range(4)]
        self.assertEqual(
            kinds, ["one-byte glyph", "control", "00 (ends the text)", "two-byte glyph"]
        )
        self.assertEqual(pair_kind(0x41, 0x42, THRESHOLD), "two separate tokens")
        self.assertEqual(pair_kind(0, 0xFE, THRESHOLD), "two-byte glyph lead alone")

    def test_initial_names_end_at_code_0f(self):
        game = bytearray(31 * 20)
        struct.pack_into("<3H", game, 0, 0, 3, 0x000F)  # slot 0: two codes, then the end
        struct.pack_into("<10H", game, 20, *[0] * 10)  # slot 1: ten codes, no end code
        names = initial_names(bytes(game), self.PAIRS)
        self.assertEqual(len(names), 31)
        self.assertEqual(names[0], b"\x41\xfe\x12\x00")
        self.assertEqual(names[1], b"\x41" * 10 + b"\x00")
        self.assertEqual(
            [t.mnemonic for t in decode_text(names[0], 0, THRESHOLD)],
            [
                "glyph",
                "glyph",
                "end",
            ],
        )


# An invented cipher: glyph 0x10 + n spells the letter n places after "A"
# (0x10 itself the blank); the bytes describe no original font.
def spell(text: str) -> bytes:
    return bytes(0x10 if c == " " else 0x10 + ord(c) - 0x40 for c in text)


class CharacterTests(unittest.TestCase):
    def test_number_codes_name_their_one_byte_glyphs(self):
        pairs = bytearray(2 * PAIR_CODES)
        for digit in range(10):
            pairs[2 * digit + 1] = 0x80 + digit  # palette 0
            pairs[2 * (0x10 + digit) + 1] = 0x90 + digit  # palette 1
        pairs[2 * 10 + 1], pairs[2 * 11 + 1] = 0x8A, 0x8B  # palette 0 signs
        pairs[2 * 0x1A : 2 * 0x1A + 2] = b"\xfe\x01"  # a two-byte glyph is left out
        pairs[2 * BLANK_CODE + 1] = 0x10
        glyphs = number_glyphs(bytes(pairs), THRESHOLD)
        self.assertEqual(glyphs[0x83], "3")
        self.assertEqual(glyphs[0x99], "9")
        self.assertEqual((glyphs[0x8A], glyphs[0x8B], glyphs[0x10]), ("-", "+", " "))
        self.assertEqual(len(glyphs), 23)  # code 0x1b's empty pair is no glyph either
        pairs[2 * 0x1B + 1] = 0x83  # one glyph read as two characters names neither
        self.assertNotIn(0x83, number_glyphs(bytes(pairs), THRESHOLD))

    def test_title_lines_read_through_the_inverted_table(self):
        table = tuple(0x8200 + n for n in range(96))  # invented: ASCII 0x20 + n

        def line(text: str) -> bytes:
            codes = b"".join(struct.pack(">H", 0x8200 + ord(c) - 0x20) for c in text)
            return codes + struct.pack(">H", 0x8200) * (TITLE_BYTES // 2 - len(text)) + b"\r\n"

        outside = b"\x81\x00" + line("Bc")[2:]  # a code the table lacks
        newline_inside = line("A*")  # '*' is 0x820a: its second byte is no newline
        data = line("Ab Cd") + outside + newline_inside + line("Ef")
        self.assertEqual(save_titles(data, table), ["Ab Cd", "A*", "Ef"])

    def test_a_title_fits_only_its_pattern(self):
        known = {0x10: " "}
        self.assertTrue(fits("Ab A", spell("Ab A"), known))
        self.assertFalse(fits("Ab A", spell("Ab C"), known))  # equal letters, equal glyphs
        self.assertFalse(fits("Ab C", spell("Ab A"), known))  # different letters apart
        self.assertFalse(fits("Ab", spell("A "), known))  # the blank is known
        self.assertFalse(fits("A ", spell("Ab"), known))  # a known character at another glyph
        self.assertFalse(fits("Ab", spell("Abc"), known))

    def test_titles_spell_whole_texts_and_extend_each_other(self):
        known = {0x10: " "}
        texts = {spell(t) for t in ("Ab Cd", "Ce Cb", "Bad", "Xy", "Xz")}
        glyphs, aligned = align_titles(["Ab Cd", "Bad", "Xy", "Qq"], texts, known)
        # Round one: only "Ab Cd" has a known character (the blank), and only
        # its own text fits it. Its "d" then lets "Bad" add "B" and "a". "Xy"
        # never has a known character and "Qq" has no text.
        self.assertEqual([title for title, _ in aligned], ["Ab Cd", "Bad"])
        self.assertEqual(glyphs, {**known, **{spell(c)[0]: c for c in "AbCdBa"}})

    def test_ambiguous_titles_add_nothing(self):
        texts = {spell("Ab Cd"), spell("Ef Gh")}
        glyphs, aligned = align_titles(["Ab Cd"], texts, {0x10: " "})
        self.assertEqual((glyphs, aligned), ({0x10: " "}, []))

    def test_the_largest_agreeing_set_wins_and_ties_add_nothing(self):
        known = {0x10: " "}
        # "Ab Cd" and "Ab Ce" agree; the third text puts "A" elsewhere.
        proposals = {
            "Ab Cd": spell("Ab Cd"),
            "Ab Ce": spell("Ab Ce"),
            "Af Gh": spell("Ib Jk"),
        }
        self.assertEqual(agreeing(proposals, known), ["Ab Cd", "Ab Ce"])
        tie = {"Ab Cd": spell("Ab Cd"), "Af Gh": spell("Ib Jk")}
        self.assertEqual(agreeing(tie, known), [])

    def test_render_prints_known_glyphs_as_characters(self):
        data = b"\x41\x10\x42\xfe\x41\x01\x5b\x00"
        tokens = decode_text(data, 0, THRESHOLD)
        self.assertEqual(render(tokens), "41 10 42 fe41 [newline] 5b [end]")
        chars = {0x41: "A", 0x10: " ", 0x5B: "{"}
        self.assertEqual(render(tokens, chars), "A {42}{fe41}[newline]\\{[end]")
        listing = table_listing(table([data]), THRESHOLD, chars)
        self.assertEqual(listing, ["     0 +0x000c 32x3: A {42}{fe41}[newline]\\{[end]"])


if __name__ == "__main__":
    unittest.main()
