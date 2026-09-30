#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007DE98);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007E450);

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
