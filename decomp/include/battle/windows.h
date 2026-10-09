#ifndef BATTLE_WINDOWS_H
#define BATTLE_WINDOWS_H

#include "common.h"
#include "psyq/libgpu.h"
#include "resident/window.h"
#include "battle/graphics.h"

/* The battle's windows and messages: the windows' primitives (80070E2C's
 * unit, 80077454) and frames (8008CCCC's, 8008F8F4-8008FAD8), the battle
 * message strip (80070E2C's 800780A8-8007819C, 800792F8's 80079E18-80079E4C),
 * and the message text window the event script overlay prints into. */

/* Window rectangle (*800d2d90[window]). */
typedef struct WindowRect {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u16 curW;          /* +0x8 opening size */
    u16 curH;
    u8 style;          /* +0xC */
} WindowRect;

/* A window's primitives (0x5a8-byte heap block). */
typedef struct WindowBlock {
    POLY_FT4 corners[8]; /* corner glyphs: [corner * 2 + draw buffer] */
    POLY_FT4 frame[4][4]; /* +0x140 edge pieces, textures 1-4 */
    POLY_G4 shade[2];     /* +0x3C0 background, one per draw buffer */
    DR_MODE mode[2];      /* +0x408 */
    u8 unk420[0x5A0 - 0x420];
    s32 cornerCount;      /* +0x5A0 corner primitives built */
    u8 buffer;            /* +0x5A4 draw buffer of the last placement */
    u8 unk5A5[3];
} WindowBlock;

extern WindowBlock *D_800D2E38[7];
extern WindowRect *D_800D2D90[7];

/* Texture page coordinates of window texture `t` (8-bit mode: two texels
 * per VRAM halfword). */
#define WINDOW_TEX_U(t) (((u8)D_800C3EA4->sprites[t].pageX & 0x3F) * 2)
#define WINDOW_TEX_V(t) (D_800C3EA4->sprites[t].pageY)

/* The battle message image (800d39b8): upload rectangle and pixels. */
typedef struct {
    RECT rect;
    u32 *pixels;
    u8 unkC[2];
    s8 width;          /* +0xE */
} MessageImage;

extern MessageImage D_800D39B8;
extern void *D_800D39F0;   /* battle message table */
extern u8 D_800C3E8C;      /* pending battle message + 1 */

/* Eight 0x60-byte message entries from 800d36c8. */
typedef struct BattleMessage {
    POLY_FT4 prims[2];
    RECT rect;         /* +0x50 text image upload rectangle */
    u32 *pixels;       /* +0x58 */
    u8 alternate;      /* +0x5C odd texture row */
    u8 shown;          /* +0x5D */
    u8 width;          /* +0x5E */
    u8 unk5F;
} BattleMessage;

extern BattleMessage D_800D36C8[8];

extern Window *D_800D2DAC; /* the message text window */

void func_80077454(u8 window); /* set up a window's primitives */
void func_80079E18(u8 index); /* show battle message window `index` */
void func_80079E4C(u8 index); /* hide it */
void func_8008F8F4(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait); /* open a window */
void func_8008FA60(u8 window); /* close a window */

void func_8008FAD8(void);    /* grow the opening windows */

#endif
