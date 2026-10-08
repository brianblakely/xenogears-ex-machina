"""Synthetic message bytes exercise the recovered text controls; they
describe no original text."""

import struct
import unittest

from tools.analysis.text_control import (
    CONTROLS,
    TextError,
    archive_entry,
    decode_text,
    decode_token,
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


if __name__ == "__main__":
    unittest.main()
