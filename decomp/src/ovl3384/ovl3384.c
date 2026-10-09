/* ovl3384: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc4c4, called by
 * the opcode handler 800b6a7c, which breaks a model into flying pieces.
 *
 * The module was built by the Cygnus CDK GCC 2.7.2 with a later ASPSX
 * (see ovl3384.mk). */
#include "debris.h"

SVECTOR D_801FCE14 = {0, 0, 0};

/* The vertex named by the primitive's n-th halfword. */
#define VERTEX(n) ((SVECTOR *)(((u16 *)desc)[n] * sizeof(SVECTOR) + (s32)vertices))

/* Release the pieces, their primitives and the task. */
void func_801FC074(Task *node) {
    DebrisTask *debris = node->data;

    func_800320E8(debris->pieces);
    func_80025180((u32)debris->prims[0]);
    func_8001CB48(&debris->draw);
    func_8001CD94(&debris->task);
    func_800320E8(debris);
}

/* Move and spin every piece under gravity; the effect ends when its life
 * runs out. */
void func_801FC0CC(Task *node) {
    DebrisTask *debris = node->data;
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
 * matrix and the camera, project its corners into its primitive and queue it.
 * Each primitive kind has its own case (the compiler merges the identical
 * ones afterwards). */
void func_801FC1A8(Task *node) {
    DebrisTask *debris;
    Piece *piece;
    ScriptCommand *desc;
    u8 *prim;
    MATRIX camera;
    s32 position[3];
    MATRIX m;
    long sxy[4];
    long p, flag;
    s32 count;
    s32 kind;
    s32 i;

    debris = node->data;
    CompMatrix(&D_8004FBB8, &debris->matrix, &camera);
    piece = debris->pieces;
    prim = debris->prims[D_800C3EB0.buffer];
    desc = (ScriptCommand *)(debris->model->commands + (s32)debris->model);
    count = debris->count;
    for (i = 0; i != count; i++, piece++) {
        position[0] = piece->position[0] >> 16;
        position[1] = piece->position[1] >> 16;
        position[2] = piece->position[2] >> 16;
        TransMatrix(&m, (VECTOR *)position);
        func_8003F738(&piece->rotation, &m);
        CompMatrix(&camera, &m, &m);
        SetTransMatrix(&m);
        SetRotMatrix(&m);
        kind = desc->code & 0x1C;
        if (kind & 8) {
            RotTransPers4(&piece->vertex[0], &piece->vertex[1], &piece->vertex[2], &piece->vertex[3],
                          &sxy[0], &sxy[1], &sxy[2], &sxy[3], &p, &flag);
            AddPrim((u32 *)D_8005956C, prim);
        } else {
            RotTransPers3(&piece->vertex[0], &piece->vertex[1], &piece->vertex[2], &sxy[0], &sxy[1],
                          &sxy[2], &p, &flag);
            AddPrim((u32 *)D_8005956C, prim);
        }
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
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0x10) = sxy[1];
            *(s32 *)(prim + 0x18) = sxy[2];
            break;
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
            *(s32 *)(prim + 0x8) = sxy[0];
            *(s32 *)(prim + 0x10) = sxy[1];
            *(s32 *)(prim + 0x18) = sxy[2];
            *(s32 *)(prim + 0x20) = sxy[3];
            break;
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
        prim += (desc->primWords + 1) * 4;
        desc = (ScriptCommand *)((u8 *)desc + (desc->words + 1) * 4);
    }
}

/* Opcode entry: break `model` (placed by `matrix`) into one piece per
 * primitive. Each piece keeps its corners about its centre and flies from the
 * model's origin outward at `speed` plus a random part of `speed_range`,
 * spinning by a random part of `spin_range`, falling under `gravity`, for
 * `life` frames. `prims` holds the model's primitives, or 0 to build them
 * (twice: one copy per display buffer). The primitives are read through
 * `source`, a copy of `model` (the original keeps the two apart). */
