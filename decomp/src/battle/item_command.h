#ifndef BATTLE_ITEM_COMMAND_H
#define BATTLE_ITEM_COMMAND_H

#include "battle_core.h"

/* Committing a chosen command or item (8008b478-8008c81c). */
extern s8 D_800C2050;  /* results are being applied for a committed item */
extern u8 D_800C3D00;  /* item list row of the chosen item */
extern u8 D_800D3670;  /* item list column of the chosen item */

void func_80079840(u8 actor, u8 target);
void func_80080B64(u8 actor);
void func_8008AC88(u16 mask, u8 actor);

#endif
