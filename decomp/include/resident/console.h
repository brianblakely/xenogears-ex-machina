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


/* A conversion's settings (defaults at D_8005A1CC). */
typedef struct {
    union {
        s32 flags;     /* 1 left-justified, 2 plus sign, 4 zero padded, 8 precision set */
        u8 bytes[4];   /* bytes[1]: the sign character */
    } u;
    s32 width;
    s32 precision;
    u32 base;
} FormatSpec;

s32 func_80036718(s32 target, const char *format, va_list args); /* the console printf core */
void func_8003700C(char *format, ...); /* printf to the console */
void func_80037324(u_long *ot);            /* flush the debug text into ot */
void func_8003747C(s32 value);
Console *func_800374E8(s32 left, s32 top, s32 width, s32 height, s32 capacity, u32 flags,
                       s32 tex_x, s32 tex_y, s32 clut_x, s32 clut_y, void *font); /* open */
void func_800379B4(s32 task); /* set D_80050618, the menu overlay's task index */
void func_800379C8(char *format, ...); /* report printf */
s32 func_800379D8(s32 scene, s32 variant, u8 **sequence, s32 *unused, u8 **bank); /* load scene music */
void func_80037B88(s32 a0);

/* Hand-written ordering table link helpers (800315a0-80031894). */
void func_800317E0(u_long *ot, void *prim); /* link a SPRT_8 */
void func_80031804(u_long *ot, void *prim); /* link a TILE */
void func_80037DC0(void);

/* More of the debug console and the system screens' services. */
void func_80036DC8(s32 r, s32 g, s32 b);
void func_80037058(s32 x, s32 y);
void func_8003708C(s32 x, s32 y);
void func_8003748C(void);
void func_80037E8C(void);
void func_80037EE4(void);
extern s32 D_80050618; /* the menu's mode (800379b4) */

#endif
