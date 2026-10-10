/*
 * The arena task's context switch (decomp/src/menu/arena_task_*.s,
 * TaskContext in decomp/src/menu/task.h) over the port's fibers
 * (port/fiber.c). The original saves the caller's registers below its stack
 * pointer, loads the task's register bank and jumps to its ra; a yield stores
 * the bank back and returns through the caller's save area. Here the task is
 * a fiber the host suspends and continues, so no register bank exists:
 * arena_task_yield leaves the TaskContext untouched, the task's MIPS stack at
 * 0x801FE000 is never written, and resuming enters entry(arg) (TaskContext
 * ra and a0) only for a context the task fiber does not run yet.
 * The scheduler's two words keep their original addresses and the values the
 * game can see: arena_current_task is the context last resumed and stays set
 * after a yield; arena_task_caller_stack, the suspended caller's sp in the
 * original, is the address of the resuming frame on the game fiber's shadow
 * stack (nothing but the unreferenced save and restore reads it).
 */
#include "common.h"
#include "xem/fiber.h"
#include "xem/memory.h"

/* 80096D88 and 80096D8C (arena_task_save_scheduler.s), as game words. */
extern u32 arena_task_caller_stack;
extern u32 arena_current_task;

/* Save the scheduler's two words to context (caller sp, then task). */
void arena_task_save_scheduler(void *context) {
    u32 at = (u32)(unsigned long)context;

    XEM_U32(at) = arena_task_caller_stack;
    XEM_U32(at + 4) = arena_current_task;
}

/* Restore the two words arena_task_save_scheduler saved. */
void arena_task_restore_scheduler(void *context) {
    u32 at = (u32)(unsigned long)context;

    arena_task_caller_stack = XEM_U32(at);
    arena_current_task = XEM_U32(at + 4);
}

/* Run the task until it yields through arena_task_yield. */
void arena_task_resume(void *task) {
    volatile u32 frame = 0;

    arena_task_caller_stack = (u32)(unsigned long)&frame;
    arena_current_task = (u32)(unsigned long)task;
    xem_fiber_resume_task((u32)(unsigned long)task);
}

/* Suspend the current task and return from the arena_task_resume that ran it. */
void arena_task_yield(void) {
    xem_fiber_yield_task();
}
