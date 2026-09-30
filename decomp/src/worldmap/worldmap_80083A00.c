#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80083A00);

/* Place the actor and show scene objects 7 and 8 at its position. */
s32 func_80083FE4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = 0x1800000;
    actor->position.vy = 0x80000;
    actor->position.vz = 0x1A00000;
    actor->state = 0;
    D_8009C620[8].visible = 1;
    D_8009C620[7].visible = 1;
    D_8009C620[7].position.vx = D_8009C620[8].position.vx = actor->position.vx >> 12;
    D_8009C620[7].position.vy = D_8009C620[8].position.vy = actor->position.vy >> 12;
    D_8009C620[7].position.vz = D_8009C620[8].position.vz = actor->position.vz >> 12;
    return 1;
}

/* Drive the lift of scene objects 7 and 8: a command (1-4) selects the
 * route state and stops the effect groups of the previous one; each state
 * lowers the actor and moves its effect groups with it. */
s32 func_80084068(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    SceneObject *objects;
    SceneObject *lift;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    lift = &objects[7];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        objects[8].visible = 0;
        objects[7].visible = 0;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        func_800894C8(3);
        func_800894C8(5);
        func_800894C8(6);
        func_800894C8(7);
        func_800894C8(10);
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 3;
        func_800894C8(7);
        func_800894C8(8);
        func_800894C8(9);
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 4;
        func_800894C8(5);
        func_800894C8(6);
        func_800894C8(7);
        func_800894C8(10);
        func_800894C8(11);
        func_800894C8(12);
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(3, &scratch->position, NULL);
        func_80089160(5, &scratch->position, NULL);
        func_80089160(6, &scratch->position, NULL);
        func_80089160(7, &scratch->position, NULL);
        func_80089160(10, &scratch->position, NULL);
        break;
    case 2:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(7, &scratch->position, NULL);
        func_80089160(8, &scratch->position, NULL);
        func_80089160(9, &scratch->position, NULL);
        break;
    case 3:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(5, &scratch->position, NULL);
        func_80089160(6, &scratch->position, NULL);
        func_80089160(7, &scratch->position, NULL);
        func_80089160(10, &scratch->position, NULL);
        func_80089160(11, &scratch->position, NULL);
        func_80089160(12, &scratch->position, NULL);
        break;
    case 4:
        actor->position.vy = func_800771D8(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        func_80089160(13, &scratch->position, NULL);
        scratch->position.vy = 0;
        func_80089160(14, &scratch->position, NULL);
        break;
    }
    lift[0].position.vx = lift[1].position.vx = actor->position.vx >> 12;
    lift[0].position.vy = lift[1].position.vy = actor->position.vy >> 12;
    lift[0].position.vz = lift[1].position.vz = actor->position.vz >> 12;
    return 1;
}

/* Unpack the area image to VRAM, then build 15 CLUT rows fading the
 * 0,0x1F0 CLUT row towards a pale blue and record the 16 CLUT ids. */
void func_8008440C(void) {
    RECT rect;
    CVECTOR fade = {0xE0, 0xF5, 0xFF, 0x00};
    s32 i;
    u16 *clut;
    void *source;
    void *faded;

    source = func_80032E88(D_8009BD20, 1);
    i = 0;
    func_8002DD20(source);
    DrawSync(0);
    func_800320E8(source);
    clut = D_8009BCE0;
    func_800320E8(D_8009BD20);
    source = func_80031BDC(0x200, 1);
    faded = func_80031BDC(0x2000, 1);
    rect.x = 0;
    rect.y = 0x1F0;
    rect.w = 0x100;
    rect.h = 1;
    StoreImage(&rect, source);
    DrawSync(0);
    func_800931D8(source, faded, 0x10, &fade);
    rect.x = 0;
    rect.y = 0x1F0;
    rect.w = 0x100;
    rect.h = 0xF;
    LoadImage(&rect, faded);
    DrawSync(0);
    do {
        i++;
        *clut = GetClut(rect.x, rect.y);
        rect.y++;
        clut++;
    } while (i < 0x10);
    func_800320E8(faded);
    func_800320E8(source);
}

/* Build the scene objects from the area's placement list: resolve the
 * animation offsets, then place, orient and build each object's
 * primitives (two buffers) from its sprite definition. */
void func_80084580(void) {
    ScenePlacement *placement;
    s32 *base;
    s32 *offsets;
    s32 i;

    i = 0;
    D_8009BD28 = func_8002C3E8(D_8009CD48);
    base = D_8009D308;
    offsets = base + 1;
    D_8009D308 = offsets;
    for (; i < D_8009BD28; i++) {
        offsets[i] = (s32)base + offsets[i];
    }
    D_8009D7E0 = *(s16 *)D_8009BD30;
    D_8009C620 = func_80031BDC(D_8009D7E0 * sizeof(SceneObject), 0);
    placement = (ScenePlacement *)((u8 *)D_8009BD30 + 2);
    SetColorMatrix(&D_8009A140);
    SetLightMatrix(&D_8009A160);
    for (i = 0; i < D_8009D7E0; i++, placement++) {
        D_8009C620[i].visible = 0;
        D_8009C620[i].unk2 = placement->def;
        D_8009C620[i].flags = placement->flags;
        D_8009C620[i].position.vx = placement->x;
        D_8009C620[i].position.vy = placement->y;
        D_8009C620[i].position.vz = -placement->z;
        D_8009C620[i].angle.vx = placement->ax;
        D_8009C620[i].angle.vy = placement->ay;
        D_8009C620[i].angle.vz = placement->az;
        func_8004A92C(&D_8009C620[i].angle, &D_8009C620[i].matrix);
        D_8009C620[i].def = &((SpriteDefTable *)D_8009CD48)->defs[D_8009C620[i].unk2];
        func_8002CB54(D_8009C620[i].def, &D_8009C620[i].prims, &D_8009C620[i].prims2, &D_8009C620[i]);
        func_8002C8CC(D_8009C620[i].def, D_8009C620[i].prims, 1);
        memcpy(D_8009C620[i].prims2, D_8009C620[i].prims, D_8009C620[i].def->size);
        D_8009C620[i].unk44 = ((s32 *)D_8009D308)[D_8009C620[i].unk2];
        ((s32 *)D_8009C620[i].unk44)[1] = D_8009C620[i].unk44 + ((s32 *)D_8009C620[i].unk44)[1];
        D_8009C620[i].parent = NULL;
    }
    D_80050100 = 2;
    D_8009C16C = -1;
    D_8009C840 = -1;
    D_8009C620[4].visible = 1;
}

