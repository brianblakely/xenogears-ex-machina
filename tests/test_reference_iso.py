"""End-to-end metadata checks using an invented, in-memory raw CD.

This fixture contains no Xenogears data and establishes no gameplay behavior.
It tests whether the reference inspector reports the actual boot extent and
rejects structural corruption instead of recording it as valid evidence.
"""

from __future__ import annotations

import hashlib
import io
import struct
import unittest

from tools.reference.inspect_disc import (
    BLOCK_SIZE,
    RAW_SECTOR_SIZE,
    SYNC,
    RawCd,
    inspect_boot,
    inspect_iso,
)


def encoded_integer(value: int, width: int) -> bytes:
    return value.to_bytes(width, "little") + value.to_bytes(width, "big")


def record(name: bytes, lba: int, size: int, *, directory: bool = False) -> bytes:
    data = bytearray((33 + len(name) + 1) & ~1)
    data[0] = len(data)
    data[2:10] = encoded_integer(lba, 4)
    data[10:18] = encoded_integer(size, 4)
    data[25] = 2 if directory else 0
    data[28:32] = encoded_integer(1, 2)
    data[32] = len(name)
    data[33 : 33 + len(name)] = name
    return bytes(data)


class InventedDisc:
    """An ISO with one config file and a deliberately synthetic executable."""

    def __init__(self) -> None:
        self.blocks = [bytearray(BLOCK_SIZE) for _ in range(32)]
        self.config = b"BOOT = cdrom:\\TEST.EXE;1\r\n"
        self.executable = bytearray(BLOCK_SIZE * 2)
        self.executable[:8] = b"PS-X EXE"
        struct.pack_into("<I", self.executable, 0x10, 0x80012000)
        struct.pack_into("<II", self.executable, 0x18, 0x80012000, BLOCK_SIZE)
        marker = b"Invented metadata fixture; not an original game"
        self.executable[0x4C : 0x4C + len(marker)] = marker
        self.executable[BLOCK_SIZE:] = bytes(range(256)) * 8
        self.blocks[21][: len(self.config)] = self.config
        self.blocks[22][:] = self.executable[:BLOCK_SIZE]
        self.blocks[23][:] = self.executable[BLOCK_SIZE:]
        self.root_records = [
            record(b"\x00", 20, BLOCK_SIZE, directory=True),
            record(b"\x01", 20, BLOCK_SIZE, directory=True),
            record(b"SYSTEM.CNF;1", 21, len(self.config)),
            record(b"TEST.EXE;1", 22, len(self.executable)),
        ]
        self.write_root()
        pvd = self.blocks[16]
        pvd[:7] = b"\x01CD001\x01"
        pvd[8:40] = b"SYNTHETIC".ljust(32, b" ")
        pvd[40:72] = b"INSPECTOR TEST".ljust(32, b" ")
        pvd[80:88] = encoded_integer(32, 4)
        pvd[120:124] = encoded_integer(1, 2)
        pvd[124:128] = encoded_integer(1, 2)
        pvd[128:132] = encoded_integer(BLOCK_SIZE, 2)
        pvd[156:190] = record(b"\x00", 20, BLOCK_SIZE, directory=True)

    def write_root(self) -> None:
        data = b"".join(self.root_records)
        if len(data) > BLOCK_SIZE:
            raise ValueError("Test root is larger than one block")
        self.blocks[20][:] = data.ljust(BLOCK_SIZE, b"\x00")

    def reader(self) -> RawCd:
        raw = bytearray()
        for payload in self.blocks:
            sector = bytearray(RAW_SECTOR_SIZE)
            sector[:12] = SYNC
            sector[15] = 2
            sector[24 : 24 + BLOCK_SIZE] = payload
            raw.extend(sector)
        return RawCd(io.BytesIO(raw), len(raw))


