"""Readable reconstruction of a recovered subset of original field instructions.

Derived from the hash-qualified field overlay: primary handlers 0x00..0x02,
0x800a2fe0/0x800a3018 variable reads and 0x800acd7c/0x800acdb8 operands.
Slot control, waits and variable stores are reconstructed in event_state.py.
This analysis module is not a runtime, complete scheduler or fallback VM.
"""

from __future__ import annotations

from dataclasses import dataclass

from tools.analysis.arithmetic import signed16
from tools.analysis.field import region


class EventError(ValueError):
    """An event cannot be interpreted within the recovered instruction subset."""


class UnknownInstruction(EventError):
    def __init__(self, pc: int, opcode: int, *, namespace: str = "primary"):
        self.pc, self.opcode, self.namespace = pc, opcode, namespace
        super().__init__(
            f"unimplemented {namespace} opcode 0x{opcode:02x} at bytecode PC +0x{pc:04x}"
        )


COMPARISONS = (
    "equal",
    "not_equal",
    "greater",
    "less",
    "greater_or_equal",
    "less_or_equal",
    "and_nonzero",
    "not_equal_alias",
    "or_nonzero",
    "and_nonzero_alias",
    "not_left_and_right_nonzero",
)

OPCODE_SPECS = {
    0x00: ("end_slot", 1),
    0x01: ("jump", 3),
    0x02: ("branch_if_false", 8),
    0x04: ("reset_idle_and_end", 1),
    0x0C: ("loop_player_control", 1),
    0x26: ("wait_countdown", 3),
    0x35: ("set_variable", 6),
    0x36: ("set_variable_one", 3),
    0x37: ("set_variable_zero", 3),
    0x38: ("add_variable", 6),
    0x39: ("subtract_variable", 6),
    0x71: ("request_battle", 3),
    0xA7: ("request_player_control", 1),
    0xFE: ("extended_dispatch", 2),
}


@dataclass(frozen=True)
class Instruction:
    pc: int
    opcode: int
    name: str
    size: int
    successors: tuple[int, ...]
    operands: tuple[int, ...] = ()


def decode_instruction(bytecode: bytes, pc: int) -> Instruction:
    if type(pc) is not int or not 0 <= pc <= 0xFFFF:
        raise EventError(f"PC outside the original u16 address space: {pc}")
    if len(bytecode) > 0x10000:
        raise EventError("bytecode exceeds the original u16 PC address space")
    opcode = region(bytecode, pc, 1, "opcode")[0]
    if opcode not in OPCODE_SPECS:
        raise UnknownInstruction(pc, opcode)
    name, size = OPCODE_SPECS[opcode]
    if opcode == 0xFE:
        # The original wrapper stores the incremented u16 working PC before
        # reading the extended opcode. This particular prefix can wrap at ffff.
        extended_pc = (pc + 1) & 0xFFFF
        extended = region(bytecode, extended_pc, 1, "extended opcode")[0]
        if extended != 0xA2:
            raise UnknownInstruction(extended_pc, extended, namespace="extended")
        following = (pc + 2) & 0xFFFF
        if following >= len(bytecode):
            raise EventError(f"extended instruction at +0x{pc:04x} advances outside bytecode")
        return Instruction(pc, opcode, "wait_music_load", 2, (pc, following), (extended,))
    source = region(bytecode, pc, size, f"opcode 0x{opcode:02x} operands")

    def word(offset: int) -> int:
        return int.from_bytes(source[offset : offset + 2], "little")

    if opcode in (0, 4):
        # The original marks the active slot ended and yields without moving PC.
        return Instruction(pc, opcode, name, size, ())
    if opcode == 0x0C:
        # The wrapper restores its input PC after a7 and requests a break.
        return Instruction(pc, opcode, name, size, (pc,))
    if opcode == 1:
        result = Instruction(pc, opcode, "jump", size, (word(1),), (word(1),))
    elif opcode == 2:
        mode, comparison = source[5] & 0xF0, source[5] & 0x0F
        if mode not in (0x00, 0x40, 0x80, 0xC0) or comparison >= len(COMPARISONS):
            raise EventError(f"unsupported branch mode 0x{source[5]:02x} at +0x{pc:04x}")
        result = Instruction(
            pc,
            opcode,
            "branch_if_false",
            size,
            ((pc + 8) & 0xFFFF, word(6)),
            (word(1), word(3), mode, comparison),
        )
    else:
        operands = () if size == 1 else (word(1), word(3), source[5]) if size == 6 else (word(1),)
        next_pc = (pc + size) & 0xFFFF
        successors = (pc, next_pc) if opcode in (0x26, 0x71) else (next_pc,)
        result = Instruction(pc, opcode, name, size, successors, operands)
    for target in result.successors:
        if target >= len(bytecode):
            raise EventError(f"opcode at +0x{pc:04x} targets +0x{target:04x} outside bytecode")
    return result


