"""Synthetic input qualification checks for the connected original return case."""

import copy
import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from tools.analysis.execution import canonical_digest, compare
from tools.analysis.return_case import (
    encountered_command,
    qualified_resource_spans,
    recorded_artifact,
    validate_report_inputs,
    verify_return_join,
)


def synthetic_join():
    defaults, restore, factories = [], [], []
    table = bytearray(25 * 92)
    descriptors = []
    for index in range(25):
        descriptor = bytearray(92)
        struct.pack_into("<I", descriptor, 0x4C, 0x80100000 + 0x200 * index)
        table[index * 92 : (index + 1) * 92] = descriptor
        descriptor[0x50:0x58] = bytes([index]) * 8
        descriptor[0x58] = 0x40
        descriptors.append(descriptor)
    restored_table = b"".join(descriptors)
    for index in range(25):
        actor = bytes([index]) * 312
        result = bytes([100 + index]) * 312
        defaults.append(({"hook": "defaults-after"}, {"actor": actor}))
        registers = [0] * 34
        registers[17] = index
        registers[7] = 0x80100000 + 0x200 * index
        restore.append(
            (
                {"hook": "restore-actor-before", "gpr_u32": registers},
                {
                    "actor": actor,
                    "descriptors-0": restored_table[:1024],
                    "descriptors-1024": restored_table[1024:2048],
                    "descriptors-2048": restored_table[2048:],
                },
            )
        )
        restore.append(({"hook": "restore-actor-after"}, {"actor": result}))
        factories.append(
            (
                {"hook": "factory-actor-before"},
                {"actor": result, "descriptor": bytes(descriptors[index])},
            )
        )
    defaults.append(
        (
            {"hook": "factory-after"},
            {
                "descriptors-0": bytes(table[:1024]),
                "descriptors-1": bytes(table[1024:2048]),
                "descriptors-2": bytes(table[2048:]),
            },
        )
    )
    return defaults, restore, factories


