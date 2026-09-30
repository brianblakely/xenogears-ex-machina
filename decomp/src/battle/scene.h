#ifndef BATTLE_SCENE_H
#define BATTLE_SCENE_H

#include "common.h"

/* Battle scene and effect state. */
extern void *D_800D3344;                /* scene actor records */
extern void *D_800D39CC;                /* scene light entries */
extern u8 D_800C3B74;
extern u8 D_800C3D6C;
extern s32 D_800D2D40;
extern s32 D_800D2D48;
extern s32 D_800C3BAC[9];
extern u8 *D_800D3368[];                /* stage objects */
extern u8 *D_800C3BEC;                  /* effect script cursor */
extern s32 D_800C3BF0;                  /* effect script step count */
extern u16 D_800C3E30;                  /* slot mask */

#endif
