"""Read standard raw-CD/ISO9660/PS-X EXE metadata, never game-specific archives.

All reports are factual measurements of the supplied bytes. Disc sequence, market
revision names, and original gameplay are deliberately not inferred here.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from typing import BinaryIO

RAW_SECTOR_SIZE = 2352
BLOCK_SIZE = 2048
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


def file_hash(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def both_endian(data: bytes, offset: int, size: int) -> int:
    if offset < 0 or offset + 2 * size > len(data):
        raise ValueError("Truncated both-endian ISO integer")
    little = int.from_bytes(data[offset : offset + size], "little")
    big = int.from_bytes(data[offset + size : offset + 2 * size], "big")
    if little != big:
        raise ValueError("ISO little/big-endian integer copies disagree")
    return little


class RawCd:
    def __init__(self, stream: BinaryIO, size: int):
        if size == 0 or size % RAW_SECTOR_SIZE:
            raise ValueError("Expected a nonempty integral number of raw 2352-byte sectors")
        self.stream = stream
        self.sectors = size // RAW_SECTOR_SIZE

    def read_sector(self, lba: int) -> bytes:
        if not 0 <= lba < self.sectors:
            raise ValueError(f"LBA {lba} outside image with {self.sectors} sectors")
        self.stream.seek(lba * RAW_SECTOR_SIZE)
        raw = self.stream.read(RAW_SECTOR_SIZE)
        if len(raw) != RAW_SECTOR_SIZE or raw[:12] != SYNC:
            raise ValueError(f"Invalid raw data-sector sync at LBA {lba}")
        if raw[15] == 1:
            return raw[16 : 16 + BLOCK_SIZE]
        if raw[15] == 2:
            if raw[16:20] != raw[20:24]:
                raise ValueError(f"Mode 2 subheader copies disagree at LBA {lba}")
            if raw[18] & 0x20:
                raise ValueError(f"Mode 2 Form 2 is not an ISO data block at LBA {lba}")
            return raw[24 : 24 + BLOCK_SIZE]
        raise ValueError(f"Unsupported data-sector mode {raw[15]} at LBA {lba}")

    def read_extent(self, lba: int, size: int, *, limit: int = 16 * 1024 * 1024) -> bytes:
        count = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
        if size < 0 or size > limit or lba < 0 or lba + count > self.sectors:
            raise ValueError("Invalid, oversized, or out-of-image ISO extent")
        return b"".join(self.read_sector(lba + i) for i in range(count))[:size]


def directory_record(data: bytes, offset: int) -> dict:
    if offset < 0 or offset >= len(data):
        raise ValueError("Directory record outside data")
    length = data[offset]
    if length < 34 or offset + length > len(data):
        raise ValueError("Truncated ISO directory record")
    rec = data[offset : offset + length]
    if 33 + rec[32] > length:
        raise ValueError("Truncated ISO filename")
    if rec[1] or rec[26] or rec[27] or rec[25] & 0x80:
        raise ValueError("Extended-attribute, interleaved, or multi-extent entries unsupported")
    name_bytes = rec[33 : 33 + rec[32]]
    name = {b"\x00": ".", b"\x01": ".."}.get(name_bytes)
    if name is None:
        name = name_bytes.decode("ascii", errors="strict")
        if any(char in name for char in ("/", "\\", "\x00")):
            raise ValueError("Unsafe ISO filename")
    return {
        "name": name,
        "lba": both_endian(rec, 2, 4),
        "size": both_endian(rec, 10, 4),
        "directory": bool(rec[25] & 2),
        "record_length": length,
    }


def inspect_iso(cd: RawCd) -> dict:
    pvd = cd.read_sector(16)
    if pvd[:7] != b"\x01CD001\x01":
        raise ValueError("No primary ISO9660 volume descriptor at LBA 16")
    if both_endian(pvd, 128, 2) != BLOCK_SIZE:
        raise ValueError("Only 2048-byte ISO logical blocks are supported")
    volume_blocks = both_endian(pvd, 80, 4)
    if not 17 <= volume_blocks <= cd.sectors:
        raise ValueError("ISO volume omits its descriptor or extends beyond raw image")
    root = directory_record(pvd, 156)
    root_end = root["lba"] + (root["size"] + BLOCK_SIZE - 1) // BLOCK_SIZE
    if not root["directory"] or root["name"] != "." or root["size"] == 0:
        raise ValueError("ISO root must be a nonempty directory with the root identifier")
    if root["lba"] < 17 or root_end > volume_blocks:
        raise ValueError("ISO root extent lies outside declared volume data")
    queue = [("", root, 0)]
    visited = set()
    paths = set()
    entries = []
    while queue:
        prefix, directory, depth = queue.pop(0)
        key = (directory["lba"], directory["size"])
        if key in visited:
            raise ValueError("Cycle or aliased directory in ISO traversal")
        if depth > 32 or len(visited) >= 4096:
            raise ValueError("ISO directory traversal budget exceeded")
        visited.add(key)
        content = cd.read_extent(directory["lba"], directory["size"])
        offset = 0
        while offset < len(content):
            if content[offset] == 0:
                offset = (offset // BLOCK_SIZE + 1) * BLOCK_SIZE
                continue
            record = directory_record(content, offset)
            if offset % BLOCK_SIZE + record["record_length"] > BLOCK_SIZE:
                raise ValueError("ISO directory record crosses a logical block")
            offset += record.pop("record_length")
            if record["name"] in (".", ".."):
                continue
            end = record["lba"] + (record["size"] + BLOCK_SIZE - 1) // BLOCK_SIZE
            if end > volume_blocks:
                raise ValueError("ISO entry extends beyond volume")
            record["path"] = prefix + "/" + record.pop("name")
            if record["path"] in paths:
                raise ValueError("Duplicate ISO directory path")
            paths.add(record["path"])
            entries.append(record)
            if len(entries) > 100_000:
                raise ValueError("ISO entry budget exceeded")
            if record["directory"]:
                queue.append((record["path"], record, depth + 1))
    return {
        "pvd_lba": 16,
        "volume_id": pvd[40:72].decode("ascii").rstrip(" \x00"),
        "system_id": pvd[8:40].decode("ascii").rstrip(" \x00"),
        "volume_blocks": volume_blocks,
        "volume_set_size": both_endian(pvd, 120, 2),
        "volume_sequence_number": both_endian(pvd, 124, 2),
        "creation_timestamp_raw": pvd[813:830].hex(),
        "root_lba": root["lba"],
        "entries": sorted(entries, key=lambda row: row["path"]),
    }


def inspect_boot(cd: RawCd, iso: dict) -> dict:
    entries = {entry["path"].upper(): entry for entry in iso["entries"]}
    config = entries.get("/SYSTEM.CNF;1")
    if not config or config["directory"]:
        raise ValueError("Expected SYSTEM.CNF;1 at ISO root")
    data = cd.read_extent(config["lba"], config["size"], limit=65536)
    match = re.search(rb"(?im)^BOOT\s*=\s*cdrom:\\([^\r\n]+)", data)
    if not match:
        raise ValueError("No recognized explicit BOOT entry in SYSTEM.CNF")
    boot_path = "/" + match[1].decode("ascii").strip().replace("\\", "/")
    boot = entries.get(boot_path.upper())
    if not boot or boot["directory"]:
        raise ValueError("BOOT executable not present in ISO directory")
    executable = cd.read_extent(boot["lba"], boot["size"])
    if len(executable) < 2048 or executable[:8] != b"PS-X EXE":
        raise ValueError("BOOT entry is not a complete PS-X EXE")
    load_address, load_size = struct.unpack_from("<II", executable, 0x18)
    if load_size > len(executable) - 2048:
        raise ValueError("PS-X EXE declared load size exceeds file")
    marker = executable[0x4C:2048].split(b"\x00", 1)[0].decode("ascii", errors="strict")
    return {
        "system_cnf": {**config, "sha256": hashlib.sha256(data).hexdigest()},
        "boot_path": boot_path,
        "executable": {**boot, "sha256": hashlib.sha256(executable).hexdigest()},
        "executable_header": {
            "initial_pc": f"0x{struct.unpack_from('<I', executable, 0x10)[0]:08x}",
            "load_address": f"0x{load_address:08x}",
            "load_size": load_size,
            "marker_offset": "0x4c",
            "marker": marker,
        },
    }


def inspect(chd: Path, raw: Path, cue: Path) -> dict:
    cue_text = cue.read_text(encoding="ascii")
    tracks = re.findall(r"^\s*TRACK\s+(\d+)\s+(\S+)", cue_text, re.MULTILINE)
    if tracks not in ([("01", "MODE2/2352")], [("01", "MODE1/2352")]):
        raise ValueError("Only one raw data track is supported; no layout guessing")
    if not re.search(r"^\s*INDEX 01 00:00:00\s*$", cue_text, re.MULTILINE):
        raise ValueError("Expected track index at raw file offset zero")
    filenames = re.findall(r'^\s*FILE "([^"]+)" BINARY\s*$', cue_text, re.MULTILINE)
    if len(filenames) != 1 or (cue.parent / filenames[0]).resolve() != raw.resolve():
        raise ValueError("CUE must refer to exactly the supplied raw file")
    with raw.open("rb") as stream:
        cd = RawCd(stream, raw.stat().st_size)
        iso = inspect_iso(cd)
        boot = inspect_boot(cd, iso)
    return {
        "schema_version": 1,
        "observation_kind": "original_source_static_metadata",
        "tool": "tools/reference/inspect_disc.py",
        "source": {
            "chd": {"sha256": file_hash(chd), "size": chd.stat().st_size},
            "raw_track": {
                "sha256": file_hash(raw),
                "size": raw.stat().st_size,
                "sector_bytes": RAW_SECTOR_SIZE,
            },
            "cue": {"sha256": file_hash(cue), "track_type": tracks[0][1]},
        },
        "iso9660": iso,
        "boot": boot,
        "limits": [
            "No gameplay execution observed.",
            "No game-specific archive or file meaning recovered.",
            "ISO volume sequence does not establish the game's disc number.",
            "Serial and timestamps do not establish a named retail revision.",
            "This inspector checks bounds and metadata, not CD ECC/EDC authenticity.",
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chd", type=Path, required=True)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--cue", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    local_root = Path(".local").resolve()
    if not args.output.resolve().is_relative_to(local_root):
        parser.error("Measurement reports must first be written under ignored .local/")
    result = inspect(args.chd, args.raw, args.cue)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    print(f"Measured source; private report: {args.output}")


if __name__ == "__main__":
    main()
