#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80070CFC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800712D0);

/* Mode step that has nothing to do; always reports done. */
s32 func_80071A50(void) {
    return 1;
}

/* One world-map frame: input, actors, camera, terrain, sky and HUD. */
s32 func_80071A58(void) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */

    if (D_8009D144 == 0) {
        func_80097440(&D_8009BD40);
    } else {
        func_80097244(&D_8009BD40);
    }
    func_80089748();
    func_80089C78();
    func_80085CDC();
    func_8008615C();
    func_800747DC();
    func_800848F4();
    func_800980D4(&D_8009BBB4);
    if (D_8009D558 != 0) {
        func_800981C8(&D_8009BE28);
        func_80096130();
        func_80098CC0();
    }
    func_800983A0(&D_8009BE28);
    func_8009932C(D_8009BE3C->ot, D_8009BE3C->unk74, &D_8009BE28);
    D_8009C5BC += 0x40;
    func_80073B04();
    func_800737EC();
    func_80086798();
    if (D_8006EE54.unk76 == 0) {
        func_800740B8();
    }
    return 1;
}

/* Select the file set of an area (by index, or for the low indices by the
 * position against the threshold table) and derive its file numbers. */
#ifdef NON_MATCHING /* last three loads scheduled differently */
void func_80071B9C(s32 index, s32 position) {
    WorldmapArea *area;
    s32 i;

    if (index < 8) {
        for (i = 1; position >= D_8009B564[i]; i++) {
        }
        area = &D_8009B57C[i];
        D_8009C610 = i - 1;
    } else {
        area = &D_8009B57C[index + 2];
    }
    D_8009D3C4 = area->file + 1;
    D_8009C174 = area->file + 3;
    D_8009C17C = area->file + 2;
    D_8009D3D0 = area->file + 5;
    D_8009CC98 = area->file + 4;
    D_8009D800 = area->file + 7;
    D_8009D3C8 = area->file + 6;
    D_8009BCD8 = area->file + 9;
    D_8009BCC8 = area->file + 8;
    D_8009D160 = area->param2;
    D_8009D2B4 = area->param4;
    D_8009BD08 = area->file + 10;
    D_8009D7CC = area->param6;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80071B9C);
#endif

/* Allocate buffers for each party member's model and gear model, then read
 * them all with one list. */
#ifdef NON_MATCHING /* second loop's index/pointer registers differ */
void func_80071CDC(void) {
    s32 i;
    s32 j;
    s32 member;
    u8 gear;

    for (i = 0; i < 3; i++) {
        member = D_8006F368[i];
        if (member != 0xFF) {
            D_8009CD34[i] = func_80031BDC(func_800288EC(member + 2), 0);
            gear = D_8006D940[member].gear;
            if (gear != 0xFF) {
                D_8009BDF8[i] = func_80031BDC(func_800288EC(gear + 0x13), 0);
            } else {
                D_8009BDF8[i] = NULL;
            }
        } else {
            D_8009BDF8[i] = NULL;
            D_8009CD34[i] = NULL;
        }
    }
    i = 0;
    D_8009C170 = 0;
    for (j = 0; j < 3; j++) {
        member = D_8006F368[j];
        if (member != 0xFF) {
            D_8009D3F8[i].file = member + 2;
            D_8009D3F8[i].dest = D_8009CD34[j];
            i++;
            D_8009C170++;
            gear = D_8006D940[member].gear;
            if (gear != 0xFF) {
                D_8009D3F8[i].file = gear + 0x13;
                D_8009D3F8[i].dest = D_8009BDF8[j];
                i++;
            }
        }
    }
    D_8009D3F8[i].file = 0;
    D_8009D3F8[i].dest = NULL;
    func_80029AFC(D_8009D3F8, 0, 0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80071CDC);
#endif

/* Allocate and read the three area files into their resident buffers.
 * Differs in store scheduling of the read list. */
#ifdef NON_MATCHING
void func_80071EF0(void) {
    D_8009C59C = func_80031BDC(func_800288EC(D_8009C17C), 1);
    D_8009BD20 = func_80031BDC(func_800288EC(D_8009C174), 1);
    D_8009C180 = func_80031BDC(func_800288EC(D_8009D3C4), 1);
    D_8009D3F8[0].file = D_8009D3C4;
    D_8009D3F8[0].dest = D_8009C180;
    D_8009D3F8[1].file = D_8009C17C;
    D_8009D3F8[1].dest = D_8009C59C;
    D_8009D3F8[2].file = D_8009C174;
    D_8009D3F8[2].dest = D_8009BD20;
    D_8009D3F8[3].file = 0;
    D_8009D3F8[3].dest = NULL;
    func_80029AFC(D_8009D3F8, 0, 0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80071EF0);
#endif

/* Allocate and read the two shared world-map files (0x25, 0x26).
 * Differs in store scheduling of the read list. */
#ifdef NON_MATCHING
void func_80071FEC(void) {
    D_8005945C = func_80031BDC(func_800288EC(0x26), 1);
    D_8009D528 = func_80031BDC(func_800288EC(0x25), 1);
    D_8009D3F8[0].file = 0x25;
    D_8009D3F8[0].dest = D_8009D528;
    D_8009D3F8[1].file = 0x26;
    D_8009D3F8[1].dest = D_8005945C;
    D_8009D3F8[2].file = 0;
    D_8009D3F8[2].dest = NULL;
    func_80029AFC(D_8009D3F8, 0, 0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80071FEC);
#endif

/* Allocate and read the area's terrain, texture and object files. */
#ifdef NON_MATCHING /* read-list base address kept in $s0 across calls */
void func_80072090(void) {
    D_8004F304++;
    D_8009D3F8[0].file = D_8009CC98;
    D_8009D3F8[0].dest = D_8009C88C = func_80031BDC(func_800288EC(D_8009CC98), 1);
    D_8009D3F8[1].file = D_8009D3D0;
    D_8009D3F8[1].dest = D_8009C884 = func_80031BDC(func_800288EC(D_8009D3D0), 0);
    D_8009D3F8[2].file = D_8009D3C8;
    D_8009D3F8[2].dest = D_8006259C = func_80031BDC(func_800288EC(D_8009D3C8), 0);
    D_8009D3F8[3].file = D_8009D800;
    D_8009D3F8[3].dest = D_8009C888 = func_80031BDC(func_800288EC(D_8009D800), 0);
    D_8009D3F8[4].file = D_8009BCC8;
    D_8009D3F8[4].dest = D_8009C614 = func_80031BDC(func_800288EC(D_8009BCC8), 0);
    D_8009D3F8[5].file = 0;
    D_8009D3F8[5].dest = NULL;
    func_80029AFC(D_8009D3F8, 0, 0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80072090);
#endif

/* Allocate and read the area's sixth file (kept, mode 0). */
void func_800721E4(void) {
    D_8006259C = func_80031BDC(func_800288EC(D_8009D3C8), 0);
    func_800295D8(D_8009D3C8, D_8006259C, 0, 0);
}

INCLUDE_RODATA(".local/decomp/worldmap/asm/nonmatchings/worldmap", D_8006FAF0);
