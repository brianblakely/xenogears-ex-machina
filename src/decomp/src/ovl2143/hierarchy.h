#ifndef OVL2143_HIERARCHY_H
#define OVL2143_HIERARCHY_H

/* Model hierarchies (battle/model.h): the model tables of relocated model
 * groups (the resident's sprite models), the 0x7c-byte parts built from them
 * and their packets, the tweens of an effect pool that move them, and the
 * keyframe tracks the tweens read. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "battle/effect.h"
#include "battle/model.h"

/* Scratchpad matrix used as a temporary. */
#define SCRATCH_MATRIX ((MATRIX *)0x1F800000)

/* Callers convert arguments differently from the resident definition (a
 * ModelBuffer and u8 ** buffers there; s16 where it takes u16): allocate a
 * model's two packet buffers, and override the models' texture pages and
 * CLUTs with those at (x, y). */
void model_alloc_packet_buffers(SpriteModel *buffer, void **first, void **second);
void model_set_tpage_override(s16 x, s16 y);
void model_set_clut_override(s16 x, s16 y);

ModelTable *gear_model_build_model_list(u8 *group, ModelTable *list);
ModelPart *gear_model_build_hierarchy(ModelTable *list, u16 *hierarchy, s32 mode, s32 offset, s16 x0, s16 y0,
                         s16 x1, s16 y1);
u32 gear_model_compose_hierarchy(ModelPart *parts, s32 scale);
u32 gear_model_compose_scaled_hierarchy(ModelPart *parts, s32 scale);
void gear_model_free_hierarchy(ModelPart *parts);
void gear_model_free_model_list(ModelTable *list, s32 release_models);
s32 gear_model_step_tweens(EffectPool *pool, ModelPart *parts, s32 tag, s32 scale);
u16 gear_model_apply_keyframe(ModelPart *root, s16 *data);
u16 gear_model_tween_to_keyframe(EffectPool *pool, ModelPart *part, s16 *data, s32 duration, s32 mode, s32 smooth,
                  s32 tag);
void gear_model_release_node_tweens(EffectPool *pool, ModelPart *part, s32 index, s32 mask);
EffectPool *gear_model_alloc_tween_pool(EffectPool *pool, s32 capacity);
void gear_model_free_tween_pool(EffectPool *pool);
void gear_model_clear_tween_pool(EffectPool *pool);
EffectEntry *gear_model_take_tween_slot(EffectPool *pool);
s32 gear_model_free_tween_slot(EffectPool *pool, EffectEntry *slot);
s32 gear_model_start_keyframe_tracks(EffectPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag);
void gear_model_release_unkept_tweens(EffectPool *pool, ModelPart *parts);
void gear_model_release_tagged_tweens(EffectPool *pool, ModelPart *parts, u8 tag);
void gear_model_turn_node_to(EffectPool *pool, ModelPart *part, s32 duration, s32 rx, s32 ry, s32 rz);
void gear_model_start_homing_turn(EffectPool *pool, ModelPart *part, s32 type, s32 limit, s32 gain, s32 duration,
                   s32 x, s32 y, s32 z);
void gear_model_transfer_subtree(EffectPool *pool, s32 index, ModelPart *parts, ModelPart *other);
void gear_model_transfer_visible_nodes(ModelPart *parts, ModelPart *other);

#endif
