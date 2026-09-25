"""Synthetic trace rows exercise the movie chain's platform input and its
in-flight MDEC transfer handling. They describe no original observation."""

import struct
import tempfile
import unittest
from pathlib import Path

from tools.analysis.movie_chain import (
    drive_after_ring,
    mode6_vsyncs,
    platform_lines,
    stopped_transfer_lines,
    without_in_flight_slice,
)

NO_DELAY = {"select": 0, "registers": [0, 0], "values": [0, 0]}

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


class FakeSnapshots:
    """Rows carry their own RAM image under "ram"."""

    def read(self, row):
        return row["ram"], b"", b""


class StreamInputTests(unittest.TestCase):
    def test_drive_follows_the_last_filled_ring_slot(self):
        ram = bytearray(0x200000)
        struct.pack_into("<III", ram, 0x1E8A14, 0x80100000, 4, 0)  # slots, count
        struct.pack_into("<I", ram, 0x1E89F8, 2)  # the next sector fills slot 2
        header = bytes(range(1, 29))
        ram[0x100020 : 0x100020 + 28] = header  # slot 1
        sectors = bytearray(2352 * 6)
        sectors[2352 * 4 + 24 : 2352 * 4 + 52] = b"\x60\x01" + header[2:]
        with tempfile.TemporaryDirectory() as directory:
            disc = Path(directory) / "disc.bin"
            disc.write_bytes(bytes(sectors))
            self.assertEqual(drive_after_ring(bytes(ram), disc), 5)

    def test_stopped_transfer_reads_both_buffers_after_the_stop(self):
        ram = bytearray(0x200000)
        ram[0x100000:0x100004] = b"\x01\x02\x03\x04"
        ram[0x100010:0x100014] = b"\x05\x06\x07\x08"
        slice_row = {"hook": "mdec-slice", "gpr_u32": [0] * 34, "load_delay": NO_DELAY,
                     "ranges": [{"name": "slice-00", "hex": "00" * 4}]}
        rows = []
        for buffer in (0x80100000, 0x80100010):
            head = dict(slice_row, gpr_u32=[0] * 5 + [buffer] + [0] * 28)
            rows += [head, {"hook": "mdec-slice-tail", "ranges": []}]
        rows += [{"hook": "mv-801d4324"}, {"hook": "m6-exit", "snapshot": 1, "ram": bytes(ram)}]
        self.assertEqual(
            stopped_transfer_lines(rows, FakeSnapshots()),
            ["mdec_abort 80100000 01020304", "mdec_abort 80100010 05060708"],
        )

    def test_mode6_vsync_results_come_from_the_table_at_the_next_head(self):
        ram = bytearray(0x200000)
        struct.pack_into("<4I", ram, 0x773B8, 1, 2, 3, 4)
        rows = [{"hook": "m6-80076758"}, {"hook": "m6-80076758"},
                {"hook": "m6-head", "snapshot": 1, "ram": bytes(ram)}]
        self.assertEqual(mode6_vsyncs(rows, FakeSnapshots()),
                         ["hblank 1", "hblank 2", "hblank 3", "hblank 4"])


if __name__ == "__main__":
    unittest.main()
