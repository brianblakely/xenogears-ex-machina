#ifndef PSYQ_LIBGPU_H
#define PSYQ_LIBGPU_H

#include "common.h"

/* PsyQ libgpu types and the resident's named GPU routines. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    u32 dr_env[16];
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} TILE;

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
} POLY_FT4;

DRAWENV *SetDefDrawEnv(DRAWENV *env, s32 x, s32 y, s32 w, s32 h);
DISPENV *SetDefDispEnv(DISPENV *env, s32 x, s32 y, s32 w, s32 h);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
u16 GetClut(s32 x, s32 y);
void AddPrims(void *ot, void *first, void *last);
void SetSemiTrans(void *prim, s32 on);
void SetPolyFT4(POLY_FT4 *poly);
void SetTile(TILE *tile);
void DrawSync(s32 mode);
void ClearImage(RECT *rect, s32 r, s32 g, s32 b);
void LoadImage(RECT *rect, u32 *pixels);
void StoreImage(RECT *rect, u32 *pixels);
void MoveImage(RECT *rect, s32 x, s32 y);
void ClearOTagR(u32 *ot, s32 count);
void DrawOTag(u32 *ot);
void PutDrawEnv(DRAWENV *env);
void PutDispEnv(DISPENV *env);
void OpenTIM(u32 *tim);
TIM_IMAGE *ReadTIM(TIM_IMAGE *image);
s32 VSync(s32 mode);

#endif
