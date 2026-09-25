"""Invented registers/code/RAM exercise guarded, bounded instruction observations."""

import copy
import ctypes as ct
import hashlib
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

from tools.reference.instruction_trace import (
    COVERAGE_BITMAP,
    COVERAGE_BYTES,
    COVERAGE_MAGIC,
    COVERAGE_WORDS,
    SNAPSHOT_BYTES,
    SNAPSHOT_HEADER,
    SNAPSHOT_KEY_INTERVAL,
    SNAPSHOT_PAGE,
    CoverageTrace,
    InstructionTrace,
    ScratchpadCallback,
    SnapshotReader,
    SnapshotWriter,
    executed_offsets,
    guarded_record,
    read_coverage_records,
    snapshot_path,
    split_coverage,
    validate_coverage,
    validate_instruction_trace,
)
from tools.reference.scenario_program import MEMORY_LIMIT


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

    def trace_core(self):
        return SimpleNamespace(
            retro_xem_trace_enable=Mock(),
            retro_xem_trace_count=Mock(return_value=1),
            retro_xem_trace_configure=Mock(return_value=1),
        )

    def test_missing_trace_exports_reject_before_configuration_or_output(self):
        for name in (
            "retro_xem_trace_enable",
            "retro_xem_trace_count",
            "retro_xem_trace_configure",
        ):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                core = self.trace_core()
                configure, enable = core.retro_xem_trace_configure, core.retro_xem_trace_enable
                delattr(core, name)
                output = Path(directory) / "trace.jsonl"
                with self.assertRaises(ValueError):
                    trace = InstructionTrace(core, self.spec, output, [])
                    trace.finish()
                configure.assert_not_called()
                enable.assert_not_called()
                self.assertFalse(output.exists())

    @staticmethod
    def cop2():
        return (ct.c_uint32 * 64)(*range(0x100, 0x140))

    @staticmethod
    def load_delay():
        return (ct.c_uint32 * 5)(1, 0, 4, 0, 0x800AFC98)

    def test_versionless_callback_records_ram_and_scratchpad_without_mutation(self):
        core = self.trace_core()
        ram = (ct.c_uint8 * MEMORY_LIMIT)()
        ram[: len(self.memory)] = self.memory
        scratchpad = (ct.c_uint8 * 0x2000)()
        scratchpad[130:134] = bytes.fromhex("deadbeef")
        registers = (ct.c_uint32 * 34)(*self.gpr)
        registers[2] = 0x1F800080
        before = bytes(ram), bytes(scratchpad), list(registers)
        errors = []
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trace.jsonl"
            trace = InstructionTrace(core, self.spec, output, errors)
            try:
                self.assertEqual(trace.spec["schema_version"], 1)
                configure = core.retro_xem_trace_configure
                self.assertIs(configure.argtypes[-1], ScratchpadCallback)
                addresses, count, budget, callback = configure.call_args.args
                self.assertEqual(list(addresses), [self.hook["pc"]])
                self.assertEqual((count, budget), (1, self.spec["max_callbacks"]))
                self.assertIsInstance(callback, ScratchpadCallback)
                trace.start_run(2)
                core.retro_xem_trace_enable.assert_called_with(True)
                callback(
                    0,
                    self.hook["pc"],
                    0x90420000,
                    123,
                    4,
                    0,
                    registers,
                    ram,
                    scratchpad,
                    self.cop2(),
                    self.load_delay(),
                )
            finally:
                status = trace.finish()
            record = json.loads(output.read_text())
            self.assertEqual(record["frontend_run"], 2)
            self.assertEqual(record["gpr_u32"], list(registers))
            self.assertEqual(record["cop2_u32"], list(range(0x100, 0x140)))
            self.assertEqual(
                record["load_delay"],
                {"select": 1, "registers": [0, 4], "values": [0, 0x800AFC98]},
            )
            self.assertEqual(
                [item["hex"] for item in record["ranges"]],
                ["01020304", "01020304", "deadbeef"],
            )
            self.assertEqual(record["ranges"][2]["resolved_space"], "scratchpad")
            self.assertEqual(
                status["trace_sha256"], hashlib.sha256(output.read_bytes()).hexdigest()
            )
        self.assertEqual((bytes(ram), bytes(scratchpad), list(registers)), before)
        self.assertEqual(status["records"], 1)
        self.assertEqual(status["unavailable_ranges"], 0)
        self.assertFalse(status["failed"])
        self.assertEqual(errors, [])

    def test_snapshot_hooks_store_exact_complete_memory_within_budget(self):
        core = self.trace_core()
        spec = copy.deepcopy(self.spec)
        spec["hooks"][0]["snapshot"] = True
        spec["max_snapshots"] = 1
        ram = (ct.c_uint8 * MEMORY_LIMIT)()
        ram[: len(self.memory)] = self.memory
        ram[MEMORY_LIMIT - 1] = 0x5A
        # Hardware backing: scratchpad, then the I/O page at +1000.
        scratchpad = (ct.c_uint8 * 0x2000)()
        scratchpad[1023] = 0xA5
        scratchpad[0x10B8] = 0x5C  # A DMA register byte.
        registers = (ct.c_uint32 * 34)(*self.gpr)
        before = bytes(ram), bytes(scratchpad[:1024]), bytes(scratchpad[0x1000:])
        errors = []
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trace.jsonl"
            trace = InstructionTrace(core, spec, output, errors)
            trace.start_run(2)
            callback = core.retro_xem_trace_configure.call_args.args[3]
            for _ in range(2):
                callback(
                    0,
                    self.hook["pc"],
                    0x90420000,
                    1,
                    0,
                    0,
                    registers,
                    ram,
                    scratchpad,
                    self.cop2(),
                    self.load_delay(),
                )
            status = trace.finish()
            record = json.loads(output.read_text().splitlines()[0])
            self.assertEqual(SnapshotReader(snapshot_path(output)).read(record), before)
            self.assertEqual(
                status["snapshot_file_sha256"],
                hashlib.sha256(snapshot_path(output).read_bytes()).hexdigest(),
            )
            corrupt = copy.deepcopy(record)
            corrupt["snapshot"]["ram_sha256"] = "0" * 64
            with self.assertRaisesRegex(ValueError, "digests"):
                SnapshotReader(snapshot_path(output)).read(corrupt)
        self.assertEqual((bytes(ram), bytes(scratchpad[:1024]), bytes(scratchpad[0x1000:])), before)
        self.assertEqual((status["snapshots"], status["records"]), (1, 1))
        self.assertTrue(status["failed"])
        self.assertIn("snapshot budget", errors[0])

    def test_snapshot_deltas_reconstruct_any_order_across_keyframes(self):
        images, records = [], []
        image = bytearray(SNAPSHOT_BYTES)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshots.bin"
            with path.open("xb") as stream:
                writer = SnapshotWriter(stream)
                for step in range(SNAPSHOT_KEY_INTERVAL + 3):
                    # Scattered writes: a page edge, the scratchpad and the last byte.
                    image[(step * 7919) % SNAPSHOT_BYTES] = step & 0xFF
                    image[SNAPSHOT_PAGE - 1] = step & 0x7F
                    image[MEMORY_LIMIT + step % 1024] ^= 0x11
                    image[-1] = (step * 3) & 0xFF
                    images.append(bytes(image))
                    records.append({"snapshot": self.digests(image, writer.write(bytes(image)))})
            self.assertEqual(
                writer.digest.hexdigest(), hashlib.sha256(path.read_bytes()).hexdigest()
            )
            # Far smaller than complete images: one keyframe of mostly zeros plus deltas.
            self.assertLess(path.stat().st_size, 64 * 1024)
            reader = SnapshotReader(path)
            order = [
                5,
                6,
                SNAPSHOT_KEY_INTERVAL + 2,
                3,
                SNAPSHOT_KEY_INTERVAL,
                0,
                SNAPSHOT_KEY_INTERVAL - 1,
            ]
            for index in order:
                ram, scratchpad, io = reader.read(records[index])
                self.assertEqual(ram + scratchpad + io, images[index], index)
            with self.assertRaisesRegex(ValueError, "outside the file"):
                reader.read({"snapshot": {**records[0]["snapshot"], "sequence": len(records)}})
            data = path.read_bytes()
            for broken, message in (
                (b"XEMSNAP1" + data[8:], "header"),
                (data[:-1], "ends inside a chunk"),
                (data + b"\x01", "ends inside a chunk length"),
            ):
                path.write_bytes(broken)
                with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                    SnapshotReader(path)
            self.assertEqual(SNAPSHOT_HEADER, data[: len(SNAPSHOT_HEADER)])
            # A keyframe followed by a delta naming a page beyond the image, and
            # a keyframe chunk with bytes after its compressed stream.
            key = zlib.compress(images[0], 6)
            bad_page = zlib.compress(struct.pack("<II", 1, SNAPSHOT_BYTES) + bytes(SNAPSHOT_PAGE))
            for chunks, sequence, message in (
                ((key, bad_page), 1, "outside the image"),
                ((key + b"\x00",), 0, "exactly one compressed stream"),
            ):
                path.write_bytes(
                    SNAPSHOT_HEADER + b"".join(struct.pack("<I", len(c)) + c for c in chunks)
                )
                reader = SnapshotReader(path)
                with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                    reader.read({"snapshot": {**records[sequence]["snapshot"]}})

    @staticmethod
    def digests(image: bytes, sequence: int) -> dict:
        return {
            "sequence": sequence,
            "ram_sha256": hashlib.sha256(image[:MEMORY_LIMIT]).hexdigest(),
            "scratchpad_sha256": hashlib.sha256(
                image[MEMORY_LIMIT : MEMORY_LIMIT + 1024]
            ).hexdigest(),
            "io_sha256": hashlib.sha256(image[MEMORY_LIMIT + 1024 :]).hexdigest(),
        }

    def test_snapshot_declarations_must_be_paired_and_bounded(self):
        for hook_value, budget in ((True, None), (None, 4), (False, 4), (True, 0), (True, 8193)):
            spec = copy.deepcopy(self.spec)
            if hook_value is not None:
                spec["hooks"][0]["snapshot"] = hook_value
            if budget is not None:
                spec["max_snapshots"] = budget
            with self.subTest(hook=hook_value, budget=budget), self.assertRaises(ValueError):
                validate_instruction_trace(spec)

    def test_null_scratchpad_stops_even_a_ram_only_capture(self):
        core = self.trace_core()
        ram = (ct.c_uint8 * MEMORY_LIMIT)()
        registers = (ct.c_uint32 * 34)(*self.gpr)
        errors = []
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trace.jsonl"
            trace = InstructionTrace(core, self.spec, output, errors)
            try:
                trace.callback(
                    0,
                    self.hook["pc"],
                    0x90420000,
                    0,
                    0,
                    0,
                    registers,
                    ram,
                    None,
                    self.cop2(),
                    self.load_delay(),
                )
                core.retro_xem_trace_enable.assert_called_with(0)
            finally:
                status = trace.finish()
            self.assertEqual(output.read_bytes(), b"")
        self.assertTrue(status["failed"])
        self.assertEqual(status["records"], 0)
        self.assertEqual(errors, ["Instruction trace: External core supplied a null scratchpad"])

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

    def test_up_to_128_distinct_hooks_are_accepted(self):
        def hooks(count):
            spec = copy.deepcopy(self.spec)
            spec["max_callbacks"] = 1000
            spec["hooks"] = []
            for index in range(count):
                hook = copy.deepcopy(self.hook)
                hook["name"] = f"hook-{index}"
                hook["pc"] = 0x80000040 + 4 * index
                hook["guard"] = {"offset": 64 + 4 * index, "expected": "00000000"}
                spec["hooks"].append(hook)
            return spec

        self.assertEqual(len(validate_instruction_trace(hooks(128))["hooks"]), 128)
        with self.assertRaisesRegex(ValueError, "1..128 hooks"):
            validate_instruction_trace(hooks(129))

    def vram_callback(self, rect: bytes, reader) -> tuple[dict, list[str], dict]:
        core = self.trace_core()
        core.retro_xem_vram_read = reader
        spec = copy.deepcopy(self.spec)
        spec["hooks"][0]["vram"] = {"register": 4}
        ram = (ct.c_uint8 * MEMORY_LIMIT)()
        ram[: len(self.memory)] = self.memory
        ram[192:200] = rect
        registers = (ct.c_uint32 * 34)(*self.gpr)
        registers[4] = 0x800000C0
        errors = []
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trace.jsonl"
            trace = InstructionTrace(core, spec, output, errors)
            try:
                trace.start_run(2)
                callback = core.retro_xem_trace_configure.call_args.args[3]
                scratchpad = (ct.c_uint8 * 0x2000)()
                callback(
                    0,
                    self.hook["pc"],
                    0x90420000,
                    123,
                    4,
                    0,
                    registers,
                    ram,
                    scratchpad,
                    self.cop2(),
                    self.load_delay(),
                )
            finally:
                status = trace.finish()
            text = output.read_text()
        return (json.loads(text) if text else {}), errors, status

    def test_vram_rectangles_read_the_pointed_rectangle_from_the_core(self):
        calls = []

        def reader(x, y, w, h, buffer, size):
            calls.append((x, y, w, h, size))
            for i in range(size):
                buffer[i] = i & 0xFF
            return 1

        record, errors, status = self.vram_callback(struct.pack("<4h", 0x3C0, 0x100, 2, 3), reader)
        self.assertEqual(errors, [])
        self.assertEqual(calls, [(0x3C0, 0x100, 2, 3, 12)])
        self.assertEqual(record["vram"]["rect"], [0x3C0, 0x100, 2, 3])
        self.assertEqual(record["vram"]["hex"], bytes(range(12)).hex())
        self.assertEqual(record["vram"]["sha256"], hashlib.sha256(bytes(range(12))).hexdigest())
        self.assertEqual(status["vram_bytes"], 12)

    def test_vram_rectangles_outside_vram_are_unavailable_without_a_read(self):
        reader = Mock(return_value=1)
        for rect in ((0x3F0, 0, 0x20, 1), (0, 0x1FF, 1, 2), (0, 0, 0, 1), (-1, 0, 1, 1)):
            with self.subTest(rect=rect):
                record, errors, _ = self.vram_callback(struct.pack("<4h", *rect), reader)
                self.assertEqual(errors, [])
                self.assertEqual(record["vram"]["unavailable"], "rectangle_outside_vram")
        reader.assert_not_called()

    def test_a_rejected_vram_read_fails_the_capture(self):
        record, errors, status = self.vram_callback(
            struct.pack("<4h", 0, 0, 1, 1), Mock(return_value=0)
        )
        self.assertEqual(record, {})
        self.assertTrue(status["failed"])
        self.assertIn("rejected a VRAM read-back", errors[0])

    def test_vram_rectangles_need_the_core_export_and_a_register(self):
        spec = copy.deepcopy(self.spec)
        spec["hooks"][0]["vram"] = {"register": 4}
        with (
            tempfile.TemporaryDirectory() as directory,
            self.assertRaisesRegex(ValueError, "observation-trace core"),
        ):
            InstructionTrace(self.trace_core(), spec, Path(directory) / "t.jsonl", [])
        for vram in ({}, {"register": 34}, {"register": 4, "size": 8}):
            spec["hooks"][0]["vram"] = vram
            with self.subTest(vram=vram), self.assertRaises(ValueError):
                validate_instruction_trace(spec)

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


