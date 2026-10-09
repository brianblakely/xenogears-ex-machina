#ifndef MENU_DISPLAY_H
#define MENU_DISPLAY_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "packets.h"

/* The display (menu7 80089210-800898BC, 8008A2B8-8008A3E0, 8008AC0C-
 * 8008AF6C; menu5 80083BB4): the two display buffers, the screen setup,
 * the drawing layers with their ordering tables, and the frame's time
 * budget. */

/* One of the two display buffers (table at 0x8009a0d8, 0xF8 bytes each). */
typedef struct DisplayBuffer {
    DRAWENV draw;      /* 0x00 */
    DISPENV disp;      /* 0x5C */
    u32 ot;            /* 0x70: one-entry ordering table */
    SPRT_16 sprite;    /* 0x74 */
    u8 unk84[0x4C];
    DR_AREA area;      /* 0xD0 */
    DR_OFFSET offset;  /* 0xDC */
    TILE background;   /* 0xE8: also the screen fade */
} DisplayBuffer;

/* Drawing layer (0x68 bytes): an ordering table per display buffer with
 * its drawing area, offset and background packets. */
typedef struct {
    s32 unk0;
    u32 *ot[2];        /* 0x04: per display buffer */
    u32 *last[2];      /* 0x0C: last entry of each */
    s16 length;        /* 0x14 */
    u8 flags;          /* 0x16: 4 own area, 8 own offset, 0x10 background */
    u8 shift;          /* 0x17: 14 - log2(length) */
    DR_AREA area[2];   /* 0x18: per buffer */
    DR_OFFSET offset[2]; /* 0x30: per buffer */
    TileWords tile[2]; /* 0x48 */
} OtPair;

/* Word count of a primitive, from its tag (libgpu P_TAG len). */
#define TAG_LEN(tag) (((u8 *)(tag))[3])

extern OtPair *D_80091C30;        /* table to compact at the end of the frame */
extern s16 D_8009285C;            /* display width */
extern DisplayBuffer *D_80092868; /* the buffer being drawn */
extern s16 D_8009286C;            /* display height */
extern DisplayBuffer *D_80092870; /* the buffer being displayed */
extern u8 D_800928A0;             /* index of the buffer being built */
extern u32 *D_800928E4;           /* ordering table primitives are added to */
extern s32 D_80092914;            /* colour changed this frame */
extern u32 *D_80092938;           /* ordering table of the buffer being built */
extern MATRIX D_80096FE0;         /* screen scale */
extern DisplayBuffer D_8009A0D8[2];

void func_80083BB4(s32 both); /* clear one or both display areas */
void func_80089210(s32 width, s32 height);
void func_80089330(s32 width, s32 height);
void func_80089534(s32 width, s32 height);
void func_8008976C(s32 width, s32 height);
OtPair *func_8008A2B8(u16 length);
void func_8008A3A8(OtPair *layer);
void func_8008AC0C(OtPair *pair);
void func_8008AC7C(OtPair *pair);
void func_8008AC8C(void);
void func_8008ACB8(s32 frames);
void func_8008AE1C(OtPair *layer);

#endif
