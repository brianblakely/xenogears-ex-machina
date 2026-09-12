"""Invented request states distinguish source gate, width and operand boundaries."""

import unittest
from dataclasses import replace

from tools.analysis.battle_request import BattleRequestGates, BattleRequestState, request_battle
from tools.analysis.events import (
    EventError,
    UnknownInstruction,
    Variables,
    decode_instruction,
    disassemble_reachable,
)


def state(**changes):
    return replace(BattleRequestState(0, 7, 0xABCDEF01, 0xFFFFFFFF, 91, 37, 255), **changes)


def gates(**changes):
    return replace(BattleRequestGates(17, 2, 0x80000000, 0, 0, 0), **changes)


def command(operand):
    return b"\x71" + operand.to_bytes(2, "little")


class BattleRequestTests(unittest.TestCase):
    def test_decoder_preserves_tagged_operand_and_both_retry_and_accepted_paths(self):
        for operand in (0, 2047, 0x8000, 0xFFFF):
            code = command(operand) + b"\x00"
            decoded = decode_instruction(code, 0)
            self.assertEqual((decoded.name, decoded.size), ("request_battle", 3))
            self.assertEqual((decoded.operands, decoded.successors), ((operand,), (0, 3)))
            self.assertEqual([row.pc for row in disassemble_reachable(code, 0)], [0, 3])

    def test_decoder_retains_operand_and_successor_bounds(self):
        for code in (b"\x71", b"\x71\x00", command(0x8000)):
            with self.subTest(code=code), self.assertRaises(ValueError):
                decode_instruction(code, 0)
        code = bytes(65533) + command(0x8000)
        self.assertEqual(decode_instruction(code, 65533).successors, (65533, 0))
        with self.assertRaises(ValueError):
            decode_instruction(b"\x00\x80" + bytes(65533) + b"\x71", 65535)

    def test_decoder_still_rejects_unrecovered_post_request_behavior(self):
        with self.assertRaises(UnknownInstruction) as caught:
            disassemble_reachable(command(0x8000) + b"\x70", 0)
        self.assertEqual((caught.exception.pc, caught.exception.opcode), (3, 0x70))

    def test_music_retry_preserves_request_and_pc_then_accepts_when_ready(self):
        before = state()
        blocked = gates(music_result=0xFFFFFFFF)
        result = request_battle(command(0x8000), before, blocked, 4)
        self.assertEqual(result.state, replace(before, break_requested=1))
        self.assertEqual(result.gates, blocked)
        self.assertEqual((result.accepted, result.retry_reason), (False, "music_pending"))
        self.assertIsNone(result.resolution_state)
        self.assertIsNone(result.selector_value)
        ready = request_battle(command(0x8000), result.state, gates(), 4)
        self.assertTrue(ready.accepted)
        self.assertEqual(ready.state, BattleRequestState(3, 1, 0, 1, 0, 4, 0))
        self.assertEqual(ready.gates, replace(gates(), field_active=0))
        self.assertEqual((before, blocked), (state(), gates(music_result=0xFFFFFFFF)))

    def test_each_gate_retries_without_reading_a_missing_operand_or_variable(self):
        for name, value, reason in (
            ("field_active", 0, "field_inactive"),
            ("gate_e4", 0, "gate_e4_clear"),
            ("gate_ec", 0, "gate_ec_clear"),
            ("menu_gate", 2, "menu_gate_set"),
            ("music_result", 0xFFFFFFFF, "music_pending"),
            ("gate_90", 0x80000000, "gate_90_set"),
        ):
            with self.subTest(gate=name):
                blocked = gates(**{name: value})
                result = request_battle(b"\x71", state(), blocked, 5)
                self.assertEqual(result.retry_reason, reason)
                self.assertEqual(result.state, replace(state(), break_requested=1))
                self.assertEqual(result.gates, blocked)
                self.assertFalse(request_battle(command(2048), state(), blocked, 5).accepted)

    def test_gate_order_and_exact_music_sentinel_are_preserved(self):
        result = request_battle(b"\x71", state(), gates(field_active=0, menu_gate=1, gate_90=1), 0)
        self.assertEqual(result.retry_reason, "field_inactive")
        result = request_battle(b"\x71", state(), gates(menu_gate=1, music_result=0xFFFFFFFF), 0)
        self.assertEqual(result.retry_reason, "menu_gate_set")
        for music in (0, 1, 0x80000000, 0xFFFFFFFE):
            self.assertTrue(
                request_battle(command(0x8000), state(), gates(music_result=music), 0).accepted
            )

    def test_mode_latches_before_resolved_selector_and_all_request_stores(self):
        before = state(pc=1)
        result = request_battle(b"\x00" + command(0xFFFF), before, gates(), 255)
        self.assertEqual(result.resolution_state, replace(before, mode=255))
        self.assertEqual(result.selector_value, 32767)
        self.assertEqual(result.state, BattleRequestState(4, 1, 0, 1, 255, 255, 0))
        self.assertIsNone(result.retry_reason)

    def test_signed_and_unsigned_last_variable_keep_odd_reference_alias(self):
        data = bytes(2046) + b"\x80\xff"
        for unsigned, expected in ((False, -128), (True, 65408)):
            bitmap = bytes(127) + bytes((128 if unsigned else 0,))
            bank = Variables(data, bitmap)
            for reference in (2046, 2047):
                result = request_battle(command(reference), state(), gates(), 3, bank)
                self.assertEqual((result.selector_value, result.state.selector), (expected, 128))
                self.assertEqual(bank.values, data)

    def test_tagged_immediate_does_not_consume_a_variable_bank(self):
        for operand, full, byte in (
            (0x8000, 0, 0),
            (0x80FF, 255, 255),
            (0x8100, 256, 0),
            (0xFFFF, 32767, 255),
        ):
            result = request_battle(command(operand), state(), gates(), 1)
            self.assertEqual((result.selector_value, result.state.selector), (full, byte))
        with self.assertRaisesRegex(EventError, "variable bank"):
            request_battle(command(0), state(), gates(), 1)

    def test_variable_and_operand_bounds_fail_without_mutating_inputs(self):
        before, inputs = state(), gates()
        bank = Variables(bytes(2048), bytes(128))
        for operand in (2048, 0x7FFF):
            with self.assertRaisesRegex(EventError, "outside recovered bank"):
                request_battle(command(operand), before, inputs, 2, bank)
        for bytecode in (b"\x71", b"\x71\x80"):
            with self.assertRaises(ValueError):
                request_battle(bytecode, before, inputs, 2)
        self.assertEqual((before, inputs), (state(), gates()))

    def test_final_pc_wraps_but_operand_addresses_do_not(self):
        bytecode = bytes(65533) + command(0x8000)
        result = request_battle(bytecode, state(pc=65533), gates(), 0)
        self.assertEqual(result.state.pc, 0)
        bytecode = b"\x00\x80" + bytes(65533) + b"\x71"
        with self.assertRaises(ValueError):
            request_battle(bytecode, state(pc=65535), gates(), 0)
        result = request_battle(bytecode, state(pc=65535), gates(field_active=0), 0)
        self.assertEqual(result.state.pc, 65535)

    def test_source_widths_unknown_opcodes_and_oversized_bytecode_fail(self):
        for value in (-1, 256, True):
            with self.assertRaises(EventError):
                state(selector=value)
            with self.assertRaises(EventError):
                request_battle(command(0x8000), state(), gates(), value)
        for value in (-1, 1 << 32, True):
            with self.assertRaises(EventError):
                gates(music_result=value)
        for value in (-1, 65536, True):
            with self.assertRaises(EventError):
                state(pc=value)
        with self.assertRaisesRegex(EventError, "primary opcode 71"):
            request_battle(b"\x70\x00\x80", state(), gates(), 0)
        with self.assertRaisesRegex(EventError, "u16 PC space"):
            request_battle(command(0x8000) + bytes(65534), state(), gates(), 0)


if __name__ == "__main__":
    unittest.main()
