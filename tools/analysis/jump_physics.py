"""Original sprite impulse, gravity prefix and field vertical-stage reconstruction.

These bounded stages do not implement the animation VM, floor selection, the
complete collision solver, or a native player controller. Values use the original
integer widths; unresolved memory accesses and zero divisors fail explicitly.
"""

from __future__ import annotations

from dataclasses import dataclass

from .arithmetic import divide32, signed16, signed32
from .field import CollisionPackage


def truncate_shift(value: int, shift: int) -> int:
    """The original signed add-before-SRA sequence, including 32-bit wrap."""
    value = signed32(value)
    if value < 0:
        value = signed32(value + (1 << shift) - 1)
    return value >> shift


@dataclass(frozen=True)
class SpriteTiming:
    scale: int
    rate_control: int
    packed_flags: int

    @property
    def divisor(self) -> int:
        return (self.packed_flags >> 7) & 0xFFF


@dataclass(frozen=True)
class Impulse:
    velocity: int
    division_numerator: int
    used_reference: bool


def sprite_impulse(
    operand_byte: int, timing: SpriteTiming, sprite_flags: int, referenced_velocity: int | None
) -> Impulse:
    """Sprite opcode A1, original resident 800219ac..80021a44.

    A set low flag requires the captured referenced word, even when it is zero.
    A nonzero reference bypasses operand scaling but still undergoes division.
    """
    reference = 0
    if sprite_flags & 1:
        if referenced_velocity is None:
            raise ValueError("sprite impulse needs its original velocity reference")
        reference = signed32(referenced_velocity)
    if reference:
        value = reference
    else:
        operand = operand_byte & 0xFF
        operand -= 256 if operand & 0x80 else 0
        value = signed32((operand << 4) * signed32(timing.rate_control + 1))
        value = signed32(value * signed16(timing.scale))
        value = signed32(truncate_shift(value, 12) << 8)
    numerator = signed32(value << 8)
    return Impulse(divide32(numerator, timing.divisor), numerator, bool(reference))


@dataclass(frozen=True)
class GravityPrefix:
    gravity: int
    sprite_flags: int
    header_pointer: int
    command_pointer: int
    frame_pointer: int


def gravity_prefix(
    header_words: tuple[int, int, int],
    header_pointer: int,
    sprite_flags: int,
    timing: SpriteTiming,
) -> GravityPrefix:
    """Header installation through 80023654; later animation work is excluded."""
    word, command_offset, frame_offset = (value & 0xFFFF for value in header_words)
    coefficient = (word >> 2) & 0x3F
    coefficient -= 64 if coefficient & 0x20 else 0
    rate = signed32(timing.rate_control + 1)
    factor = signed32(rate * rate)
    factor = truncate_shift(signed32(factor * signed16(timing.scale)), 12)
    scaled = signed32((coefficient << 10) * factor)
    ratio = divide32(65536, timing.divisor)
    ratio = truncate_shift(signed32(ratio * ratio), 8)
    gravity = truncate_shift(signed32(scaled * ratio), 8)
    return GravityPrefix(
        gravity,
        ((sprite_flags & 0xFFCFFFFF) | ((word & 3) << 20)) & 0xFFFFFFFF,
        header_pointer & 0xFFFFFFFF,
        (header_pointer + command_offset + 2) & 0xFFFFFFFF,
        (header_pointer + frame_offset + 4) & 0xFFFFFFFF,
    )


def terrain_attribute(
    disabled_layers: int, layer: int, triangle_indices: tuple[int, ...], mesh: CollisionPackage
) -> int:
    """Original 80080968: select the low triangle attribute byte, not its u16."""
    layer = signed16(layer)
    if (disabled_layers >> ((layer + 3) & 31)) & 1:
        return 0
    if not 0 <= layer < len(mesh.layers) or layer >= len(triangle_indices):
        raise ValueError("unresolved original terrain layer access")
    triangle = signed16(triangle_indices[layer])
    if not 0 <= triangle < len(mesh.layers[layer].triangles):
        raise ValueError("unresolved original terrain triangle access")
    attribute = mesh.layers[layer].triangles[triangle].attribute_raw & 0xFF
    offset = 4 * attribute
    if offset + 4 > len(mesh.attributes_raw):
        raise ValueError("unresolved original terrain attribute access")
    return int.from_bytes(mesh.attributes_raw[offset : offset + 4], "little")


@dataclass(frozen=True)
class VerticalState:
    y: int
    velocity: int
    flags: int
    vertical_marker: int


@dataclass(frozen=True)
class VerticalEffect:
    state: VerticalState
    integrated_y: int
    branch: str


def vertical_step(
    state: VerticalState, gravity: int, floor: int, layer: int, previous_layer: int, terrain: int
) -> VerticalEffect:
    """Original 8008505c..8008515c, with the previously selected floor as input.

    The following ceiling/layer/collision stages may still change this result.
    The floor comparison uses the signed high half of the integrated position.
    """
    y = signed32(state.y + state.velocity)
    integrated_y = y
    velocity = signed32(state.velocity)
    flags = state.flags & 0xFFFFFFFF
    marker = signed32(state.vertical_marker)
    floor = signed16(floor)
    if signed16(layer) != signed16(previous_layer):
        flags &= 0xFBFFFFFF
    if not flags & 0x04000000 and signed16(y >> 16) < floor:
        velocity = signed32(velocity + gravity)
        flags |= 0x1000
        marker = velocity
        branch = "airborne"
    else:
        if not terrain & 0x00420000:
            marker = 0
        if velocity > 0:
            velocity = 0
        flags &= 0xFFBFEFFF
        y = signed32(floor << 16)
        branch = "floor"
    flags &= 0xFBFFFFFF
    return VerticalEffect(VerticalState(y, velocity, flags, marker), integrated_y, branch)
