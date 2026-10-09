#ifndef BATTLE_ACTION_RESOLVE_H
#define BATTLE_ACTION_RESOLVE_H

/* Resolving the committed action: the per-target formula driver and its
 * formulas. */

#include "common.h"

extern u8 D_800D2DB8;             /* resolve status returned to the caller */
extern void (*D_800C348C[])(void); /* formula table */

u8 func_800941A4(void);

#endif
