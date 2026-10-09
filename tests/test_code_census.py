"""Code census rules on synthetic files: packed blocks at any offset, leaf
returns, and the raw words it explains as data."""

from __future__ import annotations

import hashlib
import struct
import unittest

from tools.analysis.packed import decode_block
from tools.extraction.code_census import JR_RA, census_file, returns, stream_sector, target_images
from tools.packed_container import encode

LEAF = JR_RA + bytes(4)  # jr $ra; nop: a leaf return with no frame
TEXT = bytes(range(1, 256))  # no repeats, so the packer keeps literals


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def census(data: bytes, targets: dict[str, str] | None = None):
    lines, found, unknown, _ = census_file(("f", len(data), data, targets or {}))
    return lines, found, unknown


class CodeCensusTests(unittest.TestCase):
    def test_returns_counts_aligned_words_only(self):
        self.assertEqual(returns(bytes(1) + JR_RA + bytes(3) + JR_RA), [8])

    def test_a_packed_block_at_any_offset_is_checked(self):
        stream = encode(TEXT[:252] + LEAF)
        data = b"\xff" * 13 + stream + bytes(32)
        lines, found, unknown = census(data)
        self.assertEqual((found, unknown), (set(), 1))
        self.assertIn("f packed +0xd ", lines[0])
        self.assertTrue(lines[0].endswith("UNMATCHED"))

    def test_a_block_equal_to_a_target_is_that_target(self):
        stream = encode(TEXT[:252] + LEAF)
        output = decode_block(stream + bytes(16)).data
        data = b"\xff" * 13 + stream + bytes(32)
        lines, found, unknown = census(data, {digest(output): "target"})
        self.assertEqual((found, unknown), ({"target"}, 0))

    def test_a_leaf_return_in_raw_data_is_code(self):
        lines, found, unknown = census(bytes(16) + LEAF + bytes(16))
        self.assertEqual(unknown, 1)
        self.assertEqual(lines, ["f raw +0x10 jr $ra -> UNMATCHED"])

    def test_a_raw_word_in_a_large_block_stream_is_its_data(self):
        # Literals at file offset 7 onwards: output byte 1 (unaligned there)
        # is the aligned file word at 8.
        output = TEXT[:1] + JR_RA + TEXT[1:252]
        data = bytes(2) + encode(output) + bytes(16)
        self.assertEqual(data[8:12], JR_RA)
        lines, found, unknown = census(data)
        self.assertEqual(unknown, 0)
        self.assertEqual(lines, ["f raw +0x8 jr $ra -> data of the packed block at +0x2"])

    def test_a_literal_block_repeats_its_file_words(self):
        # A one-group block of literals at 3 copies the aligned word at 8.
        data = bytes(3) + struct.pack("<I", 8) + bytes(1) + LEAF + bytes(16)
        lines, found, unknown = census(data, {digest(data): "target"})
        self.assertEqual((lines, found, unknown), (["f raw 0x20 -> target"], {"target"}, 0))
        lines, found, unknown = census(data)
        self.assertEqual((lines, unknown), (["f raw +0x8 jr $ra -> UNMATCHED"], 1))
        # Copied from an unaligned file offset, the word is new code.
        data = bytes(2) + struct.pack("<I", 8) + bytes(1) + LEAF + bytes(16)
        lines, found, unknown = census(data, {digest(data): "target"})
        self.assertEqual((lines[1:], unknown), (["f packed +0x2 0x8 -> UNMATCHED"], 1))

    def test_wave_bank_samples_are_data(self):
        header = b"wds " + bytes(12) + struct.pack("<III", 0x40, 0x20, 0x40)
        data = header + bytes(0x40 - len(header)) + bytes(16) + LEAF + bytes(8) + LEAF
        lines, found, unknown = census(data)
        self.assertEqual(
            lines, ["f raw +0x50 jr $ra -> wave bank samples", "f raw +0x60 jr $ra -> UNMATCHED"]
        )
        self.assertEqual(unknown, 1)

    def test_stream_sectors(self):
        def sector(submode: int, data: bytes) -> bytes:
            raw = bytearray(2352)
            raw[18] = submode
            raw[24 : 24 + len(data)] = data
            return bytes(raw)

        self.assertEqual(stream_sector(sector(0x48, (0x80010160).to_bytes(4, "little"))), "video")
        self.assertEqual(stream_sector(sector(0x64, b"\x01")), "audio")
        self.assertEqual(stream_sector(sector(0x00, b"")), "empty")
        self.assertEqual(stream_sector(sector(0x08, LEAF)), "other")

    def test_targets_are_the_configured_images(self):
        targets = target_images()
        self.assertEqual(len(targets), 26)
        self.assertEqual(
            targets["dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119"], "slus_006.64"
        )


if __name__ == "__main__":
    unittest.main()
