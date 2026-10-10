#ifndef BATTLE_BURST_H
#define BATTLE_BURST_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* The screen burst: the captured screen cut into 16x16 cells of two
 * triangles that twist, ripple and fly apart, run in its own frame loop.
 * The same effect is built into the battle setup overlay (ovl2615's
 * burst_modes.c, 801e8964-801e9594, between its setup phases) and the battle
 * module ovl3387 (801fc000-801fc8f4, for a battle script opcode). */

/* One cell of the captured screen: two triangles' primitives per display
 * buffer, the corners and each corner's distance from the centre. */
typedef struct {
    u32 unk0;
    POLY_GT3 prim[2];  /* +04 */
    SVECTOR corner[3]; /* +54 */
    s32 distance[3];   /* +6c */
    u32 unk78;
} BurstCell;           /* 0x7c */

/* The effect's state (0x10fa4 bytes): laid out as a resident task node pair,
 * but run directly by its own frame loop. */
typedef struct {
    Task task;        /* +00 */
    Task draw;        /* +1c */
    s32 brightness;   /* +38: 0x80 neutral, fades out at the end */
    s32 angle;        /* +3c */
    s32 twist;        /* +40 */
    s32 frame;        /* +44 */
    s32 speed;        /* +48 */
    VECTOR trans;     /* +4c */
    SVECTOR rot;      /* +5c */
    BurstCell cells[2][14][20]; /* +64: two triangles per 16x16 cell */
} BurstTask;

#endif
