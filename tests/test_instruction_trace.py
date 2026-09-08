"""Invented registers/code/RAM exercise guarded, bounded instruction observations."""

import copy
import hashlib
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

    def test_scratchpad_aliases_read_only_the_bounded_backing_bytes(self):
        scratchpad = bytearray(1024)
        scratchpad[130:134] = bytes.fromhex("deadbeef")
        memory_before, scratch_before = bytes(self.memory), bytes(scratchpad)
        for base in (0x1F800000, 0x9F800000, 0xBF800000):
            self.gpr[2] = base + 128
            registers = list(self.gpr)
            result = guarded_record(
                self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory, scratchpad
            )["ranges"][2]
            self.assertEqual(result["hex"], "deadbeef")
            self.assertEqual(result["resolved_space"], "scratchpad")
            self.assertEqual(result["resolved_offset"], 130)
            self.assertEqual(registers, self.gpr)
        self.assertEqual(memory_before, bytes(self.memory))
        self.assertEqual(scratch_before, bytes(scratchpad))

    def test_scratchpad_reads_cannot_cross_into_ram_or_hardware(self):
        scratchpad = bytes(1024)
        for pointer, relative, reason in (
            (0x1F800000, -1, "range_outside_scratchpad"),
            (0x1F8003FE, 0, "range_outside_scratchpad"),
            (0x1F800400, 0, "pointer_outside_system_ram"),
            (0x1F801000, 0, "pointer_outside_system_ram"),
            (0x9F7FFFFF, 1, "pointer_outside_system_ram"),
        ):
            self.gpr[2] = pointer
            self.hook["ranges"][2]["relative_offset"] = relative
            row = guarded_record(
                self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory, scratchpad
            )["ranges"][2]
            with self.subTest(pointer=pointer, relative=relative):
                self.assertEqual(row["unavailable"], reason)
                self.assertNotIn("hex", row)
        for wrong_size in (0, 1023, 1025, 8192):
            with self.assertRaises(ValueError):
                guarded_record(
                    self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory, bytes(wrong_size)
                )

    def test_full_region_hashes_do_not_copy_payload_or_modify_state(self):
        self.hook["digests"] = [
            {"name": "fixed", "pointer_offset": 16, "size": 128, "max_bytes": 128},
            {"name": "registers", "register": 2, "end_register": 3, "max_bytes": 128},
        ]
        self.gpr[3] = 0x80000100
        validate_instruction_trace(self.spec)
        memory, registers = bytes(self.memory), list(self.gpr)
        result = guarded_record(self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory)
        for digest in result["digests"]:
            self.assertEqual(digest["sha256"], hashlib.sha256(memory[128:]).hexdigest())
            self.assertEqual(digest["size"], 128)
            self.assertNotIn("hex", digest)
        self.assertEqual(bytes(self.memory), memory)
        self.assertEqual(self.gpr, registers)

    def test_digest_lengths_pointers_and_aliases_never_wrap(self):
        self.hook["digests"] = [
            {"name": "output", "register": 2, "end_register": 3, "max_bytes": 128}
        ]
        for start, end, reason in [
            (0, 1, "null_pointer"),
            (0x1F800000, 0x1F800004, "pointer_outside_system_ram"),
            (0x80000080, 0x8000007F, "length_outside_digest_bound"),
            (0x80000080, 0xA0000084, "length_outside_digest_bound"),
            (0x80000080, 0x80000101, "length_outside_digest_bound"),
            (0x800000FF, 0x80000101, "range_outside_exposed_ram"),
        ]:
            self.gpr[2:4] = [start, end]
            result = guarded_record(self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory)
            with self.subTest(start=start, end=end):
                self.assertEqual(result["digests"][0]["unavailable"], reason)
                self.assertNotIn("sha256", result["digests"][0])
        self.gpr[2:4] = [0x80000080, 0x80000080]
        result = guarded_record(self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory)
        self.assertEqual(result["digests"][0]["sha256"], hashlib.sha256(b"").hexdigest())

    def test_digest_pointer_source_is_bounded(self):
        self.hook["digests"] = [{"name": "fixed", "pointer_offset": 254, "size": 1, "max_bytes": 1}]
        result = guarded_record(self.hook, self.hook["pc"], 0x90420000, self.gpr, self.memory)
        self.assertEqual(result["digests"][0]["unavailable"], "pointer_source_outside_exposed_ram")

    def test_digest_configuration_and_hashing_budget_are_bounded(self):
        valid = {"name": "digest", "register": 2, "end_register": 3, "max_bytes": 128}
        variants = [[], [valid, valid]]
        for key, value in [
            ("register", 34),
            ("end_register", -1),
            ("max_bytes", True),
            ("max_bytes", 0x200001),
            ("size", 128),
            ("pointer_offset", 16),
            ("max_bytes", 0x200000),
            ("name", "invalid/name"),
            ("mutate", True),
        ]:
            variants.append([{**valid, key: value}])
        variants += [[{k: v for k, v in valid.items() if k != key}] for key in valid]
        for digests in variants:
            self.hook["digests"] = digests
            with self.subTest(digests=digests), self.assertRaises(ValueError):
                validate_instruction_trace(self.spec)


if __name__ == "__main__":
    unittest.main()
