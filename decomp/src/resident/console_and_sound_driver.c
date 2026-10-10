/* The debug console and its printf, music loading and the sound driver's
 * start-up, banks, output modes, reverb, volumes and memory
 * (800366e0-80039e18), GCC 2.7.2 at -G0; 800379b4 (the menu's task word) and
 * the report printf 800379c8 are handwritten. Its rodata opens at 0x80018b30
 * with the formatter's digit strings: 80036718's jump table (0x80018b58,
 * 0 mod 8) follows text_windows_and_pads.c's at 4 mod 8, so a unit starts between 800365fc
 * and 80036718; the console output hook 800366e0 is chosen. The sound driver
 * unit (sound.c) follows. */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libspu.h"
#include "psyq/stdarg.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "resident/text.h"
#include "own_declarations.h"
#include "sound_driver.h"

/* This unit's own variables: those of up to 8 bytes in its .sbss
 * (80059394), the larger format defaults and stage file list in its .bss
 * (8005a1cc), as the original assembler placed them (SBSS_console_and_sound_driver in
 * slus_006.64.mk). */
static Console *console_current;           /* 80059394 */
static RECT console_font_clut_rect;        /* 80059398: the console font CLUTs' VRAM rectangle */
static s32 console_external_block;         /* 800593A0: the console block is not owned (not released) */
static FormatSpec console_format_defaults; /* 8005A1CC: the format defaults */
/* The battle stage file list (mode_load_battle_stage): the stage file, the scene
 * data and the zero entry ending it, and a fourth entry that nothing
 * addresses (an 8-byte object of its own would be a small variable, in
 * .sbss). */
static FileRequest mode_battle_stage_file_list[4]; /* 8005A1DC */

void console_draw_char(s32 c);

/* The built-in console font, packed: its unpacked size and the LZSS stream
 * 80032e88 decodes (800374e8). */
extern u8 console_packed_font[];
INCLUDE_ASSET(".data", console_packed_font, 0x80050240, 0x354);
void (*console_char_output)(s32 c) = console_draw_char; /* 80050594: character output (800366f0) */
/* The console font CLUTs, built by 80036e4c: four rows of 16 colours. */
u16 console_font_cluts[64] = { /* 80050598 */
    0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF,
    0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF,
    0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
};
s32 mode_arena_task = 0; /* 80050618: the menu's mode (800379b4) */
u8 mode_arena_task_parameters[6] = {1, 0, 0, 2, 2, 0}; /* 8005061C: option bytes of the field and menu */
/* The arena bout's outcome, the byte after them: the menu writes it (arena_fighters_bout_and_effects.c
 * arena_bout_record_outcome, arena_camera_and_scenes.c arena_scene_update_bout_end) and a field event reads it
 * (field_event.c field_event_store_bout_outcome); no resident code addresses it. Whether
 * the original declared it apart or as a seventh byte of mode_arena_task_parameters is open. */
u8 mode_arena_bout_outcome = 0; /* 80050622 */


/* 800366E0: Install the character output run by 800366f0. */
void console_set_char_output(void (*callback)(s32 c)) {
    console_char_output = callback;
}

/* 800366F0: Output a character through the installed callback. */
void console_output_char(s32 c) {
    console_char_output(c);
}

/* 80036718: Format `format` with the word arguments at `args` through the console's
 * character output (800366F0): flags - + space 0, width and precision (or
 * *), conversions b d i u p X x c s n and %%. Returns the characters
 * written; an unknown conversion ends the output. Other characters and
 * %% share one output path. */
s32 console_vprintf(s32 target, const char *format, va_list args) {
    char buffer[0x100];
    FormatSpec spec;
    char *p;
    char *digits;
    char *end;
    s32 count = 0;
    s32 length;
    u32 value;
    s32 c;
    s32 i;

    c = *format;
    while (c != 0) {
        if (c != '%') {
            goto put;
        }
        spec = console_format_defaults;
        for (;;) {
            c = *++format;
            if (c == '-') {
                spec.u.flags |= 1;
            } else if (c == '+') {
                spec.u.flags |= 2;
            } else if (c == ' ') {
                spec.u.bytes[1] = c;
            } else if (c == '0') {
                spec.u.flags |= 4;
            } else {
                break;
            }
        }
        if (c == '*') {
            spec.width = va_arg(args, s32);
            if (spec.width < 0) {
                spec.width = -spec.width;
                spec.u.flags |= 1;
            }
            c = *++format;
        } else {
            while ((u32)(c - '0') <= 9) {
                spec.width = spec.width * 10 + (c - '0');
                c = *++format;
            }
        }
        if (c == '.') {
            c = *++format;
            if (c == '*') {
                spec.precision = va_arg(args, s32);
                c = *++format;
            } else {
                while ((u32)(c - '0') <= 9) {
                    spec.precision = spec.precision * 10 + (c - '0');
                    c = *++format;
                }
            }
            if (spec.precision >= 0) {
                spec.u.flags |= 8;
            }
        }
        p = (char *)&spec;
        if (spec.u.flags & 1) {
            spec.u.flags &= ~4;
        }
        switch (c) {
        case 'b':
            value = va_arg(args, s32);
            spec.u.bytes[1] = 0;
            spec.base = 2;
            goto number;
        case 'd':
        case 'i':
            value = va_arg(args, s32);
            if ((s32)value < 0) {
                value = -value;
                spec.u.bytes[1] = '-';
            } else if (spec.u.flags & 2) {
                spec.u.bytes[1] = '+';
            }
            spec.base = 10;
            goto number;
        case 'u':
            value = va_arg(args, s32);
            spec.u.bytes[1] = 0;
            spec.base = 10;
        number:
            if (!(spec.u.flags & 8)) {
                if (spec.u.flags & 4) {
                    spec.precision = spec.width;
                    if (spec.u.bytes[1] != 0) {
                        spec.precision = spec.width - 1;
                    }
                }
                if (spec.precision <= 0) {
                    spec.precision = 1;
                }
            }
            length = 0;
            while (value != 0) {
                *--p = value % spec.base + '0';
                value /= spec.base;
                length++;
            }
            while (length < spec.precision) {
                *--p = '0';
                length++;
            }
            if (spec.u.bytes[1] != 0) {
                *--p = spec.u.bytes[1];
                length++;
            }
            break;
        case 'p':
            spec.precision = 8;
            spec.u.flags |= 8;
        case 'X':
            digits = "0123456789ABCDEF";
            goto hex;
        case 'x':
            digits = "0123456789abcdef";
        hex:
            value = va_arg(args, s32);
            if (!(spec.u.flags & 8)) {
                if (spec.u.flags & 4) {
                    spec.precision = spec.width;
                }
                if (spec.precision <= 0) {
                    spec.precision = 1;
                }
            }
            length = 0;
            while (value != 0) {
                *--p = digits[value & 0xF];
                value >>= 4;
                length++;
            }
            while (length < spec.precision) {
                *--p = '0';
                length++;
            }
            break;
        case 'c':
            *--p = va_arg(args, s32);
            length = 1;
            break;
        case 's':
            p = va_arg(args, char *);
            if (!(spec.u.flags & 8)) {
                length = strlen(p);
            } else {
                end = memchr(p, 0, spec.precision);
                length = end - p;
                if (end == NULL) {
                    length = spec.precision;
                }
            }
            break;
        case 'n':
            p = va_arg(args, char *);
            *(s32 *)p = count;
            goto next;
        default:
            if (c != '%') {
                return count;
            }
        put:
            console_output_char(c);
            count++;
            goto next;
        }
        if (length < spec.width && !(spec.u.flags & 1)) {
            do {
                console_output_char(' ');
                count++;
            } while (length < --spec.width);
        }
        for (i = 0; i < length; i++) {
            console_output_char(p[i]);
        }
        count += length;
        while (length < spec.width) {
            console_output_char(' ');
            count++;
            length++;
        }
    next:
        format++;
        c = *format;
    }
    return count;
}

/* 80036CD8: Set, clear and read the console's flags and its second flag word (+0x2e). */
void console_set_flags(s32 bits) {
    console_current->flags |= bits;
}

/* 80036CF8 */
void console_clear_flags(s32 bits) {
    console_current->flags &= ~bits;
}

/* 80036D18 */
s16 console_get_flags(void) {
    return console_current->flags;
}