/* Free the scene objects' primitives and definitions, then the objects. */
void func_80084818(void) {
    s32 i;

    for (i = 0; i < D_8009D7E0; i++) {
        func_800320E8(D_8009C620[i].prims);
        func_8002CBBC(D_8009C620[i].def);
    }
    func_800320E8(D_8009C620);
}

/* Link scene object `child` to `parent`. */
void func_800848B4(s32 parent, s32 child) {
    D_8009C620[child].parent = &D_8009C620[parent];
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800848F4);

/* Probe the solid scene objects; the first hit's result, with its index. */
s16 func_80084D00(s32 probe, s16 *hit) {
    SceneObject *object;
    s16 i;
    s16 result;

    object = D_8009C620;
    i = 0;
    while (i < D_8009D7E0) {
        if (object->flags & 1) {
            result = func_80084DB8(probe, i);
            if (result != 0) {
                *hit = i;
                return result;
            }
        }
        i++;
        object++;
    }
    return 0;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80084DB8);

/* Project `position` onto face `face` of scene object `index`: `offset` gets
 * the object-relative x/z, `normal` the face normal and offset->vy the
 * height of the face plane there. */
#ifdef NON_MATCHING /* one more callee-saved register: face and vertices do not share */
void func_80085158(VECTOR *position, VECTOR *offset, VECTOR *normal, u16 index, u16 face) {
    s32 flag;
    VECTOR *edge1;
    VECTOR *edge2;
    SceneObject *object;
    MeshFace *corners;
    SVECTOR *vertices;
    s32 x;
    s32 y;
    s32 z;

    object = &D_8009C620[index];
    offset->vx = (position->vx >> 12) - object->position.vx;
    offset->vz = object->position.vz - (position->vz >> 12);
    FACE_SCRATCH->m = object->matrix;
    FACE_SCRATCH->m.t[2] = 0;
    FACE_SCRATCH->m.t[0] = 0;
    FACE_SCRATCH->p[1].vz = 0x800;
    FACE_SCRATCH->p[1].vy = 0x800;
    FACE_SCRATCH->p[1].vx = 0x800;
    FACE_SCRATCH->m.t[1] = object->position.vy;
    ScaleMatrix(&FACE_SCRATCH->m, &FACE_SCRATCH->p[1]);
    SetRotMatrix(&FACE_SCRATCH->m);
    SetTransMatrix(&FACE_SCRATCH->m);
    edge1 = &FACE_SCRATCH->p[1];
    edge2 = &FACE_SCRATCH->p[2];
    vertices = ((Mesh *)object->unk44)->vertices;
    corners = ((Mesh *)object->unk44)->faces + face;
    func_8004A6DC(&vertices[corners->corner[0]], &FACE_SCRATCH->p[0], &flag);
    func_8004A6DC(&vertices[corners->corner[1]], &FACE_SCRATCH->p[1], &flag);
    func_8004A6DC(&vertices[corners->corner[2]], &FACE_SCRATCH->p[2], &flag);
    x = FACE_SCRATCH->p[0].vx;
    y = FACE_SCRATCH->p[0].vy;
    z = FACE_SCRATCH->p[0].vz;
    edge1->vx -= x;
    edge1->vy -= y;
    edge1->vz -= z;
    edge2->vx -= x;
    edge2->vy -= y;
    edge2->vz -= z;
    OuterProduct0(&FACE_SCRATCH->p[2], &FACE_SCRATCH->p[1], &FACE_SCRATCH->p[2]);
    edge2->vx >>= 2;
    edge2->vy >>= 2;
    edge2->vz >>= 2;
    func_80048D7C(&FACE_SCRATCH->p[2], normal);
    func_800935DC(offset, &FACE_SCRATCH->p[0], normal);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085158);
#endif

/* Does the vertical segment from `position` down by `height` cross the plane
 * of face `face` of scene object `index`? Returns -1 if so, else 0. */
#ifdef NON_MATCHING /* register allocation and load scheduling differ */
s32 func_80085418(VECTOR *position, s32 height, u16 index, u16 face) {
    s32 flag;
    SceneObject *object;
    MeshFace *corners;
    SVECTOR *vertices;
    VECTOR *origin;
    VECTOR *edge1;
    VECTOR *edge2;
    s32 x;
    s32 y;
    s32 z;

    object = &D_8009C620[index];
    FACE_SCRATCH->m = object->matrix;
    FACE_SCRATCH->m.t[2] = 0;
    FACE_SCRATCH->m.t[0] = 0;
    FACE_SCRATCH->p[1].vz = 0x800;
    FACE_SCRATCH->p[1].vy = 0x800;
    FACE_SCRATCH->p[1].vx = 0x800;
    FACE_SCRATCH->m.t[1] = object->position.vy;
    ScaleMatrix(&FACE_SCRATCH->m, &FACE_SCRATCH->p[1]);
    SetRotMatrix(&FACE_SCRATCH->m);
    SetTransMatrix(&FACE_SCRATCH->m);
    edge1 = &FACE_SCRATCH->p[1];
    edge2 = &FACE_SCRATCH->p[2];
    vertices = ((Mesh *)object->unk44)->vertices;
    corners = ((Mesh *)object->unk44)->faces + face;
    func_8004A6DC(&vertices[corners->corner[0]], &FACE_SCRATCH->p[0], &flag);
    func_8004A6DC(&vertices[corners->corner[1]], &FACE_SCRATCH->p[1], &flag);
    func_8004A6DC(&vertices[corners->corner[2]], &FACE_SCRATCH->p[2], &flag);
    origin = &FACE_SCRATCH->p[0];
    x = origin->vx;
    y = origin->vy;
    z = origin->vz;
    edge1->vx -= x;
    edge1->vy -= y;
    edge1->vz -= z;
    edge2->vx -= x;
    edge2->vy -= y;
    edge2->vz -= z;
    OuterProduct0(&FACE_SCRATCH->p[2], &FACE_SCRATCH->p[1], &FACE_SCRATCH->p[2]);
    edge2->vx >>= 2;
    edge2->vy >>= 2;
    edge2->vz >>= 2;
    func_80048D7C(&FACE_SCRATCH->p[2], &FACE_SCRATCH->normal);
    FACE_SCRATCH->probe.m[1][0] = FACE_SCRATCH->probe.m[0][0] =
        (position->vx >> 12) - object->position.vx - origin->vx;
    FACE_SCRATCH->probe.m[0][1] = (position->vy >> 12) - origin->vy;
    FACE_SCRATCH->probe.m[1][1] = FACE_SCRATCH->probe.m[0][1] - height;
    FACE_SCRATCH->probe.m[1][2] = FACE_SCRATCH->probe.m[0][2] =
        object->position.vz - (position->vz >> 12) - origin->vz;
    ApplyMatrixLV(&FACE_SCRATCH->probe, &FACE_SCRATCH->normal, &FACE_SCRATCH->side);
    return (FACE_SCRATCH->side.vx ^ FACE_SCRATCH->side.vy) >> 31;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085418);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085760);

