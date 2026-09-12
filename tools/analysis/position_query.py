"""Authored source models of field layer-floor queries and party history.

Original addresses 8007d3d4, 8007c670 and 80081c54 identify reviewed routines;
geometry and coefficient tables must be supplied by the original-data caller.
These diagnostic storage layouts are not a native gameplay API.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

from tools.analysis.arithmetic import signed16, signed32
from tools.analysis.collision_math import height_and_normal
from tools.analysis.collision_query import packed_area, packed_xz
from tools.analysis.field import collision_package, region
from tools.analysis.sprite_state import put, u16, u32


@dataclass(frozen=True)
class LayerFloor:
    value: int
    floor: int | None
    upper: int | None
    triangle: int | None
    normal: tuple | None
    steps: tuple
    areas: tuple
    height: tuple | None
    reason: str


def layer_floor(component, reciprocal, actor, layer, attribute_control):
    if len(actor) != 312:
        raise ValueError("Incomplete original layer-query actor")
    layer = signed32(layer)

    def word(at):
        return struct.unpack("<I", region(component, at, 4, "layer query word"))[0]

    if not 0 <= layer < word(0) or layer >= 4:
        raise ValueError("Unqualified original layer query index")
    triangle = signed16(u16(actor, 8 + 2 * layer))
    if triangle == -1:
        return LayerFloor(-1, None, None, None, None, (), (), None, "missing-initial-triangle")
    # Reconstruction safety checks, not checks performed by the original routine.
    # A component-wide range alone admits indices into vertices or another layer.
    mesh = collision_package(component).layers[layer]
    triangle_base = word(0x18 + 8 * layer)
    x = signed32(u32(actor, 0x20) + u32(actor, 0x30)) >> 16
    z = signed32(u32(actor, 0x28) + u32(actor, 0x38)) >> 16
    candidate = packed_xz(x, z)
    origin = packed_xz(signed32(u32(actor, 0x20)) >> 16, signed32(u32(actor, 0x28)) >> 16)
    mask_attributes = (
        0xFFFFFFFF
        if not (u32(actor, 4) >> ((layer + 3) & 31)) & 1 and not attribute_control & 255
        else 0
    )
    counter, steps, areas = 0, [], []

    def vertices(index):
        if not 0 <= index < len(mesh.triangles):
            raise ValueError(f"layer {layer} triangle {index}: index outside its triangle table")
        raw = region(component, triangle_base + 14 * index, 14, "layer query triangle")
        points = tuple(mesh.vertices[i][:3] for i in mesh.triangles[index].vertices)
        return raw, points

    def area(a, b, c):
        result = packed_area((a, b, c))
        areas.append(((a, b, c), result))
        return result

    while True:
        raw, points = vertices(triangle)
        packed = tuple(packed_xz(p[0], p[2]) for p in points)
        flags = sum(
            (1 << i) for i in range(3) if area(packed[i], packed[(i + 1) % 3], candidate) < 0
        )
        steps.append((triangle, counter, flags))
        if flags == 0:
            counter = 255
        elif flags in (1, 2, 4):
            triangle = struct.unpack_from("<h", raw, {1: 6, 2: 8, 4: 10}[flags])[0]
        elif flags == 3:
            triangle = struct.unpack_from(
                "<h", raw, 8 if area(packed[1], candidate, origin) >= 0 else 6
            )[0]
        elif flags == 5:
            triangle = struct.unpack_from(
                "<h", raw, 10 if area(packed[0], candidate, origin) < 0 else 6
            )[0]
        elif flags == 6:
            triangle = struct.unpack_from(
                "<h", raw, 10 if area(packed[2], candidate, origin) >= 0 else 8
            )[0]
        else:
            triangle = -1
        counter += 1
        if triangle == -1:
            return LayerFloor(
                -1, None, None, None, None, tuple(steps), tuple(areas), None, "missing-neighbor"
            )
        if counter >= 32:
            break
    if counter == 32:
        return LayerFloor(
            -1, None, None, None, None, tuple(steps), tuple(areas), None, "iteration-limit"
        )
    raw, points = vertices(triangle)
    computed, normal = height_and_normal(points, x, z, reciprocal)
    computed = signed16(computed)
    height = (points, (x, computed, z), normal)
    attribute = word(word(0x14) + 4 * raw[12])
    if attribute & mask_attributes & 0x800000:
        return LayerFloor(
            0,
            0x7FFFFFFF,
            0x7FFFFFFF,
            triangle,
            normal,
            tuple(steps),
            tuple(areas),
            height,
            "masked-terrain",
        )
    extent = max(struct.unpack_from("<b", raw, 13)[0] << 2, 0)
    floor = (
        signed16(u16(actor, 0x72))
        if signed16(u16(actor, 0x10)) == layer and any(u32(actor, o) for o in (0x30, 0x34, 0x38))
        else computed
    )
    return LayerFloor(
        0,
        floor,
        height_bounds(floor, extent)[1],
        triangle,
        normal,
        tuple(steps),
        tuple(areas),
        height,
        "height",
    )


def history_store(
    actor_index, controlled_actor, attribute_control, index, reset, ring, actor, sprite
):
    if actor_index != controlled_actor or attribute_control & 255:
        return bytes(ring), index, reset
    if len(ring) != 32 * 72 or not 0 <= index < 32:
        raise ValueError("Unqualified original party history ring")
    out = bytearray(ring)
    base = index * 72
    for target, source in ((0, 0), (4, 4), (0x30, 0x50), (0x34, 0x54), (0x38, 0x58), (0x40, 0x14)):
        put(out, base + target, u32(actor, source))
    for target, source in (
        (8, 0x22),
        (10, 0x26),
        (12, 0x2A),
        (0x12, 0xE8),
        (0x16, 8),
        (0x18, 10),
        (0x1A, 12),
        (0x1C, 14),
    ):
        put(out, base + target, u16(actor, source), 2)
    put(out, base + 0x10, u16(sprite, 0x84), 2)
    put(out, base + 0x14, u16(actor, 0x106) & 0xFFF, 2)
    for target, source in ((0x20, 0xC), (0x24, 0x10), (0x28, 0x14)):
        put(out, base + target, u32(sprite, source))
    put(out, base + 0x44, u16(actor, 0x10), 1)
    return bytes(out), (index - 1) & 31, 0


def height_bounds(floor, extent):
    """8007c670 keeps the floor and adds only a nonnegative signed extent."""
    floor, extent = signed32(floor), signed32(extent)
    return floor, signed32(floor + max(extent, 0))
