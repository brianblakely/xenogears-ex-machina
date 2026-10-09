#ifndef PSYQ_INLINE_C_H
#define PSYQ_INLINE_C_H

/* PsyQ inline GTE macros (inline_c.h / gtemac.h form), every one the game's
 * code uses under its SDK name: GTE data and control register transfers
 * through $12-$15 and the GTE commands, each preceded by the two nops the SDK
 * macros emit. Pointer loads declare memory reads so the compiler keeps their
 * producer stores. gte_rtv0tr is not here: the battle and ovl2143 spell it
 * differently (their gte.h). */

/* Control registers: rotation (0-4), light (8-12) and color (16-20)
 * matrices, background color (13-15). */
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
                     : "$12", "$13", "$14", "memory")

#define gte_SetLightMatrix(r0)                                                 \
    __asm__ volatile("lw $12, 0(%0);"                                          \
                     "lw $13, 4(%0);"                                          \
                     "ctc2 $12, $8;"                                           \
                     "ctc2 $13, $9;"                                           \
                     "lw $12, 8(%0);"                                          \
                     "lw $13, 12(%0);"                                         \
                     "lw $14, 16(%0);"                                         \
                     "ctc2 $12, $10;"                                          \
                     "ctc2 $13, $11;"                                          \
                     "ctc2 $14, $12"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")

#define gte_SetColorMatrix(r0)                                                 \
    __asm__ volatile("lw $12, 0(%0);"                                          \
                     "lw $13, 4(%0);"                                          \
                     "ctc2 $12, $16;"                                          \
                     "ctc2 $13, $17;"                                          \
                     "lw $12, 8(%0);"                                          \
                     "lw $13, 12(%0);"                                         \
                     "lw $14, 16(%0);"                                         \
                     "ctc2 $12, $18;"                                          \
                     "ctc2 $13, $19;"                                          \
                     "ctc2 $14, $20"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")

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

#define gte_SetBackColor(r0, r1, r2)                                           \
    __asm__ volatile("sll $12, %0, 4;"                                         \
                     "sll $13, %1, 4;"                                         \
                     "sll $14, %2, 4;"                                         \
                     "ctc2 $12, $13;"                                          \
                     "ctc2 $13, $14;"                                          \
                     "ctc2 $14, $15"                                           \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
                     : "$12", "$13", "$14")

/* A matrix column (three shorts 6 bytes apart) to and from IR1-IR3. */
#define gte_ldclmv(r0)                                                         \
    __asm__ volatile("lhu $12, 0(%0);"                                         \
                     "lhu $13, 6(%0);"                                         \
                     "lhu $14, 12(%0);"                                        \
                     "mtc2 $12, $9;"                                           \
                     "mtc2 $13, $10;"                                          \
                     "mtc2 $14, $11"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14", "memory")

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

/* IR = RT * IR (sf = 1). */
#define gte_rtir() __asm__ volatile("nop;nop;.word 0x4A49E012")

/* Vertices V0 (and V1, V2) from short vectors. */
#define gte_ldv0(r0)                                                           \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0)"                                          \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#define gte_ldv1(r0)                                                           \
    __asm__ volatile("lwc2 $2, 0(%0);"                                         \
                     "lwc2 $3, 4(%0)"                                          \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#define gte_ldv2(r0)                                                           \
    __asm__ volatile("lwc2 $4, 0(%0);"                                         \
                     "lwc2 $5, 4(%0)"                                          \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#define gte_ldv3(r0, r1, r2)                                                   \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 0(%1);"                                         \
                     "lwc2 $3, 4(%1);"                                         \
                     "lwc2 $4, 0(%2);"                                         \
                     "lwc2 $5, 4(%2)"                                          \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
                     : "memory")

/* Three consecutive short vectors into V0-V2. */
#define gte_ldv3c(r0)                                                          \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 8(%0);"                                         \
                     "lwc2 $3, 12(%0);"                                        \
                     "lwc2 $4, 16(%0);"                                        \
                     "lwc2 $5, 20(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Two short vectors into V0 and V1. */
#define gte_ldv01(r0, r1)                                                      \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 0(%1);"                                         \
                     "lwc2 $3, 4(%1)"                                          \
                     :                                                         \
                     : "r"(r0), "r"(r1) : "memory")

/* A 32-bit vector's low halves into V0 (x, y) and its z word. */
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

/* The color RGB and the screen depths SZ0-SZ3. */
#define gte_ldrgb(r0) __asm__ volatile("lwc2 $6, 0(%0)" : : "r"(r0) : "memory")

/* IR0 (the interpolation factor), and IR1-IR3 from a long or a short
 * vector. */
#define gte_lddp(r0) __asm__ volatile("mtc2 %0, $8" : : "r"(r0))

#define gte_ldlvl(r0)                                                          \
    __asm__ volatile("lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0);"                                        \
                     "lwc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0) : "memory")

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

#define gte_ldsz4(r0, r1, r2, r3)                                              \
    __asm__ volatile("mtc2 %0, $16;"                                           \
                     "mtc2 %1, $17;"                                           \
                     "mtc2 %2, $18;"                                           \
                     "mtc2 %3, $19"                                            \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2), "r"(r3))

/* Three packed screen points (y << 16 | x) into SXY0-SXY2, in the SDK
 * macro's order (SXY0, SXY2, SXY1). */
#define gte_ldsxy3(r0, r1, r2)                                                 \
    __asm__ volatile("mtc2 %0, $12;"                                           \
                     "mtc2 %2, $14;"                                           \
                     "mtc2 %1, $13"                                            \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2))

/* The outer product's first vector (into the rotation matrix diagonal) and
 * second vector (IR1-IR3). */
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

/* Commands. RT: MAC1-MAC3 = RT * V0 + TR (sf = 1); RTV0: RT * V0 without TR;
 * DPCS: depth-cue the colour; SQR0: IR1-IR3 squared (sf = 0); OP: the outer
 * product (sf = 1 or 0); GPF: IR = IR0 * IR (sf = 1 or 0). */
#define gte_rtps() __asm__ volatile("nop;nop;.word 0x4A180001")
#define gte_rtpt() __asm__ volatile("nop;nop;.word 0x4A280030")
#define gte_rt() __asm__ volatile("nop;nop;.word 0x4A480012")
#define gte_rtv0() __asm__ volatile("nop;nop;.word 0x4A486012")
#define gte_nclip() __asm__ volatile("nop;nop;.word 0x4B400006")
#define gte_avsz3() __asm__ volatile("nop;nop;.word 0x4B58002D")
#define gte_avsz4() __asm__ volatile("nop;nop;.word 0x4B68002E")
#define gte_nccs() __asm__ volatile("nop;nop;.word 0x4B08041B")
#define gte_dpcs() __asm__ volatile("nop;nop;.word 0x4A780010")
#define gte_sqr0() __asm__ volatile("nop;nop;.word 0x4AA00428")
#define gte_op12() __asm__ volatile("nop;nop;.word 0x4B78000C")
#define gte_op0() __asm__ volatile("nop;nop;.word 0x4B70000C")
#define gte_gpf12() __asm__ volatile("nop;nop;.word 0x4B98003D") /* IR = IR0 * IR >> 12 */
#define gte_gpf0() __asm__ volatile("nop;nop;.word 0x4B90003D") /* sf = 0 */

/* Results: the flag register, the outer product (MAC0), screen points,
 * the interpolation factor (IR0), depths, the average depth (OTZ) and the
 * color. */
#define gte_stflg(r0)                                                          \
    __asm__ volatile("cfc2 $12, $31;"                                          \
                     "nop;"                                                    \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

#define gte_stopz(r0) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy(r0) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy0(r0) __asm__ volatile("swc2 $12, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy1(r0) __asm__ volatile("swc2 $13, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy2(r0) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(r0) : "memory")

#define gte_stsxy3(r0, r1, r2)                                                 \
    __asm__ volatile("swc2 $12, 0(%0);"                                        \
                     "swc2 $13, 0(%1);"                                        \
                     "swc2 $14, 0(%2)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
                     : "memory")

#define gte_stsxy01(r0, r1)                                                    \
    __asm__ volatile("swc2 $12, 0(%0);"                                        \
                     "swc2 $13, 0(%1)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1)                                        \
                     : "memory")

/* The three screen points into a POLY_FT4's x0, x1 and x2. */
#define gte_stsxy3_ft4(r0)                                                     \
    __asm__ volatile("swc2 $12, 8(%0);"                                        \
                     "swc2 $13, 16(%0);"                                       \
                     "swc2 $14, 24(%0)"                                        \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#define gte_stdp(r0) __asm__ volatile("swc2 $8, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsz(r0) __asm__ volatile("swc2 $19, 0(%0)" : : "r"(r0) : "memory")

#define gte_stsz3(r0, r1, r2)                                                  \
    __asm__ volatile("swc2 $17, 0(%0);"                                        \
                     "swc2 $18, 0(%1);"                                        \
                     "swc2 $19, 0(%2)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
                     : "memory")

/* SZ0-SZ3 to four consecutive words. */
#define gte_stsz4c(r0)                                                         \
    __asm__ volatile("swc2 $16, 0(%0);"                                        \
                     "swc2 $17, 4(%0);"                                        \
                     "swc2 $18, 8(%0);"                                        \
                     "swc2 $19, 12(%0)"                                        \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* SZ3 / 4 as an ordering-table depth. */
#define gte_stszotz(r0)                                                        \
    __asm__ volatile("mfc2 $12, $19;"                                          \
                     "nop;"                                                    \
                     "sra $12, $12, 2;"                                        \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

#define gte_stotz(r0) __asm__ volatile("swc2 $7, 0(%0)" : : "r"(r0) : "memory")
#define gte_strgb(r0) __asm__ volatile("swc2 $22, 0(%0)" : : "r"(r0) : "memory")

/* IR1-IR3 to a long vector. */
#define gte_stlvl(r0)                                                          \
    __asm__ volatile("swc2 $9, 0(%0);"                                         \
                     "swc2 $10, 4(%0);"                                        \
                     "swc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* IR1-IR3 to a short vector. */
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

/* MAC1-MAC3 to a long vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* gtemac.h's sequences. r2 = r0 * r1, column by column through the rotation
 * registers. */
#define gte_MulMatrix0(r0, r1, r2)                                             \
    {                                                                          \
        gte_SetRotMatrix(r0);                                                  \
        gte_ldclmv(r1);                                                        \
        gte_rtir();                                                            \
        gte_stclmv(r2);                                                        \
        gte_ldclmv((char *)(r1) + 2);                                          \
        gte_rtir();                                                            \
        gte_stclmv((char *)(r2) + 2);                                          \
        gte_ldclmv((char *)(r1) + 4);                                          \
        gte_rtir();                                                            \
        gte_stclmv((char *)(r2) + 4);                                          \
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

/* r2 = RT * r1 + TR, the flags to r3. */
#define gte_RotTrans(r1, r2, r3)                                               \
    {                                                                          \
        gte_ldv0(r1);                                                          \
        gte_rt();                                                              \
        gte_stlvnl(r2);                                                        \
        gte_stflg(r3);                                                         \
    }

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

/* r3 = r1 x r2 in 1.19.12 fixed point. */
#define gte_OuterProduct12(r1, r2, r3)                                         \
    {                                                                          \
        gte_ldopv1(r1);                                                        \
        gte_ldopv2(r2);                                                        \
        gte_op12();                                                            \
        gte_stlvnl(r3);                                                        \
    }

#endif
