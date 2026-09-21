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
from tools.analysis.original_trace import collector_fingerprint, qualify_collector
from tools.analysis.return_case import (
    encountered_command,
    first_child_command,
    qualified_resource_spans,
    recorded_artifact,
    required_child,
    required_upload_allocations,
    required_upload_requests,
    validate_report_inputs,
    verify_required_join,
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
    def test_first_child_command_excludes_unowned_capture_tail_and_qualifies_prefix(self):
        node = bytearray(320)
        struct.pack_into("<I", node, 4, 0x80102038)
        struct.pack_into("<I", node, 56 + 0x64, 0x80103000)
        struct.pack_into("<H", node, 56 + 0x9E, 1)
        sprite = bytearray(node[56:]) + bytes([0xAB]) * 92
        struct.pack_into("<H", sprite, 0x9E, 0)
        factory, callback, vm = [0] * 34, [0] * 34, [0] * 34
        factory[4], callback[4] = 19, 0x80102000
        vm[17], vm[2] = 0x80102038, 0x80103000
        rows = [
            ({"hook": "factory-entry", "gpr_u32": factory}, {}),
            ({"hook": "task-before", "gpr_u32": callback}, {"node": bytes(node)}),
            (
                {"hook": "vm-step", "gpr_u32": vm},
                {
                    "sprite": bytes(sprite),
                    "command": b"\x8d",
                    "sprite-controls": bytes(64),
                    "task-active-count": bytes(4),
                    "task-heads": bytes(12),
                    "texture-page": bytes(2),
                    "texture-mode": bytes(4),
                },
            ),
        ]
        pc, final = first_child_command(rows, 19)
        self.assertEqual(pc, vm[2])
        self.assertEqual(bytes.fromhex(final["task_nodes"][0]["bytes"]), node[:56] + sprite[:264])
        sprite[0] = 1
        rows[-1][1]["sprite"] = bytes(sprite)
        with self.assertRaisesRegex(ValueError, "Unexplained original child change"):
            first_child_command(rows, 19)

    def test_upload_oracle_qualifies_original_pixels_and_return_cursor(self):
        registers = [0] * 34
        registers[4], registers[5], registers[29], registers[31] = (
            0x801FFF10,
            0x80102000,
            0x801FFF00,
            0x8002DF7C,
        )
        rectangle = struct.pack("<4h", 10, 20, 2, 1)
        original = b"\x01\x02\x03\x04"
        returned = registers.copy()
        returned[16] = registers[5] + len(original)
        rows = [
            ({"hook": "factory-entry", "gpr_u32": [0] * 4 + [19] + [0] * 29}, {}),
            ({"hook": "upload-entry", "gpr_u32": registers}, {}),
            (
                {"hook": "image-entry", "gpr_u32": registers},
                {"rectangle": rectangle, "pixel-prefix": original, "pixel-continuation": b""},
            ),
            ({"hook": "image-after", "gpr_u32": returned}, {"frame": bytes(16) + rectangle}),
            ({"hook": "upload-after", "gpr_u32": registers}, {}),
        ]
        resources = SimpleNamespace(read=lambda address, size: original)
        expected = required_upload_requests(rows, resources, 19)
        self.assertEqual(
            expected,
            [
                {
                    "rectangle": [10, 20, 2, 1],
                    "source_address": registers[5],
                    "bytes": original.hex(),
                }
            ],
        )
        changed = copy.deepcopy(rows)
        changed[2][1]["pixel-prefix"] = bytes(4)
        with self.assertRaisesRegex(ValueError, "qualified source"):
            required_upload_requests(changed, resources, 19)
        rows[3][0]["gpr_u32"][16] += 2
        with self.assertRaisesRegex(ValueError, "return/cursor"):
            required_upload_requests(rows, resources, 19)

    def test_child_allocation_input_stays_separate_from_observed_output(self):
        allocation_address, return_pc, stack = 0x80102000, 0x800233CC, 0x801FFF00
        rows = []
        for name in (
            "factory-entry",
            "child-allocation-before",
            "child-allocation-after",
            "child-after",
        ):
            registers = [0] * 34
            registers[4] = 19 if name == "factory-entry" else 320
            registers[2] = registers[20] = allocation_address
            registers[29], registers[31] = stack, return_pc
            payload = {
                "incoming": bytes(320),
                "node": bytes([1]) * 320,
                "sprite-controls": bytes(64),
                "task-active-count": bytes(4),
                "task-heads": bytes(12),
                "texture-page": bytes(2),
                "texture-mode": bytes(4),
            }
            rows.append(({"hook": name, "gpr_u32": registers, "pc": return_pc}, payload))
        allocation, final = required_child(rows, 19)
        self.assertEqual(bytes.fromhex(allocation["bytes"]), bytes(320))
        self.assertEqual(bytes.fromhex(final["task_nodes"][0]["bytes"]), bytes([1]) * 320)
        rows[-1][0]["gpr_u32"][20] += 4
        with self.assertRaisesRegex(ValueError, "allocation/output lineage"):
            required_child(rows, 19)

    def test_required_capture_full_call_and_shared_state_join(self):
        factories, required = [], []
        for name in ("factory-entry", "vm-step"):
            row = {"hook": name, "pc": 0x80010000, "frontend_run": 10, "gpr_u32": [0] * 34}
            shared = {"sprite": bytes(356), "command": b"\xfc", "heap-controls": bytes(24)}
            factories.append(
                (row, {**shared, "sprite-controls": bytes(48), "task-controls": bytes(364)})
            )
            required.append(
                (
                    copy.deepcopy(row),
                    {
                        **shared,
                        "sprite-controls": bytes(64),
                        "task-heads": bytes(12),
                        "task-current": bytes(4),
                        "task-next": bytes(4),
                    },
                )
            )
        verify_required_join(factories, required)
        mutations = (
            lambda rows: rows[0][0]["gpr_u32"].__setitem__(4, 1),
            lambda rows: rows[0][0].update(frontend_run=11),
            lambda rows: rows[1][1].update(sprite=bytes([1]) + bytes(355)),
            lambda rows: rows[1][1].update(command=b"\xe0"),
            lambda rows: rows[0][1].update(**{"task-heads": b"\x01" + bytes(11)}),
            lambda rows: rows.pop(),
        )
        for mutate in mutations:
            changed = copy.deepcopy(required)
            mutate(changed)
            with self.assertRaisesRegex(ValueError, "Required capture"):
                verify_required_join(factories, changed)

    def test_scratch_allocations_keep_exact_incoming_bytes_modes_and_order(self):
        registers = [0] * 34
        registers[4], registers[2], registers[31] = 19, 0x80102000, 0x80010000
        rows = []
        for name in ("factory-entry", "fc-entry", "fc-allocation", "helper-allocation", "fc-after"):
            rows.append(
                (
                    {"hook": name, "gpr_u32": registers, "pc": registers[31]},
                    {"incoming-" + str(i): bytes([i]) * 1024 for i in range(8)},
                )
            )
        blocks = required_upload_allocations(rows, 19)
        self.assertEqual([block["mode"] for block in blocks], [0, 1])
        self.assertEqual(blocks[0]["address"], registers[2])
        self.assertEqual(
            bytes.fromhex(blocks[0]["bytes"]), b"".join(bytes([i]) * 1024 for i in range(8))
        )
        changed = copy.deepcopy(rows)
        changed[3][1]["incoming-7"] = bytes(10)
        with self.assertRaisesRegex(ValueError, "Incomplete original scratch"):
            required_upload_allocations(changed, 19)
        with self.assertRaisesRegex(ValueError, "Expected one complete"):
            required_upload_allocations(rows, 18)

    def test_historical_collector_requires_exact_pinned_source_contents(self):
        revision = "1" * 40
        source = b"synthetic collector source\n"
        digest = hashlib.sha256(source).hexdigest()
        metadata = {
            "tool_sha256": digest,
            "memory_helpers_sha256": digest,
            "validation_helpers_sha256": digest,
            "core_extension_inputs": {
                name: digest
                for name in ("flake.nix", "reference-trace.h", "reference-trace-patch.py")
            },
        }
        with patch("tools.analysis.original_trace.subprocess.run") as read:
            read.return_value = SimpleNamespace(stdout=source)
            qualify_collector(metadata, revision)
            self.assertEqual(read.call_count, 6)
            self.assertEqual(
                read.call_args_list[0].args[0],
                [
                    "git",
                    "--no-replace-objects",
                    "show",
                    revision + ":tools/reference/instruction_trace.py",
                ],
            )
            read.return_value = SimpleNamespace(stdout=source + b"changed")
            with self.assertRaisesRegex(ValueError, "trace collector changed"):
                qualify_collector(metadata, revision)
            read.return_value = SimpleNamespace(stdout=source)
            metadata["core_extension_inputs"].pop("reference-trace.h")
            with self.assertRaisesRegex(ValueError, "source set changed"):
                qualify_collector(metadata, revision)
            for unpinned in ("HEAD", "1" * 39, "main:path", "1" * 40 + "\n"):
                with self.subTest(revision=unpinned):
                    with self.assertRaisesRegex(ValueError, "Unpinned"):
                        collector_fingerprint("tools/reference/instruction_trace.py", unpinned)

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
