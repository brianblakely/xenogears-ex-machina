#ifndef MENU_BRAIN_H
#define MENU_BRAIN_H

#include "menu.h"

/* The computer opponent's decision state, handed to its command handlers. */
typedef struct {
    u8 unk0[0x4];
    s16 timer;          /* 0x04: frames until the next decision */
    u8 unk6[0x4];
    s16 unkA;
    s16 unkC;
    u8 unkE[0x1E];
    u32 unk2C_0 : 13;
    u32 defending : 1;  /* 0x2C bit 13 */
    u32 unk2C_14 : 18;
} Brain;

extern s32 D_8009284C;

s32 func_8008B650(s32 angle, s16 target, s32 step); /* turn angle toward target */
void func_800767C8(Actor *actor);
void func_8008FE80(Actor *actor);
void func_8007639C(Actor *actor, s32 input); /* queue a command input */

#endif
