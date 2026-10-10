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

/* A function the port does not define yet was called (build/game/stubs.txt),
 * or an inline assembly statement without a port function (unmapped-asm.txt;
 * its number + 0x10000). */
void xem_host_missing(int id);

/* An indirect call found no function of its signature at the address: the
 * image is absent or overwritten there, or the call needs an adapter. */
void xem_host_bad_call(unsigned int address, int signature);

/* A `break` instruction: the debugger and PC file server traps of the
 * development configurations. The host records the code and continues. */
void xem_host_debug_break(unsigned int code);

#endif
