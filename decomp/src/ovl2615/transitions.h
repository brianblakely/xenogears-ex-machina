#ifndef OVL2615_TRANSITIONS_H
#define OVL2615_TRANSITIONS_H

/* The load modes' screen transitions (load_modes.c, burst_modes.c): the
 * screen split into cells that fly apart (shatter, 801e8588) or ripple
 * (burst, 801e91e8; its types are in battle/burst.h, which the battle module
 * ovl3387 shares), run in their own frame loops while the setup phases
 * load. */

#include "common.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/area.h"
#include "battle/burst.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sprite.h"
#include "ovl2615.h"

/* Callers convert arguments/result differently from the resident definition
 * (words there): add to a colour, clamped to 0..255. */
u8 sprite_add_clamp_byte(u8 value, s32 delta);

typedef struct {
    SVECTOR rot;         /* +00 */
    u8 pad8[8];
    VECTOR trans;        /* +10 */
    u8 pad20[4];
    POLY_FT3 prim[2];    /* +24: per display buffer */
    u8 pad64[0x18];
} ShatterCell;           /* 0x7c */

typedef struct {
    Task task;           /* +00 */
    Task draw;           /* +1c */
    s32 frame;           /* +38 */
    ShatterCell cells[2][7][10]; /* +3c: two triangles per 32x32 cell */
} ShatterTask;           /* 0x440c */

void battle_setup_shatter_update(Task *node);
void battle_setup_shatter_draw(Task *node);
void battle_setup_shatter_release(void *block);
ShatterTask *battle_setup_shatter_create(void);
ShatterTask *battle_setup_shatter_init(ShatterTask *task);
void battle_setup_run_shatter_load_mode(void);
void battle_setup_burst_update(Task *node);
void battle_setup_burst_draw(Task *node);
void battle_setup_burst_release(void *block);
BurstTask *battle_setup_burst_create(void);
BurstTask *battle_setup_burst_init(BurstTask *burst);
void battle_setup_run_burst_load_mode(void);

/* Flip to the other display buffer and clear its ordering table. */
static inline void swap_buffers(void) {
    BattleArea *work = &battle_area;
    FrameBuffer *next = &work->buffers[0];

    if (work->current == next) {
        next = &work->buffers[1];
    }
    work->current = next;
    work->ot = next->ot;
    ClearOTagR((u_long *)next->ot, 0x1000);
}

#endif
