"""Verify/extract a user-supplied CHD into a new ignored local evidence directory."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

from inspect_disc import inspect


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("chd", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if not args.chd.is_file():
        parser.error("CHD must be an existing regular file")
    if not args.output.resolve().is_relative_to(Path(".local").resolve()):
        parser.error("Output must be under ignored .local/; source bytes remain private")
    args.output.mkdir(parents=True, exist_ok=False)
    cue, raw = args.output / "disc.cue", args.output / "disc.bin"
    operations = {
        "info": ["info", "-i", str(args.chd)],
        "verify": ["verify", "-i", str(args.chd)],
        "extract": ["extractcd", "-i", str(args.chd), "-o", str(cue), "-ob", str(raw)],
    }
    for label, command in operations.items():
        print(f"chdman {label}: {args.chd.name}", flush=True)
        with (args.output / f"chdman-{label}.log").open("x") as log:
            subprocess.run(["chdman", *command], stdout=log, stderr=log, check=True)
    report = inspect(args.chd, raw, cue)
    report["container_verification"] = {
        "command": "chdman verify -i <source.chd>",
        "exit_code": 0,
        "log": "chdman-verify.log",
    }
    (args.output / "measurement.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Verified and measured: {args.output / 'measurement.json'}", flush=True)


if __name__ == "__main__":
    main()
