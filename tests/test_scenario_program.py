"""Synthetic checks for scenario readiness, guarded writes and exact input duration."""

import copy
import unittest

from tools.reference.scenario_program import ScenarioProgram, validate_program


def program(steps):
    return {
        "schema_version": 1,
        "name": "invented-fixture",
        "source_profile": "synthetic-source",
        "kind": "analysis_probe",
        "steps": steps,
    }


def condition(offset, value):
    return {"offset": offset, "width": 1, "operator": "eq", "value": value}


class ScenarioProgramTests(unittest.TestCase):
    def test_readiness_requires_all_conditions_and_consecutive_frames(self):
        executor = ScenarioProgram(
            program(
                [
                    {
                        "name": "ready",
                        "timeout_frames": 8,
                        "stable_frames": 2,
                        "when": [condition(0, 1), condition(1, 2)],
                        "capture": True,
                    }
                ]
            )
        )
        for frame, memory in enumerate((b"\x01\x00", b"\x01\x02", b"\x00\x02", b"\x01\x02")):
            self.assertEqual(executor.tick(frame, memory, lambda *_: None), [])
            self.assertFalse(executor.complete)
        self.assertEqual(executor.tick(4, b"\x01\x02", lambda *_: None), ["ready"])
        self.assertTrue(executor.complete)

    def test_timeout_contains_observed_values_and_never_reports_ready(self):
        executor = ScenarioProgram(
            program(
                [
                    {
                        "name": "unready",
                        "timeout_frames": 2,
                        "when": [condition(0, 9)],
                    }
                ]
            )
        )
        executor.tick(0, b"\x03", lambda *_: None)
        executor.tick(1, b"\x03", lambda *_: None)
        with self.assertRaisesRegex(TimeoutError, "actual.*3"):
            executor.tick(2, b"\x03", lambda *_: None)
        self.assertFalse(executor.complete)

    def test_all_write_fingerprints_are_checked_before_any_mutation(self):
        writes = []
        executor = ScenarioProgram(
            program(
                [
                    {
                        "name": "guarded",
                        "timeout_frames": 1,
                        "writes": [
                            {
                                "offset": 0,
                                "expected": "01",
                                "value": "03",
                                "reason": "invented field",
                            },
                            {
                                "offset": 1,
                                "expected": "ff",
                                "value": "04",
                                "reason": "invented field",
                            },
                        ],
                    }
                ]
            )
        )
        with self.assertRaisesRegex(ValueError, "fingerprint mismatch"):
            executor.tick(0, b"\x01\x02", lambda *change: writes.append(change))
        self.assertEqual(writes, [])

    def test_written_memory_is_visible_to_next_step_and_buttons_have_exact_duration(self):
        executor = ScenarioProgram(
            program(
                [
                    {
                        "name": "initialize",
                        "timeout_frames": 1,
                        "writes": [
                            {
                                "offset": 0,
                                "expected": "00",
                                "value": "09",
                                "reason": "invented field",
                            }
                        ],
                    },
                    {
                        "name": "press",
                        "timeout_frames": 1,
                        "when": [condition(0, 9)],
                        "buttons": ["square"],
                        "run_frames": 2,
                        "capture": True,
                    },
                    {"name": "release", "timeout_frames": 1, "run_frames": 1, "capture": True},
                ]
            )
        )
        memory = bytearray(4)

        def write(offset, data):
            memory[offset : offset + len(data)] = data

        self.assertEqual(executor.tick(0, bytes(memory), write), [])
        self.assertEqual(memory[0], 9)
        self.assertEqual(executor.buttons, ["square"])
        executor.tick(1, bytes(memory), write)
        self.assertEqual(executor.buttons, ["square"])
        self.assertEqual(executor.tick(2, bytes(memory), write), ["press"])
        self.assertEqual(executor.buttons, [])
        self.assertFalse(executor.complete)
        self.assertEqual(executor.tick(3, bytes(memory), write), ["release"])
        self.assertTrue(executor.complete)
        self.assertEqual(executor.events[0]["writes"][0]["before"], "00")

    def test_malformed_or_unbounded_programs_are_rejected(self):
        base = program([{"name": "test", "timeout_frames": 1}])
        examples = []
        for changes in (
            {"timeout_frames": True},
            {"run_frames": -1},
            {"buttons": ["unknown"], "run_frames": 1},
            {"capture": "true"},
            {"typo": 1},
            {"when": [condition(2 * 1024 * 1024, 0)]},
        ):
            value = copy.deepcopy(base)
            value["steps"][0].update(changes)
            examples.append(value)
        value = copy.deepcopy(base)
        value["steps"][0]["writes"] = [
            {"offset": 0, "expected": "00", "value": "01", "reason": "fixture"}
        ] * 2
        examples.append(value)
        value = copy.deepcopy(base)
        value["kind"] = "recovered_scenario"
        value["steps"][0]["writes"] = [
            {"offset": 0, "expected": "00", "value": "01", "reason": "fixture"}
        ]
        examples.append(value)
        for value in examples:
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_program(value)

    def test_actual_memory_bounds_and_frame_order_are_enforced(self):
        executor = ScenarioProgram(
            program([{"name": "test", "timeout_frames": 1, "when": [condition(8, 0)]}])
        )
        with self.assertRaisesRegex(ValueError, "actual emulator RAM"):
            executor.tick(0, bytes(4), lambda *_: None)
        executor = ScenarioProgram(
            program([{"name": "test", "timeout_frames": 1, "run_frames": 3}])
        )
        with self.assertRaisesRegex(ValueError, "start at zero"):
            executor.tick(1, bytes(4), lambda *_: None)


if __name__ == "__main__":
    unittest.main()
