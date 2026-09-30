#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80072238);

/* Leave the world map: stop audio, release actor handles, shut down each
 * subsystem and free the area buffers. */
void func_8007299C(void) {
    s32 i;

    if (D_8009D7CC == 0) {
        func_8003A89C(D_80062528, 0, 0xF0);
    }
    func_80039FF8();
    func_8003852C(D_8006259C);
    func_800320E8(D_8006259C);
    for (i = 0; i < 0x40; i++) {
        if (D_8009BE24[i].handle != 0) {
            func_800230A8(D_8009BE24[i].handle);
            D_8009BE24[i].handle = 0;
        }
    }
    if (D_8009D7CC == 1) {
        func_80075460();
        D_8006F954[0] |= 0x8000;
    }
    func_80092DD0();
    func_800931B0();
    func_80084818();
    func_80086124();
    func_80024FB8();
    func_80086568();
    func_800866C8();
    func_8007474C();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    func_800320E8(D_8009BC38);
    func_800320E8(D_8009BCB0);
    func_800320E8(D_8009BC3C);
    func_800320E8(D_8009BCB4);
    func_800320E8(D_8009C180);
    for (i = 0; i < 3; i++) {
        if (D_8009CD34[i] != NULL) {
            func_800320E8(D_8009CD34[i]);
        }
        if (D_8009BDF8[i] != NULL) {
            func_800320E8(D_8009BDF8[i]);
        }
    }
    func_800976A0();
    func_800960BC();
}

/* Set up the double-buffered 320x216 display and the background colour. */
void func_80072BB0(void) {
    func_80044110(1);
    D_8009BCDC = 0x100;
    func_8004A14C(0x100);
    func_80043928(&D_8009BBC8[0].draw, 0, 0, 0x140, 0xD8);
    func_80043928(&D_8009BBC8[1].draw, 0, 0xD8, 0x140, 0xD8);
    func_800439E0(&D_8009BBC8[0].disp, 0, 0xD8, 0x140, 0xD8);
    func_800439E0(&D_8009BBC8[1].disp, 0, 0, 0x140, 0xD8);
    D_8009BBC8[1].draw.isbg = 1;
    D_8009BBC8[0].draw.isbg = 1;
    D_8009BBC8[1].draw.dtd = 1;
    D_8009BBC8[0].draw.dtd = 1;
    if (D_8009D7CC == 2) {
        D_8009BBC8[0].draw.r0 = 0;
        D_8009BBC8[0].draw.g0 = 0;
        D_8009BBC8[0].draw.b0 = 0;
        D_8009BBC8[1].draw.r0 = 0;
        D_8009BBC8[1].draw.g0 = 0;
        D_8009BBC8[1].draw.b0 = 0;
    } else {
        D_8009BBC8[0].draw.r0 = 0;
        D_8009BBC8[0].draw.g0 = 0;
        D_8009BBC8[0].draw.b0 = 0x70;
        D_8009BBC8[1].draw.r0 = 0;
        D_8009BBC8[1].draw.g0 = 0;
        D_8009BBC8[1].draw.b0 = 0x70;
    }
    D_8009BBC8[1].disp.screen.y = 0xA;
    D_8009BBC8[0].disp.screen.y = 0xA;
    D_8009BBC8[1].disp.screen.w = 0x100;
    D_8009BBC8[0].disp.screen.w = 0x100;
    D_8009BBC8[1].disp.screen.x = 0;
    D_8009BBC8[0].disp.screen.x = 0;
    D_8009BBC8[1].disp.screen.h = 0xD8;
    D_8009BBC8[0].disp.screen.h = 0xD8;
    func_8002C6E0(0x80, 0x80, 0x80);
    func_8004A0EC(0x80, 0x80, 0x80);
    func_8004A10C(D_8009BB48[0], D_8009BB48[1], D_8009BB48[2]);
    func_80048AB0(D_8009D7CC == 2 ? 0xB00 : 0x800, 0xE80, D_8009BCDC);
}

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

