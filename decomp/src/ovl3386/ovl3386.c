/* ovl3386: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file (D_800591B3
 * + 2) of the battle directory, D_800591B3 being the platform bits (6..11) of
 * the directory header of battle file 2, whenever they differ from the loaded
 * module's (D_800591B2). Battle script opcodes call fixed entry addresses in
 * the loaded module: this one provides 801fc6fc, called by the opcode handler
 * 800b3f04, which holds an actor in place and draws its sprite as a scrolling
 * row.
 *
 * The module is one unit, its whole file (801fc000-801fc784), built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (see ovl3386.mk). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "battle/sprite.h"
#include "scroll.h"

/* The bounds of the actor's sprite cells; returns the cell count and stores
 * the width and height (the same code as ovl3385's). */
s32 func_801FC000(Sprite *actor, s32 *width, s32 *height, SpriteBounds *bounds) {
    SpritePart *cell;
    u32 count;
    s32 i;
    s32 x, y, w, h, top;
    s32 right, bottom;

    bounds->y0 = 0x400;
    bounds->y1 = -0x400;
    bounds->x0 = 0x400;
    bounds->x1 = -0x400;
    count = ((SpriteFlagBits *)&actor->flags)->part_bytes;
    count >>= 2;
    cell = actor->renderer->parts[1];
    for (i = 0; i != count; i++, cell++) {
        x = cell->x;
        w = cell->w;
        h = cell->h;
        top = cell->y;
        y = top;
        bottom = y + h;
        right = x + w;
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

/* Scroll the row by the actor's speed. */
void func_801FC0EC(Task *node) {
    ScrollTask *scroll = node->data;

    scroll->scroll -= scroll->actor->speed;
}

/* Queue `count` sprite cells as textured quads offset by (x, y) through the
 * loaded matrices, mirrored or flipped per cell with the actor's sprite
 * scale; cells right of the screen's left edge are clipped to it, cells
 * wholly left of it skipped. Returns how many cells have a corner inside
 * the screen. */
u8 func_801FC110(SpritePart *cell, s32 count, s32 x, s32 y, Sprite *actor) {
    SVECTOR quad[4];
    long p, flag;
    POLY_FT4 *prim;
    s16 left, top;
    s32 w, h;
    u16 u, v;
    u16 du, dv;
    s32 depth;
    s32 visible = 0;
    s32 i;

    memset(quad, 0, sizeof(quad));
    if ((u8 *)D_80059580 + count * sizeof(POLY_FT4) >= D_80059534) {
        return; /* no value (a bug in the original: v0 keeps the failed test's 0) */
    }
    for (i = 0; i != count; i++, cell++) {
        prim = (POLY_FT4 *)D_80059580;
        D_80059580 = (SpriteQueueEntry *)((u8 *)D_80059580 + sizeof(POLY_FT4));
        ((P_TAG *)prim)->len = 9;
        *(u32 *)&prim->r0 = cell->colour;
        prim->tpage = cell->tpage;
        prim->clut = cell->clut;
        w = cell->w;
        h = cell->h;
        left = cell->x;
        top = cell->y;
        if (!((cell->flags >> 4) & 1)) {
            quad[0].vx = left;
            quad[1].vx = left + w;
            quad[2].vx = left + w;
            quad[3].vx = left;
        } else {
            quad[0].vx = left + w;
            quad[1].vx = left;
            quad[2].vx = left;
            quad[3].vx = left + w;
        }
        if (!((cell->flags >> 5) & 1)) {
            quad[0].vy = top;
            quad[1].vy = top;
            quad[2].vy = top + h;
            quad[3].vy = top + h;
        } else {
            quad[0].vy = top + h;
            quad[1].vy = top + h;
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
        quad[0].vx <<= (actor->flags >> 8) & 0x1F;
        quad[1].vx <<= (actor->flags >> 8) & 0x1F;
        quad[2].vx <<= (actor->flags >> 8) & 0x1F;
        quad[3].vx <<= (actor->flags >> 8) & 0x1F;
        quad[0].vy <<= (actor->flags >> 8) & 0x1F;
        quad[1].vy <<= (actor->flags >> 8) & 0x1F;
        quad[2].vy <<= (actor->flags >> 8) & 0x1F;
        quad[3].vy <<= (actor->flags >> 8) & 0x1F;
        if (quad[1].vx < 0) {
            continue;
        }
        if (quad[0].vx < 0) {
            quad[0].vx = 0;
            quad[3].vx = 0;
        }
        depth = RotAverage4(&quad[0], &quad[1], &quad[2], &quad[3], (long *)&prim->x0,
                            (long *)&prim->x1, (long *)&prim->x3, (long *)&prim->x2, &p, &flag) >>
                D_80050100;
        depth += actor->half30;
        if (flag & 0x8000) {
            continue;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            continue;
        }
        u = cell->u;
        v = cell->v;
        du = cell->w - 1;
        dv = cell->h - 1;
        prim->u0 = u;
        prim->v0 = v;
        prim->u1 = u + du;
        prim->v1 = v;
        prim->u2 = u;
        prim->v2 = v + dv;
        prim->u3 = u + du;
        prim->v3 = v + dv;
        addPrim((u32 *)D_8005956C + depth, prim);
        if (((u16)(prim->x0 - 1) < 319 && (u16)(prim->y0 - 1) < 223) ||
            ((u16)(prim->x1 - 1) < 319 && (u16)(prim->y1 - 1) < 223) ||
            ((u16)(prim->x2 - 1) < 319 && (u16)(prim->y2 - 1) < 223) ||
            ((u16)(prim->x3 - 1) < 319 && (u16)(prim->y3 - 1) < 223)) {
            visible++;
        }
    }
    return visible;
}

/* Keep the actor at its held position and draw its sprite sixteen times in a
 * row, offset by the scroll (wrapped to the sprite's width). */
void func_801FC5C4(Task *node) {
    ScrollTask *scroll;
    Sprite *actor;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    SpriteBounds bounds;
    MATRIX matrix;
    VECTOR position;
    s32 width, height;
    s32 count;
    s32 i, x;
    SpritePart *cells;

    scroll = node->data;
    actor = scroll->actor;
    actor->x = scroll->position[0];
    actor->y = scroll->position[1];
    actor->z = scroll->position[2];
    count = func_801FC000(actor, &width, &height, &bounds);
    func_80022038(actor);
    position.vx = actor->x >> 16;
    position.vy = actor->y >> 16;
    position.vz = actor->z >> 16;
    TransMatrix(&actor->renderer->matrix, &position);
    CompMatrix(&D_8004FBB8, &actor->renderer->matrix, &matrix);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    x = (s16)(scroll->scroll >> 16) % width - width;
    cells = actor->renderer->parts[1];
    for (i = 0; i != 16; i++) {
        func_801FC110(cells, count, x, 0, actor);
        x += width;
    }
}

/* Opcode entry: hold `actor` where it is under the scrolling effect. */
void func_801FC6FC(Sprite *actor) {
    ScrollTask *scroll;

    scroll = (ScrollTask *)func_8001D1D8(sizeof(ScrollTask), actor->block, func_801FC0EC, func_801FC5C4, NULL);
    scroll->actor = actor;
    actor->motion.word |= 0x20;
    scroll->scroll = 0;
    scroll->unk3C[0] = 0;
    scroll->unk3C[1] = 0;
    scroll->position[0] = actor->x;
    scroll->position[1] = actor->y;
    scroll->position[2] = actor->z;
}
