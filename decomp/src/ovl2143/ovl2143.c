/* ovl2143 (Disc 1 slot 2143 / Disc 2 slot 2138), the actor module linked at
 * 0x801dc000; ovl2143/actors.h has its records and the entries its callers
 * use, and says who loads it (the field, for its layers, and the Gear parts
 * shop, menu screen 5). Up to ten actors (D_801E8670), each a model hierarchy
 * (0x7c-byte parts built from a relocated model group, 8002c3e8/8002cb54/
 * 8002c8cc) with rotation/movement tweens from an effect pool, a 16-sprite
 * pool of textured quads, and actor effect scripts (801e39f0,
 * docs/scripts/battle-effect-vm.md). It drives the GTE directly (RotMatrix/
 * CompMatrix/MulMatrix0, RTPS/RTPT in 801dcec8 and 801e0398) and uses the
 * resident heap (80031bdc/800320e8, tag 4), libgpu and sound effect banks
 * (8003852c). It has no strings. Built with GCC 2.6.3 (see func_801DF7A8)
 * and ASPSX-style checked divisions.
 *
 * Most of its functions are the battle's model and effect library (8009E53C's
 * unit) linked again. 64 are the same instructions apart from addresses (54
 * of them apart from jal/j targets only): 801DC22C-801DCE18 (the battle's
 * 8009EBA8-8009F794), 801DDBF8-801E1880 (800A0838-800A44C0) but for
 * 801E011C, 801E1A14-801E3438 (800A7064-800A8A88), 801E34BC-801E37D0
 * (800AA820-800AAB34), 801E59D4-801E5C74 (800ADF1C-800AE1BC),
 * 801E632C/801E6338 (800AEEEC/800AEEF8), 801E6578-801E66BC (800AF180-
 * 800AF2C4), 801E6974-801E7094 (800AF678-800AFD98), 801E8330 (800AA320) and
 * 801E8394-801E8510 (800AA564-800AA6E0). 801E67F8, 801E6830 and 801E7FD4 are
 * 800AF400, 800AF438 and 800A9F94 with the module's counts (10 actors, 8 in
 * the reference mask, where the battle has 31 objects and 13 slots), and
 * 801E011C is 800A2D5C with another texture page and row. Shorter versions
 * take the places of 8009F844 (the actor draw 801DCEC8), 800AAD54 (the effect
 * VM 801E39F0), 800AE220 (801E5CD8, without its fourth bank source), 800AE2A4
 * (the event runner 801E5D44), 800AEF68 (801E63A8), 800AF518 (801E6910,
 * without its choice by gear warnings), 800AFF9C (801E7298, without the
 * scene's ground) and 800A9FF0 (801E8030). The entries 801E72CC-801E7D14 are
 * the module's own. The copies use the battle's records (battle/model.h,
 * battle/effect.h); their C keeps its own local names and, where it compiles
 * alike, some views of its own (the Actor where the battle has BattleObject,
 * integer widths, statement shapes).
 *
 * The whole image is this one unit: rodata 801DC000-801DC22C, text
 * 801DC22C-801E8590, data 801E8590-801E85CC and the module state to
 * 801E86B4; its rodata's jump tables keep one phase (padded twice). Its own
 * declarations are split by subsystem: model hierarchies and tweens
 * (hierarchy.h), effect sprites and colour fades (particles.h), image
 * animations (image_anim.h), surfaces (surface.h) and the actors' functions
 * (actor.h). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "gte.h"
#include "hierarchy.h"
#include "particles.h"
#include "image_anim.h"
#include "surface.h"

/* Copies of the battle overlay's extra file bases (800c3530) and gear file
 * table (800c3508, base and variant count per gear); this module never reads
 * them. The table ends the unit's data with 66 00 where battle's has 0, 0: a
 * twentieth pair, or stray bytes after 19 (open, docs/matching.md), so it
 * stays original data (ovl2143.classification.txt). */
u8 D_801E8590[] = {1, 108, 164, 99, 94, 220, 22, 123, 151, 158, 161, 143, 139, 141, 40, 214, 219, 0};
INCLUDE_ORIGINAL(".data", D_801E85A4, 0x801E85A4, 40);

/* The module state, zero in the file (its .bss, loaded), each object in a
 * slot of whole words (decomp/Makefile): D_801E869C starts its own slot
 * after D_801E8698, and the file ends with the rest of D_801E86B0's. GCC
 * emits tentative definitions in the order of their first declaration, so
 * the state is defined ahead of ovl2143/actors.h, which declares two of
 * them for the module's callers (the actors by their structure's tag). */
s32 D_801E85CC;
u8 D_801E85D0[36]; /* never read */
ModelTable D_801E85F4[8];
s32 D_801E8634;
u8 *D_801E8638;
u16 D_801E863C;
s32 D_801E8640;
MATRIX *D_801E8644;
Tracker D_801E8648[2];
struct Actor *D_801E8670[10];
s16 D_801E8698;
s16 D_801E869C;
SpritePool D_801E86A0;
EffectPool D_801E86A8;
u16 D_801E86B0;

#include "actor.h"

/* Relocate a model group and list its model records (0x38 bytes each after the
 * 0x10-byte header) in a new block. */
ModelTable *func_801DC22C(u8 *group, ModelTable *list) {
    u32 count;
    u32 i;

    func_80032498(4, 0);
    count = func_8002C3E8((ModelGroup *)group);
    list->models = func_80031BDC(count * 4, 0);
    list->count = count;
    if (list->models != NULL) {
        for (i = 0; i < count; i++) {
            list->models[i] = (SpriteModel *)(group + 0x10 + i * 0x38);
        }
    }
    return list;
}

/* Build a model hierarchy from (model, parent) pairs, up to the first pair
 * naming neither a listed model nor 0xFFFF: a root part, then one part per
 * pair with its packets for both buffers (built with mode; offset by (x0, y0)
 * and (x1, y1) when offset is set). Returns the root, NULL on failure. */
ModelPart *func_801DC2D0(ModelTable *list, u16 *hierarchy, s32 mode, s32 offset, s16 x0, s16 y0,
                         s16 x1, s16 y1) {
    ModelPart *root;
    ModelPart *part;
    u16 *pair;
    s32 count;
    s16 index;
    u16 id;
    u16 parent;

    func_80032498(4, 0);
    pair = hierarchy;
    count = 0;
    while (pair[0] < list->count || pair[0] == 0xFFFF) {
        count++;
        pair += 2;
    }
    if (count == 0) {
        return NULL;
    }
    count++;
    root = func_80031BDC(count * sizeof(ModelPart), 0);
    pair = hierarchy;
    if (root == NULL) {
        return NULL;
    }
    part = root + 1;
    index = 1;
    id = pair[0];
    parent = pair[1];
    root->dirty = 1;
    root->rotate = 1;
    root->yxz = 1;
    root->scale[0] = 0x1000;
    root->scale[1] = 0x1000;
    root->scale[2] = 0x1000;
    root->parent = NULL;
    root->visible = 0;
    root->modelId = 0xFFFF;
    root->index = count;
    root->packets[0] = NULL;
    root->packets[1] = NULL;
    root->rotation.vx = 0;
    root->rotation.vy = 0;
    root->rotation.vz = 0;
    root->translation[0] = 0;
    root->translation[1] = 0;
    root->translation[2] = 0;
    root->effects[0] = NULL;
    root->effects[1] = NULL;
    root->effects[2] = NULL;
    while (id < list->count || id == 0xFFFF) {
        if (parent == 0xFFFF) {
            part->parent = NULL;
        } else {
            part->parent = root + parent + 1;
        }
        part->index = index;
        index++;
        part->dirty = 1;
        part->rotate = 1;
        part->visible = 1;
        part->scale[0] = 0x1000;
        part->scale[1] = 0x1000;
        part->scale[2] = 0x1000;
        part->yxz = 0;
        part->field52 = 0;
        part->modelId = id;
        if (id != 0xFFFF) {
            func_8002CB54(list->models[id], &part->packets[0], &part->packets[1]);
            if (part->packets[0] == NULL) {
                func_801DCD8C(root);
                return NULL;
            }
            if (offset) {
                func_8002CC10(x0, y0);
                func_8002CC74(x1, y1);
            }
            func_8002C8CC(list->models[id], part->packets[0], mode);
            memcpy(part->packets[1], part->packets[0], list->models[id]->packet_size);
            part->rotation.vx = 0;
        } else {
            part->packets[0] = NULL;
            part->packets[1] = NULL;
            part->rotation.vx = 0;
        }
        part->rotation.vy = 0;
        part->rotation.vz = 0;
        part->translation[0] = 0;
        part->translation[1] = 0;
        part->translation[2] = 0;
        part->effects[0] = NULL;
        part->effects[1] = NULL;
        part->effects[2] = NULL;
        part++;
        pair += 2;
        id = pair[0];
        parent = pair[1];
    }
    return root;
}

/* Recompose a hierarchy's matrices: the root's world matrix from its rotation
 * and position, its transform scaled by `scale`; each other node's transform
 * from its rotation when flagged and its world matrix from its parent when it
 * or its parent changed. Returns the node count. */
