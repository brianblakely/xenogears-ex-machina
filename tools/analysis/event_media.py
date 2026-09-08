"""Recovered extended-dispatch entry and music-loading wait.

Original field wrapper 0x800869b8 and extended handler a2 at 0x8008825c.
The field music poller owns the external load result; this handler only tests
its exact pending sentinel. It does not mean that player control is ready.
"""

from dataclasses import replace

from tools.analysis.event_state import ActorScripts, EventEffect, InterpreterControl
from tools.analysis.events import EventError, Instruction


def enter_extended(actor: ActorScripts) -> ActorScripts:
    return replace(actor, pc=(actor.pc + 1) & 0xFFFF)


def wait_music_load_extended(
    actor: ActorScripts, control: InterpreterControl, load_result_u32: int
) -> EventEffect:
    """Apply a2 at its extended PC, after the prefix's original increment."""
    if type(load_result_u32) is not int or not 0 <= load_result_u32 <= 0xFFFFFFFF:
        raise EventError("original music load result must fit u32")
    delta = -1 if load_result_u32 == 0xFFFFFFFF else 1
    return EventEffect(
        replace(actor, pc=(actor.pc + delta) & 0xFFFF),
        replace(control, break_requested=1),
        None,
    )


def execute_music_wait(
    instruction: Instruction,
    actor: ActorScripts,
    control: InterpreterControl,
    load_result_u32: int,
) -> EventEffect:
    if instruction.opcode != 0xFE or instruction.operands != (0xA2,):
        raise EventError("music wait requires recovered primary fe / extended a2")
    if instruction.pc != actor.pc:
        raise EventError("music-wait instruction and working PC disagree")
    return wait_music_load_extended(enter_extended(actor), control, load_result_u32)
