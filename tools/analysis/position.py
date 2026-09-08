"""Authored reconstruction of field position integration, 80084a40..800854d0.

Layer queries, terrain selection, normalization, vertical integration and history
are computed from source models. No observed callee effects are model inputs.
Linked-contact arguments are caller inputs; the preceding contact solver is a
separate unresolved scope. Diagnostic-print execution is explicitly unsupported.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

from tools.analysis.active_motion import idle_predicate
from tools.analysis.arithmetic import signed16, signed32
from tools.analysis.collision_math import normalize
from tools.analysis.field import collision_package
from tools.analysis.jump_physics import VerticalState, terrain_attribute, vertical_step
from tools.analysis.sprite_state import put, u16, u32

from .position_query import history_store, layer_floor


@dataclass(frozen=True)
class PositionControls:
    controlled_actor: int
    forced: int
    attribute_control: int
    collision_mode: int
    collision_enabled: int
    layer_count: int
    debug_control: int | None = None


@dataclass(frozen=True)
class PositionStage:
    name: str
    actor: bytes
    sprite: bytes
    descriptor: bytes
    frame: bytes
    ring: bytes
    history_index: int
    history_reset: int
    position_result: int
    values: dict


@dataclass(frozen=True)
class PositionResult:
    value: int
    reason: str
    stages: tuple
    queries: tuple
    vertical: object | None


def position(
    actor_index,
    linked_floor,
    link_status,
    actor,
    sprite,
    descriptor,
    frame,
    sp,
    gpr,
    controls,
    component,
    reciprocal,
    ring,
    history_index,
    history_reset,
    position_result,
    observe_first_read=False,
):
    if (
        len(actor) != 312
        or len(sprite) < 180
        or len(descriptor) != 92
        or len(frame) != 256
        or len(gpr) != 34
    ):
        raise ValueError("Incomplete original position input storage")
    actor, sprite, descriptor, frame = map(bytearray, (actor, sprite, descriptor, frame))
    ring = bytes(ring)
    stages = []
    queries = []
    vertical = None
    actor_index = signed32(actor_index)
    linked_floor = signed32(linked_floor)
    link_status &= 0xFFFFFFFF
    # Original prologue stores ten words in ascending stack order.
    for i, r in enumerate((*range(16, 24), 30, 31)):
        put(frame, 0xD8 + 4 * i, gpr[r])
    if actor_index == controls.controlled_actor:
        position_result = 0xFFFF

    def mark(name, **values):
        stages.append(
            PositionStage(
                name,
                bytes(actor),
                bytes(sprite),
                bytes(descriptor),
                bytes(frame),
                ring,
                history_index,
                history_reset,
                position_result,
                values,
            )
        )

    def finish(value, reason):
        mark("position-after", result=value)
        return PositionResult(value, reason, tuple(stages), tuple(queries), vertical)

    if observe_first_read:
        mark("position-inputs")
    if u32(actor) & 0x1000000:
        return finish(-1, "actor-inhibited")
    if u32(actor, 4) & 0x200000:
        return finish(-1, "layer-inhibited")
    if u32(actor) & 0x10000:
        return finish(-1, "position-inhibited")
    if (
        not (actor_index == controls.controlled_actor and controls.forced & 255 == 1)
        and u32(sprite, 0x10) == 0
    ):
        if idle_predicate(actor, controls.collision_mode, controls.collision_enabled) == 0 and u16(
            sprite, 0x84
        ) == u16(actor, 0x26):
            return finish(-1, "idle")
    mark("position-active")
    count = signed16(controls.layer_count) - 1
    if not 0 <= count <= 4:
        raise ValueError("Unqualified original position layer count")
    old_layer = signed16(u16(actor, 0x10))
    if not 0 <= old_layer < 4:
        raise ValueError("Unqualified original position current layer")
    old_position = bytes(actor[0x20:0x2C])
    old_triangles = bytes(actor[8:16])
    frame[0x90:0x9C] = old_position
    frame[0xA0:0xA8] = old_triangles
    for i in range(4):
        put(frame, 0x18 + 4 * i, 0x7FFFFFFF)
        put(frame, 0x28 + 4 * i, 0x7FFFFFFF)
        put(frame, 0x38 + 4 * i, i)
    completed = 0
    for layer in range(count):
        put(frame, 0x10, sp + 0x48 + 2 * layer)
        put(frame, 0x14, sp + 0x28 + 4 * layer)
        mark("layer-before", layer=layer)
        query = layer_floor(component, reciprocal, actor, layer, controls.attribute_control)
        queries.append(query)
        if query.normal is not None:
            for i, v in enumerate(query.normal):
                put(frame, 0x50 + 16 * layer + 4 * i, v)
        if query.triangle is not None:
            put(frame, 0x48 + 2 * layer, query.triangle, 2)
        if query.floor is not None:
            put(frame, 0x18 + 4 * layer, query.floor)
        if query.upper is not None:
            put(frame, 0x28 + 4 * layer, query.upper)
        mark("layer-after", layer=layer, result=query.value)
        if query.value != 0:
            break
        completed += 1
    mark("position-layers", completed=completed)
    for i in range(3):
        if u32(actor, 4) & (1 << i):
            put(frame, 0x18 + 4 * i, 0x7FFFFFFF)
            put(frame, 0x28 + 4 * i, 0x7FFFFFFF)
    old_floor = signed32(u32(frame, 0x18 + 4 * old_layer))
    for _ in range(2):
        for i in range(2):
            if signed32(u32(frame, 0x1C + 4 * i)) < signed32(u32(frame, 0x18 + 4 * i)):
                for base in (0x18, 0x28, 0x38):
                    a, b = u32(frame, base + 4 * i), u32(frame, base + 4 * i + 4)
                    put(frame, base + 4 * i, b)
                    put(frame, base + 4 * i + 4, a)
    mark("position-sorted")
    mesh = collision_package(component)

    def terrain():
        return terrain_attribute(
            u32(actor, 4), u16(actor, 0x10), struct.unpack_from("<4h", actor, 8), mesh
        )

    def rollback_xz():
        actor[0x20:0x24] = old_position[:4]
        actor[0x28:0x2C] = old_position[8:]
        put(actor, 0x10, old_layer, 2)
        put(actor, 0xF0, 0)
        actor[8:16] = old_triangles

    def commit(axes):
        sprite[:12] = actor[0x20:0x2C]
        for axis in axes:
            put(descriptor, 0x20 + 4 * axis, signed16(u16(actor, 0x22 + 4 * axis)))

    def finish_history(reason):
        nonlocal ring, history_index, history_reset
        mark("position-history")
        mark("history-before")
        ring, history_index, history_reset = history_store(
            actor_index,
            controls.controlled_actor,
            controls.attribute_control,
            history_index,
            history_reset,
            ring,
            actor,
            sprite,
        )
        mark("history-after")
        return finish(0, reason)

    if completed == count:
        actor[8 : 8 + 2 * count] = frame[0x48 : 0x48 + 2 * count]
        y = signed16(u16(actor, 0x26))
        choice = 0
        if y < old_floor or u32(actor) & 0x1800:
            while choice < count:
                if y <= signed32(u32(frame, 0x18 + 4 * choice)):
                    put(actor, 0x10, u32(frame, 0x38 + 4 * choice), 2)
                    break
                choice += 1
        else:
            while choice < count and signed16(u16(actor, 0x10)) != u32(frame, 0x38 + 4 * choice):
                choice += 1
        if terrain() & 4 and choice != 0 and signed16(u16(actor, 0x10)) <= count:
            put(actor, 0x10, u32(frame, 0x34 + 4 * choice), 2)
        mark("position-selected-layer")
        attribute = terrain()
        if u32(actor) >> 8 & 7 & attribute >> 5 or attribute & 0x800000:
            if controls.debug_control is None or controls.debug_control == 0:
                raise ValueError("Unreconstructed original position diagnostic call")
            if actor_index == controls.controlled_actor:
                position_result = 0xFFF
            put(actor, 0x24, u32(actor, 0x24) + u32(sprite, 0x10))
            rollback_xz()
            if signed16(u16(actor, 0x26)) < signed16(u16(sprite, 0x84)):
                put(sprite, 0x10, u32(sprite, 0x10) + u32(sprite, 0x1C))
            else:
                if signed32(u32(sprite, 0x10)) > 0:
                    put(sprite, 0x10, 0)
                put(actor, 0, u32(actor) & 0xFFBFEFFF)
                put(actor, 0x24, signed16(u16(sprite, 0x84)) << 16)
            commit((1,))
            return finish_history("terrain-rejected")
        for a, v in ((0x20, 0x30), (0x28, 0x38)):
            put(actor, a, u32(actor, a) + u32(actor, v))
        layer = signed16(u16(actor, 0x10))
        for i in range(count):
            if layer == u32(frame, 0x38 + 4 * i):
                put(sprite, 0x84, u32(frame, 0x18 + 4 * i), 2)
                break
        normal = normalize(struct.unpack_from("<3i", frame, 0x50 + 16 * layer), reciprocal)
        for i, v in enumerate(normal):
            put(actor, 0x50 + 4 * i, v)
    else:
        put(actor, 0xF0, 0)
    mark("position-ground")
    if controls.collision_mode == 0:
        if link_status != 0:
            if signed16(u16(sprite, 0x84)) < signed32(linked_floor + 10):
                put(actor, 0x74, 255, 1)
            put(sprite, 0x84, linked_floor, 2)
            put(actor, 0x24, linked_floor << 16)
    elif link_status < 2:
        put(sprite, 0x84, linked_floor, 2)
    if u32(actor) & 0x40000:
        put(actor, 0x24, signed16(u16(actor, 0xEC)) << 16)
        put(sprite, 0x10, 0)
    mark("position-vertical-before")
    vertical = vertical_step(
        VerticalState(u32(actor, 0x24), u32(sprite, 0x10), u32(actor), u32(actor, 0xF0)),
        u32(sprite, 0x1C),
        u16(sprite, 0x84),
        u16(actor, 0x10),
        old_layer,
        terrain(),
    )
    v = vertical.state
    put(actor, 0x24, v.y)
    put(sprite, 0x10, v.velocity)
    put(actor, 0, v.flags)
    put(actor, 0xF0, v.vertical_marker)
    mark("position-vertical-after", branch=vertical.branch)
    y = signed16(u16(actor, 0x26))
    head = signed32(y - u16(actor, 0x1A))
    clear = True
    for i in range(count):
        floor, upper = signed32(u32(frame, 0x18 + 4 * i)), signed32(u32(frame, 0x28 + 4 * i))
        if floor < y and head < upper and floor != upper:
            clear = False
            break
    if clear:
        layer = signed16(u16(actor, 0x10))
        tri = signed16(u16(actor, 8 + 2 * layer))
        if not 0 <= layer < len(mesh.layers) or not 0 <= tri < len(mesh.layers[layer].triangles):
            raise ValueError("Unqualified original position headroom triangle")
        extent = mesh.layers[layer].triangles[tri].attribute_raw >> 8
        extent = (extent - 256 if extent & 128 else extent) * 4
        clear = extent >= 0 or signed32(extent + signed16(u16(sprite, 0x84))) <= head
    if clear:
        commit((0, 1, 2))
        put(actor, 0x14, terrain())
        reason = "committed"
    else:
        rollback_xz()
        if u16(sprite, 0x84) != u16(actor, 0x26):
            put(sprite, 0x10, u32(sprite, 0x10) + u32(sprite, 0x1C))
        if signed32(u32(sprite, 0x10)) < 0:
            put(sprite, 0x10, 0)
            actor[0x24:0x28] = old_position[4:8]
        commit((1,))
        reason = "volume-rejected"
    return finish_history(reason)
