#ifndef RESIDENT_GPU_H
#define RESIDENT_GPU_H

#include "common.h"

/* PsyQ libgpu structures as the resident uses them. */

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
    u32 code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter;
    u8 isrgb24;
    u8 pad0, pad1;
} DISPENV;

typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
    u8 r0, g0, b0, code;
} P_TAG;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} SPRT;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
} POLY_F3;

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

#define setlen(p, _len) (((P_TAG *)(p))->len = (u8)(_len))
#define setcode(p, _code) (((P_TAG *)(p))->code = (u8)(_code))
#define setSprt(p) setlen(p, 4), setcode(p, 0x64)

void func_80043B48(u32 *ot, void *primitive);                 /* AddPrim */
void func_80043BE4(u32 *ot);                                  /* TermPrim */
void func_80043C4C(POLY_F3 *p);                               /* SetPolyF3 */
s32 func_80043928(DRAWENV *env, s32 x, s32 y, s32 w, s32 h); /* SetDefDrawEnv */
s32 func_800439E0(DISPENV *env, s32 x, s32 y, s32 w, s32 h); /* SetDefDispEnv */
u16 func_80043A58(s32 x, s32 y);                              /* GetClut */
void func_80044764(RECT *rect, s32 r, s32 g, s32 b);          /* ClearImage */
void func_80044894(RECT *rect, void *data);                   /* LoadImage */
void func_8004495C(RECT *rect, s32 x, s32 y);                /* MoveImage */
void func_80044B70(void *primitive);                          /* DrawPrim */
void func_80044BD0(u32 *ot);                                  /* DrawOTag */
void func_80044C44(DRAWENV *env);                             /* PutDrawEnv */
void func_80044E9C(DISPENV *env);                             /* PutDispEnv */

#endif