/* Draw the actors' model sprites: place each visible model relative to the
 * camera target, project it for its depth, then add it to the ordering
 * table and turn its facing towards the actor heading, 0x100 per frame. */
void func_80085CDC(void) {
    WorldmapActor *actor;
    ModelObject *model;
    s32 i;
    s32 offset;
    s32 diff;
    s32 facing;
    s32 heading;
    DepthScratch *scratch;

    scratch = DEPTH_SCRATCH;
    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        if ((actor->unk24 == 0) & (actor->handle != 0)) {
            scratch->offset.vx = actor->position.vx - D_8009BE28.target.vx;
            scratch->offset.vz = actor->position.vz - D_8009BE28.target.vz;
            func_80093484(&scratch->offset);
            ((ModelObject *)actor->handle)->position.vx = scratch->offset.vx * 16;
            ((ModelObject *)actor->handle)->position.vz = -scratch->offset.vz * 16;
            ((ModelObject *)actor->handle)->position.vy = actor->position.vy * 16;
        }
    }
    SetRotMatrix(&D_8009C808);
    SetTransMatrix(&D_8009C808);
    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        model = (ModelObject *)actor->handle;
        if ((actor->unk24 == 0) & (model != NULL)) {
            scratch->vertex.vx = model->position.vx >> 16;
            scratch->vertex.vy = ((ModelObject *)actor->handle)->position.vy >> 16;
            scratch->vertex.vz = ((ModelObject *)actor->handle)->position.vz >> 16;
            gte_ldv0(&scratch->vertex);
            gte_rtps();
            gte_stsz(&scratch->depth[i]);
        }
    }
    func_80024FF4(&D_8009C808);
    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        if ((actor->unk24 == 0) & (actor->handle != 0) & (scratch->depth[i] < 0xB00)) {
            func_8001E298(actor->handle, &D_8009BE3C->ot[scratch->depth[i] >> 4]);
            heading = actor->unk48;
            facing = actor->unk5C;
            diff = heading - facing;
            if (diff < 0) {
                diff += 0x1000;
            }
            /* diff becomes the new facing, turned at most 0x100 */
            if (diff < 0x801) {
                if (diff < 0x100) {
                    diff = heading;
                } else {
                    diff = facing + 0x100;
                }
            } else {
                diff -= 0x1000;
                if (diff >= -0xFF) {
                    diff = heading;
                } else {
                    diff = facing - 0x100;
                }
            }
            actor->unk5C = diff;
            func_800223B0(actor->handle, (diff - (u16)D_8009BD38.vy - 0x400) & 0xFFF);
            func_80023210(actor->handle);
        }
    }
}

/* Resolve the terrain texture offsets and create the terrain CLUTs. */
void func_80085F58(void) {
    TerrainTexture *texture;
    s32 i;

    texture = D_8009C7EC;
    for (i = 0; i < 0x100; i++, texture++) {
        if (texture->data != NULL) {
            texture->data += (s32)D_8009C7EC;
        }
    }
    for (i = 0; i < 0x10; i++) {
        D_8009D478[i] = GetClut(0xF0, i + 0x1F0);
    }
}

/* Allocate the two buffers of 512 opaque textured 32x48 quads on the
 * 0x380,0x100 page, the second a copy of the first. */
void func_80085FE0(void) {
    PolyFT4 *quad;
    s32 i;

    D_8009D7E8[0] = func_80031BDC(sizeof(QuadBlock512), 1);
    D_8009D7E8[1] = func_80031BDC(sizeof(QuadBlock512), 1);
    quad = D_8009D7E8[0];
    for (i = 0; i < 0x200; i++, quad++) {
        setPolyFT4(quad);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad->u0 = 0;
        quad->v0 = 0x40;
        quad->u1 = 0x1F;
        quad->v1 = 0x40;
        quad->u2 = 0;
        quad->v2 = 0x6F;
        quad->u3 = 0x1F;
        quad->v3 = 0x6F;
        quad->clut = GetClut(0xF0, 0x1FF);
        quad->tpage = GetTPage(0, 0, 0x380, 0x100);
    }
    *(QuadBlock512 *)D_8009D7E8[1] = *(QuadBlock512 *)D_8009D7E8[0];
}

/* Free two work buffers. */
void func_80086124(void) {
    func_800320E8(D_8009D7E8[1]);
    func_800320E8(D_8009D7E8[0]);
}

/* Draw the 5x5 terrain blocks around the cursor: set up the scratchpad quad,
 * camera and roll matrices and CLUTs, then submit each present block's
 * texture list into the current quad buffer. */
