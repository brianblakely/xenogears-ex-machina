#ifndef BATTLE_CURVE_H
#define BATTLE_CURVE_H

/* Curves through control points (800BFE48's unit, 800C08CC-800C0F70). */

#include "common.h"
#include "psyq/libgte.h"

extern s32 battle_curve_segments_drawn; /* segments drawn of the current curve */

void battle_curve_draw(s32 count, SVECTOR *points, void (*draw)()); /* draw a smooth curve through points */

#endif
