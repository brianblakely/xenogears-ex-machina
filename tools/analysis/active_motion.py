"""Independent source reconstruction of field 80082bb8..80083178.

This diagnostic reconstruction retains original storage offsets, not a native API.
The callback supplies input memory only. Sweep and animation effects are computed
by their reviewed source models, never injected from original return records.
"""

import struct
from dataclasses import dataclass

from .arithmetic import signed16, signed32
from .collision_query import packed_area, packed_xz
from .field import collision_package
from .jump_physics import terrain_attribute
from .movement_sweep import SweepResult, movement_sweep, uses_ordinary_sweep
from .planar_motion import field_party_velocity, motion_mode
from .sprite_animation import field_animation
from .sprite_state import put, read_exact, u16, u32
from .sweep_math import atan_angle
from .sweep_trace import actor_state, edge_points, vector, write_edge


@dataclass(frozen=True)
class MotionControls:
    held_buttons: int
    input_updated: int
    collision_mode: int
    collision_enabled: int
    controlled_actor: int
    party_indices: tuple[int, int]
    party_flags: dict[int, int]
    jump_gate: int
    jump_mode: int
    query_attribute: int
    animation_rate: int
    platform_mode: int


def idle_predicate(actor, collision_mode, collision_enabled):
    """8008492c: -1 means that the prior-motion/collision state prevents idle."""
    if len(actor) != 312:
        raise ValueError("Incomplete original actor")
    if u32(actor, 0x14) & 0x420000 or collision_mode != 0:
        return -1
    if any(u32(actor, offset) for offset in (0x30, 0x34, 0x38)):
        return -1
    if collision_enabled != 1 or actor[0x74] != 255 or u32(actor) & 0x401800:
        return -1
    layer = signed16(u16(actor, 0x10))
    if 0 <= layer <= 2 and u32(actor, 4) & (1 << layer):
        return -1
    return 0


def motion_bounds(actor, velocity, read_memory):
    """80082494: optional pointed quadrilateral; signed packing uses addition."""
    if not u32(actor, 0x12C) & 0x1000:
        return 0
    x = signed32(u32(actor, 0x20) + velocity[0]) >> 16
    z = signed32(u32(actor, 0x28) + velocity[2]) >> 16
    candidate = packed_xz(x, z)
    coordinates = struct.unpack("<8h", read_exact(read_memory, u32(actor, 0x114), 16))
    points = [packed_xz(coordinates[i], coordinates[i + 1]) for i in range(0, 8, 2)]
    for i in range(4):
        if packed_area((points[i], points[(i + 1) % 4], candidate)) < 0:
            return -1
    return 0


@dataclass(frozen=True)
class MotionStage:
    name: str
    actor: bytes
    sprite: bytes
    locals: bytes
    mode: int | None
    direction: int
    values: dict


@dataclass(frozen=True)
class MotionResult:
    actor: bytes
    sprite: bytes
    locals: bytes
    actor_index: int
    inhibited: bool
    stages: tuple[MotionStage, ...]
    sweep: SweepResult | None
    animation: int | None
    stop_reason: str | None


