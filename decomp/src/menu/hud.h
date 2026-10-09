#ifndef MENU_HUD_H
#define MENU_HUD_H

#include "common.h"
#include "psyq/libgpu.h"
#include "actor.h"
#include "stage.h"
#include "mode.h"

/* The HUD and the map overlay (menu5 80085E34-800875EC): the name plates,
 * icons and gauges of both sides, the map frame with its arrows and
 * marks. */

/* Per-buffer overlay packets (map screen, 0x310 bytes). */
typedef struct OverlayBuffer {
    DR_TPAGE tpage[2];   /* 0x000 */
    LINE_F4 frame[4];      /* 0x010: map frame outline, both sides */
    POLY_F4 bars[6];       /* 0x080: gauge backgrounds */
    DR_TPAGE bar_tpage;  /* 0x110 */
    POLY_F4 bars_dim[6];   /* 0x118 */
    POLY_F4 bars_lit[6];   /* 0x1A8 */
    POLY_F3 arrows[2][3];  /* 0x238: three per side */
    POLY_F4 marks[4];      /* 0x2B0 */
} OverlayBuffer;

/* A three-part gauge bar. */
typedef struct {
    POLY_F4 parts[3];
} GaugeBar;

/* HUD packets (D_80095698, 0x280 bytes). */
typedef struct {
    SPRT s[2];
} SpritePair;

typedef struct {
    POLY_FT4 name_l[2];    /* 0x000 */
    POLY_FT4 name_r[2];    /* 0x050 */
    SpritePair icon[4];   /* 0x0A0 */
    SpritePair gauge[4];  /* 0x140 */
    POLY_FT4 bar_l[2];     /* 0x1E0 */
    POLY_FT4 bar_r[2];     /* 0x230 */
} Hud;

extern DVECTOR D_800917F4[8];    /* map frame corner layout */
extern u16 D_80091814[16];       /* gauge palette */
extern u8 D_80092860;            /* left bar texel row */
extern u8 D_80092864;            /* right bar texel row */
extern OverlayBuffer D_8009A2F8[2];

void func_80085E90(s32 mirrored, s16 *out, s32 x);
void func_80085EAC(s32 mirrored, s16 *out, s32 y);
void func_80085EC8(OverlayBuffer *buf);
void func_800864B4(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth);
void func_800866D4(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth);
void func_80086E24(void);

#endif
