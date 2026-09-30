#ifndef OVL3386_SCROLL_H
#define OVL3386_SCROLL_H

#include "common.h"

/* libgte matrix. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

void TransMatrix(MATRIX *m, s32 *v);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);

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
    s16 x;
    s16 y;
    u8 unk4[2];
    u8 width;
    u8 height;
    u8 unk8[0x10];
} SpriteCell;

/* An actor's sprite drawing state. */
typedef struct {
    u8 unk0[0xC];
    MATRIX matrix;     /* +0c */
    u8 unk2C[4];
    SpriteCell *cells; /* +30 */
} SpriteDraw;

/* A battle actor; only the members the module uses. Positions are 16.16
 * fixed point. */
typedef struct {
    s32 position[3]; /* +00 */
    s32 velocity[3]; /* +0c */
    s32 speed;       /* +18: the scroll's speed */
    u8 unk1C[4];
    SpriteDraw *sprite; /* +20 */
    u8 unk24[0x1C];
    u8 cell_bytes;   /* +40: four per cell */
    u8 unk41[0x2B];
    void *task;      /* +6c: owner of the actor's effect tasks */
    u8 unk70[0x3C];
    u32 flags;       /* +ac: 0x20 while an effect holds the actor */
} Actor;

typedef struct {
    s16 y0, x0, y1, x1;
} Bounds;

/* The effect task: the actor's sprite repeated in a scrolling row. */
typedef struct {
    TaskNode task;    /* +00 */
    TaskNode draw;    /* +1c */
    s32 scroll;       /* +38: 16.16, the row's offset */
    u32 unk3C[3];
    s32 position[3];  /* +48: where the actor is kept */
    u32 unk54;
    Actor *actor;     /* +58 */
} ScrollTask;

void func_801FC0EC(TaskNode *node);
void func_801FC5C4(TaskNode *node);

#endif