/* Unpack the area file and resolve its section offsets to pointers. */
#ifdef NON_MATCHING /* area pointer reaches $a0 through an extra copy */
void func_80073530(void) {
    u8 *block;
    u8 *base;
    AreaHeader *area;
    s32 *table;
    s32 i;

    block = D_8009C180;
    D_8009C180 = func_80032E88(block, 0);
    func_800320E8(block);
    base = D_8009C180;
    area = (AreaHeader *)base;
    block = base + area->spots;
    D_8009CD48 = base + area->off8;
    D_8009D308 = base + area->offC;
    D_8009BD30 = base + area->off10;
    D_8009C7EC = base + area->off14;
    D_8009BCC0 = base + area->off18;
    D_8009D784 = base + area->off1C;
    D_8009D77C = base + area->off20;
    D_8009D7C8 = base + area->off24;
    for (i = 0; i < 16; i++) {
        D_8009D73C[i] = base + area->models[i];
    }
    D_8009D3F4 = (WorldmapSpot *)(block + ((SpotHeader *)block)->spots);
    D_8009BD00 = table = (s32 *)(block + ((SpotHeader *)block)->table);
    table[0] = (s32)block + table[0];
    D_8009BD00[1] = (s32)block + table[1];
    D_8009BD00[2] = (s32)block + table[2];
    D_8009BD00[3] = (s32)block + table[3];
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80073530);
#endif

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

/* Free the position ring and two work buffers. */
void func_8007474C(void) {
    func_800320E8(D_8009BE18);
    func_800320E8(D_8009BE14);
    func_800320E8(D_8009D30C);
}

/* Record a position (in world units) with an id in the 16-entry ring. */
void func_80074794(s16 id, Vec3 *position) {
    D_8009D30C[D_8009BE38].x = position->vx >> 12;
    D_8009D30C[D_8009BE38].z = position->vz >> 12;
    D_8009D30C[D_8009BE38].id = id;
    D_8009BE38 = (D_8009BE38 + 1) & 0xF;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800747DC);

/* Create the terrain texture animations from their area section. */
void func_80074E58(void) {
    TexAnim *anim;
    s32 i;
    s32 count;

    count = *D_8009D77C;
    D_8009CC9C = count;
    D_8009D780 = anim = func_80031BDC(count * sizeof(TexAnim), 0);
    for (i = 0; i < D_8009CC9C; i++, anim++) {
        anim->images = (u8 *)D_8009D77C + D_8009D77C[i + 1];
        anim->slot = &D_8009A1E8[i];
        anim->frame = 0;
        anim->timer = 1;
    }
}

/* Free the terrain texture animations. */
void func_80074F04(void) {
    func_800320E8(D_8009D780);
}

/* Advance the terrain texture animations, uploading each new image. */
void func_80074F2C(void) {
    TexAnim *anim;
    s32 i;

    anim = D_8009D780;
    for (i = 0; i < D_8009CC9C; i++, anim++) {
        if (--anim->timer == 0) {
            anim->frame++;
            anim->timer = anim->slot->frames[anim->frame].duration;
            if (anim->timer < 0) {
                anim->frame = 0;
                anim->timer = anim->slot->frames[0].duration;
            }
            func_80044894(&anim->slot->rect, anim->images + anim->slot->frames[anim->frame].image * 16);
        }
    }
}

/* Create the second set of texture animations from their area section. */
void func_80075030(void) {
    TexAnim *anim;
    s32 i;
    s32 count;

    count = *D_8009D7C8;
    D_8009CD64 = count;
    D_8009D7D0 = anim = func_80031BDC(count * sizeof(TexAnim), 0);
    for (i = 0; i < D_8009CD64; i++, anim++) {
        anim->images = (u8 *)D_8009D7C8 + D_8009D7C8[i + 1];
        anim->slot = &D_8009A250[i];
        anim->frame = 0;
        anim->timer = 1;
    }
}

/* Free the second set of texture animations. */
void func_800750DC(void) {
    func_800320E8(D_8009D7D0);
}

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
