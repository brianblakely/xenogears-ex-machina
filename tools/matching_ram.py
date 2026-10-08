#!/usr/bin/env python3
"""Compare executable code loaded in an original-environment RAM image with rebuilt images.

    matching_ram.py RAM IMAGE@VRAM[:START-END] [IMAGE@VRAM[:START-END] ...]
    matching_ram.py RAM --targets [--min-agreement 0.99]

RAM is a 2 MiB main-RAM snapshot (physical offset 0 = 0x80000000) from an
original-game scenario run. Each rebuilt image is compared over the given
VRAM range (default: its whole text range must be supplied explicitly, since
data and BSS change while the game runs). A match shows the rebuilt code is
what the original loader placed and executed in that run; the byte-exact image
comparison (make verify) remains the build acceptance.

With --targets, every decomp/targets/*/*.mk image built in the matching shell is
compared over its linked function range; images whose code is resident (at
least --min-agreement of the words agree) are reported with the addresses of the
words that differ (e.g. the harness's startup guard at 0x80019930).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def loaded_targets(ram: bytes, min_agreement: float) -> list[dict]:
    sys.path.insert(0, str(ROOT / "tools"))
    from matching_diff import config, functions, load_offset

    results = []
    for path in sorted((ROOT / "decomp/targets").glob("*/*.mk")):
        values = config(path)
        image = ROOT / values["IMAGE"]
        elf = Path(str(image) + ".elf")
        if not elf.exists():
            continue
        spans = functions(elf)
        if not spans:
            continue
        start = min(address for address, _, _ in spans)
        end = max(address + size for address, size, _ in spans)
        delta = load_offset(elf)
        rebuilt = image.read_bytes()[start + delta : end + delta]
        loaded = ram[start - 0x80000000 : end - 0x80000000]
        differing = [
            start + i
            for i in range(0, len(rebuilt), 4)
            if rebuilt[i : i + 4] != loaded[i : i + 4]
        ]
        words = len(rebuilt) // 4
        agreement = 1 - len(differing) / words
        if agreement >= min_agreement:
            results.append(
                {
                    "target": path.stem,
                    "range": [f"{start:08x}", f"{end:08x}"],
                    "agreement": round(agreement, 6),
                    "differing_words": [f"{a:08x}" for a in differing[:16]],
                }
            )
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("ram", type=Path)
    parser.add_argument("images", nargs="*", help="PATH@VRAM:START-END (hex addresses)")
    parser.add_argument("--header", type=lambda v: int(v, 0), default=0, help="bytes before VRAM in the image file")
    parser.add_argument("--targets", action="store_true", help="scan every built target")
    parser.add_argument("--min-agreement", type=float, default=0.99)
    args = parser.parse_args()
    ram = args.ram.read_bytes()
    if args.targets:
        resident = loaded_targets(ram, args.min_agreement)
        print(json.dumps({"claim": "loaded_code_agreement", "resident_targets": resident}, indent=1))
        return 0 if resident else 1
    if not args.images:
        parser.error("give IMAGE@VRAM:START-END specs or --targets")
    results = []
    for spec in args.images:
        path, rest = spec.split("@")
        vram_text, window = rest.split(":")
        vram = int(vram_text, 16)
        start, end = (int(v, 16) for v in window.split("-"))
        image = Path(path).read_bytes()
        header = args.header if path.endswith(("SLUS_006.64", "SLUS_006.69")) else 0
        rebuilt = image[header + start - vram : header + end - vram]
        loaded = ram[start - 0x80000000 : end - 0x80000000]
        first = next((i for i, (a, b) in enumerate(zip(rebuilt, loaded)) if a != b), None)
        results.append(
            {
                "image": path,
                "range": [f"{start:08x}", f"{end:08x}"],
                "matched": rebuilt == loaded,
                "first_difference": None if first is None else f"{start + first:08x}",
                "sha256": hashlib.sha256(rebuilt).hexdigest(),
            }
        )
    print(json.dumps({"claim": "loaded_code_agreement", "results": results}, indent=1))
    return 0 if all(r["matched"] for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
