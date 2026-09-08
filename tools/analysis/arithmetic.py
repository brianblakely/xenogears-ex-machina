"""Defined integer operations for independently reconstructed original routines."""


def signed16(value: int) -> int:
    low = value & 0xFFFF
    return low - 0x10000 if low & 0x8000 else low


def signed32(value: int) -> int:
    low = value & 0xFFFFFFFF
    return low - 0x100000000 if low & 0x80000000 else low


def divide32(numerator: int, denominator: int) -> int:
    """Signed MIPS quotient, truncated toward zero, with explicit zero rejection."""
    numerator, denominator = signed32(numerator), signed32(denominator)
    if denominator == 0:
        raise ValueError("unresolved original zero-divisor path")
    magnitude = abs(numerator) // abs(denominator)
    return signed32(-magnitude if (numerator < 0) != (denominator < 0) else magnitude)
