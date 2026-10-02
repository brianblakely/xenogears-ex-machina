#ifndef RESIDENT_GPU_H
#define RESIDENT_GPU_H

#include "common.h"

#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* A texture-scroll animation: `count` bands of `step` lines of a VRAM area,
 * each rotated horizontally by its own phase (8004495c, MoveImage). */
typedef struct {
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
typedef struct {
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

Panorama *func_8002709C(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y,
                        s32 mode, s32 turn, VECTOR *position, u8 *colours, u16 fill_scale,
                        u16 fade_range, u16 fade_start);
s32 func_800273C4(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot,
                  s32 buffer);
void func_800278F8(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer);

MATRIX *func_8003F738(SVECTOR *angles, MATRIX *m); /* rotation matrix of three angles */

#endif
