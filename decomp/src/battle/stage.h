#ifndef BATTLE_STAGE_H
#define BATTLE_STAGE_H

#include "common.h"
#include "psyq.h"
#include "scene.h"
#include "battle_core.h"

/* An object's model or extra data (fields as far as recovered). */
struct ObjectData {
    u8 pad0[8];
    SoundSystem *sounds; /* 0x08: its sound bank */
};

/* Per-frame update and drawing of the stage objects. */
extern s32 D_800CCC5C;     /* frames skipped by the last frame */
extern s16 D_800C3B80;     /* pulse level of the highlight colour */
extern s16 D_800D39E8;     /* a slow wave (4..9) */
extern u16 D_800C3D14;     /* highlighted slots */
extern u8 D_800C3DF8;      /* effects run */
extern Matrix *D_800D2FC0; /* the stage colour matrix */
extern s32 D_80050104;     /* resident: drawing with lighting */
extern SoundSystem *D_800C4924;
extern SVector D_800D3354; /* camera position */
extern SVector D_800D335C; /* camera look-at point */
extern s16 D_800C3542;     /* last scene triangle under the camera's view point */
extern s16 D_800C3544;     /* its ground height */
extern s16 D_800C3546;     /* key of the last update */

/* Resident services. */
s32 func_8003F8B0(s32 angle); /* rcos (4096 = 1.0) */
s32 func_8003F8CC(s32 angle); /* rsin */
s32 ratan2(s32 y, s32 x);
void func_8004A480(Vector *a, Vector *b, Vector *out); /* OuterProduct12 */

void func_8009F844(BattleObject *object, s32 arg1, s32 arg2, s32 arg3, s32 skipped, s32 arg5, s32 arg6);
void func_800A2FD8(SpritePool *pool, s32 arg1, s32 steps, s32 arg3, s32 arg4);
void func_800A429C(u8 *anim);
void func_800A44C0(BattleObject **objects);
void func_800A4CF8(s32 index);
s32 func_800AAA20(BattleObject *object, EffectPool *pool, s32 steps, s32 arg3, s32 arg4);
void func_800AAB34(BattleObject *object);
u8 func_800AA514(s16 a, s16 b, s32 c);
s32 func_800AA600(s32 index);
void func_8009F794(ModelList *list, s32 release);
void func_800A2ACC(EffectPool *pool, ModelPart *part);
void func_800A2BB8(EffectPool *pool, ModelPart *part, u8 kind);
void func_800B026C(EffectPool *pool, s32 steps, s32 arg2, s32 arg3);

/* A resident sprite task (fields as far as the battle uses them): its
 * sprite's position from +0x38, and at +link its caller block. */
typedef struct EffectSprite {
    u8 pad0[0x38];
    s32 x, y, z; /* 0x38: 16.16 */
    u8 pad44[0xBE - 0x44];
    s16 link; /* 0xBE */
} EffectSprite;

/* The battle's block of a sprite following an object part (0x18 bytes). */
typedef struct {
    u8 pad0[4];
    void (*update)(EffectSprite *sprite); /* 0x04: the sprite's own update */
    BattleObject *object;                 /* 0x08 */
    s16 part;                             /* 0x0C: 0 the root */
    s16 onGround;                         /* 0x0E: keep the object's ground height */
    SVector offset;                       /* 0x10: from the part */
} SpriteFollow;

/* An animation script command creating a sprite (fields as far as used). */
typedef struct {
    u8 pad0[5];
    u8 part;      /* 0x05 */
    s16 offset[3]; /* 0x06 */
    u8 onGround;  /* 0x0C */
    u8 padD[0x13 - 0xD];
    u8 follow;    /* 0x13 */
} SpriteCommand;

/* Resident sprites. */
EffectSprite *func_80023FD8(s32 kind, void *resource, SVector *position, s32 size);
void func_80021FE0(s32 *body, s32 direction);
void func_800223B0(s32 *body, s32 direction);
void func_80022000(s32 *body, s32 scale);
void *func_8001CD7C(EffectSprite *sprite); /* the task's update */
void func_8001CD6C(EffectSprite *sprite, void (*update)(EffectSprite *sprite)); /* set it */

void func_800AFC68(EffectSprite *sprite);

#endif
