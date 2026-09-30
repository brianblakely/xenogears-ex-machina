#ifndef BATTLE_RESOLVER_H
#define BATTLE_RESOLVER_H

/* The action resolver's rolls and formulas (80096018-8009E53C). */

#include "combatant.h"

extern u8 D_800C34AE; /* a weapon-using attack found its item broken */
extern u16 D_800C3AA4[3]; /* each member's status7A before the battle's adjustments */
extern u8 D_800D2D10[4];  /* speeds 8009892c replaces by each gear part speed */
extern u16 D_8006EF64;    /* game data word; below 0xbb 8009892c adjusts gears */

s32 func_80099498(void);
void func_8009B684(u8 kind, u16 flag);
void func_8009B46C(u16 *damage);

#endif
