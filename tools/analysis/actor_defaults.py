"""Original field actor defaults and the resident random-number step.

Field 80080a74 and resident 8003fa38 are reconstructed from original MIPS.
This is an original-format correlation model, not a native allocator or a
scenario-entry service. Unwritten actor bytes and supplied local scratch remain
observable; absent scratch is never replaced by invented zeroes.
"""

from __future__ import annotations

from collections.abc import Callable, Sequence
from dataclasses import dataclass, replace

from .arithmetic import signed16, signed32
from .collision_math import locate
from .field import CollisionPackage
from .jump_physics import terrain_attribute
from .sprite_state import put, u16, u32


@dataclass(frozen=True)
class RandomStep:
    seed: int
    value: int


def random_step(seed: int) -> RandomStep:
    """8003fa38, including the return-delay-slot mask; seeding is separate."""
    seed = (seed * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
    return RandomStep(seed, (seed >> 16) & 0x7FFF)


@dataclass(frozen=True)
class DefaultsStage:
    name: str
    actor: bytes
    descriptor: bytes
    scratch: bytes
    seed: int
    triangle_counts: tuple[int, ...]
    layer: int | None = None
    query_result: int | None = None
    query_matched: bool | None = None
    terrain: int | None = None


@dataclass(frozen=True)
class ActorDefaults:
    actor: bytes
    descriptor: bytes
    scratch: bytes
    seed: int
    triangle_counts: tuple[int, ...]
    queried_layers: int


def initialize_actor_defaults(
    actor: bytes,
    descriptor: bytes,
    seed: int,
    layer_count: int,
    triangle_counts: Sequence[int],
    mesh: CollisionPackage,
    reciprocal: Sequence[int],
    *,
    scratch: bytes,
    on_stage: Callable[[DefaultsStage], None] | None = None,
) -> ActorDefaults:
    """80080a74's complete bounded actor/default effect.

    ``scratch`` is the original 96-byte local region: four 16-byte normals,
    then four eight-byte points. The routine writes only three components of
    each queried entry. It can consume an incoming layer-zero normal/height
    when ``signed16(layer_count) <= 1``; callers must supply that state.

    The mutable original triangle counts are distinct from the parsed resource's
    physical bounds. A zero count produces the original zero result/outputs,
    while terrain lookup still accesses the existing triangle table. No-match
    is exposed separately in observations, without altering the original actor.
    """
    if len(actor) != 0x138 or len(descriptor) != 0x5C or len(scratch) != 0x60:
        raise ValueError("Incomplete original actor, descriptor or initialization scratch")
    queries = max(0, signed16(layer_count) - 1)
    if queries > 4 or queries > len(mesh.layers) or len(triangle_counts) != 4:
        raise ValueError("Original initialization layer count exceeds supplied storage")
    out, desc, local = bytearray(actor), bytearray(descriptor), bytearray(scratch)
    counts = [value & 0xFFFFFFFF for value in triangle_counts]
    seed &= 0xFFFFFFFF

    def emit(name, *, layer=None, result=None, matched=None, terrain=None):
        if on_stage is not None:
            on_stage(
                DefaultsStage(
                    name,
                    bytes(out),
                    bytes(desc),
                    bytes(local),
                    seed,
                    tuple(counts),
                    layer,
                    result,
                    matched,
                    terrain,
                )
            )

    put(out, 0, 0xB0)
    put(out, 4, 0x800)
    for offset, value in ((0x18, 0x10), (0x1C, 0x10), (0x1A, 0x60)):
        put(out, offset, value, 2)
    put(out, 0x74, 255, 1)
    put(out, 0x75, 255, 1)
    for offset in (0x40, 0x44, 0x48, 0x30, 0x34, 0x38):
        put(out, offset, 0)
    for offset in (0x64, 0x60, 0x62):
        put(out, offset, 0, 2)
    for offset in (0xD0, 0xD4, 0xD8):
        put(out, offset, 0)
    put(out, 0xE6, 0, 2)
    put(out, 0xEA, 255, 2)
    put(out, 0xE2, 0, 1)
    put(out, 0xCC, 0, 2)
    put(out, 0x6E, 0, 2)
    put(out, 0x12C, u32(out, 0x12C) & 0xFFFFFFDF)
    put(out, 0x11E, 0x200, 2)
    put(out, 0x1E, u16(out, 0x18), 2)
    put(out, 0x12C, u32(out, 0x12C) & 0xFFFFFFFC)
    for offset in range(0x101, 0xFB, -1):
        put(out, offset, 0x80, 1)
    put(out, 0x128, 0xFFFF, 2)
    put(out, 0x12C, u32(out, 0x12C) & 0xFFFCFFFF)
    put(out, 0x130, u32(out, 0x130) & 0xF0000000)
    put(out, 0x12C, u32(out, 0x12C) & 0xF003FFFF)
    for slot in range(8):
        offset = 0x8C + slot * 8
        put(out, offset + 2, 0, 1)
        put(out, offset, 0xFFFF, 2)
        put(out, offset + 3, 255, 1)
        put(out, offset + 4, (u32(out, offset + 4) & 0xFE3CFFFF) | 0x3C0000)
        put(out, offset + 4, 0xFFFF, 2)
    put(out, 0x120, 0)
    put(out, 0xE4, 255, 2)
    put(out, 0x76, 0x100, 2)
    put(out, 0x83, 0, 1)
    put(out, 0x82, 0, 1)
    put(out, 0x8A, 0, 2)
    put(out, 0x88, 0, 2)
    put(out, 0x84, 0)
    put(out, 0xCF, 0, 1)
    put(out, 0xCE, 0, 1)
    put(out, 0xE8, 0, 2)
    put(out, 0x10, 0, 2)
    put(out, 0xEC, 0, 2)
    put(out, 0x134, u32(out, 0x134) & 0xFFFFFF7F)
    put(out, 0x12C, u32(out, 0x12C) & 0xFFFFE03F)
    put(out, 0x134, u32(out, 0x134) & 0xFFFFFF9F)
    emit("random-before")
    random = random_step(seed)
    seed = random.seed
    emit("random-after")
    put(out, 0x102, random.value, 2)
    for offset in (0xF4, 0xF6, 0xF8):
        put(out, offset, 0x1000, 2)
    put(out, 0x10D, 255, 1)
    put(out, 0x80, 255, 1)
    for offset in (0x106, 0x104, 0x108):
        put(out, offset, 0x8000, 2)
    put(out, 0x124, 0xFFFF, 2)
    put(out, 0xE3, 0, 1)
    for offset in (0xE, 0xC, 0xA, 8):
        put(out, offset, 0, 2)
    put(out, 0x12C, u32(out, 0x12C) & 0xFFFFFFE3)

    x, z = signed16(u16(desc, 0x20)), signed16(u16(desc, 0x28))
    for layer in range(queries):
        emit("locate-before", layer=layer)
        active_count = max(0, signed32(counts[layer]))
        if active_count > len(mesh.layers[layer].triangles):
            raise ValueError("Original initialization triangle count exceeds its layer")
        geometry = replace(
            mesh.layers[layer], triangles=mesh.layers[layer].triangles[:active_count]
        )
        if any(
            not 0 <= vertex < len(geometry.vertices)
            for triangle in geometry.triangles
            for vertex in triangle.vertices
        ):
            raise ValueError("Original initialization vertex index exceeds its layer")
        found = locate(geometry, x, z, reciprocal)
        index, point, normal = (0, (0, 0, 0), (0, 0, 0)) if found is None else found
        for axis in range(3):
            put(local, layer * 16 + axis * 4, normal[axis])
            put(local, 0x40 + layer * 8 + axis * 2, point[axis], 2)
        emit("locate-after", layer=layer, result=index, matched=found is not None)
        put(out, 8 + layer * 2, index, 2)
        index = signed16(index)
        if index != -1 and (index & 0xFFFFFFFF) >= counts[layer]:
            counts[layer] = 0
            for axis in range(3):
                put(local, layer * 16 + axis * 4, 0)
                put(local, 0x40 + layer * 8 + axis * 2, 0, 2)

    emit("terrain-before")
    terrain = terrain_attribute(
        u32(out, 4), u16(out, 0x10), tuple(u16(out, 8 + 2 * i) for i in range(4)), mesh
    )
    emit("terrain-after", terrain=terrain)
    put(out, 0x14, terrain)
    layer = signed16(u16(out, 0x10))
    for axis in range(3):
        put(out, 0x50 + axis * 4, u32(local, layer * 16 + axis * 4))
    if not u16(desc, 0x58) & 0x80:
        put(desc, 0x24, signed16(u16(local, 0x42 + layer * 8)))
    for offset in (0x20, 0x24, 0x28):
        put(out, offset, u32(desc, offset) << 16)
    put(out, 0x72, u32(desc, 0x24), 2)
    emit("defaults-after")
    return ActorDefaults(bytes(out), bytes(desc), bytes(local), seed, tuple(counts), queries)
