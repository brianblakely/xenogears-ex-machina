/* ovl3386: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc6fc, called by
 * the opcode handler 800b3f04, which holds an actor in place and draws its
 * sprite as a scrolling row.
 *
 * The module was built by a compiler that schedules %hi/%lo halves of
 * addresses separately (lui far from its lw/sw/addiu, even in delay slots)
 * and keeps positive li as addiu; the qualified GCC 2.6.3/2.7.2 + ASPSX 2.34
 * do neither, so functions addressing symbols stay NON_MATCHING. */
#include "scroll.h"

extern MATRIX D_8004FBB8; /* sprite camera */

s32 func_801FC110(SpriteCell *cells, s32 count, s32 x, s32 y, Actor *actor);
void func_80022038(Actor *actor); /* refresh the actor's sprite matrix */

/* The bounds of the actor's sprite cells; returns the cell count and stores
 * the width and height (the same code as ovl3385's). */
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
INCLUDE_ASM(".local/decomp/ovl3386/asm/nonmatchings/ovl3386", func_801FC000);
#endif

/* Scroll the row by the actor's speed. */
void func_801FC0EC(TaskNode *node) {
    ScrollTask *scroll = node->object;

    scroll->scroll -= scroll->actor->speed;
}

INCLUDE_ASM(".local/decomp/ovl3386/asm/nonmatchings/ovl3386", func_801FC110);

/* Keep the actor at its held position and draw its sprite sixteen times in a
 * row, offset by the scroll (wrapped to the sprite's width). */
#ifdef NON_MATCHING
void func_801FC5C4(TaskNode *node) {
    ScrollTask *scroll;
    Actor *actor;
    Bounds bounds;
    MATRIX matrix;
    s32 position[3];
    s32 width, height;
    s32 count;
    s32 i, x;

    scroll = node->object;
    actor = scroll->actor;
    actor->position[0] = scroll->position[0];
    actor->position[1] = scroll->position[1];
    actor->position[2] = scroll->position[2];
    count = func_801FC000(actor, &width, &height, &bounds);
    func_80022038(actor);
    position[0] = actor->position[0] >> 16;
    position[1] = actor->position[1] >> 16;
    position[2] = actor->position[2] >> 16;
    TransMatrix(&actor->sprite->matrix, position);
    CompMatrix(&D_8004FBB8, &actor->sprite->matrix, &matrix);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    x = (s16)(scroll->scroll >> 16) % width - width;
    for (i = 0; i != 16; i++) {
        func_801FC110(actor->sprite->cells, count, x, 0, actor);
        x += width;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3386/asm/nonmatchings/ovl3386", func_801FC5C4);
#endif

/* Opcode entry: hold `actor` where it is under the scrolling effect. */
#ifdef NON_MATCHING
void func_801FC6FC(Actor *actor) {
    ScrollTask *scroll;

    scroll = func_8001D1D8(sizeof(ScrollTask), actor->task, func_801FC0EC, func_801FC5C4, NULL);
    scroll->actor = actor;
    actor->flags |= 0x20;
    scroll->scroll = 0;
    scroll->unk3C[0] = 0;
    scroll->unk3C[1] = 0;
    scroll->position[0] = actor->position[0];
    scroll->position[1] = actor->position[1];
    scroll->position[2] = actor->position[2];
}
#else
INCLUDE_ASM(".local/decomp/ovl3386/asm/nonmatchings/ovl3386", func_801FC6FC);
#endif
