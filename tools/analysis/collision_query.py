"""Original field triangle-adjacency queries, 8007bef4 and 8007c694.

Results describe writes to caller-owned point, edge and attribute outputs. None
means that output is untouched. The query never updates the actor's triangle.
Source bytes preceding a triangle table remain significant for a -1 neighbor.
"""

from __future__ import annotations

import struct
from collections.abc import Sequence
from dataclasses import dataclass

from .arithmetic import signed16, signed32
from .collision_math import Vector, height_and_normal
from .field import collision_package, region

QueryVector = tuple[int, int | None, int]
Edge = tuple[Vector, Vector]


def packed_xz(x: int, z: int) -> int:
    """Original ADDU of shifted X and signed Z, including borrow for negative Z."""
    return ((x << 16) + z) & 0xFFFFFFFF


def packed_area(points: tuple[int, int, int]) -> int:
    """Signed low MAC0 word of NCLIP for three already packed coordinates."""
    z = [signed16(value) for value in points]
    x = [signed16(value >> 16) for value in points]
    return signed32(z[0] * (x[1] - x[2]) + z[1] * (x[2] - x[0]) + z[2] * (x[0] - x[1]))


@dataclass(frozen=True)
class QueryActor:
    flags: int
    layer_flags: int
    layer: int
    triangle: int
    position: Vector


@dataclass(frozen=True)
class AreaCall:
    site: int
    points: tuple[int, int, int]
    value: int


@dataclass(frozen=True)
class HeightCall:
    site: str
    triangle: int
    vertices: tuple[Vector, Vector, Vector]
    point: Vector
    normal: Vector


@dataclass(frozen=True)
class TriangleStep:
    triangle: int
    counter: int
    mask: int
    target: int
    origin: int
    vertices: tuple[Vector, Vector, Vector]
    next_triangle: int
    edge_mask: int
    terrain: int


@dataclass(frozen=True)
class AttributeRead:
    triangle: int
    index: int
    source_word: int
    effective_word: int


@dataclass(frozen=True)
class QueryResult:
    value: int
    point: Vector | None
    edge: Edge | None
    attribute: int | None
    terminal_triangle: int
    counter: int | None
    mask: int | None
    attribute_mask: int | None
    initial_special: bool | None
    reason: str
    steps: tuple[TriangleStep, ...]
    areas: tuple[AreaCall, ...]
    heights: tuple[HeightCall, ...]
    attribute_reads: tuple[AttributeRead, ...]


