"""Authored captures exercise pointer provenance across RAM and scratch aliases."""

import copy
import unittest

from tools.analysis.sweep_trace import qualified_ranges


def capture():
    gpr = [0] * 34
    gpr[4], gpr[5] = 0x9F80000C, 0xBF800010
    hook = {
        "name": "authored",
        "pc": 0x80010010,
        "guard": {"offset": 0x10000, "expected": "00" * 32},
        "ranges": [
            {"name": "storage", "offset": 0x100, "size": 4},
            {"name": "actor", "pointer_offset": 0x100, "relative_offset": 0, "size": 8},
            {"name": "scratch-a", "register": 4, "relative_offset": 0, "size": 8},
            {"name": "scratch-b", "register": 5, "relative_offset": 0, "size": 4},
        ],
    }
    row = {
        "pc": hook["pc"],
        "code": 0,
        "gpr_u32": gpr,
        "ranges": [
            {"name": "storage", "size": 4, "resolved_offset": 0x100, "hex": "00020080"},
            {
                "name": "actor",
                "size": 8,
                "pointer_offset": 0x100,
                "relative_offset": 0,
                "pointer_value": 0x80000200,
                "resolved_offset": 0x200,
                "hex": "0011223344556677",
            },
            {
                "name": "scratch-a",
                "size": 8,
                "register": 4,
                "relative_offset": 0,
                "register_value": gpr[4],
                "resolved_space": "scratchpad",
                "resolved_offset": 12,
                "hex": "1122334455667788",
            },
            {
                "name": "scratch-b",
                "size": 4,
                "register": 5,
                "relative_offset": 0,
                "register_value": gpr[5],
                "resolved_space": "scratchpad",
                "resolved_offset": 16,
                "hex": "55667788",
            },
        ],
    }
    return row, hook


class SweepTraceTests(unittest.TestCase):
    def test_cached_and_uncached_scratch_aliases_agree(self):
        row, hook = capture()
        result = qualified_ranges(row, hook)
        self.assertEqual(result["scratch-a"][4:], result["scratch-b"])
        self.assertEqual(result["actor"], bytes.fromhex("0011223344556677"))

    def test_conflicting_scratch_alias_is_rejected(self):
        row, hook = capture()
        row["ranges"][3]["hex"] = "54667788"
        with self.assertRaisesRegex(ValueError, "overlapping"):
            qualified_ranges(row, hook)

    def test_pointer_value_requires_captured_storage(self):
        row, hook = capture()
        row["ranges"][1].update(pointer_value=0x80000400, resolved_offset=0x400)
        with self.assertRaisesRegex(ValueError, "pointer storage"):
            qualified_ranges(row, hook)

    def test_register_provenance_cannot_be_replaced_by_direct_range(self):
        row, hook = capture()
        row["ranges"][2]["offset"] = 12
        with self.assertRaisesRegex(ValueError, "scratchpad reference"):
            qualified_ranges(row, hook)

    def test_scratch_region_cannot_be_reported_as_ram(self):
        row, hook = capture()
        row["ranges"][2]["resolved_space"] = "ram"
        with self.assertRaisesRegex(ValueError, "different region"):
            qualified_ranges(row, hook)

    def test_register_payload_cannot_cross_scratch_end(self):
        row, hook = capture()
        row["gpr_u32"][4] = 0x1F8003FC
        row["ranges"][2].update(register_value=0x1F8003FC, resolved_offset=1020)
        with self.assertRaisesRegex(ValueError, "different region"):
            qualified_ranges(row, hook)

    def test_uncached_ram_pointer_still_requires_matching_alias_in_storage(self):
        row, hook = capture()
        row["ranges"][1]["pointer_value"] = 0xA0000200
        with self.assertRaisesRegex(ValueError, "pointer storage"):
            qualified_ranges(row, hook)
        updated = copy.deepcopy(row)
        updated["ranges"][0]["hex"] = "000200a0"
        self.assertEqual(
            qualified_ranges(updated, hook)["actor"], bytes.fromhex("0011223344556677")
        )


if __name__ == "__main__":
    unittest.main()
