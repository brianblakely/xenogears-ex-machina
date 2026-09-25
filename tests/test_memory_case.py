"""Synthetic memory images exercise pairing and exact full-memory comparison."""

import json
import stat
import tempfile
import unittest
from pathlib import Path

from tools.analysis.memory_case import (
    RAM,
    STACK_BELOW_ENTRY,
    BatchRunner,
    OwnedBytes,
    compare,
    differing_positions,
    image_map,
    loop_inputs,
    pairs,
    platform_inputs,
    superseded_bytes,
    visible_registers,
    vram_read_lines,
)


class MemoryCaseTests(unittest.TestCase):
    def test_pairs_follow_entry_exit_order_and_reject_nesting(self):
        rows = [
            {"hook": "entry", "event": 0},
            {"hook": "middle", "event": 1},
            {"hook": "exit", "event": 2},
            {"hook": "exit", "event": 3},
            {"hook": "entry", "event": 4},
            {"hook": "exit", "event": 5},
        ]
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory)
            (capture / "instruction-trace.jsonl").write_text(
                "".join(json.dumps(row) + "\n" for row in rows)
            )
            result = pairs(capture, "entry", "exit")
            self.assertEqual([(a["event"], b["event"]) for a, b, _, _ in result], [(0, 2), (4, 5)])
            (capture / "instruction-trace.jsonl").write_text(
                "".join(json.dumps(row) + "\n" for row in rows[:1] * 2)
            )
            with self.assertRaisesRegex(ValueError, "Nested"):
                pairs(capture, "entry", "exit")

    def test_field_source_map_comes_from_the_image(self):
        ram = bytearray(RAM)
        ram[0x4F34C:0x4F350] = (0x4016).to_bytes(4, "little")
        self.assertEqual(image_map(bytes(ram)), 22)

    def test_interrupt_invocations_are_bracketed_per_call(self):
        rows = [
            {"hook": "entry", "event": 0},
            {"hook": "tick", "event": 1},
            {"hook": "tick-return", "event": 2},
            {"hook": "dispatch", "event": 3},
            {"hook": "dispatch-return", "event": 4},
            {"hook": "exit", "event": 5},
            {"hook": "tick", "event": 6},
            {"hook": "tick-return", "event": 7},
        ]
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory)
            (capture / "instruction-trace.jsonl").write_text(
                "".join(json.dumps(row) + "\n" for row in rows)
            )
            interrupts = (("tick", "tick-return"), ("dispatch", "dispatch-return"))
            [(entry, exit, handlers, _)] = pairs(capture, "entry", "exit", interrupts)
            self.assertEqual((entry["event"], exit["event"]), (0, 5))
            self.assertEqual([(a["event"], b["event"]) for a, b in handlers], [(1, 2), (3, 4)])
            (capture / "instruction-trace.jsonl").write_text(
                "".join(json.dumps(row) + "\n" for row in rows[:2] + rows[1:2])
            )
            with self.assertRaisesRegex(ValueError, "Nested"):
                pairs(capture, "entry", "exit", interrupts)

    def test_owned_bytes_and_unowned_writes_are_exact(self):
        entry = bytearray(RAM)
        exit = bytearray(RAM)
        exit[0x100] = 7  # Owned and computed correctly.
        exit[0x200] = 9  # Owned but computed wrongly.
        exit[0x300] = 1  # Changed by the original, not owned.
        sp = 0x80100000
        exit[(sp & 0x1FFFFF) - 4] = 3  # Transient callee stack below the entry SP.
        exit[(sp & 0x1FFFFF) - STACK_BELOW_ENTRY - 1] = 5  # Beyond the stack exclusion.
        exit[0x859C] = 4  # Kernel save area: excused only with observed interrupts.
        owned = [
            {"name": "a", "address": 0x80000100, "hex": "07"},
            {"name": "b", "address": 0x80000200, "hex": "08"},
        ]
        result = compare(bytes(entry), bytes(exit), owned, sp)
        self.assertEqual(result["owned_bytes"], 2)
        self.assertEqual(result["mismatch_count"], 1)
        self.assertEqual(result["mismatches"][0]["address"], "0x80000200")
        self.assertEqual(result["unowned_count"], 3)
        self.assertEqual(
            result["unowned_writes"],
            ["0x80000300", "0x8000859c", hex(0x80000000 + (sp & 0x1FFFFF) - STACK_BELOW_ENTRY - 1)],
        )

    def test_interrupt_attribution_excuses_only_interrupt_only_bytes(self):
        entry, exit = bytearray(RAM), bytearray(RAM)
        for offset in (0x400, 0x500, 0x859C, 0x600):
            exit[offset] = 1
        owned = [{"name": "a", "address": 0x80000600, "hex": "01"}]
        # 0x400 changed only in interrupt code; 0x500 in both call and interrupt.
        result = compare(bytes(entry), bytes(exit), owned, 0x80100000, {0x500}, {0x400, 0x500})
        self.assertEqual(result["unowned_writes"], ["0x80000500"])
        self.assertEqual(result["interrupt_conflicts"], ["0x80000500"])
        self.assertEqual(result["interrupt_attributed"], 2)  # 0x400 and the kernel byte
        # The BIOS save area changes before the dispatch hook and inside it.
        result = compare(bytes(entry), bytes(exit), owned, 0x80100000, {0x500, 0x859C}, {0x859C})
        self.assertEqual(result["interrupt_conflicts"], [])
        # Also when the save-area byte's net change is zero.
        result = compare(bytes(entry), bytes(exit), owned, 0x80100000, {0x500, 0x85A0}, {0x85A0})
        self.assertEqual(result["interrupt_conflicts"], [])
        self.assertEqual(result["unowned_writes"], ["0x80000400", "0x80000500"])
        # An owned byte changed only by interrupt code, left unchanged by the call.
        result = compare(
            bytes(entry),
            bytes(exit),
            [{"name": "queue", "address": 0x80000400, "hex": "00"}],
            0x80100000,
            set(),
            {0x400},
        )
        self.assertEqual((result["mismatch_count"], result["interrupt_conflicts"]), (0, []))
        # The same byte also changed by the C++ is a conflict and a mismatch.
        result = compare(
            bytes(entry),
            bytes(exit),
            [{"name": "queue", "address": 0x80000400, "hex": "02"}],
            0x80100000,
            set(),
            {0x400},
        )
        self.assertEqual(result["mismatch_count"], 1)
        self.assertEqual(result["interrupt_conflicts"], ["0x80000400"])
        owned.append({"name": "b", "address": 0x80000600, "hex": "01"})
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            compare(bytes(entry), bytes(exit), owned, 0x80100000)

    def test_interrupt_superseded_bytes_keep_the_call_value(self):
        entry, before, after, exit = (bytearray(RAM) for _ in range(4))
        # 0x700: the call writes 1, then interrupt code writes 2.
        before[0x700], after[0x700], exit[0x700] = 1, 2, 2
        # 0x710: interrupt code writes 2, then the call writes 3.
        after[0x710], exit[0x710] = 2, 3
        images = [bytes(image) for image in (entry, before, after, exit)]
        superseded = superseded_bytes(images)
        self.assertEqual(superseded, {0x700: (1, 0)})
        changed = ({0x700, 0x710}, {0x700, 0x710})
        owned = [{"name": "a", "address": 0x80000700, "hex": "01"}]
        result = compare(images[0], images[3], owned, 0x80100000, *changed, superseded)
        self.assertEqual(result["mismatch_count"], 0)
        self.assertEqual(result["interrupt_conflicts"], ["0x80000710"])
        self.assertEqual(
            result["interrupt_superseded"],
            [{"address": "0x80000700", "value": 1, "interrupt": 0, "exit": 2}],
        )
        # A different call value is still a mismatch and a conflict.
        owned = [{"name": "a", "address": 0x80000700, "hex": "03"}]
        result = compare(images[0], images[3], owned, 0x80100000, *changed, superseded)
        self.assertEqual(result["mismatch_count"], 1)
        self.assertIn("0x80000700", result["interrupt_conflicts"])

    def test_superseded_bytes_follow_segments_across_several_interrupts(self):
        # Images: entry, [before, after] x 3, exit.
        images = [bytearray(RAM) for _ in range(8)]

        def value(offset, *values):
            for image, byte in zip(images, values, strict=True):
                image[offset] = byte

        # 0x800: call 1, interrupt 0 keeps it, call 2 in segment 1, interrupt 1 writes 9.
        value(0x800, 0, 1, 1, 2, 9, 9, 9, 9)
        # 0x810: the call writes after the last interrupt changed it: not superseded.
        value(0x810, 0, 1, 5, 5, 5, 5, 5, 6)
        # 0x820: never changed by the call; interrupt 2 changes it.
        value(0x820, 0, 0, 0, 0, 0, 0, 4, 4)
        # 0x830: the call writes 1, interrupt 0 restores the entry value 0.
        value(0x830, 0, 1, 0, 0, 0, 0, 0, 0)
        superseded = superseded_bytes([bytes(image) for image in images])
        self.assertEqual(superseded, {0x800: (2, 1), 0x820: (0, 2), 0x830: (1, 0)})
        entry, exit = bytes(images[0]), bytes(images[-1])
        own = {0x800, 0x810, 0x830}
        interrupt = {0x800, 0x810, 0x820, 0x830}
        owned = [
            {"name": "a", "address": 0x80000800, "hex": "02"},
            {"name": "b", "address": 0x80000830, "hex": "00"},
        ]
        result = compare(entry, exit, owned, 0x80100000, own, interrupt, superseded)
        # 0x830 left at its entry value by the C++ is still a conflict, as is the
        # unowned 0x810, which the call and interrupts both changed.
        self.assertEqual(result["interrupt_conflicts"], ["0x80000810", "0x80000830"])
        self.assertEqual(
            [item["address"] for item in result["interrupt_superseded"]], ["0x80000800"]
        )
        with self.assertRaisesRegex(ValueError, "alternate"):
            superseded_bytes([bytes(image) for image in images[:3]])

    def test_pending_loads_commit_in_slot_order(self):
        row = {
            "gpr_u32": list(range(34)),
            "code": 0x27BDFFE8,  # addiu sp, sp, -0x18
            "load_delay": {"select": 1, "registers": [0, 4], "values": [0, 0x44]},
        }
        registers = visible_registers(row)
        self.assertEqual((registers[4], registers[5]), (0x44, 5))
        row["load_delay"] = {"select": 1, "registers": [5, 4], "values": [0x55, 0x44]}
        with self.assertRaisesRegex(ValueError, "More than one"):
            visible_registers(row)
        row["load_delay"] = {"select": 0, "registers": [4, 0], "values": [0x44, 0]}
        row["code"] = 0x00852021  # addu a0, a0, a1 names the pending A0
        with self.assertRaisesRegex(ValueError, "names a register"):
            visible_registers(row)
        row["load_delay"] = {"select": 0, "registers": [0, 0], "values": [0, 0]}
        self.assertEqual(visible_registers(row), list(range(34)))
        del row["load_delay"]
        with self.assertRaisesRegex(ValueError, "recapture"):
            visible_registers(row)

    def test_loop_arrivals_follow_the_reads_before_their_point(self):
        ram = bytearray(RAM)
        # lw v0, 0(v0) at each load site; DMA3 control register address.
        for site in (0x45DE4, 0x4BA34, 0x42A6C):
            ram[site : site + 4] = (0x8C420000).to_bytes(4, "little")
        ram[0x567B4:0x567B8] = (0x1F8010B8).to_bytes(4, "little")
        io = bytearray(0x1000)

        def row(hook, cycle, v0=0, sp=0x801FFFC8, pc=0):
            return {
                "hook": hook,
                "cycle_u32": cycle,
                "subcycle_u32": 0,
                "pc": pc,
                "code": 0,
                "gpr_u32": [0, 0, v0] + [0] * 26 + [sp, 0, 0, 0, 0, 0],
                "load_delay": {"select": 0, "registers": [0, 0], "values": [0, 0]},
            }

        image = [row("vsync0-return", 20)]
        loop = [
            row("frame-entry", 0),
            row("tick-entry", 30, v0=0x80060000, sp=0x85D8),
            row("tick-exit", 31),
            row("frame-exit", 40),
            row("dispatch-entry", 50, sp=0x800588BC),
            row("load-8004ba34", 51, v0=1, pc=0x8004BA38),
            row("dispatch-exit", 52),
            row("load-80042a6c", 55, pc=0x80042A70),
            row("vsync1-loop", 60, v0=0x55),
            row("load-80045de4", 70, pc=0x80045DE8),
            row("drain-call", 80),
        ]
        lines, counts, sectors, stacks = loop_inputs(
            image, loop, {50: b"\x00\x41"}, bytes(ram), bytes(io)
        )
        self.assertEqual(
            lines,
            [
                "tick 8007554c 80060000",
                "arrival 80077db4",
                "pad 0 0",
                "pad 1 41",
                "read 8004ba34 00000001",
                "read 80045de4 00000000",
            ],
        )
        self.assertEqual(sectors, [])
        self.assertEqual(stacks, {0x85D8, 0x800588BC})
        self.assertEqual(counts["datasync_reads_checked"], 1)
        io[0xB8 + 3] = 1  # DMA3 busy in the imported page, idle in the recording
        with self.assertRaisesRegex(ValueError, "DMA3 busy"):
            loop_inputs(image, loop, {50: b""}, bytes(ram), bytes(io))

    def test_music_positions_and_spu_transfer_reads(self):
        ram = bytearray(RAM)
        # lhu a0, 0x1aa(v1) at the SPU control read; lw v0, 0(v0) at the DMA read.
        ram[0x4CD8C:0x4CD90] = (0x946401AA).to_bytes(4, "little")
        ram[0x4BA34:0x4BA38] = (0x8C420000).to_bytes(4, "little")
        ram[0x567B4:0x567B8] = (0x1F8010B8).to_bytes(4, "little")

        def row(hook, cycle, v0=0, a0=0, pc=0):
            registers = [0, 0, v0, 0, a0] + [0] * 24 + [0x801FFFC8, 0, 0, 0, 0, 0]
            return {
                "hook": hook,
                "cycle_u32": cycle,
                "subcycle_u32": 0,
                "pc": pc,
                "code": 0,
                "gpr_u32": registers,
                "load_delay": {"select": 0, "registers": [0, 0], "values": [0, 0]},
            }

        loop = [
            row("frame-entry", 0),
            row("loop-return", 10),
            row("dispatch-entry", 15),
            row("dispatch-exit", 16),
            row("music-poll", 20),
            row("stream-step", 30),
            row("dispatch-entry", 40),
            row("load-8004ba34", 41, v0=8, pc=0x8004BA38),
            row("dispatch-exit", 42),
            row("load-8004cd8c", 50, a0=0xC000, pc=0x8004CD90),
            row("tick-entry", 60, v0=1),
            row("tick-exit", 61),
            row("loop-tail", 70),
        ]
        lines, counts, _, _ = loop_inputs([], loop, {15: b"", 40: b""}, bytes(ram), bytes(0x1000))
        # The arrival before the SPU read precedes it; the tick after it
        # keeps its stream-step point.
        self.assertEqual(
            lines,
            [
                "arrival 80078b88",
                "arrival 800854d0",
                "read 8004ba34 00000008",
                "read 8004cd8c 0000c000",
                "tick 800854d0 1",
            ],
        )
        self.assertEqual(counts["spu_transfer_reads"], 1)

    def test_per_call_datasync_reads_are_checked_not_supplied(self):
        ram = bytearray(RAM)
        ram[0x42A6C:0x42A70] = (0x8C420000).to_bytes(4, "little")
        ram[0x567B4:0x567B8] = (0x1F8010B8).to_bytes(4, "little")
        load = {
            "hook": "load-80042a6c",
            "pc": 0x80042A70,
            "code": 0,
            "gpr_u32": [0] * 34,
            "load_delay": {"select": 0, "registers": [0, 0], "values": [0, 0]},
        }
        self.assertEqual(platform_inputs([load], bytes(ram), bytes(0x1000), None), ("", []))
        io = bytearray(0x1000)
        io[0xB8 + 3] = 1
        with self.assertRaisesRegex(ValueError, "DMA3 busy"):
            platform_inputs([load], bytes(ram), bytes(io), None)

    def test_syscalls_excuse_the_bios_save_areas(self):
        entry, exit = bytearray(RAM), bytearray(RAM)
        exit[0xE0CC] = 1
        result = compare(bytes(entry), bytes(exit), [], 0x80100000)
        self.assertEqual(result["unowned_writes"], ["0x8000e0cc"])
        result = compare(bytes(entry), bytes(exit), [], 0x80100000, syscalls=True)
        self.assertEqual(result["unowned_writes"], [])