u32 func_801DC5C0(ModelPart *parts, s32 scale) {
    MATRIX *scaling = (MATRIX *)0x1F800000;
    ModelPart *part;
    s32 product;
    u32 count;
    u32 i;

    part = parts;
    count = part->index;
    part->world.t[0] = part->translation[0];
    part->world.t[1] = part->translation[1];
    part->world.t[2] = part->translation[2];
    if (part->yxz) {
        RotMatrixYXZ(&part->rotation, &part->world);
    } else {
        func_8003F738(&part->rotation, &part->world);
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
    MulMatrix0(&parts->world, scaling, &parts->transform);
    parts->transform.t[0] = parts->world.t[0];
    parts->transform.t[1] = parts->world.t[1];
    parts->transform.t[2] = parts->world.t[2];

    for (i = 1; i < count; i++) {
        parts++;
        if (parts->rotate) {
            if (parts->yxz) {
                RotMatrixYXZ(&parts->rotation, &parts->transform);
                parts->rotate = 0;
            } else {
                func_8003F738(&parts->rotation, &parts->transform);
                parts->rotate = 0;
            }
        }
        if (parts->parent != NULL && parts->parent->dirty == 1) {
            parts->dirty = 1;
        }
        if (parts->dirty) {
            parts->transform.t[0] = parts->translation[0];
            parts->transform.t[1] = parts->translation[1];
            parts->transform.t[2] = parts->translation[2];
            if (parts->parent != NULL) {
                CompMatrix(&parts->parent->world, &parts->transform, &parts->world);
            } else {
                parts->world = parts->transform;
            }
        }
    }
    for (i = 1; i < count; i++) {
        part++;
        part->dirty = 0;
    }
    return count;
}

/* Like func_801DC5C0, with scaled nodes: a rebuilt transform takes the
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
    count = part->index;
    part->world.t[0] = part->translation[0];
    part->world.t[1] = part->translation[1];
    part->world.t[2] = part->translation[2];
    if (part->yxz) {
        RotMatrixYXZ(&part->rotation, &part->world);
    } else {
        func_8003F738(&part->rotation, &part->world);
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
    MulMatrix0(&parts->world, scaling, &parts->transform);
    parts->transform.t[0] = parts->world.t[0];
    parts->transform.t[1] = parts->world.t[1];
    parts->transform.t[2] = parts->world.t[2];

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
                RotMatrixYXZ(&parts->rotation, &parts->transform);
            } else {
                func_8003F738(&parts->rotation, &parts->transform);
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
            MulMatrix0(&parts->transform, scratch, &parts->transform);
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
                MulMatrix0(scratch, &parts->transform, &parts->transform);
            }
        }
        if (parts->dirty) {
            parts->transform.t[0] = parts->translation[0];
            parts->transform.t[1] = parts->translation[1];
            parts->transform.t[2] = parts->translation[2];
            if (parts->parent != NULL) {
                CompMatrix(&parts->parent->world, &parts->transform, &parts->world);
            } else {
                parts->world = parts->transform;
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

/* An empty function, kept in its place. */
void func_801DCC34(void) {
}

/* Draw a hierarchy's model nodes: each node's world matrix is combined with the
 * root's light and view transforms (MulMatrix0 into the light matrix,
 * CompMatrix into the rotation/translation) before its buffer's packets are
 * drawn (8002c700). */
void func_801DCC3C(ModelTable *group, ModelPart *parts, MATRIX *view, MATRIX *light, s32 mode,
                   u32 *ot, s32 buffer) {
    MATRIX *light_root = (MATRIX *)0x1F800020;
    MATRIX *view_root = (MATRIX *)0x1F800040;
    MATRIX *m = (MATRIX *)0x1F800000;
    u32 count;
    u32 i;

    MulMatrix0(light, &parts->world, light_root);
    CompMatrix(view, &parts->transform, view_root);
    count = parts->index;
    parts++;
    for (i = 1; i < count; parts++) {
        i++;
        if (parts->modelId != 0xFFFF) {
            MulMatrix0(light_root, &parts->world, m);
            SetLightMatrix(m);
            CompMatrix(view_root, &parts->world, m);
            SetRotMatrix(m);
            SetTransMatrix(m);
            func_8002C700(group->models[parts->modelId], parts->packets[buffer], ot, mode);
        }
    }
}

/* Release a hierarchy: every node's packet buffers, then the nodes. */
void func_801DCD8C(ModelPart *parts) {
    ModelPart *part;
    s32 i;

    if (parts != NULL) {
        part = parts;
        for (i = 0; i < parts->index; part++) {
            i++;
            if (part->packets[0] != NULL) {
                func_800320E8(part->packets[0]);
                part->packets[0] = NULL;
                part->packets[1] = NULL;
            }
        }
        parts->index = 0;
        func_800320E8(parts);
    }
}

/* Release a model list; with `release_models` each model's own packets too
 * (8002cbbc). */
void func_801DCE18(ModelTable *list, s32 release_models) {
    u32 i;

    if (list != NULL) {
        for (i = 0; i < list->count; i++) {
            if (list->models != NULL && list->models[i] != NULL && release_models) {
                func_8002CBBC((ModelBuffer *)list->models[i]);
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

/* Draw an active actor under the camera `m`: its shadow quad at its ground
 * height (unless flags bit 0; the depth also sets b39), every visible model
 * node (lit by `light`; billboard nodes face the view), its surfaces, its
 * image animations (advanced by `ticks`) and its colour fades' ribbons. A
 * channel whose frame count wraps to 0 leaves the channel pointer where it
 * is, so the next channel index redraws it (as the original). */
void func_801DCEC8(Actor *actor, MATRIX *m, MATRIX *light, s32 mode, s32 ticks, u32 *ot, s32 buffer) {
    MATRIX *scratch = (MATRIX *)0x1F800000;
    MATRIX *placed = (MATRIX *)0x1F800020;
    SVECTOR v;
    VECTOR front, back;
    s32 depth, depth2;
    ModelTable *models;
    ModelPart *part;
    ModelPart *parts;
    u16 scale;
    MATRIX *view = (MATRIX *)0x1F800040;
    u32 count;
    s32 i;
    s32 k, side;
    s32 shade;
    Surface *record;
    SurfaceEntry *entry;
    ImageAnim *anim;
    ColorFade *ch;
    VECTOR *end0, *end1;
    VECTOR *start0, *start1;
    s32 oldest;

    if (!actor->active) {
        return;
    }
    part = actor->parts;
    models = actor->models;
    scale = actor->scale;
    count = part->index;
    parts = part;
    CompMatrix(m, &part->transform, view);
    if (!(actor->flags & 1)) {
        CompMatrix(&part->transform, &actor->parts[1].world, scratch);
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
        placed->t[1] = actor->groundY;
        placed->t[2] = back.vz;
        CompMatrix(m, placed, placed);
        shade = actor->scale - (actor->groundY - actor->parts->translation[1]) / 4;
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
        libgte_multiply_matrix_in_place(placed, scratch);
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
        if (part->modelId == 0xFFFF || !part->visible) {
            continue;
        }
        MulMatrix0(placed, &part->world, scratch);
        SetLightMatrix(scratch);
        CompMatrix(view, &part->world, scratch);
        if ((s16)part->field52 > 0) {
            scratch->m[0][0] = actor->scale;
            scratch->m[0][2] = 0;
            scratch->m[1][0] = 0;
            scratch->m[1][2] = 0;
            scratch->m[2][0] = 0;
            scratch->m[2][2] = actor->scale;
            if ((s16)part->field52 == 1) {
                SET_COLUMN_Y(scratch, view->m[0][1], view->m[1][1], view->m[2][1]);
            } else {
                SET_COLUMN_Y(scratch, 0, actor->scale, 0);
            }
        }
        SetRotMatrix(scratch);
        SetTransMatrix(scratch);
        func_8002C700(models->models[part->modelId], part->packets[buffer], ot, mode);
    }
    record = actor->surfaces;
    for (i = 0; i < actor->surfaceCount; record++, i++) {
        VECTOR out;
        SVECTOR sun;

        if ((s16)record->h0 < 0) {
            continue;
        }
        sun.vx = -D_801E8698 * func_8003F8CC(parts->rotation.vy + 0x400) / 4096;
        sun.vz = D_801E8698 * func_8003F8B0(parts->rotation.vy + 0x400) / 4096;
        sun.vy = actor->h3E;
        CompMatrix(&parts->transform, &parts[(s16)record->h0].world, scratch);
        SetRotMatrix(scratch);
        SetTransMatrix(scratch);
        for (k = 0; k < record->rings; k++) {
            gte_ldv0(&record->centres[k]);
            gte_rtv0tr();
            gte_stlvnl(&out);
            record->strands[k]->pos[0] = out.vx;
            record->strands[k]->pos[1] = out.vy;
            record->strands[k]->pos[2] = out.vz;
        }
        entry = record->entries;
        for (k = 0; k < record->entryCount; k++, entry++) {
            CompMatrix(&parts->transform, &parts[entry->h6].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            gte_ldv0(entry);
            gte_rtv0tr();
            gte_stlvnl(&out);
            entry->h8 = out.vx;
            entry->hA = out.vy;
            entry->hC = out.vz;
        }
        func_801E22F8(record, &sun, m, ot, buffer, scale, actor->groundY);
    }
    anim = actor->images;
    for (i = 0; i < actor->imageCount; i++, anim++) {
        func_801E1258(anim, ticks);
    }
    ch = actor->channels;
    for (i = 0; i < actor->channel_count; i++) {
        if (ch->id < 0) {
            ch++;
            continue;
        }
        ch->time = (ch->time - 1) & 7;
        if (!ch->solid) {
            CompMatrix(view, &parts[ch->id].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            gte_ldv0(&ch->ends[0]);
            gte_rtps();
            gte_stsxy(&ch->history.screen.first[ch->time]);
            gte_ldv0(&ch->ends[1]);
            gte_rtps();
            gte_stsxy(&ch->history.screen.second[ch->time]);
            if (++ch->count == 0) {
                continue;
            }
            if (ch->max < ch->count || ch->sprite == NULL) {
                ch->count = 1;
                ch->sprite = func_801E0248(ch->pool, ch->semiTrans);
                ch->sprite->projected = 0;
                ch->sprite->age = 0;
                ch->sprite->lifetime = ch->duration;
                ch->sprite->color[0] = ch->color[0];
                ch->sprite->color[1] = ch->color[1];
                ch->sprite->color[2] = ch->color[2];
                ch->sprite->fade[0] = ch->step[0];
                ch->sprite->fade[1] = ch->step[1];
                ch->sprite->fade[2] = ch->step[2];
                oldest = (ch->time + ch->count) & 7;
                ch->sprite->x0 = (ch->history.screen.first + oldest)->vx;
                ch->sprite->y0 = (ch->history.screen.first + oldest)->vy;
                ch->sprite->x2 = (ch->history.screen.second + oldest)->vx;
                ch->sprite->y2 = (ch->history.screen.second + oldest)->vy;
            }
            ch->sprite->x1 = (ch->history.screen.first + ch->time)->vx;
            ch->sprite->y1 = (ch->history.screen.first + ch->time)->vy;
            ch->sprite->x3 = (ch->history.screen.second + ch->time)->vx;
            ch->sprite->y3 = (ch->history.screen.second + ch->time)->vy;
        } else {
            CompMatrix(&parts->transform, &parts[ch->id].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            side = ch->time & 1;
            end0 = &ch->history.world.first[side];
            end1 = &ch->history.world.second[side];
            gte_ldv0(&ch->ends[0]);
            gte_rtv0tr();
            gte_stlvnl(end0);
            gte_ldv0(&ch->ends[1]);
            gte_rtv0tr();
            gte_stlvnl(end1);
            if (++ch->count == 0) {
                continue;
            }
            if (ch->max < ch->count || ch->sprite == NULL) {
                ch->count = 1;
                ch->sprite = func_801E0248(ch->pool, ch->semiTrans);
                ch->sprite->projected = 1;
                ch->sprite->age = 0;
                ch->sprite->lifetime = ch->duration;
                ch->sprite->color[0] = ch->color[0];
                ch->sprite->color[1] = ch->color[1];
                ch->sprite->color[2] = ch->color[2];
                ch->sprite->fade[0] = ch->step[0];
                ch->sprite->fade[1] = ch->step[1];
                ch->sprite->fade[2] = ch->step[2];
                start0 = &ch->history.world.first[1 - side];
                ch->sprite->x0 = start0->vx;
                ch->sprite->y0 = start0->vy;
                ch->sprite->z0 = start0->vz;
                start1 = &ch->history.world.second[1 - side];
                ch->sprite->x2 = start1->vx;
                ch->sprite->y2 = start1->vy;
                ch->sprite->z2 = start1->vz;
            }
            ch->sprite->x1 = end0->vx;
            ch->sprite->y1 = end0->vy;
            ch->sprite->z1 = end0->vz;
            ch->sprite->x3 = end1->vx;
            ch->sprite->y3 = end1->vy;
            ch->sprite->z3 = end1->vz;
        }
        ch++;
    }
}

/* Step one coordinate along a delta track: a signed byte delta, or -0x80
 * followed by an absolute 16-bit value (low byte first). */
#define TRACK_STEP(dst, pos)                \
    {                                       \
        s8 d = *(pos)++;                    \
        if (d != -0x80) {                   \
            (dst) += d;                     \
        } else {                            \
            low = *(pos)++;                 \
            high = *(s8 *)(pos)++;          \
            (dst) = low | (high << 8);      \
        }                                   \
    }

/* Step the tweens attached to each node of a hierarchy: its rotation
 * (attachment 0: set, delta or add from a track, interpolate, approach,
 * spin, or turn toward a point within a growing limit), position
 * (attachment 1: the same, or a move in the node's frame scaled by `scale`)
 * and scale (attachment 2: interpolate, approach, spin). A finished tween is
 * released (or restarted when looping: tracks rewind, others stop). Returns
 * flags: 0x100/0x200/0x400 some tween ran/ended/looped (1/2/4 when it has
 * `tag`). As in the original, the "spin" stop clears the step of the last
 * slot a computing kind used, which may belong to an earlier node. */
s32 func_801DDBF8(EffectPool *pool, ModelPart *parts, s32 tag, s32 scale) {
    Tween *slot;
    Tween *last;
    u8 *track;
    SVECTOR move;
    VECTOR moved;
    u32 i;
    u32 count;
    s32 result = 0;
    u32 kind;
    u32 pos_kind;
    u8 mode;
    s16 time;
    s32 dx, dy, dz, dist, limit, turn;
    s32 low, high;

    count = parts->index;
    for (i = 0; i < count; i++, parts++) {
        if (parts->effects[0] != NULL) {
            slot = (Tween *)parts->effects[0];
            kind = slot->kind;
            switch (kind & 0xF) {
            case 0:
                track = slot->u.track.cursor;
                if (!(kind & 0x10)) {
                    parts->rotation.vx = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x20)) {
                    parts->rotation.vy = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x40)) {
                    parts->rotation.vz = *(u16 *)track;
                    slot->u.track.cursor += 2;
                }
                break;
            case 1:
                if (!(kind & 0x10)) {
                    TRACK_STEP(parts->rotation.vx, slot->u.track.cursor);
                }
                if (!(kind & 0x20)) {
                    TRACK_STEP(parts->rotation.vy, slot->u.track.cursor);
                }
                if (!(kind & 0x40)) {
                    TRACK_STEP(parts->rotation.vz, slot->u.track.cursor);
                }
                break;
            case 2:
                track = slot->u.track.cursor;
                if (!(kind & 0x10)) {
                    parts->rotation.vx += *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x20)) {
                    parts->rotation.vy += *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x40)) {
                    parts->rotation.vz += *(u16 *)track;
                    slot->u.track.cursor += 2;
                }
                break;
            case 3:
                time = slot->time + 1;
                parts->rotation.vx = slot->u.values[0] + slot->u.values[3] * time / slot->duration;
                parts->rotation.vy = slot->u.values[1] + slot->u.values[4] * time / slot->duration;
                last = slot;
                parts->rotation.vz = slot->u.values[2] + slot->u.values[5] * time / slot->duration;
                break;
            case 4: {
                s16 sx, sy, sz;
                sx = (slot->u.values[3] - parts->rotation.vx) / slot->duration;
                sy = (slot->u.values[4] - parts->rotation.vy) / slot->duration;
                sz = (slot->u.values[5] - parts->rotation.vz) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    parts->rotation.vx = slot->u.values[3];
                    parts->rotation.vy = slot->u.values[4];
                    parts->rotation.vz = slot->u.values[5];
                } else {
                    parts->rotation.vx += sx;
                    parts->rotation.vy += sy;
                    parts->rotation.vz += sz;
                    slot->time = 0;
                }
                break;
            }
            case 5:
                slot->u.values[0] += slot->u.values[3];
                parts->rotation.vx += slot->u.values[0];
                slot->u.values[1] += slot->u.values[4];
                parts->rotation.vy += slot->u.values[1];
                slot->u.values[2] += slot->u.values[5];
                last = slot;
                parts->rotation.vz += slot->u.values[2];
                break;
            case 7:
            case 8:
                dx = slot->u.values[3] - parts->translation[0];
                dy = slot->u.values[4] - parts->translation[1];
                dz = slot->u.values[5] - parts->translation[2];
                dist = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
                last = slot;
                turn = (ratan2(-dx, -dz) - parts->rotation.vy) & 0xFFF;
                if (turn >= 0x800) {
                    turn -= 0x1000;
                }
                limit = slot->u.values[1] + (dist + slot->time) * slot->u.values[2] / slot->u.values[0];
                if ((turn < 0 ? -turn : turn) < limit) {
                    parts->rotation.vy += turn;
                } else if (turn < 0) {
                    parts->rotation.vy -= limit;
                } else {
                    parts->rotation.vy += limit;
                }
                if ((kind & 0xF) == 7) {
                    turn = (ratan2(dy, SquareRoot0(dx * dx + dz * dz)) - parts->rotation.vx) & 0xFFF;
                    if (turn >= 0x800) {
                        turn -= 0x1000;
                    }
                    if ((turn < 0 ? -turn : turn) < limit) {
                        parts->rotation.vx += turn;
                    } else if (turn < 0) {
                        parts->rotation.vx -= limit;
                    } else {
                        parts->rotation.vx += limit;
                    }
                }
                if (last->time < 0x7D00) {
                    last->time += last->duration;
                }
                goto rot_done;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->field1) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_801DF7A8(pool, (EffectEntry *)slot);
                    parts->effects[0] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    if ((kind & 0xF) < 3) {
                        slot->time = 0;
                        slot->u.track.cursor = slot->u.track.start;
                    } else {
                        slot->time = -1;
                        if ((kind & 0xF) == 5) {
                            last->u.values[3] = 0;
                            last->u.values[4] = 0;
                            last->u.values[5] = 0;
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
        if (parts->effects[1] != NULL) {
            slot = (Tween *)parts->effects[1];
            pos_kind = slot->kind;
            switch (pos_kind & 0xF) {
            case 0:
                track = slot->u.track.cursor;
                if (!(pos_kind & 0x10)) {
                    parts->translation[0] = *(s16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(pos_kind & 0x20)) {
                    parts->translation[1] = *(s16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(pos_kind & 0x40)) {
                    parts->translation[2] = *(s16 *)track;
                    slot->u.track.cursor += 2;
                }
                break;
            case 1:
                if (!(pos_kind & 0x10)) {
                    TRACK_STEP(parts->translation[0], slot->u.track.cursor);
                }
                if (!(pos_kind & 0x20)) {
                    TRACK_STEP(parts->translation[1], slot->u.track.cursor);
                }
                if (!(pos_kind & 0x40)) {
                    TRACK_STEP(parts->translation[2], slot->u.track.cursor);
                }
                break;
            case 2:
                track = slot->u.track.cursor;
                if (!(pos_kind & 0x10)) {
                    move.vx = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                } else {
                    move.vx = 0;
                }
                if (!(pos_kind & 0x20)) {
                    move.vy = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                } else {
                    move.vy = 0;
                }
                if (!(pos_kind & 0x40)) {
                    move.vz = *(u16 *)track;
                    slot->u.track.cursor += 2;
                } else {
                    move.vz = 0;
                }
                move.vx = move.vx * parts->scale[0] >> 12;
                move.vy = move.vy * parts->scale[1] >> 12;
                move.vz = move.vz * parts->scale[2] >> 12;
                ApplyMatrix(&parts->world, &move, &moved);
                parts->translation[0] += scale * moved.vx >> 12;
                parts->translation[1] += scale * moved.vy >> 12;
                parts->translation[2] += scale * moved.vz >> 12;
                break;
            case 3:
                time = slot->time + 1;
                parts->translation[0] = slot->u.values[0] + slot->u.values[3] * time / slot->duration;
                parts->translation[1] = slot->u.values[1] + slot->u.values[4] * time / slot->duration;
                last = slot;
                parts->translation[2] = slot->u.values[2] + slot->u.values[5] * time / slot->duration;
                break;
            case 4: {
                s16 sx, sy, sz;
                sx = (slot->u.values[3] - parts->translation[0]) / slot->duration;
                sy = (slot->u.values[4] - parts->translation[1]) / slot->duration;
                sz = (slot->u.values[5] - parts->translation[2]) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    parts->translation[0] = slot->u.values[3];
                    parts->translation[1] = slot->u.values[4];
                    parts->translation[2] = slot->u.values[5];
                } else {
                    parts->translation[0] += sx;
                    parts->translation[1] += sy;
                    parts->translation[2] += sz;
                    slot->time = 0;
                }
                break;
            }
            case 5:
                slot->u.values[0] += slot->u.values[3];
                parts->translation[0] += slot->u.values[0];
                slot->u.values[1] += slot->u.values[4];
                parts->translation[1] += slot->u.values[1];
                slot->u.values[2] += slot->u.values[5];
                last = slot;
                parts->translation[2] += slot->u.values[2];
                break;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->field1) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_801DF7A8(pool, (EffectEntry *)slot);
                    parts->effects[1] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    if ((pos_kind & 0xF) < 3) {
                        slot->time = 0;
                        slot->u.track.cursor = slot->u.track.start;
                    } else {
                        slot->time = -1;
                        if ((pos_kind & 0xF) == 5) {
                            last->u.values[3] = 0;
                            last->u.values[4] = 0;
                            last->u.values[5] = 0;
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
        if (parts->effects[2] != NULL) {
            slot = (Tween *)parts->effects[2];
            mode = slot->kind & 0xF;
            switch (mode) {
            case 3:
                time = slot->time + 1;
                parts->scale[0] = slot->u.values[0] + slot->u.values[3] * time / slot->duration;
                parts->scale[1] = slot->u.values[1] + slot->u.values[4] * time / slot->duration;
                last = slot;
                parts->scale[2] = slot->u.values[2] + slot->u.values[5] * time / slot->duration;
                break;
            case 4: {
                s16 sx, sy, sz;
                sx = (slot->u.values[3] - slot->u.values[0]) / slot->duration;
                sy = (slot->u.values[4] - slot->u.values[1]) / slot->duration;
                sz = (slot->u.values[5] - slot->u.values[2]) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    parts->scale[0] = slot->u.values[3];
                    parts->scale[1] = slot->u.values[4];
                    parts->scale[2] = slot->u.values[5];
                } else {
                    last->u.values[0] += sx;
                    last->u.values[1] += sy;
                    last->u.values[2] += sz;
                    parts->scale[0] = last->u.values[0];
                    parts->scale[1] = last->u.values[1];
                    parts->scale[2] = last->u.values[2];
                    slot->time = 0;
                }
                break;
            }
            case 5:
                slot->u.values[0] += slot->u.values[3];
                parts->scale[0] += slot->u.values[0];
                slot->u.values[1] += slot->u.values[4];
                parts->scale[1] += slot->u.values[1];
                slot->u.values[2] += slot->u.values[5];
                last = slot;
                parts->scale[2] += slot->u.values[2];
                break;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->field1) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_801DF7A8(pool, (EffectEntry *)slot);
                    parts->effects[2] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    slot->time = -1;
                    if (mode == 5) {
                        last->u.values[3] = 0;
                        last->u.values[4] = 0;
                        last->u.values[5] = 0;
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

/* Apply an animation frame to a hierarchy's parts: the listed rotations (unless
 * flag 1) and translations (unless flag 2), marking each changed part; parts
 * with a kept tween (tag 0xFF) keep theirs. The frame holds halfwords: [2]
 * flags, [3] base flag, [6] rotation count, [7] translation count, then from
 * [12] (x, y, z) triples, after the base rotations when [3] is 0. Returns the
 * part count less the root. */
u16 func_801DEF10(ModelPart *root, s16 *data) {
    u16 rotations;
    u16 translations;
    u16 rotationCount;
    u16 translationCount;
    u16 flags;
    ModelPart *part;
    u16 count;
    s32 i;
    s32 x;
    s32 y;
    s32 z;

    rotations = 0;
    translations = 0;
    i = data[3]; /* the base flag */
    rotationCount = data[6];
    translationCount = data[7];
    flags = data[2];
    data += 12;
    if (i == 0) {
        data += (rotationCount + 1) * 3;
    }
    count = root->index - 1;
    part = root;
    for (i = 0; i < count; i++) {
        part++;
        if (!(flags & 1) && rotations < rotationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            rotations++;
            if ((part->rotation.vx != x || part->rotation.vy != y || part->rotation.vz != z)
                && (part->effects[0] == NULL || part->effects[0]->tag != 0xFF)) {
                part->rotation.vx = x;
                part->rotation.vy = y;
                part->rotation.vz = z;
                part->dirty = 1;
                part->rotate = 1;
            }
        }
        if (!(flags & 2) && translations < translationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            translations++;
            if ((part->translation[0] != x || part->translation[1] != y || part->translation[2] != z)
                && (part->effects[1] == NULL || part->effects[1]->tag != 0xFF)) {
                part->translation[0] = x;
                part->translation[1] = y;
                part->translation[2] = z;
                part->dirty = 1;
            }
        }
    }
    return count;
}

/* Tween the parts after the root towards an animation frame over `duration`
 * ticks (at least 1): each changed rotation gets a tween (kind mode + 3) of
 * the shortest angle differences (mode 1: to the absolute angles), each
 * changed translation one of its movement (mode 1: to the absolute
 * translation); kept tweens (tag 0xFF) stay, other parts lose theirs. The
 * frame is read as 801DEF10 reads it. Returns the part count less the root. */
u16 func_801DF0B4(EffectPool *pool, ModelPart *part, s16 *data, s32 duration, s32 mode, s32 smooth,
                  s32 tag) {
    EffectEntry *entry;
    u16 rotationCount;
    u16 rotations;
    u16 translationCount;
    u16 translations;
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
    translations = 0;
    smooth &= 1;
    rotationCount = data[6];
    i = data[3]; /* the base flag */
    translationCount = data[7];
    mode &= 1;
    flags = data[2];
    data += 12;
    if (i == 0) {
        data += (rotationCount + 1) * 3;
    }
    count = part->index - 1;
    for (i = 0; i < count; i++) {
        part++;
        if (!(flags & 1) && rotations < rotationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            rotations++;
            if (part->rotation.vx != x || part->rotation.vy != y || part->rotation.vz != z) {
                if (part->effects[0] != NULL) {
                    entry = part->effects[0];
                    if (entry->tag == 0xFF) {
                        goto translation;
                    }
                } else {
                    entry = func_801DF6F0(pool);
                }
                if (entry != NULL) {
                    entry->used = 1;
                    entry->field1 = smooth;
                    entry->kind = mode + 3;
                    entry->tag = tag;
                    entry->params[0] = part->rotation.vx;
                    entry->params[1] = part->rotation.vy;
                    entry->params[2] = part->rotation.vz;
                    x = (x - part->rotation.vx) & 0xFFF;
                    if (x >= 0x800) {
                        x -= 0x1000;
                    }
                    entry->params[3] = x;
                    y = (y - part->rotation.vy) & 0xFFF;
                    if (y >= 0x800) {
                        y -= 0x1000;
                    }
                    entry->params[4] = y;
                    z = (z - part->rotation.vz) & 0xFFF;
                    if (z >= 0x800) {
                        z -= 0x1000;
                    }
                    entry->params[5] = z;
                    if (mode) {
                        entry->params[3] += part->rotation.vx;
                        entry->params[4] += part->rotation.vy;
                        entry->params[5] += part->rotation.vz;
                    }
                    entry->time = 0;
                    entry->duration = duration;
                    part->effects[0] = entry;
                    goto translation;
                }
            }
        }
        if (part->effects[0] != NULL && part->effects[0]->tag != 0xFF) {
            func_801DF7A8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
    translation:
        if (!(flags & 2) && translations < translationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            translations++;
            if (part->translation[0] != x || part->translation[1] != y || part->translation[2] != z) {
                if (part->effects[1] != NULL) {
                    entry = part->effects[1];
                    if (entry->tag == 0xFF) {
                        continue;
                    }
                } else {
                    entry = func_801DF6F0(pool);
                }
                if (entry != NULL) {
                    entry->used = 1;
                    entry->field1 = smooth;
                    entry->kind = mode + 3;
                    entry->tag = tag;
                    entry->params[0] = part->translation[0];
                    entry->params[1] = part->translation[1];
                    entry->params[2] = part->translation[2];
                    if (mode) {
                        entry->params[3] = x;
                        entry->params[4] = y;
                        entry->params[5] = z;
                    } else {
                        entry->params[3] = x - part->translation[0];
                        entry->params[4] = y - part->translation[1];
                        entry->params[5] = z - part->translation[2];
                    }
                    entry->time = 0;
                    entry->duration = duration;
                    part->effects[1] = entry;
                    continue;
                }
            }
        }
        if (part->effects[1] != NULL && part->effects[1]->tag != 0xFF) {
            func_801DF7A8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
    }
    return count;
}

/* Release the attachments of node `index` selected by `mask` (bit 0: w70,
 * bit 1: w74, bit 2: w78) back to `pool`. */
void func_801DF52C(EffectPool *pool, ModelPart *part, s32 index, s32 mask) {
    if (index < part->index) {
        part += index;
        if (part->effects[0] != NULL && (mask & 1)) {
            func_801DF7A8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && (mask & 2)) {
            func_801DF7A8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && (mask & 4)) {
            func_801DF7A8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Allocate `capacity` 0x14-byte slots for a pool, all free. */
EffectPool *func_801DF5F4(EffectPool *pool, s32 capacity) {
    if (capacity <= 0 ||
        (pool->count = capacity, func_80032498(4, 0),
         (pool->entries = func_80031BDC(capacity * sizeof(EffectEntry), 0)) == NULL)) {
        return NULL;
    }
    func_801DF6A8(pool);
    return pool;
}

/* Release a pool's slots. */
void func_801DF668(EffectPool *pool) {
    pool->next = 0;
    if (pool->entries != NULL) {
        func_800320E8(pool->entries);
    }
    pool->entries = NULL;
}

/* Mark every slot of a pool free. */
void func_801DF6A8(EffectPool *pool) {
    EffectEntry *slot;
    s32 i;

    if (pool->entries != NULL) {
        slot = pool->entries;
        pool->next = 0;
        for (i = 0; i < pool->count; i++) {
            slot->used = 0;
            slot++;
        }
    }
}

/* Take the first free slot (the caller marks it used) and advance the search
 * position past used slots; NULL when the pool is full. */
EffectEntry *func_801DF6F0(EffectPool *pool) {
    EffectEntry *slot;
    u32 capacity;

    if (pool->next < pool->count) {
        slot = &pool->entries[pool->next];
        if (slot->used) {
            return NULL;
        }
        pool->next++;
        capacity = pool->count;
        while (pool->next < capacity && pool->entries[pool->next].used != 0) {
            pool->next++;
        }
        return slot;
    }
    return NULL;
}

/* Free a slot, moving the search position back to it; returns its index or
 * -1 without a slot. */
s32 func_801DF7A8(EffectPool *pool, EffectEntry *slot) {
    s32 index;

    if (slot == NULL) {
        return -1;
    }
    index = ((u32)slot - (u32)pool->entries) / sizeof(EffectEntry);
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
s32 func_801DF7F4(EffectPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag) {
    AnimationFrame *key;
    u8 *kind;
    Tween *tween;
    u8 *values;
    u16 count;
    u16 rot_count;
    u16 pos_count;
    u16 duration;
    u16 flags;
    s32 i;

    key = (AnimationFrame *)data;
    if (key->packed != 0) {
        func_801DFE8C(pool, parts);
        func_801DEF10(parts, (s16 *)key);
        return 1;
    }
    rot_count = key->rotationCount;
    count = parts->index;
    pos_count = key->translationCount;
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
            if (parts->effects[0] != NULL) {
                tween = (Tween *)parts->effects[0];
                if (tween->tag == 0xFF) {
                    goto kept0; /* a kept tween stays */
                }
            } else {
                tween = (Tween *)func_801DF6F0(pool);
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->field1 = mode;
                tween->kind = kind[0];
                tween->tag = tag;
                tween->u.track.cursor = tween->u.track.start = values + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->effects[0] = (EffectEntry *)tween;
            }
        } else {
            if (parts->effects[0] != NULL && i != 0 && parts->effects[0]->tag != 0xFF) {
                func_801DF7A8(pool, parts->effects[0]);
                parts->effects[0] = NULL;
            }
        }
    kept0:
        data++;
        if (*data != 0xFFFF) {
            if (parts->effects[1] != NULL) {
                tween = (Tween *)parts->effects[1];
                if (tween->tag == 0xFF) {
                    goto kept1; /* a kept tween stays */
                }
            } else {
                tween = (Tween *)func_801DF6F0(pool);
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->field1 = mode;
                tween->kind = kind[1];
                tween->tag = tag;
                tween->u.track.cursor = tween->u.track.start = values + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->effects[1] = (EffectEntry *)tween;
            }
        } else {
            if (parts->effects[1] != NULL && i != 0 && parts->effects[1]->tag != 0xFF) {
                func_801DF7A8(pool, parts->effects[1]);
                parts->effects[1] = NULL;
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
s32 func_801DFAC4(EffectPool *pool, ModelPart *parts, u16 *data, s32 mode, s32 tag) {
    AnimationFrame *key;
    u8 *kind;
    Tween *tween;
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

    key = (AnimationFrame *)data;
    if (key->packed != 0) {
        func_801DFE8C(pool, parts);
        func_801DEF10(parts, (s16 *)key);
        return 1;
    }
    mode &= 1;
    rotations = 0;
    rot_count = key->rotationCount;
    count = parts->index;
    positions = 0;
    pos_count = key->translationCount;
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
            if (parts->effects[0] != NULL) {
                tween = (Tween *)parts->effects[0];
                if (tween->tag == 0xFF) {
                    goto skip0; /* a kept tween stays */
                }
            } else {
                tween = (Tween *)func_801DF6F0(pool);
            }
            if (!(flags & 1) && i != 0 && rotations < rot_count) {
                parts->rotation.vx = *values++;
                parts->rotation.vy = *values++;
                parts->rotation.vz = *values++;
                rotations++;
                parts->dirty = 1;
                parts->rotate = 1;
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->field1 = mode;
                tween->kind = kind[0];
                tween->tag = tag;
                tween->u.track.cursor = tween->u.track.start = tracks + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->effects[0] = (EffectEntry *)tween;
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
            if (parts->effects[1] != NULL) {
                tween = (Tween *)parts->effects[1];
                if (tween->tag == 0xFF) {
                    goto skip1; /* a kept tween stays */
                }
            } else {
                tween = (Tween *)func_801DF6F0(pool);
            }
            if (!(flags & 2) && i != 0 && positions < pos_count) {
                parts->translation[0] = *values++;
                parts->translation[1] = *values++;
                parts->translation[2] = *values++;
                positions++;
                parts->dirty = 1;
            }
            if (tween != NULL) {
                tween->used = 1;
                tween->field1 = mode;
                tween->kind = kind[1];
                tween->tag = tag;
                tween->u.track.cursor = tween->u.track.start = tracks + *data;
                tween->time = 0;
                tween->duration = duration;
                parts->effects[1] = (EffectEntry *)tween;
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
void func_801DFE8C(EffectPool *pool, ModelPart *parts) {
    u16 count;
    s32 i;

    count = parts->index;
    for (i = 0; i < count; i++, parts++) {
        if (parts->effects[0] != NULL && parts->effects[0]->tag != 0xFF) {
            func_801DF7A8(pool, parts->effects[0]);
            parts->effects[0] = NULL;
        }
        if (parts->effects[1] != NULL && parts->effects[1]->tag != 0xFF) {
            func_801DF7A8(pool, parts->effects[1]);
            parts->effects[1] = NULL;
        }
        if (parts->effects[2] != NULL && parts->effects[2]->tag != 0xFF) {
            func_801DF7A8(pool, parts->effects[2]);
            parts->effects[2] = NULL;
        }
    }
}

/* Release every node's attachments tagged `tag`. */
void func_801DFF78(EffectPool *pool, ModelPart *parts, u8 tag) {
    u16 count;
    s32 i;

    count = parts->index;
    for (i = 0; i < count; i++, parts++) {
        if (parts->effects[0] != NULL && parts->effects[0]->tag == tag) {
            func_801DF7A8(pool, parts->effects[0]);
            parts->effects[0] = NULL;
        }
        if (parts->effects[1] != NULL && parts->effects[1]->tag == tag) {
            func_801DF7A8(pool, parts->effects[1]);
            parts->effects[1] = NULL;
        }
        if (parts->effects[2] != NULL && parts->effects[2]->tag == tag) {
            func_801DF7A8(pool, parts->effects[2]);
            parts->effects[2] = NULL;
        }
    }
}

/* Allocate a particle pool of `capacity` effect sprites plus a spare one
 * returned when the pool is full, and initialise them. */
SpritePool *func_801E0064(SpritePool *pool, s32 capacity) {
    func_80032498(4, 0);
    pool->count = capacity;
    pool->next = 0;
    pool->records = func_80031BDC((capacity + 1) * sizeof(EffectSprite), 0);
    if (pool->records != NULL) {
        func_801E011C(pool);
        return pool;
    }
    return NULL;
}

/* Release a particle pool. */
void func_801E00DC(SpritePool *pool) {
    pool->count = 0;
    pool->next = 0;
    if (pool->records != NULL) {
        func_800320E8(pool->records);
    }
    pool->records = NULL;
}

/* Mark every particle free and set up both buffers' semi-transparent textured
 * quads (a 16x1 texel strip at v=0xbd of page (0x340, 0x100), clut (0, 0x1cd)). */
void func_801E011C(SpritePool *pool) {
    EffectSprite *particle;
    POLY_FT4 *poly;
    s32 i;
    s32 j;

    particle = pool->records;
    for (i = 0; i < pool->count + 1; i++) {
        particle->age = -1;
        particle->lifetime = 0;
        for (j = 0; j < 2; j++) {
            poly = &particle->packets[j];
            SetPolyFT4(poly);
            SetSemiTrans(poly, 1);
            particle->packets[j].clut = GetClut(0, 0x1CD);
            particle->packets[j].tpage = GetTPage(0, 1, 0x340, 0x100);
            particle->packets[j].u0 = 0;
            particle->packets[j].v0 = 0xBD;
            particle->packets[j].u1 = 0;
            particle->packets[j].v1 = 0xBD;
            particle->packets[j].u2 = 0xF;
            particle->packets[j].v2 = 0xBD;
            particle->packets[j].u3 = 0xF;
            particle->packets[j].v3 = 0xBD;
        }
        particle++;
    }
}

/* Take the next free particle, setting both quads' semi-transparency; when the
 * pool is full, the spare particle past its end. */
EffectSprite *func_801E0248(SpritePool *pool, s16 semi_trans) {
    EffectSprite *particle;

    if (pool->next < pool->count) {
        particle = &pool->records[pool->next];
        if (particle->age == -1) {
            pool->next++;
            while (pool->next < pool->count) {
                if (pool->records[pool->next].age == -1) {
                    break;
                }
                pool->next++;
            }
            SetSemiTrans(&particle->packets[0], semi_trans);
            SetSemiTrans(&particle->packets[1], semi_trans);
            return particle;
        }
    }
    return &pool->records[pool->count];
}

/* Free a particle, moving the search position back to it; returns its index. */
s32 func_801E0354(SpritePool *pool, EffectSprite *particle) {
    s32 index;

    index = ((u32)particle - (u32)pool->records) / sizeof(EffectSprite);
    if (pool->next >= index) {
        pool->next = index;
    }
    particle->age = -1;
    return index;
}

/* Draw the live particles into the ordering table (3D ones projected with
 * the GTE at their depth, 2D ones at the front), free the expired ones and
 * fade the rest by `steps` ticks. */
void func_801E0398(SpritePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer) {
    EffectSprite *particle;
    s32 otz;
    s32 i;

    SetRotMatrix(m);
    SetTransMatrix(m);
    particle = pool->records;
    for (i = 0; i < pool->count; i++, particle++) {
        if (particle->age == -1) {
            continue;
        }
        if (particle->age >= particle->lifetime) {
            func_801E0354(pool, particle);
            continue;
        }
        particle->packets[buffer].r0 = particle->color[0] >> 6;
        particle->packets[buffer].g0 = particle->color[1] >> 6;
        particle->packets[buffer].b0 = particle->color[2] >> 6;
        if (particle->projected == 0) {
            particle->packets[buffer].x0 = particle->x0;
            particle->packets[buffer].y0 = particle->y0;
            particle->packets[buffer].x1 = particle->x1;
            particle->packets[buffer].y1 = particle->y1;
            particle->packets[buffer].x2 = particle->x2;
            particle->packets[buffer].y2 = particle->y2;
            particle->packets[buffer].x3 = particle->x3;
            particle->packets[buffer].y3 = particle->y3;
            addPrim(ot, &particle->packets[buffer]);
        } else {
            gte_ldv3(&particle->x0, &particle->x1, &particle->x2);
            gte_rtpt();
            gte_stsxy3(&particle->packets[buffer].x0, &particle->packets[buffer].x1, &particle->packets[buffer].x2);
            gte_stszotz(&otz);
            otz >>= D_80050100;
            gte_ldv0(&particle->x3);
            gte_rtps();
            gte_stsxy(&particle->packets[buffer].x3);
            addPrim(ot + otz, &particle->packets[buffer]);
        }
        particle->age += steps;
        particle->color[0] -= particle->fade[0] * steps;
        particle->color[1] -= particle->fade[1] * steps;
        particle->color[2] -= particle->fade[2] * steps;
    }
}

/* Set up a colour fade from (r0, g0, b0) to (r1, g1, b1) over `duration`
 * ticks. */
s32 func_801E0698(ColorFade *fade, s32 pool, s16 id, u8 solid, s16 max, s16 duration, u8 r0, u8 g0,
                  u8 b0, u8 r1, u8 g1, u8 b1, u16 x0, u16 y0, u16 z0, u16 x1, u16 y1, u16 z1,
                  u16 semiTrans) {
    if (fade != NULL) {
        fade->semiTrans = semiTrans;
        fade->count = -1;
        fade->id = id;
        fade->solid = solid;
        fade->pool = (SpritePool *)pool;
        fade->ends[0].vx = x0;
        fade->ends[0].vy = y0;
        fade->ends[0].vz = z0;
        fade->ends[1].vx = x1;
        fade->ends[1].vy = y1;
        fade->ends[1].vz = z1;
        fade->time = 0;
        if (max < 7) {
            fade->max = max;
        } else {
            fade->max = 7;
        }
        fade->color[0] = r0 << 6;
        fade->color[1] = g0 << 6;
        fade->color[2] = b0 << 6;
        fade->duration = duration;
        fade->sprite = NULL;
        fade->step[0] = (fade->color[0] - (r1 << 6)) / duration;
        fade->step[1] = (fade->color[1] - (g1 << 6)) / duration;
        fade->step[2] = (fade->color[2] - (b1 << 6)) / duration;
        return 0;
    }
}

/* Mark a colour fade idle. */
void func_801E0844(ColorFade *fade, s32 unused) {
    fade->id = -1;
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
        anim->size = w * h;
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
            StoreImage(&rect, (u_long *)anim->pixels);
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
            StoreImage(&rect, (u_long *)anim->pixels2);
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
        anim->size = w * h;
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
 * once the animation ended. */
s16 func_801E1258(ImageAnim *anim, s32 ticks) {
    RECT src;
    RECT dst;
    ImageAnim *target;
    u16 *pixels;
    u16 *work;
    u16 result;
    s16 frame;
    s32 value;
    s32 x;
    s32 y;

    if (!anim->active) {
        return -1;
    }
    anim->time += anim->speed * (ticks + 1);
    value = result = anim->curve(anim->time, anim->divisor, anim->base);
    frame = value;
    if (frame < 0) {
        func_801E165C(anim);
        return frame;
    }
    value = anim->frame;
    if (frame != value) {
        anim->frame = result;
        switch (anim->mode) {
        case 0:
            func_80026F44(anim->size, frame, anim->work, anim->pixels);
            if (anim->target == NULL) {
                LoadImage(&anim->rect, (u_long *)anim->work);
            }
            break;
        case 1:
            func_80026FE8(anim->size, frame, anim->work, anim->pixels2, anim->pixels);
            if (anim->target == NULL) {
                LoadImage(&anim->rect, (u_long *)anim->work);
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
        return result;
    }
    return frame;
}

/* Stop an image animation: restore its original pixels to VRAM (resident
 * decoder modes) and release its blocks. */
void func_801E165C(ImageAnim *anim) {
    if (anim->active) {
        if (anim->pixels != NULL) {
            if (anim->mode < 4) {
                LoadImage(&anim->rect, (u_long *)anim->pixels);
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
            if (D_801E8648[i].object >= 0 && (actor = actors[D_801E8648[i].object]) != NULL) {
                CompMatrix(&actor->parts->transform, &actor->parts[D_801E8648[i].part + 1].world, m);
                SetRotMatrix(m);
                SetTransMatrix(m);
                gte_ldv0(&D_801E8648[i].offset);
                gte_rtv0tr();
                gte_stlvnl(&world);
                D_801E8648[i].x = world.vx;
                D_801E8648[i].y = world.vy;
                D_801E8648[i].z = world.vz;
            } else {
                D_801E8648[i].x = D_801E8648[i].offset.vx;
                D_801E8648[i].y = D_801E8648[i].offset.vy;
                D_801E8648[i].z = D_801E8648[i].offset.vz;
            }
        }
    }
}

/* Build a record's surface from `table`: a scaled centre per ring (offset by
 * ox/oy/oz), each strand's points (segment length and sag), and two textured
 * triangles per point pair between neighbouring rings, their texture
 * spanning u_span x v_span from (tx, ty) with the CLUT at (clut_x, clut_y);
 * then `count` zeroed entries. The texture page origin is tx/ty rounded down
 * to a multiple of 64/256 as a halfword; the u/v bases and steps are signed
 * halfwords, and the cells between two strands use the smaller of their
 * point counts. On an allocation failure the record is left empty. The same
 * code as the battle overlay's func_800A7064. */
void func_801E1A14(Surface *record, u16 *table, s32 angle_base, s32 scale, s16 ox, s16 oy, s16 oz,
                   s32 count, s16 tx, s16 ty, s16 u_span, s16 v_span, s16 clut_x, s16 clut_y, u8 b0,
                   u8 b1, u8 b2, u8 b3, u8 b4, u8 b5) {
    SVECTOR *centre;
    SurfacePoint **rings;
    SurfacePoint *points_base;
    SurfacePoint *point;
    SurfacePoly *polys;
    u16 *counts;
    u16 *radii;
    u8 *angles;
    SurfaceEntry *entry;
    s32 start;
    u16 tpage, clut;
    s16 page_x, page_y;
    s16 u_base;
    s16 v_base;
    s16 u_step;
    s16 v_step;
    s32 i, k, b;
    s32 n;
    u16 total;

    record->rings = *table++;
    record->polys = *table * 2;
    func_80032498(4, 0);
    table++;
    centre = func_80031BDC(record->rings * sizeof(SVECTOR), 0);
    if (centre == NULL) {
        record->centres = NULL;
        return;
    }
    record->centres = centre;
    for (i = 0; i < record->rings; i++) {
        centre->vx = (*table++ + ox) * scale / 4096;
        centre->vy = (*table++ + oy) * scale / 4096;
        centre->vz = (*table++ + oz) * scale / 4096;
        centre++;
    }
    total = table[record->rings];
    record->points = total + record->rings;
    rings = func_80031BDC(record->rings * sizeof(SurfacePoint *), 0);
    if (rings == NULL) {
        record->centres = NULL;
        func_800320E8(NULL);
        return;
    }
    record->strands = rings;
    counts = table;
    radii = table + record->rings + 1;
    angles = (u8 *)(radii + total);
    point = func_80031BDC((total + record->rings) * sizeof(SurfacePoint), 0);
    if (point == NULL) {
        record->centres = NULL;
        func_800320E8(NULL);
        func_800320E8(record->strands);
        return;
    }
    centre = record->centres;
    points_base = point;
    for (i = 0; i < record->rings; rings++, counts++, centre++, i++) {
        *rings = point;
        for (k = 0; k < *counts; k++) {
            point->length = *radii++ * scale / 4096;
            point->sag = *angles++ + angle_base;
            point->pos[0] = centre->vx;
            point->pos[1] = centre->vy;
            point->pos[2] = centre->vz;
            point++;
        }
        point->length = 0;
        point->sag = 0;
        point->pos[0] = centre->vx;
        point->pos[1] = centre->vy;
        point->pos[2] = centre->vz;
        point++;
    }
    counts = table;
    polys = func_80031BDC(record->polys * sizeof(SurfacePoly), 0);
    if (polys == NULL) {
        record->centres = NULL;
        func_800320E8(NULL);
        func_800320E8(record->strands);
        func_800320E8(points_base);
        return;
    }
    record->polyList = polys;
    page_x = tx / 64 * 64;
    page_y = ty / 256 * 256;
    tpage = GetTPage(0, 1, page_x, page_y);
    clut = GetClut(clut_x, clut_y);
    u_base = (tx - page_x) * 4;
    v_base = ty - page_y;
    start = 0;
    u_step = u_span / (record->rings - 1);
    for (i = 0; i < record->rings - 1; i++) {
        if (counts[0] < counts[1]) {
            n = counts[0];
        } else {
            n = counts[1];
        }
        v_step = v_span / n;
        for (k = 0; k < n; k++) {
            polys->index[0] = start + k;
            polys->index[1] = start + k + counts[0] + 1;
            polys->index[2] = start + k + 1;
            for (b = 0; b < 2; b++) {
                SetPolyGT3(&polys->prim[b]);
                polys->prim[b].tpage = tpage;
                polys->prim[b].clut = clut;
                polys->prim[b].u0 = u_base + u_step * i;
                polys->prim[b].v0 = v_base + v_step * k;
                polys->prim[b].u1 = u_base + u_step * (i + 1);
                polys->prim[b].v1 = v_base + v_step * k;
                polys->prim[b].u2 = u_base + u_step * i;
                polys->prim[b].v2 = v_base + v_step * (k + 1);
            }
            polys++;
            polys->index[0] = start + k + counts[0] + 1;
            polys->index[1] = start + k + counts[0] + 2;
            polys->index[2] = start + k + 1;
            for (b = 0; b < 2; b++) {
                SetPolyGT3(&polys->prim[b]);
                polys->prim[b].tpage = tpage;
                polys->prim[b].clut = clut;
                polys->prim[b].u0 = u_base + u_step * (i + 1);
                polys->prim[b].v0 = v_base + v_step * k;
                polys->prim[b].u1 = u_base + u_step * (i + 1);
                polys->prim[b].v1 = v_base + v_step * (k + 1);
                polys->prim[b].u2 = u_base + u_step * i;
                polys->prim[b].v2 = v_base + v_step * (k + 1);
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
    record->entryCount = count;
    if ((s16)count > 0) {
        entry = func_80031BDC((s16)count * sizeof(SurfaceEntry), 0);
        if (entry == NULL) {
            record->entryCount = 0;
        }
        record->entries = entry;
        for (i = 0; i < record->entryCount; i++) {
            entry->h0 = 0;
            entry->h2 = 0;
            entry->h4 = 0;
            entry->h6 = 0;
            entry->h8 = 0;
            entry->hA = 0;
            entry->hC = 0;
            entry->hE = 0;
            entry++;
        }
    } else {
        record->entries = NULL;
    }
}

/* Simulate and draw a surface (hair or cloth): each strand's
 * segments hang from their start pulled by `wind` (plus each point's sag),
 * keep their length, stay above `floor` and are pushed out of the record's
 * collision spheres; then the points' normals are averaged from their
 * triangles, and the visible triangles are lit (front and back colours) and
 * queued. As in the original, a triangle the GTE flags as off screen does
 * not advance the triangle pointer, and one variable is both the collision
 * loop's counter and the GTE flag store. */
void func_801E22F8(Surface *record, SVECTOR *wind, MATRIX *m, u32 *ot, s32 buffer, s32 scale,
                   s16 floor) {
    VECTOR d;
    SVECTOR normal;
    VECTOR e1, e2, n;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    u8 rgb[4];
    s32 k; /* the collision loop's counter, then the GTE flag */
    s32 opz, otz;
    SurfacePoint *p;
    SurfacePoly *poly;
    SurfaceEntry *entry;
    s32 len, radius;
    s32 i;

    if (record->centres == NULL) {
        return;
    }
    rgb[3] = record->polyList->prim[0].code;
    for (i = 0; i < record->rings; i++) {
        p = record->strands[i];
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
            entry = record->entries;
            for (k = 0; k < record->entryCount; k++, entry++) {
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
    p = *record->strands;
    for (i = 0; i < record->points; i++) {
        p->normalCount = 0;
        p->normal[0] = 0;
        p->normal[1] = 0;
        p->normal[2] = 0;
        p++;
    }
    poly = record->polyList;
    p = *record->strands;
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
        p[poly->index[0]].normalCount++;
        p[poly->index[1]].normal[0] += e1.vx;
        p[poly->index[1]].normal[1] += e1.vy;
        p[poly->index[1]].normal[2] += e1.vz;
        p[poly->index[1]].normalCount++;
        p[poly->index[2]].normal[0] += e1.vx;
        p[poly->index[2]].normal[1] += e1.vy;
        p[poly->index[2]].normal[2] += e1.vz;
        p[poly->index[2]].normalCount++;
    }
    p = *record->strands;
    for (i = 0; i < record->points; i++) {
        p->normal[0] /= (s16)p->normalCount;
        p->normal[1] /= (s16)p->normalCount;
        p->normal[2] /= (s16)p->normalCount;
        p++;
    }
    SetRotMatrix(m);
    SetTransMatrix(m);
    poly = record->polyList;
    p = *record->strands;
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
void func_801E3438(Surface *record) {
    if (record->centres != NULL) {
        func_800320E8(record->centres);
        func_800320E8(*record->strands);
        func_800320E8(record->strands);
        func_800320E8(record->polyList);
        if (record->entries != NULL) {
            func_800320E8(record->entries);
        }
        record->centres = NULL;
    }
}

/* The frame curve of mode: 801E08D4, 801E0938 or 801E0988 for 1-3, any other
 * the cosine curve 801E0850. */
FrameCurve func_801E34BC(s32 mode) {
    switch (mode) {
    case 1:
        return (FrameCurve)func_801E08D4;
    case 2:
        return (FrameCurve)func_801E0938;
    case 3:
        return (FrameCurve)func_801E0988;
    default:
        return (FrameCurve)func_801E0850;
    }
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
void func_801E3534(Actor *actor, EffectPool *pool, s32 *entries, s32 *locals) {
    s16 none;

    none = -1;
    RESET_SCRIPT(actor, entries, locals, none);
}

/* Call script entry `entry` of `source` in `actor`: queued while the actor is
 * already in a call, else run at once from the entry. */
void func_801E35D0(Actor *actor, Actor *source, EffectPool *pool, s32 entry) {
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
s32 func_801E36BC(Actor *actor, EffectPool *pool, s32 ticks, s32 arg3, s32 arg4) {
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
                MulMatrix2(m, &actor->parts->transform);
                MulMatrix2(m, &actor->parts->world);
            }
            if (actor->parent_node != 0) {
                CompMatrix(&D_801E8670[actor->parent]->parts->transform,
                           &D_801E8670[actor->parent]->parts[actor->parent_node].world, m);
            } else {
                m = &D_801E8670[actor->parent]->parts->transform;
            }
            SetRotMatrix(m);
            SetTransMatrix(m);
            offset.vx = actor->offset[0];
            offset.vy = actor->offset[1];
            offset.vz = actor->offset[2];
            gte_ldv0(&offset);
            gte_rtv0tr();
            gte_stlvnl(actor->parts->transform.t);
            gte_stlvnl(actor->parts->translation);
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
 * The script word is an s16 (its bytes taken as (u8)word and
 * (u8)(word >> 8)): the arithmetic shift narrowed to a byte compiles to
 * srl, but combine cannot prove the byte's high bits clear, so the original
 * zero-extends decoded bytes (andi 0xff) at later byte consumers.
 * Block-scope aggregates take the stack slots of the original: the landing
 * probe's block frees 16 bytes that 0x25's `d` reuses before n/word (whose
 * slots are made when their address is first taken) and 0x25's matrix and
 * vectors; spilled scalars follow in declaration order.
 * Case 0x4A keeps its offsets and distance in block-scope variables: they
 * live in one basic block, so local allocation gives them s0-s2, which
 * moves pc and actor to the original's s3/s4. `reference` and `entry` also
 * carry 0x3C's sound and volume (so `reference` crosses a call and takes
 * s0), and 0x13/0x14 reuse `op` and `depth` for their extra bytes and the
 * resolved actor index (their registers and the zero-extensions at use).
 * 0x26 keeps its 0xff marker in a block-scope variable: as a constant it
 * would join the other 0xff loads, which loop.c then hoists out of the
 * interpreter loop (the original loads 0xff at each use).
 * The 16-bit roll argument recovers the original 0x168-byte frame.
 * The shared offset scalars follow 0x42/0x43's Z, Y, X component order.
 * In 0x4F, dist holds the vertical difference for the norm; the horizontal
 * offsets survive the norm call for the heading. Case 0x6E consumes its
 * word even when the resolved actor is absent, and keeps the observed
 * actor pointer local to that wait. */
void func_801E39F0(Actor *actor, EffectPool *pool, s32 changed, s32 ticks, s32 arg4) {
    Actor *self;
    Actor *other;
    ModelPart *part;
    ModelPart *node;
    VECTOR moved;
    SVECTOR step;
    RECT rect;
    s32 n;
    u16 *pc;
    u16 *start;
    u16 *counter;
    /* Reused by word decoding and actor-mask resolution. Save decoded
     * bytes before a resolver overwrites the halfword. */
    s16 word;
    u16 operand;
    u8 reference, entry;
    u8 op, arg;
    s32 running, redraw;
    Actor *copy;
    s32 found;
    s32 key;
    s32 offset0, offset1, offset2, dist;
    s32 pitch;
    s32 yaw;
    s16 roll;
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
        actor->parts->rotation.vx += actor->spin[0] >> 3;
        actor->parts->rotation.vy += actor->spin[1] >> 3;
        actor->parts->rotation.vz += actor->spin[2] >> 3;
        step.vx = actor->drift[0] * actor->parts->scale[0] >> 12;
        step.vy = actor->drift[1] * actor->parts->scale[1] >> 12;
        step.vz = actor->drift[2] * actor->parts->scale[2] >> 12;
        ApplyMatrix(&actor->parts->world, &step, &moved);
        actor->parts->translation[0] += actor->scale * moved.vx >> 12;
        actor->parts->translation[1] += actor->scale * moved.vy >> 12;
        actor->parts->translation[2] += actor->scale * moved.vz >> 12;
    }
    running = 1;
    pc = (u16 *)actor->pc;
    if (actor->w4C != 0 && func_801E6338(actor) <= actor->h48) {
        pc = (u16 *)actor->w4C;
        actor->w4C = 0;
    } else {
        if (actor->w54 != 0) {
            SVECTOR unused; /* unused in the original; reserves 8 bytes */
            SVECTOR probe;

            probe.vx = actor->parts->translation[0];
            probe.vy = 0;
            probe.vz = actor->parts->translation[2];
            probe.vy = actor->groundY;
            if (probe.vy < actor->parts->translation[1]) {
                actor->parts->translation[1] = probe.vy;
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
        word = *pc++;
        arg = word >> 8;
        op = (u8)word;
        switch (op) {
        case 0x00: /* end: stay on this command */
            pc = start;
            running = 0;
            break;
        case 0x01: /* wait (word) frames */
            if (changed != -1) {
                word = *pc++;
                actor->h40 += ticks;
                if ((s16)actor->h40 < (s16)word) {
                    pc = start;
                    running = 0;
                    break;
                }
                actor->h40 = 0;
                ticks = 0;
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x02: /* call 800796f4 after the run (`redraw`) */
            redraw = 1;
            break;
        case 0x03: /* the same */
            redraw = 1;
            break;
        case 0x08: /* drop the node tweens and the animation */
            func_801DFE8C(pool, actor->parts);
            func_801E632C(actor);
            break;
        case 0x0A: /* release node `arg`'s attachments 0-2 */
            func_801DF52C(pool, actor->parts, arg, 7);
            break;
        case 0x0B: { /* drop the tweens and reset every node below the root */
            s32 j, count;

            part = actor->parts;
            func_801DFE8C(pool, part);
            count = part->index - 1;
            for (j = 0; j < count; j++) {
                part++;
                part->rotation.vx = 0;
                part->rotation.vy = 0;
                part->rotation.vz = 0;
                part->translation[0] = 0;
                part->translation[1] = 0;
                part->translation[2] = 0;
                part->dirty = 1;
                part->rotate = 1;
            }
            break;
        }
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
        case 0x0D: /* release node `arg`'s rotation attachment */
            func_801DF52C(pool, actor->parts, arg, 1);
            break;
        case 0x0E: /* release node `arg`'s position attachment */
            func_801DF52C(pool, actor->parts, arg, 2);
            break;
        case 0x10: /* apply keyframe `arg` */
            func_801DEF10(actor->parts, (s16 *)func_801E6910(actor, arg, &n));
            break;
        case 0x11: { /* start animation `arg` (word: loop high, tag low byte) */
            s32 size;

            key = func_801E6910(actor, arg, &n);
            operand = *pc++;
            word = operand;
            if (n == 0) {
                entry = operand >> 8;
                func_801DF7F4(pool, actor->parts, (u16 *)key, entry, (u8)word);
                changed = -1;
                size = (s16)((u16 *)key)[8] * (actor->scale * actor->parts->scale[2] >> 12) >> 12;
                actor->h8E = size < 0 ? -size : size;
                func_801E5C74(actor, (Animation *)key, entry);
            }
            break;
        }
        case 0x13: /* tween to keyframe (word 0: keyframe low, tag high; word 1:
                    * smoothing low, duration high; mode `arg`) */
            word = *pc++;
            entry = word >> 8;
            reference = (u8)word;
            word = *pc++;
            /* op and depth double as the second word's bytes */
            op = word >> 8;
            depth = (u8)word;
            key = func_801E6910(actor, reference, &n);
            if (D_801E85CC != 0) {
                func_801DEF10(actor->parts, (s16 *)key);
            } else {
                func_801DF0B4(pool, actor->parts, (s16 *)key, op, arg, depth,
                              entry);
            }
            changed = -1;
            func_801E632C(actor);
            break;
        case 0x14: /* call entry (word high byte) of a source (low byte; 0xff each
                    * actor's own) in the actors of code `arg`; 0xfd hands over */
            word = *pc++;
            reference = (u8)word;
            entry = word >> 8;
            op = func_801E6830(actor, reference, &word);
            func_801E6830(actor, arg, &word);
            depth = actor->depth;
            if (arg == 0xFD) {
                actor->depth = 0;
            }
            for (n = 0; n < 8; n++) {
                if (((s16)word >> n) & 1) {
                    if (reference == 0xFF) {
                        func_801E35D0(D_801E8670[n], D_801E8670[n], pool, entry);
                    } else {
                        func_801E35D0(D_801E8670[n], D_801E8670[op], pool, entry);
                    }
                }
            }
            actor->depth = depth;
            if (arg == 0xFD) {
                return;
            }
            break;
        case 0x15: { /* clone this actor into a free slot 8 or 9; node (word) moves
                      * to the copy, which runs entry `arg` (0xff none) */
            ModelPart *parts;

            word = *pc++;
            for (n = 8; n < 10; n++) {
                if (D_801E8670[n] == NULL) {
                    copy = func_80031BDC(sizeof(Actor), 1);
                    break;
                }
            }
            *copy = *actor;
            D_801E8670[n] = copy;
            copy->anim_state = -1;
            copy->h3C = 0xFFFF;
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
            copy->index = n;
            copy->surfaceCount = 0;
            copy->imageCount = 0;
            func_801E8510(copy);
            parts = func_80031BDC(actor->parts->index * sizeof(ModelPart), 1);
            copy->parts = parts;
            for (n = 0; n < actor->parts->index; n++) {
                parts[n] = actor->parts[n];
                if (actor->parts[n].parent != NULL) {
                    parts[n].parent =
                        parts + ((u32)actor->parts[n].parent - (u32)actor->parts) / sizeof(ModelPart);
                }
                parts[n].visible = 0;
                parts[n].effects[0] = NULL;
                parts[n].effects[1] = NULL;
            }
            func_801E6578(pool, word, actor->parts, parts);
            if (arg != 0xFF) {
                func_801E35D0(copy, copy, pool, arg);
            }
            break;
        }
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
        case 0x18: /* start animation `arg`, looping when the word is set */
            key = func_801E6910(actor, arg, &n);
            func_801E5C74(actor, (Animation *)key, (s16)*pc++);
            break;
        case 0x19: /* stop the animation */
            func_801E632C(actor);
            break;
        case 0x1A: { /* move a VRAM rectangle (words x, y, dst x, dst y, w, h;
                      * arg bit 0: by the actor's image offset) */
            s16 x, y;

            rect.x = *pc++;
            rect.y = *pc++;
            x = *pc++;
            y = *pc++;
            rect.w = *pc++;
            rect.w = (rect.w + 1) / 2 * 2;
            rect.h = *pc++;
            if (arg & 1) {
                if (actor->h90 < 0) {
                    break;
                }
                rect.x += actor->h90;
                rect.y += actor->h92;
                x += actor->h90;
                y += actor->h92;
            }
            MoveImage(&rect, x, y);
            break;
        }
        case 0x1D: /* tween node `arg` between two poses (words: flags/mode,
                    * tag/field, start x y z, end x y z, duration) */
            word = *pc++;
            entry = word >> 8;
            reference = (u8)word;
            word = *pc++;
            func_801E6974(actor, pool, &actor->parts[arg], reference, entry, (u8)word, word >> 8,
                          *pc++, *pc++, *pc++, *pc++, *pc++, *pc++, *pc++);
            changed = -1;
            break;
        case 0x1E: /* use the scaled hierarchy update */
            actor->scaled = arg;
            break;
        case 0x1F: /* continue in another actor */
            other = D_801E8670[func_801E6830(self, arg, &word) & 0xFF];
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
        case 0x22: /* wait for (word) loops (of all, or those tagged `arg`) */
            if (changed != -1) {
                word = *pc++;
                if (arg == 0xFF) {
                    if (!(changed & 0x400)) {
                        pc = start;
                        running = 0;
                        break;
                    }
                    actor->h42++;
                    if (actor->h42 < (s16)word) {
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
                    if (actor->h42 < (s16)word) {
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
        case 0x23: /* show or hide node (word) by flags `arg` (801e6d94) */
            func_801E6D94(actor, &actor->parts[(s16)*pc++], arg);
            break;
        case 0x24: /* show or hide */
            if (actor != NULL) {
                actor->active = arg & 1;
            }
            break;
        case 0x25: { /* attach the masked actors to node (high byte), keeping
                      * their place (arg bit 0) or at an offset */
            VECTOR d;
            MATRIX m;
            VECTOR ex, ey, ez;
            SVECTOR v;
            s16 c0, c1, c2;

            word = *pc++;
            entry = word >> 8;
            func_801E6830(actor, (u8)word, &word);
            c0 = *pc++;
            c1 = *pc++;
            c2 = *pc++;
            for (n = 0; n < 8; n++) {
                if (!(((s16)word >> n) & 1) || D_801E8670[n] == NULL) {
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
                    d.vx = D_801E8670[n]->parts->translation[0] - node->world.t[0];
                    d.vy = D_801E8670[n]->parts->translation[1] - node->world.t[1];
                    d.vz = D_801E8670[n]->parts->translation[2] - node->world.t[2];
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
        }
        case 0x26: { /* detach the masked actors */
            u8 none;

            func_801E6830(actor, arg, &word);
            for (n = 0, none = 0xFF; n < 8; n++) {
                if ((((s16)word >> n) & 1) && D_801E8670[n] != NULL) {
                    D_801E8670[n]->parent = none;
                }
            }
            break;
        }
        case 0x27: /* recompose the hierarchy */
            if (actor->scaled) {
                func_801DC848(actor->parts, actor->scale);
            } else {
                func_801DC5C0(actor->parts, actor->scale);
            }
            break;
        case 0x28:
        case 0x29: { /* wait until the distance (in h8e units) passes `arg` while
                    * node (low byte) tweens with the given time */
            ModelPart *tween;

            dist = func_801E6338(actor);
            if (actor->h8E == 0) {
                actor->h8E = 1;
            }
            word = *pc++;
            tween = &actor->parts[(u8)word];
            entry = word >> 8;
            dist /= actor->h8E;
            found = 0;
            if (tween->effects[0] != NULL) {
                found = entry == (s16)tween->effects[0]->time;
            } else if (tween->effects[1] != NULL && (s16)tween->effects[1]->time == entry) {
                found = 1;
            }
            if (found) {
                if (op == 0x28) {
                    if (dist >= arg) {
                        pc = start;
                        running = 0;
                    }
                } else if (dist < arg) {
                    pc = start;
                    running = 0;
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        }
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
        case 0x2E: /* jump (word: offset; `arg` 0 cancels) when near the target */
            actor->h48 = actor->h8E;
            word = *pc++;
            actor->w4C = arg ? (s32)((u8 *)start + (s16)word) : 0;
            break;
        case 0x30: /* reset a loop counter */
            *pc++ = 0;
            break;
        case 0x31: { /* loop back to a counter until it reaches its limit */
            s32 limit;

            word = *pc++;
            counter = (u16 *)((u8 *)start + (s16)word);
            limit = (u16)(word = *counter++) >> 8;
            *counter = word = *counter + 1;
            counter++;
            if ((s16)word < limit) {
                pc = counter;
            }
            break;
        }
        case 0x32: /* jump */
            word = *pc;
            pc = (u16 *)((u8 *)start + (s16)word);
            break;
        case 0x35: /* jump at random (half the time) */
            word = *pc++;
            if (rand() >= 0x4000) {
                pc = (u16 *)((u8 *)start + (s16)word);
            }
            break;
        case 0x36: /* jump after a number of frames (words: frames, offset) */
            actor->h44 = 0;
            actor->h46 = *pc++;
            word = *pc++;
            actor->w50 = arg ? (s32)((u8 *)start + (s16)word) : 0;
            break;
        case 0x37: /* jump on landing */
            word = *pc++;
            actor->w54 = arg ? (s32)((u8 *)start + (s16)word) : 0;
            break;
        case 0x38: /* turn the root toward the target each frame within a limit from
                    * `arg` (801e5b50 kind 7; gain the word's low byte, rate its high) */
            word = *pc++;
            func_801E5B50(pool, actor->parts, 0, arg, (u8)word, (u8)(word >> 8),
                          actor->target[0], actor->target[1], actor->target[2]);
            break;
        case 0x39: /* the same, heading only (kind 8) */
            word = *pc++;
            func_801E5B50(pool, actor->parts, 1, arg, (u8)word, (u8)(word >> 8),
                          actor->target[0], actor->target[1], actor->target[2]);
            break;
        case 0x33: /* the battle's conditional jumps: consume the word */
        case 0x34:
        case 0x3B:
            word = *pc++;
            break;
        case 0x3C: /* fade sound (word low byte) of bank source `arg` (801e5cd8) to
                    * silence over (high byte) frames (8003a3b8) */
            word = *pc++;
            reference = word;
            entry = word >> 8;
            func_8003A3B8(reference + func_801E5CD8(actor, arg), 0, entry);
            break;
        case 0x3D: /* run the queued calls once `arg` is among them */
            word = actor->depth;
            if ((s16)word < 2) {
                break;
            }
            for (n = 1; n < (s16)word; n++) {
                if (actor->queue_entry[n - 1] == arg) {
                    goto dequeue;
                }
            }
            break;
        case 0x40: { /* turn the root to a rotation */
            s16 rx, ry, rz;

            rx = *pc++;
            ry = *pc++;
            rz = *pc++;
            func_801E59D4(pool, actor->parts, arg, rx, ry, rz);
            changed = -1;
            break;
        }
        case 0x41: { /* turn the root by a rotation */
            s32 rx, ry, rz;
            ModelPart *root;

            rx = *pc++;
            ry = *pc++;
            rz = *pc++;
            root = actor->parts;
            rx = (s16)(root->rotation.vx + rx);
            ry = (s16)(root->rotation.vy + ry);
            rz = (s16)(root->rotation.vz + rz);
            func_801E59D4(pool, actor->parts, arg, (s16)rx, (s16)ry, (s16)rz);
            changed = -1;
            break;
        }
        case 0x42:
        case 0x43: /* turn the root toward the target (0x43: heading only) */
            offset1 = actor->target[1] - actor->parts->translation[1];
            offset2 = actor->target[0] - actor->parts->translation[0];
            offset0 = actor->target[2] - actor->parts->translation[2];
            if (op == 0x43) {
                pitch = 0;
                offset1 = 0;
            } else {
                pitch = ratan2(offset1, SquareRoot0(offset2 * offset2 + offset0 * offset0));
            }
            yaw = ratan2(-offset2, -offset0);
            roll = 0;
            if (offset2 == 0 && offset1 == 0 && offset0 == 0) {
                break;
            }
            func_801E59D4(pool, actor->parts, arg, (s16)pitch, (s16)yaw, (s16)roll);
            changed = -1;
            break;
        case 0x44: /* spin = three words */
            actor->spin[0] = *pc++;
            actor->spin[1] = *pc++;
            actor->spin[2] = *pc++;
            break;
        case 0x45: /* spin += three words */
            actor->spin[0] += *pc++;
            actor->spin[1] += *pc++;
            actor->spin[2] += *pc++;
            break;
        case 0x46: /* spin acceleration = three words */
            actor->spin_accel[0] = *pc++;
            actor->spin_accel[1] = *pc++;
            actor->spin_accel[2] = *pc++;
            break;
        case 0x47: /* spin acceleration += three words */
            actor->spin_accel[0] += *pc++;
            actor->spin_accel[1] += *pc++;
            actor->spin_accel[2] += *pc++;
            break;
        case 0x48: /* b36 = arg */
            actor->b36 = arg;
            break;
        case 0x49: /* place the root */
            actor->parts->translation[0] = (s16)*pc++;
            actor->parts->translation[1] = (s16)*pc++;
            actor->parts->translation[2] = (s16)*pc++;
            break;
        case 0x4A: /* (arg 0xfb) put the root at distance h8e from the target */
            if (arg != 0xFB) {
                break;
            }
            {
                s32 dx, dy, dz, dist;

                dx = actor->parts->translation[0] - actor->target[0];
                dy = actor->parts->translation[1] - actor->target[1];
                dz = actor->parts->translation[2] - actor->target[2];
                dist = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
                actor->parts->translation[0] = actor->target[0] + dx * actor->h8E / dist;
                actor->parts->translation[1] = actor->target[1] + dy * actor->h8E / dist;
                actor->parts->translation[2] = actor->target[2] + dz * actor->h8E / dist;
            }
            break;
        case 0x4B: /* drift = three words */
            actor->drift[0] = *pc++;
            actor->drift[1] = *pc++;
            actor->drift[2] = *pc++;
            break;
        case 0x4C: /* drift += three words */
            actor->drift[0] += *pc++;
            actor->drift[1] += *pc++;
            actor->drift[2] += *pc++;
            break;
        case 0x4D: /* drift acceleration = three words */
            actor->drift_accel[0] = *pc++;
            actor->drift_accel[1] = *pc++;
            actor->drift_accel[2] = *pc++;
            break;
        case 0x4E: /* drift acceleration += three words */
            actor->drift_accel[0] += *pc++;
            actor->drift_accel[1] += *pc++;
            actor->drift_accel[2] += *pc++;
            break;
        case 0x4F: { /* drift toward the target over `arg` frames */
            s32 dist_drift;
            offset0 = actor->target[0] - actor->parts->translation[0];
            dist = actor->target[1] - actor->parts->translation[1];
            offset2 = actor->target[2] - actor->parts->translation[2];
            if (arg == 0) {
                arg = 1;
            }
            dist_drift = SquareRoot0(offset0 * offset0 + dist * dist + offset2 * offset2) / arg;
            actor->drift[2] = (dist_drift << 12) / (actor->scale * actor->parts->scale[2] >> 12) / 2;
            if (((ratan2(-offset0, -offset2) - (u16)actor->parts->rotation.vy + 0x400) & 0xFFF) < 0x800) {
                actor->drift[2] = -actor->drift[2];
            }
            break;
        }
        case 0x50: /* set the target */
            actor->aim_actor = 0;
            actor->target[0] = *pc++;
            actor->target[1] = *pc++;
            actor->target[2] = *pc++;
            break;
        case 0x54: /* h8e = word scaled by the actor and root scales */
            actor->h8E = (s16)*pc++ * (actor->scale * actor->parts->scale[2] >> 12) >> 12;
            break;
        case 0x55: /* h8e += word scaled */
            actor->h8E += (s16)*pc++ * (actor->scale * actor->parts->scale[2] >> 12) >> 12;
            break;
        case 0x56: /* h8e += word */
            actor->h8E += *pc++;
            break;
        case 0x57: /* h8e += the width of the actor of code `arg` */
            n = func_801E6830(actor, arg, &word) & 0xFF;
            actor->h8E += func_801E8480(n);
            break;
        case 0x5B: /* set the call depth; 2 runs the queued calls */
            word = actor->depth;
            if (arg == 1 && (s16)word >= 2) {
                break;
            }
            actor->depth = arg;
            if (arg != 2) {
                break;
            }
        dequeue:
            actor->depth = 0;
            if ((s16)word < 2) {
                break;
            }
            for (n = 1; n < (s16)word; n++) {
                func_801E35D0(actor, D_801E8670[actor->queue_source[n - 1]], pool,
                              actor->queue_entry[n - 1]);
            }
            return;
        case 0x5C: /* jump when at the target */
            word = *pc++;
            if (actor->target[0] == actor->parts->translation[0] && actor->target[1] == actor->parts->translation[1] &&
                actor->target[2] == actor->parts->translation[2]) {
                pc = (u16 *)((u8 *)start + (s16)word);
            }
            break;
        case 0x5D: /* node (word)'s billboard mode = `arg` */
            word = *pc++;
            actor->parts[(s16)word].field52 = arg;
            break;
        case 0x5E: /* scale = word */
            word = *pc++;
            actor->scale = word;
            break;
        case 0x5F: /* flags = word */
            word = *pc++;
            actor->flags = word;
            break;
        case 0x62: /* set or add node (word 0)'s transform to words 1-3 (801e7094,
                    * flags `arg`) */
            /* The operands are read in order. */
            func_801E7094(actor, &actor->parts[(s16)*pc++], arg, (s16)*pc++, (s16)*pc++, (s16)*pc++);
            break;
        case 0x63: /* start an event animation and jump */
            word = *pc++;
            actor->anim_state = 0;
            actor->anim_loop = -1;
            actor->anim_frame = 0;
            actor->anim_frames = arg;
            actor->anim_pos = (u8 *)pc;
            pc = (u16 *)((u8 *)start + (s16)word);
            break;
        case 0x64: /* h3e = word */
            actor->h3E = *pc++;
            break;
        case 0x6B: /* node (word) uses RotMatrixYXZ = `arg` */
            word = *pc++;
            actor->parts[(s16)word].yxz = arg;
            break;
        case 0x6C: /* wait while the resident is busy */
            if (func_800286CC() != 0) {
                pc = start;
                running = 0;
            }
            break;
        case 0x6D: /* b38 = `arg` bit 0 */
            actor->b38 = arg & 1;
            break;
        case 0x6E: { /* wait while an actor's b38 equals the word's bit 0 */
            Actor *observed;

            entry = func_801E6830(actor, arg, &word);
            word = *pc++;
            observed = D_801E8670[entry & 0xFF];
            if (observed != NULL && observed->b38 == (word & 1)) {
                pc = start;
                running = 0;
            }
            break;
        }
        case 0x6F: /* h3a = the current mask (801e863c), or -1 for `arg` 0 */
            actor->h3A = arg ? D_801E863C : -1;
            break;
        case 0x70: /* jump and stop when h3a is the current value */
            word = *pc++;
            if (actor->h3A == D_801E863C) {
                pc = (u16 *)((u8 *)start + (s16)word);
                running = 0;
            }
            break;
        case 0x04: /* the battle-only commands: no effect, no words */
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

/* Turn a node to (rx, ry, rz): at once when `duration` is below 2, else
 * through its first attachment (a kind-3 tween of the shortest angle
 * differences over `duration` ticks), taking a pool slot if it has none. */
void func_801E59D4(EffectPool *pool, ModelPart *part, s32 duration, s32 rx, s32 ry, s32 rz) {
    EffectEntry *tween;

    if (duration < 2) {
        part->rotation.vx = rx;
        part->rotation.vy = ry;
        part->rotation.vz = rz;
        part->rotate = 1;
        return;
    }
    if (part->rotation.vx != rx || part->rotation.vy != ry || part->rotation.vz != rz) {
        if (part->effects[0] != NULL) {
            tween = part->effects[0];
        } else {
            tween = func_801DF6F0(pool);
        }
        if (tween != NULL) {
            tween->used = 1;
            tween->kind = 3;
            tween->field1 = 0;
            tween->tag = 0xFE;
            tween->params[0] = part->rotation.vx;
            tween->params[1] = part->rotation.vy;
            tween->params[2] = part->rotation.vz;
            rx = (rx - part->rotation.vx) & 0xFFF;
            if (rx >= 0x800) {
                rx -= 0x1000;
            }
            tween->params[3] = rx;
            ry = (ry - part->rotation.vy) & 0xFFF;
            if (ry >= 0x800) {
                ry -= 0x1000;
            }
            tween->params[4] = ry;
            rz = (rz - part->rotation.vz) & 0xFFF;
            if (rz >= 0x800) {
                rz -= 0x1000;
            }
            tween->params[5] = rz;
            tween->time = 0;
            tween->duration = duration;
            part->effects[0] = tween;
        }
    }
}

/* Start a homing turn (kind `type` + 7: 7 pitch and yaw, 8 yaw) of a node
 * towards (x, y, z) in its first attachment: each tick it turns by at most
 * value 1 + (distance + time) * value 2 / value 0 (the first distance + 1),
 * time growing by `duration`, until released. */
void func_801E5B50(EffectPool *pool, ModelPart *part, s32 type, s32 arg3, s32 arg4, s32 duration,
                   s32 x, s32 y, s32 z) {
    EffectEntry *tween;
    s32 dx;
    s32 dy;
    s32 dz;

    if (part->effects[0] != NULL) {
        tween = part->effects[0];
    } else {
        tween = func_801DF6F0(pool);
    }
    if (tween != NULL) {
        tween->used = 1;
        tween->kind = type + 7;
        tween->field1 = 0;
        tween->tag = 0xFE;
        dx = x - part->translation[0];
        dy = y - part->translation[1];
        dz = z - part->translation[2];
        tween->params[0] = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
        tween->params[1] = arg3;
        tween->params[2] = arg4;
        tween->params[3] = x;
        tween->params[4] = y;
        tween->params[5] = z;
        tween->time = 0;
        tween->duration = duration;
        part->effects[0] = tween;
    }
}

/* Start an animation (looping to its loop frame when `loop`); none without
 * frames. */
void func_801E5C74(Actor *actor, Animation *anim, s32 loop) {
    if (anim->length != 0) {
        actor->anim_state = 0;
        if (loop) {
            actor->anim_loop = anim->loop;
        } else {
            actor->anim_loop = -1;
        }
        actor->anim_frame = 0;
        actor->anim_frames = anim->length;
        actor->anim_start = actor->anim_pos = (u8 *)anim + anim->dataOffset;
        return;
    }
    actor->anim_state = -1;
}

/* The sound bank id (in the high half) of source: 0 the system bank, 1 and 2
 * those of the actor's sound blocks (800AE220 without its source 3, the bank
 * D_800C4924). */
s32 func_801E5CD8(Actor *actor, s32 source) {
    if (source == 0) {
        return D_8005919C->bank << 16;
    } else if (source != 1) {
        if (source == 2) {
            return actor->ownerB4->bank->id << 16;
        }
    } else {
        return actor->ownerB0->bank->id << 16;
    }
}

/* Run an actor's animation events of the current frame (anchors and their
 * light columns, channel stops, node visibility, calls into the masked
 * actors and image animations), then advance the frame, looping at the
 * loop frame. The events are the battle's records (800AE2A4); anim_frame
 * counts them and anim_state is the frame. The call event (type 8) reads a
 * local the original never sets; it is spilled, so it is loaded from its
 * stack slot. */
void func_801E5D44(Actor *actor, EffectPool *pool, s32 arg2) {
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
        if (actor->anim_state != event->header.time) {
            break;
        }
        switch (event->header.type) {
        case 1: /* a battle sprite: stepped over */
            actor->anim_pos += 0x14;
            break;
        case 2: /* set up or deactivate (6 bytes) an anchor */
            if (event->light.on) {
                if (event->light.light < 2) {
                    anchor = event;
                    if (anchor->light.free) {
                        D_801E8648[anchor->light.light].object = -1;
                    } else {
                        D_801E8648[anchor->light.light].object = actor->index;
                    }
                    D_801E8648[anchor->light.light].part = anchor->light.part;
                    D_801E8644->m[0][anchor->light.light + 1] = anchor->light.r << 4;
                    D_801E8644->m[1][anchor->light.light + 1] = anchor->light.g << 4;
                    D_801E8644->m[2][anchor->light.light + 1] = anchor->light.b << 4;
                    D_801E8648[anchor->light.light].offset.vx = anchor->light.offset[0];
                    D_801E8648[anchor->light.light].offset.vy = anchor->light.offset[1];
                    D_801E8648[anchor->light.light].offset.vz = anchor->light.offset[2];
                    D_801E8648[anchor->light.light].active = anchor->light.active;
                }
                actor->anim_pos += 0x12;
            } else {
                D_801E8648[event->light.light].active = 0;
                actor->anim_pos += 6;
            }
            break;
        case 3:
        case 4: /* stop a channel (the battle's colour fade events) */
            func_801E0844(&actor->channels[event->channel.channel], arg2);
            if (event->channel.on) {
                actor->anim_pos += 0x1C;
            } else {
                actor->anim_pos += 6;
            }
            break;
        case 5: /* a battle sound: stepped over */
            actor->anim_pos += 8;
            break;
        case 6: /* a battle menu update: stepped over */
            actor->anim_pos += 4;
            break;
        case 7: /* show or hide a node */
            actor->parts[event->show.part].visible = event->show.visible & 1;
            actor->anim_pos += 6;
            break;
        case 8: /* call an entry (SlotEvent: its first script) of the masked actors */
            /* The original tests a local it never sets. */
            call = event;
            state = unset;
            saved_index = D_801E86B0;
            saved_mask = D_801E863C;
            bit = 1 << saved_index;
            for (i = 0, other = D_801E8670; i < 8; i++, other++) {
                if ((actor->mask >> i) & 1) {
                    entry = call->slots.scripts[0];
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
        case 9: /* start or stop (6 bytes) an image animation */
            if (event->image.on) {
                if (event->image.anim < actor->imageCount) {
                    if (event->image.target != 0xFF && event->image.target < actor->imageCount) {
                        target = &actor->images[event->image.target];
                    } else {
                        target = NULL;
                    }
                    m = NULL;
                    if ((event->image.mode & 0x7F) >= 4) {
                        m = D_801E8644;
                    }
                    curve = func_801E34BC(event->image.curve);
                    x = event->image.x;
                    y = event->image.y;
                    x2 = event->image.x2;
                    y2 = event->image.y2;
                    z2 = event->image.field10;
                    if (event->image.mode & 0x80) {
                        if (actor->h90 < 0) {
                            break;
                        }
                        x += actor->shift_x;
                        y += actor->shift_y;
                        if ((event->image.field12 >> 4) == 1) {
                            x2 += actor->shift_x;
                            y2 += actor->shift_y;
                        }
                    }
                    func_801E0A00(&actor->images[event->image.anim], target, event->image.mode & 0x7F,
                                  event->image.field12 | 0x700, (ColorRow *)m, x, y, 0, x2, y2, z2,
                                  x, y, event->image.field13, event->image.field14, event->image.field16,
                                  event->image.field18, event->image.field1A, curve);
                }
                actor->anim_pos += 0x1C;
            } else {
                func_801E165C(&actor->images[event->image.anim]);
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
    dx = actor->target[0] - root->translation[0];
    dy = actor->target[1] - root->translation[1];
    dz = actor->target[2] - root->translation[2];
    return SquareRoot0(dx * dx + dy * dy + dz * dz);
}

/* Aim an actor at a point of another actor's node (`aim_actor` is a
 * reference: 0xff the mask's lowest actor, 0xfe the current one, 1-0x7f an
 * index + 1), updating its target and a running movement tween. */
void func_801E63A8(Actor *actor) {
    VECTOR world;
    EffectEntry *tween;
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
            CompMatrix(&other->parts->transform, &other->parts[actor->aim_node].world, SCRATCH_MATRIX);
        } else {
            m = &other->parts->transform;
        }
        SetRotMatrix(m);
        SetTransMatrix(m);
        gte_ldv0(actor->aim_offset);
        gte_rtv0tr();
        gte_stlvnl(&world);
        actor->target[0] = world.vx;
        actor->target[1] = world.vy;
        actor->target[2] = world.vz;
        tween = actor->parts->effects[0];
        if (tween != NULL && (u32)(tween->kind - 7) < 2) {
            tween->params[3] = world.vx;
            tween->params[4] = world.vy;
            tween->params[5] = world.vz;
        }
    }
}

/* Hide node `index` of `parts` and its descendants, showing the same nodes of
 * `other`, and release their attachments. */
void func_801E6578(EffectPool *pool, s32 index, ModelPart *parts, ModelPart *other) {
    ModelPart *child;
    s32 count;
    s32 i;

    child = parts;
    count = parts->index;
    parts[index].visible = 0;
    other[index].visible = 1;
    func_801DF7A8(pool, parts[index].effects[0]);
    parts[index].effects[0] = NULL;
    func_801DF7A8(pool, parts[index].effects[1]);
    parts[index].effects[1] = NULL;
    func_801DF7A8(pool, parts[index].effects[2]);
    parts[index].effects[2] = NULL;
    for (i = 1; i < count;) {
        child++;
        i++;
        if (child->parent == &parts[index]) {
            func_801E6578(pool, child->index, parts, other);
        }
    }
}

/* Move the visibility of every shown node of `parts` to the same node of
 * `other`. */
void func_801E6668(ModelPart *parts, ModelPart *other) {
    u16 count;
    s32 i;

    count = parts->index;
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

    OuterProduct12(a, b, &v);
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
void func_801E6974(Actor *actor, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag,
                   u8 smooth, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, s16 duration) {
    EffectEntry *tween;
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
        tween = part->effects[0];
    } else if (type == 1) {
        tween = part->effects[1];
    } else {
        tween = part->effects[2];
    }
    if (tween != NULL || (tween = func_801DF6F0(pool)) != NULL) {
        tween->used = 1;
        tween->field1 = smooth;
        tween->kind = mode + 3;
        tween->tag = tag;
        if (flags & 0x20) {
            if (type == 0) {
                bx = part->rotation.vx;
                by = part->rotation.vy;
                bz = part->rotation.vz;
            } else if (type == 1) {
                bx = part->translation[0];
                by = part->translation[1];
                bz = part->translation[2];
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
                ex = part->rotation.vx;
                ey = part->rotation.vy;
                ez = part->rotation.vz;
            } else if (type == 1) {
                ex = part->translation[0];
                ey = part->translation[1];
                ez = part->translation[2];
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
        tween->params[0] = x0 + bx;
        tween->params[1] = y0 + by;
        tween->params[2] = z0 + bz;
        if (mode == 0) {
            tween->params[3] = x1 + ex - tween->params[0];
            tween->params[4] = y1 + ey - tween->params[1];
            tween->params[5] = z1 + ez - tween->params[2];
        } else {
            tween->params[3] = x1 + ex;
            tween->params[4] = y1 + ey;
            tween->params[5] = z1 + ez;
        }
        tween->time = 0;
        tween->duration = duration;
        if (type == 0) {
            if (mode < 2) {
                part->rotation.vx = tween->params[0];
                part->rotation.vy = tween->params[1];
                part->rotation.vz = tween->params[2];
            }
            part->effects[0] = tween;
        } else if (type == 1) {
            if (mode < 2) {
                part->translation[0] = (s16)tween->params[0];
                part->translation[1] = (s16)tween->params[1];
                part->translation[2] = (s16)tween->params[2];
            }
            part->effects[1] = tween;
        } else {
            if (mode < 2) {
                part->scale[0] = tween->params[0];
                part->scale[1] = tween->params[1];
                part->scale[2] = tween->params[2];
            }
            part->effects[2] = tween;
        }
    }
    if (flags & 0x80) {
        child = actor->parts;
        for (i = 1; i < actor->parts->index; i++) {
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
        for (i = 1; i < actor->parts->index; i++) {
            child++;
            if (child->parent == part) {
                func_801E6D94(actor, child, flags);
            }
        }
    }
}

/* Create a resident sprite linked to an actor node. */
void func_801E6E48(SpriteSource *source, s32 index, SVECTOR *position, s16 value, s16 scale,
                   SpriteCommand *command, Actor *actor) {
    SpriteTask *sprite;
    SpriteLink *link;

    sprite = func_80023FD8(index, source, position, sizeof(SpriteLink));
    func_80021FE0(&sprite->sprite, value);
    func_800223B0(&sprite->sprite, value);
    func_80022000(&sprite->sprite, scale);
    link = (SpriteLink *)((u8 *)sprite + (s16)sprite->sprite.size);
    link->actor = actor;
    link->node = command->part;
    if (command->follow) {
        link->update = func_8001CD7C(&sprite->task);
        func_8001CD6C(&sprite->task, func_801E6F64);
        link->offset.vx = command->offset[0];
        link->offset.vy = command->offset[1];
        link->offset.vz = command->offset[2];
        link->follow = command->mode;
    }
}

/* Sprite update: place the sprite at its offset through its actor node,
 * then run its own update. */
void func_801E6F64(Task *node) {
    SpriteTask *sprite = (SpriteTask *)node;
    SpriteLink *link = (SpriteLink *)((u8 *)sprite + (s16)sprite->sprite.size);
    MATRIX *m = SCRATCH_MATRIX;
    VECTOR world;

    if (link->node != 0) {
        CompMatrix(&link->actor->parts->transform, &link->actor->parts[link->node].world, m);
    } else {
        m = &link->actor->parts->transform;
    }
    SetRotMatrix(m);
    SetTransMatrix(m);
    gte_ldv0(&link->offset);
    gte_rtv0tr();
    gte_stlvnl(&world);
    if (link->follow) {
        world.vy = link->actor->groundY;
    }
    sprite->sprite.x = world.vx << 16;
    sprite->sprite.y = world.vy << 16;
    sprite->sprite.z = world.vz << 16;
    link->update(&sprite->task);
}

/* Set or (flag bit 5) add to a node's rotation (mode 0), position (1) or
 * scale, and with bit 7 its descendants'. */
void func_801E7094(Actor *actor, ModelPart *part, u8 flags, s16 x, s16 y, s16 z) {
    ModelPart *child;
    s32 i;

    if ((flags & 7) == 0) {
        if (flags & 0x20) {
            part->rotation.vx += x;
            part->rotation.vy += y;
            part->rotation.vz += z;
        } else {
            part->rotation.vx = x;
            part->rotation.vy = y;
            part->rotation.vz = z;
        }
    } else if ((flags & 7) == 1) {
        if (flags & 0x20) {
            part->translation[0] += x;
            part->translation[1] += y;
            part->translation[2] += z;
        } else {
            part->translation[0] = x;
            part->translation[1] = y;
            part->translation[2] = z;
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
        for (i = 1; i < actor->parts->index; i++) {
            child++;
            if (child->parent == part) {
                func_801E7094(actor, child, flags, x, y, z);
            }
        }
    }
}

/* Put an actor's root at its ground height unless it is held (b36). */
void func_801E7298(Actor *actor) {
    VECTOR unused;
    SVECTOR pos;

    pos.vy = actor->groundY;
    if (actor->b36 == 0) {
        actor->parts->translation[1] = pos.vy;
    }
}

/* The world matrix of node `node` of actor `index` (its root's transform for
 * node 0). */
void func_801E72CC(MATRIX *out, MATRIX *unused, s32 index, s32 node) {
    MATRIX m;
    Actor *actor;

    actor = D_801E8670[index];
    if (actor != NULL) {
        if (node != 0) {
            CompMatrix(&actor->parts->transform, &actor->parts[node].world, out);
        } else {
            *out = actor->parts->transform;
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
        D_801E85F4[j].models = NULL;
    }
    /* Both anchors' active flags, by byte offset. */
    for (offset = sizeof(Tracker); offset >= 0; offset -= sizeof(Tracker)) {
        *(s16 *)((u8 *)&D_801E8648[0].active + offset) = 0;
    }
}

/* Create actor `index` (when its slot is free) from its files: relocate them
 * (unless `flags` bit 0), load its sound bank (unless bit 2), copy its model
 * group into a free model list and build the hierarchy at `pos`, set up its
 * shadow quads, image animations and surfaces, reset its script (unless bit
 * 6) and keep a compacted copy of its model group (unless bit 1).
 * The model list is attached even when the files were already relocated. */
void func_801E742C(s32 index, u16 flags, ActorScript *script, ObjectModelFile *file, s16 x, s16 y,
                   s16 z, s16 w, s16 *pos) {
    Actor *actor;
    ObjectHeader *info;
    /* The descriptor, read as its header and then as the surfaces'
     * parameter words that follow it. */
    union {
        ObjectDesc *header;
        s16 *p;
    } desc;
    SoundOwner *sounds;
    ScriptBlock *block;
    void *images;
    u8 *group;
    u16 *hierarchy;
    s32 size;
    u8 *compact;
    s32 i, k;
    s32 count;
    Surface *record;
    POLY_FT4 *prim;

    func_80032498(4, 0);
    if (index >= 10 || D_801E8670[index] != NULL) {
        return;
    }
    actor = func_80031BDC(sizeof(Actor), 0);
    if (!(flags & 1)) {
        func_8003342C(file);
        func_8003342C(file->header);
    }
    actor->b62 = 0;
    actor->b63 = 0;
    if (!(flags & 4)) {
        func_8003342C(script);
        func_8003342C(script->owner);
        block = script->script;
        func_8003342C(block);
        func_8003342C(block->locals);
        sounds = script->owner;
        if (sounds->end != sounds->bank && func_8003864C(sounds->bank, 0) == 0) {
            func_80038428(sounds->bank);
            actor->b62 = 1;
        }
    }
    info = file->header;
    desc.header = info->desc;
    images = file->images;
    group = file->models;
    hierarchy = file->hierarchy;
    D_801E8670[index] = actor;
    actor->size[0] = desc.header->size[0];
    actor->size[1] = desc.header->size[1];
    actor->size[2] = desc.header->size[2];
    actor->reference = desc.header->field2A;
    actor->flags = desc.header->flags;
    size = (u8 *)hierarchy - group;
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
            if (D_801E85F4[D_801E8634].models == NULL) {
                break;
            }
        }
        func_801DC22C(D_801E8638, &D_801E85F4[D_801E8634]);
    }
    actor->models = &D_801E85F4[D_801E8634];
    if (!(flags & 0x40)) {
        if (actor->flags & 4) {
            actor->parts = func_801DC2D0(actor->models, hierarchy, 2, 0, 0, 0, 0, 0);
        } else {
            actor->parts = func_801DC2D0(actor->models, hierarchy, 2, 1, x, y, z, w);
        }
    } else {
        actor->parts = func_801DC2D0(actor->models, hierarchy, 0, 0, 0, 0, 0, 0);
    }
    if (pos != NULL) {
        actor->parts->translation[0] = pos[0];
        actor->parts->translation[1] = pos[1];
        actor->parts->translation[2] = pos[2];
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
    actor->scale = desc.header->scale;
    actor->channel_count = desc.header->channelCount;
    func_801E8510(actor);
    actor->imageCount = desc.header->imageAnimCount;
    if (actor->imageCount != 0) {
        actor->images = func_80031BDC(actor->imageCount * sizeof(ImageAnim), 0);
        for (i = 0; i < actor->imageCount; i++) {
            actor->images[i].active = 0;
            actor->images[i].pixels = NULL;
            actor->images[i].pixels2 = NULL;
            actor->images[i].work = NULL;
        }
    }
    actor->surfaceCount = desc.header->meshCount;
    if (actor->surfaceCount != 0) {
        desc.p = desc.header->meshes;
        record = func_80031BDC(actor->surfaceCount * sizeof(Surface), 0);
        actor->surfaces = record;
        for (i = 0; i < actor->surfaceCount; i++, record++) {
            count = desc.p[17];
            record->h0 = *desc.p++;
            /* The parameters are read in order. */
            func_801E1A14(record, info->meshData[i], *desc.p++, *desc.p++, *desc.p++,
                          *desc.p++, *desc.p++, count, x + *desc.p++, y + *desc.p++, *desc.p++,
                          *desc.p++, z + *desc.p++, w, *desc.p++, *desc.p++, *desc.p++, *desc.p++,
                          *desc.p++, *desc.p);
            desc.p += 2; /* past the last parameter and the entry count read above */
            for (k = 0; k < count; k++) {
                record->entries[k].h6 = *desc.p++;
                record->entries[k].hE = *desc.p++;
                record->entries[k].h0 = *desc.p++;
                record->entries[k].h2 = *desc.p++;
                record->entries[k].h4 = *desc.p++;
            }
        }
    }
    actor->index = index;
    actor->b22 = 0;
    actor->active = 1;
    if (!(flags & 0x40)) {
        block = script->script;
        actor->ownerB0 = script->owner;
        func_801E3534(actor, &D_801E86A8, block->entries, block->locals);
        func_801E35D0(actor, actor, &D_801E86A8, 0);
    }
    if (!(flags & 2)) {
        func_8002C644((ModelGroup *)D_801E8638);
        func_8002C4BC((ModelGroup *)D_801E8638);
        size = func_80031894(D_801E8638);
        compact = func_80031BDC(size, 0);
        memcpy(compact, D_801E8638, size);
        func_800320E8(D_801E8638);
        func_801DCE18(actor->models, 0);
        func_801DC22C(compact, actor->models);
        actor->group = compact;
    } else {
        actor->group = NULL;
    }
}

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
            D_801E8670[i]->previous[0] = D_801E8670[i]->parts->translation[0];
            D_801E8670[i]->previous[1] = D_801E8670[i]->parts->translation[1];
            D_801E8670[i]->previous[2] = D_801E8670[i]->parts->translation[2];
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
            D_801E8670[i]->moved[0] = D_801E8670[i]->previous[0] - D_801E8670[i]->parts->translation[0];
            D_801E8670[i]->moved[1] = D_801E8670[i]->previous[1] - D_801E8670[i]->parts->translation[1];
            D_801E8670[i]->moved[2] = D_801E8670[i]->previous[2] - D_801E8670[i]->parts->translation[2];
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
            func_8003852C(D_801E8670[index]->ownerB0->bank);
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
        if (D_801E8670[index]->imageCount != 0) {
            for (i = 0; i < D_801E8670[index]->imageCount; i++) {
                func_801E165C(&D_801E8670[index]->images[i]);
            }
            func_800320E8(D_801E8670[index]->images);
        }
        if (D_801E8670[index]->surfaceCount != 0) {
            for (i = 0; i < D_801E8670[index]->surfaceCount; i++) {
                func_801E3438(&D_801E8670[index]->surfaces[i]);
            }
            func_800320E8(D_801E8670[index]->surfaces);
        }
        func_800320E8(D_801E8670[index]);
        D_801E8670[index] = NULL;
    }
}

/* Select actor `index` and bit mask `mask`, then run its script step
 * (func_801E35D0) with itself as the source. */
void func_801E8330(u16 index, u16 mask, s32 entry) {
    Actor *actor;

    actor = D_801E8670[index];
    D_801E86B0 = index;
    D_801E863C = mask;
    actor->b35 = 0;
    if (D_801E8670[index] != NULL) {
        func_801E35D0(D_801E8670[index], D_801E8670[index], &D_801E86A8, entry);
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
    ColorFade *channels;
    s32 i;

    if (actor->channel_count != 0) {
        channels = func_80031BDC(actor->channel_count * sizeof(ColorFade), 0);
        for (i = 0; i < actor->channel_count; i++) {
            channels[i].id = -1;
            channels[i].sprite = NULL;
        }
        actor->channels = channels;
    }
}
