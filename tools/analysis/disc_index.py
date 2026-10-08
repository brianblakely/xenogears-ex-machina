"""The original disc files as the resident file routines address them.

Resident 80028230 reads the file index (LBA 0x18: seven-byte records, a 24-bit
start sector and a signed size; a negative size heads a sub-directory of that
many entries) and the directory table (LBA 0x28, 0x7A bytes: the 1-based first
index entry of each directory). 80028470(group, index) selects directory
`table[group + index] - 1`; 800295d8 then reads file `n` of it from index
entry `n + selected - 1`. Inputs are the user's raw tracks and the manifests of
tools/extraction/disc_files.py under .local/; nothing here is original data.
"""

from __future__ import annotations

import json
import struct
from functools import cached_property
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SECTOR = 2352
USER_OFFSET = 24
DIRECTORY_LBA = 0x28
DIRECTORY_BYTES = 0x7A


class Disc:
    """One disc: its manifest, raw track, boot program and directory table."""

    def __init__(self, number: int, root: Path = ROOT):
        self.number = number
        self.track = root / f".local/discs/disc{number}.bin"
        extract = root / f".local/extract/disc{number}"
        self.manifest = json.loads((extract / "manifest.json").read_text())
        self.boot_path = extract / self.manifest["boot"]["name"]
        self.entries = {entry["slot"]: entry for entry in self.manifest["files"]}

    def read_sectors(self, lba: int, count: int) -> bytes:
        out = bytearray()
        with self.track.open("rb") as track:
            for n in range(count):
                track.seek((lba + n) * SECTOR + USER_OFFSET)
                out += track.read(2048)
        return bytes(out)

    @cached_property
    def directories(self) -> tuple[int, ...]:
        data = self.read_sectors(DIRECTORY_LBA, 1)[:DIRECTORY_BYTES]
        return struct.unpack(f"<{DIRECTORY_BYTES // 2}H", data)

    @cached_property
    def boot(self) -> bytes:
        return self.boot_path.read_bytes()

    def slot(self, group: int, index: int, file: int) -> int:
        """Index entry of `file` after 80028470(group, index)."""
        return file + self.directories[group + index] - 2

    def files(self) -> list[dict]:
        """Index entries of files (positive sizes), in slot order."""
        return [entry for entry in self.manifest["files"] if entry["size"] > 0]

    def sectors(self, slot: int) -> bytes:
        """The file's whole sectors as the drive delivers them (packed streams
        may read their final flag byte from the sector padding)."""
        entry = self.entries[slot]
        return self.read_sectors(entry["lba"], (entry["size"] + 2047) // 2048)

    def data(self, slot: int) -> bytes:
        return self.sectors(slot)[: self.entries[slot]["size"]]


def discs(root: Path = ROOT) -> list[Disc]:
    missing = [n for n in (1, 2) if not (root / f".local/discs/disc{n}.bin").is_file()]
    if missing:
        raise SystemExit(
            f"missing raw track(s) {missing} under .local/discs and extraction under "
            ".local/extract (tools/extraction/disc_files.py)"
        )
    return [Disc(1, root), Disc(2, root)]
