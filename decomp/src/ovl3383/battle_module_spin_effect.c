/* ovl3383: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file (sprite_requested_battle_module
 * + 2) of the battle directory, sprite_requested_battle_module being the platform bits (6..11) of
 * the directory header of battle file 2, whenever they differ from the loaded
 * module's (sprite_loaded_battle_module). Battle script opcodes call fixed entry addresses in
 * the loaded module: this one provides 801fc53c, called by the opcode handler
 * 800b6b98, which starts an effect circling an actor.
 *
 * The module is one unit, its whole file (801fc000-801fc5e4), built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (see ovl3383.mk). */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "battle/sprite.h"
#include "spin.h"

/* 801FC000: Advance the effect's angle by its step. */
void battle_module_spin_update(Task *node) {
    SpinTask *spin = node->data;

    spin->angle += spin->step;
}

/* 801FC020: Draw the actor's first sprite part as 4-pixel rows from the bottom up, each
 * row's top edge shifted by the sine of an angle that advances (with a growing
 * step and swing) from row to row, so the sprite twists around its axis. */
void battle_module_spin_draw(Task *node) {
    SpinTask *spin = node->data;
    Sprite *actor = spin->actor;
    Sprite *owner; /* never set before its use below (a bug in the original) */
    SpritePart *part;
    POLY_FT4 *prim;
    SVECTOR pos;
    VECTOR trans;
    long sxy, flag;
    s32 depth;
    s32 angle, angle_step, swing;
    s32 shift_prev;
    s32 top, row;
    s16 x;
    u8 u, w;
    s16 y;

    pos.vx = actor->x >> 16;
    pos.vy = actor->y >> 16;
    pos.vz = actor->z >> 16;
    SetRotMatrix(&sprite_view_matrix);
    SetTransMatrix(&sprite_view_matrix);
    depth = (RotTransPers(&pos, &sxy, &sxy, &flag) >> model_ot_depth_shift) + actor->half30;
    if (flag & 0x8000) {
        depth = 0;
    }
    actor->depth = depth;
    if ((actor->render.word >> 24) & 1) {
        sprite_update_orientation(actor);
        trans.vx = FIXED_WHOLE(actor->x);
        trans.vy = FIXED_WHOLE(actor->y);
        trans.vz = FIXED_WHOLE(actor->z);
        TransMatrix(&actor->renderer->matrix, &trans);
        SetRotMatrix(&actor->renderer->matrix);
        SetTransMatrix(&actor->renderer->matrix);
        if ((actor->render.word >> 25) & 1) {
            depth = 0xFFF;
        } else {
            depth = actor->half30;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            return;
        }
    } else {
        if ((actor->render.word >> 29) & 1) {
            owner = owner->parent;
            depth = owner->depth;
        }
        if ((u32)(depth - 1) >= 0xFFF) {
            return;
        }
        sprite_set_draw_matrix(actor);
    }
    angle = spin->angle;
    shift_prev = gpu_get_sin(angle) * spin->radius / 4096;
    swing = spin->radius << 8;
    part = actor->renderer->parts[1];
    angle_step = spin->arg4 << 8;
    row = part->y + part->h - 4;
    top = part->y;
    u = part->u;
    w = part->w;
    if (top >= row) {
        return;
    }
    do {
        s32 shift = gpu_get_sin(angle) * (swing >> 8) / 4096;
        prim = (POLY_FT4 *)sprite_queue_next_free;
        if (!((u8 *)sprite_queue_next_free + sizeof(POLY_FT4) < sprite_queue_block_end)) {
            return;
        }
        sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(POLY_FT4));
        ((u8 *)prim)[3] = 9;
        *(u32 *)&prim->r0 = part->colour;
        prim->tpage = part->tpage;
        prim->clut = part->clut;
        x = part->x;
        sprite_quad_corners[0].vx = x + shift;
        sprite_quad_corners[1].vx = x + w + shift;
        sprite_quad_corners[2].vx = x + w + shift_prev;
        sprite_quad_corners[3].vx = x + shift_prev;
        y = part->y + part->h + row;
        sprite_quad_corners[0].vy = y;
        sprite_quad_corners[1].vy = y;
        sprite_quad_corners[2].vy = y + 4;
        sprite_quad_corners[3].vy = y + 4;
        sprite_quad_corners[0].vx <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[1].vx <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[2].vx <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[3].vx <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[0].vy <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[1].vy <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[2].vy <<= (actor->flags >> 8) & 0x1F;
        sprite_quad_corners[3].vy <<= (actor->flags >> 8) & 0x1F;
        RotTransPers4(&sprite_quad_corners[0], &sprite_quad_corners[1], &sprite_quad_corners[2], &sprite_quad_corners[3],
                      (long *)&prim->x0, (long *)&prim->x1, (long *)&prim->x3, (long *)&prim->x2,
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
            AddPrim((u32 *)sprite_ot + depth, prim);
        }
        row -= 4;
        angle += angle_step >> 8;
        angle_step += spin->arg5;
        swing += spin->arg3;
    } while (top < row);
}

/* 801FC53C: Opcode entry: start the effect circling `actor` from `angle`, advancing by
 * `step` each frame (operands from the battle script, see 800b6b98). */
void battle_module_spin_start(Sprite *actor, s32 angle, s32 radius, s32 swing_growth, s32 row_angle_step, s32 angle_step_growth, s32 step) {
    SpinTask *spin;

    spin = (SpinTask *)task_alloc_two_node_task(sizeof(SpinTask), actor->block, battle_module_spin_update, battle_module_spin_draw, NULL);
    spin->actor = actor;
    spin->radius = radius;
    spin->arg3 = swing_growth;
    spin->arg4 = row_angle_step;
    spin->arg5 = angle_step_growth;
    spin->angle = angle;
    spin->step = step;
}