void func_801FC4C4(ScriptEntry *model, u8 *prims, MATRIX *matrix, s32 gravity, s32 speed, s32 speed_range,
                   s32 spin_range, s32 life) {
    DebrisTask *debris;
    Piece *piece;
    ScriptCommand *desc;
    SVECTOR *vertices;
    SVECTOR *v0, *v1, *v2, *v3;
    SVECTOR centre;
    SVECTOR angles;
    VECTOR velocity;
    MATRIX m;
    s32 count, size, kind, i, r;
    u8 *buffer;
    ScriptEntry *source;

    debris = (DebrisTask *)func_8001D1D8(sizeof(DebrisTask), NULL, func_801FC0CC, func_801FC1A8, func_801FC074);
    debris->model = model;
    count = model->count;
    debris->count = count;
    piece = func_80031BDC(count * sizeof(Piece), 0);
    debris->pieces = piece;
    debris->matrix = *matrix;
    debris->life = life;
    source = model;
    size = func_800B16A4(source);
    if (prims == NULL) {
        buffer = func_80031BDC(size * 2, 0);
        func_800B1720(source, buffer, 0, 1);
        memcpy(buffer + size, buffer, size);
    } else {
        buffer = prims;
    }
    debris->prims[0] = buffer;
    debris->prims[1] = buffer + size;
    desc = (ScriptCommand *)(source->commands + (s32)source);
    vertices = (SVECTOR *)(source->vertices + (s32)source);
    for (i = 0; i != count; i++) {
        /* Bit 8 selects the unlit command layout; descriptor flags bit 0
         * selects lighting. Vertex indices follow its header and colours. */
        kind = desc->code & 0x1C;
        kind |= ((desc->flags ^ 1) & 1) << 8;
        switch (kind) {
        case 0x00:
            v0 = VERTEX(4);
            v1 = VERTEX(5);
            v2 = VERTEX(6);
            break;
        case 0x10:
            v0 = VERTEX(8);
            v1 = VERTEX(9);
            v2 = VERTEX(10);
            break;
        case 0x18:
            v0 = VERTEX(10);
            v1 = VERTEX(11);
            v2 = VERTEX(12);
            v3 = VERTEX(13);
            break;
        case 0x08:
            v0 = VERTEX(4);
            v1 = VERTEX(5);
            v2 = VERTEX(6);
            v3 = VERTEX(7);
            break;
        case 0x04:
            v0 = VERTEX(10);
            v1 = VERTEX(11);
            v2 = VERTEX(12);
            break;
        case 0x14:
            v0 = VERTEX(14);
            v1 = VERTEX(15);
            v2 = VERTEX(16);
            break;
        case 0x0C:
            v0 = VERTEX(12);
            v1 = VERTEX(13);
            v2 = VERTEX(14);
            v3 = VERTEX(15);
            break;
        case 0x1C:
            v0 = VERTEX(18);
            v1 = VERTEX(19);
            v2 = VERTEX(20);
            v3 = VERTEX(21);
            break;
        case 0x100:
            v0 = VERTEX(5);
            v1 = VERTEX(6);
            v2 = VERTEX(7);
            break;
        case 0x110:
            v0 = VERTEX(8);
            v1 = VERTEX(11);
            v2 = VERTEX(13);
            break;
        case 0x108:
            v0 = VERTEX(5);
            v1 = VERTEX(6);
            v2 = VERTEX(7);
            v3 = VERTEX(8);
            break;
        case 0x104:
            v0 = VERTEX(9);
            v1 = VERTEX(10);
            v2 = VERTEX(11);
            break;
        case 0x114:
            v0 = VERTEX(9);
            v1 = VERTEX(11);
            v2 = VERTEX(13);
            break;
        case 0x10C:
            v0 = VERTEX(11);
            v1 = VERTEX(12);
            v2 = VERTEX(13);
            v3 = VERTEX(14);
            break;
        case 0x118:
        case 0x11C:
            v0 = VERTEX(11);
            v1 = VERTEX(13);
            v2 = VERTEX(15);
            v3 = VERTEX(17);
            break;
        }
        if (kind & 8) {
            centre = *v0;
            centre.vx += v1->vx;
            centre.vy += v1->vy;
            centre.vz += v1->vz;
            centre.vx += v2->vx;
            centre.vy += v2->vy;
            centre.vz += v2->vz;
            centre.vx += v3->vx;
            centre.vy += v3->vy;
            centre.vz += v3->vz;
            centre.vx /= 4;
            centre.vy /= 4;
            centre.vz /= 4;
        } else {
            centre = *v0;
            centre.vx += v1->vx;
            centre.vy += v1->vy;
            centre.vz += v1->vz;
            centre.vx += v2->vx;
            centre.vy += v2->vy;
            centre.vz += v2->vz;
            centre.vx /= 3;
            centre.vy /= 3;
            centre.vz /= 3;
            v3 = v2;
        }
        piece->vertex[0].vx = v0->vx - centre.vx;
        piece->vertex[0].vy = v0->vy - centre.vy;
        piece->vertex[0].vz = v0->vz - centre.vz;
        piece->vertex[1].vx = v1->vx - centre.vx;
        piece->vertex[1].vy = v1->vy - centre.vy;
        piece->vertex[1].vz = v1->vz - centre.vz;
        piece->vertex[2].vx = v2->vx - centre.vx;
        piece->vertex[2].vy = v2->vy - centre.vy;
        piece->vertex[2].vz = v2->vz - centre.vz;
        piece->vertex[3].vx = v3->vx - centre.vx;
        piece->vertex[3].vy = v3->vy - centre.vy;
        piece->vertex[3].vz = v3->vz - centre.vz;
        piece->position[0] = centre.vx << 16;
        piece->position[1] = centre.vy << 16;
        piece->position[2] = centre.vz << 16;
        r = (rand() & 0xFF) * speed_range / 256;
        velocity.vx = speed + r;
        velocity.vy = 0;
        velocity.vz = 0;
        func_800C0828(&centre, &D_801FCE14, &angles);
        func_8003F738(&angles, &m);
        ApplyMatrixLV(&m, &velocity, &velocity);
        piece->velocity[0] = velocity.vx;
        piece->velocity[1] = velocity.vy;
        piece->velocity[2] = velocity.vz;
        piece->rotation.vx = 0;
        piece->rotation.vy = 0;
        piece->rotation.vz = 0;
        r = (rand() & 0xFF) * spin_range / 256;
        r -= r / 2;
        piece->spin.vx = r;
        r = (rand() & 0xFF) * spin_range / 256;
        r -= r / 2;
        piece->spin.vy = r;
        r = (rand() & 0xFF) * spin_range / 256;
        r -= r / 2;
        piece->spin.vz = r;
        piece->gravity = gravity;
        piece++;
        desc = (ScriptCommand *)((u8 *)desc + (desc->words + 1) * 4);
    }
}