/* 80036D30 */
void console_set_flags2(s32 bits) {
    console_current->flags2E |= bits;
}

/* 80036D50 */
void console_clear_flags2(s32 bits) {
    console_current->flags2E &= ~bits;
}

/* 80036D70 */
s16 console_get_flags2(void) {
    return console_current->flags2E;
}

/* 80036D88: Set the console's character width and line height. */
void console_set_char_width(s16 value) {
    console_current->unk14 = value;
}

/* 80036D98 */
void console_set_line_height(s16 value) {
    console_current->unk16 = value;
}

/* 80036DA8: Move the console cursor: its column, then its row. */
void console_set_cursor_x(s16 value) {
    console_current->x = value;
}

/* 80036DB8 */
void console_set_cursor_y(s16 value) {
    console_current->y = value;
}

/* 80036DC8: Set the console text colour; any channel below 0x80 clears bright mode. */
void console_set_color(s32 r, s32 g, s32 b) {
    console_current->r = r;
    console_current->g = g;
    console_current->b = b;
    if (r < 0x80 || g < 0x80 || b < 0x80) {
        console_current->mode &= ~1;
    } else {
        console_current->mode |= 1;
    }
}

/* 80036E4C: Build and upload the console font CLUTs: four 16-color rows of
 * foreground/background stripes 1, 2, 4 and 8 entries wide. */
void console_load_font_cluts(u16 foreground, u16 background) {
    u16 *p = console_font_cluts;
    s32 i;
    s32 j;

    for (i = 0; i < 8; i++) {
        *p++ = background;
        *p++ = foreground;
    }
    for (; i < 12; i++) {
        *p++ = background;
        *p++ = background;
        *p++ = foreground;
        *p++ = foreground;
    }
    for (; i < 14; i++) {
        for (j = 0; j < 4; j++) {
            *p++ = background;
        }
        for (j = 0; j < 4; j++) {
            *p++ = foreground;
        }
    }
    for (j = 0; j < 8; j++) {
        *p++ = background;
    }
    for (; j < 16; j++) {
        *p++ = foreground;
    }
    LoadImage(&console_font_clut_rect, (u_long *)console_font_cluts);
}

/* 80036F44: The console's character width, line height, cursor column and row, and
 * the sprites it drew this frame. */
s16 console_get_char_width(void) {
    return console_current->unk14;
}

/* 80036F5C */
s16 console_get_line_height(void) {
    return console_current->unk16;
}

/* 80036F74 */
s16 console_get_cursor_x(void) {
    return console_current->x;
}

/* 80036F8C */
s16 console_get_cursor_y(void) {
    return console_current->y;
}

/* 80036FA4 */
s16 console_get_sprite_count(void) {
    return console_current->unk34;
}

/* 80036FBC: Save the console cursor. */
void console_save_cursor(void) {
    console_current->saved_x = console_current->x;
    console_current->saved_y = console_current->y;
    console_current->saved_36 = console_current->unk36;
}

/* 80036FE4: Restore the saved console cursor. */
void console_restore_cursor(void) {
    console_current->x = console_current->saved_x;
    console_current->y = console_current->saved_y;
    console_current->unk36 = console_current->saved_36;
}

/* 8003700C: printf to the console, when there is one. */
void console_printf(char *format, ...) {
    va_list args;

    if (console_current != NULL) {
        va_start(args, format);
        console_vprintf(0, format, args);
    }
}

/* 80037058: Place the console cursor relative to its origin. */
void console_place_cursor(s32 x, s32 y) {
    if (console_current != NULL) {
        console_current->x = console_current->left + x;
        console_current->y = console_current->top + y;
    }
}

/* 8003708C: Place the console cursor and line start relative to the origin. */
void console_place_cursor_and_line_start(s32 x, s32 y) {
    if (console_current != NULL) {
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        console_current->unk36 = console_current->x = console_current->left + x;
        console_current->y = console_current->top + y;
    }
}

/* 800370DC: Put a character on the console: a font sprite for printable characters
 * (wrapping, or stopping, at the right edge) and newlines; nothing once the
 * window or the sprite budget is full. */
void console_draw_char(s32 c) {
    Console *con = console_current;
    s32 width;

    if (con == NULL) {
        return;
    }
    if (con->y + con->unk16 > con->top + con->height) {
        return;
    }
    if (con->unk34 > con->capacity) {
        return;
    }
    if (c < 0x20) {
        if (c == '\n') {
            con->x = con->unk36;
            con->y += con->unk16;
        }
        return;
    }
    if ((con->flags2E & 4) && c >= 0x60) {
        c -= 0x20;
    }
    c -= 0x20;
    if (con->flags2E & 8) {
        width = con->widths[c];
    } else {
        width = con->unk14;
    }
    if (con->x + width >= con->left + con->width) {
        if (con->flags & 8) {
            return;
        }
        con->x = con->unk36;
        con->y += con->unk16;
    }
    if (c != 0) {
        *(u32 *)&((SPRT_8 *)con->current)->r0 = *(u32 *)&con->r;
        *(u32 *)&((SPRT_8 *)con->current)->x0 = con->x | (con->y << 16);
        if (con->flags2E & 2) {
            /* glyphs on a 16-pixel grid, 8 to a row */
            ((SPRT_8 *)con->current)->clut = con->cluts[(c & 0x18) >> 3];
            *(u16 *)&((SPRT_8 *)con->current)->u0 =
                ((c & 7) << 4) | ((con->texture_v + ((c & 0x60) >> 1)) << 8);
        } else {
            /* glyphs on an 8-pixel grid, 16 to a row */
            ((SPRT_8 *)con->current)->clut = con->cluts[(c & 0x30) >> 4];
            *(u16 *)&((SPRT_8 *)con->current)->u0 =
                ((c & 0xF) << 3) | ((con->texture_v + ((c & 0xC0) >> 3)) << 8);
        }
        con->current += sizeof(SPRT_8);
        con->unk34++;
    }
    con->x += width;
}

/* 800372CC: Home the console cursor and select the active text buffer. Capture the
 * window coordinates, cleared mode byte and buffer before writing the cursor. */
void console_home_cursor(void) {
    Console *console = console_current;
    s32 slot = console->flags2E & 1;
    s16 top = console->top;
    u8 mode = console->mode & 0xFE;
    u8 *current = console->buffer[slot];
    s16 left = console->left;

    console->unk34 = 0;
    console->y = top;
    console->saved_y = top;
    console->mode = mode;
    console->x = left;
    console->saved_x = left;
    console->unk36 = left;
    console->saved_36 = left;
    console->current = current;
}

/* 80037324: Flush this frame's console sprites (and texture page, and background
 * tile) into `ot`, or into the console's own ordering table, drawn at once,
 * when `ot` is NULL or -1; then flip the sprite buffers and home the cursor. */
void console_flush(u_long *ot) {
    Console *con = console_current;
    s32 own;
    s16 index;
    s32 n;
    SPRT_8 *sprite;

    if (con != NULL) {
        own = 0;
        if (con->flags & 1) {
            index = 0;
            con->flags2E &= ~1;
        } else {
            index = con->flags2E & 1;
            if (index) {
                con->flags2E &= ~1;
            } else {
                con->flags2E |= 1;
            }
        }
        if (ot == NULL || ot == (u_long *)-1) {
            DrawSync(0);
            ot = &con->ot[index];
            own = 1;
            TermPrim(ot);
        }
        n = con->unk34;
        sprite = (SPRT_8 *)con->buffer[index];
        while (n != 0) {
            gpu_ot_link_sprt_8(ot, sprite++);
            n--;
        }
        AddPrim(ot, &con->tpage[index]);
        if (con->flags & 0x10) {
            gpu_ot_link_tile(ot, &con->tile[index]);
        }
        console_home_cursor();
        if ((s16)own) {
            DrawOTag(ot);
        }
    }
}

/* 8003747C: Mark the console block as not owned (it is not released on closing). */
void console_set_external_block(s32 value) {
    console_external_block = value;
}

/* 8003748C: Close the console: restore the default report output and release the
 * console block unless it is not owned. */
void console_close(void) {
    if (console_current != NULL) {
        heap_report_output = (void (*)(char *))console_printf;
        if (console_external_block == 0) {
            heap_free(console_current);
        }
        console_current = NULL;
    }
    console_external_block = 0;
}

