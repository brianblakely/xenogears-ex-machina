#!/usr/bin/env python3
"""Write every executable overlay image of both discs under .local/extract/overlays/.

Overlay files were found by scanning all numbered disc files (whole-file packed
blocks decoded) for MIPS function structure; load addresses come from each
image's internal call targets and pointers (decomp/targets/overlays/*.yaml).
Packed mode overlays are decoded from their complete sectors, because the
resident decoder reads its final flag byte past the file's declared size. The
two discs carry byte-identical overlays; this tool checks that and fails
otherwise. Output is original content for local matching only.
"""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.analysis.packed import decode_block  # noqa: E402

SECTOR = 2352
# name: (Disc 1 slot, Disc 2 slot, packed)
OVERLAYS = {
    "menu": (35, 30, True),
    "field": (36, 31, True),
    "worldmap": (37, 32, True),
    "battle": (38, 33, True),
    "slot39": (2597, 2592, False),
    "movie": (40, 35, True),
    "mdec": (19, 14, False),
    "debug595": (595, 590, False),
    "ovl2143": (2143, 2138, False),
    "ovl2596": (2596, 2591, False),
    "ovl2598": (2598, 2593, False),
    "ovl2600": (2600, 2595, False),
    "ovl2601": (2601, 2596, False),
    "ovl2602": (2602, 2597, False),
    "ovl2606": (2606, 2601, False),
    "debug2611": (2611, 2606, False),
    "ovl2615": (2615, 2610, False),
    "ovl3087": (3087, 3082, False),
    "ovl3381": (3381, 3376, False),
    "ovl3383": (3383, 3378, False),
    "ovl3384": (3384, 3379, False),
    "ovl3385": (3385, 3380, False),
    "ovl3386": (3386, 3381, False),
    "ovl3387": (3387, 3382, False),
}


def image(raw: bytes, entry: dict, packed: bool) -> bytes:
    count = (entry["size"] + 2047) // 2048
    base = entry["lba"] * SECTOR + 24
    data = b"".join(raw[base + n * SECTOR : base + n * SECTOR + 2048] for n in range(count))
    return decode_block(data).data if packed else data[: entry["size"]]


def main() -> None:
    out = ROOT / ".local/extract/overlays"
    out.mkdir(parents=True, exist_ok=True)
    images: dict[str, list[bytes]] = {name: [] for name in OVERLAYS}
    for disc in (1, 2):
        raw = (ROOT / f".local/discs/disc{disc}.bin").read_bytes()
        manifest = json.loads((ROOT / f".local/extract/disc{disc}/manifest.json").read_text())
        slots = {entry["slot"]: entry for entry in manifest["files"]}
        for name, (slot1, slot2, packed) in OVERLAYS.items():
            images[name].append(image(raw, slots[slot1 if disc == 1 else slot2], packed))
    for name, (first, second) in images.items():
        if first != second:
            raise SystemExit(f"{name}: Disc 1 and Disc 2 images differ")
        (out / f"{name}.bin").write_bytes(first)
        print(f"{name} {len(first):#x} {hashlib.sha256(first).hexdigest()}")


if __name__ == "__main__":
    main()
