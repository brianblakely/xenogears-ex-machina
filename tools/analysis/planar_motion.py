"""Bounded original motion-mode, sprite speed/vector and PC-store reconstruction.

Source lookup tables are inputs, never regenerated with host trigonometry. The
remaining field collision body and sprite command effects are not implemented.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

from .arithmetic import divide32, signed16, signed32
from .jump_physics import truncate_shift


def motion_mode(flags: int, held_buttons: int, input_updated: int, old_mode: int) -> int | None:
    """80082bf8..80082c8c; None means the whole motion body is inhibited.

    The caller still stores its current actor index before this branch. This
    prefix selects register S4 and does not yet store the actor's animation mode.
    """
    if flags & 0x01000000:
        return None
    mode = 2 if flags & 0x4000 and held_buttons & 0x40 and input_updated == 1 else 1
    if flags & 0x1800 and signed16(old_mode) in (1, 2):
        mode = signed16(old_mode)
    return mode


def animation_speed(operand_byte: int, scale: int, rate_control: int) -> int:
    """Sprite A0, 80021958..800219a4: store speed before rebuilding its vector."""
    operand = operand_byte & 0xFF
    operand -= 256 if operand & 0x80 else 0
    value = signed32((operand << 4) * signed32(rate_control + 1))
    value = signed32(value * signed16(scale))
    return signed32(truncate_shift(value, 12) << 8)


def trig_pair(table: bytes, angle: int) -> tuple[int, int]:
    """8003f8b0/8003f8cc: signed sine/cosine pair, indexed by twelve angle bits."""
    if len(table) != 4096 * 4:
        raise ValueError("original trigonometric table must contain 4096 complete pairs")
    return struct.unpack_from("<hh", table, (angle & 0xFFF) * 4)


@dataclass(frozen=True)
class SpriteVector:
    scalar: int
    x: int
    z: int


def sprite_velocity(speed: int, packed_flags: int, sine: int, cosine: int) -> SpriteVector:
    """80022974..800229e8; preserve the signed shifts and each low-word product."""
    numerator = signed32((signed32(speed) >> 4) << 8)
    scalar = divide32(numerator, (packed_flags >> 7) & 0xFFF)
    x = signed32((signed16(cosine) >> 2) * scalar) >> 6
    z = signed32(-signed32((signed16(sine) >> 2) * scalar)) >> 6
    return SpriteVector(scalar, x, z)


@dataclass(frozen=True)
class FieldVector:
    angle: int
    x: int
    z: int
    sprite: SpriteVector | None


def field_party_velocity(
    direction: int,
    previous_angle: int,
    descriptor_flags: int,
    actor_flags: int | None,
    speed: int,
    packed_flags: int,
    table: bytes,
) -> FieldVector:
    """Ordinary party branch of 80081f80..800821f4, including its stop sentinel.

    Descriptor bit40 selects this path. Direction bit8000 stops before reading
    actor flags or changing the stored angle. Other actor/descriptor paths need
    additional original state and fail explicitly. They are not approximated.
    """
    if not descriptor_flags & 0x40:
        raise ValueError("unreconstructed field velocity ratio path")
    if direction & 0x8000:
        return FieldVector(signed16(previous_angle), 0, 0, None)
    if actor_flags is None or actor_flags & 0x82000:
        raise ValueError("unreconstructed alternate actor velocity path")
    angle = signed16(direction)
    vector = sprite_velocity(speed, packed_flags, *trig_pair(table, angle))
    return FieldVector(
        angle, signed32(vector.x & 0xFFFFF000), signed32(vector.z & 0xFFFFF000), vector
    )


def command_pc_store(current_pc: int, opcode: int, widths: bytes) -> int:
    """80024edc..80024efc: width addition to the post-handler PC, then its store.

    A handler may already have changed current_pc. This does not execute it or
    claim that every VM path reaches this store. A source width of zero is kept.
    """
    if len(widths) != 256 or not 0 <= opcode < 256:
        raise ValueError("unresolved original sprite command-width access")
    return (current_pc + widths[opcode]) & 0xFFFFFFFF


def sprite_bundle_offsets(data: bytes) -> tuple[int, ...]:
    """Field component3's count/relative-offset index; inner formats stay opaque.

    Original selection reads word(index+1) and adds the component base. Bounds
    and monotonicity are analysis checks for the observed packed layout, not an
    assertion that the original loader validates malformed input.
    """
    if len(data) < 4:
        raise ValueError("truncated original field sprite index")
    count = struct.unpack_from("<I", data)[0]
    if not 1 <= count < len(data) // 4:
        raise ValueError("unresolved original field sprite count")
    offsets = struct.unpack_from(f"<{count}I", data, 4)
    if offsets[0] < 4 * (count + 1) or any(
        begin >= end for begin, end in zip(offsets, offsets[1:] + (len(data),), strict=True)
    ):
        raise ValueError("unresolved original field sprite offsets")
    return offsets