/* 800374E8: Open the debug text console at (left, top, width, height) with room for
 * `capacity` characters per frame: allocate it (unless a block was
 * supplied through 8003747C), load the font (packed; the built-in one when
 * `font` is NULL) to VRAM at (tex_x, tex_y) with its four CLUTs at
 * (clut_x, clut_y), and make it the report output. */
Console *console_open(s32 left, s32 top, s32 width, s32 height, s32 capacity, u32 flags,
                       s32 tex_x, s32 tex_y, s32 clut_x, s32 clut_y, void *font) {
    Console *console;
    u8 *data;
    s16 lower;
    s16 wide;
    s32 rows;
    u8 *p;             /* the console block, later the font pixels */
    RECT rect;
    s32 size;

    if (console_external_block != 0) {
        p = (u8 *)console_external_block;
    } else {
        heap_set_next_class(0x32);
        if (!(flags & 1)) {
            size = capacity * 32 + sizeof(Console);
        } else {
            size = capacity * 16;
            size += sizeof(Console);
        }
        p = heap_alloc(size, ((flags >> 2) ^ 1) & 1);
    }
    console = (Console *)p;
    console->buffer[0] = (u8 *)(console + 1);
    if (flags & 1) {
        console->buffer[1] = console->buffer[0];
    } else {
        console->buffer[1] = console->buffer[0] + capacity * 16;
    }
    if (font == NULL) {
        font = console_packed_font;
    }
    data = text_unpack_lzss_alloc(font, 0);
    wide = data[0] & 1;
    lower = data[0] & 2;
    console->unk14 = data[2];
    console->unk16 = data[3];
    if (flags & 2) {
        lower = 0;
    }
    console->x = console->left = left;
    console->flags = flags;
    console->y = console->top = top;
    console->width = width;
    console->height = height;
    console->r = console->g = console->b = 0xFF;
    console->capacity = capacity;
    console->unk34 = 0;
    console->flags2E = 0;
    console->mode = wide ? 0x7D : 0x75;
    console->texture_v = tex_y;
    if (lower == 0) {
        console->flags2E |= 4;
    }
    if (wide) {
        console->flags2E |= 2;
        rows = lower ? 0x30 : 0x20;
    } else {
        rows = lower ? 0x10 : 8;
    }
    rect.x = tex_x;
    rect.y = tex_y;
    rect.w = 0x20;
    rect.h = rows;
    p = data + 4;
    if (console->unk14 == 0) {
        console->flags2E |= 8;
        memmove(console->widths, data + 4, 0x60);
        p = data + 0x64;
    }
    LoadImage(&rect, (u_long *)p);
    console->tpage_id = GetTPage(0, 0, tex_x, tex_y);
    rect.x = clut_x;
    rect.y = clut_y;
    rect.w = 0x40;
    rect.h = 1;
    console->cluts[0] = GetClut(clut_x, clut_y);
    console->cluts[1] = GetClut(clut_x + 0x10, clut_y);
    console->cluts[2] = GetClut(clut_x + 0x20, clut_y);
    console->cluts[3] = GetClut(clut_x + 0x30, clut_y);
    console_font_clut_rect = rect;
    console_load_font_cluts(0x7FFF, 0);
    SetDrawTPage(&console->tpage[0], 0, 0, console->tpage_id);
    SetDrawTPage(&console->tpage[1], 0, 0, console->tpage_id);
    setTile(&console->tile[0]);
    setRGB0(&console->tile[0], 0, 0, 0);
    *(u32 *)&console->tile[0].x0 = left | (top << 16);
    *(u32 *)&console->tile[0].w = width | (height << 16);
    setSemiTrans(&console->tile[0], 1);
    console->tile[1] = console->tile[0];
    console_current = console;
    console_home_cursor();
    heap_free(data);
    return console;
}

/* 80037878: Sort `count` elements of `size` bytes at `base` in place (selection
 * sort): `compare` is positive when its second element goes first. */
void console_selection_sort(u8 *base, s32 count, s32 size, s32 (*compare)(void *a, void *b)) {
    u8 *temp = heap_alloc(size, 0);
    s32 i;
    s32 j;
    s32 best;

    for (i = 0; i < count; i++) {
        best = i;
        for (j = i; j < count; j++) {
            if (compare(base + size * best, base + j * size) > 0) {
                best = j;
            }
        }
        memcpy(temp, base + i * size, size);
        memcpy(base + i * size, base + size * best, size);
        memcpy(base + size * best, temp, size);
    }
    heap_free(temp);
}

/* 800379B4 */
INCLUDE_ASM("decomp/src/resident", mode_set_arena_task);

/* 800379C8 */
INCLUDE_ASM("decomp/src/resident", console_report_printf);

/* 800379D0: The no-op entry that the stripped debug hooks share. */
void console_empty_debug_hook(void) {
}

/* 800379D8: Load battle stage `stage` from directory 12/3: its stage file (file
 * 6 + 2 * stage) and its scene data (file 7 + 2 * stage + `variant`, whose
 * first word is its size), which ovl2615 battle_setup_build_stage sets up as its
 * `stage` and `scene`. Returns 0 with the stage file, 0 and the scene data
 * after its first word (also in mode_battle_scene_data), or -1 with zeros when the
 * directory has no such stage. */
s32 mode_load_battle_stage(s32 stage, s32 variant, u8 **stage_file, s32 *unused, u8 **scene) {
    s32 group;
    s32 index;
    s32 result = 0;
    u8 *scene_file;
    u8 *data;
    s32 file;
    s32 base;

    cd_get_selected_directory(&group, &index);
    cd_select_directory(12, 3);
    heap_select_owner_tag(4, 0);
    if (stage >= cd_get_directory_file_count(5) / 2) {
        result = -1;
        *stage_file = NULL;
        *unused = 0;
        *scene = NULL;
    } else {
        base = stage * 2;
        scene_file = heap_alloc(cd_get_aligned_file_size(base + 7 + variant), 1);
        heap_protect_block(scene_file);
        file = base + 6;
        data = heap_alloc(cd_get_aligned_file_size(file), 1);
        heap_protect_block(data);
        mode_battle_stage_file_list[0].file = file;
        mode_battle_stage_file_list[0].destination = data;
        base += variant + 7;
        mode_battle_stage_file_list[1].file = base;
        mode_battle_stage_file_list[1].destination = scene_file;
        mode_battle_stage_file_list[2].file = 0;
        mode_battle_stage_file_list[2].destination = NULL;
        cd_read_file_list(mode_battle_stage_file_list, 0, 0);
        *stage_file = data;
        *unused = 0;
        *scene = (u8 *)mode_battle_stage_file_list[1].destination + 4;
        mode_battle_scene_data = (u8 *)mode_battle_stage_file_list[1].destination + 4;
    }
    cd_select_directory(group, index);
    return result;
}

void sound_set_cd_mix_volume(s32 volume);
void sound_apply_volumes(void);
void sound_init_memory_pool(u32 start, s32 size);
void sound_reset_spu_memory_map(void);
void sound_set_cd_reverb_and_mix(s32 reverb, s32 mix);
void sound_set_master_volume(s32 volume, s32 frames);

/* 80037B88: Start the sound driver (error 0x28 when it runs): memory pools, the SPU
 * memory map and transfer ring, the tick event on root counter 2 and the
 * SPU callbacks, then default volumes, output mode, effect channels and
 * reverb. */
