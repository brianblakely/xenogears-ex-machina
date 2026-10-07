/* ovl2143 (Disc 1 slot 2143 / Disc 2 slot 2138), loaded at 0x801dc000.
 * A 3D scene module: up to ten actors (D_801E8670), each a model hierarchy
 * (0x7c-byte nodes built from a relocated model group, 8002c3e8/8002cb54/
 * 8002c8cc) with rotation/movement tweens from a 0x14-byte slot pool, a
 * 16-particle pool of textured quads, and actor scripts. It drives the GTE
 * directly (RotMatrix/CompMatrix/MulMatrix0, RTPS/RTPT in 801dcec8 and
 * 801e0398) and uses the resident heap (80031bdc/800320e8, tag 4), libgpu
 * and sound effect banks (8003852c). It has no strings. The coverage census
 * runs it only on the title -> New Game routes (sound-mode scenarios), so it
 * is presumably part of the opening scene; no resident or overlay code in
 * the split images names 0x801dc000 directly (loaded as a file). Built with
 * GCC 2.6.3 (see func_801DF7A8) and ASPSX-style checked divisions. */
#include "ovl2143.h"

/* Relocate a model group and list its model records (0x38 bytes each after the
 * 0x10-byte header) in a new block. */
ModelList *func_801DC22C(u8 *group, ModelList *list) {
    u32 count;
    u32 i;

    func_80032498(4, 0);
    count = func_8002C3E8(group);
    list->models = func_80031BDC(count * 4, 0);
    list->count = count;
    if (list->models != NULL) {
        for (i = 0; i < count; i++) {
            list->models[i] = (ModelRecord *)(group + 0x10 + i * 0x38);
        }
    }
    return list;
}

/* Build a model hierarchy: a root node and one node per link up to the first
 * model index past the group; each model node gets its packets for both
 * buffers (optionally after setting 8002cc10/8002cc74 parameters). Returns the
 * nodes, or NULL when there are none or an allocation fails. */
ModelPart *func_801DC2D0(ModelList *group, HierarchyLink *links, s32 mode, s32 configure,
                         s16 param0, s16 param1, s16 param2, s16 param3) {
    HierarchyLink *link;
    ModelPart *parts;
    ModelPart *part;
    s32 count;
    s32 index;
    u16 model;
    u16 parent;

    func_80032498(4, 0);
    link = links;
    count = 0;
    while (link->model < group->count || link->model == 0xFFFF) {
        count++;
        link++;
    }
    if (count == 0) {
        return NULL;
    }
    count++;
    parts = func_80031BDC(count * sizeof(ModelPart), 0);
    link = links;
    if (parts == NULL) {
        return NULL;
    }
    part = parts + 1;
    index = 1;
    model = link->model;
    parent = link->parent;
    parts->dirty = 1;
    parts->rotate = 1;
    parts->yxz = 1;
    parts->scale[0] = 0x1000;
    parts->scale[1] = 0x1000;
    parts->scale[2] = 0x1000;
    parts->parent = NULL;
    parts->visible = 0;
    parts->model = 0xFFFF;
    parts->count = count;
    parts->packets[0] = NULL;
    parts->packets[1] = NULL;
    parts->rot.vx = 0;
    parts->rot.vy = 0;
    parts->rot.vz = 0;
    parts->pos[0] = 0;
    parts->pos[1] = 0;
    parts->pos[2] = 0;
    parts->attachments[0] = NULL;
    parts->attachments[1] = NULL;
    parts->attachments[2] = NULL;
    while (model < group->count || model == 0xFFFF) {
        if (parent == 0xFFFF) {
            part->parent = NULL;
        } else {
            part->parent = parts + parent + 1;
        }
        part->count = index++;
        part->dirty = 1;
        part->rotate = 1;
        part->visible = 1;
        part->scale[0] = 0x1000;
        part->scale[1] = 0x1000;
        part->scale[2] = 0x1000;
        part->yxz = 0;
        part->billboard = 0;
        part->model = model;
        if (model != 0xFFFF) {
            func_8002CB54(group->models[model], &part->packets[0], &part->packets[1]);
            if (part->packets[0] == NULL) {
                func_801DCD8C(parts);
                return NULL;
            }
            if (configure) {
                func_8002CC10(param0, param1);
                func_8002CC74(param2, param3);
            }
            func_8002C8CC(group->models[model], part->packets[0], mode);
            memcpy(part->packets[1], part->packets[0], group->models[model]->packet_bytes);
            part->rot.vx = 0;
        } else {
            part->packets[0] = NULL;
            part->packets[1] = NULL;
            part->rot.vx = 0;
        }
        part->rot.vy = 0;
        part->rot.vz = 0;
        part->pos[0] = 0;
        part->pos[1] = 0;
        part->pos[2] = 0;
        part->attachments[0] = NULL;
        part->attachments[1] = NULL;
        part->attachments[2] = NULL;
        part++;
        link++;
        model = link->model;
        parent = link->parent;
    }
    return parts;
}

/* Recompose a hierarchy's matrices: the root's world matrix from its rotation
 * and position, its local one scaled by `scale`; each other node's local
 * matrix from its rotation when flagged and its world matrix from its parent
 * when it or its parent changed. Returns the node count. */
u32 func_801DC5C0(ModelPart *parts, s32 scale) {
    MATRIX *scaling = (MATRIX *)0x1F800000;
    ModelPart *part;
    s32 product;
    u32 count;
    u32 i;

    part = parts;
    count = part->count;
    part->world.t[0] = part->pos[0];
    part->world.t[1] = part->pos[1];
    part->world.t[2] = part->pos[2];
    if (part->yxz) {
        func_8004A92C(&part->rot, &part->world);
    } else {
        func_8003F738(&part->rot, &part->world);
    }
    product = scale * parts->scale[0];
    product >>= 12;
    scaling->m[0][0] = product;
    scaling->m[0][1] = 0;
    scaling->m[0][2] = 0;
    scaling->m[1][0] = 0;
    product = scale * parts->scale[1];
    product >>= 12;
    scaling->m[1][1] = product;
    scaling->m[1][2] = 0;
    scaling->m[2][0] = 0;
    scaling->m[2][1] = 0;
    product = scale * parts->scale[2];
    product >>= 12;
    scaling->m[2][2] = product;
    MulMatrix0(&parts->world, scaling, &parts->local);
    parts->local.t[0] = parts->world.t[0];
    parts->local.t[1] = parts->world.t[1];
    parts->local.t[2] = parts->world.t[2];

    for (i = 1; i < count; i++) {
        parts++;
        if (parts->rotate) {
            if (parts->yxz) {
                func_8004A92C(&parts->rot, &parts->local);
                parts->rotate = 0;
            } else {
                func_8003F738(&parts->rot, &parts->local);
                parts->rotate = 0;
            }
        }
        if (parts->parent != NULL && parts->parent->dirty == 1) {
            parts->dirty = 1;
        }
        if (parts->dirty) {
            parts->local.t[0] = parts->pos[0];
            parts->local.t[1] = parts->pos[1];
            parts->local.t[2] = parts->pos[2];
            if (parts->parent != NULL) {
                CompMatrix(&parts->parent->world, &parts->local, &parts->world);
            } else {
                parts->world = parts->local;
            }
        }
    }
    for (i = 1; i < count; i++) {
        part++;
        part->dirty = 0;
    }
    return count;
}

/* Like func_801DC5C0, with scaled nodes: a rebuilt local matrix takes the
 * node's own scale and the inverse of its parent's, and a parent's rebuild or
 * change propagates to its children. Clears both flags afterwards. */
u32 func_801DC848(ModelPart *parts, s32 scale) {
    MATRIX *scaling = (MATRIX *)0x1F800000;
    MATRIX *scratch;
    ModelPart *part;
    s32 product;
    u32 count;
    u32 i;

    part = parts;
    count = part->count;
    part->world.t[0] = part->pos[0];
    part->world.t[1] = part->pos[1];
    part->world.t[2] = part->pos[2];
    if (part->yxz) {
        func_8004A92C(&part->rot, &part->world);
    } else {
        func_8003F738(&part->rot, &part->world);
    }
    product = scale * parts->scale[0];
    product >>= 12;
    scaling->m[0][0] = product;
    scaling->m[0][1] = 0;
    scaling->m[0][2] = 0;
    scaling->m[1][0] = 0;
    product = scale * parts->scale[1];
    product >>= 12;
    scaling->m[1][1] = product;
    scaling->m[1][2] = 0;
    scaling->m[2][0] = 0;
    scaling->m[2][1] = 0;
    product = scale * parts->scale[2];
    product >>= 12;
    scaling->m[2][2] = product;
    MulMatrix0(&parts->world, scaling, &parts->local);
    parts->local.t[0] = parts->world.t[0];
    parts->local.t[1] = parts->world.t[1];
    parts->local.t[2] = parts->world.t[2];

    for (i = 1; i < count; i++) {
        parts++;
        if (parts->parent != NULL) {
            if (parts->parent->rotate == 1) {
                parts->rotate = 1;
            }
            if (parts->parent->dirty == 1) {
                parts->dirty = 1;
            }
        }
        if (parts->rotate) {
            scratch = (MATRIX *)0x1F800000;
            if (parts->yxz) {
                func_8004A92C(&parts->rot, &parts->local);
            } else {
                func_8003F738(&parts->rot, &parts->local);
            }
            scratch->m[0][0] = parts->scale[0];
            scratch->m[0][1] = 0;
            scratch->m[0][2] = 0;
            scratch->m[1][0] = 0;
            scratch->m[1][1] = parts->scale[1];
            scratch->m[1][2] = 0;
            scratch->m[2][0] = 0;
            scratch->m[2][1] = 0;
            scratch->m[2][2] = parts->scale[2];
            MulMatrix0(&parts->local, scratch, &parts->local);
            if (parts->parent != NULL) {
                scratch->m[0][0] = 0x1000000 / parts->parent->scale[0];
                scratch->m[0][1] = 0;
                scratch->m[0][2] = 0;
                scratch->m[1][0] = 0;
                scratch->m[1][1] = 0x1000000 / parts->parent->scale[1];
                scratch->m[1][2] = 0;
                scratch->m[2][0] = 0;
                scratch->m[2][1] = 0;
                scratch->m[2][2] = 0x1000000 / parts->parent->scale[2];
                MulMatrix0(scratch, &parts->local, &parts->local);
            }
        }
        if (parts->dirty) {
            parts->local.t[0] = parts->pos[0];
            parts->local.t[1] = parts->pos[1];
            parts->local.t[2] = parts->pos[2];
            if (parts->parent != NULL) {
                CompMatrix(&parts->parent->world, &parts->local, &parts->world);
            } else {
                parts->world = parts->local;
            }
        }
    }
    for (i = 1; i < count; i++) {
        part++;
        part->rotate = 0;
        part->dirty = 0;
    }
    return count;
}

void func_801DCC34(void) {
}

/* Draw a hierarchy's model nodes: each node's world matrix is combined with the
 * root's light and view transforms (MulMatrix0 into the light matrix,
 * CompMatrix into the rotation/translation) before its buffer's packets are
 * drawn (8002c700). */
void func_801DCC3C(ModelList *group, ModelPart *parts, MATRIX *view, MATRIX *light, s32 arg4,
                   s32 arg5, s32 buffer) {
    MATRIX *light_root = (MATRIX *)0x1F800020;
    MATRIX *view_root = (MATRIX *)0x1F800040;
    MATRIX *m = (MATRIX *)0x1F800000;
    u32 count;
    u32 i;

    MulMatrix0(light, &parts->world, light_root);
    CompMatrix(view, &parts->local, view_root);
    count = parts->count;
    parts++;
    for (i = 1; i < count; parts++) {
        i++;
        if (parts->model != 0xFFFF) {
            MulMatrix0(light_root, &parts->world, m);
            SetLightMatrix(m);
            CompMatrix(view_root, &parts->world, m);
            SetRotMatrix(m);
            SetTransMatrix(m);
            func_8002C700(group->models[parts->model], parts->packets[buffer], arg5, arg4);
        }
    }
}

/* Release a hierarchy: every node's packet buffers, then the nodes. */
void func_801DCD8C(ModelPart *parts) {
    ModelPart *part;
    s32 i;

    if (parts != NULL) {
        part = parts;
        for (i = 0; i < parts->count; part++) {
            i++;
            if (part->packets[0] != NULL) {
                func_800320E8(part->packets[0]);
                part->packets[0] = NULL;
                part->packets[1] = NULL;
            }
        }
        parts->count = 0;
        func_800320E8(parts);
    }
}

/* Release a model list; with `release_models` each model's own packets too
 * (8002cbbc). */
void func_801DCE18(ModelList *list, s32 release_models) {
    u32 i;

    if (list != NULL) {
        for (i = 0; i < list->count; i++) {
            if (list->models != NULL && list->models[i] != NULL && release_models) {
                func_8002CBBC(list->models[i]);
            }
        }
        if (list->models != NULL) {
            func_800320E8(list->models);
            list->models = NULL;
        }
    }
}

/* Set the middle column of a rotation matrix (its local y axis). */
#define SET_COLUMN_Y(mat, x, y, z)                                             \
    do {                                                                       \
        (mat)->m[0][1] = (x);                                                  \
        (mat)->m[1][1] = (y);                                                  \
        (mat)->m[2][1] = (z);                                                  \
    } while (0)

/* Draw an active actor under the camera `m`: its shadow quad at the root's
 * floor height (unless flags bit 0; the depth also sets b39), every visible
 * model node (lit by `light`; billboard nodes face the view), its records24
 * surfaces, its image animations (advanced by `ticks`) and its channels'
 * ribbons. A channel whose frame count wraps to 0 leaves the channel pointer
 * where it is, so the next channel index redraws it (as the original). */
