"""Invented sprite objects and resource bytes; no original assets or tables."""

import struct

from tools.analysis.sprite_state import put

ADDRESS = 0x80001000
RESOURCE = 0x80010000
HEADER = RESOURCE + 0xC0


def invented_sprite():
    sprite = bytearray(512)
    for offset, value in (
        (0x20, ADDRESS + 0xB4),
        (0x24, ADDRESS + 0x110),
        (0x3C, 1),
        (0x44, RESOURCE),
        (0x48, RESOURCE),
        (0x54, RESOURCE + 0x170),
        (0x58, HEADER),
        (0x64, RESOURCE + 0x180),
        (0xA8, (2 << 20) | (2 << 17)),
        (0xAC, 256 << 7),
        (0xE8, ADDRESS + 0x140),
        (0x110, RESOURCE + 0x20),
        (0x120, RESOURCE + 0x80),
    ):
        put(sprite, offset, value)
    put(sprite, 0x82, 4096, 2)
    put(sprite, 0x8C, 16, 1)
    struct.pack_into("<3h", sprite, 0xBA, 4096, 4096, 4096)
    resource = bytearray(512)
    put(resource, 4, 0x80)
    put(resource, 0x20, 3, 2)
    for frame, offset in ((1, 0x10), (2, 0x30), (3, 0x50)):
        put(resource, 0x20 + frame * 2, offset, 2)
    put(resource, 0x80, 1, 2)
    put(resource, 0x82, 0x40, 2)
    struct.pack_into("<2H", resource, 0xC0, 0x42, 0xBE)
    for group in range(5):
        at = 0xC4 + group * 2
        put(resource, at, 0x160 + group * 8 - at, 2)
        for frame in range(4):
            put(resource, 0x160 + group * 8 + frame * 2, frame + 1, 2)
    resource[0x180] = 0x80
    table = struct.pack("<2h", 0, 4096) * 4096
    return sprite, resource, table


def reader(base, data):
    def read(pointer, size):
        offset = pointer - base
        if not 0 <= offset <= len(data) - size:
            raise ValueError("Invented resource read outside supplied block")
        return bytes(data[offset : offset + size])

    return read


def forbidden_read(pointer, size):
    raise AssertionError(f"Unexpected fixture read at {pointer:#x}, size {size}")
