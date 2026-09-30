#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007DE98);

/* Start a scripted camera at the player position (step 0, speed 0x40). */
s32 func_8007E450(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FC8C);

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
