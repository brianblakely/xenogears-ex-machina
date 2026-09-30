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

/* A battle actor; only the member the module reads here. */
typedef struct {
    u8 unk0[0x6C];
    void *task; /* +6c: owner of the actor's effect tasks */
} Actor;

/* The effect task func_801FC53C creates (0x54 bytes). */
typedef struct {
    TaskNode task;  /* +00 */
    TaskNode draw;  /* +1c */
    Actor *actor;   /* +38: the battle actor the effect circles */
    s32 radius;     /* +3c */
    s32 arg3;       /* +40 */
    s32 arg4;       /* +44 */
    s32 arg5;       /* +48 */
    s32 angle;      /* +4c: advanced by step every frame */
    s32 step;       /* +50 */
} SpinTask;

void func_801FC000(TaskNode *node);
void func_801FC020(TaskNode *node);

#endif