class ReturnCaseTests(unittest.TestCase):
    def test_starting_input_and_expected_outputs_remain_independent(self):
        records = synthetic_join()
        old_records = copy.deepcopy(records)
        inputs, expected = verify_return_join(*records)
        self.assertEqual(records, old_records)
        self.assertEqual(len(inputs), 25)
        self.assertEqual(bytes.fromhex(inputs[0]["actor"]), bytes(312))
        self.assertEqual(bytes.fromhex(expected[0]["actor"]), bytes([100]) * 312)
        self.assertEqual(bytes.fromhex(inputs[0]["descriptor"])[0x58], 0)
        self.assertEqual(bytes.fromhex(expected[0]["descriptor"])[0x58], 0x40)

    def test_actor_and_descriptor_lineage_corruptions_reject(self):
        mutations = (
            lambda a, r, f: a[4][1].update(actor=bytes(312)),
            lambda a, r, f: f[3][1].update(actor=bytes(312)),
            lambda a, r, f: f[2][1].update(descriptor=bytes(92)),
            lambda a, r, f: r[0][0]["gpr_u32"].__setitem__(7, 0),
            lambda a, r, f: r[0][0]["gpr_u32"].__setitem__(17, 2),
            lambda a, r, f: r.pop(),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                records = synthetic_join()
                mutate(*records)
                with self.assertRaises(ValueError):
                    verify_return_join(*records)

    def test_unwritten_descriptor_change_is_not_masked(self):
        defaults, restore, factories = synthetic_join()
        changed = bytearray(defaults[-1][1]["descriptors-0"])
        changed[7] = 1
        defaults[-1][1]["descriptors-0"] = bytes(changed)
        with self.assertRaisesRegex(ValueError, "Unexplained descriptor"):
            verify_return_join(defaults, restore, factories)

    def test_resource_gaps_remain_unsupported(self):
        source = SimpleNamespace(ram=b"abXdefY", resources={0x80000000: b"abcdefg"})
        self.assertEqual(
            qualified_resource_spans(source),
            [
                {"address": 0x80000000, "bytes": b"ab".hex()},
                {"address": 0x80000003, "bytes": b"def".hex()},
            ],
        )
        source.ram = b"a"
        with self.assertRaisesRegex(ValueError, "beyond qualified RAM"):
            qualified_resource_spans(source)

    def test_existing_finding_and_transitive_input_fingerprints(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "analysis/findings"
            evidence.mkdir(parents=True)
            artifact = root / "report.json"
            artifact.write_text('{"result": "passed"}')
            digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
            finding = {"validation": {"artifacts": [{"path": str(artifact), "sha256": digest}]}}
            (evidence / "EVID-TEST.json").write_text(json.dumps(finding))
            with patch("tools.analysis.return_case.ROOT", root):
                self.assertEqual(recorded_artifact("EVID-TEST", artifact), {"result": "passed"})
                validate_report_inputs({"hashes": {str(artifact): digest}})
                artifact.write_text('{"result": "changed"}')
                with self.assertRaisesRegex(ValueError, "Historical evidence changed"):
                    recorded_artifact("EVID-TEST", artifact)
                with self.assertRaisesRegex(ValueError, "qualification input changed"):
                    validate_report_inputs({"hashes": {str(artifact): digest}})

    def test_encountered_command_uses_committed_original_pc(self):
        registers = [0] * 34
        registers[2], registers[4], registers[17] = 0x80101002, 19, 0x80102000
        sprite = bytearray(356)
        struct.pack_into("<I", sprite, 0x64, registers[2])
        rows = [
            ({"hook": "factory-entry", "gpr_u32": registers}, {}),
            (
                {"hook": "vm-step", "gpr_u32": registers},
                {"command": b"\x96", "sprite": bytes(sprite)},
            ),
        ]
        pc, partial = encountered_command(rows, 19, 0x96)
        self.assertEqual(pc, 0x80101002)
        self.assertEqual(partial["sprite"]["bytes"], sprite.hex())
        self.assertEqual(partial["sprite"]["address"], 0x80102000)
        with self.assertRaisesRegex(ValueError, "Missing original"):
            encountered_command(rows, 18, 0x96)
        registers[2] += 1
        with self.assertRaisesRegex(ValueError, "sprite PC differs"):
            encountered_command(rows, 19, 0x96)

    def test_comparison_keeps_case_binding_and_independent_output_oracles(self):
        case = {"schema_version": 1, "entry": "field_return", "source": {"kind": "synthetic"}}
        digest = hashlib.sha256(
            json.dumps(case, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        self.assertEqual(digest, canonical_digest(case))
        checkpoints = [{"operation": "restore_field_data", "actor": None, "variables": "0100"}]
        sprite = {"address": 0x80102000, "bytes": "1234"}
        expected = {
            **case,
            "case_sha256": digest,
            "checkpoints": checkpoints,
            "stop": {
                "actor_index": 0,
                "operation": "ordinary_sprite_command",
                "opcode": 0x96,
                "sprite_bytecode_pc": 0x80101002,
            },
            "partial_actor": {"index": 0, "sprite": sprite},
        }
        actual = {
            **case,
            "case_sha256": digest,
            "checkpoints": copy.deepcopy(checkpoints),
            "status": "dependency_needs_recovery",
            "opcode": 0x96,
            "sprite_bytecode_pc": 0x80101002,
            "location": {"actor": 0, "operation": "ordinary_sprite_command"},
            "partial_state": {
                "actors": [{"actor": "", "descriptor": ""}],
                "sprites": [{"sprite": copy.deepcopy(sprite), "parts": {}}],
            },
        }
        self.assertEqual(compare(actual, expected)["status"], "matched")
        mutations = (
            lambda value: value["checkpoints"][0].update(variables="0200"),
            lambda value: value["stop"].update(opcode=0x97),
            lambda value: value["stop"].update(sprite_bytecode_pc=0x80101003),
            lambda value: value["partial_actor"]["sprite"].update(bytes="1235"),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                changed = copy.deepcopy(expected)
                mutate(changed)
                self.assertEqual(compare(actual, changed)["status"], "behavioral_divergence")
        changed_input = {**expected, "case_sha256": "0" * 64}
        with self.assertRaisesRegex(ValueError, "starting input differs"):
            compare(actual, changed_input)


if __name__ == "__main__":
    unittest.main()
