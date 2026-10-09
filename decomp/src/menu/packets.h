#ifndef MENU_PACKETS_H
#define MENU_PACKETS_H

#include "common.h"

/* A TILE, a TILE_1 and a POLY_FT4 as the menu writes them: the colour and
 * code as one word (and the POLY_FT4's positions as words, its texture
 * coordinates as halfwords), where the libgpu layouts have bytes. */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    s16 x0, y0;
    s16 w, h;
} TileWords;

typedef struct {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    s16 x0, y0;
} Tile1Words;

typedef struct PolyFT4Words {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    u32 xy0;
    u16 uv0;
    u16 clut;
    u32 xy1;
    u16 uv1;
    u16 tpage;
    u32 xy2;
    u16 uv2;
    u16 pad1;
    u32 xy3;
    u16 uv3;
    u16 pad2;
} PolyFT4Words;

#endif
