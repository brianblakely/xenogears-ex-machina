"""Authored source-branch fixtures, separate from local original-run evidence."""

import unittest
from dataclasses import replace

from tools.analysis.events import (
    EventError,
    UnknownInstruction,
    decode_instruction,
    disassemble_reachable,
)
from tools.analysis.field_control import (
    ControlActor,
    ControlError,
    ControlInputs,
    ControlState,
    EncounterGates,
    UnrecoveredEncounter,
    encounter_early_exit,
    request_control,
    terrain_jump_result,
)


def actor(**changes):
    return replace(ControlActor(0x4000, 0, (12, -5, 31), (12, -5, 31), 0x8123, 7, 123), **changes)


def state(**changes):
    return replace(ControlState(0, 0, 987, 0, 9), **changes)


def inputs(**changes):
    # Invented direction tables; original tables are imported only for local comparison.
    tables = (tuple(range(0, 4096, 256)), tuple(0x8000 + i for i in range(16)))
    gates = EncounterGates(1, 1, 1, 0, 0, 0, 0, 1)
    return replace(
        ControlInputs(0, 0, (-1, -1, -1, -1), 0, 255, 0, 8, 42, 0, tables, 0, gates), **changes
    )


class FieldControlTests(unittest.TestCase):
    def test_non_owner_preserves_direction_and_control_state(self):
        before = actor(flags=0xA0040080, pc=65535)
        result = request_control(0xA7, before, state(), inputs(pressed_buttons=0x80))
        self.assertEqual(result.actor, replace(before, flags=0xA1040080, pc=0))
        self.assertEqual(result.state, state())
        self.assertEqual((result.eligibility, result.calls), ("not_control_owner", ()))
        result = request_control(0xA7, before, state(), inputs(preserve_nonplayer_motion=2))
        self.assertEqual(result.actor, replace(before, pc=0))

    def test_dialogue_and_inhibition_block_before_encounter_and_jump(self):
        for index in range(4):
            statuses = [-1] * 4
            statuses[index] = 0
            result = request_control(
                0xA7,
                actor(),
                state(),
                inputs(dialogue_status=tuple(statuses), held_buttons=0x1080, pressed_buttons=0x80),
            )
            self.assertEqual(result.actor, replace(actor(), direction=0x8000, pc=124))
            self.assertEqual(result.state, state())
            self.assertEqual((result.eligibility, result.calls), ("dialogue", ()))
        for inhibition in (-32768, -1, 1, 32767):
            result = request_control(
                0xA7,
                actor(),
                state(),
                inputs(encounter=replace(inputs().encounter, inhibition=inhibition)),
            )
            self.assertEqual((result.eligibility, result.actor.direction), ("inhibited", 0x8000))

    def test_pressed_and_held_jump_are_distinct(self):
        result = request_control(0xA7, actor(), state(), inputs(pressed_buttons=0x80))
        self.assertEqual(
            (result.actor.flags, result.jump, result.calls), (0x4800, "pressed", ("terrain",))
        )
        self.assertEqual((result.state.latched_jump_setting, result.state.updated), (42, 1))
        held = request_control(0xA7, actor(), state(), inputs(held_buttons=0x80))
        self.assertEqual(
            (held.actor.flags, held.jump, held.state.latched_jump_setting),
            (0x4000, "not_pressed", 987),
        )

    def test_normal_jump_rejections_have_no_jump_side_effect(self):
        cases = [
            (actor(flags=0x4800), inputs(pressed_buttons=0x80), "jump_or_airborne"),
            (actor(flags=0x5000), inputs(pressed_buttons=0x80), "jump_or_airborne"),
            (actor(terrain_flags=0x400000), inputs(pressed_buttons=0x80), "counter_terrain"),
            (actor(), inputs(pressed_buttons=0x80, jump_contact=0x100FF), "contact"),
            (actor(flags=0x4200, terrain_flags=8), inputs(pressed_buttons=0x80), "terrain_mask"),
        ]
        for before, controls, reason in cases:
            result = request_control(0xA7, before, state(), controls)
            self.assertEqual(
                (result.actor.flags, result.state.latched_jump_setting), (before.flags, 987)
            )
            self.assertEqual(result.jump, reason)

    def test_terrain_predicate_matches_only_two_corresponding_bits(self):
        for flags, terrain, result in (
            (0, 24, 0),
            (0x200, 8, -1),
            (0x200, 16, 0),
            (0x400, 8, 0),
            (0x400, 16, -1),
            (0x600, 24, -1),
            (0x800, 32, 0),
            (0xFFFFFFFF, 0xFFFFFFE7, 0),
        ):
            self.assertEqual(terrain_jump_result(actor(flags=flags, terrain_flags=terrain)), result)

    def test_counter_held_retry_threshold_and_retention_on_movement(self):
        before = actor(terrain_flags=0x400000)
        controls = inputs(held_buttons=0x80)
        first = request_control(0xA7, before, state(stationary_counter=31), controls)
        self.assertEqual((first.state.stationary_counter, first.jump), (32, "not_pressed"))
        second = request_control(0xA7, before, first.state, controls)
        self.assertEqual((second.state.stationary_counter, second.jump), (32, "held_retry"))
        moving = replace(before, integer_position=(13, -5, 31))
        result = request_control(0xA7, moving, state(stationary_counter=17), controls)
        self.assertEqual(result.state.stationary_counter, 17)
        result = request_control(0xA7, moving, state(stationary_counter=33), controls)
        self.assertEqual((result.state.stationary_counter, result.jump), (32, "held_retry"))
        self.assertEqual(
            request_control(0xA7, actor(), second.state, controls).state.stationary_counter, 0
        )

    def test_counter_wraps_before_signed_threshold_comparison(self):
        for value, expected in ((0x7FFF, 0x8000), (0xFFFE, 0xFFFF), (0xFFFF, 0)):
            result = request_control(
                0xA7,
                actor(terrain_flags=0x400000),
                state(stationary_counter=value),
                inputs(held_buttons=0x80),
            )
            self.assertEqual(
                (result.state.stationary_counter, result.jump), (expected, "not_pressed")
            )

    def test_alternate_mode_reloads_then_decrements_without_airborne_gate(self):
        controls = inputs(jump_mode=-1, pressed_buttons=0x80, repeat_delay=2)
        result = request_control(0xA7, actor(flags=0x5800), state(), controls)
        self.assertEqual(
            (result.jump, result.actor.animation_mode, result.state.repeat_remaining),
            ("pressed", 255, 1),
        )
        blocked = request_control(0xA7, actor(), result.state, controls)
        self.assertEqual(
            (blocked.jump, blocked.state.repeat_remaining, blocked.calls), ("repeat_delay", 0, ())
        )
        empty = request_control(0xA7, actor(), state(), replace(controls, repeat_delay=0))
        self.assertEqual(empty.state.repeat_remaining, 0)
        negative = request_control(
            0xA7, actor(), state(repeat_remaining=65535), inputs(jump_mode=1)
        )
        self.assertEqual(negative.state.repeat_remaining, 65534)

    def test_held_retry_bypasses_alternate_mode_even_when_terrain_rejects(self):
        for terrain, jump in ((0x400000, "held_retry"), (0x400008, "terrain_mask")):
            result = request_control(
                0xA7,
                actor(flags=0x4200, terrain_flags=terrain),
                state(stationary_counter=32, repeat_remaining=7),
                inputs(jump_mode=1, held_buttons=0x80),
            )
            self.assertEqual((result.jump, result.terrain_call_variant), (jump, "normal"))
            self.assertEqual((result.actor.animation_mode, result.state.repeat_remaining), (7, 7))

    def test_direction_uses_all_nibble_combinations_and_preserves_sentinel_bits(self):
        expected = (
            0xEEE,
            0xDEE,
            0xCEE,
            0xBEE,
            0xAEE,
            0x9EE,
            0x8EE,
            0x7EE,
            0x6EE,
            0x5EE,
            0x4EE,
            0x3EE,
            0x2EE,
            0x1EE,
            0x0EE,
            0xFEE,
        )
        for nibble, direction in enumerate(expected):
            result = request_control(
                0xA7, actor(), state(), inputs(held_buttons=nibble << 12, camera_angle=18)
            )
            self.assertEqual(result.actor.direction, direction)
            sentinel = request_control(
                0xA7,
                actor(),
                state(),
                inputs(held_buttons=nibble << 12, camera_angle=-32768, alternate_directions=2),
            )
            self.assertEqual(sentinel.actor.direction, 0x800F - nibble)

    def test_encounter_gates_use_exact_values_and_first_return(self):
        open_gates = replace(inputs().encounter, encounter_gate=1, menu_gate=2)
        self.assertIsNone(encounter_early_exit(open_gates))
        for name, value, reason in (
            ("field_active", 0, "field_inactive"),
            ("gate_e4", 0, "gate_e4_clear"),
            ("gate_ec", 0, "gate_ec_clear"),
            ("music_result", 0xFFFFFFFF, "music_pending"),
            ("encounter_gate", 0, "encounter_gate_clear"),
            ("inhibition", -1, "inhibited"),
            ("menu_gate", 1, "menu_gate_one"),
            ("enabled_byte", 0, "enabled_byte_clear"),
        ):
            self.assertEqual(encounter_early_exit(replace(open_gates, **{name: value})), reason)
        self.assertIsNone(
            encounter_early_exit(replace(open_gates, inhibition=1, music_result=0xFFFFFFFE))
        )
        self.assertEqual(
            encounter_early_exit(replace(open_gates, field_active=0, gate_e4=0)), "field_inactive"
        )
        with self.assertRaises(UnrecoveredEncounter):
            request_control(
                0xA7, actor(), state(), inputs(held_buttons=0x1000, encounter=open_gates)
            )
        self.assertEqual(
            request_control(0xA7, actor(), state(), inputs(encounter=open_gates)).calls, ()
        )

    def test_looping_wrapper_restores_pc_and_requests_break_even_when_blocked(self):
        for before in (actor(pc=65535), actor(flags=0, pc=65535)):
            result = request_control(0x0C, before, state(), inputs(pressed_buttons=0x80))
            plain = request_control(0xA7, before, state(), inputs(pressed_buttons=0x80))
            self.assertEqual(result.actor, replace(plain.actor, pc=65535))
            self.assertEqual(result.state, replace(plain.state, break_requested=1))

    def test_invalid_widths_and_unrecovered_opcode_fail_explicitly(self):
        for invalid in (-1, 65536, True):
            with self.assertRaises(ControlError):
                inputs(held_buttons=invalid)
        for invalid in (-32769, 32768):
            with self.assertRaises(ControlError):
                actor(integer_position=(invalid, 0, 0))
        with self.assertRaises(ControlError):
            inputs(direction_tables=((0,) * 15, (0,) * 16))
        with self.assertRaises(ControlError):
            request_control(0x0B, actor(), state(), inputs())

    def test_control_instruction_lengths_and_reachable_successors(self):
        plain = decode_instruction(bytes([0xA7, 0]), 0)
        self.assertEqual((plain.size, plain.operands, plain.successors), (1, (), (1,)))
        loop = decode_instruction(bytes([0x0C]), 0)
        self.assertEqual((loop.size, loop.operands, loop.successors), (1, (), (0,)))
        self.assertEqual(disassemble_reachable(bytes([0x0C]), 0), (loop,))
        self.assertEqual([i.pc for i in disassemble_reachable(bytes([0xA7, 0]), 0)], [0, 1])
        wrapped = decode_instruction(bytes(65535) + bytes([0xA7]), 65535)
        self.assertEqual(wrapped.successors, (0,))
        with self.assertRaises(EventError):
            decode_instruction(bytes([0xA7]), 0)
        with self.assertRaises(UnknownInstruction):
            decode_instruction(bytes([0x0B, 0]), 0)


if __name__ == "__main__":
    unittest.main()
