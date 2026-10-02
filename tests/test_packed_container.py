"""Packed-container encoder: round trip through the recovered decoder and its end rule."""

from __future__ import annotations

import random
import unittest

from tools.analysis.packed import decode_block
from tools.packed_container import encode


def tokens(stream: bytes) -> list[tuple[str, int]]:
    size, pos, out, result = int.from_bytes(stream[:4], "little"), 4, 0, []
    while out < size:
        flags = stream[pos]
        pos += 1
        for bit in range(8):
            if out >= size:
                break
            if flags >> bit & 1:
                length = (stream[pos + 1] >> 4) + 3
                result.append(("copy", length))
                pos, out = pos + 2, out + length
            else:
                result.append(("literal", 1))
                pos, out = pos + 1, out + 1
    return result


class PackedContainerTests(unittest.TestCase):
    def test_round_trip(self):
        rng = random.Random(7)
        for size in (64, 65, 66, 67, 4096, 5003):
            data = bytes(rng.choice(b"\x00\x00\x01ab") for _ in range(size))
            self.assertEqual(decode_block(encode(data) + b"\x00" * 16).data, data)

    def test_last_group_is_complete(self):
        rng = random.Random(3)
        for size in range(1000, 1400, 7):
            data = bytes(rng.choice(b"\x00\x00\x01ab") for _ in range(size))
            self.assertEqual(len(tokens(encode(data))) % 8, 0, size)

    def test_trailing_copies_become_literals(self):
        # Greedy gives a literal and copies of 18, 18 and 6: the 6 cannot shrink
        # below three bytes, so it becomes literals and the 18 is shortened.
        self.assertEqual(
            tokens(encode(b"\x00" * 43)),
            [("literal", 1), ("copy", 18), ("copy", 11)] + [("literal", 1)] * 13,
        )

    def test_no_copy_reaches_before_the_data(self):
        stream = encode(b"    " * 8)
        self.assertEqual(tokens(stream)[0], ("literal", 1))


if __name__ == "__main__":
    unittest.main()
