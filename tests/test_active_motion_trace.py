"""Invented call records exercise proof lineage and source-state corruption checks."""

import copy
import struct
import unittest
from collections import Counter
from types import SimpleNamespace

from tests.sprite_fixtures import ADDRESS, forbidden_read
from tests.test_active_motion import invented_motion
from tools.analysis.active_motion_spec import SEGMENTS, specification
from tools.analysis.active_motion_trace import compare_motion
from tools.analysis.sprite_state import put
from tools.reference.instruction_trace import guarded_record


def inhibited_pair():
    actor, descriptor, sprite, _, _, tables = invented_motion()
    ram = bytearray(0x200000)
    put(actor, 0, 0x1000000)
    put(descriptor, 0x4C, 0x80002000)
    put(ram, 0x4F34C, 23)
    put(ram, 0xAFB10, 0x80000200)
    ram[0x314:0x370] = descriptor
    companion = bytearray(descriptor)
    put(companion, 4, 0x80003000)
    put(companion, 0x4C, 0x80002400)
    ram[0x3CC:0x428] = companion
    ram[0x2000:0x2138] = actor
    ram[0x2400:0x2538] = actor
    ram[0x1000:0x1200] = sprite
    ram[0x3000:0x3200] = sprite
    source = {
        "map": 23,
        "profile": {"id": "invented-source"},
        "exe": bytes(0x4A000),
        "overlay": bytes(0x40000),
        "component": b"",
    }
    spec = specification(source, ram, "control")
    hooks = {h["name"]: h for h in spec["hooks"]}
    original = [0] * 34
    original[4:7] = [3, 0x80000314, 0x80002000]
    original[16:23] = [100 + i for i in range(7)]
    original[29], original[31] = 0x80004000, 0x80081288
    rows = []
    for event, name in enumerate(("motion-before", "motion-after")):
        gpr = list(original)
        if event:
            gpr[16], gpr[19], gpr[21], gpr[22] = original[6], ADDRESS, original[4], original[5]
            gpr[29] -= 0x58
            struct.pack_into(
                "<8I",
                ram,
                gpr[29] - 0x80000000 + 0x38,
                *(original[i] for i in (16, 17, 18, 19, 20, 21, 22, 31)),
            )
            put(ram, 0x65B08, 3)
        hook = hooks[name]
        row = guarded_record(hook, hook["pc"], 0, gpr, bytes(ram))
        row.update(event=event, frontend_run=4301, hook=name, pc=hook["pc"], code=0)
        rows.append(row)
    resources = SimpleNamespace(read=forbidden_read, reads=Counter())
    return rows, spec, source, resources, tables


def change_byte(row, pointer):
    changed = 0
    for item in row["ranges"]:
        start = item["resolved_offset"] + 0x80000000
        if start <= pointer < start + item["size"]:
            data = bytearray.fromhex(item["hex"])
            data[pointer - start] ^= 1
            item["hex"] = data.hex()
            changed += 1
    if not changed:
        raise AssertionError("Uncaptured invented mutation")


class ActiveMotionTraceTests(unittest.TestCase):
    def test_inhibited_original_call_preserves_all_input_storage_and_saved_registers(self):
        result = compare_motion(*inhibited_pair())
        self.assertEqual(result["counts"], {"motion-after": 1, "inhibited": 1})
        self.assertEqual(result["excluded_actor_calls"], {})

    def test_changed_actor_sprite_local_and_global_outputs_are_rejected_even_with_aliases(self):
        for pointer in (0x80002004, ADDRESS + 0x1C, 0x80003FB8, 0x80065B08, 0x80003010, 0x80003FFC):
            with self.subTest(pointer=pointer):
                args = list(inhibited_pair())
                args[0] = copy.deepcopy(args[0])
                change_byte(args[0][1], pointer)
                with self.assertRaisesRegex(ValueError, "differs"):
                    compare_motion(*args)

    def test_changed_caller_event_order_and_return_run_are_rejected(self):
        for which in ("caller", "event", "run", "pc", "code"):
            args = list(inhibited_pair())
            rows = args[0]
            if which == "caller":
                rows[0]["gpr_u32"][31] ^= 4
            elif which == "event":
                rows[1]["event"] = 7
            elif which == "run":
                rows[1]["frontend_run"] += 1
            elif which == "pc":
                rows[1]["pc"] += 4
            else:
                rows[1]["code"] ^= 1
            with self.subTest(which=which), self.assertRaises(ValueError):
                compare_motion(*args)

    def test_missing_return_and_changed_saved_argument_registers_are_rejected(self):
        args = list(inhibited_pair())
        args[0] = args[0][:1]
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            compare_motion(*args)
        args = list(inhibited_pair())
        args[0][1]["gpr_u32"][21] = 5
        with self.assertRaisesRegex(ValueError, "saved argument"):
            compare_motion(*args)

    def test_source_specification_windows_are_contiguous_and_payload_bounded(self):
        _, spec, source, _, _ = inhibited_pair()
        ram = bytearray(0x200000)
        put(ram, 0xAFB10, 0x80000200)
        for name in SEGMENTS:
            candidate = specification(source, ram, name)
            self.assertEqual(len(candidate["hooks"]), 16)
            self.assertEqual(candidate["hooks"], spec["hooks"])
        windows = [SEGMENTS[name][:2] for name in ("traversal-a", "traversal-b", "traversal-c")]
        self.assertEqual(windows, [(4300, 4900), (4900, 5500), (5500, 6200)])
        with self.assertRaisesRegex(ValueError, "Unqualified"):
            specification(source, ram, "unknown")
        with self.assertRaisesRegex(ValueError, "outside RAM"):
            specification(source, bytes(0x200000), "control")


if __name__ == "__main__":
    unittest.main()
