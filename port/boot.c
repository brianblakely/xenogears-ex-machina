/*
 * Entry, stack reset and the module's exports (docs/runtime.md). The
 * handwritten boot_entry_point, boot_reset_stack_and_gp and
 * boot_clear_bss_range (decomp/src/resident/*.s) set MIPS registers; here the
 * host's restart gives the game an empty stack instead.
 */
#include "common.h"
#include "resident/mode.h"

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
 * calls it again with the same arguments to resume a suspended game. */
void xem_run(s32 kind, s32 arg) {
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

/* Asyncify's save area: {current, end} and the unwound frames. */
static u32 xem_unwind_words[2 + 0x10000];

u32 *xem_unwind_area(void) {
    xem_unwind_words[0] = (u32)&xem_unwind_words[2];
    xem_unwind_words[1] = (u32)&xem_unwind_words[2 + 0x10000];
    return xem_unwind_words;
}
