#ifndef FIELD_FIELD_SCREEN_H
#define FIELD_FIELD_SCREEN_H

/* Screen effects and transitions (800a4748-800a91f0, field_800A4748.c): the
 * distortion, the requested transitions over the saved screen split into
 * five pieces or a grid of quads, the reload on a map change, the screen
 * band an event saves and the saved VRAM column. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The screen distortion (800a484c), its values in the work block. */
extern s32 D_800ADB24;         /* its buffers are allocated */
extern DVECTOR D_800AEB24[15]; /* saved strip sources (x, y) */
/* The distortion's two wave phases (x, y) at 800b20b0. */
#define EFFECT_PHASE ((s16 *)D_800B2078.unk20B0)
void func_800A484C(s32 mode);  /* start it */
void func_800A4CC4(s32, s32, s32, s32, s32, s32, s32); /* move its values over `steps` frames */
void func_800A4DAC(void);      /* draw it */
void func_800A47D4(void);      /* stop it and release its buffers */

/* Transitions: the requested one (800adb38) runs over 800adb3c frames. */
extern s32 D_800ADB38;         /* requested transition */
extern s32 D_800ADB3C;         /* transition operand */
void func_800A5924(void);      /* run the requested transition */
void func_800A5884();          /* set the pieces up and draw them over the masked screen */
void func_800A5C40(void);      /* reload the field for a map change */
void func_800A476C(s32 x, s32 y); /* restore the projection, move the screen image to (x, y) */
void func_800A6924(void);      /* present the current draw block */
void func_800A4748(void);      /* reset the effect view, the image at (2c0, 100) */
void func_800A5600(s32 shade); /* grey the next buffer's pieces */

/* The five screen pieces (800b11ac), each with a quad and draw mode per
 * draw buffer. */
typedef struct ScreenPieces {
    DR_MODE modes[5][2];   /* 000 */
    RECT windows[5][2];    /* 078: texture windows */
    POLY_FT4 quads[5][2];  /* 0C8 */
    SVECTOR corners[5][4]; /* 258 */
} ScreenPieces;

extern ScreenPieces D_800B11AC;
extern SVECTOR D_800B00B8;     /* piece rotation */
extern s32 D_800C2684;         /* piece scale, 0x1000 = 1 */
void func_800A6408(void);      /* rotate and scale the pieces, link them */

/* The screen grid: the 320x224 screen split into 14 rows of 20 16x16
 * Gouraud quads per draw buffer (800b00c4), each corner with a brightness
 * that fades near the screen centre. */
#define GRID_ROWS 14
#define GRID_COLUMNS 20
#define GRID_QUADS (GRID_ROWS * GRID_COLUMNS)

typedef struct ScreenGrid {
    POLY_GT4 quads[2][GRID_QUADS]; /* 0000: per draw buffer */
    s16 shade[4][GRID_QUADS];      /* 71C0: per corner */
} ScreenGrid;

extern ScreenGrid *D_800B00C4;
extern s32 D_800C3A40;         /* fade radius */
void func_800A6E70(void);      /* build the grid */
void func_800A6C40(void);      /* fade and link it */
void func_800A7064(void);      /* release it */

/* The 256-wide screen band event dd saves and restores. */
extern RECT D_800AFC58;        /* its area */
extern u16 *D_800C3A48;        /* its saved pixels */
extern u16 *D_800AF87C;        /* its working pixels */
extern s32 D_800ADBB4;         /* it is saved */

/* The saved 64x256 16-bit VRAM column at (3c0, 100). */
typedef struct ScreenColumn {
    u32 words[0x2000];
} ScreenColumn;

extern RECT D_800AFC28;          /* its area */
extern ScreenColumn *D_800AFC70; /* its copy */
extern s32 D_800ADB34;           /* it is saved */
void func_800A915C(void);        /* save it once */
void func_800A91F0(void);        /* restore it and release the copy */
void func_800A90B4(s32 flags);   /* move the copy into a new block */

/* Two blocks 800a55c8 releases; nothing in the field allocates them. */
extern void *D_800AFE80;
extern void *D_800B069C;

#endif
