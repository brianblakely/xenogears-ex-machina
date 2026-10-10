#ifndef RESIDENT_CONSOLE_H
#define RESIDENT_CONSOLE_H

#include "common.h"
#include "psyq/types.h"
#include "psyq/libgpu.h"
#include "psyq/stdarg.h"

/* Header of a 16-bit TIM file written by the screenshot helper 80035F1C. */
typedef struct {
    u32 id;    /* 0x10 */
    u32 flag;  /* pixel mode: 2, 16-bit */
    u32 bytes; /* image block size: 12 + pixels */
    RECT rect;
} TimHeader;

/* Resident debug text console (the default heap/printf report output).
 * Unknown bytes keep their offsets. */
typedef struct {
    u16 flags;       /* bit 0: single buffered; bit 3: stop at the right edge
                      * instead of wrapping; bit 4: background tile */
    s16 tpage_id;    /* font texture page */
    u8 *buffer[2];   /* sprite packet buffers, selected by flags2E bit 0 */
    s16 left;        /* window */
    s16 top;
    s16 width;
    s16 height;
    s16 unk14;       /* character width */
    s16 unk16;       /* line height */
    u8 r, g, b;
    u8 mode;         /* bit 0: bright colour */
    DR_TPAGE tpage[2]; /* font texture page, per buffer */
    s16 capacity;    /* sprites per frame */
    u16 flags2E;     /* bit 1: 8-column font sheet; bit 2: upper case only;
                      * bit 3: proportional widths */
    s16 x;           /* cursor */
    s16 y;
    s16 unk34;       /* sprites this frame */
    s16 unk36;       /* line start */
    u8 *current;     /* next sprite in the active buffer */
    u16 cluts[4];    /* font CLUTs */
    TILE tile[2];    /* background, per buffer */
    u8 widths[0x60]; /* proportional widths from character 0x20 */
    u_long ot[2];    /* own ordering tables, per buffer */
    s16 saved_x;
    s16 saved_y;
    s16 saved_36;
    u8 texture_v;    /* font sheet row in its texture page */
} Console;


/* A conversion's settings (defaults at console_format_defaults). */
typedef struct {
    union {
        s32 flags;     /* 1 left-justified, 2 plus sign, 4 zero padded, 8 precision set */
        u8 bytes[4];   /* bytes[1]: the sign character */
    } u;
    s32 width;
    s32 precision;
    u32 base;
} FormatSpec;

s32 console_vprintf(s32 target, const char *format, va_list args); /* the console printf core */
void console_printf(char *format, ...);                            /* printf to the console */
void console_flush(u_long *ot);                                    /* flush the debug text into ot */
void console_set_external_block(s32 value);
Console *console_open(s32 left, s32 top, s32 width, s32 height, s32 capacity, u32 flags,
                       s32 tex_x, s32 tex_y, s32 clut_x, s32 clut_y, void *font); /* open */
void mode_set_arena_task(s32 task); /* set mode_arena_task, the menu overlay's task index */
void console_report_printf(char *format, ...); /* report printf */
s32 mode_load_battle_stage(s32 stage, s32 variant, u8 **stage_file, s32 *unused, u8 **scene); /* load a battle stage */
void sound_start_driver(s32 flags);

/* Hand-written ordering table link helpers (800315a0-80031894). */
void gpu_ot_link_sprt_8(u_long *ot, void *prim); /* link a SPRT_8 */
void gpu_ot_link_tile(u_long *ot, void *prim); /* link a TILE */
void sound_stop_driver(void);

/* More of the debug console and the system screens' services. */
void console_set_color(s32 r, s32 g, s32 b);
void console_load_font_cluts(u16 foreground, u16 background);
void console_place_cursor(s32 x, s32 y);
void console_place_cursor_and_line_start(s32 x, s32 y);
void console_close(void);
void sound_restore_voices(void);
void sound_silence_voices(void);
extern s32 mode_arena_task; /* the menu's mode (800379b4) */
extern u8 mode_arena_bout_outcome; /* the arena bout's outcome: the menu writes it, a field event reads it */

#endif
