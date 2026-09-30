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

/* Store MAC1-MAC3 as a 32-bit vector. */
#define gte_stlvnl(r0)                                                         \
    __asm__ volatile("swc2 $25, 0(%0);"                                        \
                     "swc2 $26, 4(%0);"                                        \
                     "swc2 $27, 8(%0)"                                         \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#endif
