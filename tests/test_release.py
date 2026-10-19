"""Release versions, immutable asset sets and deterministic portable archives."""
import json
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from tools.release.package import zip_tree
from tools.release.publish import expected, main, previous_release, release_notes, render_notes
from tools.release.version import android_code, next_tag, version


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


if __name__ == "__main__":
    unittest.main()
