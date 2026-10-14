/*
 * The R3000's DIV and DIVU, which the matched code's C division compiles to.
 * Most of them have no zero check (no break 7 follows them), and the R3000
 * does not trap: a zero divisor leaves LO -1 for a dividend of 0 or more and
 * 1 for a negative one (DIVU: all ones) and HI the dividend; 0x80000000 / -1
 * leaves LO 0x80000000 and HI 0 (psx-spx, "CPU Arithmetic Instructions").
 * WebAssembly's i32.div and rem trap there instead, so tools/game_module.py
 * calls these for each matched-code division whose divisor is not a
 * constant: `/` is LO, `%` is HI.
 */
#include "common.h"

s32 xem_mips_div(s32 dividend, s32 divisor) {
    if (divisor == 0) {
        return dividend >= 0 ? -1 : 1;
    }
    if (dividend == (s32)0x80000000 && divisor == -1) {
        return dividend;
    }
    return dividend / divisor;
}

u32 xem_mips_divu(u32 dividend, u32 divisor) {
    return divisor == 0 ? 0xFFFFFFFF : dividend / divisor;
}

s32 xem_mips_rem(s32 dividend, s32 divisor) {
    if (divisor == 0) {
        return dividend;
    }
    if (dividend == (s32)0x80000000 && divisor == -1) {
        return 0;
    }
    return dividend % divisor;
}

u32 xem_mips_remu(u32 dividend, u32 divisor) {
    return divisor == 0 ? dividend : dividend % divisor;
}
