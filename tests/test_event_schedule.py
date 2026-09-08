"""Authored boundary cases for original interpreter budgeting and exact flags."""

import unittest

from tools.analysis.event_schedule import (
    DispatchGate,
    after_handler,
    begin_batch,
    dispatch_allowed,
)
from tools.analysis.event_state import InterpreterControl
from tools.analysis.events import EventError


class EventScheduleTests(unittest.TestCase):
    def test_initialization_resets_break_even_when_budget_is_nonpositive(self):
        for budget in (0, 0x80000000, 0xFFFFFFFF):
            result = begin_batch(budget, InterpreterControl(2, 1))
            self.assertEqual(
                (result.next_stage, result.counter, result.budget_u32), ("return", 0, budget)
            )
            self.assertEqual(result.control, InterpreterControl(2, 0))
        self.assertEqual(begin_batch(8, InterpreterControl(1, 1)).next_stage, "loop-check")
        with self.assertRaises(EventError):
            begin_batch(-1, InterpreterControl(1, 0))

    def test_initialization_mode_replaces_budget_and_ignores_break_request(self):
        result = after_handler(8, 1, InterpreterControl(0, 1), DispatchGate(0, (0, 0, 0)))
        self.assertEqual(
            (result.counter, result.budget_u32, result.next_stage), (9, 65535, "loop-check")
        )
        self.assertEqual(result.control, InterpreterControl(0, 1))

    def test_break_requires_both_controls_to_equal_one(self):
        gate = DispatchGate(0, (0, 0, 0))
        result = after_handler(4, 8, InterpreterControl(1, 1), gate)
        self.assertEqual((result.counter, result.reason), (4, "break-request"))
        for mode, request in ((1, 2), (2, 1), (2, 2), (1, 0)):
            result = after_handler(4, 8, InterpreterControl(mode, request), gate)
            self.assertEqual((result.counter, result.next_stage), (5, "loop-check"))

    def test_dispatch_gate_precedes_break_and_preserves_the_counter(self):
        for missing in range(3):
            values = [2, 3, 4]
            values[missing] = 0
            result = after_handler(3, 8, InterpreterControl(1, 1), DispatchGate(2, tuple(values)))
            self.assertEqual((result.counter, result.reason), (3, "dispatch-gate"))
        self.assertFalse(DispatchGate(0, (0, 0, 0)).stops_batch)
        self.assertFalse(DispatchGate(2, (2, 3, 4)).stops_batch)
        result = after_handler(3, 8, InterpreterControl(0, 1), DispatchGate(1, (0, 1, 1)))
        self.assertEqual(
            (result.budget_u32, result.counter, result.reason), (65535, 3, "dispatch-gate")
        )

    def test_budget_comparison_is_signed_and_uses_incremented_counter(self):
        gate, control = DispatchGate(0, (0, 0, 0)), InterpreterControl(1, 0)
        self.assertEqual(after_handler(6, 8, control, gate).next_stage, "loop-check")
        self.assertEqual(after_handler(7, 8, control, gate).next_stage, "return")
        for budget in (0, 0x80000000, 0xFFFFFFFF):
            self.assertEqual(after_handler(0, budget, control, gate).reason, "budget-exhausted")

    def test_hard_limit_permits_1025_dispatches_and_then_stops(self):
        self.assertTrue(dispatch_allowed(1024))
        result = after_handler(1024, 65535, InterpreterControl(0, 0), DispatchGate(0, (0, 0, 0)))
        self.assertEqual((result.counter, result.next_stage), (1025, "loop-check"))
        self.assertFalse(dispatch_allowed(result.counter))
        with self.assertRaises(EventError):
            after_handler(1025, 65535, InterpreterControl(0, 0), DispatchGate(0, (0, 0, 0)))


if __name__ == "__main__":
    unittest.main()
