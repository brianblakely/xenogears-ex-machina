#ifndef BATTLE_RESOLVER_H
#define BATTLE_RESOLVER_H

/* The action resolver's rolls and formulas (80096018-8009E53C). */

#include "common.h"

extern u8 D_800C34AE; /* a weapon-using attack found its item broken */
extern u8 D_800D2D10[4];  /* speeds 8009892c replaces by each gear part speed */

void func_80098C6C(u16 param);
u16 func_80099890(u8 slot);
s32 func_8009ADA0(u8 slot, s32 *amounts);
void func_8009AEFC(u8 slot);
void func_8009E868(u8 kind, u16 flag);

#endif
