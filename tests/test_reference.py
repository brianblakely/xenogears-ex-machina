"""Original synthetic standard-format fixtures test bounds, never game behavior."""

from __future__ import annotations

import io
import unittest

from tools.reference.inspect_disc import RAW_SECTOR_SIZE, SYNC, RawCd, both_endian, directory_record


class RawDiscTests(unittest.TestCase):
    def sector(self, mode: int = 2) -> bytearray:
        raw = bytearray(RAW_SECTOR_SIZE)
        raw[:12] = SYNC
        raw[15] = mode
        return raw

    def test_mode_one_and_mode_two_payload(self) -> None:
        for mode, offset in ((1, 16), (2, 24)):
            with self.subTest(mode=mode):
                raw = self.sector(mode)
                payload = bytes(range(256)) * 8
                raw[offset : offset + 2048] = payload
                self.assertEqual(RawCd(io.BytesIO(raw), len(raw)).read_sector(0), payload)

    def test_invalid_sector_modes_sync_and_subheaders(self) -> None:
        cases = [self.sector(0), self.sector(), self.sector(), self.sector()]
        cases[1][1] = 0
        cases[2][16] = 1
        cases[3][18] = cases[3][22] = 0x20
        for raw in cases:
            with self.assertRaises(ValueError):
                RawCd(io.BytesIO(raw), len(raw)).read_sector(0)

    def test_truncation_extent_limits_and_negative_offsets(self) -> None:
        raw = self.sector()
        for size in (0, 1, RAW_SECTOR_SIZE - 1):
            with self.assertRaises(ValueError):
                RawCd(io.BytesIO(raw), size)
        cd = RawCd(io.BytesIO(raw), len(raw))
        for lba in (-1, 1):
            with self.assertRaises(ValueError):
                cd.read_sector(lba)
        for lba, size in ((0, -1), (-1, 2), (1, 2048), (0, 17 * 1024 * 1024)):
            with self.assertRaises(ValueError):
                cd.read_extent(lba, size)
        self.assertEqual(cd.read_extent(0, 0), b"")
        truncated = RawCd(io.BytesIO(raw[:-1]), len(raw))
        with self.assertRaises(ValueError):
            truncated.read_sector(0)

    def test_both_endian_fields_are_checked(self) -> None:
        self.assertEqual(both_endian(b"\x01\x00\x00\x01", 0, 2), 1)
        for data, offset in ((b"\x01\x00\x00\x02", 0), (b"\x00", 0), (b"", -1)):
            with self.assertRaises(ValueError):
                both_endian(data, offset, 2)

    def test_directory_record_rejects_unsafe_or_truncated_entries(self) -> None:
        valid = bytearray(34)
        valid[0] = 34
        valid[32] = 1
        valid[33] = ord("X")
        self.assertEqual(directory_record(bytes(valid), 0)["name"], "X")
        for data in (b"", b"\x00", bytes(valid[:-1])):
            with self.assertRaises(ValueError):
                directory_record(data, 0)
        for offset, value in ((33, ord("/")), (32, 2), (1, 1), (25, 0x80), (26, 1)):
            malformed = valid.copy()
            malformed[offset] = value
            with self.assertRaises(ValueError):
                directory_record(bytes(malformed), 0)


if __name__ == "__main__":
    unittest.main()
