#ifndef OVL3087_SCRIPT_ACTOR_H
#define OVL3087_SCRIPT_ACTOR_H

#include "common.h"
#include "resident/sprite.h"

/* Actor and model helpers of the battle event scripts (801e93e8-801e9b58),
 * a separate unit built by a later compiler (see ovl3087.mk), on the battle
 * slots' sprites (the resident's Sprite) and the script slots' model sprites
 * (SpriteTasks). The interpreter (ovl3087.c) declares these helpers with its
 * own prototypes. */

/* The battle's actor task callbacks (800B9B30: put the sprite at its target;
 * 800BAB0C, 800BABDC, 800BAC50: a slot sprite task's update, second update
 * and destroy), under the names this overlay's link gives them. */
extern void D_800B9B30(Sprite *sprite);
extern void D_800BAB0C(Task *task);
extern void D_800BABDC(Task *task);
extern void D_800BAC50(Task *task);

/* Resident functions whose callers convert arguments/result differently
 * from the resident definition (decomp/src/resident/own_declarations.h). */
void func_80021FE0(Sprite *sprite, s32 arg);
void func_800223B0(Sprite *sprite, s32 arg);

/* Battle functions whose callers convert arguments/result differently from
 * the battle's definition (decomp/src/battle/own_declarations.h; 800BF7C8 is
 * declared in its unit). */
s32 func_800B7E94(void);
void func_800BC404(u16 arg);
s16 func_800BEEB4(s32 mask, Sprite **list, Sprite *target);
void func_800BF7C8(Sprite *sprite, s32 arg1, void (*callback)(Sprite *sprite));

void func_801E93E8(Sprite *actor);
void func_801E9430(s32 actor, s32 animation);
void func_801E950C(s32 actor);
void func_801E9550(s32 actor);
void func_801E958C(s32 actor);
void func_801E95B0(Sprite *actor);
void func_801E95E4(s16 actor, s16 x, s16 y, s16 z);
void func_801E9694(s16 actor, s16 x, s16 y, s16 z);
void func_801E9700(s32 actor, s32 arg1);
void func_801E9760(s32 actor, s32 target);
void func_801E9894(s32 actor, u16 target);
void func_801E9958(SpriteTask *model, s32 animation);
SpriteTask *func_801E9978(void *file, s16 *position);
void func_801E9AD4(SpriteTask *model);
void func_801E9B2C(void);

#endif
