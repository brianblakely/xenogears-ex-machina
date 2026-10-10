/*
 * A stand-in game module for the fiber test (tests/fiber_module/build.sh,
 * runtime/crates/xem-game/tests/fiber_module.rs): the port's real
 * port/fiber.c and port/arena_task.c, built straight to wasm32 and
 * instrumented by asyncify like build/game/game.wasm, under a game fiber and
 * an arena task that suspend in every way the arena does. Each records
 * address-taken locals (on its fiber's shadow stack) into a log in game
 * memory, which the test reads.
 */
#include "common.h"
#include "xem/memory.h"
#include "xem/port.h"

void arena_task_resume(void *task);
void arena_task_yield(void);
void xem_fiber_reset(void);

/* The scheduler's words (the game's at 80096D88, here the module's own). */
u32 arena_task_caller_stack;
u32 arena_current_task;

#define TASK 0x80100000u /* a TaskContext */
#define LOG 0x80100200u  /* count, then the noted words */

static void note(u32 value) {
    u32 count = XEM_U32(LOG);

    XEM_U32(LOG + 4 + count * 4) = value;
    XEM_U32(LOG) = count + 1;
}

static void task_entry(s32 arg) {
    volatile u32 local = 0x7000 + arg;

    note(local);
    xem_host_yield(XEM_YIELD_POLL); /* a wait inside the task */
    note(local + 1);
    arena_task_yield();
    note(local + 2);
    for (;;) {
        arena_task_yield();
        note(0x7100);
    }
}

void xem_run(s32 kind, s32 arg) {
    volatile u32 local = 0x1000 + kind;
    s32 i;

    xem_fiber_reset();
    for (i = 0; i < 32; i++) {
        XEM_U32(TASK + i * 4) = 0;
    }
    XEM_U32(TASK + 31 * 4) = (u32)(unsigned long)task_entry;
    XEM_U32(TASK + 4 * 4) = 5;
    note(local);
    arena_task_resume((void *)TASK);
    note(local + 1);
    xem_host_yield(XEM_YIELD_VSYNC);
    note(local + 2);
    arena_task_resume((void *)TASK);
    note(local + 3);
    xem_host_restart(XEM_RESTART_DISPATCH, 9);
}

void xem_call(u32 address) {
}

void xem_interrupt(u32 irq, u32 detail) {
    note(0x9000 + irq);
}
