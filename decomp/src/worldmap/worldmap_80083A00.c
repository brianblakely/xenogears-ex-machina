#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80083A00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80083FE4);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80085F58);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80086700);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008868C);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088D64);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088EA0);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80088F64);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_800894C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089514);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089580);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089748);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_80089C78);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008A2C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008A52C);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008BFD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008C040);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008C1DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80083A00", func_8008C28C);
