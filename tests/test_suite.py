"""Exercise the suite's change guard and its runner without starting Nix or any platform tests."""

import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

SUITE = Path(__file__).resolve().parents[1] / "src/runtime/scripts/suite.sh"
_spec = importlib.util.spec_from_file_location("suite_runner", SUITE.with_suffix(".py"))
runner = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = runner
_spec.loader.exec_module(runner)


class SuiteGuardTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="suite guard ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.script = self.root / "src/runtime/scripts/suite.sh"
        self.script.parent.mkdir(parents=True)
        shutil.copy2(SUITE, self.script)
        self.code = self.root / "src/runtime/crates/example/src/lib.rs"
        self.code.parent.mkdir(parents=True)
        self.code.write_text("pub fn original() {}\n")
        (self.root / ".gitignore").write_text("/build/\n/src/runtime/target/\n")
        self.git("init", "--quiet", "--initial-branch=master")
        self.commit()
        self.git("update-ref", "refs/remotes/origin/master", "HEAD")
        self.calls = self.root / "nix-calls"
        fake_bin = self.root / "fake-bin"
        fake_bin.mkdir()
        nix = fake_bin / "nix"
        nix.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$SUITE_NIX_CALLS"\n')
        nix.chmod(0o755)
        self.env = dict(os.environ, PATH=f"{fake_bin}:{os.environ['PATH']}", SUITE_NIX_CALLS=str(self.calls))
        self.env.pop("XEM_SUITE_SHELL", None)
        self.log = self.root / "build/suite/previous.log"
        self.log.parent.mkdir(parents=True)
        self.log.write_text("previous results\n")

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True).strip()

    def commit(self):
        self.git("add", ".")
        self.git("-c", "user.name=Suite test", "-c", "user.email=suite@example.invalid",
                 "commit", "--quiet", "--no-gpg-sign", "-m", "Test inputs")

    def run_suite(self, *args):
        return subprocess.run([str(self.script), *args], cwd=self.root / "src", env=self.env,
                              text=True, capture_output=True, check=False)

    def assert_skipped(self, *args):
        result = self.run_suite(*args)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Skipping", result.stdout)
        self.assertFalse(self.calls.exists(), "unchanged inputs must not start Nix")
        self.assertEqual(self.log.read_text(), "previous results\n")

    def assert_started(self, *args):
        result = self.run_suite(*args)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("Skipping", result.stdout)
        self.assertIn("XEM_SUITE_SHELL=1", self.calls.read_text())

    def test_clean_checkout_and_unrelated_changes_skip_every_tier(self):
        (self.root / "README.md").write_text("Documentation update\n")
        (self.root / "src/runtime/README.md").write_text("Runtime documentation update\n")
        release = self.root / "tools/release/publish.py"
        release.parent.mkdir(parents=True)
        release.write_text("# Publisher change\n")
        output = self.root / "src/runtime/target/generated.rs"
        output.parent.mkdir()
        output.write_text("ignored output\n")
        for tier in ["desktop", "web", "android", "xr", "full"]:
            with self.subTest(tier=tier):
                self.assert_skipped(tier)

    def test_unstaged_runtime_change_starts_the_selected_tier(self):
        self.code.write_text("pub fn updated() {}\n")
        self.assert_started("web")
        self.assertEqual(self.calls.read_text().splitlines()[-3:], ["web", "--base", "origin/master"])

    def test_staged_runtime_change_is_detected(self):
        self.code.write_text("pub fn updated() {}\n")
        self.git("add", str(self.code))
        self.assert_started()

    def test_committed_branch_changes_count_but_an_explicit_head_base_can_skip(self):
        self.git("checkout", "--quiet", "-b", "topic")
        self.code.write_text("pub fn updated() {}\n")
        self.commit()
        self.assert_skipped("--base", "HEAD")
        self.assert_started("desktop")

    def test_deleted_runtime_code_is_detected(self):
        self.code.unlink()
        self.assert_started("desktop")

    def test_untracked_sources_tests_and_build_inputs_are_detected(self):
        for name in ["src/runtime/crates/example/src/new.rs", "src/port/port.c",
                     "nix/runtime/flake.lock", "tools/game_schema.py", "tests/test_port_gpu.py"]:
            with self.subTest(name=name):
                path = self.root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("new input\n")
                self.assert_started("desktop")
                path.unlink()
                self.calls.unlink()

    def test_force_bypasses_change_detection_and_is_forwarded_into_nix(self):
        self.git("update-ref", "-d", "refs/remotes/origin/master")
        self.assert_started("--force", "desktop")
        self.assertEqual(self.calls.read_text().splitlines()[-4:],
                         ["desktop", "--base", "origin/master", "--force"])

    def test_a_thread_count_is_forwarded_into_nix_after_the_tier_and_its_base(self):
        self.code.write_text("pub fn updated() {}\n")
        self.assert_started("--threads", "6", "web")
        self.assertEqual(self.calls.read_text().splitlines()[-5:], ["web", "--base", "origin/master", "--threads", "6"])
        self.assert_started("--force", "--threads", "12")
        self.assertEqual(self.calls.read_text().splitlines()[-6:], ["desktop", "--base", "origin/master", "--force", "--threads", "12"])

    def test_missing_base_is_an_error_instead_of_a_skip(self):
        result = self.run_suite("--base", "missing-ref")
        self.assertEqual(result.returncode, 2)
        self.assertIn("Cannot resolve suite base", result.stderr)
        self.assertNotIn("Skipping", result.stdout)
        self.assertFalse(self.calls.exists())

    def test_help_and_invalid_arguments_do_not_start_nix(self):
        result = self.run_suite("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--force", result.stdout)
        self.assertIn("--threads", result.stdout)
        for args in [["--base"], ["--base", "--force"], ["desktop", "web"], ["--unknown"], ["--threads"], ["--threads", "0"],
                     ["--threads", "many"], ["--threads", "--force"]]:
            with self.subTest(args=args):
                self.assertEqual(self.run_suite(*args).returncode, 2)
        self.assertFalse(self.calls.exists())


