"""Packed-container encoder: round trip through the recovered decoder and its end rule."""

from __future__ import annotations

import io
import json
import random
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

from tools.analysis.packed import decode_block
from tools.packed_container import SECTOR, encode, main


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
                result.append(("literal", stream[pos]))
                pos, out = pos + 1, out + 1
    return result


class PackedContainerTests(unittest.TestCase):
    def test_round_trip_appends_only_the_zero_literals(self):
        rng = random.Random(7)
        for size in (64, 65, 66, 67, 4096, 5003):
            data = bytes(rng.choice(b"\x00\x00\x01ab") for _ in range(size))
            decoded = decode_block(encode(data) + b"\x00" * 16).data
            self.assertEqual(decoded[:size], data)
            self.assertLess(len(decoded) - size, 8)
            self.assertFalse(any(decoded[size:]))

    def test_last_group_is_complete(self):
        rng = random.Random(3)
        for size in range(1000, 1400, 7):
            data = bytes(rng.choice(b"\x00\x00\x01ab") for _ in range(size))
            self.assertEqual(len(tokens(encode(data))) % 8, 0, size)

    def test_zero_literals_complete_the_last_group(self):
        # Greedy gives a literal and copies of 18, 18 and 6; four zero literals
        # complete the group and count in the decoded length.
        stream = encode(b"\x00" * 43)
        self.assertEqual(
            tokens(stream),
            [("literal", 0), ("copy", 18), ("copy", 18), ("copy", 6)] + [("literal", 0)] * 4,
        )
        self.assertEqual(int.from_bytes(stream[:4], "little"), 47)

    def test_no_copy_reaches_before_the_data(self):
        stream = encode(b"    " * 8)
        self.assertEqual(tokens(stream)[0], ("literal", 0x20))

    def test_image_tail_must_be_the_packers_zero_literals(self):
        program = b"\x00" * 43  # encodes with four zero literals
        stream = encode(program)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sector = bytearray(SECTOR)
            sector[24 : 24 + len(stream)] = stream
            (root / "disc.bin").write_bytes(bytes(sector))
            (root / "manifest.json").write_text(json.dumps({"files": [{"slot": 1, "lba": 0, "size": 2048}]}))

            def run(image: bytes, tail: int) -> tuple[int, dict]:
                (root / "image.bin").write_bytes(image)
                argv = ["packed_container.py", str(root / "image.bin"), "--tail", str(tail),
                        "--disc", str(root / "disc.bin"), "--manifest", str(root / "manifest.json"),
                        "--slot", "1"]
                out = io.StringIO()
                with mock.patch.object(sys, "argv", argv), redirect_stdout(out):
                    code = main()
                return code, json.loads(out.getvalue())

            code, report = run(program + bytes(4), 4)
            self.assertEqual((code, report["zero_literals"], report["stream_matches_disc"]), (0, 4, True))
            # A tail the program's encoding does not leave fails, even where
            # the bytes are zero: the program would end elsewhere.
            code, report = run(program + bytes(4), 3)
            self.assertEqual((code, report["zero_literals"]), (1, 4))
            with self.assertRaisesRegex(SystemExit, "not a packer tail"):
                run(program + b"\x00\x00\x00\x01", 4)


if __name__ == "__main__":
    unittest.main()
