#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007C3B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007C724);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007C7D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CC6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CD20);

/* Link scene object 6 to 5, hide 5 and reset its rotation; place the actor. */
#ifdef NON_MATCHING /* actor and constants swap registers */
s32 func_8007CE84(s32 index) {
    WorldmapActor *actor;

    func_800848B4(5, 6);
    D_8009C620[5].visible = 0;
    D_8009C620[5].angle.vz = 0;
    D_8009C620[5].angle.vy = 0;
    D_8009C620[5].angle.vx = 0;
    func_8004A92C(&D_8009C620[5].angle, &D_8009C620[5].matrix);
    actor = &D_8009BE24[index];
    actor->unk40 = -0x4000;
    actor->position.vx = 0xB00000;
    actor->state = 0;
    actor->unk3C = 0;
    actor->unk38 = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x200000;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CE84);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007CF18);

/* Link scene objects 7 and 8 to 9, hide 9 and reset its rotation; place the actor. */
#ifdef NON_MATCHING /* actor and constants swap registers */
s32 func_8007D078(s32 index) {
    WorldmapActor *actor;

    func_800848B4(9, 7);
    func_800848B4(9, 8);
    D_8009C620[9].visible = 0;
    D_8009C620[9].angle.vz = 0;
    D_8009C620[9].angle.vy = 0;
    D_8009C620[9].angle.vx = 0;
    func_8004A92C(&D_8009C620[9].angle, &D_8009C620[9].matrix);
    actor = &D_8009BE24[index];
    actor->unk40 = -0x4000;
    actor->unk3C = 0;
    actor->unk38 = 0;
    actor->position.vx = 0xC00000;
    actor->position.vy = 0;
    actor->position.vz = 0;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D078);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D110);

/* Link scene object 11 to 10, hide 10 and reset its rotation; place the actor. */
#ifdef NON_MATCHING /* actor and constants swap registers */
s32 func_8007D228(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0xA, 0xB);
    D_8009C620[10].visible = 0;
    D_8009C620[10].angle.vz = 0;
    D_8009C620[10].angle.vy = 0;
    D_8009C620[10].angle.vx = 0;
    func_8004A92C(&D_8009C620[10].angle, &D_8009C620[10].matrix);
    actor = &D_8009BE24[index];
    actor->unk40 = -0x4000;
    actor->position.vx = 0xE00000;
    actor->unk3C = 0;
    actor->unk38 = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x100000;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D228);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D2B8);

/* Link scene object 15 to 12, hide 12 and reset its rotation; place the actor. */
#ifdef NON_MATCHING /* actor and constants swap registers */
s32 func_8007D414(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0xC, 0xF);
    D_8009C620[12].visible = 0;
    D_8009C620[12].angle.vz = 0;
    D_8009C620[12].angle.vy = 0;
    D_8009C620[12].angle.vx = 0;
    func_8004A92C(&D_8009C620[12].angle, &D_8009C620[12].matrix);
    actor = &D_8009BE24[index];
    actor->unk40 = -0x4000;
    actor->position.vx = 0xE80000;
    actor->unk3C = 0;
    actor->unk38 = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x280000;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D414);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D4A4);

/* Link scene object 14 to 13, hide 13 and reset its rotation; place the actor. */
#ifdef NON_MATCHING /* actor and constants swap registers */
s32 func_8007D600(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0xD, 0xE);
    D_8009C620[13].visible = 0;
    D_8009C620[13].angle.vz = 0;
    D_8009C620[13].angle.vy = 0;
    D_8009C620[13].angle.vx = 0;
    func_8004A92C(&D_8009C620[13].angle, &D_8009C620[13].matrix);
    actor = &D_8009BE24[index];
    actor->unk40 = -0x4000;
    actor->position.vx = 0xF80000;
    actor->unk3C = 0;
    actor->unk38 = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x380000;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007C3B8", func_8007D600);
#endif

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
