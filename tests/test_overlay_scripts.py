"""Synthetic scripts exercise the recovered overlay script decoders."""

import re
import struct
import unittest

from tools.analysis.overlay_scripts import (
    ARENA,
    BASE,
    DIRECTORS,
    ROOT,
    WORLDMAP,
    ScriptError,
    UnknownOpcode,
    decode,
    disassemble,
    expected_sha256,
    run,
    scene_sweep,
    sequence_tables,
    sweep,
)


def halfwords(*values: int) -> bytes:
    return struct.pack(f"<{len(values)}h", *values)


def image(size: int, **placed: bytes) -> bytearray:
    data = bytearray(size)
    for name, raw in placed.items():
        offset = int(name[2:], 16) - BASE
        data[offset : offset + len(raw)] = raw
    return data


def handler_table() -> bytes:
    return b"".join(
        int(spec.handler[5:], 16).to_bytes(4, "little") for spec in WORLDMAP.opcodes.values()
    )


class WorldmapScriptTests(unittest.TestCase):
    def test_lengths_signed_operands_and_stop(self):
        data = halfwords(1, 10, 2, 0, 13, 0, 3, 4985, -288, 19242, 9, 40, 0, 0)
        listing = disassemble(WORLDMAP, data, BASE)
        self.assertEqual([i.opcode for i in listing], [1, 2, 3, 9, 0])
        self.assertEqual([i.size for i in listing], [4, 8, 8, 4, 4])
        self.assertEqual([i.address - BASE for i in listing], [0, 4, 12, 20, 24])
        self.assertEqual(listing[2].operands, (4985, -288, 19242))
        self.assertEqual(listing[1].text(), "send_actor actor=0, argument=13")
        self.assertEqual(listing[-1].spec.flow, "stop")

    def test_unused_halfwords_are_read_and_shown_when_set(self):
        instruction = decode(WORLDMAP, halfwords(11, 2, 8, 7), BASE)
        self.assertEqual(instruction.text(), "set_fade rate=2, step=8, unused=7")

    def test_opcode_is_the_unsigned_low_halfword_of_the_word(self):
        with self.assertRaises(UnknownOpcode) as caught:
            decode(WORLDMAP, halfwords(-1, 0), BASE)
        self.assertEqual(caught.exception.opcode, 0xFFFF)

    def test_unknown_opcode_misalignment_and_truncation_fail(self):
        with self.assertRaises(UnknownOpcode) as caught:
            disassemble(WORLDMAP, halfwords(1, 5, 12, 0), BASE)
        self.assertEqual((caught.exception.address, caught.exception.opcode), (BASE + 4, 12))
        with self.assertRaises(ScriptError):
            disassemble(WORLDMAP, halfwords(3, 1, 2), BASE)
        with self.assertRaises(ScriptError):
            decode(WORLDMAP, halfwords(0, 0, 0, 0), BASE + 2)

    def test_sweep_checks_the_dispatch_table_and_reads_both_scripts(self):
        first = halfwords(1, 60, 5, 44, 8, 0, 240, 0, 0, 0)
        second = halfwords(12, 0)
        data = image(0x2C0C6, D_8009A3C0=handler_table(), D_8009A758=first, D_8009AC60=second)
        result = sweep(WORLDMAP, bytes(data))
        self.assertEqual((result.scripts, result.instructions), (2, 4))
        self.assertEqual(result.uses, {1: 1, 5: 1, 8: 1, 0: 1})
        self.assertEqual(result.failures[0][:2], ("func_800838E8", 0x8009AC60))
        data[0x8009A3C0 - BASE] ^= 4
        with self.assertRaises(ScriptError):
            sweep(WORLDMAP, bytes(data))


