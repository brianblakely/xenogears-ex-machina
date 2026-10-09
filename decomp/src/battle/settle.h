#ifndef BATTLE_SETTLE_H
#define BATTLE_SETTLE_H

/* Returning the slots' sprites to their places after an action, knocking
 * down slots and waiting for the sprites to settle (800BFE48-800C0564). */

#include "common.h"

/* The distance between a and b on one axis. */
#define DISTANCE(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

extern u16 D_800C37D4[]; /* the idle motion of each shown condition */

void func_800BFE48(void);
s32 func_800C0314(void);
void func_800C0564(void);

#endif
