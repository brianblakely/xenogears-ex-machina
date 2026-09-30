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
    func_8009932C(D_8009BE3C->unk70, D_8009BE3C->unk74, &D_8009BE28);
    D_8009C5BC += 0x40;
    func_80073B04();
    func_800737EC();
    func_80086798();
    if (D_8006EE76 == 0) {
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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80072238);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007299C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80072BB0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80072DB4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80073300);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80073398);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80073448);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80073530);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007369C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800736DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800737EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800739B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80073B04);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80073E30);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800740B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80074594);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007474C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80074794);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800747DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80074E58);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80074F04);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80074F2C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075030);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800750DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075104);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075228);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007528C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075460);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007565C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800758C0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075B58);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075D4C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80075E7C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076098);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800762FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007634C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076594);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800767D4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076858);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076954);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076A14);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076A1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076B34);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076BC4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076BDC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076C18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076C3C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076C68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076C88);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076CB4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076CD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076CF4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076D1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076D50);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076D8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076DA4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076F54);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80076FA8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800771D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077214);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077480);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007756C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800776E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077954);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007795C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077A64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077CC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077DC8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80077E68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007828C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800783E8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80078948);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80078950);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80078A60);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80078D24);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80078E2C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80078EA4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800794D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80079538);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800795E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80079778);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A06C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A144);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A1B4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A410);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A430);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A568);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A570);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A5DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A8AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A9B4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007A9F8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007AD34);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007ADD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007B200);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007B394);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007B604);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007B798);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007BA08);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007BA10);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007BB60);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007BBEC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007BF50);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007C260);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007C36C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007C3B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007C724);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007C7D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007CC6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007CD20);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007CE84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007CF18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D078);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D110);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D228);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D2B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D414);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D4A4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D600);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D690);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D774);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D7FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007D918);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007DCE0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007DE14);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007DE98);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007E450);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007E4E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007EBBC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007ECA4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007EE34);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007F8AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007F968);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007FC8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007FD30);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8007FF70);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080218);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008032C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080370);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080578);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080600);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080900);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080944);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800809EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080A28);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080AC4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80080D00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008106C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081174);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800811C0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800813E8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081470);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800816DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800817A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081868);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800819C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081B24);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081C3C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081D80);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081FB4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80081FD8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80082324);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800826B4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800827C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800827EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800828DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80082F64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80083108);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800831D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80083214);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80083264);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800834D0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800834D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008355C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800837DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800838E8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008390C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80083A00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80083FE4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80084068);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008440C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80084580);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80084818);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800848B4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800848F4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80084D00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80084DB8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80085158);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80085418);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80085760);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80085CDC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80085F58);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80085FE0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80086124);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008615C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800863E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80086568);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800865A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800866C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80086700);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80086798);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087710);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087734);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800877E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087804);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087904);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800879A8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800879E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087A8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087B84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087C6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087F60);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80087FD0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088570);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008868C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088720);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088B40);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088C90);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088D00);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088D64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088DE4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088E1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088E68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088EA0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088F1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088F54);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088F5C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088F64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80088FF4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008901C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80089128);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80089160);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800893E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800894C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80089514);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80089580);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80089748);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80089C78);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008A2C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008A52C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008A5B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008A72C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008B2BC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008B498);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008B54C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008B644);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008BB40);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008BD1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008BDD0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008BEC8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008BFD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C040);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C1DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C28C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C364);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C530);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C6EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C75C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008C844);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008D3F0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008D520);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008D590);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008D678);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008DD6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008DE9C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008DF0C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008DFF4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E034);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E078);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E0F0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E190);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E4F4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E680);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8008E76C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800906E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800907C4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800907F4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80090A18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80090A84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80090C68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80090E14);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80090FB4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80091430);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800914D0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80091B54);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80091C18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80091FF8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80092234);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800922AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800923A8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800925A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80092BE4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80092C70);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80092DD0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80092DF8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80092FD8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800931B0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800931D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093354);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800933EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093484);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093534);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800935DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093660);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093740);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093978);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093A5C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093E8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093F18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80093FE4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094004);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094028);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094060);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094088);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094154);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800941C4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094238);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094364);

void func_80094434(void) {
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8009443C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800945C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094750);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800948D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80094A5C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800951A8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800952B0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80095324);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80095414);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80095CD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80095F78);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800960BC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096130);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8009623C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800962B0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096328);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800963E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800964B0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800965A4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096668);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096694);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800966CC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800967E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800968E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8009699C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096A6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096C0C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80096F18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097070);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097244);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097440);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8009766C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800976A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800976C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800976FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097718);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097770);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800977A8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800977C4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800977E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097800);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800978FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800979C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097BC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097CB8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097D64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80097DC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80098044);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800980D4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800981C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800983A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_800987AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80098CC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8009932C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80099708);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_8009980C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80099BFC);
