"""Host-wide caps on concurrent heavy local work: captures, builds and comparisons.

Slots are advisory file locks in the repository's git common directory, so every
worktree of the checkout shares them. A process holds its slot until it exits.
"""

from __future__ import annotations

import contextlib
import fcntl
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Concurrent holders allowed per kind of work.
LIMITS = {"capture": 1, "build": 1, "compare": 2}


def lock_directory(root: Path = ROOT) -> Path:
    """The lock directory shared by all worktrees of `root`'s repository."""
    common = subprocess.run(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        cwd=root,
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    return Path(common) / "xem-locks"


@contextlib.contextmanager
def slot(kind: str, directory: Path | None = None, poll_seconds: float = 0.5):
    """Hold one of LIMITS[kind] slots, waiting while all are taken."""
    directory = lock_directory() if directory is None else directory
    directory.mkdir(parents=True, exist_ok=True)
    handles = [(directory / f"{kind}-{i}.lock").open("a") for i in range(LIMITS[kind])]
    try:
        waiting = False
        while True:
            for handle in handles:
                try:
                    fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    continue
                try:
                    yield
                finally:
                    fcntl.flock(handle, fcntl.LOCK_UN)
                return
            if not waiting:
                print(f"Waiting for a free {kind} slot in {directory}", flush=True)
                waiting = True
            time.sleep(poll_seconds)
    finally:
        for handle in handles:
            handle.close()