def active_motion(
    actor_index,
    descriptor,
    actor,
    sprite,
    locals_before,
    controls,
    component,
    tables,
    read_resource,
    read_memory,
):
    if len(actor) != 312 or len(descriptor) != 92 or len(locals_before) != 40:
        raise ValueError("Incomplete original motion input storage")
    address = u32(descriptor, 4)
    actor, sprite, local = bytearray(actor), bytearray(sprite), bytearray(locals_before)
    mode = motion_mode(u32(actor), controls.held_buttons, controls.input_updated, u16(actor, 0xE8))
    direction = signed16(u16(actor, 0x104))
    stages, sweep, animation, stop_reason = [], None, None, None

    def mark(name, **values):
        stages.append(
            MotionStage(name, bytes(actor), bytes(sprite), bytes(local), mode, direction, values)
        )

    def stop(reason):
        nonlocal stop_reason
        stop_reason = reason
        put(actor, 0xF0, 65536)
        for offset in (0x40, 0x44, 0x48):
            put(actor, offset, 0)
        for offset in (0, 4, 8):
            put(local, offset, 0)
        for offset in (0x0C, 0x14):
            put(sprite, offset, 0)
        put(actor, 0x106, u16(actor, 0x106) | 0x8000, 2)

    if mode is None:
        mark("motion-after")
        return MotionResult(
            bytes(actor),
            bytes(sprite),
            bytes(local),
            actor_index & 0xFFFFFFFF,
            True,
            tuple(stages),
            None,
            None,
            None,
        )
    mark("motion-mode")
    if actor[0xE3] > 8:
        put(actor, 0xE3, actor[0xE3] - 1, 1)
    extra = u32(actor, 0x40) | u32(actor, 0x44) | u32(actor, 0x48)
    predicate = idle_predicate(actor, controls.collision_mode, controls.collision_enabled)
    mark("after-predicate", result=predicate, extra=extra)
    if predicate == -1:
        extra = 1
    if not direction & 0x8000 or extra or u32(actor) & 0x40800:
        if not direction & 0x8000:
            velocity = field_party_velocity(
                direction,
                signed16(u16(sprite, 0x32)),
                u32(descriptor, 0x58),
                u32(actor, 4),
                u32(sprite, 0x18),
                u32(sprite, 0xAC),
                tables.trigonometry,
            )
            put(sprite, 0x32, velocity.angle, 2)
            put(sprite, 0x0C, velocity.x)
            put(sprite, 0x14, velocity.z)
            mark("after-vector")
            for lo, ac, sp in ((0, 0x40, 0x0C), (4, 0x44, 0x10), (8, 0x48, 0x14)):
                put(local, lo, u32(actor, ac) + u32(sprite, sp))
            put(actor, 0x106, direction, 2)
        else:
            for lo, ac in ((0, 0x40), (4, 0x44), (8, 0x48)):
                put(local, lo, u32(actor, ac))
            direction = u16(actor, 0x106) & 0xFFF
        mark("before-bounds")
        bounds = motion_bounds(actor, vector(local[:12]), read_memory)
        mark("after-bounds", result=bounds)
        if bounds:
            stop("bounds")
        else:
            if u32(local) or u32(local, 8):
                direction = (
                    -signed16(atan_angle(u32(local, 8), u32(local), tables.angle).angle) & 0xFFF
                )
            layer = signed16(u16(actor, 0x10))
            if not 0 <= layer < 4:
                raise ValueError("Unqualified original main motion layer access")
            result = -1
            if signed16(u16(actor, 8 + 2 * layer)) != -1:
                saved_flags = u32(actor)
                if actor_index & 0xFFFFFFFF == controls.controlled_actor & 0xFFFFFFFF:
                    for index in controls.party_indices:
                        if index == 255:
                            continue
                        if index not in controls.party_flags:
                            raise ValueError("Uncaptured original party collision flags")
                        put(actor, 0, u32(actor) | (controls.party_flags[index] & 0x600))
                state = actor_state(bytes(actor))
                ordinary = uses_ordinary_sweep(state, controls.collision_mode)
                mark("sweep-ordinary-before" if ordinary else "sweep-special-before")
                sweep = movement_sweep(
                    component,
                    tables,
                    state,
                    vector(local[:12]),
                    edge_points(local[16:32]),
                    direction,
                    ordinary=ordinary,
                    collision_mode=controls.collision_mode,
                    attribute_control=controls.query_attribute,
                )
                put(actor, 0, sweep.actor.query.flags)
                put(actor, 0x72, sweep.actor.floor, 2)
                for offset, value in zip((0, 4, 8), sweep.velocity, strict=True):
                    put(local, offset, value)
                local[16:32] = write_edge(local[16:32], sweep.edge)
                result = sweep.value
                mark("after-sweep", result=result)
                put(actor, 0, (u32(actor) & ~0x600) | (saved_flags & 0x600))
            if result == -1:
                stop("sweep" if sweep is not None else "missing-triangle")
    else:
        mode = signed16(u16(actor, 0xE6))
        put(actor, 0x104, u16(actor, 0x104) | 0x8000, 2)
        stop("idle")
    put(actor, 4, u32(actor, 4) & ~0x1000)
    if u32(actor) & 0x800:
        if signed16(controls.jump_gate) == 0:
            speed = (
                0
                if u16(sprite, 6) == u16(sprite, 0x84)
                else signed16(u16(sprite, 0x82)) * (96 if mode == 2 else 48)
            )
            put(sprite, 0x18, speed)
        mode = signed16(controls.jump_mode)
    else:
        if u16(actor, 0x104) & 0x8000:
            mode = signed16(u16(actor, 0xE6))
        terrain = terrain_attribute(
            u32(actor, 4),
            signed16(u16(actor, 0x10)),
            struct.unpack_from("<4h", actor, 8),
            collision_package(component),
        )
        if terrain & 0x200000:
            if u16(actor, 0x104) & 0x8000 and signed16(u16(actor, 0xE8)) == 6:
                put(actor, 4, u32(actor, 4) | 0x1000)
            mode = 6
    override = signed16(u16(actor, 0xEA))
    if override != 255:
        mode = override
    mark("animation-choice")
    if signed16(u16(actor, 0xE8)) != mode and not u32(actor) & 0x2000000:
        put(actor, 0xE8, mode, 2)
        animation = mode
        mark("animation-call")
        sprite, actor = map(
            bytearray,
            field_animation(
                bytes(sprite),
                address,
                mode,
                descriptor,
                bytes(actor),
                controls.jump_gate,
                controls.jump_mode,
                controls.animation_rate,
                controls.platform_mode,
                read_resource,
                tables.trigonometry,
            ),
        )
    mark("animation-after")
    if u32(actor, 0x14) & 0x100:
        for offset in (0, 8):
            put(local, offset, signed32(u32(local, offset)) >> 1)
    for lo, ac in ((0, 0x30), (4, 0x34), (8, 0x38)):
        put(actor, ac, u32(local, lo))
    for offset in (0x40, 0x44, 0x48):
        put(actor, offset, 0)
    mark("motion-after")
    return MotionResult(
        bytes(actor),
        bytes(sprite),
        bytes(local),
        actor_index & 0xFFFFFFFF,
        False,
        tuple(stages),
        sweep,
        animation,
        stop_reason,
    )
