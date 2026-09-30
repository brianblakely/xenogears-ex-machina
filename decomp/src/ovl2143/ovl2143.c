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
        part->pad52 = 0;
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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DCEC8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DDBF8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DEF10);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF0B4);

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
 * position past used slots; NULL when the pool is full. Differs only in the
 * allocation of two argument registers. */
#ifdef NON_MATCHING
PoolSlot *func_801DF6F0(SlotPool *pool) {
    PoolSlot *slot;
    u32 capacity;

    if (pool->next < pool->capacity) {
        slot = &pool->slots[pool->next];
        if (slot->used != 0) {
            return NULL;
        }
        pool->next++;
        capacity = pool->capacity;
        while (pool->next < capacity) {
            if (pool->slots[pool->next].used == 0) {
                return slot;
            }
            pool->next++;
        }
        return slot;
    }
    return NULL;
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF6F0);
#endif

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF7F4);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DFAC4);

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0398);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0698);

/* Mark a particle free. */
void func_801E0844(s16 *age) {
    *age = -1;
}

/* base + (cos(angle) + 1.0) / divisor. */
s16 func_801E0850(s16 angle, s16 divisor, s32 base) {
    return base + (func_8003F8CC(angle) + 0x1000) / divisor;
}

/* base + value / divisor, or -1 past 32. Differs only in register choice. */
#ifdef NON_MATCHING
s32 func_801E08D4(s16 value, s16 divisor, s32 base) {
    s16 sum;

    sum = base + value / divisor;
    return sum < 0x21 ? sum : -1;
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E08D4);
#endif

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0A00);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1258);

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1880);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1A14);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E22F8);

/* Release a record's heap blocks. */
void func_801E3438(Record24 *record) {
    if (record->block14 != NULL) {
        func_800320E8(record->block14);
        func_800320E8(*record->block1C);
        func_800320E8(record->block1C);
        func_800320E8(record->block20);
        if (record->block18 != NULL) {
            func_800320E8(record->block18);
        }
        record->block14 = NULL;
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

/* Reset an actor's script state to run `entries` with the tables `locals`.
 * Differs only in where the -1 constant is loaded. */
#ifdef NON_MATCHING
void func_801E3534(Actor *actor, SlotPool *pool, s32 *entries, s32 *locals) {
    s16 none;

    none = -1;
    actor->h3C = 0xFFFF;
    actor->b5C = 0xFF;
    actor->b39 = 0x6B;
    actor->entries = entries;
    actor->shared = NULL;
    actor->pc = 0;
    actor->locals = locals;
    actor->globals = NULL;
    actor->depth = 0;
    actor->anim_state = none;
    actor->h58 = 0;
    actor->b35 = 0;
    actor->scaled = 0;
    actor->b38 = 0;
    actor->h3A = none;
    actor->h70[0] = 0;
    actor->h70[1] = 0;
    actor->h70[2] = 0;
    actor->h70[3] = 0;
    actor->h70[4] = 0;
    actor->h70[5] = 0;
    actor->h70[6] = 0;
    actor->h70[7] = 0;
    actor->h70[8] = 0;
    actor->h70[9] = 0;
    actor->h70[10] = 0;
    actor->h70[11] = 0;
    actor->h70[12] = 0;
    actor->h70[13] = 0;
    actor->h70[14] = 0;
    actor->h8E = 1;
    actor->b36 = 0;
    actor->h1E = none;
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E3534);
#endif

/* Call script entry `entry` of `source` in `actor`: queued while the actor is
 * already in a call, else run at once from the entry. Differs only in the
 * scheduling of the +23 byte store. */
#ifdef NON_MATCHING
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
        actor->b23 = 0;
        actor->mask = D_801E863C;
        func_801E39F0(actor, pool, -1, 1, 0);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E35D0);
#endif

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E36BC);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E37D0);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E39F0);

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
            tween->value[0] = part->rot.vx;
            tween->value[1] = part->rot.vy;
            tween->value[2] = part->rot.vz;
            rx = (rx - part->rot.vx) & 0xFFF;
            if (rx >= 0x800) {
                rx -= 0x1000;
            }
            tween->value[3] = rx;
            ry = (ry - part->rot.vy) & 0xFFF;
            if (ry >= 0x800) {
                ry -= 0x1000;
            }
            tween->value[4] = ry;
            rz = (rz - part->rot.vz) & 0xFFF;
            if (rz >= 0x800) {
                rz -= 0x1000;
            }
            tween->value[5] = rz;
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
        tween->value[0] = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
        tween->value[1] = arg3;
        tween->value[2] = arg4;
        tween->value[3] = x;
        tween->value[4] = y;
        tween->value[5] = z;
        tween->time = 0;
        tween->duration = duration;
        part->attachments[0] = tween;
    }
}

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5C74);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5CD8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5D44);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E632C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6338);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E63A8);

/* Hide node `index` of `parts` and its descendants, showing the same nodes of
 * `other`, and release their attachments. Differs in register assignment and
 * one addu operand order. */
#ifdef NON_MATCHING
void func_801E6578(SlotPool *pool, s32 index, ModelPart *parts, ModelPart *other) {
    ModelPart *part;
    ModelPart *child;
    s32 count;
    s32 i;

    child = parts;
    part = &parts[index];
    count = parts->count;
    part->visible = 0;
    other[index].visible = 1;
    func_801DF7A8(pool, part->attachments[0]);
    part->attachments[0] = NULL;
    func_801DF7A8(pool, part->attachments[1]);
    part->attachments[1] = NULL;
    func_801DF7A8(pool, part->attachments[2]);
    part->attachments[2] = NULL;
    for (i = 1; i < count;) {
        child++;
        i++;
        if (child->parent == part) {
            func_801E6578(pool, child->count, parts, other);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6578);
#endif

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E66BC);

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6830);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6910);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6974);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6D94);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6E48);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6F64);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7094);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7298);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E72CC);

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
    /* Both 0x14-byte records' second halfword, by byte offset. */
    for (offset = sizeof(Record14); offset >= 0; offset -= sizeof(Record14)) {
        *(s16 *)((u8 *)&D_801E864C[0].h2 + offset) = 0;
    }
}

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E742C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7D14);

/* Release every actor and both pools. */
void func_801E7FD4(void) {
    s32 i;

    for (i = 0; i < 10; i++) {
        func_801E8030(i);
    }
    func_801DF668(&D_801E86A8);
    func_801E00DC(&D_801E86A0);
}

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8030);

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
            channels[i].w8 = 0;
        }
        actor->channels = channels;
    }
}
