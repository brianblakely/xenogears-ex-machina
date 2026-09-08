"""Source availability and unsupported-feature checks for the scenario compiler."""

import copy
import unittest

from tools.reference.scenario import PROFILE_IDS, PROFILE_SOURCES, compile_scenario


class ScenarioCompilerTests(unittest.TestCase):
    def setUp(self):
        self.scenario = {
            "schema_version": 1,
            "name": "invented-scenario",
            "source_profile": PROFILE_IDS[0],
            "entry": {"type": "field", "map": 14},
        }
        self.catalog = {
            "profiles": [
                {
                    "source_profile": PROFILE_IDS[0],
                    **PROFILE_SOURCES[PROFILE_IDS[0]],
                    "pairs": [
                        {
                            "candidate_map_index": 14,
                            "source_slots": [634, 635],
                            "availability": "non_dummy_source_pair",
                            "exact_cdmake_dummy": [False, False],
                        },
                        {
                            "candidate_map_index": 15,
                            "source_slots": [636, 637],
                            "availability": "exact_dummy_source_pair",
                            "exact_cdmake_dummy": [True, True],
                        },
                    ],
                }
            ],
        }

    def test_compile_known_source_pair_with_explicit_readiness_and_input_release(self):
        self.scenario["steps"] = [
            {"buttons": ["square"], "hold_frames": 8, "after_frames": 30, "capture": True},
        ]
        compiled, budget = compile_scenario(self.scenario, self.catalog)
        self.assertEqual(compiled["source_profile"], PROFILE_IDS[0])
        self.assertLessEqual(budget, 36000)
        self.assertEqual(compiled["steps"][-2]["buttons"], ["square"])
        self.assertNotIn("buttons", compiled["steps"][-1])
        self.assertEqual(compiled["steps"][-1]["run_frames"], 30)
        self.assertTrue(any(step.get("when") for step in compiled["steps"]))

    def test_dummy_missing_and_wrong_source_pairs_are_rejected(self):
        for map_id, pattern in ((15, "dummy"), (16, "no measured source")):
            with self.subTest(map=map_id):
                self.scenario["entry"]["map"] = map_id
                with self.assertRaisesRegex(ValueError, pattern):
                    compile_scenario(self.scenario, self.catalog)
        self.scenario["source_profile"] = PROFILE_IDS[1]
        with self.assertRaisesRegex(ValueError, "No measured source-pair"):
            compile_scenario(self.scenario, self.catalog)

    def test_unrecovered_capabilities_cannot_silently_fall_back(self):
        variants = []
        for key, value in (
            ("entry", {"type": "battle"}),
            ("launch", {"ready": "field"}),
            ("inventory", {"items": [1]}),
            ("position", {"x": 1, "y": 2}),
        ):
            variant = copy.deepcopy(self.scenario)
            variant[key] = value
            variants.append(variant)
        for variant in variants:
            with self.subTest(variant=variant), self.assertRaises(ValueError):
                compile_scenario(variant, self.catalog)

    def test_relabelled_catalog_and_relocated_pair_cannot_launch(self):
        for key, replacement in (
            ("raw_track_sha256", "0" * 64),
            ("source_index_table_sha256", "1" * 64),
            ("first_child_slot", 601),
        ):
            catalog = copy.deepcopy(self.catalog)
            catalog["profiles"][0][key] = replacement
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "catalog identity"):
                compile_scenario(self.scenario, catalog)
        self.catalog["profiles"][0]["pairs"][0]["source_slots"] = [635, 636]
        with self.assertRaisesRegex(ValueError, "field-slot formula"):
            compile_scenario(self.scenario, self.catalog)

    def test_custom_writes_do_not_inherit_recovered_loader_evidence(self):
        self.scenario["state_writes"] = [
            {"offset": 128, "expected": "00", "value": "01", "reason": "Synthetic probe"}
        ]
        program, _ = compile_scenario(self.scenario, self.catalog)
        self.assertEqual(program["kind"], "analysis_probe")
        setup = next(step for step in program["steps"] if step["name"] == "unreviewed-state-setup")
        self.assertNotIn("evidence", setup)
        request = next(step for step in program["steps"] if step["name"] == "request-field")
        self.assertFalse(any(write["offset"] == 128 for write in request["writes"]))

    def test_explicit_state_setup_cannot_clobber_loader_controls(self):
        self.scenario["state_writes"] = [
            {
                "offset": 0x18088,
                "expected": "00000000",
                "value": "02000000",
                "reason": "invented conflicting setup",
            }
        ]
        with self.assertRaisesRegex(ValueError, "launcher controls"):
            compile_scenario(self.scenario, self.catalog)

    def test_invalid_inputs_and_unbounded_scenarios_fail_during_compilation(self):
        for steps in ([{"buttons": ["typo"]}], [{"wait_frames": 30000}] * 2):
            self.scenario["steps"] = steps
            with self.subTest(steps=steps), self.assertRaises(ValueError):
                compile_scenario(self.scenario, self.catalog)


if __name__ == "__main__":
    unittest.main()
