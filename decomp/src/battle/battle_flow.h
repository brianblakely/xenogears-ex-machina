#ifndef BATTLE_FLOW_H
#define BATTLE_FLOW_H

/* Battle start/end steps of the late unit (800B838C-800B9F78): sound
 * playback that waits for its end, the battle's closing, and the acting
 * sprite's walks run through the battle menu (D_800C3610). */

#include "common.h"
#include "scene.h"
#include "battle_core.h"
#include "frame.h"

/* A sound of the battle's table (4 bytes): its sound bank (the wave bank
 * follows it) and its sound number in the bank. */
typedef struct {
    u16 bank;
    s16 sound;
} BattleSound;

extern BattleSound D_800C35DC[];

/* A sound bank load request for 80029AFC as 800B838C builds it on its
 * stack: SoundBanks' fields with unsigned bank numbers, in 0x20 bytes. */
typedef struct {
    u16 bank0;
    void *data0; /* 0x04 */
    u16 bank1;   /* 0x08 */
    void *data1; /* 0x0C */
    s16 field10; /* 0x10 */
    s32 field14; /* 0x14 */
    u8 pad18[8];
} SoundLoad;
extern void *D_800D39C8; /* the enemy set data copy */

s32 func_8003A5D0(s32 sound); /* voices still playing the sound */

#endif
