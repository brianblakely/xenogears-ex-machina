"""Bounded reconstruction of the original eight-token packed-block decoder.

Derived only from original resident instructions at 0x80032eb4..0x80032f50
in the two hash-defined reference executables. This is analysis source, not a
production asset importer. EVID-REF-014 documents its original-data validation.
"""

from __future__ import annotations

from dataclasses import dataclass


class PackedError(ValueError):
    """The supplied block cannot follow the recovered decoder safely."""


@dataclass(frozen=True)
class PackedBlock:
    data: bytes
    token_bytes: int
    source_bytes_read: int
    groups: int


def decode_block(source: bytes, *, output_limit: int = 0x200000) -> PackedBlock:
    """Decode complete flag groups, preserving overlapping byte copies.

    The original checks output equality only between groups and speculatively
    reads the following flag even on exit. token_bytes excludes that final
    read; source_bytes_read includes it. Callers must supply measured physical
    source bytes, including real padding, and must never synthesize padding to
    make a truncated resource pass. Unsafe original reads/writes raise instead
    of reproducing undefined memory access.
    """
    if output_limit < 0:
        raise PackedError("negative output limit")
    if len(source) < 4:
        raise PackedError("missing u32le output length at source +0x0")
    size = int.from_bytes(source[:4], "little")
    if size > output_limit:
        raise PackedError(f"declared output {size} exceeds bound {output_limit}")
    position, groups = 4, 0
    output = bytearray()

    def read_byte() -> int:
        nonlocal position
        if position >= len(source):
            raise PackedError(f"source truncated at +0x{position:x}")
        value = source[position]
        position += 1
        return value

    while True:
        flag_position = position
        flags = read_byte()
        if len(output) == size:
            return PackedBlock(bytes(output), flag_position, position, groups)
        groups += 1
        for token in range(8):
            token_offset = position
            low = read_byte()
            if flags & (1 << token):
                high = read_byte()
                distance = low | ((high & 0x0F) << 8)
                length = (high >> 4) + 3
                if not 1 <= distance <= len(output):
                    raise PackedError(
                        f"invalid backward distance {distance} at +0x{token_offset:x}"
                    )
                if length > size - len(output):
                    raise PackedError(f"copy crosses output length at +0x{token_offset:x}")
                for _ in range(length):
                    output.append(output[-distance])
            else:
                if len(output) >= size:
                    raise PackedError(f"flag group crosses output length at +0x{token_offset:x}")
                output.append(low)