void func_8008615C(void) {
    TerrainTexture *textures;
    s32 index;
    s32 i;
    s32 row;

    TERRAIN_SCRATCH->corner[0].vx = -0x18;
    TERRAIN_SCRATCH->corner[0].vy = -0x48;
    TERRAIN_SCRATCH->corner[0].vz = 0;
    TERRAIN_SCRATCH->corner[1].vx = 0x18;
    TERRAIN_SCRATCH->corner[1].vy = -0x48;
    TERRAIN_SCRATCH->corner[1].vz = 0;
    TERRAIN_SCRATCH->corner[2].vx = -0x18;
    TERRAIN_SCRATCH->corner[2].vy = 0;
    TERRAIN_SCRATCH->corner[2].vz = 0;
    TERRAIN_SCRATCH->corner[3].vx = 0x18;
    TERRAIN_SCRATCH->corner[3].vy = 0;
    TERRAIN_SCRATCH->corner[3].vz = 0;
    TERRAIN_SCRATCH->view = D_8009C808;
    TERRAIN_SCRATCH->roll = *(MATRIX *)&D_8009A180;
    RotMatrixZ(-D_8009BD38.vz, &TERRAIN_SCRATCH->roll);
    for (i = 0; i < 0x10; i++) {
        TERRAIN_SCRATCH->clut[i] = D_8009D478[i];
    }
    textures = D_8009C7EC;
    D_8009BE04 = 0;
    for (row = 0; row < 5; row++) {
        for (i = 0; i < 5; i++) {
            if (D_8009D618[row * 5 + i] != -1) {
                index = D_8009D570[(row + D_8009C838.z) * 9 + i + D_8009C838.x];
                if (textures[index].unk4 != 0) {
                    func_80099BFC(textures[index].data, textures[index].unk4, D_8009BE3C->ot,
                                  (PolyFT4 *)D_8009D7E8[D_8009D7F0] + D_8009BE04);
                }
            }
        }
    }
}

/* Scatter the 80 drifting positions: five template points repeated over a
 * 4x4 grid of 0x800-unit cells at random heights, with random velocities. */
void func_800863E0(void) {
    Drift *drift;
    DriftVelocity *velocity;
    s32 row;
    s32 column;
    s32 i;

    D_8009D150 = func_80031BDC(0x50 * sizeof(Drift), 0);
    D_8009CEB4 = func_80031BDC(0x50 * sizeof(DriftVelocity), 0);
    drift = D_8009D150;
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            for (i = 0; i < 5; i++, drift++) {
                drift->x = (D_8009AF30[i].x + (column << 11)) << 12;
                drift->z = (D_8009AF30[i].z + (row << 11)) << 12;
                drift->unk4 = (-(rand() >> 10) * 8 - 0x200) << 12;
            }
        }
    }
    velocity = D_8009CEB4;
    for (i = 0; i < 0x50; i++) {
        column = (rand() & 3) + 1; /* reused as the speed */
        velocity->dx = column * 0xDDB;
        velocity->dz = -(column << 11);
        velocity->unk2 = rand() & 1;
        velocity++;
    }
}

/* Free two work buffers. */
void func_80086568(void) {
    func_800320E8(D_8009CEB4);
    func_800320E8(D_8009D150);
}

/* Allocate the two buffers of 0x120 semi-transparent grey textured quads
 * on the 0x3C0,0x100 page, the second a copy of the first. */
#ifdef NON_MATCHING /* the code byte 0x2C is not hoisted out of the loop */
void func_800865A0(void) {
    PolyFT4 *quad;
    s32 i;

    D_8009D7F8[0] = func_80031BDC(sizeof(QuadBlock288), 1);
    D_8009D7F8[1] = func_80031BDC(sizeof(QuadBlock288), 1);
    quad = D_8009D7F8[0];
    for (i = 0; i < 0x120; i++, quad++) {
        setPolyFT4(quad);
        setRGB0(quad, 0x26, 0x26, 0x26);
        quad->tpage = GetTPage(0, 1, 0x3C0, 0x100);
        quad->clut = GetClut(0x130, 0x1FE);
        SetSemiTrans(quad, 1);
    }
    *(QuadBlock288 *)D_8009D7F8[1] = *(QuadBlock288 *)D_8009D7F8[0];
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800865A0);
#endif

/* Free two work buffers. */
void func_800866C8(void) {
    func_800320E8(D_8009D7F8[1]);
    func_800320E8(D_8009D7F8[0]);
}

/* Move the 80 drifting positions, wrapping them on the 0x2000-unit world. */
void func_80086700(void) {
    Drift *drift;
    DriftVelocity *velocity;
    s32 x;
    s32 z;
    s32 i;

    for (i = 0; i < 0x50; i++) {
        drift = &D_8009D150[i];
        velocity = &D_8009CEB4[i];
        x = drift->x + velocity->dx;
        z = drift->z + velocity->dz;
        if (x > 0x1FFFFFF) {
            x -= 0x2000000;
        }
        if (x < 0) {
            x += 0x2000000;
        }
        if (z > 0x1FFFFFF) {
            z -= 0x2000000;
        }
        if (z < 0) {
            z += 0x2000000;
        }
        drift->x = x;
        drift->z = z;
    }
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80086798);

