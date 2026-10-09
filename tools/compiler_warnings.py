#!/usr/bin/env python3
"""Count the distinct compiler warnings of a decomp build log.

A unit's cc1 (and cpp) report each warning as `PATH:LINE: warning: TEXT`; a
header's warning repeats in every unit that includes it, and the two resident
targets compile the same sources, so a warning counts once per distinct
(path, line, text). Paths are made relative to --root (default: the current
directory). The summary groups the warnings by kind (the text with quoted
names and argument numbers abstracted) and, for warnings about a call's
argument, by callee and argument. Build every unit first, for instance with a
forced rebuild that keeps each recipe's output together:

    make -B -j8 -O -C decomp all-verify > .local/build.log 2>&1
    python3 tools/compiler_warnings.py .local/build.log
"""

from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

WARNING = re.compile(r"^(?P<path>[^:\s][^:]*):(?P<line>\d+): warning: (?P<text>.*)$")
ARGUMENT = re.compile(r"\barg (?P<arg>\d+) of `(?P<callee>[^']+)'")
QUOTED = re.compile(r"`[^']*'")


def parse(lines, root: str) -> set[tuple[str, int, str]]:
    prefix = root.rstrip("/") + "/"
    found = set()
    for line in lines:
        match = WARNING.match(line.rstrip("\n"))
        if not match:
            continue
        path = match["path"]
        if path.startswith(prefix):
            path = path[len(prefix):]
        found.add((path, int(match["line"]), match["text"]))
    return found


def kind(text: str) -> str:
    return QUOTED.sub("`_'", re.sub(r"\barg \d+\b", "arg N", text))


def ranked(counter: collections.Counter) -> dict:
    return dict(sorted(counter.items(), key=lambda item: (-item[1], item[0])))


def summary(warnings: set[tuple[str, int, str]]) -> dict:
    kinds = collections.Counter(kind(text) for _, _, text in warnings)
    callees = collections.Counter()
    arguments = collections.Counter()
    for _, _, text in warnings:
        match = ARGUMENT.search(text)
        if match:
            callees[match["callee"]] += 1
            arguments[f"{match['callee']} arg {match['arg']}"] += 1
    return {
        "unique": len(warnings),
        "kinds": ranked(kinds),
        "callees": ranked(callees),
        "arguments": ranked(arguments),
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("logs", nargs="*", type=Path, help="build logs (default: stdin)")
    parser.add_argument("--root", default=str(Path.cwd()), help="path prefix to strip")
    parser.add_argument("--list", action="store_true", help="print each distinct warning")
    args = parser.parse_args(argv)
    warnings = set()
    if args.logs:
        for log in args.logs:
            with log.open(errors="replace") as handle:
                warnings |= parse(handle, args.root)
    else:
        warnings = parse(sys.stdin, args.root)
    if args.list:
        for path, line, text in sorted(warnings):
            print(f"{path}:{line}: warning: {text}")
    else:
        print(json.dumps(summary(warnings), indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
