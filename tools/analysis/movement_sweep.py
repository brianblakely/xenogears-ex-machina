"""Source reconstruction of ordinary and special field movement sweeps.

Original 8007bac0 and 8007b814 produce a candidate vector and selected floor.
The later position/layer integrator is separate. This analysis model uses typed
state and original lookup data; it does not execute emulated instructions.
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass, replace

from .arithmetic import signed16, signed32
from .collision_math import Vector
from .collision_query import Edge, QueryActor, QueryResult, QueryVector, collision_query
from .planar_motion import trig_pair
from .sweep_math import EdgeProjection, SlopeProjection, edge_projection, slope_projection


@dataclass(frozen=True)
class SweepActor:
    query: QueryActor
    terrain_flags: int
    linked_actor: int
    floor: int
    forced_floor: int


@dataclass(frozen=True)
class SweepTables:
    reciprocal: Sequence[int]
    square_root: Sequence[int]
    angle: Sequence[int]
    trigonometry: bytes


@dataclass(frozen=True)
class SweepQuery:
    stage: str
    candidate: QueryVector
    mode: int
    result: QueryResult


@dataclass(frozen=True)
class SweepResult:
    value: int
    actor: SweepActor
    velocity: Vector
    edge: Edge
    queries: tuple[SweepQuery, ...]
    projection: EdgeProjection | None
    slope: SlopeProjection | None
    slope_reason: str | None


def uses_ordinary_sweep(actor: SweepActor, collision_mode: int) -> bool:
    """The main motion caller's selection between 8007bac0 and 8007b814."""
    return (
        not actor.query.flags & 0x41800
        and actor.linked_actor & 0xFF == 0xFF
        and collision_mode & 0xFFFFFFFF == 0
    )


def movement_sweep(
    component: bytes,
    tables: SweepTables,
    actor: SweepActor,
    velocity: Vector,
    edge: Edge,
    direction: int,
    *,
    ordinary: bool,
    collision_mode: int,
    attribute_control: int,
) -> SweepResult:
    """Compute the selected original sweep, preserving its query order and widths.

    The edge buffer is caller-owned input: failed queries can preserve its prior
    contents. Probe Y remains unspecified because neither original query reads
    it. A failure preserves the input velocity and any edge writes already made.
    """
    velocity = tuple(signed32(v) for v in velocity)
    direction = signed16(direction)
    queries = []
    projection, slope, slope_reason = None, None, None

    def query(candidate: QueryVector, mode: int, stage: str) -> QueryResult:
        nonlocal edge
        result = collision_query(
            component,
            tables.reciprocal,
            actor.query,
            candidate,
            ordinary=ordinary,
            mode=mode,
            attribute_control=attribute_control,
        )
        if result.edge is not None:
            edge = result.edge
        queries.append(SweepQuery(stage, candidate, mode, result))
        return result

    def finish(value: int, result_velocity: Vector = velocity) -> SweepResult:
        return SweepResult(
            value, actor, result_velocity, edge, tuple(queries), projection, slope, slope_reason
        )

    probes = (
        ((-0x100, "probe-left"), (0x100, "probe-right"), (0, "probe-center"))
        if ordinary
        else ((0, "probe-center"), (-0x100, "probe-left"), (0x100, "probe-right"))
    )
    working = velocity
    for offset, stage in probes:
        sine, cosine = trig_pair(tables.trigonometry, direction + offset)
        candidate = (
            signed32(velocity[0] + (cosine << 6)),
            None,
            signed32(velocity[2] - (sine << 6)),
        )
        result = query(candidate, -1, stage)
        if result.value == -1:
            projection = edge_projection(
                direction, edge, velocity, tables.reciprocal, tables.square_root, tables.angle
            )
            working = projection.velocity
            break
    result = query(working, 0, "floor")
    if result.value == -1:
        return finish(-1)
    floor = result.point[1]
    actor_y = signed32(actor.query.position[1])
    if ordinary:
        integer_y = signed16(actor_y >> 16)
        terrain = result.attribute
        if floor < integer_y:
            slope_reason = "upward-floor"
        elif terrain & 0x200000:
            slope_reason = "terrain-200000"
        elif terrain & 0x420000:
            if actor.terrain_flags & 0x420000:
                slope_reason = "existing-terrain-420000"
        elif floor < integer_y + 64:
            slope_reason = "within-64-height"
        if slope_reason is not None:
            slope = slope_projection(working, actor_y, floor, tables.reciprocal, tables.square_root)
            working = slope.velocity
            result = query(working, 0, "slope-floor")
            if result.value == -1:
                return finish(-1)
            floor = result.point[1]
            actor = replace(actor, query=replace(actor.query, flags=actor.query.flags | 0x04000000))
    elif actor.query.flags & 0x40000:
        floor = signed16(actor.forced_floor)
    elif signed32(floor << 16) < actor_y and collision_mode & 0xFFFFFFFF == 0:
        return finish(-1)
    final = (working[0], signed32((floor << 16) - actor_y), working[2])
    actor = replace(actor, floor=signed16(signed32(actor_y + final[1]) >> 16))
    return finish(0, final)
