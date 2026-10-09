#ifndef MENU_BRAIN_H
#define MENU_BRAIN_H

#include "common.h"
#include "actor.h"

/* The computer opponent (menu7 8008EE1C-80090F38): a brain per side that
 * picks commands by mode (idle, attack, distance, approach) from its
 * tendencies and the actors' distance, charge and hp, and enters them as
 * the actor's pad inputs. */

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

extern u8 D_800928C4; /* enables the retreat rule */

void func_8008F280(Actor *actor);
s32 func_8008F4F4(Actor *actor, s32 fraction);
s32 func_8008F530(Actor *actor, s32 check);
s32 func_8008F570(Actor *actor, Brain *brain);
s32 func_8008F720(Actor *actor, s32 eager);
void func_8008F7B8(Brain *brain);
s32 func_8008F9B0(Actor *actor);
s32 func_8008FACC(Actor *actor, Brain *brain);
void func_8008FBD8(Actor *actor, Brain *brain);
void func_8008FC7C(Actor *actor);
void func_8008FE80(Actor *actor);
void func_8008FF24(Actor *actor, Brain *brain);
s32 func_8008FFEC(Actor *actor, Brain *brain);
void func_80090174(Actor *actor);
s32 func_80090258(Actor *actor, Brain *brain);
void func_80090504(Actor *actor, s32 kind);
void func_80090894(Actor *actor, s32 kind);
void func_80090CC0(Actor *actor);
void func_80090E10(Actor *actor);

#endif
