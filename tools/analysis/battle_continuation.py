"""Source model of field request continuation through FE/7F and primary 86.

The exact field overlay dispatches these to 8008a244 and 80096724. FE/7F
tests the request-pending word. Primary 86 compares typed variable reference zero
with a tagged operand; this does not identify that variable's gameplay meaning.
This bounded analysis model is not a battle result or scheduler implementation.
Original replay and independent review are recorded separately from this source.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

from .events import EventError, Variables
from .field import region


def _unsigned(value: int, bits: int) -> None:
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise EventError(f"continuation value must fit u{bits}")


def _opcode(bytecode: bytes, pc: int, opcode: int) -> None:
    if len(bytecode) > 0x10000:
        raise EventError("continuation bytecode exceeds the original u16 PC space")
    if region(bytecode, pc, 1, "continuation opcode")[0] != opcode:
        raise EventError(f"continuation requires primary opcode {opcode:02x}")


@dataclass(frozen=True)
class ContinuationState:
    pc: int
    break_requested: int

    def __post_init__(self) -> None:
        _unsigned(self.pc, 16)
        _unsigned(self.break_requested, 32)


@dataclass(frozen=True)
class PendingWaitEffect:
    state: ContinuationState
    # The FE wrapper stores this u16 PC before reading the extended opcode.
    dispatch_state: ContinuationState
    pending: int
    ready: bool


@dataclass(frozen=True)
class VariableZeroBranchEffect:
    state: ContinuationState
    comparison_value: int
    variable_zero: int
    equal: bool
    target_read: int | None


def wait_request_pending(
    bytecode: bytes, state: ContinuationState, pending: int
) -> PendingWaitEffect:
    """Apply FE/7F from the prefix PC, including the wrapper's u16 increment.

    The handler always requests a break. It advances past both bytes when the
    u32 pending word is zero, or returns to the FE prefix when nonzero. The word
    itself is preserved. Prefix dispatch wraps; this differs from operand-reader
    pointer addition. Rejected host input does not expose partial original stores.
    """
    _unsigned(pending, 32)
    _opcode(bytecode, state.pc, 0xFE)
    dispatch_pc = (state.pc + 1) & 0xFFFF
    if region(bytecode, dispatch_pc, 1, "extended continuation opcode")[0] != 0x7F:
        raise EventError("continuation wait requires extended opcode 7f")
    dispatched = replace(state, pc=dispatch_pc)
    ready = pending == 0
    next_pc = (dispatch_pc + (1 if ready else -1)) & 0xFFFF
    return PendingWaitEffect(
        replace(state, pc=next_pc, break_requested=1), dispatched, pending, ready
    )


def branch_variable_zero(
    bytecode: bytes, state: ContinuationState, variables: Variables
) -> VariableZeroBranchEffect:
    """Apply primary 86: continue when typed variable zero equals the operand.

    The first operand is a 15-bit tagged immediate or a typed variable reference.
    Unequal values read a raw u16 destination at PC+3; equal values advance by
    five without reading those target bytes. Operand addresses do not wrap, and
    the final PC store does. No break or variable-bank store occurs here.

    A supplied byte buffer and recovered variable bank bound the host interface;
    missing data is rejected without mutating inputs. This does not reproduce
    original invalid-memory effects or require the resulting PC to be in buffer.
    """
    _opcode(bytecode, state.pc, 0x86)
    operand = int.from_bytes(
        region(bytecode, state.pc + 1, 2, "continuation comparison operand"), "little"
    )
    comparison = operand & 0x7FFF if operand & 0x8000 else variables.read(operand)
    zero = variables.read(0)
    equal = zero == comparison
    target = None
    if equal:
        next_pc = (state.pc + 5) & 0xFFFF
    else:
        target = int.from_bytes(
            region(bytecode, state.pc + 3, 2, "continuation branch target"), "little"
        )
        next_pc = target
    return VariableZeroBranchEffect(replace(state, pc=next_pc), comparison, zero, equal, target)
