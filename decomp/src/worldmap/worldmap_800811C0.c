#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800811C0);

/* Start a scripted camera at the player position. */
s32 func_800813E8(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk4 = 1;
    actor->state = 0;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081470);

/* Build `count` semi-transparent textured quads on page 0x180,0. */
void func_800816DC(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = func_80043A1C(0, abr, 0x180, 0);
        setSemiTrans(quads, 1);
        quads++;
    }
    func_8003F968(object->prims2, object->prims, count * sizeof(PolyFT4));
}

/* Start the actor above the player and build scene object 2 there. */
#ifdef NON_MATCHING /* scene-object pointer loaded at a different point */
s32 func_800817A0(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position.vx = D_8009C5AC.vx;
    objects = D_8009C620;
    actor->position.vy = D_8009C5AC.vy - 0x100000;
    actor->u.step = 0;
    actor->unk54 = 0;
    actor->unk58 = 0;
    actor->position.vz = D_8009C5AC.vz;
    func_800816DC(&objects[2], objects[2].prims, objects[2].def->count, 1);
    objects[2].position.vx = actor->position.vx >> 12;
    objects[2].position.vy = actor->position.vy >> 12;
    objects[2].position.vz = actor->position.vz >> 12;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800817A0);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081868);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800819C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081B24);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081C3C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081D80);

/* Reset an actor to state 0, step 1. */
s32 func_80081FB4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->u.step = 1;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081FD8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80082324);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800826B4);

/* Give an actor its script. */
s32 func_800827C8(s32 index) {
    D_8009BE24[index].u.script = D_8009A758;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800827EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800828DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80082F64);

/* Build `count` semi-transparent black textured triangles on page 0x2C0,0x100. */
void func_80083108(SceneObject *object, PolyFT3 *prims, s32 count, s32 abr) {
    s32 i;

    for (i = count - 1; i != -1; i--) {
        ((u8 *)prims)[3] = 7;
        prims->code = 0x24;
        prims->tpage = func_80043A1C(0, abr, 0x2C0, 0x100);
        prims->r0 = 0;
        prims->g0 = 0;
        prims->b0 = 0;
        prims->code |= 2;
        prims++;
    }
    func_8003F968(object->prims2, object->prims, count * sizeof(PolyFT3));
}

/* Set the colour of `count` textured triangles. */
void func_800831D8(PolyFT3 *prims, s32 count, s32 r, s32 g, s32 b) {
    for (count--; count != -1; count--) {
        setRGB0(prims, r, g, b);
        prims++;
    }
}

/* Rebuild the triangles of scene effect `index`. */
s32 func_80083214(s32 index) {
    SceneObject *object;

    object = &D_8009C620[78 + index];
    func_80083108(object, object->prims, object->def->count, 3);
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80083264);

/* Mode step that has nothing to do; always reports done. */
s32 func_800834D0(void) {
    return 1;
}

/* On command, reload the terrain around the player and drain the frames. */
s32 func_800834D8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        func_800445D0(0);
        func_8004B54C(0);
        func_80097D64();
        func_80097BC0(&D_8009C5AC);
        do {
            func_800967E4();
            func_8004B54C(0);
        } while (func_80096668() > 0);
    }
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_8008355C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800837DC);

/* Give an actor its script. */
s32 func_800838E8(s32 index) {
    D_8009BE24[index].u.script = D_8009AC60;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_8008390C);
