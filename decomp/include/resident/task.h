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
extern Task *task_main_list;           /* main task list */
extern Task *task_draw_list;           /* second task list */
extern Task *task_next_node;           /* the next task of the running pass */
extern Task *task_unread_current_node; /* the running task (never read) */
extern u32 task_next_serial;           /* next task serial */
extern s32 task_main_count;
extern s32 task_draw_count;     /* live tasks */

/* Small globals of other units (sprite_settings.c, the commons): the sprite
 * units address them absolutely. */
extern u8 task_alloc_mode;            /* allocation mode for sprite tasks */
extern s32 task_main_pause_timer;     /* frames the main task list stays paused */
extern s16 task_catch_up_frame_count; /* cleared when the pause ends */
extern u8 task_new_tasks_active;      /* new main-list tasks count as active */
extern s32 task_active_main_count;    /* active main-list tasks */

void task_destroy_all(void);                                            /* destroy every task of both lists */
void task_clear_lists(void);                                            /* empty both lists */
void task_run_main_list(void);                                          /* run the main list, unless paused */
void task_run_draw_list(void);                                          /* run the second list */
void task_link_draw_node(Task *owner, Task *node);                      /* link `node` on the second list */
void task_unlink_draw_node(Task *task);                                 /* unlink from the second list */
void task_destroy_draw_task(Task *task);                                /* destroy callback: unlink and free */
void task_link_main_node(Task *owner, Task *node);                      /* link `node` on the main list */
Task *task_alloc_main_task(Task *owner, s32 size);                      /* allocate a main-list task */
void task_set_draw_callback(Task *task, void (*update)(Task *));
void task_set_update_callback(Task *task, void (*update)(Task *));
void task_set_destroy_callback(Task *task, void (*destroy)(Task *));
void *task_get_update_callback(Task *task);                             /* the update callback */
void task_unlink_main_node(Task *task);                                 /* unlink from the main list */
void task_destroy_main_task(Task *task);                                /* destroy callback: unlink and free */
void task_destroy_owned_by(Task *owner);                                /* destroy every task `owner` created */
void task_clear_child_sprite_parents(Task *owner);
Task *task_find_owned_with_update(Task *owner, void (*update)(Task *)); /* find by owner and update */
Task *task_find_by_update(void (*update)(Task *));                      /* find by update */
void task_destroy_two_node_task(Task *task);                            /* destroy callback of a two-node task */
/* Allocate a task that starts with two nodes, one on each list. */
Task *task_alloc_two_node_task(s32 size, Task *owner, void (*update)(Task *), void (*update2)(Task *), void (*destroy)(Task *));

#endif
