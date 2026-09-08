"""Entirely invented observations test original-call provenance and effect checking."""

import copy
import unittest

from tests.sprite_fixtures import ADDRESS, invented_sprite
from tools.analysis.animation_trace import calls, compare_effect, list_memory
from tools.analysis.sprite_state import put
from tools.reference.instruction_trace import guarded_record


def observation_pair():
    ram = bytearray(0x200000)
    sprite, _, _ = invented_sprite()
    ram[ADDRESS - 0x80000000 : ADDRESS - 0x80000000 + len(sprite)] = sprite
    put(ram, 0x4F34C, 23)
    put(ram, 0xAFB10, 0x80000200)
    common = [
        {"name": "field", "offset": 0x4F34C, "size": 4},
        {"name": "descriptor-table", "offset": 0xAFB10, "size": 4},
        {"name": "player-descriptor", "offset": 0x314, "size": 92},
    ]
    hooks, rows = [], []
    for index, (boundary, pc, register) in enumerate(
        (("before", 0x800223B0, 4), ("after", 0x80022648, 17))
    ):
        gpr = [0] * 34
        gpr[4], gpr[17], gpr[28], gpr[29], gpr[31] = (
            ADDRESS,
            ADDRESS,
            0x80059170,
            0x80004000 - 32 * index,
            0x80073DB4,
        )
        ranges = common + [
            {"name": "sprite", "register": register, "relative_offset": 0, "size": 512}
        ]
        if index:
            put(ram, gpr[29] - 0x80000000 + 24, gpr[31])
            ranges += [{"name": "frame", "register": 29, "relative_offset": 0, "size": 32}]
        hook = {
            "name": "orientation-" + boundary,
            "pc": pc,
            "guard": {"offset": pc - 0x80000000, "expected": "00000000"},
            "ranges": ranges,
        }
        row = guarded_record(hook, pc, 0, gpr, bytes(ram))
        row.update(event=index, frontend_run=4300, hook=hook["name"], pc=pc, code=0)
        hooks.append(hook)
        rows.append(row)
    return rows, {"hooks": hooks, "start_frame": 4300, "end_frame": 5241}


class AnimationTraceTests(unittest.TestCase):
    def test_authored_saved_frame_and_object_lineage_are_accepted(self):
        rows, spec = observation_pair()
        result = list(calls(rows, spec, "replay"))
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].kind, "orientation")
        compare_effect(result[0], [])

    def test_changed_gp_and_nonconsecutive_event_are_rejected(self):
        for field, value, message in (("gp", 0, "GP context"), ("event", 4, "event ordering")):
            rows, spec = observation_pair()
            if field == "gp":
                rows[0]["gpr_u32"][28] = value
            else:
                rows[0]["event"] = value
            with self.assertRaisesRegex(ValueError, message):
                list(calls(rows, spec, "replay"))

    def test_saved_return_address_and_frame_offset_are_verified(self):
        rows, spec = observation_pair()
        frame = rows[1]["ranges"][-1]
        data = bytearray.fromhex(frame["hex"])
        data[24] ^= 1
        frame["hex"] = data.hex()
        with self.assertRaisesRegex(ValueError, "saved return"):
            list(calls(rows, spec, "replay"))
        rows, spec = observation_pair()
        rows[1]["gpr_u32"][29] += 4
        with self.assertRaisesRegex(ValueError, "register range"):
            list(calls(rows, spec, "replay"))

    def test_unexpected_parent_and_incomplete_call_are_rejected(self):
        rows, spec = observation_pair()
        rows[0]["hook"] = spec["hooks"][0]["name"] = "clear-before"
        rows[0]["gpr_u32"][31] = 0x8001D320
        with self.assertRaisesRegex(ValueError, "nesting"):
            list(calls(rows, spec, "replay"))
        rows, spec = observation_pair()
        with self.assertRaisesRegex(ValueError, "incomplete"):
            list(calls(rows[:1], spec, "replay"))

    def test_changed_output_byte_cannot_be_hidden_as_an_opaque_effect(self):
        rows, spec = observation_pair()
        result = list(calls(rows, spec, "replay"))[0]
        changed = bytearray(result.new["sprite"])
        changed[0x34] = 9
        result.new["sprite"] = bytes(changed)
        with self.assertRaisesRegex(ValueError, "sprite differs"):
            compare_effect(result, [])
        compare_effect(result, [(ADDRESS + 0x34, b"\x09")])

    def test_readonly_list_node_inputs_must_be_consistent_across_traversal(self):
        rows, spec = observation_pair()
        result = list(calls(rows, spec, "replay"))[0]
        packet = {
            "hook": "frame-list-step",
            "ranges": [{"name": "list-sprite", "resolved_offset": 0x7000}],
        }
        data = bytearray(512)
        put(data, 0x20, 0x800070B4)
        result.children = [(packet, {"list-sprite": bytes(data)})]
        self.assertEqual(list_memory(result, 0x80007020, 4), b"\xb4\x70\x00\x80")
        other = copy.deepcopy(result.children[0])
        changed = bytearray(other[1]["list-sprite"])
        changed[0x20] ^= 1
        other[1]["list-sprite"] = bytes(changed)
        result.children.append(other)
        with self.assertRaisesRegex(ValueError, "changing original frame list"):
            list_memory(result, 0x80007020, 4)


if __name__ == "__main__":
    unittest.main()
