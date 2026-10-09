#ifndef OVL3387_BURST_H
#define OVL3387_BURST_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "battle/burst.h"

/* Callers convert arguments/result differently from the resident definition:
 * add, clamped to 0..255 (the module passes and takes a byte). */
u8 func_80021AD8(u8 value, s32 delta);

/* The screen burst (battle/burst.h), which func_801FC8F4 runs in its own
 * frame loop. */
extern SVECTOR D_801FCE18[3]; /* first triangle of a cell */
extern SVECTOR D_801FCE30[3]; /* second triangle */
extern u32 *D_801FCE48;       /* ordering table being filled */

extern u8 D_801FCE14; /* the effect's variant (1 in the module's data) */

void func_801FC000(Task *node);
void func_801FC11C(Task *node);
void func_801FC400(BurstTask *burst);
BurstTask *func_801FC470(void);
BurstTask *func_801FC4A8(BurstTask *burst);
void func_801FC8F4(void);

#endif
