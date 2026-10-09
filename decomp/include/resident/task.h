#ifndef RESIDENT_TASK_H
#define RESIDENT_TASK_H

#include "common.h"

/* The resident's task lists (the first sprite unit, 8001c8dc-8001d2b0): the
 * nodes that run the sprites and other actors through their update and
 * destroy callbacks. The main list runs first each frame (unless paused),
 * then the second list. */

/* A task's link word: its owner's serial and its state flags. */
typedef union {
    u32 word;
    struct {
        unsigned owner_serial : 29;  /* the owner's creation number */
        unsigned flag29 : 1;
        unsigned flag30 : 1;
        unsigned active : 1;
    } bits;
} TaskLink;

/* A task node of the sprite engine's lists. */
typedef struct Task {
    struct Task *owner;
    void *data;                          /* +0x4: the task's sprite */
    void (*update)(struct Task *task);  /* +0x8 */
    void (*destroy)(struct Task *task); /* +0xc */
    union {
        u32 word;
        struct {
            unsigned serial : 29;        /* creation number */
            unsigned flags : 3;
        } bits;
    } id;                                /* +0x10 */
    TaskLink link;                       /* +0x14: flag tests read the word */
    struct Task *next;                   /* +0x18 */
} Task;

/* Task lists: the main list runs first each frame, then the second list. */
extern Task *D_8005958C;   /* main task list */
extern Task *D_80059594;   /* second task list */
extern Task *D_80059590;   /* the next task of the running pass */
extern Task *D_800594C0;   /* the running task */
extern u32 D_80059184;     /* next task serial */
extern s32 D_80059188;
extern s32 D_8005918C;     /* live tasks */

/* Small globals of other units (sprite_settings.c, the commons): the sprite
 * units address them absolutely. */
extern u8 D_800591AF;  /* allocation mode for sprite tasks */
extern s32 D_80059428; /* frames the main task list stays paused */
extern s16 D_80059494; /* cleared when the pause ends */
extern u8 D_800591AC;  /* new main-list tasks count as active */
extern s32 D_80059464; /* active main-list tasks */

void func_8001C8DC(void); /* destroy every task of both lists */
void func_8001C944(void); /* empty both lists */
void func_8001C964(void); /* run the main list, unless paused */
void func_8001C9F8(void); /* run the second list */
void func_8001CA58(Task *owner, Task *node); /* link `node` on the second list */
void func_8001CB48(Task *task);              /* unlink from the second list */
void func_8001CBE8(Task *task); /* destroy callback: unlink and free */
void func_8001CC18(Task *owner, Task *node); /* link `node` on the main list */
Task *func_8001CD08(Task *owner, s32 size); /* allocate a main-list task */
void func_8001CD64(Task *task, void (*update)(Task *));
void func_8001CD6C(Task *task, void (*update)(Task *));
void func_8001CD74(Task *task, void (*destroy)(Task *));
void *func_8001CD7C(Task *task); /* the update callback */
void func_8001CD94(Task *task);  /* unlink from the main list */
void func_8001CE44(Task *task);  /* destroy callback: unlink and free */
void func_8001CE74(Task *owner); /* destroy every task `owner` created */
void func_8001D034(Task *owner);
Task *func_8001D0A4(Task *owner, void (*update)(Task *)); /* find by owner and update */
Task *func_8001D164(void (*update)(Task *));              /* find by update */
void func_8001D19C(Task *task); /* destroy callback of a two-node task */
/* Allocate a task that starts with two nodes, one on each list. */
Task *func_8001D1D8(s32 size, Task *owner, void (*update)(Task *), void (*update2)(Task *), void (*destroy)(Task *));

#endif
