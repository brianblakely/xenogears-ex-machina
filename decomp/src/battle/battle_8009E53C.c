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
 * 800B15D8, where the code generation changes (see battle_800B15D8.c). */
#include "common.h"
#include "psyq/abs.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/combatant.h"
#include "battle/effect.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/model.h"
#include "battle/objects.h"
#include "battle/resolver.h"
#include "battle/scene.h"
#include "battle/stage.h"
#include "battle/turn.h"
#include "battle/work.h"
#include "effect_vm.h"
#include "files.h"
#include "gte.h"
#include "own_declarations.h"
#include "resident_views.h"

/* This unit's functions, declared before their first use. */
void func_8009F708(ModelPart *root);
void func_800A22E8(EffectPool *pool);
EffectEntry *func_800A2330(EffectPool *pool);
s32 func_800A23E8(EffectPool *pool, EffectEntry *entry);
void func_800A2ACC(EffectPool *pool, ModelPart *part);
void func_800A2D5C(SpritePool *pool);
s16 func_800A3E98(ImageAnim *anim, s32 ticks);
void func_800A429C(ImageAnim *anim);
void func_800A4348(ImageAnim *anim, s16 level);
void func_800A43F8(ImageAnim *anim, s16 level);
void func_800A48EC(ModelTable *models, ModelPart *root, MATRIX *view, s32 arg3, s32 arg4, u32 *ot, s32 buffer,
                   s32 depth);
void func_800A4DB8(StageGeometry *sky, SVECTOR *eye, SVECTOR *target, MATRIX *view, u32 *ot, s32 buffer);
s32 func_800A5A48(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *point);
void func_800A5BE8(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *point, VECTOR *normal);
s32 func_800A5D54(SVECTOR *point, s32 triangle, s32 depth);
void func_800A6AE8(void);
u8 func_800AA514(s16 a, s16 b, s32 c);
s32 func_800AA650(s32 index);
void func_800AA6E0(BattleObject *object);
s32 func_800AAA20(BattleObject *object, EffectPool *pool, s32 steps, s32 arg3, s32 arg4);
void func_800AAB34(BattleObject *object);
void func_800AAD54(BattleObject *object, EffectPool *pool, s32 flags, s32 steps, s32 arg4);
void func_800ADF1C(EffectPool *pool, ModelPart *part, s32 duration, s32 x, s32 y, s32 z);
void func_800AE098(EffectPool *pool, ModelPart *part, s32 type, s32 param1, s32 param2, s32 duration, s32 x, s32 y,
                   s32 z);
void func_800AE1BC(BattleObject *object, Animation *animation, s32 loop);
s32 func_800AE220(BattleObject *object, s32 source);
void func_800AE2A4(BattleObject *object, EffectPool *pool, s32 arg2);
void func_800AEEEC(BattleObject *object);
s32 func_800AEEF8(BattleObject *object);
void func_800AEF68(BattleObject *object);
void func_800AF180(EffectPool *pool, s32 index, ModelPart *from, ModelPart *to);
void func_800AF270(ModelPart *from, ModelPart *to);
s16 func_800AF2C4(VECTOR *direction, VECTOR *a, VECTOR *b, s32 scale);
s32 func_800AF400(void);
u8 func_800AF438(BattleObject *object, u8 slot, u16 *mask); /* the slot of a target code */
u8 *func_800AF518(BattleObject *object, u8 index, s32 *flag);
void func_800AF678(BattleObject *object, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag, u8 field1,
                   s16 startX, s16 startY, s16 startZ, s16 endX, s16 endY, s16 endZ, s16 duration);
void func_800AFA98(BattleObject *object, ModelPart *part, s32 flags);
void func_800AFB4C(void *resource, s32 kind, SVECTOR *position, s16 direction, s16 scale, SpriteCommand *command,
                   BattleObject *object);
void func_800AFC68(Task *node);
void func_800AFD98(BattleObject *object, ModelPart *part, u8 mode, s16 x, s16 y, s16 z);
void func_800AFF9C(BattleObject *object);
void func_800B0060(BattleObject *object);
void func_800B00D0(void);
void func_800B00F4(EffectPool *pool);
/* Called unprototyped by the VM (its halfwords passed sign-extended). */
void func_800B0164(EffectPool *pool, s32 index, u8 mode, u8 tag, u16 p0, u16 p1, u16 p2, u16 p3, u16 p4,
                   u16 p5, u16 duration);
void func_800B026C(EffectPool *pool, s32 steps, s32 arg2, s32 arg3);
s16 func_800B0B14(s32 key);
s32 func_800B0FF4(SVECTOR *from, SVECTOR *point);
void func_800B10EC(s32 index, s32 x, s32 z, s32 distance);
s32 func_800B12D0(s32 slot, u8 mask);

/* The unit's own uninitialized variables (its .bss, after
 * battle_8008CCCC.c's), each in a slot of whole words (decomp/Makefile). */
static LightSlot D_800C3AAC[4];
static StageColors *D_800C3AC4; /* the stage's colours as loaded */
static StageColors *D_800C3AC8; /* their working copy */
static ModelTable D_800C3ACC[20]; /* the scene objects' model tables */
static s32 D_800C3B6C; /* the model list slot being filled */
static u8 *D_800C3B70; /* the model group being loaded */
static u8 D_800C3B74;
static FileRequest *D_800C3B78; /* the file list being read */
static s16 D_800C3B7C;
static s16 D_800C3B80; /* pulse level of the highlight colour */
/* The camera. */
static u8 D_800C3B84;  /* the channel tag reported in D_800C3B88 */
static u8 D_800C3B88;  /* bit 0: that channel runs, bit 1: it finished */
static u8 D_800C3B8C;  /* snap: channels 7 and 8 start at their targets */
static s16 D_800C3B90; /* orbit yaw */
static s16 D_800C3B94; /* orbit pitch */
static s16 D_800C3B98; /* orbit distance */
static s16 D_800C3B9C; /* orbit height */
static s16 D_800C3BA0; /* look-at yaw */
static s16 D_800C3BA4; /* look-at distance */
static s16 D_800C3BA8; /* look-at height */
static EffectEntry *D_800C3BAC[9];

/* Per gear: its first extra file in directory 0x28 and its variant count.
 * The last pair, 0, 0, is a twentieth gear's or the fill before D_800C3530
 * (open, docs/matching.md; ovl2143 holds a copy). */
u8 D_800C3508[] = {
    1,  0, 3,  0, 5,  6, 13, 0, 15, 3, 20, 4, 26, 0, 28, 0, 30, 0, 32, 0,
    34, 0, 36, 4, 42, 3, 47, 4, 53, 0, 55, 0, 57, 0, 59, 0, 61, 0, 0,   0,
};
u8 D_800C3530[] = {1, 108, 164, 99, 94, 220, 22, 123, 151, 158, 161, 143, 139, 141, 40, 214, 219, 0};
s16 D_800C3542 = -1;
s16 D_800C3544 = 0x7D00;
s16 D_800C3546 = -1;

/* Whether gear part 50 + index is one of the parts of character 4's gear. */
s32 func_8009E53C(u8 index) {
    BattlePart *part = &D_800C34B0->lists.parts.members[index];

    if (game_data.gears[game_data.characters[4].gearId].entries[0].id == part->id || game_data.gears[game_data.characters[4].gearId].entries[1].id == part->id) {
        return 1;
    }
    return game_data.gears[game_data.characters[4].gearId].entries[2].id == part->id;
}

/* Put battle gear part index into character 4's gear entry holding its id
 * (entry k when none does; the third entry's match selects entry 3): copy its
 * values, record the part slot, set the id's rounds from the part's +0xC (in
 * the characters' array, as 8009a854 does, not gearAmmo), and update the
 * battle copies of character 4's gear. No image calls it or 8009e53c. */
