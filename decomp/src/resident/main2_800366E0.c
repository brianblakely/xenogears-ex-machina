/* Resident unit from the console character output (800366E0) to the sound
 * driver (80039E18). Its rodata opens at 0x80018B30 with the formatter's
 * digit strings; see the target yaml for the seam. */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "text.h"
#include "window.h"
#include "pad.h"
#include "console.h"
#include "sound.h"
#include "cd.h"
#include "heap.h"
#include "mode.h"

/* This unit's own variables: those of up to 8 bytes in its .sbss
 * (80059394), the larger format defaults and music file list in its .bss
 * (8005a1cc), as the original assembler placed them (SBSS_main2_800366E0 in
 * slus_006.64.mk). */
static Console *D_80059394;
static RECT D_80059398;   /* the console font CLUTs' VRAM rectangle */
static s32 D_800593A0;    /* the console block is not owned (not released) */
static FormatSpec D_8005A1CC; /* the format defaults */
/* The music file list (func_800379D8): sequence, wave bank and the zero
 * entry ending it, and a fourth entry that nothing addresses (an 8-byte
 * object of its own would be a small variable, in .sbss). */
static FileRequest D_8005A1DC[4];

void func_800370DC(s32 c);

/* The built-in console font, packed: its unpacked size and the LZSS stream
 * 80032e88 decodes (800374e8). */
extern u8 D_80050240[];
INCLUDE_ASSET(".data", D_80050240, 0x80050240, 0x354);
void (*D_80050594)(s32 c) = func_800370DC; /* character output (800366f0) */
/* The console font CLUTs, built by 80036e4c: four rows of 16 colours. */
u16 D_80050598[64] = {
    0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF,
    0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF, 0x0000, 0x7FFF,
    0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
};
s32 D_80050618 = 0; /* the menu's mode (800379b4) */
u8 D_8005061C[6] = {1, 0, 0, 2, 2, 0}; /* option bytes of the field and menu */


/* Install the character output run by 800366f0. */
void func_800366E0(void (*callback)(s32 c)) {
    D_80050594 = callback;
}

/* Output a character through the installed callback. */
void func_800366F0(s32 c) {
    D_80050594(c);
}

/* Format `format` with the word arguments at `args` through the console's
 * character output (800366F0): flags - + space 0, width and precision (or
 * *), conversions b d i u p X x c s n and %%. Returns the characters
 * written; an unknown conversion ends the output. Other characters and
 * %% share one output path. */
s32 func_80036718(s32 target, const char *format, va_list args) {
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
        spec = D_8005A1CC;
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
            func_800366F0(c);
            count++;
            goto next;
        }
        if (length < spec.width && !(spec.u.flags & 1)) {
            do {
                func_800366F0(' ');
                count++;
            } while (length < --spec.width);
        }
        for (i = 0; i < length; i++) {
            func_800366F0(p[i]);
        }
        count += length;
        while (length < spec.width) {
            func_800366F0(' ');
            count++;
            length++;
        }
    next:
        format++;
        c = *format;
    }
    return count;
}

void func_80036CD8(s32 bits) {
    D_80059394->flags |= bits;
}

void func_80036CF8(s32 bits) {
    D_80059394->flags &= ~bits;
}

s16 func_80036D18(void) {
    return D_80059394->flags;
}

void func_80036D30(s32 bits) {
    D_80059394->flags2E |= bits;
}

void func_80036D50(s32 bits) {
    D_80059394->flags2E &= ~bits;
}

s16 func_80036D70(void) {
    return D_80059394->flags2E;
}

void func_80036D88(s16 value) {
    D_80059394->unk14 = value;
}

void func_80036D98(s16 value) {
    D_80059394->unk16 = value;
}

/* Move the console cursor. */
void func_80036DA8(s16 value) {
    D_80059394->x = value;
}

void func_80036DB8(s16 value) {
    D_80059394->y = value;
}

/* Set the console text colour; any channel below 0x80 clears bright mode. */
void func_80036DC8(s32 r, s32 g, s32 b) {
    D_80059394->r = r;
    D_80059394->g = g;
    D_80059394->b = b;
    if (r < 0x80 || g < 0x80 || b < 0x80) {
        D_80059394->mode &= ~1;
    } else {
        D_80059394->mode |= 1;
    }
}

/* Build and upload the console font CLUTs: four 16-color rows of
 * foreground/background stripes 1, 2, 4 and 8 entries wide. */
void func_80036E4C(u16 foreground, u16 background) {
    u16 *p = D_80050598;
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
    LoadImage(&D_80059398, (u_long *)D_80050598);
}

s16 func_80036F44(void) {
    return D_80059394->unk14;
}

s16 func_80036F5C(void) {
    return D_80059394->unk16;
}

s16 func_80036F74(void) {
    return D_80059394->x;
}

s16 func_80036F8C(void) {
    return D_80059394->y;
}

s16 func_80036FA4(void) {
    return D_80059394->unk34;
}

/* Save the console cursor. */
void func_80036FBC(void) {
    D_80059394->saved_x = D_80059394->x;
    D_80059394->saved_y = D_80059394->y;
    D_80059394->saved_36 = D_80059394->unk36;
}

/* Restore the saved console cursor. */
void func_80036FE4(void) {
    D_80059394->x = D_80059394->saved_x;
    D_80059394->y = D_80059394->saved_y;
    D_80059394->unk36 = D_80059394->saved_36;
}

/* printf to the console, when there is one. */
void func_8003700C(char *format, ...) {
    va_list args;

    if (D_80059394 != NULL) {
        va_start(args, format);
        func_80036718(0, format, args);
    }
}

/* Place the console cursor relative to its origin. */
void func_80037058(s32 x, s32 y) {
    if (D_80059394 != NULL) {
        D_80059394->x = D_80059394->left + x;
        D_80059394->y = D_80059394->top + y;
    }
}

