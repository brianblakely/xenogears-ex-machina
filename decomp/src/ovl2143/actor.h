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
void model_load_image_list(void *images, s32 mode, s32 x, s32 y, s32 mode2, s32 x2, s32 y2);

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

void gear_model_draw_actor(Actor *actor, MATRIX *m, MATRIX *light, s32 mode, s32 ticks, u32 *ot, s32 buffer);
void gear_model_place_anchors(Actor **actors);
void gear_model_reset_actor_script(Actor *actor, EffectPool *pool, s32 *entries, s32 *locals);
void gear_model_call_entry(Actor *actor, Actor *source, EffectPool *pool, s32 entry);
s32 gear_model_step_actor(Actor *actor, EffectPool *pool, s32 ticks, s32 unused_buffer, s32 unused_substeps);
void gear_model_carry_actor(Actor *actor);
void gear_model_run_effect_script(Actor *actor, EffectPool *pool, s32 changed, s32 ticks, s32 unused_substeps);
void gear_model_start_animation(Actor *actor, Animation *anim, s32 loop);
s32 gear_model_get_sound_bank_base(Actor *actor, s32 source);
void gear_model_run_animation_events(Actor *actor, EffectPool *pool, s32 unused_buffer);
void gear_model_stop_animation(Actor *actor);
void gear_model_update_aim_target(Actor *actor);
s16 gear_model_project_on_cross_axis(VECTOR *dir, void *a, void *b, s32 divisor);
s32 gear_model_find_lowest_masked_actor(void);
s32 gear_model_resolve_actor_reference(Actor *actor, u8 ref, u16 *mask);
s32 gear_model_get_actor_animation(Actor *actor, u8 ref, s32 *flag);
void gear_model_start_node_tween(Actor *actor, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag,
                   u8 smooth, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, s16 duration);
void gear_model_show_node(Actor *actor, ModelPart *part, s32 flags);
void gear_model_update_linked_sprite(Task *node);
void gear_model_set_node_transform(Actor *actor, ModelPart *part, u8 flags, s16 x, s16 y, s16 z);
void gear_model_apply_root_height(Actor *actor);
void gear_model_select_and_call_source_entry(Actor *source, u16 index, u16 mask, s32 entry);
s32 gear_model_get_actor_width(s32 index);
void gear_model_alloc_channels(Actor *actor);

/* The draw entry, which each target declares itself (ovl2143/actors.h says
 * why). */
void gear_model_step_and_draw(MATRIX *m, MATRIX *light, u32 *ot, s32 buffer, s32 elapsed);

#endif
