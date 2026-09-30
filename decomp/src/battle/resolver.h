#ifndef BATTLE_RESOLVER_H
#define BATTLE_RESOLVER_H

/* The action resolver's rolls and formulas (80096018-8009E53C). */

#include "combatant.h"

extern u8 D_800C34AE; /* a weapon-using attack found its item broken */

s32 func_80099498(void);
void func_8009B684(u8 kind, u16 flag);
void func_8009B46C(u16 *damage);

#endif
