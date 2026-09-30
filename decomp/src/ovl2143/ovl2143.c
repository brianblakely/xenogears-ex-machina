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
            func_8003F968(part->packets[1], part->packets[0], group->models[model]->packet_bytes);
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
    func_8004920C(&parts->world, scaling, &parts->local);
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
                func_8004931C(&parts->parent->world, &parts->local, &parts->world);
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
    func_8004920C(&parts->world, scaling, &parts->local);
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
            func_8004920C(&parts->local, scratch, &parts->local);
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
                func_8004920C(scratch, &parts->local, &parts->local);
            }
        }
        if (parts->dirty) {
            parts->local.t[0] = parts->pos[0];
            parts->local.t[1] = parts->pos[1];
            parts->local.t[2] = parts->pos[2];
            if (parts->parent != NULL) {
                func_8004931C(&parts->parent->world, &parts->local, &parts->world);
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

    func_8004920C(light, &parts->world, light_root);
    func_8004931C(view, &parts->local, view_root);
    count = parts->count;
    parts++;
    for (i = 1; i < count; parts++) {
        i++;
        if (parts->model != 0xFFFF) {
            func_8004920C(light_root, &parts->world, m);
            func_80049F2C(m);
            func_8004931C(view_root, &parts->world, m);
            func_80049EFC(m);
            func_80049F8C(m);
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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DFE8C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DFF78);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0064);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E00DC);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E011C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0248);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0354);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0398);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0698);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0844);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0850);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E08D4);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0938);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0988);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E0A00);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1258);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E165C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1708);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E17B8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1880);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E1A14);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E22F8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E3438);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E34BC);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E3534);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E35D0);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E36BC);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E37D0);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E39F0);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E59D4);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5B50);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5C74);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5CD8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E5D44);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E632C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6338);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E63A8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6578);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6668);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E66BC);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E67F8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6830);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6910);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6974);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6D94);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6E48);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E6F64);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7094);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7298);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E72CC);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7378);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E738C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E742C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7D14);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E7FD4);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8030);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8330);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8394);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8430);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8480);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801E8510);
