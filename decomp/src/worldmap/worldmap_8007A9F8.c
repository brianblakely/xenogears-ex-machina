#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007A9F8);

/* Start a scripted camera looking at the player from yaw 0x480. */
#ifdef NON_MATCHING /* return value loaded before the stores */
s32 func_8007AD34(s32 index) {
    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x1C0000;
    D_8009BD38.vx = 0x40;
    D_8009BD38.vy = 0x480;
    D_8009BD38.vz = 0;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    D_8009BE24[index].u.step = 0x1000;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007AD34);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007ADD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007B200);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007B394);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007B604);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007B798);

/* Scene step with nothing to do. */
s32 func_8007BA08(void) {
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007BA10);

/* Link scene objects 1-3 to object 0 and reset its rotation. */
s32 func_8007BB60(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 1);
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 0x100;
    actor->unk58 = 0x100;
    D_8009C620[0].angle.vx = 0;
    D_8009C620[0].angle.vy = 0;
    D_8009C620[0].angle.vz = 0;
    func_8004A92C(&D_8009C620[0].angle, &D_8009C620[0].matrix);
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007BBEC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007BF50);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007C260);

/* Start an actor's timed sequence: first state and its duration. */
#ifdef NON_MATCHING /* state constant is loaded before the step reset */
s32 func_8007C36C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A4D8;
    actor->wait = D_8009A4E8[actor->u.step++];
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007C36C);
#endif
