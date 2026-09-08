"""Invented registers/code/RAM exercise guarded, bounded instruction observations."""

import copy
import unittest

from tools.reference.instruction_trace import guarded_record, validate_instruction_trace


class InstructionTraceTests(unittest.TestCase):
    def setUp(self):
        self.memory = bytearray(256)
        self.memory[64:72] = bytes.fromhex("0000429021004234")
        self.memory[16:20] = (0x80000080).to_bytes(4, "little")
        self.memory[130:134] = bytes.fromhex("01020304")
        self.gpr = [0] * 34
        self.gpr[2] = 0x80000080
        self.gpr[16] = 19
        self.hook = {
            "name": "synthetic-dispatch",
            "pc": 0x80000040,
            "guard": {"offset": 64, "expected": "0000429021004234"},
            "ranges": [
                {"name": "direct", "offset": 130, "size": 4},
                {"name": "actor", "pointer_offset": 16, "relative_offset": 2, "size": 4},
                {"name": "code", "register": 2, "relative_offset": 2, "size": 4},
            ],
        }
        self.spec = {
            "schema_version": 1,
            "name": "synthetic-trace",
            "source_profile": "synthetic",
            "start_frame": 2,
            "end_frame": 20,
            "max_callbacks": 100,
            "hooks": [self.hook],
        }

    def test_guarded_register_and_pointer_reads_are_observation_only(self):
        validate_instruction_trace(self.spec)
        before = bytes(self.memory)
        registers = self.gpr.copy()
        result = guarded_record(
            self.hook, self.hook["pc"], 0x90420000, self.gpr, memoryview(self.memory).toreadonly()
        )
        self.assertEqual([item["hex"] for item in result["ranges"]], ["01020304"] * 3)
        self.assertEqual(result["gpr_u32"][16], 19)
        self.assertEqual(bytes(self.memory), before)
        self.assertEqual(self.gpr, registers)

    def test_reused_address_and_stale_fetched_instruction_fail_the_guard(self):
        for pc, code, changed in (
            (0x80000040, 0, False),
            (0x80000044, 0x90420000, False),
            (0x80000040, 0x90420000, True),
        ):
            memory = self.memory.copy()
            if changed:
                memory[68] ^= 1
            with self.subTest(pc=pc, code=code, changed=changed):
                self.assertIsNone(guarded_record(self.hook, pc, code, self.gpr, memory))

    def test_invalid_register_pointers_are_recorded_without_wrapping(self):
        for value, reason in (
            (0, "null_pointer"),
            (0x1F800000, "pointer_outside_system_ram"),
            (0x80200000, "pointer_outside_system_ram"),
            (0x800000FE, "range_outside_exposed_ram"),
        ):
            self.gpr[2] = value
            result = guarded_record(self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory)
            with self.subTest(value=value):
                self.assertEqual(result["ranges"][2]["unavailable"], reason)
                self.assertNotIn("hex", result["ranges"][2])

    def test_unbounded_incomplete_ambiguous_and_misaligned_hooks_are_rejected(self):
        variants = []
        for key, value in (
            ("max_callbacks", 1_000_001),
            ("end_frame", 2),
            ("schema_version", True),
            ("hooks", []),
        ):
            variant = copy.deepcopy(self.spec)
            variant[key] = value
            variants.append(variant)
        for key, value in (
            ("pc", 0x80000041),
            ("pc", 0x1F801000),
            ("guard", {"offset": 64, "expected": "0000"}),
            ("ranges", [{"name": "register", "register": 34, "relative_offset": 0, "size": 4}]),
        ):
            variant = copy.deepcopy(self.spec)
            variant["hooks"][0][key] = value
            variants.append(variant)
        duplicate = copy.deepcopy(self.spec)
        duplicate["hooks"] *= 2
        variants.append(duplicate)
        excessive = copy.deepcopy(self.spec)
        excessive["max_callbacks"] = 1_000_000
        variants.append(excessive)
        for variant in variants:
            with self.subTest(variant=variant), self.assertRaises(ValueError):
                validate_instruction_trace(variant)


if __name__ == "__main__":
    unittest.main()
