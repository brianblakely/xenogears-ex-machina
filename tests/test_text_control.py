"""Synthetic message bytes exercise the recovered text controls; they
describe no original text."""

import struct
import unittest

from tools.analysis.text_control import (
    CONTROLS,
    PAIR_CODES,
    Sweep,
    TextError,
    archive_entry,
    code_text,
    decode_text,
    decode_token,
    initial_names,
    pair_kind,
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
        self.assertEqual(table_listing(struct.pack("<HHI", 0xFFFF, 0, 0), THRESHOLD),
                         ["  no messages (count 0xFFFF)"])

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


if __name__ == "__main__":
    unittest.main()
