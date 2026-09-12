"""Original resident sprite construction and non-platform resource binding.

Source: 80023804, 8002393c/800239a0, 80022000, 80022224/800222bc,
8002435c and 80024524. Allocator results include their original input bytes;
uninitialized heap content is not replaced by zeroes. The field scheduler,
destruction and complete native ownership remain separate dependencies.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, replace

from .arithmetic import signed16, signed32
from .jump_physics import truncate_shift
from .sprite_animation import select_animation
from .sprite_state import inside, put, read_exact, u16, u32

SPRITE_BYTES = 0x164


@dataclass(frozen=True)
class CreationEnvironment:
    rate_control: int
    platform_mode: int
    variant: int
    binding_control: int


@dataclass(frozen=True)
class OriginalAllocation:
    address: int
    data: bytes


@dataclass(frozen=True)
class SpriteConstruction:
    sprite: OriginalAllocation
    parts: OriginalAllocation
    environment: CreationEnvironment


@dataclass(frozen=True)
class ConstructionStage:
    name: str
    sprite: bytes
    environment: CreationEnvironment
    allocation_bytes: int | None = None
    allocation: OriginalAllocation | None = None


def allocation(allocate, size):
    result = allocate(size, 0)
    if (
        not isinstance(result, OriginalAllocation)
        or not isinstance(result.data, bytes)
        or len(result.data) != size
        or not 0 <= result.address <= 0x100000000 - size
        or (size and result.address == 0)
    ):
        raise ValueError("Incomplete original sprite allocation or input bytes")
    return result


def sprite_defaults(sprite, rate):
    """80023804; scale at +82 is read from the incoming allocation."""
    if len(sprite) != SPRITE_BYTES:
        raise ValueError("Incomplete original 356-byte sprite allocation")
    out = bytearray(sprite)
    put(out, 0x3C, 0)
    put(out, 0x2B, 0x2D, 1)
    put(out, 0x40, 0)
    for offset in (0x3A, 0x30, 0x32, 0x34):
        put(out, offset, 0, 2)
    for offset in (0xA8, 0xAC, 0xB0):
        put(out, offset, 0)
    put(out, 0xB0, 0, 1)
    put(out, 0xAF, 0, 1)
    rate = signed32(rate + 1)
    value = signed32(rate * signed32(rate << 14))
    value = signed32(value * signed16(u16(out, 0x82)))
    put(out, 0xAC, 0x8000)
    put(out, 0x1C, truncate_shift(value, 12))
    for offset in (0x64, 0x70, 0x44, 0x68):
        put(out, offset, 0)
    put(out, 0x80, 0, 2)
    put(out, 0x8C, 16, 1)
    put(out, 0x84, 0, 2)
    put(out, 0x6C, 0)
    put(out, 0x50, 0)
    return bytes(out)


def bind_inline_storage(sprite, address):
    """800239a0 and 8002393c: renderer, sequencer, binding and auxiliary."""
    if len(sprite) != SPRITE_BYTES or not 0 <= address <= 0x100000000 - SPRITE_BYTES:
        raise ValueError("Original inline sprite storage exceeds its allocation")
    out = bytearray(sprite)
    put(out, 0x20, address + 0xB4)
    for offset in (0, 2, 4):
        put(out, 0xB4 + offset, 0, 2)
    put(out, 0xE0, 0)
    put(out, 0x7C, address + 0xF4)
    put(out, 0xE8, address + 0x124)
    put(out, 0x24, address + 0x110)
    put(out, 0xEC, 0)
    return bytes(out)


def set_renderer_scale(sprite, address, scale):
    """80022000 changes the renderer scale, not sprite's +82 scale."""
    out = bytearray(sprite)
    renderer = u32(out, 0x20)
    if renderer:
        offset = inside(out, address, renderer, 12)
        put(out, 0x2C, scale, 2)
        for at in (10, 8, 6):
            put(out, offset + at, scale, 2)
        put(out, 0x3C, u32(out, 0x3C) | 0x10000000)
    return bytes(out)


def bind_resource(sprite, address, resource, environment, read):
    """800222bc/80022224's non-platform path; only new bindings clear 591b0."""
    out = bytearray(sprite)
    binding = u32(out, 0x24)
    if not resource:
        return bytes(out), environment
    if environment.platform_mode:
        raise ValueError("Alternate sprite platform resource binding remains unreconstructed")
    if resource != u32(out, 0x44):
        at = inside(out, address, binding, 20)
        # Original coordinates are loaded before the callee writes this table.
        coordinates = u32(out, at + 4), u32(out, at + 8)
        put(out, at + 4, coordinates[0])
        put(out, at + 8, coordinates[1])
        for source, destination in ((12, 12), (8, 0)):
            offset = int.from_bytes(read_exact(read, resource + source, 4), "little")
            put(out, at + destination, resource + offset)
        directory = int.from_bytes(read_exact(read, resource + 4, 4), "little")
        environment = replace(environment, binding_control=0)
        put(out, at + 16, resource + directory)
        put(out, 0x44, resource)
        put(out, 0x3C, u32(out, 0x3C) | 0x40000000)
    return bytes(out), environment


