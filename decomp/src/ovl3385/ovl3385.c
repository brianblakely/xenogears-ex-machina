/* ovl3385: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc7b0, called by
 * the opcode handler 800b3f04, which makes an effect hold an actor in place.
 *
 * The module was built by the Cygnus CDK GCC 2.7.2 with a later ASPSX
 * (see ovl3385.mk). */
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

/* Queue `count` sprite cells as textured quads offset by (dx, dy) in the
 * sprite frame, mirrored or flipped per cell, projected with the actor's
 * sprite scale and linked at `depth`. Nothing is drawn unless all fit. */
#ifdef NON_MATCHING
void func_801FC168(SpriteCell *cell, s32 count, s32 dx, s32 dy, s32 depth, Actor *actor) {
    SVECTOR quad[4];
    s32 p, flag;
    POLY_FT4 *prim;
    s32 x, y, right, bottom;
    s32 i;
    u8 w, h;

    memset(quad, 0, sizeof(quad));
    if (D_80059580 + count * sizeof(POLY_FT4) >= D_80059534) {
        return;
    }
    for (i = 0; i != count; i++, cell++) {
        prim = (POLY_FT4 *)D_80059580;
        D_80059580 += sizeof(POLY_FT4);
        ((P_TAG *)prim)->len = 9;
        prim->color = cell->color;
        prim->tpage = cell->tpage;
        prim->clut = cell->clut;
        w = cell->width;
        h = cell->height;
        x = cell->x;
        y = cell->y;
        right = x + w;
        if (!((cell->flags >> 4) & 1)) {
            quad[0].vx = x;
            quad[1].vx = right;
            quad[2].vx = right;
            quad[3].vx = x;
        } else {
            quad[0].vx = right;
            quad[1].vx = x;
            quad[2].vx = x;
            quad[3].vx = right;
        }
        bottom = y + h;
        if (!((cell->flags >> 5) & 1)) {
            quad[0].vy = y;
            quad[1].vy = y;
            quad[2].vy = bottom;
            quad[3].vy = bottom;
        } else {
            quad[0].vy = bottom;
            quad[1].vy = bottom;
            quad[2].vy = y;
            quad[3].vy = y;
        }
        quad[0].vx += dx;
        quad[1].vx += dx;
        quad[2].vx += dx;
        quad[3].vx += dx;
        quad[0].vy += dy;
        quad[1].vy += dy;
        quad[2].vy += dy;
        quad[3].vy += dy;
        quad[0].vx <<= actor->shift;
        quad[1].vx <<= actor->shift;
        quad[2].vx <<= actor->shift;
        quad[3].vx <<= actor->shift;
        quad[0].vy <<= actor->shift;
        quad[1].vy <<= actor->shift;
        quad[2].vy <<= actor->shift;
        quad[3].vy <<= actor->shift;
        RotAverage4(&quad[0], &quad[1], &quad[2], &quad[3], (s32 *)&prim->x0, (s32 *)&prim->x1,
                    (s32 *)&prim->x3, (s32 *)&prim->x2, &p, &flag);
        prim->u0 = cell->u;
        prim->v0 = cell->v;
        prim->u1 = cell->u + cell->width - 1;
        prim->v1 = cell->v;
        prim->u2 = cell->u;
        prim->v2 = cell->v + cell->height - 1;
        prim->u3 = cell->u + cell->width - 1;
        prim->v3 = cell->v + cell->height - 1;
        addPrim(D_8005956C + depth, prim);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3385/asm/nonmatchings/ovl3385", func_801FC168);
#endif

/* Draw the held actor's sprite repeated side by side across the screen: from
 * the place it would have moved to rightward until off screen (x 320), then
 * leftward until off screen. */
#ifdef NON_MATCHING
void func_801FC508(TaskNode *node) {
    HoldTask *hold = node->object;
    Actor *actor = hold->actor;
    Actor *owner; /* never set before its use below (a bug in the original) */
    SVECTOR pos;
    VECTOR trans;
    Bounds bounds;
    s32 sxy, p, flag;
    s32 width, height;
    s32 depth, count;
    s32 x, y;
    SpriteCell *cells;

    pos.vx = actor->position[0] >> 16;
    pos.vy = actor->position[1] >> 16;
    pos.vz = actor->position[2] >> 16;
    SetRotMatrix(&D_8004FBB8);
    SetTransMatrix(&D_8004FBB8);
    depth = (RotTransPers(&pos, &sxy, &sxy, &flag) >> D_80050100) + actor->depth_bias;
    if (flag & 0x8000) {
        depth = 0;
    }
    actor->depth = depth;
    if ((actor->draw_flags >> 24) & 1) {
        func_80022038(actor);
        trans.vx = actor->position[0] >> 16;
        trans.vy = actor->position[1] >> 16;
        trans.vz = actor->position[2] >> 16;
        TransMatrix(&actor->sprite->matrix, &trans);
        SetRotMatrix(&actor->sprite->matrix);
        SetTransMatrix(&actor->sprite->matrix);
        if ((actor->draw_flags >> 25) & 1) {
            depth = 0xFFF;
        } else {
            depth = actor->depth_bias;
            if ((u32)(depth - 1) >= 0xFFF) {
                return;
            }
        }
    } else {
        if ((actor->draw_flags >> 29) & 1) {
            owner = owner->parent;
            depth = owner->depth;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            return;
        }
        func_8001E148(actor);
    }
    count = func_801FC000(actor, &width, &height, &bounds);
    cells = actor->sprite->cells;
    pos.vz = 0;
    pos.vy = bounds.y0;
    y = hold->moved[1] >> 16;
    for (x = hold->moved[0] >> 16;; x += width) {
        pos.vx = bounds.x0 + x;
        RotTransPers(&pos, &sxy, &p, &p);
        if ((s16)sxy > 320) {
            break;
        }
        func_801FC168(cells, count, x, y, depth, actor);
    }
    pos.vy = bounds.y0;
    y = hold->moved[1] >> 16;
    for (x = hold->moved[0] >> 16;; x -= width) {
        pos.vx = bounds.x1 + x;
        RotTransPers(&pos, &sxy, &p, &p);
        if ((s16)sxy < 0) {
            break;
        }
        func_801FC168(cells, count, x, y, depth, actor);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3385/asm/nonmatchings/ovl3385", func_801FC508);
#endif

/* Opcode entry: hold `actor` in place under the effect. */
void func_801FC7B0(Actor *actor) {
    HoldTask *hold;

    hold = func_8001D1D8(sizeof(HoldTask), actor->task, func_801FC0EC, func_801FC508, NULL);
    hold->actor = actor;
    actor->flags |= 0x20;
    hold->moved[0] = 0;
    hold->moved[1] = 0;
    hold->moved[2] = 0;
}
