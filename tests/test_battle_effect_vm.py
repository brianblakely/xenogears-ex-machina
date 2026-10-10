"""Invented effect scripts exercise the effect VM's lengths, flow and containers;
the tables are checked against the recovered interpreters in decomp/src."""

import re
import struct
import unittest
from pathlib import Path

from tools.analysis.battle_effect_vm import (
    BATTLE,
    BATTLE_EVENTS,
    MODEL_VIEWER,
    MODEL_VIEWER_EVENTS,
    EffectError,
    ScriptTable,
    Sweep,
    animation_entries,
    animation_events,
    decode,
    decode_events,
    disassemble,
    file_tables,
    script_file,
    sweep_table,
    unreached,
)

ROOT = Path(__file__).resolve().parents[1]


def words(*values):
    return struct.pack(f"<{len(values)}H", *(v & 0xFFFF for v in values))


def command(op, arg=0, *params):
    return words(op | arg << 8, *params)


class DecodeTests(unittest.TestCase):
    def test_parameter_words_follow_the_command(self):
        insn = decode(command(0x1A, 1, 1, 2, 3, 4, 5, 6), 0)
        self.assertEqual((insn.opcode, insn.argument, insn.length), (0x1A, 1, 14))
        self.assertEqual(insn.operands, (1, 2, 3, 4, 5, 6))
        self.assertEqual(insn.successors, (14,))

    def test_jumps_count_bytes_from_the_command_start(self):
        code = command(0x0C) + command(0x32, 0, -2)
        self.assertEqual(decode(code, 2).successors, (0,))
        branch = decode(command(0x35, 0, 8), 0)
        self.assertEqual(branch.successors, (4, 8))

    def test_end_and_returns_stop_the_path(self):
        self.assertEqual(decode(command(0x00), 0).successors, ())
        self.assertEqual(decode(command(0x17), 0).successors, ())
        self.assertEqual(decode(command(0x14, 0xFD, 0x0100), 0).successors, ())
        self.assertEqual(decode(command(0x14, 0xFE, 0x0100), 0).successors, (4,))

    def test_deferred_jumps_need_a_nonzero_argument(self):
        self.assertEqual(decode(command(0x36, 1, 30, 12), 0).successors, (6, 12))
        self.assertEqual(decode(command(0x36, 0, 30, 12), 0).successors, (6,))

    def test_loop_returns_past_its_counter_command(self):
        code = command(0x30, 3, 0) + command(0x0C) + command(0x31, 0, -6) + command(0x00)
        listing = disassemble(code, [0])
        self.assertEqual(listing.errors, [])
        self.assertEqual(listing.instructions[6].successors, (10, 4))
        self.assertEqual(sorted(listing.instructions), [0, 4, 6, 10])

    def test_object_events_follow_the_command(self):
        show = words(3) + bytes([7, 0, 2, 1])  # frame 3: draw part 2
        code = command(0x63, 1, 10) + show + command(0x00)
        listing = disassemble(code, [0])
        self.assertEqual(listing.errors, [])
        insn = listing.instructions[0]
        self.assertEqual(insn.data, (4, 10))
        self.assertEqual(
            [(e.offset, e.time, e.type, e.name, e.length) for e in insn.events],
            [(4, 3, 7, "show_part", 6)],
        )
        self.assertEqual(sorted(listing.instructions), [0, 10])

    def test_event_lengths_follow_type_and_on_byte(self):
        light_off = words(0) + bytes([2, 1, 0, 0])
        light_on = words(1) + bytes([2, 1, 1, 0]) + bytes(12)
        menu = words(2) + bytes([6, 0])
        sound = words(2) + bytes([5, 0x39, 0, 1, 0, 0])
        block = light_off + light_on + menu + sound
        events = decode_events(block, 0, 4, len(block))
        self.assertEqual([e.length for e in events], [6, 0x12, 4, 8])
        self.assertEqual([e.name for e in events], ["light", "light", "menu_update", "sound"])
        self.assertEqual(decode_events(block, 0, 1, len(block), "ovl2143")[0].name, "anchor")
        self.assertEqual(decode_events(block, 0, 0, 0), ())

    def test_bad_events_are_reported(self):
        with self.assertRaises(EffectError):  # no case for type 0
            decode_events(words(0) + bytes(4), 0, 1, 6)
        with self.assertRaises(EffectError):  # an image event longer than its block
            decode_events(words(0) + bytes([9, 0, 1, 0]), 0, 1, 6)
        listing = disassemble(command(0x63, 2, 10) + words(0) + bytes([7, 0, 1, 1]), [0])
        self.assertEqual(listing.errors, ["animation event at 0xa runs past its block"])
        self.assertEqual(set(BATTLE_EVENTS), set(MODEL_VIEWER_EVENTS))

    def test_unknown_opcodes_and_overlaps_are_reported(self):
        self.assertNotIn(0x76, BATTLE)
        with self.assertRaises(EffectError):
            decode(command(0x76), 0)
        reached = disassemble(command(0x0C) + command(0x80), [0])
        self.assertEqual(reached.errors, ["unknown battle opcode 0x80 at 0x2"])
        skipped = command(0x32, 0, 6) + command(0x76) + command(0x00)
        self.assertEqual(disassemble(skipped, [0]).errors, [])
        clash = command(0x35, 0, 6) + command(0x01, 0, 0x32) + command(0x00)
        (error,) = disassemble(clash, [0]).errors  # a jump into a parameter word
        self.assertIn("0x4", error)
        self.assertIn("0x6", error)
        self.assertTrue(disassemble(command(0x01, 0, 1), [0]).errors)  # runs off the end

    def test_dialects_differ_in_parameter_words(self):
        self.assertEqual(decode(command(0x61, 0, 4), 0).length, 4)
        self.assertEqual(decode(command(0x61, 0, 4), 0, "ovl2143").length, 2)
        self.assertEqual(decode(command(0x33, 0, 4), 0, "ovl2143").successors, (4,))
        self.assertEqual(len(MODEL_VIEWER), 0x71)
        with self.assertRaises(EffectError):
            decode(command(0x71, 0, 1), 0, "ovl2143")


