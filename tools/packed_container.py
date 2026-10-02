#!/usr/bin/env python3
"""Reproduce the original packed-block containers from decoded (or rebuilt) images.

The format is decoded by the resident routine 0x80032eb4 (tools/analysis/packed.py):
a u32 decoded length, then groups of one flag byte and eight tokens; a set flag
bit is a two-byte copy (12-bit backward distance, 4-bit length - 3), a clear bit
a literal. The original packer behaves exactly like Haruhiko Okumura's
public-domain LZSS binary-tree encoder (N 4096, F 18, threshold 2) with two
game-specific differences established against every packed mode overlay:

* the preset ring bytes are never inserted into the search tree, so no copy
  reaches before the start of the data;
* the stream always ends with a complete group of eight tokens (the decoder
  consumes whole groups): trailing copies are turned into literals, the last
  one needed only shortened, until the token count is a multiple of eight.

Copies take the tree's first longest match; distances are relative. The packed
stream ends inside its last 2048-byte sector, which is zero-filled; the disc
index's declared file size is not the stream length and is not derived here.

    packed_container.py verify IMAGE --disc RAW --slot N
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
N = 4096
F = 18
THRESHOLD = 2
NIL = N
SECTOR = 2352


def encode(data: bytes) -> bytes:
    size = len(data)
    text = bytearray(b" ") * (N + F - 1)
    lson = [NIL] * (N + 1)
    rson = [NIL] * (N + 257)
    dad = [NIL] * (N + 1)
    match = [0, 0]  # position, length

    def insert(r: int) -> None:
        cmp = 1
        p = N + 1 + text[r]
        rson[r] = lson[r] = NIL
        match[1] = 0
        while True:
            if cmp >= 0:
                if rson[p] == NIL:
                    rson[p] = r
                    dad[r] = p
                    return
                p = rson[p]
            else:
                if lson[p] == NIL:
                    lson[p] = r
                    dad[r] = p
                    return
                p = lson[p]
            i = 1
            while i < F:
                cmp = text[r + i] - text[p + i]
                if cmp:
                    break
                i += 1
            if i > match[1]:
                match[0], match[1] = p, i
                if i >= F:
                    break
        dad[r], lson[r], rson[r] = dad[p], lson[p], rson[p]
        dad[lson[p]] = dad[rson[p]] = r
        if rson[dad[p]] == p:
            rson[dad[p]] = r
        else:
            lson[dad[p]] = r
        dad[p] = NIL

    def delete(p: int) -> None:
        if dad[p] == NIL:
            return
        if rson[p] == NIL:
            q = lson[p]
        elif lson[p] == NIL:
            q = rson[p]
        else:
            q = lson[p]
            if rson[q] != NIL:
                while rson[q] != NIL:
                    q = rson[q]
                rson[dad[q]] = lson[q]
                dad[lson[q]] = dad[q]
                lson[q] = lson[p]
                dad[lson[p]] = q
            rson[q] = rson[p]
            dad[rson[p]] = q
        dad[q] = dad[p]
        if rson[dad[p]] == p:
            rson[dad[p]] = q
        else:
            lson[dad[p]] = q
        dad[p] = NIL

    tokens: list[tuple[int, int]] = []  # (distance, length); distance 0 is a literal byte

    s, r = 0, N - F
    read = 0
    length = min(F, size)
    text[r : r + length] = data[:length]
    read = length
    insert(r)
    while length > 0:
        best = min(match[1], length)
        if best <= THRESHOLD:
            best = 1
            tokens.append((0, text[r]))
        else:
            tokens.append(((r - match[0]) & (N - 1), best))
        i = 0
        while i < best and read < size:
            delete(s)
            text[s] = data[read]
            if s < F - 1:
                text[s + N] = data[read]
            read += 1
            s = (s + 1) & (N - 1)
            r = (r + 1) & (N - 1)
            insert(r)
            i += 1
        while i < best:
            i += 1
            delete(s)
            s = (s + 1) & (N - 1)
            r = (r + 1) & (N - 1)
            length -= 1
            if length:
                insert(r)
    return serialize(data, complete_groups(data, tokens))


def complete_groups(data: bytes, tokens: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Split trailing copies into literals until the last group is full.

    Working back from the end, a copy is shortened (keeping at least three
    bytes) when that alone completes the group, otherwise it becomes literals.
    Input too short to fill a group keeps its partial last group.
    """
    need = -len(tokens) % 8
    end = len(data)
    index = len(tokens)
    while need and index:
        index -= 1
        distance, length = tokens[index]
        end -= length if distance else 1
        if not distance:
            continue
        if length - need >= 3 and need < length - 1:
            kept = length - need
            tokens[index : index + 1] = [(distance, kept)] + [(0, data[end + kept + k]) for k in range(need)]
            need = 0
        else:
            tokens[index : index + 1] = [(0, data[end + k]) for k in range(length)]
            need = (need - (length - 1)) % 8
    return tokens


def serialize(data: bytes, tokens: list[tuple[int, int]]) -> bytes:
    out = bytearray(len(data).to_bytes(4, "little"))
    for start in range(0, len(tokens), 8):
        flags, body = 0, bytearray()
        for bit, (distance, value) in enumerate(tokens[start : start + 8]):
            if distance:
                flags |= 1 << bit
                body += bytes([distance & 0xFF, (distance >> 8) | ((value - 3) << 4)])
            else:
                body.append(value)
        out.append(flags)
        out += body
    return bytes(out)


def disc_file(raw: bytes, manifest: Path, slot: int) -> bytes:
    entry = next(e for e in json.loads(manifest.read_text())["files"] if e["slot"] == slot)
    sectors = (entry["size"] + 2047) // 2048
    base = entry["lba"] * SECTOR + 24
    return b"".join(raw[base + n * SECTOR : base + n * SECTOR + 2048] for n in range(sectors))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", type=Path, help="decoded or rebuilt image to pack")
    parser.add_argument("--disc", type=Path, required=True, help="MODE2/2352 raw track")
    parser.add_argument("--manifest", type=Path, required=True, help="tools/extraction/disc_files.py manifest")
    parser.add_argument("--slot", type=int, required=True)
    args = parser.parse_args()
    packed = encode(args.image.read_bytes())
    original = disc_file(args.disc.read_bytes(), args.manifest, args.slot)
    result = {
        "claim": "compressed_container_reproduction",
        "slot": args.slot,
        "stream_bytes": len(packed),
        "stream_sha256": hashlib.sha256(packed).hexdigest(),
        "stream_matches_disc": original[: len(packed)] == packed,
        "sector_tail_zero": not any(original[len(packed) :]),
    }
    print(json.dumps(result, sort_keys=True))
    return 0 if result["stream_matches_disc"] and result["sector_tail_zero"] else 1


if __name__ == "__main__":
    sys.exit(main())
