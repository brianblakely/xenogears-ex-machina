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
            list->models[i] = group + 0x10 + i * 0x38;
        }
    }
    return list;
}

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DC2D0);

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

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DC848);

void func_801DCC34(void) {
}

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DCC3C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DCD8C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DCE18);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DCEC8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DDBF8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DEF10);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF0B4);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF52C);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF5F4);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF668);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF6A8);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF6F0);

INCLUDE_ASM(".local/decomp/ovl2143/asm/nonmatchings/ovl2143", func_801DF7A8);

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
