"""Exercise the public baseline command contract; no original gameplay claimed."""

from __future__ import annotations

import subprocess
import sys


def main() -> None:
    binary = sys.argv[1]
    cases = [
        ([], 0, "No game runtime is implemented yet.", ""),
        (["--help"], 0, "Usage: xem-baseline", ""),
        (["--version"], 0, "Xenogears Ex Machina 0.0.0\n", ""),
        (["--not-supported"], 2, "", "Unsupported argument."),
        (["--version", "extra"], 2, "", "Unsupported argument."),
    ]
    for arguments, code, stdout, stderr in cases:
        result = subprocess.run([binary, *arguments], capture_output=True, text=True, check=False)
        if result.returncode != code or stdout not in result.stdout or stderr not in result.stderr:
            raise AssertionError((arguments, result))
        if not stdout and result.stdout:
            raise AssertionError(f"Unexpected stdout for {arguments}: {result.stdout!r}")
        if not stderr and result.stderr:
            raise AssertionError(f"Unexpected stderr for {arguments}: {result.stderr!r}")
    print("Baseline CLI success/error paths passed")


if __name__ == "__main__":
    main()
