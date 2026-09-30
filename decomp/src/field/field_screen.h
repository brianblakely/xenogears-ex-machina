#ifndef FIELD_FIELD_SCREEN_H
#define FIELD_FIELD_SCREEN_H

#include "field.h"

/* Screen transitions (800adb38): the saved screen split into five pieces
 * or a grid of quads. */

/* The screen grid of the field transition: the 320x224 screen split into
 * 14 rows of 20 16x16 Gouraud quads per draw buffer (800b00c4), each
 * corner with a brightness that fades near the screen centre. */

#define GRID_ROWS 14
#define GRID_COLUMNS 20
#define GRID_QUADS (GRID_ROWS * GRID_COLUMNS)

typedef struct {
    POLY_GT4 quads[2][GRID_QUADS]; /* 0000: per draw buffer */
    s16 shade[4][GRID_QUADS];      /* 71C0: per corner */
} ScreenGrid;

/* A draw mode followed by per-buffer primitives not used here (800b1e24,
 * 0xc0 bytes per draw buffer). */
typedef struct {
    DR_MODE mode;
    u8 unk0C[0xC0 - 0xC];
} GridMode;

extern ScreenGrid *D_800B00C4;
extern GridMode D_800B1E24[2];
extern s32 D_800C3A40; /* fade radius */

extern s32 D_8005A4C0;   /* resident: map read-ahead size */
extern s32 D_800AFD04;   /* reloading */

void func_8001B044(void);
void func_8001B3A8(void);
void func_8003748C(void); /* resident */
void func_800700B0(void);
void func_80070CC8(void);
void func_8007554C(void);
void func_80077DAC(void);
void func_800A6C40(void);
void func_800A6E70(void);
void func_800A7064(void);
void func_800A915C(void);
void func_800A91F0(void);

#endif
