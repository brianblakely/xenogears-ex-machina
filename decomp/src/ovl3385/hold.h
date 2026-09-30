#ifndef OVL3385_HOLD_H
#define OVL3385_HOLD_H

#include "common.h"

/* Resident task system: a task node (update) followed by its drawing node;
 * both callbacks receive their node, whose +4 names the task's object. */
typedef struct TaskNode {
    u32 unk0;
    void *object;
    void (*update)(struct TaskNode *node);
    void (*destroy)(struct TaskNode *node);
    u32 unk10;
    u32 unk14;
    struct TaskNode *next;
} TaskNode;

void *func_8001D1D8(s32 size, void *owner, void (*update)(TaskNode *),
                    void (*draw)(TaskNode *), void (*destroy)(TaskNode *)); /* create a task */

/* One cell of a sprite frame (0x18 bytes). */
typedef struct {
    s16 x;       /* +00 */
    s16 y;       /* +02 */
    u8 u, v;     /* +04 */
    u8 width;    /* +06 */
    u8 height;   /* +07 */
    u8 unk8[2];
    u16 tpage;   /* +0a */
    u16 clut;    /* +0c */
    u8 unkE[2];
    u32 color;   /* +10 */
    u32 flags;   /* +14: bit 4 mirrored, bit 5 flipped */
} SpriteCell;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

/* A primitive's tag: the next primitive's address and this one's length. */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
} P_TAG;

#define addPrim(ot, p) \
    (((P_TAG *)(p))->addr = ((P_TAG *)(ot))->addr, ((P_TAG *)(ot))->addr = (u32)(p))

typedef struct {
    u32 tag;
    u32 color;          /* +04: r0, g0, b0, code */
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
s32 RotTransPers(SVECTOR *v0, s32 *sxy, s32 *p, s32 *flag);
s32 RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s32 *sxy0, s32 *sxy1,
                s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag);
void *memset(void *dst, s32 c, u32 n);
void func_80022038(void *actor); /* refresh the actor's sprite matrix */
void func_8001E148(void *actor); /* place the actor's sprite in view */

extern MATRIX D_8004FBB8;  /* sprite camera */
extern s32 D_80050100;     /* model depth shift */
extern u32 *D_8005956C;    /* current ordering table */
extern u8 *D_80059534;     /* primitive buffer end */
extern u8 *D_80059580;     /* next free primitive */

/* An actor's sprite drawing state. */
typedef struct {
    u8 unk0[0xC];
    MATRIX matrix;     /* +0c */
    u8 unk2C[4];
    SpriteCell *cells; /* +30 */
} SpriteDraw;

/* A battle actor; only the members the module uses. Positions are 16.16
 * fixed point. */
typedef struct Actor {
    s32 position[3];    /* +00 */
    s32 velocity[3];    /* +0c */
    u8 unk18[8];
    SpriteDraw *sprite; /* +20 */
    u8 unk24[0xA];
    s16 depth;          /* +2e: ordering-table slot this frame */
    s16 depth_bias;     /* +30 */
    u8 unk32[0xA];
    u32 draw_flags;     /* +3c: bit 24 own matrix, 25 in front, 29 parent's depth */
    u32 cell_bytes : 8; /* +40: four per cell */
    u32 shift : 5;      /* sprite scale shift */
    u32 unk40 : 19;
    u8 unk44[0x28];
    void *task;         /* +6c: owner of the actor's effect tasks */
    struct Actor *parent; /* +70 */
    u8 unk74[0x38];
    u32 flags;          /* +ac: 0x20 while an effect holds the actor */
} Actor;

typedef struct {
    s16 y0, x0, y1, x1;
} Bounds;

/* The effect task: it takes over the actor's movement (16.16; the drawing
 * reads the whole parts). */
typedef struct {
    TaskNode task;  /* +00 */
    TaskNode draw;  /* +1c */
    s32 moved[3];   /* +38: the movement the actor did not make */
    u32 unk44;
    Actor *actor;   /* +48 */
} HoldTask;

s32 func_801FC000(struct Actor *actor, s32 *width, s32 *height, Bounds *bounds);
void func_801FC0EC(TaskNode *node);
void func_801FC168(SpriteCell *cell, s32 count, s32 dx, s32 dy, s32 depth, struct Actor *actor);
void func_801FC508(TaskNode *node);

#endif
