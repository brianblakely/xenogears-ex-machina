"""Synthetic checks for the host-wide caps on captures, builds and comparisons."""

from __future__ import annotations

import fcntl
import tempfile
import unittest
from pathlib import Path

from tools.reference.host_slots import LIMITS, ROOT, lock_directory, slot


def taken(path: Path) -> bool:
    with path.open("a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
        fcntl.flock(handle, fcntl.LOCK_UN)
        return False


class HostSlotTests(unittest.TestCase):
    def test_limits_serialize_captures_and_builds(self) -> None:
        self.assertEqual(LIMITS["capture"], 1)
        self.assertEqual(LIMITS["build"], 1)
        self.assertEqual(LIMITS["compare"], 2)

    def test_holders_take_distinct_slots_and_release_them(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, second = root / "compare-0.lock", root / "compare-1.lock"
            with slot("compare", root):
                self.assertTrue(taken(first))
                self.assertFalse(taken(second))
                with slot("compare", root):
                    self.assertTrue(taken(second))
            self.assertFalse(taken(first) or taken(second))

    def test_locks_are_shared_by_every_worktree(self) -> None:
        directory = lock_directory(ROOT)
        self.assertEqual(directory.name, "xem-locks")
        self.assertEqual(directory, lock_directory(ROOT / "tools"))


if __name__ == "__main__":
    unittest.main()
