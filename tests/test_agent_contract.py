"""Adversarial authored protocol fixtures; no native behavior is claimed here."""

from __future__ import annotations

import copy
import tempfile
import unittest
from pathlib import Path

from tools.repository.agent_contract import (
    ROOT,
    ContractSchemas,
    decode_line,
    read,
    validate,
    validate_parity,
    validate_semantics,
)
from tools.repository.matrix import build_matrix, plan_sources
from tools.repository.validate import validate_traceability


class AgentSpecificationTests(unittest.TestCase):
    def setUp(self):
        self.schemas = ContractSchemas()
        self.examples = {
            item["id"]: item["value"] for item in read(ROOT, "docs/agent/examples.json")["examples"]
        }

    def request(self, value):
        self.schemas.validate(value, "protocol.schema.json#/$defs/request")
        validate_semantics(value)

    def test_all_artifacts_and_examples_form_one_contract(self):
        validate(ROOT, build_matrix())

    def test_phase_exits_cannot_drop_native_architecture_gates(self):
        matrix = build_matrix()
        matrix["crosscutting"]["phase_exits"][2]["requires_agent_gates"].pop()
        with self.assertRaisesRegex(ValueError, "exit omits its native architecture gates"):
            validate(ROOT, matrix)

    def test_protocol_is_not_a_generic_json_object(self):
        for change in (
            {"version": "99.0"},
            {"method": "os.inject_key"},
            {"id": True},
            {"session_id": None},
            {"params": {"at": "latest"}},
            {"unknown": 1},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.request({**self.examples["query"], **change})

    def test_action_limits_types_and_atomic_conflicts(self):
        bad_actions = [
            [{"kind": "axes", "space": "world", "x": 32768, "y": 0}],
            [{"kind": "axes", "space": "world", "x": True, "y": 0}],
            [{"kind": "button", "action": "jump", "phase": "tap"}],
            [{"kind": "keyboard", "key": "Space"}],
            [{"kind": "button", "action": "jump", "phase": p} for p in ("press", "release")],
            [{"kind": "button", "action": "jump", "phase": "press"}] * 65,
        ]
        for actions in bad_actions:
            packet = copy.deepcopy(self.examples["press"])
            packet["params"]["actions"] = actions
            with self.subTest(actions=actions[:2]), self.assertRaises(ValueError):
                self.request(packet)

    def test_ticks_preserve_large_integer_precision_and_reject_overflow(self):
        packet = copy.deepcopy(self.examples["press"])
        packet["params"]["at_tick"] = "9007199254740993"
        self.request(packet)
        for value in ("18446744073709551616", "-1", "01", 1, True, "0"):
            packet["params"]["at_tick"] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.request(packet)

    def test_advance_has_independent_positive_work_and_wall_budgets(self):
        for key, value in (("max_ticks", "0"), ("max_work_units", "0"), ("watchdog_ms", 0)):
            packet = copy.deepcopy(self.examples["advance"])
            packet["params"]["budget"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.request(packet)
        packet = copy.deepcopy(self.examples["advance"])
        packet["params"]["budget"]["max_ticks"] = "120000"
        self.request(packet)  # The emulator's 36000-frame cap is not inherited.

    def test_guards_and_instruction_boundaries_are_unambiguous(self):
        for key, value in (("session_id", "another-session"), ("boundary", "instruction")):
            packet = copy.deepcopy(self.examples["press"])
            packet["params"]["guard"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.request(packet)
        token = copy.deepcopy(self.examples["press"]["params"]["guard"])
        token.update(boundary="instruction", continuation_id="task:1/instruction:4")
        self.schemas.validate(token, "state.schema.json#/$defs/token")
        validate_semantics(token)

    def test_query_pages_cannot_silently_truncate(self):
        packet = copy.deepcopy(self.examples["query-result"])
        packet["result"]["complete"] = False
        self.schemas.validate(packet, "protocol.schema.json#/$defs/completed")
        with self.assertRaisesRegex(ValueError, "pagination"):
            validate_semantics(packet)
        packet["result"]["next_cursor"] = "page:2"
        validate_semantics(packet)

    def test_native_scenario_requires_explicit_time_mode_and_typed_entry(self):
        for change in ("missing_mode", "raw_write", "emulator_frame", "invented_entry"):
            packet = copy.deepcopy(self.examples["create"])
            scenario = packet["params"]["scenario"]
            if change == "missing_mode":
                del packet["params"]["mode"]
            elif change == "raw_write":
                scenario["state_writes"] = [{"offset": 123, "value": "00"}]
            elif change == "emulator_frame":
                scenario["budget"]["timeout_frames"] = 60
            else:
                scenario["entry"]["kind"] = "emulator_checkpoint"
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.request(packet)

    def test_wire_rejects_duplicate_keys_nonfinite_oversize_and_deep_json(self):
        cases = [
            b'{"id":1,"id":2}\n',
            b'{"x":NaN}\n',
            b'{"x":1e999}\n',
            b"{}\n{}\n",
            b"\xef\xbb\xbf{}\n",
            b'{"x":"\xff"}\n',
            b'{"x":"' + b"a" * 1048576 + b'"}\n',
            b'{"x":' + b"[" * 33 + b"0" + b"]" * 33 + b"}\n",
        ]
        for number, data in enumerate(cases):
            with self.subTest(case=number), self.assertRaises(ValueError):
                decode_line(data)
        self.assertEqual(decode_line(b'{"id":"one"}\n'), {"id": "one"})

    def test_schema_audit_rejects_unknown_keywords_and_remote_refs(self):
        for change in ({"silentlyIgnored": True}, {"$ref": "https://example.com/schema#/$defs/x"}):
            self.schemas = ContractSchemas()
            self.schemas.documents["state.schema.json"]["$defs"]["token"].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.schemas.audit()

    def test_emulator_backlog_and_source_drift_cannot_be_promoted(self):
        parity = read(ROOT, "docs/agent/emulator-parity.json")
        methods = {m["id"] for m in read(ROOT, "docs/agent/methods.json")["methods"]}
        gates = {g["id"] for g in read(ROOT, "docs/agent/acceptance.json")["gates"]}
        bad = copy.deepcopy(parity)
        bad["baseline_sources"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "source drift"):
            validate_parity(ROOT, bad, methods, gates)
        bad = copy.deepcopy(parity)
        next(r for r in bad["rows"] if r["id"] == "PARITY-BATTLE")["native"]["relationship"] = (
            "equivalent"
        )
        with self.assertRaisesRegex(ValueError, "backlog"):
            validate_parity(ROOT, bad, methods, gates)

    def test_foundational_table_and_prose_cannot_fall_out_of_traceability(self):
        matrix = build_matrix()
        self.assertEqual(
            len(plan_sources((ROOT / "plan.md").read_text())["tables"]["agent_contract"]), 8
        )
        matrix["crosscutting"]["agent_contract"].pop()
        with self.assertRaisesRegex(ValueError, "agent_contract"):
            validate_traceability(matrix)
        matrix = build_matrix()
        matrix["crosscutting"]["foundational_requirement"]["source"] = "lost"
        with self.assertRaisesRegex(ValueError, "foundational requirement"):
            validate_traceability(matrix)

    def test_changed_source_cannot_be_regenerated_without_coverage_review(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "docs").mkdir()
            for name in ("plan.md", "docs/requirement-facets.txt", "docs/requirement-review.json"):
                (root / name).write_bytes((ROOT / name).read_bytes())
            path = root / "plan.md"
            path.write_text(
                path.read_text().replace(
                    "Create a requirement-to-test matrix",
                    "Create an expanded requirement-to-test matrix",
                    1,
                )
            )
            with self.assertRaisesRegex(ValueError, "renewed source/facet coverage review"):
                build_matrix(root)


if __name__ == "__main__":
    unittest.main()
