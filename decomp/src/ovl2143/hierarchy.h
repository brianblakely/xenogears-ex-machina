#ifndef OVL2143_HIERARCHY_H
#define OVL2143_HIERARCHY_H

/* Model hierarchies: the model records of a relocated model group (the
 * resident's sprite models), the 0x7c-byte nodes built from them and their
 * packets, the tweens of a 0x14-byte slot pool that move them, and the
 * keyframe tracks the tweens read. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "battle/effect.h"

/* Scratchpad matrix used as a temporary. */
#define SCRATCH_MATRIX ((MATRIX *)0x1F800000)

/* Callers convert arguments differently from the resident definition (a
 * ModelBuffer and u8 ** buffers there; s16 where it takes u16): allocate a
 * model's two packet buffers, and override the models' texture pages and
 * CLUTs with those at (x, y). */
void func_8002CB54(SpriteModel *model, void **packets0, void **packets1);
void func_8002CC10(s16 x, s16 y);
void func_8002CC74(s16 x, s16 y);

/* The model records of a relocated group (they follow its 0x10-byte header). */
typedef struct {
    SpriteModel **models;
    u32 count;
} GroupModels;

/* A hierarchy entry: a model index (ffff: none) and its parent entry. */
typedef struct {
    u16 model;
    u16 parent;
} HierarchyLink;

/* One node of a model hierarchy (0x7c bytes); element 0 is the root and holds
 * the node count. */
typedef struct ModelPart {
    struct ModelPart *parent;
    u8 dirty;       /* +4: world matrix must be recomposed */
    u8 rotate;      /* +5: local matrix must be rebuilt from rot */
    u8 yxz;         /* +6: RotMatrixYXZ instead of RotMatrix */
    u8 visible;     /* +7 */
    u16 model;      /* +8: model index, ffff none */
    u16 count;      /* +a: root: node count; else node index */
    MATRIX local;   /* +c */
    MATRIX world;   /* +2c */
    s16 scale[3];   /* +4c */
    s16 billboard;  /* +52: 1 upright, 2 fully facing the view */
    SVECTOR rot;    /* +54 */
    s32 pos[3];     /* +5c */
    void *packets[2]; /* +68: the model's packets for both buffers */
    struct PoolSlot *attachments[3]; /* +70: pool slots attached to the node */
} ModelPart;

/* A pool slot (0x14 bytes): a tween attached to a model node. */
typedef struct PoolSlot {
    u8 used;
    u8 flag;
    u8 kind;        /* 3: rotation, 7 + n: movement */
    u8 tag;         /* 0xff: kept by func_801DFE8C */
    union {
        s16 value[6];   /* +4: start values and deltas / targets */
        struct {
            u8 *start;  /* +4: keyframe track data */
            u8 *pos;    /* +8 */
        } track;
    } u;
    s16 time;       /* +10 */
    s16 duration;   /* +12 */
} PoolSlot;

/* A pool of slots with the position where the search for a free one starts. */
typedef struct {
    PoolSlot *slots;
    u16 next;
    u16 capacity;
} SlotPool;

GroupModels *func_801DC22C(u8 *group, GroupModels *list);
ModelPart *func_801DC2D0(GroupModels *group, HierarchyLink *links, s32 mode, s32 configure,
                         s16 param0, s16 param1, s16 param2, s16 param3);
u32 func_801DC5C0(ModelPart *parts, s32 scale);
u32 func_801DC848(ModelPart *parts, s32 scale);
void func_801DCD8C(ModelPart *parts);
void func_801DCE18(GroupModels *list, s32 release_models);
s32 func_801DDBF8(SlotPool *pool, ModelPart *parts, s32 tag, s32 scale);
u16 func_801DEF10(ModelPart *parts, s16 *data);
u16 func_801DF0B4(SlotPool *pool, ModelPart *parts, s16 *data, s32 duration, s32 mode, s32 smooth,
                  s32 tag);
void func_801DF52C(SlotPool *pool, ModelPart *part, s32 index, s32 mask);
SlotPool *func_801DF5F4(SlotPool *pool, s32 capacity);
void func_801DF668(SlotPool *pool);
void func_801DF6A8(SlotPool *pool);
PoolSlot *func_801DF6F0(SlotPool *pool);
s32 func_801DF7A8(SlotPool *pool, PoolSlot *slot);
s32 func_801DF7F4(SlotPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag);
void func_801DFE8C(SlotPool *pool, ModelPart *parts);
void func_801DFF78(SlotPool *pool, ModelPart *parts, u8 tag);
void func_801E59D4(SlotPool *pool, ModelPart *part, s32 duration, s32 rx, s32 ry, s32 rz);
void func_801E5B50(SlotPool *pool, ModelPart *part, s32 type, s32 arg3, s32 arg4, s32 duration,
                   s32 x, s32 y, s32 z);
void func_801E6578(SlotPool *pool, s32 index, ModelPart *parts, ModelPart *other);
void func_801E6668(ModelPart *parts, ModelPart *other);

#endif
