/* Battle unit 8009E53C-800B16F0: gear part bookkeeping, model hierarchies,
 * effect and sprite pools, scene geometry and battle objects.
 *
 * GCC aligns jump tables to 8 within a unit's rodata; the tables up to
 * 8009BAC4's (0x800704A0, ending at 0x800704CC) sit at 0 mod 8, those from
 * 8009E788's (0x800704CC) at 4 mod 8, so this unit's rodata starts at
 * 0x800704CC. Its code divides with ASPSX's checked division (break 7/6,
 * 8009F1C4 through 800B10EC), the code up to 8009DBFC without (MASPSX
 * --expand-div for this unit). The text boundary lies after 8009DBFC and at
 * or before 8009E788; 8009E53C is where the gear formula functions end and
 * the part bookkeeping (8009E53C-8009E788) starts. The unit ends before
 * 800B1720, where the code generation changes (see battle_800B1720.c). */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "mesh.h"
#include "files.h"
#include "stage.h"

/* Whether gear part 50 + index is one of the parts of character 4's gear. */
s32 func_8009E53C(u8 index) {
    BattlePart *part = &D_800C34B0->lists.parts.members[index];

    if (D_8006D8A0.gears[D_8006D8A0.characters[4].gearId].entries[0].id == part->id || D_8006D8A0.gears[D_8006D8A0.characters[4].gearId].entries[1].id == part->id) {
        return 1;
    }
    return D_8006D8A0.gears[D_8006D8A0.characters[4].gearId].entries[2].id == part->id;
}

/* Put battle gear part index into character 4's gear entry holding its id
 * (entry k when none does; the third entry's match selects entry 3): copy its
 * values, record the part slot and durability, and update the battle copies of
 * character 4's gear. */
void func_8009E5C8(u8 index, u8 k) {
    BattlePart *part = &D_800C34B0->lists.parts.list[index];
    u8 gearId = D_8006D8A0.characters[4].gearId;
    u8 i;

    if (D_8006D8A0.gears[gearId].entries[0].id == part->id) {
        k = 0;
    }
    if (D_8006D8A0.gears[gearId].entries[1].id == part->id) {
        k = 1;
    }
    if (D_8006D8A0.gears[gearId].entries[2].id == part->id) {
        k = 3;
    }
    D_8006D8A0.gears[gearId].entries[k].valueE = part->valueE;
    D_8006D8A0.gears[gearId].entries[k].value11 = part->value11;
    D_8006D8A0.gears[gearId].entries[k].value10 = part->value10;
    D_8006D8A0.gears[gearId].entries[k].value11 = part->value11;
    D_8006D8A0.gears[gearId].partItems[k] = index;
    D_8006F8BA[index] = part->durability;
    for (i = 0; i < 3; i++) {
        if ((D_800C34B0->records + i)->pilot.characterId == 4) {
            D_800C34B0->records[i].gear.entries[k].valueE = part->valueE;
            D_800C34B0->records[i].gear.entries[k].value11 = part->value11;
            D_800C34B0->records[i].gear.entries[k].value10 = part->value10;
            D_800C34B0->records[i].gear.entries[k].value11 = part->value11;
            D_800C34B0->records[i].gear.partItems[k] = index;
        }
    }
}

/* Wear the attacker gear's parts for the current command: command 0 the first
 * part's, 2 and 17 the fourth's, 3-14 both, 15 the first's. */
void func_8009E788(void) {
    switch (D_800C34B0->commandIndex) {
    case 0:
        if (D_8006F8EA[D_800D2D6C->partItems[0]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[0]] += -1;
        }
        break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        if (D_8006F8EA[D_800D2D6C->partItems[0]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[0]] += -1;
        }
        if (D_8006F8EA[D_800D2D6C->partItems[3]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[3]] += -1;
        }
        break;
    case 15:
        if (D_8006F8EA[D_800D2D6C->partItems[0]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[0]] += -1;
        }
        break;
    case 2:
    case 17:
        if (D_8006F8EA[D_800D2D6C->partItems[3]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[3]] += -1;
        }
        break;
    }
}

/* Show the message for a gear status that was applied, named by its kind and
 * flag bit (the gear counterpart of 8009B684). */
void func_8009E868(u8 kind, u16 flag) {
    switch (kind) {
    case 0:
        switch (flag) {
        case 0x400:
            D_800C34B0->message = 0x24;
            break;
        case 0x200:
            D_800C34B0->message = 0x25;
            break;
        case 0x100:
            D_800C34B0->message = 0x26;
            break;
        case 0x80:
            D_800C34B0->message = 0x27;
            break;
        case 0x40:
            D_800C34B0->message = 0x28;
            break;
        case 0x20:
            D_800C34B0->message = 0x29;
            break;
        case 0x10:
            D_800C34B0->message = 0x2A;
            break;
        case 0x4:
            D_800C34B0->message = 0x2B;
            break;
        }
        break;
    case 1:
        switch (flag) {
        case 0x1000:
            D_800C34B0->message = 0x2D;
            break;
        case 0x800:
            D_800C34B0->message = 0x2E;
            break;
        case 0x400:
            D_800C34B0->message = 0x2F;
            break;
        case 0x40:
            D_800C34B0->message = 0x15;
            break;
        case 0x20:
            D_800C34B0->message = 0x16;
            break;
        case 0x2:
        case 0x8:
            D_800C34B0->message = 0x19;
            break;
        case 0x1:
        case 0x4:
            D_800C34B0->message = 0x1A;
            break;
        }
        break;
    case 3:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x1B;
            break;
        case 0x4000:
            D_800C34B0->message = 0x1C;
            break;
        case 0x2000:
            D_800C34B0->message = 0x1D;
            break;
        case 0x1000:
            D_800C34B0->message = 0x1E;
            break;
        case 0x400:
            D_800C34B0->message = 0x1F;
            break;
        case 0x800:
            D_800C34B0->message = 0x20;
            break;
        case 0x100:
            D_800C34B0->message = 0x21;
            break;
        case 0x200:
            D_800C34B0->message = 0x22;
            break;
        }
        break;
    }
}

/* Relocate a model group and list its models in a new table. */
ModelList *func_8009EBA8(u8 *group, ModelList *list) {
    u32 count;
    u32 i;

    func_80032498(4, 0);
    count = func_8002C3E8(group);
    list->models = func_80031BDC(count * 4, 0);
    list->count = count;
    if (list->models != NULL) {
        for (i = 0; i < count; i++) {
            list->models[i] = (Model *)(group + 0x10 + i * 0x38);
        }
    }
    return list;
}

/* Build a model hierarchy from (model, parent) pairs, up to the first pair
 * naming neither a listed model nor 0xFFFF: a root part, then one part per
 * pair with its packets for both buffers (built with mode; offset by (x0, y0)
 * and (x1, y1) when offset is set). Returns the root, NULL on failure. */
