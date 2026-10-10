/*
 * Small resident handwritten routines (decomp/src/resident/
 * mode_set_arena_task.s, console_report_printf.s) as portable C.
 */
#include "common.h"
#include "psyq/stdarg.h"

extern s32 mode_arena_task;
extern void *console_current;

s32 console_vprintf(s32 target, const char *format, va_list args);

/* Store the menu overlay's task index in mode_arena_task. */
void mode_set_arena_task(s32 task) {
    mode_arena_task = task;
}

/* The report printf: the original jumps into console_printf with its
 * arguments untouched. C cannot pass on a variable argument list, so this
 * is console_printf's own body (console_and_sound_driver.c): format to the
 * console when there is one. */
void console_report_printf(char *format, ...) {
    va_list args;

    if (console_current != NULL) {
        va_start(args, format);
        console_vprintf(0, format, args);
        va_end(args);
    }
}
