"""Original field interpreter batch policy, separated from handler side effects.

Derived from 0x800a1ec8..0x800a202c of the qualified shared field overlay.
This reconstruction consumes control values after an actual handler. It neither
implements unknown handlers nor assumes that they have no effect on the budget.
"""

from dataclasses import dataclass

from tools.analysis.arithmetic import signed32
from tools.analysis.event_state import InterpreterControl
from tools.analysis.events import EventError


@dataclass(frozen=True)
class DispatchGate:
    enabled: int
    values: tuple[int, int, int]

    @property
    def stops_batch(self) -> bool:
        return self.enabled != 0 and any(value == 0 for value in self.values)


@dataclass(frozen=True)
class BatchStep:
    counter: int
    budget_u32: int
    control: InterpreterControl
    next_stage: str
    reason: str


def begin_batch(requested_u32: int, control: InterpreterControl) -> BatchStep:
    if type(requested_u32) is not int or not 0 <= requested_u32 <= 0xFFFFFFFF:
        raise EventError("original batch budget must be a u32 register value")
    control = InterpreterControl(control.budget_mode, 0)
    if signed32(requested_u32) <= 0:
        return BatchStep(0, requested_u32, control, "return", "nonpositive-budget")
    return BatchStep(0, requested_u32, control, "loop-check", "positive-budget")


def dispatch_allowed(counter: int) -> bool:
    if type(counter) is not int or not 0 <= counter <= 0x401:
        raise EventError("original loop counter outside the reachable batch range")
    # Counter zero dispatches the first handler. Counter 1024 still dispatches;
    # the next top-of-loop check stops at 1025, possibly emitting a diagnostic.
    return counter < 0x401


def after_handler(
    counter: int, budget_u32: int, control: InterpreterControl, gate: DispatchGate
) -> BatchStep:
    if not dispatch_allowed(counter):
        raise EventError("original batch limit would prevent this handler call")
    if type(budget_u32) is not int or not 0 <= budget_u32 <= 0xFFFFFFFF:
        raise EventError("original post-handler budget must be a u32 register value")
    if control.budget_mode == 0:
        budget_u32 = 0xFFFF
    if gate.stops_batch:
        return BatchStep(counter, budget_u32, control, "return", "dispatch-gate")
    if control.break_requested == 1 and control.budget_mode == 1:
        return BatchStep(counter, budget_u32, control, "return", "break-request")
    counter += 1
    if counter < signed32(budget_u32):
        return BatchStep(counter, budget_u32, control, "loop-check", "remaining-budget")
    return BatchStep(counter, budget_u32, control, "return", "budget-exhausted")
