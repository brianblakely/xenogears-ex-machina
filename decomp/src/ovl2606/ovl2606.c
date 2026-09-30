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
    D_8006F364.party[1] = 1;
    D_8006F364.party[2] = 2;
    D_801E1DA0[0][0] = 3;
    D_801E1DA0[1][1] = 1;
    D_8006F364.party[0] = 0;
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

/* Entry: run the selector, then set up the chosen battle: the party and
 * whether each member starts in a gear, the enemy set, the scene's formation
 * table and event data from disc, full HP/EP for every character and every
 * member joined. */
void func_801E0A34(void) {
    s32 i;
    s32 member;
    s32 size;
    void *data;

    func_801E0124();
    func_801E0238();
    member = 0;
    for (i = 0; i < 3; i++) {
        D_8006F364.inGear[i] = 0;
        D_8006F364.party[i] = 0xFF;
        if (D_801E1DA0[1][i] < 11) {
            D_8006F364.party[member] = D_801E1DA0[1][i];
            switch (D_801E1DA0[2][i]) {
            case 0:
                D_8006F364.inGear[member] = 0;
                break;
            case 2:
                D_8006D8A0[D_8006F364.party[member]].gear = D_801E1D90[D_8006F364.party[member]];
                D_8006F364.inGear[member] = 1;
                break;
            case 1:
                D_8006D8A0[D_8006F364.party[member]].gear = D_801E1D80[D_8006F364.party[member]];
                D_8006F364.inGear[member] = 1;
                break;
            }
            member++;
        }
    }
    func_80028470(0x20, 3);
    if (D_801E1DA0[3][0] < 0xFD) {
        i = D_801E1DA0[3][0] + 7;
    } else {
        i = D_801E1DA0[3][0] - 0xF9;
    }
    D_80059508 = D_801E1DA0[0][0];
    if (D_80059508 == 0xF && i == 7) {
        D_80059508 = 3;
        D_8005947C = 3;
    }
    size = func_800288EC(i);
    func_80032498(2, 0);
    data = func_8008ABB8(size, 1);
    func_800295D8(i, data, 0, 0x80);
    func_80028A60(0);
    func_8003F99C(D_800658DC, data, 0x200);
    func_800320E8(data);
    func_8001B66C();
    func_8008AB70();
    size = func_800288EC(4);
    D_800D39D8 = func_8008ABB8(size, 1);
    func_800295D8(4, D_800D39D8, 0, 0x80);
    func_80028A60(0);
    func_8003F99C(D_80062648, D_800D39D8, size);
    func_800320E8(D_800D39D8);
    func_8009B1E4();
    for (i = 0; i < 11; i++) {
        D_8006D8A0[i].hp = 999;
        D_8006D8A0[i].maxHp = 999;
        D_8006D8A0[i].ep = 99;
        D_8006D8A0[i].maxEp = 99;
    }
    D_8006F364.joined = 0xFFFF;
    func_8003748C();
    D_8005954C = D_80059508 & 3;
}
