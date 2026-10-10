/*
 * The game's two fibers (docs/runtime.md, Suspension). The arena runs its mode
 * task as a coroutine on its own MIPS stack (arena_task_resume.s,
 * arena_task_yield.s); the port cannot switch stacks, so each fiber is a wasm
 * call stack that asyncify unwinds into the fiber's own save area while the
 * other runs. Fiber 0 is the game (bottom frame xem_run), fiber 1 the arena
 * task (bottom frame xem_task_run). The host keeps each fiber's shadow stack
 * pointer and decides which one to rewind; the port says which fiber unwinds
 * (xem_unwind_area) and when the task fiber starts afresh.
 */
#include "common.h"
#include "xem/fiber.h"
#include "xem/port.h"
#include "xem/memory.h"

/* The fiber running, or the one the host last suspended. */
static u32 xem_fiber;

/* The TaskContext (a game address) the task fiber runs, 0 when none. */
static u32 xem_fiber_task;

/* Asyncify's save area of each fiber: {current, end} and the unwound frames. */
#define XEM_UNWIND_WORDS 0x10000
static u32 xem_unwind_words[2][2 + XEM_UNWIND_WORDS];

/* The task fiber's shadow stack (the wasm locals whose address is taken; the
 * game fiber's is the module's own). The task's PS1 stack at 0x801FE000 stays
 * in game memory untouched, as the port's C keeps no MIPS frames. */
static u8 xem_task_stack[0x40000] __attribute__((aligned(16)));

void xem_fiber_reset(void) {
    xem_fiber = XEM_FIBER_GAME;
    xem_fiber_task = 0;
}

/* The save area for the unwind that is starting: the running fiber's. */
u32 *xem_unwind_area(void) {
    u32 *words = xem_unwind_words[xem_fiber];

    words[0] = (u32)(unsigned long)&words[2];
    words[1] = (u32)(unsigned long)&words[2 + XEM_UNWIND_WORDS];
    return words;
}

void xem_fiber_resume_task(u32 task) {
    u32 stack_top = 0;

    if (task != xem_fiber_task) {
        /* A context the fiber does not run: start it afresh (any task
         * suspended there is abandoned; the game creates one task per
         * arena run, and the arena leaves through a restart). */
        xem_fiber_task = task;
        stack_top = (u32)(unsigned long)&xem_task_stack[sizeof(xem_task_stack)];
    }
    xem_host_task_switch(XEM_FIBER_TASK, stack_top);
    /* The task yielded and the host rewound this fiber. */
    xem_fiber = XEM_FIBER_GAME;
}

void xem_fiber_yield_task(void) {
    xem_host_task_switch(XEM_FIBER_GAME, 0);
    /* The game resumed the task and the host rewound this fiber. */
    xem_fiber = XEM_FIBER_TASK;
}

/* The task fiber's bottom frame, which the host calls to start the fiber and
 * to rewind it: run entry(arg) as arena_task_resume enters a new context,
 * through its saved ra (register 31) with a0 (register 4). The original's
 * entry never returns (arena_mode_task loops); if it did, the original would
 * re-enter it through ra, while here xem_task_run returns and the host stops
 * the game with a diagnostic. */
void xem_task_run(void) {
    u32 task = xem_fiber_task;

    xem_fiber = XEM_FIBER_TASK;
    ((void (*)(s32))(unsigned long)XEM_U32(task + 4 * 31))((s32)XEM_U32(task + 4 * 4));
}
