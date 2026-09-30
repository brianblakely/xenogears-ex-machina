#ifndef MENU_GPU_H
#define MENU_GPU_H

#include "common.h"

/* libgpu primitive layouts. */

/* Screen rectangle (libgpu RECT). */
typedef struct {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} Rect;

/* Textured sprite packet (libgpu SPRT). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} Sprite;

/* Flat rectangle packet (libgpu TILE). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} Tile;

/* Flat quadrilateral packet (libgpu POLY_F4). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} PolyF4;

/* Gouraud-shaded quadrilateral packet (libgpu POLY_G4). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} PolyG4;

/* Gouraud-less textured triangle packet (libgpu POLY_FT3). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u16 uv0, clut;
    s16 x1, y1;
    u16 uv1, tpage;
    s16 x2, y2;
    u16 uv2, pad;
} PolyFT3;

/* Textured quadrilateral packet (libgpu POLY_FT4). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} PolyFT4;

/* libgpu TIM_IMAGE. */
typedef struct {
    u32 mode;
    Rect *crect;
    u16 *caddr;
    Rect *prect;
    void *paddr;
} TimImage;

/* The common packet head (libgpu P_TAG). */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0, g0, b0, code;
} PacketTag;

#endif
