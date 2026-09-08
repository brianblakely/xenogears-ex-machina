"""Source reconstruction of the field sweep's integer geometry helpers.

Original resident 8004b32c/80048c4c, field 80099a4c/8007b6c4/8007bd04.
Lookup tables are supplied original data. These functions describe arithmetic
results, not GTE register side effects, hardware timing, or a native controller.
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass

from .arithmetic import divide32, signed16, signed32
from .collision_math import Vector, normalize


@dataclass(frozen=True)
class AngleResult:
    angle: int
    table_index: int | None
    division_path: str


def atan_angle(z: int, x: int, table: Sequence[int]) -> AngleResult:
    """8004b32c: quadrant correction around a 1025-entry signed lookup table.

    Absolute values, shifted numerators and quadrant arithmetic wrap to 32 bits.
    The original explicitly executes BREAK on invalid division; analysis rejects
    that path instead of supplying a direction or emulating an exception handler.
    """
    if len(table) != 1025:
        raise ValueError("original atan table must contain 1025 signed halfwords")
    z, x = signed32(z), signed32(x)
    negative_z, negative_x = z < 0, x < 0
    if negative_z:
        z = signed32(-z)
    if negative_x:
        x = signed32(-x)
    if z == x == 0:
        return AngleResult(0, None, "zero")
    z_smaller = z < x
    small, large = (z, x) if z_smaller else (x, z)
    reduced = bool(small & 0x7FE00000)
    numerator, denominator = (small, large >> 10) if reduced else (signed32(small << 10), large)
    if denominator == 0:
        raise ValueError("unresolved original atan BREAK 7 path")
    if numerator == -0x80000000 and denominator == -1:
        raise ValueError("unresolved original atan BREAK 6 path")
    index = divide32(numerator, denominator)
    if not 0 <= index < len(table):
        raise ValueError("original atan lookup outside the supplied table")
    angle = signed16(table[index])
    if not z_smaller:
        angle = signed32(1024 - angle)
    if negative_x:
        angle = signed32(2048 - angle)
    if negative_z:
        angle = signed32(-angle)
    path = ("z-smaller" if z_smaller else "x-smaller") + ("-reduced" if reduced else "-shifted")
    return AngleResult(angle, index, path)


@dataclass(frozen=True)
class SquareRootResult:
    value: int
    leading_zeroes: int
    scale_shift: int | None
    table_index: int | None
    shifted_word: int | None


def fixed_sqrt(magnitude: int, table: Sequence[int]) -> SquareRootResult:
    """80048c4c: original positive-input square-root approximation.

    The final shift is logical, after a wrapping left shift. The zero branch
    stores zero in the return delay slot. Negative inputs can select preceding
    memory through GTE leading-sign-bit behavior; that domain is unqualified.
    """
    if len(table) != 192:
        raise ValueError("original square-root table must contain 192 signed halfwords")
    magnitude = signed32(magnitude)
    if magnitude < 0:
        raise ValueError("unqualified negative original square-root input")
    if magnitude == 0:
        return SquareRootResult(0, 32, None, None, None)
    leading = 32 - magnitude.bit_length()
    even_leading = leading & ~1
    scale = (31 - even_leading) >> 1
    normalized = (
        magnitude << (even_leading - 24) if even_leading >= 24 else magnitude >> (24 - even_leading)
    )
    index = normalized - 64
    if not 0 <= index < len(table):
        raise ValueError("original square-root lookup outside the supplied table")
    shifted = (signed16(table[index]) << (scale & 31)) & 0xFFFFFFFF
    return SquareRootResult(shifted >> 12, leading, scale, index, shifted)


def planar_length(x: int, z: int, table: Sequence[int]) -> int:
    """80099a4c: square signed IR halfwords, add low words, then use fixed_sqrt."""
    magnitude = signed32(signed16(x) ** 2 + signed16(z) ** 2)
    return fixed_sqrt(magnitude, table).value


@dataclass(frozen=True)
class EdgeProjection:
    direction: int
    velocity: Vector
    branch: str
    normal_input: Vector | None
    normal: Vector | None
    length: int | None


def edge_projection(
    direction: int,
    edge: tuple[Vector, Vector],
    velocity: Vector,
    reciprocal_table: Sequence[int],
    sqrt_table: Sequence[int],
    atan_table: Sequence[int],
) -> EdgeProjection:
    """8007b6c4: stop or select an edge tangent and preserve quantized speed.

    The caller's terrain argument is unused by this original routine. The gate
    includes relative angles 128 and 3968. Y is cleared on every return path.
    """
    first, second = (tuple(signed16(v) for v in point) for point in edge)
    angle = -atan_angle(second[2] - first[2], second[0] - first[0], atan_table).angle
    angle &= 0xFFF
    relative = (0xC00 - signed16(direction) + angle) & 0xFFF
    if ((relative - 0x80) & 0xFFFFFFFF) >= 0xF01:
        return EdgeProjection(angle, (0, 0, 0), "stop", None, None, None)
    if relative < 0x800:
        delta = (first[0] - second[0], 0, first[2] - second[2])
        angle = (angle + 0x800) & 0xFFF
    else:
        delta = (second[0] - first[0], 0, second[2] - first[2])
    normal = normalize(delta, reciprocal_table)
    speed = planar_length(signed32(velocity[0]) >> 12, signed32(velocity[2]) >> 12, sqrt_table)
    result = (signed32(normal[0] * speed), 0, signed32(normal[2] * speed))
    return EdgeProjection(angle, result, "slide", delta, normal, speed)


@dataclass(frozen=True)
class SlopeProjection:
    velocity: Vector
    normal_input: Vector
    normal: Vector
    length_input: tuple[int, int]
    length: int


def slope_projection(
    velocity: Vector,
    actor_y: int,
    floor: int,
    reciprocal_table: Sequence[int],
    sqrt_table: Sequence[int],
) -> SlopeProjection:
    """8007bd04..8007bdb8: normalize a candidate on the selected floor slope.

    Negation wraps before each arithmetic shift, including after multiplication.
    The enclosing sweep subsequently requeries the floor and replaces Y again.
    """
    x, _, z = (signed32(v) for v in velocity)
    delta = (
        signed32(-x) >> 8,
        signed32((signed16(floor) << 16) - signed32(actor_y)) >> 8,
        signed32(-z) >> 8,
    )
    normal = normalize(delta, reciprocal_table)
    length_input = (x >> 8, z >> 8)
    speed = planar_length(*length_input, sqrt_table)
    result = (
        signed32(-signed32(speed * normal[0])) >> 4,
        signed32(speed * normal[1]) >> 4,
        signed32(-signed32(speed * normal[2])) >> 4,
    )
    return SlopeProjection(result, delta, normal, length_input, speed)
