#ifndef MENU_TASK_H
#define MENU_TASK_H

#include "common.h"

/* Cooperative tasks (menu7 8008BA2C-8008BCC8): a task runs on its own
 * stack until it yields back to the frame loop (handwritten context
 * switches, func_8008BB00.s and its kin). */

/* Cooperative task: registers saved by number, then its stack. */
typedef struct {
    u32 regs[32];      /* 2 v0 .. 31 ra; 28 gp, 29 sp, 30 fp */
    u32 *stack;        /* 0x80 */
} TaskContext;

/* The caller state pair saved/restored around a nested task scheduler. */
typedef struct {
    u32 *caller_stack;
    TaskContext *task;
} TaskCallerContext;

TaskContext *func_8008BA2C(void (*entry)(s32), s32 arg, u32 *stack, s32 words); /* create */
void func_8008BB00(TaskCallerContext *context); /* save the scheduler state */
void func_8008BB1C(TaskCallerContext *context); /* restore it */
void func_8008BB3C(TaskContext *task); /* run the task until it yields */
void func_8008BC04(void);              /* yield the current task */

#endif
