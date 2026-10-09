#ifndef OVL3387_BURST_H
#define OVL3387_BURST_H

#include "common.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/sprite.h"
#include "battle/area.h"
#include "battle/sprite.h"

/* Callers convert arguments/result differently from the resident definition:
 * add, clamped to 0..255 (the module passes and takes a byte). */
u8 func_80021AD8(u8 value, s32 delta);

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
 * but run directly by func_801FC8F4's own frame loop. */
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
} Burst;

extern SVECTOR D_801FCE18[3]; /* first triangle of a cell */
extern SVECTOR D_801FCE30[3]; /* second triangle */
extern u32 *D_801FCE48;       /* ordering table being filled */

extern u8 D_801FCE14; /* the effect's variant (1 in the module's data) */

void func_801FC000(Task *node);
void func_801FC11C(Task *node);
void func_801FC400(Burst *burst);
Burst *func_801FC470(void);
Burst *func_801FC4A8(Burst *burst);
void func_801FC8F4(void);

#endif
