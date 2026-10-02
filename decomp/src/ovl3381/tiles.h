#ifndef OVL3381_TILES_H
#define OVL3381_TILES_H

#include "common.h"

/* libgte / libgpu types. */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

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
} POLY_FT3;

void SetPolyFT3(POLY_FT3 *p);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
void AddPrim(void *ot, void *p);
s32 ReadGeomScreen(void);
void ReadGeomOffset(s32 *ofx, s32 *ofy);
void SetGeomOffset(s32 ofx, s32 ofy);
void SetGeomScreen(s32 h);
s32 ratan2(s32 y, s32 x);

/* Battle overlay work area (800c3eb0); only the members the module reads. */
typedef struct {
    u8 unk0[0x8C5E];
    u16 flags;         /* +8c5e: bit 8 ends the battle-entry effect */
    u8 unk8C60[0x24];
    s32 buffer;        /* +8c84: double-buffer index being drawn */
} BattleWork;
extern BattleWork D_800C3EB0;

/* Resident task system: a task node (update) followed by its drawing node;
 * both callbacks receive their node, whose +4 names the task's object. */
typedef struct TaskNode {
    u32 unk0;
    void *object;
    void (*update)(struct TaskNode *node);
    void (*destroy)(struct TaskNode *node);
    u32 unk10;
    u32 unk14;
    struct TaskNode *next;
} TaskNode;

void *func_8001D1D8(s32 size, void *owner, void (*update)(TaskNode *),
                    void (*draw)(TaskNode *), void (*destroy)(TaskNode *)); /* create a task */
void func_8001CB48(TaskNode *node); /* unlink a drawing node */
void func_8001CD94(TaskNode *node); /* unlink a task */
void func_80025180(void *block);    /* release a block after the frame */
s32 func_8003F8B0(s32 angle);       /* sine (4096 = 1.0) */
s32 func_8003F8CC(s32 angle);       /* cosine (4096 = 1.0) */

extern void *D_8005956C; /* current ordering table */
extern s32 D_80050100;   /* model depth shift */

/* One half (a triangle) of an 8x8 cell of the captured screen: its primitive
 * for each display buffer and its three corners. */
typedef struct {
    u32 unk0;
    POLY_FT3 prim[2];     /* +04: per display buffer */
    SVECTOR corner[3];    /* +44 */
    SVECTOR spread[3];    /* +5c: corners pushed out along their direction */
} Tile;

/* The task: a 16x16 grid of 8x8 cells, each split into two triangles. */
typedef struct {
    TaskNode task;       /* +00 */
    TaskNode draw;       /* +1c */
    s32 frame;           /* +38 */
    u32 unk3C;
    Tile tiles[2][16][16]; /* +40: [half][row][column] */
} TileTask;

#endif
