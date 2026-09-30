#!/usr/bin/env python3
"""Where the original jump tables sit mod 8, per target (docs/matching.md).

Finds every GCC switch dispatch in each original image,
    sltiu rX, rY, N ... lui $at, %hi(T); addu $at, $at, rZ; lw rW, %lo(T)($at)
and lists consecutive tables T (N entries) whose relation tells how the
original assembler treated GCC's `.align 3` before each table:
  PAD   an odd-length table, one zero word, the next table (phase kept);
  FLIP  the next table has the other phase mod 8 (a unit boundary);
  ODD   an odd-length table abutting a same-phase table (never seen: that
        would mean the directive was ignored or taken as 4-byte).
Owners come from the linked maps, so run it after `make -C decomp all-verify`.
"""
import argparse
import bisect
import collections
import glob
import re
import struct
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent


def functions(map_path):
    found = []
    if map_path.exists():
        for line in map_path.read_text().splitlines():
            m = re.match(r"\s+0x([0-9a-f]{8})\s+([A-Za-z_]\w*)$", line)
            if m and not m.group(2).startswith(("D_", "jtbl_", "_")):
                found.append((int(m.group(1), 16), m.group(2)))
    return sorted(found)


def tables(data, vram, start):
    word = lambda off: struct.unpack_from("<I", data, off)[0]
    found = {}
    for off in range(start + 44, len(data) - 12, 4):
        lui, addu, lw = word(off), word(off + 4), word(off + 8)
        if lui >> 16 != 0x3C01:
            continue
        if addu & 0xFFE0F83F != 0x00200821 or (addu >> 21) & 31 != 1:
            continue
        if lw >> 26 != 0x23 or (lw >> 21) & 31 != 1:
            continue
        table = ((lui & 0xFFFF) << 16) + (((lw & 0xFFFF) ^ 0x8000) - 0x8000)
        count = next((word(off - 4 * b) & 0xFFFF for b in range(1, 12) if word(off - 4 * b) >> 26 == 0x0B), None)
        found.setdefault(table, (count, vram + off - start))
    return sorted((t, n, pc) for t, (n, pc) in found.items())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-v", "--verbose", action="store_true", help="also list every PAD")
    args = parser.parse_args()
    total = collections.Counter()
    for config in sorted(glob.glob(str(ROOT / "decomp/targets/*/*.yaml"))):
        cfg = yaml.safe_load(open(config))
        opts = cfg["options"]
        seg = next(s for s in cfg["segments"] if isinstance(s, dict) and "vram" in s)
        vram, start = seg["vram"], seg.get("start", 0)
        data = (ROOT / opts["target_path"]).read_bytes()
        mk = Path(config).with_suffix(".mk").read_text()
        image = re.search(r"^IMAGE := (.*)$", mk, re.M).group(1).strip()
        funcs = functions(ROOT / (image.replace(".bin", "") + ".bin.map" if image.endswith(".bin") else image + ".map"))
        addrs = [a for a, _ in funcs]
        owner = lambda pc: funcs[bisect.bisect_right(addrs, pc) - 1][1] if addrs and pc >= addrs[0] else "?"
        rows = tables(data, vram, start)
        if not rows:
            continue
        word = lambda addr: struct.unpack_from("<I", data, addr - vram + start)[0]
        phases = collections.Counter(t % 8 for t, _, _ in rows)
        print(f"== {opts['basename']}: {len(rows)} tables, {phases[0]} at 0 mod 8, {phases[4]} at 4 mod 8")
        for (a, n, pc), (b, _, pc2) in zip(rows, rows[1:]):
            if n is None:
                continue
            gap = b - (a + 4 * n)
            if a % 8 != b % 8:
                kind = "FLIP-IN-ONE-FUNCTION" if owner(pc) == owner(pc2) else "FLIP"
            elif gap == 4 and n % 2 and word(a + 4 * n) == 0:
                kind = "PAD"
            elif gap == 0 and n % 2:
                kind = "ODD"
            else:
                continue
            total[kind] += 1
            if kind != "PAD" or args.verbose:
                print(f"  {a:08X} ({n} entries, {owner(pc)}) -> {b:08X} ({owner(pc2)}), gap {gap}: {kind}")
    print(dict(total))


if __name__ == "__main__":
    main()
