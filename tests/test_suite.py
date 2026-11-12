"""Exercise the suite's change guard without starting Nix or any platform tests."""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SUITE = Path(__file__).resolve().parents[1] / "src/runtime/scripts/suite.sh"


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
        for args in [["--base"], ["--base", "--force"], ["desktop", "web"], ["--unknown"]]:
            with self.subTest(args=args):
                self.assertEqual(self.run_suite(*args).returncode, 2)
        self.assertFalse(self.calls.exists())


if __name__ == "__main__":
    unittest.main()