ModelPart *func_8009EC4C(ModelList *list, u16 *hierarchy, s32 mode, s32 offset, u16 x0, u16 y0,
                         u16 x1, u16 y1) {
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
    root->flag4 = 1;
    root->flag5 = 1;
    root->flag6 = 1;
    root->scale[0] = 0x1000;
    root->scale[1] = 0x1000;
    root->scale[2] = 0x1000;
    root->parent = NULL;
    root->flag7 = 0;
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
        part->flag4 = 1;
        part->flag5 = 1;
        part->flag7 = 1;
        part->scale[0] = 0x1000;
        part->scale[1] = 0x1000;
        part->scale[2] = 0x1000;
        part->flag6 = 0;
        part->field52 = 0;
        part->modelId = id;
        if (id != 0xFFFF) {
            func_8002CB54(list->models[id], &part->packets[0], &part->packets[1]);
            if (part->packets[0] == NULL) {
                func_8009F708(root);
                return NULL;
            }
            if (offset) {
                func_8002CC10(x0, y0);
                func_8002CC74(x1, y1);
            }
            func_8002C8CC(list->models[id], part->packets[0], mode);
            memcpy(part->packets[1], part->packets[0], list->models[id]->packetSize);
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

/* Pose a model hierarchy: the root's rotation (YXZ order when flag6 is set)
 * with its translation, scaled by scale (4.12) per axis into its transform;
 * each changed part (flag5) its rotation, and each part marked (flag4, or
 * under a marked parent) its translation and world matrix. Clears the marks
 * and returns the part count. */
u16 func_8009EF3C(ModelPart *part, s32 scale) {
    Matrix *diagonal = (Matrix *)0x1F800000;
    ModelPart *root = part;
    u32 count = root->index;
    u32 i;

    root->world.t[0] = root->translation[0];
    root->world.t[1] = root->translation[1];
    root->world.t[2] = root->translation[2];
    if (root->flag6) {
        func_8004A92C(&root->rotation, &root->world);
    } else {
        func_8003F738(&root->rotation, &root->world);
    }
    diagonal->m[0][0] = scale * part->scale[0] >> 12;
    diagonal->m[0][1] = 0;
    diagonal->m[0][2] = 0;
    diagonal->m[1][0] = 0;
    diagonal->m[1][1] = scale * part->scale[1] >> 12;
    diagonal->m[1][2] = 0;
    diagonal->m[2][0] = 0;
    diagonal->m[2][1] = 0;
    diagonal->m[2][2] = scale * part->scale[2] >> 12;
    MulMatrix0(&part->world, diagonal, &part->transform);
    part->transform.t[0] = part->world.t[0];
    part->transform.t[1] = part->world.t[1];
    part->transform.t[2] = part->world.t[2];
    for (i = 1; i < count; i++) {
        part++;
        if (part->flag5) {
            if (part->flag6) {
                func_8004A92C(&part->rotation, &part->transform);
            } else {
                func_8003F738(&part->rotation, &part->transform);
            }
            part->flag5 = 0;
        }
        if (part->parent != NULL && part->parent->flag4 == 1) {
            part->flag4 = 1;
        }
        if (part->flag4) {
            part->transform.t[0] = part->translation[0];
            part->transform.t[1] = part->translation[1];
            part->transform.t[2] = part->translation[2];
            if (part->parent != NULL) {
                CompMatrix(&part->parent->world, &part->transform, &part->world);
            } else {
                part->world = part->transform;
            }
        }
    }
    for (i = 1; i < count; i++) {
        root++;
        root->flag4 = 0;
    }
    return count;
}

#ifdef NON_MATCHING
/* Pose a model hierarchy with per-part scales: as 8009EF3C, but a changed
 * part's transform is scaled by its own scale and by the inverse of its
 * parent's, and a part changes or is marked with its parent. Clears the marks
 * and returns the part count. */
u16 func_8009F1C4(ModelPart *part, s32 scale) {
    Matrix *diagonal = (Matrix *)0x1F800000;
    ModelPart *root = part;
    u32 count = root->index;
    u32 i;

    root->world.t[0] = root->translation[0];
    root->world.t[1] = root->translation[1];
    root->world.t[2] = root->translation[2];
    if (root->flag6) {
        func_8004A92C(&root->rotation, &root->world);
    } else {
        func_8003F738(&root->rotation, &root->world);
    }
    diagonal->m[0][0] = scale * part->scale[0] >> 12;
    diagonal->m[0][1] = 0;
    diagonal->m[0][2] = 0;
    diagonal->m[1][0] = 0;
    diagonal->m[1][1] = scale * part->scale[1] >> 12;
    diagonal->m[1][2] = 0;
    diagonal->m[2][0] = 0;
    diagonal->m[2][1] = 0;
    diagonal->m[2][2] = scale * part->scale[2] >> 12;
    MulMatrix0(&part->world, diagonal, &part->transform);
    part->transform.t[0] = part->world.t[0];
    part->transform.t[1] = part->world.t[1];
    part->transform.t[2] = part->world.t[2];
    for (i = 1; i < count; i++) {
        part++;
        if (part->parent != NULL) {
            if (part->parent->flag5 == 1) {
                part->flag5 = 1;
            }
            if (part->parent->flag4 == 1) {
                part->flag4 = 1;
            }
        }
        if (part->flag5) {
            if (part->flag6) {
                func_8004A92C(&part->rotation, &part->transform);
            } else {
                func_8003F738(&part->rotation, &part->transform);
            }
            diagonal->m[0][0] = part->scale[0];
            diagonal->m[0][1] = 0;
            diagonal->m[0][2] = 0;
            diagonal->m[1][0] = 0;
            diagonal->m[1][1] = part->scale[1];
            diagonal->m[1][2] = 0;
            diagonal->m[2][0] = 0;
            diagonal->m[2][1] = 0;
            diagonal->m[2][2] = part->scale[2];
            MulMatrix0(&part->transform, diagonal, &part->transform);
            if (part->parent != NULL) {
                diagonal->m[0][0] = 0x1000000 / part->parent->scale[0];
                diagonal->m[0][1] = 0;
                diagonal->m[0][2] = 0;
                diagonal->m[1][0] = 0;
                diagonal->m[1][1] = 0x1000000 / part->parent->scale[1];
                diagonal->m[1][2] = 0;
                diagonal->m[2][0] = 0;
                diagonal->m[2][1] = 0;
                diagonal->m[2][2] = 0x1000000 / part->parent->scale[2];
                MulMatrix0(diagonal, &part->transform, &part->transform);
            }
        }
        if (part->flag4) {
            part->transform.t[0] = part->translation[0];
            part->transform.t[1] = part->translation[1];
            part->transform.t[2] = part->translation[2];
            if (part->parent != NULL) {
                CompMatrix(&part->parent->world, &part->transform, &part->world);
            } else {
                part->world = part->transform;
            }
        }
    }
    for (i = 1; i < count; i++) {
        root++;
        root->flag5 = 0;
        root->flag4 = 0;
    }
    return count;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_8009F1C4);
#endif

void func_8009F5B0(void) {
}

/* Draw a posed hierarchy's parts into packet buffer `buffer`: each part's light
 * matrix from `light` and its world matrix, and its rotation and translation
 * composed with `view` and the root's transform, then its model (8002C700). */
void func_8009F5B8(ModelList *list, ModelPart *part, Matrix *view, Matrix *light, s32 arg4, s32 arg5,
                   s32 buffer) {
    Matrix *scratch = (Matrix *)0x1F800000;
    Matrix *lighting = (Matrix *)0x1F800020;
    Matrix *camera = (Matrix *)0x1F800040;
    u32 count;
    u32 i;

    MulMatrix0(light, &part->world, lighting);
    CompMatrix(view, &part->transform, camera);
    count = part->index;
    part++;
    for (i = 1; i < count; i++, part++) {
        if (part->modelId != 0xFFFF) {
            MulMatrix0(lighting, &part->world, scratch);
            SetLightMatrix(scratch);
            CompMatrix(camera, &part->world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            func_8002C700(list->models[part->modelId], part->packets[buffer], arg5, arg4);
        }
    }
}

/* Free a model hierarchy: every part's packets, then the parts. */
void func_8009F708(ModelPart *root) {
    ModelPart *part;
    s32 i;

    if (root != NULL) {
        part = root;
        for (i = 0; i < root->index; i++, part++) {
            if (part->packets[0] != NULL) {
                func_800320E8(part->packets[0]);
                part->packets[0] = NULL;
                part->packets[1] = NULL;
            }
        }
        root->index = 0;
        func_800320E8(root);
    }
}

/* Free a model list's table, releasing its models first when release is
 * set. */
void func_8009F794(ModelList *list, s32 release) {
    u32 i;

    if (list != NULL) {
        for (i = 0; i < list->count; i++) {
            if (list->models != NULL && list->models[i] != NULL && release) {
                func_8002CBBC(list->models[i]);
            }
        }
        if (list->models != NULL) {
            func_800320E8(list->models);
            list->models = NULL;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_8009F844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A0838);

/* Apply an animation frame to a hierarchy's parts: the listed rotations (unless
 * flag 1) and translations (unless flag 2), marking each changed part; parts
 * with a persistent effect attached keep theirs. The frame holds halfwords:
 * [2] flags, [3] base flag, [6] rotation count, [7] translation count, then
 * from [12] (x, y, z) triples, after the base rotations when [3] is 0.
 * Returns the part count less the root. */
u16 func_800A1B50(ModelPart *root, s16 *data) {
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
                && (part->effects[0] == NULL || part->effects[0]->kind != 0xFF)) {
                part->rotation.vx = x;
                part->rotation.vy = y;
                part->rotation.vz = z;
                part->flag4 = 1;
                part->flag5 = 1;
            }
        }
        if (!(flags & 2) && translations < translationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            translations++;
            if ((part->translation[0] != x || part->translation[1] != y || part->translation[2] != z)
                && (part->effects[1] == NULL || part->effects[1]->kind != 0xFF)) {
                part->translation[0] = x;
                part->translation[1] = y;
                part->translation[2] = z;
                part->flag4 = 1;
            }
        }
    }
    return count;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A1CF4);

/* Release the effects attached to part index of a hierarchy, those selected by
 * mask (bit n: attachment n). */
void func_800A216C(EffectPool *pool, ModelPart *part, s32 index, s32 mask) {
    if (index < part->index) {
        part += index;
        if (part->effects[0] != NULL && (mask & 1)) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && (mask & 2)) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && (mask & 4)) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Create a pool of count effect entries. */
EffectPool *func_800A2234(EffectPool *pool, s32 count) {
    if (count <= 0) {
        return NULL;
    }
    pool->count = count;
    func_80032498(4, 0);
    pool->entries = func_80031BDC(count * sizeof(EffectEntry), 0);
    if (pool->entries != NULL) {
        func_800A22E8(pool);
        return pool;
    }
    return NULL;
}

/* Free a pool's entries. */
void func_800A22A8(EffectPool *pool) {
    pool->next = 0;
    if (pool->entries != NULL) {
        func_800320E8(pool->entries);
    }
    pool->entries = NULL;
}

/* Mark every entry of a pool free. */
void func_800A22E8(EffectPool *pool) {
    EffectEntry *entry;
    s32 i;

    if (pool->entries != NULL) {
        entry = pool->entries;
        pool->next = 0;
        for (i = 0; i < pool->count; i++) {
            entry->used = 0;
            entry++;
        }
    }
}

/* Take the first free entry of a pool (NULL when none), advancing the free
 * index past the entries in use. */
EffectEntry *func_800A2330(EffectPool *pool) {
    EffectEntry *entry;
    u32 count;

    if (pool->next < pool->count) {
        entry = &pool->entries[pool->next];
        if (entry->used) {
            return NULL;
        }
        pool->next++;
        count = pool->count;
        while (pool->next < count && pool->entries[pool->next].used != 0) {
            pool->next++;
        }
        return entry;
    }
    return NULL;
}

/* Return an entry to its pool; its index, or -1 for none. */
s32 func_800A23E8(EffectPool *pool, EffectEntry *entry) {
    s32 index;

    if (entry == NULL) {
        return -1;
    }
    index = ((u32)entry - (u32)pool->entries) / sizeof(EffectEntry);
    if (index < pool->next) {
        pool->next = index;
    }
    entry->used = 0;
    return index;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A2434);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A2704);

/* Release every part's attached effects that are not persistent. */
void func_800A2ACC(EffectPool *pool, ModelPart *part) {
    u16 count = part->index;
    s32 i;

    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL && part->effects[0]->kind != 0xFF) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && part->effects[1]->kind != 0xFF) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && part->effects[2]->kind != 0xFF) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Release every part's attached effects of kind. */
void func_800A2BB8(EffectPool *pool, ModelPart *part, u8 kind) {
    u16 count = part->index;
    s32 i;

    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL && part->effects[0]->kind == kind) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && part->effects[1]->kind == kind) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && part->effects[2]->kind == kind) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Create a pool of count sprite records (and a spare). */
SpritePool *func_800A2CA4(SpritePool *pool, s32 count) {
    func_80032498(4, 0);
    pool->count = count;
    pool->next = 0;
    pool->records = func_80031BDC((count + 1) * sizeof(SpriteRecord), 0);
    if (pool->records != NULL) {
        func_800A2D5C(pool);
        return pool;
    }
    return NULL;
}

/* Free a sprite pool's records. */
void func_800A2D1C(SpritePool *pool) {
    pool->count = 0;
    pool->next = 0;
    if (pool->records != NULL) {
        func_800320E8(pool->records);
    }
    pool->records = NULL;
}

/* Mark every record of a sprite pool free and set up both of its
 * semi-transparent quadrilaterals. */
