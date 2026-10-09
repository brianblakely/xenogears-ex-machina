#ifndef OVL2143_GTE_H
#define OVL2143_GTE_H

/* Geometry transformation engine macros in the libgte (gtemac) form that
 * psyq/inline_c.h does not define; the unit takes the others (vertex loads,
 * RTPS/RTPT, NCLIP, NCCS and their stores) from it. */

#include "psyq/inline_c.h"

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

/* Store SZ3 / 4 as an ordering-table depth. */
#define gte_stszotz(r0)                                                        \
    __asm__ volatile("mfc2 $12, $19;"                                          \
                     "nop;"                                                    \
                     "sra $12, $12, 2;"                                        \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

/* Load the outer product's first vector (into the rotation matrix diagonal)
 * and second vector (IR1-IR3), and compute it. */
#define gte_ldopv1(r0)                                                         \
    __asm__ volatile("lw $12, 0(%0);"                                          \
                     "lw $13, 4(%0);"                                          \
                     "ctc2 $12, $0;"                                           \
                     "lw $14, 8(%0);"                                          \
                     "ctc2 $13, $2;"                                           \
                     "ctc2 $14, $4"                                            \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")
#define gte_ldopv2(r0)                                                         \
    __asm__ volatile("lwc2 $11, 8(%0);"                                        \
                     "lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0)"                                         \
                     :                                                         \
                     : "r"(r0) : "memory")
#define gte_op0() __asm__ volatile("nop;nop;.word 0x4B70000C")

/* Average of three depths into OTZ. */
#define gte_avsz3() __asm__ volatile("nop;nop;.word 0x4B58002D")

#endif
