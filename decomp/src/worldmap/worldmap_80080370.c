#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080370);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080578);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080600);

/* Copy the player position into the actor. */
s32 func_80080900(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->unk28 = D_8009C5AC.vx;
    actor->unk2C = D_8009C5AC.vy;
    actor->unk30 = D_8009C5AC.vz;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080944);

/* Set the colour of `count` textured quads. */
void func_800809EC(PolyFT4 *quads, s32 count, s32 r, s32 g, s32 b) {
    s32 i;

    for (i = 0; i < count; i++, quads++) {
        setRGB0(quads, r, g, b);
    }
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080A28);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080AC4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80080D00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_8008106C);

/* Start an actor's timed sequence: first state and its duration. */
#ifdef NON_MATCHING /* state constant is loaded before the step reset */
s32 func_80081174(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A6C0;
    actor->wait = D_8009A70C[actor->u.step++];
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80080370", func_80081174);
#endif
