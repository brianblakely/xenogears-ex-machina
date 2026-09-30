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

/* Resident services. */
s32 func_8003F8CC(s32 angle); /* cosine (4096 = 1.0) */

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

#endif
