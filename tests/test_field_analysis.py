"""Original synthetic tooling fixtures, independent of any game payload.

These test parser bounds and recovered representation rules. They do not serve
as original-game behavioral oracles; those comparisons run locally.
"""

from __future__ import annotations

import struct
import unittest

from tools.analysis.field import (
    FieldError,
    collision_package,
    event_package,
    field_components,
    region,
)
from tools.analysis.packed import PackedError, decode_block


def literal_block(data: bytes) -> bytes:
    """Author a fixture of complete groups, plus the observed exit prefetch."""
    if len(data) % 8:
        raise ValueError("fixture needs complete eight-literal groups")
    return (
        struct.pack("<I", len(data))
        + b"".join(b"\0" + data[start : start + 8] for start in range(0, len(data), 8))
        + b"\xa5"
    )


def synthetic_field() -> bytes:
    field = bytearray(0x154)
    for index in range(9):
        struct.pack_into("<I", field, 0x10C + index * 4, 5)
        struct.pack_into("<I", field, 0x130 + index * 4, len(field))
        field += literal_block(bytes([index]) * 8)
    return bytes(field)


def synthetic_events() -> bytes:
    # Different rows and last entries catch row-stride and PC-base mistakes.
    return (
        bytes(128)
        + struct.pack("<I", 2)
        + struct.pack("<32H", *range(32))
        + struct.pack("<32H", *range(32, 64))
        + bytes(range(64))
    )


def synthetic_collision() -> bytes:
    # Two independent triangle/vertex spans with opaque sentinel attributes.
    data = bytearray(0x28)
    struct.pack_into("<6I", data, 0, 2, 14, 14, 0x10203040, 0, 116)
    struct.pack_into("<4I", data, 0x18, 40, 54, 78, 92)
    for height in (-32768, 32767):
        data += struct.pack("<3h4H", 0, 1, 2, 0xFFFF, 0x8001, 0, 0xF234)
        data += struct.pack("<12h", -30, height, -20, -1, 30, height, 0, 9, 0, height, 40, 2)
    return bytes(data) + b"opaque attributes"


class PackedTests(unittest.TestCase):
    def test_complete_groups_and_terminal_prefetch(self) -> None:
        source = literal_block(b"abcdefghABCDEFGH")
        block = decode_block(source + b"unused")
        self.assertEqual(block.data, b"abcdefghABCDEFGH")
        self.assertEqual((block.token_bytes, block.source_bytes_read, block.groups), (22, 23, 2))

    def test_empty_output_still_reads_flag(self) -> None:
        result = decode_block(bytes(4) + b"\xff")
        self.assertEqual((result.data, result.token_bytes, result.source_bytes_read), (b"", 4, 5))
        with self.assertRaisesRegex(PackedError, "truncated"):
            decode_block(bytes(4))

    def test_flag_order_and_overlapping_copy(self) -> None:
        source = struct.pack("<I", 10) + b"\x02a\x01\x00BCDEFG\xa5"
        self.assertEqual(decode_block(source).data, b"aaaaBCDEFG")

    def test_adjacent_memory_is_an_input_not_invented_disc_padding(self) -> None:
        # An authored example of the original final-group boundary issue.
        source_file = struct.pack("<I", 8) + b"\0ABCD"
        disc_context = decode_block(source_file + bytes(5))
        ram_context = decode_block(source_file + b"wxyz\xa5")
        self.assertEqual(disc_context.data[:4], ram_context.data[:4])
        self.assertEqual(ram_context.data, b"ABCDwxyz")
        self.assertNotEqual(disc_context.data, ram_context.data)
        self.assertEqual(disc_context.source_bytes_read, ram_context.source_bytes_read)
        with self.assertRaisesRegex(PackedError, "truncated"):
            decode_block(source_file)

    def test_full_distance_and_length_fields(self) -> None:
        prefix = bytes(range(256)) * 16
        literals = literal_block(prefix)[:-1]
        source = struct.pack("<I", 4121) + literals[4:] + b"\x01\xff\xff1234567\0"
        self.assertEqual(decode_block(source).data, prefix + prefix[1:19] + b"1234567")

    def test_every_truncation_is_rejected(self) -> None:
        source = struct.pack("<I", 10) + b"\x02a\x01\x00BCDEFG\0"
        for size in range(len(source)):
            with self.subTest(size=size), self.assertRaises(PackedError):
                decode_block(source[:size])

    def test_unsafe_original_paths_are_explicit(self) -> None:
        cases = [
            (struct.pack("<I", 10) + b"\x02a\0\0BCDEFG\0", "distance 0"),
            (struct.pack("<I", 10) + b"\x02a\x02\0BCDEFG\0", "distance 2"),
            (struct.pack("<I", 8) + b"\x02a\x01\xf0BCDEFG\0", "copy crosses"),
            (struct.pack("<I", 7) + b"\0abcdefgh\0", "group crosses"),
        ]
        for source, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(PackedError, message):
                decode_block(source)
        with self.assertRaisesRegex(PackedError, "exceeds bound"):
            decode_block(literal_block(b"12345678"), output_limit=7)
        with self.assertRaisesRegex(PackedError, "negative output limit"):
            decode_block(literal_block(b""), output_limit=-1)


