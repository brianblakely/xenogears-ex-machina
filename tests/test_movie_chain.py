"""Synthetic trace rows exercise the movie chain's platform input and its
in-flight MDEC transfer handling. They describe no original observation."""

import struct
import unittest

from tools.analysis.movie_chain import platform_lines, without_in_flight_slice

RAM = bytearray(0x200000)
struct.pack_into("<I", RAM, 0x567B4, 0x1F8010B8)  # DMA3's CHCR register
IO = bytes(0x1000)
PADS = [{"name": "pads", "hex": "00" * 0x44}]


def interrupt() -> list[dict]:
    return [{"hook": "dispatch-entry", "ranges": PADS}, {"hook": "dispatch-exit"}]


def position(pc: int) -> dict:
    return {"hook": f"mv-{pc:08x}"}


class MovieChainTests(unittest.TestCase):
    def test_arrivals_of_a_later_visit_follow_a_pass(self):
        rows = [
            position(0x800A7E5C),
            *interrupt(),
            position(0x800A7E64),
            position(0x800A7E5C),
            *interrupt(),
            position(0x800A7E64),
        ]
        lines = [line for line in platform_lines(rows, bytes(RAM), IO, bytes(RAM))[0]
                 if not line.startswith("pad ")]
        self.assertEqual(lines, ["arrival 800a7e64", "pass 800a7e64", "arrival 800a7e64"])

    def test_arrivals_before_put_draw_env_queue_go_to_its_delay_slot(self):
        rows = [position(0x800A7F3C), *interrupt(), {"hook": "alarm"}, position(0x800A7F44)]
        lines = [line for line in platform_lines(rows, bytes(RAM), IO, bytes(RAM))[0]
                 if not line.startswith("pad ")]
        self.assertEqual(lines, ["arrival 800a7f40"])

    def test_in_flight_slice_bytes_are_restored_and_counted(self):
        buffer = 0x80100000
        old, new = b"\x01\x02\x03\x04", b"\x05\x06\x07\x08"
        records = [(0, buffer, old), (4, buffer, new)]
        rows = [{"hook": "x"} for _ in range(5)]
        image = bytearray(0x200000)
        image[0x100000:0x100004] = new[:2] + old[2:]
        restored_image, restored = without_in_flight_slice(rows, records, 2, bytes(image))
        self.assertEqual(restored, 2)
        self.assertEqual(restored_image[0x100000:0x100004], old)
        rows[3] = {"hook": "mv-801d3e54"}
        unchanged, restored = without_in_flight_slice(rows, records, 2, bytes(image))
        self.assertEqual((unchanged, restored), (bytes(image), 0))


if __name__ == "__main__":
    unittest.main()
