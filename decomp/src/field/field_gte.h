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

#endif
