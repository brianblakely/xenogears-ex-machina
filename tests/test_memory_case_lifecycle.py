"""Synthetic runner outcomes test comparison gates, never original fidelity."""

import argparse
import collections
import copy
import hashlib
import io
import json
import struct
import tempfile
import unittest
from contextlib import ExitStack, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from tools.analysis import memory_case as case


class LifecycleComparisonTests(unittest.TestCase):
    def run_synthetic(self, menu, mutate=lambda report: None, *, returned=True, limit=10):
        with tempfile.TemporaryDirectory() as directory, ExitStack() as patches:
            root = Path(directory)
            (root / "runner").write_bytes(b"synthetic runner")
            snapshot_file = case.snapshot_path(root / "instruction-trace.jsonl")
            snapshot_file.write_bytes(b"synthetic snapshots")

            def row(hook, event):
                registers = [0] * 34
                registers[2], registers[29], registers[31] = 7, 0x801FFFF0, case.MAIN_LOOP_RETURN
                return {
                    "hook": hook,
                    "event": event,
                    "cycle_u32": 100 + event * 10,
                    "subcycle_u32": 0,
                    "frontend_run": event,
                    "pc": 0,
                    "code": 0,
                    "gpr_u32": registers,
                    "cop2_u32": [0] * 64,
                    "load_delay": {"select": 0, "registers": [0, 0], "values": [0, 0]},
                }

            if menu:
                rows = [row("menu-entry", 0), row("frame-entry", 1), row("frame-entry", 2)]
                if returned:
                    rows.append(row("menu-exit", 3))
                boundaries = ["frame_entry"] * 2
            else:
                rows = [
                    row(hook, k)
                    for k, hook in enumerate(
                        ["frame-entry", "frame-exit", "frame-entry", "frame-exit"]
                    )
                ]
                boundaries = ["exit", "entry", "exit"]
            trace_file = root / "instruction-trace.jsonl"
            trace_file.write_text("".join(json.dumps(row) + "\n" for row in rows))
            (root / "observation.json").write_text(
                json.dumps(
                    {
                        "instruction_trace": {
                            "trace_sha256": hashlib.sha256(trace_file.read_bytes()).hexdigest(),
                            "snapshot_file_sha256": hashlib.sha256(
                                snapshot_file.read_bytes()
                            ).hexdigest(),
                        }
                    }
                )
            )
            report = {
                "status": "completed_boundary",
                "frames": [
                    {"boundary": kind, "owned": [], "gte": [0] * 31, "stack_windows": []}
                    for kind in boundaries
                ],
                "owned": [],
                "gte": [0] * 31,
                "return_value": 7,
                "platform_unconsumed": 0,
                "services_unconsumed": 0,
                "card_unconsumed": 0,
                "delivered_sectors": [],
            }
            mutate(report)
            reader = patches.enter_context(patch.object(case, "SnapshotReader"))
            reader.return_value.read.return_value = (bytes(case.RAM), b"", bytes(0x1000))
            runner = patches.enter_context(patch.object(case, "BatchRunner"))
            runner.return_value.__enter__.return_value.call.return_value = report
            patches.enter_context(
                patch.object(case, "loop_inputs", return_value=([], {}, [], set()))
            )
            patches.enter_context(
                patch.object(case, "chain_services", return_value=([], collections.Counter()))
            )
            patches.enter_context(
                patch.object(case, "menu_inputs", return_value=([], {}, [], set()))
            )
            patches.enter_context(patch.object(case, "menu_otc_alarms", return_value=set()))
            patches.enter_context(patch.object(case, "call_services", return_value=[]))
            patches.enter_context(patch.object(case, "card_lines", return_value=[]))
            patches.enter_context(
                patch.object(case, "qualify_disc_source", return_value={"synthetic": True})
            )
            patches.enter_context(
                patch.object(
                    case,
                    "qualify_captures",
                    return_value={
                        "images": rows,
                        "platform": rows,
                        "services": rows,
                    },
                )
            )
            sources = patches.enter_context(patch.object(case, "Sources"))
            sources.return_value.field = sources.return_value.overlay = b"synthetic source"
            sources.return_value.manifest.return_value = ""
            args = argparse.Namespace(
                capture=root,
                platform=root,
                services=root,
                entry="menu_call" if menu else "field_frame",
                frames=2,
                start=0,
                limit=limit,
                field_entry=None,
                map=None,
                field_slot=None,
                runner=root / "runner",
                raw=root / "absent-disc",
                budget=100,
                timeout=1,
                report=root / "comparison.json",
                frame_above=0,
                entry_hook="menu-entry",
                exit_hook="menu-exit",
                function="801c62a8",
                return_register=2,
                call_platform=[],
            )
            with redirect_stdout(io.StringIO()):
                status = case.run_menu(args) if menu else case.run_frames(args)
            summary = json.loads(args.report.read_text())
            return status, summary if menu else summary["chains"][0]

    def test_complete_matching_boundaries_pass(self):
        for menu in (False, True):
            with self.subTest(menu=menu):
                status, summary = self.run_synthetic(menu)
                self.assertEqual(status, 0)
                self.assertTrue(summary["comparison_passed"])

    def test_matching_prefix_or_extra_boundaries_fail(self):
        for menu in (False, True):
            for change in (
                lambda r: r["frames"].pop(),
                lambda r: r["frames"].append(copy.deepcopy(r["frames"][-1])),
            ):
                with self.subTest(menu=menu, change=change):
                    status, summary = self.run_synthetic(menu, change)
                    self.assertEqual(status, 1)
                    self.assertFalse(summary["comparison_passed"])

    def test_matching_state_does_not_override_execution_or_input_failures(self):
        for menu in (False, True):
            for key, value in (
                ("status", "dependency_needs_recovery"),
                ("platform_unconsumed", 1),
                ("services_unconsumed", 1),
                ("card_unconsumed", 1),
                ("delivered_sectors", [123]),
                ("status", "runner_failure"),
            ):
                with self.subTest(menu=menu, key=key):
                    status, summary = self.run_synthetic(
                        menu, lambda r, key=key, value=value: r.update({key: value})
                    )
                    self.assertEqual(status, 1)
                    self.assertFalse(summary["comparison_passed"])

    def test_menu_requires_original_return_and_all_recorded_frames(self):
        for settings in ({"returned": False}, {"limit": 1}):
            with self.subTest(settings=settings):
                status, summary = self.run_synthetic(True, **settings)
                self.assertEqual(status, 1)
                self.assertFalse(summary["comparison_passed"])

    def test_menu_compares_gte_at_each_frame_and_at_return_and_return_value(self):
        for change in (
            lambda r: r["frames"][0]["gte"].__setitem__(0, 1),
            lambda r: r["gte"].__setitem__(0, 1),
            lambda r: r.update(return_value=8),
        ):
            with self.subTest(change=change):
                status, summary = self.run_synthetic(True, change)
                self.assertEqual(status, 1)
                self.assertIsNotNone(summary["first_divergence"])

    def test_menu_rejects_unexpected_runner_boundaries(self):
        for change in (
            lambda r: r["frames"][0].update(boundary="unexpected"),
            lambda r: r["frames"].append({"boundary": "unexpected"}),
        ):
            with self.subTest(change=change):
                status, summary = self.run_synthetic(True, change)
                self.assertEqual(status, 1)
                self.assertFalse(summary["comparison_passed"])

    def test_service_order_uses_the_selected_entry_across_cycle_wrap(self):
        def row(event, cycle, value):
            registers = [0] * 34
            registers[2] = value
            return {
                "event": event,
                "cycle_u32": cycle,
                "subcycle_u32": 0,
                "hook": "vsync1-loop",
                "code": 0,
                "gpr_u32": registers,
                "load_delay": {"select": 0, "registers": [0, 0], "values": [0, 0]},
            }

        lines, _ = case.chain_services(
            [row(0, 50, 99), row(1, 0xFFFFFFFE, 1), row(2, 5, 2)],
            [],
            lambda r: r["event"] > 0,
            (),
            0xFFFFFFF0,
        )
        self.assertEqual(lines, ["hblank 1", "hblank 2"])

    def test_reported_heap_stacks_do_not_hide_owned_or_unowned_writes(self):
        entry, exit = bytearray(case.RAM), bytearray(case.RAM)
        exit[0x100], exit[0x101] = 2, 3
        result = case.compare(
            bytes(entry),
            bytes(exit),
            [{"name": "reused-heap", "address": 0x80000100, "hex": "01"}],
            0x801FFFF0,
            stack_windows=((0x80000100, 0x100),),
        )
        self.assertEqual(result["mismatch_count"], 1)
        self.assertEqual(result["unowned_writes"], ["0x80000101"])
        self.assertEqual(result["reported_stack_mismatches"], 1)

    def test_payload_entry_is_an_external_arrival_position(self):
        ram = bytearray(case.RAM)
        struct.pack_into("<I", ram, 0x567B4, case.IO_BASE + 0xB0)

        def row(hook, event):
            return {
                "hook": hook,
                "event": event,
                "code": 0,
                "gpr_u32": [0] * 34,
                "load_delay": {"select": 0, "registers": [0, 0], "values": [0, 0]},
            }

        rows = [
            row(hook, i)
            for i, hook in enumerate(
                (
                    "frame-entry",
                    "tick-entry",
                    "tick-exit",
                    "apply-entry",
                    "tick-entry",
                    "tick-exit",
                )
            )
        ]
        lines, counts, sectors, _ = case.menu_inputs(rows, bytes(ram), bytes(0x1000), set())
        self.assertEqual(lines, ["tick 1 0", "tick 2 0"])
        self.assertEqual(counts["tick_arrivals"], 2)
        self.assertEqual(sectors, [])

    def test_supplemental_position_stays_in_the_selected_call_across_wrap(self):
        entry = {"frontend_run": 10, "cycle_u32": 0xFFFFFFF0}
        exit = {"frontend_run": 12, "cycle_u32": 0x20}
        for frontend in (9, 13):
            self.assertFalse(
                case.menu_position_in_call({"frontend_run": frontend, "cycle_u32": 5}, entry, exit)
            )
        self.assertTrue(
            case.menu_position_in_call({"frontend_run": 11, "cycle_u32": 5}, entry, exit)
        )
        self.assertFalse(
            case.menu_position_in_call({"frontend_run": 11, "cycle_u32": 0x30}, entry, exit)
        )


if __name__ == "__main__":
    unittest.main()
