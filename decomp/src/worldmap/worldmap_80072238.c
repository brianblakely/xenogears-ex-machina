#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80072238);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007299C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80072BB0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80072DB4);

/* Choose the movement mode from the saved state: a vehicle kind, or on foot
 * (1 when no party flag is set, else 2). */
void func_80073300(void) {
    if (D_8006EE54.flags & 0x4000) {
        switch (D_8006EE54.flags & 0x1FFF) {
        case 0:
            break;
        case 1:
            D_8009BE10 = 4;
            break;
        case 2:
            D_8009BE10 = 5;
            break;
        case 3:
            D_8009BE10 = 7;
            break;
        case 4:
            D_8009BE10 = 7;
            break;
        }
    } else if ((D_8006F8E5 | D_8006F8E6 | D_8006F8E7) == 0) {
        D_8009BE10 = 1;
    } else {
        D_8009BE10 = 2;
    }
}

/* Restore the player position and heading for the current movement mode. */
void func_80073398(void) {
    D_8006EE54.unk6A = 0;
    switch (D_8009BE10) {
    case 1:
    case 2:
        D_8009C5AC.vx = D_8006EE54.x << 12;
        D_8009C5AC.vz = D_8006EE54.z << 12;
        D_8009C584 = D_8006EE54.heading;
        break;
    case 4:
    case 5:
    case 7:
        func_8008DFF4(&D_8009C5AC);
        D_8009C584 = D_8006EE54.vehicle_heading;
        break;
    }
}

/* Place the player at the arrival point with the given id (or restore a
 * saved vehicle position); unknown ids place it at the origin. */
void func_80073448(s32 id) {
    s32 unused; /* unreferenced; the original frame reserves it */
    WorldmapSpot *spot;

    if (D_8006EE54.flags & 0x2000) {
        D_8006EE54.flags &= ~0x2000;
        func_8008DFF4(&D_8009C5AC);
        D_8009C584 = D_8006EE54.vehicle_heading;
        return;
    }
    for (spot = D_8009D3F4; spot->id != -1; spot++) {
        if (spot->id == id) {
            D_8009C5AC.vx = spot->x << 12;
            D_8009C5AC.vy = 0;
            D_8009C5AC.vz = spot->z << 12;
            return;
        }
    }
    D_8009C5AC.vx = 0;
    D_8009C5AC.vy = 0;
    D_8009C5AC.vz = 0;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80073530);

/* Allocate the two 4 KiB work buffers. */
void func_8007369C(void) {
    D_8009BC38 = func_80031BDC(0x1000, 0);
    D_8009BCB0 = func_80031BDC(0x1000, 0);
}

/* Initialise the sky gradient: four bands of Gouraud quads in both buffers. */
#ifdef NON_MATCHING /* store order and base register differ */
void func_800736DC(void) {
    D_8009D194[0][0].rgb0 = D_8009D194[0][0].rgb1 = D_8009D194[0][1].rgb0 = D_8009D194[0][1].rgb1 = 0xFF7A70;
    D_8009D194[0][0].rgb2 = D_8009D194[0][0].rgb3 = D_8009D194[0][1].rgb2 = D_8009D194[0][1].rgb3 = 0xFFF5E0;
    setPolyG4(&D_8009D194[0][0]);
    setPolyG4(&D_8009D194[0][1]);
    D_8009D194[1][0].rgb0 = D_8009D194[1][0].rgb1 = D_8009D194[1][1].rgb0 = D_8009D194[1][1].rgb1 = 0xC03745;
    D_8009D194[1][0].rgb2 = D_8009D194[1][0].rgb3 = D_8009D194[1][1].rgb2 = D_8009D194[1][1].rgb3 = 0xFF7A70;
    setPolyG4(&D_8009D194[1][0]);
    setPolyG4(&D_8009D194[1][1]);
    D_8009D194[2][0].rgb0 = D_8009D194[2][0].rgb1 = D_8009D194[2][0].rgb2 = D_8009D194[2][0].rgb3 =
        D_8009D194[2][1].rgb0 = D_8009D194[2][1].rgb1 = D_8009D194[2][1].rgb2 = D_8009D194[2][1].rgb3 = 0xC03745;
    setPolyG4(&D_8009D194[2][0]);
    setPolyG4(&D_8009D194[2][1]);
    D_8009D194[3][0].rgb0 = D_8009D194[3][0].rgb1 = D_8009D194[3][0].rgb2 = D_8009D194[3][0].rgb3 =
        D_8009D194[3][1].rgb0 = D_8009D194[3][1].rgb1 = D_8009D194[3][1].rgb2 = D_8009D194[3][1].rgb3 = 0xC03745;
    setPolyG4(&D_8009D194[3][0]);
    setPolyG4(&D_8009D194[3][1]);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800736DC);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800737EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800739B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80073B04);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80073E30);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800740B8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80074594);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007474C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80074794);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800747DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80074E58);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80074F04);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80074F2C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075030);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800750DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075104);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075228);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007528C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075460);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007565C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800758C0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075B58);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075D4C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075E7C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076098);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800762FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007634C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076594);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800767D4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076858);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076954);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076A14);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076A1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076B34);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076BC4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076BDC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076C18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076C3C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076C68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076C88);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076CB4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076CD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076CF4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076D1C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076D50);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076D8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076DA4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076F54);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076FA8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800771D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077214);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077480);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007756C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800776E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077954);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007795C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077A64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077CC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077DC8);