void func_800A2D5C(SpritePool *pool) {
    SpriteRecord *record = pool->records;
    s32 i;
    s32 j;

    for (i = 0; i < pool->count + 1; i++) {
        record->id = -1;
        record->field1E = 0;
        for (j = 0; j < 2; j++) {
            SetPolyFT4(&record->packets[j]);
            SetSemiTrans(&record->packets[j], 1);
            record->packets[j].clut = GetClut(0, 0x1CD);
            record->packets[j].tpage = GetTPage(0, 1, 0x380, 0);
            record->packets[j].u0 = 0;
            record->packets[j].v0 = 0xC1;
            record->packets[j].u1 = 0;
            record->packets[j].v1 = 0xC1;
            record->packets[j].u2 = 0xF;
            record->packets[j].v2 = 0xC1;
            record->packets[j].u3 = 0xF;
            record->packets[j].v3 = 0xC1;
        }
        record++;
    }
}

/* Take the first free record of a sprite pool with its quadrilaterals'
 * semi-transparency set to abe, advancing the free index past the records in
 * use; the spare record when none is free. */
SpriteRecord *func_800A2E88(SpritePool *pool, s16 abe) {
    SpriteRecord *record;
    s16 next = pool->next;

    if (next < pool->count) {
        record = &pool->records[next];
        if (record->id == -1) {
            pool->next = next + 1;
            while (pool->next < pool->count && pool->records[pool->next].id != -1) {
                pool->next++;
            }
            SetSemiTrans(&record->packets[0], abe);
            SetSemiTrans(&record->packets[1], abe);
            return record;
        }
    }
    return &pool->records[pool->count];
}

/* Return a record to its sprite pool; its index. */
s32 func_800A2F94(SpritePool *pool, SpriteRecord *record) {
    s32 index = ((u32)record - (u32)pool->records) / sizeof(SpriteRecord);

    if (index <= pool->next) {
        pool->next = index;
    }
    record->id = -1;
    return index;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A2FD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A32D8);

/* Mark a halfword slot empty. */
void func_800A3484(s16 *slot) {
    *slot = -1;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A3490);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A3514);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A3578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A35C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A3640);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A3E98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A429C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A4348);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A43F8);

/* Update the active trackers' positions: an offset from a part of a stage
 * object's hierarchy when the object exists, else the offset itself. */
