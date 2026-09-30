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
    s16 x;
    s16 y;
    u8 unk4[2];
    u8 width;
    u8 height;
    u8 unk8[0x10];
} SpriteCell;

/* An actor's sprite drawing state; only the member the module reads. */
typedef struct {
    u8 unk0[0x30];
    SpriteCell *cells; /* +30 */
} SpriteDraw;

/* A battle actor; only the members the module uses. Positions are 16.16
 * fixed point. */
typedef struct {
    s32 position[3];    /* +00 */
    s32 velocity[3];    /* +0c */
    u8 unk18[8];
    SpriteDraw *sprite; /* +20 */
    u8 unk24[0x1C];
    u8 cell_bytes;      /* +40: four per cell */
    u8 unk41[0x2B];
    void *task;         /* +6c: owner of the actor's effect tasks */
    u8 unk70[0x3C];
    u32 flags;          /* +ac: 0x20 while an effect holds the actor */
} Actor;

typedef struct {
    s16 y0, x0, y1, x1;
} Bounds;

/* The effect task: it takes over the actor's movement. */
typedef struct {
    TaskNode task;  /* +00 */
    TaskNode draw;  /* +1c */
    s32 moved[3];   /* +38: the movement the actor did not make */
    u32 unk44;
    Actor *actor;   /* +48 */
} HoldTask;

void func_801FC0EC(TaskNode *node);
void func_801FC508(TaskNode *node);

#endif
