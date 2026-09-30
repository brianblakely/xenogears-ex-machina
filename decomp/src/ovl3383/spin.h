#ifndef OVL3383_SPIN_H
#define OVL3383_SPIN_H

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

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

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
s32 RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s32 *sxy0, s32 *sxy1,
                  s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag);
void AddPrim(void *ot, void *p);
s32 func_8003F8B0(s32 angle);         /* sine (4096 = 1.0) */
void func_80022038(void *actor);      /* refresh the actor's sprite matrix */
void func_8001E148(void *actor);      /* place the actor's sprite in view */

extern MATRIX D_8004FBB8;   /* sprite camera */
extern SVECTOR D_8004FB98[4]; /* sprite quad corners */
extern s32 D_80050100;      /* model depth shift */
extern u32 *D_8005956C;     /* current ordering table */
extern u8 *D_80059534;      /* primitive buffer end */
extern u8 *D_80059580;      /* next free primitive */

/* One sprite part: its rectangle in the sprite frame and its texture. */
typedef struct {
    s16 x, y;           /* +00 */
    u8 u, v;            /* +04 */
    u8 w, h;            /* +06 */
    u8 unk8[2];
    u16 tpage;          /* +0a */
    u16 clut;           /* +0c */
    u8 unkE[2];
    u32 color;          /* +10 */
} SpritePart;

/* An actor's sprite renderer. */
typedef struct {
    u8 unk0[0xC];
    MATRIX matrix;      /* +0c */
    u8 unk2C[4];
    SpritePart *parts;  /* +30 */
} SpriteRenderer;

/* A 16.16 fixed-point coordinate. */
typedef union {
    s32 raw;
    struct {
        u16 frac;
        s16 whole;
    } part;
} Fixed;

/* A battle actor. */
typedef struct Actor {
    Fixed pos[3];              /* +00 */
    u8 unkC[0x14];
    SpriteRenderer *renderer;  /* +20 */
    u8 unk24[0xA];
    s16 depth;                 /* +2e: ordering-table slot this frame */
    s16 depth_bias;            /* +30 */
    u8 unk32[0xA];
    u32 flags;                 /* +3c: bit 24 own matrix, 25 in front, 29 parent's depth */
    u32 flags40;               /* +40: bits 8..12 sprite scale shift */
    u8 unk44[0x28];
    void *task;                /* +6c: owner of the actor's effect tasks */
    struct Actor *parent;      /* +70 */
} Actor;

/* The effect task func_801FC53C creates (0x54 bytes). */
typedef struct {
    TaskNode task;  /* +00 */
    TaskNode draw;  /* +1c */
    Actor *actor;   /* +38: the battle actor the effect circles */
    s32 radius;     /* +3c: horizontal swing of the rows */
    s32 arg3;       /* +40: added to the swing each row (8.8) */
    s32 arg4;       /* +44: angle step between rows (8.8) */
    s32 arg5;       /* +48: added to the angle step each row */
    s32 angle;      /* +4c: advanced by step every frame */
    s32 step;       /* +50 */
} SpinTask;

void func_801FC000(TaskNode *node);
void func_801FC020(TaskNode *node);

#endif
