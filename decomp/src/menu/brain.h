#ifndef MENU_BRAIN_H
#define MENU_BRAIN_H

#include "menu.h"

/* The computer opponent's decision state, handed to its command handlers. */
typedef struct Brain {
    Actor *owner;
    s16 timer;          /* 0x04: frames until the next decision */
    u8 unk6;
    u8 unk7;
    u8 mode;            /* 0x08: 0 idle, 1 attack, 2 distance, 3 approach */
    u8 unk9;
    s16 unkA;
    s16 unkC;
    u8 unkE;
    u8 unkF;
    s32 unk10;          /* 0x10: attack eagerness */
    s32 unk14;
    s32 unk18;          /* 0x18: chance to press an attack */
    s32 unk1C;          /* 0x1C: eagerness when not keen */
    s32 unk20;
    s32 unk24;          /* 0x24: charge it waits for */
    s32 unk28;
    u32 roll : 8;       /* 0x2C: a random byte for this round */
    u32 unk2C_8 : 1;
    u32 unk2C_9 : 1;
    u32 unk2C_10 : 1;
    u32 unk2C_11 : 1;
    u32 unk2C_12 : 1;
    u32 defending : 1;  /* 0x2C bit 13 */
    u32 unk2C_14 : 2;
    u32 unk2E : 8;      /* 0x2E */
    u32 unk2C_24 : 8;
    s16 unk30;
} Brain;

/* Which special moves an actor has learned and may use. */
typedef struct MoveList {
    u8 unk0[0x6];
    u8 tendency[4]; /* 0x06: eagerness values for the brain */
    u8 learned[14]; /* 0x0A */
} MoveList;

typedef struct MoveSlot {
    u8 unk0[0x3];
    u8 usable;
} MoveSlot;

extern u8 D_800925A4[14][3]; /* each special move's command inputs */

extern s32 D_8009284C;
extern s32 D_80092850;
extern u8 D_80092848; /* the command the brain last started */
extern u8 D_800928C4; /* enables the retreat rule */

s32 func_8008B650(s32 angle, s16 target, s32 step); /* turn angle toward target */
void func_800767C8(Actor *actor);
void func_8008FE80(Actor *actor);
void func_8007639C(Actor *actor, s32 input); /* queue a command input */
void func_80090E10(Actor *actor);
s32 func_80073DE4(Actor *actor, s32 amount);
void func_80076424(Actor *actor);
void func_8008F7B8(struct Brain *brain);
void func_80090174(Actor *actor);
void func_80090894(Actor *actor, s32 kind);
void func_80090504(Actor *actor, s32 kind);
s32 func_8008FACC(Actor *actor, struct Brain *brain);
void func_8008FBD8(Actor *actor, struct Brain *brain);
void func_8008FC7C(Actor *actor);
void func_8008FF24(Actor *actor, struct Brain *brain);
s32 func_8008FFEC(Actor *actor, struct Brain *brain);
s32 func_80090258(Actor *actor, struct Brain *brain);
s32 func_8008F720(Actor *actor, s32 eager);

extern Brain D_80096F30; /* brain of the side-0 opponent */
extern Brain D_80096F64; /* brain of the side-1 opponent */
/* Opponent settings. */
typedef struct {
    u8 level;   /* 0x0: 0-2 */
    u8 unk1[0x5];
    u8 driven;  /* 0x6: nonzero while the opponent is being driven */
    u8 unk7[0x3];
    u8 command; /* 0xA: the opponent's current command */
} OpponentSettings;

extern OpponentSettings D_80099D98;
s32 func_8008F570(Actor *actor, struct Brain *brain);

#endif
