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

u8 func_801FC110(SpriteCell *cell, s32 count, s32 x, s32 y, Actor *actor);
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

/* Queue `count` sprite cells as textured quads offset by (x, y) through the
 * loaded matrices, mirrored or flipped per cell with the actor's sprite
 * scale; cells right of the screen's left edge are clipped to it, cells
 * wholly left of it skipped. Returns how many cells have a corner inside
 * the screen. */
#ifdef NON_MATCHING
u8 func_801FC110(SpriteCell *cell, s32 count, s32 x, s32 y, Actor *actor) {
    SVECTOR quad[4];
    s32 p, flag;
    POLY_FT4 *prim;
    s32 left, top, right, bottom;
    s32 depth;
    s32 visible = 0;
    s32 i;

    memset(quad, visible, sizeof(quad));
    if (D_80059580 + count * sizeof(POLY_FT4) >= D_80059534) {
        return 0;
    }
    for (i = 0; i != count; i++, cell++) {
        prim = (POLY_FT4 *)D_80059580;
        D_80059580 += sizeof(POLY_FT4);
        ((P_TAG *)prim)->len = 9;
        prim->color = cell->color;
        prim->tpage = cell->tpage;
        prim->clut = cell->clut;
        left = cell->x;
        top = cell->y;
        right = left + cell->width;
        if (!((cell->flags >> 4) & 1)) {
            quad[0].vx = left;
            quad[1].vx = right;
            quad[2].vx = right;
            quad[3].vx = left;
        } else {
            quad[0].vx = right;
            quad[1].vx = left;
            quad[2].vx = left;
            quad[3].vx = right;
        }
        bottom = top + cell->height;
        if (!((cell->flags >> 5) & 1)) {
            quad[0].vy = top;
            quad[1].vy = top;
            quad[2].vy = bottom;
            quad[3].vy = bottom;
        } else {
            quad[0].vy = bottom;
            quad[1].vy = bottom;
            quad[2].vy = top;
            quad[3].vy = top;
        }
        quad[0].vx += x;
        quad[1].vx += x;
        quad[2].vx += x;
        quad[3].vx += x;
        quad[0].vy += y;
        quad[1].vy += y;
        quad[2].vy += y;
        quad[3].vy += y;
        quad[0].vx <<= actor->shift;
        quad[1].vx <<= actor->shift;
        quad[2].vx <<= actor->shift;
        quad[3].vx <<= actor->shift;
        quad[0].vy <<= actor->shift;
        quad[1].vy <<= actor->shift;
        quad[2].vy <<= actor->shift;
        quad[3].vy <<= actor->shift;
        if (quad[1].vx < 0) {
            continue;
        }
        if (quad[0].vx < 0) {
            quad[0].vx = 0;
            quad[3].vx = 0;
        }
        depth = (RotAverage4(&quad[0], &quad[1], &quad[2], &quad[3], (s32 *)&prim->x0,
                             (s32 *)&prim->x1, (s32 *)&prim->x3, (s32 *)&prim->x2, &p, &flag) >>
                 D_80050100) +
                actor->depth_bias;
        if (flag & 0x8000) {
            continue;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            continue;
        }
        prim->u0 = cell->u;
        prim->v0 = cell->v;
        prim->u1 = cell->u + cell->width - 1;
        prim->v1 = cell->v;
        prim->u2 = cell->u;
        prim->v2 = cell->v + cell->height - 1;
        prim->u3 = cell->u + cell->width - 1;
        prim->v3 = cell->v + cell->height - 1;
        addPrim(D_8005956C + depth, prim);
        if (((u16)(prim->x0 - 1) < 319 && (u16)(prim->y0 - 1) < 223) ||
            ((u16)(prim->x1 - 1) < 319 && (u16)(prim->y1 - 1) < 223) ||
            ((u16)(prim->x2 - 1) < 319 && (u16)(prim->y2 - 1) < 223) ||
            ((u16)(prim->x3 - 1) < 319 && (u16)(prim->y3 - 1) < 223)) {
            visible++;
        }
    }
    return visible;
}
#else
INCLUDE_ASM(".local/decomp/ovl3386/asm/nonmatchings/ovl3386", func_801FC110);
#endif

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