void sound_start_driver(s32 flags) {
    if (sound_driver_flags < 0) {
        sound_report_error(0x28);
        return;
    }
    sound_driver_flags = flags | 0xB801;
    SpuInitMalloc(4, sound_spu_malloc_table);
    sound_init_memory_pool((u32)sound_memory_pool, 0x6300);
    sound_reset_spu_memory_map();
    sound_transfer_ring = sound_alloc_memory_low(0xA0);
    sound_clear_voice_owners();
    sound_random_state = 0x12345678;
    sound_playing_seq_list = NULL;
    sound_effect_channels = NULL;
    sound_effect_bank_list = NULL;
    sound_wave_bank_list = NULL;
    sound_output_mode_voice = NULL;
    sound_pending_key_on_mask = 0;
    sound_pending_key_off_mask = 0;
    sound_fast_key_off_mask = 0;
    sound_volumes.attr.mvolmode.left = 0;
    sound_volumes.attr.mvolmode.right = 0;
    sound_volumes.attr.mask = 0xC;
    EnterCriticalSection();
    sound_tick_event = OpenEvent(0xF2000002, 2, 0x1000, (long (*)())sound_run_tick);
    SetRCnt(0xF2000002, 0x44E8, 0x1000);
    StartRCnt(0xF2000002);
    SpuSetTransferCallback(sound_complete_transfer);
    SpuSetIRQCallback(sound_dispatch_spu_irq);
    SpuSetIRQ(0);
    sound_tick_count = 0;
    sound_unread_spu_irq_count = 0;
    ExitCriticalSection();
    sound_alloc_spu_memory_at(0x2000, 0x10000, 4);
    sound_set_output_mode(1);
    sound_set_cd_reverb_and_mix(0, 1);
    sound_set_master_volume(0x3FFF, 0);
    sound_set_cd_volume(0x7FFF, 0);
    if (sound_driver_flags & 0x4000) {
        sound_set_cd_mix_volume(0x80);
    }
    sound_effect_channels = sound_create_effect_channels(0x10);
    sound_effect_voice_count = 8;
    sound_reverb_work_address = -1;
    sound_reverb_clear_buffer = 0;
    sound_reverb_settings.type = 0xFF;
    sound_set_reverb(4, 0, 0, 0);
    SpuSetReverb(1);
    sound_unread_last_error = 0;
}

/* 80037DC0: Shut the sound driver down: remove its SPU interrupt, transfer callback,
 * timer and event, release every voice and clear the reverb (error 0x29
 * when it is not running). */
void sound_stop_driver(void) {
    s32 i;

    if (sound_driver_flags == 0) {
        sound_report_error(0x29);
        return;
    }
    EnterCriticalSection();
    sound_driver_flags = 0;
    SpuSetIRQ(0);
    SpuSetTransferCallback(NULL);
    SpuSetIRQCallback(NULL);
    StopRCnt(0xF2000002);
    CloseEvent(sound_tick_event);
    ExitCriticalSection();
    for (i = 0; i < 24; i++) {
        sound_write_voice_release(i, 6, 3);
    }
    sound_write_key_off(0xFFFFFF);
    SpuSetReverbModeDepth(0, 0);
    SpuSetReverbModeType(0);
    sound_unread_last_error = 0;
}

/* 80037E8C: Mark every voice's channel for a full register update and clear the
 * voices-silenced state. */
void sound_restore_voices(void) {
    SoundChannel **channel = sound_voice_owners;
    SoundChannel *current;
    s32 i = 0;

    do {
        current = *channel;
        i++;
        if (current != NULL) {
            current->flags |= 0x1F5;
        }
        channel++;
    } while (i < 24);
    sound_driver_flags &= ~0x40;
}

/* 80037EE4: Silence every SPU voice, preserving the low ADSR1 byte. */
void sound_silence_voices(void) {
    SpuVoice *voice = sound_spu_registers->voice;
    s32 i;

    sound_driver_flags |= 0x40;
    for (i = 0; i < 24; i++) {
        voice->volume_left = 0;
        voice->volume_right = 0;
        voice->pitch = 0;
        voice->adsr1 = (voice->adsr1 & 0xFF) + 0x7F00;
        voice->adsr2 = 0x1FDF;
        voice++;
    }
}


/* 80037F44: Enable the driver's tick event once. */
void sound_enable_tick(void) {
    if (!(sound_driver_flags & 1)) {
        sound_driver_flags |= 1;
        EnableEvent(sound_tick_event);
    }
}

/* 80037F88: Disable the driver's tick event once. */
void sound_disable_tick(void) {
    if (sound_driver_flags & 1) {
        DisableEvent(sound_tick_event);
        sound_driver_flags &= ~1;
    }
}

void sound_set_wave_bank_stream_target(s32 address, s32 size);

/* 80037FD8: Load a wave bank: allocate SPU memory for its samples (800381F4),
 * transfer them and add a copy of its header to the loaded banks. Returns
 * the copy, or NULL (error 0x1F without SPU memory, 0x1E without memory
 * for the copy). */
