#ifndef OVL3383_SPIN_H
#define OVL3383_SPIN_H

#include "common.h"
#include "resident/sprite.h"

/* The effect task battle_module_spin_start creates (0x54 bytes; resident tasks: the
 * update node, then the drawing node, both with the task as their data). */
typedef struct {
    Task task;      /* +00 */
    Task draw;      /* +1c */
    Sprite *actor;  /* +38: the battle sprite the effect circles */
    s32 radius;     /* +3c: horizontal swing of the rows */
    s32 arg3;       /* +40: added to the swing each row (8.8) */
    s32 arg4;       /* +44: angle step between rows (8.8) */
    s32 arg5;       /* +48: added to the angle step each row */
    s32 angle;      /* +4c: advanced by step every frame */
    s32 step;       /* +50 */
} SpinTask;

void battle_module_spin_update(Task *node);
void battle_module_spin_draw(Task *node);

#endif
