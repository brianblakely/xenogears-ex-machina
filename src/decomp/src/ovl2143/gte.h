#ifndef OVL2143_GTE_H
#define OVL2143_GTE_H

/* ovl2143's spelling of gte_rtv0tr, MAC1-MAC3 = RT * V0 + TR (sf = 1): the
 * instruction word 4A480012, as psyq/inline_c.h writes its commands; the
 * battle spells the same word as the cop2 mnemonic (its gte.h). The two
 * spellings build the same objects, but each is part of its unit's cc1
 * output. The unit takes the other GTE macros from psyq/inline_c.h. */

#include "psyq/inline_c.h"

#define gte_rtv0tr() __asm__ volatile("nop;nop;.word 0x4A480012")

#endif
