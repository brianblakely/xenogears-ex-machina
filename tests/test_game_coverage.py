"""Coverage must observe actual execution, and hangs must preserve a replay."""

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

from tools import game_coverage, game_explore, game_frontier


class Instrumentation(unittest.TestCase):
    @unittest.skipUnless(
        os.environ.get("XEM_CLANG") and shutil.which("node"), "needs the pinned runtime shell"
    )
    def test_field_dispatches_outcomes_discs_and_epoch_reset_in_real_wasm(self):
        # Different scripts can run exactly the same native blocks. Both
        # outcomes of one instruction, and the same PC on another disc/map,
        # must survive deduplication. No per-frame sampling is involved.
        schema = {"globals": {
            "field_current_event_actor": {"address": 0x80000000, "type": 0},
            "cd_directory_table": {"address": 0x80000004},
            "mode_field_map_id": {"address": 0x80000008},
        }, "types": [
            {"kind": "pointer", "to": "record FieldActor"},
            {"kind": "struct", "name": "FieldActor", "members": [
                {"name": "pc", "offset": 12, "type": 2}]},
            {"kind": "base", "size": 2},
        ]}
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "ir").mkdir()
            (out / "schema.json").write_text(json.dumps(schema))
            ir, _ = game_coverage.field_probe(out)
            obj, wasm = out / "probe.o", out / "probe.wasm"
            subprocess.run([os.environ["XEM_CLANG"], "--target=wasm32-unknown-unknown",
                            "-O2", "-c", str(ir), "-o", str(obj)], check=True)
            subprocess.run([os.environ["XEM_WASM_LD"], "--no-entry",
                            "--export=xem_coverage_field_begin", "--export=xem_coverage_field_end",
                            "--export=xem_coverage_field_load",
                            "--initial-memory=2149580800", str(obj), "-o", str(wasm)], check=True)
            script = """const fs = require('fs');
(async () => {
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(process.argv[1]));
  const e = instance.exports, m = new DataView(e.memory.buffer);
  const layout = JSON.parse(process.argv[2]), h = layout.header, actor = 0x80003000;
  m.setUint32(0x80000004, 0x80002000, true);
  const run = (disc, map, pc, next) => {
    m.setUint16(0x80002078, disc, true); m.setUint32(0x80000008, map, true);
    e.xem_coverage_field_load();
    m.setUint16(actor + 12, pc, true);
    const key = e.xem_coverage_field_begin(actor);
    m.setUint16(actor + 12, next, true); e.xem_coverage_field_end(key, actor);
  };
  run(1, 23, 10, 20); run(1, 23, 10, 20); run(1, 23, 10, 30);
  run(2, 23, 10, 20); run(1, 24, 10, 20);
  // Next-map/disc requests must not relabel the still-loaded bytecode.
  // Restore just the identity words, as the runtime's port-region snapshot does.
  const saved = [m.getUint32(layout.identity, true), m.getUint32(layout.identity + 4, true)];
  m.setUint16(0x80002078, 2, true); m.setUint32(0x80000008, 99, true);
  e.xem_coverage_field_load();
  m.setUint32(layout.identity, saved[0], true); m.setUint32(layout.identity + 4, saved[1], true);
  m.setUint16(actor + 12, 12, true);
  e.xem_coverage_field_begin(actor);
  const keys = () => Array.from({length: m.getUint32(h + 4, true)}, (_, i) =>
    m.getBigUint64(layout.queue + i * 8, true).toString());
  const first = keys();
  m.setUint32(h, 2, true); m.setUint32(h + 4, 0, true);
  run(1, 23, 10, 20);
  const second = keys();
  m.setUint32(h + 4, layout.capacity, true); run(1, 25, 11, 30);
  console.log(JSON.stringify([first, second, m.getUint32(h + 8, true)]));
})();"""
            output = subprocess.run(["node", "-e", script, str(wasm),
                                     json.dumps(game_coverage.SCRIPT)], check=True,
                                    capture_output=True, text=True)
            first, second, overflow = json.loads(output.stdout)
            key = lambda d, m, pc, to: (d << 45) | (m << 33) | (pc << 17) | to
            expected = {key(d, m, 10, to) for d, m in ((1, 23), (2, 23), (1, 24))
                        for to in (20, 65536)} | {key(1, 23, 10, 30), key(1, 24, 12, 65536)}
            self.assertEqual(set(map(int, first)), expected)
            self.assertEqual(set(map(int, second)), {key(1, 23, 10, to) for to in (20, 65536)})
            self.assertEqual(overflow, 1)

    def test_script_overflow_cannot_look_like_a_coverage_plateau(self):
        manifest = {"field_scripts": game_coverage.SCRIPT}
        with self.assertRaisesRegex(ValueError, "overflow"):
            game_explore.read_scripts(lambda _: struct.pack("<III", 1, 10, 2).hex(), manifest)

    def test_dispatch_hook_fails_closed_if_interpreter_changes(self):
        with self.assertRaisesRegex(ValueError, "expected one"):
            game_coverage.instrument_field("define void @other() {\nret void\n}", 1)
        with self.assertRaisesRegex(ValueError, "expected one"):
            game_coverage.instrument_field_load("define void @other() {\nret void\n}")

    @unittest.skipUnless(
        os.environ.get("XEM_CLANG") and shutil.which("node"), "needs the pinned runtime shell"
    )
    def test_real_wasm_branches_phi_and_readonly_callers(self):
        # The function's old memory(none) attribute must not let a caller
        # elide the second call, or coverage disappears despite valid IR.
        text = """target triple = "wasm32-unknown-unknown"
@message = private constant [9 x i8] c"readonly\\00"
define i32 @choose(i32 %x) #0 {
entry:
  %condition = icmp slt i32 %x, 0
  br i1 %condition, label %negative, label %positive
negative:
  br label %merge
positive:
  br label %merge
merge:
  %result = phi i32 [ 17, %negative ], [ 31, %positive ]
  ret i32 %result
}
define i32 @readonly(i32 %x) #0 {
  %a = call i32 @choose(i32 %x) #0
  %b = call i32 @choose(i32 1) #0
  ret i32 %a
}
attributes #0 = { nounwind memory(none) }
"""
        blocks = []
        result = game_coverage.instrument(text, "fixture.c", "fixture", blocks)
        self.assertEqual(len(blocks), 5)
        self.assertIn('c"readonly\\00"', result)
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            ir, wasm = (directory / name for name in ("test.ll", "game.wasm"))
            ir.write_text(result)
            objects = []
            for source in (ir,):
                obj = source.with_suffix(".o")
                subprocess.run(
                    [
                        os.environ["XEM_CLANG"],
                        "--target=wasm32-unknown-unknown",
                        "-O2",
                        "-c",
                        str(source),
                        "-o",
                        str(obj),
                    ],
                    check=True,
                )
                objects.append(str(obj))
            subprocess.run(
                [
                    os.environ["XEM_WASM_LD"],
                    "--no-entry",
                    "--export=choose",
                    "--export=readonly",
                    f"--initial-memory={game_coverage.BASE + 65536}",
                    *objects,
                    "-o",
                    str(wasm),
                ],
                check=True,
            )
            manifest = {"blocks": blocks, "signature": "ab" * 32, "address": game_coverage.BASE}
            game_coverage.initialize(wasm, manifest)
            game_coverage.finish(directory, manifest)
            script = """const fs = require('fs');
(async () => {
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(process.argv[1]));
  const e = instance.exports;
  const bytes = new Uint8Array(e.memory.buffer);
  const address = Number(process.argv[2]);
  const start = address + 32;
  const run = f => {
    bytes.fill(0, start, start + 5);
    const result = f();
    return [result, [...bytes.slice(start, start + 5)]];
  };
  console.log(JSON.stringify([[...bytes.slice(address, start)], run(() => e.choose(-1)),
                             run(() => e.choose(0)), run(() => e.readonly(-1))]));
})();"""
            execution = subprocess.run(
                ["node", "-e", script, str(wasm), str(game_coverage.BASE)],
                check=True,
                capture_output=True,
                text=True,
            )
            signature, negative, positive, caller = json.loads(execution.stdout)
            self.assertEqual(signature, [0xAB] * 32)
            self.assertEqual(negative, [17, [1, 1, 0, 1, 0]])
            self.assertEqual(positive, [31, [1, 0, 1, 1, 0]])
            self.assertEqual(caller, [17, [1, 1, 1, 1, 1]])

    def test_wrong_build_and_corrupted_probes_are_not_coverage(self):
        manifest = {"address": 100, "signature": "ab" * 32, "blocks": [None]}
        with self.assertRaisesRegex(ValueError, "does not match"):
            game_explore.read_coverage(lambda _: "00" * 32, manifest)
        with self.assertRaisesRegex(ValueError, "invalid coverage"):
            game_explore.read_coverage(
                lambda cmd: manifest["signature"] if cmd["address"] == 100 else "02", manifest
            )


