#ifndef BATTLE_FLOW_H
#define BATTLE_FLOW_H

#include "common.h"
#include "battle/area.h"
#include "battle/frame.h"

/* The battle's flow (800B8098's unit, 800B8098-800B9F78): its start and
 * closing, the loads it waits for, and the acting slot's turn run through
 * the battle menu (D_800C3610): sound playback that waits for its end and
 * the acting sprite's walks. */

/* A sound of the battle's table (4 bytes): its sound bank (the wave bank
 * follows it) and its sound number in the bank. */
typedef struct {
    u16 bank;
    s16 sound;
} BattleSound;

extern BattleSound D_800C35DC[];

extern void *D_800D39C8; /* the enemy set data copy */

/* The acting slot's turn (800B89FC-800B9F78). */
extern u8 D_800C3624;

/* D_800C4928 (set when the actor acts with its partner, 8008B478), as the
 * late unit addresses it inside the area. */
#define AREA_PARTNER_ACTION (((u8 *)&BATTLE_AREA)[0xA78])

/* D_800C4923, after the acting slot, as the late unit addresses it. */
#define AREA_BYTE_A73 (((u8 *)&BATTLE_AREA)[0xA73])

extern u16 D_800D39E4; /* the single action to request (effect VM 71, 73) */

extern s32 D_800C3660; /* the battle menu update is running */
extern s16 D_800C3614; /* frames before the next event */
extern u8 D_800C3623;
extern u16 D_800C3626; /* slots whose gear sound played */
extern s16 D_800C3630[]; /* per target code: its first command */
extern s16 D_800C3648[]; /* per target code: its commands from here play motion 0x11 */

void func_800B8098(s32 kind);  /* start the battle in a mode */
void func_800B81BC(s32 a);     /* enter the battle */
void func_800B8354(void);      /* run frames while the disc is busy */
void func_800B853C(s32 mode);  /* close the battle */
void func_800B89FC(s32 mode, s32 slot, s32 targets, s32 arg3); /* open the battle menu for a turn */
void func_800B8D04(void);      /* finish the battle's loads */
void func_800B8D7C(void);      /* stop the resident transfer and finish the loads */
void func_800B9258(void);      /* count a step of the battle menu */
/* Mark the battle menu (field48) with its state; a sprite callback (also the
 * event script overlay's), defined without a prototype (800B9508 also calls
 * it with the sprite). */
void func_800B9B30();
/* Put the sprite at its target, idle, facing the other; defined without a
 * prototype (800BF0C4 calls it with the sprite alone). */
void func_800B9C00();
void func_800B9F78(BattleMenu *menu); /* the battle menu's update */

#endif