class Machine:
    """Stands in for running a unit's command: notes what ran, and how many at once."""

    def __init__(self, failing=(), seconds=0.01, each=None):
        self.failing, self.seconds, self.each = set(failing), seconds, each or {}
        self.ran, self.beside = [], {}
        self.active = set()
        self.lock = threading.Lock()

    def execute(self, unit, log):
        with self.lock:
            self.active.add(unit.name)
            self.ran.append(unit.name)
            for name in self.active:
                self.beside[name] = max(self.beside.get(name, 0), len(self.active))
        seconds = self.each.get(unit.name, self.seconds)
        time.sleep(seconds)
        with self.lock:
            self.active.discard(unit.name)
        return runner.Ran(unit.name not in self.failing, seconds, seconds * unit.cpus, 100)


class Working(Machine):
    """And the processor seconds a machine has worked that keeps `elsewhere` threads busy with
    something else and each unit's own while it runs."""

    def __init__(self, elsewhere, **options):
        super().__init__(**options)
        self.elsewhere, self.spans = elsewhere, []

    def execute(self, unit, log):
        span = [unit.cpus, time.monotonic(), None]
        self.spans.append(span)
        try:
            return super().execute(unit, log)
        finally:
            span[2] = time.monotonic()

    def worked(self):
        now = time.monotonic()
        return self.elsewhere * now + sum(cpus * ((end or now) - start) for cpus, start, end in list(self.spans))


class SuiteRunnerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="suite runner ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.records = {}

    def run_units(self, units, machine=None, force=False, cpus=4.0, memory=4096, spare=None, worked=None):
        machine = machine or Machine()
        lines = []
        run = runner.Run(units, runner.Machine(cpus, memory, spare, worked), self.records, force, machine.execute, self.root, lines.append)
        run.patience, run.window = 0.01, 0.02
        return {name: result.outcome for name, result in run.run().items()}, machine, run

    def test_a_unit_runs_once_its_needs_passed_and_is_left_when_one_failed(self):
        units = [runner.Unit("tests", ["true"], needs=("build",)), runner.Unit("build", ["true"]),
                 runner.Unit("more", ["true"], needs=("tests",)), runner.Unit("apart", ["true"])]
        outcomes, machine, _ = self.run_units(units)
        self.assertEqual(outcomes, {"build": "pass", "tests": "pass", "more": "pass", "apart": "pass"})
        self.assertLess(machine.ran.index("build"), machine.ran.index("tests"))
        self.assertLess(machine.ran.index("tests"), machine.ran.index("more"))
        outcomes, machine, run = self.run_units(units, Machine(failing={"build"}))
        self.assertEqual(outcomes, {"build": "FAIL", "tests": "left", "more": "left", "apart": "pass"})
        self.assertEqual(sorted(machine.ran), ["apart", "build"])
        self.assertEqual(run.results["tests"].note, "needs build")
        # A failure names its log, and what to run by hand to look into it.
        self.assertEqual(run.results["build"].note, f"{self.root / 'build'}.log")
        outcomes, _, run = self.run_units([runner.Unit("binary", ["true"], look="cargo test -p example")], Machine(failing={"binary"}))
        self.assertEqual(run.results["binary"].note, f"{self.root / 'binary'}.log; cargo test -p example")

    def test_units_run_together_only_within_the_machines_threads_and_memory(self):
        units = [runner.Unit(f"threads-{n}", ["true"], cpus=2) for n in range(6)]
        _, machine, _ = self.run_units(units, Machine(seconds=0.05), cpus=4.0)
        self.assertEqual(max(machine.beside.values()), 2)
        units = [runner.Unit(f"memory-{n}", ["true"], cpus=0.5, memory=1500) for n in range(6)]
        _, machine, _ = self.run_units(units, Machine(seconds=0.05), memory=4096)
        self.assertEqual(max(machine.beside.values()), 2)
        # One larger than the machine still runs, by itself.
        outcomes, machine, _ = self.run_units([runner.Unit("large", ["true"], cpus=64, memory=1 << 20), runner.Unit("small", ["true"])],
                                              Machine(seconds=0.05))
        self.assertEqual(outcomes, {"large": "pass", "small": "pass"})
        self.assertEqual(machine.beside["large"], 1)

    def test_a_unit_starts_beside_others_only_while_the_machine_has_its_memory_to_give(self):
        units = lambda: [runner.Unit(f"memory-{n}", ["true"], cpus=0.5, memory=1000) for n in range(4)]  # noqa: E731
        # What else the machine runs left less than a unit counts for: one at a time, and all of them still.
        outcomes, machine, _ = self.run_units(units(), Machine(seconds=0.03), spare=lambda: 900)
        self.assertEqual((set(outcomes.values()), max(machine.beside.values())), ({"pass"}, 1))
        # It comes back while the first runs (after two looks at the three waiting): the others start then, not at its end.
        given = iter([900] * 6)
        _, machine, _ = self.run_units(units(), Machine(seconds=0.3), spare=lambda: next(given, 4096))
        self.assertEqual(max(machine.beside.values()), 4)
        _, machine, _ = self.run_units(units(), Machine(seconds=0.03), spare=lambda: 4096)
        self.assertEqual(max(machine.beside.values()), 4)

    def test_a_unit_starts_beside_others_only_on_threads_nothing_else_keeps_busy(self):
        units = lambda: [runner.Unit(f"threads-{n}", ["true"]) for n in range(8)]  # noqa: E731
        idle = Working(0.0, seconds=0.2)
        self.run_units(units(), idle, worked=idle.worked)
        self.assertEqual([idle.beside[f"threads-{n}"] for n in range(8)], [4] * 8)
        # Most of its four threads busy with something else: the run keeps to the one left, once it has looked.
        shared = Working(3.5, seconds=0.2)
        outcomes, _, _ = self.run_units(units(), shared, worked=shared.worked)
        self.assertEqual(set(outcomes.values()), {"pass"})
        self.assertEqual([shared.beside[f"threads-{n}"] for n in range(4, 8)], [1] * 4)

    def test_a_unit_that_measures_time_runs_after_the_others_with_nothing_beside_it(self):
        units = [runner.Unit("timed", ["true"], alone=True), runner.Unit("build", ["true"]),
                 runner.Unit("tests", ["true"], needs=("build",)), runner.Unit("timed-too", ["true"], alone=True, needs=("build",))]
        outcomes, machine, _ = self.run_units(units, Machine(seconds=0.03))
        self.assertEqual(set(outcomes.values()), {"pass"})
        self.assertEqual(machine.ran[:2], ["build", "tests"])
        self.assertEqual((machine.beside["timed"], machine.beside["timed-too"]), (1, 1))

    def test_a_unit_counts_for_the_memory_it_held_the_last_time(self):
        took = {"seconds": 9.0, "work": 9.0, "held": 4000, "threads": 1}
        self.records.update({"heavy": dict(took), "build": dict(took), "check": dict(took)})
        units = [runner.Unit("heavy", ["true"], memory=100, tests=True), runner.Unit("light", ["true"], memory=100),
                 runner.Unit("build", ["true"], memory=100)]
        _, machine, run = self.run_units(units, Machine(seconds=0.05))
        self.assertEqual((machine.beside["heavy"], machine.beside["build"]), (1, 2))
        # A test binary with twice the threads counts for twice the memory; with fewer, for no less.
        self.assertEqual(run.memory(runner.Unit("heavy", ["true"], cpus=2, tests=True)), 200)
        self.records["heavy"]["threads"] = 4
        self.assertEqual(run.memory(runner.Unit("heavy", ["true"], cpus=2, tests=True)), 100)
        # A check that reads its inputs counts for what it held; a build, and one never run here, as they say.
        self.assertEqual(run.memory(runner.Unit("check", ["true"], cpus=2, memory=512, reads=lambda: ["inputs"])), 4000)
        self.assertEqual(run.memory(runner.Unit("build", ["true"], memory=512)), 512)
        self.assertEqual(run.memory(runner.Unit("new", ["true"], memory=512, reads=lambda: ["inputs"])), 512)

    def test_a_unit_holds_what_the_processes_it_started_hold_together(self):
        holding = "import sys, time; held = bytearray(48 << 20); print('held', flush=True); time.sleep(float(sys.argv[1]))"
        with subprocess.Popen([sys.executable, "-c", f"import subprocess, sys; subprocess.run([sys.executable, '-c', {holding!r}, '30'])"],
                              stdout=subprocess.PIPE, text=True) as parent:
            try:
                self.assertEqual(parent.stdout.readline(), "held\n")
                held = runner.trees([parent.pid, os.getpid()])
                under = runner.trees([os.getpid()])[os.getpid()]
            finally:
                parent.kill()
        self.assertGreaterEqual(held[parent.pid], 48)
        self.assertLess(held[parent.pid], 48 + 64)
        # A process is counted once, under the nearest of those asked about.
        self.assertGreaterEqual(under - held[os.getpid()], 48)
        # What a unit held is what the watch saw of it and the processes under it.
        log = self.root / "unit.log"
        ran = runner.execute(runner.Unit("unit", [sys.executable, "-c", f"import subprocess, sys; subprocess.run([sys.executable, '-c', {holding!r}, '5'])"]), log)
        self.assertTrue(ran.passed)
        self.assertGreaterEqual(ran.held, 48)

    def test_a_unit_counts_for_the_threads_its_last_run_kept_busy(self):
        records = {"browser.slice": {"seconds": 190.0, "work": 57.0}, "arm64": {"seconds": 240.0, "work": 600.0}, "brief": {"seconds": 0.4, "work": 2.0}}
        self.assertEqual(runner.kept(records, "browser.slice", 2), 1.0)
        self.assertEqual(runner.kept(records, "arm64", 8), 2.5)
        self.assertEqual((runner.kept(records, "brief", 2), runner.kept(records, "new", 3)), (2, 3))

    def test_a_unit_waiting_for_room_is_overtaken_only_by_what_does_not_delay_it(self):
        units = lambda: [runner.Unit("first", ["true"], cpus=2), runner.Unit("wide", ["true"], cpus=4),  # noqa: E731
                         runner.Unit("long", ["true"], cpus=2), runner.Unit("short", ["true"], cpus=2)]
        each = {"first": 0.3, "wide": 0.05, "long": 0.6, "short": 0.05}
        # None has run before: each waits its turn behind the one that needs the whole machine.
        _, machine, _ = self.run_units(units(), Machine(each=each))
        self.assertEqual(machine.ran, ["first", "wide", "long", "short"])
        # Known times: the short one is done before the wide one's room comes, the long one is not.
        _, machine, _ = self.run_units(units(), Machine(each=each))
        self.assertEqual(machine.ran, ["first", "short", "wide", "long"])
        self.assertEqual(machine.beside["wide"], 1)
        # What is to spare beside the waiting unit is taken too.
        units = [runner.Unit("first", ["true"], cpus=3), runner.Unit("wide", ["true"], cpus=3), runner.Unit("thin", ["true"], cpus=1),
                 runner.Unit("thick", ["true"], cpus=2)]
        self.records.clear()
        _, machine, _ = self.run_units(units, Machine(each={"first": 0.2, "thin": 0.4, "thick": 0.05}))
        self.assertEqual(machine.ran, ["first", "thin", "wide", "thick"])

    def test_test_binaries_share_the_threads_by_their_work(self):
        work = {"render": 2000.0, "text": 1500.0, "walk": 100.0, "route": 50.0, "unit": 0.0}
        threads = runner.shares(work, 8)
        self.assertEqual(threads, {"render": 4, "text": 3, "walk": 1, "route": 1, "unit": 1})
        # Every binary ends within the same time: the large ones on their threads, the small ones one after another on the rest.
        within = max(work["render"] / threads["render"], work["text"] / threads["text"])
        self.assertLessEqual(threads["render"] + threads["text"] + (work["walk"] + work["route"]) / within, 8)
        self.assertEqual(runner.shares({"only": 0.0}, 8), {"only": 1})
        self.assertEqual(runner.shares({"only": 900.0}, 8), {"only": 8})
        self.assertEqual(runner.shares({}, 8), {})
        self.assertEqual(set(runner.shares(dict.fromkeys("abcdefghij", 10.0), 4).values()), {1})

    def test_a_test_binary_runs_on_its_threads_and_its_test_count_is_kept(self):
        log = self.root / "binary.log"
        unit = runner.Unit("binary", ["sh", "-c", "echo; echo running 3 tests; echo \"threads $RUST_TEST_THREADS\"; echo running 1 test"],
                           cpus=3, tests=True)
        outcomes, _, _ = self.run_units([unit], type("Real", (), {"execute": staticmethod(runner.execute)})())
        self.assertEqual(outcomes, {"binary": "pass"})
        self.assertIn("threads 3\n", log.read_text())
        self.assertEqual((self.records["binary"]["count"], self.records["binary"]["threads"]), (4, 3))

    def test_a_pass_stands_while_what_the_unit_reads_is_the_same(self):
        read = ["first"]
        made = self.root / "made"
        made.touch()
        units = lambda: [runner.Unit("tests", ["true"], reads=lambda: list(read), makes=(made,)), runner.Unit("build", ["true"])]  # noqa: E731
        outcomes, machine, _ = self.run_units(units())
        self.assertEqual((outcomes, sorted(machine.ran)), ({"tests": "pass", "build": "pass"}, ["build", "tests"]))
        # The same inputs: the pass stands, and a build runs every time.
        outcomes, machine, run = self.run_units(units())
        self.assertEqual((outcomes, machine.ran), ({"tests": "same", "build": "pass"}, ["build"]))
        self.assertIn("as passed", run.results["tests"].note)
        # --force, another input, another command and a missing output each run it again.
        self.assertEqual(self.run_units(units(), force=True)[0]["tests"], "pass")
        read[0] = "second"
        self.assertEqual(self.run_units(units())[0]["tests"], "pass")
        self.assertEqual(self.run_units(units())[0]["tests"], "same")
        changed = [runner.Unit("tests", ["true", "--more"], reads=lambda: list(read), makes=(made,))]
        self.assertEqual(self.run_units(changed)[0]["tests"], "pass")
        self.assertEqual(self.run_units(units())[0]["tests"], "pass")
        made.unlink()
        self.assertEqual(self.run_units(units())[0]["tests"], "pass")
        made.touch()
        # A failure never stands: the unit runs again with the same inputs.
        self.assertEqual(self.run_units(units(), Machine(failing={"tests"}))[0]["tests"], "same")
        self.assertEqual(self.run_units(units(), Machine(failing={"tests"}), force=True)[0]["tests"], "FAIL")
        outcomes, machine, _ = self.run_units(units())
        self.assertEqual((outcomes["tests"], "tests" in machine.ran), ("pass", True))

    def test_a_builds_units_join_the_run_once_it_passed(self):
        built = lambda: [runner.Unit("binary-a", ["true"], needs=("build",)), runner.Unit("binary-b", ["true"], needs=("build",))]  # noqa: E731
        outcomes, machine, _ = self.run_units([runner.Unit("build", ["true"], then=built), runner.Unit("timed", ["true"], alone=True)])
        self.assertEqual(outcomes, {"build": "pass", "binary-a": "pass", "binary-b": "pass", "timed": "pass"})
        self.assertEqual(machine.ran[-1], "timed")
        outcomes, _, _ = self.run_units([runner.Unit("build", ["true"], then=built)], Machine(failing={"build"}))
        self.assertEqual(outcomes, {"build": "FAIL"})

    def test_a_command_runs_into_its_log_with_its_result_and_what_it_took(self):
        log = self.root / "unit.log"
        ran = runner.execute(runner.Unit("unit", ["sh", "-c", "echo out; echo err >&2; test \"$SUITE_TEST\" = set"],
                                         cwd=self.root, env={"SUITE_TEST": "set"}), log)
        self.assertTrue(ran.passed)
        self.assertEqual(log.read_text(), "out\nerr\n")
        self.assertFalse(runner.execute(runner.Unit("unit", ["false"]), log).passed)
        self.assertFalse(runner.execute(runner.Unit("unit", ["/nonexistent/program"]), log).passed)

    def test_a_tree_is_its_files_contents_staged_or_not_and_leaves_out_what_git_ignores(self):
        git = lambda *args: subprocess.check_output(["git", *args], cwd=self.root, text=True)  # noqa: E731
        git("init", "--quiet", "--initial-branch=master")
        (self.root / ".gitignore").write_text("/build/\n")
        (self.root / "src").mkdir()
        (self.root / "src/code.rs").write_text("original\n")
        (self.root / "src/data.json").write_text("{}\n")
        commit = lambda: (git("add", "."), git("-c", "user.name=Suite test", "-c", "user.email=suite@example.invalid",  # noqa: E731
                                               "commit", "--quiet", "--no-gpg-sign", "-m", "Inputs"))
        commit()

        def tree(*specs):
            runner._trees.clear()
            return runner.tree(*specs, root=self.root)

        clean, whole = tree("src"), tree(".")
        (self.root / "build").mkdir()
        (self.root / "build/output").write_text("ignored\n")
        self.assertEqual((tree("src"), tree(".")), (clean, whole))
        # A change counts the same unstaged, staged and committed.
        (self.root / "src/code.rs").write_text("changed\n")
        changed = tree("src")
        self.assertNotEqual(changed, clean)
        git("add", "src/code.rs")
        self.assertEqual(tree("src"), changed)
        commit()
        self.assertEqual(tree("src"), changed)
        # Untracked and deleted files count; a path left out does not.
        data = tree("src", ":(exclude)*.rs")
        (self.root / "src/code.rs").write_text("changed again\n")
        self.assertEqual(tree("src", ":(exclude)*.rs"), data)
        (self.root / "src/new.json").write_text("[]\n")
        added = tree("src", ":(exclude)*.rs")
        self.assertNotEqual(added, data)
        (self.root / "src/new.json").unlink()
        (self.root / "src/data.json").unlink()
        self.assertNotIn(tree("src", ":(exclude)*.rs"), (data, added))

    def test_a_files_digest_follows_its_contents(self):
        file = self.root / "binary"
        file.write_bytes(b"one")
        first = runner.file_digest(file)
        self.assertEqual(runner.file_digest(file), first)
        file.write_bytes(b"two!")
        self.assertNotEqual(runner.file_digest(file), first)
        self.assertEqual(runner.file_digest(self.root / "absent"), "absent")
        (self.root / "out").mkdir()
        (self.root / "out/a").write_bytes(b"a")
        before = runner.directory_digest(self.root / "out")
        (self.root / "out/b").write_bytes(b"b")
        self.assertNotEqual(runner.directory_digest(self.root / "out"), before)

    def test_a_run_keeps_a_thread_a_core_busy_unless_told_how_many(self):
        allowed = sorted(os.sched_getaffinity(0))
        self.assertEqual(sorted(cpu for core in runner.cores() for cpu in core), allowed)
        # A thread of every core comes before any core's second.
        order = runner.processors()
        self.assertEqual(sorted(order), allowed)
        self.assertEqual(sorted(order[: len(runner.cores())]), sorted(core[0] for core in runner.cores()))
        self.assertEqual(runner.Machine.this().cpus, len(runner.cores()))
        self.assertEqual(runner.Machine.this(1).cpus, 1)
        self.assertEqual(runner.Machine.this(1 << 20).cpus, len(allowed))
        self.assertGreaterEqual(runner.Machine.this().memory, 2048)
        # Builds leave two of the threads to what is already built, on any machine.
        for threads in (1, 2, 8, 16):
            units = {unit.name: unit for unit in runner.plan("full", runner.Machine(float(threads), 8192), {})}
            self.assertEqual(units["hosts"].cpus, max(threads - 2, 1))
            self.assertEqual(units["hosts"].env["CARGO_BUILD_JOBS"], str(max(threads - 2, 1)))
            self.assertIn(str(max(threads - 2, 1)), units["module"].command)
            self.assertLessEqual(max(unit.cpus for unit in units.values()), max(threads, 3))

    def test_the_hosts_graphics_card_comes_before_the_software_rasterizer_where_there_is_one(self):
        drivers = self.root / "drivers"
        drivers.mkdir()
        (drivers / "libGLX_card.so.0").write_bytes(b"driver")
        host = self.root / "gpu-host"
        host.write_text(f"#!/bin/sh\nexport VK_ICD_FILENAMES=/card.json:/software.json VK_DRIVER_FILES=/card.json:/software.json\n"
                        f"export LD_LIBRARY_PATH='{drivers}:/shell/lib'\nexec \"$@\"\n")
        host.chmod(0o755)
        env, driver = runner.card(host)
        self.assertEqual(env, {"VK_ICD_FILENAMES": "/card.json:/software.json", "VK_DRIVER_FILES": "/card.json:/software.json",
                               "LD_LIBRARY_PATH": f"{drivers}:/shell/lib"})
        # Another driver is another result.
        (drivers / "libGLX_card.so.0").write_bytes(b"another driver")
        self.assertNotEqual(runner.card(host)[1], driver)
        # No card (the wrapper refuses): the software rasterizer alone.
        host.write_text("#!/bin/sh\nexit 1\n")
        self.assertEqual(runner.card(host), (runner.LAVAPIPE, None))
        self.assertEqual(runner.card(self.root / "absent"), (runner.LAVAPIPE, None))

    def test_every_tier_plans_units_whose_needs_are_in_it(self):
        machine = runner.Machine(16.0, 8192)
        for tier in ["desktop", "web", "android", "xr", "full"]:
            with self.subTest(tier=tier):
                units = runner.plan(tier, machine, {})
                names = [unit.name for unit in units]
                self.assertEqual(len(names), len(set(names)))
                for unit in units:
                    self.assertLessEqual(set(unit.needs), set(names), unit.name)
        full = {unit.name for unit in runner.plan("full", machine, {})}
        for tier in ["desktop", "web", "android", "xr"]:
            self.assertLessEqual({unit.name for unit in runner.plan(tier, machine, {})}, full)


if __name__ == "__main__":
    unittest.main()
