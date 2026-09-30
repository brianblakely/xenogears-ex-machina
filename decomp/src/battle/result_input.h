#ifndef BATTLE_RESULT_INPUT_H
#define BATTLE_RESULT_INPUT_H

#include "battle_core.h"
#include "../resident/pad.h"

/* Battle input with pausing (8008a3ec). */
extern u8 D_800C3444;  /* the battle is paused */
extern s32 D_80059488; /* vsync count */

/* Resident services not yet in a resident header. */
void func_80037EE4(void);           /* pause sound */
void func_80037E8C(void);           /* resume sound */
void func_8001FAB4(s32 a, s32 b);

#endif
