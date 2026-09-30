#ifndef BATTLE_GTE_H
#define BATTLE_GTE_H

/* GTE instruction macros as the PsyQ library's inline_c.h expands them. */

/* Load vector 0 (three halfwords). */
#define gte_ldv0(r0)                                                                               \
    __asm__ volatile("lwc2 $0, 0(%0);"                                                             \
                     "lwc2 $1, 4(%0)"                                                              \
                     :                                                                             \
                     : "r"(r0))

/* Rotate and translate vector 0 (MVMVA 1, 0, 0, 0, 0). */
#define gte_rtv0tr()                                                                               \
    __asm__ volatile("nop;"                                                                        \
                     "nop;"                                                                        \
                     "cop2 0x0480012")

/* Store the result vector (three words). */
#define gte_stlvnl(r0)                                                                             \
    __asm__ volatile("swc2 $25, 0(%0);"                                                            \
                     "swc2 $26, 4(%0);"                                                            \
                     "swc2 $27, 8(%0)"                                                             \
                     :                                                                             \
                     : "r"(r0)                                                                     \
                     : "memory")

#endif