class Search(unittest.TestCase):
    def test_reseed_keeps_input_and_disc_but_never_imports_old_probe_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            commands = [{"cmd": "pad", "buttons": ["cross"]}, {"cmd": "frames", "count": 30}]
            (root / "case.json").write_text(json.dumps({
                "origin": "scenario", "disc": "/private/disc2.chd", "features": [99999999],
                "prefix": commands[:1], "commands": commands[1:],
            }))
            (root / "suite.json").write_text(json.dumps({"cases": ["case.json"]}))
            self.assertEqual(game_frontier.replay_seeds(root / "suite.json"),
                             [("scenario", commands, None, Path("/private/disc2.chd"))])

    def test_minimization_keeps_unique_branch_witness_and_avoids_expensive_duplicate(self):
        cases = [
            {"id": "expensive", "features": {1, 2}, "cost": 100},
            {"id": "short", "features": {1, 2}, "cost": 1},
            {"id": "branch", "features": {2, 3}, "cost": 2},
            {"id": "other_disc", "features": {4}, "cost": 3},
        ]
        selected = game_frontier.minimize(cases)
        self.assertEqual({c["id"] for c in selected}, {"short", "branch", "other_disc"})
        self.assertEqual(set().union(*(c["features"] for c in selected)), {1, 2, 3, 4})

    def test_dialogue_choices_are_reached_by_distinct_released_pad_pulses(self):
        targets = {"what": "dialogue", "selected": 0, "confirm": True,
                   "steps": {"kind": "ring", "count": 3, "next": "Down"},
                   "entries": [{}, {}, {}]}
        choices = game_frontier.menu_actions(targets)
        self.assertEqual(len(choices), 3)
        moves = [[c["actions"] for c in commands if c["cmd"] == "act"]
                 for _, commands in choices]
        self.assertEqual(moves, [[["Confirm"]], [["Down"], ["Confirm"]],
                                 [["Up"], ["Confirm"]]])
        for _, commands in choices:
            self.assertFalse(any(c["cmd"] in ("write", "write_memory") for c in commands))


