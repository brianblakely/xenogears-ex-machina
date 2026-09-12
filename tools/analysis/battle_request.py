"""Recovered primary 71 field battle-request model, based on original 80093568.

The handler retries without reading its operand until six gates allow a request.
It then latches a mode, resolves the tagged selector and publishes request state.
This source model does not enter battle, resolve combat or establish readiness.
Unknown gate roles retain their source suffixes rather than invented semantics.
EVID-REF-031 records source review and six exact original calls. Other gate and
variable paths have source/synthetic coverage, not original-execution coverage.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

from .events import EventError, Variables
from .field import region


def _integer(value: int, bits: int) -> None:
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise EventError(f"battle request value must fit u{bits}")


@dataclass(frozen=True)
class BattleRequestGates:
    field_active: int
    gate_e4: int
    gate_ec: int
    menu_gate: int
    music_result: int
    gate_90: int

    def __post_init__(self) -> None:
        for value in (
            self.field_active,
            self.gate_e4,
            self.gate_ec,
            self.menu_gate,
            self.music_result,
            self.gate_90,
        ):
            _integer(value, 32)


@dataclass(frozen=True)
class BattleRequestState:
    pc: int
    break_requested: int
    gate_e0: int
    pending: int
    selector: int
    mode: int
    resident_flag: int

    def __post_init__(self) -> None:
        _integer(self.pc, 16)
        for value in (self.break_requested, self.gate_e0, self.pending):
            _integer(value, 32)
        for value in (self.selector, self.mode, self.resident_flag):
            _integer(value, 8)


@dataclass(frozen=True)
class BattleRequestEffect:
    state: BattleRequestState
    gates: BattleRequestGates
    accepted: bool
    retry_reason: str | None
    selector_value: int | None
    # Original 8009360c: mode is already stored, before the request stores.
    resolution_state: BattleRequestState | None


def request_battle(
    bytecode: bytes,
    state: BattleRequestState,
    gates: BattleRequestGates,
    mode: int,
    variables: Variables | None = None,
) -> BattleRequestEffect:
    """Model primary 71 with explicit original inputs and bounded source semantics.

    Tagged immediates use 15 bits; variable reads retain the original signedness
    and odd-reference alias before the selector store truncates to one byte.
    Operand addresses do not wrap at the PC boundary, but the final PC store does.
    Malformed input fails without mutating caller state; this is a bounded host
    interface, not original invalid-memory or transactional runtime behavior.
    """
    _integer(mode, 8)
    if len(bytecode) > 0x10000:
        raise EventError("battle request bytecode exceeds the original u16 PC space")
    if region(bytecode, state.pc, 1, "battle request opcode")[0] != 0x71:
        raise EventError("battle request requires primary opcode 71")
    for blocked, reason in (
        (gates.field_active == 0, "field_inactive"),
        (gates.gate_e4 == 0, "gate_e4_clear"),
        (gates.gate_ec == 0, "gate_ec_clear"),
        (gates.menu_gate != 0, "menu_gate_set"),
        (gates.music_result == 0xFFFFFFFF, "music_pending"),
        (gates.gate_90 != 0, "gate_90_set"),
    ):
        if blocked:
            return BattleRequestEffect(
                replace(state, break_requested=1), gates, False, reason, None, None
            )
    latched = replace(state, mode=mode)
    operand = int.from_bytes(region(bytecode, state.pc + 1, 2, "battle selector operand"), "little")
    if operand & 0x8000:
        selector = operand & 0x7FFF
    else:
        if variables is None:
            raise EventError("battle selector variable requires the original variable bank")
        selector = variables.read(operand)
    result = replace(
        latched,
        pc=(state.pc + 3) & 0xFFFF,
        break_requested=1,
        gate_e0=0,
        pending=1,
        selector=selector & 255,
        resident_flag=0,
    )
    return BattleRequestEffect(
        result, replace(gates, field_active=0), True, None, selector, latched
    )
