/* ovl3385: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc7b0, called by
 * the opcode handler 800b3f04, which makes an effect hold an actor in place.
 *
 * The module was built by a compiler that schedules %hi/%lo halves of
 * addresses separately (lui far from its lw/sw/addiu, even in delay slots)
 * and keeps positive li as addiu; the qualified GCC 2.6.3/2.7.2 + ASPSX 2.34
 * do neither, so functions addressing symbols stay NON_MATCHING. */
#include "hold.h"

/* The bounds of a sprite frame's cells; returns the cell count and stores
 * the width and height. */
#ifdef NON_MATCHING
/* Same operations; the original's loop steps a second pointer to the cell's
 * y and loads the height before the x. */
s32 func_801FC000(Actor *actor, s32 *width, s32 *height, Bounds *bounds) {
    SpriteCell *cell;
    s32 count;
    s32 i;
    s32 x, y;
    s32 right, bottom;

    bounds->y0 = 0x400;
    bounds->y1 = -0x400;
    bounds->x0 = 0x400;
    bounds->x1 = -0x400;
    count = actor->cell_bytes >> 2;
    cell = actor->sprite->cells;
    for (i = 0; i != count; i++, cell++) {
        y = cell->y;
        bottom = y + cell->height;
        x = cell->x;
        right = x + cell->width;
        if (x < bounds->x0) {
            bounds->x0 = x;
        }
        if (y < bounds->y0) {
            bounds->y0 = y;
        }
        if (bounds->x1 < right) {
            bounds->x1 = right;
        }
        if (bounds->y1 < bottom) {
            bounds->y1 = bottom;
        }
    }
    *width = bounds->x1 - bounds->x0;
    *height = bounds->y1 - bounds->y0;
    return count;
}
#else
INCLUDE_ASM(".local/decomp/ovl3385/asm/nonmatchings/ovl3385", func_801FC000);
#endif

/* Hold the actor in place, keeping the movement it would have made. */
void func_801FC0EC(TaskNode *node) {
    HoldTask *hold = node->object;
    Actor *actor = hold->actor;

    hold->moved[0] += actor->velocity[0];
    hold->moved[1] += actor->velocity[1];
    hold->moved[2] += actor->velocity[2];
    actor->position[0] -= actor->velocity[0];
    actor->position[1] -= actor->velocity[1];
    actor->position[2] -= actor->velocity[2];
}

INCLUDE_ASM(".local/decomp/ovl3385/asm/nonmatchings/ovl3385", func_801FC168);

INCLUDE_ASM(".local/decomp/ovl3385/asm/nonmatchings/ovl3385", func_801FC508);

/* Opcode entry: hold `actor` in place under the effect. */
#ifdef NON_MATCHING
void func_801FC7B0(Actor *actor) {
    HoldTask *hold;

    hold = func_8001D1D8(sizeof(HoldTask), actor->task, func_801FC0EC, func_801FC508, NULL);
    hold->actor = actor;
    actor->flags |= 0x20;
    hold->moved[0] = 0;
    hold->moved[1] = 0;
    hold->moved[2] = 0;
}
#else
INCLUDE_ASM(".local/decomp/ovl3385/asm/nonmatchings/ovl3385", func_801FC7B0);
#endif
