"""Release versions, immutable asset sets and deterministic portable archives."""
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.release.package import zip_tree
from tools.release.publish import expected
from tools.release.version import android_code, next_tag, version


class ReleaseTests(unittest.TestCase):
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
