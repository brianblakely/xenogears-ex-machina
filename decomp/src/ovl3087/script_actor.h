#ifndef OVL3087_SCRIPT_ACTOR_H
#define OVL3087_SCRIPT_ACTOR_H

#include "common.h"
#include "resident/sprite.h"

/* Actor and model helpers of the battle event scripts (801e93e8-801e9b58),
 * a separate unit built by a later compiler (see ovl3087.mk), on the battle
 * slots' sprites (the resident's Sprite) and the script slots' model sprites
 * (SpriteTasks). The interpreter (ovl3087.c) declares these helpers with its
 * own prototypes. */

/* Resident functions whose callers convert arguments/result differently
 * from the resident definition (decomp/src/resident/own_declarations.h). */
void sprite_set_direction(Sprite *sprite, s32 arg);
void sprite_set_facing(Sprite *sprite, s32 arg);

/* Defined u8 in the battle (own_declarations.h); u8 here adds andi 0xff to the result's test. */
s32 battle_single_action_start(void);

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
