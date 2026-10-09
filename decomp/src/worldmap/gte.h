#ifndef WORLDMAP_GTE_H
#define WORLDMAP_GTE_H

/* The cloud drawing's (func_80086798) reads of the screen points SXY0-SXY2,
 * or SXY2 alone, into registers, then a nop for the load delay: C forms of
 * LIBGTE.H's read_sxsy_fifo3 and read_sxsy2 assembler macros, which the SDK's
 * inline_c.h does not have. The world map's other GTE macros are
 * psyq/inline_c.h's. */
#define gte_getsxy3(r0, r1, r2)                                                \
    __asm__ volatile("mfc2 %0, $12;"                                           \
                     "mfc2 %1, $13;"                                           \
                     "mfc2 %2, $14;"                                           \
                     "nop"                                                     \
                     : "=r"(r0), "=r"(r1), "=r"(r2))
#define gte_getsxy2(r0) __asm__ volatile("mfc2 %0, $14; nop" : "=r"(r0))

#endif
