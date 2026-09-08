"""Invented raw-CD headers test survey accounting without any game content."""

from __future__ import annotations

import hashlib
import io
import unittest

from tools.reference.inspect_disc import RAW_SECTOR_SIZE, SYNC
from tools.reference.sector_survey import classify, survey


def sector(submode: int) -> bytes:
    raw = bytearray(RAW_SECTOR_SIZE)
    raw[:12] = SYNC
    raw[15] = 2
    raw[16:20] = raw[20:24] = bytes([1, 2, submode, 0])
    return bytes(raw)


class SectorSurveyTests(unittest.TestCase):
    def test_exact_hash_counts_and_half_open_span_boundaries(self) -> None:
        raw = sector(0x08) * 2 + sector(0x64) + sector(0x08)
        measured = survey(io.BytesIO(raw), len(raw))
        self.assertEqual(measured["raw_track_sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(measured["surveyed_sectors"], 4)
        self.assertEqual(
            measured["header_class_counts"], {"mode2_form1_data": 3, "mode2_form2_audio": 1}
        )
        self.assertEqual(
            [
                (row["start_lba"], row["end_lba_exclusive"])
                for row in measured["contiguous_header_spans"]
            ],
            [(0, 2), (2, 3), (3, 4)],
        )

    def test_unknown_header_never_becomes_content(self) -> None:
        self.assertEqual(classify(bytes(RAW_SECTOR_SIZE))[0], "no_data_sync")
        self.assertEqual(classify(sector(0))[0], "mode2_form1_untyped")

    def test_truncation_size_changes_and_inconsistent_subheaders_fail(self) -> None:
        raw = sector(0x08)
        for stream, size in (
            (raw[:-1], len(raw)),
            (raw, len(raw) * 2),
            (raw * 2, len(raw)),
            (raw, 0),
        ):
            with self.assertRaises(ValueError):
                survey(io.BytesIO(stream), size)
        bad = bytearray(raw)
        bad[20] = 2
        with self.assertRaises(ValueError):
            classify(bytes(bad))

    def test_xa_end_markers_retain_exact_source_lbas(self) -> None:
        raw = sector(0x08) + sector(0x09) + sector(0x89)
        markers = survey(io.BytesIO(raw), len(raw))["xa_record_markers"]
        self.assertEqual([row["lba"] for row in markers], [1, 2])
        self.assertEqual([row["end_of_file"] for row in markers], [False, True])
        self.assertTrue(all(row["end_of_record"] for row in markers))


if __name__ == "__main__":
    unittest.main()