class Watchdog(unittest.TestCase):
    def client(self, code, timeout=1):
        client = game_explore.Client(
            [sys.executable, "-u", "-c", code], subprocess.DEVNULL, timeout
        )
        self.addCleanup(client.close)
        return client

    def test_partial_reply_cannot_bypass_timeout(self):
        client = self.client(
            'import sys, time; input(); sys.stdout.write("{"); sys.stdout.flush(); time.sleep(30)',
            0.15,
        )
        began = time.monotonic()
        with self.assertRaises(game_explore.Failure) as caught:
            client({"cmd": "frames", "count": 1})
        self.assertEqual(caught.exception.kind, "timeout")
        self.assertLess(time.monotonic() - began, 2)

    def test_process_exit_and_query_trap_are_failures(self):
        client = self.client("import sys; input(); sys.exit(3)")
        with self.assertRaises(game_explore.Failure) as caught:
            client({"cmd": "frames"})
        self.assertEqual(caught.exception.kind, "process_exit")
        client = self.client('input(); print(\'{"query_trap":"bad pointer"}\')')
        with self.assertRaises(game_explore.Failure) as caught:
            client({"cmd": "status"})
        self.assertEqual(caught.exception.kind, "query_trap")

    def test_unread_command_and_unmet_seed_condition_fail(self):
        client = self.client("import time; time.sleep(30)", 0.15)
        with self.assertRaises(game_explore.Failure) as caught:
            client({"cmd": "schedule", "padding": "x" * 1_000_000})
        self.assertEqual(caught.exception.kind, "timeout")
        client = self.client('input(); print(\'{"met":false}\')')
        with self.assertRaises(game_explore.Failure) as caught:
            client({"cmd": "run_until", "max_frames": 1, "until": {"frames": 100}})
        self.assertEqual(caught.exception.kind, "condition_not_met")

    def test_debug_log_never_counts_as_legal_progression(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "log.json"
            path.write_text(
                json.dumps(
                    {
                        "commands": [[0, {"cmd": "write_memory", "address": 1, "hex": "00"}]],
                        "frames": 10,
                    }
                )
            )
            kind, _, _ = game_explore.seed_commands(argparse.Namespace(field=None, log=path))
            self.assertEqual(kind, "scenario")
            path.write_text(
                json.dumps({"commands": [[0, {"cmd": "pad", "buttons": ["cross"]}]], "frames": 10})
            )
            kind, _, _ = game_explore.seed_commands(argparse.Namespace(field=None, log=path))
            self.assertEqual(kind, "legal")


if __name__ == "__main__":
    unittest.main()
