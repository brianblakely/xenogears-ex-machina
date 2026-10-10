#ifndef OVL3387_BURST_H
#define OVL3387_BURST_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "battle/burst.h"

/* Callers convert arguments/result differently from the resident definition:
 * add, clamped to 0..255 (the module passes and takes a byte). */
u8 sprite_add_clamp_byte(u8 value, s32 delta);

/* The screen burst (battle/burst.h), which battle_module_burst_run_frame_loop runs in its own
 * frame loop. */
extern SVECTOR battle_module_burst_upper_left_triangle[3]; /* first triangle of a cell */
extern SVECTOR battle_module_burst_lower_right_triangle[3]; /* second triangle */
extern u32 *battle_module_burst_current_ot;       /* ordering table being filled */

extern u8 battle_module_burst_variant; /* the effect's variant (1 in the module's data) */

void battle_module_burst_update(Task *node);
void battle_module_burst_draw(Task *node);
void battle_module_burst_release(BurstTask *burst);
BurstTask *battle_module_burst_create(void);
BurstTask *battle_module_burst_init(BurstTask *burst);
void battle_module_burst_run_frame_loop(void);

#endif
