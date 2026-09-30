#ifndef PSYQ_INLINE_C_H
#define PSYQ_INLINE_C_H

/* PsyQ inline GTE macros (inline_c.h / gtemac.h form): GTE data and control
 * register transfers through $12-$15 and the GTE commands, each preceded by
 * the two nops the SDK macros emit. */

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
                     : "$12", "$13", "$14")

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
                     : "$12", "$13", "$14")

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
                     : "$12", "$13", "$14")

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
                     : "$12", "$13", "$14")

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

/* r2 = r0 * r1, column by column through the rotation registers. */
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

#endif
