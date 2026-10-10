#ifndef BATTLE_GTE_H
#define BATTLE_GTE_H

/* The battle's spelling of gte_rtv0tr, MAC1-MAC3 = RT * V0 + TR (sf = 1):
 * the cop2 mnemonic of the instruction word 4A480012, which ovl2143 writes as
 * the word itself (its gte.h). The two spellings build the same objects, but
 * each is part of its unit's cc1 output, so neither is in psyq/inline_c.h,
 * which holds the other GTE macros. */
#define gte_rtv0tr()                                                                               \
    __asm__ volatile("nop;"                                                                        \
                     "nop;"                                                                        \
                     "cop2 0x0480012")

#endif