@dataclass(frozen=True)
class Variables:
    """The original field's 1024 halfwords and per-variable unsigned type bits."""

    values: bytes
    unsigned_bits: bytes

    def __post_init__(self) -> None:
        if len(self.values) != 2048 or len(self.unsigned_bits) != 128:
            raise EventError("variable bank needs 2048 value bytes and 128 type bytes")

    def index(self, reference: int) -> int:
        if type(reference) is not int or not 0 <= reference < 2048:
            raise EventError(f"variable byte reference outside recovered bank: {reference}")
        # The original SRA discards the low bit; odd references alias the even one.
        return reference >> 1

    def unsigned(self, reference: int) -> bool:
        index = self.index(reference)
        return bool(self.unsigned_bits[index >> 3] & (1 << (index & 7)))

    def read(self, reference: int) -> int:
        offset = self.index(reference) * 2
        value = int.from_bytes(self.values[offset : offset + 2], "little")
        return value if self.unsigned(reference) else signed16(value)


def branch_operands(instruction: Instruction, variables: Variables) -> tuple[int, int]:
    if instruction.opcode != 2:
        raise EventError("operand resolution requires a decoded conditional branch")
    left_raw, right_raw, mode, _ = instruction.operands
    left = signed16(left_raw) if mode & 0x80 else variables.read(left_raw)
    right = signed16(right_raw) if mode & 0x40 else variables.read(right_raw)
    if mode in (0x00, 0x40):
        right = right & 0xFFFF if variables.unsigned(left_raw) else signed16(right)
    elif mode == 0x80 and variables.unsigned(right_raw):
        left &= 0xFFFF
    return left, right


def branch_matches(comparison: int, left: int, right: int) -> bool:
    if comparison == 0:
        return left == right
    if comparison in (1, 7):
        return left != right
    if comparison == 2:
        return left > right
    if comparison == 3:
        return left < right
    if comparison == 4:
        return left >= right
    if comparison == 5:
        return left <= right
    if comparison in (6, 9):
        return bool(left & right)
    if comparison == 8:
        return bool(left | right)
    if comparison == 10:
        return bool(~left & right)
    raise EventError(f"unsupported branch comparison {comparison}")


def branch_next_pc(instruction: Instruction, variables: Variables) -> int:
    left, right = branch_operands(instruction, variables)
    matches = branch_matches(instruction.operands[3], left, right)
    return instruction.successors[0 if matches else 1]


def disassemble_reachable(bytecode: bytes, entry: int) -> tuple[Instruction, ...]:
    """Follow both known branches. Unknown instructions fail with their exact PC.

    Never guess an unknown instruction's length to continue a linear sweep.
    Overlapping instructions are rejected rather than reinterpreting operands.
    """
    pending, decoded, owners = [entry], {}, {}
    while pending:
        pc = pending.pop()
        if pc in decoded:
            continue
        instruction = decode_instruction(bytecode, pc)
        for relative in range(instruction.size):
            offset = (pc + relative) & 0xFFFF
            if offset in owners:
                raise EventError(
                    f"overlapping instructions at +0x{pc:04x} and +0x{owners[offset]:04x}"
                )
            owners[offset] = pc
        decoded[pc] = instruction
        pending.extend(instruction.successors)
    return tuple(decoded[pc] for pc in sorted(decoded))