/* Place the console cursor and line start relative to the origin. */
void func_8003708C(s32 x, s32 y) {
    if (D_80059394 != NULL) {
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        D_80059394->unk36 = D_80059394->x = D_80059394->left + x;
        D_80059394->y = D_80059394->top + y;
    }
}

/* Put a character on the console: a font sprite for printable characters
 * (wrapping, or stopping, at the right edge) and newlines; nothing once the
 * window or the sprite budget is full. */
void func_800370DC(s32 c) {
    Console *con = D_80059394;
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

/* Home the console cursor and select the active text buffer. Capture the
 * window coordinates, cleared mode byte and buffer before writing the cursor. */
void func_800372CC(void) {
    Console *console = D_80059394;
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

/* Flush this frame's console sprites (and texture page, and background
 * tile) into `ot`, or into the console's own ordering table, drawn at once,
 * when `ot` is NULL or -1; then flip the sprite buffers and home the cursor. */
void func_80037324(u_long *ot) {
    Console *con = D_80059394;
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
            func_800317E0(ot, sprite++);
            n--;
        }
        AddPrim(ot, &con->tpage[index]);
        if (con->flags & 0x10) {
            func_80031804(ot, &con->tile[index]);
        }
        func_800372CC();
        if ((s16)own) {
            DrawOTag(ot);
        }
    }
}

void func_8003747C(s32 value) {
    D_800593A0 = value;
}

/* Close the console: restore the default report output and release the
 * console block unless it is not owned. */
void func_8003748C(void) {
    if (D_80059394 != NULL) {
        D_800592B8 = (void (*)(char *))func_8003700C;
        if (D_800593A0 == 0) {
            func_800320E8(D_80059394);
        }
        D_80059394 = NULL;
    }
    D_800593A0 = 0;
}

/* Open the debug text console at (left, top, width, height) with room for
 * `capacity` characters per frame: allocate it (unless a block was
 * supplied through 8003747C), load the font (packed; the built-in one when
 * `font` is NULL) to VRAM at (tex_x, tex_y) with its four CLUTs at
 * (clut_x, clut_y), and make it the report output. */
Console *func_800374E8(s32 left, s32 top, s32 width, s32 height, s32 capacity, u32 flags,
                       s32 tex_x, s32 tex_y, s32 clut_x, s32 clut_y, void *font) {
    Console *console;
    u8 *data;
    s16 lower;
    s16 wide;
    s32 rows;
    u8 *p;             /* the console block, later the font pixels */
    RECT rect;
    s32 size;

    if (D_800593A0 != 0) {
        p = (u8 *)D_800593A0;
    } else {
        func_800324B8(0x32);
        if (!(flags & 1)) {
            size = capacity * 32 + sizeof(Console);
        } else {
            size = capacity * 16;
            size += sizeof(Console);
        }
        p = func_80031BDC(size, ((flags >> 2) ^ 1) & 1);
    }
    console = (Console *)p;
    console->buffer[0] = (u8 *)(console + 1);
    if (flags & 1) {
        console->buffer[1] = console->buffer[0];
    } else {
        console->buffer[1] = console->buffer[0] + capacity * 16;
    }
    if (font == NULL) {
        font = D_80050240;
    }
    data = func_80032E88(font, 0);
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
    D_80059398 = rect;
    func_80036E4C(0x7FFF, 0);
    SetDrawTPage(&console->tpage[0], 0, 0, console->tpage_id);
    SetDrawTPage(&console->tpage[1], 0, 0, console->tpage_id);
    setTile(&console->tile[0]);
    setRGB0(&console->tile[0], 0, 0, 0);
    *(u32 *)&console->tile[0].x0 = left | (top << 16);
    *(u32 *)&console->tile[0].w = width | (height << 16);
    setSemiTrans(&console->tile[0], 1);
    console->tile[1] = console->tile[0];
    D_80059394 = console;
    func_800372CC();
    func_800320E8(data);
    return console;
}

/* Sort `count` elements of `size` bytes at `base` in place (selection
 * sort): `compare` is positive when its second element goes first. */
void func_80037878(u8 *base, s32 count, s32 size, s32 (*compare)(void *a, void *b)) {
    u8 *temp = func_80031BDC(size, 0);
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
    func_800320E8(temp);
}

INCLUDE_ASM("decomp/src/resident", func_800379B4);

INCLUDE_ASM("decomp/src/resident", func_800379C8);

void func_800379D0(void) {
}

extern u8 *D_800658C8;            /* the loaded music's instrument data */

/* Load the music of `scene` from directory 12/3: its sequence (file
 * 6 + 2 * scene) and wave bank (file 7 + 2 * scene + `variant`). Returns 0
 * with the sequence, 0 and the bank data after its first word, or -1 with
 * zeros when the directory has no such scene. */
s32 func_800379D8(s32 scene, s32 variant, u8 **sequence, s32 *unused, u8 **bank) {
    s32 group;
    s32 index;
    s32 result = 0;
    u8 *samples;
    u8 *data;
    s32 file;
    s32 base;

    func_800284B4(&group, &index);
    func_80028470(12, 3);
    func_80032498(4, 0);
    if (scene >= func_80028928(5) / 2) {
        result = -1;
        *sequence = NULL;
        *unused = 0;
        *bank = NULL;
    } else {
        base = scene * 2;
        samples = func_80031BDC(func_800288EC(base + 7 + variant), 1);
        func_800320A4(samples);
        file = base + 6;
        data = func_80031BDC(func_800288EC(file), 1);
        func_800320A4(data);
        D_8005A1DC[0].file = file;
        D_8005A1DC[0].destination = data;
        base += variant + 7;
        D_8005A1DC[1].file = base;
        D_8005A1DC[1].destination = samples;
        D_8005A1DC[2].file = 0;
        D_8005A1DC[2].destination = NULL;
        func_80029AFC(D_8005A1DC, 0, 0);
        *sequence = data;
        *unused = 0;
        *bank = (u8 *)D_8005A1DC[1].destination + 4;
        D_800658C8 = (u8 *)D_8005A1DC[1].destination + 4;
    }
    func_80028470(group, index);
    return result;
}