class CoverageTests(unittest.TestCase):
    """An invented core hands out bitmaps; the collector only reads and hashes RAM."""

    def setUp(self):
        self.spec = {
            "schema_version": 1,
            "name": "synthetic-coverage",
            "source_profile": "synthetic",
            "windows": [
                {"name": "first", "start_frame": 1, "end_frame": 3},
                {"name": "second", "start_frame": 3, "end_frame": 5},
                {"name": "late", "start_frame": 8, "end_frame": 12},
            ],
            "code_ranges": [
                {"name": "low", "offset": 0x100, "size": 8},
                {"name": "high", "offset": MEMORY_LIMIT - 8, "size": 8},
            ],
        }

    def coverage_core(self, bitmaps):
        pending = list(bitmaps)

        def take(buffer, size):
            data = pending.pop(0) if pending else bytes(COVERAGE_BYTES)
            ct.memmove(buffer, data, size)
            return 1

        return SimpleNamespace(
            retro_xem_coverage_enable=Mock(), retro_xem_coverage_take=Mock(side_effect=take)
        )

    @staticmethod
    def bitmap(*offsets, changed=()):
        """A coverage record whose executed words fetched `offset + 1`."""
        data = bytearray(COVERAGE_BYTES)
        words = memoryview(data)[2 * COVERAGE_BITMAP :].cast("I")
        for offset in offsets:
            word = offset // 4
            data[word // 8] |= 1 << (word % 8)
            words[word] = words[COVERAGE_WORDS + word] = offset + 1
        for offset in changed:
            word = offset // 4
            data[COVERAGE_BITMAP + word // 8] |= 1 << (word % 8)
            words[COVERAGE_WORDS + word] = 0xFFFFFFFF
        return bytes(data)

    def test_windows_record_words_and_code_hashes_without_mutation(self):
        first = self.bitmap(0, 0x100, MEMORY_LIMIT - 4)
        second = self.bitmap(0x104, changed=[0x104])
        # Window openings discard stale bits; closings return the window's bits.
        core = self.coverage_core([self.bitmap(8), first, second, self.bitmap(12)])
        memory = bytearray(MEMORY_LIMIT)
        memory[0x100:0x108] = b"codecode"
        before = bytes(memory)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            coverage = CoverageTrace(core, self.spec, output)
            enabled = []
            for frame in range(10):
                if frame == 3:
                    memory[0x100:0x108] = b"overlay!"
                view = bytes(memory) if coverage.due(frame) else None
                self.assertEqual(coverage.due(frame), frame in (1, 3, 5, 8))
                coverage.boundary(frame, view)
                enabled.append(core.retro_xem_coverage_enable.call_args.args[0])
            status = coverage.finish(bytes(memory))
            self.assertEqual(enabled, [0, 1, 1, 1, 1, 0, 0, 0, 1, 1])
            bitmaps = read_coverage_records(output / status["records"])
            self.assertEqual(
                status["records_sha256"],
                hashlib.sha256((output / status["records"]).read_bytes()).hexdigest(),
            )
            stored = {path.name for path in (output / "coverage-ranges").iterdir()}
            self.assertEqual(
                (
                    output / "coverage-ranges" / f"{hashlib.sha256(b'overlay!').hexdigest()}.bin"
                ).read_bytes(),
                b"overlay!",
            )
        self.assertEqual(bytes(memory[:0x100]), before[:0x100])
        self.assertEqual(bitmaps, [first, second, bytes(COVERAGE_BYTES)])
        self.assertEqual(executed_offsets(first), [0, 0x100, MEMORY_LIMIT - 4])
        executed, changed, first_words, last_words = split_coverage(second)
        self.assertEqual(executed_offsets(changed), [0x104])
        self.assertEqual((first_words[0x41], last_words[0x41]), (0x105, 0xFFFFFFFF))
        windows = status["windows"]
        self.assertEqual([w["name"] for w in windows], ["first", "second", "late"])
        self.assertEqual([w["executed_words"] for w in windows], [3, 1, 0])
        self.assertEqual([w["changed_words"] for w in windows], [0, 1, 0])
        self.assertEqual([w["complete"] for w in windows], [True, True, False])
        code, overlay = (hashlib.sha256(v).hexdigest() for v in (b"codecode", b"overlay!"))
        self.assertEqual(windows[0]["start_ranges"]["low"], code)
        self.assertEqual(windows[0]["end_ranges"]["low"], overlay)
        self.assertEqual(windows[1]["start_ranges"]["low"], overlay)
        self.assertEqual(len(stored), 3)
        core.retro_xem_coverage_enable.assert_called_with(0)

    def test_missing_coverage_exports_fail_before_output(self):
        for name in ("retro_xem_coverage_enable", "retro_xem_coverage_take"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                core = self.coverage_core([])
                delattr(core, name)
                with self.assertRaises(ValueError):
                    CoverageTrace(core, self.spec, Path(directory))
                self.assertEqual(list(Path(directory).iterdir()), [])

    def test_rejected_bitmap_transfer_fails(self):
        core = self.coverage_core([])
        core.retro_xem_coverage_take = Mock(return_value=0)
        with tempfile.TemporaryDirectory() as directory:
            coverage = CoverageTrace(core, self.spec, Path(directory))
            with self.assertRaises(ValueError):
                coverage.boundary(1, bytes(MEMORY_LIMIT))

    def test_windows_and_ranges_are_ordered_bounded_and_aligned(self):
        validate_coverage(self.spec)
        for mutate in (
            lambda s: s["windows"][1].update(start_frame=2),
            lambda s: s["windows"][0].update(end_frame=1),
            lambda s: s["windows"][2].update(end_frame=36001),
            lambda s: s["windows"][1].update(name="first"),
            lambda s: s["windows"].clear(),
            lambda s: s["windows"].extend(
                {"name": f"w{i}", "start_frame": 20 + i, "end_frame": 21 + i} for i in range(1022)
            ),
            lambda s: s["code_ranges"][0].update(offset=0x102),
            lambda s: s["code_ranges"][0].update(size=6),
            lambda s: s["code_ranges"][1].update(size=12),
            lambda s: s["code_ranges"][1].update(name="low"),
            lambda s: s["code_ranges"].clear(),
            lambda s: s.update(schema_version=2),
            lambda s: s.update(extra=True),
            lambda s: s["windows"][0].update(extra=True),
        ):
            spec = copy.deepcopy(self.spec)
            mutate(spec)
            with self.subTest(spec=spec), self.assertRaises(ValueError):
                validate_coverage(spec)

    def test_corrupt_bitmap_files_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "coverage.bin"
            chunk = zlib.compress(bytes(COVERAGE_BYTES - 1))
            for data in (
                b"XEMCOV00",
                COVERAGE_MAGIC + b"\x01",
                COVERAGE_MAGIC + struct.pack("<I", 100),
                COVERAGE_MAGIC + struct.pack("<I", len(chunk)) + chunk,
            ):
                path.write_bytes(data)
                with self.subTest(data=data[:12]), self.assertRaises(ValueError):
                    read_coverage_records(path)


if __name__ == "__main__":
    unittest.main()