class ContainerTests(unittest.TestCase):
    def script_file_bytes(self):
        code = command(0x0C) + command(0x00)
        table = struct.pack("<IIII", 3, 0x10, 0x10, 0)  # animations, a script, an empty id
        return struct.pack("<III", 2, 0x0C, 0) + table + code

    def test_object_script_file(self):
        data = self.script_file_bytes()
        self.assertEqual(script_file(data), ScriptTable([0x1C, 0], 0x1C, 0))
        self.assertEqual(sorted(disassemble(data, [0x1C]).instructions), [0x1C, 0x1E])

    def test_enemy_set_model_entries(self):
        inner = self.script_file_bytes()
        header = bytes([2, 0, 0, 0]) + struct.pack("<I", 32 + len(inner))
        entries = struct.pack("<IIB3x", 32, 8, 1) + struct.pack("<IIB3x", 0, 3, 1)
        data = header + entries + inner
        tables = list(file_tables("enemy", data))
        self.assertEqual(
            [(label, table.scripts) for label, table in tables], [(".0", [32 + 0x1C, 0])]
        )

    def test_scene_motion_block_has_no_animations(self):
        data = bytearray(4 + 0x518)
        struct.pack_into("<i", data, 4 + 0x514, 0x518)
        data += struct.pack("<III", 2, 8, 12) + command(0x00)
        self.assertEqual(
            list(file_tables("scene", bytes(data))), [("", ScriptTable([4 + 0x518 + 12]))]
        )

    def test_animations_run_their_event_lists(self):
        # header, ObjectScripts (animations, one script), animation table (an
        # animation, two unused ids at the script, one at the data block), the
        # animation, the script, the data block
        header = struct.pack("<III", 2, 0x0C, 0x50)
        scripts = struct.pack("<III", 2, 0x0C, 0x42)
        table = struct.pack("<IIIII", 4, 0x14, 0x36, 0x36, 0x38)
        animation = bytearray(0x18)
        struct.pack_into("<HI", animation, 0x12, 2, 0x18)
        events = words(0) + bytes([7, 0, 1, 1]) + words(1) + bytes([6, 0])
        data = header + scripts + table + animation + events + command(0x00) + bytes(4)
        parsed = script_file(data)
        self.assertEqual(parsed, ScriptTable([0x4E], 0x18, 0x50))
        self.assertEqual(
            animation_entries(data, parsed), [(0x2C, True), (0x4E, False), (0x50, False)]
        )
        self.assertEqual(
            [(e.offset, e.time, e.name) for e in animation_events(data, 0x2C)],
            [(0x44, 0, "show_part"), (0x4A, 1, "menu_update")],
        )
        result = Sweep()
        self.assertEqual(sweep_table(result, "extra", "battle", data, parsed), [])
        self.assertEqual((result.animations, result.not_animations), (1, 2))
        self.assertEqual(result.events, {("animation", 7): 1, ("animation", 6): 1})
        self.assertEqual(result.instructions, {"extra": 1})

    def test_words_nothing_starts_are_reported_apart(self):
        # a started script, its 7777 separator, a script no table names, a
        # separator, an unknown word, then the data block
        code = (
            command(0x0C)
            + command(0x00)
            + b"\x77\x77"
            + command(0x0C)
            + command(0x00)
            + b"\x77\x77"
            + command(0x80)
            + bytes(2)
        )
        data = struct.pack("<IIIIII", 2, 0x0C, 0x2C, 2, 0x0C, 0x10) + bytes(4) + code + bytes(4)
        table = script_file(data)
        self.assertEqual(table, ScriptTable([0x1C], 0x18, 0x2C))
        found = unreached(data, table, disassemble(data, table.scripts), "battle")
        self.assertEqual((found.separators, found.scripts, found.instructions), (2, 1, 2))
        self.assertEqual(found.errors, ["0x28: unknown battle opcode 0x80 at 0x28"])


