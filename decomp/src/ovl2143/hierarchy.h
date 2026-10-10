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
void model_alloc_packet_buffers(SpriteModel *model, void **packets0, void **packets1);
void model_set_tpage_override(s16 x, s16 y);
void model_set_clut_override(s16 x, s16 y);

ModelTable *func_801DC22C(u8 *group, ModelTable *list);
ModelPart *func_801DC2D0(ModelTable *list, u16 *hierarchy, s32 mode, s32 offset, s16 x0, s16 y0,
                         s16 x1, s16 y1);
u32 func_801DC5C0(ModelPart *parts, s32 scale);
u32 func_801DC848(ModelPart *parts, s32 scale);
void func_801DCD8C(ModelPart *parts);
void func_801DCE18(ModelTable *list, s32 release_models);
s32 func_801DDBF8(EffectPool *pool, ModelPart *parts, s32 tag, s32 scale);
u16 func_801DEF10(ModelPart *root, s16 *data);
u16 func_801DF0B4(EffectPool *pool, ModelPart *part, s16 *data, s32 duration, s32 mode, s32 smooth,
                  s32 tag);
void func_801DF52C(EffectPool *pool, ModelPart *part, s32 index, s32 mask);
EffectPool *func_801DF5F4(EffectPool *pool, s32 capacity);
void func_801DF668(EffectPool *pool);
void func_801DF6A8(EffectPool *pool);
EffectEntry *func_801DF6F0(EffectPool *pool);
s32 func_801DF7A8(EffectPool *pool, EffectEntry *entry);
s32 func_801DF7F4(EffectPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag);
void func_801DFE8C(EffectPool *pool, ModelPart *parts);
void func_801DFF78(EffectPool *pool, ModelPart *parts, u8 tag);
void func_801E59D4(EffectPool *pool, ModelPart *part, s32 duration, s32 rx, s32 ry, s32 rz);
void func_801E5B50(EffectPool *pool, ModelPart *part, s32 type, s32 arg3, s32 arg4, s32 duration,
                   s32 x, s32 y, s32 z);
void func_801E6578(EffectPool *pool, s32 index, ModelPart *parts, ModelPart *other);
void func_801E6668(ModelPart *parts, ModelPart *other);

#endif
