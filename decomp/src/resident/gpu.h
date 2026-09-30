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

MATRIX *func_8003F738(SVECTOR *angles, MATRIX *m); /* rotation matrix of three angles */

#endif
