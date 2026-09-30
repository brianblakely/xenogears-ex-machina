#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E190);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E4F4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E680);

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
