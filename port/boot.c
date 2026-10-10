/*
 * Entry, stack reset and the module's exports (docs/runtime.md). The
 * handwritten boot_entry_point, boot_reset_stack_and_gp and
 * boot_clear_bss_range (decomp/src/resident/*.s) set MIPS registers; here the
 * host's restart gives the game an empty stack instead.
 */
#include "common.h"
#include "resident/mode.h"
#include "xem/fiber.h"

extern void (*heap_report_output)(char *line);
extern u8 boot_bss_last_word[];

void boot_main(void);
void xem_original_mode_dispatch(s32 error);

/* Set while xem_run enters the dispatcher on an empty stack. */
static s32 xem_dispatch_on_empty_stack;

/* Zero the words after `start` through `end` inclusive (boot_clear_bss_range.s). */
void boot_clear_bss_range(u8 *start, u8 *end) {
    u32 *word = (u32 *)start;

    while (word != (u32 *)end) {
        *++word = 0;
    }
}

/* Clear the BSS, the words after heap_report_output through
 * boot_bss_last_word, then run boot_main (boot_entry_point.s). Called from
 * the soft reset deep in the game stack, it restarts on an empty one. */
void boot_entry_point(void) {
    xem_host_restart(XEM_RESTART_BOOT, 0);
}

/* The stack is already empty where the port reaches this (mode_dispatch). */
void boot_reset_stack_and_gp(void) {
}

/* The dispatcher never returns and runs each mode on an emptied stack: called
 * from any depth, it restarts on an empty stack and runs there. */
void mode_dispatch(s32 error) {
    if (xem_dispatch_on_empty_stack) {
        xem_dispatch_on_empty_stack = 0;
        xem_original_mode_dispatch(error);
    }
    xem_host_restart(XEM_RESTART_DISPATCH, error);
}

/* The host runs the game from here, at start-up and after each restart; it
 * calls it again to resume the suspended game fiber (port/fiber.c). A start
 * on an empty stack abandons the task fiber; asyncify skips that reset when
 * it rewinds. */
void xem_run(s32 kind, s32 arg) {
    xem_fiber_reset();
    if (kind == XEM_RESTART_BOOT) {
        boot_clear_bss_range((u8 *)&heap_report_output, boot_bss_last_word);
        boot_main();
    } else {
        xem_dispatch_on_empty_stack = 1;
        mode_dispatch(arg);
    }
}

/* Run a game function the host delivers as an interrupt callback. */
void xem_call(u32 address) {
    ((void (*)(void))address)();
}

/* Whether game memory at `address` holds the code a dispatcher expects there:
 * FNV-1a over its first `length` original bytes. */
s32 xem_code_matches(u32 address, s32 length, unsigned long long hash) {
    u8 *code = (u8 *)address;
    unsigned long long h = 0xCBF29CE484222325ULL;
    s32 i;

    for (i = 0; i < length; i++) {
        h = (h ^ code[i]) * 0x100000001B3ULL;
    }
    return h == hash;
}

void xem_bad_call(u32 address, s32 signature) {
    xem_host_bad_call(address, signature);
}

void xem_unmapped_asm(s32 number) {
    xem_host_missing(0x10000 + number);
}

/* Loop iterations since the game last suspended. */
static u32 xem_loop_iterations;

s32 xem_in_critical_section(void);

/* Called at every loop back-edge of the game (tools/game_module.py): a loop
 * that spins on memory an interrupt changes would never let the host deliver
 * it, so a long-running loop suspends now and then, outside critical sections
 * and interrupt handlers (which cannot suspend). */
void xem_loop_poll(void) {
    extern s32 xem_in_interrupt;

    if (++xem_loop_iterations >= 0x10000 && !xem_in_critical_section() && !xem_in_interrupt) {
        xem_loop_iterations = 0;
        xem_host_yield(XEM_YIELD_POLL);
    }
}