class ArenaScriptTests(unittest.TestCase):
    def test_one_to_three_byte_commands_until_end(self):
        data = bytes([0x02, 0x12, 0x2B, 0x15, 0x07, 0x40, 0x1A, 0x01, 0xFF, 0x00, 0x99])
        listing = disassemble(ARENA, data, BASE)
        self.assertEqual([i.size for i in listing], [1, 2, 3, 1, 2, 1])
        self.assertEqual(listing[2].text(), "show_marker x/2=7, y=64")
        self.assertEqual(listing[4].operands, (255,))
        self.assertEqual(listing[-1].spec.mnemonic, "end")

    def test_values_without_a_case_fail_and_spinning_cases_end_the_listing(self):
        with self.assertRaises(UnknownOpcode) as caught:
            disassemble(ARENA, bytes([0x02, 0x23]), BASE)
        self.assertEqual((caught.exception.address, caught.exception.opcode), (BASE + 1, 0x23))
        for code in (16, 17):
            self.assertEqual(disassemble(ARENA, bytes([code, 0x12]), BASE)[-1].spec.flow, "hang")
        with self.assertRaises(ScriptError):
            disassemble(ARENA, bytes([0x15, 0x07]), BASE)

    def test_sweep_follows_the_scene_table_and_direct_starts(self):
        scene = bytes([0x03, 0x0E, 0x2D, 0x19, 0x00])
        table = struct.pack("<10I", *([0x80090F40] * 9 + [0x80090F48]))
        data = image(0x22E69, D_8009105C=table, D_80090F40=scene, D_80090F48=b"\x24")
        result = sweep(ARENA, bytes(data))
        self.assertEqual((result.scripts, result.instructions), (4, 6))
        self.assertEqual(result.uses, {0: 3, 3: 1, 14: 1, 25: 1})
        self.assertEqual(result.failures[0][:2], ("D_8009105C[9]", 0x80090F48))


def director(name: str):
    return next(d for d in DIRECTORS if d.interpreter == name)


def u16s(*values: int) -> bytes:
    return struct.pack(f"<{len(values)}H", *values)


class SceneDirectorTests(unittest.TestCase):
    def test_a_starter_that_does_not_step_fetches_entry_0_twice(self):
        flight = director("func_80080370")
        outcome = run(flight, [1, 2, 0x40], [3, 5, 0])
        self.assertEqual(
            [(s.index, s.state, s.wait, s.frame) for s in outcome.steps],
            [(0, 1, 3, 1), (0, 1, 3, 5), (1, 2, 5, 9), (2, 0x40, 0, 16)],
        )
        self.assertEqual((outcome.stop, outcome.frame, outcome.next), ("exit", 16, 3))

    def test_a_stepping_starter_runs_entry_0_on_the_first_update(self):
        vehicle = director("func_8007C3B8")
        outcome = run(vehicle, [2, 0x40], [10, 0])
        self.assertEqual([(s.index, s.frame) for s in outcome.steps], [(0, 1), (1, 13)])
        self.assertEqual((outcome.stop, outcome.frame, outcome.next), ("exit", 13, 2))
        self.assertEqual(vehicle.cues[0x40].state, 1)  # a further update would fetch entry 2

    def test_states_without_a_case_idle_and_fetches_past_the_table_stop(self):
        vehicle = director("func_8007C3B8")
        self.assertEqual(run(vehicle, [1, 5], [0, 0]).stop, "no case")
        self.assertEqual(run(vehicle, [1, 0], [0, 0]).stop, "idle")
        outcome = run(director("func_8007A9F8"), [1], [0])
        self.assertEqual((outcome.stop, outcome.frame, outcome.next), ("past table", 2, 1))

    def test_waits_are_signed_like_the_actor_field(self):
        outcome = run(director("func_800811C0"), [1, 0x40], [0xFFFF, 0])
        self.assertEqual([(s.index, s.frame) for s in outcome.steps], [(0, 1), (1, 2)])

    def test_cues_name_their_actions(self):
        cues = director("func_8007C3B8").cues
        self.assertEqual(cues[0x11].mnemonic, "requests+fade_out+sounds")
        self.assertEqual(cues[0x40].flow, "exit")
        self.assertEqual(cues[1].flow, "wait")
        self.assertEqual(cues[0].flow, "idle")
        self.assertEqual(cues[2].text(), "slot 2 request 1, sound 0xd, sound 0xe, sound 0xf")
        slots = director("func_8007C3B8").slots
        self.assertEqual(
            cues[0x16].text(slots),
            "slot 0 (func_800925A0) request 13, fade_rate 0x2, fade_step 0x4",
        )
        self.assertEqual(director("func_80080370").cues[0x40].text(), "exit_worldmap, state 0")

    def test_the_sweep_reads_every_sequence_and_checks_the_picker(self):
        placed = {}
        for each in DIRECTORS:
            for sequence in each.sequences:
                states = [1] * (sequence.entries - 1) + [0x40 if 0x40 in each.cues else 11]
                placed[f"D_{sequence.states:08X}"] = u16s(*states)
                placed[f"D_{sequence.durations:08X}"] = u16s(*[0] * sequence.entries)
        scene = director("func_8007DE98")
        pointers = [a for s in scene.sequences for a in (s.states, s.durations)]
        placed["D_8009A65C"] = struct.pack("<6I", *pointers)
        data = image(0x2AC70, **placed)
        result = scene_sweep(bytes(data))
        self.assertEqual((result.sequences, len(result.failures)), (7, 0))
        self.assertEqual(
            result.instructions, sum(s.entries for d in DIRECTORS for s in d.sequences)
        )
        data[0x8009A65C - BASE] ^= 4
        with self.assertRaises(ScriptError):
            sequence_tables(scene, bytes(data))


