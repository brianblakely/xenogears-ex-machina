#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E190);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E4F4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E680);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E76C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_800906E0);

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
