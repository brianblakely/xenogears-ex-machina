#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007C3B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007C724);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007C7D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CC6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CD20);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CE84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CF18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D078);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D110);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D228);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D2B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D414);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D4A4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D600);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D690);

/* Show scene object 16, reset its rotation and place the actor. */
#ifdef NON_MATCHING /* actor and constants swap registers */
s32 func_8007D774(s32 index) {
    WorldmapActor *actor;

    D_8009C620[16].visible = 1;
    D_8009C620[16].angle.vz = 0;
    D_8009C620[16].angle.vy = 0;
    D_8009C620[16].angle.vx = 0;
    func_8004A92C(&D_8009C620[16].angle, &D_8009C620[16].matrix);
    actor = &D_8009BE24[index];
    actor->position.vx = 0xD00000;
    actor->position.vz = 0x400000;
    actor->position.vy = -0x280000;
    actor->unk3C = 0;
    actor->unk38 = 0;
    actor->unk40 = -0x6000;
    return 3;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D774);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D7FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D918);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007DCE0);

/* Start the selected timed sequence on an actor. */
#ifdef NON_MATCHING /* register allocation of the sequence index and actor differ */
s32 func_8007DE14(s32 index) {
    WorldmapActor *actor;
    s32 sequence;

    sequence = D_8009D3D4;
    actor = &D_8009BE24[index];
    actor->unk54 = (s32)D_8009A65C[sequence].states;
    actor->u.step = 0;
    actor->unk58 = (s32)D_8009A65C[sequence].durations;
    actor->state = *(s16 *)actor->unk54;
    actor->wait = ((u16 *)actor->unk58)[actor->u.step++];
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007DE14);
#endif
