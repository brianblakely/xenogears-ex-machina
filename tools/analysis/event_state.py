"""Reconstructed field slot selection, termination, waits and variable writes.

Original evidence: field overlay with SHA256 38a1ce82...79467e1fdfc, identified
fully in EVID-REF-016. Native semantic values are separated from the external
RAM correlation codec. Unknown bits and all unrelated bytes remain intact.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, replace

from tools.analysis.arithmetic import signed16
from tools.analysis.events import EventError, Instruction, Variables


@dataclass(frozen=True)
class EventSlot:
    resume_pc: int
    countdown: int
    event_tag: int
    control_bits: int

    @property
    def priority(self) -> int:
        return (self.control_bits >> 18) & 15


@dataclass(frozen=True)
class ActorScripts:
    slots: tuple[EventSlot, ...]
    selected_slot: int
    pc: int

    def __post_init__(self) -> None:
        if len(self.slots) != 8 or not 0 <= self.selected_slot < 8 or not 0 <= self.pc <= 0xFFFF:
            raise EventError("actor scripts need eight slots, a valid selected slot and u16 PC")
        for slot in self.slots:
            if not (
                0 <= slot.resume_pc <= 0xFFFF
                and 0 <= slot.countdown <= 255
                and 0 <= slot.event_tag <= 255
                and 0 <= slot.control_bits <= 0xFFFFFFFF
            ):
                raise EventError("original event slot field exceeds its integer width")

    def with_slot(self, index: int, slot: EventSlot) -> ActorScripts:
        if not 0 <= index < 8:
            raise EventError("event slot index outside the original eight slots")
        slots = list(self.slots)
        slots[index] = slot
        return replace(self, slots=tuple(slots))


@dataclass(frozen=True)
class InterpreterControl:
    # Correlated with 0x800affec and 0x800b00c0 in the original field overlay.
    # Preserve integers: the dispatch loop tests exact 0/1 values differently.
    budget_mode: int
    break_requested: int


@dataclass(frozen=True)
class EventEffect:
    actor: ActorScripts
    control: InterpreterControl
    variables: Variables | None


def select_slot(actor: ActorScripts, idle_entry: int) -> ActorScripts:
    """0x800a2194..0x800a2214: minimum priority, last slot wins a tie.

    If every slot has priority 15, original slot zero resumes event entry one at
    priority seven. Other bits, countdown and tag are deliberately preserved.
    The working PC is assigned by the caller after this selection stage.
    """
    if not 0 <= idle_entry <= 0xFFFF:
        raise EventError("idle entry must fit the original u16 PC")
    selected = min(range(8), key=lambda index: (actor.slots[index].priority, -index))
    if actor.slots[selected].priority == 15:
        selected = 0
        slot = actor.slots[0]
        actor = actor.with_slot(
            0,
            replace(
                slot, resume_pc=idle_entry, control_bits=(slot.control_bits & ~0x3C0000) | 0x1C0000
            ),
        )
    return replace(actor, selected_slot=selected)


def execute(
    instruction: Instruction,
    actor: ActorScripts,
    control: InterpreterControl,
    variables: Variables | None = None,
    *,
    idle_entry: int | None = None,
) -> EventEffect:
    """Apply exactly one recovered original handler, without an invented clock."""
    if instruction.pc != actor.pc:
        raise EventError("instruction and actor PC disagree")
    opcode = instruction.opcode
    if opcode in (0, 4):
        if opcode == 4:
            if idle_entry is None or not 0 <= idle_entry <= 0xFFFF:
                raise EventError("reset_idle_and_end requires the actor's original event-one entry")
            actor = replace(
                actor,
                slots=tuple(
                    replace(slot, resume_pc=idle_entry) if slot.priority == 7 else slot
                    for slot in actor.slots
                ),
            )
        slot = actor.slots[actor.selected_slot]
        actor = actor.with_slot(
            actor.selected_slot,
            replace(slot, control_bits=slot.control_bits | 0x3C0000, event_tag=255),
        )
        control = replace(
            control, break_requested=1, budget_mode=1 if opcode == 0 else control.budget_mode
        )
    elif opcode == 1:
        actor = replace(actor, pc=instruction.operands[0])
    elif opcode == 0x26:
        slot = actor.slots[actor.selected_slot]
        if slot.countdown:
            countdown = slot.countdown - 1
        else:
            operand = instruction.operands[0]
            if operand & 0x8000:
                value = operand & 0x7FFF
            elif variables is not None:
                value = variables.read(operand)
            else:
                raise EventError("variable wait requires the original variable bank")
            countdown = value & 255
        actor = actor.with_slot(actor.selected_slot, replace(slot, countdown=countdown))
        if countdown == 0:
            actor = replace(actor, pc=(actor.pc + 3) & 0xFFFF)
        control = replace(control, break_requested=1)
    elif opcode in (0x35, 0x36, 0x37, 0x38, 0x39):
        if variables is None:
            raise EventError("variable instruction requires the original variable bank")
        destination = instruction.operands[0]
        offset = variables.index(destination) * 2
        if opcode in (0x36, 0x37):
            value = 1 if opcode == 0x36 else 0
        else:
            raw, mode = instruction.operands[1:]
            value = signed16(raw) if mode & 0x40 else variables.read(raw)
            if opcode == 0x38:
                value = variables.read(destination) + value
            elif opcode == 0x39:
                value = variables.read(destination) - value
        bank = bytearray(variables.values)
        struct.pack_into("<H", bank, offset, value & 0xFFFF)
        variables = Variables(bytes(bank), variables.unsigned_bits)
        actor = replace(actor, pc=(actor.pc + instruction.size) & 0xFFFF)
    else:
        raise EventError(f"no recovered state transition for opcode 0x{opcode:02x}")
    return EventEffect(actor, control, variables)


def read_actor_scripts(original: bytes) -> ActorScripts:
    """External original-memory correlation only; not a native memory-layout API."""
    if len(original) < 0xD0:
        raise EventError("original actor prefix is too short for event state")
    slots = tuple(EventSlot(*struct.unpack_from("<HBBI", original, 0x8C + i * 8)) for i in range(8))
    return ActorScripts(slots, original[0xCE], struct.unpack_from("<H", original, 0xCC)[0])


def correlate_actor_scripts(original: bytes, actor: ActorScripts) -> bytes:
    """Preserve opaque bytes when comparing a semantic transition with original RAM."""
    read_actor_scripts(original)
    output = bytearray(original)
    for i, slot in enumerate(actor.slots):
        struct.pack_into(
            "<HBBI",
            output,
            0x8C + i * 8,
            slot.resume_pc,
            slot.countdown,
            slot.event_tag,
            slot.control_bits,
        )
    struct.pack_into("<H", output, 0xCC, actor.pc)
    output[0xCE] = actor.selected_slot
    return bytes(output)
