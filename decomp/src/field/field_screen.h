#ifndef FIELD_FIELD_SCREEN_H
#define FIELD_FIELD_SCREEN_H

/* Screen effects and transitions (800a4748-800a91f0, field_screen.c): the
 * distortion, the requested transitions over the saved screen split into
 * five pieces or a grid of quads, the reload on a map change, the screen
 * band an event saves and the saved VRAM column. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The screen distortion (800a484c), its values in the work block. */
extern s32 field_distortion_buffers_allocated;         /* its buffers are allocated */
extern DVECTOR field_distortion_strip_sources[15];     /* saved strip sources (x, y) */
/* The distortion's two wave phases (x, y) at 800b20b0. */
#define EFFECT_PHASE ((s16 *)field_work.unk20B0)
void field_distortion_start(s32 resume);                                /* start it */
void field_distortion_set_targets(s32, s32, s32, s32, s32, s32, s32); /* move its values over `steps` frames */
void field_distortion_draw(void);                                     /* draw it */
void field_distortion_stop(void);                                     /* stop it and release its buffers */

/* Transitions: the requested one (800adb38) runs over 800adb3c frames. */
extern s32 field_transition_kind;              /* requested transition */
extern s32 field_transition_frames;            /* transition operand */
void field_transition_run(void);               /* run the requested transition */
void field_screen_pieces_show();               /* set the pieces up and draw them over the masked screen */
void field_reload_for_map_change(void);        /* reload the field for a map change */
void field_screen_copy_to(s32 x, s32 y);       /* restore the projection, move the screen image to (x, y) */
void field_draw_present_overlay(void);         /* present the current draw block */
void field_screen_save_copy(void);             /* reset the effect view, the image at (2c0, 100) */
void field_screen_pieces_set_shade(s32 shade); /* grey the next buffer's pieces */

/* The five screen pieces (800b11ac), each with a quad and draw mode per
 * draw buffer. */
typedef struct ScreenPieces {
    DR_MODE modes[5][2];   /* 000 */
    RECT windows[5][2];    /* 078: texture windows */
    POLY_FT4 quads[5][2];  /* 0C8 */
    SVECTOR corners[5][4]; /* 258 */
} ScreenPieces;

extern ScreenPieces field_screen_pieces;
extern SVECTOR field_screen_pieces_rotation;     /* piece rotation */
extern s32 field_screen_pieces_scale;            /* piece scale, 0x1000 = 1 */
void field_screen_pieces_draw(void);             /* rotate and scale the pieces, link them */

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

extern ScreenGrid *field_screen_grid;
extern s32 field_screen_grid_fade_radius;         /* fade radius */
void field_screen_grid_build(void);               /* build the grid */
void field_screen_grid_draw(void);                /* fade and link it */
void field_screen_grid_release(void);             /* release it */

/* The 256-wide screen band event dd saves and restores. */
extern RECT field_screen_band_rect;          /* its area */
extern u16 *field_screen_band_saved_pixels;  /* its saved pixels */
extern u16 *field_screen_band_work_pixels;   /* its working pixels */
extern s32 field_screen_band_upload_pending; /* it is saved */

/* The saved 64x256 16-bit VRAM column at (3c0, 100). */
typedef struct ScreenColumn {
    u_long words[0x2000];
} ScreenColumn;

extern RECT field_vram_column_rect;          /* its area */
extern ScreenColumn *field_vram_column_copy; /* its copy */
extern s32 field_vram_column_saved;          /* it is saved */
void field_vram_column_save(void);           /* save it once */
void field_vram_column_restore(void);        /* restore it and release the copy */
void field_vram_column_relocate(s32 flags);  /* move the copy into a new block */

/* Two blocks 800a55c8 releases; nothing in the field allocates them. */
extern void *field_block_pair_first;
extern void *field_block_pair_second;

#endif
