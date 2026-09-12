"""Original later field checkpoint policy and connected sprite restoration.

Field 800a3c8c chooses checkpoints after sprite recreation. Resident 80021d50
temporarily changes the rate, selects the animation, executes ordinary timed
commands to the saved step, then restores position and sequencer words. Resource
creation and complete native ownership/readiness are separate obligations.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

from .arithmetic import signed16
from .sprite_animation import select_animation
from .sprite_state import inside, put, u16, u32
from .sprite_vm import SpriteExecution, advance_sprite_timer


@dataclass(frozen=True)
class CheckpointDecision:
    checkpoint: bytes
    restore: bool
    reason: str
    record_bytes: int


def sprite_return_policy(actor, checkpoint, saved_modes, current_modes, return_gate):
    """800a3c8c's actor decision; inputs are the current recreated actor state.

    A skipped actor can still update its saved animation. The record stride uses
    the current actor's extension flags; it is independent of the skip decision.
    """
    if len(actor) != 0x138 or len(checkpoint) != 48:
        raise ValueError("Incomplete original sprite return actor or checkpoint")
    if len(saved_modes) != 3 or len(current_modes) != 3:
        raise ValueError("Original return policy requires three saved/current party modes")
    result = bytearray(checkpoint)
    animation = signed16(u16(actor, 0xEA))
    if signed16(u16(actor, 0x124)) != -1 and animation != 255:
        put(result, 0x14, animation, 2)
    changed = any(
        (saved & 0xFFFFFFFF) != (current & 255)
        for saved, current in zip(saved_modes, current_modes, strict=True)
    )
    reason = "restore"
    if u32(actor, 4) & 0x1000000:
        reason = "actor-layer-flag"
    elif return_gate and u32(actor) & 0x600 and changed:
        reason = "party-mode-change"
    stride = 0x174 + (12 if u32(actor, 0x134) & 0x80 else 0)
    stride += 16 if u32(actor, 0x12C) & 0x1000 else 0
    return CheckpointDecision(bytes(result), reason == "restore", reason, stride)


def restore_sprite_checkpoint(
    sprite,
    address,
    checkpoint,
    environment,
    widths,
    read,
    read_memory,
    trig,
    *,
    on_event=None,
    inspection_limit=4096,
):
    """Compose 80021d50 with recovered animation and ordinary command effects.

    The returned environment restores the incoming rate, retaining the computed
    frame-list changes. Unknown VM, binding, allocation and rendering paths fail;
    reaching a bounded inspection limit never becomes a ready return.
    """
    if len(checkpoint) != 48 or inspection_limit <= 0:
        raise ValueError("Incomplete original sprite checkpoint or inspection bound")
    target = signed16(u16(checkpoint, 0x18))
    if not 0 <= target <= 63:
        raise ValueError("Original sprite checkpoint has unreachable six-bit step")
    out = bytearray(sprite)
    active = replace(environment, rate_control=0)
    put(out, 0x80, u16(checkpoint, 0x10), 2)
    put(out, 0xAF, checkpoint[0x14], 1)
    put(out, 0xB0, checkpoint[0x16], 1)
    for src, dest in ((0x24, 6), (0x26, 8), (0x28, 10)):
        # The original reloads the renderer pointer between the stores.
        renderer = inside(out, address, u32(out, 0x20), 12)
        put(out, renderer + dest, u16(checkpoint, src), 2)
    put(out, 0x82, u16(checkpoint, 0x2C), 2)
    put(out, 0x2C, u16(checkpoint, 0x2A), 2)
    # Renderer stores can alias sprite fields in an original-format buffer.
    # The signed animation load follows those writes in the original.
    animation = out[0xAF]
    animation -= 256 if animation & 128 else 0
    out = bytearray(select_animation(out, address, animation, 0, active.platform_mode, read, trig))
    if on_event:
        on_event("restore-after-select", bytes(out), active, {})
    iterations, commands = 0, 0
    while (u32(out, 0xA8) >> 22) & 63 != target:
        if iterations == inspection_limit:
            raise ValueError("Original sprite checkpoint replay exceeded inspection bound")
        if on_event:
            on_event("restore-step", bytes(out), active, {})
        result = advance_sprite_timer(
            out,
            address,
            active,
            widths,
            read,
            read_memory,
            on_event=on_event,
            inspection_limit=inspection_limit,
        )
        out, active = bytearray(result.sprite), result.environment
        commands += result.commands
        for position, velocity in ((0, 0xC), (8, 0x14), (4, 0x10)):
            put(out, position, u32(out, position) + u32(out, velocity))
        put(out, 0x10, u32(out, 0x10) + u32(out, 0x1C))
        iterations += 1
    for offset in (0, 4, 8):
        put(out, offset, u32(checkpoint, offset))
    sequencer = inside(out, address, u32(out, 0x7C), 8)
    put(out, sequencer, u32(checkpoint, 0x1C))
    # Reload after the first store, matching the original's possible aliases.
    sequencer = inside(out, address, u32(out, 0x7C), 8)
    put(out, sequencer + 4, u32(checkpoint, 0x20))
    active = replace(active, rate_control=environment.rate_control)
    return SpriteExecution(bytes(out), active, commands)