void func_801DCEC8(Actor *actor, MATRIX *m, MATRIX *light, s32 mode, s32 ticks, u32 *ot, s32 buffer) {
    MATRIX *scratch = (MATRIX *)0x1F800000;
    MATRIX *placed = (MATRIX *)0x1F800020;
    SVECTOR v;
    VECTOR front, back;
    s32 depth, depth2;
    ModelList *models;
    ModelPart *part;
    ModelPart *parts;
    u16 scale;
    MATRIX *view = (MATRIX *)0x1F800040;
    u32 count;
    s32 i;
    s32 k, side;
    s32 shade;
    Record24 *record;
    Record24Entry *entry;
    ImageAnim *anim;
    Channel *ch;
    VECTOR *end0, *end1;
    VECTOR *start0, *start1;
    s32 oldest;

    if (!actor->active) {
        return;
    }
    part = actor->parts;
    models = actor->models;
    scale = actor->scale;
    count = part->count;
    parts = part;
    CompMatrix(m, &part->local, view);
    if (!(actor->flags & 1)) {
        CompMatrix(&part->local, &actor->parts[1].world, scratch);
        SetRotMatrix(scratch);
        SetTransMatrix(scratch);
        v.vx = 0;
        v.vy = 0;
        v.vz = 0x1000;
        gte_ldv0(&v);
        gte_rtv0tr();
        gte_stlvnl(&front);
        v.vx = 0;
        v.vy = 0;
        v.vz = 0;
        gte_ldv0(&v);
        gte_rtv0tr();
        gte_stlvnl(&back);
        v.vy = -ratan2(front.vz - back.vz, front.vx - back.vx);
        v.vx = 0;
        v.vz = 0;
        func_8003F738(&v, placed);
        placed->t[0] = back.vx;
        placed->t[1] = actor->h60;
        placed->t[2] = back.vz;
        CompMatrix(m, placed, placed);
        shade = actor->scale - (actor->h60 - actor->parts->pos[1]) / 4;
        if (shade < 0) {
            shade = 0;
        }
        scratch->m[0][0] = shade;
        scratch->m[0][1] = 0;
        scratch->m[0][2] = 0;
        scratch->m[1][0] = 0;
        scratch->m[1][1] = shade;
        scratch->m[1][2] = 0;
        scratch->m[2][0] = 0;
        scratch->m[2][1] = 0;
        scratch->m[2][2] = shade;
        func_80049ACC(placed, scratch);
        SetRotMatrix(placed);
        SetTransMatrix(placed);
        v.vx = actor->size[1];
        v.vy = 0;
        v.vz = actor->size[2];
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&actor->prims[buffer].x0);
        gte_stszotz(&depth);
        v.vx = -actor->size[1];
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&actor->prims[buffer].x1);
        gte_stszotz(&depth2);
        if (depth2 < depth) {
            depth = depth2;
        }
        v.vx = actor->size[1];
        v.vz = -actor->size[2];
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&actor->prims[buffer].x2);
        gte_stszotz(&depth2);
        if (depth2 < depth) {
            depth = depth2;
        }
        v.vx = -actor->size[1];
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&actor->prims[buffer].x3);
        gte_stszotz(&depth2);
        if (depth2 < depth) {
            depth = depth2;
        }
        depth >>= D_80050100;
        addPrim(ot + depth, &actor->prims[buffer]);
        if (depth > 0x2D8) {
            depth = 0x2D8;
        }
        actor->b39 = 0x6B - depth / 8;
    }
    MulMatrix0(light, &part->world, placed);
    for (i = 1, part++; i < count; i++, part++) {
        if (part->model == 0xFFFF || !part->visible) {
            continue;
        }
        MulMatrix0(placed, &part->world, scratch);
        SetLightMatrix(scratch);
        CompMatrix(view, &part->world, scratch);
        if (part->billboard > 0) {
            scratch->m[0][0] = actor->scale;
            scratch->m[0][2] = 0;
            scratch->m[1][0] = 0;
            scratch->m[1][2] = 0;
            scratch->m[2][0] = 0;
            scratch->m[2][2] = actor->scale;
            if (part->billboard == 1) {
                SET_COLUMN_Y(scratch, view->m[0][1], view->m[1][1], view->m[2][1]);
            } else {
                SET_COLUMN_Y(scratch, 0, actor->scale, 0);
            }
        }
        SetRotMatrix(scratch);
        SetTransMatrix(scratch);
        func_8002C700(models->models[part->model], part->packets[buffer], (s32)ot, mode);
    }
    record = actor->records24;
    for (i = 0; i < actor->count10D; record++, i++) {
        VECTOR out;
        SVECTOR sun;

        if ((s16)record->h0 < 0) {
            continue;
        }
        sun.vx = -D_801E8698 * func_8003F8CC(parts->rot.vy + 0x400) / 4096;
        sun.vz = D_801E8698 * func_8003F8B0(parts->rot.vy + 0x400) / 4096;
        sun.vy = actor->h3E;
        CompMatrix(&parts->local, &parts[(s16)record->h0].world, scratch);
        SetRotMatrix(scratch);
        SetTransMatrix(scratch);
        for (k = 0; k < record->rings; k++) {
            gte_ldv0(&record->centres[k]);
            gte_rtv0tr();
            gte_stlvnl(&out);
            record->block1C[k]->pos[0] = out.vx;
            record->block1C[k]->pos[1] = out.vy;
            record->block1C[k]->pos[2] = out.vz;
        }
        entry = record->block18;
        for (k = 0; k < record->entry_count; k++, entry++) {
            CompMatrix(&parts->local, &parts[entry->h6].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            gte_ldv0(entry);
            gte_rtv0tr();
            gte_stlvnl(&out);
            entry->h8 = out.vx;
            entry->hA = out.vy;
            entry->hC = out.vz;
        }
        func_801E22F8(record, &sun, m, ot, buffer, scale, actor->h60);
    }
    anim = actor->records30;
    for (i = 0; i < actor->count10E; i++, anim++) {
        func_801E1258(anim, ticks);
    }
    ch = actor->channels;
    for (i = 0; i < actor->channel_count; i++) {
        if (ch->id < 0) {
            ch++;
            continue;
        }
        ch->frame = (ch->frame - 1) & 7;
        if (!ch->solid) {
            CompMatrix(view, &parts[ch->id].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            gte_ldv0(&ch->ends[0]);
            gte_rtps();
            gte_stsxy(&ch->trail.sxy[0][ch->frame]);
            gte_ldv0(&ch->ends[1]);
            gte_rtps();
            gte_stsxy(&ch->trail.sxy[1][ch->frame]);
            if (++ch->count == 0) {
                continue;
            }
            if (ch->max < ch->count || ch->particle == NULL) {
                ch->count = 1;
                ch->particle = func_801E0248(ch->pool, ch->semi_trans);
                ch->particle->projected = 0;
                ch->particle->age = 0;
                ch->particle->lifetime = ch->lifetime;
                ch->particle->color[0] = ch->color[0];
                ch->particle->color[1] = ch->color[1];
                ch->particle->color[2] = ch->color[2];
                ch->particle->fade[0] = ch->fade[0];
                ch->particle->fade[1] = ch->fade[1];
                ch->particle->fade[2] = ch->fade[2];
                oldest = (ch->frame + ch->count) & 7;
                ch->particle->x0 = (ch->trail.sxy[0] + oldest)->x;
                ch->particle->y0 = (ch->trail.sxy[0] + oldest)->y;
                ch->particle->x2 = (ch->trail.sxy[1] + oldest)->x;
                ch->particle->y2 = (ch->trail.sxy[1] + oldest)->y;
            }
            ch->particle->x1 = (ch->trail.sxy[0] + ch->frame)->x;
            ch->particle->y1 = (ch->trail.sxy[0] + ch->frame)->y;
            ch->particle->x3 = (ch->trail.sxy[1] + ch->frame)->x;
            ch->particle->y3 = (ch->trail.sxy[1] + ch->frame)->y;
        } else {
            CompMatrix(&parts->local, &parts[ch->id].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            side = ch->frame & 1;
            end0 = &ch->trail.pos[0][side];
            end1 = &ch->trail.pos[1][side];
            gte_ldv0(&ch->ends[0]);
            gte_rtv0tr();
            gte_stlvnl(end0);
            gte_ldv0(&ch->ends[1]);
            gte_rtv0tr();
            gte_stlvnl(end1);
            if (++ch->count == 0) {
                continue;
            }
            if (ch->max < ch->count || ch->particle == NULL) {
                ch->count = 1;
                ch->particle = func_801E0248(ch->pool, ch->semi_trans);
                ch->particle->projected = 1;
                ch->particle->age = 0;
                ch->particle->lifetime = ch->lifetime;
                ch->particle->color[0] = ch->color[0];
                ch->particle->color[1] = ch->color[1];
                ch->particle->color[2] = ch->color[2];
                ch->particle->fade[0] = ch->fade[0];
                ch->particle->fade[1] = ch->fade[1];
                ch->particle->fade[2] = ch->fade[2];
                start0 = &ch->trail.pos[0][1 - side];
                ch->particle->x0 = start0->vx;
                ch->particle->y0 = start0->vy;
                ch->particle->z0 = start0->vz;
                start1 = &ch->trail.pos[1][1 - side];
                ch->particle->x2 = start1->vx;
                ch->particle->y2 = start1->vy;
                ch->particle->z2 = start1->vz;
            }
            ch->particle->x1 = end0->vx;
            ch->particle->y1 = end0->vy;
            ch->particle->z1 = end0->vz;
            ch->particle->x3 = end1->vx;
            ch->particle->y3 = end1->vy;
            ch->particle->z3 = end1->vz;
        }
        ch++;
    }
}

/* Step the tweens attached to each node of a hierarchy: its rotation
 * (attachment 0: set, delta or add from a track, interpolate, approach,
 * spin, or turn toward a point within a growing limit), position
 * (attachment 1: the same, or a move in the node's frame scaled by `scale`)
 * and scale (attachment 2: interpolate, approach, spin). A finished tween is
 * released (or restarted when looping: tracks rewind, others stop). Returns
 * flags: 0x100/0x200/0x400 some tween ran/ended/looped (1/2/4 when it has
 * `tag`). As in the original, the "spin" stop clears the step of the last
 * slot a computing kind used, which may belong to an earlier node.
 * NON_MATCHING (12 bytes shorter): the original stores the third rotation
 * delta in both arms (its other track steps share one store), the position
 * track steps keep the delta and track pointer in a0/v1 the other way round,
 * and a few temporaries get other registers. */
#ifdef NON_MATCHING
s32 func_801DDBF8(SlotPool *pool, ModelPart *parts, s32 tag, s32 scale) {
    PoolSlot *slot;
    PoolSlot *last;
    u8 *track;
    SVECTOR move;
    VECTOR moved;
    u32 i;
    u32 count;
    s32 result = 0;
    u32 kind;
    u32 pos_kind;
    u8 mode;
    s32 time;
    s32 dx, dy, dz, dist, limit, turn;
    s16 sx, sy, sz;
    s32 delta, low, high;

    count = parts->count;
    for (i = 0; i < count; i++, parts++) {
        if (parts->attachments[0] != NULL) {
            slot = parts->attachments[0];
            kind = slot->kind;
            switch (kind & 0xF) {
            case 0:
                track = slot->u.track.pos;
                if (!(kind & 0x10)) {
                    parts->rot.vx = *(u16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                }
                if (!(kind & 0x20)) {
                    parts->rot.vy = *(u16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                }
                if (!(kind & 0x40)) {
                    parts->rot.vz = *(u16 *)track;
                    slot->u.track.pos += 2;
                }
                break;
            case 1:
                if (!(kind & 0x10)) {
                    delta = *(s8 *)slot->u.track.pos++;
                    if (delta != -0x80) {
                        parts->rot.vx += delta;
                    } else {
                        low = *slot->u.track.pos++;
                        high = *(s8 *)slot->u.track.pos++;
                        parts->rot.vx = low | (high << 8);
                    }
                }
                if (!(kind & 0x20)) {
                    delta = *(s8 *)slot->u.track.pos++;
                    if (delta != -0x80) {
                        parts->rot.vy += delta;
                    } else {
                        low = *slot->u.track.pos++;
                        high = *(s8 *)slot->u.track.pos++;
                        parts->rot.vy = low | (high << 8);
                    }
                }
                if (!(kind & 0x40)) {
                    delta = *(s8 *)slot->u.track.pos++;
                    if (delta != -0x80) {
                        parts->rot.vz += delta;
                    } else {
                        low = *slot->u.track.pos++;
                        high = *(s8 *)slot->u.track.pos++;
                        parts->rot.vz = low | (high << 8);
                    }
                }
                break;
            case 2:
                track = slot->u.track.pos;
                if (!(kind & 0x10)) {
                    parts->rot.vx += *(u16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                }
                if (!(kind & 0x20)) {
                    parts->rot.vy += *(u16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                }
                if (!(kind & 0x40)) {
                    parts->rot.vz += *(u16 *)track;
                    slot->u.track.pos += 2;
                }
                break;
            case 3:
                time = (s16)(slot->time + 1);
                parts->rot.vx = slot->u.value[0] + slot->u.value[3] * time / slot->duration;
                parts->rot.vy = slot->u.value[1] + slot->u.value[4] * time / slot->duration;
                last = slot;
                parts->rot.vz = slot->u.value[2] + slot->u.value[5] * time / slot->duration;
                break;
            case 4:
                sx = (slot->u.value[3] - parts->rot.vx) / slot->duration;
                sy = (slot->u.value[4] - parts->rot.vy) / slot->duration;
                sz = (slot->u.value[5] - parts->rot.vz) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    parts->rot.vx = slot->u.value[3];
                    parts->rot.vy = slot->u.value[4];
                    parts->rot.vz = slot->u.value[5];
                } else {
                    parts->rot.vx += sx;
                    parts->rot.vz += sz;
                    parts->rot.vy += sy;
                    slot->time = 0;
                }
                break;
            case 5:
                slot->u.value[0] += slot->u.value[3];
                parts->rot.vx += slot->u.value[0];
                slot->u.value[1] += slot->u.value[4];
                parts->rot.vy += slot->u.value[1];
                slot->u.value[2] += slot->u.value[5];
                last = slot;
                parts->rot.vz += slot->u.value[2];
                break;
            case 7:
            case 8:
                dx = slot->u.value[3] - parts->pos[0];
                dy = slot->u.value[4] - parts->pos[1];
                dz = slot->u.value[5] - parts->pos[2];
                dist = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
                last = slot;
                turn = (ratan2(-dx, -dz) - parts->rot.vy) & 0xFFF;
                if (turn >= 0x800) {
                    turn -= 0x1000;
                }
                limit = slot->u.value[1] + (dist + slot->time) * slot->u.value[2] / slot->u.value[0];
                if ((turn < 0 ? -turn : turn) < limit) {
                    parts->rot.vy += turn;
                } else if (turn < 0) {
                    parts->rot.vy -= limit;
                } else {
                    parts->rot.vy += limit;
                }
                if ((kind & 0xF) == 7) {
                    turn = (ratan2(dy, SquareRoot0(dx * dx + dz * dz)) - parts->rot.vx) & 0xFFF;
                    if (turn >= 0x800) {
                        turn -= 0x1000;
                    }
                    if ((turn < 0 ? -turn : turn) < limit) {
                        parts->rot.vx += turn;
                    } else if (turn < 0) {
                        parts->rot.vx -= limit;
                    } else {
                        parts->rot.vx += limit;
                    }
                }
                if (last->time < 0x7D00) {
                    last->time += last->duration;
                }
                goto rot_done;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->flag) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_801DF7A8(pool, slot);
                    parts->attachments[0] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    if ((kind & 0xF) < 3) {
                        slot->time = 0;
                        slot->u.track.pos = slot->u.track.start;
                    } else {
                        slot->time = -1;
                        if ((kind & 0xF) == 5) {
                            last->u.value[3] = 0;
                            last->u.value[4] = 0;
                            last->u.value[5] = 0;
                        }
                    }
                }
            } else {
                if (slot->tag == tag) {
                    result |= 1;
                }
                result |= 0x100;
            }
        rot_done:
            parts->rotate = 1;
            parts->dirty = 1;
        }
        if (parts->attachments[1] != NULL) {
            slot = parts->attachments[1];
            pos_kind = slot->kind;
            switch (pos_kind & 0xF) {
            case 0:
                track = slot->u.track.pos;
                if (!(pos_kind & 0x10)) {
                    parts->pos[0] = *(s16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                }
                if (!(pos_kind & 0x20)) {
                    parts->pos[1] = *(s16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                }
                if (!(pos_kind & 0x40)) {
                    parts->pos[2] = *(s16 *)track;
                    slot->u.track.pos += 2;
                }
                break;
            case 1:
                if (!(pos_kind & 0x10)) {
                    delta = *(s8 *)slot->u.track.pos++;
                    if (delta != -0x80) {
                        parts->pos[0] += delta;
                    } else {
                        low = *slot->u.track.pos++;
                        high = *(s8 *)slot->u.track.pos++;
                        parts->pos[0] = low | (high << 8);
                    }
                }
                if (!(pos_kind & 0x20)) {
                    delta = *(s8 *)slot->u.track.pos++;
                    if (delta != -0x80) {
                        parts->pos[1] += delta;
                    } else {
                        low = *slot->u.track.pos++;
                        high = *(s8 *)slot->u.track.pos++;
                        parts->pos[1] = low | (high << 8);
                    }
                }
                if (!(pos_kind & 0x40)) {
                    delta = *(s8 *)slot->u.track.pos++;
                    if (delta != -0x80) {
                        parts->pos[2] += delta;
                    } else {
                        low = *slot->u.track.pos++;
                        high = *(s8 *)slot->u.track.pos++;
                        parts->pos[2] = low | (high << 8);
                    }
                }
                break;
            case 2:
                track = slot->u.track.pos;
                if (!(pos_kind & 0x10)) {
                    move.vx = *(u16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                } else {
                    move.vx = 0;
                }
                if (!(pos_kind & 0x20)) {
                    move.vy = *(u16 *)track;
                    track += 2;
                    slot->u.track.pos += 2;
                } else {
                    move.vy = 0;
                }
                if (!(pos_kind & 0x40)) {
                    move.vz = *(u16 *)track;
                    slot->u.track.pos += 2;
                } else {
                    move.vz = 0;
                }
                move.vx = move.vx * parts->scale[0] >> 12;
                move.vy = move.vy * parts->scale[1] >> 12;
                move.vz = move.vz * parts->scale[2] >> 12;
                ApplyMatrix(&parts->world, &move, &moved);
                parts->pos[0] += scale * moved.vx >> 12;
                parts->pos[1] += scale * moved.vy >> 12;
                parts->pos[2] += scale * moved.vz >> 12;
                break;
            case 3:
                time = (s16)(slot->time + 1);
                parts->pos[0] = slot->u.value[0] + slot->u.value[3] * time / slot->duration;
                parts->pos[1] = slot->u.value[1] + slot->u.value[4] * time / slot->duration;
                last = slot;
                parts->pos[2] = slot->u.value[2] + slot->u.value[5] * time / slot->duration;
                break;
            case 4:
                sx = (slot->u.value[3] - parts->pos[0]) / slot->duration;
                sy = (slot->u.value[4] - parts->pos[1]) / slot->duration;
                sz = (slot->u.value[5] - parts->pos[2]) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    parts->pos[0] = slot->u.value[3];
                    parts->pos[1] = slot->u.value[4];
                    parts->pos[2] = slot->u.value[5];
                } else {
                    parts->pos[0] += sx;
                    parts->pos[1] += sy;
                    parts->pos[2] += sz;
                    slot->time = 0;
                }
                break;
            case 5:
                slot->u.value[0] += slot->u.value[3];
                parts->pos[0] += slot->u.value[0];
                slot->u.value[1] += slot->u.value[4];
                parts->pos[1] += slot->u.value[1];
                slot->u.value[2] += slot->u.value[5];
                last = slot;
                parts->pos[2] += slot->u.value[2];
                break;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->flag) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_801DF7A8(pool, slot);
                    parts->attachments[1] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    if ((pos_kind & 0xF) < 3) {
                        slot->time = 0;
                        slot->u.track.pos = slot->u.track.start;
                    } else {
                        slot->time = -1;
                        if ((pos_kind & 0xF) == 5) {
                            last->u.value[3] = 0;
                            last->u.value[4] = 0;
                            last->u.value[5] = 0;
                        }
                    }
                }
            } else {
                if (slot->tag == tag) {
                    result |= 1;
                }
                result |= 0x100;
            }
            parts->dirty = 1;
        }
        if (parts->attachments[2] != NULL) {
            slot = parts->attachments[2];
            mode = slot->kind & 0xF;
            switch (mode) {
            case 3:
                time = (s16)(slot->time + 1);
                parts->scale[0] = slot->u.value[0] + slot->u.value[3] * time / slot->duration;
                parts->scale[1] = slot->u.value[1] + slot->u.value[4] * time / slot->duration;
                last = slot;
                parts->scale[2] = slot->u.value[2] + slot->u.value[5] * time / slot->duration;
                break;
            case 4:
                sx = (slot->u.value[3] - slot->u.value[0]) / slot->duration;
                sy = (slot->u.value[4] - slot->u.value[1]) / slot->duration;
                sz = (slot->u.value[5] - slot->u.value[2]) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    parts->scale[0] = slot->u.value[3];
                    parts->scale[1] = slot->u.value[4];
                    parts->scale[2] = slot->u.value[5];
                } else {
                    last->u.value[0] += sx;
                    last->u.value[1] += sy;
                    last->u.value[2] += sz;
                    parts->scale[0] = last->u.value[0];
                    parts->scale[1] = last->u.value[1];
                    parts->scale[2] = last->u.value[2];
                    slot->time = 0;
                }
                break;
            case 5:
                slot->u.value[0] += slot->u.value[3];
                parts->scale[0] += slot->u.value[0];
                slot->u.value[1] += slot->u.value[4];
                parts->scale[1] += slot->u.value[1];
                slot->u.value[2] += slot->u.value[5];
                last = slot;
                parts->scale[2] += slot->u.value[2];
                break;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->flag) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_801DF7A8(pool, slot);
                    parts->attachments[2] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    slot->time = -1;
                    if (mode == 5) {
                        last->u.value[3] = 0;
                        last->u.value[4] = 0;
                        last->u.value[5] = 0;
                    }
                }
            } else {
                if (slot->tag == tag) {
                    result |= 1;
                }
                result |= 0x100;
            }
            parts->rotate = 1;
            parts->dirty = 1;
        }
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DDBF8);
#endif

/* Apply a keyframe to the nodes after the root: each rotation or position
 * that changed is set unless the node's tween is kept (tag 0xff). Returns
 * the node count. */
u16 func_801DEF10(ModelPart *parts, s16 *data) {
    u16 rotations;
    u16 positions;
    u16 rot_count;
    u16 pos_count;
    u16 flags;
    u16 count;
    s32 x;
    s32 y;
    s32 z;
    s32 i;

    rotations = 0;
    positions = 0;
    i = data[3];
    rot_count = ((Keyframe *)data)->rot_count;
    pos_count = ((Keyframe *)data)->pos_count;
    flags = ((Keyframe *)data)->flags;
    data += sizeof(Keyframe) / sizeof(s16);
    if (i == 0) {
        data += (rot_count + 1) * 3;
    }
    count = parts->count - 1;
    for (i = 0; i < count; i++) {
        parts++;
        if (!(flags & 1) && rotations < rot_count) {
            x = *data++;
            y = *data++;
            z = *data++;
            rotations++;
            if ((parts->rot.vx != x || parts->rot.vy != y || parts->rot.vz != z) &&
                (parts->attachments[0] == NULL || parts->attachments[0]->tag != 0xFF)) {
                parts->rot.vx = x;
                parts->rot.vy = y;
                parts->rot.vz = z;
                parts->dirty = 1;
                parts->rotate = 1;
            }
        }
        if (!(flags & 2) && positions < pos_count) {
            x = *data++;
            y = *data++;
            z = *data++;
            positions++;
            if ((parts->pos[0] != x || parts->pos[1] != y || parts->pos[2] != z) &&
                (parts->attachments[1] == NULL || parts->attachments[1]->tag != 0xFF)) {
                parts->pos[0] = x;
                parts->pos[1] = y;
                parts->pos[2] = z;
                parts->dirty = 1;
            }
        }
    }
    return count;
}

/* Tween the nodes after the root towards a keyframe over `duration` ticks
 * (at least 1): each changed rotation gets a tween (kind mode + 3) of the
 * shortest angle differences (mode 1: to the absolute angles), each changed
 * position one of its movement (mode 1: to the absolute position); kept
 * tweens (tag 0xff) stay, other nodes lose theirs. Returns the node count. */
u16 func_801DF0B4(SlotPool *pool, ModelPart *parts, s16 *data, s32 duration, s32 mode,
                  s32 smooth, s32 tag) {
    PoolSlot *tween;
    u16 rot_count;
    u16 rotations;
    u16 pos_count;
    u16 positions;
    u16 count;
    u16 flags;
    s32 x;
    s32 y;
    s32 z;
    s32 i;

    if (duration == 0) {
        duration = 1;
    }
    rotations = 0;
    positions = 0;
    smooth &= 1;
    rot_count = ((Keyframe *)data)->rot_count;
    i = data[3];
    pos_count = ((Keyframe *)data)->pos_count;
    mode &= 1;
    flags = ((Keyframe *)data)->flags;
    data += sizeof(Keyframe) / sizeof(s16);
    if (i == 0) {
        data += (rot_count + 1) * 3;
    }
    count = parts->count - 1;
    for (i = 0; i < count; i++) {
        parts++;
        if (!(flags & 1) && rotations < rot_count) {
            x = *data++;
            y = *data++;
            z = *data++;
            rotations++;
            if (parts->rot.vx != x || parts->rot.vy != y || parts->rot.vz != z) {
                if (parts->attachments[0] != NULL) {
                    tween = parts->attachments[0];
                    if (tween->tag == 0xFF) {
                        goto positions; /* a kept tween stays */
                    }
                } else {
                    tween = func_801DF6F0(pool);
                }
                if (tween != NULL) {
                    tween->used = 1;
                    tween->flag = smooth;
                    tween->kind = mode + 3;
                    tween->tag = tag;
                    tween->u.value[0] = parts->rot.vx;
                    tween->u.value[1] = parts->rot.vy;
                    tween->u.value[2] = parts->rot.vz;
                    x = (x - parts->rot.vx) & 0xFFF;
                    if (x >= 0x800) {
                        x -= 0x1000;
                    }
                    tween->u.value[3] = x;
                    y = (y - parts->rot.vy) & 0xFFF;
                    if (y >= 0x800) {
                        y -= 0x1000;
                    }
                    tween->u.value[4] = y;
                    z = (z - parts->rot.vz) & 0xFFF;
                    if (z >= 0x800) {
                        z -= 0x1000;
                    }
                    tween->u.value[5] = z;
                    if (mode) {
                        tween->u.value[3] += parts->rot.vx;
                        tween->u.value[4] += parts->rot.vy;
                        tween->u.value[5] += parts->rot.vz;
                    }
                    tween->time = 0;
                    tween->duration = duration;
                    parts->attachments[0] = tween;
                    goto positions;
                }
            }
        }
        if (parts->attachments[0] != NULL && parts->attachments[0]->tag != 0xFF) {
            func_801DF7A8(pool, parts->attachments[0]);
            parts->attachments[0] = NULL;
        }
    positions:
        if (!(flags & 2) && positions < pos_count) {
            x = *data++;
            y = *data++;
            z = *data++;
            positions++;
            if (parts->pos[0] != x || parts->pos[1] != y || parts->pos[2] != z) {
                if (parts->attachments[1] != NULL) {
                    tween = parts->attachments[1];
                    if (tween->tag == 0xFF) {
                        continue; /* a kept tween stays */
                    }
                } else {
                    tween = func_801DF6F0(pool);
                }
                if (tween != NULL) {
                    tween->used = 1;
                    tween->flag = smooth;
                    tween->kind = mode + 3;
                    tween->tag = tag;
                    tween->u.value[0] = parts->pos[0];
                    tween->u.value[1] = parts->pos[1];
                    tween->u.value[2] = parts->pos[2];
                    if (mode) {
                        tween->u.value[3] = x;
                        tween->u.value[4] = y;
                        tween->u.value[5] = z;
                    } else {
                        tween->u.value[3] = x - parts->pos[0];
                        tween->u.value[4] = y - parts->pos[1];
                        tween->u.value[5] = z - parts->pos[2];
                    }
                    tween->time = 0;
                    tween->duration = duration;
                    parts->attachments[1] = tween;
                    continue;
                }
            }
        }
        if (parts->attachments[1] != NULL && parts->attachments[1]->tag != 0xFF) {
            func_801DF7A8(pool, parts->attachments[1]);
            parts->attachments[1] = NULL;
        }
    }
    return count;
}

/* Release the attachments of node `index` selected by `mask` (bit 0: w70,
 * bit 1: w74, bit 2: w78) back to `pool`. */
void func_801DF52C(SlotPool *pool, ModelPart *part, s32 index, s32 mask) {
    if (index < part->count) {
        part += index;
        if (part->attachments[0] != NULL && (mask & 1)) {
            func_801DF7A8(pool, part->attachments[0]);
            part->attachments[0] = NULL;
        }
        if (part->attachments[1] != NULL && (mask & 2)) {
            func_801DF7A8(pool, part->attachments[1]);
            part->attachments[1] = NULL;
        }
        if (part->attachments[2] != NULL && (mask & 4)) {
            func_801DF7A8(pool, part->attachments[2]);
            part->attachments[2] = NULL;
        }
    }
}

/* Allocate `capacity` 0x14-byte slots for a pool, all free. */
SlotPool *func_801DF5F4(SlotPool *pool, s32 capacity) {
    if (capacity <= 0 ||
        (pool->capacity = capacity, func_80032498(4, 0),
         (pool->slots = func_80031BDC(capacity * sizeof(PoolSlot), 0)) == NULL)) {
        return NULL;
    }
    func_801DF6A8(pool);
    return pool;
}

/* Release a pool's slots. */
void func_801DF668(SlotPool *pool) {
    pool->next = 0;
    if (pool->slots != NULL) {
        func_800320E8(pool->slots);
    }
    pool->slots = NULL;
}

/* Mark every slot of a pool free. */
void func_801DF6A8(SlotPool *pool) {
    PoolSlot *slot;
    s32 i;

    if (pool->slots != NULL) {
        slot = pool->slots;
        pool->next = 0;
        for (i = 0; i < pool->capacity; i++) {
            slot->used = 0;
            slot++;
        }
    }
}

/* Take the first free slot (the caller marks it used) and advance the search
 * position past used slots; NULL when the pool is full. */
PoolSlot *func_801DF6F0(SlotPool *pool) {
    PoolSlot *slot;
    u32 capacity;

    if (pool->next < pool->capacity) {
        slot = &pool->slots[pool->next];
        if (slot->used) {
            return NULL;
        }
        pool->next++;
        capacity = pool->capacity;
        while (pool->next < capacity && pool->slots[pool->next].used != 0) {
            pool->next++;
        }
        return slot;
    }
    return NULL;
}

/* Free a slot, moving the search position back to it; returns its index or
 * -1 without a slot. */
s32 func_801DF7A8(SlotPool *pool, PoolSlot *slot) {
    s32 index;

    if (slot == NULL) {
        return -1;
    }
    index = ((u32)slot - (u32)pool->slots) / sizeof(PoolSlot);
    if (index < pool->next) {
        pool->next = index;
    }
    slot->used = 0;
    return index;
}

/* Start a keyframe on a hierarchy through track tweens (a packed keyframe
 * is applied at once): each node with a track gets a tween (unless it holds
 * a kept one), a node without one loses its tween. The keyframe is walked
 * with a u16 cursor. */
s32 func_801DF7F4(SlotPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag) {
    Keyframe *key;
    u8 *kind;
    PoolSlot *tween;
    u8 *values;
    u16 count;
    u16 rot_count;
    u16 pos_count;
    u16 duration;
    u16 flags;
    s32 i;

    key = (Keyframe *)data;
    if (key->packed != 0) {
        func_801DFE8C(pool, parts);
        func_801DEF10(parts, (s16 *)key);
        return 1;
    }
    rot_count = key->rot_count;
    count = parts->count;
    pos_count = key->pos_count;
    if (rot_count + 1 < count) {
        count = rot_count + 1;
    }
    mode &= 1;
    duration = key->duration;
    if (!mode) {
        duration--;
    }
    flags = key->flags;
    data = (u16 *)(key + 1);
    values = (u8 *)data + (rot_count + 1) * sizeof(TrackEntry);
    if (!(flags & 1)) {
        values += rot_count * 6;
    }
    if (!(flags & 2)) {
        values += pos_count * 6;
    }
    for (i = 0; i < count; i++) {
        kind = (u8 *)(data + 2);
        if (*data != 0xFFFF) {
            if (parts->attachments[0] != NULL) {
                tween = parts->attachments[0];
                if (tween->tag == 0xFF) {
                    goto kept0; /* a kept tween stays */
                }
            } else {
                tween = func_801DF6F0(pool);
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->flag = mode;
                tween->kind = kind[0];
                tween->tag = tag;
                tween->u.track.pos = tween->u.track.start = values + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->attachments[0] = tween;
            }
        } else {
            if (parts->attachments[0] != NULL && i != 0 && parts->attachments[0]->tag != 0xFF) {
                func_801DF7A8(pool, parts->attachments[0]);
                parts->attachments[0] = NULL;
            }
        }
    kept0:
        data++;
        if (*data != 0xFFFF) {
            if (parts->attachments[1] != NULL) {
                tween = parts->attachments[1];
                if (tween->tag == 0xFF) {
                    goto kept1; /* a kept tween stays */
                }
            } else {
                tween = func_801DF6F0(pool);
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->flag = mode;
                tween->kind = kind[1];
                tween->tag = tag;
                tween->u.track.pos = tween->u.track.start = values + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->attachments[1] = tween;
            }
        } else {
            if (parts->attachments[1] != NULL && i != 0 && parts->attachments[1]->tag != 0xFF) {
                func_801DF7A8(pool, parts->attachments[1]);
                parts->attachments[1] = NULL;
            }
        }
    kept1:
        data += 2;
        parts++;
    }
    return 0;
}

/* Like func_801DF7F4, but each node after the root first takes the
 * keyframe's start values for the tracks that are started. */
s32 func_801DFAC4(SlotPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag) {
    Keyframe *key;
    u8 *kind;
    PoolSlot *tween;
    s16 *values;
    u8 *tracks;
    u16 rot_count;
    u16 pos_count;
    u16 count;
    u16 duration;
    u16 rotations;
    u16 positions;
    u16 flags;
    s32 i;

    key = (Keyframe *)data;
    if (key->packed != 0) {
        func_801DFE8C(pool, parts);
        func_801DEF10(parts, (s16 *)key);
        return 1;
    }
    mode &= 1;
    rotations = 0;
    rot_count = key->rot_count;
    count = parts->count;
    positions = 0;
    pos_count = key->pos_count;
    if (rot_count + 1 < count) {
        count = rot_count + 1;
    }
    duration = key->duration;
    if (!mode) {
        duration--;
    }
    flags = key->flags;
    data = (u16 *)(key + 1);
    values = (s16 *)(data + (rot_count + 1) * 3);
    tracks = (u8 *)values;
    if (!(flags & 1)) {
        tracks += rot_count * 6;
    }
    if (!(flags & 2)) {
        tracks += pos_count * 6;
    }
    for (i = 0; i < count; i++) {
        kind = (u8 *)(data + 2);
        if (*data != 0xFFFF) {
            if (parts->attachments[0] != NULL) {
                tween = parts->attachments[0];
                if (tween->tag == 0xFF) {
                    goto skip0; /* a kept tween stays */
                }
            } else {
                tween = func_801DF6F0(pool);
            }
            if (!(flags & 1) && i != 0 && rotations < rot_count) {
                parts->rot.vx = *values++;
                parts->rot.vy = *values++;
                parts->rot.vz = *values++;
                rotations++;
                parts->dirty = 1;
                parts->rotate = 1;
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->flag = mode;
                tween->kind = kind[0];
                tween->tag = tag;
                tween->u.track.pos = tween->u.track.start = tracks + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->attachments[0] = tween;
            }
        } else {
        skip0:
            if (!(flags & 1) && i != 0 && rotations < rot_count) {
                values += 3;
                rotations++;
            }
        }
        data++;
        if (*data != 0xFFFF) {
            if (parts->attachments[1] != NULL) {
                tween = parts->attachments[1];
                if (tween->tag == 0xFF) {
                    goto skip1; /* a kept tween stays */
                }
            } else {
                tween = func_801DF6F0(pool);
            }
            if (!(flags & 2) && i != 0 && positions < pos_count) {
                parts->pos[0] = *values++;
                parts->pos[1] = *values++;
                parts->pos[2] = *values++;
                positions++;
                parts->dirty = 1;
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->flag = mode;
                tween->kind = kind[1];
                tween->tag = tag;
                tween->u.track.pos = tween->u.track.start = tracks + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->attachments[1] = tween;
            }
        } else {
        skip1:
            if (!(flags & 2) && i != 0 && positions < pos_count) {
                values += 3;
                positions++;
            }
        }
        data += 2;
        parts++;
    }
    return 0;
}

/* Release every node's attachments except those tagged 0xff. */
void func_801DFE8C(SlotPool *pool, ModelPart *parts) {
    u16 count;
    s32 i;

    count = parts->count;
    for (i = 0; i < count; i++, parts++) {
        if (parts->attachments[0] != NULL && parts->attachments[0]->tag != 0xFF) {
            func_801DF7A8(pool, parts->attachments[0]);
            parts->attachments[0] = NULL;
        }
        if (parts->attachments[1] != NULL && parts->attachments[1]->tag != 0xFF) {
            func_801DF7A8(pool, parts->attachments[1]);
            parts->attachments[1] = NULL;
        }
        if (parts->attachments[2] != NULL && parts->attachments[2]->tag != 0xFF) {
            func_801DF7A8(pool, parts->attachments[2]);
            parts->attachments[2] = NULL;
        }
    }
}

/* Release every node's attachments tagged `tag`. */
void func_801DFF78(SlotPool *pool, ModelPart *parts, u8 tag) {
    u16 count;
    s32 i;

    count = parts->count;
    for (i = 0; i < count; i++, parts++) {
        if (parts->attachments[0] != NULL && parts->attachments[0]->tag == tag) {
            func_801DF7A8(pool, parts->attachments[0]);
            parts->attachments[0] = NULL;
        }
        if (parts->attachments[1] != NULL && parts->attachments[1]->tag == tag) {
            func_801DF7A8(pool, parts->attachments[1]);
            parts->attachments[1] = NULL;
        }
        if (parts->attachments[2] != NULL && parts->attachments[2]->tag == tag) {
            func_801DF7A8(pool, parts->attachments[2]);
            parts->attachments[2] = NULL;
        }
    }
}

/* Allocate a particle pool of `capacity` particles plus a spare one returned
 * when the pool is full, and initialise them. */
ParticlePool *func_801E0064(ParticlePool *pool, s32 capacity) {
    func_80032498(4, 0);
    pool->capacity = capacity;
    pool->next = 0;
    pool->items = func_80031BDC((capacity + 1) * sizeof(Particle), 0);
    if (pool->items != NULL) {
        func_801E011C(pool);
        return pool;
    }
    return NULL;
}

/* Release a particle pool. */
void func_801E00DC(ParticlePool *pool) {
    pool->capacity = 0;
    pool->next = 0;
    if (pool->items != NULL) {
        func_800320E8(pool->items);
    }
    pool->items = NULL;
}

/* Mark every particle free and set up both buffers' semi-transparent textured
 * quads (a 16x1 texel strip at v=0xbd of page (0x340, 0x100), clut (0, 0x1cd)). */
void func_801E011C(ParticlePool *pool) {
    Particle *particle;
    POLY_FT4 *poly;
    s32 i;
    s32 j;

    particle = pool->items;
    for (i = 0; i < pool->capacity + 1; i++) {
        particle->age = -1;
        particle->lifetime = 0;
        for (j = 0; j < 2; j++) {
            poly = &particle->poly[j];
            SetPolyFT4(poly);
            SetSemiTrans(poly, 1);
            particle->poly[j].clut = GetClut(0, 0x1CD);
            particle->poly[j].tpage = GetTPage(0, 1, 0x340, 0x100);
            particle->poly[j].u0 = 0;
            particle->poly[j].v0 = 0xBD;
            particle->poly[j].u1 = 0;
            particle->poly[j].v1 = 0xBD;
            particle->poly[j].u2 = 0xF;
            particle->poly[j].v2 = 0xBD;
            particle->poly[j].u3 = 0xF;
            particle->poly[j].v3 = 0xBD;
        }
        particle++;
    }
}

/* Take the next free particle, setting both quads' semi-transparency; when the
 * pool is full, the spare particle past its end. */
Particle *func_801E0248(ParticlePool *pool, s16 semi_trans) {
    Particle *particle;

    if (pool->next < pool->capacity) {
        particle = &pool->items[pool->next];
        if (particle->age == -1) {
            pool->next++;
            while (pool->next < pool->capacity) {
                if (pool->items[pool->next].age == -1) {
                    break;
                }
                pool->next++;
            }
            SetSemiTrans(&particle->poly[0], semi_trans);
            SetSemiTrans(&particle->poly[1], semi_trans);
            return particle;
        }
    }
    return &pool->items[pool->capacity];
}

/* Free a particle, moving the search position back to it; returns its index. */
s32 func_801E0354(ParticlePool *pool, Particle *particle) {
    s32 index;

    index = ((u32)particle - (u32)pool->items) / sizeof(Particle);
    if (pool->next >= index) {
        pool->next = index;
    }
    particle->age = -1;
    return index;
}

/* Draw the live particles into the ordering table (3D ones projected with
 * the GTE at their depth, 2D ones at the front), free the expired ones and
 * fade the rest by `steps` ticks. */
void func_801E0398(ParticlePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer) {
    Particle *particle;
    s32 otz;
    s32 i;

    SetRotMatrix(m);
    SetTransMatrix(m);
    particle = pool->items;
    for (i = 0; i < pool->capacity; i++, particle++) {
        if (particle->age == -1) {
            continue;
        }
        if (particle->age >= particle->lifetime) {
            func_801E0354(pool, particle);
            continue;
        }
        particle->poly[buffer].r0 = particle->color[0] >> 6;
        particle->poly[buffer].g0 = particle->color[1] >> 6;
        particle->poly[buffer].b0 = particle->color[2] >> 6;
        if (particle->projected == 0) {
            particle->poly[buffer].x0 = particle->x0;
            particle->poly[buffer].y0 = particle->y0;
            particle->poly[buffer].x1 = particle->x1;
            particle->poly[buffer].y1 = particle->y1;
            particle->poly[buffer].x2 = particle->x2;
            particle->poly[buffer].y2 = particle->y2;
            particle->poly[buffer].x3 = particle->x3;
            particle->poly[buffer].y3 = particle->y3;
            addPrim(ot, &particle->poly[buffer]);
        } else {
            gte_ldv3(&particle->x0, &particle->x1, &particle->x2);
            gte_rtpt();
            gte_stsxy3(&particle->poly[buffer].x0, &particle->poly[buffer].x1, &particle->poly[buffer].x2);
            gte_stszotz(&otz);
            otz >>= D_80050100;
            gte_ldv0(&particle->x3);
            gte_rtps();
            gte_stsxy(&particle->poly[buffer].x3);
            addPrim(ot + otz, &particle->poly[buffer]);
        }
        particle->age += steps;
        particle->color[0] -= particle->fade[0] * steps;
        particle->color[1] -= particle->fade[1] * steps;
        particle->color[2] -= particle->fade[2] * steps;
    }
}

/* Set up a colour fade from (r0, g0, b0) to (r1, g1, b1) over `duration`
 * ticks. */
s32 func_801E0698(ColorFade *fade, s32 w4, s16 h0, u8 b2, s16 h60, s16 duration, u8 r0, u8 g0,
                  u8 b0, u8 r1, u8 g1, u8 b1, u16 hC, u16 hE, u16 h10, u16 h14, u16 h16, u16 h18,
                  u16 b3) {
    if (fade != NULL) {
        fade->b3 = b3;
        fade->h5E = -1;
        fade->h0 = h0;
        fade->b2 = b2;
        fade->w4 = w4;
        fade->hC = hC;
        fade->hE = hE;
        fade->h10 = h10;
        fade->h14 = h14;
        fade->h16 = h16;
        fade->h18 = h18;
        fade->time = 0;
        if (h60 < 7) {
            fade->h60 = h60;
        } else {
            fade->h60 = 7;
        }
        fade->color[0] = r0 << 6;
        fade->color[1] = g0 << 6;
        fade->color[2] = b0 << 6;
        fade->duration = duration;
        fade->w8 = 0;
        fade->step[0] = (fade->color[0] - (r1 << 6)) / duration;
        fade->step[1] = (fade->color[1] - (g1 << 6)) / duration;
        fade->step[2] = (fade->color[2] - (b1 << 6)) / duration;
        return 0;
    }
}

/* Mark a particle or channel record free. */
void func_801E0844(s16 *id, s32 unused) {
    *id = -1;
}

/* base + (cos(angle) + 1.0) / divisor. */
s16 func_801E0850(s16 angle, s16 divisor, s32 base) {
    return base + (func_8003F8CC(angle) + 0x1000) / divisor;
}

/* base + value / divisor, or -1 past 32. */
s16 func_801E08D4(s16 value, s16 divisor, s16 base) {
    base += value / divisor;
    if (base > 0x20) {
        return -1;
    }
    return base;
}

/* base - value / divisor. */
s16 func_801E0938(s16 value, s16 divisor, s32 base) {
    return base - value / divisor;
}

/* 32 - value / divisor, at least `minimum`. */
s16 func_801E0988(s16 value, s16 divisor, s16 minimum) {
    s16 result;

    result = 0x20 - value / divisor;
    if (result < minimum) {
        result = minimum;
    }
    return result;
}

/* Start an image animation (once): its target, curve and timing, the w x h
 * VRAM rectangle at (x3, y3) (256 by default; modes 0/1 use an even width),
 * the work and frame buffers `flags` bits 8-10 ask for, and each frame
 * buffer's first contents (flags nibbles 0 and 1): 1 the VRAM rectangle at
 * (x, y) / (x2, y2) (modes 4/5: rows of `colors` from there), 2 one colour
 * (modes 4/5: the three values cycling by row). */
ImageAnim *func_801E0A00(ImageAnim *anim, ImageAnim *target, u16 mode, u16 flags, ColorRow *colors,
                         s16 x, s16 y, s16 z, s16 x2, s16 y2, s16 z2, s16 x3, s16 y3, s16 w, s16 h,
                         s16 speed, s16 divisor, s16 base, FrameCurve curve) {
    RECT rect;
    s32 half;
    s32 i, col;
    u16 color, fill;

    if (anim->active != 0) {
        return NULL;
    }
    func_80032498(4, 0);
    anim->active = 1;
    anim->mode = mode;
    anim->dirty = 0;
    anim->target = target;
    anim->time = 0;
    anim->frame = 0xFFFF;
    anim->speed = speed;
    anim->divisor = divisor;
    anim->base = base;
    anim->curve = curve;
    anim->colors = colors;
    if (!(mode & 1)) {
        flags &= 0xFD0F;
    }
    if (mode & 4) {
        flags &= 0xFEFF;
    }
    if (w == 0) {
        w = 0x100;
    }
    if (h == 0) {
        h = 0x100;
    }
    switch (mode) {
    case 0:
    case 1:
        half = (w + 1) / 2;
        w = half * 2;
        anim->rect.x = x3;
        anim->rect.y = y3;
        anim->rect.w = w;
        anim->rect.h = h;
        anim->h12 = w * h;
        if (flags & 0x100) {
            anim->work = func_80031BDC((s16)(half * 2) * h * 2, 0);
        }
        if (flags & 0x200) {
            anim->pixels2 = func_80031BDC((s16)(half * 2) * h * 2, 0);
        }
        if (flags & 0x400) {
            anim->pixels = func_80031BDC((s16)(half * 2) * h * 2, 0);
        }
        switch (flags & 0xF) {
        case 1:
            rect.x = x;
            rect.y = y;
            rect.w = w;
            rect.h = h;
            StoreImage(&rect, anim->pixels);
            DrawSync(0);
            break;
        case 2:
            fill = ((z & 0x3F) << 10) + ((y & 0x1F) << 5) | (x & 0x1F);
            for (i = 0; i < (s16)(half * 2) * h; i++) {
                anim->pixels[i] = fill;
            }
            break;
        }
        switch ((flags >> 4) & 0xF) {
        case 1:
            rect.x = x2;
            rect.y = y2;
            rect.w = w;
            rect.h = h;
            StoreImage(&rect, anim->pixels2);
            DrawSync(0);
            break;
        case 2:
            fill = ((z2 & 0x3F) << 10) + ((y2 & 0x1F) << 5) | (x2 & 0x1F);
            for (i = 0; i < w * h; i++) {
                anim->pixels2[i] = fill;
            }
            break;
        }
        break;
    case 4:
    case 5:
        anim->rect.x = x3;
        anim->rect.y = y3;
        anim->rect.w = w;
        anim->rect.h = h;
        anim->h12 = w * h;
        if (flags & 0x100) {
            anim->work = func_80031BDC(w * h * 2, 0);
        }
        if (flags & 0x200) {
            anim->pixels2 = func_80031BDC(w * h * 2, 0);
        }
        if (flags & 0x400) {
            anim->pixels = func_80031BDC(w * h * 2, 0);
        }
        switch (flags & 0xF) {
        case 1:
            for (i = 0; i < h; i++) {
                for (col = 0; col < w; col++) {
                    anim->pixels[i * w + col] = colors[y + i].c[x + col];
                }
            }
            break;
        case 2:
            for (i = 0; i < h; i++) {
                for (col = 0; col < w; col++) {
                    switch ((y3 + i) % 3) {
                    case 0:
                        color = x;
                        break;
                    case 1:
                        color = y;
                        break;
                    case 2:
                        color = z;
                        break;
                    }
                    anim->pixels[i * w + col] = color;
                }
            }
            break;
        }
        switch ((flags >> 4) & 0xF) {
        case 1:
            for (i = 0; i < h; i++) {
                for (col = 0; col < w; col++) {
                    anim->pixels2[i * w + col] = colors[y2 + i].c[x2 + col];
                }
            }
            break;
        case 2:
            for (i = 0; i < h; i++) {
                for (col = 0; col < w; col++) {
                    switch ((y3 + i) % 3) {
                    case 0:
                        color = x2;
                        break;
                    case 1:
                        color = y2;
                        break;
                    case 2:
                        color = z2;
                        break;
                    }
                    anim->pixels2[i * w + col] = color;
                }
            }
            break;
        }
        break;
    }
    return anim;
}

/* Advance an image animation by `ticks` + 1: when its curve selects another
 * frame, rebuild the image (resident decoders or fades) and copy the
 * overlap into its target image. Returns the frame, or a negative value
 * once the animation ended; an unchanged frame returns nothing (the
 * original falls off the end). */
s16 func_801E1258(ImageAnim *anim, s32 ticks) {
    RECT src;
    RECT dst;
    ImageAnim *target;
    u16 *pixels;
    u16 *work;
    s16 frame;
    s32 x;
    s32 y;

    if (!anim->active) {
        return -1;
    }
    anim->time += anim->speed * (ticks + 1);
    frame = anim->curve(anim->time, anim->divisor, anim->base);
    if (frame < 0) {
        func_801E165C(anim);
        return frame;
    }
    if (frame != anim->frame) {
        anim->frame = frame;
        switch (anim->mode) {
        case 0:
            func_80026F44(anim->h12, frame, anim->work, anim->pixels);
            if (anim->target == NULL) {
                LoadImage(&anim->rect, anim->work);
            }
            break;
        case 1:
            func_80026FE8(anim->h12, frame, anim->work, anim->pixels2, anim->pixels);
            if (anim->target == NULL) {
                LoadImage(&anim->rect, anim->work);
            }
            break;
        case 4:
            func_801E1708(anim, frame);
            break;
        case 5:
            func_801E17B8(anim, frame);
            break;
        }
        target = anim->target;
        if (target != NULL && target->active) {
            if (target->rect.x < anim->rect.x) {
                dst.x = anim->rect.x - target->rect.x;
                src.x = 0;
                dst.w = target->rect.x + target->rect.w - anim->rect.x;
            } else {
                dst.x = 0;
                src.x = target->rect.x - anim->rect.x;
                dst.w = anim->rect.x + anim->rect.w - target->rect.x;
            }
            if (target->rect.y < anim->rect.y) {
                dst.y = anim->rect.y - target->rect.y;
                src.y = 0;
                dst.h = target->rect.y + target->rect.h - anim->rect.y;
            } else {
                dst.y = 0;
                src.y = target->rect.y - anim->rect.y;
                dst.h = anim->rect.y + anim->rect.h - target->rect.y;
            }
            if (dst.w > 0 && dst.h > 0) {
                target->dirty = 1;
                pixels = target->pixels;
                if (target->mode < 4) {
                    work = anim->work;
                    for (y = 0; y < dst.h; y++) {
                        for (x = 0; x < dst.w; x++) {
                            *(pixels + dst.x + x + (dst.y + y) * target->rect.w) =
                                *(work + src.x + x + (src.y + y) * anim->rect.w);
                        }
                    }
                } else {
                    for (y = 0; y < dst.h; y++) {
                        for (x = 0; x < dst.w; x++) {
                            *(pixels + dst.x + x + (dst.y + y) * target->rect.w) =
                                anim->colors[src.y + y].c[src.x + x];
                        }
                    }
                }
            }
        }
        return frame;
    }
}

/* Stop an image animation: restore its original pixels to VRAM (resident
 * decoder modes) and release its blocks. */
void func_801E165C(ImageAnim *anim) {
    if (anim->active) {
        if (anim->pixels != NULL) {
            if (anim->mode < 4) {
                LoadImage(&anim->rect, anim->pixels);
            }
            func_800320E8(anim->pixels);
            anim->pixels = NULL;
        }
        if (anim->pixels2 != NULL) {
            func_800320E8(anim->pixels2);
            anim->pixels2 = NULL;
        }
        if (anim->work != NULL) {
            func_800320E8(anim->work);
            anim->work = NULL;
        }
        anim->active = 0;
    }
}

/* Fade an image animation's colours to `level` / 32 of its pixels. */
void func_801E1708(ImageAnim *anim, s16 level) {
    u16 *pixel;
    s32 x;
    s32 y;
    s32 value;

    pixel = anim->pixels;
    for (y = 0; y < anim->rect.h; y++) {
        for (x = 0; x < anim->rect.w; x++) {
            value = *pixel * level;
            anim->colors[anim->rect.y + y].c[anim->rect.x + x] = value / 32;
            pixel++;
        }
    }
}

/* Blend an image animation's colours from its second pixels towards its
 * first by `level` / 32. */
void func_801E17B8(ImageAnim *anim, s16 level) {
    u16 *pixel;
    u16 *from;
    s32 x;
    s32 y;
    s32 value;

    pixel = anim->pixels;
    from = anim->pixels2;
    for (y = 0; y < anim->rect.h; y++) {
        for (x = 0; x < anim->rect.w; x++) {
            value = (*pixel - *from) * level;
            anim->colors[anim->rect.y + y].c[anim->rect.x + x] = *from + value / 32;
            pixel++;
            from++;
        }
    }
}

/* Place each active anchor: its offset through its actor node's matrix, or
 * the offset itself without an actor. */
void func_801E1880(Actor **actors) {
    VECTOR world;
    MATRIX *m;
    Actor *actor;
    s32 i;

    m = SCRATCH_MATRIX;
    for (i = 0; i < 2; i++) {
        if (D_801E8648[i].active) {
            if (D_801E8648[i].actor >= 0 && (actor = actors[D_801E8648[i].actor]) != NULL) {
                CompMatrix(&actor->parts->local, &actor->parts[D_801E8648[i].node + 1].world, m);
                SetRotMatrix(m);
                SetTransMatrix(m);
                gte_ldv0(&D_801E8648[i].offset);
                gte_rtv0tr();
                gte_stlvnl(&world);
                D_801E8648[i].pos[0] = world.vx;
                D_801E8648[i].pos[1] = world.vy;
                D_801E8648[i].pos[2] = world.vz;
            } else {
                D_801E8648[i].pos[0] = D_801E8648[i].offset.vx;
                D_801E8648[i].pos[1] = D_801E8648[i].offset.vy;
                D_801E8648[i].pos[2] = D_801E8648[i].offset.vz;
            }
        }
    }
}

/* Build a record's surface from `table`: a scaled centre per ring (offset by
 * ox/oy/oz), each strand's points (segment length and sag), and two textured
 * triangles per point pair between neighbouring rings, their texture
 * spanning u_span x v_span from (tx, ty) with the CLUT at (clut_x, clut_y);
 * then `count` zeroed entries. On an allocation failure the record is left
 * empty. The original keeps the loop state in caller-saved registers across
 * SetPolyGT3 (saved on the stack).
 * NON_MATCHING (8 bytes shorter): register allocation and spills differ
 * throughout (the original spills `record` and keeps fewer values live
 * across SetPolyGT3: it saves 8 caller-saved registers there, this C 12). */
#ifdef NON_MATCHING
void func_801E1A14(Record24 *record, u16 *table, s16 angle_base, s32 scale, s16 ox, s16 oy, s16 oz,
                   s32 count, s16 tx, s16 ty, s16 u_span, s16 v_span, s16 clut_x, s16 clut_y, u8 b0,
                   u8 b1, u8 b2, u8 b3, u8 b4, u8 b5) {
    SVECTOR *centres;
    SVECTOR *centre;
    RingPoint **rings;
    RingPoint *points;
    RingPoint *point;
    RingPoly *polys;
    POLY_GT3 *prim;
    u16 *counts;
    u16 *radii;
    u8 *angles;
    u16 tpage, clut;
    s32 page_x, page_y;
    s32 u_base, v_base;
    s16 u_step;
    s32 u, u_next;
    u16 v_step;
    s32 start, first;
    s32 i, k, b;
    u16 n;
    u16 total;

    record->rings = *table++;
    record->polys = *table * 2;
    func_80032498(4, 0);
    table++;
    centres = func_80031BDC(record->rings * sizeof(SVECTOR), 0);
    if (centres == NULL) {
        record->centres = NULL;
        return;
    }
    record->centres = centres;
    for (i = 0; i < record->rings; i++, centres++) {
        centres->vx = (*table++ + ox) * scale / 4096;
        centres->vy = (*table++ + oy) * scale / 4096;
        centres->vz = (*table++ + oz) * scale / 4096;
    }
    total = table[record->rings];
    record->points = total + record->rings;
    rings = func_80031BDC(record->rings * sizeof(RingPoint *), 0);
    if (rings == NULL) {
        record->centres = NULL;
        func_800320E8(NULL);
        return;
    }
    record->block1C = rings;
    counts = table;
    radii = table + record->rings + 1;
    angles = (u8 *)(radii + total);
    points = func_80031BDC((total + record->rings) * sizeof(RingPoint), 0);
    if (points == NULL) {
        record->centres = NULL;
        func_800320E8(NULL);
        func_800320E8(record->block1C);
        return;
    }
    centre = record->centres;
    point = points;
    for (i = 0; i < record->rings; i++, counts++, centre++) {
        *rings++ = point;
        for (k = 0; k < *counts; k++, point++) {
            point->length = *radii++ * scale / 4096;
            point->sag = *angles++ + angle_base;
            point->pos[0] = centre->vx;
            point->pos[1] = centre->vy;
            point->pos[2] = centre->vz;
        }
        point->length = 0;
        point->sag = 0;
        point->pos[0] = centre->vx;
        point->pos[1] = centre->vy;
        point->pos[2] = centre->vz;
        point++;
    }
    counts = table;
    polys = func_80031BDC(record->polys * sizeof(RingPoly), 0);
    if (polys == NULL) {
        record->centres = NULL;
        func_800320E8(record->block1C);
        func_800320E8(points);
        return;
    }
    record->block20 = polys;
    page_x = tx / 64;
    page_y = ty / 256;
    tpage = GetTPage(0, 1, (s16)(page_x << 6), (s16)(page_y << 8));
    clut = GetClut(clut_x, clut_y);
    u_base = (tx - (s16)(page_x << 6)) * 4;
    v_base = ty - (page_y << 8);
    start = 0;
    u_step = u_span / (record->rings - 1);
    for (i = 0, u = 0, u_next = u_step; i < record->rings - 1; i++, u += u_step, u_next += u_step) {
        n = counts[1];
        if (counts[0] < n) {
            n = counts[0];
        }
        v_step = v_span / n;
        first = start;
        for (k = 0; k < n; k++, first++) {
            polys->index[0] = first;
            polys->index[2] = first + 1;
            polys->index[1] = first + counts[0] + 1;
            for (b = 0; b < 2; b++) {
                prim = &polys->prim[b];
                SetPolyGT3(prim);
                prim->tpage = tpage;
                prim->u0 = u_base + u;
                prim->v0 = v_base + v_step * k;
                prim->clut = clut;
                prim->u1 = u_base + u_next;
                prim->v1 = v_base + v_step * k;
                prim->u2 = u_base + u;
                prim->v2 = v_base + v_step * (k + 1);
            }
            polys++;
            polys->index[0] = first + counts[0] + 1;
            polys->index[2] = first + 1;
            polys->index[1] = first + counts[0] + 2;
            for (b = 0; b < 2; b++) {
                prim = &polys->prim[b];
                SetPolyGT3(prim);
                prim->tpage = tpage;
                prim->u0 = u_base + u_next;
                prim->v0 = v_base + v_step * k;
                prim->clut = clut;
                prim->u1 = u_base + u_next;
                prim->v1 = v_base + v_step * (k + 1);
                prim->u2 = u_base + u;
                prim->v2 = v_base + v_step * (k + 1);
            }
            polys++;
        }
        start += counts[0] + 1;
        counts++;
    }
    record->b[0] = b0;
    record->b[1] = b1;
    record->b[2] = b2;
    record->b[3] = b3;
    record->b[4] = b4;
    record->b[5] = b5;
    record->entry_count = count;
    if ((s16)count > 0) {
        record->block18 = func_80031BDC((s16)count * sizeof(Record24Entry), 0);
        if (record->block18 == NULL) {
            record->entry_count = 0;
        }
        for (i = 0; i < record->entry_count; i++) {
            record->block18[i].h0 = 0;
            record->block18[i].h2 = 0;
            record->block18[i].h4 = 0;
            record->block18[i].h6 = 0;
            record->block18[i].h8 = 0;
            record->block18[i].hA = 0;
            record->block18[i].hC = 0;
            record->block18[i].hE = 0;
        }
    } else {
        record->block18 = NULL;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1A14);
#endif

/* Simulate and draw a records24 surface (hair or cloth): each strand's
 * segments hang from their start pulled by `wind` (plus each point's sag),
 * keep their length, stay above `floor` and are pushed out of the record's
 * collision spheres; then the points' normals are averaged from their
 * triangles, and the visible triangles are lit (front and back colours) and
 * queued. As in the original, a triangle the GTE flags as off screen does
 * not advance the triangle pointer, and one variable is both the collision
 * loop's counter and the GTE flag store. */
void func_801E22F8(Record24 *record, SVECTOR *wind, MATRIX *m, u32 *ot, s32 buffer, s32 scale,
                   s16 floor) {
    VECTOR d;
    SVECTOR normal;
    VECTOR e1, e2, n;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    u8 rgb[4];
    s32 k; /* the collision loop's counter, then the GTE flag */
    s32 opz, otz;
    RingPoint *p;
    RingPoly *poly;
    Record24Entry *entry;
    s32 len, radius;
    s32 i;

    if (record->centres == NULL) {
        return;
    }
    rgb[3] = record->block20->prim[0].code;
    for (i = 0; i < record->rings; i++) {
        p = record->block1C[i];
        while (p->length != 0) {
            d.vx = p[1].pos[0] - p->pos[0] + wind->vx;
            d.vy = p[1].pos[1] - p->pos[1] + wind->vy + p->sag;
            d.vz = p[1].pos[2] - p->pos[2] + wind->vz;
            len = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
            if (len != 0) {
                d.vx = (d.vx << 8) / len;
                d.vy = (d.vy << 8) / len;
                d.vz = (d.vz << 8) / len;
            } else {
                d.vx = 0;
                d.vy = 0;
                d.vz = 0;
            }
            p[1].pos[0] = p->pos[0] + p->length * d.vx * scale / 0x100000;
            p[1].pos[1] = p->pos[1] + p->length * d.vy * scale / 0x100000;
            p[1].pos[2] = p->pos[2] + p->length * d.vz * scale / 0x100000;
            if (floor < p[1].pos[1]) {
                p[1].pos[1] = floor;
            }
            entry = record->block18;
            for (k = 0; k < record->entry_count; k++, entry++) {
                d.vx = p[1].pos[0] - entry->h8;
                d.vy = p[1].pos[1] - entry->hA;
                d.vz = p[1].pos[2] - entry->hC;
                len = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
                radius = (s16)(entry->hE * scale / 4096);
                if (len < radius) {
                    if (len != 0) {
                        d.vx = entry->h8 + radius * d.vx / len;
                        d.vy = entry->hA + radius * d.vy / len;
                        d.vz = entry->hC + radius * d.vz / len;
                    } else {
                        d.vx = entry->h8;
                        d.vy = entry->hA;
                        d.vz = entry->hC;
                    }
                    d.vx -= p->pos[0];
                    d.vy -= p->pos[1];
                    d.vz -= p->pos[2];
                    len = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
                    if (len != 0) {
                        d.vx = (d.vx << 8) / len;
                        d.vy = (d.vy << 8) / len;
                        d.vz = (d.vz << 8) / len;
                    } else {
                        d.vx = 0;
                        d.vy = 0;
                        d.vz = 0;
                    }
                    p[1].pos[0] = p->pos[0] + p->length * d.vx * scale / 0x100000;
                    p[1].pos[1] = p->pos[1] + p->length * d.vy * scale / 0x100000;
                    p[1].pos[2] = p->pos[2] + p->length * d.vz * scale / 0x100000;
                }
            }
            p++;
        }
    }
    p = *record->block1C;
    for (i = 0; i < record->points; i++) {
        p->normal_count = 0;
        p->normal[0] = 0;
        p->normal[1] = 0;
        p->normal[2] = 0;
        p++;
    }
    poly = record->block20;
    p = *record->block1C;
    for (i = 0; i < record->polys; i++, poly++) {
        e1.vx = p[poly->index[0]].pos[0] - p[poly->index[1]].pos[0];
        e1.vy = p[poly->index[0]].pos[1] - p[poly->index[1]].pos[1];
        e1.vz = p[poly->index[0]].pos[2] - p[poly->index[1]].pos[2];
        e2.vx = p[poly->index[0]].pos[0] - p[poly->index[2]].pos[0];
        e2.vy = p[poly->index[0]].pos[1] - p[poly->index[2]].pos[1];
        e2.vz = p[poly->index[0]].pos[2] - p[poly->index[2]].pos[2];
        gte_ldopv1(&e1);
        gte_ldopv2(&e2);
        gte_op0();
        gte_stlvnl(&n);
        n.vx /= 8;
        n.vy /= 8;
        n.vz /= 8;
        VectorNormal(&n, &e1);
        p[poly->index[0]].normal[0] += e1.vx;
        p[poly->index[0]].normal[1] += e1.vy;
        p[poly->index[0]].normal[2] += e1.vz;
        p[poly->index[0]].normal_count++;
        p[poly->index[1]].normal[0] += e1.vx;
        p[poly->index[1]].normal[1] += e1.vy;
        p[poly->index[1]].normal[2] += e1.vz;
        p[poly->index[1]].normal_count++;
        p[poly->index[2]].normal[0] += e1.vx;
        p[poly->index[2]].normal[1] += e1.vy;
        p[poly->index[2]].normal[2] += e1.vz;
        p[poly->index[2]].normal_count++;
    }
    p = *record->block1C;
    for (i = 0; i < record->points; i++) {
        p->normal[0] /= (s16)p->normal_count;
        p->normal[1] /= (s16)p->normal_count;
        p->normal[2] /= (s16)p->normal_count;
        p++;
    }
    SetRotMatrix(m);
    SetTransMatrix(m);
    poly = record->block20;
    p = *record->block1C;
    for (i = 0; i < record->polys; i++) {
        gte_ldv3(p[poly->index[0]].pos, p[poly->index[1]].pos, p[poly->index[2]].pos);
        gte_rtpt();
        k = 0;
        gte_stflg(&k);
        if (k & 0x40000) {
            continue;
        }
        gte_nclip();
        gte_stopz(&opz);
        gte_stsxy3(&poly->prim[buffer].x0, &poly->prim[buffer].x1, &poly->prim[buffer].x2);
        gte_avsz3();
        gte_stotz(&otz);
        otz >>= D_80050100;
        if (opz < 0) {
            rgb[0] = record->b[0];
            rgb[1] = record->b[1];
            rgb[2] = record->b[2];
            normal.vx = p[poly->index[0]].normal[0];
            normal.vy = p[poly->index[0]].normal[1];
            normal.vz = p[poly->index[0]].normal[2];
        } else {
            rgb[0] = record->b[3];
            rgb[1] = record->b[4];
            rgb[2] = record->b[5];
            normal.vx = -p[poly->index[0]].normal[0];
            normal.vy = -p[poly->index[0]].normal[1];
            normal.vz = -p[poly->index[0]].normal[2];
        }
        gte_ldv0(&normal);
        gte_ldrgb(rgb);
        gte_nccs();
        gte_strgb(&poly->prim[buffer].r0);
        if (opz < 0) {
            normal.vx = p[poly->index[1]].normal[0];
            normal.vy = p[poly->index[1]].normal[1];
            normal.vz = p[poly->index[1]].normal[2];
        } else {
            normal.vx = -p[poly->index[1]].normal[0];
            normal.vy = -p[poly->index[1]].normal[1];
            normal.vz = -p[poly->index[1]].normal[2];
        }
        gte_ldv0(&normal);
        gte_nccs();
        gte_strgb(&poly->prim[buffer].r1);
        if (opz < 0) {
            normal.vx = p[poly->index[2]].normal[0];
            normal.vy = p[poly->index[2]].normal[1];
            normal.vz = p[poly->index[2]].normal[2];
        } else {
            normal.vx = -p[poly->index[2]].normal[0];
            normal.vy = -p[poly->index[2]].normal[1];
            normal.vz = -p[poly->index[2]].normal[2];
        }
        gte_ldv0(&normal);
        gte_nccs();
        gte_strgb(&poly->prim[buffer].r2);
        addPrim(ot + otz, &poly->prim[buffer]);
        poly++;
    }
}

/* Release a record's heap blocks. */
void func_801E3438(Record24 *record) {
    if (record->centres != NULL) {
        func_800320E8(record->centres);
        func_800320E8(*record->block1C);
        func_800320E8(record->block1C);
        func_800320E8(record->block20);
        if (record->block18 != NULL) {
            func_800320E8(record->block18);
        }
        record->centres = NULL;
    }
}

/* The frame curve of type `type` (0: cosine). */
FrameCurve func_801E34BC(s32 type) {
    switch (type) {
    case 1:
        return (FrameCurve)func_801E08D4;
    case 2:
        return (FrameCurve)func_801E0938;
    case 3:
        return (FrameCurve)func_801E0988;
    }
    return (FrameCurve)func_801E0850;
}

/* Reset actor `a`'s script state: run `list` with the tables `tables`, no
 * animation (`none`), no motion. A statement macro (do/while (0)). */
#define RESET_SCRIPT(a, list, tables, none) \
    do {                             \
        (a)->h3C = 0xFFFF;           \
        (a)->parent = 0xFF;          \
        (a)->b39 = 0x6B;             \
        (a)->entries = (list);       \
        (a)->shared = NULL;          \
        (a)->pc = 0;                 \
        (a)->locals = (tables);      \
        (a)->globals = NULL;         \
        (a)->depth = 0;              \
        (a)->anim_state = (none);    \
        (a)->aim_actor = 0;          \
        (a)->b35 = 0;                \
        (a)->scaled = 0;             \
        (a)->b38 = 0;                \
        (a)->h3A = (none);           \
        (a)->spin[0] = 0;            \
        (a)->spin[1] = 0;            \
        (a)->spin[2] = 0;            \
        (a)->spin_accel[0] = 0;      \
        (a)->spin_accel[1] = 0;      \
        (a)->spin_accel[2] = 0;      \
        (a)->drift[0] = 0;           \
        (a)->drift[1] = 0;           \
        (a)->drift[2] = 0;           \
        (a)->drift_accel[0] = 0;     \
        (a)->drift_accel[1] = 0;     \
        (a)->drift_accel[2] = 0;     \
        (a)->target[0] = 0;          \
        (a)->target[1] = 0;          \
        (a)->target[2] = 0;          \
        (a)->h8E = 1;                \
        (a)->b36 = 0;                \
        (a)->h1E = (none);           \
    } while (0)

/* Reset an actor's script state to run `entries` with the tables `locals`. */
void func_801E3534(Actor *actor, SlotPool *pool, s32 *entries, s32 *locals) {
    s16 none;

    none = -1;
    RESET_SCRIPT(actor, entries, locals, none);
}

/* Call script entry `entry` of `source` in `actor`: queued while the actor is
 * already in a call, else run at once from the entry. */
void func_801E35D0(Actor *actor, Actor *source, SlotPool *pool, s32 entry) {
    if (actor != NULL && source != NULL) {
        if (actor->depth != 0) {
            if (actor->depth < 5) {
                actor->depth++;
            }
            actor->queue_source[actor->depth - 2] = source->index;
            actor->queue_entry[actor->depth - 2] = entry;
            return;
        }
        if (entry < 0x50) {
            actor->pc = source->entries[entry];
        } else {
            actor->pc = source->shared->entries[entry - 0x4E];
        }
        actor->h42 = 0;
        actor->h40 = 0;
        actor->w50 = 0;
        actor->w54 = 0;
        actor->w4C = 0;
        actor->mask = D_801E863C;
        actor->b23 = 0;
        func_801E39F0(actor, pool, -1, 1, 0);
    }
}

/* Run `ticks` steps of an actor: its hierarchy matrices, node tweens and
 * animation, then its script. Returns the tween changes. */
s32 func_801E36BC(Actor *actor, SlotPool *pool, s32 ticks, s32 arg3, s32 arg4) {
    s32 changed;
    s32 i;

    if (actor->models != NULL) {
        changed = 0;
        if (actor->active) {
            func_801E7298(actor);
            if (actor->scaled) {
                func_801DC848(actor->parts, actor->scale);
            } else {
                func_801DC5C0(actor->parts, actor->scale);
            }
            for (i = 0; i < ticks; i++) {
                changed |= func_801DDBF8(pool, actor->parts, actor->h3C, actor->scale);
                func_801E5D44(actor, pool, arg3);
            }
        }
        func_801E39F0(actor, pool, changed, ticks, arg4);
    }
    return changed;
}

/* Carry an actor on its carrier's node: optionally take the node's rotation,
 * then place its root at its offset in the node's space. Without a carrier
 * the link is dropped. */
void func_801E37D0(Actor *actor) {
    SVECTOR offset;
    MATRIX *m;

    m = SCRATCH_MATRIX;
    if (D_801E8670[actor->parent] != NULL) {
        if (!(actor->flags & 0x10)) {
            actor->active = D_801E8670[actor->parent]->active;
        }
        if (actor->active) {
            if (actor->inherit) {
                if (actor->parent_node != 0) {
                    MulMatrix0(&D_801E8670[actor->parent]->parts->world,
                               &D_801E8670[actor->parent]->parts[actor->parent_node].world, SCRATCH_MATRIX);
                } else {
                    m = &D_801E8670[actor->parent]->parts->world;
                }
                MulMatrix2(m, &actor->parts->local);
                MulMatrix2(m, &actor->parts->world);
            }
            if (actor->parent_node != 0) {
                CompMatrix(&D_801E8670[actor->parent]->parts->local,
                           &D_801E8670[actor->parent]->parts[actor->parent_node].world, m);
            } else {
                m = &D_801E8670[actor->parent]->parts->local;
            }
            SetRotMatrix(m);
            SetTransMatrix(m);
            offset.vx = actor->offset[0];
            offset.vy = actor->offset[1];
            offset.vz = actor->offset[2];
            gte_ldv0(&offset);
            gte_rtv0tr();
            gte_stlvnl(actor->parts->local.t);
            gte_stlvnl(actor->parts->pos);
        }
    } else {
        actor->parent = 0xFF;
    }
}

/* Run an actor's script for `ticks` frames: integrate its spin and drift
 * with their accelerations that many times, take a pending jump (w4C after the
 * distance to the target falls to h48, w54 on landing at the root height, w50
 * after h46 frames), then execute opcodes (low byte; the high byte is the
 * argument) until one waits or stops. `changed` holds the tween flags of the
 * last step (-1: unknown), which the wait opcodes test. Opcodes that switch
 * to another actor (0x1f) store the script position in that actor; those
 * that release or hand over (0x14 with 0xfd, 0x16, 0x17, 0x5b) return without
 * storing it, and 0x15 copies into an uninitialised block when all actor
 * slots 8 and 9 are used, as in the original.
 * NON_MATCHING (20 bytes shorter): the case bodies are in the original's
 * order (jump table checked case by case, 26 of 0x71 bodies still differ in
 * size); the original zero-extends `arg` at each use (andi 0xff), uses the
 * register copy of an operand where this C reloads `word` (lh 0x6c), keeps
 * m/ex/ey/ez and the d/v vectors in block scopes (v and d share a slot, so
 * n and word sit at 0x68/0x6c), and allocates registers differently. */
#ifdef NON_MATCHING
void func_801E39F0(Actor *actor, SlotPool *pool, s32 changed, s32 ticks, s32 arg4) {
    Actor *self;
    Actor *other;
    Actor *copy;
    ModelPart *part;
    ModelPart *node;
    VECTOR moved;
    SVECTOR step;
    RECT rect;
    VECTOR d;
    s32 n;
    u16 *pc;
    u16 *start;
    u16 *counter;
    /* Reused by word decoding and actor-mask resolution. Save decoded
     * bytes before a resolver overwrites the halfword. */
    union {
        u16 value;
        u8 low;
    } word;
    MATRIX m;
    VECTOR ex, ey, ez;
    SVECTOR v;
    u16 raw;
    u8 reference, entry, low2, high2;
    u8 op, arg;
    s32 running, redraw;
    s32 i, key;
    s32 dx, dy, dz, dist, pitch, yaw;
    s16 c0, c1, c2;
    u8 depth;

    if (ticks == 0 || actor->pc == 0) {
        return;
    }
    func_80032498(4, 0);
    redraw = 0;
    self = actor;
    for (n = 0; n < ticks; n++) {
        actor->spin[0] += actor->spin_accel[0];
        actor->spin[1] += actor->spin_accel[1];
        actor->spin[2] += actor->spin_accel[2];
        actor->drift[0] += actor->drift_accel[0];
        actor->drift[1] += actor->drift_accel[1];
        actor->drift[2] += actor->drift_accel[2];
        actor->parts->rot.vx += actor->spin[0] >> 3;
        actor->parts->rot.vy += actor->spin[1] >> 3;
        actor->parts->rot.vz += actor->spin[2] >> 3;
        step.vx = actor->drift[0] * actor->parts->scale[0] >> 12;
        step.vy = actor->drift[1] * actor->parts->scale[1] >> 12;
        step.vz = actor->drift[2] * actor->parts->scale[2] >> 12;
        ApplyMatrix(&actor->parts->world, &step, &moved);
        actor->parts->pos[0] += actor->scale * moved.vx >> 12;
        actor->parts->pos[1] += actor->scale * moved.vy >> 12;
        actor->parts->pos[2] += actor->scale * moved.vz >> 12;
    }
    running = 1;
    pc = (u16 *)actor->pc;
    if (actor->w4C != 0 && func_801E6338(actor) <= actor->h48) {
        pc = (u16 *)actor->w4C;
        actor->w4C = 0;
    } else {
        if (actor->w54 != 0) {
            v.vx = actor->parts->pos[0];
            v.vy = 0;
            v.vz = actor->parts->pos[2];
            v.vy = actor->h60;
            if (v.vy < actor->parts->pos[1]) {
                actor->parts->pos[1] = v.vy;
                pc = (u16 *)actor->w54;
                actor->w54 = 0;
                goto aim;
            }
        }
        if (actor->w50 != 0) {
            actor->h44 += ticks;
            if (actor->h44 >= actor->h46) {
                pc = (u16 *)actor->w50;
                actor->w50 = 0;
            }
        }
    }
aim:
    if (actor->aim_actor != 0) {
        func_801E63A8(actor);
    }
    while (running) {
        start = pc;
        raw = *pc++;
        word.value = raw;
        arg = raw >> 8;
        op = word.low;
        switch (op) {
        case 0x00: /* wait */
            pc = start;
            running = 0;
            break;
        case 0x01: /* wait for a number of frames */
            if (changed == -1) {
                pc = start;
                running = 0;
            } else {
                word.value = *pc++;
                actor->h40 += ticks;
                if ((s16)actor->h40 < (s16)word.value) {
                    pc = start;
                    running = 0;
                    break;
                }
                actor->h40 = 0;
                ticks = 0;
            }
            break;
        case 0x02: /* redraw */
            redraw = 1;
            break;
        case 0x03:
            redraw = 1;
            break;
        case 0x08: /* drop the node tweens and the animation */
            func_801DFE8C(pool, actor->parts);
            func_801E632C(actor);
            break;
        case 0x0A:
            func_801DF52C(pool, actor->parts, arg, 7);
            break;
        case 0x0B: /* drop the tweens and reset every node below the root */
            part = actor->parts;
            func_801DFE8C(pool, part);
            n = part->count - 1;
            for (i = 0; i < n; i++) {
                part++;
                part->rot.vx = 0;
                part->rot.vy = 0;
                part->rot.vz = 0;
                part->pos[0] = 0;
                part->pos[1] = 0;
                part->pos[2] = 0;
                part->dirty = 1;
                part->rotate = 1;
            }
            break;
        case 0x0C: /* stop spinning and drifting */
            actor->spin[0] = 0;
            actor->spin[1] = 0;
            actor->spin[2] = 0;
            actor->spin_accel[0] = 0;
            actor->spin_accel[1] = 0;
            actor->spin_accel[2] = 0;
            actor->drift[0] = 0;
            actor->drift[1] = 0;
            actor->drift[2] = 0;
            actor->drift_accel[0] = 0;
            actor->drift_accel[1] = 0;
            actor->drift_accel[2] = 0;
            break;
        case 0x0D:
            func_801DF52C(pool, actor->parts, arg, 1);
            break;
        case 0x0E:
            func_801DF52C(pool, actor->parts, arg, 2);
            break;
        case 0x10: /* apply keyframe `arg` */
            func_801DEF10(actor->parts, (s16 *)func_801E6910(actor, arg, &n));
            break;
        case 0x11: /* start animation `arg` */
            key = func_801E6910(actor, arg, &n);
            raw = *pc++;
            word.value = raw;
            entry = raw >> 8;
            if (n == 0) {
                func_801DF7F4(pool, actor->parts, (u16 *)key, entry, word.low);
                changed = -1;
                dist = (s16)((u16 *)key)[8] * (actor->scale * actor->parts->scale[2] >> 12) >> 12;
                actor->h8E = dist < 0 ? -dist : dist;
                func_801E5C74(actor, (Animation *)key, entry);
            }
            break;
        case 0x13: /* tween to keyframe */
            raw = *pc++;
            word.value = raw;
            entry = raw >> 8;
            reference = word.low;
            raw = *pc++;
            word.value = raw;
            high2 = raw >> 8;
            low2 = word.low;
            key = func_801E6910(actor, reference, &n);
            if (D_801E85CC != 0) {
                func_801DEF10(actor->parts, (s16 *)key);
            } else {
                func_801DF0B4(pool, actor->parts, (s16 *)key, high2, arg, low2,
                              entry);
            }
            changed = -1;
            func_801E632C(actor);
            break;
        case 0x14: /* call an entry in the masked actors */
            raw = *pc++;
            word.value = raw;
            reference = word.low;
            entry = raw >> 8;
            i = func_801E6830(actor, reference, &word.value);
            func_801E6830(actor, arg, &word.value);
            depth = actor->depth;
            if (arg == 0xFD) {
                actor->depth = 0;
            }
            for (n = 0; n < 8; n++) {
                if (((s16)word.value >> n) & 1) {
                    if (reference == 0xFF) {
                        func_801E35D0(D_801E8670[n], D_801E8670[n], pool, entry);
                    } else {
                        func_801E35D0(D_801E8670[n], D_801E8670[i & 0xFF], pool, entry);
                    }
                }
            }
            actor->depth = depth;
            if (arg == 0xFD) {
                return;
            }
            break;
        case 0x15: /* clone this actor into a free slot 8 or 9 */
            word.value = *pc++;
            for (n = 8; n < 10; n++) {
                if (D_801E8670[n] == NULL) {
                    copy = func_80031BDC(sizeof(Actor), 1);
                    break;
                }
            }
            *copy = *actor;
            D_801E8670[n] = copy;
            copy->h3C = 0xFFFF;
            copy->anim_state = -1;
            copy->parent = 0xFF;
            copy->h40 = 0;
            copy->aim_actor = 0;
            copy->h90 = -1;
            copy->b62 = 0;
            copy->group = NULL;
            copy->blockAC = NULL;
            copy->b39 = 0x6B;
            copy->pc = 0;
            copy->b21 = actor->index;
            copy->count10D = 0;
            copy->count10E = 0;
            copy->index = n;
            func_801E8510(copy);
            copy->parts = func_80031BDC(actor->parts->count * sizeof(ModelPart), 1);
            for (n = 0; n < actor->parts->count; n++) {
                copy->parts[n] = actor->parts[n];
                if (actor->parts[n].parent != NULL) {
                    copy->parts[n].parent = copy->parts + (actor->parts[n].parent - actor->parts);
                }
                copy->parts[n].visible = 0;
                copy->parts[n].attachments[0] = NULL;
                copy->parts[n].attachments[1] = NULL;
            }
            func_801E6578(pool, (s16)word.value, actor->parts, copy->parts);
            if (arg != 0xFF) {
                func_801E35D0(copy, copy, pool, arg);
            }
            break;
        case 0x16: /* merge back into the actor this one was cloned from */
            func_801E6668(actor->parts, D_801E8670[actor->b21]->parts);
            func_801E8030(actor->index);
            if (arg != 0xFF) {
                func_801E35D0(D_801E8670[actor->b21], D_801E8670[actor->b21], pool, arg);
            }
            return;
        case 0x17: /* release this actor */
            func_801E8030(actor->index);
            return;
        case 0x18: /* start animation `arg` looping from a frame */
            key = func_801E6910(actor, arg, &n);
            func_801E5C74(actor, (Animation *)key, (s16)*pc++);
            break;
        case 0x19: /* stop the animation */
            func_801E632C(actor);
            break;
        case 0x1A: /* move a VRAM rectangle (arg bit 0: by the actor's image offset) */
            rect.x = *pc++;
            rect.y = *pc++;
            dx = *pc++;
            dy = *pc++;
            rect.w = ((s16)*pc++ + 1) / 2 * 2;
            rect.h = *pc++;
            if (arg & 1) {
                if (actor->h90 < 0) {
                    break;
                }
                rect.x += actor->h90;
                rect.y += actor->h92;
                dx += actor->h90;
                dy += actor->h92;
            }
            MoveImage(&rect, (s16)dx, (s16)dy);
            break;
        case 0x1D: { /* tween node `arg` between two poses */
            s32 x0, y0, z0, x1, y1, z1, duration;

            raw = *pc++;
            word.value = raw;
            entry = raw >> 8;
            reference = word.low;
            raw = *pc++;
            word.value = raw;
            high2 = raw >> 8;
            low2 = word.low;
            changed = -1;
            x0 = (s16)*pc++;
            y0 = (s16)*pc++;
            z0 = (s16)*pc++;
            x1 = (s16)*pc++;
            y1 = (s16)*pc++;
            z1 = (s16)*pc++;
            duration = (s16)*pc++;
            func_801E6974(actor, pool, &actor->parts[arg], reference, entry, low2,
                          high2, x0, y0, z0, x1, y1, z1, duration);
            break;
        }
        case 0x1E: /* use the scaled hierarchy update */
            actor->scaled = arg;
            break;
        case 0x1F: /* continue in another actor */
            other = D_801E8670[func_801E6830(self, arg, &word.value) & 0xFF];
            if (other != NULL) {
                actor = other;
            }
            break;
        case 0x20: /* wait for the tweens to end */
            if (changed != -1) {
                if (changed & 0x100) {
                    pc = start;
                    running = 0;
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x21: /* wait for the tweens tagged `arg` */
            actor->h3C = arg;
            if (changed != -1) {
                if (changed & 1) {
                    pc = start;
                    running = 0;
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x22: /* wait for a number of loops (of all, or those tagged `arg`) */
            if (changed != -1) {
                word.value = *pc++;
                if (arg == 0xFF) {
                    if (!(changed & 0x400)) {
                        pc = start;
                        running = 0;
                        break;
                    }
                    actor->h42++;
                    if (actor->h42 < (s16)word.value) {
                        pc = start;
                        running = 0;
                        break;
                    }
                } else {
                    actor->h3C = arg;
                    if (!(changed & 4)) {
                        pc = start;
                        running = 0;
                        break;
                    }
                    actor->h42++;
                    if (actor->h42 < (s16)word.value) {
                        pc = start;
                        running = 0;
                        break;
                    }
                }
                actor->h42 = 0;
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x23:
            func_801E6D94(actor, &actor->parts[(s16)*pc++], arg);
            break;
        case 0x24: /* show or hide */
            if (actor != NULL) {
                actor->active = arg & 1;
            }
            break;
        case 0x25: /* attach the masked actors to node (high byte), keeping
                    * their place (arg bit 0) or at an offset */
            raw = *pc++;
            word.value = raw;
            entry = raw >> 8;
            func_801E6830(actor, word.low, &word.value);
            c0 = *pc++;
            c1 = *pc++;
            c2 = *pc++;
            for (n = 0; n < 8; n++) {
                if (!(((s16)word.value >> n) & 1) || D_801E8670[n] == NULL) {
                    continue;
                }
                D_801E8670[n]->parent_node = entry;
                D_801E8670[n]->parent = actor->index;
                D_801E8670[n]->inherit = arg & 2;
                D_801E8670[n]->b36 = 1;
                if (arg & 1) {
                    node = &actor->parts[entry];
                    SetRotMatrix(&node->world);
                    m.t[0] = 0;
                    m.t[1] = 0;
                    m.t[2] = 0;
                    SetTransMatrix(&m);
                    v.vx = 0x1000;
                    v.vy = 0;
                    v.vz = 0;
                    gte_ldv0(&v);
                    gte_rtv0tr();
                    gte_stlvnl(&ex);
                    v.vx = 0;
                    v.vy = 0x1000;
                    gte_ldv0(&v);
                    gte_rtv0tr();
                    gte_stlvnl(&ey);
                    v.vy = 0;
                    v.vz = 0x1000;
                    gte_ldv0(&v);
                    gte_rtv0tr();
                    gte_stlvnl(&ez);
                    d.vx = D_801E8670[n]->parts->pos[0] - node->world.t[0];
                    d.vy = D_801E8670[n]->parts->pos[1] - node->world.t[1];
                    d.vz = D_801E8670[n]->parts->pos[2] - node->world.t[2];
                    D_801E8670[n]->offset[0] = func_801E66BC(&d, &ey, &ez, actor->scale);
                    D_801E8670[n]->offset[1] = func_801E66BC(&d, &ez, &ex, actor->scale);
                    D_801E8670[n]->offset[2] = func_801E66BC(&d, &ex, &ey, actor->scale);
                } else {
                    D_801E8670[n]->offset[0] = c0;
                    D_801E8670[n]->offset[1] = c1;
                    D_801E8670[n]->offset[2] = c2;
                }
            }
            break;
        case 0x26: /* detach the masked actors */
            func_801E6830(actor, arg, &word.value);
            for (n = 0; n < 8; n++) {
                if ((((s16)word.value >> n) & 1) && D_801E8670[n] != NULL) {
                    D_801E8670[n]->parent = 0xFF;
                }
            }
            break;
        case 0x27: /* recompose the hierarchy */
            if (actor->scaled) {
                func_801DC848(actor->parts, actor->scale);
            } else {
                func_801DC5C0(actor->parts, actor->scale);
            }
            break;
        case 0x28:
        case 0x29: /* wait until the distance (in h8e units) passes `arg` while
                    * node (low byte) tweens with the given time */
            dist = func_801E6338(actor);
            if (actor->h8E == 0) {
                actor->h8E = 1;
            }
            word.value = *pc++;
            node = &actor->parts[word.low];
            dist /= actor->h8E;
            n = 0;
            if (node->attachments[0] != NULL) {
                n = (word.value >> 8) == node->attachments[0]->time;
            } else if (node->attachments[1] != NULL &&
                       node->attachments[1]->time == ((word.value >> 8) & 0xFF)) {
                n = 1;
            }
            if (!n) {
                pc = start;
                running = 0;
            } else if (op == 0x28) {
                if (dist >= arg) {
                    pc = start;
                    running = 0;
                }
            } else if (dist < arg) {
                pc = start;
                running = 0;
            }
            break;
        case 0x2A: /* wait while nearer than h8e */
            if (func_801E6338(actor) >= actor->h8E) {
                pc = start;
                running = 0;
            }
            break;
        case 0x2B: /* wait while farther than h8e */
            if (func_801E6338(actor) <= actor->h8E) {
                pc = start;
                running = 0;
            }
            break;
        case 0x2E: /* jump when near the target */
            actor->h48 = actor->h8E;
            word.value = *pc++;
            actor->w4C = arg ? (s32)((u8 *)start + (s16)word.value) : 0;
            break;
        case 0x30: /* reset a loop counter */
            *pc++ = 0;
            break;
        case 0x31: /* loop back to a counter until it reaches its limit */
            word.value = *pc++;
            counter = (u16 *)((u8 *)start + (s16)word.value);
            n = counter[0] >> 8;
            if ((s16)++counter[1] < n) {
                pc = counter + 2;
            }
            break;
        case 0x32: /* jump */
            pc = (u16 *)((u8 *)start + (s16)*pc);
            break;
        case 0x35: /* jump at random (half the time) */
            word.value = *pc++;
            if (rand() >= 0x4000) {
                pc = (u16 *)((u8 *)start + (s16)word.value);
            }
            break;
        case 0x36: /* jump after a number of frames */
            actor->h44 = 0;
            actor->h46 = *pc++;
            word.value = *pc++;
            actor->w50 = arg ? (s32)((u8 *)start + (s16)word.value) : 0;
            break;
        case 0x37: /* jump on landing */
            word.value = *pc++;
            actor->w54 = arg ? (s32)((u8 *)start + (s16)word.value) : 0;
            break;
        case 0x38: /* move node `arg` toward the target */
            word.value = *pc++;
            func_801E5B50(pool, actor->parts, 0, arg, word.low, word.value >> 8,
                          actor->target[0], actor->target[1], actor->target[2]);
            break;
        case 0x39: /* the same, flag 1 */
            word.value = *pc++;
            func_801E5B50(pool, actor->parts, 1, arg, word.low, word.value >> 8,
                          actor->target[0], actor->target[1], actor->target[2]);
            break;
        case 0x33:
        case 0x34:
        case 0x3B:
            word.value = *pc++;
            break;
        case 0x3C: /* play a sound */
            word.value = *pc++;
            func_8003A3B8(word.low + func_801E5CD8(actor, arg), 0, word.value >> 8);
            break;
        case 0x3D: /* run the queued calls once `arg` is among them */
            word.value = actor->depth;
            if ((s16)word.value < 2) {
                break;
            }
            for (n = 1; n < (s16)word.value; n++) {
                if (actor->queue_entry[n - 1] == arg) {
                    goto dequeue;
                }
            }
            break;
        case 0x40: /* turn the root to a rotation */
            c0 = *pc++;
            c1 = *pc++;
            c2 = *pc++;
            changed = -1;
            func_801E59D4(pool, actor->parts, arg, c0, c1, c2);
            break;
        case 0x41: /* turn the root by a rotation */
            c0 = *pc++;
            c1 = *pc++;
            c2 = *pc++;
            changed = -1;
            func_801E59D4(pool, actor->parts, arg, (s16)(actor->parts->rot.vx + c0),
                          (s16)(actor->parts->rot.vy + c1), (s16)(actor->parts->rot.vz + c2));
            break;
        case 0x42:
        case 0x43: /* turn the root toward the target (0x43: heading only) */
            dy = actor->target[1] - actor->parts->pos[1];
            dx = actor->target[0] - actor->parts->pos[0];
            dz = actor->target[2] - actor->parts->pos[2];
            if (op == 0x43) {
                pitch = 0;
                dy = 0;
            } else {
                pitch = ratan2(dy, SquareRoot0(dx * dx + dz * dz));
            }
            yaw = ratan2(-dx, -dz);
            if (dx == 0 && dy == 0 && dz == 0) {
                break;
            }
            changed = -1;
            func_801E59D4(pool, actor->parts, arg, (s16)pitch, (s16)yaw, 0);
            break;
        case 0x44:
            actor->spin[0] = *pc++;
            actor->spin[1] = *pc++;
            actor->spin[2] = *pc++;
            break;
        case 0x45:
            actor->spin[0] += *pc++;
            actor->spin[1] += *pc++;
            actor->spin[2] += *pc++;
            break;
        case 0x46:
            actor->spin_accel[0] = *pc++;
            actor->spin_accel[1] = *pc++;
            actor->spin_accel[2] = *pc++;
            break;
        case 0x47:
            actor->spin_accel[0] += *pc++;
            actor->spin_accel[1] += *pc++;
            actor->spin_accel[2] += *pc++;
            break;
        case 0x48:
            actor->b36 = arg;
            break;
        case 0x49: /* place the root */
            actor->parts->pos[0] = (s16)*pc++;
            actor->parts->pos[1] = (s16)*pc++;
            actor->parts->pos[2] = (s16)*pc++;
            break;
        case 0x4A: /* (arg 0xfb) put the root at distance h8e from the target */
            if (arg != 0xFB) {
                break;
            }
            dx = actor->parts->pos[0] - actor->target[0];
            dy = actor->parts->pos[1] - actor->target[1];
            dz = actor->parts->pos[2] - actor->target[2];
            dist = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
            actor->parts->pos[0] = actor->target[0] + dx * actor->h8E / dist;
            actor->parts->pos[1] = actor->target[1] + dy * actor->h8E / dist;
            actor->parts->pos[2] = actor->target[2] + dz * actor->h8E / dist;
            break;
        case 0x4B:
            actor->drift[0] = *pc++;
            actor->drift[1] = *pc++;
            actor->drift[2] = *pc++;
            break;
        case 0x4C:
            actor->drift[0] += *pc++;
            actor->drift[1] += *pc++;
            actor->drift[2] += *pc++;
            break;
        case 0x4D:
            actor->drift_accel[0] = *pc++;
            actor->drift_accel[1] = *pc++;
            actor->drift_accel[2] = *pc++;
            break;
        case 0x4E:
            actor->drift_accel[0] += *pc++;
            actor->drift_accel[1] += *pc++;
            actor->drift_accel[2] += *pc++;
            break;
        case 0x4F: /* drift toward the target over `arg` frames */
            dx = actor->target[0] - actor->parts->pos[0];
            dy = actor->target[1] - actor->parts->pos[1];
            dz = actor->target[2] - actor->parts->pos[2];
            if (arg == 0) {
                arg = 1;
            }
            dist = SquareRoot0(dx * dx + dy * dy + dz * dz) / arg;
            actor->drift[2] = (dist << 12) / (actor->scale * actor->parts->scale[2] >> 12) / 2;
            if (((ratan2(-dx, -dz) - (u16)actor->parts->rot.vy + 0x400) & 0xFFF) < 0x800) {
                actor->drift[2] = -actor->drift[2];
            }
            break;
        case 0x50: /* set the target */
            actor->aim_actor = 0;
            actor->target[0] = *pc++;
            actor->target[1] = *pc++;
            actor->target[2] = *pc++;
            break;
        case 0x54:
            actor->h8E = (s16)*pc++ * (actor->scale * actor->parts->scale[2] >> 12) >> 12;
            break;
        case 0x55:
            actor->h8E += (s16)*pc++ * (actor->scale * actor->parts->scale[2] >> 12) >> 12;
            break;
        case 0x56:
            actor->h8E += *pc++;
            break;
        case 0x57:
            n = func_801E6830(actor, arg, &word.value) & 0xFF;
            actor->h8E += func_801E8480(n);
            break;
        case 0x5B: /* set the call depth; 2 runs the queued calls */
            word.value = actor->depth;
            if (arg == 1 && (s16)word.value >= 2) {
                break;
            }
            actor->depth = arg;
            if (arg != 2) {
                break;
            }
        dequeue:
            actor->depth = 0;
            if ((s16)word.value < 2) {
                break;
            }
            for (n = 1; n < (s16)word.value; n++) {
                func_801E35D0(actor, D_801E8670[actor->queue_source[n - 1]], pool,
                              actor->queue_entry[n - 1]);
            }
            return;
        case 0x5C: /* jump when at the target */
            word.value = *pc++;
            if (actor->target[0] == actor->parts->pos[0] && actor->target[1] == actor->parts->pos[1] &&
                actor->target[2] == actor->parts->pos[2]) {
                pc = (u16 *)((u8 *)start + (s16)word.value);
            }
            break;
        case 0x5D:
            word.value = *pc++;
            actor->parts[(s16)word.value].billboard = arg;
            break;
        case 0x5E:
            word.value = *pc++;
            actor->scale = word.value;
            break;
        case 0x5F:
            word.value = *pc++;
            actor->flags = word.value;
            break;
        case 0x62:
            /* The operands are read in order. */
            func_801E7094(actor, &actor->parts[(s16)*pc++], arg, (s16)*pc++, (s16)*pc++, (s16)*pc++);
            break;
        case 0x63: /* start an event animation and jump */
            word.value = *pc++;
            actor->anim_state = 0;
            actor->anim_loop = -1;
            actor->anim_frame = 0;
            actor->anim_frames = arg;
            actor->anim_pos = (u8 *)pc;
            pc = (u16 *)((u8 *)start + (s16)word.value);
            break;
        case 0x64:
            actor->h3E = *pc++;
            break;
        case 0x6B:
            word.value = *pc++;
            actor->parts[(s16)word.value].yxz = arg;
            break;
        case 0x6C: /* wait while the resident is busy */
            if (func_800286CC() != 0) {
                pc = start;
                running = 0;
            }
            break;
        case 0x6D:
            actor->b38 = arg & 1;
            break;
        case 0x6E: /* wait while an actor's b38 equals the word's bit 0 */
            other = D_801E8670[func_801E6830(actor, arg, &word.value) & 0xFF];
            word.value = *pc++;
            if (other != NULL && other->b38 == (word.value & 1)) {
                pc = start;
                running = 0;
            }
            break;
        case 0x6F:
            actor->h3A = arg ? D_801E863C : -1;
            break;
        case 0x70: /* jump and stop when h3a is the current value */
            word.value = *pc++;
            if (actor->h3A == D_801E863C) {
                pc = (u16 *)((u8 *)start + (s16)word.value);
                running = 0;
            }
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x09:
        case 0x0F:
        case 0x12:
        case 0x1B:
        case 0x1C:
        case 0x2C:
        case 0x2D:
        case 0x2F:
        case 0x3A:
        case 0x3E:
        case 0x3F:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x60:
        case 0x61:
        case 0x65:
        case 0x66:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
            break;
        default: /* unknown: stop before it */
            pc--;
            running = 0;
            break;
        }
    }
    actor->pc = (s32)pc;
    if (redraw) {
        func_800796F4();
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E39F0);
#endif

/* Turn a node to (rx, ry, rz): at once when `duration` is below 2, else
 * through its first attachment (a kind-3 tween of the shortest angle
 * differences over `duration` ticks), taking a pool slot if it has none. */
void func_801E59D4(SlotPool *pool, ModelPart *part, s32 duration, s32 rx, s32 ry, s32 rz) {
    PoolSlot *tween;

    if (duration < 2) {
        part->rot.vx = rx;
        part->rot.vy = ry;
        part->rot.vz = rz;
        part->rotate = 1;
        return;
    }
    if (part->rot.vx != rx || part->rot.vy != ry || part->rot.vz != rz) {
        if (part->attachments[0] != NULL) {
            tween = part->attachments[0];
        } else {
            tween = func_801DF6F0(pool);
        }
        if (tween != NULL) {
            tween->used = 1;
            tween->kind = 3;
            tween->flag = 0;
            tween->tag = 0xFE;
            tween->u.value[0] = part->rot.vx;
            tween->u.value[1] = part->rot.vy;
            tween->u.value[2] = part->rot.vz;
            rx = (rx - part->rot.vx) & 0xFFF;
            if (rx >= 0x800) {
                rx -= 0x1000;
            }
            tween->u.value[3] = rx;
            ry = (ry - part->rot.vy) & 0xFFF;
            if (ry >= 0x800) {
                ry -= 0x1000;
            }
            tween->u.value[4] = ry;
            rz = (rz - part->rot.vz) & 0xFFF;
            if (rz >= 0x800) {
                rz -= 0x1000;
            }
            tween->u.value[5] = rz;
            tween->time = 0;
            tween->duration = duration;
            part->attachments[0] = tween;
        }
    }
}

/* Start a movement tween (kind `type` + 7) of a node towards (x, y, z) over
 * `duration` ticks in its first attachment; value 0 is the distance + 1. */
void func_801E5B50(SlotPool *pool, ModelPart *part, s32 type, s32 arg3, s32 arg4, s32 duration,
                   s32 x, s32 y, s32 z) {
    PoolSlot *tween;
    s32 dx;
    s32 dy;
    s32 dz;

    if (part->attachments[0] != NULL) {
        tween = part->attachments[0];
    } else {
        tween = func_801DF6F0(pool);
    }
    if (tween != NULL) {
        tween->used = 1;
        tween->kind = type + 7;
        tween->flag = 0;
        tween->tag = 0xFE;
        dx = x - part->pos[0];
        dy = y - part->pos[1];
        dz = z - part->pos[2];
        tween->u.value[0] = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
        tween->u.value[1] = arg3;
        tween->u.value[2] = arg4;
        tween->u.value[3] = x;
        tween->u.value[4] = y;
        tween->u.value[5] = z;
        tween->time = 0;
        tween->duration = duration;
        part->attachments[0] = tween;
    }
}

/* Start an animation (looping to its loop frame when `loop`); none without
 * frames. */
void func_801E5C74(Actor *actor, Animation *anim, s32 loop) {
    if (anim->frames != 0) {
        actor->anim_state = 0;
        if (loop) {
            actor->anim_loop = anim->loop;
        } else {
            actor->anim_loop = -1;
        }
        actor->anim_frame = 0;
        actor->anim_frames = anim->frames;
        actor->anim_start = actor->anim_pos = (u8 *)anim + anim->data;
        return;
    }
    actor->anim_state = -1;
}

/* The +14 value of view `which` (0: the resident one, 1/2: the actor's) in
 * 16.16. */
s32 func_801E5CD8(Actor *actor, s32 which) {
    if (which == 0) {
        return D_8005919C->h14 << 16;
    } else if (which != 1) {
        if (which == 2) {
            return actor->ownerB4->view->h14 << 16;
        }
    } else {
        return actor->ownerB0->view->h14 << 16;
    }
}

/* Run an actor's animation events of the current frame (anchors and their
 * light columns, channel stops, node visibility, calls into the masked
 * actors and image animations), then advance the frame, looping at the
 * loop frame. The call event (type 8) reads a local the original never
 * sets; it is spilled, so it is loaded from its stack slot. */
void func_801E5D44(Actor *actor, SlotPool *pool, s32 arg2) {
    u16 unset;
    AnimEvent *event;
    AnimEvent *call;
    AnimEvent *anchor;
    Actor **other;
    ImageAnim *target;
    MATRIX *m;
    FrameCurve curve;
    u16 x;
    u16 y;
    u16 x2;
    u16 y2;
    u16 z2;
    u16 saved_mask;
    s16 saved_index;
    s32 bit;
    s32 entry;
    s16 state;
    s32 i;

    if (actor->anim_state < 0) {
        return;
    }
    while (actor->anim_frame < actor->anim_frames) {
        event = (AnimEvent *)actor->anim_pos;
        if (actor->anim_state != event->frame) {
            break;
        }
        switch (event->type) {
        case 1:
            actor->anim_pos += 0x14;
            break;
        case 2:
            if (event->u.anchor.active) {
                if (event->index < 2) {
                    anchor = event;
                    if (anchor->u.anchor.fixed) {
                        D_801E8648[anchor->index].actor = -1;
                    } else {
                        D_801E8648[anchor->index].actor = actor->index;
                    }
                    D_801E8648[anchor->index].node = anchor->u.anchor.node;
                    D_801E8644->m[0][anchor->index + 1] = anchor->u.anchor.color[0] << 4;
                    D_801E8644->m[1][anchor->index + 1] = anchor->u.anchor.color[1] << 4;
                    D_801E8644->m[2][anchor->index + 1] = anchor->u.anchor.color[2] << 4;
                    D_801E8648[anchor->index].offset.vx = anchor->u.anchor.offset[0];
                    D_801E8648[anchor->index].offset.vy = anchor->u.anchor.offset[1];
                    D_801E8648[anchor->index].offset.vz = anchor->u.anchor.offset[2];
                    D_801E8648[anchor->index].active = anchor->u.anchor.enable;
                }
                actor->anim_pos += 0x12;
            } else {
                D_801E8648[event->index].active = 0;
                actor->anim_pos += 6;
            }
            break;
        case 3:
        case 4:
            func_801E0844(&actor->channels[event->index].id, arg2);
            if (event->u.more) {
                actor->anim_pos += 0x1C;
            } else {
                actor->anim_pos += 6;
            }
            break;
        case 5:
            actor->anim_pos += 8;
            break;
        case 6:
            actor->anim_pos += 4;
            break;
        case 7:
            actor->parts[event->u.show.node].visible = event->u.show.visible & 1;
            actor->anim_pos += 6;
            break;
        case 8:
            /* The original tests a local it never sets. */
            call = event;
            state = unset;
            saved_index = D_801E86B0;
            saved_mask = D_801E863C;
            bit = 1 << saved_index;
            for (i = 0, other = D_801E8670; i < 8; i++, other++) {
                if ((actor->mask >> i) & 1) {
                    entry = call->u.call.entry;
                    if (*other != NULL && entry > 0) {
                        if (state != 0) {
                            func_801E8394(actor, i, bit, entry);
                        } else {
                            func_801E8330(i, bit, entry);
                        }
                    }
                }
            }
            D_801E86B0 = saved_index;
            D_801E863C = saved_mask;
            actor->anim_pos += 0xA;
            break;
        case 9:
            if (event->u.image.active) {
                if (event->index < actor->count10E) {
                    if (event->u.image.target != 0xFF && event->u.image.target < actor->count10E) {
                        target = &actor->records30[event->u.image.target];
                    } else {
                        target = NULL;
                    }
                    m = NULL;
                    if ((event->u.image.mode & 0x7F) >= 4) {
                        m = D_801E8644;
                    }
                    curve = func_801E34BC(event->u.image.curve);
                    x = event->u.image.x;
                    y = event->u.image.y;
                    x2 = event->u.image.x2;
                    y2 = event->u.image.y2;
                    z2 = event->u.image.h10;
                    if (event->u.image.mode & 0x80) {
                        if (actor->h90 < 0) {
                            break;
                        }
                        x += actor->shift_x;
                        y += actor->shift_y;
                        if ((event->u.image.b12 >> 4) == 1) {
                            x2 += actor->shift_x;
                            y2 += actor->shift_y;
                        }
                    }
                    func_801E0A00(&actor->records30[event->index], target, event->u.image.mode & 0x7F,
                                  event->u.image.b12 | 0x700, (ColorRow *)m, x, y, 0, x2, y2, z2,
                                  x, y, event->u.image.b13, event->u.image.b14, event->u.image.h16,
                                  event->u.image.h18, event->u.image.h1A, curve);
                }
                actor->anim_pos += 0x1C;
            } else {
                func_801E165C(&actor->records30[event->index]);
                actor->anim_pos += 6;
            }
            break;
        }
        actor->anim_frame++;
    }
    actor->anim_state++;
    if (actor->anim_loop >= 0 && actor->anim_state >= actor->anim_loop) {
        actor->anim_state = 0;
        actor->anim_frame = 0;
        actor->anim_pos = actor->anim_start;
    }
}

/* Stop an actor's animation. */
void func_801E632C(Actor *actor) {
    actor->anim_state = -1;
}

/* Distance from an actor's root to its target. */
s32 func_801E6338(Actor *actor) {
    ModelPart *root;
    s32 dx;
    s32 dy;
    s32 dz;

    root = actor->parts;
    dx = actor->target[0] - root->pos[0];
    dy = actor->target[1] - root->pos[1];
    dz = actor->target[2] - root->pos[2];
    return SquareRoot0(dx * dx + dy * dy + dz * dz);
}

/* Aim an actor at a point of another actor's node (`aim_actor` is a
 * reference: 0xff the mask's lowest actor, 0xfe the current one, 1-0x7f an
 * index + 1), updating its target and a running movement tween. */
void func_801E63A8(Actor *actor) {
    VECTOR world;
    PoolSlot *tween;
    Actor *other;
    MATRIX *m;
    s32 index;

    index = 0;
    if (actor->aim_actor == 0xFF) {
        for (index = 0; index < 8; index++) {
            if ((actor->mask >> index) & 1) {
                break;
            }
        }
    }
    if (actor->aim_actor == 0xFE) {
        index = D_801E86B0;
    }
    if (actor->aim_actor == 0xFD) {
        index = actor->index;
    }
    if (actor->aim_actor == 0xFC) {
        index = actor->b21;
    }
    if (actor->aim_actor == 0xFA) {
        index = 10;
    }
    if (actor->aim_actor > 0 && actor->aim_actor < 0x80) {
        index = actor->aim_actor - 1;
    }
    other = D_801E8670[index];
    if (other != NULL && index != actor->index) {
        m = SCRATCH_MATRIX;
        if (actor->aim_node != 0) {
            CompMatrix(&other->parts->local, &other->parts[actor->aim_node].world, SCRATCH_MATRIX);
        } else {
            m = &other->parts->local;
        }
        SetRotMatrix(m);
        SetTransMatrix(m);
        gte_ldv0(actor->aim_offset);
        gte_rtv0tr();
        gte_stlvnl(&world);
        actor->target[0] = world.vx;
        actor->target[1] = world.vy;
        actor->target[2] = world.vz;
        tween = actor->parts->attachments[0];
        if (tween != NULL && (u32)(tween->kind - 7) < 2) {
            tween->u.value[3] = world.vx;
            tween->u.value[4] = world.vy;
            tween->u.value[5] = world.vz;
        }
    }
}

/* Hide node `index` of `parts` and its descendants, showing the same nodes of
 * `other`, and release their attachments. */
void func_801E6578(SlotPool *pool, s32 index, ModelPart *parts, ModelPart *other) {
    ModelPart *child;
    s32 count;
    s32 i;

    child = parts;
    count = parts->count;
    parts[index].visible = 0;
    other[index].visible = 1;
    func_801DF7A8(pool, parts[index].attachments[0]);
    parts[index].attachments[0] = NULL;
    func_801DF7A8(pool, parts[index].attachments[1]);
    parts[index].attachments[1] = NULL;
    func_801DF7A8(pool, parts[index].attachments[2]);
    parts[index].attachments[2] = NULL;
    for (i = 1; i < count;) {
        child++;
        i++;
        if (child->parent == &parts[index]) {
            func_801E6578(pool, child->count, parts, other);
        }
    }
}

/* Move the visibility of every shown node of `parts` to the same node of
 * `other`. */
void func_801E6668(ModelPart *parts, ModelPart *other) {
    u16 count;
    s32 i;

    count = parts->count;
    for (i = 1; i < count; i++) {
        parts++;
        other++;
        if (parts->visible) {
            parts->visible = 0;
            other->visible = 1;
        }
    }
}

/* The cosine-like ratio of `dir` to the vector made from `a` and `b`, scaled
 * by 16 * 256 and divided by `divisor`. */
s16 func_801E66BC(VECTOR *dir, void *a, void *b, s32 divisor) {
    VECTOR v;
    s32 dot;
    s32 length;

    func_8004A480(a, b, &v);
    dot = v.vx * dir->vx + v.vy * dir->vy + v.vz * dir->vz;
    length = SquareRoot0(v.vx * v.vx + v.vy * v.vy + v.vz * v.vz) + 1;
    return (((dot * 16) / length) << 8) / divisor;
}

/* The lowest set bit of D_801E863C's low byte (8 when none). */
s32 func_801E67F8(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if ((D_801E863C >> i) & 1) {
            break;
        }
    }
    return i;
}

/* The actor index (returned) and bit mask of reference `ref` (0xff: the mask's lowest actor,
 * 0xfe: the current actor, 0xfd/0xf9: this actor, ...). */
s32 func_801E6830(Actor *actor, u8 ref, u16 *mask) {
    if (ref == 0xFF) {
        ref = func_801E67F8();
    } else if (ref == 0xFE) {
        ref = D_801E86B0;
    } else if (ref == 0xFD || ref == 0xF9) {
        ref = actor->index;
    } else if (ref == 0xFC) {
        ref = actor->b21;
    } else if (ref == 0xFA) {
        ref = 10;
    } else if (ref == 0xF8) {
        ref = actor->index * 2 + 8;
    } else if (ref == 0xF7) {
        ref = actor->index * 2 + 9;
    }
    *mask = 1 << ref;
    return ref;
}

/* Script variable `ref` (0xfe/0xff: the actor's default reference, whose bit
 * 7 is returned in `*flag`): a local below 0x40, else a global. */
s32 func_801E6910(Actor *actor, u8 ref, s32 *flag) {
    *flag = 0;
    if (ref >= 0xFE) {
        ref = actor->reference & 0x7F;
        *flag = actor->reference & 0x80;
    }
    if (ref < 0x40) {
        return actor->locals[ref + 1];
    }
    return actor->globals[ref - 0x3F];
}

/* Start a tween (kind `mode` + 3) of a node's rotation (flags & 7 = 0),
 * position (1) or scale from (x0, y0, z0) to (x1, y1, z1), both optionally
 * relative to the current values (flag bits 5 and 6; mode 0: the end is a
 * target, else a delta); modes below 2 also set the start at once. With
 * flag bit 7 the node's descendants get the same tween. */
void func_801E6974(Actor *actor, SlotPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag,
                   u8 smooth, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, s16 duration) {
    PoolSlot *tween;
    ModelPart *child;
    s16 bx;
    s16 by;
    s16 bz;
    s16 ex;
    s16 ey;
    s16 ez;
    s32 i;
    u8 type;

    type = flags & 7;
    if (type == 0) {
        tween = part->attachments[0];
    } else if (type == 1) {
        tween = part->attachments[1];
    } else {
        tween = part->attachments[2];
    }
    if (tween != NULL || (tween = func_801DF6F0(pool)) != NULL) {
        tween->used = 1;
        tween->flag = smooth;
        tween->kind = mode + 3;
        tween->tag = tag;
        if (flags & 0x20) {
            if (type == 0) {
                bx = part->rot.vx;
                by = part->rot.vy;
                bz = part->rot.vz;
            } else if (type == 1) {
                bx = part->pos[0];
                by = part->pos[1];
                bz = part->pos[2];
            } else {
                bx = part->scale[0];
                by = part->scale[1];
                bz = part->scale[2];
            }
        } else {
            bx = 0;
            by = 0;
            bz = 0;
        }
        if (flags & 0x40) {
            if (type == 0) {
                ex = part->rot.vx;
                ey = part->rot.vy;
                ez = part->rot.vz;
            } else if (type == 1) {
                ex = part->pos[0];
                ey = part->pos[1];
                ez = part->pos[2];
            } else {
                ex = part->scale[0];
                ey = part->scale[1];
                ez = part->scale[2];
            }
        } else {
            ex = 0;
            ey = 0;
            ez = 0;
        }
        tween->u.value[0] = x0 + bx;
        tween->u.value[1] = y0 + by;
        tween->u.value[2] = z0 + bz;
        if (mode == 0) {
            tween->u.value[3] = x1 + ex - tween->u.value[0];
            tween->u.value[4] = y1 + ey - tween->u.value[1];
            tween->u.value[5] = z1 + ez - tween->u.value[2];
        } else {
            tween->u.value[3] = x1 + ex;
            tween->u.value[4] = y1 + ey;
            tween->u.value[5] = z1 + ez;
        }
        tween->time = 0;
        tween->duration = duration;
        if (type == 0) {
            if (mode < 2) {
                part->rot.vx = tween->u.value[0];
                part->rot.vy = tween->u.value[1];
                part->rot.vz = tween->u.value[2];
            }
            part->attachments[0] = tween;
        } else if (type == 1) {
            if (mode < 2) {
                part->pos[0] = tween->u.value[0];
                part->pos[1] = tween->u.value[1];
                part->pos[2] = tween->u.value[2];
            }
            part->attachments[1] = tween;
        } else {
            if (mode < 2) {
                part->scale[0] = tween->u.value[0];
                part->scale[1] = tween->u.value[1];
                part->scale[2] = tween->u.value[2];
            }
            part->attachments[2] = tween;
        }
    }
    if (flags & 0x80) {
        child = actor->parts;
        for (i = 1; i < actor->parts->count; i++) {
            child++;
            if (child->parent == part) {
                func_801E6974(actor, pool, child, flags, mode, tag, smooth, x0, y0, z0, x1, y1, z1,
                              duration);
            }
        }
    }
}

/* Show or hide (flag bit 0) a node, and with bit 7 its descendants. */
void func_801E6D94(Actor *actor, ModelPart *part, s32 flags) {
    ModelPart *child;
    s32 i;

    part->visible = flags & 1;
    if (flags & 0x80) {
        child = actor->parts;
        for (i = 1; i < actor->parts->count; i++) {
            child++;
            if (child->parent == part) {
                func_801E6D94(actor, child, flags);
            }
        }
    }
}

/* Create a resident sprite linked to an actor node. */
void func_801E6E48(s32 a, s32 b, s32 c, s16 value, s16 scale, SpriteSpec *spec, Actor *actor) {
    Sprite *sprite;
    SpriteLink *link;

    sprite = func_80023FD8(b, a, c, 0x18);
    func_80021FE0(&sprite->body, value);
    func_800223B0(&sprite->body, value);
    func_80022000(&sprite->body, scale);
    link = (SpriteLink *)((u8 *)sprite + sprite->link);
    link->actor = actor;
    link->node = spec->node;
    if (spec->linked) {
        link->update = func_8001CD7C(sprite);
        func_8001CD6C(sprite, func_801E6F64);
        link->offset.vx = spec->offset[0];
        link->offset.vy = spec->offset[1];
        link->offset.vz = spec->offset[2];
        link->follow = spec->follow;
    }
}

/* Sprite update: place the sprite at its offset through its actor node,
 * then run its own update. */
void func_801E6F64(Sprite *sprite) {
    VECTOR world;
    SpriteLink *link;
    MATRIX *m;

    link = (SpriteLink *)((u8 *)sprite + sprite->link);
    m = SCRATCH_MATRIX;
    if (link->node != 0) {
        CompMatrix(&link->actor->parts->local, &link->actor->parts[link->node].world, SCRATCH_MATRIX);
    } else {
        m = &link->actor->parts->local;
    }
    SetRotMatrix(m);
    SetTransMatrix(m);
    gte_ldv0(&link->offset);
    gte_rtv0tr();
    gte_stlvnl(&world);
    if (link->follow) {
        world.vy = link->actor->h60;
    }
    sprite->body.x = world.vx << 16;
    sprite->body.y = world.vy << 16;
    sprite->body.z = world.vz << 16;
    link->update(sprite);
}

/* Set or (flag bit 5) add to a node's rotation (mode 0), position (1) or
 * scale, and with bit 7 its descendants'. */
void func_801E7094(Actor *actor, ModelPart *part, u8 flags, s16 x, s16 y, s16 z) {
    ModelPart *child;
    s32 i;

    if ((flags & 7) == 0) {
        if (flags & 0x20) {
            part->rot.vx += x;
            part->rot.vy += y;
            part->rot.vz += z;
        } else {
            part->rot.vx = x;
            part->rot.vy = y;
            part->rot.vz = z;
        }
    } else if ((flags & 7) == 1) {
        if (flags & 0x20) {
            part->pos[0] += x;
            part->pos[1] += y;
            part->pos[2] += z;
        } else {
            part->pos[0] = x;
            part->pos[1] = y;
            part->pos[2] = z;
        }
    } else {
        if (flags & 0x20) {
            part->scale[0] += x;
            part->scale[1] += y;
            part->scale[2] += z;
        } else {
            part->scale[0] = x;
            part->scale[1] = y;
            part->scale[2] = z;
        }
    }
    part->dirty = 1;
    part->rotate = 1;
    if (flags & 0x80) {
        child = actor->parts;
        for (i = 1; i < actor->parts->count; i++) {
            child++;
            if (child->parent == part) {
                func_801E7094(actor, child, flags, x, y, z);
            }
        }
    }
}

/* Put an actor's root at its height unless it is held. */
void func_801E7298(Actor *actor) {
    VECTOR unused;
    SVECTOR pos;

    pos.vy = actor->h60;
    if (actor->b36 == 0) {
        actor->parts->pos[1] = pos.vy;
    }
}

/* The world matrix of node `node` of actor `index` (its root's local matrix
 * for node 0). */
void func_801E72CC(MATRIX *out, s32 unused, s32 index, s32 node) {
    MATRIX m;
    Actor *actor;

    actor = D_801E8670[index];
    if (actor != NULL) {
        if (node != 0) {
            CompMatrix(&actor->parts->local, &actor->parts[node].world, out);
        } else {
            *out = actor->parts->local;
        }
    }
}

/* Set flag D_801E85CC from bit 0. */
void func_801E7378(s32 value) {
    D_801E85CC = value & 1;
}

/* Reset the module state: the tween pool of `slot_count` slots, the 16-particle
 * pool and the state tables. */
void func_801E738C(s32 slot_count) {
    s32 i;
    s32 j;
    s32 offset;

    D_801E8640 = 0;
    D_801E869C = 0;
    func_801DF5F4(&D_801E86A8, slot_count);
    func_801E0064(&D_801E86A0, 0x10);
    for (i = 9; i >= 0; i--) {
        D_801E8670[i] = 0;
    }
    for (j = 7; j >= 0; j--) {
        D_801E85F4[j].w0 = 0;
    }
    /* Both anchors' active flags, by byte offset. */
    for (offset = sizeof(Anchor); offset >= 0; offset -= sizeof(Anchor)) {
        *(s16 *)((u8 *)&D_801E8648[0].active + offset) = 0;
    }
}

/* Create actor `index` (when its slot is free) from its files: relocate them
 * (unless `flags` bit 0), load its sound bank (unless bit 2), copy its model
 * group into a free model list and build the hierarchy at `pos`, set up its
 * shadow quads, image animations and records24, reset its script (unless bit
 * 6) and keep a compacted copy of its model group (unless bit 1).
 * NON_MATCHING (same size): register allocation differs (the original has
 * actor s3, file then the loop index s4, links s0, desc/p s2), and the
 * original rematerializes &D_801E85F4 after func_801DC22C where this C keeps
 * it in s0. */
#ifdef NON_MATCHING
void func_801E742C(s32 index, u16 flags, ActorScript *script, ActorFile *file, s16 x, s16 y, s16 z,
                   s16 w, s16 *pos) {
    Actor *actor;
    ActorInfo *info;
    ActorDesc *desc;
    SoundBlock *bank;
    ScriptBlock *block;
    void *images;
    u8 *group;
    HierarchyLink *links;
    s32 size;
    s32 i, k;
    s32 count;
    Record24 *record;
    s16 *p;
    POLY_FT4 *prim;

    func_80032498(4, 0);
    if (index >= 10 || D_801E8670[index] != NULL) {
        return;
    }
    actor = func_80031BDC(sizeof(Actor), 0);
    if (!(flags & 1)) {
        func_8003342C(file);
        func_8003342C(file->info);
    }
    actor->b62 = 0;
    actor->b63 = 0;
    if (!(flags & 4)) {
        func_8003342C(script);
        func_8003342C(script->owner);
        block = script->script;
        func_8003342C(block);
        func_8003342C(block->locals);
        bank = (SoundBlock *)script->owner;
        if (bank->end != bank->bank && func_8003864C(bank->bank, 0) == 0) {
            func_80038428(bank->bank);
            actor->b62 = 1;
        }
    }
    info = file->info;
    desc = info->desc;
    images = file->images;
    group = file->group;
    links = file->links;
    D_801E8670[index] = actor;
    actor->size[0] = desc->size[0];
    actor->size[1] = desc->size[1];
    actor->size[2] = desc->size[2];
    actor->reference = desc->reference;
    actor->flags = desc->flags;
    size = (u8 *)links - group;
    if (actor->flags & 0x200) {
        func_80030988(2, 2, 0x40, 0x40);
    }
    if (!(flags & 1)) {
        /* i: the images are shown */
        i = 0;
        if (!(flags & 0x40) && !(actor->flags & 4)) {
            i = 1;
        }
        func_8002DDE4(images, (s16)i, x, y, (s16)i, z, w);
        D_801E8638 = func_80031BDC(size, 1);
        memcpy(D_801E8638, group, size);
        for (D_801E8634 = 0; D_801E8634 < 8; D_801E8634++) {
            if (D_801E85F4[D_801E8634].w0 == 0) {
                break;
            }
        }
        func_801DC22C(D_801E8638, (ModelList *)&D_801E85F4[D_801E8634]);
        actor->models = (ModelList *)&D_801E85F4[D_801E8634];
    }
    if (!(flags & 0x40)) {
        if (actor->flags & 4) {
            actor->parts = func_801DC2D0(actor->models, links, 2, 0, 0, 0, 0, 0);
        } else {
            actor->parts = func_801DC2D0(actor->models, links, 2, 1, x, y, z, w);
        }
    } else {
        actor->parts = func_801DC2D0(actor->models, links, 0, 0, 0, 0, 0, 0);
    }
    if (pos != NULL) {
        actor->parts->pos[0] = pos[0];
        actor->parts->pos[1] = pos[1];
        actor->parts->pos[2] = pos[2];
    }
    if (!(flags & 1)) {
        actor->h90 = x;
        actor->h92 = y;
        actor->shift_x = z;
        actor->shift_y = w;
    } else {
        actor->h90 = -1;
    }
    if (!(flags & 4) && !(flags & 0x80)) {
        actor->blockAC = script;
    } else {
        actor->blockAC = NULL;
    }
    for (i = 0; i < 2; i++) {
        prim = &actor->prims[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        actor->prims[i].r0 = 0x40;
        actor->prims[i].g0 = 0x40;
        actor->prims[i].b0 = 0x40;
        actor->prims[i].clut = GetClut(0x100, 0xF3);
        actor->prims[i].tpage = GetTPage(0, 2, 0x280, 0x100);
        actor->prims[i].u0 = 0;
        actor->prims[i].v0 = 0xE0;
        actor->prims[i].u1 = 0xF;
        actor->prims[i].v1 = 0xE0;
        actor->prims[i].u2 = 0;
        actor->prims[i].v2 = 0xEF;
        actor->prims[i].u3 = 0xF;
        actor->prims[i].v3 = 0xEF;
    }
    actor->scale = desc->scale;
    actor->channel_count = desc->channel_count;
    func_801E8510(actor);
    actor->count10E = desc->count30;
    if (actor->count10E != 0) {
        actor->records30 = func_80031BDC(actor->count10E * sizeof(ImageAnim), 0);
        for (i = 0; i < actor->count10E; i++) {
            actor->records30[i].active = 0;
            actor->records30[i].pixels = NULL;
            actor->records30[i].pixels2 = NULL;
            actor->records30[i].work = NULL;
        }
    }
    actor->count10D = desc->count24;
    if (actor->count10D != 0) {
        p = desc->records;
        record = func_80031BDC(actor->count10D * sizeof(Record24), 0);
        actor->records24 = record;
        for (i = 0; i < actor->count10D; record++, i++) {
            count = p[17];
            record->h0 = *p++;
            /* The parameters are read in order. */
            func_801E1A14(record, (u16 *)info->tables[i], *p++, *p++, *p++, *p++, *p++, count, x + *p++,
                          y + *p++, *p++, *p++, z + *p++, w, *p++, *p++, *p++, *p++, *p++, *p++);
            p += 2;
            for (k = 0; k < count; k++) {
                record->block18[k].h6 = *p++;
                record->block18[k].hE = *p++;
                record->block18[k].h0 = *p++;
                record->block18[k].h2 = *p++;
                record->block18[k].h4 = *p++;
            }
        }
    }
    actor->b22 = 0;
    actor->active = 1;
    actor->index = index;
    if (!(flags & 0x40)) {
        block = script->script;
        actor->ownerB0 = script->owner;
        func_801E3534(actor, &D_801E86A8, block->entries, block->locals);
        func_801E35D0(actor, actor, &D_801E86A8, 0);
    }
    if (!(flags & 2)) {
        func_8002C644(D_801E8638);
        func_8002C4BC(D_801E8638);
        size = func_80031894(D_801E8638);
        group = func_80031BDC(size, 0);
        memcpy(group, D_801E8638, size);
        func_800320E8(D_801E8638);
        func_801DCE18(actor->models, 0);
        func_801DC22C(group, actor->models);
        actor->group = group;
    } else {
        actor->group = NULL;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E742C);
#endif

/* Advance the scene by the elapsed half-frames: step every actor, carry the
 * carried ones, place the anchors, then draw the actors and particles. */
void func_801E7D14(MATRIX *m, MATRIX *light, u32 *ot, s32 buffer, s32 elapsed) {
    SVECTOR unused;
    Actor *actor;
    s32 steps;
    s32 i;

    steps = 0;
    D_801E8640 += elapsed + 1;
    if (D_801E8640 >= 7) {
        D_801E8640 = 6;
    }
    while (D_801E8640 >= 2) {
        D_801E8640 -= 2;
        steps++;
    }
    D_801E869C += steps * 56;
    D_801E8698 = (func_8003F8CC(D_801E869C) + 0x1000) / 800 + 4;
    for (i = 0; i < 10; i++) {
        if (D_801E8670[i] != NULL) {
            D_801E8670[i]->previous[0] = D_801E8670[i]->parts->pos[0];
            D_801E8670[i]->previous[1] = D_801E8670[i]->parts->pos[1];
            D_801E8670[i]->previous[2] = D_801E8670[i]->parts->pos[2];
            func_801E36BC(D_801E8670[i], &D_801E86A8, steps, buffer, 1);
        }
    }
    for (i = 0; i < 10; i++) {
        actor = D_801E8670[i];
        if (actor != NULL && actor->parent < 0xFF) {
            func_801E37D0(actor);
        }
    }
    func_801E1880(D_801E8670);
    SetColorMatrix(D_801E8644);
    for (i = 0; i < 10; i++) {
        if (D_801E8670[i] != NULL) {
            D_801E8670[i]->moved[0] = D_801E8670[i]->previous[0] - D_801E8670[i]->parts->pos[0];
            D_801E8670[i]->moved[1] = D_801E8670[i]->previous[1] - D_801E8670[i]->parts->pos[1];
            D_801E8670[i]->moved[2] = D_801E8670[i]->previous[2] - D_801E8670[i]->parts->pos[2];
            func_801DCEC8(D_801E8670[i], m, light, 1, 1, ot, buffer);
        }
    }
    func_801E0398(&D_801E86A0, m, steps, ot, buffer);
}

/* Release every actor and both pools. */
void func_801E7FD4(void) {
    s32 i;

    for (i = 0; i < 10; i++) {
        func_801E8030(i);
    }
    func_801DF668(&D_801E86A8);
    func_801E00DC(&D_801E86A0);
}

/* Release actor `index`: its model group, sound bank, hierarchy (actors 8
 * and 9 share their models), channels and records. */
void func_801E8030(s32 index) {
    s32 i;

    if (D_801E8670[index] != NULL) {
        if (D_801E8670[index]->group != NULL) {
            func_800320E8(D_801E8670[index]->group);
            func_801DCE18(D_801E8670[index]->models, 1);
        }
        if (D_801E8670[index]->b62) {
            func_8003852C(D_801E8670[index]->ownerB0->view);
        }
        if (D_801E8670[index]->blockAC != NULL) {
            func_800320E8(D_801E8670[index]->blockAC);
        }
        if (index != 8 && index != 9) {
            if (D_801E8670[index]->parts != NULL) {
                func_801DFE8C(&D_801E86A8, D_801E8670[index]->parts);
                func_801DFF78(&D_801E86A8, D_801E8670[index]->parts, 0xFF);
                func_801DCD8C(D_801E8670[index]->parts);
                D_801E8670[index]->models = NULL;
                D_801E8670[index]->parts = NULL;
            }
        } else if (D_801E8670[index]->parts != NULL) {
            func_801DFE8C(&D_801E86A8, D_801E8670[index]->parts);
            func_801DFF78(&D_801E86A8, D_801E8670[index]->parts, 0xFF);
            func_800320E8(D_801E8670[index]->parts);
        }
        if (D_801E8670[index]->channel_count != 0) {
            func_800320E8(D_801E8670[index]->channels);
        }
        if (D_801E8670[index]->count10E != 0) {
            for (i = 0; i < D_801E8670[index]->count10E; i++) {
                func_801E165C(&D_801E8670[index]->records30[i]);
            }
            func_800320E8(D_801E8670[index]->records30);
        }
        if (D_801E8670[index]->count10D != 0) {
            for (i = 0; i < D_801E8670[index]->count10D; i++) {
                func_801E3438(&D_801E8670[index]->records24[i]);
            }
            func_800320E8(D_801E8670[index]->records24);
        }
        func_800320E8(D_801E8670[index]);
        D_801E8670[index] = NULL;
    }
}

/* Select actor `index` and bit mask `mask`, then run its script step
 * (func_801E35D0) with itself as the source. */
void func_801E8330(u16 index, u16 mask, s32 arg2) {
    Actor *actor;

    actor = D_801E8670[index];
    D_801E86B0 = index;
    D_801E863C = mask;
    actor->b35 = 0;
    if (D_801E8670[index] != NULL) {
        func_801E35D0(D_801E8670[index], D_801E8670[index], &D_801E86A8, arg2);
    }
}

/* Select actor `index` and bit mask `mask`, then run its script step with
 * `source` unless it is the actor of the mask's lowest bit. */
void func_801E8394(Actor *source, u16 index, u16 mask, s32 arg3) {
    Actor *actor;

    actor = D_801E8670[index];
    D_801E86B0 = index;
    D_801E863C = mask;
    actor->b35 = 0;
    if (D_801E8670[index] != NULL && index != func_801E67F8()) {
        func_801E35D0(D_801E8670[index], source, &D_801E86A8, arg3);
    }
}

/* Actor `index`'s height: its size scaled by its scale and the root's y
 * scale; 0 without an actor. */
s32 func_801E8430(s32 index) {
    Actor *actor;

    actor = D_801E8670[index];
    if (actor != NULL) {
        return (actor->size[0] * ((actor->scale * actor->parts->scale[1]) >> 12)) >> 12;
    }
    return 0;
}

/* Actor `index`'s width: its x or (flag 8 clear) z size scaled like
 * func_801E8430; 0 without an actor. */
s32 func_801E8480(s32 index) {
    Actor *actor;
    s32 size;
    s32 root_scale;

    actor = D_801E8670[index];
    if (actor != NULL) {
        if (actor->flags & 8) {
            size = actor->size[1];
            root_scale = actor->parts->scale[0];
        } else {
            size = actor->size[2];
            root_scale = actor->parts->scale[2];
        }
        return (size * ((D_801E8670[index]->scale * root_scale) >> 12)) >> 12;
    }
    return 0;
}

/* Allocate an actor's 0x70-byte channel records, each marked unused. */
void func_801E8510(Actor *actor) {
    Channel *channels;
    s32 i;

    if (actor->channel_count != 0) {
        channels = func_80031BDC(actor->channel_count * sizeof(Channel), 0);
        for (i = 0; i < actor->channel_count; i++) {
            channels[i].id = -1;
            channels[i].particle = NULL;
        }
        actor->channels = channels;
    }
}
