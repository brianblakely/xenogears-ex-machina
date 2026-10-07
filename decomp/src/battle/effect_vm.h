#ifndef BATTLE_EFFECT_VM_H
#define BATTLE_EFFECT_VM_H

/* The battle objects' effect script VM (800AAD54): the object fields it uses
 * beyond BattleObject's, and the services it calls. */

#include "common.h"
#include "psyq.h"
#include "model.h"
#include "scene.h"
#include "effect.h"
#include "objects.h"

/* A script's pending jumps: to atScript once the object comes within
 * atDistance of its position (2E), to groundScript once it reaches the
 * ground (37), to timerScript after timerLimit frames (36). */
#define OBJECT_TIMER(object) (((u16 *)(object)->pad44)[0])       /* 0x44 */
#define OBJECT_TIMER_LIMIT(object) (((u16 *)(object)->pad44)[1]) /* 0x46 */
#define OBJECT_AT_DISTANCE(object) (((s16 *)(object)->pad44)[2]) /* 0x48 */
#define OBJECT_AT_SCRIPT(object) (*(u16 **)&(object)->field4C)
#define OBJECT_TIMER_SCRIPT(object) (*(u16 **)&(object)->field50)
#define OBJECT_GROUND_SCRIPT(object) (*(u16 **)&(object)->field54)
#define OBJECT_FIELD3E(object) (*(u16 *)(object)->pad3E)

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

/* The travel of an animation (its s16 at 0x10), in model units. */
#define ANIMATION_SPAN(animation) (((s16 *)(animation))[8])

extern u8 D_800C3530[]; /* extra file bases */
extern u16 D_800D39E4;
extern u8 D_8005A474[];
extern u8 D_800591B1; /* the sound request is done */
extern u8 D_800D36B8; /* the battle's start mode */

void func_80022224(); /* upload an image (resource, image, at, clut, mode; the points by value) */

/* Services of this unit defined after the VM. */
u16 func_800A1CF4(EffectPool *pool, ModelPart *part, s16 *data, s32 duration, s32 mode, s32 smooth, s32 tag);
void func_800A216C(EffectPool *pool, ModelPart *part, s32 index, s32 mask);
s32 func_800A2434(EffectPool *pool, ModelPart *part, u16 *data, s32 mode, s32 tag);
s32 func_800A2704(EffectPool *pool, ModelPart *part, u16 *data, s32 mode, s32 tag);
u8 func_800AA7DC(s32 index);
void func_800ADF1C(EffectPool *pool, ModelPart *part, s32 duration, s32 x, s32 y, s32 z);
void func_800AE098(EffectPool *pool, ModelPart *part, s32 type, s32 param1, s32 param2, s32 field12, s32 x, s32 y,
                   s32 z);
void func_800AE1BC(BattleObject *object, Animation *animation, s32 loop);
s32 func_800AE220(BattleObject *object, s32 source);
void func_800AEEEC(BattleObject *object);
s32 func_800AEEF8(BattleObject *object);
void func_800AEF68(BattleObject *object);
void func_800AF270(ModelPart *from, ModelPart *to);
s16 func_800AF2C4(VECTOR *direction, VECTOR *a, VECTOR *b, s32 scale);
u8 *func_800AF518(BattleObject *object, u8 index, s32 *flag);
void func_800AF678(BattleObject *object, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 kind, u8 field1,
                   s16 startX, s16 startY, s16 startZ, s16 endX, s16 endY, s16 endZ, s16 duration);
void func_800AFD98(BattleObject *object, ModelPart *part, u8 mode, s16 x, s16 y, s16 z);
void func_800B00F4(EffectPool *pool);
/* Called unprototyped by the VM (its halfwords passed sign-extended). */
void func_800B0164(EffectPool *pool, s32 index, u8 field2, u8 kind, u16 p0, u16 p1, u16 p2, u16 p3, u16 p4,
                   u16 p5, u16 field12);

/* Services of other units. */
void func_8003A3B8(s32 sound, s32 b, s32 c); /* play a sound effect */
void func_80080C6C(u8 index);
u8 func_800885D0(u8 slot);
void func_800B8054(s32 sound);
void func_800B9258(void);
void func_800BCAA4(void);
void func_800BCAD0(void);
void func_800BF998(void);
u8 func_800AF438(BattleObject *object, u8 slot, u16 *mask); /* the slot of a target code */

#endif
