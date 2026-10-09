#ifndef BATTLE_RESULT_INPUT_H
#define BATTLE_RESULT_INPUT_H

#include "battle_core.h"
#include "resident/pad.h"
#include "resident/console.h"

/* Battle input with pausing (8008a3ec). */
extern u8 D_800C3444;  /* the battle is paused */
extern s32 D_80059488; /* vsync count */

/* A party member's result screen block (the 801de000 module). */
typedef struct ResultPanel {
    u8 pad0[0x15F8];
    u8 unk15F8;   /* the second value is counted too */
    u8 counting;  /* +0x15F9 */
    u8 done[2];   /* +0x15FA per value */
} ResultPanel;

extern ResultPanel *D_800D32F8[3];

void func_801DF270(void);
void func_801DF4C0(void);

/* Resident services not yet in a resident header. */
void func_80037EE4(void);           /* pause sound */
void func_80037E8C(void);           /* resume sound */
void func_8001FAB4(s32 a, s32 b);

#endif
