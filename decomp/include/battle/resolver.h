#ifndef BATTLE_RESOLVER_H
#define BATTLE_RESOLVER_H

#include "common.h"

/* The action resolver's rolls, formulas, statuses and messages (8008CCCC's
 * unit, 80098C6C-8009E53C, and 8009E53C's 8009E868). */

extern u8 D_800C34AE; /* a weapon-using attack found its item broken */
extern u8 D_800D2D10[4];  /* speeds 8009892c replaces by each gear part speed */

void func_80098C6C(u16 param);            /* resolve an item or effect on its targets */
u16 func_80099890(u8 slot);               /* count down a slot's timed statuses */
s32 func_8009ADA0(u8 slot, s32 *amounts); /* a slot's regeneration amounts */
void func_8009AEFC(u8 slot);              /* revive a slot's record */
void func_8009E868(u8 kind, u16 flag);    /* show an applied gear status's message */

#endif
