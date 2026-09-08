"""Bounded byte views used by the independently reconstructed sprite routines.

These offsets describe original diagnostic storage, not a native runtime API.
Original resources and pointed storage must be supplied explicitly.
"""

from __future__ import annotations

import struct
from collections.abc import Callable

MemoryRead = Callable[[int, int], bytes]


def u32(data: bytes | bytearray, offset: int = 0) -> int:
    if not 0 <= offset <= len(data) - 4:
        raise ValueError("Original word read outside supplied storage")
    return struct.unpack_from("<I", data, offset)[0]


def u16(data: bytes | bytearray, offset: int = 0) -> int:
    if not 0 <= offset <= len(data) - 2:
        raise ValueError("Original halfword read outside supplied storage")
    return struct.unpack_from("<H", data, offset)[0]


def put(data: bytearray, offset: int, value: int, size: int = 4) -> None:
    if size not in (1, 2, 4) or not 0 <= offset <= len(data) - size:
        raise ValueError("Original store outside supplied object storage")
    data[offset : offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


def inside(data: bytes | bytearray, base: int, pointer: int, size: int) -> int:
    offset = pointer - base
    if size < 0 or not 0 <= offset <= len(data) - size:
        raise ValueError("Original pointed storage was not supplied")
    return offset


def read_exact(read: MemoryRead, pointer: int, size: int) -> bytes:
    if size < 0 or not 0 <= pointer <= 0x100000000 - size:
        raise ValueError("Original pointed read outside the address space")
    data = read(pointer, size)
    if not isinstance(data, bytes) or len(data) != size:
        raise ValueError("Incomplete original pointed read")
    return data
