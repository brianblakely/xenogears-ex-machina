#!/usr/bin/env python3
"""Extract the boot program and the numbered disc files of one raw track.

The original index occupies logical sectors 24-38 (EVID-REF-007): 30720 bytes of
seven-byte records, a 24-bit start sector followed by a signed 32-bit size.
A negative size marks a directory entry whose magnitude is its file count.
Only Mode 2 Form 1 user data (2048 bytes per sector) is copied; the output is
original content and belongs under ignored .local/.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

SECTOR = 2352
USER = 24
INDEX_LBA = 24
INDEX_BYTES = 30720


def read_sectors(raw: bytes, lba: int, size: int) -> bytes:
    count = (size + 2047) // 2048
    out = bytearray()
    for n in range(count):
        base = (lba + n) * SECTOR + USER
        out += raw[base : base + 2048]
    return bytes(out[:size])


def iso_boot_program(raw: bytes) -> tuple[str, int, int]:
    pvd = read_sectors(raw, 16, 2048)
    root_lba, root_size = struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0]
    root = read_sectors(raw, root_lba, root_size)
    pos = 0
    while pos < len(root):
        length = root[pos]
        if length == 0:
            pos = (pos // 2048 + 1) * 2048
            continue
        lba, size = struct.unpack_from("<I", root, pos + 2)[0], struct.unpack_from("<I", root, pos + 10)[0]
        name = root[pos + 33 : pos + 33 + root[pos + 32]].decode("ascii", "replace")
        if name.startswith("SLUS_"):
            return name.split(";")[0], lba, size
        pos += length
    raise SystemExit("no SLUS boot program in the ISO root")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path, help="MODE2/2352 raw track")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.raw.read_bytes()
    args.output.mkdir(parents=True, exist_ok=True)

    name, lba, size = iso_boot_program(raw)
    exe = read_sectors(raw, lba, size)
    (args.output / name).write_bytes(exe)
    manifest = {"boot": {"name": name, "lba": lba, "size": size, "sha256": hashlib.sha256(exe).hexdigest()}, "files": []}

    index = read_sectors(raw, INDEX_LBA, INDEX_BYTES)
    files = args.output / "files"
    files.mkdir(exist_ok=True)
    for slot in range(INDEX_BYTES // 7):
        record = index[slot * 7 : slot * 7 + 7]
        start = record[0] | record[1] << 8 | record[2] << 16
        (length,) = struct.unpack_from("<i", record, 3)
        if start == 0xFFFFFF or (start == 0 and length == 0):
            continue
        entry = {"slot": slot, "lba": start, "size": length}
        if length > 0:
            data = read_sectors(raw, start, length)
            (files / f"{slot:04d}.bin").write_bytes(data)
            entry["sha256"] = hashlib.sha256(data).hexdigest()
        manifest["files"].append(entry)
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")
    print(f"{name} {manifest['boot']['sha256']} files={sum(1 for f in manifest['files'] if f['size'] > 0)}")


if __name__ == "__main__":
    main()