if __name__ == "__main__":
    unittest.main()


class FastComparisonTests(unittest.TestCase):
    def test_vram_read_backs_come_from_the_image_after_the_call(self) -> None:
        ram = bytearray(0x200000)
        ram[0xC3EA4:0xC3EA8] = (0x80110000).to_bytes(4, "little")
        for index in range(16):
            at = 0x110000 + 0x8970 + (index % 4) * 0x630 + (index // 4) * 0x18C
            ram[at : at + 0x18C] = bytes([index]) * 0x18C

        class Snapshots:
            def read(self, row):
                return bytes(ram), b"", b""

        entry = {"hook": "op-77990", "event": 1}
        lines = vram_read_lines({"hook": "op-camera", "event": 2}, Snapshots(), entry)
        self.assertEqual(len(lines), 16)
        # Call order: each row once, then the next copy.
        self.assertEqual(lines[1], "vram_read 63 " + "01" * 0x18C)
        self.assertEqual(lines[4], "vram_read 63 " + "04" * 0x18C)
        self.assertEqual(vram_read_lines({"hook": "op-camera", "event": 1}, Snapshots(), entry), [])

    def test_differing_positions_match_a_byte_scan(self) -> None:
        a = bytes(range(256)) * 16
        b = bytearray(a)
        for i in (0, 1, 255, 2048, 4095):
            b[i] ^= 0x5A
        self.assertEqual(differing_positions(a, bytes(b)), [0, 1, 255, 2048, 4095])
        self.assertEqual(differing_positions(a, a), [])

    def test_owned_bytes_lookups_and_differences(self) -> None:
        owned = [
            {"name": "late", "address": 0x80001000, "hex": "0102"},
            {"name": "early", "address": 0x80000010, "hex": "aabbcc"},
        ]
        spans = OwnedBytes(owned)
        self.assertEqual(len(spans), 5)
        self.assertTrue(0x11 in spans and 0x1001 in spans and 0x13 not in spans)
        self.assertEqual(spans.get(0x12), 0xCC)
        self.assertIsNone(spans.get(0x0F))
        self.assertEqual(spans.name(0x1001), ("late", 0x80001000, 1))
        image = bytearray(0x2000)
        image[0x10:0x13] = bytes.fromhex("aabbcc")
        image[0x1000] = 0x01
        self.assertEqual(list(spans.differing(bytes(image))), [(0x1001, 0x02)])
        with self.assertRaises(ValueError):
            OwnedBytes(owned + [{"name": "overlap", "address": 0x80000012, "hex": "00"}])

    def test_batch_runner_serves_calls_and_recovers_from_failures(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            runner = Path(directory) / "runner"
            runner.write_text(
                "#!/usr/bin/env python3\n"
                "import json, sys, time\n"
                "assert sys.argv[1:] == ['--batch']\n"
                "for line in sys.stdin:\n"
                "    fields = line.rstrip('\\n').split('\\t')\n"
                "    if fields[0] == 'crash':\n"
                "        sys.exit(3)\n"
                "    if fields[0] == 'hang':\n"
                "        time.sleep(30)\n"
                "    report = {'status': 'completed_boundary', 'fields': fields}\n"
                "    print(json.dumps(report), flush=True)\n"
            )
            runner.chmod(runner.stat().st_mode | stat.S_IXUSR)
            with BatchRunner(runner) as batch:
                self.assertEqual(batch.call(["a", "", "c"], 10)["fields"], ["a", "", "c"])
                self.assertEqual(batch.call(["b"], 10)["fields"], ["b"])
                crashed = batch.call(["crash"], 10)
                self.assertEqual(crashed["status"], "runner_failure")
                self.assertIn("runner exit 3", crashed["reason"])
                self.assertEqual(batch.call(["again"], 10)["fields"], ["again"])
                timed_out = batch.call(["hang"], 0.5)
                self.assertEqual(timed_out["status"], "runner_failure")
                self.assertEqual(batch.call(["after"], 10)["fields"], ["after"])
                with self.assertRaises(ValueError):
                    batch.call(["tab\there"], 10)
