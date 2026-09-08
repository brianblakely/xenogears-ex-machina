"""Reconstructed field direction and jump-request handlers.

Original field functions 0x8009f5a8/0x8009f5f4 and predicate 0x80081f5c.
This models requests, not motion integration. The active encounter-selection
service is unresolved and raises explicitly; its early exits are recovered.
No original tables or RAM addresses form part of the semantic input API.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

from tools.analysis.arithmetic import signed16


class ControlError(ValueError):
    pass


class UnrecoveredEncounter(ControlError):
    pass


def integer(value: int, bits: int, *, signed: bool = False) -> None:
    low = -(1 << (bits - 1)) if signed else 0
    high = (1 << (bits - int(signed))) - 1
    if type(value) is not int or not low <= value <= high:
        raise ControlError(f"control value must fit {'i' if signed else 'u'}{bits}")


@dataclass(frozen=True)
class ControlActor:
    flags: int
    terrain_flags: int
    integer_position: tuple[int, int, int]
    cached_position: tuple[int, int, int]
    direction: int
    animation_mode: int
    pc: int

    def __post_init__(self) -> None:
        integer(self.flags, 32)
        integer(self.terrain_flags, 32)
        for position in (self.integer_position, self.cached_position):
            if len(position) != 3:
                raise ControlError("control position needs three signed coordinates")
            for value in position:
                integer(value, 16, signed=True)
        for value in (self.direction, self.animation_mode, self.pc):
            integer(value, 16)


@dataclass(frozen=True)
class EncounterGates:
    field_active: int
    gate_e4: int
    gate_ec: int
    music_result: int
    encounter_gate: int
    inhibition: int
    menu_gate: int
    enabled_byte: int

    def __post_init__(self) -> None:
        for value in (
            self.field_active,
            self.gate_e4,
            self.gate_ec,
            self.music_result,
            self.encounter_gate,
            self.menu_gate,
        ):
            integer(value, 32)
        integer(self.inhibition, 16, signed=True)
        integer(self.enabled_byte, 8)


def encounter_early_exit(gates: EncounterGates) -> str | None:
    """First return in 0x80079288, before counters, RNG or requests change."""
    for blocked, reason in (
        (gates.field_active == 0, "field_inactive"),
        (gates.gate_e4 == 0, "gate_e4_clear"),
        (gates.gate_ec == 0, "gate_ec_clear"),
        (gates.music_result == 0xFFFFFFFF, "music_pending"),
        (gates.encounter_gate == 0, "encounter_gate_clear"),
        (gates.inhibition == -1, "inhibited"),
        (gates.menu_gate == 1, "menu_gate_one"),
        (gates.enabled_byte == 0, "enabled_byte_clear"),
    ):
        if blocked:
            return reason
    return None


@dataclass(frozen=True)
class ControlInputs:
    held_buttons: int
    pressed_buttons: int
    dialogue_status: tuple[int, int, int, int]
    preserve_nonplayer_motion: int
    jump_contact: int
    jump_mode: int
    repeat_delay: int
    jump_setting: int
    alternate_directions: int
    direction_tables: tuple[tuple[int, ...], tuple[int, ...]]
    camera_angle: int
    encounter: EncounterGates

    def __post_init__(self) -> None:
        for value in (self.held_buttons, self.pressed_buttons, self.repeat_delay):
            integer(value, 16)
        for value in (self.preserve_nonplayer_motion, self.alternate_directions):
            integer(value, 8)
        for value in (self.jump_contact, self.jump_setting):
            integer(value, 32)
        for value in (self.jump_mode, self.camera_angle):
            integer(value, 16, signed=True)
        if len(self.dialogue_status) != 4:
            raise ControlError("control checks exactly four dialogue statuses")
        for value in self.dialogue_status:
            integer(value, 16, signed=True)
        if len(self.direction_tables) != 2 or any(len(t) != 16 for t in self.direction_tables):
            raise ControlError("control needs two original 16-entry direction tables")
        for table in self.direction_tables:
            for value in table:
                integer(value, 16)


@dataclass(frozen=True)
class ControlState:
    stationary_counter: int
    repeat_remaining: int
    latched_jump_setting: int
    updated: int
    break_requested: int

    def __post_init__(self) -> None:
        for value in (self.stationary_counter, self.repeat_remaining):
            integer(value, 16)
        for value in (self.latched_jump_setting, self.updated, self.break_requested):
            integer(value, 32)


@dataclass(frozen=True)
class ControlEffect:
    actor: ControlActor
    state: ControlState
    eligibility: str
    jump: str
    # Ordered nested calls make unobserved/omitted work visible to comparisons.
    calls: tuple[str, ...]
    encounter_exit: str | None
    terrain_call_variant: str | None


def terrain_jump_result(actor: ControlActor) -> int:
    return -int(bool(((actor.flags >> 9) & 3) & (actor.terrain_flags >> 3)))


def request_control(
    opcode: int, actor: ControlActor, state: ControlState, inputs: ControlInputs
) -> ControlEffect:
    """Apply primary a7 or looping wrapper 0c with exact integer widths.

    Blocked requests still advance a7's PC. Wrapper 0c restores that PC and
    requests a scheduler break, including when its inner call is blocked.
    Direction input also invokes the original encounter poll: the recovered
    early-return cases are supported, and active selection is explicit unknown.
    """
    if type(opcode) is not int or opcode not in (0x0C, 0xA7):
        raise ControlError("player control requires primary 0c or a7")
    original_pc = actor.pc
    calls: list[str] = []
    encounter_exit = None
    terrain_call_variant = None
    jump = "control_unavailable"
    if not actor.flags & 0x4000:
        eligibility = "not_control_owner"
        if inputs.preserve_nonplayer_motion == 0:
            actor = replace(actor, flags=actor.flags | 0x01000000)
    elif 0 in inputs.dialogue_status or inputs.encounter.inhibition != 0:
        eligibility = "dialogue" if 0 in inputs.dialogue_status else "inhibited"
        actor = replace(actor, direction=0x8000)
    else:
        eligibility = "available"
        if inputs.held_buttons >> 12:
            calls.append("encounter")
            encounter_exit = encounter_early_exit(inputs.encounter)
            if encounter_exit is None:
                raise UnrecoveredEncounter("active direction-triggered encounter selection")
        counter = state.stationary_counter
        if actor.terrain_flags & 0x00400000:
            if actor.cached_position == actor.integer_position:
                counter = (counter + 1) & 0xFFFF
            # Movement retains the counter on this terrain; it does not reset it.
        else:
            counter = 0
        exceeded = signed16(counter) > 32
        state = replace(state, stationary_counter=32 if exceeded else counter, updated=1)
        held_retry = (
            exceeded
            and bool(inputs.held_buttons & 0x80)
            and not actor.flags & 0x1800
            and inputs.jump_contact == 0xFF
        )
        pressed = bool(inputs.pressed_buttons & 0x80)
        jump = "not_pressed"
        alternate = inputs.jump_mode != 0 and not held_retry
        test_terrain = False
        if held_retry:
            test_terrain = True
        elif alternate:
            if pressed:
                if state.repeat_remaining:
                    jump = "repeat_delay"
                elif inputs.jump_contact != 0xFF:
                    jump = "contact"
                else:
                    test_terrain = True
        elif pressed:
            if actor.flags & 0x1800:
                jump = "jump_or_airborne"
            elif actor.terrain_flags & 0x00400000:
                jump = "counter_terrain"
            elif inputs.jump_contact != 0xFF:
                jump = "contact"
            else:
                test_terrain = True
        if test_terrain:
            calls.append("terrain")
            terrain_call_variant = "alternate" if alternate else "normal"
            if terrain_jump_result(actor):
                jump = "terrain_mask"
            else:
                jump = "held_retry" if held_retry else "pressed"
                actor = replace(
                    actor,
                    flags=actor.flags | 0x800,
                    animation_mode=0xFF if alternate else actor.animation_mode,
                )
                state = replace(state, latched_jump_setting=inputs.jump_setting)
                if alternate:
                    state = replace(state, repeat_remaining=inputs.repeat_delay)
        if alternate and state.repeat_remaining:
            state = replace(state, repeat_remaining=(state.repeat_remaining - 1) & 0xFFFF)
        table = inputs.direction_tables[int(inputs.alternate_directions != 0)]
        direction = table[(inputs.held_buttons >> 12) ^ 15]
        if not direction & 0x8000:
            direction = (direction - inputs.camera_angle) & 0xFFF
        actor = replace(actor, direction=direction)
    actor = replace(actor, pc=(actor.pc + 1) & 0xFFFF)
    if opcode == 0x0C:
        actor = replace(actor, pc=original_pc)
        state = replace(state, break_requested=1)
    return ControlEffect(
        actor, state, eligibility, jump, tuple(calls), encounter_exit, terrain_call_variant
    )
