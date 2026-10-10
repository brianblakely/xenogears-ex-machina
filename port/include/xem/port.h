#ifndef XEM_PORT_H
#define XEM_PORT_H

/*
 * The game module's interface with its host (docs/runtime.md). A function
 * declared xem_host_<name> is imported as <name> from the "xem" module
 * (tools/game_module.py marks it; the port is compiled by the MIPS front end,
 * which has no wasm attributes); the exports are game_module.py's EXPORTS.
 * Game memory is the PS1 address space, so a game pointer is its original
 * address.
 */

/* Why the game suspends (xem_host_yield): the host's clock decides when it goes on. */
enum {
    XEM_YIELD_VSYNC = 1,    /* waiting for the next vertical blank */
    XEM_YIELD_DRAWSYNC = 2, /* waiting for drawing to end */
    XEM_YIELD_POLL = 3,     /* spinning on a device or interrupt-driven state */
};

/* Where the game restarts after a non-local exit (xem_host_restart). */
enum {
    XEM_RESTART_BOOT = 0,     /* the entry point: clear the BSS, run boot_main */
    XEM_RESTART_DISPATCH = 1, /* mode_dispatch(arg) on an empty stack */
};

/* Suspend the game: asyncify unwinds the game stack into game-module memory
 * and the host resumes it from the same point. */
void xem_host_yield(int reason);

/* Abandon the whole game stack and continue with xem_run(kind, arg): the
 * original resets the stack pointer and jumps (the mode dispatcher, soft
 * reset). Does not return. */
__attribute__((noreturn)) void xem_host_restart(int kind, int arg);

/* The game's fibers (port/fiber.c): the game itself, whose bottom frame is
 * xem_run, and the arena task (arena_task_resume), whose bottom frame is
 * xem_task_run. */
enum {
    XEM_FIBER_GAME = 0,
    XEM_FIBER_TASK = 1,
};

/* Suspend the running fiber (asyncify unwinds it into its own save area) and
 * continue fiber `fiber`: start it afresh on the shadow stack that ends at
 * `stack_top` when that is nonzero, else rewind it where it suspended. */
void xem_host_task_switch(unsigned int fiber, unsigned int stack_top);

/* A function the port does not define yet was called (build/game/stubs.txt),
 * or an inline assembly statement without a port function (unmapped-asm.txt;
 * its number + 0x10000). */
void xem_host_missing(int id);

/* An indirect call found no function of its signature at the address: the
 * image is absent or overwritten there, or the call needs an adapter. */
void xem_host_bad_call(unsigned int address, int signature);

/* Interrupt sources the host delivers through xem_interrupt (the PS1's I_STAT
 * bits). */
enum {
    XEM_IRQ_VBLANK = 0,
    XEM_IRQ_GPU = 1,
    XEM_IRQ_CDROM = 2,
    XEM_IRQ_DMA = 3,
    XEM_IRQ_RCNT0 = 4,
    XEM_IRQ_RCNT1 = 5,
    XEM_IRQ_RCNT2 = 6,
    XEM_IRQ_PAD = 7,
    XEM_IRQ_SIO = 8,
    XEM_IRQ_SPU = 9,
};

/* Root counters (the host's virtual clock): program, start, stop and read
 * counter `n` (0-2). The host raises XEM_IRQ_RCNT0 + n at its target when the
 * mode asks for an interrupt. */
void xem_host_rcnt_set(unsigned int n, unsigned int target, unsigned int mode);
void xem_host_rcnt_start(unsigned int n);
void xem_host_rcnt_stop(unsigned int n);
unsigned int xem_host_rcnt_read(unsigned int n);

/* The BIOS pad driver: write controller `port`'s receive buffer (the BIOS
 * format: status, id, data) of `length` bytes at `buffer`. */
void xem_host_pad_read(unsigned int port, void *buffer, unsigned int length);

/* A `break` instruction: the debugger and PC file server traps of the
 * development configurations. The host records the code and continues. */
void xem_host_debug_break(unsigned int code);

#endif
