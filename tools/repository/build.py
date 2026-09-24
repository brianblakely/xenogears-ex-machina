"""Configure, build and optionally test one preset while holding the host build slot."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.reference.host_slots import slot  # noqa: E402


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("preset", choices=["debug", "sanitize", "release"])
    parser.add_argument("--target", action="append", default=[], help="Build only these targets")
    parser.add_argument("--test", metavar="REGEX", help="Then run ctest -R REGEX")
    args = parser.parse_args()
    if not os.environ.get("IN_NIX_SHELL"):
        parser.error("Enter the pinned Nix shell first; see docs/development.md")
    build = ["cmake", "--build", "--preset", args.preset]
    if args.target:
        build += ["--target", *args.target]
    commands = [["cmake", "--preset", args.preset], build]
    if args.test:
        commands.append(["ctest", "--preset", args.preset, "-R", args.test])
    with slot("build"):
        for command in commands:
            if subprocess.run(command, cwd=ROOT, check=False).returncode:
                raise SystemExit(1)


if __name__ == "__main__":
    main()
