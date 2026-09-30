#ifndef MENU_BRAIN_H
#define MENU_BRAIN_H

#include "menu.h"

/* The computer opponent's decision state, handed to its command handlers. */
typedef struct Brain {
    u8 unk0[0x4];
    s16 timer;          /* 0x04: frames until the next decision */
    u8 unk6[0x2];
    u8 mode;            /* 0x08 */
    u8 unk9;
    s16 unkA;
    s16 unkC;
    u8 unkE;
    u8 unkF;
    u8 unk10[0x1C];
    u32 unk2C_0 : 13;
    u32 defending : 1;  /* 0x2C bit 13 */
    u32 unk2C_14 : 18;
} Brain;

extern s32 D_8009284C;
extern u8 D_80099DA2; /* the opponent's current command */
extern u8 D_80092848; /* the command the brain last started */
extern u8 D_80099D9E; /* nonzero while the opponent is being driven */

s32 func_8008B650(s32 angle, s16 target, s32 step); /* turn angle toward target */
void func_800767C8(Actor *actor);
void func_8008FE80(Actor *actor);
void func_8007639C(Actor *actor, s32 input); /* queue a command input */
void func_80090E10(Actor *actor);

#endif
