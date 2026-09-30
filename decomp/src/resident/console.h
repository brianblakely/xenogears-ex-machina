#ifndef RESIDENT_CONSOLE_H
#define RESIDENT_CONSOLE_H

#include "common.h"
#include "psyq/types.h"
#include "psyq/libgpu.h"

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
    u8 unk2[2];
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

extern Console *D_80059394;
extern s32 D_800593A0; /* the console block is not owned (not released) */

void func_80036718(s32 target, char *format, void *args);
void func_8003700C(char *format, ...); /* printf to the console */
void func_80037324(u_long *ot);            /* flush the debug text into ot */
void func_8003747C(s32 value);
void func_800374E8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9, s32 a10);
void func_800379B4(s32 a0);
void func_800379C8(char *format, ...); /* report printf */
void func_800379D8(s32 scene, s32 a1, void *a2, void *a3, void *a4);
void func_80037B88(s32 a0);

/* Hand-written ordering table link helpers (800315a0-80031894). */
void func_800317E0(u_long *ot, void *prim);
void func_80031804(u_long *ot, void *prim);
void func_80037DC0(void);

#endif
