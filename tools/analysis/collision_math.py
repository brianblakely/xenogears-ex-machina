"""Source reconstruction of original field point location and height calculation.

Game-specific source: field 0x8007b07c/0x8007b1c4; resident 0x80048d7c,
0x80048dd8 and 0x8004a480. General GTE arithmetic was checked against the exact
pinned external observation core; that is emulator behavior, not hardware proof.
The original normalization lookup table is user-supplied data, never embedded.
"""

from __future__ import annotations

from collections.abc import Sequence

from tools.analysis.arithmetic import divide32, signed16, signed32
from tools.analysis.field import CollisionLayer

Vector = tuple[int, int, int]


def packed_coordinate(x: int, z: int) -> tuple[int, int]:
    # Original ADDU of a shifted x and signed z, not concatenation with OR.
    packed = ((x << 16) + z) & 0xFFFFFFFF
    return signed16(packed), signed16(packed >> 16)


def edge_area(a: Vector, b: Vector, x: int, z: int) -> int:
    """Original GTE MAC0 signed result for one directed edge and query."""
    az, ax = packed_coordinate(a[0], a[2])
    bz, bx = packed_coordinate(b[0], b[2])
    pz, px = packed_coordinate(x, z)
    # Determinant form, independently expressed from the command's intent.
    return signed32((bz - az) * (px - ax) - (bx - ax) * (pz - az))


def normalize(vector: Vector, reciprocal_table: Sequence[int]) -> Vector:
    """Original integer normalization, including lookup quantization."""
    values = tuple(signed16(v) for v in vector)
    magnitude = sum(v * v for v in values)
    if magnitude == 0:
        raise ValueError("unresolved original zero-vector normalization table read")
    if magnitude > 0x7FFFFFFF:
        raise ValueError("original normalization ADD would overflow signed32")
    even_leading_zeroes = (32 - magnitude.bit_length()) & ~1
    scale_shift = (31 - even_leading_zeroes) >> 1
    table_shift = even_leading_zeroes - 24
    normalized_magnitude = (
        magnitude << table_shift if table_shift >= 0 else magnitude >> -table_shift
    )
    index = normalized_magnitude - 64
    if not 0 <= index < len(reciprocal_table):
        raise ValueError(f"normalization table index {index} outside supplied original table")
    multiplier = signed16(reciprocal_table[index])
    return tuple(signed32(multiplier * v) >> (scale_shift & 31) for v in values)


def height_and_normal(
    vertices: tuple[Vector, Vector, Vector],
    x: int,
    z: int,
    reciprocal_table: Sequence[int],
) -> tuple[int, Vector]:
    a, b, c = vertices
    ab = normalize(tuple(b[i] - a[i] for i in range(3)), reciprocal_table)
    ac = normalize(tuple(c[i] - a[i] for i in range(3)), reciprocal_table)
    normal = tuple(
        signed32((ab[(i + 1) % 3] * ac[(i + 2) % 3] - ab[(i + 2) % 3] * ac[(i + 1) % 3]) >> 12)
        for i in range(3)
    )
    if normal[1] == 0:
        return 0, normal
    first = signed32(normal[0] * (signed16(x) - a[0]))
    second = signed32(normal[2] * (signed16(z) - a[2]))
    numerator = signed32(-first - second)
    return signed16(a[1] + divide32(numerator, normal[1])), normal


def locate(
    layer: CollisionLayer, x: int, z: int, reciprocal_table: Sequence[int]
) -> tuple[int, Vector, Vector] | None:
    """Return the first original-order containing triangle and interpolated point.

    The original no-match return ambiguously uses triangle zero and zero outputs.
    Analysis exposes None so callers cannot confuse failure with valid triangle 0.
    """
    for index, triangle in enumerate(layer.triangles):
        vertices = tuple(layer.vertices[v][:3] for v in triangle.vertices)
        if all(edge_area(vertices[i], vertices[(i + 1) % 3], x, z) >= 0 for i in range(3)):
            height, normal = height_and_normal(vertices, x, z, reciprocal_table)
            return index, (signed16(x), height, signed16(z)), normal
    return None
