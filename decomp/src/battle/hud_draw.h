#ifndef BATTLE_HUD_DRAW_H
#define BATTLE_HUD_DRAW_H

#include "battle_core.h"

extern u8 D_800D2C0C[3][2]; /* values shown in number strings 2-4 */
void func_8009A2D4(u8 member);
void func_80089AF8(u8 member);
void func_800898F0(u8 member);



extern u8 D_800D3294;
/* This unit passes the u16 coordinates, scale and angle as full words. */
s32 func_80025FA8(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y,
                  s32 scaleX, s32 scaleY, s32 angle);
s32 func_80076A6C(s32 id, POLY_FT4 *prims, s16 x, s16 y);
void func_80076C34(POLY_FT4 *prim);
void func_800765C4(s32 member);
void func_80076710(s32 member);

#endif
