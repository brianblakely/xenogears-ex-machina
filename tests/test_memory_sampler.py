"""Invented RAM verifies sampling bounds, pointer handling and observation-only behavior."""

import copy
import tempfile
import unittest
from pathlib import Path

from tools.reference.memory_sampler import MemorySampler, load_sampling, validate_sampling


class MemorySamplerTests(unittest.TestCase):
    def setUp(self):
        self.spec = {
            "schema_version": 1,
            "name": "invented-actor-sample",
            "source_profile": "synthetic-source",
            "start_frame": 2,
            "every_frames": 3,
            "max_samples": 2,
            "ranges": [
                {"name": "direct", "offset": 8, "size": 4},
                {"name": "actor", "pointer_offset": 16, "relative_offset": 4, "size": 2},
            ],
        }
        self.memory = bytearray(128)
        self.memory[8:12] = bytes.fromhex("01020304")
        self.memory[16:20] = (0x80000040).to_bytes(4, "little")
        self.memory[68:70] = bytes.fromhex("aabb")

    def test_cadence_budget_and_reads_leave_memory_unchanged(self):
        sampler = MemorySampler(self.spec)
        before = bytes(self.memory)
        captures = [sampler.sample(frame, self.memory) for frame in range(10)]
        samples = [capture for capture in captures if capture is not None]
        self.assertEqual([sample["frame"] for sample in samples], [2, 5])
        self.assertEqual(samples[0]["ranges"][0]["hex"], "01020304")
        self.assertEqual(samples[0]["ranges"][1]["hex"], "aabb")
        self.assertEqual(samples[0]["ranges"][1]["resolved_offset"], 68)
        self.assertEqual(bytes(self.memory), before)
        self.assertEqual(sampler.status()["payload_bytes"], 12)
        self.assertTrue(sampler.status()["budget_reached"])
        self.assertEqual(sampler.status()["unavailable_by_range"], {"direct": 0, "actor": 0})

    def test_pointer_is_resolved_again_and_aliases_preserve_physical_offset(self):
        for alias in (0, 0x80000000, 0xA0000000):
            with self.subTest(alias=alias):
                sampler = MemorySampler(self.spec)
                first = sampler.sample(2, self.memory)
                self.memory[16:20] = (alias + 80).to_bytes(4, "little")
                self.memory[84:86] = bytes.fromhex("ccdd")
                second = sampler.sample(5, self.memory)
                self.assertEqual(second["ranges"][1]["hex"], "ccdd")
                self.assertEqual(second["ranges"][1]["pointer_value"], alias + 80)
                self.assertEqual(second["ranges"][1]["resolved_offset"], 84)
                self.assertNotEqual(first["frame"], second["frame"])

    def test_null_io_unmapped_wrapped_and_truncated_pointers_are_explicit(self):
        cases = [
            (0, "null_pointer"),
            (0x1F801000, "pointer_outside_system_ram"),
            (0xDEADBEEF, "pointer_outside_system_ram"),
            (0x80200000, "pointer_outside_system_ram"),
            (0xBFC00000, "pointer_outside_system_ram"),
            (0x8000007F, "range_outside_exposed_ram"),
        ]
        for pointer, reason in cases:
            with self.subTest(pointer=pointer):
                sampler = MemorySampler(self.spec)
                self.memory[16:20] = pointer.to_bytes(4, "little")
                result = sampler.sample(2, self.memory)["ranges"][1]
                self.assertEqual(result["unavailable"], reason)
                self.assertNotIn("hex", result)
                self.assertEqual(sampler.status()["unavailable_by_range"]["actor"], 1)
        sampler = MemorySampler(self.spec)
        result = sampler.sample(2, self.memory[:18])["ranges"][1]
        self.assertEqual(result["unavailable"], "pointer_source_outside_exposed_ram")

    def test_relative_offsets_never_wrap_or_read_negative_slices(self):
        self.spec["ranges"][1]["relative_offset"] = -65
        sampler = MemorySampler(self.spec)
        result = sampler.sample(2, self.memory)["ranges"][1]
        self.assertEqual(result["resolved_offset"], -1)
        self.assertEqual(result["unavailable"], "range_outside_exposed_ram")
        with self.assertRaisesRegex(ValueError, "increase"):
            sampler.sample(2, self.memory)
        with self.assertRaisesRegex(ValueError, "bounded"):
            MemorySampler(self.spec).sample(2, b"")

    def test_invalid_ambiguous_and_excessive_specs_are_rejected(self):
        variants = []
        for key, value in (
            ("schema_version", True),
            ("every_frames", 0),
            ("start_frame", -1),
            ("max_samples", 36002),
            ("ranges", []),
            ("ranges", self.spec["ranges"] * 17),
            ("write_value", "00"),
        ):
            variant = copy.deepcopy(self.spec)
            variant[key] = value
            variants.append(variant)
        for change in (
            {"offset": -1},
            {"offset": 2097150},
            {"size": True},
            {"size": 1025},
            {"pointer_offset": 20},
            {"pointer_chain": [20, 30]},
            {"name": "actor"},
        ):
            variant = copy.deepcopy(self.spec)
            variant["ranges"][0].update(change)
            variants.append(variant)
        excessive = copy.deepcopy(self.spec)
        excessive["max_samples"] = 36001
        excessive["ranges"] = [
            {"name": f"range-{index}", "offset": index * 1024, "size": 1024} for index in range(3)
        ]
        variants.append(excessive)
        for variant in variants:
            with self.subTest(variant=variant), self.assertRaises(ValueError):
                validate_sampling(variant)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "oversized.json"
            path.write_bytes(b" " * 65537)
            with self.assertRaisesRegex(ValueError, "64 KiB"):
                load_sampling(path)


if __name__ == "__main__":
    unittest.main()
