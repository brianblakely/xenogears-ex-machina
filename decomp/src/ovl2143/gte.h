#ifndef OVL2143_GTE_H
#define OVL2143_GTE_H

/* Geometry transformation engine macros in the libgte (gtemac) form. */

/* Load a short vector into V0. */
#define gte_ldv0(r0)                                                           \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0)"                                          \
                     :                                                         \
                     : "r"(r0) : "memory")

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

/* Load three short vectors into V0-V2. */
#define gte_ldv3(r0, r1, r2)                                                   \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 0(%1);"                                         \
                     "lwc2 $3, 4(%1);"                                         \
                     "lwc2 $4, 0(%2);"                                         \
                     "lwc2 $5, 4(%2)"                                          \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2) : "memory")

/* Perspective-transform V0, or V0-V2. */
#define gte_rtps() __asm__ volatile("nop;nop;.word 0x4A180001")
#define gte_rtpt() __asm__ volatile("nop;nop;.word 0x4A280030")

/* Store the screen points. */
#define gte_stsxy(r0) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy3(r0, r1, r2)                                                 \
    __asm__ volatile("swc2 $12, 0(%0);"                                        \
                     "swc2 $13, 0(%1);"                                        \
                     "swc2 $14, 0(%2)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
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

/* Store the GTE flag register. */
#define gte_stflg(r0)                                                          \
    __asm__ volatile("cfc2 $12, $31;"                                          \
                     "nop;"                                                    \
                     "sw $12, 0(%0)"                                           \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "memory")

/* Normal clipping (the screen triangle's winding into OPZ) and its store. */
#define gte_nclip() __asm__ volatile("nop;nop;.word 0x4B400006")
#define gte_stopz(r0) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(r0) : "memory")

/* Average of three depths into OTZ and its store. */
#define gte_avsz3() __asm__ volatile("nop;nop;.word 0x4B58002D")
#define gte_stotz(r0) __asm__ volatile("swc2 $7, 0(%0)" : : "r"(r0) : "memory")

/* Light a normal with a colour: load RGB, NCCS, store the result. */
#define gte_ldrgb(r0) __asm__ volatile("lwc2 $6, 0(%0)" : : "r"(r0) : "memory")
#define gte_nccs() __asm__ volatile("nop;nop;.word 0x4B08041B")
#define gte_strgb(r0) __asm__ volatile("swc2 $22, 0(%0)" : : "r"(r0) : "memory")

#endif