SoundSequence *sound_load_wave_bank(SoundSequence *bank, s32 mode) {
    s32 address = sound_alloc_wave_bank_spu_memory(bank, mode);
    SoundSequence *copy;
    SoundSequence **link;

    if (address == 0) {
        sound_report_error(0x1F);
        return NULL;
    }
    sound_queue_spu_write(address, (u8 *)bank + bank->offset, bank->size, NULL);
    copy = sound_alloc_memory_high(bank->header_size);
    if (copy == NULL) {
        sound_free_spu_memory(address);
        sound_report_error(0x1E);
        return NULL;
    }
    sound_copy_memory(copy, bank, bank->header_size);
    copy->address = address;
    DisableEvent(sound_tick_event);
    link = &sound_wave_bank_list;
    if (sound_wave_bank_list != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = copy;
    copy->next = NULL;
    EnableEvent(sound_tick_event);
    return copy;
}

/* 800380D0: Start loading a wave bank whose samples arrive in parts: allocate its SPU
 * memory, transfer the samples among the first `size` bytes of the file
 * (8003827C takes the rest) and add a copy of its header to the loaded
 * banks. Returns the copy, or NULL (error 0x16 when a bank with its key is
 * loaded, 0x1F without SPU memory, 0x1E without memory for the copy). */
SoundSequence *sound_start_wave_bank_stream(SoundSequence *bank, s32 size, s32 mode) {
    s32 address;
    SoundSequence *copy;
    SoundSequence **link;

    if (sound_find_wave_bank(bank->key) != NULL) {
        sound_report_error(0x16);
        return NULL;
    }
    address = sound_alloc_wave_bank_spu_memory(bank, mode);
    if (address == 0) {
        sound_report_error(0x1F);
        return NULL;
    }
    sound_set_wave_bank_stream_target(address, bank->size);
    sound_transfer_wave_bank_part((u8 *)bank + bank->offset, size - bank->header_size);
    copy = sound_alloc_memory_high(bank->header_size);
    if (copy == NULL) {
        sound_free_spu_memory(address);
        sound_report_error(0x1E);
        return NULL;
    }
    sound_copy_memory(copy, bank, bank->header_size);
    copy->address = address;
    DisableEvent(sound_tick_event);
    link = &sound_wave_bank_list;
    if (sound_wave_bank_list != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = copy;
    copy->next = NULL;
    EnableEvent(sound_tick_event);
    return copy;
}

/* 800381F4: Allocate SPU memory for a wave bank's samples: anywhere for mode -1, or
 * for mode 0 when the bank asks for no address; otherwise at the bank's
 * address. Returns the address, 0 when there is no room. */
s32 sound_alloc_wave_bank_spu_memory(SoundSequence *bank, s32 mode) {
    if (mode == 0) {
        mode = bank->address;
    } else if (mode == -1) {
        mode = 0;
    }
    /* The allocator's result is returned as the value it leaves; the
     * function has no return expression. */
    if (mode == 0) {
        sound_alloc_spu_memory(bank->size, bank->volume);
        return;
    }
    sound_alloc_spu_memory_at(bank->size, bank->address, bank->volume);
}

/* 80038264: Start a streamed wave bank's transfer: its SPU address and size. */
void sound_set_wave_bank_stream_target(s32 address, s32 size) {
    sound_wave_bank_stream_address = address;
    sound_wave_bank_stream_bytes_left = size;
}

/* 8003827C: Transfer the next part of a streamed wave bank's samples (at most what
 * is still missing). Returns the bytes still missing. */
s32 sound_transfer_wave_bank_part(u8 *data, s32 size) {
    s32 left = sound_wave_bank_stream_bytes_left;
    s32 address;

    if (left == 0) {
        return 0;
    }
    size = left < size ? left : size;
    address = sound_wave_bank_stream_address;
    sound_queue_spu_write(address, data, size, NULL);
    sound_wave_bank_stream_address = (u32)address + (u32)size;
    return sound_wave_bank_stream_bytes_left = (u32)left - (u32)size;
}


/* 80038310: Release a loaded wave bank: unlink it, free its SPU memory (error 0x24
 * when that fails, 0x11 when the bank is not loaded) and its copy. */
void sound_release_wave_bank(SoundSequence *bank) {
    SoundSequence *entry;
    SoundSequence *prev = NULL;

    for (entry = sound_wave_bank_list; entry != NULL; entry = entry->next) {
        if (entry == bank) {
            break;
        }
        prev = entry;
    }
    if (entry == NULL) {
        sound_report_error(0x11);
        return;
    }
    DisableEvent(sound_tick_event);
    if (prev != NULL) {
        prev->next = bank->next;
    } else {
        sound_wave_bank_list = bank->next;
    }
    EnableEvent(sound_tick_event);
    if (bank->address != sound_free_spu_memory(bank->address)) {
        sound_report_error(0x24);
    }
    sound_free_memory(bank);
}

/* 800383EC: The loaded wave bank with `key`, or NULL. */
SoundSequence *sound_find_wave_bank(s32 key) {
    SoundSequence *sequence;

    for (sequence = sound_wave_bank_list; sequence != NULL; sequence = sequence->next) {
        if (sequence->key == key) {
            break;
        }
    }
    return sequence;
}

/* 80038428: Add a sound effect bank to the loaded banks: error 0x15 when a bank
 * with its id is loaded (unless the driver is in its error state), or the
 * bank data's error. */
void sound_add_effect_bank(SoundBank *bank) {
    SoundBank *added = bank;
    SoundBank *entry;
    SoundBank **link;
    s16 error;

    if (!(sound_driver_flags & 0x80)) {
        for (entry = sound_effect_bank_list; entry != NULL; entry = entry->next) {
            if (bank->id == entry->id) {
                sound_report_error(0x15);
                return;
            }
        }
    }
    error = sound_check_file((u32 *)bank, 0x73646573, 0x101);
    if (error != 0) {
        sound_report_error(error);
        return;
    }
    DisableEvent(sound_tick_event);
    link = &sound_effect_bank_list;
    if (sound_effect_bank_list != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = added;
    added->next = NULL;
    EnableEvent(sound_tick_event);
}

/* 8003852C: Remove a sound effect bank from the loaded banks (error 0x10 when it is
 * not loaded, 0xB when its data is no longer valid). */
void sound_remove_effect_bank(SoundBank *bank) {
    SoundBank *entry;
    SoundBank *prev = NULL;
    SoundBank *target = bank;
    s16 error;

    for (entry = sound_effect_bank_list; entry != NULL; entry = entry->next) {
        if (entry == target) {
            break;
        }
        prev = entry;
    }
    if (entry == NULL) {
        sound_report_error(0x10);
        return;
    }
    sound_stop_bank_effects(bank);
    DisableEvent(sound_tick_event);
    if (prev != NULL) {
        prev->next = target->next;
    } else {
        sound_effect_bank_list = target->next;
    }
    target->next = NULL;
    error = sound_check_file((u32 *)bank, 0x73646573, 0x101);
    if (error != 0) {
        sound_report_error(0xB);
        return;
    }
    EnableEvent(sound_tick_event);
}

/* 80038624: Stop the effect channels and forget the loaded banks. */
void sound_clear_effect_banks(void) {
    sound_stop_all_effects();
    sound_effect_bank_list = NULL;
}

/* 8003864C: The loaded bank with the id of `bank` (or `id` when NULL). */
SoundBank *sound_find_effect_bank(SoundBank *bank, s16 id) {
    SoundBank *entry;

    if (bank != NULL) {
        id = bank->id;
    }
    for (entry = sound_effect_bank_list; entry != NULL; entry = entry->next) {
        if (id == entry->id) {
            break;
        }
    }
    return entry;
}

/* 8003869C: Stop the sequences flagged 1 (sound_stop_all_seqs) and the effect
 * channels. */
void sound_stop_all_seqs_and_effects(void) {
    sound_stop_all_seqs();
    sound_stop_all_effects();
}


/* 800386C4: Select the output mode (1, 2 or 3; otherwise the plain one) and reapply
 * the volumes: master and CD, reverb depth, every sequence, the CD mix and
 * the mode voice. */
void sound_set_output_mode(s32 mode) {
    SoundSeq *seq;
    SoundModeVoice *voice;
    s16 volume;

    sound_driver_flags &= 0xF8FF;
    switch (mode) {
    case 0:
        break;
    case 1:
        sound_driver_flags |= 0x100;
        break;
    case 2:
        sound_driver_flags |= 0x300;
        break;
    case 3:
        sound_driver_flags |= 0x500;
        break;
    }
    sound_apply_volumes();
    SpuSetReverbModeDepth(sound_reverb_depth.left, sound_reverb_depth.right);
    for (seq = sound_playing_seq_list; seq != NULL; seq = seq->next) {
        sound_request_seq_channel_updates(0x100, seq);
    }
    if (sound_driver_flags & 0x4000) {
        sound_set_cd_mix_volume(sound_volumes.cd_request);
    }
    voice = sound_output_mode_voice;
    if (voice != NULL && (voice->flags & 1)) {
        volume = voice->volume;
        if (sound_get_output_mode() != 0) {
            volume <<= 7;
            voice->left = volume;
            voice->right = 0;
            voice->unk64 = 0;
            voice->unk66 = volume;
        } else {
            volume <<= 6;
            voice->left = volume;
            voice->right = volume;
            voice->unk64 = volume;
            voice->unk66 = volume;
        }
        voice->unk36 = 1;
        voice->unk62 = 1;
    }
}

/* 80038824: The output mode sound_set_output_mode selected, by its 0x700 bits: 0 for
 * mode 0, 1 for mode 1, 2 for modes 2 and 3. */
s32 sound_get_output_mode(void) {
    s32 mode;

    if (sound_driver_flags & 0x700) {
        mode = 1;
        if (sound_driver_flags & 0x600) {
            mode = 2;
        }
    } else {
        mode = 0;
    }
    return mode;
}

/* These fixed, contiguous attenuation bytes form CdMix's CdlATV argument
 * (one object; the names of its last three are in link.ld). */
extern u8 sound_cd_mix, sound_cd_mix_left_to_right, sound_cd_mix_right_to_right, sound_cd_mix_right_to_left;

/* 8003885C: Remember CD volume; Mono halves it into both channels, while stereo
 * modes keep the same-channel volume and clear the cross channels. */
void sound_set_cd_mix_volume(s32 volume) {
    u16 flags = sound_driver_flags;
    s32 same;

    sound_volumes.cd_request = volume;
    if (flags & 0x700) {
        same = volume;
        volume = 0;
    } else {
        volume >>= 1;
        same = volume;
    }
    sound_cd_mix = sound_cd_mix_right_to_right = same;
    sound_cd_mix_left_to_right = sound_cd_mix_right_to_left = volume;
    CdMix((CdlATV *)&sound_cd_mix);
}

/* 800388D4: Whether sequences set the reverb (driver flag 0x1000). */
void sound_set_seq_reverb_enabled(s32 enable) {
    if (enable != 0) {
        sound_driver_flags |= 0x1000;
    } else {
        sound_driver_flags &= ~0x1000;
    }
}

/* 8003890C: Enable or disable a bank (its flag 1). */
void sound_set_effect_bank_enabled(SoundBank *bank, s32 enable) {
    if (enable != 0) {
        bank->flags &= ~1;
    } else {
        bank->flags |= 1;
    }
}

void sound_clear_reverb_work_area(s32 address, s32 size);

/* Compiled-out debug trace of the reverb work area allocation. */
#define REVERB_TRACE_ALLOC(address) do { } while (0)

/* 80038934: Set the reverb type, depth, delay and feedback: type 0 turns it off, -1
 * keeps the current type and -2 changes nothing. A new type moves the work
 * area to the top of SPU memory and clears it (error 0x20, reverb off,
 * when there is no room). */
void sound_set_reverb(s32 type, s32 depth, s32 delay, s32 feedback) {
    SpuReverbAttr attr; /* unused in the original; reserves 20 bytes */
    long current;
    s32 changed = 0;
    s32 size;
    s32 address;

    if (type == -2) {
        return;
    }
    if (type == 0) {
        feedback = 0;
        delay = 0;
        depth = 0;
    } else if (type == -1) {
        type = sound_reverb_settings.type;
    }
    SpuGetReverbModeType(&current);
    if (current != type || type == 0) {
        if (sound_reverb_work_address != -1) {
            sound_free_spu_memory(sound_reverb_work_address);
        }
        size = sound_reverb_work_area_sizes[type];
        address = 0x80000 - size;
        sound_reverb_work_address = sound_alloc_spu_memory_at(size, address, 5);
        REVERB_TRACE_ALLOC(sound_reverb_work_address);
        changed = 1;
        if (sound_reverb_work_address == 0) {
            sound_report_error(0x20);
            type = 0;
            feedback = 0;
            delay = 0;
            depth = 0;
        }
    }
    sound_reverb_settings.type = type;
    sound_volumes.unk2C = depth;
    sound_reverb_settings.delay = delay;
    sound_reverb_settings.feedback = feedback;
    sound_apply_volumes();
    if (changed) {
        SpuSetReverbModeDepth(0, 0);
        SpuSetReverbModeType(type);
        sound_clear_reverb_work_area(address, size);
    } else {
        SpuSetReverbModeDepth(sound_reverb_depth.left, sound_reverb_depth.right);
        SpuSetReverbModeDelayTime(delay);
        SpuSetReverbModeFeedback(feedback);
    }
}

/* 80038AD4: Clear `size` bytes of SPU memory at `address` (the reverb work area)
 * from a zeroed buffer, allocated on first use, in chained transfers. */
void sound_clear_reverb_work_area(s32 address, s32 size) {
    sound_reverb_clear_address = address;
    sound_reverb_clear_bytes_left = size;
    if (sound_reverb_clear_buffer == 0) {
        sound_reverb_clear_buffer = (s32)sound_alloc_memory_low(0x840);
        if (sound_reverb_clear_buffer == 0) {
            sound_report_error(0x1E);
        }
    }
    sound_driver_flags |= 0x20;
    sound_clear_reverb_work_part();
}


/* 80038B4C: Clear the next part (at most 0x840 bytes, else 0x800) of the reverb work
 * area, chaining itself as the transfer callback; when done, release the
 * buffer and restore the reverb settings. */
void sound_clear_reverb_work_part(void) {
    s32 size;
    s32 address;

    if (sound_reverb_clear_bytes_left == 0) {
        sound_free_memory((void *)sound_reverb_clear_buffer);
        sound_reverb_clear_buffer = 0;
        SpuSetReverbModeDepth(sound_reverb_depth.left, sound_reverb_depth.right);
        SpuSetReverbModeDelayTime(sound_reverb_settings.delay);
        SpuSetReverbModeFeedback(sound_reverb_settings.feedback);
        sound_driver_flags &= ~0x20;
        return;
    }
    size = 0x800;
    if (sound_reverb_clear_bytes_left <= 0x840) {
        size = sound_reverb_clear_bytes_left;
    }
    address = sound_reverb_clear_address;
    sound_reverb_clear_bytes_left -= size;
    sound_reverb_clear_address = address + size;
    sound_queue_spu_write(address, (u8 *)sound_reverb_clear_buffer, size, sound_clear_reverb_work_part);
    if (!(sound_driver_flags & 0x10)) {
        sound_queue_spu_write(address, (u8 *)sound_reverb_clear_buffer, size, NULL);
    }
}

/* 80038C68: Set the master volume at once, or fade to it over `frames`. */
void sound_set_master_volume(s32 volume, s32 frames) {
    s32 delta;

    sound_volumes.master_slide.target = volume;
    if (frames == 0) {
        sound_volumes.master_slide.value.value = volume << 16;
        sound_volumes.master_slide.frames = 0;
        sound_volumes.master = volume;
        sound_set_stereo_volume((s16)volume, &sound_volumes.attr.mvol, 0);
        sound_volumes.attr.mask |= 3;
        return;
    }
    delta = (volume << 8) - (sound_volumes.master_slide.value.value >> 8);
    if (delta != 0) {
        sound_volumes.master_slide.frames = frames;
        sound_volumes.master_slide.step = (delta / frames) << 8;
    }
}

/* 80038D18: Set the CD volume at once, or fade to it over `frames`. */
void sound_set_cd_volume(s32 volume, s32 frames) {
    s32 delta;

    sound_volumes.cd_slide.target = volume;
    if (frames == 0) {
        sound_volumes.cd_slide.value.value = volume << 16;
        sound_volumes.cd_slide.frames = 0;
        sound_volumes.attr.cd.volume.left = sound_volumes.attr.cd.volume.right = sound_volumes.cd = volume;
        sound_volumes.attr.mask |= 0xC0;
        return;
    }
    delta = (volume << 8) - (sound_volumes.cd_slide.value.value >> 8);
    if (delta != 0) {
        sound_volumes.cd_slide.frames = frames;
        sound_volumes.cd_slide.step = (delta / frames) << 8;
    }
}


/* 80038DB4: Set the CD audio reverb and mix switches. */
void sound_set_cd_reverb_and_mix(s32 reverb, s32 mix) {
    sound_volumes.attr.cd.reverb = reverb;
    sound_volumes.attr.cd.mix = mix;
    sound_volumes.attr.mask |= 0x300;
    SpuSetCommonAttr(&sound_volumes.attr);
}

/* 80038DF4: Apply the master and CD volumes. */
void sound_apply_volumes(void) {
    sound_set_stereo_volume(sound_volumes.master, &sound_volumes.attr.mvol, 0);
    sound_volumes.attr.cd.volume.left = sound_volumes.attr.cd.volume.right = sound_volumes.cd;
    sound_set_stereo_volume(sound_volumes.unk2C, &sound_reverb_depth, 1);
    sound_volumes.attr.mask |= 0xC3;
}

/* 80038E6C: Set a stereo volume pair from the low volume halfword, inverting one
 * side for the surround modes. The channel selector is an unsigned byte. */
void sound_set_stereo_volume(s32 volume, SpuVolume *out, s32 channel) {
    u16 flags = sound_driver_flags;

    out->right = volume;
    out->left = volume;
    if (flags & 0x600) {
        channel = (u8)channel;
        if (!(flags & 0x200)) {
            if ((channel ^ 1) != 0) {
                out->left = 0U - (u32)volume;
            } else {
                out->right = 0U - (u32)volume;
            }
        } else if (channel != 0) {
            out->left = 0U - (u32)volume;
        } else {
            out->right = 0U - (u32)volume;
        }
    }
}

/* 80038EC0: Make [start, start + size) the driver memory pool, aligned to 16 bytes. */
void sound_init_memory_pool(u32 start, s32 size) {
    SoundBlock *block;

    size &= ~0xF;
    if (start & 0xF) {
        size -= 0x10;
        start = (start + 0xF) & ~0xF;
    }
    sound_memory_pool_end = start + size;
    block = (SoundBlock *)start;
    block->flags = 0x8000;
    sound_memory_pool_head = block;
    sound_unread_memory_pool_size = size;
    block->unk2 = 0;
    block->unk4 = 0;
    block->end = start + 0x10;
    block->next = NULL;
}

/* 80038F18: Allocate `size` bytes of driver memory, cleared, from the first gap that
 * fits (between blocks or after the last one). Returns the data or NULL
 * (then the driver event stays disabled). */
void *sound_alloc_memory_low(s32 size) {
    SoundBlock *block;
    SoundBlock *new;
    u32 need;
    u32 limit;
    u8 *data;

    DisableEvent(sound_tick_event);
    need = ((size + 0xF) & ~0xF) + 0x10;
    for (block = sound_memory_pool_head; block->next != NULL; block = block->next) {
        limit = (u32)block->next;
        if (limit - block->end >= need) {
            goto found;
        }
    }
    limit = sound_memory_pool_end;
    if (limit - block->end >= need) {
    found:
        new = (SoundBlock *)((block->end + 0xF) & ~0xF);
        data = (u8 *)(new + 1);
        new->end = (u32)(data + size);
        new->next = NULL;
        new->unk4 = 0;
        new->flags = 2;
        new->unk2 = 0;
        new->next = block->next;
        block->next = new;
        EnableEvent(sound_tick_event);
        sound_clear_memory((u32 *)data, size);
        return data;
    }
    return NULL;
}

/* 80039024: Allocate `size` bytes of driver memory, cleared, from the highest gap
 * that fits (between blocks or after the last one). Returns the data or
 * NULL (then the driver event stays disabled). */
void *sound_alloc_memory_high(s32 size) {
    s32 need;
    SoundBlock *entry;
    SoundBlock *after;
    u32 limit;
    SoundBlock *block;
    u8 *data;

    DisableEvent(sound_tick_event);
    need = ((size + 0xF) & ~0xF) + 0x10;
    after = NULL;
    limit = 0;
    for (entry = sound_memory_pool_head;; entry = entry->next) {
        if (entry->next == NULL) {
            if ((s32)(sound_memory_pool_end - entry->end) >= need) {
                after = entry;
                limit = sound_memory_pool_end;
            }
            break;
        }
        if ((s32)((u32)entry->next - entry->end) >= need) {
            after = entry;
            limit = (u32)entry->next;
        }
    }
    limit -= need;
    if (after == NULL) {
        return NULL;
    }
    block = (SoundBlock *)((limit + 0xF) & ~0xF);
    data = (u8 *)(block + 1);
    block->end = (u32)(data + size);
    block->next = NULL;
    block->unk4 = 0;
    block->flags = 2;
    block->unk2 = 0;
    block->next = after->next;
    after->next = block;
    EnableEvent(sound_tick_event);
    sound_clear_memory((u32 *)data, size);
    return data;
}

/* 80039144: Release a block of driver memory: unlink it from the pool list (the head
 * is never released), with the driver's event disabled. The head variable
 * is reused for the predecessor test; that keeps the pool head in its own
 * register through the first comparison and the walk in a copy of it, as in
 * the original. */
void sound_free_memory(void *data) {
    SoundBlock *head = sound_memory_pool_head;
    SoundBlock *block;
    SoundBlock *entry;
    SoundBlock *prev;

    DisableEvent(sound_tick_event);
    block = (SoundBlock *)data - 1;
    entry = head;
    prev = NULL;
    while (entry != block) {
        prev = entry;
        entry = entry->next;
    }
    head = prev;
    if (head != NULL) {
        prev->next = block->next;
    }
    EnableEvent(sound_tick_event);
}

/* 800391CC: The largest free gap of the driver memory pool, in 16-byte units. */
s32 sound_get_largest_free_gap(void) {
    SoundBlock *block = sound_memory_pool_head;
    s32 largest = 0;
    s32 gap;

    while (block->next != NULL) {
        gap = (u32)block->next - block->end;
        if (largest < gap) {
            largest = gap;
        }
        block = block->next;
    }
    gap = sound_memory_pool_end - block->end;
    if (largest < gap) {
        largest = gap;
    }
    return largest & ~0xF;
}

/* 80039248: Copy `size` bytes: sixteen at a time, then words, then bytes. */
void sound_copy_memory(void *dst, void *src, s32 size) {
    s32 *d = dst;
    s32 *s = src;
    s32 n;
    s32 a, b, c;

    for (n = size >> 4; n != 0; n--) {
        a = s[1];
        b = s[2];
        c = s[3];
        d[0] = s[0];
        d[1] = a;
        d[2] = b;
        d[3] = c;
        s += 4;
        d += 4;
    }
    for (n = (size >> 2) & 3; n != 0; n--) {
        *d++ = *s++;
    }
    for (n = size & 3; n != 0; n--) {
        *(u8 *)d = *(u8 *)s;
        d = (s32 *)((u8 *)d + 1);
        s = (s32 *)((u8 *)s + 1);
    }
}
/* 800392EC: Clear `size` bytes: sixteen at a time, then words, then bytes. */
void sound_clear_memory(void *data, s32 size) {
    u32 *p = data;
    s32 n;

    for (n = size >> 4; n != 0; n--) {
        p[3] = 0;
        p[2] = 0;
        p[1] = 0;
        p[0] = 0;
        p += 4;
    }
    for (n = (size >> 2) & 3; n != 0; n--) {
        *p++ = 0;
    }
    for (n = size & 3; n != 0; n--) {
        *(u8 *)p = 0;
        p = (u32 *)((u8 *)p + 1);
    }
}

/* 80039360: Reset the SPU memory map to one reserved entry for the first 0x1010
 * bytes. */
void sound_reset_spu_memory_map(void) {
    s32 i;

    for (i = 11; i >= 0; i--) {
        sound_spu_memory_map[i].flags = 0;
    }
    sound_spu_memory_map[0].flags = 0x81;
    sound_spu_memory_map[0].unk1 = 5;
    sound_spu_memory_map[0].address = 0;
    sound_spu_memory_map[0].size = 0x1010;
    sound_spu_memory_map[0].next = 0;
}

s32 sound_find_free_spu_block(void);

/* 800393B8: Allocate `size` bytes of SPU memory in the first gap of the map that
 * fits (or after its last entry). Returns the address, 0 when none. */
s32 sound_alloc_spu_memory(s32 size, u16 mode) {
    SpuMemBlock *entry = sound_spu_memory_map;
    SpuMemBlock *block;
    u32 end = sound_spu_memory_map[0].address + sound_spu_memory_map[0].size;
    s32 i;

    if (entry->next != 0) {
    scan_gap:
        block = &sound_spu_memory_map[entry->next];
        if ((s32)(block->address - end) >= size) {
            goto found;
        }
        entry = block;
        end = block->address + block->size;
        if (entry->next != 0) {
            goto scan_gap;
        }
    }
    if ((s32)(0x80000 - end) < size) {
        return 0;
    }
found:
    i = sound_find_free_spu_block();
    if (i < 0) {
        return 0;
    }
    block = &sound_spu_memory_map[i];
    block->flags = 0x80;
    block->unk1 = 0;
    block->address = end;
    block->size = size;
    block->next = entry->next;
    entry->next = i;
    return end;
}

/* 800394B8: Allocate `size` bytes of SPU memory at the top of the last gap of the
 * map that fits (the space after the last entry included). The new entry
 * is linked after the map's last entry. Returns the address, 0 when none. */
s32 sound_alloc_spu_memory_high(s32 size) {
    SpuMemBlock *entry = sound_spu_memory_map;
    SpuMemBlock *found = NULL;
    SpuMemBlock *block;
    u32 end;
    u32 address;
    s32 i;

    for (;;) {
        end = entry->address + entry->size;
        if (entry->next == 0) {
            if ((s32)(0x80000 - end) >= size) {
                found = entry;
                address = 0x80000 - size;
            }
            break;
        }
        block = &sound_spu_memory_map[entry->next];
        if ((s32)(block->address - end) >= size) {
            found = entry;
            address = block->address - size;
        }
        entry = block;
    }
    if (found == NULL) {
        return 0;
    }
    i = sound_find_free_spu_block();
    if (i < 0) {
        return 0;
    }
    block = &sound_spu_memory_map[i];
    block->flags = 0x80;
    block->unk1 = 0;
    block->address = address;
    block->size = size;
    block->next = entry->next;
    entry->next = i;
    return address;
}

/* 800395B8: Reserve `size` bytes of SPU memory at `address` when the map leaves
 * that range free, linking the new entry after the one before it.
 * Returns the address, 0 when the range is taken or no entry is free. */
s32 sound_alloc_spu_memory_at(s32 size, s32 address, u16 mode) {
    SpuMemBlock *entry = sound_spu_memory_map;
    SpuMemBlock *block;
    s32 gap = 0;
    s32 end = entry->address + entry->size;
    s32 top = address + size;
    s32 i;

    while ((s32)entry->address < address) {
        if (entry->next == 0) {
            gap = 0x80000 - end;
            break;
        }
        block = &sound_spu_memory_map[entry->next];
        if ((s32)block->address >= top) {
            gap = block->address - end;
            break;
        }
        entry = block;
        end = block->address + block->size;
    }
    if (gap < size || address < end) {
        return 0;
    }
    i = sound_find_free_spu_block();
    if (i < 0) {
        return 0;
    }
    block = &sound_spu_memory_map[i];
    block->flags = 0x80;
    block->unk1 = 0;
    block->address = address;
    block->size = size;
    block->next = entry->next;
    entry->next = i;
    return address;
}

/* 800396E0: Release the SPU memory map entry at `address`, unlinking it. Returns the
 * address, 0 when no entry has it. */
u32 sound_free_spu_memory(u32 address) {
    SpuMemBlock *entry = sound_spu_memory_map;
    SpuMemBlock *prev = NULL;

    for (;;) {
        if (entry->address == address) {
            prev->next = entry->next;
            entry->flags = 0;
            entry->unk1 = 0;
            entry->address = 0;
            entry->next = 0;
            return address;
        }
        prev = entry;
        if (entry->next == 0) {
            return 0;
        }
        entry = &sound_spu_memory_map[entry->next];
    }
}

/* 80039748: Set the second byte of the SPU memory map entry at `address`. */
void sound_set_spu_block_tag(u32 address, u8 value) {
    SpuMemBlock *entry = sound_find_spu_block(address);

    if (entry != NULL) {
        entry->unk1 = value;
    }
}

/* 8003977C: Always 0. */
s32 sound_return_zero(void) {
    return 0;
}

/* 80039784: The index of an unused SPU memory map entry, 0 when all are in use. */
s32 sound_find_free_spu_block(void) {
    s32 i;

    for (i = 0; i < 12; i++) {
        if (sound_spu_memory_map[i].flags == 0) {
            return i;
        }
    }
    return 0;
}

/* 800397C0: The SPU memory map entry at `address`, or NULL. */
SpuMemBlock *sound_find_spu_block(u32 address) {
    SpuMemBlock *entry = sound_spu_memory_map;
    SpuMemBlock *head = entry;

    for (;;) {
        if (entry->address == address) {
            return entry;
        }
        if (entry->next != 0) {
            return NULL;
        }
        entry = head;
    }
}

/* 800397FC: Create a sequence for `header` and play it. Returns the sequence. */
SoundSeq *sound_create_and_play_seq(SoundSeqHeader *header, s32 fade, s32 frames) {
    SoundSeq *seq = sound_create_seq(header);

    sound_play_seq(seq, fade, frames);
    return seq;
}

/* 80039850: Create a sequence for valid sequence data in driver memory (with room
 * for a snapshot when the data has a table). Returns it, or NULL (the
 * data's error, or 0x1E without memory). */
SoundSeq *sound_create_seq(SoundSeqHeader *header) {
    SoundSeqHeader *data = header;
    s16 error = sound_check_seq_header(header);
    s32 size;
    SoundSeq *seq;

    if (error != 0) {
        sound_report_error(error);
        return NULL;
    }
    size = sound_get_seq_size(data->channels);
    if (data->entries != 0) {
        size += 0x180;
    }
    seq = sound_alloc_memory_low(size);
    if (seq == NULL) {
        sound_report_error(0x1E);
        return NULL;
    }
    seq->header = data;
    if (data->entries != 0) {
        sound_load_seq_table(seq, data);
    }
    sound_read_seq_header(seq);
    sound_start_seq_channels(seq);
    seq->muted = 0;
    sound_link_seq(seq);
    return seq;
}

/* 80039910: Create a sequence for valid sequence data in memory the caller provides
 * (flag 0x4000: not released with it). Returns it, or NULL. */
SoundSeq *sound_create_seq_in_place(SoundSeqHeader *header, SoundSeq *seq) {
    s16 error = sound_check_seq_header(header);
    s32 size;

    if (error != 0) {
        sound_report_error(error);
        return NULL;
    }
    size = sound_get_seq_size(header->channels);
    if (header->entries != 0) {
        size += 0x180;
    }
    sound_clear_memory((u32 *)seq, size);
    seq->header = header;
    if (header->entries != 0) {
        sound_load_seq_table(seq, header);
    }
    sound_read_seq_header(seq);
    sound_start_seq_channels(seq);
    seq->muted = 0;
    sound_link_seq(seq);
    seq->flags |= 0x4000;
    return seq;
}

/* 800399D4: Stop and release a sequence (its memory unless the caller provided it). */
void sound_release_seq(SoundSeq *seq) {
    if ((s16)seq->flags & 0x8000) {
        sound_stop_seq(seq);
    }
    if (sound_check_seq_header(seq->header) != 0) {
        sound_report_error(0xA);
        return;
    }
    if (sound_unlink_seq(seq) != 0) {
        sound_report_error(5);
        return;
    }
    sound_free_seq_snapshots(seq);
    if (!(seq->flags & 0x4000)) {
        sound_free_memory(seq);
    }
}

/* 80039A80: Play a sequence from its start, fading in over `frames`. */
void sound_play_seq(SoundSeq *seq, s32 fade, s32 frames) {
    if (seq == NULL) {
        sound_report_error(5);
        return;
    }
    seq->flags &= 0x7FFF;
    if (sound_check_seq_header(seq->header) != 0) {
        sound_report_error(0xA);
        return;
    }
    if ((s16)seq->flags & 0x8000) {
        sound_stop_seq(seq);
    }
    DisableEvent(sound_tick_event);
    sound_read_seq_header(seq);
    sound_start_seq_channels(seq);
    seq->fade.value = 0;
    sound_set_seq_fade(seq, fade, frames);
    seq->flags |= 0x8000;
    EnableEvent(sound_tick_event);
}

/* 80039B68: Restart `seq` from its start: reload every channel's wave bank and
 * sample addresses, mark it stopped by a fade and fade it in. */
void sound_restart_seq(SoundSeq *seq, s32 fade, s32 frames) {
    SoundSeqChannel *channel;
    SoundSequence *bank;
    SoundInstrument *instrument;
    s32 count;
    u32 start;

    if (seq == NULL) {
        sound_report_error(5);
        return;
    }
    count = seq->channels;
    channel = seq->channel;
    do {
        count--;
        bank = sound_find_wave_bank(channel->unk25);
        channel->instruments = bank;
        instrument = &bank->instrument[channel->instrument];
        start = instrument->start * 8;
        channel->state.sample_start = start + bank->address;
        channel->state.sample_loop = start + instrument->loop * 8;
        channel->state.flags = 0xFFFF;
        channel++;
    } while (count != 0);
    seq->fade.value = 0;
    seq->flags |= 0x100;
    sound_set_seq_fade(seq, fade, frames);
}

/* 80039C4C: Stop a sequence: clear its playing flag and release its channels' voices
 * (error 5 without one). */
void sound_stop_seq(SoundSeq *seq) {
    if (seq == NULL) {
        sound_report_error(5);
        return;
    }
    seq->flags &= 0x7FFF;
    sound_release_seq_voices(seq);
}

/* 80039C8C: Fade a sequence out over `frames` frames (error 5 without one). */
void sound_fade_out_seq(s32 seq, s32 frames) {
    if (seq == 0) {
        sound_report_error(5);
        return;
    }
    sound_set_seq_fade((SoundSeq *)seq, 0, frames);
}


/* 80039CC4: Stop every listed sequence flagged 1: clear its playing flag (0x8000)
 * and release its channels' voices, as sound_stop_seq does. */
void sound_stop_all_seqs(void) {
    SoundSeq *seq;

    for (seq = sound_playing_seq_list; seq != NULL; seq = seq->next) {
        if (seq->flags & 1) {
            seq->flags &= 0x7FFF;
            sound_release_seq_voices(seq);
        }
    }
}

/* 80039D24: An empty driver entry. */
void sound_empty_entry_after_seq_stops(void) {
}

/* 80039D2C: Enable or disable (and flush) the sound effect channel. */
void sound_set_effects_enabled(s32 enable) {
    if (enable != 0) {
        sound_driver_flags |= 0x800;
    } else {
        sound_stop_all_effects();
        sound_driver_flags &= ~0x800;
    }
}


/* 80039D78: Set the voice count (even, 4 to 16) unless 0; returns the setting. */
s32 sound_set_effect_voice_count(s32 count) {
    if (count != 0) {
        if (count > 16) {
            count = 16;
        }
        if (count < 4) {
            count = 4;
        }
        sound_effect_voice_count = count & 0xFE;
    }
    return sound_effect_voice_count;
}


/* 80039DB8: Play `effect` (its bank id in the high half) on the last two effect
 * channels with priority 0x80, at volume 0x60 and centre pan, while
 * effects are on (as 80039e18). */
void sound_play_effect_on_last_channels(s32 effect) {
    if (sound_driver_flags & 0x800) {
        sound_channels_per_effect = 2;
        sound_start_effect((sound_effect_channel_count - 2) | 0x8000, effect, 0x6000, 0x4000);
    }
}
