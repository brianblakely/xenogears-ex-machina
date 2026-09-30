#ifndef OVL2143_GTE_H
#define OVL2143_GTE_H

/* Geometry transformation engine macros in the libgte (gtemac) form. */

/* Load a short vector into V0. */
#define gte_ldv0(r0)                                                           \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0)"                                          \
                     :                                                         \
                     : "r"(r0))

/* Rotate V0 and add the translation: MAC1-3 = RT * V0 + TR (sf = 1). */
#define gte_rtv0tr() __asm__ volatile("nop;nop;.word 0x4A480012")

/* Store MAC1-MAC3 as a 32-bit vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#endif
