#ifndef RESIDENT_GTE_H
#define RESIDENT_GTE_H

/* PsyQ inline GTE macros (inline_c.h) the sprite colour scaling uses. */

/* IR0 (the interpolation factor). */
#define gte_lddp(r0) __asm__ volatile("mtc2 %0, $8" : : "r"(r0))

/* IR1-IR3 from a long vector. */
#define gte_ldlvl(r0)                                                          \
    __asm__ volatile("lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0);"                                        \
                     "lwc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0))

/* IR = IR0 * IR >> 12. */
#define gte_gpf12() __asm__ volatile("nop;nop;.word 0x4B98003D")

/* IR1-IR3 to a long vector. */
#define gte_stlvl(r0)                                                          \
    __asm__ volatile("swc2 $9, 0(%0);"                                         \
                     "swc2 $10, 4(%0);"                                        \
                     "swc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#endif
