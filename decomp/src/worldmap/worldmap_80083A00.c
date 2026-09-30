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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80084068);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008440C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80084580);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80084818);

/* Link scene object `child` to `parent`. */
void func_800848B4(s32 parent, s32 child) {
    D_8009C620[child].parent = &D_8009C620[parent];
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800848F4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80084D00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80084DB8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085158);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085418);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085760);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085CDC);

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
        D_8009D478[i] = func_80043A58(0xF0, i + 0x1F0);
    }
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085FE0);

/* Free two work buffers. */
void func_80086124(void) {
    func_800320E8(D_8009D7EC);
    func_800320E8(D_8009D7E8);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008615C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800863E0);

/* Free two work buffers. */
void func_80086568(void) {
    func_800320E8(D_8009CEB4);
    func_800320E8(D_8009D150);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800865A0);

/* Free two work buffers. */
void func_800866C8(void) {
    func_800320E8(D_8009D7FC);
    func_800320E8(D_8009D7F8);
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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087734);

/* Reset an actor to step 0 with parameter 8. */
s32 func_800877E0(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087804);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087904);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087A8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087B84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80087C6C);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088570);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088720);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088B40);

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
#ifdef NON_MATCHING /* object pointer folded into the field offsets */
s32 func_80088D00(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = &D_8009C620[69];
    actor = &D_8009BE24[index];
    if (D_8006EF64 == 0x99) {
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088D00);
#endif

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008901C);

/* Free two effect buffers. */
void func_80089128(void) {
    func_800320E8(D_8009BE1C);
    func_800320E8(D_8009BE20);
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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008A2C8);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008A5B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008A72C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008B2BC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008B498);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008B54C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008B644);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008BB40);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008BD1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008BDD0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008BEC8);

/* Queue a placement of actor `index` at `position`, facing (x, z). */
void func_8008BFD4(s32 index, Vec3 *position, s32 x, s32 z) {
    PlaceRequest *request;

    request = &D_8009BE6C[D_8009BD04];
    request->actor = index;
    request->position.vx = position->vx;
    request->position.vy = position->vy;
    request->position.vz = position->vz;
    request->z = (s16)z;
    request->x = x;
    D_8009BD04 = (D_8009BD04 + 1) & 0x1F;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008C040);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008C1DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008C28C);
