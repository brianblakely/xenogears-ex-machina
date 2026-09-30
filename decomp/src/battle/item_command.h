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
u8 func_8008C81C(u8 member);          /* run the combo menu */
extern u8 D_80059468[3]; /* per party member: its character data index */
void func_80085C48(); /* commit an item's targets; K&R, callers pass them unconverted */
u8 func_8008BD50(); /* use the list item at (column, row); K&R */
void func_8008B108(u8 keep);                    /* hide the command windows */
u8 func_8008B224(u8 member, u8 column, u8 row); /* confirm a technique */
void func_800BAF40(u8 slot, s32 mode);
extern s8 D_800C4928;

#endif
