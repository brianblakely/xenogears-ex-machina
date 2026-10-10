#ifndef XEM_FIBER_H
#define XEM_FIBER_H

/*
 * The port's fibers (port/fiber.c, docs/runtime.md "Suspension"): the game
 * and the arena task, each with its own asyncify save area and shadow stack.
 * The original switches MIPS stacks (arena_task_resume.s, arena_task_yield.s);
 * the port suspends one fiber into the host and continues the other.
 */

/* Forget the task fiber: the host restarted the game on an empty stack. */
void xem_fiber_reset(void);

/* On the game fiber: run the task fiber with the TaskContext at `task` until
 * it yields, starting it when `task` is not the context it runs. */
void xem_fiber_resume_task(unsigned int task);

/* On the task fiber: suspend it and continue the game fiber. */
void xem_fiber_yield_task(void);

#endif
