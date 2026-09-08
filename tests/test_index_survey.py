"""Original invented index candidates test parsing, bounds, unknowns and grouping."""

from __future__ import annotations

import unittest

from tools.reference.index_survey import parse_candidates


def entry(lba: int, size: int) -> bytes:
    return lba.to_bytes(3, "little") + size.to_bytes(4, "little", signed=True)


class IndexSurveyTests(unittest.TestCase):
    def test_group_sizes_slot_coordinates_and_unknowns_are_preserved(self) -> None:
        data = b"".join(
            entry(lba, size)
            for lba, size in [(10, -2), (10, 2048), (11, 24), (0, 0), (12, 0), (0xFFFFFF, 0)]
        )
        result = parse_candidates(data + bytes(6), 20)
        self.assertEqual(result["groups"][0]["positive_child_count"], 2)
        self.assertEqual(result["groups"][0]["first_child_slot"], 1)
        self.assertEqual(result["records"][2]["table_byte_offset"], 14)
        self.assertEqual(result["records"][3]["kind"], "zero_slot_unknown_or_padding")
        self.assertEqual(result["records"][4]["kind"], "zero_length_marker_unknown")
        self.assertEqual(result["terminal"]["slot"], 5)

    def test_bad_group_count_and_start_are_rejected(self) -> None:
        for records in ([(10, -2), (10, 1)], [(10, -1), (11, 1)], [(10, -1), (0, 0)]):
            data = b"".join(entry(*row) for row in records) + entry(0xFFFFFF, 0)
            with self.assertRaises(ValueError):
                parse_candidates(data, 20)

    def test_truncated_terminal_out_of_bounds_and_trailing_payload_are_rejected(self) -> None:
        for data in (
            entry(10, 1),
            entry(0xFFFFFF, 0)[:-1],
            entry(21, 1) + entry(0xFFFFFF, 0),
            entry(0xFFFFFF, 0) + b"\x01",
            entry(10, 50000) + entry(0xFFFFFF, 0),
        ):
            with self.assertRaises(ValueError):
                parse_candidates(data, 20)


if __name__ == "__main__":
    unittest.main()
