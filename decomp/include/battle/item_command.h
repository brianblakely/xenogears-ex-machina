#ifndef BATTLE_ITEM_COMMAND_H
#define BATTLE_ITEM_COMMAND_H

#include "common.h"

/* Committing a chosen command or item (8008b478-8008c81c). */
extern u8 D_800C2050;  /* results are being applied for a committed item (read with lbu) */
extern u8 D_800C3D00;  /* item list row of the chosen item */
extern u8 D_800D3670;  /* item list column of the chosen item */

extern s8 D_800C4928;
extern u8 D_800D3688[0x30]; /* gear part counts */

void func_80079840(u8 actor, u8 target);
void func_80080B64(u8 actor);
void func_80085C48(); /* commit an item's targets; K&R, callers pass them unconverted */
void func_8008AC88(u16 mask, u8 actor);
void func_8008B108(u8 keep);                    /* hide the command windows */
u8 func_8008B224(u8 member, u8 column, u8 row); /* confirm a technique */
u8 func_8008B478(u8 member); /* the member has a gear list */
void func_8008B908(u8 member);
u8 func_8008BED8(u8 member);
s32 func_8009A7E4(u8 index);
void func_8009A854(u8 index, u8 k);

#endif
