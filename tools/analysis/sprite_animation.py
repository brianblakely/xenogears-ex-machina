"""Ghidra-reviewed original animation selection and orientation reconstruction.

Unreconstructed resource rebinding, alternate platform/model dispatch and command
replay without an explicit source model are failures. See the bounded evidence
and unresolved branches in analysis/formats/sprite-animation.md.
"""

import struct

from .arithmetic import signed16, signed32
from .jump_physics import SpriteTiming, gravity_prefix
from .sprite_matrix import sprite_matrix
from .sprite_state import inside, put, read_exact, u16, u32


def bind_current(sprite, resource, platform_mode):
    if platform_mode:
        raise ValueError("Unreconstructed alternate sprite platform mode")
    if resource and resource != u32(sprite, 0x44):
        raise ValueError("Original resource rebinding remains unreconstructed")
    return bytes(sprite)


def apply_header(sprite, address, header, words, rate, platform_mode, table):
    if platform_mode:
        raise ValueError("Unreconstructed alternate sprite platform mode")
    out = bytearray(sprite)
    result = gravity_prefix(
        words, header, u32(out, 0xA8), SpriteTiming(signed16(u16(out, 0x82)), rate, u32(out, 0xAC))
    )
    for offset, value in (
        (0x58, header),
        (0x64, result.command_pointer),
        (0x54, result.frame_pointer),
        (0xA8, result.sprite_flags),
        (0x1C, result.gravity),
    ):
        put(out, offset, value)
    word = words[0]
    if not word & 0x800:
        for offset in (0x0C, 0x10, 0x14, 0x18):
            put(out, offset, 0)
    renderer = u32(out, 0x20)
    if renderer:
        at = inside(out, address, renderer, 64)
        if not word & 0x1000:
            for offset in (0, 2, 4):
                put(out, at + offset, 0, 2)
            out = bytearray(sprite_matrix(out, address, table))
        if u32(out, 0x3C) & 3 == 1:
            put(out, at + 0x3C, 0, 1)
            put(out, at + 0x3D, 0, 1)
            if not u32(out, 0x40) & 0x100000 and u32(out, at + 0x34):
                clear = inside(out, address, u32(out, at + 0x34), 64)
                out[clear : clear + 64] = bytes(64)
    put(out, 0x8C, 0x10, 1)
    put(out, 0x30, 0, 2)
    put(out, 0x9E, 1, 2)
    flags = (u32(out, 0xA8) & 0xC03FF801) | 0x2001F800
    put(out, 0xA8, flags)
    auxiliary = u32(out, 0x7C)
    if auxiliary and flags & 1:
        at = inside(out, address, auxiliary, 14)
        put(out, at, 0)
        put(out, at + 4, 0)
        put(out, at + 12, 0, 2)
    return bytes(out)


def select_orientation(sprite, angle, read, replay=None):
    out = bytearray(sprite)
    angle = signed16(angle)
    put(out, 0x80, angle, 2)
    flags = u32(out, 0xAC)
    put(out, 0xAC, flags | 4 if (angle + 0x400) & 1 else flags & ~4)
    if not u32(out, 0x48):
        return bytes(out)
    old = (u32(out, 0xA8) >> 17) & 7
    mode = (u32(out, 0xA8) >> 20) & 3
    header = u32(out, 0x58)

    def half(pointer):
        return int.from_bytes(read_exact(read, pointer, 2), "little")

    if mode == 0:
        flip = ((angle + 0x400) & 0xFFF) > 0x800
        put(out, 0xAC, u32(out, 0xAC) | 4 if flip else u32(out, 0xAC) & ~4)
        put(out, 0xA8, u32(out, 0xA8) & 0xFFF1FFFF)
        put(out, 0x5C, header + 6)
        put(out, 0x54, header + 4 + half(header + 4))
    elif mode in (1, 2):
        if mode == 1:
            group = ((angle + 0x600) >> 10) & 3
            index = 1 if group == 3 else group
            flip = group == 3
        else:
            group = ((angle + 0x500) >> 9) & 7
            flip = group > 4
            if flip:
                group = (group - 5) ^ 3
            index = group
        put(out, 0xAC, u32(out, 0xAC) | 4 if flip else u32(out, 0xAC) & ~4)
        slot = header + 4 + index * 2
        put(out, 0x54, slot + half(slot))
        put(out, 0xA8, (u32(out, 0xA8) & 0xFFF1FFFF) | ((group & 7) << 17))
    if old != ((u32(out, 0xA8) >> 17) & 7):
        if replay is None:
            raise ValueError("Original command replay required")
        delay = u16(out, 0x9E)
        target = u32(out, 0x64)
        step = (u32(out, 0xA8) >> 22) & 63
        put(out, 0xA8, (u32(out, 0xA8) & 0xF03FFFFF) | 0x1F800)
        put(out, 0x64, header + 2 + half(header + 2))
        out = bytearray(replay(bytes(out), target, step))
        put(out, 0x9E, delay, 2)
    flags = u32(out, 0xAC)
    put(out, 0x3C, (u32(out, 0x3C) & ~8) | ((((flags >> 3) ^ (flags >> 2)) & 1) << 3))
    return bytes(out)


def select_animation(sprite, address, animation, rate, platform_mode, read, table, *, replay=None):
    out = bytearray(sprite)
    animation = signed32(animation)
    resource = u32(out, 0x48)
    if not resource:
        put(out, 0x64, 0)
        return bytes(out)
    if animation < 0:
        raise ValueError("Alternate negative animation resource remains unreconstructed")
    put(
        out, 0xB0, u32(out, 0xB0) & ~0x400 if u32(out, 0x44) == resource else u32(out, 0xB0) | 0x400
    )
    out = bytearray(bind_current(out, resource, platform_mode))
    put(out, 0xAF, animation, 1)
    binding = inside(out, address, u32(out, 0x24), 20)
    directory = u32(out, binding + 16)
    if (
        directory
        != (resource + int.from_bytes(read_exact(read, resource + 4, 4), "little")) & 0xFFFFFFFF
    ):
        raise ValueError("Animation directory pointer differs from original resource header")
    header = (
        directory + int.from_bytes(read_exact(read, directory + 2 + 2 * animation, 2), "little")
    ) & 0xFFFFFFFF
    put(out, 0x40, u32(out, 0x40) | 0x100000)
    put(out, 0x58, header)
    words = struct.unpack("<3H", read_exact(read, header, 6))
    out = apply_header(out, address, header, words, rate, platform_mode, table)
    return select_orientation(out, signed16(u16(out, 0x80)), read, replay)


def field_animation(
    sprite,
    address,
    animation,
    descriptor,
    actor,
    global_jump_gate,
    global_jump_mode,
    rate,
    platform_mode,
    read,
    table,
):
    animation = signed32(animation)
    result = bytearray(actor)
    if u16(descriptor, 0x58) & 0x40:
        if animation != 3 and signed16(global_jump_gate) == 0:
            put(result, 0, u32(result) & ~0x800)
        if animation == 255:
            animation = 0
        if animation != signed16(global_jump_mode):
            put(result, 0, u32(result) & ~0x800)
        flags = u32(result, 4)
        if flags & 0x2000:
            raise ValueError("Alternate model animation dispatcher remains unreconstructed")
        if not flags & 0x1000000:
            sprite = select_animation(sprite, address, animation, rate, platform_mode, read, table)
    return bytes(sprite), bytes(result)
