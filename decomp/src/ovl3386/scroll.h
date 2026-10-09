#ifndef OVL3386_SCROLL_H
#define OVL3386_SCROLL_H

#include "common.h"
#include "resident/sprite.h"
#include "battle/sprite.h"

/* The effect task (resident tasks: the update node, then the drawing node,
 * both with the task as their data): the sprite repeated in a scrolling
 * row. */
typedef struct {
    Task task;        /* +00 */
    Task draw;        /* +1c */
    s32 scroll;       /* +38: 16.16, the row's offset */
    u32 unk3C[3];
    s32 position[3];  /* +48: where the sprite is kept */
    u32 unk54;
    Sprite *actor;    /* +58 */
} ScrollTask;

s32 func_801FC000(Sprite *actor, s32 *width, s32 *height, SpriteBounds *bounds);
void func_801FC0EC(Task *node);
u8 func_801FC110(SpritePart *cell, s32 count, s32 x, s32 y, Sprite *actor);
void func_801FC5C4(Task *node);

#endif
