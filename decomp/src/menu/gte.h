#ifndef MENU_GTE_H
#define MENU_GTE_H

/* The PsyQ inline GTE macros (inline_c.h form) the menu uses beyond those of
 * psyq/inline_c.h, and two single screen-depth stores of its own. */

#include "psyq/inline_c.h"

/* The translation of a MATRIX into TRX-TRZ. */
#define gte_SetTransMatrix(r0)                                                 \
    __asm__ volatile("lw $12, 20(%0);"                                         \
                     "lw $13, 24(%0);"                                         \
                     "ctc2 $12, $5;"                                           \
                     "lw $14, 28(%0);"                                         \
                     "ctc2 $13, $6;"                                           \
                     "ctc2 $14, $7"                                            \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")

/* Load two short vectors into V0 and V1. */
#define gte_ldv01(r0, r1)                                                      \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 0(%1);"                                         \
                     "lwc2 $3, 4(%1)"                                          \
                     :                                                         \
                     : "r"(r0), "r"(r1) : "memory")

/* Load three consecutive short vectors into V0-V2. */
#define gte_ldv3c(r0)                                                          \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 8(%0);"                                         \
                     "lwc2 $3, 12(%0);"                                        \
                     "lwc2 $4, 16(%0);"                                        \
                     "lwc2 $5, 20(%0)"                                         \
                     :                                                         \
                     : "r"(r0) : "memory")

/* Load a 32-bit vector into IR1-IR3. */
#define gte_ldlvl(r0)                                                          \
    __asm__ volatile("lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0);"                                        \
                     "lwc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0) : "memory")

/* Load a short vector into IR1-IR3. */
#define gte_ldsv(r0)                                                           \
    __asm__ volatile("lhu $12, 0(%0);"                                         \
                     "lhu $13, 2(%0);"                                         \
                     "lhu $14, 4(%0);"                                         \
                     "mtc2 $12, $9;"                                           \
                     "mtc2 $13, $10;"                                          \
                     "mtc2 $14, $11"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")

/* Load IR0 (the interpolation factor). */
#define gte_lddp(r0) __asm__ volatile("mtc2 %0, $8" : : "r"(r0))

/* Rotate and translate V0 by the current matrix into MAC1-MAC3 (sf = 1). */
#define gte_rt() __asm__ volatile("nop;nop;.word 0x4A480012")

/* Rotate V0 by the current matrix, without the translation (sf = 1). */
#define gte_rtv0() __asm__ volatile("nop;nop;.word 0x4A486012")

/* Scale IR1-IR3 by IR0 (GPF, sf = 1). */
#define gte_gpf12() __asm__ volatile("nop;nop;.word 0x4B98003D")

/* Square IR1-IR3 into MAC1-MAC3 (sf = 0). */
#define gte_sqr0() __asm__ volatile("nop;nop;.word 0x4AA00428")

/* Depth-cue the colour (DPCS). */
#define gte_dpcs() __asm__ volatile("nop;nop;.word 0x4A780010")

/* Store MAC1-MAC3 as a 32-bit vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Store IR1-IR3 as a short vector. */
#define gte_stsv(r0)                                                           \
    __asm__ volatile("mfc2 $12, $9;"                                           \
                     "mfc2 $13, $10;"                                          \
                     "mfc2 $14, $11;"                                          \
                     "sh $12, 0(%0);"                                          \
                     "sh $13, 2(%0);"                                          \
                     "sh $14, 4(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")

/* Store the first two screen points. */
#define gte_stsxy01(r0, r1)                                                    \
    __asm__ volatile("swc2 $12, 0(%0);"                                        \
                     "swc2 $13, 0(%1)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1)                                        \
                     : "memory")

/* Store the three screen points into a POLY_FT4's x0, x1 and x2. */
#define gte_stsxy3_ft4(r0)                                                     \
    __asm__ volatile("swc2 $12, 8(%0);"                                        \
                     "swc2 $13, 16(%0);"                                       \
                     "swc2 $14, 24(%0)"                                        \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Store SZ3 divided by four (an ordering table index). */
#define gte_stszotz(r0)                                                        \
    __asm__ volatile("mfc2 $12, $19;"                                          \
                     "nop;"                                                    \
                     "sra $12, $12, 2;"                                        \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

/* Store one screen depth of the last RTPT: SZ1 or SZ2 (gte_stsz stores SZ3). */
#define gte_stsz1(r0) __asm__ volatile("swc2 $17, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsz2(r0) __asm__ volatile("swc2 $18, 0(%0)" : : "r"(r0) : "memory")

#endif
