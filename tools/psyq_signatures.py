#!/usr/bin/env python3
"""Locate PsyQ SDK library objects in an executable image by their byte signatures.

Uses the pinned lab313ru/psx_psyq_signatures set (nix/ghidra flake input). Each
signature is one library object's complete text with relocated fields masked.
A hit classifies that address range as SDK library code; it identifies the
object and version family, not a Xenogears source file. Output lines are
`start end version library object labels` for review; only unique, complete
object matches are reported, and overlapping candidates from several SDK
versions are all listed so the version remains an explicit choice.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def pattern_of(sig: str) -> re.Pattern[bytes]:
    parts = []
    for token in sig.split():
        parts.append(b"." if token == "??" else re.escape(bytes([int(token, 16)])))
    return re.compile(b"".join(parts), re.DOTALL)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("signatures", type=Path, help="psx_psyq_signatures checkout")
    parser.add_argument("image", type=Path, help="raw image bytes (no PS-X EXE header)")
    parser.add_argument("--vram", type=lambda v: int(v, 0), required=True)
    parser.add_argument("--min-bytes", type=int, default=16)
    args = parser.parse_args()
    data = args.image.read_bytes()

    hits: dict[tuple[int, int], list[str]] = {}
    for version in sorted(p for p in args.signatures.iterdir() if p.is_dir() and p.name.isdigit()):
        for lib in sorted(version.glob("*.json")):
            for obj in json.loads(lib.read_text()):
                sig = obj.get("sig", "").strip()
                if len(sig.split()) < args.min_bytes:
                    continue
                # Trailing zero words are alignment padding inside the archive
                # member, not code; the linked image may place the next object there.
                tokens = sig.split()
                while len(tokens) > 4 and tokens[-4:] == ["00"] * 4:
                    tokens = tokens[:-4]
                found = [m.start() for m in pattern_of(" ".join(tokens)).finditer(data)]
                if len(found) != 1:
                    continue
                start = found[0]
                labels = ",".join(f"{l['name']}+{l['offset']:#x}" for l in obj.get("labels", []))
                key = (args.vram + start, args.vram + start + len(tokens))
                hits.setdefault(key, []).append(f"{version.name} {lib.stem} {obj['name']} {labels}")
    for (start, end), names in sorted(hits.items()):
        for name in names:
            print(f"{start:08x} {end:08x} {name}")


if __name__ == "__main__":
    main()
