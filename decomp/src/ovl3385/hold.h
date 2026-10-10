#ifndef OVL3385_HOLD_H
#define OVL3385_HOLD_H

#include "common.h"
#include "resident/sprite.h"
#include "battle/sprite.h"

/* The effect task (resident tasks: the update node, then the drawing node,
 * both with the task as their data): it takes over the sprite's movement
 * (16.16; the drawing reads the whole parts). */
typedef struct {
    Task task;      /* +00 */
    Task draw;      /* +1c */
    s32 moved[3];   /* +38: the movement the sprite did not make */
    u32 unk44;
    Sprite *actor;  /* +48 */
} HoldTask;

s32 battle_module_hold_get_cell_bounds(Sprite *actor, s32 *width, s32 *height, SpriteBounds *bounds);
void battle_module_hold_update(Task *node);
void battle_module_hold_queue_cells(SpritePart *cell, s32 count, s32 dx, s32 dy, s32 depth, Sprite *actor);
void battle_module_hold_draw(Task *node);

#endif