class FieldStructureTests(unittest.TestCase):
    def test_logical_and_decoder_sizes_are_separate(self) -> None:
        parts = field_components(synthetic_field())
        self.assertEqual(len(parts), 9)
        for index, part in enumerate(parts):
            self.assertEqual(part.logical_data, bytes([index]) * 5)
            self.assertEqual(part.decoded.data, bytes([index]) * 8)

    def test_source_is_bounded_by_measured_resource(self) -> None:
        source = synthetic_field()
        with self.assertRaisesRegex(PackedError, "truncated"):
            field_components(source[:-1])
        for offset, value, message in [(0x130, 0, "offset"), (0x10C, 9, "shorter")]:
            changed = bytearray(source)
            struct.pack_into("<I", changed, offset, value)
            with self.subTest(offset=offset), self.assertRaisesRegex(FieldError, message):
                field_components(changed)

    def test_negative_and_overflowing_regions_fail(self) -> None:
        for offset, size in [(-1, 1), (0, -1), (4, 1), (1 << 63, 1), (1, 1 << 63)]:
            with self.subTest(offset=offset, size=size), self.assertRaises(FieldError):
                region(b"1234", offset, size, "synthetic")
        self.assertEqual(region(b"1234", 4, 0, "empty"), b"")

    def test_event_rows_and_relative_pc(self) -> None:
        events = event_package(synthetic_events())
        self.assertEqual(events.bytecode_offset, 0x104)
        self.assertEqual(events.entry(0, 31), 31)
        self.assertEqual(events.entry(1, 31), 63)
        self.assertEqual(events.bytecode[events.entry(1, 0)], 32)
        for actor, event in [(-1, 0), (2, 0), (0, -1), (0, 32)]:
            with self.subTest(actor=actor, event=event), self.assertRaises(FieldError):
                events.entry(actor, event)

    def test_event_tables_and_targets_are_bounded(self) -> None:
        source = synthetic_events()
        for size in range(len(source)):
            with self.subTest(size=size), self.assertRaises(FieldError):
                event_package(source[:size])
        changed = bytearray(source)
        struct.pack_into("<I", changed, 0x80, 0xFFFFFFFF)
        with self.assertRaisesRegex(FieldError, "actor event table"):
            event_package(changed)
        with self.assertRaisesRegex(FieldError, "u16 PC"):
            event_package(source[:0x104] + bytes(0x10001))

    def test_collision_layers_signed_vertices_and_opaque_fields(self) -> None:
        collision = collision_package(synthetic_collision())
        self.assertEqual(len(collision.layers), 2)
        self.assertEqual(collision.layers[0].vertices[0], (-30, -32768, -20, -1))
        self.assertEqual(collision.layers[1].vertices[0], (-30, 32767, -20, -1))
        triangle = collision.layers[0].triangles[0]
        self.assertEqual(triangle.vertices, (0, 1, 2))
        self.assertEqual(triangle.adjacent_raw, (0xFFFF, 0x8001, 0))
        self.assertEqual(triangle.attribute_raw, 0xF234)
        self.assertEqual(collision.reserved_triangle_sizes, (0x10203040, 0))
        self.assertEqual(collision.attributes_raw, b"opaque attributes")

    def test_collision_malformed_structures(self) -> None:
        cases = [
            (0, 5, "layer count"),
            (4, 13, "multiple of 14"),
            (0x18, 0, "overlapping"),
            (0x1C, 53, "overlapping"),
            (0x20, 79, "multiple of 8"),
            (0x14, 0xFFFFFFFF, "multiple of 8|exceeds"),
        ]
        for offset, value, message in cases:
            changed = bytearray(synthetic_collision())
            struct.pack_into("<I", changed, offset, value)
            with self.subTest(offset=offset), self.assertRaisesRegex(FieldError, message):
                collision_package(changed)
        for vertex in (-1, 3):
            changed = bytearray(synthetic_collision())
            struct.pack_into("<h", changed, 40, vertex)
            with self.subTest(vertex=vertex), self.assertRaisesRegex(FieldError, "vertex index"):
                collision_package(changed)


if __name__ == "__main__":
    unittest.main()
