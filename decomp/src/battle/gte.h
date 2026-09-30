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

/* Load three short vectors into V0-V2. */
#define gte_ldv3(r0, r1, r2)                                                                       \
    __asm__ volatile("lwc2 $0, 0(%0);"                                                             \
                     "lwc2 $1, 4(%0);"                                                             \
                     "lwc2 $2, 0(%1);"                                                             \
                     "lwc2 $3, 4(%1);"                                                             \
                     "lwc2 $4, 0(%2);"                                                             \
                     "lwc2 $5, 4(%2)"                                                              \
                     :                                                                             \
                     : "r"(r0), "r"(r1), "r"(r2))

/* Perspective-transform vector 0, or vectors 0-2. */
#define gte_rtps() __asm__ volatile("nop;nop;.word 0x4A180001")
#define gte_rtpt() __asm__ volatile("nop;nop;.word 0x4A280030")

/* Store the screen points. */
#define gte_stsxy(r0) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy3(r0, r1, r2)                                                                     \
    __asm__ volatile("swc2 $12, 0(%0);"                                                            \
                     "swc2 $13, 0(%1);"                                                            \
                     "swc2 $14, 0(%2)"                                                             \
                     :                                                                             \
                     : "r"(r0), "r"(r1), "r"(r2)                                                   \
                     : "memory")

/* Store SZ3 / 4 as an ordering-table depth. */
#define gte_stszotz(r0)                                                                            \
    __asm__ volatile("mfc2 $12, $19;"                                                              \
                     "nop;"                                                                        \
                     "sra $12, $12, 2;"                                                            \
                     "sw $12, 0(%0)"                                                               \
                     :                                                                             \
                     : "r"(r0)                                                                     \
                     : "$12", "memory")

/* Normal clipping (the screen triangle's winding into OPZ) and its store. */
#define gte_nclip() __asm__ volatile("nop;nop;.word 0x4B400006")
#define gte_stopz(r0) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(r0) : "memory")

#endif
