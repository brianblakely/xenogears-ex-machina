#ifndef BATTLE_ACTION_RESOLVE_H
#define BATTLE_ACTION_RESOLVE_H

/* Resolving the committed action: the per-target formula driver and its
 * formulas (8008CCCC's unit, 800941A4-80096AB8, 80099FB0, 8009AFD8,
 * 8009C198). */

#include "common.h"

extern u8 D_800D2DB8;             /* resolve status returned to the caller */
extern void (*D_800C348C[])(void); /* formula table */

u8 func_800941A4(void); /* resolve the committed action */

#endif
