#ifndef BATTLE_ITEM_COMMAND_H
#define BATTLE_ITEM_COMMAND_H

#include "common.h"

/* The technique and item menus and committing their choice (8008B478's unit,
 * 8008B478-8008C81C; battle.c 80085C48, 8008AC88-8008B224; the enemy and
 * event queue sides in 800792F8's and 80079ED8's units; character 4's gear
 * items in 8008CCCC's, 8009A7E4-8009A854). */
extern u8 D_800C2050;  /* results are being applied for a committed item (read with lbu) */
extern u8 D_800C3D00;  /* item list row of the chosen item */
extern u8 D_800D3670;  /* item list column of the chosen item */

extern s8 D_800C4928;
extern u8 D_800D3688[0x30]; /* gear part counts */

void func_80079840(u8 actor, u8 target);        /* tell an enemy who acts on it */
void func_80080B64(u8 actor);                   /* close the actor's event queue */
void func_80085C48(); /* commit an item's targets; K&R, callers pass them unconverted */
void func_8008AC88(u16 mask, u8 actor);         /* queue event 0xf3 for the reacting enemies */
void func_8008B108(u8 keep);                    /* hide the command windows */
u8 func_8008B224(u8 member, u8 column, u8 row); /* confirm a technique */
u8 func_8008B478(u8 member);                    /* run the technique menu; 1 when chosen */
void func_8008B908(u8 member);                  /* execute the chosen item */
u8 func_8008BED8(u8 member);                    /* run the item menu; 1 when chosen */
s32 func_8009A7E4(u8 index);                    /* whether a gear item is one of character 4's */
void func_8009A854(u8 index, u8 k);             /* put a battle item into character 4's entries */

#endif
