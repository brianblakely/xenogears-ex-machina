#ifndef RESIDENT_GPU_H
#define RESIDENT_GPU_H

#include "common.h"

#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* A texture-scroll animation: `count` bands of `step` lines of a VRAM area,
 * each rotated horizontally by its own phase (8004495c, MoveImage). */
typedef struct TextureScroll {
    u16 x, y;          /* area */
    u16 w, h;
    u16 step;          /* lines per band */
    u16 count;         /* bands */
    u16 source_x;      /* source column */
    u16 source_y;      /* source line */
    s8 *speeds;        /* phase step per band (4.4 fixed point) */
    u16 *phases;
} TextureScroll;

/* A panoramic backdrop: a strip of `width` x `height` texels at (tex_x,
 * tex_y) drawn as up to eight textured quads per draw buffer, turning with
 * the view direction, with optional sky and ground fills and a fade
 * between them. */
typedef struct Panorama {
    POLY_FT4 quads[2][8]; /* per buffer */
    POLY_F4 fills[4];     /* per buffer: sky [0..1], ground [2..3] */
    POLY_G4 fades[2];     /* per buffer */
    s32 width;
    s32 height;
    s32 turn;             /* texels per turn, 4.12 (negative when vz is) */
    s16 tex_x;
    s16 tex_y;
    s16 mode;             /* texture mode (0 4-bit, 1 8-bit, 2 16-bit) */
    s16 v;                /* first texture row in its page */
    s16 vx, vy, vz;       /* position: vz ahead of the target, vy high */
    s16 unk342;
    s16 fill;             /* the fills are drawn */
    s16 fill_scale;       /* distance of the ground edge, 8.8 */
    s16 fade_range;       /* distance over which the strip shrinks */
    s16 fade_start;
} Panorama;

s32 gpu_draw_panorama(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot,
                  s32 buffer);
void gpu_draw_panorama_strip(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer);

MATRIX *gpu_build_rotation_matrix(SVECTOR *angles, MATRIX *m); /* Rx * Ry * Rz of three angles */

/* More of the display services. */
void gpu_free_panorama(void *block);
void gpu_update_texture_scroll(TextureScroll *scroll);
void gpu_free_texture_scroll(TextureScroll *scroll);

/* Link a primitive at the head of an ordering table entry (handwritten,
 * gpu_ot_link_poly_g4.s and its kin: the word count after the tag is the
 * primitive's). */
void gpu_ot_link_poly_g4(u_long *ot, POLY_G4 *prim);
void gpu_ot_link_line_f2(u_long *ot, LINE_F2 *prim);
void gpu_ot_link_line_f3(u_long *ot, LINE_F3 *prim);
void gpu_ot_link_line_f4(u_long *ot, LINE_F4 *prim);
void gpu_ot_link_tile_1(u_long *ot, TILE_1 *prim);

#endif
