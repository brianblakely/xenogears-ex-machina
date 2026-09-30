#ifndef FIELD_GTE_H
#define FIELD_GTE_H

/* Geometry transformation engine macros in the libgte inline (gtemac) form. */

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
