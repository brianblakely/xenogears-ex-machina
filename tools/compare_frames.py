#!/usr/bin/env python3
"""Compare a native run's checkpoint frames with an original-environment capture.

    python3 tools/compare_frames.py RUN_DIR CAPTURE_DIR

RUN_DIR holds frame-NNNNNN.png from `xem-headless --inputs ROUTE --checkpoint-every N
--checkpoint-dir RUN_DIR`; CAPTURE_DIR the capture of the same route
(tools/reference/observe.py --inputs ROUTE --capture-every N). The display is
the game's display range (e.g. 320x216) and the capture the frontend's 240
lines, so each pair is compared at the vertical offset that matches best; the
score is the mean absolute difference per colour channel (0-255). Equal game
state scores near 0; moving pictures (movies) also differ by their timing.
"""

import struct
import sys
import zlib
from pathlib import Path


def read_png(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos, idat = 8, b""
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
            if depth != 8 or colour not in (2, 6):
                raise ValueError(f"{path}: only 8-bit RGB(A) PNGs")
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    channels = 3 if colour == 2 else 4
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        filter_type = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0
            b = previous[x]
            c = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                line[x] = (line[x] + a) & 255
            elif filter_type == 2:
                line[x] = (line[x] + b) & 255
            elif filter_type == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif filter_type == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([line[i] for i in range(stride) if i % channels < 3])
        previous = line
    return width, height, rows


def score(run, capture):
    """(best mean difference, offset) over the vertical alignments."""
    (w1, h1, r1), (w2, h2, r2) = run, capture
    if (w1, h1) == (w2, h2):
        offsets = [0]
    elif w1 == w2 and h1 < h2:
        offsets = range(h2 - h1 + 1)
    else:
        raise ValueError(f"cannot align {w1}x{h1} with {w2}x{h2}")
    best = None
    for top in offsets:
        total = sum(abs(p - q) for a, b in zip(r1, r2[top:top + h1]) for p, q in zip(a, b))
        diff = total / (w1 * h1 * 3)
        if best is None or diff < best[0]:
            best = (diff, top)
    return best


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    run_dir, capture_dir = map(Path, sys.argv[1:])
    frames = sorted(run_dir.glob("frame-*.png"))
    if not frames:
        sys.exit(f"no checkpoints in {run_dir}")
    for frame in frames:
        capture = capture_dir / frame.name
        if not capture.exists():
            print(f"{frame.name}: no capture frame")
            continue
        diff, top = score(read_png(frame), read_png(capture))
        print(f"{frame.name}: {diff:6.2f} (offset {top})")


if __name__ == "__main__":
    main()
