#ifndef OVL2143_ACTOR_H
#define OVL2143_ACTOR_H

/* The unit's own view of its actors (ovl2143/actors.h has the records and
 * the entries its callers share): their script, animation and drawing
 * functions, and the sprites linked to their nodes. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "ovl2143/actors.h"
#include "hierarchy.h"
#include "image_anim.h"
#include "particles.h"
#include "surface.h"

/* Callers convert arguments/result differently from the resident
 * definition (s16 angles and ids there, its result a bank pointer; s16
 * modes): turn a sprite and set its angle, the loaded bank with `bank`'s id
 * (nonzero when there is one) and an image list upload. */
void sprite_set_direction(Sprite *sprite, s32 direction);
void sprite_set_facing(Sprite *sprite, s32 angle);
s32 sound_find_effect_bank(SoundBank *bank, s32 id);
void model_load_image_list(void *images, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);

/* This overlay's link of a sprite to an actor node. */
typedef struct {
    u8 pad0[4];
    void (*update)(Task *task); /* +4: the sprite's own update */
    Actor *actor;           /* +8 */
    s16 node;               /* +c */
    s16 follow;             /* +e: take the height from the actor */
    SVECTOR offset;         /* +10 */
} SpriteLink;

void field_layer_redraw_hook(void);

void func_801DCEC8(Actor *actor, MATRIX *m, MATRIX *light, s32 mode, s32 ticks, u32 *ot, s32 buffer);
void func_801E1880(Actor **actors);
void func_801E3534(Actor *actor, EffectPool *pool, s32 *entries, s32 *locals);
void func_801E35D0(Actor *actor, Actor *source, EffectPool *pool, s32 entry);
s32 func_801E36BC(Actor *actor, EffectPool *pool, s32 ticks, s32 arg3, s32 arg4);
void func_801E37D0(Actor *actor);
void func_801E39F0(Actor *actor, EffectPool *pool, s32 arg2, s32 arg3, s32 arg4);
void func_801E5C74(Actor *actor, Animation *anim, s32 loop);
s32 func_801E5CD8(Actor *actor, s32 source);
void func_801E5D44(Actor *actor, EffectPool *pool, s32 arg2);
void func_801E632C(Actor *actor);
void func_801E63A8(Actor *actor);
s16 func_801E66BC(VECTOR *dir, void *a, void *b, s32 divisor);
s32 func_801E67F8(void);
s32 func_801E6830(Actor *actor, u8 ref, u16 *mask);
s32 func_801E6910(Actor *actor, u8 ref, s32 *flag);
void func_801E6974(Actor *actor, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag,
                   u8 smooth, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, s16 duration);
void func_801E6D94(Actor *actor, ModelPart *part, s32 flags);
void func_801E6F64(Task *node);
void func_801E7094(Actor *actor, ModelPart *part, u8 flags, s16 x, s16 y, s16 z);
void func_801E7298(Actor *actor);
void func_801E8394(Actor *source, u16 index, u16 mask, s32 arg3);
s32 func_801E8480(s32 index);
void func_801E8510(Actor *actor);

/* The draw entry, which each target declares itself (ovl2143/actors.h says
 * why). */
void func_801E7D14(MATRIX *m, MATRIX *light, u32 *ot, s32 buffer, s32 elapsed);

#endif
