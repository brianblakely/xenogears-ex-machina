#ifndef MENU_TRAIL_H
#define MENU_TRAIL_H

#include "common.h"

/* libgte/libgpu layouts used by the menu's small screen effects. */
typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVector;

typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
} PrimTag;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LineF2;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    u32 pad;
} LineF3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
    u32 pad;
} LineF4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} Tile;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
} Tile1;

#define setlen(p, _len) (((PrimTag *)(p))->len = (u8)(_len))
#define setcode(p, _code) (((PrimTag *)(p))->code = (u8)(_code))
#define getcode(p) (u8)(((PrimTag *)(p))->code)
#define setSemiTrans(p, abe) \
    ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))
#define setRGB0(p, _r0, _g0, _b0) ((p)->r0 = _r0, (p)->g0 = _g0, (p)->b0 = _b0)

/* The effect a spark belongs to; its colour tints every primitive. */
typedef struct {
    u8 unk0[0x74];
    u8 r;
    u8 g;
    u8 b;
} SparkSource;

/* Sparks drawn as a line through their last positions, with one primitive
 * per draw buffer. */
typedef struct {
    SVector pos;
    SVector unk8;
    SVector trail[3];
    LineF4 line[2];
} SparkLine4;

typedef struct {
    SVector pos;
    SVector unk8;
    SVector trail[2];
    LineF3 line[2];
} SparkLine3;

typedef struct {
    SVector pos;
    SVector unk8;
    SVector trail[1];
    LineF2 line[2];
} SparkLine2;

typedef struct {
    SVector pos;
    SVector unk8;
    Tile tile[2];
} SparkTile;

typedef struct {
    SVector pos;
    SVector unk8;
    Tile1 dot[2];
} SparkDot;

extern u8 D_800928A0; /* draw buffer being built (0/1) */

s32 func_8003FA38(void); /* rand */
s32 func_8004A64C(SVector *v, s32 *sxy, s32 *p, s32 *flag);
s32 func_8004A67C(SVector *v0, SVector *v1, s32 *v2, s32 *sxy0, s32 *sxy1, s32 *sxy2, s32 *p,
                  s32 *flag);
s32 func_8004A73C(SVector *v0, SVector *v1, SVector *v2, SVector *v3, s32 *sxy0, s32 *sxy1,
                  s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag);
void func_800316C0(u32 *ot, LineF2 *prim);
void func_80031708(u32 *ot, LineF3 *prim);
void func_80031750(u32 *ot, LineF4 *prim);
void func_80031804(u32 *ot, Tile *prim);
void func_80031870(u32 *ot, Tile1 *prim);

#endif
