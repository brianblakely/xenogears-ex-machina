#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800811C0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800813E8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081470);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800816DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800817A0);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80083108);

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
