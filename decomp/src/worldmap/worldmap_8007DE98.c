#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007DE98);

/* Start a scripted camera at the player position (step 0, speed 0x40). */
s32 func_8007E450(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk4 = 1;
    actor->unk58 = 0x40;
    actor->state = 0;
    actor->u.step = 0;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007E4E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007EBBC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007ECA4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007EE34);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007F8AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007F968);

/* Rebuild scene objects 9 and 10 and start a descent at a fixed point. */
#ifdef NON_MATCHING /* return value loaded before the stores */
s32 func_8007FC8C(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    func_8007A06C(&objects[9], objects[9].prims, objects[9].def->count);
    func_8007A06C(&objects[10], objects[10].prims, objects[10].def->count);
    actor->position.vx = 0x1498000;
    actor->position.vy = -0x80000;
    actor->position.vz = 0x4AF2000;
    actor->state = 0;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    return 3;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FC8C);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FD30);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FF70);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_80080218);

/* Restart an actor's timed sequence at its first step. */
s32 func_8008032C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A698;
    actor->wait = D_8009A6AC[actor->u.step];
    return 1;
}
