#ifndef BATTLE_SETTLE_H
#define BATTLE_SETTLE_H

/* Returning the slots' sprites to their places after an action, knocking
 * down slots and waiting for the sprites to settle (800BFE48-800C0564). */

#include "common.h"
#include "sprite_effect.h"

/* The distance between a and b on one axis. */
#define DISTANCE(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

extern u16 D_800C37D4[]; /* the idle motion of each shown condition */

/* Called unprototyped here (their slot argument is a u8). */
s32 func_8009A0DC(); /* the condition shown for a slot */
s32 func_8009A1AC(); /* the status bits shown for a slot (a u16, taken as int) */

void func_800BAEB8(s32 slot);
void func_800BFD88(Sprite *sprite, s32 mode);
void func_800BFDA8(Sprite *sprite, s32 mode);
s32 func_800C0314(void);
void func_800C0564(void);

#endif
