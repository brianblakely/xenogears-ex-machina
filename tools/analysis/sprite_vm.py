"""Recovered ordinary sprite timer and frame-command execution.

Resident 80023210 dispatches 800248d4 when the signed halfword countdown reaches
zero. This module recovers its timed frame families, index store and relative
jump; all other commands and alternate platform execution fail explicitly.
The command table supplies original widths. This is original-format analysis,
not a native tick scheduler or a complete sprite VM.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

from .arithmetic import signed16, signed32
from .jump_physics import truncate_shift
from .sprite_replay import frame_change, lookup_frame
from .sprite_state import put, read_exact, u16, u32


@dataclass(frozen=True)
class SpriteEnvironment:
    rate_control: int
    platform_mode: int
    frame_head: int


@dataclass(frozen=True)
class SpriteExecution:
    sprite: bytes
    environment: SpriteEnvironment
    commands: int


class UnsupportedSpriteCommand(ValueError):
    def __init__(self, pointer, opcode):
        self.pointer, self.opcode = pointer, opcode
        super().__init__(
            f"Unreconstructed ordinary sprite command 0x{opcode:02x} at 0x{pointer:08x}"
        )


def execute_sprite_commands(
    sprite,
    address,
    environment,
    widths,
    read,
    read_memory,
    *,
    incoming_duration=None,
    on_event=None,
    inspection_limit=4096,
):
    """800248d4: supported control/frame effects, from entry through return.

    Commands 40..7f consume the incoming S3 value in the original. Absence of
    that context is an explicit error. The inspection limit bounds hostile loops;
    it is not an original gameplay safeguard or a successful yield.
    """
    if len(widths) != 256 or inspection_limit <= 0:
        raise ValueError("Incomplete original command widths or inspection bound")
    if environment.platform_mode:
        raise ValueError("Alternate original sprite VM 800c11cc remains unreconstructed")
    out = bytearray(sprite)
    head, commands = environment.frame_head, 0
    duration = None if incoming_duration is None else signed32(incoming_duration)

    def event(name, data, **values):
        if on_event:
            on_event(name, bytes(data), replace(environment, frame_head=head), values)

    def frame_before(data, frame):
        event("frame-before", data, frame=frame)

    def list_step(node, data):
        event("frame-list-step", data, node=node)

    event("vm-before", out)
    while u16(out, 0x9E) == 0:
        if commands == inspection_limit:
            raise ValueError("Original sprite command execution exceeded inspection bound")
        pointer = u32(out, 0x64)
        opcode = read_exact(read, pointer, 1)[0]
        commands += 1
        event("vm-step", out, pointer=pointer, opcode=opcode)
        if opcode < 0x80:
            put(out, 0x64, pointer + 1)
            if opcode < 0x30:
                if opcode < 0x10 or opcode >= 0x20:
                    frame = u16(out, 0x34) + (1 if opcode < 0x10 else -1)
                    frame_before(out, frame)
                    updated, head = frame_change(
                        out, address, frame, head, read, read_memory, list_step
                    )
                else:
                    flags = u32(out, 0xA8)
                    put(out, 0xA8, (flags & 0xFFFE07FF) | ((((flags >> 11) + 1) & 63) << 11))
                    updated, head = lookup_frame(
                        out, address, head, read, read_memory, frame_before, list_step
                    )
                out = bytearray(updated)
                event("frame-after", out)
            if opcode < 0x40:
                duration = (opcode & 15) + 1
            if duration is None:
                raise ValueError("Original ordinary frame command needs the incoming S3 duration")
            delay = truncate_shift(signed32(duration * ((u32(out, 0xAC) >> 7) & 0xFFF)), 8)
            if delay == 0:
                delay = 1
            put(out, 0x9E, u16(out, 0x9E) + delay, 2)
            flags = u32(out, 0xA8)
            step = (((flags >> 22) + 1) & 63) or 63
            put(out, 0xA8, (flags & 0xF03FFFFF) | (step << 22))
            break
        if opcode == 0xB3:
            value = read_exact(read, pointer + 1, 1)[0]
            put(out, 0xA8, (u32(out, 0xA8) & 0xFFFE07FF) | ((value & 63) << 11))
            put(out, 0x64, u32(out, 0x64) + widths[opcode])
        elif opcode == 0xE1:
            delta = signed16(int.from_bytes(read_exact(read, pointer + 1, 2), "little"))
            put(out, 0x64, u32(out, 0x64) + delta)
        else:
            raise UnsupportedSpriteCommand(pointer, opcode)
    event("vm-after", out)
    return SpriteExecution(bytes(out), replace(environment, frame_head=head), commands)


def advance_sprite_timer(
    sprite, address, environment, widths, read, read_memory, *, inspection_limit=4096, **kwargs
):
    """80023210: countdown and conditional command dispatch, preserving wrap.

    A zero timer remains zero. Rate -1 skips all work. The source reloads the
    global rate after a dispatch; the result's environment supplies that value.
    Other rates are bounded for inspection rather than silently shortened.
    """
    if inspection_limit <= 0:
        raise ValueError("Invalid original timer inspection bound")
    out, count, iterations = bytearray(sprite), 0, 0
    if signed32(environment.rate_control) == -1:
        return SpriteExecution(bytes(out), environment, 0)
    while True:
        if iterations == inspection_limit:
            raise ValueError("Original sprite timer exceeded inspection bound")
        timer = u16(out, 0x9E)
        if timer:
            put(out, 0x9E, timer - 1, 2)
            if timer == 1:
                result = execute_sprite_commands(
                    out,
                    address,
                    environment,
                    widths,
                    read,
                    read_memory,
                    inspection_limit=inspection_limit,
                    **kwargs,
                )
                out, environment = bytearray(result.sprite), result.environment
                count += result.commands
        iterations += 1
        if iterations == (environment.rate_control + 1) & 0xFFFFFFFF:
            return SpriteExecution(bytes(out), environment, count)
