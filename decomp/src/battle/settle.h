#ifndef BATTLE_SETTLE_H
#define BATTLE_SETTLE_H

/* Returning the slots' sprites to their places after an action, knocking
 * down slots and waiting for the sprites to settle (800BFE48's unit,
 * 800BFE48-800C06E4). */

#include "common.h"

/* The distance between a and b on one axis. */
#define DISTANCE(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

extern u16 battle_condition_idle_motions[]; /* the idle motion of each shown condition */

void battle_return_sprites_home(void);  /* return the slots' sprites to their places */
s32 battle_knock_down_slots(void);      /* knock down the slots of the down mask */
void battle_wait_sprites_settled(void); /* run frames until every slot's sprite has settled */

#endif
