/* ovl3384: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc4c4, called by
 * the opcode handler 800b6a7c, which breaks a model into flying pieces.
 *
 * The module was built by a compiler that schedules %hi/%lo halves of
 * addresses separately (lui far from its lw/sw/addiu, even in delay slots)
 * and keeps positive li as addiu; the qualified GCC 2.6.3/2.7.2 + ASPSX 2.34
 * do neither, so functions addressing symbols stay NON_MATCHING. */
#include "debris.h"

/* Release the pieces, their primitives and the task. */
void func_801FC074(TaskNode *node) {
    DebrisTask *debris = node->object;

    func_800320E8(debris->pieces);
    func_80025180(debris->prims[0]);
    func_8001CB48(&debris->draw);
    func_8001CD94(&debris->task);
    func_800320E8(debris);
}

/* Move and spin every piece under gravity; the effect ends when its life
 * runs out. */
void func_801FC0CC(TaskNode *node) {
    DebrisTask *debris = node->object;
    Piece *piece = debris->pieces;
    s32 count = debris->count;
    s32 i;

    for (i = 0; i != count; i++, piece++) {
        piece->position[0] += piece->velocity[0];
        piece->position[1] += piece->velocity[1];
        piece->position[2] += piece->velocity[2];
        piece->rotation.vx += piece->spin.vx;
        piece->rotation.vy += piece->spin.vy;
        piece->rotation.vz += piece->spin.vz;
        piece->velocity[1] += piece->gravity;
    }
    if (--debris->life < 0) {
        node->destroy(node);
    }
}

/* Draw every piece: place it by its position and rotation under the model's
 * matrix and the camera, project its corners into its primitive and queue it. */
#ifdef NON_MATCHING
void func_801FC1A8(TaskNode *node) {
    DebrisTask *debris;
    Piece *piece;
    PacketDesc *desc;
    u8 *prim;
    MATRIX camera;
    MATRIX m;
    s32 position[3];
    s32 sxy[4];
    s32 p, flag;
    s32 count;
    s32 kind;
    s32 i;

    debris = node->object;
    CompMatrix(&D_8004FBB8, &debris->matrix, &camera);
    piece = debris->pieces;
    prim = debris->prims[D_800C3EB0.buffer];
    desc = (PacketDesc *)((u8 *)debris->model + debris->model->packets);
    count = debris->count;
    for (i = 0; i != count; i++, piece++) {
        position[0] = piece->position[0] >> 16;
        position[1] = piece->position[1] >> 16;
        position[2] = piece->position[2] >> 16;
        TransMatrix(&m, position);
        func_8003F738(&piece->rotation, &m);
        CompMatrix(&camera, &m, &m);
        SetTransMatrix(&m);
        SetRotMatrix(&m);
        kind = desc->kind & 0x1C;
        if (kind & 8) {
            RotTransPers4(&piece->vertex[0], &piece->vertex[1], &piece->vertex[2], &piece->vertex[3],
                          &sxy[0], &sxy[1], &sxy[2], &sxy[3], &p, &flag);
        } else {
            RotTransPers3(&piece->vertex[0], &piece->vertex[1], &piece->vertex[2], &sxy[0], &sxy[1],
                          &sxy[2], &p, &flag);
        }
        AddPrim(D_8005956C, prim);
        /* The corners' places in POLY_F3/F4/FT3/G3/GT3/FT4/G4/GT4. */
        switch (kind) {
        case 0:
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0xC) = sxy[1];
            *(s32 *)(prim + 0x10) = sxy[2];
            break;
        case 8:
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0xC) = sxy[1];
            *(s32 *)(prim + 0x10) = sxy[2];
            *(s32 *)(prim + 0x14) = sxy[3];
            break;
        case 4:
        case 16:
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0x10) = sxy[1];
            *(s32 *)(prim + 0x18) = sxy[2];
            break;
        case 20:
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0x14) = sxy[1];
            *(s32 *)(prim + 0x20) = sxy[2];
            break;
        case 12:
        case 24:
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0x10) = sxy[1];
            *(s32 *)(prim + 0x18) = sxy[2];
            *(s32 *)(prim + 0x20) = sxy[3];
            break;
        case 28:
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0x14) = sxy[1];
            *(s32 *)(prim + 0x20) = sxy[2];
            *(s32 *)(prim + 0x2C) = sxy[3];
            break;
        }
        prim += (desc->prim_words + 1) * 4;
        desc = (PacketDesc *)((u8 *)desc + (desc->words + 1) * 4);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3384/asm/nonmatchings/ovl3384", func_801FC1A8);
#endif

INCLUDE_ASM(".local/decomp/ovl3384/asm/nonmatchings/ovl3384", func_801FC4C4);
