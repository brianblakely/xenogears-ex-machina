"""Run the reproducible public Phase 0 build/check gate from the pinned Nix shell."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=["debug", "sanitize", "release", "all"], default="all")
    args = parser.parse_args()
    if not os.environ.get("IN_NIX_SHELL"):
        parser.error("Enter the pinned Nix shell first; see docs/development.md")
    commands = [
        [sys.executable, "tools/repository/matrix.py", "--check"],
        [sys.executable, "tools/repository/validate.py"],
        [sys.executable, "tools/repository/format.py", "--check"],
    ]
    presets = ["debug", "sanitize", "release"] if args.preset == "all" else [args.preset]
    for preset in presets:
        commands += [
            ["cmake", "--preset", preset],
            ["cmake", "--build", "--preset", preset],
            ["ctest", "--preset", preset],
        ]
    results = []
    for command in commands:
        print("Running: " + " ".join(command), flush=True)
        result = subprocess.run(command, cwd=ROOT, check=False)
        results.append({"command": command, "exit_code": result.returncode})
        if result.returncode:
            raise SystemExit(result.returncode)
    output = ROOT / ".local/verification/public-check.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"schema_version": 1, "commands": results}, indent=2) + "\n")
    print(f"Public Phase 0 checks passed; local command record: {output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