void func_800A44C0(BattleObject **objects) {
    Matrix *m = (Matrix *)0x1F800000;
    s32 i;
    ModelPart *root;
    Vector position;

    for (i = 0; i < 2; i++) {
        if (D_800D3304[i].active != 0) {
            if (D_800D3304[i].object >= 0 && objects[D_800D3304[i].object] != NULL) {
                root = objects[D_800D3304[i].object]->hierarchy;
                CompMatrix(&root->transform, &root[D_800D3304[i].part + 1].world, m);
                SetRotMatrix(m);
                SetTransMatrix(m);
                gte_ldv0(&D_800D3304[i].offset);
                gte_rtv0tr();
                gte_stlvnl(&position);
                D_800D3304[i].x = position.vx;
                D_800D3304[i].y = position.vy;
                D_800D3304[i].z = position.vz;
            } else {
                D_800D3304[i].x = D_800D3304[i].offset.vx;
                D_800D3304[i].y = D_800D3304[i].offset.vy;
                D_800D3304[i].z = D_800D3304[i].offset.vz;
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A4654);

/* Free the battle scene's resources: the stage objects, the scene data, both
 * resident handles of D_800C3D50 (80027D40), the block D_800C3EA0 and both
 * records of D_800C3DA0 (8002800C). */
void func_800A4820(void) {
    s32 i;

    func_800A9FF0(31);
    D_800C3E38 = 0;
    if (D_800658C8 != NULL) {
        func_800320E8(D_800658C8);
    }
    D_800658C8 = NULL;
    for (i = 0; i < 2; i++) {
        if (D_800C3D50[i] != NULL) {
            func_80027D40(D_800C3D50[i]);
        }
        D_800C3D50[i] = NULL;
    }
    if (D_800C3EA0 != NULL) {
        func_800320E8(D_800C3EA0);
    }
    D_800C3EA0 = NULL;
    for (i = 0; i < 2; i++) {
        func_8002800C(&D_800C3DA0[i]);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A48EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A4B3C);

/* Place a copy of stage object index (800B10EC) at every listed point, its
 * height raised by the object's size. */
void func_800A4CF8(s32 index) {
    u16 *point = D_800D2FD0;
    s32 i;
    s16 x;
    s16 z;
    s16 y;

    for (i = 0; i < D_800D2FC8; i++) {
        x = *point++;
        z = *point++;
        y = *point++;
        func_800B10EC(index, x, z, func_800AA650(index) + y);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A4DB8);

/* The scene's points. */
SVector *func_800A577C(void) {
    return D_800D3344;
}

/* The scene's triangles. */
SceneTriangle *func_800A578C(void) {
    return D_800D39CC;
}

/* The first scene triangle containing point (800A5A48 gives -1), -1 for
 * none. */
s32 func_800A579C(SVector *point) {
    s32 i;

    if (D_800D3344 != NULL && D_800D39CC != NULL) {
        for (i = 0; i < D_800D3348; i++) {
            if (func_800A5A48(&D_800D3344[D_800D39CC[i].vertices[0]], &D_800D3344[D_800D39CC[i].vertices[1]],
                              &D_800D3344[D_800D39CC[i].vertices[2]], point)
                == -1) {
                return i;
            }
        }
    }
    return -1;
}

/* Relate point to scene triangle index (800A5BE8, into out); the triangle's
 * id, or -1 without scene geometry. */
s32 func_800A5870(SVector *point, s32 index, void *out) {
    SceneTriangle *triangle;

    if (D_800D3344 == NULL || D_800D39CC == NULL || index < 0) {
        return -1;
    }
    triangle = &D_800D39CC[index];
    func_800A5BE8(&D_800D3344[triangle->vertices[0]], &D_800D3344[triangle->vertices[1]],
                  &D_800D3344[triangle->vertices[2]], point, out);
    return D_800D39CC[index].id;
}

/* The scene triangle containing point, searched from triangle through its
 * neighbours (800A5D54, depth levels, up to depth tries); -1 for none. Each
 * search uses a new visit stamp; when the stamp wraps the marks are cleared. */
s32 func_800A5914(SVector *point, s32 triangle, s32 depth) {
    s32 found;
    s32 i;

    if (D_800D3344 == NULL) {
        return -1;
    }
    if (D_800D39CC == NULL) {
        return -1;
    }
    if (triangle >= D_800D3348) {
        return -1;
    }
    for (i = 0; i < depth; i++) {
        found = func_800A5D54(point, triangle, depth);
        if (found >= 0) {
            break;
        }
    }
    D_800D2F64++;
    if (D_800D2F64 == 0) {
        D_800D2F64 = 1;
        for (i = 0; i < D_800D3348; i++) {
            D_800D39CC[i].visited = 0;
        }
    }
    return found;
}

/* Whether point lies within triangle (a, b, c) in the ground plane: -1 when
 * it is on the inner side of all three edges (cross products, 8004A4D8),
 * otherwise 0. */
s32 func_800A5A48(SVector *a, SVector *b, SVector *c, SVector *point) {
    Vector edge;
    Vector toPoint;
    Vector cross;

    edge.vx = b->vx - a->vx;
    edge.vy = 0;
    edge.vz = b->vz - a->vz;
    toPoint.vx = point->vx - a->vx;
    toPoint.vy = 0;
    toPoint.vz = point->vz - a->vz;
    OuterProduct0(&edge, &toPoint, &cross);
    if (cross.vy < 0) {
        return 0;
    }
    edge.vx = c->vx - b->vx;
    edge.vy = 0;
    edge.vz = c->vz - b->vz;
    toPoint.vx = point->vx - b->vx;
    toPoint.vy = 0;
    toPoint.vz = point->vz - b->vz;
    OuterProduct0(&edge, &toPoint, &cross);
    if (cross.vy < 0) {
        return 0;
    }
    edge.vx = a->vx - c->vx;
    edge.vy = 0;
    edge.vz = a->vz - c->vz;
    toPoint.vx = point->vx - c->vx;
    toPoint.vy = 0;
    toPoint.vz = point->vz - c->vz;
    OuterProduct0(&edge, &toPoint, &cross);
    return -(cross.vy >= 0);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A5BE8);

#ifdef NON_MATCHING
/* Search triangle and, up to depth levels, its unvisited neighbours for the one
 * containing point (800A5A48 gives -1); -1 for none. */
s32 func_800A5D54(SVector *point, s32 triangle, s32 depth) {
    s32 found;

    if (triangle >= 0) {
        if (D_800D39CC[triangle].visited != D_800D2F64) {
            D_800D39CC[triangle].visited = D_800D2F64;
            if (func_800A5A48(&D_800D3344[D_800D39CC[triangle].vertices[0]],
                              &D_800D3344[D_800D39CC[triangle].vertices[1]],
                              &D_800D3344[D_800D39CC[triangle].vertices[2]], point)
                == -1) {
                return triangle;
            }
            if (depth > 0) {
                found = func_800A5D54(point, D_800D39CC[triangle].neighbours[0], depth - 1);
                if (found >= 0) {
                    return found;
                }
                found = func_800A5D54(point, D_800D39CC[triangle].neighbours[1], depth - 1);
                if (found >= 0) {
                    return found;
                }
                found = func_800A5D54(point, D_800D39CC[triangle].neighbours[2], depth - 1);
                if (found >= 0) {
                    return found;
                }
            }
        }
    }
    return -1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A5D54);
#endif

/* Set the two words D_800D2D40 and D_800D2D48. */
void func_800A5E9C(s32 first, s32 second) {
    D_800D2D40 = first;
    D_800D2D48 = second;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A5EB4);

/* Set light slot index (0-3) to a color and two values; a negative red turns
 * it off. */
void func_800A6444(s32 index, s32 r, s32 g, s32 b, s32 field4, s32 field5) {
    if (index < 4) {
        if (r >= 0) {
            D_800D3611 = 1;
            D_800C3AAC[index].active = 1;
            D_800C3AAC[index].r = r;
            D_800C3AAC[index].g = g;
            D_800C3AAC[index].b = b;
            D_800C3AAC[index].field4 = field4;
            D_800C3AAC[index].field5 = field5;
        } else {
            D_800C3AAC[index].active = 0;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A64E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A6884);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A6AE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A6F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A7064);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A7948);

/* Free a stage mesh's buffers (once). */
void func_800A8A88(StageMesh *mesh) {
    if (mesh->points != NULL) {
        func_800320E8(mesh->points);
        func_800320E8(mesh->rows[0]);
        func_800320E8(mesh->rows);
        func_800320E8(mesh->polys);
        if (mesh->keys != NULL) {
            func_800320E8(mesh->keys);
        }
        mesh->points = NULL;
    }
}

#ifdef NON_MATCHING
/* Reset the battle scene: its flags, the effect and sprite pools sized by the
 * scene data, and the object and slot tables. */
void func_800A8B0C(void) {
    s32 i;
    s32 j;
    s32 k;

    D_800C3E88 = 0;
    D_800C3CF0 = 0;
    D_800C3D6C = 0;
    D_800C3D68 = 0;
    D_800C3B7C = 0;
    D_800C3B74 = 1;
    func_800A2234(&D_800C3D0C, D_800658C8->effectCount);
    func_800A2CA4(&D_800C3D04, D_800658C8->spriteCount);
    func_800B00D0();
    for (i = 31; i >= 0; i--) {
        D_800D3368[i] = NULL;
    }
    for (j = 19; j >= 0; j--) {
        D_800C3ACC[j].value = 0;
    }
    for (k = 1; k >= 0; k--) {
        D_800D3304[k].active = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A8B0C);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A8BF0);

/* Read the files of combatant slot's gear from directory 0x28 (D_800C3508)
 * into new buffers, listed in D_800C3B78: base + 1, base + 2 and, when the
 * gear's variant is within the gear's count, base + 2 + variant. */
void func_800A9540(s32 slot) {
    s32 saved0;
    s32 saved1;
    u8 gearId;
    s32 variant;
    DiscFile *entry;
    DiscFile *files;
    s32 base;
    s32 file;

    func_800284B4(&saved0, &saved1);
    func_80028470(0x28, 1);
    func_80032498(4, 0);
    gearId = D_800CCCE8.records[slot].pilot.gearId;
    variant = D_800CCCE8.records[slot].gear.fileVariant;
    if (D_800C3508[gearId * 2 + 1] < variant) {
        variant = 0;
    }
    files = func_80031BDC(sizeof(DiscFile) * 4, 1);
    D_800C3B78 = (SoundBanks *)files;
    base = D_800C3508[gearId * 2];
    file = base + 1;
    entry = files;
    entry->file = file;
    entry->data = func_80031BDC(func_800288EC(file), 1);
    entry++;
    file = base + 2;
    entry->file = file;
    entry->data = func_80031BDC(func_800288EC(file), 0);
    entry++;
    if (variant != 0) {
        file += variant;
        entry->file = file;
        entry->data = func_80031BDC(func_800288EC(file), 1);
        entry++;
    }
    entry->file = 0;
    entry->data = NULL;
    func_80029AFC(D_800C3B78, 0, 0);
    func_80028470(saved0, saved1);
}

/* Load the battle's sound banks for set: banks 2 * set + 1 and 2 * set + 2,
 * each with a buffer of its size (800288EC), into a new record D_800C3B78. */
void func_800A96B4(s32 set) {
    s32 saved0;
    s32 saved1;
    SoundBanks *banks;
    s32 bank;

    func_800284B4(&saved0, &saved1);
    func_80028470(0x28, 0);
    func_80032498(4, 0);
    banks = func_80031BDC(sizeof(SoundBanks), 1);
    set *= 2;
    bank = set + 1;
    D_800C3B78 = banks;
    func_80028998(bank);
    banks->bank0 = bank;
    banks->data0 = func_80031BDC(func_800288EC(bank), 1);
    bank = set + 2;
    banks->bank1 = bank;
    banks->data1 = func_80031BDC(func_800288EC(bank), 1);
    banks->field10 = 0;
    banks->field14 = 0;
    func_80029AFC(D_800C3B78, 0, 0);
    func_80028470(saved0, saved1);
}

#ifdef NON_MATCHING
/* Create stage object index from the gear files read by 800A9540 (its model
 * file and images) at x, y, z, facing angle; with a variant file, also its
 * extra parts as objects 2 * index + 13 + k attached to parts of the gear,
 * then free the files. Nonmatching: GCC hoists index * 2 + 13 out of the
 * loop and spills angle instead of y and z. */
void func_800A979C(s32 index, s16 x, s16 y, s16 z, s16 angle) {
    GearPartFile *parts;
    s16 *entry;
    s32 count;
    s32 size;
    void *model;
    s32 k;
    s32 flags;
    s32 slot;

    func_800A8BF0(index, 0, D_800C3B78->data1, D_800C3B78->data0, x, y, z, angle, NULL);
    D_800D3368[index]->field38 = 1;
    D_800D3368[index]->field22 = 1;
    parts = (GearPartFile *)D_800C3B78->field14;
    if (parts != NULL) {
        func_8003342C(parts);
        entry = parts->table;
        count = *entry;
        entry += 2;
        if (count != 0) {
            size = parts->end - parts->model;
            model = func_80031BDC(size, 0);
            memcpy(model, parts->model, size);
            for (k = 0; k < count; k++) {
                flags = 7;
                if (k == 0) {
                    flags = 2;
                }
                if (k == count - 1) {
                    flags -= 2;
                }
                slot = index * 2 + (k + 13);
                func_800A8BF0(slot, flags, model, parts->end, x, y, z, angle, NULL);
                D_800D3368[slot]->parentPart = *entry++;
                D_800D3368[slot]->field5C = index;
                D_800D3368[slot]->field5D = 2;
                D_800D3368[slot]->field36 = 1;
                D_800D3368[slot]->offset2[0] = *entry++;
                D_800D3368[slot]->offset2[1] = *entry++;
                D_800D3368[slot]->offset2[2] = *entry++;
            }
        } else {
            func_8002DDE4(parts->model, 1, x, y, 1, z, angle);
        }
        func_800320E8(parts);
    }
    func_800320E8(D_800C3B78);
    DrawSync(0);
    func_800320E8(D_800C3B78->data0);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800A979C);
#endif

/* Run the stage for the elapsed frames (two frames per step, at most three
 * steps): advance the waves and the highlight pulse, push the acting object
 * and the objects it overlaps apart (800B10EC), animate the objects, run the
 * effects, attach the child objects and draw the objects (the highlighted
 * slots in the pulse colour) and the sprites. */
void func_800A9A50(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    u8 pulse[3];
    s32 steps;
    s32 i;
    s32 index;
    BattleObject **others;
    BattleObject **objects;
    BattleObject *object;
    ModelPart *self;
    ModelPart *other;
    s32 extent;

    D_800C3E88 += 1 + D_800CCC5C;
    steps = 0;
    if (D_800C3E88 > 6) {
        D_800C3E88 = 6;
    }
    while (D_800C3E88 >= 2) {
        D_800C3E88 -= 2;
        steps++;
    }
    D_800C3CF0 += steps * 56;
    D_800D39E8 = (func_8003F8CC(D_800C3CF0) + 0x1000) / 800 + 4;
    D_800C3B7C += 0x80;
    D_800C3B80 = (func_8003F8CC(D_800C3B7C) + 0x1000) / 32;
    pulse[0] = func_800AA514(D_800C3B80, 32, D_800658C8->ambient[0]);
    pulse[1] = func_800AA514(D_800C3B80, 32, D_800658C8->ambient[1]);
    pulse[2] = func_800AA514(D_800C3B80, 32, D_800658C8->ambient[2]);
    if (D_800D3368[D_800C3D40] != NULL && !(D_800D3368[D_800C3D40]->flags4A & 0x20)) {
        for (index = 0, others = D_800D3368; index < 11; index++, others++) {
            if (*others != NULL && D_800C3D40 != index && (*others)->field5C == 0xFF &&
                !((D_800C3E30 >> index) & 1) && (*others)->active) {
                self = D_800D3368[D_800C3D40]->hierarchy;
                extent = func_800AA600(index);
                other = (*others)->hierarchy;
                if (other->translation[1] - extent < self->translation[1]) {
                    extent = func_800AA600(D_800C3D40);
                    if (D_800D3368[D_800C3D40]->hierarchy->translation[1] - extent < other->translation[1]) {
                        extent = func_800AA650(index);
                        extent += func_800AA650(D_800C3D40);
                        func_800B10EC(D_800C3D40, (*others)->hierarchy->translation[0],
                                      (*others)->hierarchy->translation[2], extent);
                    }
                }
            }
        }
        func_800A4CF8(D_800C3D40);
    }
    for (i = 0, objects = D_800D3368; i < 32; i++, objects++) {
        if (*objects != NULL) {
            func_800AAA20(*objects, &D_800C3D0C, steps, arg3, D_800CCC5C);
        }
    }
    if (D_800C3DF8 != 0) {
        func_800B026C(&D_800C3D0C, steps, 0, arg3);
    }
    for (i = 0, objects = D_800D3368; i < 32; i++) {
        object = *objects++;
        if (object != NULL && object->field5C < 0xFF) {
            func_800AAB34(object);
        }
    }
    objects = D_800D3368;
    func_800A44C0(objects);
    SetColorMatrix(D_800D2FC0);
    if (D_800C3B74 != 0) {
        for (i = 0; i < 31; i++, objects++) {
            if (*objects != NULL) {
                if ((D_800C3D14 >> i) & 1) {
                    SetBackColor(pulse[0], pulse[1], pulse[2]);
                } else {
                    SetBackColor(D_800658C8->ambient[0], D_800658C8->ambient[1], D_800658C8->ambient[2]);
                }
                if ((*objects)->flags4A & 0x40) {
                    D_80050104 = 0;
                } else {
                    D_80050104 = 1;
                }
                func_8009F844(*objects, arg0, arg1, 1, D_800CCC5C, arg2, arg3);
                D_80050104 = 0;
            }
        }
    }
    func_800A2FD8(&D_800C3D04, arg0, steps, arg2, arg3);
}

/* Free the stage objects (800A9FF0), the effect pool and the sprite pool. */
void func_800A9F94(void) {
    s32 i;

    for (i = 0; i < 31; i++) {
        func_800A9FF0(i);
    }
    func_800A22A8(&D_800C3D0C);
    func_800A2D1C(&D_800C3D04);
}

/* Free stage object index: its images and model files, its hierarchy and
 * attached effects (or, for the gear part objects 19-30, the hierarchy
 * block), its image animations and meshes and the object; for a party slot
 * (0-2) also its part objects 2 * index + 13 and + 14. */
void func_800A9FF0(s32 index) {
    BattleObject **objects = D_800D3368;
    BattleObject **slot = &objects[index];
    s32 i;

    if (*slot != NULL) {
        if ((*slot)->imageFile != NULL) {
            func_800320E8((*slot)->imageFile);
            func_8009F794((*slot)->field0, 1);
        }
        if ((*slot)->ownSounds) {
            func_8003852C((*slot)->model->sounds);
        }
        if ((*slot)->modelFile != NULL) {
            func_800320E8((*slot)->modelFile);
        }
        if ((u32)(index - 19) >= 12) {
            if ((*slot)->hierarchy != NULL) {
                func_800A2ACC(&D_800C3D0C, (*slot)->hierarchy);
                func_800A2BB8(&D_800C3D0C, (*slot)->hierarchy, 0xFF);
                func_8009F708((*slot)->hierarchy);
                (*slot)->field0 = NULL;
                (*slot)->hierarchy = NULL;
            }
            func_800B0060(*slot);
        } else if ((*slot)->hierarchy != NULL) {
            func_800A2ACC(&D_800C3D0C, (*slot)->hierarchy);
            func_800A2BB8(&D_800C3D0C, (*slot)->hierarchy, 0xFF);
            func_800320E8((*slot)->hierarchy);
        }
        if (D_800D3368[index]->channelCount) {
            func_800320E8(D_800D3368[index]->channels);
        }
        if (D_800D3368[index]->imageAnimCount != 0) {
            for (i = 0; i < D_800D3368[index]->imageAnimCount; i++) {
                func_800A429C(D_800D3368[index]->imageAnims + i * 0x30);
            }
            func_800320E8(D_800D3368[index]->imageAnims);
        }
        if (D_800D3368[index]->meshCount != 0) {
            for (i = 0; i < D_800D3368[index]->meshCount; i++) {
                func_800A8A88(&D_800D3368[index]->meshes[i]);
            }
            func_800320E8(D_800D3368[index]->meshes);
        }
        func_800320E8(D_800D3368[index]);
        D_800D3368[index] = NULL;
    }
    if (index < 3) {
        func_800A9FF0(index * 2 + 13);
        func_800A9FF0(index * 2 + 14);
    }
}

/* Select stage object index with slot mask, and start its effect (800AA934). */
void func_800AA320(u16 index, s16 mask, s32 arg2) {
    BattleObject *object = D_800D3368[index];

    D_800C3D40 = index;
    D_800C3E30 = mask;
    object->field35 = 0;
    if (D_800D3368[index] != NULL) {
        func_800AA934(D_800D3368[index], D_800D3368[index], &D_800C3D0C, arg2);
    }
}

/* Make stage object index the acting object with slot mask: mark it, reset
 * the camera state (800BF85C) for the first selected slot, and start its
 * effect (800AA934). */
void func_800AA384(u16 index, u16 mask, s32 arg2) {
    BattleObject *object;

    D_800C3D40 = index;
    object = D_800D3368[index];
    D_800C3E30 = mask;
    D_800C3D68 = 1;
    object->field35 = 1;
    D_80059464 = 0;
    D_800591AC = 1;
    func_800BF85C(index, func_800AF400());
    D_800C360C = 1;
    D_800C4000[func_800AF400()] = 0;
    if (D_800D3368[index] != NULL) {
        func_800AA934(D_800D3368[index], D_800D3368[index], &D_800C3D0C, arg2);
    }
}

/* Select stage object index with slot mask and start its effect (800AA934)
 * unless it is the first selected slot's object; the selection is restored. */
void func_800AA454(u16 index, u16 mask, s32 arg2) {
    u16 savedIndex = D_800C3D40;
    u16 savedMask = D_800C3E30;
    BattleObject *object = D_800D3368[index];

    D_800C3D40 = index;
    D_800C3E30 = mask;
    object->field35 = 0;
    if (D_800D3368[index] != NULL && index != func_800AF400()) {
        func_800AA934(D_800D3368[index], D_800D3368[index], &D_800C3D0C, arg2);
    }
    D_800C3D40 = savedIndex;
    D_800C3E30 = savedMask;
}

/* c plus a * b / 256, capped at 255. */
u8 func_800AA514(s16 a, s16 b, s32 c) {
    s16 value = c + b * a / 256;

    if (value > 255) {
        value = 255;
    }
    return value;
}

/* Select stage object index with slot mask and start its effect on target
 * (800AA934) unless it is the first selected slot's object. */
void func_800AA564(BattleObject *target, u16 index, u16 mask, s32 arg3) {
    BattleObject *object = D_800D3368[index];

    D_800C3D40 = index;
    D_800C3E30 = mask;
    object->field35 = 0;
    if (D_800D3368[index] != NULL && index != func_800AF400()) {
        func_800AA934(D_800D3368[index], target, &D_800C3D0C, arg3);
    }
}

/* The scaled size of stage object index (0 when absent). */
s32 func_800AA600(s32 index) {
    BattleObject *object = D_800D3368[index];
    s32 size = 0;

    if (object != NULL) {
        size = object->scale24 * (object->scale1C * object->hierarchy->scale[1] >> 12) >> 12;
    }
    return size;
}

/* The scaled size of stage object index along its hierarchy's first axis
 * (flag 8) or its third; 0 when absent. */
s32 func_800AA650(s32 index) {
    BattleObject *object = D_800D3368[index];
    s32 size = 0;
    s32 scale;
    s32 axis;

    if (object != NULL) {
        if (object->flags4A & 8) {
            scale = object->scale26;
            axis = object->hierarchy->scale[0];
        } else {
            scale = object->scale28;
            axis = object->hierarchy->scale[2];
        }
        size = scale * (D_800D3368[index]->scale1C * axis >> 12) >> 12;
    }
    return size;
}

/* Allocate object's script effect channels, all idle. */
void func_800AA6E0(BattleObject *object) {
    ObjectChannel *channels;
    s32 i;

    if (object->channelCount != 0) {
        channels = func_80031BDC(object->channelCount * sizeof(ObjectChannel), 0);
        for (i = 0; i < object->channelCount; i++) {
            channels[i].id = -1;
            channels[i].data = NULL;
        }
        object->channels = channels;
    }
}

/* Set stage object index's byte 0x2A, when it exists. */
void func_800AA760(s32 index, u8 value) {
    if (D_800D3368[index] != NULL) {
        D_800D3368[index]->field2A = value;
    }
}

/* Set the flag D_800C3B74 to the low bit of value. */
void func_800AA788(s32 value) {
    D_800C3B74 = value & 1;
}

/* Swap stage objects a and b, deactivating a and activating b first. */
void func_800AA79C(s32 a, s32 b) {
    BattleObject **objects = D_800D3368;
    BattleObject **first = &objects[a];
    BattleObject **second = &objects[b];
    BattleObject *swap;

    (*first)->active = 0;
    (*second)->active = 1;
    swap = *first;
    *first = *second;
    *second = swap;
}

/* The type of the first event from index on that is not a continuation
 * (0xF7); 0xFE for the end (0xFF). */
u8 func_800AA7DC(s32 index) {
    u8 type;

    do {
        type = D_800C3FE8[index++].type;
    } while (type == 0xF7);
    if (type == 0xFF) {
        type = 0xFE;
    }
    return type;
}

/* The effect step handler for mode (1-3; any other the default). */
void *func_800AA820(s32 mode) {
    switch (mode) {
    case 1:
        return func_800A3514;
    case 2:
        return func_800A3578;
    case 3:
        return func_800A35C8;
    default:
        return func_800A3490;
    }
}

#ifdef NON_MATCHING
/* Reset a battle object's state. */
void func_800AA898(BattleObject *object, EffectPool *pool, u8 **scripts, u8 **animations) {
    object->field3C = 0xFFFF;
    object->field5C = 0xFF;
    object->field39 = 0x6B;
    object->scripts = scripts;
    object->extra = NULL;
    object->script = NULL;
    object->animations = animations;
    object->moreAnimations = NULL;
    object->queueCount = 0;
    object->animation = -1;
    object->field58 = 0;
    object->field35 = 0;
    object->field37 = 0;
    object->field38 = 0;
    object->field3A = -1;
    object->motion[0] = 0;
    object->motion[1] = 0;
    object->motion[2] = 0;
    object->motion[3] = 0;
    object->motion[4] = 0;
    object->motion[5] = 0;
    object->motion[6] = 0;
    object->motion[7] = 0;
    object->motion[8] = 0;
    object->motion[9] = 0;
    object->motion[10] = 0;
    object->motion[11] = 0;
    object->position[0] = 0;
    object->position[1] = 0;
    object->position[2] = 0;
    object->field8E = 1;
    object->field36 = 0;
    object->field1E = -1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800AA898);
#endif

/* Start target's effect script id on object (ids from 0x50 come from the
 * target's extra file), or queue it (up to five) while one is running. */
void func_800AA934(BattleObject *object, BattleObject *target, EffectPool *pool, s32 id) {
    if (object != NULL && target != NULL) {
        if (object->queueCount != 0) {
            if (object->queueCount < 5) {
                object->queueCount++;
            }
            object->queueTargets[object->queueCount - 2] = target->slot;
            object->queueScripts[object->queueCount - 2] = id;
            return;
        }
        if (id < 0x50) {
            object->script = target->scripts[id];
        } else {
            object->script = target->extra->scripts[id - 0x4E];
        }
        object->field42 = 0;
        object->scriptWait = 0;
        object->field50 = 0;
        object->field54 = 0;
        object->field4C = 0;
        object->slotMask = D_800C3E30;
        object->field23 = 0;
        func_800AAD54(object, pool, -1, 1, 0);
    }
}

/* Update a battle object for steps frames: put it on the ground, pose its
 * hierarchy (per-part scales when field37 is set) and animate it (800A0838,
 * 800AE2A4) each step, then run its effects (800AAD54). Returns the combined
 * animation flags. */
s32 func_800AAA20(BattleObject *object, EffectPool *pool, s32 steps, s32 arg3, s32 arg4) {
    s32 flags;
    s32 i;

    if (object->field0 != 0) {
        flags = 0;
        if (object->active) {
            func_800AFF9C(object);
            if (object->field37) {
                func_8009F1C4(object->hierarchy, object->scale1C);
            } else {
                func_8009EF3C(object->hierarchy, object->scale1C);
            }
            for (i = 0; i < steps; i++) {
                flags |= func_800A0838((ModelList *)pool, object->hierarchy, object->field3C, object->scale1C);
                func_800AE2A4(object, pool, arg3);
            }
        }
        func_800AAD54(object, pool, flags, steps, arg4);
    }
    return flags;
}

/* Carry a battle object along with its parent object (index field5C; it
 * becomes 0xFF once the parent is gone): take its active state unless flag
 * 0x10, turn it with the parent (or a part of the parent) when field5D is set,
 * and place its hierarchy's root at its offset from there. */
void func_800AAB34(BattleObject *object) {
    Matrix *m = (Matrix *)0x1F800000;
    SVector offset;

    if (D_800D3368[object->field5C] != NULL) {
        if (!(object->flags4A & 0x10)) {
            object->active = D_800D3368[object->field5C]->active;
        }
        if (object->active) {
            if (object->field5D) {
                if (object->parentPart != 0) {
                    MulMatrix0(&D_800D3368[object->field5C]->hierarchy->world,
                                  &D_800D3368[object->field5C]->hierarchy[object->parentPart].world, m);
                } else {
                    m = &D_800D3368[object->field5C]->hierarchy->world;
                }
                func_80049BDC(m, &object->hierarchy->transform);
                func_80049BDC(m, &object->hierarchy->world);
            }
            if (object->parentPart != 0) {
                CompMatrix(&D_800D3368[object->field5C]->hierarchy->transform,
                              &D_800D3368[object->field5C]->hierarchy[object->parentPart].world, m);
            } else {
                m = &D_800D3368[object->field5C]->hierarchy->transform;
            }
            SetRotMatrix(m);
            SetTransMatrix(m);
            offset.vx = object->offset2[0];
            offset.vy = object->offset2[1];
            offset.vz = object->offset2[2];
            gte_ldv0(&offset);
            gte_rtv0tr();
            gte_stlvnl(object->hierarchy->transform.t);
            gte_stlvnl(object->hierarchy->translation);
        }
    } else {
        object->field5C = 0xFF;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800AAD54);

/* Turn a part to rotation (x, y, z): at once for a duration below 2, else by
 * a turning effect (kind 0xFE) over duration frames along the shortest way
 * (x, y, z become the turns). */
void func_800ADF1C(EffectPool *pool, ModelPart *part, s32 duration, s32 x, s32 y, s32 z) {
    EffectEntry *entry;

    if (duration < 2) {
        part->rotation.vx = x;
        part->rotation.vy = y;
        part->rotation.vz = z;
        part->flag5 = 1;
        return;
    }
    if (part->rotation.vx != x || part->rotation.vy != y || part->rotation.vz != z) {
        if (part->effects[0] != NULL) {
            entry = part->effects[0];
        } else {
            entry = func_800A2330(pool);
        }
        if (entry != NULL) {
            entry->used = 1;
            entry->field2 = 3;
            entry->field1 = 0;
            entry->kind = 0xFE;
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
            entry->field10 = 0;
            entry->field12 = duration;
            part->effects[0] = entry;
        }
    }
}

/* Attach a travelling effect (kind 0xFE) to a part: from its translation to
 * (x, y, z), over a length of its distance plus one. */
void func_800AE098(EffectPool *pool, ModelPart *part, s32 type, s32 param1, s32 param2, s32 field12, s32 x,
                   s32 y, s32 z) {
    EffectEntry *entry;
    s32 dx;
    s32 dy;
    s32 dz;

    if (part->effects[0] != NULL) {
        entry = part->effects[0];
    } else {
        entry = func_800A2330(pool);
    }
    if (entry != NULL) {
        entry->used = 1;
        entry->field2 = type + 7;
        entry->field1 = 0;
        entry->kind = 0xFE;
        dx = x - part->translation[0];
        dy = y - part->translation[1];
        dz = z - part->translation[2];
        entry->params[0] = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
        entry->params[1] = param1;
        entry->params[2] = param2;
        entry->params[3] = x;
        entry->params[4] = y;
        entry->params[5] = z;
        entry->field10 = 0;
        entry->field12 = field12;
        part->effects[0] = entry;
    }
}

/* Start an animation on a battle object (looping when loop is set); an empty
 * animation stops it. */
void func_800AE1BC(BattleObject *object, Animation *animation, s32 loop) {
    u8 *data;

    if (animation->length != 0) {
        object->animation = 0;
        if (loop) {
            object->animationLoop = animation->loop;
        } else {
            object->animationLoop = -1;
        }
        object->animationFrame = 0;
        object->animationLength = animation->length;
        data = (u8 *)animation + animation->dataOffset;
        object->animationStart = data;
        object->animationCursor = data;
    } else {
        object->animation = -1;
    }
}

/* The sound bank id (in the high half) of source: 0 the system bank, 1 the
 * object's model data, 2 its extra data, 3 the bank D_800C4924. */
s32 func_800AE220(BattleObject *object, s32 source) {
    if (source == 0) {
        return D_8005919C->bank << 16;
    }
    if (source == 1) {
        return object->model->sounds->bank << 16;
    }
    if (source == 2) {
        return object->extraData->sounds->bank << 16;
    }
    if (source == 3) {
        return D_800C4924->bank << 16;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800AE2A4);

/* Stop a battle object's animation. */
void func_800AEEEC(BattleObject *object) {
    object->animation = -1;
}

/* Distance from a battle object's position to its hierarchy's translation. */
s32 func_800AEEF8(BattleObject *object) {
    ModelPart *root = object->hierarchy;
    s32 dx = object->position[0] - root->translation[0];
    s32 dy = object->position[1] - root->translation[1];
    s32 dz = object->position[2] - root->translation[2];

    return SquareRoot0(dx * dx + dy * dy + dz * dz);
}

/* Place a battle object at its offset from its target (code 0xFF the first
 * slot of its mask, 0xFE the selected object, 0xFD/0xFC its own slots, 0xFA
 * slot 31, 1-127 the slot below): through the target's hierarchy (or a part
 * of it) when the target exists and is not the object's own slot, carrying a
 * following effect along; otherwise at the slot's battle position. */
void func_800AEF68(BattleObject *object) {
    s32 slot = 0;
    BattleObject *target;
    Matrix *m;
    Vector position;
    EffectEntry *entry;

    if (object->field58 == 0xFF) {
        for (slot = 0; slot < 13; slot++) {
            if ((object->slotMask >> slot) & 1) {
                break;
            }
        }
    }
    if (object->field58 == 0xFE) {
        slot = D_800C3D40;
    }
    if (object->field58 == 0xFD) {
        slot = object->slot;
    }
    if (object->field58 == 0xFC) {
        slot = object->slot2;
    }
    if (object->field58 == 0xFA) {
        slot = 31;
    }
    if (object->field58 > 0 && object->field58 < 0x80) {
        slot = object->field58 - 1;
    }
    target = D_800D3368[slot];
    if (target != NULL && slot != object->slot) {
        m = (Matrix *)0x1F800000;
        if (object->targetPart != 0) {
            CompMatrix(&target->hierarchy->transform, &target->hierarchy[object->targetPart].world, m);
        } else {
            m = &target->hierarchy->transform;
        }
        SetRotMatrix(m);
        SetTransMatrix(m);
        gte_ldv0(object->offset);
        gte_rtv0tr();
        gte_stlvnl(&position);
        object->position[0] = position.vx;
        object->position[1] = position.vy;
        object->position[2] = position.vz;
        entry = object->hierarchy->effects[0];
        if (entry != NULL && (u32)(entry->field2 - 7) < 2) {
            entry->params[3] = position.vx;
            entry->params[4] = position.vy;
            entry->params[5] = position.vz;
        }
    } else {
        object->position[0] = D_800C3EB4[slot].x;
        object->position[1] = D_800C3EB4[slot].y;
        object->position[2] = D_800C3EB4[slot].z;
    }
}

#ifdef NON_MATCHING
/* Detach part index of a hierarchy and its descendants: move their marks to
 * the same parts of another hierarchy and release their effects. */
void func_800AF180(EffectPool *pool, s32 index, ModelPart *from, ModelPart *to) {
    ModelPart *part = from + index;
    s32 count = from->index;
    ModelPart *child;
    s32 i;

    part->flag7 = 0;
    to[index].flag7 = 1;
    func_800A23E8(pool, part->effects[0]);
    part->effects[0] = NULL;
    func_800A23E8(pool, part->effects[1]);
    part->effects[1] = NULL;
    func_800A23E8(pool, part->effects[2]);
    part->effects[2] = NULL;
    child = from;
    for (i = 1; i < count; i++) {
        child++;
        if (child->parent == part) {
            func_800AF180(pool, child->index, from, to);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800AF180);
#endif

/* Move the parts' marks (flag7) of a hierarchy to another of the same
 * shape. */
void func_800AF270(ModelPart *from, ModelPart *to) {
    s32 count = from->index;
    s32 i;

    for (i = 1; i < count; i++) {
        from++;
        to++;
        if (from->flag7) {
            from->flag7 = 0;
            to->flag7 = 1;
        }
    }
}

/* The dot product of direction with the unit normal (the cross product) of
 * a and b, in 4.12 through 16 << 8 / its length, divided by scale. */
s16 func_800AF2C4(Vector *direction, Vector *a, Vector *b, s32 scale) {
    Vector normal;
    s32 dot;
    s32 length;

    func_8004A480(a, b, &normal);
    dot = normal.vx * direction->vx + normal.vy * direction->vy + normal.vz * direction->vz;
    length = SquareRoot0(normal.vx * normal.vx + normal.vy * normal.vy + normal.vz * normal.vz) + 1;
    return (dot * 16 / length << 8) / scale;
}

/* The lowest slot (0-12) set in the mask D_800C3E30; 13 when none. */
s32 func_800AF400(void) {
    s32 slot;

    for (slot = 0; slot < 13; slot++) {
        if ((D_800C3E30 >> slot) & 1) {
            break;
        }
    }
    return slot;
}

#ifdef NON_MATCHING
/* The slot mask of a target code: 0xFF the first selected slot, 0xFE the
 * selected object, 0xFD/0xF9 and 0xFC the object's slots, 0xFA slot 31, 0xF8
 * and 0xF7 slots derived from the object's slot; any other code is a slot. */
void func_800AF438(BattleObject *object, s32 code, u16 *mask) {
    u8 slot = code;

    if (slot == 0xFF) {
        slot = func_800AF400();
    } else if (slot == 0xFE) {
        slot = D_800C3D40;
    } else if (slot == 0xFD || slot == 0xF9) {
        slot = object->slot;
    } else if (slot == 0xFC) {
        slot = object->slot2;
    } else if (slot == 0xFA) {
        slot = 31;
    } else if (slot == 0xF8) {
        slot = object->slot * 2 + 13;
    } else if (slot == 0xF7) {
        slot = object->slot * 2 + 14;
    }
    *mask = 1 << slot;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800AF438);
#endif

/* The animation of a battle object for index; 0xFE repeats the object's
 * current one and 0xFF chooses it from the slot's gear warnings (0x1B, 6 or
 * 1; bit 0x80 when the gear status is on), reporting that bit in flag. */
u8 *func_800AF518(BattleObject *object, u8 index, s32 *flag) {
    s32 warnings;

    *flag = 0;
    if (index >= 0xFE) {
        if (index == 0xFF) {
            warnings = func_8009C050(object->slot);
            if ((warnings & 4) && (object->flags4A & 0x100)) {
                object->field2A = 0x1B;
            } else if ((warnings & 2) && (object->flags4A & 0x80)) {
                if (!(D_800CCCE8.records[object->slot].pilot.flags36 & 1)) {
                    object->field2A = 6;
                }
            } else if (!(object->flags4A & 0x400)) {
                object->field2A = 1;
            }
            if (warnings & 1) {
                object->field2A |= 0x80;
            }
        }
        index = object->field2A & 0x7F;
        *flag = object->field2A & 0x80;
    }
    if (index < 0x40) {
        return object->animations[index + 1];
    }
    return object->moreAnimations[index - 0x3F];
}

/* Start effect channel flags & 7 (0 rotation, 1 translation, 2 scale) on
 * part: from start to end over duration, each relative to the part's current
 * values with flag 0x20 (start) or 0x40 (end); mode 0 keeps the end as a
 * difference and modes 0 and 1 put the part at the start at once. With flag
 * 0x80 the part's children get the same effect. */
void func_800AF678(BattleObject *object, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 kind, u8 field1,
                   s16 startX, s16 startY, s16 startZ, s16 endX, s16 endY, s16 endZ, s16 duration) {
    EffectEntry *entry;
    u8 channel = flags & 7;
    u16 baseX;
    u16 baseY;
    u16 baseZ;
    u16 offsetX;
    u16 offsetY;
    u16 offsetZ;
    ModelPart *child;
    s32 i;

    if (channel == 0) {
        entry = part->effects[0];
    } else if (channel == 1) {
        entry = part->effects[1];
    } else {
        entry = part->effects[2];
    }
    if (entry != NULL || (entry = func_800A2330(pool)) != NULL) {
        entry->used = 1;
        entry->field1 = field1;
        entry->field2 = mode + 3;
        entry->kind = kind;
        if (flags & 0x20) {
            if (channel == 0) {
                baseX = part->rotation.vx;
                baseY = part->rotation.vy;
                baseZ = part->rotation.vz;
            } else if (channel == 1) {
                baseX = part->translation[0];
                baseY = part->translation[1];
                baseZ = part->translation[2];
            } else {
                baseX = part->scale[0];
                baseY = part->scale[1];
                baseZ = part->scale[2];
            }
        } else {
            baseX = 0;
            baseY = 0;
            baseZ = 0;
        }
        if (flags & 0x40) {
            if (channel == 0) {
                offsetX = part->rotation.vx;
                offsetY = part->rotation.vy;
                offsetZ = part->rotation.vz;
            } else if (channel == 1) {
                offsetX = part->translation[0];
                offsetY = part->translation[1];
                offsetZ = part->translation[2];
            } else {
                offsetX = part->scale[0];
                offsetY = part->scale[1];
                offsetZ = part->scale[2];
            }
        } else {
            offsetX = 0;
            offsetY = 0;
            offsetZ = 0;
        }
        entry->params[0] = startX + baseX;
        entry->params[1] = startY + baseY;
        entry->params[2] = startZ + baseZ;
        if (mode == 0) {
            entry->params[3] = endX + offsetX - entry->params[0];
            entry->params[4] = endY + offsetY - entry->params[1];
            entry->params[5] = endZ + offsetZ - entry->params[2];
        } else {
            entry->params[3] = endX + offsetX;
            entry->params[4] = endY + offsetY;
            entry->params[5] = endZ + offsetZ;
        }
        entry->field10 = 0;
        entry->field12 = duration;
        if (channel == 0) {
            if (mode < 2) {
                part->rotation.vx = entry->params[0];
                part->rotation.vy = entry->params[1];
                part->rotation.vz = entry->params[2];
            }
            part->effects[0] = entry;
        } else if (channel == 1) {
            if (mode < 2) {
                part->translation[0] = (s16)entry->params[0];
                part->translation[1] = (s16)entry->params[1];
                part->translation[2] = (s16)entry->params[2];
            }
            part->effects[1] = entry;
        } else {
            if (mode < 2) {
                part->scale[0] = entry->params[0];
                part->scale[1] = entry->params[1];
                part->scale[2] = entry->params[2];
            }
            part->effects[2] = entry;
        }
    }
    if (flags & 0x80) {
        child = object->hierarchy;
        for (i = 1; i < object->hierarchy->index; i++) {
            child++;
            if (child->parent == part) {
                func_800AF678(object, pool, child, flags, mode, kind, field1, startX, startY, startZ, endX, endY, endZ,
                              duration);
            }
        }
    }
}

/* Mark (flags bit 0) or unmark part of a battle object's hierarchy, and with
 * flags bit 0x80 its descendants. */
void func_800AFA98(BattleObject *object, ModelPart *part, s32 flags) {
    ModelPart *child;
    s32 i;

    part->flag7 = flags & 1;
    if (flags & 0x80) {
        child = object->hierarchy;
        for (i = 1; i < object->hierarchy->index; i++) {
            child++;
            if (child->parent == part) {
                func_800AFA98(object, child, flags);
            }
        }
    }
}

/* Create a sprite of kind from resource at position with a direction and a
 * scale; when the command says so, it follows a part of object (800AFC68). */
void func_800AFB4C(void *resource, s32 kind, SVector *position, s16 direction, s16 scale, SpriteCommand *command,
                   BattleObject *object) {
    EffectSprite *sprite;
    SpriteFollow *follow;

    sprite = func_80023FD8(kind, resource, position, sizeof(SpriteFollow));
    func_80021FE0(&sprite->x, direction);
    func_800223B0(&sprite->x, direction);
    func_80022000(&sprite->x, scale);
    follow = (SpriteFollow *)((u8 *)sprite + sprite->link);
    follow->object = object;
    follow->part = command->part;
    if (command->follow) {
        follow->update = func_8001CD7C(sprite);
        func_8001CD6C(sprite, func_800AFC68);
        follow->offset.vx = command->offset[0];
        follow->offset.vy = command->offset[1];
        follow->offset.vz = command->offset[2];
        follow->onGround = command->onGround;
    }
}

/* Update of a following sprite: place it at its offset from its object's
 * part (on the object's ground height when asked), then run its own update. */
void func_800AFC68(EffectSprite *sprite) {
    SpriteFollow *follow = (SpriteFollow *)((u8 *)sprite + sprite->link);
    Matrix *m = (Matrix *)0x1F800000;
    Vector out;

    if (follow->part != 0) {
        CompMatrix(&follow->object->hierarchy->transform, &follow->object->hierarchy[follow->part].world, m);
    } else {
        m = &follow->object->hierarchy->transform;
    }
    SetRotMatrix(m);
    SetTransMatrix(m);
    gte_ldv0(&follow->offset);
    gte_rtv0tr();
    gte_stlvnl(&out);
    if (follow->onGround) {
        out.vy = follow->object->groundY;
    }
    sprite->x = out.vx << 16;
    sprite->y = out.vy << 16;
    sprite->z = out.vz << 16;
    follow->update(sprite);
}

/* Set (or with mode bit 0x20 add to) a part's rotation (mode & 7 == 0),
 * translation (1) or scale (other) and mark it changed; with mode bit 0x80
 * also its descendants. */
void func_800AFD98(BattleObject *object, ModelPart *part, u8 mode, s16 x, s16 y, s16 z) {
    ModelPart *child;
    s32 i;

    if ((mode & 7) == 0) {
        if (mode & 0x20) {
            part->rotation.vx += x;
            part->rotation.vy += y;
            part->rotation.vz += z;
        } else {
            part->rotation.vx = x;
            part->rotation.vy = y;
            part->rotation.vz = z;
        }
    } else if ((mode & 7) == 1) {
        if (mode & 0x20) {
            part->translation[0] += x;
            part->translation[1] += y;
            part->translation[2] += z;
        } else {
            part->translation[0] = x;
            part->translation[1] = y;
            part->translation[2] = z;
        }
    } else if (mode & 0x20) {
        part->scale[0] += x;
        part->scale[1] += y;
        part->scale[2] += z;
    } else {
        part->scale[0] = x;
        part->scale[1] = y;
        part->scale[2] = z;
    }
    part->flag4 = 1;
    part->flag5 = 1;
    if (mode & 0x80) {
        child = object->hierarchy;
        for (i = 1; i < object->hierarchy->index; i++) {
            child++;
            if (child->parent == part) {
                func_800AFD98(object, child, mode, x, y, z);
            }
        }
    }
}

/* Put a battle object on the ground: find the scene triangle under its
 * hierarchy's translation and take its height (into the translation unless
 * field36 is set). */
void func_800AFF9C(BattleObject *object) {
    u8 out[16];
    SVector point;

    point.vx = object->hierarchy->translation[0];
    point.vy = object->hierarchy->translation[1];
    point.vz = object->hierarchy->translation[2];
    object->field1E = func_800A5914(&point, object->field1E, 4);
    if (object->field1E < 0) {
        object->field1E = func_800A579C(&point);
    }
    func_800A5870(&point, object->field1E, out);
    object->groundY = point.vy;
    if (object->field36 == 0) {
        object->hierarchy->translation[1] = point.vy;
    }
}

/* Free a battle object's extra file (and its sound bank when loaded). */
void func_800B0060(BattleObject *object) {
    if (object->extra != NULL) {
        if (object->extraSounds) {
            func_8003852C(object->extraData->sounds);
            object->extraSounds = 0;
        }
        func_800320E8(object->extra);
        object->extra = NULL;
        object->moreAnimations = NULL;
    }
}

/* Clear the words D_800C3BAC[0..8]. */
void func_800B00D0(void) {
    s32 i;

    for (i = 8; i >= 0; i--) {
        D_800C3BAC[i] = NULL;
    }
}

/* Release the nine effect entries of D_800C3BAC to pool, then clear them. */
void func_800B00F4(EffectPool *pool) {
    s32 i;

    for (i = 0; i < 9; i++) {
        if (D_800C3BAC[i] != NULL) {
            func_800A23E8(pool, D_800C3BAC[i]);
        }
    }
    func_800B00D0();
}

/* Start effect index (D_800C3BAC, taken from pool when unset) with its kind
 * and parameters, unless effects are disabled (D_800C37C8). */
void func_800B0164(EffectPool *pool, s32 index, u8 field2, u8 kind, u16 p0, u16 p1, u16 p2, u16 p3, u16 p4,
                   u16 p5, u16 field12) {
    EffectEntry **slots;
    EffectEntry **slot;
    EffectEntry *entry;

    if (D_800C37C8 == 0) {
        slots = D_800C3BAC;
        slot = &slots[index];
        if (*slot == NULL) {
            *slot = func_800A2330(pool);
        }
        entry = *slot;
        if (entry != NULL) {
            entry->used = 1;
            entry->field1 = 0;
            entry->field2 = field2;
            entry->kind = kind;
            entry->params[0] = p0;
            entry->params[1] = p1;
            entry->params[2] = p2;
            entry->params[3] = p3;
            entry->params[4] = p4;
            entry->params[5] = p5;
            entry->field10 = 0;
            entry->field12 = field12;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800B026C);

/* Whether a point (x at [0], z at [2]) lies strictly inside the scene's
 * bounds. */
s16 func_800B0AB4(s16 *point) {
    BattleSceneData *scene = D_800658C8;
    s16 x = point[0];
    s16 z;

    if (scene->minX < x && x < scene->maxX) {
        z = point[2];
        if (z > scene->minZ && z < scene->maxZ) {
            return 1;
        }
    }
    return 0;
}

/* The ground height 512 units in front of the camera (towards its look-at
 * point, the height difference quartered) for view key; the previous height
 * when the point is off the stage or the key is unchanged. */
s16 func_800B0B14(s32 key) {
    Vector out;
    SVector point;
    s16 dx;
    s32 dy;
    s16 dz;
    s32 length;
    s32 stepX;
    s32 stepY;
    s32 stepZ;
    s16 triangle;

    dx = D_800D335C.vx - D_800D3354.vx;
    dy = (D_800D335C.vy - D_800D3354.vy) / 4;
    dz = D_800D335C.vz - D_800D3354.vz;
    length = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
    stepX = (dx << 9) / length;
    stepY = (dy << 9) / length;
    stepZ = (dz << 9) / length;
    triangle = -1;
    point.vx = D_800D3354.vx + stepX;
    point.vy = D_800D3354.vy + stepY;
    point.vz = D_800D3354.vz + stepZ;
    if (func_800B0AB4(&point.vx) && key != D_800C3546) {
        D_800C3546 = key;
        triangle = func_800A5914(&point, D_800C3542, 5);
        if (triangle < 0) {
            triangle = func_800A579C(&point);
        }
        if (triangle >= 0) {
            D_800C3542 = triangle;
        }
    }
    if (triangle >= 0) {
        func_800A5870(&point, D_800C3542, &out);
        D_800C3544 = point.vy;
    } else {
        point.vy = D_800C3544;
    }
    return point.vy;
}

/* The party or enemy object (1 + slot, 0 none) whose footprint (its size
 * plus 0x80, centred at its position less a sixth of motion) point is inside
 * and above the foot of, the largest such; point is pushed out to its
 * edge. */
s32 func_800B0D70(SVector *motion, SVector *point) {
    s16 best = -1;
    s32 i;
    s16 radius;
    s16 bestRadius;
    s16 pushX;
    s16 pushZ;
    s16 centreX;
    s16 centreZ;
    s16 dx;
    s16 dz;
    s32 length;
    ModelPart *root;

    for (i = 0; i < 11; i++) {
        radius = func_800AA650(i) + 0x80;
        if (D_800D3368[i] != NULL && D_800D3368[i]->active) {
            if (point->vy > (root = D_800D3368[i]->hierarchy)->translation[1] - func_800AA600(i) &&
                (best < 0 || radius > bestRadius)) {
                centreX = root->translation[0] - motion->vx / 6;
                dx = point->vx - centreX;
                centreZ = root->translation[2] - motion->vz / 6;
                dz = point->vz - centreZ;
                length = SquareRoot0(dx * dx + dz * dz) + 1;
                if (length < radius) {
                    best = i;
                    bestRadius = radius;
                    pushX = centreX + dx * radius / length;
                    pushZ = centreZ + dz * radius / length;
                }
            }
        }
    }
    if (best >= 0) {
        point->vx = pushX;
        point->vz = pushZ;
        return best + 1;
    }
    return 0;
}

/* Push point out of the objects' footprints (800B0D70) moving from from:
 * the motion is the direction from from, 512 long. */
void func_800B0FF4(SVector *from, SVector *point) {
    SVector motion;
    s32 length;

    motion.vx = from->vx - point->vx;
    motion.vz = from->vz - point->vz;
    length = SquareRoot0(motion.vx * motion.vx + motion.vz * motion.vz) + 1;
    motion.vx = (motion.vx << 9) / length;
    motion.vz = (motion.vz << 9) / length;
    func_800B0D70(&motion, point);
}

/* Keep stage object index at least distance from point (x, z): when closer,
 * move it back to that distance, along the line from the point but shifted
 * sideways to the side it is facing. */
void func_800B10EC(s32 index, s32 x, s32 z, s32 distance) {
    BattleObject **objects = D_800D3368;
    BattleObject **slot = &objects[index];
    s32 dx = x - (*slot)->hierarchy->translation[0];
    s32 dz = z - (*slot)->hierarchy->translation[2];
    s32 length = SquareRoot0(dx * dx + dz * dz) + 1;
    s32 angle;
    s32 gap;

    if (length < distance) {
        angle = ratan2(dz, -dx);
        if ((u32)((((*slot)->hierarchy->rotation.vy - angle) & 0xFFF) - 0x401) < 0x7FF) {
            angle += 0x800;
        }
        gap = distance - length;
        (*slot)->hierarchy->translation[0] = x - dx * distance / length - func_8003F8B0(angle) * gap / 4096;
        (*slot)->hierarchy->translation[2] = z - dz * distance / length - func_8003F8CC(angle) * gap / 4096;
    }
}

/* Whether slot's code in the current presentation event is not allowed by
 * the kinds in mask (1: codes 0-1, 2: code 5, 4: code 4, 8: codes 2-3), or
 * mask is empty. */
s32 func_800B12D0(s32 slot, u8 mask) {
    s32 result;
    s32 allowed;

    result = 0;
    switch (D_800C3FE8[D_800C360C - 1].codes[slot]) {
    case 0:
    case 1:
        allowed = mask & 1;
        break;
    case 5:
        allowed = mask & 2;
        break;
    case 4:
        allowed = mask & 4;
        break;
    case 2:
    case 3:
        allowed = mask & 8;
        break;
    default:
        goto empty;
    }
    if (!allowed) {
        result = 1;
    }
empty:
    if (mask == 0) {
        result = 1;
    }
    return result;
}

/* Wait frames (800BE790) until no stage object is busy (field38) and 800BF6F8
 * reports nothing pending; then, when an object holds packets, stop the
 * resident transfer (8002A498) and free their packets once it is idle. */
void func_800B136C(void) {
    s32 i;
    s32 busy;
    s32 loaded = 0;

    for (;;) {
        busy = 0;
        for (i = 0; i < 11; i++) {
            if (D_800D3368[i] != NULL) {
                if (D_800D3368[i]->field38) {
                    busy = 1;
                }
                if (D_800D3368[i]->extra != NULL) {
                    loaded = 1;
                }
            }
        }
        if (func_800BF6F8() != 0) {
            busy = 1;
        }
        if (!busy) {
            break;
        }
        func_800BE790();
    }
    if (loaded) {
        func_8002A498(0);
        busy = 1;
        for (;;) {
            if (func_800286CC() == 0) {
                for (i = 0; i < 11; i++) {
                    if (D_800D3368[i] != NULL && D_800D3368[i]->extra != NULL) {
                        func_800B0060(D_800D3368[i]);
                    }
                }
                busy = 0;
            }
            if (!busy) {
                break;
            }
            func_800BE790();
        }
    }
}

/* Set the flag D_800C3D6C. */
void func_800B14B8(void) {
    D_800C3D6C = 1;
}

/* End the party members' stage objects other than keep: start their exit
 * effect (5), wait frames (800BE790) until none is active or busy and one more,
 * then free them. */
void func_800B14CC(s32 keep) {
    s32 i;
    s32 busy;

    for (i = 0; i < 3; i++) {
        if (i != keep) {
            func_800AA934(D_800D3368[i], D_800D3368[i], &D_800C3D0C, 5);
        }
    }
    do {
        busy = 0;
        for (i = 0; i < 3; i++) {
            if (i != keep && D_800D3368[i] != NULL && (D_800D3368[i]->active || D_800D3368[i]->field38)) {
                busy = 1;
            }
        }
        func_800BE790();
    } while (busy);
    func_800BE790();
    for (i = 0; i < 3; i++) {
        if (i != keep) {
            func_800A9FF0(i);
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800B15D8);

/* Address of entry index (0x1C bytes each) of a table with a 0xC-byte
 * header. */
u8 *func_800B168C(u8 *table, s32 index) {
    return table + (index * 0x1C + 0xC);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800B16A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8009E53C", func_800B16F0);
