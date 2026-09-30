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

/* Store MAC1-MAC3 as a 32-bit vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

/* Normal clip of the three transformed points (MAC0 = outer product). */
#define gte_nclip() __asm__ volatile("nop;nop;.word 0x4B400006")

/* Store MAC0 (the nclip result). */
#define gte_stopz(r0) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(r0) : "memory")

/* Store the three screen depths SZ1-SZ3. */
#define gte_stsz3(r0, r1, r2)                                                  \
    __asm__ volatile("swc2 $17, 0(%0);"                                        \
                     "swc2 $18, 0(%1);"                                        \
                     "swc2 $19, 0(%2)"                                         \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
                     : "memory")

/* Store the three screen points into a POLY_FT4's x0, x1 and x2. */
#define gte_stsxy3_ft4(r0)                                                     \
    __asm__ volatile("swc2 $12, 8(%0);"                                        \
                     "swc2 $13, 16(%0);"                                       \
                     "swc2 $14, 24(%0)"                                        \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#endif
