"""A synthetic card image exercises locating the saved file block."""

import unittest

from tools.analysis.menu_save_file import BLOCK, CARD, FRAME, card_file


def card_with(entries: dict[int, tuple[int, bytes]]) -> bytearray:
    card = bytearray(CARD)
    card[:2] = b"MC"
    for index, (state, name) in entries.items():
        frame = index * FRAME
        card[frame : frame + 4] = state.to_bytes(4, "little")
        card[frame + 0xA : frame + 0xA + len(name)] = name
        card[index * BLOCK : (index + 1) * BLOCK] = bytes([index]) * BLOCK
    return card


class MenuSaveFileTests(unittest.TestCase):
    def test_finds_the_first_block_of_the_named_file(self):
        card = card_with({1: (0x51, b"OTHER"), 3: (0x51, b"NAME0")})
        frame, block = card_file(bytes(card), b"NAME0")
        self.assertEqual(frame, 3)
        self.assertEqual(block, bytes([3]) * BLOCK)

    def test_rejects_missing_duplicate_free_and_unformatted(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            card_file(bytes(card_with({1: (0x51, b"OTHER")})), b"NAME0")
        with self.assertRaisesRegex(ValueError, "exactly one"):
            card_file(bytes(card_with({1: (0x51, b"NAME0"), 2: (0x51, b"NAME0")})), b"NAME0")
        with self.assertRaisesRegex(ValueError, "exactly one"):
            card_file(bytes(card_with({1: (0xA1, b"NAME0")})), b"NAME0")
        with self.assertRaisesRegex(ValueError, "formatted"):
            card_file(bytes(CARD), b"NAME0")


if __name__ == "__main__":
    unittest.main()
