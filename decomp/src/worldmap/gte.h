#ifndef WORLDMAP_GTE_H
#define WORLDMAP_GTE_H

/* The PsyQ GTE macros (inline_c.h/gtemac.h forms) the world map uses beyond
 * psyq/inline_c.h: the translation vector, a long vector load and its
 * product store, the RT command with and without the translation, the
 * composed matrix and RotTrans sequences, and the screen point and depth
 * transfers of the cloud drawing (func_80086798). */

#include "psyq/inline_c.h"

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

/* MAC1-MAC3 = RT * V0 + TR (sf = 1); RTV0: RT * V0 without TR. */
#define gte_rt() __asm__ volatile("nop;nop;.word 0x4A480012")
#define gte_rtv0() __asm__ volatile("nop;nop;.word 0x4A486012")

#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* r3 = r1 * r2, translation included. */
#define gte_CompMatrix(r1, r2, r3)                                             \
    {                                                                          \
        gte_MulMatrix0(r1, r2, r3);                                            \
        gte_SetTransMatrix(r1);                                                \
        gte_ldlv0((u8 *)(r2) + 20);                                            \
        gte_rt();                                                              \
        gte_stlvnl((u8 *)(r3) + 20);                                           \
    }

/* r2 = RT * r1 + TR, the flags to r3. */
#define gte_RotTrans(r1, r2, r3)                                               \
    {                                                                          \
        gte_ldv0(r1);                                                          \
        gte_rt();                                                              \
        gte_stlvnl(r2);                                                        \
        gte_stflg(r3);                                                         \
    }

/* Screen points SXY0-SXY2 from registers, in the SDK macro's order (SXY0,
 * SXY2, SXY1). */
#define gte_ldsxy3(r0, r1, r2)                                                 \
    __asm__ volatile("mtc2 %0, $12;"                                           \
                     "mtc2 %2, $14;"                                           \
                     "mtc2 %1, $13"                                            \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2))

/* Vertices V0-V2 from three consecutive short vectors; the memory read keeps
 * the producer stores of vertices built through a pointer. */
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

/* Screen depths SZ0-SZ3 to four consecutive words. */
#define gte_stsz4c(r0)                                                         \
    __asm__ volatile("swc2 $16, 0(%0);"                                        \
                     "swc2 $17, 4(%0);"                                        \
                     "swc2 $18, 8(%0);"                                        \
                     "swc2 $19, 12(%0)"                                        \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Screen points SXY0-SXY2, or SXY2 alone, to registers (LIBGTE.H's
 * register-argument forms), then a nop for the load delay. */
#define gte_getsxy3(r0, r1, r2)                                                \
    __asm__ volatile("mfc2 %0, $12;"                                           \
                     "mfc2 %1, $13;"                                           \
                     "mfc2 %2, $14;"                                           \
                     "nop"                                                     \
                     : "=r"(r0), "=r"(r1), "=r"(r2))
#define gte_getsxy2(r0) __asm__ volatile("mfc2 %0, $14; nop" : "=r"(r0))

#endif
