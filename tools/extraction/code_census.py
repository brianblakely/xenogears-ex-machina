#!/usr/bin/env python3
"""Check that every code-bearing file on both discs is a matching target image.

    code_census.py [--threshold N]

Scans every numbered file of both discs (raw, and LZSS-decoded when the whole
file decodes as one packed block) for MIPS function structure: `jr $ra` words
near an `addiu $sp,$sp,+N` epilogue or before an `addiu $sp,$sp,-N` prologue.
Each file with at least N such returns (default 1) must equal, byte for byte,
the boot executable or one of the decoded overlay images written by
tools/extraction/overlays.py; the tool lists which, and exits 1 for any
code-bearing file that matches no target. MDEC movie streams (files whose
first sector opens with the STR frame header 0x80010160) hold compressed video
whose bytes can mimic code; they are reported as streams. It does not look
inside packed sections at nonzero offsets of archive files. Inputs are the local raw tracks
and manifests (tools/extraction/disc_files.py); output names original files
only by slot.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.analysis.packed import PackedError, decode_block  # noqa: E402

SECTOR = 2352
JR_RA = 0x03E00008
STR_FRAME = 0x80010160


def returns(data: bytes) -> int:
    """Function-like returns: jr $ra near a stack release or before a stack allocation."""
    words = struct.unpack_from(f"<{len(data) // 4}I", data)
    count = 0
    for i, word in enumerate(words):
        if word != JR_RA:
            continue
        before = words[max(0, i - 64) : i + 2]
        after = words[i + 1 : i + 10]
        if any(w >> 16 == 0x27BD and not w & 0x8000 for w in before) or any(
            w >> 16 == 0x27BD and w & 0x8000 for w in after
        ):
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--threshold", type=int, default=1)
    args = parser.parse_args()
    targets = {
        hashlib.sha256(p.read_bytes()).hexdigest(): p.stem
        for p in sorted((ROOT / ".local/extract/overlays").glob("*.bin"))
    }
    unknown = 0
    for disc in (1, 2):
        raw = (ROOT / f".local/discs/disc{disc}.bin").read_bytes()
        manifest = json.loads((ROOT / f".local/extract/disc{disc}/manifest.json").read_text())
        boot = ROOT / f".local/extract/disc{disc}" / manifest["boot"]["name"]
        targets[hashlib.sha256(boot.read_bytes()).hexdigest()] = boot.name
        for entry in manifest["files"]:
            if entry["size"] <= 0:
                continue
            count = (entry["size"] + 2047) // 2048
            base = entry["lba"] * SECTOR + 24
            sectors = b"".join(raw[base + n * SECTOR : base + n * SECTOR + 2048] for n in range(count))
            if int.from_bytes(sectors[:4], "little") == STR_FRAME:
                print(f"disc{disc} slot {entry['slot']} stream {entry['size']:#x} -> movie stream")
                continue
            candidates = [("raw", sectors[: entry["size"]])]
            try:
                candidates.append(("packed", decode_block(sectors).data))
            except PackedError:
                pass
            for form, data in candidates:
                if returns(data) < args.threshold:
                    continue
                name = targets.get(hashlib.sha256(data).hexdigest())
                print(f"disc{disc} slot {entry['slot']} {form} {len(data):#x} -> {name or 'UNMATCHED'}")
                unknown += name is None
    print(f"unmatched code-bearing files: {unknown}")
    return 1 if unknown else 0


if __name__ == "__main__":
    raise SystemExit(main())