/* Reset an actor to step 0 with parameter 8. */
s32 func_80087710(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

/* Spin scene objects 12 and 14 about their own axis. */
s32 func_80087734(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    SVECTOR *first;
    SVECTOR *second;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    first = SCRIPT_VECTOR;
    second = SCRIPT_VECTOR + 1;
    second->vx = 0;
    first->vx = 0;
    first->vy = objects[12].angle.vy;
    second->vy = objects[14].angle.vy;
    first->vz = second->vz = actor->u.step;
    func_8003F738(first, &objects[12].matrix);
    func_8003F738(second, &objects[14].matrix);
    return 1;
}

/* Reset an actor to step 0 with parameter 8. */
s32 func_800877E0(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

/* Spin the current area's two scene objects about their own axis. */
s32 func_80087804(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    SceneObject *objects;
    SceneObject *first;
    SceneObject *second;
    s32 first_id;
    s32 second_id;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    first_id = D_8009B624[D_8009C610][0];
    second_id = D_8009B624[D_8009C610][1];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    SCRIPT_VECTOR[1].vx = 0;
    SCRIPT_VECTOR[0].vx = 0;
    first = &objects[first_id];
    SCRIPT_VECTOR[0].vy = first->angle.vy;
    second = &objects[second_id];
    SCRIPT_VECTOR[1].vy = second->angle.vy;
    SCRIPT_VECTOR[0].vz = SCRIPT_VECTOR[1].vz = actor->u.step;
    func_8003F738(&SCRIPT_VECTOR[0], &first->matrix);
    func_8003F738(&SCRIPT_VECTOR[1], &second->matrix);
    return 1;
}

/* Give `count` quads the semi-transparent 0x1A0,0xA0 texture page. */
void func_80087904(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        quads->tpage = GetTPage(0, abr, 0x1A0, 0xA0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}

/* Reset an actor to step 0 with parameter 0x10 and rebuild the area's two
 * scene objects. */
#ifdef NON_MATCHING /* actor index scaled into a separate register */
s32 func_800879A8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 0x10;
    func_800879E0();
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800879A8);
#endif

/* Rebuild the primitives of the current area's two scene objects. */
s32 func_800879E0(void) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    SceneObject *first;
    SceneObject *second;

    first = &D_8009C620[D_8009B64C[D_8009C610][0]];
    second = &D_8009C620[D_8009B64C[D_8009C610][1]];
    func_80087904(first, first->prims, first->def->count, 3);
    func_80087904(second, second->prims, second->def->count, 3);
    return 1;
}

/* Roll the current area's two scene objects together. */
s32 func_80087A8C(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    SceneObject *objects;
    SceneObject *first;
    SceneObject *second;
    s32 first_id;
    s32 second_id;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    first_id = D_8009B64C[D_8009C610][0];
    second_id = D_8009B64C[D_8009C610][1];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    first = &objects[first_id];
    SCRIPT_VECTOR[1].vz = 0;
    SCRIPT_VECTOR[0].vz = 0;
    SCRIPT_VECTOR[1].vx = 0;
    SCRIPT_VECTOR[0].vx = 0;
    SCRIPT_VECTOR[0].vy = SCRIPT_VECTOR[1].vy = actor->u.step;
    second = &objects[second_id];
    func_8003F738(&SCRIPT_VECTOR[0], &first->matrix);
    func_8003F738(&SCRIPT_VECTOR[1], &second->matrix);
    return 1;
}

/* Build a rotation matrix whose third row faces `direction`, using `up`
 * as scratch for the first two rows. */
void func_80087B84(VECTOR *direction, VECTOR *up, MATRIX *m) {
    up->vz = 0;
    up->vx = 0;
    up->vy = 0x1000;
    func_8004A480(up, direction, up);
    func_80048D7C(up, up);
    m->m[0][0] = up->vx;
    m->m[0][1] = up->vy;
    m->m[0][2] = up->vz;
    func_8004A480(direction, up, up);
    func_80048D7C(up, up);
    m->m[1][0] = up->vx;
    m->m[1][1] = up->vy;
    m->m[1][2] = up->vz;
    m->m[2][0] = direction->vx;
    m->m[2][1] = direction->vy;
    m->m[2][2] = direction->vz;
    func_8004A8EC(m, m);
}

/* Start the area's ferry: before scene 0xCD it rests at a fixed dock;
 * otherwise it resumes its route (first time: at waypoint 0), advancing
 * when within 8 units of the waypoint, and heads for the next one. */
#ifdef NON_MATCHING /* load/store scheduling around the waypoint reads and the history index */
s32 func_80087C6C(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    VECTOR *work;
    s32 id;
    s32 distance;
    s32 i;
    u16 z;

    id = D_8009B674[D_8009C610];
    func_80087F60();
    work = (VECTOR *)0x1F800000;
    actor = &D_8009BE24[index];
    actor->unk4A = 0x4000;
    actor->state = 0;
    object = &D_8009C620[id];
    if (D_8006EF64[0] < 0xCD) {
        actor->position.vx = 0xD80000;
        actor->position.vz = 0x7280000;
        object->matrix = *(MATRIX *)&D_8009A180;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    } else {
        if (D_8006EE7E == 0) {
            D_8006EE7E++;
            actor->u.step = 0;
            actor->position.vx = D_8009AF80[actor->u.step] << 12;
            z = D_8009AF90[actor->u.step];
        } else {
            actor->u.step = D_8006EE78[2];
            actor->position.vx = D_8006EE78[0] << 12;
            z = D_8006EE78[1];
        }
        actor->position.vz = z << 12;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        work->vx = D_8009AF80[actor->u.step] << 12;
        work->vz = D_8009AF90[actor->u.step] << 12;
        distance = func_80094154(&actor->position, work);
        if (distance < 0) {
            distance = -distance;
        }
        if (distance < 8) {
            actor->u.step = (actor->u.step + 1) & 7;
        }
        work->vx = D_8009AF80[actor->u.step] - (actor->position.vx >> 12);
        work->vy = 0;
        work->vz = D_8009AF90[actor->u.step] - (actor->position.vz >> 12);
        func_80093534(work);
        func_80048D7C(work, work);
        for (i = 0; i < 0x20; i++) {
            D_8009CD68[i].dx = work->vx;
            D_8009CD68[i].dz = work->vz;
        }
        actor->unk58 = 1;
        actor->unk54 = 0;
        actor->motion.vx = D_8009CD68[actor->unk58].dx;
        actor->motion.vy = 0;
        actor->motion.vz = D_8009CD68[actor->unk58].dz;
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087C6C);
#endif

/* Link the four objects before the area's scene object to it. */
s32 func_80087F60(void) {
    u16 object;

    object = D_8009B674[D_8009C610];
    func_800848B4(object, object - 4);
    func_800848B4(object, object - 3);
    func_800848B4(object, object - 2);
    func_800848B4(object, object - 1);
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087FD0);

/* Start the actor circling above the map from the saved position (first
 * time: from 0, 0x480), and save it back. */
s32 func_80088570(s32 index) {
    WorldmapActor *actor;

    func_8008868C();
    actor = &D_8009BE24[index];
    actor->unk54 = 4;
    actor->state = 0;
    actor->u.step = 0;
    actor->unk58 = 0;
    actor->unk5C = 0xC;
    if (D_8006EE80.count == 0) {
        D_8006EE80.count++;
        actor->position.vy = -0x280000;
        actor->position.vx = 0;
        actor->position.vz = 0x4800000;
    } else {
        actor->position.vx = (D_8006EE80.x << 12) + D_8006EE80.x_frac;
        actor->position.vy = -0x280000;
        actor->position.vz = (D_8006EE80.z << 12) + D_8006EE80.z_frac;
    }
    actor->motion.vz = 0xB50;
    actor->motion.vx = 0xB50;
    actor->motion.vy = 0;
    actor->unk4A = 1;
    D_8006EE80.x_frac = actor->position.vx;
    D_8006EE80.x = actor->position.vx >> 12;
    D_8006EE80.z_frac = actor->position.vz;
    D_8006EE80.z = actor->position.vz >> 12;
    return 1;
}

/* Link the area's scene objects in the listed (parent, child) pairs. */
s32 func_8008868C(void) {
    s32 i;
    s32 base;

    base = D_8009B688[D_8009C610];
    for (i = 0; D_8009AFA0[i] != -1; i += 2) {
        func_800848B4(base + D_8009AFA0[i], base + D_8009AFA0[i + 1]);
    }
    return 1;
}

/* Fly the airship: spin its rotors, stop over the saved landing point when
 * low enough, move, place its shadow object and save the position. */
#ifdef NON_MATCHING /* scheduling of the rotor angle stores and block-move registers */
s32 func_80088720(s32 index) {
    WorldmapActor *actor;
    VECTOR *work;
    s32 base;

    actor = &D_8009BE24[index];
    base = D_8009B688[D_8009C610];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    FLIGHT_SCRATCH->rotor.vx = FLIGHT_SCRATCH->rotor.vz = 0;
    FLIGHT_SCRATCH->tail.vx = FLIGHT_SCRATCH->tail.vz = 0;
    FLIGHT_SCRATCH->rotor.vy = actor->u.step;
    FLIGHT_SCRATCH->tail.vy = actor->unk58;
    func_8004A92C(&FLIGHT_SCRATCH->rotor, &FLIGHT_SCRATCH->rotor_matrix);
    func_8004A92C(&FLIGHT_SCRATCH->tail, &FLIGHT_SCRATCH->tail_matrix);
    D_8009C620[base + 5].matrix = FLIGHT_SCRATCH->rotor_matrix;
    work = &FLIGHT_SCRATCH->work;
    D_8009C620[base].matrix = D_8009C620[base + 1].matrix = D_8009C620[base + 2].matrix =
        D_8009C620[base + 3].matrix = FLIGHT_SCRATCH->tail_matrix;
    work->vx = (u16)D_8006EE54.unk60 - (actor->position.vx >> 12);
    work->vy = D_8006EE54.unk62;
    work->vz = (u16)D_8006EE54.unk64 - (actor->position.vz >> 12);
    func_80093534(work);
    if (SquareRoot0(work->vx * work->vx + work->vz * work->vz) < 0x300 && work->vy < -0x240) {
        actor->unk4A = 0;
    } else {
        actor->unk4A = 1;
    }
    actor->position.vx += actor->motion.vx * actor->unk4A;
    actor->position.vz += actor->motion.vz * actor->unk4A;
    func_80093354(&actor->position);
    work->vx = actor->position.vx >> 12;
    work->vy = actor->position.vy >> 12;
    work->vz = actor->position.vz >> 12;
    D_8009C620[base + 12].position = *work;
    func_8008BFD4(index, &actor->position, 0x180, 0xC0);
    D_8006EE80.x_frac = actor->position.vx;
    D_8006EE80.x = actor->position.vx >> 12;
    D_8006EE80.z_frac = actor->position.vz;
    D_8006EE80.z = actor->position.vz >> 12;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088720);
#endif

/* Link the listed scene objects to object 69 and place it at the actor;
 * outside scene 0x99 also show them. */
s32 func_80088B40(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 i;
    s32 result;

    for (i = 0; D_8009AFDC[i] != -1; i++) {
        func_800848B4(69, D_8009AFDC[i]);
    }
    actor = &D_8009BE24[index];
    object = D_8009C620;
    actor->state = 0;
    if (D_8006EF64[0] == 0x99) {
        result = 1;
        actor->position.vx = 0x2000000;
        actor->position.vz = 0x4120000;
        actor->position.vy = 0;
    } else {
        object[69].visible = 1;
        for (i = 0; D_8009AFDC[i] != -1; i++) {
            object[D_8009AFDC[i]].visible = 1;
        }
        result = 3;
    }
    object += 69;
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    return result;
}

/* Show scene object 69 and the listed objects. */
s32 func_80088C90(void) {
    SceneObject *objects;
    s32 i;

    objects = D_8009C620;
    objects[69].visible = 1;
    for (i = 0; D_8009AFDC[i] != -1; i++) {
        objects[D_8009AFDC[i]].visible = 1;
    }
    return 3;
}

/* In scene 0x99, move scene object 69 to the actor (world units). */
s32 func_80088D00(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = D_8009C620;
    actor = &D_8009BE24[index];
    object += 69;
    if (D_8006EF64[0] == 0x99) {
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
    }
    return 1;
}

/* Place the actor just above the current area's scene object. */
s32 func_80088D64(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = &D_8009C620[D_8009B69C[D_8009C610]];
    actor = &D_8009BE24[index];
    actor->position.vx = object->position.vx << 12;
    actor->position.vy = (object->position.vy << 12) + 0x40000;
    actor->position.vz = object->position.vz << 12;
    return 1;
}

/* Place the actor at (0x68, 0x60). */
s32 func_80088DE4(s32 index) {
    func_8008BFD4(index, &D_8009BE24[index].position, 0x68, 0x60);
    return 1;
}

/* Place the actor at scene object 75. */
s32 func_80088E1C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = D_8009C620[75].position.vx << 12;
    actor->position.vy = D_8009C620[75].position.vy << 12;
    actor->position.vz = D_8009C620[75].position.vz << 12;
    return 1;
}

/* Place the actor at (0x10C, 0x1A6). */
s32 func_80088E68(s32 index) {
    func_8008BFD4(index, &D_8009BE24[index].position, 0x10C, 0x1A6);
    return 1;
}

/* Place the actor at the current area's scene object. */
s32 func_80088EA0(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = &D_8009C620[D_8009B6B0[D_8009C610]];
    actor = &D_8009BE24[index];
    actor->position.vx = object->position.vx << 12;
    actor->position.vy = object->position.vy << 12;
    actor->position.vz = object->position.vz << 12;
    return 1;
}

/* Place the actor at (0xC9, 0x392). */
s32 func_80088F1C(s32 index) {
    func_8008BFD4(index, &D_8009BE24[index].position, 0xC9, 0x392);
    return 1;
}

/* Scene step with nothing to do. */
s32 func_80088F54(void) {
    return 3;
}

/* Scene step with nothing to do. */
s32 func_80088F5C(void) {
    return 3;
}

/* Clear the area objects and allocate the effect slots. */
void func_80088F64(void) {
    AreaObject *object;
    EffectSlot *slot;
    s32 i;

    object = D_8009BCC0;
    for (i = 0x1FF; i != -1; i--) {
        object->unk4 = 0;
        object->unkA = 0;
        object->unk12 = 0;
        object->unk18 = 0;
        object->unk16 = 0;
        object->unk14 = 0;
        object->unk20 = 0;
        object->unk1E = 0;
        object->unk1C = 0;
        object++;
    }
    D_8009BDF4 = slot = func_80031BDC(0x4C00, 0);
    for (i = 0xFF; i != -1; i--) {
        slot->unk6 = 0;
        slot->active = 0;
        slot++;
    }
}

/* Free the effect table. */
void func_80088FF4(void) {
    func_800320E8(D_8009BDF4);
}

/* Allocate the two effect quad buffers: semi-transparent textured quads
 * on the 0x340,0x100 page, the second a copy of the first. */
void func_8008901C(void) {
    PolyFT4 *quad;
    s32 i;

    D_8009BE1C[0] = func_80031BDC(sizeof(EffectQuads), 1);
    D_8009BE1C[1] = func_80031BDC(sizeof(EffectQuads), 1);
    quad = D_8009BE1C[0];
    for (i = 0xFF; i != -1; i--) {
        setPolyFT4(quad);
        quad->tpage = GetTPage(1, 1, 0x340, 0x100);
        quad->clut = GetClut(0x100, 0x1FF);
        setSemiTrans(quad, 1);
        quad++;
    }
    *(EffectQuads *)D_8009BE1C[1] = *(EffectQuads *)D_8009BE1C[0];
}

/* Free two effect buffers. */
void func_80089128(void) {
    func_800320E8(D_8009BE1C[0]);
    func_800320E8(D_8009BE1C[1]);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089160);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800893E0);

