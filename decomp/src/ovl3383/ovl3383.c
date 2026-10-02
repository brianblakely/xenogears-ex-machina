/* ovl3383: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc53c, called by
 * the opcode handler 800b6b98, which starts an effect circling an actor.
 *
 * The module was built by the Cygnus CDK GCC 2.7.2 with a later ASPSX
 * (see ovl3383.mk). */
#include "spin.h"

/* Advance the effect's angle by its step. */
void func_801FC000(TaskNode *node) {
    SpinTask *spin = node->object;

    spin->angle += spin->step;
}

/* Draw the actor's first sprite part as 4-pixel rows from the bottom up, each
 * row's top edge shifted by the sine of an angle that advances (with a growing
 * step and swing) from row to row, so the sprite twists around its axis. */
void func_801FC020(TaskNode *node) {
    SpinTask *spin = node->object;
    Actor *actor = spin->actor;
    Actor *owner; /* never set before its use below (a bug in the original) */
    SpritePart *part;
    POLY_FT4 *prim;
    SVECTOR pos;
    VECTOR trans;
    s32 sxy, flag;
    s32 depth;
    s32 angle, angle_step, swing;
    s32 shift_prev;
    s32 top, row;
    s16 x;
    u8 u, w;
    s16 y;

    pos.vx = actor->pos[0].raw >> 16;
    pos.vy = actor->pos[1].raw >> 16;
    pos.vz = actor->pos[2].raw >> 16;
    SetRotMatrix(&D_8004FBB8);
    SetTransMatrix(&D_8004FBB8);
    depth = (RotTransPers(&pos, &sxy, &sxy, &flag) >> D_80050100) + actor->depth_bias;
    if (flag & 0x8000) {
        depth = 0;
    }
    actor->depth = depth;
    if ((actor->flags >> 24) & 1) {
        func_80022038(actor);
        trans.vx = actor->pos[0].part.whole;
        trans.vy = actor->pos[1].part.whole;
        trans.vz = actor->pos[2].part.whole;
        TransMatrix(&actor->renderer->matrix, &trans);
        SetRotMatrix(&actor->renderer->matrix);
        SetTransMatrix(&actor->renderer->matrix);
        if ((actor->flags >> 25) & 1) {
            depth = 0xFFF;
        } else {
            depth = actor->depth_bias;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            return;
        }
    } else {
        if ((actor->flags >> 29) & 1) {
            owner = owner->parent;
            depth = owner->depth;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            return;
        }
        func_8001E148(actor);
    }
    angle = spin->angle;
    shift_prev = func_8003F8B0(angle) * spin->radius / 4096;
    swing = spin->radius << 8;
    part = actor->renderer->parts;
    angle_step = spin->arg4 << 8;
    row = part->y + part->h - 4;
    top = part->y;
    u = part->u;
    w = part->w;
    if (top >= row) {
        return;
    }
    do {
        s32 shift = func_8003F8B0(angle) * (swing >> 8) / 4096;
        prim = (POLY_FT4 *)D_80059580;
        if (!(D_80059580 + sizeof(POLY_FT4) < D_80059534)) {
            return;
        }
        D_80059580 += sizeof(POLY_FT4);
        ((u8 *)prim)[3] = 9;
        prim->color = part->color;
        prim->tpage = part->tpage;
        prim->clut = part->clut;
        x = part->x;
        D_8004FB98[0].vx = x + shift;
        D_8004FB98[1].vx = x + w + shift;
        D_8004FB98[2].vx = x + w + shift_prev;
        D_8004FB98[3].vx = x + shift_prev;
        y = part->y + part->h + row;
        D_8004FB98[0].vy = y;
        D_8004FB98[1].vy = y;
        D_8004FB98[2].vy = y + 4;
        D_8004FB98[3].vy = y + 4;
        D_8004FB98[0].vx <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[1].vx <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[2].vx <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[3].vx <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[0].vy <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[1].vy <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[2].vy <<= (actor->flags40 >> 8) & 0x1F;
        D_8004FB98[3].vy <<= (actor->flags40 >> 8) & 0x1F;
        RotTransPers4(&D_8004FB98[0], &D_8004FB98[1], &D_8004FB98[2], &D_8004FB98[3],
                      (s32 *)&prim->x0, (s32 *)&prim->x1, (s32 *)&prim->x3, (s32 *)&prim->x2,
                      &sxy, &flag);
        prim->u0 = u;
        prim->v0 = part->v + part->h + row;
        prim->u1 = u + w;
        prim->v1 = part->v + part->h + row;
        prim->u2 = u;
        prim->v2 = part->v + part->h + row + 4;
        prim->u3 = u + w;
        prim->v3 = part->v + part->h + row + 4;
        shift_prev = shift;
        if (!(flag & 0x8000) && depth < 0x1000) {
            AddPrim(D_8005956C + depth, prim);
        }
        row -= 4;
        angle += angle_step >> 8;
        angle_step += spin->arg5;
        swing += spin->arg3;
    } while (top < row);
}

/* Opcode entry: start the effect circling `actor` from `angle`, advancing by
 * `step` each frame (operands from the battle script, see 800b6b98). */
void func_801FC53C(Actor *actor, s32 angle, s32 radius, s32 arg3, s32 arg4, s32 arg5, s32 step) {
    SpinTask *spin;

    spin = func_8001D1D8(sizeof(SpinTask), actor->task, func_801FC000, func_801FC020, NULL);
    spin->actor = actor;
    spin->radius = radius;
    spin->arg3 = arg3;
    spin->arg4 = arg4;
    spin->arg5 = arg5;
    spin->angle = angle;
    spin->step = step;
}