def construct_sprite(
    sprite,
    address,
    resource,
    coordinates,
    environment,
    read,
    trig,
    allocate: Callable[[int, int], OriginalAllocation],
    *,
    on_stage: Callable[[ConstructionStage], None] | None = None,
):
    """8002435c with original allocator input bytes and four binding halfwords.

    Coordinates are in original argument order: the first pair is stored at
    binding+8/+a; the second at +4/+6. Part storage has header-derived size and
    is owned separately from the 356-byte sprite. Unwritten bytes stay intact.
    """
    if len(coordinates) != 4:
        raise ValueError("Original sprite constructor requires four binding coordinates")
    if environment.platform_mode:
        raise ValueError("Alternate sprite platform construction remains unreconstructed")
    active = environment

    def emit(name, size=None, block=None):
        if on_stage is not None:
            on_stage(ConstructionStage(name, bytes(out), active, size, block))

    out = bytearray(sprite_defaults(sprite, active.rate_control))
    emit("constructor-after-defaults")
    out = bytearray(bind_inline_storage(out, address))
    emit("constructor-after-inline")
    out = bytearray(set_renderer_scale(out, address, 0x1000))
    emit("constructor-after-scale")
    put(out, 0x3C, (u32(out, 0x3C) & ~3) | 1)
    put(out, 0x40, u32(out, 0x40) & 0xFFFE1FFF)
    sequencer = inside(out, address, u32(out, 0x7C), 28)
    put(out, 0xA8, u32(out, 0xA8) | 1)
    put(out, sequencer + 24, 0)
    put(out, 0x6C, address)
    variant = active.variant & 15
    put(out, 0x3C, (u32(out, 0x3C) & 0xFF00FFFF) | (variant << 20) | (variant << 16))
    directory = (
        resource + int.from_bytes(read_exact(read, resource + 8, 4), "little")
    ) & 0xFFFFFFFF
    count = (int.from_bytes(read_exact(read, directory, 2), "little") >> 9) & 63
    emit("allocate-parts-before", count * 24)
    parts = allocation(allocate, count * 24)
    if parts.data and max(address, parts.address) < min(
        address + len(out), parts.address + len(parts.data)
    ):
        raise ValueError("Original part and sprite allocations overlap")
    emit("allocate-parts-after", count * 24, parts)
    renderer = inside(out, address, u32(out, 0x20), 52)
    put(out, renderer + 44, parts.address)
    put(out, renderer + 48, parts.address)
    for source, target in ((2, 4), (3, 6), (0, 8), (1, 10)):
        binding = inside(out, address, u32(out, 0x24), 20)
        put(out, binding + target, coordinates[source], 2)
    put(out, 0x48, resource)
    emit("binding-before")
    bound, active = bind_resource(out, address, resource, active, read)
    out = bytearray(bound)
    emit("binding-after")
    binding = inside(out, address, u32(out, 0x24), 20)
    directory = u32(out, binding + 16)
    count = int.from_bytes(read_exact(read, directory, 2), "little") & 63
    put(out, 0x60, directory + 2 * (count + 1))
    emit("animation-before")
    out = bytearray(
        select_animation(out, address, 0, active.rate_control, active.platform_mode, read, trig)
    )
    emit("constructor-after")
    return SpriteConstruction(OriginalAllocation(address, bytes(out)), parts, active)


def create_sprite(resource, parameters, environment, read, trig, allocate, *, on_stage=None):
    """80024524's 356-byte allocation and constructor forwarding.

    Five original short parameters follow the resource. The last is written to
    a seventh stack argument that 8002435c does not read; it has no state effect.
    """
    if len(parameters) != 5:
        raise ValueError("Original sprite allocator wrapper requires five short parameters")
    initial = allocation(allocate, SPRITE_BYTES)
    sprite = bytearray(initial.data)
    put(sprite, 0x86, SPRITE_BYTES, 2)
    return construct_sprite(
        sprite,
        initial.address,
        resource,
        parameters[:4],
        environment,
        read,
        trig,
        allocate,
        on_stage=on_stage,
    )