def collision_query(
    component: bytes,
    reciprocal_table: Sequence[int],
    actor: QueryActor,
    candidate: QueryVector,
    *,
    ordinary: bool,
    mode: int,
    attribute_control: int,
) -> QueryResult:
    """Walk original adjacency order and apply the selected query's terrain rules.

    Candidate Y is never read and may be None for an unwritten probe-stack word.
    Mode -1 skips the final height calculation; mode 0x80 bypasses the ordinary
    query's height rejection when entering a special surface. Other modes still
    compute height. Bounds errors describe unresolved original accesses, not validation
    performed by the original program.
    """

    def word(offset: int) -> int:
        return struct.unpack("<I", region(component, offset, 4, "collision query word"))[0]

    layer = signed16(actor.layer)
    if not 0 <= layer < word(0) or layer >= 4:
        raise ValueError("unresolved original collision query layer access")
    triangle_base = word(0x18 + 8 * layer)
    vertex_base = word(0x1C + 8 * layer)
    attribute_base = word(0x14)
    triangle = signed16(actor.triangle)
    point, edge, attribute = None, None, None
    counter, mask, attribute_mask, initial_special = None, None, None, None
    steps, areas, heights, attribute_reads = [], [], [], []

    def check_triangle(index: int) -> None:
        if not 0 <= index < len(mesh.triangles):
            raise ValueError(f"layer {layer} triangle {index}: index outside its triangle table")

    def triangle_bytes(index: int) -> bytes:
        check_triangle(index)
        return region(component, triangle_base + index * 14, 14, "collision query triangle")

    def vertices(index: int) -> tuple[Vector, Vector, Vector]:
        indices = struct.unpack_from("<3h", triangle_bytes(index))
        return tuple(
            struct.unpack_from(
                "<3h", region(component, vertex_base + vertex * 8, 8, "collision query vertex")
            )
            for vertex in indices
        )

    def terrain(index: int) -> int:
        # Both routines read this byte before testing a newly selected -1 index.
        # Only that sentinel may read before the validated triangle table.
        if index != -1:
            check_triangle(index)
        offset = triangle_base + index * 14 + 12
        identifier = region(component, offset, 1, "collision query attribute index")[0]
        source_word = word(attribute_base + identifier * 4)
        effective = source_word & attribute_mask
        attribute_reads.append(AttributeRead(index, identifier, source_word, effective))
        return effective

    def area(points: tuple[int, int, int], site: int) -> int:
        value = packed_area(points)
        areas.append(AreaCall(site, points, value))
        return value

    def height(index: int, site: str) -> int:
        nonlocal point
        source_vertices = vertices(index)
        y, normal = height_and_normal(source_vertices, point[0], point[2], reciprocal_table)
        point = (point[0], y, point[2])
        heights.append(HeightCall(site, index, source_vertices, point, normal))
        return y

    def result(value: int, reason: str) -> QueryResult:
        return QueryResult(
            value,
            point,
            edge,
            attribute,
            triangle,
            counter,
            mask,
            attribute_mask,
            initial_special,
            reason,
            tuple(steps),
            tuple(areas),
            tuple(heights),
            tuple(attribute_reads),
        )

    if triangle == -1:
        return result(-1, "initial-triangle-missing")
    # Reconstruction safety checks, not checks performed by the original routine.
    # Whole-component bounds can admit triangle/vertex indices into adjacent tables.
    mesh = collision_package(component).layers[layer]
    check_triangle(triangle)
    x = signed32(actor.position[0] + candidate[0]) >> 16
    z = signed32(actor.position[2] + candidate[2]) >> 16
    point = (signed16(x), 0, signed16(z))
    target = packed_xz(x, z)
    origin = packed_xz(signed32(actor.position[0]) >> 16, signed32(actor.position[2]) >> 16)
    disabled = bool((actor.layer_flags >> ((layer + 3) & 31)) & 1)
    attribute_mask = 0 if disabled or attribute_control & 0xFF else 0xFFFFFFFF
    mode = signed32(mode)
    if ordinary:
        initial_special = bool(terrain(triangle) & 0x400000 or mode == 0x80)
    counter = 0
    reason = ""
    while True:
        prior_triangle, prior_counter = triangle, counter
        source_vertices = vertices(triangle)
        packed = tuple(packed_xz(v[0], v[2]) for v in source_vertices)
        values = [area((packed[i], packed[(i + 1) % 3], target), i) for i in range(3)]
        mask = sum((value < 0) << index for index, value in enumerate(values))
        original_mask = mask
        if mask == 0:
            counter = 255
        elif mask in (1, 2, 4):
            triangle = struct.unpack_from(
                "<h", triangle_bytes(prior_triangle), {1: 6, 2: 8, 4: 10}[mask]
            )[0]
        elif mask in (3, 5, 6):
            vertex, site = {3: (1, 3), 5: (0, 4), 6: (2, 5)}[mask]
            value = area((packed[vertex], target, origin), site)
            mask = {3: 1, 5: 4, 6: 2}[mask] if value < 0 else {3: 2, 5: 1, 6: 4}[mask]
            triangle = struct.unpack_from(
                "<h", triangle_bytes(prior_triangle), {1: 6, 2: 8, 4: 10}[mask]
            )[0]
        else:
            triangle = -1
        value = terrain(triangle)
        if ordinary:
            attribute = value
        steps.append(
            TriangleStep(
                prior_triangle,
                prior_counter,
                original_mask,
                target,
                origin,
                source_vertices,
                triangle,
                mask,
                value,
            )
        )
        flags = actor.flags & 0xFFFFFFFF
        if (not ordinary and (((flags >> 9) & 3) & (value >> 3))) or (
            ((flags >> 8) & 7) & (value >> 5)
        ):
            triangle, reason = -1, "actor-terrain-flags"
            break
        if value & 0x800000 and layer == 0:
            triangle, reason = -1, "layer-zero-terrain"
            break
        if ordinary and value & 0x400000 and not initial_special:
            if height(triangle, "terrain") < signed16(signed32(actor.position[1]) >> 16):
                triangle, reason = -1, "terrain-floor-height"
                break
        if triangle == -1:
            reason = "missing-neighbor"
            break
        counter += 1
        if counter >= 32:
            reason = "iteration-limit" if counter == 32 else "inside"
            break
    if triangle != -1 and counter != 32:
        if mode != -1:
            height(triangle, "final")
        return result(0, "probe" if mode == -1 else "height")
    if mask in (1, 2, 4):
        first, second = {1: (0, 1), 2: (1, 2), 4: (2, 0)}[mask]
        edge = (vertices(prior_triangle)[first], vertices(prior_triangle)[second])
    return result(-1, reason)
