#ifndef MENU_H
#define MENU_H

#include "common.h"

/* libgte-layout vector: three 32-bit components and padding. */
typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} Vector;

/* A character moved in the menu scene. */
typedef struct {
    Vector pos;          /* 0x00 */
    u8 unk10[0x38];
    s32 state;           /* 0x48 */
    u8 unk4C[0x8];
    s32 angle;           /* 0x54: facing, 4096 = full turn */
    s32 target_angle;    /* 0x58 */
    u8 unk5C[0x72];
    s16 unkCE;
    s32 flags;           /* 0xD0 */
    u8 unkD4[0x837];
    u8 unk90B;
} Actor;

/* Menu camera: eye position (D_8009867C) and look-at point (D_8009871C). */
extern Vector D_8009867C;
extern Vector D_8009871C;
extern s32 D_800925F4; /* vertical camera lift of the current view */
extern Actor D_80097010;
extern Actor D_8009872C;
extern Vector D_80099078;
extern u8 D_80092954[];

s32 func_8003F8B0(s32 angle); /* sine, 4096 = 1.0 */
s32 func_8003F8CC(s32 angle); /* cosine, 4096 = 1.0 */
void func_800346D4(void *arg);
void func_80083C0C(s32 arg);
void func_80083738(Actor *actor, Actor *other);
void func_800828F8(Vector *position, Vector *step, s32 limit);
s32 func_80082488(Vector *position, s32 arg);
s32 func_8004B32C(s32 x, s32 z); /* angle of a direction, 4096 = full turn */
void func_8007E24C(void);

extern s32 D_800925F8;
extern s32 D_800925FC;
extern s32 D_80092934;

#endif