void func_8009E5C8(u8 index, u8 k) {
    BattlePart *part = &D_800C34B0->lists.parts.list[index];
    u8 gearId = game_data.characters[4].gearId;
    u8 i;

    if (game_data.gears[gearId].entries[0].id == part->id) {
        k = 0;
    }
    if (game_data.gears[gearId].entries[1].id == part->id) {
        k = 1;
    }
    if (game_data.gears[gearId].entries[2].id == part->id) {
        k = 3;
    }
    game_data.gears[gearId].entries[k].valueE = part->valueE;
    game_data.gears[gearId].entries[k].value11 = part->value11;
    game_data.gears[gearId].entries[k].value10 = part->value10;
    game_data.gears[gearId].entries[k].value11 = part->value11;
    game_data.gears[gearId].partItems[k] = index;
    game_data.ammo[index - 50] = part->rounds;
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

/* Take a round of the attacker gear's ammo for the current command (none
 * below 0): command 0 the first slot's, 2 and 17 the fourth's, 3-14 both, 15
 * the first's. */
void func_8009E788(void) {
    switch (D_800C34B0->commandIndex) {
    case 0:
        if (game_data.gearAmmo[D_800D2D6C->partItems[0] - 50] != 0) {
            game_data.gearAmmo[D_800D2D6C->partItems[0] - 50] += -1;
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
        if (game_data.gearAmmo[D_800D2D6C->partItems[0] - 50] != 0) {
            game_data.gearAmmo[D_800D2D6C->partItems[0] - 50] += -1;
        }
        if (game_data.gearAmmo[D_800D2D6C->partItems[3] - 50] != 0) {
            game_data.gearAmmo[D_800D2D6C->partItems[3] - 50] += -1;
        }
        break;
    case 15:
        if (game_data.gearAmmo[D_800D2D6C->partItems[0] - 50] != 0) {
            game_data.gearAmmo[D_800D2D6C->partItems[0] - 50] += -1;
        }
        break;
    case 2:
    case 17:
        if (game_data.gearAmmo[D_800D2D6C->partItems[3] - 50] != 0) {
            game_data.gearAmmo[D_800D2D6C->partItems[3] - 50] += -1;
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
ModelTable *func_8009EBA8(u8 *group, ModelTable *list) {
    u32 count;
    u32 i;

    heap_select_owner_tag(4, 0);
    count = model_relocate_group((ModelGroup *)group);
    list->models = heap_alloc(count * 4, 0);
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
ModelPart *func_8009EC4C(ModelTable *list, u16 *hierarchy, s32 mode, s32 offset, s16 x0, s16 y0,
                         s16 x1, s16 y1) {
    ModelPart *root;
    ModelPart *part;
    u16 *pair;
    s32 count;
    s16 index;
    u16 id;
    u16 parent;

    heap_select_owner_tag(4, 0);
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
    root = heap_alloc(count * sizeof(ModelPart), 0);
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
            model_alloc_packet_buffers(list->models[id], &part->packets[0], &part->packets[1]);
            if (part->packets[0] == NULL) {
                func_8009F708(root);
                return NULL;
            }
            if (offset) {
                model_set_tpage_override(x0, y0);
                model_set_clut_override(x1, y1);
            }
            model_build_packets(list->models[id], part->packets[0], mode);
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

/* Pose a model hierarchy: the root's rotation (YXZ order when yxz is set)
 * with its translation, scaled by scale (4.12) per axis into its transform;
 * each part to rotate (rotate) its rotation, and each dirty part (dirty, or
 * under a dirty parent) its translation and world matrix. Clears both marks
 * and returns the part count. */
u16 func_8009EF3C(ModelPart *part, s32 scale) {
    MATRIX *diagonal = (MATRIX *)0x1F800000;
    ModelPart *root = part;
    u32 count = root->index;
    u32 i;

    root->world.t[0] = root->translation[0];
    root->world.t[1] = root->translation[1];
    root->world.t[2] = root->translation[2];
    if (root->yxz) {
        RotMatrixYXZ(&root->rotation, &root->world);
    } else {
        gpu_build_rotation_matrix(&root->rotation, &root->world);
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
        if (part->rotate) {
            if (part->yxz) {
                RotMatrixYXZ(&part->rotation, &part->transform);
            } else {
                gpu_build_rotation_matrix(&part->rotation, &part->transform);
            }
            part->rotate = 0;
        }
        if (part->parent != NULL && part->parent->dirty == 1) {
            part->dirty = 1;
        }
        if (part->dirty) {
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
        root->dirty = 0;
    }
    return count;
}

/* Pose a model hierarchy with per-part scales: as 8009EF3C, but a changed
 * part's transform is scaled by its own scale and by the inverse of its
 * parent's, and a part changes or is marked with its parent. Clears the marks
 * and returns the part count. */
u16 func_8009F1C4(ModelPart *part, s32 scale) {
    MATRIX *diagonal = (MATRIX *)0x1F800000;
    MATRIX *scratch;
    ModelPart *root;
    s32 product;
    u32 count;
    u32 i;

    root = part;
    count = root->index;
    root->world.t[0] = root->translation[0];
    root->world.t[1] = root->translation[1];
    root->world.t[2] = root->translation[2];
    if (root->yxz) {
        RotMatrixYXZ(&root->rotation, &root->world);
    } else {
        gpu_build_rotation_matrix(&root->rotation, &root->world);
    }
    product = scale * part->scale[0];
    product >>= 12;
    diagonal->m[0][0] = product;
    diagonal->m[0][1] = 0;
    diagonal->m[0][2] = 0;
    diagonal->m[1][0] = 0;
    product = scale * part->scale[1];
    product >>= 12;
    diagonal->m[1][1] = product;
    diagonal->m[1][2] = 0;
    diagonal->m[2][0] = 0;
    diagonal->m[2][1] = 0;
    product = scale * part->scale[2];
    product >>= 12;
    diagonal->m[2][2] = product;
    MulMatrix0(&part->world, diagonal, &part->transform);
    part->transform.t[0] = part->world.t[0];
    part->transform.t[1] = part->world.t[1];
    part->transform.t[2] = part->world.t[2];
    for (i = 1; i < count; i++) {
        part++;
        if (part->parent != NULL) {
            if (part->parent->rotate == 1) {
                part->rotate = 1;
            }
            if (part->parent->dirty == 1) {
                part->dirty = 1;
            }
        }
        if (part->rotate) {
            scratch = (MATRIX *)0x1F800000;
            if (part->yxz) {
                RotMatrixYXZ(&part->rotation, &part->transform);
            } else {
                gpu_build_rotation_matrix(&part->rotation, &part->transform);
            }
            scratch->m[0][0] = part->scale[0];
            scratch->m[0][1] = 0;
            scratch->m[0][2] = 0;
            scratch->m[1][0] = 0;
            scratch->m[1][1] = part->scale[1];
            scratch->m[1][2] = 0;
            scratch->m[2][0] = 0;
            scratch->m[2][1] = 0;
            scratch->m[2][2] = part->scale[2];
            MulMatrix0(&part->transform, scratch, &part->transform);
            if (part->parent != NULL) {
                scratch->m[0][0] = 0x1000000 / part->parent->scale[0];
                scratch->m[0][1] = 0;
                scratch->m[0][2] = 0;
                scratch->m[1][0] = 0;
                scratch->m[1][1] = 0x1000000 / part->parent->scale[1];
                scratch->m[1][2] = 0;
                scratch->m[2][0] = 0;
                scratch->m[2][1] = 0;
                scratch->m[2][2] = 0x1000000 / part->parent->scale[2];
                MulMatrix0(scratch, &part->transform, &part->transform);
            }
        }
        if (part->dirty) {
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
        root->rotate = 0;
        root->dirty = 0;
    }
    return count;
}

/* Empty; nothing in the overlay calls it. */
void func_8009F5B0(void) {
}

/* Draw a posed hierarchy's parts into packet buffer `buffer`: each part's light
 * matrix from `light` and its world matrix, and its rotation and translation
 * composed with `view` and the root's transform, then its model (8002C700). */
void func_8009F5B8(ModelTable *list, ModelPart *part, MATRIX *view, MATRIX *light, s32 mode, u32 *ot,
                   s32 buffer) {
    MATRIX *scratch = (MATRIX *)0x1F800000;
    MATRIX *lighting = (MATRIX *)0x1F800020;
    MATRIX *camera = (MATRIX *)0x1F800040;
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
            model_draw_sprite_model(list->models[part->modelId], part->packets[buffer], ot, mode);
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
                heap_free(part->packets[0]);
                part->packets[0] = NULL;
                part->packets[1] = NULL;
            }
        }
        root->index = 0;
        heap_free(root);
    }
}

/* Free a model list's table, releasing its models first when release is
 * set. */
void func_8009F794(ModelTable *list, s32 release) {
    u32 i;

    if (list != NULL) {
        for (i = 0; i < list->count; i++) {
            if (list->models != NULL && list->models[i] != NULL && release) {
                model_free_owned_block((ModelBuffer *)list->models[i]);
            }
        }
        if (list->models != NULL) {
            heap_free(list->models);
            list->models = NULL;
        }
    }
}

/* The in-place matrix product used to scale the shadow's rotation. */
EffectSprite *func_800A2E88(SpritePool *pool, s16 abe);
void func_800A7948(Surface *surface, SVECTOR *wind, MATRIX *m, u32 *ot, s32 buffer, s32 scale, s16 floor);

/* Draw an active object: attenuate its two tracked lights, draw its ground
 * shadow and visible model parts, carry the cloth anchors/collision centres
 * with their model parts, step image animations and extend sprite trails.
 * When a trail's signed age becomes zero (including its initial -1 to 0),
 * it keeps the same channel cursor for the next iteration, matching the
 * original's conditional pointer advance. */
void func_8009F844(BattleObject *object, MATRIX *view, MATRIX *light, s32 mode, s32 skipped, u32 *ot,
                   s32 buffer) {
    u32 count;
    ModelTable *models;
    ModelPart *part;
    ModelPart *root;
    u16 scale;
    MATRIX *scratch = (MATRIX *)0x1F800000;
    MATRIX *lighting = (MATRIX *)0x1F800020;
    MATRIX *camera = (MATRIX *)0x1F800040;
    SVECTOR point;
    VECTOR forward;
    VECTOR origin;
    s32 depth;
    s32 nextDepth;
    Surface *surface;
    SurfaceEntry *entry;
    ImageAnim *image;
    ColorFade *channel;
    VECTOR *world0;
    VECTOR *world1;
    VECTOR *prev0;
    VECTOR *prev1;
    POLY_FT4 *shadow;
    s16 dx, dy, dz;
    s32 distance;
    s32 shadowScale;
    s32 index;
    s32 slot;
    s32 i;
    s32 j;

    if (object->active) {
        models = object->field0;
        part = object->hierarchy;
        scale = object->scale1C;
        root = part;
        count = part->index;
        for (i = 0; i < 2; i++) {
            if (D_800D3304[i].active != 0) {
                dx = D_800D3304[i].x - part->translation[0];
                dy = D_800D3304[i].y - ((part->translation[1] - object->scale24 * (scale * part->scale[1] >> 12)) >> 13);
                dz = D_800D3304[i].z - part->translation[2];
                distance = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
                light->m[i + 1][0] = ((dx << 12) / distance) * D_800D3304[i].active / (D_800D3304[i].active + distance);
                light->m[i + 1][1] = ((dy << 12) / distance) * D_800D3304[i].active / (D_800D3304[i].active + distance);
                light->m[i + 1][2] = ((dz << 12) / distance) * D_800D3304[i].active / (D_800D3304[i].active + distance);
            } else {
                light->m[i + 1][0] = 0;
                light->m[i + 1][1] = 0;
                light->m[i + 1][2] = 0;
            }
        }
        CompMatrix(view, &part->transform, camera);
        if (!(object->flags4A & 1) && D_800C3D6C != 0) {
            CompMatrix(&part->transform, &object->hierarchy[1].world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            point.vx = 0;
            point.vy = 0;
            point.vz = 0x1000;
            gte_ldv0(&point);
            gte_rtv0tr();
            gte_stlvnl(&forward);
            point.vx = 0;
            point.vy = 0;
            point.vz = 0;
            gte_ldv0(&point);
            gte_rtv0tr();
            gte_stlvnl(&origin);
            point.vy = -ratan2(forward.vz - origin.vz, forward.vx - origin.vx);
            point.vx = 0;
            point.vz = 0;
            gpu_build_rotation_matrix(&point, lighting);
            lighting->t[0] = origin.vx;
            lighting->t[1] = object->groundY;
            lighting->t[2] = origin.vz;
            CompMatrix(view, lighting, lighting);
            shadowScale = object->scale1C - (object->groundY - object->hierarchy->translation[1]) / 4;
            if (shadowScale < 0) {
                shadowScale = 0;
            }
            scratch->m[0][0] = shadowScale;
            scratch->m[0][1] = 0;
            scratch->m[0][2] = 0;
            scratch->m[1][0] = 0;
            scratch->m[1][1] = shadowScale;
            scratch->m[1][2] = 0;
            scratch->m[2][0] = 0;
            scratch->m[2][1] = 0;
            scratch->m[2][2] = shadowScale;
            libgte_multiply_matrix_in_place(lighting, scratch);
            SetRotMatrix(lighting);
            SetTransMatrix(lighting);
            point.vx = object->scale26;
            point.vy = 0;
            point.vz = object->scale28;
            gte_ldv0(&point);
            gte_rtps();
            shadow = &object->shadow[buffer];
            gte_stsxy(&shadow->x0);
            gte_stszotz(&depth);
            point.vx = -object->scale26;
            gte_ldv0(&point);
            gte_rtps();
            gte_stsxy(&shadow->x1);
            gte_stszotz(&nextDepth);
            if (nextDepth < depth) {
                depth = nextDepth;
            }
            point.vx = object->scale26;
            point.vz = -object->scale28;
            gte_ldv0(&point);
            gte_rtps();
            gte_stsxy(&shadow->x2);
            gte_stszotz(&nextDepth);
            if (nextDepth < depth) {
                depth = nextDepth;
            }
            point.vx = -object->scale26;
            gte_ldv0(&point);
            gte_rtps();
            gte_stsxy(&shadow->x3);
            gte_stszotz(&nextDepth);
            if (nextDepth < depth) {
                depth = nextDepth;
            }
            depth >>= model_ot_depth_shift;
            addPrim(ot + depth, &object->shadow[buffer]);
            if (depth >= 0x2D9) {
                depth = 0x2D8;
            }
            object->field39 = 0x6B - depth / 8;
        }
        MulMatrix0(light, &part->world, lighting);
        part++;
        for (i = 1; i < count; i++, part++) {
            if (part->modelId != 0xFFFF && part->visible != 0) {
                MulMatrix0(lighting, &part->world, scratch);
                SetLightMatrix(scratch);
                CompMatrix(camera, &part->world, scratch);
                if ((s16)part->field52 > 0) {
                    scratch->m[0][0] = object->scale1C;
                    scratch->m[0][2] = 0;
                    scratch->m[1][0] = 0;
                    scratch->m[1][2] = 0;
                    scratch->m[2][0] = 0;
                    scratch->m[2][2] = object->scale1C;
                    if ((s16)part->field52 == 1) {
                        scratch->m[0][1] = camera->m[0][1];
                        scratch->m[1][1] = camera->m[1][1];
                        scratch->m[2][1] = camera->m[2][1];
                    } else {
                        scratch->m[0][1] = 0;
                        scratch->m[1][1] = object->scale1C;
                        scratch->m[2][1] = 0;
                    }
                }
                SetRotMatrix(scratch);
                SetTransMatrix(scratch);
                model_draw_sprite_model(models->models[part->modelId], part->packets[buffer], ot, mode);
            }
        }
        surface = object->surfaces;
        for (i = 0; i < object->surfaceCount; surface++, i++) {
            if ((s16)surface->h0 >= 0) {
                VECTOR transformed;
                SVECTOR wind;

                wind.vx = -D_800D39E8 * gpu_get_cos(root->rotation.vy + 0x400) / 0x1000;
                wind.vz = D_800D39E8 * gpu_get_sin(root->rotation.vy + 0x400) / 0x1000;
                wind.vy = OBJECT_FIELD3E(object);
                CompMatrix(&root->transform, &root[(s16)surface->h0].world, scratch);
                SetRotMatrix(scratch);
                SetTransMatrix(scratch);
                for (j = 0; j < surface->rings; j++) {
                    gte_ldv0(&surface->centres[j]);
                    gte_rtv0tr();
                    gte_stlvnl(&transformed);
                    surface->strands[j]->pos[0] = transformed.vx;
                    surface->strands[j]->pos[1] = transformed.vy;
                    surface->strands[j]->pos[2] = transformed.vz;
                }
                entry = surface->entries;
                for (j = 0; j < surface->entryCount; j++, entry++) {
                    CompMatrix(&root->transform, &root[entry->h6].world, scratch);
                    SetRotMatrix(scratch);
                    SetTransMatrix(scratch);
                    gte_ldv0(entry);
                    gte_rtv0tr();
                    gte_stlvnl(&transformed);
                    entry->h8 = transformed.vx;
                    entry->hA = transformed.vy;
                    entry->hC = transformed.vz;
                }
                func_800A7948(surface, &wind, view, ot, buffer, scale, object->groundY);
            }
        }
        image = object->images;
        for (i = 0; i < object->imageCount; i++, image++) {
            func_800A3E98(image, skipped);
        }
        channel = object->channels;
        for (i = 0; i < object->channelCount; i++) {
            if (channel->id >= 0) {
                channel->time = (channel->time - 1) & 7;
                if (channel->solid == 0) {
                    CompMatrix(camera, &root[channel->id].world, scratch);
                    SetRotMatrix(scratch);
                    SetTransMatrix(scratch);
                    gte_ldv0(&channel->ends[0]);
                    gte_rtps();
                    gte_stsxy(&channel->history.screen.first[channel->time]);
                    gte_ldv0(&channel->ends[1]);
                    gte_rtps();
                    gte_stsxy(&channel->history.screen.second[channel->time]);
                    channel->count++;
                    if (channel->count == 0) {
                        continue;
                    }
                    if (channel->count > channel->max || channel->sprite == NULL) {
                        channel->count = 1;
                        channel->sprite = func_800A2E88(channel->pool, channel->semiTrans);
                        channel->sprite->projected = 0;
                        channel->sprite->age = 0;
                        channel->sprite->lifetime = channel->duration;
                        channel->sprite->color[0] = channel->color[0];
                        channel->sprite->color[1] = channel->color[1];
                        channel->sprite->color[2] = channel->color[2];
                        channel->sprite->fade[0] = channel->step[0];
                        channel->sprite->fade[1] = channel->step[1];
                        channel->sprite->fade[2] = channel->step[2];
                        index = (channel->time + channel->count) & 7;
                        channel->sprite->x0 = (channel->history.screen.first + index)->vx;
                        channel->sprite->y0 = (channel->history.screen.first + index)->vy;
                        channel->sprite->x2 = (channel->history.screen.second + index)->vx;
                        channel->sprite->y2 = (channel->history.screen.second + index)->vy;
                    }
                    channel->sprite->x1 = (channel->history.screen.first + channel->time)->vx;
                    channel->sprite->y1 = (channel->history.screen.first + channel->time)->vy;
                    channel->sprite->x3 = (channel->history.screen.second + channel->time)->vx;
                    channel->sprite->y3 = (channel->history.screen.second + channel->time)->vy;
                } else {
                    CompMatrix(&root->transform, &root[channel->id].world, scratch);
                    SetRotMatrix(scratch);
                    SetTransMatrix(scratch);
                    slot = channel->time & 1;
                    world0 = &channel->history.world.first[slot];
                    world1 = &channel->history.world.second[slot];
                    gte_ldv0(&channel->ends[0]);
                    gte_rtv0tr();
                    gte_stlvnl(world0);
                    gte_ldv0(&channel->ends[1]);
                    gte_rtv0tr();
                    gte_stlvnl(world1);
                    channel->count++;
                    if (channel->count == 0) {
                        continue;
                    }
                    if (channel->count > channel->max || channel->sprite == NULL) {
                        channel->count = 1;
                        channel->sprite = func_800A2E88(channel->pool, channel->semiTrans);
                        channel->sprite->projected = 1;
                        channel->sprite->age = 0;
                        channel->sprite->lifetime = channel->duration;
                        channel->sprite->color[0] = channel->color[0];
                        channel->sprite->color[1] = channel->color[1];
                        channel->sprite->color[2] = channel->color[2];
                        channel->sprite->fade[0] = channel->step[0];
                        channel->sprite->fade[1] = channel->step[1];
                        channel->sprite->fade[2] = channel->step[2];
                        prev0 = &channel->history.world.first[1 - slot];
                        prev1 = &channel->history.world.second[1 - slot];
                        channel->sprite->x0 = prev0->vx;
                        channel->sprite->y0 = prev0->vy;
                        channel->sprite->z0 = prev0->vz;
                        channel->sprite->x2 = prev1->vx;
                        channel->sprite->y2 = prev1->vy;
                        channel->sprite->z2 = prev1->vz;
                    }
                    channel->sprite->x1 = world0->vx;
                    channel->sprite->y1 = world0->vy;
                    channel->sprite->z1 = world0->vz;
                    channel->sprite->x3 = world1->vx;
                    channel->sprite->y3 = world1->vy;
                    channel->sprite->z3 = world1->vz;
                }
            }
            channel++;
        }
    }
}

/* Apply one packed delta of a tween track to dst: a signed byte added to it
 * or, after the escape byte -0x80, a signed little-endian halfword replacing
 * it (the cursor is stored before each byte is read). */
#define TRACK_DELTA(slot, dst)                                                 \
    {                                                                          \
        u8 *at = (slot)->u.track.cursor++;                                     \
        s32 byte, high;                                                        \
                                                                               \
        if ((byte = (s8)at[0]) != -0x80) {                                     \
            dst += byte;                                                       \
        } else {                                                               \
            (slot)->u.track.cursor = at + 2;                                   \
            byte = at[1];                                                      \
            (slot)->u.track.cursor = at + 3;                                   \
            high = (s8)at[2] << 8;                                             \
            dst = byte | high;                                                 \
        }                                                                      \
    }

/* Step the tweens attached to each node of a hierarchy: its rotation
 * (attachment 0: set, delta or add from a track, interpolate, approach,
 * spin, or turn toward a point within a growing limit), position
 * (attachment 1: the same, or a move in the node's frame scaled by `scale`)
 * and scale (attachment 2: interpolate, approach, spin). A finished tween is
 * released (or restarted when looping: tracks rewind, others stop). Returns
 * flags: 0x100/0x200/0x400 some tween ran/ended/looped (1/2/4 when it has
 * `tag`). As in the original, the "spin" stop clears the step of the last
 * slot a computing kind used, which may belong to an earlier node. Tag and
 * scale remain full words; packed escapes advance the cursor before reading
 * each byte. Each approach case keeps its own steps. */
s32 func_800A0838(EffectPool *pool, ModelPart *part, s32 tag, s32 scale) {
    Tween *slot;
    Tween *last;
    u8 *track;
    SVECTOR move;
    VECTOR moved;
    u32 i;
    u32 count;
    s32 result = 0;
    u8 kind;
    u8 type;
    u8 mode;
    s16 time;
    s32 dx, dy, dz, dist, limit, turn;

    count = part->index;
    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL) {
            slot = (Tween *)part->effects[0];
            kind = slot->kind;
            switch (kind & 0xF) {
            case 0:
                track = slot->u.track.cursor;
                if (!(kind & 0x10)) {
                    part->rotation.vx = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x20)) {
                    part->rotation.vy = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x40)) {
                    part->rotation.vz = *(u16 *)track;
                    slot->u.track.cursor += 2;
                }
                goto check_rot;
            case 1:
                if (!(kind & 0x10)) {
                    TRACK_DELTA(slot, part->rotation.vx);
                }
                if (!(kind & 0x20)) {
                    TRACK_DELTA(slot, part->rotation.vy);
                }
                if (!(kind & 0x40)) {
                    TRACK_DELTA(slot, part->rotation.vz);
                }
                goto check_rot;
            case 2:
                track = slot->u.track.cursor;
                if (!(kind & 0x10)) {
                    part->rotation.vx += *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x20)) {
                    part->rotation.vy += *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(kind & 0x40)) {
                    part->rotation.vz += *(u16 *)track;
                    slot->u.track.cursor += 2;
                }
                goto check_rot;
            case 3:
                time = slot->time + 1;
                part->rotation.vx = slot->u.values[0] + slot->u.values[3] * time / slot->duration;
                part->rotation.vy = slot->u.values[1] + slot->u.values[4] * time / slot->duration;
                last = slot;
                part->rotation.vz = slot->u.values[2] + slot->u.values[5] * time / slot->duration;
                goto check_rot;
            case 4: {
                s16 sx, sy, sz;

                sx = (slot->u.values[3] - part->rotation.vx) / slot->duration;
                sy = (slot->u.values[4] - part->rotation.vy) / slot->duration;
                sz = (slot->u.values[5] - part->rotation.vz) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    part->rotation.vx = slot->u.values[3];
                    part->rotation.vy = slot->u.values[4];
                    part->rotation.vz = slot->u.values[5];
                } else {
                    part->rotation.vx += sx;
                    part->rotation.vy += sy;
                    part->rotation.vz += sz;
                    slot->time = 0;
                }
                goto check_rot;
            }
            case 5:
                slot->u.values[0] += slot->u.values[3];
                part->rotation.vx += slot->u.values[0];
                slot->u.values[1] += slot->u.values[4];
                part->rotation.vy += slot->u.values[1];
                slot->u.values[2] += slot->u.values[5];
                last = slot;
                part->rotation.vz += slot->u.values[2];
                goto check_rot;
            case 7:
            case 8:
                dx = slot->u.values[3] - part->translation[0];
                dy = slot->u.values[4] - part->translation[1];
                dz = slot->u.values[5] - part->translation[2];
                dist = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
                last = slot;
                turn = (ratan2(-dx, -dz) - part->rotation.vy) & 0xFFF;
                if (turn >= 0x800) {
                    turn -= 0x1000;
                }
                limit = slot->u.values[1] + (dist + slot->time) * slot->u.values[2] / slot->u.values[0];
                if (ABS(turn) < limit) {
                    part->rotation.vy += turn;
                } else if (turn < 0) {
                    part->rotation.vy -= limit;
                } else {
                    part->rotation.vy += limit;
                }
                if ((kind & 0xF) == 7) {
                    turn = (ratan2(dy, SquareRoot0(dx * dx + dz * dz)) - part->rotation.vx) & 0xFFF;
                    if (turn >= 0x800) {
                        turn -= 0x1000;
                    }
                    if (ABS(turn) < limit) {
                        part->rotation.vx += turn;
                    } else if (turn < 0) {
                        part->rotation.vx -= limit;
                    } else {
                        part->rotation.vx += limit;
                    }
                }
                if (last->time < 0x7D00) {
                    last->time += last->duration;
                }
                goto rot_done;
            default:
            check_rot:
                if (++slot->time >= slot->duration) {
                    if (!slot->field1) {
                        if (slot->tag == tag) {
                            result |= 2;
                        }
                        result |= 0x200;
                        func_800A23E8(pool, (EffectEntry *)slot);
                        part->effects[0] = NULL;
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
                break;
            }
        rot_done:
            part->rotate = 1;
            part->dirty = 1;
        }
        if (part->effects[1] != NULL) {
            slot = (Tween *)part->effects[1];
            type = slot->kind;
            switch (type & 0xF) {
            case 0:
                track = slot->u.track.cursor;
                if (!(type & 0x10)) {
                    part->translation[0] = *(s16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(type & 0x20)) {
                    part->translation[1] = *(s16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                }
                if (!(type & 0x40)) {
                    part->translation[2] = *(s16 *)track;
                    slot->u.track.cursor += 2;
                }
                break;
            case 1:
                if (!(type & 0x10)) {
                    TRACK_DELTA(slot, part->translation[0]);
                }
                if (!(type & 0x20)) {
                    TRACK_DELTA(slot, part->translation[1]);
                }
                if (!(type & 0x40)) {
                    TRACK_DELTA(slot, part->translation[2]);
                }
                break;
            case 2:
                track = slot->u.track.cursor;
                if (!(type & 0x10)) {
                    move.vx = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                } else {
                    move.vx = 0;
                }
                if (!(type & 0x20)) {
                    move.vy = *(u16 *)track;
                    track += 2;
                    slot->u.track.cursor += 2;
                } else {
                    move.vy = 0;
                }
                if (!(type & 0x40)) {
                    move.vz = *(u16 *)track;
                    slot->u.track.cursor += 2;
                } else {
                    move.vz = 0;
                }
                move.vx = move.vx * part->scale[0] >> 12;
                move.vy = move.vy * part->scale[1] >> 12;
                move.vz = move.vz * part->scale[2] >> 12;
                ApplyMatrix(&part->world, &move, &moved);
                part->translation[0] += scale * moved.vx >> 12;
                part->translation[1] += scale * moved.vy >> 12;
                part->translation[2] += scale * moved.vz >> 12;
                break;
            case 3:
                time = slot->time + 1;
                part->translation[0] = slot->u.values[0] + slot->u.values[3] * time / slot->duration;
                part->translation[1] = slot->u.values[1] + slot->u.values[4] * time / slot->duration;
                last = slot;
                part->translation[2] = slot->u.values[2] + slot->u.values[5] * time / slot->duration;
                break;
            case 4: {
                s16 sx, sy, sz;

                sx = (slot->u.values[3] - part->translation[0]) / slot->duration;
                sy = (slot->u.values[4] - part->translation[1]) / slot->duration;
                sz = (slot->u.values[5] - part->translation[2]) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    part->translation[0] = slot->u.values[3];
                    part->translation[1] = slot->u.values[4];
                    part->translation[2] = slot->u.values[5];
                } else {
                    part->translation[0] += sx;
                    part->translation[1] += sy;
                    part->translation[2] += sz;
                    slot->time = 0;
                }
                break;
            }
            case 5:
                slot->u.values[0] += slot->u.values[3];
                part->translation[0] += slot->u.values[0];
                slot->u.values[1] += slot->u.values[4];
                part->translation[1] += slot->u.values[1];
                slot->u.values[2] += slot->u.values[5];
                last = slot;
                part->translation[2] += slot->u.values[2];
                break;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->field1) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_800A23E8(pool, (EffectEntry *)slot);
                    part->effects[1] = NULL;
                } else {
                    if (slot->tag == tag) {
                        result |= 4;
                    }
                    result |= 0x400;
                    if ((type & 0xF) < 3) {
                        slot->time = 0;
                        slot->u.track.cursor = slot->u.track.start;
                    } else {
                        slot->time = -1;
                        if ((type & 0xF) == 5) {
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
            part->dirty = 1;
        }
        if (part->effects[2] != NULL) {
            slot = (Tween *)part->effects[2];
            mode = slot->kind & 0xF;
            switch (mode) {
            case 3:
                time = slot->time + 1;
                part->scale[0] = slot->u.values[0] + slot->u.values[3] * time / slot->duration;
                part->scale[1] = slot->u.values[1] + slot->u.values[4] * time / slot->duration;
                last = slot;
                part->scale[2] = slot->u.values[2] + slot->u.values[5] * time / slot->duration;
                break;
            case 4: {
                s16 sx, sy, sz;

                sx = (slot->u.values[3] - slot->u.values[0]) / slot->duration;
                sy = (slot->u.values[4] - slot->u.values[1]) / slot->duration;
                sz = (slot->u.values[5] - slot->u.values[2]) / slot->duration;
                last = slot;
                if (sx == 0 && sy == 0 && sz == 0) {
                    slot->time = slot->duration;
                    part->scale[0] = slot->u.values[3];
                    part->scale[1] = slot->u.values[4];
                    part->scale[2] = slot->u.values[5];
                } else {
                    last->u.values[0] += sx;
                    last->u.values[1] += sy;
                    last->u.values[2] += sz;
                    part->scale[0] = last->u.values[0];
                    part->scale[1] = last->u.values[1];
                    part->scale[2] = last->u.values[2];
                    slot->time = 0;
                }
                break;
            }
            case 5:
                slot->u.values[0] += slot->u.values[3];
                part->scale[0] += slot->u.values[0];
                slot->u.values[1] += slot->u.values[4];
                part->scale[1] += slot->u.values[1];
                slot->u.values[2] += slot->u.values[5];
                last = slot;
                part->scale[2] += slot->u.values[2];
                break;
            }
            if (++slot->time >= slot->duration) {
                if (!slot->field1) {
                    if (slot->tag == tag) {
                        result |= 2;
                    }
                    result |= 0x200;
                    func_800A23E8(pool, (EffectEntry *)slot);
                    part->effects[2] = NULL;
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
            part->rotate = 1;
            part->dirty = 1;
        }
    }
    return result;
}

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

/* Tween the parts after the root towards an animation frame over duration
 * ticks (at least 1): each changed rotation gets an effect entry (type
 * mode + 3) of the shortest angle differences (mode 1: to the absolute
 * angles), each changed translation one of its movement (mode 1: to the
 * absolute translation); persistent entries (0xFF) stay, other parts lose
 * theirs. Returns the part count less the root. */
u16 func_800A1CF4(EffectPool *pool, ModelPart *part, s16 *data, s32 duration, s32 mode, s32 smooth,
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
                    entry = func_800A2330(pool);
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
            func_800A23E8(pool, part->effects[0]);
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
                    entry = func_800A2330(pool);
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
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
    }
    return count;
}

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
    heap_select_owner_tag(4, 0);
    pool->entries = heap_alloc(count * sizeof(EffectEntry), 0);
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
        heap_free(pool->entries);
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

/* Start an animation frame on a hierarchy through track entries (a packed
 * frame is applied at once, 800A1B50): each part with a track gets an entry
 * (unless it holds a persistent one), a part after the root without one loses
 * its entry. Returns 1 for a packed frame. */
s32 func_800A2434(EffectPool *pool, ModelPart *part, u16 *data, s32 mode, s32 tag) {
    AnimationFrame *frame;
    u8 *types;
    Tween *entry;
    u8 *tracks;
    u16 count;
    u16 rotationCount;
    u16 translationCount;
    u16 duration;
    u16 flags;
    s32 i;

    frame = (AnimationFrame *)data;
    if (frame->packed != 0) {
        func_800A2ACC(pool, part);
        func_800A1B50(part, (s16 *)frame);
        return 1;
    }
    rotationCount = frame->rotationCount;
    count = part->index;
    translationCount = frame->translationCount;
    if (rotationCount + 1 < count) {
        count = rotationCount + 1;
    }
    mode &= 1;
    duration = frame->duration;
    if (!mode) {
        duration--;
    }
    flags = frame->flags;
    data = (u16 *)(frame + 1);
    tracks = (u8 *)data + (rotationCount + 1) * sizeof(TrackEntry);
    if (!(flags & 1)) {
        tracks += rotationCount * 6;
    }
    if (!(flags & 2)) {
        tracks += translationCount * 6;
    }
    for (i = 0; i < count; i++) {
        types = (u8 *)(data + 2);
        if (*data != 0xFFFF) {
            if (part->effects[0] != NULL) {
                entry = (Tween *)part->effects[0];
                if (entry->tag == 0xFF) {
                    goto translation;
                }
            } else {
                entry = (Tween *)func_800A2330(pool);
            }
            if (entry != NULL) {
                entry->used = 1;
                entry->field1 = mode;
                entry->kind = types[0];
                entry->tag = tag;
                entry->u.track.cursor = entry->u.track.start = tracks + *data;
                entry->time = 0;
                entry->duration = duration;
                part->effects[0] = (EffectEntry *)entry;
            }
        } else {
            if (part->effects[0] != NULL && i != 0 && part->effects[0]->tag != 0xFF) {
                func_800A23E8(pool, part->effects[0]);
                part->effects[0] = NULL;
            }
        }
    translation:
        data++;
        if (*data != 0xFFFF) {
            if (part->effects[1] != NULL) {
                entry = (Tween *)part->effects[1];
                if (entry->tag == 0xFF) {
                    goto next;
                }
            } else {
                entry = (Tween *)func_800A2330(pool);
            }
            if (entry != NULL) {
                entry->used = 1;
                entry->field1 = mode;
                entry->kind = types[1];
                entry->tag = tag;
                entry->u.track.cursor = entry->u.track.start = tracks + *data;
                entry->time = 0;
                entry->duration = duration;
                part->effects[1] = (EffectEntry *)entry;
            }
        } else {
            if (part->effects[1] != NULL && i != 0 && part->effects[1]->tag != 0xFF) {
                func_800A23E8(pool, part->effects[1]);
                part->effects[1] = NULL;
            }
        }
    next:
        data += 2;
        part++;
    }
    return 0;
}

/* As 800A2434, but each part after the root first takes the frame's start
 * values of the tracks that are started. */
s32 func_800A2704(EffectPool *pool, ModelPart *part, u16 *data, s32 mode, s32 tag) {
    AnimationFrame *frame;
    u8 *types;
    Tween *entry;
    s16 *values;
    u8 *tracks;
    u16 rotationCount;
    u16 translationCount;
    u16 count;
    u16 duration;
    u16 rotations;
    u16 translations;
    u16 flags;
    s32 i;

    frame = (AnimationFrame *)data;
    if (frame->packed != 0) {
        func_800A2ACC(pool, part);
        func_800A1B50(part, (s16 *)frame);
        return 1;
    }
    mode &= 1;
    rotations = 0;
    rotationCount = frame->rotationCount;
    count = part->index;
    translations = 0;
    translationCount = frame->translationCount;
    if (rotationCount + 1 < count) {
        count = rotationCount + 1;
    }
    duration = frame->duration;
    if (!mode) {
        duration--;
    }
    flags = frame->flags;
    data = (u16 *)(frame + 1);
    values = (s16 *)(data + (rotationCount + 1) * 3);
    tracks = (u8 *)values;
    if (!(flags & 1)) {
        tracks += rotationCount * 6;
    }
    if (!(flags & 2)) {
        tracks += translationCount * 6;
    }
    for (i = 0; i < count; i++) {
        types = (u8 *)(data + 2);
        if (*data != 0xFFFF) {
            if (part->effects[0] != NULL) {
                entry = (Tween *)part->effects[0];
                if (entry->tag == 0xFF) {
                    goto skipRotation;
                }
            } else {
                entry = (Tween *)func_800A2330(pool);
            }
            if (!(flags & 1) && i != 0 && rotations < rotationCount) {
                part->rotation.vx = *values++;
                part->rotation.vy = *values++;
                part->rotation.vz = *values++;
                rotations++;
                part->dirty = 1;
                part->rotate = 1;
            }
            if (entry != NULL) {
                entry->used = 1;
                entry->field1 = mode;
                entry->kind = types[0];
                entry->tag = tag;
                entry->u.track.cursor = entry->u.track.start = tracks + *data;
                entry->time = 0;
                entry->duration = duration;
                part->effects[0] = (EffectEntry *)entry;
            }
        } else {
        skipRotation:
            if (!(flags & 1) && i != 0 && rotations < rotationCount) {
                values += 3;
                rotations++;
            }
        }
        data++;
        if (*data != 0xFFFF) {
            if (part->effects[1] != NULL) {
                entry = (Tween *)part->effects[1];
                if (entry->tag == 0xFF) {
                    goto skipTranslation;
                }
            } else {
                entry = (Tween *)func_800A2330(pool);
            }
            if (!(flags & 2) && i != 0 && translations < translationCount) {
                part->translation[0] = *values++;
                part->translation[1] = *values++;
                part->translation[2] = *values++;
                translations++;
                part->dirty = 1;
            }
            if (entry != NULL) {
                entry->used = 1;
                entry->field1 = mode;
                entry->kind = types[1];
                entry->tag = tag;
                entry->u.track.cursor = entry->u.track.start = tracks + *data;
                entry->time = 0;
                entry->duration = duration;
                part->effects[1] = (EffectEntry *)entry;
            }
        } else {
        skipTranslation:
            if (!(flags & 2) && i != 0 && translations < translationCount) {
                values += 3;
                translations++;
            }
        }
        data += 2;
        part++;
    }
    return 0;
}

/* Release every part's attached effects that are not persistent. */
void func_800A2ACC(EffectPool *pool, ModelPart *part) {
    u16 count = part->index;
    s32 i;

    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL && part->effects[0]->tag != 0xFF) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && part->effects[1]->tag != 0xFF) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && part->effects[2]->tag != 0xFF) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Release every part's attached effects tagged `tag`. */
void func_800A2BB8(EffectPool *pool, ModelPart *part, u8 tag) {
    u16 count = part->index;
    s32 i;

    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL && part->effects[0]->tag == tag) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && part->effects[1]->tag == tag) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && part->effects[2]->tag == tag) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Create a pool of count sprite records (and a spare). */
SpritePool *func_800A2CA4(SpritePool *pool, s32 count) {
    heap_select_owner_tag(4, 0);
    pool->count = count;
    pool->next = 0;
    pool->records = heap_alloc((count + 1) * sizeof(EffectSprite), 0);
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
        heap_free(pool->records);
    }
    pool->records = NULL;
}

/* Mark every record of a sprite pool free and set up both of its
 * semi-transparent quadrilaterals. */
void func_800A2D5C(SpritePool *pool) {
    EffectSprite *record = pool->records;
    s32 i;
    s32 j;

    for (i = 0; i < pool->count + 1; i++) {
        record->age = -1;
        record->lifetime = 0;
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
EffectSprite *func_800A2E88(SpritePool *pool, s16 abe) {
    EffectSprite *record;
    s16 next = pool->next;

    if (next < pool->count) {
        record = &pool->records[next];
        if (record->age == -1) {
            pool->next = next + 1;
            while (pool->next < pool->count && pool->records[pool->next].age != -1) {
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
s32 func_800A2F94(SpritePool *pool, EffectSprite *record) {
    s32 index = ((u32)record - (u32)pool->records) / sizeof(EffectSprite);

    if (index <= pool->next) {
        pool->next = index;
    }
    record->age = -1;
    return index;
}

/* Draw the live sprites of a pool into the ordering table (3D ones projected
 * with the GTE at their depth, 2D ones at the front), free the expired ones
 * and fade the rest by steps ticks. */
void func_800A2FD8(SpritePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer) {
    EffectSprite *sprite;
    s32 otz;
    s32 i;

    SetRotMatrix(m);
    SetTransMatrix(m);
    sprite = pool->records;
    for (i = 0; i < pool->count; i++, sprite++) {
        if (sprite->age == -1) {
            continue;
        }
        if (sprite->age >= sprite->lifetime) {
            func_800A2F94(pool, sprite);
            continue;
        }
        sprite->packets[buffer].r0 = sprite->color[0] >> 6;
        sprite->packets[buffer].g0 = sprite->color[1] >> 6;
        sprite->packets[buffer].b0 = sprite->color[2] >> 6;
        if (sprite->projected == 0) {
            sprite->packets[buffer].x0 = sprite->x0;
            sprite->packets[buffer].y0 = sprite->y0;
            sprite->packets[buffer].x1 = sprite->x1;
            sprite->packets[buffer].y1 = sprite->y1;
            sprite->packets[buffer].x2 = sprite->x2;
            sprite->packets[buffer].y2 = sprite->y2;
            sprite->packets[buffer].x3 = sprite->x3;
            sprite->packets[buffer].y3 = sprite->y3;
            addPrim(ot, &sprite->packets[buffer]);
        } else {
            gte_ldv3(&sprite->x0, &sprite->x1, &sprite->x2);
            gte_rtpt();
            gte_stsxy3(&sprite->packets[buffer].x0, &sprite->packets[buffer].x1, &sprite->packets[buffer].x2);
            gte_stszotz(&otz);
            otz >>= model_ot_depth_shift;
            gte_ldv0(&sprite->x3);
            gte_rtps();
            gte_stsxy(&sprite->packets[buffer].x3);
            addPrim(ot + otz, &sprite->packets[buffer]);
        }
        sprite->age += steps;
        sprite->color[0] -= sprite->fade[0] * steps;
        sprite->color[1] -= sprite->fade[1] * steps;
        sprite->color[2] -= sprite->fade[2] * steps;
    }
}

/* Set up a colour fade from (r0, g0, b0) to (r1, g1, b1) over duration
 * ticks. Original calls use default argument promotion for the channel bytes
 * and signed event halfwords. */
s32 func_800A32D8(fade, pool, id, solid, max, duration, r0, g0, b0, r1, g1, b1, x0, y0, z0, x1, y1, z1,
                  semiTrans)
    ColorFade *fade;
    s32 pool;
    s16 id;
    u8 solid;
    s16 max, duration;
    u8 r0, g0, b0, r1, g1, b1;
    u16 x0, y0, z0, x1, y1, z1, semiTrans;
{
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
void func_800A3484(ColorFade *fade, s32 arg1) {
    fade->id = -1;
}

/* A frame curve: base + (cos(angle) + 1.0) / divisor. */
s16 func_800A3490(s16 angle, s16 divisor, s32 base) {
    return base + (gpu_get_cos(angle) + 0x1000) / divisor;
}

/* A frame curve: base + value / divisor, or -1 past 32. */
s16 func_800A3514(s16 value, s16 divisor, s16 base) {
    base += value / divisor;
    if (base > 0x20) {
        return -1;
    }
    return base;
}

/* A frame curve: base - value / divisor. */
s16 func_800A3578(s16 value, s16 divisor, s32 base) {
    return base - value / divisor;
}

/* A frame curve: 32 - value / divisor, at least minimum. */
s16 func_800A35C8(s16 value, s16 divisor, s16 minimum) {
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
 * (modes 4/5: the three values cycling by row). Original calls promote the
 * event halfwords before speed is stored as an unsigned halfword. */
ImageAnim *func_800A3640(anim, target, mode, flags, colors, x, y, z, x2, y2, z2, x3, y3, w, h, speed,
                         divisor, base, curve)
    ImageAnim *anim, *target;
    u16 mode, flags;
    ColorRow *colors;
    s16 x, y, z, x2, y2, z2, x3, y3, w, h;
    u16 speed;
    s16 divisor, base;
    FrameCurve curve;
{
    RECT rect;
    s32 half;
    s32 i, col;
    u16 color, fill;

    if (anim->active != 0) {
        return NULL;
    }
    heap_select_owner_tag(4, 0);
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
            anim->work = heap_alloc((s16)(half * 2) * h * 2, 0);
        }
        if (flags & 0x200) {
            anim->pixels2 = heap_alloc((s16)(half * 2) * h * 2, 0);
        }
        if (flags & 0x400) {
            anim->pixels = heap_alloc((s16)(half * 2) * h * 2, 0);
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
            anim->work = heap_alloc(w * h * 2, 0);
        }
        if (flags & 0x200) {
            anim->pixels2 = heap_alloc(w * h * 2, 0);
        }
        if (flags & 0x400) {
            anim->pixels = heap_alloc(w * h * 2, 0);
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
s16 func_800A3E98(ImageAnim *anim, s32 ticks) {
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
        func_800A429C(anim);
        return frame;
    }
    value = anim->frame;
    if (frame != value) {
        anim->frame = result;
        switch (anim->mode) {
        case 0:
            sprite_darken_pixels(anim->size, frame, anim->work, anim->pixels);
            if (anim->target == NULL) {
                LoadImage(&anim->rect, (u_long *)anim->work);
            }
            break;
        case 1:
            sprite_blend_pixels(anim->size, frame, anim->work, anim->pixels2, anim->pixels);
            if (anim->target == NULL) {
                LoadImage(&anim->rect, (u_long *)anim->work);
            }
            break;
        case 4:
            func_800A4348(anim, frame);
            break;
        case 5:
            func_800A43F8(anim, frame);
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
void func_800A429C(ImageAnim *anim) {
    if (anim->active) {
        if (anim->pixels != NULL) {
            if (anim->mode < 4) {
                LoadImage(&anim->rect, (u_long *)anim->pixels);
            }
            heap_free(anim->pixels);
            anim->pixels = NULL;
        }
        if (anim->pixels2 != NULL) {
            heap_free(anim->pixels2);
            anim->pixels2 = NULL;
        }
        if (anim->work != NULL) {
            heap_free(anim->work);
            anim->work = NULL;
        }
        anim->active = 0;
    }
}

/* Fade an image animation's colours to `level` / 32 of its pixels. */
void func_800A4348(ImageAnim *anim, s16 level) {
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
void func_800A43F8(ImageAnim *anim, s16 level) {
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

/* Update the active trackers' positions: an offset from a part of a stage
 * object's hierarchy when the object exists, else the offset itself. */
void func_800A44C0(BattleObject **objects) {
    MATRIX *m = (MATRIX *)0x1F800000;
    s32 i;
    ModelPart *root;
    VECTOR position;

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

/* Draw the stage: advance the stage object's image animations, run the
 * stage update (800A6AE8) on the scratchpad stack, step both resident
 * records, draw the stage hierarchy (800A48EC), both resident handles and
 * the sky (800A4DB8) seen from eye towards target into ot[depth - 1]. */
void func_800A4654(MATRIX *view, MATRIX *light, s32 arg2, u32 *ot, s32 buffer, SVECTOR *eye, SVECTOR *target,
                   s32 depth) {
    ImageAnim *anim;
    s32 i;

    if (light != NULL) {
        SetLightMatrix(light);
    }
    anim = D_800D3368[31]->images;
    for (i = 0; i < D_800D3368[31]->imageCount; i++, anim++) {
        func_800A3E98(anim, D_800CCC5C);
    }
    SPAD_STACK_ENTER();
    func_800A6AE8();
    SPAD_STACK_LEAVE();
    for (i = 0; i < 2; i++) {
        gpu_update_texture_scroll(&D_800C3DA0[i]);
    }
    if (D_800C3E38 != 0) {
        func_800A48EC(D_800C3E48, D_800C3E38, view, (s32)light, arg2, ot, buffer, depth);
    }
    for (i = 0; i < 2; i++) {
        gpu_draw_panorama(D_800C3D50[i], eye, target, view, (u_long *)(ot + depth - 1), buffer);
    }
    func_800A4DB8(D_800C3EA0, eye, target, view, ot + depth - 1, buffer);
}

/* Free the battle scene's resources: the stage objects, the scene data, both
 * resident handles of D_800C3D50 (80027D40), the block D_800C3EA0 and both
 * records of D_800C3DA0 (8002800C). */
void func_800A4820(void) {
    s32 i;

    func_800A9FF0(31);
    D_800C3E38 = 0;
    if (mode_battle_scene_data != NULL) {
        heap_free(mode_battle_scene_data);
    }
    mode_battle_scene_data = NULL;
    for (i = 0; i < 2; i++) {
        if (D_800C3D50[i] != NULL) {
            gpu_free_panorama(D_800C3D50[i]);
        }
        D_800C3D50[i] = NULL;
    }
    if (D_800C3EA0 != NULL) {
        heap_free(D_800C3EA0);
    }
    D_800C3EA0 = NULL;
    for (i = 0; i < 2; i++) {
        gpu_free_texture_scroll(&D_800C3DA0[i]);
    }
}

/* Draw the scene hierarchy's visible model parts under view: billboard parts
 * (field52 1: upright, 2: facing the view) drop the parts' rotation, and
 * field52 selects the model drawing mode (4-7: 2-5); plain parts (field52 0)
 * draw at ordering-table depth 16 into ot[depth - 1]. */
void func_800A48EC(ModelTable *models, ModelPart *part, MATRIX *view, s32 arg3, s32 arg4, u32 *ot, s32 buffer,
                   s32 depth) {
    MATRIX *m;
    s32 shift;
    u32 count;
    s32 i;
    s32 mode;

    m = (MATRIX *)0x1F800040;
    count = part++->index - 1;
    shift = model_ot_depth_shift;
    for (i = 0; i < count; i++, part++) {
        if (part->modelId != 0xFFFF && part->visible) {
            CompMatrix(view, &part->world, m);
            if ((u16)(part->field52 - 1) < 2) {
                m->m[0][0] = 0x1000;
                m->m[0][2] = 0;
                m->m[1][0] = 0;
                m->m[1][2] = 0;
                m->m[2][0] = 0;
                m->m[2][2] = 0x1000;
                if ((s16)part->field52 == 1) {
                    m->m[0][1] = view->m[0][1];
                    m->m[1][1] = view->m[1][1];
                    m->m[2][1] = view->m[2][1];
                } else {
                    m->m[0][1] = 0;
                    m->m[1][1] = 0x1000;
                    m->m[2][1] = 0;
                }
            }
            SetRotMatrix(m);
            SetTransMatrix(m);
            switch ((s16)part->field52) {
            case 4:
                mode = 2;
                break;
            case 5:
                mode = 3;
                break;
            case 6:
                mode = 4;
                break;
            case 7:
                mode = 5;
                break;
            default:
                mode = 0;
                break;
            }
            if ((s16)part->field52 == 0) {
                model_ot_depth_shift = 16;
                model_draw_sprite_model(models->models[part->modelId], part->packets[buffer], ot + depth - 1, mode);
            } else {
                model_ot_depth_shift = shift;
                model_draw_sprite_model(models->models[part->modelId], part->packets[buffer], ot, mode);
            }
        }
    }
    model_ot_depth_shift = shift;
}

/* Push point (relative to origin, in the ground plane) out of the first
 * listed circle (x, z, radius) it lies inside, onto its rim; whether it was
 * pushed. */
s32 func_800A4B3C(SVECTOR *origin, SVECTOR *point) {
    u16 *circle = D_800D2FD0;
    s32 i;
    s32 pushed = 0;
    s16 cx;
    s16 cz;
    s16 radius;
    s32 dx;
    s32 dz;
    s32 distance;

    for (i = 0; i < D_800D2FC8; i++) {
        cx = *circle++ - origin->vx;
        dx = point->vx - cx;
        cz = *circle++ - origin->vz;
        dz = point->vz - cz;
        radius = *circle++;
        distance = SquareRoot0(dx * dx + dz * dz) + 1;
        if (distance < radius) {
            pushed = 1;
            point->vx = cx + dx * radius / distance;
            point->vz = cz + dz * radius / distance;
            break;
        }
    }
    return pushed;
}

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

/* Draw the stage sky seen from eye towards target: the horizon bands at the
 * projected horizon (near and far, clamped to the screen), then the tiles
 * of the scrolling ceiling under a camera turned and tilted with the view,
 * each front-facing tile textured from the scroll position. Each visible
 * tile takes a short copy of u0 for its u corners: the copy and its u8
 * lowpart are the two-insn invariant that loop.c hoists out of both loops
 * (with half's two-insn extension, four moved insns double the row loop's
 * count, so (u8)half and the addPrim masks stay in the row body) and that
 * combine folds into the original's plain copy of u0 before the rows.
 * Measured otherwise: u0 used directly leaves no invariant, (u8)u0 leaves an
 * andi, a short u0 set before the loops a one-insn copy too short-lived to
 * move, a u8 u an extra insn that costs u0's term t8, and a short copy of v0
 * as well a fifth moved insn that keeps u's copy in the row body. */
void func_800A4DB8(StageGeometry *sky, SVECTOR *eye, SVECTOR *target, MATRIX *view, u32 *ot,
                   s32 buffer) {
    SVECTOR unused; /* declared, never used (its slot stays in the frame) */
    MATRIX camera;
    MATRIX turn;
    SVECTOR angles;
    VECTOR delta;
    SVECTOR top;
    SVECTOR bottom;
    VECTOR direction;
    SVECTOR point;
    SVECTOR normal;
    s32 clip;
    s32 angle;
    s32 tilt;
    s32 size;
    s16 half;
    s32 u0;
    s32 v0;
    s16 u;
    s32 v;
    s32 row;
    s32 col;
    s32 n;
    SVECTOR *vertex;

    if (sky == NULL) {
        return;
    }
    addPrim(ot, &sky->modes2[buffer]);
    direction.vx = target->vx - eye->vx;
    direction.vy = 0;
    direction.vz = target->vz - eye->vz;
    VectorNormalS(&direction, &normal);
    point.vx = normal.vx * sky->distance / 4096 + target->vx;
    point.vy = sky->horizon;
    point.vz = normal.vz * sky->distance / 4096 + target->vz;
    SetRotMatrix(view);
    SetTransMatrix(view);
    gte_ldv0(&point);
    gte_rtps();
    gte_stsxy(&top);
    top.vx = top.vy;
    if (top.vy > 240) {
        top.vy = 240;
    }
    point.vx = normal.vx * sky->distance / 4096 * sky->nearScale / 256 + target->vx;
    point.vy = sky->horizon * sky->nearScale / 256;
    point.vz = normal.vz * sky->distance / 4096 * sky->nearScale / 256 + target->vz;
    gte_ldv0(&point);
    gte_rtps();
    gte_stsxy(&bottom);
    if (bottom.vy >= 0 && top.vy < 240) {
        sky->quads[buffer + 2].y0 = top.vy;
        sky->quads[buffer + 2].y1 = top.vy;
        sky->quads[buffer + 2].y2 = bottom.vy;
        sky->quads[buffer + 2].y3 = bottom.vy;
        addPrim(ot, &sky->quads[buffer + 2]);
    }
    if (bottom.vy < 0) {
        bottom.vy = 0;
    }
    if (bottom.vy < 240) {
        sky->flats[buffer + 2].y0 = bottom.vy;
        sky->flats[buffer + 2].y1 = bottom.vy;
        addPrim(ot, &sky->flats[buffer + 2]);
    }
    point.vx = normal.vx * sky->distance / 4096 * sky->farScale / 256 + target->vx;
    point.vy = sky->horizon * sky->farScale / 256;
    point.vz = normal.vz * sky->distance / 4096 * sky->farScale / 256 + target->vz;
    gte_ldv0(&point);
    gte_rtps();
    gte_stsxy(&bottom);
    bottom.vy = top.vy * 2 - bottom.vy - 8;
    if (top.vx >= 0 && top.vx < 480 && bottom.vy < 240) {
        sky->quads[buffer].y0 = top.vx;
        sky->quads[buffer].y1 = top.vx;
        sky->quads[buffer].y2 = bottom.vy;
        sky->quads[buffer].y3 = bottom.vy;
        addPrim(ot, &sky->quads[buffer]);
    }

    delta.vx = target->vx - eye->vx;
    delta.vy = 0;
    delta.vz = target->vz - eye->vz;
    angle = ratan2(target->vy - eye->vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz));
    tilt = (angle - 0x100) * sky->tilt / 512 * (0x400 - (angle < 0 ? -angle : angle)) / 1024;
    SetGeomScreen(sky->screen);
    angles.vx = 0;
    angles.vy = -ratan2(delta.vx, delta.vz);
    angles.vz = 0;
    gpu_build_rotation_matrix(&angles, &turn);
    turn.t[0] = 0;
    turn.t[1] = 0;
    turn.t[2] = 0;
    angles.vx = tilt;
    angles.vy = -angles.vy;
    RotMatrixYXZ(&angles, &camera);
    VectorNormalS(&delta, &angles);
    delta.vx = eye->vx + angles.vx * 2;
    delta.vy = eye->vy / 4 - sky->height;
    delta.vz = eye->vz + angles.vz * 2;
    camera.t[0] = delta.vx;
    camera.t[1] = delta.vy;
    camera.t[2] = delta.vz;
    CompMatrix(&camera, &turn, &camera);
    CompMatrix(view, &camera, &camera);
    SetRotMatrix(&camera);
    SetTransMatrix(&camera);

    sky->scrollX += sky->speedX;
    sky->scrollY += sky->speedY;
    u0 = (sky->scrollX / 16 + delta.vx / 12) & ((size = sky->tileSize) - 1);
    v0 = (sky->scrollY / 16 + delta.vz / 12) & (size - 1);
    n = buffer * 64;
    vertex = &sky->grid[0][0];
    half = (s16)size / 2;
    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++, n++, vertex++) {
            gte_ldv3(&vertex[0], &vertex[1], &vertex[9]);
            gte_rtpt();
            gte_nclip();
            gte_stopz(&clip);
            if (clip >= 0) {
                gte_stsxy3(&sky->tiles[n].x0, &sky->tiles[n].x1, &sky->tiles[n].x2);
                gte_ldv0(&vertex[10]);
                gte_rtps();
                gte_stsxy(&sky->tiles[n].x3);
                v = (row & 1) * half + v0;
                u = u0;
                setUV4(&sky->tiles[n], (col & 1) * half + u, v,
                       (col & 1) * half + u + half - 1, v,
                       (col & 1) * half + u, v + half - 1,
                       (col & 1) * half + u + half - 1, v + half - 1);
                addPrim(ot, &sky->tiles[n]);
            }
        }
        vertex++;
    }
    SetGeomScreen(0x200);
    if (top.vy >= 0) {
        sky->flats[buffer].y2 = top.vy;
        sky->flats[buffer].y3 = top.vy;
        addPrim(ot, &sky->flats[buffer]);
    }
    addPrim(ot, &sky->modes[buffer]);
}

/* The scene's points. */
SVECTOR *func_800A577C(void) {
    return D_800D3344;
}

/* The scene's triangles. */
SceneTriangle *func_800A578C(void) {
    return D_800D39CC;
}

/* The first scene triangle containing point (800A5A48 gives -1), -1 for
 * none. */
s32 func_800A579C(SVECTOR *point) {
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
s32 func_800A5870(SVECTOR *point, s32 index, void *out) {
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
s32 func_800A5914(SVECTOR *point, s32 triangle, s32 depth) {
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
s32 func_800A5A48(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *point) {
    VECTOR edge;
    VECTOR toPoint;
    VECTOR cross;

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

/* The ground height of point on the plane through triangle (a, b, c): the
 * plane's unit normal goes to normal; a vertical plane leaves height 0. */
void func_800A5BE8(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *point, VECTOR *normal) {
    VECTOR edgeB;
    VECTOR edgeC;
    VECTOR edge;

    edge.vx = b->vx - a->vx;
    edge.vy = b->vy - a->vy;
    edge.vz = b->vz - a->vz;
    VectorNormal(&edge, &edgeB);
    edge.vx = c->vx - a->vx;
    edge.vy = c->vy - a->vy;
    edge.vz = c->vz - a->vz;
    VectorNormal(&edge, &edgeC);
    OuterProduct12(&edgeB, &edgeC, normal);
    if (normal->vy == 0) {
        point->vy = 0;
        return;
    }
    point->vy = a->vy + (-((point->vx - a->vx) * normal->vx) - (point->vz - a->vz) * normal->vz) / normal->vy;
}

/* Search triangle and, up to depth levels, its neighbours for the one
 * containing point (800A5A48 gives -1), testing each triangle once per visit
 * stamp; -1 for none. */
s32 func_800A5D54(SVECTOR *point, s32 triangle, s32 depth) {
    s32 found;

    if (triangle < 0) {
        return -1;
    }
    if (D_800D39CC[triangle].visited != D_800D2F64) {
        D_800D39CC[triangle].visited = D_800D2F64;
        if (func_800A5A48(&D_800D3344[D_800D39CC[triangle].vertices[0]], &D_800D3344[D_800D39CC[triangle].vertices[1]],
                          &D_800D3344[D_800D39CC[triangle].vertices[2]], point)
            == -1) {
            return triangle;
        }
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
    return -1;
}

/* Set the two words D_800D2D40 and D_800D2D48. */
void func_800A5E9C(s32 first, s32 second) {
    D_800D2D40 = first;
    D_800D2D48 = second;
}

/* Set up the stage lighting: turn the light slots off, make the stage image
 * (the w x h VRAM rectangle at (x, y), with a working copy) the target of
 * the stage object's active image animations, and save the stage's colours
 * twice (as loaded and a working copy). */
void func_800A5EB4(void) {
    CVECTOR *color;
    ImageAnim *anim;
    s32 i;

    for (i = 0; i < 4; i++) {
        D_800C3AAC[i].active = 0;
    }
    anim = D_800D3368[31]->images;
    for (i = 0; i < D_800D3368[31]->imageCount; i++, anim++) {
        if (anim->active) {
            anim->target = &D_800D3600;
        }
    }
    func_800A3640(&D_800D3600, NULL, 1, 0x601, NULL, D_800D2D30, D_800D2D34, 0, 0, 0, 0, D_800D2D30, D_800D2D34,
                  D_800D2D2C, D_800C3EA8, 0, 0, 0, NULL);
    D_800D3600.work = NULL;
    D_800C3AC4 = heap_alloc(sizeof(StageColors), 1);
    D_800C3AC8 = heap_alloc(sizeof(StageColors), 1);
    color = (CVECTOR *)D_800C3AC4;
    for (i = 0; i < 4; i++) {
        if (D_800C3EA0 != NULL) {
            color->r = ((StageGeometry *)D_800C3EA0)->quads[i].r0;
            color->g = ((StageGeometry *)D_800C3EA0)->quads[i].g0;
            color->b = ((StageGeometry *)D_800C3EA0)->quads[i].b0;
            color++;
            color->r = ((StageGeometry *)D_800C3EA0)->quads[i].r1;
            color->g = ((StageGeometry *)D_800C3EA0)->quads[i].g1;
            color->b = ((StageGeometry *)D_800C3EA0)->quads[i].b1;
            color++;
            color->r = ((StageGeometry *)D_800C3EA0)->quads[i].r2;
            color->g = ((StageGeometry *)D_800C3EA0)->quads[i].g2;
            color->b = ((StageGeometry *)D_800C3EA0)->quads[i].b2;
            color++;
            color->r = ((StageGeometry *)D_800C3EA0)->quads[i].r3;
            color->g = ((StageGeometry *)D_800C3EA0)->quads[i].g3;
            color->b = ((StageGeometry *)D_800C3EA0)->quads[i].b3;
            color++;
            color->r = ((StageGeometry *)D_800C3EA0)->flats[i].r0;
            color->g = ((StageGeometry *)D_800C3EA0)->flats[i].g0;
            color->b = ((StageGeometry *)D_800C3EA0)->flats[i].b0;
            color++;
        } else {
            color += 5;
        }
        if (D_800C3D50[0] != NULL) {
            color->r = D_800C3D50[0]->fills[i].r0;
            color->g = D_800C3D50[0]->fills[i].g0;
            color->b = D_800C3D50[0]->fills[i].b0;
        }
        color++;
    }
    for (i = 0; i < 2; i++) {
        if (D_800C3D50[0] != NULL) {
            color->r = D_800C3D50[0]->fades[i].r0;
            color->g = D_800C3D50[0]->fades[i].g0;
            color->b = D_800C3D50[0]->fades[i].b0;
            color++;
            color->r = D_800C3D50[0]->fades[i].r1;
            color->g = D_800C3D50[0]->fades[i].g1;
            color->b = D_800C3D50[0]->fades[i].b1;
            color++;
            color->r = D_800C3D50[0]->fades[i].r2;
            color->g = D_800C3D50[0]->fades[i].g2;
            color->b = D_800C3D50[0]->fades[i].b2;
            color++;
            color->r = D_800C3D50[0]->fades[i].r3;
            color->g = D_800C3D50[0]->fades[i].g3;
            color->b = D_800C3D50[0]->fades[i].b3;
            color++;
        } else {
            color += 4;
        }
    }
    color->r = ((u8 *)D_800D2D40)[0];
    color->g = ((u8 *)D_800D2D40)[1];
    color->b = ((u8 *)D_800D2D40)[2];
    color++;
    color->r = ((u8 *)D_800D2D48)[0];
    color->g = ((u8 *)D_800D2D48)[1];
    color->b = ((u8 *)D_800D2D48)[2];
}

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

/* Restore the stage's colours as loaded (saved by 800A5EB4). */
void func_800A64E4(void) {
    CVECTOR *color;
    s32 i;

    color = (CVECTOR *)D_800C3AC4;
    for (i = 0; i < 4; i++) {
        if (D_800C3EA0 != NULL) {
            ((StageGeometry *)D_800C3EA0)->quads[i].r0 = color->r;
            ((StageGeometry *)D_800C3EA0)->quads[i].g0 = color->g;
            ((StageGeometry *)D_800C3EA0)->quads[i].b0 = color->b;
            color++;
            ((StageGeometry *)D_800C3EA0)->quads[i].r1 = color->r;
            ((StageGeometry *)D_800C3EA0)->quads[i].g1 = color->g;
            ((StageGeometry *)D_800C3EA0)->quads[i].b1 = color->b;
            color++;
            ((StageGeometry *)D_800C3EA0)->quads[i].r2 = color->r;
            ((StageGeometry *)D_800C3EA0)->quads[i].g2 = color->g;
            ((StageGeometry *)D_800C3EA0)->quads[i].b2 = color->b;
            color++;
            ((StageGeometry *)D_800C3EA0)->quads[i].r3 = color->r;
            ((StageGeometry *)D_800C3EA0)->quads[i].g3 = color->g;
            ((StageGeometry *)D_800C3EA0)->quads[i].b3 = color->b;
            color++;
            ((StageGeometry *)D_800C3EA0)->flats[i].r0 = color->r;
            ((StageGeometry *)D_800C3EA0)->flats[i].g0 = color->g;
            ((StageGeometry *)D_800C3EA0)->flats[i].b0 = color->b;
            color++;
        } else {
            color += 5;
        }
        if (D_800C3D50[0] != NULL) {
            D_800C3D50[0]->fills[i].r0 = color->r;
            D_800C3D50[0]->fills[i].g0 = color->g;
            D_800C3D50[0]->fills[i].b0 = color->b;
        }
        color++;
    }
    for (i = 0; i < 2; i++) {
        if (D_800C3D50[0] != NULL) {
            D_800C3D50[0]->fades[i].r0 = color->r;
            D_800C3D50[0]->fades[i].g0 = color->g;
            D_800C3D50[0]->fades[i].b0 = color->b;
            color++;
            D_800C3D50[0]->fades[i].r1 = color->r;
            D_800C3D50[0]->fades[i].g1 = color->g;
            D_800C3D50[0]->fades[i].b1 = color->b;
            color++;
            D_800C3D50[0]->fades[i].r2 = color->r;
            D_800C3D50[0]->fades[i].g2 = color->g;
            D_800C3D50[0]->fades[i].b2 = color->b;
            color++;
            D_800C3D50[0]->fades[i].r3 = color->r;
            D_800C3D50[0]->fades[i].g3 = color->g;
            D_800C3D50[0]->fades[i].b3 = color->b;
            color++;
        } else {
            color += 4;
        }
    }
    ((u8 *)D_800D2D40)[0] = color->r;
    ((u8 *)D_800D2D40)[1] = color->g;
    ((u8 *)D_800D2D40)[2] = color->b;
    color++;
    ((u8 *)D_800D2D48)[0] = color->r;
    ((u8 *)D_800D2D48)[1] = color->g;
    ((u8 *)D_800D2D48)[2] = color->b;
}

/* Light a colour with light slot index: when active, its mode (field r)
 * adds the colour (0), half (1) or a quarter (2) of it, its grey level (3),
 * or nothing (4) to the light's signed colour (b, field4, field5, times 8); the
 * colour then moves towards that by field g / 32, clamped to 0-255, and is
 * copied to out. */
void func_800A6884(u8 *out, s32 index, u8 *color) {
    s32 r;
    s32 g;
    s32 b;
    s32 grey;

    r = (s8)D_800C3AAC[index].b * 8;
    g = (s8)D_800C3AAC[index].field4 * 8;
    b = (s8)D_800C3AAC[index].field5 * 8;
    if (D_800C3AAC[index].active) {
        switch (D_800C3AAC[index].r) {
        case 0:
            r += color[0];
            g += color[1];
            b += color[2];
            break;
        case 1:
            r += color[0] >> 1;
            g += color[1] >> 1;
            b += color[2] >> 1;
            break;
        case 2:
            r += color[0] >> 2;
            g += color[1] >> 2;
            b += color[2] >> 2;
            break;
        case 3:
            grey = (color[0] + color[1] + color[2]) / 3;
            r += grey;
            g += grey;
            b += grey;
            break;
        case 4:
            r = 0;
            g = 0;
            b = 0;
            break;
        }
    }
    r += (color[0] - r) * D_800C3AAC[index].g / 32;
    g += (color[1] - g) * D_800C3AAC[index].g / 32;
    b += (color[2] - b) * D_800C3AAC[index].g / 32;
    if (r >= 0x100) {
        color[0] = 0xFF;
    } else if (r < 0) {
        color[0] = 0;
    } else {
        color[0] = r;
    }
    if (g >= 0x100) {
        color[1] = 0xFF;
    } else if (g < 0) {
        color[1] = 0;
    } else {
        color[1] = g;
    }
    if (b >= 0x100) {
        color[2] = 0xFF;
    } else if (b < 0) {
        color[2] = 0;
    } else {
        color[2] = b;
    }
    out[0] = color[0];
    out[1] = color[1];
    out[2] = color[2];
}

/* Relight the stage when its image is marked dirty (800A6444 marks it on a
 * light slot change, through D_800D3611): restart the stage image from
 * its original pixels and the working colours from the loaded ones, then
 * apply each active light slot to the image (modes 0-3 through 80025D4C,
 * mode 4 through 80026F44) and to every stage colour (800A6884), and load
 * the image into VRAM. */
void func_800A6AE8(void) {
    CVECTOR *color;
    s32 i;
    s32 j;

    if (D_800D3600.dirty && D_800D3600.active) {
        D_800D3600.dirty = 0;
        memcpy(D_800D3600.pixels2, D_800D3600.pixels, D_800D2D2C * D_800C3EA8 * 2);
        *D_800C3AC8 = *D_800C3AC4;
        for (i = 0; i < 4; i++) {
            if (D_800C3AAC[i].active) {
                switch (D_800C3AAC[i].r) {
                case 0:
                case 1:
                case 2:
                case 3:
                    sprite_tint_blend_pixels(D_800D2D2C * D_800C3EA8, D_800D3600.pixels2, D_800D3600.pixels2,
                                  D_800D3600.pixels2, (s8)D_800C3AAC[i].b, (s8)D_800C3AAC[i].field4,
                                  (s8)D_800C3AAC[i].field5, D_800C3AAC[i].r, D_800C3AAC[i].g);
                    break;
                case 4:
                    sprite_darken_pixels(D_800D2D2C * D_800C3EA8, D_800C3AAC[i].g, D_800D3600.pixels2,
                                  D_800D3600.pixels);
                    break;
                }
                color = (CVECTOR *)D_800C3AC8;
                for (j = 0; j < 4; j++) {
                    if (D_800C3EA0 != NULL) {
                        func_800A6884(&((StageGeometry *)D_800C3EA0)->quads[j].r0, i, (u8 *)color++);
                        func_800A6884(&((StageGeometry *)D_800C3EA0)->quads[j].r1, i, (u8 *)color++);
                        func_800A6884(&((StageGeometry *)D_800C3EA0)->quads[j].r2, i, (u8 *)color++);
                        func_800A6884(&((StageGeometry *)D_800C3EA0)->quads[j].r3, i, (u8 *)color++);
                        func_800A6884(&((StageGeometry *)D_800C3EA0)->flats[j].r0, i, (u8 *)color++);
                    } else {
                        color += 5;
                    }
                    if (D_800C3D50[0] != NULL) {
                        func_800A6884(&D_800C3D50[0]->fills[j].r0, i, (u8 *)color++);
                    } else {
                        color++;
                    }
                }
                for (j = 0; j < 2; j++) {
                    if (D_800C3D50[0] != NULL) {
                        func_800A6884(&D_800C3D50[0]->fades[j].r0, i, (u8 *)color++);
                        func_800A6884(&D_800C3D50[0]->fades[j].r1, i, (u8 *)color++);
                        func_800A6884(&D_800C3D50[0]->fades[j].r2, i, (u8 *)color++);
                        func_800A6884(&D_800C3D50[0]->fades[j].r3, i, (u8 *)color++);
                    } else {
                        color += 4;
                    }
                }
                func_800A6884((u8 *)D_800D2D40, i, (u8 *)color++);
                func_800A6884((u8 *)D_800D2D48, i, (u8 *)color);
            }
        }
        LoadImage(&D_800D3600.rect, (u_long *)D_800D3600.pixels2);
    }
}

/* Release the stage image: detach the stage object's active image
 * animations from their targets, restore the image's VRAM and stop it; then
 * restore the stage colours (800A64E4) and free their saved copies. */
void func_800A6F98(void) {
    ImageAnim *anim;
    s32 i;

    if (D_800D3600.active) {
        anim = D_800D3368[31]->images;
        for (i = 0; i < D_800D3368[31]->imageCount; i++, anim++) {
            if (anim->active) {
                anim->target = NULL;
            }
        }
        LoadImage(&D_800D3600.rect, (u_long *)D_800D3600.pixels);
        DrawSync(0);
        func_800A429C(&D_800D3600);
    }
    func_800A64E4();
    heap_free(D_800C3AC4);
    heap_free(D_800C3AC8);
}

/* Build a surface from `table`: a scaled centre per ring (offset by
 * ox/oy/oz), each strand's points (segment length and sag), and two textured
 * triangles per point pair between neighbouring rings, their texture
 * spanning u_span x v_span from (tx, ty) with the CLUT at (clut_x, clut_y);
 * then `count` zeroed entries (using its signed low halfword). The table's
 * first halfword is the ring count; h0 is the mesh identifier set by the caller.
 * The texture page origin is tx/ty rounded down to a multiple of 64/256 as a
 * halfword; the u/v bases and steps are signed halfwords, and the cells
 * between two strands use the smaller of their point counts.
 * On an allocation failure the surface is left empty; the original clears
 * centres before passing NULL to the free service, even when a centre block
 * was allocated. */
void func_800A7064(Surface *surface, u16 *table, s32 angle_base, s32 scale, s16 ox, s16 oy, s16 oz,
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

    surface->rings = *table++;
    surface->polys = *table * 2;
    heap_select_owner_tag(4, 0);
    table++;
    centre = heap_alloc(surface->rings * sizeof(SVECTOR), 0);
    if (centre == NULL) {
        surface->centres = NULL;
        return;
    }
    surface->centres = centre;
    for (i = 0; i < surface->rings; i++) {
        centre->vx = (*table++ + ox) * scale / 4096;
        centre->vy = (*table++ + oy) * scale / 4096;
        centre->vz = (*table++ + oz) * scale / 4096;
        centre++;
    }
    total = table[surface->rings];
    surface->points = total + surface->rings;
    rings = heap_alloc(surface->rings * sizeof(SurfacePoint *), 0);
    if (rings == NULL) {
        surface->centres = NULL;
        heap_free(NULL);
        return;
    }
    surface->strands = rings;
    counts = table;
    radii = table + surface->rings + 1;
    angles = (u8 *)(radii + total);
    point = heap_alloc((total + surface->rings) * sizeof(SurfacePoint), 0);
    if (point == NULL) {
        surface->centres = NULL;
        heap_free(NULL);
        heap_free(surface->strands);
        return;
    }
    centre = surface->centres;
    points_base = point;
    for (i = 0; i < surface->rings; rings++, counts++, centre++, i++) {
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
    polys = heap_alloc(surface->polys * sizeof(SurfacePoly), 0);
    if (polys == NULL) {
        surface->centres = NULL;
        heap_free(NULL);
        heap_free(surface->strands);
        heap_free(points_base);
        return;
    }
    surface->polyList = polys;
    page_x = tx / 64 * 64;
    page_y = ty / 256 * 256;
    tpage = GetTPage(0, 1, page_x, page_y);
    clut = GetClut(clut_x, clut_y);
    u_base = (tx - page_x) * 4;
    v_base = ty - page_y;
    start = 0;
    u_step = u_span / (surface->rings - 1);
    for (i = 0; i < surface->rings - 1; i++) {
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
    surface->b[0] = b0;
    surface->b[1] = b1;
    surface->b[2] = b2;
    surface->b[3] = b3;
    surface->b[4] = b4;
    surface->b[5] = b5;
    surface->entryCount = count;
    if ((s16)count > 0) {
        entry = heap_alloc((s16)count * sizeof(SurfaceEntry), 0);
        if (entry == NULL) {
            surface->entryCount = 0;
        }
        surface->entries = entry;
        for (i = 0; i < surface->entryCount; i++) {
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
        surface->entries = NULL;
    }
}

/* Simulate and draw a surface (hair or cloth): each strand's
 * segments hang from their start pulled by `wind` (plus each point's sag),
 * keep their length, stay above `floor` and are pushed out of the surface's
 * collision spheres; then the points' normals are averaged from their
 * triangles, and the visible triangles are lit (front and back colours) and
 * queued. As in the original, a triangle the GTE flags as off screen does
 * not advance the triangle pointer. */
void func_800A7948(Surface *surface, SVECTOR *wind, MATRIX *m, u32 *ot, s32 buffer, s32 scale,
                   s16 floor) {
    VECTOR d;
    SVECTOR normal;
    VECTOR e1;
    VECTOR e2;
    VECTOR n;
    SVECTOR spare; /* unused in the original; reserves 8 bytes */
    u8 rgb[4];
    s32 k; /* the sphere counter, then the GTE flag */
    s32 opz;
    s32 otz;
    SurfacePoint *p, *q;
    SurfacePoly *poly;
    SurfaceEntry *entry;
    s32 len, radius;
    s32 i;

    if (surface->centres == NULL) {
        return;
    }
    rgb[3] = surface->polyList->prim[0].code;
    for (i = 0; i < surface->rings; i++) {
        p = surface->strands[i];
        if (p->length == 0) {
            continue;
        }
        do {
            q = p + 1;
            d.vx = q->pos[0] - p->pos[0] + wind->vx;
            d.vy = q->pos[1] - p->pos[1] + wind->vy + p->sag;
            d.vz = q->pos[2] - p->pos[2] + wind->vz;
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
            q->pos[0] = p->pos[0] + p->length * d.vx * scale / 0x100000;
            q->pos[1] = p->pos[1] + p->length * d.vy * scale / 0x100000;
            q->pos[2] = p->pos[2] + p->length * d.vz * scale / 0x100000;
            if (floor < q->pos[1]) {
                q->pos[1] = floor;
            }
            entry = surface->entries;
            for (k = 0; k < surface->entryCount; k++, entry++) {
                d.vx = q->pos[0] - entry->h8;
                d.vy = q->pos[1] - entry->hA;
                d.vz = q->pos[2] - entry->hC;
                len = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
                radius = (s16)(entry->hE * scale / 4096);
                if (len >= radius) {
                    continue;
                }
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
                q->pos[0] = p->pos[0] + p->length * d.vx * scale / 0x100000;
                q->pos[1] = p->pos[1] + p->length * d.vy * scale / 0x100000;
                q->pos[2] = p->pos[2] + p->length * d.vz * scale / 0x100000;
            }
            p++;
        } while (p->length != 0);
    }
    p = *surface->strands;
    for (i = 0; i < surface->points; i++) {
        p->normalCount = 0;
        p->normal[0] = 0;
        p->normal[1] = 0;
        p->normal[2] = 0;
        p++;
    }
    poly = surface->polyList;
    p = *surface->strands;
    for (i = 0; i < surface->polys; i++, poly++) {
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
    p = *surface->strands;
    for (i = 0; i < surface->points; i++) {
        p->normal[0] /= (s16)p->normalCount;
        p->normal[1] /= (s16)p->normalCount;
        p->normal[2] /= (s16)p->normalCount;
        p++;
    }
    SetRotMatrix(m);
    SetTransMatrix(m);
    poly = surface->polyList;
    p = *surface->strands;
    for (i = 0; i < surface->polys; i++) {
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
        otz >>= model_ot_depth_shift;
        if (opz < 0) {
            rgb[0] = surface->b[0];
            rgb[1] = surface->b[1];
            rgb[2] = surface->b[2];
            normal.vx = p[poly->index[0]].normal[0];
            normal.vy = p[poly->index[0]].normal[1];
            normal.vz = p[poly->index[0]].normal[2];
        } else {
            rgb[0] = surface->b[3];
            rgb[1] = surface->b[4];
            rgb[2] = surface->b[5];
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

/* Free a surface's buffers (once). */
void func_800A8A88(Surface *surface) {
    if (surface->centres != NULL) {
        heap_free(surface->centres);
        heap_free(surface->strands[0]);
        heap_free(surface->strands);
        heap_free(surface->polyList);
        if (surface->entries != NULL) {
            heap_free(surface->entries);
        }
        surface->centres = NULL;
    }
}

/* Reset the battle scene: its flags, the effect and sprite pools sized by the
 * scene data, and the object and slot tables. */
void func_800A8B0C(void) {
    s32 i;

    D_800C3E88 = 0;
    D_800C3CF0 = 0;
    D_800C3D6C = 0;
    D_800C3D68 = 0;
    D_800C3B7C = 0;
    D_800C3B74 = 1;
    func_800A2234(&D_800C3D0C, SCENE_DATA->effectCount);
    func_800A2CA4(&D_800C3D04, SCENE_DATA->spriteCount);
    func_800B00D0();
    for (i = 0; i < 32; i++) {
        D_800D3368[i] = NULL;
    }
    for (i = 0; i < 20; i++) {
        D_800C3ACC[i].models = NULL;
    }
    for (i = 0; i < 2; i++) {
        D_800D3304[i].active = 0;
    }
}

/* Read a stage object's description through a stream pointer. */
#define OBJECT_DESC(p) ((ObjectDesc *)(p))

/* Create stage object index (unless it exists) from its script and model
 * files with its images placed at x, y, z, w (flag 1: the model file is
 * already set up, no images; 4: no script file; 0x40: a plain object with no
 * scripts; 0x80: the script file is shared; 2: its models stay in the loaded
 * group), its root at position when given. One pointer reads the object's
 * description and then walks its mesh stream (the original keeps both in one
 * variable). */
void func_800A8BF0(s32 index, u16 flags, ObjectScriptFile *script_file, ObjectModelFile *model_file, s16 x, s16 y,
                   s16 z, s16 w, SVECTOR *position) {
    BattleObject *object;
    ObjectScripts *scripts;
    struct ObjectData *data;
    ObjectHeader *header;
    void *images;
    u8 *models;
    u16 *hierarchy;
    s32 size;
    s32 i;
    s32 j;
    Surface *surface;
    s16 *stream;
    s32 keyCount;
    u8 *copy;
    s32 copySize;

    heap_select_owner_tag(4, 0);
    if (index < 32 && D_800D3368[index] == NULL) {
        object = heap_alloc(sizeof(BattleObject), 0);
        if (!(flags & 1)) {
            text_relocate_offset_table(model_file);
            text_relocate_offset_table(model_file->header);
        }
        object->ownSounds = 0;
        object->extraSounds = 0;
        if (!(flags & 4)) {
            text_relocate_offset_table(script_file);
            text_relocate_offset_table(script_file->data);
            scripts = script_file->scripts;
            text_relocate_offset_table(scripts);
            text_relocate_offset_table(scripts->animations);
            data = script_file->data;
            if (data->soundsEnd != data->sounds && sound_find_effect_bank(data->sounds, 0) == 0) {
                sound_add_effect_bank(data->sounds);
                object->ownSounds = 1;
            }
        }
        header = model_file->header;
        stream = (s16 *)header->desc;
        images = model_file->images;
        models = model_file->models;
        hierarchy = model_file->hierarchy;
        D_800D3368[index] = object;
        object->scale24 = OBJECT_DESC(stream)->size[0];
        object->scale26 = OBJECT_DESC(stream)->size[1];
        object->scale28 = OBJECT_DESC(stream)->size[2];
        object->field2A = OBJECT_DESC(stream)->field2A;
        object->flags4A = OBJECT_DESC(stream)->flags;
        size = (u8 *)hierarchy - models;
        if (object->flags4A & 0x200) {
            model_set_envmap_mapping(2, 2, 0x40, 0x40);
        }
        if (!(flags & 1)) {
            i = 0; /* the images' on flag (the original reuses the loop counter) */
            if (!(flags & 0x40)) {
                s32 bit = object->flags4A & 4;

                i = !bit;
            }
            model_load_image_list(images, i, x, y, i, z, w);
            D_800C3B70 = heap_alloc(size, 1);
            memcpy(D_800C3B70, models, size);
            for (D_800C3B6C = 0; D_800C3B6C < 20; D_800C3B6C++) {
                if (D_800C3ACC[D_800C3B6C].models == NULL) {
                    break;
                }
            }
            func_8009EBA8(D_800C3B70, &D_800C3ACC[D_800C3B6C]);
        }
        object->field0 = &D_800C3ACC[D_800C3B6C];
        if (!(flags & 0x40)) {
            if (object->flags4A & 4) {
                object->hierarchy = func_8009EC4C(object->field0, hierarchy, 2, 0, 0, 0, 0, 0);
            } else {
                object->hierarchy = func_8009EC4C(object->field0, hierarchy, 2, 1, x, y, z, w);
            }
        } else {
            object->hierarchy = func_8009EC4C(object->field0, hierarchy, 0, 0, 0, 0, 0, 0);
        }
        if (position != NULL) {
            object->hierarchy->translation[0] = position->vx;
            object->hierarchy->translation[1] = position->vy;
            object->hierarchy->translation[2] = position->vz;
        }
        if (!(flags & 1)) {
            object->placement[0] = x;
            object->placement[1] = y;
            object->placement[2] = z;
            object->placement[3] = w;
        } else {
            object->placement[0] = -1;
        }
        if (!(flags & 4) && !(flags & 0x80)) {
            object->scriptFile = script_file;
        } else {
            object->scriptFile = NULL;
        }
        for (i = 0; i < 2; i++) {
            SetPolyFT4(&object->shadow[i]);
            SetSemiTrans(&object->shadow[i], 1);
            object->shadow[i].r0 = SCENE_DATA->shadow[0];
            object->shadow[i].g0 = SCENE_DATA->shadow[1];
            object->shadow[i].b0 = SCENE_DATA->shadow[2];
            object->shadow[i].clut = GetClut(0x30, 0x1CC);
            object->shadow[i].tpage = GetTPage(0, 2, 0x380, 0);
            object->shadow[i].u0 = 0xC0;
            object->shadow[i].v0 = 0xC0;
            object->shadow[i].u1 = 0xFE;
            object->shadow[i].v1 = 0xC0;
            object->shadow[i].u2 = 0xC0;
            object->shadow[i].v2 = 0xFE;
            object->shadow[i].u3 = 0xFE;
            object->shadow[i].v3 = 0xFE;
        }
        if (!(flags & 0x40)) {
            object->scale1C = OBJECT_DESC(stream)->scale * SCENE_DATA->objectScale >> 12;
        } else {
            object->scale1C = OBJECT_DESC(stream)->scale;
        }
        object->channelCount = OBJECT_DESC(stream)->channelCount;
        func_800AA6E0(object);
        object->imageCount = OBJECT_DESC(stream)->imageAnimCount;
        if (object->imageCount != 0) {
            object->images = heap_alloc(object->imageCount * sizeof(ImageAnim), 0);
            for (i = 0; i < object->imageCount; i++) {
                object->images[i].active = 0;
                object->images[i].pixels = NULL;
                object->images[i].pixels2 = NULL;
                object->images[i].work = NULL;
            }
        }
        object->surfaceCount = OBJECT_DESC(stream)->meshCount;
        if (object->surfaceCount != 0) {
            stream = OBJECT_DESC(stream)->meshes;
            surface = heap_alloc(object->surfaceCount * sizeof(Surface), 0);
            object->surfaces = surface;
            for (i = 0; i < object->surfaceCount; i++, surface++) {
                keyCount = stream[17];
                surface->h0 = *stream++;
                func_800A7064(surface, header->meshData[i], *stream++, *stream++, *stream++, *stream++, *stream++,
                              keyCount, x + *stream++, y + *stream++, *stream++, *stream++, z + *stream++, w,
                              *stream++, *stream++, *stream++, *stream++, *stream++, *stream);
                stream += 2;
                for (j = 0; j < keyCount; j++) {
                    surface->entries[j].h6 = *stream++;
                    surface->entries[j].hE = *stream++;
                    surface->entries[j].h0 = *stream++;
                    surface->entries[j].h2 = *stream++;
                    surface->entries[j].h4 = *stream++;
                }
            }
        }
        object->slot = index;
        object->field22 = 0;
        if (D_800C3EB4[index].hidden && index < 11) {
            object->active = 0;
        } else {
            object->active = 1;
        }
        if (!(flags & 0x40)) {
            scripts = script_file->scripts;
            object->model = script_file->data;
            func_800AA898(object, &D_800C3D0C, scripts->scripts, scripts->animations);
            func_800AA934(object, object, &D_800C3D0C, 0);
            func_800AFF9C(object);
        }
        if (!(flags & 2)) {
            model_trim_group((ModelGroup *)D_800C3B70);
            model_unrelocate_group((ModelGroup *)D_800C3B70);
            copySize = heap_get_block_size(D_800C3B70);
            copy = heap_alloc(copySize, 0);
            memcpy(copy, D_800C3B70, copySize);
            heap_free(D_800C3B70);
            func_8009F794(object->field0, 0);
            func_8009EBA8(copy, object->field0);
            object->modelBlock = copy;
        } else {
            object->modelBlock = NULL;
        }
    }
}

/* Read the files of combatant slot's gear from directory 0x28 (D_800C3508)
 * into new buffers, listed in D_800C3B78: base + 1, base + 2 and, when the
 * gear's variant is within the gear's count, base + 2 + variant. */
void func_800A9540(s32 slot) {
    s32 saved0;
    s32 saved1;
    u8 gearId;
    s32 variant;
    FileRequest *entry;
    FileRequest *files;
    s32 base;
    s32 file;

    cd_get_selected_directory(&saved0, &saved1);
    cd_select_directory(0x28, 1);
    heap_select_owner_tag(4, 0);
    gearId = D_800CCCE8.records[slot].pilot.gearId;
    variant = D_800CCCE8.records[slot].gear.fileVariant;
    if (D_800C3508[gearId * 2 + 1] < variant) {
        variant = 0;
    }
    files = heap_alloc(sizeof(FileRequest) * 4, 1);
    D_800C3B78 = files;
    base = D_800C3508[gearId * 2];
    file = base + 1;
    entry = files;
    entry->file = file;
    entry->destination = heap_alloc(cd_get_aligned_file_size(file), 1);
    entry++;
    file = base + 2;
    entry->file = file;
    entry->destination = heap_alloc(cd_get_aligned_file_size(file), 0);
    entry++;
    if (variant != 0) {
        file += variant;
        entry->file = file;
        entry->destination = heap_alloc(cd_get_aligned_file_size(file), 1);
        entry++;
    }
    entry->file = 0;
    entry->destination = NULL;
    cd_read_file_list(D_800C3B78, 0, 0);
    cd_select_directory(saved0, saved1);
}

/* Load the battle's sound banks for set: banks 2 * set + 1 and 2 * set + 2,
 * each with a buffer of its size (800288EC), into a new record D_800C3B78. */
void func_800A96B4(s32 set) {
    s32 saved0;
    s32 saved1;
    FileRequest *banks;
    s32 bank;

    cd_get_selected_directory(&saved0, &saved1);
    cd_select_directory(0x28, 0);
    heap_select_owner_tag(4, 0);
    banks = heap_alloc(sizeof(FileRequest) * 3, 1);
    set *= 2;
    bank = set + 1;
    D_800C3B78 = banks;
    cd_get_pc_file_name(bank);
    banks[0].file = bank;
    banks[0].destination = heap_alloc(cd_get_aligned_file_size(bank), 1);
    bank = set + 2;
    banks[1].file = bank;
    banks[1].destination = heap_alloc(cd_get_aligned_file_size(bank), 1);
    banks[2].file = 0;
    banks[2].destination = NULL;
    cd_read_file_list(D_800C3B78, 0, 0);
    cd_select_directory(saved0, saved1);
}

/* Create stage gear object index from the files read by 800A9540, using
 * (texture_x, texture_y) and (clut_x, clut_y) as the VRAM placement bases
 * for its textures and CLUTs. With a variant file, also create its extra
 * parts as objects 2 * index + 13 + k attached to parts of the gear, then
 * free the files. The part table starts with the signed part count; the
 * part entries (parent part, then three offsets) follow two halfwords on.
 * A nonzero count copies the shared data even when negative; only positive
 * counts create child objects. */
void func_800A979C(s32 index, s16 texture_x, s16 texture_y, s16 clut_x, s16 clut_y) {
    GearPartFile *parts;
    s16 *entry;
    s16 count;
    s32 size;
    void *model;
    s32 k;
    s32 flags;
    s32 slot;

    func_800A8BF0(index, 0, D_800C3B78[1].destination, D_800C3B78[0].destination,
                  texture_x, texture_y, clut_x, clut_y, NULL);
    D_800D3368[index]->field38 = 1;
    D_800D3368[index]->field22 = 1;
    parts = D_800C3B78[2].destination;
    if (parts != NULL) {
        text_relocate_offset_table(parts);
        entry = parts->table;
        count = *entry;
        if (count != 0) {
            entry += 2;
            size = parts->end - parts->model;
            model = heap_alloc(size, 0);
            memcpy(model, parts->model, size);
            for (k = 0; k < count; k++) {
                flags = 7;
                if (k == 0) {
                    flags = 2;
                }
                if (k == count - 1) {
                    flags -= 2;
                }
                slot = index * 2 + 13 + k;
                func_800A8BF0(slot, flags, model, (ObjectModelFile *)parts->end,
                              texture_x, texture_y, clut_x, clut_y, NULL);
                D_800D3368[slot]->parentPart = *entry++;
                D_800D3368[slot]->field5C = index;
                D_800D3368[slot]->field5D = 2;
                D_800D3368[slot]->field36 = 1;
                D_800D3368[slot]->offset2[0] = *entry++;
                D_800D3368[slot]->offset2[1] = *entry++;
                D_800D3368[slot]->offset2[2] = *entry++;
            }
        } else {
            model_load_image_list(parts->model, 1, texture_x, texture_y, 1, clut_x, clut_y);
        }
        heap_free(parts);
    }
    heap_free(D_800C3B78);
    DrawSync(0);
    heap_free(D_800C3B78[0].destination);
}

/* Run the stage for the elapsed frames (two frames per step, at most three
 * steps): advance the waves and the highlight pulse, push the acting object
 * and the objects it overlaps apart (800B10EC), animate the objects, run the
 * effects, attach the child objects and draw the objects (the highlighted
 * slots in the pulse colour) and the sprites. */
void func_800A9A50(MATRIX *m, s32 arg1, u32 *ot, s32 buffer) {
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
    D_800D39E8 = (gpu_get_cos(D_800C3CF0) + 0x1000) / 800 + 4;
    D_800C3B7C += 0x80;
    D_800C3B80 = (gpu_get_cos(D_800C3B7C) + 0x1000) / 32;
    pulse[0] = func_800AA514(D_800C3B80, 32, SCENE_DATA->ambient[0]);
    pulse[1] = func_800AA514(D_800C3B80, 32, SCENE_DATA->ambient[1]);
    pulse[2] = func_800AA514(D_800C3B80, 32, SCENE_DATA->ambient[2]);
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
            func_800AAA20(*objects, &D_800C3D0C, steps, buffer, D_800CCC5C);
        }
    }
    if (D_800C3DF8 != 0) {
        func_800B026C(&D_800C3D0C, steps, 0, buffer);
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
                    SetBackColor(SCENE_DATA->ambient[0], SCENE_DATA->ambient[1], SCENE_DATA->ambient[2]);
                }
                if ((*objects)->flags4A & 0x40) {
                    model_box_test_mode = 0;
                } else {
                    model_box_test_mode = 1;
                }
                func_8009F844(*objects, m, (MATRIX *)arg1, 1, D_800CCC5C, ot, buffer);
                model_box_test_mode = 0;
            }
        }
    }
    func_800A2FD8(&D_800C3D04, m, steps, ot, buffer);
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
        if ((*slot)->modelBlock != NULL) {
            heap_free((*slot)->modelBlock);
            func_8009F794((*slot)->field0, 1);
        }
        if ((*slot)->ownSounds) {
            sound_remove_effect_bank((*slot)->model->sounds);
        }
        if ((*slot)->scriptFile != NULL) {
            heap_free((*slot)->scriptFile);
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
            heap_free((*slot)->hierarchy);
        }
        if (D_800D3368[index]->channelCount) {
            heap_free(D_800D3368[index]->channels);
        }
        if (D_800D3368[index]->imageCount != 0) {
            for (i = 0; i < D_800D3368[index]->imageCount; i++) {
                func_800A429C(&D_800D3368[index]->images[i]);
            }
            heap_free(D_800D3368[index]->images);
        }
        if (D_800D3368[index]->surfaceCount != 0) {
            for (i = 0; i < D_800D3368[index]->surfaceCount; i++) {
                func_800A8A88(&D_800D3368[index]->surfaces[i]);
            }
            heap_free(D_800D3368[index]->surfaces);
        }
        heap_free(D_800D3368[index]);
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
    task_active_main_count = 0;
    task_new_tasks_active = 1;
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
    ColorFade *channels;
    s32 i;

    if (object->channelCount != 0) {
        channels = heap_alloc(object->channelCount * sizeof(ColorFade), 0);
        for (i = 0; i < object->channelCount; i++) {
            channels[i].id = -1;
            channels[i].sprite = NULL;
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

/* The frame curve of mode: 800A3514, 800A3578 or 800A35C8 for 1-3, any other
 * the cosine curve 800A3490. */
FrameCurve func_800AA820(s32 mode) {
    switch (mode) {
    case 1:
        return (FrameCurve)func_800A3514;
    case 2:
        return (FrameCurve)func_800A3578;
    case 3:
        return (FrameCurve)func_800A35C8;
    default:
        return (FrameCurve)func_800A3490;
    }
}

/* Reset a battle object's state. */
void func_800AA898(BattleObject *object, EffectPool *pool, u8 **scripts, u8 **animations) {
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
    object->field3C = 0xFFFF;
    object->field5C = 0xFF;
    object->field39 = 0x6B;
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
                flags |= func_800A0838(pool, object->hierarchy, object->field3C, object->scale1C);
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
    MATRIX *m = (MATRIX *)0x1F800000;
    SVECTOR offset;

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
                MulMatrix2(m, &object->hierarchy->transform);
                MulMatrix2(m, &object->hierarchy->world);
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

/* A script jump: offset is in bytes from the command's start. */
#define SCRIPT_JUMP(start, offset) ((u16 *)((u8 *)(start) + (offset)))

/* Run a battle object's effect script for steps frames: first move it by its
 * angular and linear velocities (substeps + 1 times), take a pending jump
 * whose condition came true (2E distance, 37 ground, 36 timer), then run
 * its commands until one waits. flags are the animation flags of the step
 * (-1 when called to start a script). The command word is signed; its
 * opcode and argument views are unsigned bytes. Jump offsets are in bytes
 * from the command's start (SCRIPT_JUMP). b0..b3 are the shared byte
 * temporaries that most byte-taking commands decode the low/high bytes of
 * their first and second parameter words into; 11 keeps its high byte in
 * loop, 1D its high bytes in mode/smooth (passing the second low byte as
 * (u8)word) and 31 its loop count in count, while 14 and 3E also keep a
 * slot and the saved queueCount, or a flag and a code, in them. m1 is the
 * first word's high byte of 1B, 25 and the camera commands. 11 and 12 share
 * the looked-up animation (pointers of their own change the allocation),
 * and 1F looks its slot up into op (a slot variable of its own spills op
 * out of $fp at the dispatch). The camera commands pass their second high
 * byte (b3) as a short, like their halfword arguments. The turn towards a
 * position passes a roll that is only ever zero. A relative camera turn (67
 * with 0x20 in b2) starts at the old angle plus angle, and its end starts
 * at the old angle: with 0x40 the turn ends at the old angle plus word,
 * otherwise at the computed or absolute end. */
void func_800AAD54(BattleObject *object, EffectPool *pool, s32 flags, s32 steps, s32 substeps) {
    VECTOR delta;
    SVECTOR velocity;
    VECTOR ground;
    SVECTOR probe;
    s32 savedA;
    s32 savedB;
    s32 i;
    s16 word;
    u16 value;
    BattleObject *self;
    u16 *pc;
    u16 *start;
    s32 running;
    s32 reloadScene;
    u8 op;
    u8 arg;
    u8 b0;
    u8 b1;
    u8 b2;
    u8 b3;
    Animation *animation;
    u8 m1;

    if (steps == 0 || object->script == NULL) {
        return;
    }
    heap_select_owner_tag(4, 0);
    reloadScene = 0;
    self = object;
    for (i = 0; i < substeps + 1; i++) {
        object->motion[0] += object->motion[3];
        object->motion[1] += object->motion[4];
        object->motion[2] += object->motion[5];
        object->motion[6] += object->motion[9];
        object->motion[7] += object->motion[10];
        object->motion[8] += object->motion[11];
        object->hierarchy->rotation.vx += object->motion[0] >> 3;
        object->hierarchy->rotation.vy += object->motion[1] >> 3;
        object->hierarchy->rotation.vz += object->motion[2] >> 3;
        velocity.vx = object->motion[6] * object->hierarchy->scale[0] >> 12;
        velocity.vy = object->motion[7] * object->hierarchy->scale[1] >> 12;
        velocity.vz = object->motion[8] * object->hierarchy->scale[2] >> 12;
        ApplyMatrix(&object->hierarchy->world, &velocity, &delta);
        object->hierarchy->translation[0] += object->scale1C * delta.vx >> 12;
        object->hierarchy->translation[1] += object->scale1C * delta.vy >> 12;
        object->hierarchy->translation[2] += object->scale1C * delta.vz >> 12;
    }

    running = 1;
    pc = (u16 *)object->script;
    if (OBJECT_AT_SCRIPT(object) != NULL && func_800AEEF8(object) <= OBJECT_AT_DISTANCE(object)) {
        pc = OBJECT_AT_SCRIPT(object);
        OBJECT_AT_SCRIPT(object) = NULL;
    } else {
        if (OBJECT_GROUND_SCRIPT(object) != NULL) {
            probe.vx = object->hierarchy->translation[0];
            probe.vy = 0;
            probe.vz = object->hierarchy->translation[2];
            object->field1E = func_800A5914(&probe, object->field1E, 4);
            if (object->field1E < 0) {
                object->field1E = func_800A579C(&probe);
            }
            func_800A5870(&probe, object->field1E, &ground);
            if (probe.vy < object->hierarchy->translation[1]) {
                object->hierarchy->translation[1] = probe.vy;
                pc = OBJECT_GROUND_SCRIPT(object);
                OBJECT_GROUND_SCRIPT(object) = NULL;
                goto chosen;
            }
        }
        if (OBJECT_TIMER_SCRIPT(object) != NULL) {
            OBJECT_TIMER(object) += steps;
            if (OBJECT_TIMER(object) >= OBJECT_TIMER_LIMIT(object)) {
                pc = OBJECT_TIMER_SCRIPT(object);
                OBJECT_TIMER_SCRIPT(object) = NULL;
            }
        }
    }
chosen:
    if (object->field58 != 0) {
        func_800AEF68(object);
    }

    while (running) {
        start = pc;
        arg = (word = *pc++) >> 8;
        op = word;
        switch (op) {
        case 0x00: /* end */
            pc = start;
            running = 0;
            break;
        case 0x01: /* wait frames */
            if (flags != -1) {
                word = *pc++;
                object->scriptWait += steps;
                if ((s16)object->scriptWait < (s16)word) {
                    pc = start;
                    running = 0;
                } else {
                    object->scriptWait = 0;
                    steps = 0;
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x02:
            if (object->field35 != 0) {
                if (func_800BF6F8() != 0) {
                    pc = start;
                    running = 0;
                    break;
                }
                task_active_main_count = 0;
                task_new_tasks_active = 0;
                object->field35 = 0;
                func_80080C6C(object->slot);
            } else {
                reloadScene = 1;
            }
            break;
        case 0x03:
            if (object->field35 != 0) {
                if (func_800BF6F8() != 0) {
                    pc = start;
                    running = 0;
                    break;
                }
                task_active_main_count = 0;
                task_new_tasks_active = 0;
                object->field35 = 0;
                func_80080C6C(object->slot);
            }
            break;
        case 0x04: /* load the extra file */
            word = *pc++;
            if (object->extra == NULL) {
                s32 file;

                cd_get_selected_directory(&savedA, &savedB);
                cd_select_directory(0x28, 2);
                file = arg + D_800C3530[(s16)word];
                object->extra = heap_alloc(cd_get_aligned_file_size(file), 0);
                cd_read_file(file, object->extra, 0, 0);
                cd_select_directory(savedA, savedB);
            }
            break;
        case 0x05: /* set up the extra file once loaded */
            if (object->field23 == 0) {
                if (object->extra != NULL && object->moreAnimations == NULL) {
                    if (cd_get_pending_read_count() == 0) {
                        struct ObjectData *data;
                        ObjectScripts *scripts;
                        u8 **animations;
                        DVECTOR image;
                        DVECTOR clut;

                        text_relocate_offset_table(object->extra);
                        data = ((ObjectScriptFile *)object->extra)->data;
                        text_relocate_offset_table(data);
                        object->extraData = data;
                        if (data->imagesEnd != data->images) {
                            model_load_image_list(data->images, 1, 0x380, 0x100, 1, 0, 0x1D0);
                        }
                        if (object->extraData->soundsEnd != object->extraData->sounds
                            && sound_find_effect_bank(object->extraData->sounds, 0) == 0) {
                            sound_add_effect_bank(object->extraData->sounds);
                            object->extraSounds = 1;
                        } else {
                            object->extraSounds = 0;
                        }
                        scripts = ((ObjectScriptFile *)object->extra)->scripts;
                        text_relocate_offset_table(scripts);
                        animations = scripts->animations;
                        text_relocate_offset_table(animations);
                        object->moreAnimations = animations;
                        DrawSync(0);
                        heap_shrink_block((u8 *)object->extra, (u8 *)object->extraData->soundsEnd - (u8 *)object->extra);
                        if (object->extraData->sounds != object->extraData->image) {
                            image.vx = 0x380;
                            image.vy = 0x100;
                            clut.vx = 0;
                            clut.vy = 0x1D0;
                            sprite_resolve_resource(sprite_effect_source, object->extraData->image, image, clut, 0);
                        }
                        object->field23 = 1;
                    }
                    pc = start;
                    running = 0;
                    break;
                }
            } else {
                if (cd_get_pending_read_count() != 0) {
                    pc = start;
                    running = 0;
                    break;
                }
                object->field23 = 0;
            }
            break;
        case 0x06:
            func_800B0060(object);
            break;
        case 0x07:
            cd_stop_read(0);
            break;
        case 0x08:
            func_800A2ACC(pool, object->hierarchy);
            func_800AEEEC(object);
            break;
        case 0x09:
            func_800A2BB8(pool, object->hierarchy, arg);
            break;
        case 0x0A:
            func_800A216C(pool, object->hierarchy, arg, 7);
            break;
        case 0x0B: /* stop the parts' effects and reset their transforms */
            {
                ModelPart *part = object->hierarchy;
                s32 count;
                s32 k;

                func_800A2ACC(pool, part);
                count = part->index - 1;
                for (k = 0; k < count; k++) {
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
            }
            break;
        case 0x0C: /* stop moving */
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
            break;
        case 0x0D:
            func_800A216C(pool, object->hierarchy, arg, 1);
            break;
        case 0x0E:
            func_800A216C(pool, object->hierarchy, arg, 2);
            break;
        case 0x0F:
            func_800BCAD0();
            func_800B00F4(pool);
            D_800C3DF8 = 0;
            break;
        case 0x10: /* pose */
            func_800A1B50(object->hierarchy, (s16 *)func_800AF518(object, arg, &i));
            break;
        case 0x11: /* play an animation */
            {
                u8 loop;

                animation = (Animation *)func_800AF518(object, arg, &i);
                word = *pc++;
                if (i == 0) {
                    loop = word >> 8;
                    b0 = word;
                    func_800A2434(pool, object->hierarchy, (u16 *)animation, loop, b0);
                    flags = -1;
                    object->field8E = ABS(ANIMATION_SPAN(animation) * (object->scale1C * object->hierarchy->scale[2] >> 12) >> 12);
                    func_800AE1BC(object, animation, loop);
                }
            }
            break;
        case 0x12:
            animation = (Animation *)func_800AF518(object, arg, &i);
            word = *pc++;
            if (i == 0) {
                b1 = word >> 8;
                b0 = word;
                func_800A2704(pool, object->hierarchy, (u16 *)animation, b1, b0);
                flags = -1;
                object->field8E = ABS(ANIMATION_SPAN(animation) * (object->scale1C * object->hierarchy->scale[2] >> 12) >> 12);
            }
            break;
        case 0x13:
            b1 = (word = *pc++) >> 8;
            b0 = word;
            b3 = (word = *pc++) >> 8;
            b2 = word;
            func_800A1CF4(pool, object->hierarchy, (s16 *)func_800AF518(object, b0, &i), b3, arg, b2, b1);
            flags = -1;
            func_800AEEEC(object);
            break;
        case 0x14: /* start a script on the objects of a mask */
            word = *pc++;
            b0 = word;
            b1 = word >> 8;
            b3 = func_800AF438(object, b0, &word);
            func_800AF438(object, arg, &word);
            b2 = object->queueCount;
            if (arg == 0xFD) {
                object->queueCount = 0;
            }
            for (i = 0; i < 13; i++) {
                if (((s16)word >> i) & 1) {
                    if (b0 == 0xFF) {
                        func_800AA934(D_800D3368[i], D_800D3368[i], pool, b1);
                    } else {
                        func_800AA934(D_800D3368[i], D_800D3368[b3], pool, b1);
                    }
                }
            }
            object->queueCount = b2;
            if (arg == 0xFD) {
                return;
            }
            break;
        case 0x15: /* create a copy of the object */
            {
                BattleObject *created;
                ModelPart *parts;

                word = *pc++;
                for (i = 0x13; i < 0x1F; i++) {
                    if (D_800D3368[i] == NULL) {
                        created = heap_alloc(sizeof(BattleObject), 1);
                        break;
                    }
                }
                *created = *object;
                D_800D3368[i] = created;
                created->animation = -1;
                created->field3C = 0xFFFF;
                created->scriptWait = 0;
                created->field58 = 0;
                created->field5C = 0xFF;
                created->placement[0] = -1;
                created->ownSounds = 0;
                created->modelBlock = NULL;
                created->scriptFile = NULL;
                created->field39 = 0x6B;
                created->script = NULL;
                created->slot2 = object->slot;
                created->slot = i;
                created->surfaceCount = 0;
                created->imageCount = 0;
                func_800AA6E0(created);
                parts = heap_alloc(object->hierarchy->index * sizeof(ModelPart), 1);
                created->hierarchy = parts;
                for (i = 0; i < object->hierarchy->index; i++) {
                    parts[i] = object->hierarchy[i];
                    if (object->hierarchy[i].parent != NULL) {
                        parts[i].parent =
                            &parts[((u8 *)object->hierarchy[i].parent - (u8 *)object->hierarchy) / sizeof(ModelPart)];
                    }
                    parts[i].visible = 0;
                    parts[i].effects[0] = NULL;
                    parts[i].effects[1] = NULL;
                }
                func_800AF180(pool, (s16)word, object->hierarchy, parts);
                if (arg != 0xFF) {
                    func_800AA934(created, created, pool, arg);
                }
            }
            break;
        case 0x16:
            func_800AF270(object->hierarchy, D_800D3368[object->slot2]->hierarchy);
            func_800A9FF0(object->slot);
            if (arg != 0xFF) {
                func_800AA934(D_800D3368[object->slot2], D_800D3368[object->slot2], pool, arg);
            }
            return;
        case 0x17:
            func_800A9FF0(object->slot);
            return;
        case 0x18:
            {
                func_800AE1BC(object, (Animation *)func_800AF518(object, arg, &i), (s16)*pc++);
            }
            break;
        case 0x19:
            func_800AEEEC(object);
            break;
        case 0x1A: /* move an image */
            {
                RECT rect;
                u16 x;
                u16 y;
                s16 w;

                rect.x = *pc++;
                rect.y = *pc++;
                x = *pc++;
                y = *pc++;
                w = *pc++;
                rect.w = (w + 1) / 2 * 2;
                rect.h = *pc++;
                if (arg & 1) {
                    if (object->placement[0] < 0) {
                        break;
                    }
                    rect.x += object->placement[0];
                    rect.y += object->placement[1];
                    x += object->placement[0];
                    y += object->placement[1];
                }
                MoveImage(&rect, (s16)x, (s16)y);
            }
            break;
        case 0x1B: /* start an image animation */
            if (arg < object->imageCount) {
                ImageAnim *target;
                ColorRow *colors;
                FrameCurve curve;
                s16 x, y, z, x2, y2, z2, x3, y3;

                word = *pc++;
                b0 = word;
                if (b0 != 0xFF && b0 < object->imageCount) {
                    target = &object->images[b0];
                } else {
                    target = NULL;
                }
                if (((m1 = (s16)word >> 8) & 0x7F) < 4) {
                    colors = NULL;
                } else {
                    colors = (ColorRow *)D_800D2FC0;
                }
                word = *pc++;
                b2 = word;
                b3 = word >> 8;
                curve = func_800AA820(b3);
                x = *pc++;
                y = *pc++;
                z = *pc++;
                x2 = *pc++;
                y2 = *pc++;
                z2 = *pc++;
                x3 = *pc++;
                y3 = *pc++;
                if (m1 & 0x80) {
                    if (object->placement[0] < 0) {
                        pc += 5;
                        break;
                    }
                    x3 += object->placement[2];
                    y3 += object->placement[3];
                    if ((b2 & 0xF) == 1) {
                        x += object->placement[2];
                        y += object->placement[3];
                    }
                    if ((b2 >> 4) == 1) {
                        x2 += object->placement[2];
                        y2 += object->placement[3];
                    }
                }
                func_800A3640(&object->images[arg], target, m1 & 0x7F, b2 | 0x700, colors, x, y, z,
                              x2, y2, z2, x3, y3, (s16)*pc++, (s16)*pc++, (s16)*pc++, (s16)*pc++,
                              (s16)*pc++, curve);
            } else {
                pc += 15;
            }
            break;
        case 0x1C:
            if (arg < object->imageCount) {
                func_800A429C(&object->images[arg]);
            }
            break;
        case 0x1D: /* start a tween of a part */
            {
                u8 mode;
                u8 smooth;

                mode = (word = *pc++) >> 8;
                b0 = word;
                smooth = (word = *pc++) >> 8;
                func_800AF678(object, pool, &object->hierarchy[arg], b0, mode, (u8)word, smooth, *pc++, *pc++,
                              *pc++, *pc++, *pc++, *pc++, *pc++);
                flags = -1;
            }
            break;
        case 0x1E:
            object->field37 = arg;
            break;
        case 0x1F: /* continue on another object (the slot reuses op) */
            op = func_800AF438(self, arg, &word);
            if (D_800D3368[op] != NULL) {
                object = D_800D3368[op];
            }
            break;
        case 0x20: /* wait for the animation to loop */
            if (flags != -1) {
                if (flags & 0x100) {
                    pc = start;
                    running = 0;
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x21:
            object->field3C = arg;
            if (flags != -1) {
                if (flags & 1) {
                    pc = start;
                    running = 0;
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x22: /* wait for the animation to loop word times */
            if (flags != -1) {
                word = *pc++;
                if (arg == 0xFF) {
                    if (!(flags & 0x400)) {
                        pc = start;
                        running = 0;
                    } else {
                        object->field42++;
                        if (object->field42 < (s16)word) {
                            pc = start;
                            running = 0;
                        } else {
                            object->field42 = 0;
                        }
                    }
                } else {
                    object->field3C = arg;
                    if (!(flags & 4)) {
                        pc = start;
                        running = 0;
                    } else {
                        object->field42++;
                        if (object->field42 < (s16)word) {
                            pc = start;
                            running = 0;
                        } else {
                            object->field42 = 0;
                        }
                    }
                }
            } else {
                pc = start;
                running = 0;
            }
            break;
        case 0x23:
            func_800AFA98(object, &object->hierarchy[(s16)*pc++], arg);
            break;
        case 0x24:
            if (object != NULL) {
                object->active = arg & 1;
            }
            break;
        case 0x25: /* attach the objects of a mask to a part */
            {
                MATRIX m;
                VECTOR offset;
                VECTOR axisX;
                VECTOR axisY;
                VECTOR axisZ;
                SVECTOR unit;
                u16 ax, ay, az;

                word = *pc++;
                m1 = word >> 8;
                b0 = word;
                func_800AF438(object, b0, &word);
                ax = *pc++;
                ay = *pc++;
                az = *pc++;
                for (i = 0; i < 13; i++) {
                    if (((s16)word >> i) & 1) {
                        if (D_800D3368[i] != NULL) {
                            D_800D3368[i]->parentPart = m1;
                            D_800D3368[i]->field5C = object->slot;
                            D_800D3368[i]->field5D = arg & 2;
                            D_800D3368[i]->field36 = 1;
                            if (arg & 1) {
                                ModelPart *at = &object->hierarchy[m1];

                                SetRotMatrix(&at->world);
                                m.t[0] = 0;
                                m.t[1] = 0;
                                m.t[2] = 0;
                                SetTransMatrix(&m);
                                unit.vx = 0x1000;
                                unit.vy = 0;
                                unit.vz = 0;
                                gte_ldv0(&unit);
                                gte_rtv0tr();
                                gte_stlvnl(&axisX);
                                unit.vx = 0;
                                unit.vy = 0x1000;
                                gte_ldv0(&unit);
                                gte_rtv0tr();
                                gte_stlvnl(&axisY);
                                unit.vy = 0;
                                unit.vz = 0x1000;
                                gte_ldv0(&unit);
                                gte_rtv0tr();
                                gte_stlvnl(&axisZ);
                                offset.vx = D_800D3368[i]->hierarchy->translation[0] - at->world.t[0];
                                offset.vy = D_800D3368[i]->hierarchy->translation[1] - at->world.t[1];
                                offset.vz = D_800D3368[i]->hierarchy->translation[2] - at->world.t[2];
                                D_800D3368[i]->offset2[0] = func_800AF2C4(&offset, &axisY, &axisZ, object->scale1C);
                                D_800D3368[i]->offset2[1] = func_800AF2C4(&offset, &axisZ, &axisX, object->scale1C);
                                D_800D3368[i]->offset2[2] = func_800AF2C4(&offset, &axisX, &axisY, object->scale1C);
                            } else {
                                D_800D3368[i]->offset2[0] = ax;
                                D_800D3368[i]->offset2[1] = ay;
                                D_800D3368[i]->offset2[2] = az;
                            }
                        }
                    }
                }
            }
            break;
        case 0x26: /* detach the objects of a mask */
            func_800AF438(object, arg, &word);
            for (i = 0; i < 13; i++) {
                if (((s16)word >> i) & 1) {
                    if (D_800D3368[i] != NULL) {
                        D_800D3368[i]->field5C = 0xFF;
                    }
                }
            }
            break;
        case 0x27:
            if (object->field37) {
                func_8009F1C4(object->hierarchy, object->scale1C);
            } else {
                func_8009EF3C(object->hierarchy, object->scale1C);
            }
            break;
        case 0x28:
        case 0x29: /* wait for a part's effect over a distance */
            {
                s32 distance = func_800AEEF8(object);
                ModelPart *part;
                s32 frames;
                s32 found;

                if (object->field8E == 0) {
                    object->field8E = 1;
                }
                word = *pc++;
                b1 = word >> 8;
                b0 = word;
                part = &object->hierarchy[b0];
                frames = distance / object->field8E;
                found = 0;
                if (part->effects[0] != NULL) {
                    found = b1 == (s16)part->effects[0]->time;
                } else if (part->effects[1] != NULL && (s16)part->effects[1]->time == b1) {
                    found = 1;
                }
                if (found) {
                    if (op == 0x28) {
                        if (frames >= arg) {
                            pc = start;
                            running = 0;
                        }
                    } else if (frames < arg) {
                        pc = start;
                        running = 0;
                    }
                } else {
                    pc = start;
                    running = 0;
                }
            }
            break;
        case 0x2A:
            if (func_800AEEF8(object) >= object->field8E) {
                pc = start;
                running = 0;
            }
            break;
        case 0x2B:
            if (func_800AEEF8(object) <= object->field8E) {
                pc = start;
                running = 0;
            }
            break;
        case 0x2C:
        case 0x2D: /* wait for a distance to a point */
            {
                ModelPart *root = object->hierarchy;
                s32 dx = (s16)*pc++ - root->translation[0];
                s32 dy = (s16)*pc++ - root->translation[1];
                s32 dz = (s16)*pc++ - root->translation[2];
                s32 distance = SquareRoot0(dx * dx + dy * dy + dz * dz);

                if (op == 0x2C) {
                    if (distance >= object->field8E) {
                        pc = start;
                        running = 0;
                    }
                } else if (object->field8E >= distance) {
                    pc = start;
                    running = 0;
                }
            }
            break;
        case 0x2E:
            OBJECT_AT_DISTANCE(object) = object->field8E;
            word = *pc++;
            if (arg) {
                OBJECT_AT_SCRIPT(object) = SCRIPT_JUMP(start, (s16)word);
            } else {
                OBJECT_AT_SCRIPT(object) = NULL;
            }
            break;
        case 0x2F:
            if (func_800BF6F8() != 0) {
                pc = start;
                running = 0;
            }
            break;
        case 0x30: /* reset a loop counter */
            *pc++ = 0;
            break;
        case 0x31: /* loop */
            {
                u16 *counter;
                s32 count;

                word = *pc++;
                counter = SCRIPT_JUMP(start, (s16)word);
                word = *counter++;
                count = (u8)(word >> 8);
                word = *counter + 1;
                *counter++ = word;
                if ((s16)word < count) {
                    pc = counter;
                }
            }
            break;
        case 0x32: /* jump */
            word = *pc++;
            pc = SCRIPT_JUMP(start, (s16)word);
            break;
        case 0x33:
            word = *pc++;
            if (D_800D36B8 != 0) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x34:
            word = *pc++;
            if (object->field22 == 0) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x35:
            word = *pc++;
            if (rand() >= 0x4000) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x36:
            OBJECT_TIMER(object) = 0;
            OBJECT_TIMER_LIMIT(object) = *pc++;
            word = *pc++;
            if (arg) {
                OBJECT_TIMER_SCRIPT(object) = SCRIPT_JUMP(start, (s16)word);
            } else {
                OBJECT_TIMER_SCRIPT(object) = NULL;
            }
            break;
        case 0x37:
            word = *pc++;
            if (arg) {
                OBJECT_GROUND_SCRIPT(object) = SCRIPT_JUMP(start, (s16)word);
            } else {
                OBJECT_GROUND_SCRIPT(object) = NULL;
            }
            break;
        case 0x38:
            word = *pc++;
            b0 = word;
            b1 = word >> 8;
            func_800AE098(pool, object->hierarchy, 0, arg, b0, b1, object->position[0], object->position[1],
                          object->position[2]);
            break;
        case 0x39:
            word = *pc++;
            b0 = word;
            b1 = word >> 8;
            func_800AE098(pool, object->hierarchy, 1, arg, b0, b1, object->position[0], object->position[1],
                          object->position[2]);
            break;
        case 0x3A:
            if ((func_800AA7DC(2) == 0xFE || object->field22 == 0 || object->field35 != 0) && func_800BF6F8() != 0) {
                pc = start;
                running = 0;
            }
            break;
        case 0x3B:
            word = *pc++;
            if ((D_800C48E8 >> object->slot) & 1) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x3C: /* play a sound */
            word = *pc++;
            b0 = word;
            b1 = word >> 8;
            sound_slide_effect_volume(b0 + func_800AE220(object, arg), 0, b1);
            break;
        case 0x3D: /* replay the queued scripts when one is arg */
            word = object->queueCount;
            if ((s16)word >= 2) {
                for (i = 1; i < (s16)word; i++) {
                    if (object->queueScripts[i - 1] == arg) {
                        goto replay;
                    }
                }
            }
            break;
        case 0x3E: /* jump when the targets were all hit so */
            b0 = 1;
            for (i = 0; i < 13; i++) {
                if ((D_800C3E30 >> i) & 1) {
                    switch (D_800C3FE8[D_800C360C - 1].codes[i]) {
                    case 0:
                    case 1:
                        b1 = 0;
                        break;
                    case 2:
                    case 3:
                    case 5:
                        b1 = 5;
                        break;
                    default:
                        b1 = 4;
                        break;
                    }
                    if (b1 != arg && arg < 8) {
                        b0 = 0;
                    }
                    if (arg == 8 && (D_800D3368[i] == NULL || !(D_800D3368[i]->flags4A & 2))) {
                        b0 = 0;
                    }
                }
            }
            word = *pc++;
            if (b0) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x3F:
            func_800BF6CC();
            break;
        case 0x40: /* turn to angles */
            {
                s16 x = *pc++;
                s16 y = *pc++;
                s16 z = *pc++;

                func_800ADF1C(pool, object->hierarchy, arg, x, y, z);
                flags = -1;
            }
            break;
        case 0x41: /* turn by angles */
            {
                s32 x = *pc++;
                s32 y = *pc++;
                s32 z = *pc++;

                x = (s16)(object->hierarchy->rotation.vx + x);
                y = (s16)(object->hierarchy->rotation.vy + y);
                z = (s16)(object->hierarchy->rotation.vz + z);
                func_800ADF1C(pool, object->hierarchy, arg, (s16)x, (s16)y, (s16)z);
                flags = -1;
            }
            break;
        case 0x42:
        case 0x43: /* turn towards the object's position */
            {
                ModelPart *root = object->hierarchy;
                s32 dy = object->position[1] - root->translation[1];
                s32 dx = object->position[0] - root->translation[0];
                s32 dz = object->position[2] - root->translation[2];
                s16 pitch;
                s16 yaw;
                s16 roll;

                if (op == 0x43) {
                    pitch = 0;
                    dy = 0;
                } else {
                    pitch = ratan2(dy, SquareRoot0(dx * dx + dz * dz));
                }
                yaw = ratan2(-dx, -dz);
                roll = 0;
                if (dx != 0 || dy != 0 || dz != 0) {
                    func_800ADF1C(pool, object->hierarchy, arg, pitch, yaw, roll);
                    flags = -1;
                }
            }
            break;
        case 0x44:
            object->motion[0] = *pc++;
            object->motion[1] = *pc++;
            object->motion[2] = *pc++;
            break;
        case 0x45:
            object->motion[0] += *pc++;
            object->motion[1] += *pc++;
            object->motion[2] += *pc++;
            break;
        case 0x46:
            object->motion[3] = *pc++;
            object->motion[4] = *pc++;
            object->motion[5] = *pc++;
            break;
        case 0x47:
            object->motion[3] += *pc++;
            object->motion[4] += *pc++;
            object->motion[5] += *pc++;
            break;
        case 0x48:
            object->field36 = arg;
            break;
        case 0x49: /* place */
            object->hierarchy->translation[0] = (s16)*pc++;
            object->hierarchy->translation[1] = (s16)*pc++;
            object->hierarchy->translation[2] = (s16)*pc++;
            break;
        case 0x4A: /* place at the object's distance from its position, or at a slot */
            if (arg == 0xFB) {
                s32 dx = object->hierarchy->translation[0] - object->position[0];
                s32 dy = object->hierarchy->translation[1] - object->position[1];
                s32 dz = object->hierarchy->translation[2] - object->position[2];
                s32 distance = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;

                object->hierarchy->translation[0] = object->position[0] + dx * object->field8E / distance;
                object->hierarchy->translation[1] = object->position[1] + dy * object->field8E / distance;
                object->hierarchy->translation[2] = object->position[2] + dz * object->field8E / distance;
            } else {
                u8 slot = func_800AF438(object, arg, &word);

                object->hierarchy->translation[0] = (u16)D_800C3EB4[slot].x;
                object->hierarchy->translation[2] = (u16)D_800C3EB4[slot].z;
            }
            break;
        case 0x4B:
            object->motion[6] = *pc++;
            object->motion[7] = *pc++;
            object->motion[8] = *pc++;
            break;
        case 0x4C:
            object->motion[6] += *pc++;
            object->motion[7] += *pc++;
            object->motion[8] += *pc++;
            break;
        case 0x4D:
            object->motion[9] = *pc++;
            object->motion[10] = *pc++;
            object->motion[11] = *pc++;
            break;
        case 0x4E:
            object->motion[9] += *pc++;
            object->motion[10] += *pc++;
            object->motion[11] += *pc++;
            break;
        case 0x4F: /* speed to reach the object's position in arg frames */
            {
                ModelPart *root = object->hierarchy;
                s32 dx = object->position[0] - root->translation[0];
                s32 dy = object->position[1] - root->translation[1];
                s32 dz = object->position[2] - root->translation[2];

                if (arg == 0) {
                    arg = 1;
                }
                object->motion[8] = (SquareRoot0(dx * dx + dy * dy + dz * dz) / arg << 12)
                                    / (object->scale1C * object->hierarchy->scale[2] >> 12) / 2;
                if (((ratan2(-dx, -dz) - (u16)object->hierarchy->rotation.vy + 0x400) & 0xFFF) < 0x800) {
                    object->motion[8] = -object->motion[8];
                }
            }
            break;
        case 0x50: /* set the position */
            object->field58 = 0;
            object->position[0] = *pc++;
            object->position[1] = *pc++;
            object->position[2] = *pc++;
            break;
        case 0x51: /* set the position to a slot's */
            {
                u8 slot;

                object->field58 = 0;
                slot = func_800AF438(object, arg, &word);
                object->position[0] = D_800C3EB4[slot].x;
                object->position[1] = D_800C3EB4[slot].y;
                object->position[2] = D_800C3EB4[slot].z;
            }
            break;
        case 0x52: /* follow a target */
            object->field58 = arg;
            object->targetPart = *pc++;
            object->offset[0] = *pc++;
            object->offset[1] = *pc++;
            object->offset[2] = *pc++;
            if (arg != 0) {
                func_800AEF68(object);
            }
            break;
        case 0x53: /* put the position on the ground */
            {
                VECTOR hit;
                SVECTOR point;

                point.vx = object->position[0];
                point.vy = 0;
                point.vz = object->position[2];
                func_800A5870(&point, func_800A579C(&point), &hit);
                object->position[1] = point.vy;
            }
            break;
        case 0x54:
            object->field8E = (s16)*pc++ * (object->scale1C * object->hierarchy->scale[2] >> 12) >> 12;
            break;
        case 0x55:
            object->field8E += (s16)*pc++ * (object->scale1C * object->hierarchy->scale[2] >> 12) >> 12;
            break;
        case 0x56:
            object->field8E += *pc++;
            break;
        case 0x57:
            i = func_800AF438(object, arg, &word);
            object->field8E += func_800AA650(i);
            break;
        case 0x58:
            object->field58 = arg;
            object->targetPart = 0;
            object->offset[0] = 0;
            object->offset[1] = 0;
            object->offset[2] = 0;
            if (arg != 0) {
                func_800AEF68(object);
            }
            break;
        case 0x59:
            object->field58 = 0;
            object->position[0] = SCENE_DATA->cameras[arg].eye[0];
            object->position[1] = SCENE_DATA->cameras[arg].eye[1];
            object->position[2] = SCENE_DATA->cameras[arg].eye[2];
            break;
        case 0x5A:
            object->field58 = 0;
            object->position[0] = SCENE_DATA->cameras[arg].lookAt[0];
            object->position[1] = SCENE_DATA->cameras[arg].lookAt[1];
            object->position[2] = SCENE_DATA->cameras[arg].lookAt[2];
            break;
        case 0x5B: /* set the queue mode; 2 replays the queue */
            word = object->queueCount;
            if (arg == 1 && (s16)word >= 2) {
                break;
            }
            object->queueCount = arg;
            if (arg != 2) {
                break;
            }
        replay:
            object->queueCount = 0;
            if ((s16)word < 2) {
                break;
            }
            for (i = 1; i < (s16)word; i++) {
                func_800AA934(object, D_800D3368[object->queueTargets[i - 1]], pool, object->queueScripts[i - 1]);
            }
            return;
        case 0x5C:
            word = *pc++;
            if (object->position[0] == object->hierarchy->translation[0]
                && object->position[1] == object->hierarchy->translation[1]
                && object->position[2] == object->hierarchy->translation[2]) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x5D:
            word = *pc++;
            object->hierarchy[(s16)word].field52 = arg;
            break;
        case 0x5E:
            word = *pc++;
            object->scale1C = word;
            break;
        case 0x5F:
            word = *pc++;
            object->flags4A = word;
            break;
        case 0x60: /* the centre of the slot's area */
            {
                s32 group = D_800C3EB4[object->slot].group;

                object->position[0] = (SCENE_DATA->areas[group].x0 + SCENE_DATA->areas[group].x1) >> 1;
                object->position[1] = 0;
                object->position[2] = (SCENE_DATA->areas[group].z0 + SCENE_DATA->areas[group].z1) >> 1;
            }
            break;
        case 0x61:
            word = *pc++;
            if (func_800885D0(object->slot)) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        case 0x62:
            func_800AFD98(object, &object->hierarchy[(s16)*pc++], arg, (s16)*pc++, (s16)*pc++, (s16)*pc++);
            break;
        case 0x63: /* start the object's own animation */
            word = *pc++;
            object->animationLoop = -1;
            object->animation = 0;
            object->animationFrame = 0;
            object->animationLength = arg;
            object->animationStart = (u8 *)pc;
            pc = SCRIPT_JUMP(start, (s16)word);
            break;
        case 0x64:
            OBJECT_FIELD3E(object) = *pc++;
            break;
        case 0x65:
        case 0x66: /* start a camera effect */
            {
                s16 camX, camY, camZ;
                s16 x, y;
                s16 mode;

                word = *pc++;
                m1 = word >> 8;
                b0 = word;
                word = *pc++;
                b3 = word >> 8;
                b2 = word;
                if (op == 0x65) {
                    camX = D_800D335C.vx;
                    camY = D_800D335C.vy;
                    camZ = D_800D335C.vz;
                } else {
                    camX = D_800D3354.vx;
                    camY = D_800D3354.vy;
                    camZ = D_800D3354.vz;
                }
                x = func_800AF438(object, m1, &value);
                value = *pc++;
                mode = 2;
                if (m1 == 0xF6) {
                    x = object->position[0];
                    y = object->position[2];
                    value += object->position[1];
                } else if (m1 == 0xF5) {
                    x = SCENE_DATA->cameras[b2].lookAt[0];
                    y = SCENE_DATA->cameras[b2].lookAt[2];
                    value += SCENE_DATA->cameras[b2].lookAt[1];
                } else if (m1 == 0xF4) {
                    x = SCENE_DATA->cameras[b2].eye[0];
                    y = SCENE_DATA->cameras[b2].eye[2];
                    value += SCENE_DATA->cameras[b2].eye[1];
                } else if (D_800D3368[x] != NULL) {
                    value = func_800AA600(x) * (s16)value / 4096;
                    mode = 0;
                    y = -1;
                    if (m1 == 0xF9) {
                        for (y = 0; y < 13; y++) {
                            if ((D_800C3E30 >> y) & 1) {
                                break;
                            }
                        }
                    }
                } else {
                    value = (s16)value / 64;
                    mode = 0;
                    y = -1;
                }
                ((void (*)())func_800B0164)(pool, op - 0x5E, arg + mode, b0, camX, camY, camZ, x, (s16)value, y, (s16)b3);
            }
            break;
        case 0x67: /* start a camera turn */
            {
                u16 angle;
                u16 base;
                u16 from;
                u16 to;

                word = *pc++;
                m1 = word >> 8;
                b0 = word;
                word = *pc++;
                b3 = word >> 8;
                b2 = word;
                angle = *pc++;
                word = *pc++;
                if (arg & 0x20) {
                    base = 0;
                } else {
                    base = object->hierarchy->rotation.vy;
                }
                arg &= 0x1F;
                if (m1 == 0) {
                    from = D_800C3B90 & 0xFFF;
                } else if (m1 == 1) {
                    from = D_800C3BA0 & 0xFFF;
                } else if (m1 == 2) {
                    from = D_800C3B94 & 0xFFF;
                } else {
                    if (m1 == 3) {
                        from = D_800C3B98;
                    } else if (m1 == 4) {
                        from = D_800C3BA4;
                    } else if (m1 == 5) {
                        from = D_800C3BA8;
                    } else if (m1 == 6) {
                        from = D_800C3B9C;
                    }
                    angle = (s16)angle * SCENE_DATA->objectScale >> 12;
                    word = (s16)word * SCENE_DATA->objectScale >> 12;
                }
                if (b2 & 0x20) {
                    to = from;
                    from += angle;
                } else {
                    if (m1 < 2) {
                        from = (angle + base) & 0xFFF;
                    } else if (m1 < 3) {
                        from = angle & 0xFFF;
                    } else {
                        from = angle;
                    }
                    to = from;
                }
                if (b2 & 0x40) {
                    to += (u16)word;
                } else if (m1 < 3) {
                    u16 start2;
                    s32 turn;

                    if (m1 < 2) {
                        start2 = (angle + base) & 0xFFF;
                    } else {
                        start2 = angle & 0xFFF;
                    }
                    turn = (u16)(word - angle) & 0xFFF;
                    to = turn;
                    if (turn >= 0x800) {
                        to = turn | 0xF000;
                    }
                    to += start2;
                } else {
                    to = (u16)word;
                }
                ((void (*)())func_800B0164)(pool, m1, arg + 2, b0, (s16)from, 0, 0, (s16)to, 0, 0, (s16)b3);
            }
            break;
        case 0x68: /* start the camera */
            {
                s32 dx = D_800D335C.vx - D_800D3354.vx;
                s32 dz = D_800D335C.vz - D_800D3354.vz;

                func_800B00F4(pool);
                func_800BCAA4();
                D_800C3DF8 = 1;
                D_800C3B84 = 0xFF;
                D_800C3BA0 = D_800C3B90 = ratan2(dx, dz) & 0xFFF;
                D_800C3B94 = 0;
                D_800C3B98 = 0;
                D_800C3B9C = 0;
                D_800C3B8C = 0;
                D_800C3BA8 = 0;
                D_800C3BA4 = 0;
            }
            break;
        case 0x69:
            if (D_800C3B84 != 0xFF) {
                if (D_800C3B88 & 1) {
                    pc = start;
                    running = 0;
                } else {
                    D_800C3B84 = 0xFF;
                }
            } else {
                D_800C3B84 = arg;
                pc = start;
                running = 0;
            }
            break;
        case 0x6A:
            D_800C3B8C = 1;
            break;
        case 0x6B:
            word = *pc++;
            object->hierarchy[(s16)word].yxz = arg;
            break;
        case 0x6C:
            if (cd_get_pending_read_count() != 0) {
                pc = start;
                running = 0;
            }
            break;
        case 0x6D:
            object->field38 = arg & 1;
            break;
        case 0x6E:
            {
                u8 index = func_800AF438(object, arg, &word);
                BattleObject *other;

                word = *pc++;
                other = D_800D3368[index];
                if (other != NULL && other->field38 == (word & 1)) {
                    pc = start;
                    running = 0;
                }
            }
            break;
        case 0x6F:
            if (arg) {
                object->field3A = D_800C3E30;
            } else {
                object->field3A = -1;
            }
            break;
        case 0x70:
            word = *pc++;
            if (object->field3A == D_800C3E30) {
                pc = SCRIPT_JUMP(start, (s16)word);
                running = 0;
            }
            break;
        case 0x71:
            word = *pc++;
            if (arg == 0) {
                D_800D39E4 = word;
            }
            break;
        case 0x72:
            if (sprite_single_action_done == 0) {
                pc = start;
            }
            running = 0;
            break;
        case 0x73:
            running = 0;
            func_800B8054((s16)D_800D39E4);
            break;
        case 0x74:
            func_800BF998();
            return;
        case 0x75: /* jump when facing the object's position */
            word = *pc++;
            if ((ratan2(object->hierarchy->translation[0] - object->position[0],
                        object->hierarchy->translation[2] - object->position[2])
                 & 0xFFF)
                == (object->hierarchy->rotation.vy & 0xFFF)) {
                pc = SCRIPT_JUMP(start, (s16)word);
            }
            break;
        default:
            pc--;
            running = 0;
            break;
        }
    }
    object->script = (u8 *)pc;
    if (reloadScene) {
        func_800B9258();
    }
}

/* Turn a part to rotation (x, y, z): at once for a duration below 2, else by
 * a turning effect (kind 0xFE) over duration frames along the shortest way
 * (x, y, z become the turns). */
void func_800ADF1C(EffectPool *pool, ModelPart *part, s32 duration, s32 x, s32 y, s32 z) {
    EffectEntry *entry;

    if (duration < 2) {
        part->rotation.vx = x;
        part->rotation.vy = y;
        part->rotation.vz = z;
        part->rotate = 1;
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
            entry->kind = 3;
            entry->field1 = 0;
            entry->tag = 0xFE;
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
            entry->time = 0;
            entry->duration = duration;
            part->effects[0] = entry;
        }
    }
}

/* Attach a homing turn (kind 0xFE) toward (x, y, z) to a part's rotation: type
 * 0 (step type 7) turns pitch and yaw, 1 (8) the yaw only. Each frame 800A0838
 * turns by at most param1 + (distance + time) * param2 / params[0] (the first
 * distance plus one), time growing by duration; it runs until released. */
void func_800AE098(EffectPool *pool, ModelPart *part, s32 type, s32 param1, s32 param2, s32 duration, s32 x,
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
        entry->kind = type + 7;
        entry->field1 = 0;
        entry->tag = 0xFE;
        dx = x - part->translation[0];
        dy = y - part->translation[1];
        dz = z - part->translation[2];
        entry->params[0] = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
        entry->params[1] = param1;
        entry->params[2] = param2;
        entry->params[3] = x;
        entry->params[4] = y;
        entry->params[5] = z;
        entry->time = 0;
        entry->duration = duration;
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
        return sprite_script_sound_bank->bank << 16;
    }
    if (source == 1) {
        return object->model->sounds->id << 16;
    }
    if (source == 2) {
        return object->extraData->sounds->id << 16;
    }
    if (source == 3) {
        return D_800C4924->id << 16;
    }
}

/* Run the events of object's animation for its current frame (sprites,
 * lights, effect channels, sounds, part flags, the slots' effect scripts and
 * image animations), then advance the frame, looping at its loop length.
 * An event is an s16 frame time and a type byte, then the type's fields
 * (AnimEvent); an animation (800AE1BC) or effect command 63 supplies the
 * list and its count. Each case steps over its event; a type without a case
 * is not stepped over (tools/analysis/battle_effect_vm.py decodes them). */
void func_800AE2A4(BattleObject *object, EffectPool *pool, s32 arg2) {
    AnimEvent *event;
    SpriteCommand *sprite;
    LightEvent *light;
    SoundEvent *sound;
    SlotEvent *slots;
    BattleObject *target;
    MATRIX *m;
    SVECTOR point;
    VECTOR out;
    VECTOR ground;
    s16 kind;
    u16 offset;
    s16 slot;
    s32 angle;
    s16 scale;
    u8 *resource;
    s32 base;
    s32 variant;
    s16 volume;
    s32 i;
    s32 one = 1;
    BattleObject **stage = D_800D3368;
    BattleObject **objects;
    u16 savedMask;
    s16 savedIndex;
    s32 mask;
    u8 onTarget;
    s32 script;
    ImageAnim *targetImage;
    ColorRow *colour;
    s16 x;
    s16 y;
    s16 x2;
    s16 y2;
    s16 field10;
    FrameCurve curve;

    if (object->animation >= 0) {
        for (; object->animationFrame < object->animationLength; object->animationFrame++) {
            {
                event = (AnimEvent *)object->animationStart;
                if (object->animation != event->header.time) {
                    break;
                }
                switch (event->header.type) {
                case 1: /* create a sprite (SpriteCommand, 0x14 bytes) */
                    slot = 0;
                    m = (MATRIX *)0x1F800000;
                    sprite = &event->sprite;
                    if (sprite->flags & 0x80) {
                        offset = D_800CCCE8.records[object->slot].gear.spriteVariants[sprite->kind] - 1;
                        kind = sprite->kind + offset;
                    } else {
                        kind = sprite->kind;
                    }
                    if ((sprite->mode & 0x80) || (sprite->flags & 0x80)) {
                        target = stage[func_800AF400()];
                        if (target == NULL) {
                            slot = func_800AF400() + 1;
                            target = object;
                        }
                    } else {
                        target = object;
                    }
                    if (kind >= 0) {
                        if (sprite->part != 0) {
                            CompMatrix(&target->hierarchy->transform, &target->hierarchy[sprite->part].world, m);
                        } else {
                            *m = target->hierarchy->transform;
                        }
                        if (slot != 0) {
                            m->t[0] = (u16)D_800C3EB4[slot - 1].x;
                            m->t[1] = (u16)D_800C3EB4[slot - 1].y;
                            m->t[2] = (u16)D_800C3EB4[slot - 1].z;
                        }
                        SetRotMatrix(m);
                        SetTransMatrix(m);
                        point.vx = sprite->offset[0];
                        point.vy = sprite->offset[1];
                        point.vz = sprite->offset[2];
                        gte_ldv0(&point);
                        gte_rtv0tr();
                        gte_stlvnl(&out);
                        point.vx = out.vx;
                        point.vz = out.vz;
                        if ((sprite->mode & 0x7F) == one) {
                            point.vy = object->groundY;
                        } else if ((sprite->mode & 0x7F) == 2) {
                            point.vx = SCENE_DATA->centre.vx;
                            point.vy = SCENE_DATA->centre.vy;
                            point.vz = SCENE_DATA->centre.vz;
                            func_800A5870(&point, func_800A579C(&point), &ground);
                        } else {
                            point.vy = out.vy;
                        }
                        if (sprite->absolute) {
                            angle = sprite->angle;
                        } else {
                            s32 relative = sprite->angle + 0x400;

                            angle = object->hierarchy->rotation.vy + relative;
                        }
                        resource = sprite_shared_source;
                        scale = sprite->scale * object->scale1C >> 8;
                        if (sprite->resource) {
                            resource = sprite_effect_source;
                        }
                        if (func_800B12D0(func_800AF400(), sprite->flags)) {
                            func_800AFB4C(resource, kind, &point, angle, scale, sprite, object);
                        }
                    }
                    object->animationStart += sizeof(SpriteCommand);
                    break;
                case 2: /* a light follows a part, or goes off (LightEvent; 6 bytes off) */
                    if (event->light.on) {
                        if (event->light.light < 2) {
                            light = &event->light;
                            if (light->free) {
                                D_800D3304[light->light].object = -1;
                            } else {
                                D_800D3304[light->light].object = object->slot;
                            }
                            D_800D3304[light->light].part = light->part;
                            D_800D2FC0->m[0][light->light + 1] = light->r * 16;
                            D_800D2FC0->m[1][light->light + 1] = light->g * 16;
                            D_800D2FC0->m[2][light->light + 1] = light->b * 16;
                            D_800D3304[light->light].offset.vx = light->offset[0];
                            D_800D3304[light->light].offset.vy = light->offset[1];
                            D_800D3304[light->light].offset.vz = light->offset[2];
                            D_800D3304[light->light].active = light->active;
                        }
                        object->animationStart += sizeof(LightEvent);
                    } else {
                        D_800D3304[event->light.light].active = 0;
                        object->animationStart += 6;
                    }
                    break;
                case 3:
                case 4: /* stop a colour fade channel; on: restart it in mode type - 3
                         * (ChannelEvent; 6 bytes off) */
                    func_800A3484(&object->channels[event->channel.channel], arg2);
                    if (event->channel.on) {
                        if (event->channel.channel < object->channelCount) {
                            func_800A32D8(&object->channels[event->channel.channel], (s32)&D_800C3D04,
                                          event->channel.field5, event->channel.type - 3, event->channel.bytes[0],
                                          event->channel.bytes[1], event->channel.bytes[2], event->channel.bytes[3],
                                          event->channel.bytes[4], event->channel.bytes[5], event->channel.bytes[6],
                                          event->channel.bytes[7], event->channel.values[0],
                                          event->channel.values[1], event->channel.values[2],
                                          event->channel.values[3], event->channel.values[4],
                                          event->channel.values[5], event->channel.last);
                        }
                        object->animationStart += sizeof(ChannelEvent);
                    } else {
                        object->animationStart += 6;
                    }
                    break;
                case 5: /* play a sound (SoundEvent) */
                    sound = &event->sound;
                    if (func_800B12D0(func_800AF400(), sound->flags)) {
                        variant = 0;
                        if (sound->kind == one && SCENE_DATA->soundMode != 0) {
                            sound_play_effect((object->model->sounds->id << 16) | (SCENE_DATA->soundMode + 10));
                        }
                        base = func_800AE220(object, sound->source);
                        if (SCENE_DATA->soundMode == 3 && sound->kind == 2) {
                            variant = 8;
                        }
                        sound_play_effect(base + sound->sound + variant);
                        if (sound->sound2 != 0) {
                            sound_play_effect(base + sound->sound2 + variant);
                        }
                        volume = object->field39;
                        if (SCENE_DATA->soundMode == 3 && sound->kind != 2) {
                            volume = volume * 60 / 107;
                        }
                        sound_set_effect_volume(base + sound->sound + variant, volume);
                        if (sound->sound2 != 0) {
                            sound_set_effect_volume(base + sound->sound2 + variant, volume);
                        }
                    }
                    object->animationStart += sizeof(SoundEvent);
                    break;
                case 6: /* update the battle menu (4 bytes) */
                    func_800BF6CC();
                    object->animationStart += 4;
                    break;
                case 7: /* draw part (byte 4) = byte 5 bit 0 (ShowEvent, 6 bytes) */
                    object->hierarchy[event->show.part].visible = event->show.visible & 1;
                    object->animationStart += 6;
                    break;
                case 8: /* start effect scripts on the object's slots (SlotEvent) */
                    slots = &event->slots;
                    i = 0;
                    objects = D_800D3368;
                    savedIndex = D_800C3D40;
                    savedMask = D_800C3E30;
                    mask = one << savedIndex;
                    for (; i < 13; i++, objects++) {
                        if ((object->slotMask >> i) & 1) {
                            onTarget = slots->onTarget;
                            script = -1;
                            if (*objects != NULL && ((*objects)->flags4A & 2)) {
                                onTarget = 0;
                            }
                            switch (D_800C3FE8[D_800C360C - 1].codes[i]) {
                            case 0:
                            case 1:
                                if (*objects != NULL && ((*objects)->flags4A & 2)) {
                                    script = slots->scripts[1];
                                } else {
                                    script = slots->scripts[0];
                                }
                                break;
                            case 5:
                                script = slots->scripts[2];
                                break;
                            case 4:
                                script = slots->scripts[3];
                                break;
                            case 2:
                            case 3:
                                script = slots->scripts[4];
                                break;
                            }
                            if (!func_800B12D0(i, slots->kinds)) {
                                script = -1;
                            }
                            if (*objects != NULL && script > 0) {
                                if (onTarget) {
                                    func_800AA564(object, i, mask, script);
                                } else {
                                    func_800AA454(i, mask, script);
                                }
                            }
                        }
                    }
                    D_800C3D40 = savedIndex;
                    D_800C3E30 = savedMask;
                    object->animationStart += sizeof(SlotEvent);
                    break;
                case 9: /* start or stop an image animation (ImageEvent; 6 bytes off) */
                    if (event->image.on) {
                        if (event->image.anim < object->imageCount) {
                            if (event->image.target != 0xFF && event->image.target < object->imageCount) {
                                targetImage = &object->images[event->image.target];
                            } else {
                                targetImage = NULL;
                            }
                            colour = NULL;
                            if ((event->image.mode & 0x7F) >= 4) {
                                colour = (ColorRow *)D_800D2FC0;
                            }
                            curve = func_800AA820(event->image.curve);
                            x = event->image.x;
                            y = event->image.y;
                            x2 = event->image.x2;
                            y2 = event->image.y2;
                            field10 = event->image.field10;
                            if (event->image.mode & 0x80) {
                                if (object->placement[0] < 0) {
                                    break;
                                }
                                x += object->placement[2];
                                y += object->placement[3];
                                if (event->image.field12 >> 4 == one) {
                                    x2 += object->placement[2];
                                    y2 += object->placement[3];
                                }
                            }
                            func_800A3640(&object->images[event->image.anim], targetImage, event->image.mode & 0x7F,
                                          event->image.field12 | 0x700, colour, x, y, 0, x2, y2, field10, x, y,
                                          event->image.field13, event->image.field14, event->image.field16, event->image.field18,
                                          event->image.field1A, curve);
                        }
                        object->animationStart += sizeof(ImageEvent);
                    } else {
                        func_800A429C(&object->images[event->image.anim]);
                        object->animationStart += 6;
                    }
                    break;
                }
            }
        }
        object->animation++;
        if (object->animationLoop >= 0 && object->animation >= object->animationLoop) {
            object->animation = 0;
            object->animationFrame = 0;
            object->animationStart = object->animationCursor;
        }
    }
}

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
    MATRIX *m;
    VECTOR position;
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
        m = (MATRIX *)0x1F800000;
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
        if (entry != NULL && (u32)(entry->kind - 7) < 2) {
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

/* Detach part index of a hierarchy and its descendants: move their marks to
 * the same parts of another hierarchy and release their effects (the root
 * entry's index is the part count). */
void func_800AF180(EffectPool *pool, s32 index, ModelPart *from, ModelPart *to) {
    ModelPart *child;
    s32 count;
    s32 i;

    child = from;
    count = child->index;
    from[index].visible = 0;
    to[index].visible = 1;
    func_800A23E8(pool, from[index].effects[0]);
    from[index].effects[0] = NULL;
    func_800A23E8(pool, from[index].effects[1]);
    from[index].effects[1] = NULL;
    func_800A23E8(pool, from[index].effects[2]);
    from[index].effects[2] = NULL;
    for (i = 1; i < count; i++) {
        child++;
        if (child->parent == &from[index]) {
            func_800AF180(pool, child->index, from, to);
        }
    }
}

/* Move the parts' visible flags of a hierarchy to another of the same
 * shape. */
void func_800AF270(ModelPart *from, ModelPart *to) {
    s32 count = from->index;
    s32 i;

    for (i = 1; i < count; i++) {
        from++;
        to++;
        if (from->visible) {
            from->visible = 0;
            to->visible = 1;
        }
    }
}

/* The dot product of direction with the unit normal (the cross product) of
 * a and b, in 4.12 through 16 << 8 / its length, divided by scale. */
s16 func_800AF2C4(VECTOR *direction, VECTOR *a, VECTOR *b, s32 scale) {
    VECTOR normal;
    s32 dot;
    s32 length;

    OuterProduct12(a, b, &normal);
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

/* The slot of a target code, and its mask: 0xFF the first selected slot, 0xFE the
 * selected object, 0xFD/0xF9 and 0xFC the object's slots, 0xFA slot 31, 0xF8
 * and 0xF7 slots derived from the object's slot; any other code is a slot. */
u8 func_800AF438(BattleObject *object, u8 slot, u16 *mask) {
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
    return slot;
}

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
void func_800AF678(BattleObject *object, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag, u8 field1,
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
        entry->kind = mode + 3;
        entry->tag = tag;
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
        entry->time = 0;
        entry->duration = duration;
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
                func_800AF678(object, pool, child, flags, mode, tag, field1, startX, startY, startZ, endX, endY, endZ,
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

    part->visible = flags & 1;
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
void func_800AFB4C(void *resource, s32 kind, SVECTOR *position, s16 direction, s16 scale, SpriteCommand *command,
                   BattleObject *object) {
    SpriteTask *task;
    SpriteFollow *follow;

    task = sprite_create_effect(kind, resource, position, sizeof(SpriteFollow));
    sprite_set_direction(&task->sprite, direction);
    sprite_set_facing(&task->sprite, direction);
    sprite_set_scale(&task->sprite, scale);
    follow = (SpriteFollow *)((u8 *)task + (s16)task->sprite.size);
    follow->object = object;
    follow->part = command->part;
    if (command->follow) {
        follow->update = task_get_update_callback(&task->task);
        task_set_update_callback(&task->task, func_800AFC68);
        follow->offset.vx = command->offset[0];
        follow->offset.vy = command->offset[1];
        follow->offset.vz = command->offset[2];
        follow->onGround = command->mode;
    }
}

/* Update of a following sprite: place it at its offset from its object's
 * part (on the object's ground height when asked), then run its own update. */
void func_800AFC68(Task *node) {
    SpriteTask *task = (SpriteTask *)node;
    SpriteFollow *follow = (SpriteFollow *)((u8 *)task + (s16)task->sprite.size);
    MATRIX *m = (MATRIX *)0x1F800000;
    VECTOR out;

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
    task->sprite.x = out.vx << 16;
    task->sprite.y = out.vy << 16;
    task->sprite.z = out.vz << 16;
    follow->update(&task->task);
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
    part->dirty = 1;
    part->rotate = 1;
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
    SVECTOR point;

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
            sound_remove_effect_bank(object->extraData->sounds);
            object->extraSounds = 0;
        }
        heap_free(object->extra);
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
void func_800B0164(EffectPool *pool, s32 index, u8 mode, u8 tag, u16 p0, u16 p1, u16 p2, u16 p3, u16 p4,
                   u16 p5, u16 duration) {
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
            entry->kind = mode;
            entry->tag = tag;
            entry->params[0] = p0;
            entry->params[1] = p1;
            entry->params[2] = p2;
            entry->params[3] = p3;
            entry->params[4] = p4;
            entry->params[5] = p5;
            entry->time = 0;
            entry->duration = duration;
        }
    }
}

/* Run the camera channels (D_800C3BAC) for steps frames. Each channel moves
 * a value towards its target (a stage object's or slot's position, halfway
 * to a second one when set, or given values), linearly over its duration
 * (mode bit 0 clear) or by a fraction of the rest each frame; channels 0-6
 * are the orbit and look-at parameters, 7 the look-at point (from the target
 * at the look-at distance, angle and height) and 8 the camera position (at
 * the orbit distance and angles, kept off the objects and above the
 * ground). Finished channels 0-6 are released; D_800C3B88 tells whether the
 * channel tagged D_800C3B84 is running (1) or has finished (2). */
void func_800B026C(EffectPool *pool, s32 steps, s32 arg2, s32 key) {
    s32 step;
    s32 i;
    EffectEntry *entry;
    CameraChannel *channel;
    s16 x;
    s16 y;
    s16 z;
    s16 valueX;
    s16 valueY;
    s16 valueZ;
    u8 mode;
    s16 t;
    s16 dx;
    s16 dy;
    s16 dz;
    s32 slot;
    ModelPart *root;
    s32 vertical;
    s32 horizontal;
    SVECTOR point;
    s16 ground;

    D_800C3B88 = 0;
    for (step = 0; step < steps; step++) {
        for (i = 0; i < 9; i++) {
            if (D_800C3BAC[i] == NULL) {
                continue;
            }
            entry = D_800C3BAC[i];
            channel = (CameraChannel *)entry;
            mode = entry->kind;
            if ((mode & 0xF) < 2) {
                slot = (s16)entry->params[3];
                if (D_800D3368[slot] != NULL) {
                    x = D_800D3368[slot]->hierarchy->translation[0];
                    y = D_800D3368[slot]->hierarchy->translation[1] - entry->params[4];
                    z = D_800D3368[slot]->hierarchy->translation[2];
                } else {
                    x = D_800C3EB4[slot].x;
                    y = D_800C3EB4[slot].y - entry->params[4];
                    z = D_800C3EB4[slot].z;
                }
                slot = channel->slot2;
                if (slot >= 0) {
                    if (D_800D3368[slot] != NULL) {
                        root = D_800D3368[slot]->hierarchy;
                        x = (x + root->translation[0]) / 2;
                        y = (y + root->translation[1] - channel->height) / 2;
                        z = (z + root->translation[2]) / 2;
                    } else {
                        x = (x + (u16)D_800C3EB4[slot].x) / 2;
                        y = (y + (u16)D_800C3EB4[slot].y - channel->height) / 2;
                        z = (z + (u16)D_800C3EB4[slot].z) / 2;
                    }
                }
            } else {
                x = entry->params[3];
                y = entry->params[4];
                z = entry->params[5];
            }
            if (i == 7) {
                y -= D_800C3BA8;
                x += -D_800C3BA4 * gpu_get_sin(D_800C3BA0) / 4096;
                z += -D_800C3BA4 * gpu_get_cos(D_800C3BA0) / 4096;
            }
            if (i == 8) {
                vertical = D_800C3B98 * gpu_get_sin(D_800C3B94) / 4096;
                horizontal = D_800C3B98 * gpu_get_cos(D_800C3B94) / 4096;
                x += -horizontal * gpu_get_sin(D_800C3B90) / 4096;
                y += -vertical - D_800C3B9C;
                z += -horizontal * gpu_get_cos(D_800C3B90) / 4096;
                point.vx = x;
                point.vy = y;
                point.vz = z;
                if (func_800B0FF4(&D_800D335C, &point)) {
                    x = point.vx;
                    z = point.vz;
                }
                ground = func_800B0B14(key);
                if (y > ground) {
                    y = ground;
                }
            }
            if (D_800C3B8C != 0 && i >= 7) {
                channel->current[0] = x;
                channel->current[1] = y;
                channel->current[2] = z;
            }
            switch (mode & 1) {
            case 0:
                t = entry->time + 1;
                valueX = channel->current[0] + (x - channel->current[0]) * t / (s16)entry->duration;
                valueY = channel->current[1] + (y - channel->current[1]) * t / (s16)entry->duration;
                valueZ = channel->current[2] + (z - channel->current[2]) * t / (s16)entry->duration;
                break;
            case 1:
                dx = (x - channel->current[0]) / (s16)entry->duration;
                dy = (y - channel->current[1]) / (s16)entry->duration;
                dz = (z - channel->current[2]) / (s16)entry->duration;
                valueX = channel->current[0];
                valueY = channel->current[1];
                valueZ = channel->current[2];
                if (dx == 0 && dy == 0 && dz == 0) {
                    entry->time = (s16)entry->duration;
                } else {
                    valueX += dx;
                    valueY += dy;
                    valueZ += dz;
                    entry->time = 0;
                }
                channel->current[0] = valueX;
                channel->current[1] = valueY;
                channel->current[2] = valueZ;
                break;
            }
            if ((s16)++entry->time >= (s16)entry->duration) {
                if (i >= 7) {
                    entry->time = (s16)entry->duration - 1;
                } else {
                    if (entry->tag == D_800C3B84) {
                        D_800C3B88 |= 2;
                    }
                    func_800A23E8(pool, entry);
                    D_800C3BAC[i] = NULL;
                }
            } else if (entry->tag == D_800C3B84) {
                D_800C3B88 |= 1;
            }
            switch (i) {
            case 0:
                D_800C3B90 = valueX;
                break;
            case 1:
                D_800C3BA0 = valueX;
                break;
            case 2:
                D_800C3B94 = valueX;
                break;
            case 3:
                D_800C3B98 = valueX;
                break;
            case 4:
                D_800C3BA4 = valueX;
                break;
            case 5:
                D_800C3BA8 = valueX;
                break;
            case 6:
                D_800C3B9C = valueX;
                break;
            case 7:
                D_800D335C.vx = valueX;
                D_800D335C.vy = valueY;
                D_800D335C.vz = valueZ;
                break;
            case 8:
                D_800D3354.vx = valueX;
                D_800D3354.vy = valueY;
                D_800D3354.vz = valueZ;
                break;
            }
        }
    }
    D_800C3B8C = 0;
}

/* Whether a point (x at [0], z at [2]) lies strictly inside the scene's
 * bounds. */
s16 func_800B0AB4(s16 *point) {
    BattleSceneData *scene = SCENE_DATA;
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
    VECTOR out;
    SVECTOR point;
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
s32 func_800B0D70(SVECTOR *motion, SVECTOR *point) {
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

/* Push point out of the objects' footprints (800B0D70) moving from from
 * (the motion is the direction from from, 512 long); the object hit. */
s32 func_800B0FF4(SVECTOR *from, SVECTOR *point) {
    SVECTOR motion;
    s32 length;

    motion.vx = from->vx - point->vx;
    motion.vz = from->vz - point->vz;
    length = SquareRoot0(motion.vx * motion.vx + motion.vz * motion.vz) + 1;
    motion.vx = (motion.vx << 9) / length;
    motion.vz = (motion.vz << 9) / length;
    return func_800B0D70(&motion, point);
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
        (*slot)->hierarchy->translation[0] = x - dx * distance / length - gpu_get_sin(angle) * gap / 4096;
        (*slot)->hierarchy->translation[2] = z - dz * distance / length - gpu_get_cos(angle) * gap / 4096;
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
        cd_stop_read(0);
        busy = 1;
        for (;;) {
            if (cd_get_pending_read_count() == 0) {
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