class IsoMetadataTests(unittest.TestCase):
    def test_complete_iso_and_boot_measurement(self) -> None:
        fixture = InventedDisc()
        reader = fixture.reader()
        iso = inspect_iso(reader)
        self.assertEqual(iso["volume_id"], "INSPECTOR TEST")
        self.assertEqual(
            [(entry["path"], entry["lba"], entry["size"]) for entry in iso["entries"]],
            [("/SYSTEM.CNF;1", 21, len(fixture.config)), ("/TEST.EXE;1", 22, 4096)],
        )
        boot = inspect_boot(reader, iso)
        self.assertEqual(boot["boot_path"], "/TEST.EXE;1")
        self.assertEqual(
            boot["executable"]["sha256"], hashlib.sha256(fixture.executable).hexdigest()
        )
        self.assertEqual(boot["system_cnf"]["sha256"], hashlib.sha256(fixture.config).hexdigest())
        self.assertEqual(boot["executable_header"]["load_address"], "0x80012000")
        self.assertEqual(boot["executable_header"]["load_size"], BLOCK_SIZE)

    def test_descriptor_rejects_wrong_signature_block_size_and_volume(self) -> None:
        for start, data in (
            (1, b"BAD!!"),
            (128, encoded_integer(512, 2)),
            (80, encoded_integer(33, 4)),
            (80, b"\x20\x00\x00\x00\x00\x00\x00\x21"),
        ):
            with self.subTest(offset=start, data=data):
                fixture = InventedDisc()
                fixture.blocks[16][start : start + len(data)] = data
                with self.assertRaises(ValueError):
                    inspect_iso(fixture.reader())

    def test_root_must_be_a_directory(self) -> None:
        fixture = InventedDisc()
        fixture.blocks[16][156 + 25] = 0
        with self.assertRaises(ValueError):
            inspect_iso(fixture.reader())

    def test_root_extent_must_fit_declared_volume_even_if_physical_image_is_larger(self) -> None:
        fixture = InventedDisc()
        fixture.blocks[26][:] = fixture.blocks[20]
        fixture.blocks[16][80:88] = encoded_integer(24, 4)
        fixture.blocks[16][156:190] = record(b"\x00", 26, BLOCK_SIZE, directory=True)
        with self.assertRaises(ValueError):
            inspect_iso(fixture.reader())

    def test_directory_cycle_is_rejected(self) -> None:
        fixture = InventedDisc()
        fixture.root_records.append(record(b"LOOP", 20, BLOCK_SIZE, directory=True))
        fixture.write_root()
        with self.assertRaisesRegex(ValueError, "Cycle|aliased"):
            inspect_iso(fixture.reader())

    def test_file_extent_cannot_exceed_declared_volume(self) -> None:
        fixture = InventedDisc()
        fixture.root_records[-1] = record(b"TEST.EXE;1", 31, BLOCK_SIZE * 2)
        fixture.write_root()
        with self.assertRaises(ValueError):
            inspect_iso(fixture.reader())

    def test_boot_requires_present_configuration_and_target(self) -> None:
        for removed_name in (b"SYSTEM.CNF;1", b"TEST.EXE;1"):
            with self.subTest(removed_name=removed_name):
                fixture = InventedDisc()
                fixture.root_records = [
                    row for row in fixture.root_records if removed_name not in row
                ]
                fixture.write_root()
                reader = fixture.reader()
                with self.assertRaises(ValueError):
                    inspect_boot(reader, inspect_iso(reader))

    def test_boot_rejects_invalid_magic_and_truncated_load_payload(self) -> None:
        for offset, replacement in ((0, b"NOT EXE!"), (0x1C, struct.pack("<I", BLOCK_SIZE + 1))):
            with self.subTest(offset=offset):
                fixture = InventedDisc()
                fixture.blocks[22][offset : offset + len(replacement)] = replacement
                reader = fixture.reader()
                with self.assertRaises(ValueError):
                    inspect_boot(reader, inspect_iso(reader))


if __name__ == "__main__":
    unittest.main()
