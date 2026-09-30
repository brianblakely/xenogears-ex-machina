#ifndef OVL3384_DEBRIS_H
#define OVL3384_DEBRIS_H

#include "common.h"

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *TransMatrix(MATRIX *m, s32 *v);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
s32 RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, s32 *sxy0, s32 *sxy1, s32 *sxy2,
                  s32 *p, s32 *flag);
s32 RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s32 *sxy0, s32 *sxy1,
                  s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag);
void AddPrim(void *ot, void *p);
MATRIX *func_8003F738(SVECTOR *rotation, MATRIX *m); /* RotMatrix */

extern MATRIX D_8004FBB8; /* sprite camera */
extern void *D_8005956C;  /* current ordering table */

/* Battle overlay work area (800c3eb0); only the member the module reads. */
typedef struct {
    u8 unk0[0x8C84];
    s32 buffer; /* +8c84: double-buffer index being drawn */
} BattleWork;
extern BattleWork D_800C3EB0;

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

void func_8001CB48(TaskNode *node); /* unlink a drawing node */
void func_8001CD94(TaskNode *node); /* unlink a task */
void func_80025180(void *block);    /* release a block after the frame */
void func_800320E8(void *block);    /* release a heap block */

/* A model's packet descriptor: the words of its primitive and of itself,
 * and the primitive kind (bits 2..4: 4 textured, 8 quad, 16 gouraud). */
typedef struct {
    u8 prim_words;
    u8 words;
    u8 unk2;
    u8 kind;
} PacketDesc;

/* A model; only the member the module reads. */
typedef struct {
    u8 unk0[0x10];
    s32 packets; /* +10: offset of the packet descriptors */
} Model;

/* One flying piece of the broken model (0x54 bytes). */
typedef struct {
    SVECTOR rotation;  /* +00 */
    SVECTOR spin;      /* +08: added to the rotation every frame */
    s32 position[3];   /* +10: 16.16 fixed point */
    u32 unk1C;
    s32 velocity[3];   /* +20 */
    u32 unk2C;
    s32 gravity;       /* +30: added to the vertical velocity */
    SVECTOR vertex[4]; /* +34 */
} Piece;

/* The effect task (0x74 bytes): a model broken into flying pieces. */
typedef struct {
    TaskNode task;      /* +00 */
    TaskNode draw;      /* +1c */
    MATRIX matrix;      /* +38: the model's placement */
    s32 count;          /* +58: pieces */
    s32 life;           /* +5c: frames left */
    u32 unk60;
    void *prims[2];     /* +64: the pieces' primitives, per display buffer */
    Model *model;       /* +6c */
    Piece *pieces;      /* +70 */
} DebrisTask;

#endif
