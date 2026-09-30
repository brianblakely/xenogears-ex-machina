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

/* libgte-layout short vector. */
typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVector;

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
    u8 unkD4[0x1E];
    s16 unkF2;
    u8 unkF4[0x815];
    u8 model_id;         /* 0x909 */
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

/* Idle scene camera. */
extern s32 D_80092770;
extern s32 D_80092774;
extern u8 D_8009287C;
extern s32 D_8009290C;
extern s32 D_800927AC; /* orbit angle */
extern s32 D_800927B0; /* orbit speed */
extern s32 D_80092794; /* scene mode */
extern s32 D_80092790;
extern s32 D_8009294C;
extern s32 D_80092944; /* elapsed frames */

s32 func_8003FA38(void); /* rand */
s32 func_8003FBF8(char *out, const char *format, ...); /* sprintf */
void func_80083310(s32 arg);
void func_8007A21C(s32 arg);
void func_8007AC3C(void);
void func_800725B0(Actor *actor);

/* Scene actor models. */
typedef struct {
    u16 file;
    void *data;
} Resource;

extern void *D_800927B4[2]; /* loaded model of each actor slot */
extern u8 D_80092920;
extern u8 D_80099D9D;
extern u8 D_80099D9E;
extern s32 D_800928C8; /* menu screen state */
extern s32 D_80059488; /* resident pad buttons */

void func_800320E8(void *block); /* free */
void func_80028470(s32 arg0, s32 arg1);
void func_80028A60(s32 arg);
void *func_800891C0(s32 id);
s32 func_800288EC(s32 file);
void *func_80031BDC(s32 file, s32 arg);
void func_80083BB4(s32 both);

#endif