class SourceTests(unittest.TestCase):
    """The opcode tables follow the recovered interpreters in decomp/src."""

    def test_worldmap_entries_follow_d_8009a3c0_and_handler_returns(self):
        text = (ROOT / "decomp/src/worldmap/worldmap_80072238.c").read_text()
        table = re.search(r"ScriptOp D_8009A3C0\[12\] = \{(.*?)\};", text, re.S).group(1)
        handlers = [spec.handler for spec in WORLDMAP.opcodes.values()]
        self.assertEqual(re.findall(r"func_[0-9A-F]{8}", table), handlers)
        for code, spec in WORLDMAP.opcodes.items():
            body = re.search(rf"\ns32 {spec.handler}\([^)]*\) \{{.*?\n\}}", text, re.S).group(0)
            advances = {int(n) for n in re.findall(r"return (\d+);", body)} - {0}
            expected = set() if spec.flow == "stop" else {WORLDMAP.size(spec) // 2}
            self.assertEqual(advances, expected, code)

    def test_arena_entries_follow_the_switch_cases_and_their_advances(self):
        text = (ROOT / "decomp/src/menu/menu2.c").read_text()
        body = text[text.index("s32 func_8007107C(void) {") : text.index("void func_80071724(")]
        parts = re.split(r"\n\s*case (\d+):", body)
        advances = {}
        for number, case in zip(parts[1::2], parts[2::2], strict=True):
            steps = case.count("D_800925F8++")
            steps += sum(int(n) for n in re.findall(r"D_800925F8 \+= (\d+);", case))
            advances[int(number)] = steps
        self.assertEqual(sorted(advances), sorted(ARENA.opcodes))
        for code, spec in ARENA.opcodes.items():
            self.assertEqual(spec.handler, f"case {code}")
            expected = 0 if spec.flow in ("stop", "hang") else ARENA.size(spec)
            self.assertEqual(advances[code], expected, code)


def code(text: str) -> str:
    """C source without its comments."""
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def function(text: str, name: str) -> str:
    """The definition of `name`, without comments."""
    text = code(text)
    start = re.search(rf"\n\w[\w *]* {name}\([^)]*\) \{{", text).start()
    return text[start : text.index("\n}\n", start) + 2]


def cases(body: str, switch: str) -> dict[int, str]:
    """The case bodies of `switch (switch)`, up to its closing brace."""
    start = body.index(f"switch ({switch}) {{")
    depth, end = 0, start
    for end in range(start, len(body)):
        depth += {"{": 1, "}": -1}.get(body[end], 0)
        if depth == 0 and body[end] == "}":
            break
    parts = re.split(r"\n\s*(?:case (\w+)|(default)):", body[start:end])
    found = {}
    for label, default, case in zip(parts[1::3], parts[2::3], parts[3::3], strict=True):
        found["default" if default else int(label, 0)] = case
    return found


# A director case's statements, as the cue actions of overlay_scripts.py.
ACTIONS = (
    (r"func_80097770\((\w+), (\w+)\);", "request"),
    (r"func_80039E60\(\(D_8006259C->id << 16\) \| D_8009A5A0\[D_8009D3D4\]\[(\d)\]\);", "ambient"),
    (r"func_80039E60\(\(D_8006259C->id << 16\) \| (\w+)\);", "sound"),
    (r"func_80039E18\(\(D_8006259C->id << 16\) \| (\w+)\);", "sound_12"),
    (r"func_80089160\((\w+), NULL, NULL\);", "emitters"),
    (r"func_80089160\((\w+), &scratch->position, NULL\);", "emitters_at_target"),
    (r"func_80089514\((\w+)\);", "stop_effects"),
    (r"func_8003A89C\(D_80062528, (\w+), (\w+)\);", "music_fade"),
    (r"D_8009CCA4 = (\w+);", "fade_rate"),
    (r"D_8009D3CC = (\w+);", "fade_step"),
    (r"D_8009D554 = 0;", "exit_worldmap"),
)
QUIET = (r"break;", r"D_8009D7CC = 0;", r"scratch->position\.v[xyz] = .*;")


def case_actions(case: str) -> tuple[tuple, int | None]:
    actions, state = [], None
    for line in (line.strip() for line in case.splitlines()):
        if not line:
            continue
        if found := re.fullmatch(r"actor->state = (\d+);", line):
            state = int(found.group(1))
            continue
        for pattern, verb in ACTIONS:
            if found := re.fullmatch(pattern, line):
                actions.append((verb, *(int(value, 0) for value in found.groups())))
                break
        else:
            if not any(re.fullmatch(pattern, line) for pattern in QUIET):
                raise AssertionError(f"statement without a cue action: {line}")
    return tuple(actions), state


class SceneSourceTests(unittest.TestCase):
    def test_every_case_of_each_director_is_its_cue(self):
        for each in DIRECTORS:
            body = function((ROOT / each.source).read_text(), each.interpreter)
            found = cases(body, "actor->state")
            self.assertEqual(sorted(found), sorted(each.cues), each.interpreter)
            for state, case in found.items():
                with self.subTest(director=each.interpreter, case=state):
                    if state == 1:
                        self.assertIn("if (--actor->wait < 0) {", case)
                        self.assertIn("actor->u.step++;", case)
                        if each.picker is None:
                            sequence = each.sequences[0]
                            self.assertIn(f"D_{sequence.states:08X}[actor->u.step]", case)
                            self.assertIn(f"D_{sequence.durations:08X}[actor->u.step]", case)
                        else:
                            self.assertIn("((u16 *)actor->unk54)[actor->u.step]", case)
                            self.assertIn("((u16 *)actor->unk58)[actor->u.step]", case)
                        continue
                    cue = each.cues[state]
                    self.assertEqual(case_actions(case), (cue.actions, cue.state))

    def test_starters_tables_and_setups_follow_their_definitions(self):
        sources = "".join(
            path.read_text() for path in sorted((ROOT / "decomp/src/worldmap").glob("*.c"))
        )
        modes = re.search(r"WorldmapMode D_8009A058\[19\] = \{(.*?)\n\};", sources, re.S).group(1)
        modes = re.findall(r"\{(\w+), (\w+), (\w+)\}", modes)
        for each in DIRECTORS:
            with self.subTest(director=each.interpreter):
                starter = function(sources, each.starter)
                self.assertEqual("actor->u.step++;" in starter, each.advances)
                if each.picker is None:
                    self.assertIn(f"actor->state = D_{each.sequences[0].states:08X}[0];", starter)
                else:
                    self.assertIn(f"D_{each.picker:08X}[sequence].states", starter)
                    pairs = re.search(
                        rf"Sequence D_{each.picker:08X}\[3\] = \{{(.*?)\}};", sources, re.S
                    )
                    names = [(f"D_{s.states:08X}", f"D_{s.durations:08X}") for s in each.sequences]
                    self.assertEqual(re.findall(r"\{(\w+), (\w+)\}", pairs.group(1)), names)
                for sequence in each.sequences:
                    states = re.search(rf"[us]16 D_{sequence.states:08X}\[(\d+)\] = ", sources)
                    durations = re.search(rf"u16 D_{sequence.durations:08X}\[(\d+)\] = ", sources)
                    self.assertEqual(int(states.group(1)), sequence.entries)
                    self.assertGreaterEqual(int(durations.group(1)), sequence.entries)
                self.assertEqual(modes[each.mode][1], each.setup)
                setup = function(sources, each.setup)
                calls = re.findall(r"func_80097718\(\(s32\)(\w+), \(s32\)(\w+)\);", setup)
                self.assertEqual(
                    calls[:2],
                    [("func_800923A8", "func_800925A0"), (each.starter, each.interpreter)],
                )
                # fresh slots (func_8009766C), so slot n is the setup's n-th actor
                self.assertLess(setup.index("func_8009766C();"), setup.index("func_80097718("))
                self.assertEqual(tuple(update for _, update in calls), each.slots)
                targets = {
                    a[1] for cue in each.cues.values() for a in cue.actions if a[0] == "request"
                }
                self.assertLess(max(targets), len(each.slots))


class TargetTests(unittest.TestCase):
    def test_overlays_name_their_recovered_images(self):
        for machine in (WORLDMAP, ARENA):
            self.assertRegex(expected_sha256(machine.overlay), "^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