/* A voice whose volume pair follows the output mode (D_80059518). */
typedef struct SoundModeVoice {
    u16 flags;         /* bit 0: in use */
    u8 unk2[0x10];
    u16 volume;
    u8 unk14[0x22];
    s16 unk36;
    s16 left;
    s16 right;
    u8 unk3C[0x26];
    s16 unk62;
    s16 unk64;
    s16 unk66;
} SoundModeVoice;

extern SoundModeVoice *D_80059518;
extern SpuVolume D_8005940C;                   /* reverb depth */
extern SoundTrack *D_80059564;
s32 func_80038824(void);
void func_8003885C(s32 volume);
void func_80038DF4(void);

extern void *D_80059458;     /* the SPU transfer ring */
extern s32 D_800594E4;       /* random state */
extern SoundSeq *D_800595D8; /* the sound effect channels */
extern u32 D_800594FC;       /* voices held */
extern u32 D_80059550;       /* voices to key off */
extern u32 D_80059554;       /* voices whose registers changed */
extern u32 D_80059504;       /* effect start clock */
extern s32 D_80059514;
extern u8 D_80065B0C[0x6300]; /* the driver memory pool */
extern u8 D_8006FAC8[];      /* the SPU memory management table */
extern u32 D_800594D8;       /* SPU address of the reverb work area, -1 none */
extern s32 D_800595A4;       /* the zeroed transfer buffer */
s32 func_8003C020(void);    /* the driver tick */
void func_8003BB64(void);    /* SPU transfer callback */
void func_8003BFA0(void);    /* SPU interrupt callback */
void func_8003E700(void);
SoundSeq *func_8003B148(s32 count);
void func_80038EC0(u32 start, s32 size);
void func_80039360(void);
void func_800386C4(s32 mode);
void func_80038DB4(s32 reverb, s32 mix);
void func_80038C68(s32 volume, s32 frames);
void func_80038D18(s32 volume, s32 frames);
void func_8003885C(s32 volume);
void func_80038934(s32 type, s32 depth, s32 delay, s32 feedback);

/* Start the sound driver (error 0x28 when it runs): memory pools, the SPU
 * memory map and transfer ring, the tick event on root counter 2 and the
 * SPU callbacks, then default volumes, output mode, effect channels and
 * reverb. */
void func_80037B88(s32 flags) {
    if (D_8005957C < 0) {
        func_8003F6B0(0x28);
        return;
    }
    D_8005957C = flags | 0xB801;
    SpuInitMalloc(4, D_8006FAC8);
    func_80038EC0((u32)D_80065B0C, 0x6300);
    func_80039360();
    D_80059458 = func_80038F18(0xA0);
    func_8003E700();
    D_800594E4 = 0x12345678;
    D_80059564 = NULL;
    D_800595D8 = NULL;
    D_80059440 = NULL;
    D_80059558 = NULL;
    D_80059518 = NULL;
    D_800594FC = 0;
    D_80059550 = 0;
    D_80059554 = 0;
    D_8005A3C0.attr.mvolmode.left = 0;
    D_8005A3C0.attr.mvolmode.right = 0;
    D_8005A3C0.attr.mask = 0xC;
    EnterCriticalSection();
    D_800595BC = OpenEvent(0xF2000002, 2, 0x1000, (long (*)())func_8003C020);
    SetRCnt(0xF2000002, 0x44E8, 0x1000);
    StartRCnt(0xF2000002);
    SpuSetTransferCallback(func_8003BB64);
    SpuSetIRQCallback(func_8003BFA0);
    SpuSetIRQ(0);
    D_80059504 = 0;
    D_80059514 = 0;
    ExitCriticalSection();
    func_800395B8(0x2000, 0x10000, 4);
    func_800386C4(1);
    func_80038DB4(0, 1);
    func_80038C68(0x3FFF, 0);
    func_80038D18(0x7FFF, 0);
    if (D_8005957C & 0x4000) {
        func_8003885C(0x80);
    }
    D_800595D8 = func_8003B148(0x10);
    D_80059544 = 8;
    D_800594D8 = -1;
    D_800595A4 = 0;
    D_80059408.type = 0xFF;
    func_80038934(4, 0, 0, 0);
    SpuSetReverb(1);
    D_80059500 = 0;
}

/* Shut the sound driver down: remove its SPU interrupt, transfer callback,
 * timer and event, release every voice and clear the reverb (error 0x29
 * when it is not running). */
void func_80037DC0(void) {
    s32 i;

    if (D_8005957C == 0) {
        func_8003F6B0(0x29);
        return;
    }
    EnterCriticalSection();
    D_8005957C = 0;
    SpuSetIRQ(0);
    SpuSetTransferCallback(NULL);
    SpuSetIRQCallback(NULL);
    StopRCnt(0xF2000002);
    CloseEvent(D_800595BC);
    ExitCriticalSection();
    for (i = 0; i < 24; i++) {
        func_8003F5BC(i, 6, 3);
    }
    func_8003F484(0xFFFFFF);
    SpuSetReverbModeDepth(0, 0);
    SpuSetReverbModeType(0);
    D_80059500 = 0;
}

/* Mark every voice's channel for a full register update and clear the
 * voices-silenced state. */
void func_80037E8C(void) {
    SoundChannel **channel = D_8006252C;
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
    D_8005957C &= ~0x40;
}

