"""Original field bundle and script/collision structures, recovered for analysis.

Original field overlay 0x80070cc8 and its callees define these offsets.
Unknown fields remain bytes. No geometry, script, or gameplay fallback is implied.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

from tools.analysis.packed import PackedBlock, decode_block


class FieldError(ValueError):
    """A source component violates the bounded recovered structure."""


def region(data: bytes, offset: int, size: int, label: str) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise FieldError(f"{label}: range +0x{offset:x}, {size} bytes exceeds {len(data)}")
    return data[offset : offset + size]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack("<I", region(data, offset, 4, "u32le"))[0]


@dataclass(frozen=True)
class FieldComponent:
    index: int
    source_offset: int
    logical_size: int
    decoded: PackedBlock

    @property
    def logical_data(self) -> bytes:
        return self.decoded.data[: self.logical_size]


def field_components(physical_source: bytes) -> tuple[FieldComponent, ...]:
    """Decode the nine source offsets actually read by the original loader.

    logical_size is the field header's size, while the packed stream declares
    its own output size. Original allocation adds 16 bytes to the logical size.
    The final flag group can read into the next component and can write padding.
    Therefore source slices are bounded by the whole measured physical resource,
    not by the next component's start. Decoded padding is retained separately.
    """
    region(physical_source, 0, 0x154, "field header")
    result = []
    for index in range(9):
        logical_size = u32(physical_source, 0x10C + 4 * index)
        offset = u32(physical_source, 0x130 + 4 * index)
        if offset < 0x154 or offset >= len(physical_source):
            raise FieldError(f"component {index}: invalid source offset +0x{offset:x}")
        decoded = decode_block(
            physical_source[offset:], output_limit=min(logical_size + 16, 0x200000)
        )
        if len(decoded.data) < logical_size:
            raise FieldError(f"component {index}: output is shorter than logical size")
        result.append(FieldComponent(index, offset, logical_size, decoded))
    return tuple(result)


@dataclass(frozen=True)
class EventPackage:
    variable_unsigned_bits: bytes
    entries: tuple[tuple[int, ...], ...]
    bytecode: bytes
    bytecode_offset: int

    def entry(self, actor: int, event: int) -> int:
        if not 0 <= actor < len(self.entries) or not 0 <= event < 32:
            raise FieldError(f"event entry outside actor/event table: {actor}/{event}")
        return self.entries[actor][event]


def event_package(logical_data: bytes) -> EventPackage:
    """Read the 128-byte variable type map and 32 u16 entry PCs per actor."""
    bits = region(logical_data, 0, 0x80, "variable type map")
    count = u32(logical_data, 0x80)
    offset = 0x84 + count * 64
    table = region(logical_data, 0x84, count * 64, "actor event table")
    code = region(logical_data, offset, len(logical_data) - offset, "event bytecode")
    if len(code) > 0x10000:
        raise FieldError("bytecode exceeds the original u16 PC address space")
    entries = tuple(struct.unpack_from("<32H", table, actor * 64) for actor in range(count))
    for actor, row in enumerate(entries):
        for event, pc in enumerate(row):
            if pc >= len(code):
                raise FieldError(f"actor {actor} event {event}: PC +0x{pc:x} outside bytecode")
    return EventPackage(bits, entries, code, offset)


@dataclass(frozen=True)
class CollisionTriangle:
    vertices: tuple[int, int, int]
    adjacent_raw: tuple[int, int, int]
    attribute_raw: int


@dataclass(frozen=True)
class CollisionLayer:
    triangles: tuple[CollisionTriangle, ...]
    vertices: tuple[tuple[int, int, int, int], ...]


@dataclass(frozen=True)
class CollisionPackage:
    layers: tuple[CollisionLayer, ...]
    attributes_raw: bytes
    reserved_triangle_sizes: tuple[int, ...]


def collision_package(logical_data: bytes) -> CollisionPackage:
    """Parse the original 14-byte triangle and 8-byte signed vertex records.

    The loader reads four triangle byte counts irrespective of active layer count.
    Adjacency/attribute values are preserved without invented traversal semantics.
    """
    count = u32(logical_data, 0)
    if not 1 <= count <= 4:
        raise FieldError(f"unsupported collision layer count {count}")
    sizes = tuple(u32(logical_data, 4 + i * 4) for i in range(4))
    attribute_offset = u32(logical_data, 0x14)
    pointers = [
        (u32(logical_data, 0x18 + i * 8), u32(logical_data, 0x1C + i * 8)) for i in range(count)
    ]
    layers = []
    for index, (tri_offset, vertex_offset) in enumerate(pointers):
        if sizes[index] % 14:
            raise FieldError(f"layer {index}: triangle bytes are not a multiple of 14")
        vertex_end = pointers[index + 1][0] if index + 1 < count else attribute_offset
        triangles_raw = region(logical_data, tri_offset, sizes[index], "collision triangles")
        if tri_offset < 0x18 + count * 8 or tri_offset + sizes[index] > vertex_offset:
            raise FieldError(f"layer {index}: overlapping header/triangle/vertex tables")
        vertex_bytes = vertex_end - vertex_offset
        if vertex_bytes % 8:
            raise FieldError(f"layer {index}: vertex span is not a multiple of 8")
        raw_vertices = region(logical_data, vertex_offset, vertex_bytes, "collision vertices")
        vertices = tuple(struct.iter_unpack("<4h", raw_vertices))
        triangles = []
        for triangle_index, values in enumerate(struct.iter_unpack("<3h4H", triangles_raw)):
            indices = values[:3]
            if any(v < 0 or v >= len(vertices) for v in indices):
                raise FieldError(f"layer {index} triangle {triangle_index}: invalid vertex index")
            triangles.append(CollisionTriangle(indices, values[3:6], values[6]))
        layers.append(CollisionLayer(tuple(triangles), vertices))
    attributes = region(
        logical_data, attribute_offset, len(logical_data) - attribute_offset, "collision attributes"
    )
    return CollisionPackage(tuple(layers), attributes, sizes[count:])
