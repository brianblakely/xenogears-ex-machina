#ifndef FIELD_FIELD_GTE_H
#define FIELD_FIELD_GTE_H

/* Geometry transformation engine macros in the libgte (gtemac) form. */

/* Load three packed screen points (y << 16 | x) into SXY0-SXY2. */
#define gte_ldsxy3(r0, r1, r2)                                                 \
    __asm__ volatile("mtc2 %0, $12;"                                           \
                     "mtc2 %2, $14;"                                           \
                     "mtc2 %1, $13"                                            \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2))

/* The winding of SXY0-SXY2 (normal clip) into MAC0. */
#define gte_nclip() __asm__ volatile("nop;nop;.word 0x4B400006")

/* Store MAC0 (the outer product). */
#define gte_stopz(r0) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(r0) : "memory")

/* Load a short vector into V0. */
#define gte_ldv0(r0)                                                           \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0)"                                          \
                     :                                                         \
                     : "r"(r0))

/* Perspective-transform V0. */
#define gte_rtps() __asm__ volatile("nop;nop;.word 0x4A180001")

/* Store the transformed screen point (SXY2). */
#define gte_stsxy(r0) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(r0) : "memory")

/* Store the depth-cue interpolation factor (IR0). */
#define gte_stdp(r0) __asm__ volatile("swc2 $8, 0(%0)" : : "r"(r0) : "memory")

/* Store the GTE flag register. */
#define gte_stflg(r0)                                                          \
    __asm__ volatile("cfc2 $12, $31;"                                          \
                     "nop;"                                                    \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

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

/* Load a colour (CVECTOR) into RGBC. */
#define gte_ldrgb(r0) __asm__ volatile("lwc2 $6, 0(%0)" : : "r"(r0))

/* Depth-cue the colour (DPCS). */
#define gte_dpcs() __asm__ volatile("nop;nop;.word 0x4A780010")

/* Store the last colour of the colour FIFO (RGB2). */
#define gte_strgb(r0) __asm__ volatile("swc2 $22, 0(%0)" : : "r"(r0) : "memory")

/* libgte (gtemac) square: IR1-IR3 from a 32-bit vector, squared into
 * MAC1-MAC3. */
#define gte_ldlvl(r0)                                                          \
    __asm__ volatile("lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0);"                                        \
                     "lwc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0))
#define gte_sqr0() __asm__ volatile("nop;nop;.word 0x4AA00428")

/* Load a matrix's rotation into the GTE. */
#define gte_SetRotMatrix(r0)                                                   \
    __asm__ volatile("lw $12, 0(%0);"                                          \
                     "lw $13, 4(%0);"                                          \
                     "ctc2 $12, $0;"                                           \
                     "ctc2 $13, $1;"                                           \
                     "lw $12, 8(%0);"                                          \
                     "lw $13, 12(%0);"                                         \
                     "lw $14, 16(%0);"                                         \
                     "ctc2 $12, $2;"                                           \
                     "ctc2 $13, $3;"                                           \
                     "ctc2 $14, $4"                                            \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14")

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
                     : "$12", "$13", "$14")

/* Load a matrix column into IR1-IR3. */
#define gte_ldclmv(r0)                                                         \
    __asm__ volatile("lhu $12, 0(%0);"                                         \
                     "lhu $13, 6(%0);"                                         \
                     "lhu $14, 12(%0);"                                        \
                     "mtc2 $12, $9;"                                           \
                     "mtc2 $13, $10;"                                          \
                     "mtc2 $14, $11"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14")

/* Rotate IR1-IR3 by the rotation matrix (sf = 1). */
#define gte_rtir() __asm__ volatile("nop;nop;.word 0x4A49E012")

/* Store IR1-IR3 as a matrix column. */
#define gte_stclmv(r0)                                                         \
    __asm__ volatile("mfc2 $12, $9;"                                           \
                     "mfc2 $13, $10;"                                          \
                     "mfc2 $14, $11;"                                          \
                     "sh $12, 0(%0);"                                          \
                     "sh $13, 6(%0);"                                          \
                     "sh $14, 12(%0)"                                          \
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
                     : "$12", "$13")

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
                     : "$12", "$13", "$14")

/* Load the outer product's second vector into IR1-IR3. */
#define gte_ldopv2(r0)                                                         \
    __asm__ volatile("lwc2 $11, 8(%0);"                                        \
                     "lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0)"                                         \
                     :                                                         \
                     : "r"(r0))

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

/* r3 = r1 * r2 (rotation only). */
#define gte_MulMatrix0(r1, r2, r3)                                             \
    {                                                                          \
        gte_SetRotMatrix(r1);                                                  \
        gte_ldclmv(r2);                                                        \
        gte_rtir();                                                            \
        gte_stclmv(r3);                                                        \
        gte_ldclmv((char *)(r2) + 2);                                          \
        gte_rtir();                                                            \
        gte_stclmv((char *)(r3) + 2);                                          \
        gte_ldclmv((char *)(r2) + 4);                                          \
        gte_rtir();                                                            \
        gte_stclmv((char *)(r3) + 4);                                          \
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
