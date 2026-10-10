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

/* HUD packets (arena_hud_packets, 0x280 bytes). */
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

extern DVECTOR arena_hud_gauge_frame_layout[8];    /* map frame corner layout */
extern u16 arena_hud_gauge_palette[16];       /* gauge palette */
extern u8 arena_hud_left_charge_bar_v;            /* left bar texel row */
extern u8 arena_hud_unread_right_charge_bar_v;            /* right bar texel row */
extern OverlayBuffer arena_hud_overlay_buffers[2];

void arena_hud_set_gauge_x(s32 mirrored, s16 *out, s32 x);
void arena_hud_set_gauge_y(s32 mirrored, s16 *out, s32 y);
void arena_hud_build_overlay_buffer(OverlayBuffer *buf);
void arena_hud_build_tim_quad(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth);
void arena_hud_build_mirrored_tim_quad(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth);
void arena_hud_build_packets(MenuImageFile *files);
void arena_hud_link_overlay_tpage(void);
void arena_hud_draw(Actor *left, Actor *right);

#endif
