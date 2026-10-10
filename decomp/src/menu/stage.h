#ifndef MENU_STAGE_H
#define MENU_STAGE_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "actor.h"
#include "node.h"
#include "mode.h"

/* The arena (menu5 80081ECC-800831C8, 80083DCC-80084BEC, 800875EC-80087E38;
 * menu2's handwritten 80072D18): the stage colours, the floor and its
 * height map, the backdrop, the wall and the actors' shadows, the map
 * triangles and row spans, and the drawing of the 3D views. */

/* Stage colours (17 bytes each). */
typedef struct Environment {
    u8 top[3];         /* sky gradient top */
    u8 unk3;
    u8 unk4, unk5, unk6;
    u8 unk7;
    u8 bottom[3];      /* sky gradient bottom, far and fade colour */
    u8 unkB;
    u8 back[3];        /* back colour */
    u8 unkF;
    u8 dim;            /* 0x10: halve the actor glow */
} Environment;

/* Ground height map: 128 columns of 256-unit squares, 4 bytes each. */
typedef struct GroundSquare {
    u16 height;
    u16 unk2; /* map renderer: UV high nibbles 0xF0F0, orientation bits 0..1,
               * texture-page/CLUT selector bits 2..3 */
} GroundSquare;

/* Map drawing table copied into the scratchpad; ends with the icons. */
typedef struct {
    u16 uv[4][4];      /* four orientations: upper-left/right, lower-left/right */
    u16 icons[8];      /* 0x20: per icon its texture page, then its palette */
} MapTable;

extern Environment arena_stage_color_table[];
extern u8 arena_stage_row_left_limits[];          /* per map row: leftmost allowed column */
extern u8 arena_stage_row_right_limits[];          /* per map row: rightmost allowed column */
extern MapTable arena_stage_ground_draw_table;
extern POLY_FT3 *arena_stage_ground_triangles[2];  /* map triangle pool per draw buffer */
extern Environment *arena_current_stage_colors;  /* current stage colours */
extern s32 arena_stage_uses_narrow_view;           /* selects the look-at marker (arena_stage_mark_narrow_view_cells or arena_stage_mark_wide_view_cells) */
extern u8 arena_stage_index;            /* stage */
extern GroundSquare *arena_stage_height_map; /* the height map */
extern s32 arena_stage_back_color_blue;           /* back colour blue */
extern s32 arena_stage_back_color_green;           /* back colour green */
extern s32 arena_stage_back_color_red;           /* back colour red */

/* Draw selected map cells using scratchpad row spans and MapTable.
 * Return the emitted triangle count; loaded GTE view/depth-cue state is used. */
u32 arena_stage_draw_ground_cells(u32 *ot, s32 originX, s32 originZ);
void arena_stage_init(void);
void arena_stage_get_ground_normal(SVECTOR *out);
s32 arena_stage_get_ground_height(VECTOR *pos, s32 lift);
s32 arena_stage_get_ground_square(VECTOR *pos);
void arena_stage_keep_step_inside(VECTOR *pos, VECTOR *step, s32 radius);
void arena_stage_load_wall(MenuImageFile *files);
void arena_hud_build_overlay_buffers(void);
void arena_stage_clear_row_spans(void);
void arena_stage_widen_row_spans(s32 x0, s32 y0, s32 x1, s32 y1); /* widen the map's row spans along a line */
void arena_stage_draw_ground(u32 *ot, s32 originX, s32 originZ);
void arena_stage_init_ground_pools(void);
void arena_stage_load_images(MenuImageFile *files);
void arena_stage_build_shadow_quad(Actor *actor);
void arena_stage_draw_shadow(Actor *actor, u32 *ot, MATRIX *view);

#endif
