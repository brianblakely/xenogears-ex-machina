#!/usr/bin/env python3
"""Compare executable code loaded in an original-environment RAM image with rebuilt images.

    matching_ram.py RAM IMAGE@VRAM[:START-END] [IMAGE@VRAM[:START-END] ...]

RAM is a 2 MiB main-RAM snapshot (physical offset 0 = 0x80000000) from an
original-game scenario run. Each rebuilt image is compared over the given
VRAM range (default: its whole text range must be supplied explicitly, since
data and BSS change while the game runs). A match shows the rebuilt code is
what the original loader placed and executed in that run; the byte-exact image
comparison (make verify) remains the build acceptance.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("ram", type=Path)
    parser.add_argument("images", nargs="+", help="PATH@VRAM:START-END (hex addresses)")
    parser.add_argument("--header", type=lambda v: int(v, 0), default=0, help="bytes before VRAM in the image file")
    args = parser.parse_args()
    ram = args.ram.read_bytes()
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
