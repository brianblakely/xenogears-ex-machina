#!/usr/bin/env python3
"""Reproduce the original packed-block containers from decoded (or rebuilt) images.

The format is decoded by the resident routine 0x80032eb4 (tools/analysis/packed.py):
a u32 decoded length, then groups of one flag byte and eight tokens; a set flag
bit is a two-byte copy (12-bit backward distance, 4-bit length - 3), a clear bit
a literal. The original packer behaves exactly like Haruhiko Okumura's
public-domain LZSS binary-tree encoder (N 4096, F 18, threshold 2) with two
game-specific differences:

* the preset ring bytes are never inserted into the search tree, so no copy
  reaches before the start of the data;
* the decoder consumes whole groups, so the packer completes the last group
  with zero literal tokens and records the program's length plus theirs: a
  decoded image ends with those 0-7 zero bytes, which belong to no object.

Copies take the tree's first longest match; distances are relative. The plain
encoding of each target's program followed by its zero literals is the disc
stream of all 14 packed containers (worldmap 2, field 6, movie 7, menu 5,
battle and slot39 0), and some such tail reproduces each of the 24 other disc-1
files that hold one whole packed stream of up to 384 KiB (`--sample`).
Splitting trailing copies into literals instead also reproduces the 14, but
not files/0090.bin. Where a program itself ends in zero bytes the stream does
not fix the split (field's tail could be 4-6 bytes, worldmap's 1-3); there
the resident's mode table, which starts the overlay's BSS at its program end,
does. A target appends its tail after the link (PACKER_TAIL in its .mk), so the
image only matches when the link ends at the program end. The packed stream ends inside its last 2048-byte sector,
which is zero-filled; the disc index's declared file size is not the stream
length and is not derived here.

    packed_container.py IMAGE --tail K --disc RAW --manifest MANIFEST --slot N
    packed_container.py --sample .local/extract/disc1/files [--max-size 0x60000]

IMAGE's last K bytes are the packer's zero literals and the rest the program
it encoded; the rule is checked, not assumed: the program's plain encoding
must leave exactly K tokens to complete its last group. `--sample` lists, for
each file of a directory that holds one packed stream (decoded 256 bytes to
--max-size, at most a sector or zeros after it), the zero tails that
reproduce it, and fails if none does.
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
    """Pack a program as the original packer did: the decoded image is `data`
    followed by the zero literals that complete the last group."""
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
    tail = -len(tokens) % 8
    return serialize(size + tail, tokens + [(0, 0)] * tail)


def serialize(size: int, tokens: list[tuple[int, int]]) -> bytes:
    out = bytearray(size.to_bytes(4, "little"))
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


def sample(directory: Path, max_size: int) -> int:
    """Print the zero tails that reproduce each whole packed file of directory."""
    sys.path.insert(0, str(ROOT))
    from tools.analysis.packed import PackedError, decode_block

    failures = 0
    for path in sorted(directory.glob("*.bin")):
        raw = path.read_bytes()
        size = int.from_bytes(raw[:4], "little") if len(raw) >= 16 else 0
        if not 256 <= size <= max_size:
            continue
        try:
            block = decode_block(raw, output_limit=max_size)
        except (PackedError, IndexError):
            continue
        rest = raw[block.token_bytes :]
        if any(rest) and len(rest) > 2048:
            continue  # more data follows the stream
        data = block.data
        tails = []
        for k in range(8):
            if any(data[len(data) - k :]):
                break
            stream = encode(data[: len(data) - k])
            if stream == raw[: len(stream)]:
                tails.append(k)
        failures += not tails
        print(f"{path.name}\t{size:#x}\tzero literals {tails or 'NONE'}", flush=True)
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", type=Path, nargs="?", help="decoded or rebuilt image to pack")
    parser.add_argument("--tail", type=int, default=0, help="the image's trailing zero literals (PACKER_TAIL)")
    parser.add_argument("--disc", type=Path, help="MODE2/2352 raw track")
    parser.add_argument("--manifest", type=Path, help="tools/extraction/disc_files.py manifest")
    parser.add_argument("--slot", type=int)
    parser.add_argument("--sample", type=Path, help="directory of extracted disc files")
    parser.add_argument("--max-size", type=lambda s: int(s, 0), default=0x60000)
    args = parser.parse_args()
    if args.sample:
        return sample(args.sample, args.max_size)
    if args.image is None or args.disc is None or args.manifest is None or args.slot is None:
        parser.error("IMAGE, --disc, --manifest and --slot are required without --sample")
    image = args.image.read_bytes()
    program = image[: len(image) - args.tail]
    if not 0 <= args.tail < 8 or any(image[len(program) :]):
        raise SystemExit(f"{args.image}: the last {args.tail} bytes are not a packer tail of zero literals")
    packed = encode(program)
    original = disc_file(args.disc.read_bytes(), args.manifest, args.slot)
    result = {
        "claim": "compressed_container_reproduction",
        "slot": args.slot,
        "program_bytes": len(program),
        "zero_literals": int.from_bytes(packed[:4], "little") - len(program),
        "stream_bytes": len(packed),
        "stream_sha256": hashlib.sha256(packed).hexdigest(),
        "stream_matches_disc": original[: len(packed)] == packed,
        "sector_tail_zero": not any(original[len(packed) :]),
    }
    print(json.dumps(result, sort_keys=True))
    ok = result["zero_literals"] == args.tail and result["stream_matches_disc"] and result["sector_tail_zero"]
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
