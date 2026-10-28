"""Release versions, immutable asset sets and deterministic portable archives."""
import json
import os
import subprocess
import tempfile
import unittest
import zipfile
from contextlib import chdir
from pathlib import Path
from unittest.mock import patch

from tools.release.package import zip_tree
from tools.release.publish import (
    current_native_release,
    expected,
    main,
    previous_release,
    release_notes,
    render_notes,
)
from tools.release.version import android_code, native_changed, next_tag, version


class ReleaseTests(unittest.TestCase):
    def test_changelog_uses_the_latest_earlier_published_ancestor_across_pages(self):
        def release(tag, **flags):
            return {"tag_name": tag, "draft": False, "prerelease": False, **flags}

        pages = [
            [release("v0.5.5"), release("v0.5.4"), release("v0.5.3")],
            [release("v0.5.2", draft=True), release("v0.5.1", prerelease=True), release("v0.5")],
        ]
        with patch("tools.release.publish.subprocess.check_output", return_value=json.dumps(pages)), \
             patch("tools.release.publish.subprocess.run") as run:
            run.side_effect = [subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 0)]
            self.assertEqual(previous_release("owner/game", "v0.5.4", "release-commit"), "v0.5")
            self.assertEqual([call.args[0] for call in run.call_args_list], [
                ["git", "merge-base", "--is-ancestor", "v0.5.3", "release-commit"],
                ["git", "merge-base", "--is-ancestor", "v0.5", "release-commit"],
            ])

    def test_missing_release_history_fails_instead_of_repeating_old_changes(self):
        pages = [[{"tag_name": "v0.5", "draft": False, "prerelease": False}]]
        with patch("tools.release.publish.subprocess.check_output", return_value=json.dumps(pages)), \
             patch("tools.release.publish.subprocess.run", return_value=subprocess.CompletedProcess([], 128)):
            with self.assertRaises(subprocess.CalledProcessError):
                previous_release("owner/game", "v0.5.1", "release-commit")

    def test_docs_merged_during_build_do_not_stop_the_current_app_becoming_latest(self):
        with patch("tools.release.publish.subprocess.check_output",
                   return_value=json.dumps({"object": {"sha": "docs-merge"}})), \
             patch("tools.release.publish.subprocess.run", return_value=subprocess.CompletedProcess([], 0)) as run, \
             patch("tools.release.publish.native_changed", return_value=False) as changed, \
             patch("tools.release.publish.published_releases", return_value=[((1, 0, 0), "v1.0")]):
            self.assertTrue(current_native_release("owner/game", "v1.0.1", "app-commit"))
            changed.assert_called_once_with("app-commit", "docs-merge")
            self.assertEqual([call.args[0] for call in run.call_args_list], [
                ["git", "fetch", "--no-tags", "origin", "docs-merge"],
                ["git", "merge-base", "--is-ancestor", "app-commit", "docs-merge"],
            ])

    def test_superseded_or_unrelated_build_cannot_become_latest(self):
        for changed, ancestor, published in [
            (True, 0, []),
            (False, 1, []),
            (False, 0, [((1, 0, 2), "v1.0.2")]),
        ]:
            with self.subTest(changed=changed, ancestor=ancestor, published=published), \
                 patch("tools.release.publish.subprocess.check_output",
                       return_value=json.dumps({"object": {"sha": "new-head"}})), \
                 patch("tools.release.publish.subprocess.run", return_value=subprocess.CompletedProcess([], ancestor)), \
                 patch("tools.release.publish.native_changed", return_value=changed), \
                 patch("tools.release.publish.published_releases", return_value=published):
                self.assertFalse(current_native_release("owner/game", "v1.0.1", "app-commit"))

    def test_first_release_at_master_can_become_latest_without_fetching(self):
        with patch("tools.release.publish.subprocess.check_output",
                   return_value=json.dumps({"object": {"sha": "app-commit"}})), \
             patch("tools.release.publish.subprocess.run") as run, \
             patch("tools.release.publish.published_releases", return_value=[]):
            self.assertTrue(current_native_release("owner/game", "v1.0", "app-commit"))
            run.assert_not_called()

    def test_changelog_reads_only_new_entries_from_the_exact_release_commit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root, text=True).strip()

            def commit():
                git("add", ".")
                git("-c", "user.name=Release test", "-c", "user.email=release@example.invalid",
                    "commit", "--quiet", "--no-gpg-sign", "-m", "Internal implementation details")
                return git("rev-parse", "HEAD")

            git("init", "--quiet")
            changes = root / "docs/changes"
            changes.mkdir(parents=True)
            (changes / "old.md").write_text("- An earlier improvement.\n")
            baseline = commit()
            git("tag", "v0.5")
            (changes / "new.md").write_text("- Your saves now reopen correctly.\n")
            released = commit()
            (changes / "new.md").write_text("- Unreleased edits.\n")
            (changes / "future.md").write_text("- A future feature.\n")
            check_output = subprocess.check_output

            def in_repository(command, **kwargs):
                return check_output(command, cwd=root, **kwargs)

            with patch("tools.release.publish.previous_release", return_value="v0.5"), \
                 patch("tools.release.publish.subprocess.check_output", side_effect=in_repository):
                notes = release_notes("owner/game", "v0.5.1", released)
                self.assertIn("- Your saves now reopen correctly.", notes)
                for omitted in ["An earlier improvement", "Unreleased edits", "A future feature",
                                "Internal implementation details"]:
                    self.assertNotIn(omitted, notes)
                self.assertIn("https://github.com/owner/game/compare/v0.5...v0.5.1", notes)
                unchanged = release_notes("owner/game", "v0.5.1", baseline)
                self.assertIn("No player-facing changes were noted", unchanged)

            with patch("tools.release.publish.previous_release", return_value=None), \
                 patch("tools.release.publish.subprocess.check_output", side_effect=in_repository):
                first = release_notes("owner/game", "v0.5", baseline)
                self.assertIn("Welcome to our first release!", first)
                self.assertIn("- An earlier improvement.", first)
                self.assertNotIn("compare/", first)

    def test_installation_links_follow_the_release_instead_of_v05(self):
        notes = render_notes("owner/game", "v0.6.2", "v0.6.1", ["- Enjoy the new camera controls."])
        for suffix in ["windows-x86_64.zip", "macos-aarch64.zip", "macos-x86_64.zip",
                       "linux-x86_64.AppImage", "linux-aarch64.AppImage", "android-horizon.apk"]:
            self.assertIn(f"https://github.com/owner/game/releases/download/v0.6.2/"
                          f"xenogears-ex-machina-v0.6.2-{suffix}", notes)
        self.assertIn("https://owner.github.io/game/", notes)
        self.assertIn("https://github.com/owner/game/blob/v0.6.2/docs/releases.md", notes)
        self.assertIn("https://github.com/owner/game/blob/v0.6.2/MANUAL.md", notes)
        self.assertIn("- Enjoy the new camera controls.", notes)
        self.assertNotIn("v0.5", notes)

    def test_a_public_release_retry_preserves_its_existing_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            artifacts = root / "release-test"
            artifacts.mkdir()
            for name in expected("v0.5.1"):
                (artifacts / name).write_text("test artifact")

            def gh(command, **kwargs):
                if command[:3] == ["gh", "release", "view"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"isDraft": false}')
                self.assertEqual(command[:3], ["gh", "release", "download"])
                (root / "previous/SHA256SUMS").write_text((root / "SHA256SUMS").read_text())
                return subprocess.CompletedProcess(command, 0)

            with patch("sys.argv", ["publish", "--tag", "v0.5.1", "--commit", "released",
                                    "--repository", "owner/game", "--artifacts", str(root)]), \
                 patch("tools.release.publish.subprocess.run", side_effect=gh), \
                 patch("tools.release.publish.release_notes") as generate:
                main()
                generate.assert_not_called()
                self.assertFalse((root / "notes.md").exists())

    def test_first_release_and_monotonic_patch_versions(self):
        self.assertEqual(next_tag([]), "v0.5")
        self.assertEqual(next_tag(["v0.5", "unrelated", "v0.5-rc1"]), "v0.5.1")
        self.assertEqual(next_tag(["v0.5.9", "v0.5.10"]), "v0.5.11")
        self.assertEqual(next_tag(["v1.0", "v0.5.99"]), "v1.0.1")
        self.assertIsNone(version("v0.5.1-extra"))
        self.assertLess(android_code("v0.5"), android_code("v0.5.1"))
        self.assertLess(android_code("v0.5.9999"), android_code("v0.6"))
        with self.assertRaises(ValueError):
            android_code("v0.5.10000")

    def test_release_requires_every_platform_and_corresponding_source(self):
        names = expected("v0.5")
        self.assertEqual(len(names), 14)
        for suffix in ["linux-x86_64.AppImage", "linux-aarch64.AppImage", "windows-x86_64.zip",
                       "macos-x86_64.zip", "macos-aarch64.zip", "android-horizon.apk", "web.zip", "source.tar.gz"]:
            self.assertIn("xenogears-ex-machina-v0.5-" + suffix, names)

    def test_archive_has_stable_bytes_and_preserves_executables(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            staged = root / "stage"
            staged.mkdir()
            binary = staged / "executable"
            binary.write_bytes(b"authored test payload")
            binary.chmod(0o755)
            zip_tree(staged, root / "one.zip")
            binary.touch()
            zip_tree(staged, root / "two.zip")
            self.assertEqual((root / "one.zip").read_bytes(), (root / "two.zip").read_bytes())
            with zipfile.ZipFile(root / "one.zip") as archive:
                self.assertEqual(archive.getinfo("executable").external_attr >> 16 & 0o777, 0o755)
            (staged / "link").symlink_to(binary)
            with self.assertRaisesRegex(ValueError, "symlink"):
                zip_tree(staged, root / "bad.zip")


class NativeReleaseGuardTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="native release ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.enterContext(chdir(self.root))
        self.git("init", "--quiet", "--initial-branch=master")
        self.write("src/runtime/crates/xem-host/src/lib.rs", "original\n")
        self.base = self.commit()

    def git(self, *args):
        # The guard and these helpers use the same temporary repository.
        return subprocess.check_output(["git", *args], cwd=self.root, text=True).strip()

    def write(self, name, text="change\n"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def commit(self):
        self.git("add", ".")
        self.git("-c", "user.name=Release test", "-c", "user.email=release@example.invalid",
                 "commit", "--quiet", "--no-gpg-sign", "-m", "Test inputs")
        return self.git("rev-parse", "HEAD")

    def test_docs_tests_browser_and_publishing_do_not_release(self):
        self.assertFalse(native_changed(self.base, self.base))
        for path in ["MANUAL.md", "docs/changes/manual.md", ".github/workflows/release.yml",
                     "tools/release/notes.md", "tools/release/publish.py", "tools/release/version.py",
                     "tools/release/smoke.py", "packaging/source-files.txt", "packaging/install.txt",
                     "tests/test_release.py", "src/runtime/crates/xem-host/tests/states.rs",
                     "src/runtime/crates/xem-ui/examples/panel.rs", "src/runtime/scripts/suite.sh",
                     "src/runtime/crates/xem-core/font/README.md", "src/decomp/README.md",
                     "src/runtime/web/build.py", "src/runtime/web/index.html",
                     "src/runtime/crates/xem-web/src/lib.rs", "src/runtime/crates/xem-webxr/src/lib.rs"]:
            self.write(path)
        self.assertFalse(native_changed(self.base, self.commit()))

    def test_native_sources_and_build_inputs_release(self):
        for path in ["src/decomp/src/resident/main.c", "src/decomp/include/common.h",
                     "src/port/symbols.json", "src/port/main.c", "tools/game_module.py", "tools/game_schema.py",
                     "src/runtime/crates/xem-host/src/lib.rs", "src/runtime/crates/xem-ui/ui/settings.slint",
                     "src/runtime/crates/xem-gpu/src/game.wgsl", "src/runtime/crates/xem-core/font/substitute.txt",
                     "src/runtime/crates/xem-host/Cargo.toml", "src/runtime/crates/xem-game/build.rs",
                     "src/runtime/crates/xem-android/src/lib.rs", "src/runtime/crates/xem-xr/src/lib.rs",
                     "src/runtime/Cargo.toml", "src/runtime/Cargo.lock", "src/runtime/.cargo/config.toml",
                     "src/runtime/android/app/src/main/AndroidManifest.xml", "src/runtime/scripts/android-build.sh",
                     "nix/runtime/flake.lock", "nix/runtime/freesurround.cpp", "packaging/linux.nix",
                     "tools/release/package.py", "tools/release/shell.sh"]:
            with self.subTest(path=path):
                base = self.git("rev-parse", "HEAD")
                self.write(path)
                self.assertTrue(native_changed(base, self.commit()))

    def test_whole_push_counts_earlier_code_but_not_a_net_revert(self):
        self.write("src/runtime/crates/xem-host/src/lib.rs", "updated\n")
        self.commit()
        self.write("MANUAL.md")
        self.assertTrue(native_changed(self.base, self.commit()))
        self.write("src/runtime/crates/xem-host/src/lib.rs", "original\n")
        self.assertFalse(native_changed(self.base, self.commit()))

    def test_doc_only_merge_uses_master_before_the_push(self):
        self.git("checkout", "--quiet", "-b", "manual")
        self.write("MANUAL.md")
        self.commit()
        self.git("checkout", "--quiet", "master")
        self.git("-c", "user.name=Release test", "-c", "user.email=release@example.invalid",
                 "merge", "--quiet", "--no-ff", "--no-gpg-sign", "-m", "Merge manual", "manual")
        self.assertFalse(native_changed(self.base, "HEAD"))

    def test_removing_or_renaming_native_code_counts(self):
        source = self.root / "src/runtime/crates/xem-host/src/lib.rs"
        source.rename(source.with_name("renamed.rs"))
        renamed = self.commit()
        self.assertTrue(native_changed(self.base, renamed))
        source.with_name("renamed.rs").unlink()
        self.assertTrue(native_changed(renamed, self.commit()))

    def test_initial_push_and_invalid_comparisons(self):
        self.assertTrue(native_changed("0" * 40, self.base))
        with self.assertRaises(subprocess.CalledProcessError):
            native_changed("missing-base", self.base)

    def test_check_only_outputs_skip_without_contacting_github(self):
        from tools.release.version import main as check

        output = self.root / "output"
        with patch("sys.argv", ["version", "--commit", self.base, "--check-native-since", self.base]), \
             patch.dict(os.environ, GITHUB_OUTPUT=str(output)), \
             patch("tools.release.version.gh") as gh:
            check()
        gh.assert_not_called()
        self.assertEqual(output.read_text(), "changed=false\n")


if __name__ == "__main__":
    unittest.main()
