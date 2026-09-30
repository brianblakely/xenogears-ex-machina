#ifndef BATTLE_SETTLE_H
#define BATTLE_SETTLE_H

/* Knocking down slots and waiting for the slots' sprites to settle
 * (800C0314, 800C0564). */

#include "common.h"

/* The slots to go down (a u16 at 0xA38 of the battle area). */
#define AREA_DOWN_MASK(area) (*(u16 *)(area)->padA38)

extern u16 D_800C37D4[]; /* the idle motion of each shown condition */

s32 func_8009A0DC(); /* the condition shown for a slot (u8 slot; called unprototyped) */

#endif