/* Deactivate the eight area objects of group `group`. */
void func_800894C8(s32 group) {
    AreaObject *object;
    s32 i;

    object = &D_8009BCC0[group * 8];
    for (i = 7; i != -1; i--) {
        object->flags &= 0x7F;
        object++;
    }
}

/* Stop the effects that belong to group `group`. */
void func_80089514(s32 group) {
    EffectSlot *slot;
    s32 i;
    s32 j;

    slot = D_8009BDF4;
    for (i = 0xFF; i != -1; i--) {
        for (j = 0; j < 8; j++) {
            if (slot->id == group * 8 + j && slot->unk6 != 0) {
                slot->active = 0;
                break;
            }
        }
        slot++;
    }
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089580);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089748);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089C78);

/* Create the party leader's model sprite at the scene's entry position; in
 * movement modes 1-7 follow the player or start hidden. Fill the position
 * trail and save the position as the world-map return point. */
s32 func_8008A2C8(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 i;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->unk24 = 0;
    actor->position.vx = D_8006EF64[0] << 12;
    actor->position.vz = D_8006EF64[1] << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk48 = D_8009C584;
    actor->unk4A = 8;
    actor->unk5C = actor->unk48;
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        if (D_8006F8E5 == 0) {
            actor->position.vx = D_8009C5AC.vx;
            actor->position.vz = D_8009C5AC.vz;
            actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
            actor->unk48 = D_8009C584;
            D_8009D55C.target = actor->position;
            D_8009D52C = actor->unk48;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (D_8006F368[0] != 0xFF) {
            D_8006F8E5 = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (D_8006F368[0] != 0xFF) {
            D_8006F8E5 = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    D_8009D154 = 0;
    i = 0;
    point = D_8009CEC4;
    do {
        point->position = actor->position;
        i++;
        point->heading = actor->unk48;
        point++;
    } while (i < 0x20);
    D_8006EE54.x = actor->position.vx >> 12;
    D_8006EE54.z = actor->position.vz >> 12;
    D_8006EE54.heading = actor->unk48;
    return 1;
}

/* Create the lead party member's model sprite for the actor. */
s32 func_8008A52C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    return 1;
}

/* Create the party leader's model sprite at the saved world-map position
 * and fill the position trail and saved camera target with it. */
s32 func_8008A5B8(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 i;
    s16 heading;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    i = 0;
    point = D_8009CEC4;
    heading = D_8006EE54.heading;
    actor->unk24 = 1;
    actor->unk4A = 8;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->state = 2;
    D_8009D154 = 0;
    actor->unk48 = heading;
    do {
        point->position = actor->position;
        i++;
        point->heading = actor->unk48;
        point++;
    } while (i < 0x20);
    D_8009D55C.target = actor->position;
    D_8009D52C = actor->unk48;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008A72C);

/* Create party member 2's model sprite (if present) at the saved world-map
 * position; in movement modes 1-7 follow the player or start hidden. */
s32 func_8008B2BC(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[1] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
        actor->unk24 = 0;
    } else {
        actor->unk24 = 1;
        result = 3;
    }
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk48 = D_8006EE54.heading;
    actor->unk4A = 8;
    actor->unk58 = 0xF;
    actor->unk5C = actor->unk48;
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        if (D_8006F8E6 == 0) {
            actor->position.vx = D_8009C5AC.vx;
            actor->position.vz = D_8009C5AC.vz;
            actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
            actor->unk48 = D_8009C584;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (D_8006F368[1] != 0xFF) {
            D_8006F8E6 = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (D_8006F368[1] != 0xFF) {
            D_8006F8E6 = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    return result;
}

/* Create the second party member's model sprite, if present. */
s32 func_8008B498(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[1] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
    } else {
        result = 3;
    }
    return result;
}

/* Create party member 2's model sprite at the saved world-map position. */
s32 func_8008B54C(s32 index) {
    WorldmapActor *actor;
    s16 heading;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    heading = D_8006EE54.heading;
    actor->unk24 = 1;
    actor->unk4A = 8;
    actor->state = 2;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk58 = 0xF;
    actor->unk48 = heading;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008B644);

/* Create party member 3's model sprite (if present) at the saved world-map
 * position; in movement modes 1-7 follow the player or start hidden. */
s32 func_8008BB40(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[2] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
        actor->unk24 = 0;
    } else {
        actor->unk24 = 1;
        result = 3;
    }
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk48 = D_8006EE54.heading;
    actor->unk4A = 8;
    actor->unk58 = 0x1E;
    actor->unk5C = actor->unk48;
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        if (D_8006F8E7 == 0) {
            actor->position.vx = D_8009C5AC.vx;
            actor->position.vz = D_8009C5AC.vz;
            actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
            actor->unk48 = D_8009C584;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (D_8006F368[2] != 0xFF) {
            D_8006F8E7 = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (D_8006F368[2] != 0xFF) {
            D_8006F8E7 = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    return result;
}

/* Create the third party member's model sprite, if present. */
s32 func_8008BD1C(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (D_8006F368[2] != 0xFF) {
        actor->handle = func_80024524(D_8009CD34[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
        func_800245D8(actor->handle, 0);
        func_80022000(actor->handle, 0x1800);
        ((s32 *)actor->handle)[15] &= ~4;
    } else {
        result = 3;
    }
    return result;
}

/* Create party member 3's model sprite at the saved world-map position. */
s32 func_8008BDD0(s32 index) {
    WorldmapActor *actor;
    s16 heading;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009CD34[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x1800);
    ((s32 *)actor->handle)[15] &= ~4;
    actor->position.vx = D_8006EE54.x << 12;
    actor->position.vz = D_8006EE54.z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    heading = D_8006EE54.heading;
    actor->unk24 = 1;
    actor->unk4A = 8;
    actor->state = 2;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk58 = 0x1E;
    actor->unk48 = heading;
    return 1;
}

/* Step the actor towards its target (x, z in world units) at half its
 * speed; bit 0/1 of the result report x/z arrival. */
#ifdef NON_MATCHING /* target and position registers swapped */
s32 func_8008BEC8(WorldmapActor *actor) {
    s32 arrived;
    s32 distance;
    s32 position;

    position = actor->position.vx;
    distance = actor->u.step - (position >> 12);
    arrived = 0;
    if (ABS(distance) >= 5) {
        actor->position.vx = position + actor->motion.vx * (actor->unk4A / 2);
    } else {
        arrived = 1;
    }
    position = actor->position.vz;
    distance = actor->unk54 - (position >> 12);
    if (ABS(distance) >= 5) {
        actor->position.vz = position + actor->motion.vz * (actor->unk4A / 2);
    } else {
        arrived |= 2;
    }
    func_80093354(&actor->position);
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    return arrived;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008BEC8);
#endif

/* Queue a placement of actor `index` at `position`, facing (x, z). */
void func_8008BFD4(s32 index, VECTOR *position, s32 x, s32 z) {
    PlaceRequest *request;

    request = &D_8009BE6C[D_8009BD04];
    request->actor = index;
    request->px = position->vx;
    request->py = position->vy;
    request->pz = position->vz;
    request->z = (s16)z;
    request->x = x;
    D_8009BD04 = (D_8009BD04 + 1) & 0x1F;
}

/* Find the first queued placement that overlaps a cylinder at `position`
 * (radius, height): *hit is 2 inside its radius, 1 within 16 more units. */
void func_8008C040(VECTOR *position, s32 radius, s32 height, u8 *hit, u8 *actor) {
    VECTOR delta;
    VECTOR unused; /* unreferenced; the original frame reserves it */
    PlaceRequest *request;
    s32 i;
    s32 top;
    s32 bottom;
    s32 distance;
    s32 reach;

    *hit = 0;
    *actor = 0;
    request = D_8009BE6C;
    if (D_8009BD04 <= 0) {
        return;
    }
    for (i = 0; i < D_8009BD04; i++, request++) {
        top = position->vy;
        if (top < request->py) {
            bottom = top - (height << 12);
            top = request->py;
        } else {
            bottom = request->py - (request->z << 12);
        }
        if ((top - bottom) >> 12 < height + request->z) {
            delta.vx = (request->px - position->vx) >> 12;
            delta.vz = (request->pz - position->vz) >> 12;
            func_80093534(&delta);
            distance = SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz);
            reach = request->x + radius;
            if (distance < reach) {
                *hit = 2;
                *actor = request->actor;
                return;
            }
            if (distance < reach + 0x10) {
                *hit = 1;
                *actor = request->actor;
                return;
            }
        }
    }
}

/* Emit effect `effect` at the actor while it stands on terrain type 3,
 * otherwise stop the effect group. */
void func_8008C1DC(s32 effect, WorldmapActor *actor, ActorScratch *scratch) {
    if (func_80093F18(&actor->position) == 3) {
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->angle.vz = 0;
        scratch->angle.vx = 0;
        scratch->angle.vy = actor->unk5C;
        func_80089160(effect, &scratch->position, &scratch->angle);
        return;
    }
    func_800894C8(effect);
}

/* Create a party member's gear sprite for the actor. */
void func_8008C28C(WorldmapActor *actor, s32 member) {
    actor->handle = func_80024524(D_8009BDF8[member], D_8009B18C[member], D_8009B194[member],
                                  D_8009B19C[member], D_8009B1A4[member], 0x40);
    if ((&D_8006F8E5)[member] == 1) {
        func_800245D8(actor->handle, 0);
    } else {
        func_800245D8(actor->handle, 3);
    }
    func_80022000(actor->handle, 0x2000);
    ((s32 *)actor->handle)[15] &= ~4;
}
