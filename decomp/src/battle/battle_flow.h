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
s32 func_8001EE68(u8 *frame); /* whether a sprite frame is a gear's (this unit takes it as a word) */

/* The acting slot's turn (800B89FC-800B9F78). */
extern u8 D_800C3624;
extern s16 D_800C3DF0;

/* D_800C4928 (set when the actor acts with its partner, 8008B478), as the
 * late unit addresses it inside the area. */
#define AREA_PARTNER_ACTION (((u8 *)&BATTLE_AREA)[0xA78])

extern u8 D_800C35D4; /* a sound to fade at the turn's end */

void func_80080BD0(void);
void func_800B9508(BattleSprite *sprite);
void func_800B9B30(void);
void func_800BA8F4(BattleSprite *sprite);
void func_800BEDE8(void);
void func_800BFA9C(void);
void func_800C0314(void);
void func_800C0564(void);
void func_800BC3F8(s32 value);
BattleMenu *func_800BED4C(void);
/* Defined without a return value: 800B89FC takes what it leaves in v0, the
 * new acting sprite. */
BattleSprite *func_800BEFF4(s32 slot);
void func_800BF3E8(BattleSprite *sprite);

#endif