/* Silence every SPU voice, preserving the low ADSR1 byte. */
void func_80037EE4(void) {
    SpuVoice *voice = D_800508E4->voice;
    s32 i;

    D_8005957C |= 0x40;
    for (i = 0; i < 24; i++) {
        voice->volume_left = 0;
        voice->volume_right = 0;
        voice->pitch = 0;
        voice->adsr1 = (voice->adsr1 & 0xFF) + 0x7F00;
        voice->adsr2 = 0x1FDF;
        voice++;
    }
}


/* Enable the driver's tick event once. */
void func_80037F44(void) {
    if (!(D_8005957C & 1)) {
        D_8005957C |= 1;
        EnableEvent(D_800595BC);
    }
}

void func_80037F88(void) {
    if (D_8005957C & 1) {
        DisableEvent(D_800595BC);
        D_8005957C &= ~1;
    }
}

SoundSequence *func_800383EC(s32 key);
void func_80038264(s32 address, s32 size);
s32 func_8003827C(u8 *data, s32 size);

/* Load a wave bank: allocate SPU memory for its samples (800381F4),
 * transfer them and add a copy of its header to the loaded banks. Returns
 * the copy, or NULL (error 0x1F without SPU memory, 0x1E without memory
 * for the copy). */
SoundSequence *func_80037FD8(SoundSequence *bank, s32 mode) {
    s32 address = func_800381F4(bank, mode);
    SoundSequence *copy;
    SoundSequence **link;

    if (address == 0) {
        func_8003F6B0(0x1F);
        return NULL;
    }
    func_8003BC10(address, (u8 *)bank + bank->offset, bank->size, NULL);
    copy = func_80039024(bank->header_size);
    if (copy == NULL) {
        func_800396E0(address);
        func_8003F6B0(0x1E);
        return NULL;
    }
    func_80039248(copy, bank, bank->header_size);
    copy->address = address;
    DisableEvent(D_800595BC);
    link = &D_80059558;
    if (D_80059558 != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = copy;
    copy->next = NULL;
    EnableEvent(D_800595BC);
    return copy;
}

/* Start loading a wave bank whose samples arrive in parts: allocate its SPU
 * memory, transfer the samples among the first `size` bytes of the file
 * (8003827C takes the rest) and add a copy of its header to the loaded
 * banks. Returns the copy, or NULL (error 0x16 when a bank with its key is
 * loaded, 0x1F without SPU memory, 0x1E without memory for the copy). */
SoundSequence *func_800380D0(SoundSequence *bank, s32 size, s32 mode) {
    s32 address;
    SoundSequence *copy;
    SoundSequence **link;

    if (func_800383EC(bank->key) != NULL) {
        func_8003F6B0(0x16);
        return NULL;
    }
    address = func_800381F4(bank, mode);
    if (address == 0) {
        func_8003F6B0(0x1F);
        return NULL;
    }
    func_80038264(address, bank->size);
    func_8003827C((u8 *)bank + bank->offset, size - bank->header_size);
    copy = func_80039024(bank->header_size);
    if (copy == NULL) {
        func_800396E0(address);
        func_8003F6B0(0x1E);
        return NULL;
    }
    func_80039248(copy, bank, bank->header_size);
    copy->address = address;
    DisableEvent(D_800595BC);
    link = &D_80059558;
    if (D_80059558 != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = copy;
    copy->next = NULL;
    EnableEvent(D_800595BC);
    return copy;
}

/* Allocate SPU memory for a wave bank's samples: anywhere for mode -1, or
 * for mode 0 when the bank asks for no address; otherwise at the bank's
 * address. Returns the address, 0 when there is no room. */
s32 func_800381F4(SoundSequence *bank, s32 mode) {
    if (mode == 0) {
        mode = bank->address;
    } else if (mode == -1) {
        mode = 0;
    }
    /* The allocator's result is returned as the value it leaves; the
     * function has no return expression. */
    if (mode == 0) {
        func_800393B8(bank->size, bank->volume);
        return;
    }
    func_800395B8(bank->size, bank->address, bank->volume);
}

extern s32 D_80059584;
extern s32 D_80059588;

void func_80038264(s32 a, s32 b) {
    D_80059584 = a;
    D_80059588 = b;
}

/* Transfer the next part of a streamed wave bank's samples (at most what
 * is still missing). Returns the bytes still missing. */
s32 func_8003827C(u8 *data, s32 size) {
    s32 left = D_80059588;
    s32 address;

    if (left == 0) {
        return 0;
    }
    size = left < size ? left : size;
    address = D_80059584;
    func_8003BC10(address, data, size, NULL);
    D_80059584 = (u32)address + (u32)size;
    return D_80059588 = (u32)left - (u32)size;
}


/* Release a loaded wave bank: unlink it, free its SPU memory (error 0x24
 * when that fails, 0x11 when the bank is not loaded) and its copy. */
void func_80038310(SoundSequence *bank) {
    SoundSequence *entry;
    SoundSequence *prev = NULL;

    for (entry = D_80059558; entry != NULL; entry = entry->next) {
        if (entry == bank) {
            break;
        }
        prev = entry;
    }
    if (entry == NULL) {
        func_8003F6B0(0x11);
        return;
    }
    DisableEvent(D_800595BC);
    if (prev != NULL) {
        prev->next = bank->next;
    } else {
        D_80059558 = bank->next;
    }
    EnableEvent(D_800595BC);
    if (bank->address != func_800396E0(bank->address)) {
        func_8003F6B0(0x24);
    }
    func_80039144(bank);
}

/* The playing sequence with `key`, or NULL. */
SoundSequence *func_800383EC(s32 key) {
    SoundSequence *sequence;

    for (sequence = D_80059558; sequence != NULL; sequence = sequence->next) {
        if (sequence->key == key) {
            break;
        }
    }
    return sequence;
}

/* Add a sound effect bank to the loaded banks: error 0x15 when a bank
 * with its id is loaded (unless the driver is in its error state), or the
 * bank data's error. */
void func_80038428(SoundBank *bank) {
    SoundBank *added = bank;
    SoundBank *entry;
    SoundBank **link;
    s16 error;

    if (!(D_8005957C & 0x80)) {
        for (entry = D_80059440; entry != NULL; entry = entry->next) {
            if (bank->id == entry->id) {
                func_8003F6B0(0x15);
                return;
            }
        }
    }
    error = func_8003F614((u32 *)bank, 0x73646573, 0x101);
    if (error != 0) {
        func_8003F6B0(error);
        return;
    }
    DisableEvent(D_800595BC);
    link = &D_80059440;
    if (D_80059440 != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = added;
    added->next = NULL;
    EnableEvent(D_800595BC);
}

/* Remove a sound effect bank from the loaded banks (error 0x10 when it is
 * not loaded, 0xB when its data is no longer valid). */
void func_8003852C(SoundBank *bank) {
    SoundBank *entry;
    SoundBank *prev = NULL;
    SoundBank *target = bank;
    s16 error;

    for (entry = D_80059440; entry != NULL; entry = entry->next) {
        if (entry == target) {
            break;
        }
        prev = entry;
    }
    if (entry == NULL) {
        func_8003F6B0(0x10);
        return;
    }
    func_8003A094(bank);
    DisableEvent(D_800595BC);
    if (prev != NULL) {
        prev->next = target->next;
    } else {
        D_80059440 = target->next;
    }
    target->next = NULL;
    error = func_8003F614((u32 *)bank, 0x73646573, 0x101);
    if (error != 0) {
        func_8003F6B0(0xB);
        return;
    }
    EnableEvent(D_800595BC);
}

void func_80038624(void) {
    func_80039FF8();
    D_80059440 = NULL;
}

/* The loaded bank with the id of `bank` (or `id` when NULL). */
SoundBank *func_8003864C(SoundBank *bank, s16 id) {
    SoundBank *entry;

    if (bank != NULL) {
        id = bank->id;
    }
    for (entry = D_80059440; entry != NULL; entry = entry->next) {
        if (id == entry->id) {
            break;
        }
    }
    return entry;
}

void func_8003869C(void) {
    func_80039CC4();
    func_80039FF8();
}


/* Select the output mode (1, 2 or 3; otherwise the plain one) and reapply
 * the volumes: master and CD, reverb depth, every sequence, the CD mix and
 * the mode voice. */
void func_800386C4(s32 mode) {
    SoundTrack *track;
    SoundModeVoice *voice;
    s16 volume;

    D_8005957C &= 0xF8FF;
    switch (mode) {
    case 0:
        break;
    case 1:
        D_8005957C |= 0x100;
        break;
    case 2:
        D_8005957C |= 0x300;
        break;
    case 3:
        D_8005957C |= 0x500;
        break;
    }
    func_80038DF4();
    SpuSetReverbModeDepth(D_8005940C.left, D_8005940C.right);
    for (track = D_80059564; track != NULL; track = track->next) {
        func_8003E680(0x100, (SoundSeq *)track);
    }
    if (D_8005957C & 0x4000) {
        func_8003885C(D_8005A3C0.cd_request);
    }
    voice = D_80059518;
    if (voice != NULL && (voice->flags & 1)) {
        volume = voice->volume;
        if (func_80038824() != 0) {
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

/* 0 without reverb, 1 or 2 by reverb mode. */
s32 func_80038824(void) {
    s32 mode;

    if (D_8005957C & 0x700) {
        mode = 1;
        if (D_8005957C & 0x600) {
            mode = 2;
        }
    } else {
        mode = 0;
    }
    return mode;
}

/* These fixed, contiguous attenuation bytes form CdMix's CdlATV argument
 * (one object; the names of its last three are in link.ld). */
extern u8 D_80059530, D_80059531, D_80059532, D_80059533;

/* Remember CD volume; Mono halves it into both channels, while stereo
 * modes keep the same-channel volume and clear the cross channels. */
void func_8003885C(s32 volume) {
    u16 flags = D_8005957C;
    s32 same;

    D_8005A3C0.cd_request = volume;
    if (flags & 0x700) {
        same = volume;
        volume = 0;
    } else {
        volume >>= 1;
        same = volume;
    }
    D_80059530 = D_80059532 = same;
    D_80059531 = D_80059533 = volume;
    CdMix((CdlATV *)&D_80059530);
}

void func_800388D4(s32 enable) {
    if (enable != 0) {
        D_8005957C |= 0x1000;
    } else {
        D_8005957C &= ~0x1000;
    }
}

void func_8003890C(SoundBank *bank, s32 enable) {
    if (enable != 0) {
        bank->flags &= ~1;
    } else {
        bank->flags |= 1;
    }
}

extern s32 D_800508E8[10]; /* reverb work area size of each reverb type (sound.c) */
void func_80038AD4(s32 address, s32 size);

/* Compiled-out debug trace of the reverb work area allocation. */
#define REVERB_TRACE_ALLOC(address) do { } while (0)

/* Set the reverb type, depth, delay and feedback: type 0 turns it off, -1
 * keeps the current type and -2 changes nothing. A new type moves the work
 * area to the top of SPU memory and clears it (error 0x20, reverb off,
 * when there is no room). */
void func_80038934(s32 type, s32 depth, s32 delay, s32 feedback) {
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
        type = D_80059408.type;
    }
    SpuGetReverbModeType(&current);
    if (current != type || type == 0) {
        if (D_800594D8 != -1) {
            func_800396E0(D_800594D8);
        }
        size = D_800508E8[type];
        address = 0x80000 - size;
        D_800594D8 = func_800395B8(size, address, 5);
        REVERB_TRACE_ALLOC(D_800594D8);
        changed = 1;
        if (D_800594D8 == 0) {
            func_8003F6B0(0x20);
            type = 0;
            feedback = 0;
            delay = 0;
            depth = 0;
        }
    }
    D_80059408.type = type;
    D_8005A3C0.unk2C = depth;
    D_80059408.delay = delay;
    D_80059408.feedback = feedback;
    func_80038DF4();
    if (changed) {
        SpuSetReverbModeDepth(0, 0);
        SpuSetReverbModeType(type);
        func_80038AD4(address, size);
    } else {
        SpuSetReverbModeDepth(D_8005940C.left, D_8005940C.right);
        SpuSetReverbModeDelayTime(delay);
        SpuSetReverbModeFeedback(feedback);
    }
}

extern s32 D_800595DC; /* next SPU address to clear */
extern s32 D_800595E0; /* bytes left to clear */

/* Clear `size` bytes of SPU memory at `address` (the reverb work area)
 * from a zeroed buffer, allocated on first use, in chained transfers. */
void func_80038AD4(s32 address, s32 size) {
    D_800595DC = address;
    D_800595E0 = size;
    if (D_800595A4 == 0) {
        D_800595A4 = (s32)func_80038F18(0x840);
        if (D_800595A4 == 0) {
            func_8003F6B0(0x1E);
        }
    }
    D_8005957C |= 0x20;
    func_80038B4C();
}


/* Clear the next part (at most 0x840 bytes, else 0x800) of the reverb work
 * area, chaining itself as the transfer callback; when done, release the
 * buffer and restore the reverb settings. */
void func_80038B4C(void) {
    s32 size;
    s32 address;

    if (D_800595E0 == 0) {
        func_80039144((void *)D_800595A4);
        D_800595A4 = 0;
        SpuSetReverbModeDepth(D_8005940C.left, D_8005940C.right);
        SpuSetReverbModeDelayTime(D_80059408.delay);
        SpuSetReverbModeFeedback(D_80059408.feedback);
        D_8005957C &= ~0x20;
        return;
    }
    size = 0x800;
    if (D_800595E0 <= 0x840) {
        size = D_800595E0;
    }
    address = D_800595DC;
    D_800595E0 -= size;
    D_800595DC = address + size;
    func_8003BC10(address, (u8 *)D_800595A4, size, func_80038B4C);
    if (!(D_8005957C & 0x10)) {
        func_8003BC10(address, (u8 *)D_800595A4, size, NULL);
    }
}

/* Set the master volume at once, or fade to it over `frames`. */
void func_80038C68(s32 volume, s32 frames) {
    s32 delta;

    D_8005A3C0.master_slide.target = volume;
    if (frames == 0) {
        D_8005A3C0.master_slide.value.value = volume << 16;
        D_8005A3C0.master_slide.frames = 0;
        D_8005A3C0.master = volume;
        func_80038E6C((s16)volume, &D_8005A3C0.attr.mvol, 0);
        D_8005A3C0.attr.mask |= 3;
        return;
    }
    delta = (volume << 8) - (D_8005A3C0.master_slide.value.value >> 8);
    if (delta != 0) {
        D_8005A3C0.master_slide.frames = frames;
        D_8005A3C0.master_slide.step = (delta / frames) << 8;
    }
}

/* Set the CD volume at once, or fade to it over `frames`. */
void func_80038D18(s32 volume, s32 frames) {
    s32 delta;

    D_8005A3C0.cd_slide.target = volume;
    if (frames == 0) {
        D_8005A3C0.cd_slide.value.value = volume << 16;
        D_8005A3C0.cd_slide.frames = 0;
        D_8005A3C0.attr.cd.volume.left = D_8005A3C0.attr.cd.volume.right = D_8005A3C0.cd = volume;
        D_8005A3C0.attr.mask |= 0xC0;
        return;
    }
    delta = (volume << 8) - (D_8005A3C0.cd_slide.value.value >> 8);
    if (delta != 0) {
        D_8005A3C0.cd_slide.frames = frames;
        D_8005A3C0.cd_slide.step = (delta / frames) << 8;
    }
}


/* Set the CD audio reverb and mix switches. */
void func_80038DB4(s32 reverb, s32 mix) {
    D_8005A3C0.attr.cd.reverb = reverb;
    D_8005A3C0.attr.cd.mix = mix;
    D_8005A3C0.attr.mask |= 0x300;
    SpuSetCommonAttr(&D_8005A3C0.attr);
}

/* Apply the master and CD volumes. */
void func_80038DF4(void) {
    func_80038E6C(D_8005A3C0.master, &D_8005A3C0.attr.mvol, 0);
    D_8005A3C0.attr.cd.volume.left = D_8005A3C0.attr.cd.volume.right = D_8005A3C0.cd;
    func_80038E6C(D_8005A3C0.unk2C, &D_8005940C, 1);
    D_8005A3C0.attr.mask |= 0xC3;
}

/* Set a stereo volume pair from the low volume halfword, inverting one
 * side for the surround modes. The channel selector is an unsigned byte. */
void func_80038E6C(s32 volume, SpuVolume *out, s32 channel) {
    u16 flags = D_8005957C;

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

extern s32 D_8005951C;

/* Make [start, start + size) the driver memory pool, aligned to 16 bytes. */
void func_80038EC0(u32 start, s32 size) {
    SoundBlock *block;

    size &= ~0xF;
    if (start & 0xF) {
        size -= 0x10;
        start = (start + 0xF) & ~0xF;
    }
    D_800595E4 = start + size;
    block = (SoundBlock *)start;
    block->flags = 0x8000;
    D_80059410 = block;
    D_8005951C = size;
    block->unk2 = 0;
    block->unk4 = 0;
    block->end = start + 0x10;
    block->next = NULL;
}

/* Allocate `size` bytes of driver memory, cleared, from the first gap that
 * fits (between blocks or after the last one). Returns the data or NULL
 * (then the driver event stays disabled). */
void *func_80038F18(s32 size) {
    SoundBlock *block;
    SoundBlock *new;
    u32 need;
    u32 limit;
    u8 *data;

    DisableEvent(D_800595BC);
    need = ((size + 0xF) & ~0xF) + 0x10;
    for (block = D_80059410; block->next != NULL; block = block->next) {
        limit = (u32)block->next;
        if (limit - block->end >= need) {
            goto found;
        }
    }
    limit = D_800595E4;
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
        EnableEvent(D_800595BC);
        func_800392EC((u32 *)data, size);
        return data;
    }
    return NULL;
}

/* Allocate `size` bytes of driver memory, cleared, from the highest gap
 * that fits (between blocks or after the last one). Returns the data or
 * NULL (then the driver event stays disabled). */
void *func_80039024(s32 size) {
    s32 need;
    SoundBlock *entry;
    SoundBlock *after;
    u32 limit;
    SoundBlock *block;
    u8 *data;

    DisableEvent(D_800595BC);
    need = ((size + 0xF) & ~0xF) + 0x10;
    after = NULL;
    limit = 0;
    for (entry = D_80059410;; entry = entry->next) {
        if (entry->next == NULL) {
            if ((s32)(D_800595E4 - entry->end) >= need) {
                after = entry;
                limit = D_800595E4;
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
    EnableEvent(D_800595BC);
    func_800392EC((u32 *)data, size);
    return data;
}

/* Release a block of driver memory: unlink it from the pool list (the head
 * is never released), with the driver's event disabled. The head variable
 * is reused for the predecessor test; that keeps the pool head in its own
 * register through the first comparison and the walk in a copy of it, as in
 * the original. */
void func_80039144(void *data) {
    SoundBlock *head = D_80059410;
    SoundBlock *block;
    SoundBlock *entry;
    SoundBlock *prev;

    DisableEvent(D_800595BC);
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
    EnableEvent(D_800595BC);
}

/* The largest free gap of the driver memory pool, in 16-byte units. */
s32 func_800391CC(void) {
    SoundBlock *block = D_80059410;
    s32 largest = 0;
    s32 gap;

    while (block->next != NULL) {
        gap = (u32)block->next - block->end;
        if (largest < gap) {
            largest = gap;
        }
        block = block->next;
    }
    gap = D_800595E4 - block->end;
    if (largest < gap) {
        largest = gap;
    }
    return largest & ~0xF;
}

/* Copy `size` bytes: sixteen at a time, then words, then bytes. */
void func_80039248(void *dst, void *src, s32 size) {
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
/* Clear `size` bytes: sixteen at a time, then words, then bytes. */
void func_800392EC(void *data, s32 size) {
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

/* Reset the SPU memory map to one reserved entry for the first 0x1010
 * bytes. */
void func_80039360(void) {
    s32 i;

    for (i = 11; i >= 0; i--) {
        D_8006F9FC[i].flags = 0;
    }
    D_8006F9FC[0].flags = 0x81;
    D_8006F9FC[0].unk1 = 5;
    D_8006F9FC[0].address = 0;
    D_8006F9FC[0].size = 0x1010;
    D_8006F9FC[0].next = 0;
}

/* Allocate `size` bytes of SPU memory in the first gap of the map that
 * fits (or after its last entry). Returns the address, 0 when none. */
s32 func_800393B8(s32 size, u16 mode) {
    SpuMemBlock *entry = D_8006F9FC;
    SpuMemBlock *block;
    u32 end = D_8006F9FC[0].address + D_8006F9FC[0].size;
    s32 i;

    if (entry->next != 0) {
    scan_gap:
        block = &D_8006F9FC[entry->next];
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
    i = func_80039784();
    if (i < 0) {
        return 0;
    }
    block = &D_8006F9FC[i];
    block->flags = 0x80;
    block->unk1 = 0;
    block->address = end;
    block->size = size;
    block->next = entry->next;
    entry->next = i;
    return end;
}

/* Allocate `size` bytes of SPU memory at the top of the last gap of the
 * map that fits (the space after the last entry included). The new entry
 * is linked after the map's last entry. Returns the address, 0 when none. */
s32 func_800394B8(s32 size) {
    SpuMemBlock *entry = D_8006F9FC;
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
        block = &D_8006F9FC[entry->next];
        if ((s32)(block->address - end) >= size) {
            found = entry;
            address = block->address - size;
        }
        entry = block;
    }
    if (found == NULL) {
        return 0;
    }
    i = func_80039784();
    if (i < 0) {
        return 0;
    }
    block = &D_8006F9FC[i];
    block->flags = 0x80;
    block->unk1 = 0;
    block->address = address;
    block->size = size;
    block->next = entry->next;
    entry->next = i;
    return address;
}

/* Reserve `size` bytes of SPU memory at `address` when the map leaves
 * that range free, linking the new entry after the one before it.
 * Returns the address, 0 when the range is taken or no entry is free. */
s32 func_800395B8(s32 size, s32 address, u16 mode) {
    SpuMemBlock *entry = D_8006F9FC;
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
        block = &D_8006F9FC[entry->next];
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
    i = func_80039784();
    if (i < 0) {
        return 0;
    }
    block = &D_8006F9FC[i];
    block->flags = 0x80;
    block->unk1 = 0;
    block->address = address;
    block->size = size;
    block->next = entry->next;
    entry->next = i;
    return address;
}

/* Release the SPU memory map entry at `address`, unlinking it. Returns the
 * address, 0 when no entry has it. */
u32 func_800396E0(u32 address) {
    SpuMemBlock *entry = D_8006F9FC;
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
        entry = &D_8006F9FC[entry->next];
    }
}

/* Set the second byte of the SPU memory map entry at `address`. */
void func_80039748(u32 address, u8 value) {
    SpuMemBlock *entry = func_800397C0(address);

    if (entry != NULL) {
        entry->unk1 = value;
    }
}

s32 func_8003977C(void) {
    return 0;
}

/* The index of an unused SPU memory map entry, 0 when all are in use. */
s32 func_80039784(void) {
    s32 i;

    for (i = 0; i < 12; i++) {
        if (D_8006F9FC[i].flags == 0) {
            return i;
        }
    }
    return 0;
}

/* The SPU memory map entry at `address`, or NULL. */
SpuMemBlock *func_800397C0(u32 address) {
    SpuMemBlock *entry = D_8006F9FC;
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

/* Create a sequence for `header` and play it. Returns the sequence. */
SoundSeq *func_800397FC(SoundSeqHeader *header, s32 fade, s32 frames) {
    SoundSeq *seq = func_80039850(header);

    func_80039A80(seq, fade, frames);
    return seq;
}

/* Create a sequence for valid sequence data in driver memory (with room
 * for a snapshot when the data has a table). Returns it, or NULL (the
 * data's error, or 0x1E without memory). */
SoundSeq *func_80039850(SoundSeqHeader *header) {
    SoundSeqHeader *data = header;
    s16 error = func_8003F67C(header);
    s32 size;
    SoundSeq *seq;

    if (error != 0) {
        func_8003F6B0(error);
        return NULL;
    }
    size = func_8003BB40(data->channels);
    if (data->entries != 0) {
        size += 0x180;
    }
    seq = func_80038F18(size);
    if (seq == NULL) {
        func_8003F6B0(0x1E);
        return NULL;
    }
    seq->header = data;
    if (data->entries != 0) {
        func_8003B0AC(seq, data);
    }
    func_8003B22C(seq);
    func_8003B424(seq);
    seq->muted = 0;
    func_8003B9E4(seq);
    return seq;
}

/* Create a sequence for valid sequence data in memory the caller provides
 * (flag 0x4000: not released with it). Returns it, or NULL. */
SoundSeq *func_80039910(SoundSeqHeader *header, SoundSeq *seq) {
    s16 error = func_8003F67C(header);
    s32 size;

    if (error != 0) {
        func_8003F6B0(error);
        return NULL;
    }
    size = func_8003BB40(header->channels);
    if (header->entries != 0) {
        size += 0x180;
    }
    func_800392EC((u32 *)seq, size);
    seq->header = header;
    if (header->entries != 0) {
        func_8003B0AC(seq, header);
    }
    func_8003B22C(seq);
    func_8003B424(seq);
    seq->muted = 0;
    func_8003B9E4(seq);
    seq->flags |= 0x4000;
    return seq;
}

/* Stop and release a sequence (its memory unless the caller provided it). */
void func_800399D4(SoundSeq *seq) {
    if ((s16)seq->flags & 0x8000) {
        func_80039C4C((SoundTrack *)seq);
    }
    if (func_8003F67C(seq->header) != 0) {
        func_8003F6B0(0xA);
        return;
    }
    if (func_8003BA38(seq) != 0) {
        func_8003F6B0(5);
        return;
    }
    func_8003B930(seq);
    if (!(seq->flags & 0x4000)) {
        func_80039144(seq);
    }
}

/* Play a sequence from its start, fading in over `frames`. */
void func_80039A80(SoundSeq *seq, s32 fade, s32 frames) {
    if (seq == NULL) {
        func_8003F6B0(5);
        return;
    }
    seq->flags &= 0x7FFF;
    if (func_8003F67C(seq->header) != 0) {
        func_8003F6B0(0xA);
        return;
    }
    if ((s16)seq->flags & 0x8000) {
        func_80039C4C((SoundTrack *)seq);
    }
    DisableEvent(D_800595BC);
    func_8003B22C(seq);
    func_8003B424(seq);
    seq->fade.value = 0;
    func_8003A89C(seq, fade, frames);
    seq->flags |= 0x8000;
    EnableEvent(D_800595BC);
}

/* Restart `seq` from its start: reload every channel's wave bank and
 * sample addresses, mark it stopped by a fade and fade it in. */
void func_80039B68(SoundSeq *seq, s32 fade, s32 frames) {
    SoundSeqChannel *channel;
    SoundSequence *bank;
    SoundInstrument *instrument;
    s32 count;
    u32 start;

    if (seq == NULL) {
        func_8003F6B0(5);
        return;
    }
    count = seq->channels;
    channel = seq->channel;
    do {
        count--;
        bank = func_800383EC(channel->unk25);
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
    func_8003A89C(seq, fade, frames);
}

/* Resume a track (error 5 without one). */
void func_80039C4C(SoundTrack *track) {
    if (track == NULL) {
        func_8003F6B0(5);
        return;
    }
    track->flags &= 0x7FFF;
    func_8003B060((SoundSeq *)track);
}

void func_80039C8C(s32 a, s32 c) {
    if (a == 0) {
        func_8003F6B0(5);
        return;
    }
    func_8003A89C((SoundSeq *)a, 0, c);
}


/* Resume every paused track (flag 1). */
void func_80039CC4(void) {
    SoundTrack *track;

    for (track = D_80059564; track != NULL; track = track->next) {
        if (track->flags & 1) {
            track->flags &= 0x7FFF;
            func_8003B060((SoundSeq *)track);
        }
    }
}

void func_80039D24(void) {
}

/* Enable or disable (and flush) the sound effect channel. */
void func_80039D2C(s32 enable) {
    if (enable != 0) {
        D_8005957C |= 0x800;
    } else {
        func_80039FF8();
        D_8005957C &= ~0x800;
    }
}


/* Set the voice count (even, 4 to 16) unless 0; returns the setting. */
s32 func_80039D78(s32 count) {
    if (count != 0) {
        if (count > 16) {
            count = 16;
        }
        if (count < 4) {
            count = 4;
        }
        D_80059544 = count & 0xFE;
    }
    return D_80059544;
}


void func_80039DB8(s32 channel) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644((D_80059478 - 2) | 0x8000, channel, 0x6000, 0x4000);
    }
}
