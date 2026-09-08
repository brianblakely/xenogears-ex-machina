"""Invented sectors test fingerprint scope; no original behavior oracle is used."""

from __future__ import annotations

import hashlib
import io
import unittest

from tools.reference.fingerprint_survey import compare, fingerprint
from tools.reference.inspect_disc import SYNC


def sector(payload: bytes, submode: int = 0x08) -> bytes:
    raw = bytearray(2352)
    raw[:12] = SYNC
    raw[15] = 2
    raw[16:20] = raw[20:24] = bytes([0, 0, submode, 0])
    raw[24 : 24 + len(payload)] = payload
    return bytes(raw)


class FingerprintTests(unittest.TestCase):
    def test_projection_clips_tail_and_excludes_or_includes_subheaders_explicitly(self) -> None:
        raw = sector(b"A" * 2048) + sector(b"B" * 2048)
        data = fingerprint(io.BytesIO(raw), 0, 2049, 2048, 2)
        self.assertEqual(data["projection_sha256"], hashlib.sha256(b"A" * 2048 + b"B").hexdigest())
        self.assertEqual(data["source_sector_count"], 2)
        self.assertFalse(data["all_projected_bytes_zero"])
        mode2 = fingerprint(io.BytesIO(raw), 0, 2337, 2336, 2)
        self.assertEqual(
            mode2["projection_sha256"], hashlib.sha256(raw[16:2352] + raw[2368:2369]).hexdigest()
        )
        zeros = fingerprint(io.BytesIO(sector(bytes(2048))), 0, 19, 2048, 1)
        self.assertTrue(zeros["all_projected_bytes_zero"])

    def test_bad_projection_bounds_sector_and_truncation_are_rejected(self) -> None:
        raw = sector(b"invented")
        for lba, size, unit, sectors in (
            (0, 0, 2048, 1),
            (-1, 1, 2048, 1),
            (0, 1, 2352, 1),
            (1, 1, 2048, 1),
        ):
            with self.subTest(parameters=(lba, size, unit, sectors)), self.assertRaises(ValueError):
                fingerprint(io.BytesIO(raw), lba, size, unit, sectors)
        for truncated in (b"", raw[:100]):
            with self.assertRaises(ValueError):
                fingerprint(io.BytesIO(truncated), 0, 2048, 2048, 1)
        with self.assertRaises(ValueError):
            fingerprint(io.BytesIO(sector(b"invented", 0x24)), 0, 20, 2048, 1)
        damaged = bytearray(raw)
        damaged[20] ^= 1
        with self.assertRaisesRegex(ValueError, "Invalid Mode 2"):
            fingerprint(io.BytesIO(damaged), 0, 20, 2336, 1)

    def test_shared_hashes_do_not_collapse_source_slot_counts(self) -> None:
        record = {"projection_unit": 2048, "projected_bytes": 10, "projection_sha256": "a" * 64}
        reports = [
            {
                "source_profile": f"invented-{index}",
                "raw_track_sha256": str(index) * 64,
                "records": [{"slot": slot, **record} for slot in range(1, count + 1)],
                "groups": [
                    {"header_slot": 0, "first_child_slot": 1, "end_child_slot_exclusive": count + 1}
                ],
            }
            for index, count in enumerate((2, 1))
        ]
        result = compare(reports)
        self.assertEqual(result["distinct_projection_keys_shared_between_sources"], 1)
        self.assertEqual(result["profiles"][0]["group_summaries"][0]["source_child_count"], 2)
        self.assertFalse(result["content_catalog_complete"])
        reports[1]["records"][0]["projection_unit"] = 2336
        self.assertEqual(compare(reports)["distinct_projection_keys_shared_between_sources"], 0)


if __name__ == "__main__":
    unittest.main()
