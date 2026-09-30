/* Debug battle-scene selector (slot 2606, loaded at 801e0000).
 * The battle entry 80070f40 (battle overlay, at 80071050) loads this module
 * from directory 16 when the mode-2 byte 800594f8 is nonzero and calls
 * 801e0a34 before building the battle. A pad-driven screen edits rows of
 * decimal digits ("SceneNo " and the party slots) drawn with the resident
 * font; the chosen values feed the battle setup. Its load address is fixed by
 * the absolute jump table at 801e00e8, whose 15 targets are case labels of
 * 801e0238. */
#include "scene_select.h"

/* Put the first frame's environments, enable the display and reset the
 * selector: party order 0/1/2, default values and every digit visible. */
void func_801E0124(void) {
    s32 i;
    s32 j;

    func_80044C44(&D_800C4A20[0].draw);
    func_80044C44(&D_800C4A20[1].draw);
    func_80044E9C(&D_800C4A20[0].disp);
    func_80044E9C(&D_800C4A20[1].disp);
    func_80044534(1);
    D_8006F368[1] = 1;
    D_8006F368[2] = 2;
    D_801E1DA0[0][0] = 3;
    D_801E1DA0[1][1] = 1;
    D_8006F368[0] = 0;
    D_801E1DA0[1][0] = 0;
    D_801E1DA0[1][2] = 2;
    D_801E1DA0[2][2] = 0;
    D_801E1DA0[2][1] = 0;
    D_801E1DA0[2][0] = 0;
    D_801E1DA0[3][0] = 0;
    for (i = 0; i < 4; i++) {
        for (j = 2; j >= 0; j--) {
            D_801E1DD0[i][j] = 1;
        }
    }
    D_801E1DD0[3][2] = 0;
    D_801E1DD0[3][1] = 0;
    D_801E1DD0[0][2] = 0;
    D_801E1DD0[0][1] = 0;
}

INCLUDE_ASM(".local/decomp/ovl2606/asm/nonmatchings/ovl2606", func_801E0238);

INCLUDE_ASM(".local/decomp/ovl2606/asm/nonmatchings/ovl2606", func_801E0A34);
