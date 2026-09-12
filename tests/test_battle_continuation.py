"""Synthetic source-boundary cases; these do not extend original-route coverage."""

import unittest

from tools.analysis.battle_continuation import (
    ContinuationState,
    branch_variable_zero,
    wait_request_pending,
)
from tools.analysis.events import EventError, Variables


def bank(zero=0, *, unsigned=False, last=0, last_unsigned=False):
    values = zero.to_bytes(2, "little") + bytes(2044) + last.to_bytes(2, "little")
    types = bytes((1 if unsigned else 0,)) + bytes(126) + bytes((128 if last_unsigned else 0,))
    return Variables(values, types)


def command(operand=0x8020, target=0x08BB):
    return b"\x86" + operand.to_bytes(2, "little") + target.to_bytes(2, "little")


class BattleContinuationTests(unittest.TestCase):
    def test_pending_wait_retries_without_changing_pending_word(self):
        before = ContinuationState(1, 0xFFFFFFFF)
        for pending in (1, 2, 0x80000000, 0xFFFFFFFF):
            effect = wait_request_pending(b"\x00\xfe\x7f", before, pending)
            self.assertEqual(effect.state, ContinuationState(1, 1))
            self.assertEqual(effect.dispatch_state, ContinuationState(2, 0xFFFFFFFF))
            self.assertEqual((effect.pending, effect.ready), (pending, False))
        self.assertEqual(before, ContinuationState(1, 0xFFFFFFFF))

    def test_clear_pending_advances_and_still_requests_break(self):
        effect = wait_request_pending(b"\xfe\x7f", ContinuationState(0, 7), 0)
        self.assertEqual(effect.state, ContinuationState(2, 1))
        self.assertEqual(effect.dispatch_state, ContinuationState(1, 7))
        self.assertTrue(effect.ready)

    def test_extended_dispatch_and_final_pc_wrap_independently(self):
        code = b"\x7f" + bytes(65534) + b"\xfe"
        for pending, expected in ((0, 1), (0x80000000, 65535)):
            effect = wait_request_pending(code, ContinuationState(65535, 9), pending)
            self.assertEqual(effect.dispatch_state, ContinuationState(0, 9))
            self.assertEqual(effect.state, ContinuationState(expected, 1))
        code = bytes(65534) + b"\xfe\x7f"
        self.assertEqual(wait_request_pending(code, ContinuationState(65534, 0), 0).state.pc, 0)

    def test_wait_rejects_wrong_or_missing_opcodes(self):
        for code in (b"", b"\xfe", b"\xfe\x7e", b"\x71\x7f"):
            with self.subTest(code=code), self.assertRaises(ValueError):
                wait_request_pending(code, ContinuationState(0, 0), 0)
        with self.assertRaisesRegex(EventError, "u16 PC space"):
            wait_request_pending(b"\xfe\x7f" + bytes(65535), ContinuationState(0, 0), 0)

    def test_equal_variable_zero_continues_without_reading_target(self):
        before = ContinuationState(0, 0x80000000)
        effect = branch_variable_zero(command()[:3], before, bank(32))
        self.assertEqual(effect.state, ContinuationState(5, 0x80000000))
        self.assertEqual((effect.comparison_value, effect.variable_zero), (32, 32))
        self.assertEqual((effect.equal, effect.target_read), (True, None))

    def test_unequal_reads_raw_target_and_preserves_break(self):
        for target in (0, 1, 0x8000, 0xFFFF):
            effect = branch_variable_zero(command(target=target), ContinuationState(0, 7), bank(0))
            self.assertEqual(effect.state, ContinuationState(target, 7))
            self.assertEqual((effect.equal, effect.target_read), (False, target))
        for code in (command()[:3], command()[:4]):
            with self.assertRaises(ValueError):
                branch_variable_zero(code, ContinuationState(0, 0), bank(0))

    def test_tag_is_15_bit_immediate_not_signed_halfword(self):
        effect = branch_variable_zero(command(0xFFFF), ContinuationState(0, 0), bank(0xFFFF))
        self.assertEqual(
            (effect.comparison_value, effect.variable_zero, effect.equal), (32767, -1, False)
        )
        effect = branch_variable_zero(command(0xFFFF), ContinuationState(0, 0), bank(32767))
        self.assertTrue(effect.equal)

    def test_comparison_variable_and_zero_keep_independent_signedness(self):
        for operand in (2046, 2047):
            for zero_unsigned, other_unsigned, equal in (
                (False, False, True),
                (True, True, True),
                (False, True, False),
                (True, False, False),
            ):
                variables = bank(
                    0x8000, unsigned=zero_unsigned, last=0x8000, last_unsigned=other_unsigned
                )
                effect = branch_variable_zero(command(operand), ContinuationState(0, 8), variables)
                self.assertEqual(effect.equal, equal)
                self.assertEqual(effect.variable_zero, 32768 if zero_unsigned else -32768)
                self.assertEqual(effect.comparison_value, 32768 if other_unsigned else -32768)

    def test_reference_one_aliases_variable_zero(self):
        for unsigned in (False, True):
            effect = branch_variable_zero(
                command(1)[:3], ContinuationState(0, 0), bank(0xFFFF, unsigned=unsigned)
            )
            self.assertTrue(effect.equal)
            self.assertEqual(effect.variable_zero, 65535 if unsigned else -1)

    def test_pc_wrap_does_not_wrap_comparison_or_branch_operand_addresses(self):
        code = bytes(65533) + command()[:3]
        effect = branch_variable_zero(code, ContinuationState(65533, 3), bank(32))
        self.assertEqual(effect.state, ContinuationState(2, 3))
        with self.assertRaises(ValueError):
            branch_variable_zero(code, ContinuationState(65533, 3), bank(0))
        code = command()[1:3] + bytes(65533) + b"\x86"
        with self.assertRaises(ValueError):
            branch_variable_zero(code, ContinuationState(65535, 3), bank(32))

    def test_invalid_comparison_reference_does_not_mutate_inputs(self):
        before, variables = ContinuationState(0, 7), bank(32)
        for operand in (2048, 0x7FFF):
            with self.assertRaisesRegex(EventError, "outside recovered bank"):
                branch_variable_zero(command(operand), before, variables)
        self.assertEqual(before, ContinuationState(0, 7))
        self.assertEqual(variables, bank(32))

    def test_branch_rejects_missing_comparison_wrong_opcode_and_oversized_buffer(self):
        for code in (b"", b"\x86", b"\x86\x20", b"\x85\x20\x80\x00\x00"):
            with self.subTest(code=code), self.assertRaises(ValueError):
                branch_variable_zero(code, ContinuationState(0, 0), bank(32))
        with self.assertRaisesRegex(EventError, "u16 PC space"):
            branch_variable_zero(command() + bytes(65532), ContinuationState(0, 0), bank(32))

    def test_state_and_pending_widths_reject_bool_negative_and_overflow(self):
        for value in (-1, 65536, True):
            with self.assertRaises(EventError):
                ContinuationState(value, 0)
        for value in (-1, 1 << 32, True):
            with self.assertRaises(EventError):
                ContinuationState(0, value)
            with self.assertRaises(EventError):
                wait_request_pending(b"\xfe\x7f", ContinuationState(0, 0), value)


if __name__ == "__main__":
    unittest.main()