def case_bodies(path: str, head: str, label: str) -> dict:
    """The case bodies of the switch in the function starting with head, by the
    label pattern's group; a label without statements takes the next body."""
    text = (ROOT / path).read_text()
    # the definition, not a declaration of it ahead of its first use
    body = text[re.search(re.escape(head) + r"[^;{]*\)\s*\{", text).start() :]
    body = re.sub(r"/\*.*?\*/", "", body[: body.index("\n}\n")], flags=re.S)
    parts = re.split(label, body)
    cases, pending = {}, []
    for name, code in zip(parts[1::2], parts[2::2], strict=True):
        pending.append(name)
        if code.strip():
            cases.update(dict.fromkeys(pending, code))
            pending = []
    return cases


CASE = r"\n {8}(?:case 0x([0-9A-F]{2})|default):"


class SourceTests(unittest.TestCase):
    """The tables follow the recovered interpreters in decomp/src."""

    def check_parameter_words(self, path, head, table):
        cases = case_bodies(path, head, CASE)
        cases.pop(None)
        self.assertEqual(sorted(int(code, 16) for code in cases), sorted(table))
        for code, body in cases.items():
            spec = table[int(code, 16)]
            self.assertEqual(spec.handler.split()[-1], code.lower())
            self.assertEqual(len(re.findall(r"\*pc\b", body)), len(spec.operands), code)

    def test_battle_commands_read_their_parameter_words(self):
        self.check_parameter_words(
            "decomp/src/battle/battle_scene.c", "void battle_run_effect_script(", BATTLE
        )

    def test_model_viewer_commands_read_their_parameter_words(self):
        self.check_parameter_words(
            "decomp/src/ovl2143/ovl2143.c", "void func_801E39F0(", MODEL_VIEWER
        )

    def test_event_records_follow_the_runners(self):
        viewer = case_bodies(
            "decomp/src/ovl2143/ovl2143.c", "void func_801E5D44(", r"\n {8}case (\d):"
        )
        self.assertEqual(sorted(map(int, viewer)), sorted(MODEL_VIEWER_EVENTS))
        for kind, body in viewer.items():
            spec = MODEL_VIEWER_EVENTS[int(kind)]
            steps = {int(n, 0) for n in re.findall(r"anim_pos \+= (0x[0-9A-F]+|\d+);", body)}
            self.assertEqual(steps, {spec.length, spec.short} - {None}, kind)
        records = {1: "SpriteCommand", 2: "LightEvent", 3: "ChannelEvent", 4: "ChannelEvent"}
        records.update({5: "SoundEvent", 8: "SlotEvent", 9: "ImageEvent"})
        battle = case_bodies(
            "decomp/src/battle/battle_scene.c", "void battle_run_animation_events(", r"\n {16}case (\d):"
        )
        self.assertEqual(sorted(map(int, battle)), sorted(BATTLE_EVENTS))
        for kind, body in battle.items():
            spec = BATTLE_EVENTS[int(kind)]
            steps = re.findall(r"animationStart \+= (?:sizeof\((\w+)\)|(\d+));", body)
            names = {name for name, _ in steps if name}
            numbers = {int(n) for _, n in steps if n}
            self.assertEqual(names, {records[int(kind)]} if int(kind) in records else set(), kind)
            expected = {spec.short} if spec.short else set()
            if int(kind) not in records:
                expected = {spec.length}
            self.assertEqual(numbers, expected, kind)
            viewer_spec = MODEL_VIEWER_EVENTS[int(kind)]
            self.assertEqual((spec.length, spec.short), (viewer_spec.length, viewer_spec.short))


if __name__ == "__main__":
    unittest.main()
