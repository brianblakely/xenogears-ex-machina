#ifndef FIELD_FIELD_GTE_H
#define FIELD_FIELD_GTE_H

/* The field's geometry transformation engine macros beyond psyq/inline_c.h,
 * in the libgte (gtemac) form: field.c and field_8007A44C.c use them. No
 * name here is also defined in psyq/inline_c.h. */

#include "psyq/inline_c.h"

/* Load three packed screen points (y << 16 | x) into SXY0-SXY2. */
#define gte_ldsxy3(r0, r1, r2)                                                 \
    __asm__ volatile("mtc2 %0, $12;"                                           \
                     "mtc2 %2, $14;"                                           \
                     "mtc2 %1, $13"                                            \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2))

/* Store the depth-cue interpolation factor (IR0). */
#define gte_stdp(r0) __asm__ volatile("swc2 $8, 0(%0)" : : "r"(r0) : "memory")

/* Store SZ3 / 4 (the ordering-table depth). */
#define gte_stszotz(r0)                                                        \
    __asm__ volatile("mfc2 $12, $19;"                                          \
                     "nop;"                                                    \
                     "sra $12, $12, 2;"                                        \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

/* RotTransPers in line: the screen point, interpolation, flag and depth. */
#define gte_RotTransPers(r1, r2, r3, r4, r5)                                   \
    {                                                                          \
        gte_ldv0(r1);                                                          \
        gte_rtps();                                                            \
        gte_stsxy(r2);                                                         \
        gte_stdp(r3);                                                          \
        gte_stflg(r4);                                                         \
        gte_stszotz(r5);                                                       \
    }

/* Depth-cue the colour (DPCS). */
#define gte_dpcs() __asm__ volatile("nop;nop;.word 0x4A780010")

/* libgte (gtemac) square: IR1-IR3 from a 32-bit vector, squared into
 * MAC1-MAC3. */
#define gte_ldlvl(r0)                                                          \
    __asm__ volatile("lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0);"                                        \
                     "lwc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0) : "memory")
#define gte_sqr0() __asm__ volatile("nop;nop;.word 0x4AA00428")

/* Load a matrix's translation into the GTE. */
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

/* Load a 32-bit vector's low halves into V0 (x, y) and its z word. */
#define gte_ldlv0(r0)                                                          \
    __asm__ volatile("lhu $13, 4(%0);"                                         \
                     "lhu $12, 0(%0);"                                         \
                     "sll $13, $13, 16;"                                       \
                     "or $12, $12, $13;"                                       \
                     "mtc2 $12, $0;"                                           \
                     "lwc2 $1, 8(%0)"                                          \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "memory")

/* Rotate and translate V0 into MAC1-MAC3 (sf = 1). */
#define gte_rt() __asm__ volatile("nop;nop;.word 0x4A480012")

/* Store MAC1-MAC3 as a 32-bit vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Load the outer product's first vector into the rotation diagonal. */
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

/* Load the outer product's second vector into IR1-IR3. */
#define gte_ldopv2(r0)                                                         \
    __asm__ volatile("lwc2 $11, 8(%0);"                                        \
                     "lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0)"                                         \
                     :                                                         \
                     : "r"(r0) : "memory")

/* Outer product (sf = 1). */
#define gte_op12() __asm__ volatile("nop;nop;.word 0x4B78000C")

/* r3 = r1 x r2 in 1.19.12 fixed point. */
#define gte_OuterProduct12(r1, r2, r3)                                         \
    {                                                                          \
        gte_ldopv1(r1);                                                        \
        gte_ldopv2(r2);                                                        \
        gte_op12();                                                            \
        gte_stlvnl(r3);                                                        \
    }

/* r3 = r1 * r2 with r2's translation transformed by r1. */
#define gte_CompMatrix(r1, r2, r3)                                             \
    {                                                                          \
        gte_MulMatrix0(r1, r2, r3);                                            \
        gte_SetTransMatrix(r1);                                                \
        gte_ldlv0((char *)(r2) + 20);                                          \
        gte_rt();                                                              \
        gte_stlvnl((char *)(r3) + 20);                                         \
    }

#endif
