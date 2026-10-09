#ifndef OVL3381_TILES_H
#define OVL3381_TILES_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* One half (a triangle) of an 8x8 cell of the captured screen: its primitive
 * for each display buffer and its three corners. */
typedef struct {
    u32 unk0;
    POLY_FT3 prim[2];     /* +04: per display buffer */
    SVECTOR corner[3];    /* +44 */
    SVECTOR spread[3];    /* +5c: corners pushed out along their direction */
} Tile;

/* The task (resident tasks: the update node, then the drawing node; both
 * callbacks receive their node, whose data is the task): a 16x16 grid of
 * 8x8 cells, each split into two triangles. */
typedef struct {
    Task task;           /* +00 */
    Task draw;           /* +1c */
    s32 frame;           /* +38 */
    u32 unk3C;
    Tile tiles[2][16][16]; /* +40: [half][row][column] */
} TileTask;

#endif
