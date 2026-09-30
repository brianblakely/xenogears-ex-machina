#ifndef OVL3387_BURST_H
#define OVL3387_BURST_H

#include "common.h"

/* Resident task system: a task node (update) followed by its drawing node;
 * both callbacks receive their node, whose +4 names the node's object. */
typedef struct TaskNode {
    u32 unk0;
    void *object;
    void (*update)(struct TaskNode *node);
    void (*destroy)(struct TaskNode *node);
    u32 unk10;
    u32 unk14;
    struct TaskNode *next;
} TaskNode;

void func_8001CB48(TaskNode *node);        /* unlink a drawing node */
void func_8001CD94(TaskNode *node);        /* unlink a task */
void func_80025180(void *block);           /* release a block after the frame */
void *func_80031BDC(s32 size, s32 mode);   /* allocate a heap block */
void func_800320E8(void *block);           /* release a heap block */
s32 DrawSync(s32 mode);

/* The effect's state (0x10fa4 bytes): laid out as a task node pair, but run
 * directly by func_801FC8F4's own frame loop. */
typedef struct {
    TaskNode task;    /* +00 */
    TaskNode draw;    /* +1c */
    s32 brightness;   /* +38: 0x80 neutral, fades out at the end */
    s32 angle;        /* +3c */
    s32 twist;        /* +40 */
    s32 frame;        /* +44 */
    s32 speed;        /* +48 */
    u8 unk4C[8];
    s32 height;       /* +54 */
    u8 unk58[8];
    u16 size;         /* +60 */
    u8 unk62[0x10FA4 - 0x62];
} Burst;

extern u8 D_801FCE14; /* the effect's variant (1 in the module's data) */

Burst *func_801FC4A8(Burst *burst);
void func_801FC8F4(void);

#endif
