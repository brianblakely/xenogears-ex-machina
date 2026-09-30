#ifndef MENU_GTE_H
#define MENU_GTE_H

/* Geometry transformation engine macros in the libgte (gtemac) form. */

/* Load a 32-bit vector into IR1-IR3. */
#define gte_ldlvl(r0)                                                          \
    __asm__ volatile("lwc2 $9, 0(%0);"                                         \
                     "lwc2 $10, 4(%0);"                                        \
                     "lwc2 $11, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0))

/* Load a colour (CVECTOR) into RGBC. */
#define gte_ldrgb(r0) __asm__ volatile("lwc2 $6, 0(%0)" : : "r"(r0))

/* Square IR1-IR3 into MAC1-MAC3 (sf = 0). */
#define gte_sqr0() __asm__ volatile("nop;nop;.word 0x4AA00428")

/* Load two short vectors into V0 and V1. */
#define gte_ldv01(r0, r1)                                                      \
    __asm__ volatile("lwc2 $0, 0(%0);"                                         \
                     "lwc2 $1, 4(%0);"                                         \
                     "lwc2 $2, 0(%1);"                                         \
                     "lwc2 $3, 4(%1)"                                          \
                     :                                                         \
                     : "r"(r0), "r"(r1))

/* Perspective-transform V0-V2. */
#define gte_rtpt() __asm__ volatile("nop;nop;.word 0x4A280030")

/* Store the first two screen points. */
#define gte_stsxy01(r0, r1)                                                    \
    __asm__ volatile("swc2 $12, 0(%0);"                                        \
                     "swc2 $13, 0(%1)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1)                                        \
                     : "memory")

/* Store the second-last screen Z (SZ2). */
#define gte_stsz1(r0) __asm__ volatile("swc2 $18, 0(%0)" : : "r"(r0) : "memory")

/* Store MAC1-MAC3 as a 32-bit vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Rotate and translate V0 by the current matrix into MAC1-MAC3 (sf = 1). */
#define gte_rt() __asm__ volatile("nop;nop;.word 0x4A480012")

#endif
