#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E190);

/* Move scene object 0 to the vehicle actor and orient objects 0 and 1;
 * modes 4-5 show objects 1-3, modes 6-7 hide them. 3 once the vehicle kind
 * is cleared. */
s32 func_8008E4F4(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    s32 result;

    scratch = (ActorScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    D_8009C620[0].position = actor->position;
    result = 1;
    D_8009C620[0].visible = actor->unk24;
    if (!(D_8006EE54.flags & 0x1FFF)) {
        result = 3;
    }
    switch (D_8009BE10) {
    case 1:
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 2:
    case 3:
        actor->state = 1;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 4:
    case 5:
        D_8009C620[3].visible = 1;
        D_8009C620[2].visible = 1;
        D_8009C620[1].visible = 1;
        break;
    case 6:
    case 7:
        D_8009C620[3].visible = 0;
        D_8009C620[2].visible = 0;
        D_8009C620[1].visible = 0;
        break;
    }
    scratch->position.vx = -(actor->unk70 >> 12);
    scratch->position.vy = actor->heading;
    scratch->position.vz = D_8009BD38.vz;
    func_8004A92C(&scratch->position, &D_8009C620[0].matrix);
    func_8004A92C(&scratch->position, &D_8009C620[1].matrix);
    return result;
}

/* Start the flying vehicle at its saved position, height 0x80 and heading;
 * the camera and scene object 0 follow it. */
s32 func_8008E680(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    func_8008DFF4(&actor->position);
    actor->position.vy = 0x80000;
    actor->heading = D_8006EE54.vehicle_heading;
    actor->turn = 0x20;
    actor->unk74 = 3;
    actor->unk68 = -0x280000;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk24 = 0;
    actor->unk60 = 0;
    actor->unk64 = 0;
    actor->unk6C = 0;
    actor->state = 8;
    D_8009D55C.target = actor->position;
    D_8009D52C = actor->heading;
    D_8009C620[0].position = actor->position;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E76C);

/* Restore the actor from scene objects 2 and 3 after linking them to 0. */
s32 func_800906E0(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 2);
    func_800848B4(0, 3);
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position = D_8009C620[2].position;
    actor->motion = D_8009C620[3].position;
    actor->u.step = 0;
    actor->unk54 = 0x40;
    switch (D_8009BE10) {
    case 4:
    case 5:
    case 6:
    case 7:
        actor->state = 3;
        actor->unk5C = 0x80;
        actor->unk58 = 0x80;
        break;
    }
    return 1;
}

/* Link scene objects 2 and 3 to object 0. */
s32 func_800907C4(void) {
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_800907F4);

/* Track the two-button combination and latch its press edge. */
void func_80090A18(void) {
    if ((D_8009CD4C & 1) && (D_8009CD4C & 2)) {
        D_8009CEC0 = 1;
    } else {
        D_8009CEC0 = 0;
    }
    D_8009BD34 = (D_8009CEC0 ^ D_8009C7E8) & D_8009CEC0;
    D_8009C7E8 = D_8009CEC0;
}
