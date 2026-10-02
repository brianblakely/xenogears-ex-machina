#ifndef BATTLE_CURVE_H
#define BATTLE_CURVE_H

/* Curves through control points (800C08CC, 800C0D18). */

#include "common.h"
#include "psyq.h"

extern s32 D_800D2FCC; /* segments drawn of the current curve */

void func_800C0D18(s32 row, s32 column, SVECTOR *points, VECTOR *out);

#endif
