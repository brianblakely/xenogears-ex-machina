"""Use pinned formatters only on explicitly reviewed project source files."""

from __future__ import annotations

import argparse
import subprocess

from source_archive import ROOT, source_files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    paths = [path.relative_to(ROOT).as_posix() for path in source_files()]
    cpp = [path for path in paths if path.endswith((".cpp", ".h", ".hpp", ".hpp.in"))]
    python = [path for path in paths if path.endswith(".py")]
    nix = [path for path in paths if path.endswith(".nix")]
    commands = [
        ["clang-format", *(["--dry-run", "--Werror"] if args.check else ["-i"]), *cpp],
        ["ruff", "format", *(["--check"] if args.check else []), *python],
        ["ruff", "check", *python],
        ["nixfmt", *(["--check"] if args.check else []), *nix],
    ]
    for command in commands:
        subprocess.run(command, cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
