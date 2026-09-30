#include "worldmap.h"

/* Actor handlers this unit installs by address (see func_80097718). */
s32 func_800923A8();
s32 func_800925A0();
s32 func_80077DC8();
s32 func_80077E68();
s32 func_8007828C();
s32 func_800783E8();
s32 func_80078948();
s32 func_80078950();
s32 func_8007756C();
s32 func_800776E0();
s32 func_80087710();
s32 func_80087734();
s32 func_80071A50();
s32 func_80071A58();
s32 func_8008A52C();
s32 func_8008B498();
s32 func_8008BD1C();
s32 func_8008C6EC();
s32 func_8008D520();
s32 func_8008DE9C();
s32 func_8008E4F4();
s32 func_800907C4();
s32 func_80092BE4();
s32 func_80092DF8();
s32 func_80087F60();
s32 func_8008868C();
s32 func_800879E0();
s32 func_80088C90();

/* Enter the world map: set up the display, load or restore the area, start the
 * subsystems, the music and the area's actors. */
#ifdef NON_MATCHING /* flag stores scheduled early; music buffer address kept in $s0 */
void func_80072238(void) {
    RECT rect;
    ActorSpawn *spawn;
    void *seq;
    void *data;
    s32 file;
    s32 i;

    func_80072BB0();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 2);
    func_80028A60(0);
    func_80071EF0();
    while (func_800286CC() >= 3) {
    }
    func_80073530();
    func_8009766C();
    D_8009BE4C = D_8009A180;
    D_8009CCA4 = 2;
    D_8009D3CC = 4;
    D_8009D804 = 0;
    D_8009CEC0 = 0;
    D_8009C7E8 = 0;
    D_8009BD34 = 0;
    D_8009D144 = 0;
    D_8009C178 = D_80059198 = 1;
    D_8009CD40 = func_80086700;
    func_80098044();
    if (D_8009C894 == 0) {
        func_8001B66C();
    } else {
        func_80039CC4();
        func_800399D4(D_80062528);
        seq = D_8004F2FC;
        D_8004F2FC = NULL;
        D_80062528 = seq;
    }
    if ((u16)D_8006EE54.unk6A != 0) {
        func_80073398();
    } else if (D_8009C894 == 0) {
        func_80073448(D_8009D3D4);
    } else {
        func_8007565C();
        func_80075D4C();
    }
    func_80028A60(0);
    func_8008440C();
    func_800979C8();
    func_80084580();
    func_80072090();
    func_800736DC();
    func_80073E30();
    func_80085F58();
    func_80024F64(0x1400, 0);
    func_80074594();
    func_800863E0();
    func_80074E58();
    func_80075030();
    func_800739B8();
    func_80088F64();
    func_80028A60(0);
    if (D_8009C894 == 0) {
        D_8006258C = func_80037FD8(D_8009C88C, 0);
    }
    func_80028470(0x24, 0);
    if (D_8009C894 == 0) {
        func_80097BC0(&D_8009C5AC);
        do {
            func_800967E4();
            VSync(0);
        } while (func_80096668() >= 2);
    } else {
        func_80097CB8(&D_8009BE28);
        func_80096694();
    }
    if (D_8009C894 == 0) {
        while (D_8005957C & 0x10) {
        }
        func_800320E8(D_8009C88C);
        func_80038428(D_8006259C);
        if (D_8009BE10 == 7) {
            file = D_8009D800;
            data = D_8009C888;
        } else {
            file = D_8009D3D0;
            data = D_8009C884;
        }
        memcpy(D_80062648, data, func_800288EC(file));
        seq = func_80039850(D_80062648);
        D_80062528 = seq;
        func_80039A80(D_80062528, 0x7F, 0);
    } else {
        func_800320E8(D_8009C88C);
        func_80038428(D_8006259C);
        if (D_8009BE10 == 7) {
            file = D_8009D800;
            data = D_8009C888;
        } else {
            file = D_8009D3D0;
            data = D_8009C884;
        }
        memcpy(D_80062648, data, func_800288EC(file));
        func_80039B68(D_80062528, 0x7F, 0xF0);
    }
    if (D_8009C894 == 0) {
        if (D_80099E8C[0].kind != 0) {
            i = 0;
            do {
                func_80097718(D_80099E8C[i].kind, D_80099E8C[i].update);
                i++;
            } while (D_80099E8C[i].kind != 0);
        }
        for (spawn = D_8009A034[D_8009C610]; spawn->kind != 0; spawn++) {
            func_80097718(spawn->kind, spawn->update);
        }
    } else if (D_8009C894 == 1) {
        func_800976FC((s32)func_800923A8, 0);
        func_800976FC((s32)func_8008A52C, 1);
        func_800976FC((s32)func_8008B498, 2);
        func_800976FC((s32)func_8008BD1C, 3);
        func_800976FC((s32)func_8008C6EC, 4);
        func_800976FC((s32)func_8008D520, 5);
        func_800976FC((s32)func_8008DE9C, 6);
        func_800976FC((s32)func_8008E4F4, 7);
        func_800976FC((s32)func_800907C4, 8);
        func_800976FC((s32)func_80092BE4, 0xC);
        func_800976FC((s32)func_80092DF8, 0xD);
        func_800976FC((s32)func_80071A50, 0xE);
        switch (D_8009C610) {
        case 3:
            func_800976FC((s32)func_80087F60, 0xF);
            func_800976FC((s32)func_8008868C, 0x10);
            break;
        case 4:
            func_800976FC((s32)func_80087F60, 0xF);
            func_800976FC((s32)func_8008868C, 0x10);
            func_800976FC((s32)func_800879E0, 0x11);
            func_800976FC((s32)func_80088C90, 0x13);
            break;
        case 5:
        case 6:
        case 7:
            func_800976FC((s32)func_80087F60, 0xF);
            func_800976FC((s32)func_8008868C, 0x10);
        case 8:
            func_800976FC((s32)func_800879E0, 0x11);
            break;
        }
    }
    D_80059179 = 0;
    if (D_8009C610 == 0) {
        func_80089160(0xE, NULL, NULL);
        D_80059179 = 1;
    }
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80085FE0();
    if (D_8009C894 == 0) {
        func_80075228();
    }
    func_80033698(0x130, 0x1E0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80072238);
#endif

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
    func_800320E8(D_8009BC38[0]);
    func_800320E8(D_8009BCB0[0]);
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
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
    ResetGraph(1);
    D_8009BCDC = 0x100;
    SetGeomScreen(0x100);
    SetDefDrawEnv(&D_8009BBC8[0].draw, 0, 0, 0x140, 0xD8);
    SetDefDrawEnv(&D_8009BBC8[1].draw, 0, 0xD8, 0x140, 0xD8);
    SetDefDispEnv(&D_8009BBC8[0].disp, 0, 0xD8, 0x140, 0xD8);
    SetDefDispEnv(&D_8009BBC8[1].disp, 0, 0, 0x140, 0xD8);
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
    SetBackColor(0x80, 0x80, 0x80);
    func_8004A10C(D_8009BB48[0], D_8009BB48[1], D_8009BB48[2]);
    SetFogNearFar(D_8009D7CC == 2 ? 0xB00 : 0x800, 0xE80, D_8009BCDC);
}

/* Fade the saved screen (VRAM x 0x2C0) for `frames` frames: redraw it as three
 * textured quads under a translucent black quad whose level starts at `level`
 * and changes by `step`, blended with mode `abr`. */
#ifdef NON_MATCHING /* register allocation: original spills the step and three link words */
void func_80072DB4(s32 frames, s32 level, s32 step, s32 abr) {
    PolyFT4 *quads;
    PolyG4v *shades;
    DR_TPAGE *mode;
    DisplayBuffer *buffer;
    PolyG4v *shade;
    s32 side;
    s32 i;

    quads = func_80031BDC(3 * sizeof(PolyFT4), 1);
    shades = func_80031BDC(2 * sizeof(PolyG4v), 1);
    mode = func_80031BDC(sizeof(DR_TPAGE), 1);
    for (i = 0; i < 3; i++) {
        setPolyFT4(&quads[i]);
        setRGB0(&quads[i], 0x80, 0x80, 0x80);
        setShadeTex(&quads[i], 1);
    }
    quads[1].x1 = 0x100;
    quads[1].x3 = 0x100;
    quads[2].x0 = 0x100;
    quads[2].x2 = 0x100;
    quads[0].x1 = 0x80;
    quads[0].x3 = 0x80;
    quads[1].x0 = 0x80;
    quads[1].x2 = 0x80;
    quads[0].x0 = 0;
    quads[0].y0 = 0;
    quads[0].y1 = 0;
    quads[0].x2 = 0;
    quads[0].y2 = 0xEF;
    quads[0].y3 = 0xEF;
    quads[1].y0 = 0;
    quads[1].y1 = 0;
    quads[1].y2 = 0xEF;
    quads[1].y3 = 0xEF;
    quads[2].y0 = 0;
    quads[2].x1 = 0x140;
    quads[2].y1 = 0;
    quads[2].y2 = 0xEF;
    quads[2].x3 = 0x140;
    quads[2].y3 = 0xEF;
    quads[0].u0 = 0;
    quads[0].v0 = 0;
    quads[0].u1 = 0x80;
    quads[0].v1 = 0;
    quads[0].u2 = 0;
    quads[0].v2 = 0xEF;
    quads[0].u3 = 0x80;
    quads[0].v3 = 0xEF;
    quads[1].u0 = 0;
    quads[1].v0 = 0;
    quads[1].u1 = 0x80;
    quads[1].u3 = 0x80;
    quads[1].v1 = 0;
    quads[1].u2 = 0;
    quads[1].v2 = 0xEF;
    quads[1].v3 = 0xEF;
    quads[2].u0 = 0;
    quads[2].v0 = 0;
    quads[2].u1 = 0x40;
    quads[2].v1 = 0;
    quads[2].u2 = 0;
    quads[2].v2 = 0xEF;
    quads[2].u3 = 0x40;
    quads[2].v3 = 0xEF;
    quads[0].tpage = GetTPage(2, 0, 0x2C0, 0x100);
    quads[1].tpage = GetTPage(2, 0, 0x340, 0x100);
    quads[2].tpage = GetTPage(2, 0, 0x3C0, 0x100);
    SetDrawTPage(mode, 0, 1, GetTPage(0, abr, 0, 0));
    setPolyG4(&shades[0]);
    shades[0].x0 = 0;
    shades[0].y0 = 0;
    shades[0].x1 = 0x140;
    shades[0].y1 = 0;
    shades[0].x2 = 0;
    shades[0].y2 = 0xF0;
    shades[0].x3 = 0x140;
    shades[0].y3 = 0xF0;
    setRGB0(&shades[0], 0, 0, 0);
    shades[0].r1 = 0;
    shades[0].g1 = 0;
    shades[0].b1 = 0;
    shades[0].r2 = 0;
    shades[0].g2 = 0;
    shades[0].b2 = 0;
    shades[0].r3 = 0;
    shades[0].g3 = 0;
    shades[0].b3 = 0;
    SetSemiTrans(&shades[0], 1);
    setShadeTex(&shades[0], 1);
    shades[1] = shades[0];
    DrawSync(0);
    VSync(0);
    PutDispEnv(&D_8009BBC8[1].disp);
    PutDrawEnv(&D_8009BBC8[1].draw);
    buffer = &D_8009BBC8[0];
    side = 0;
    while (frames--) {
        buffer = (buffer == D_8009BBC8) ? buffer + 1 : D_8009BBC8;
        ClearOTagR((u32 *)buffer->unk70, 0x400);
        addPrim((u32 *)buffer->unk70 + 1, &quads[0]);
        addPrim((u32 *)buffer->unk70 + 1, &quads[1]);
        addPrim((u32 *)buffer->unk70 + 1, &quads[2]);
        side ^= 1;
        shade = &shades[side];
        setRGB0(shade, level, level, level);
        shade->r1 = level;
        shade->g1 = level;
        shade->b1 = level;
        shade->r2 = level;
        shade->g2 = level;
        shade->b2 = level;
        shade->r3 = level;
        shade->g3 = level;
        shade->b3 = level;
        addPrim((u32 *)buffer->unk70, shade);
        addPrim((u32 *)buffer->unk70, mode);
        DrawSync(0);
        VSync(0);
        PutDispEnv(&buffer->disp);
        PutDrawEnv(&buffer->draw);
        level += step;
        DrawOTag((u32 *)buffer->unk70 + 0x3FF);
    }
    DrawSync(0);
    VSync(0);
    PutDispEnv(&D_8009BBC8[1].disp);
    func_800320E8(quads);
    func_800320E8(shades);
    func_800320E8(mode);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80072DB4);
#endif

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
    D_8009C7EC = (TerrainTexture *)(base + area->off14);
    D_8009BCC0 = (AreaObject *)(base + area->off18);
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
    D_8009BC38[0] = func_80031BDC(0x1000, 0);
    D_8009BCB0[0] = func_80031BDC(0x1000, 0);
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

/* Transform the four sky bands with the camera yaw and link them into the
 * ordering table. */
#ifdef NON_MATCHING /* register allocation and scheduling differ */
void func_800737EC(void) {
    SVECTOR *corners;
    PolyG4 *band;
    s32 otz;
    s32 i;
    s32 offset;

    corners = D_8009A280[0];
    SKY_SCRATCH->angle.vz = 0;
    SKY_SCRATCH->angle.vx = 0;
    SKY_SCRATCH->angle.vy = D_8009BD38.vy;
    offset = 0;
    func_8004A92C(&SKY_SCRATCH->angle, &SKY_SCRATCH->rotation);
    SKY_SCRATCH->rotation.t[2] = 0;
    SKY_SCRATCH->rotation.t[1] = 0;
    SKY_SCRATCH->rotation.t[0] = 0;
    CompMatrix(&D_8009C808, &SKY_SCRATCH->rotation, &SKY_SCRATCH->view);
    SetRotMatrix(&SKY_SCRATCH->view);
    SetTransMatrix(&SKY_SCRATCH->view);
    for (i = 0; i < 4; i++) {
        band = (PolyG4 *)((u8 *)&D_8009D194[0][D_8009D7F0] + offset);
        otz = RotTransPers4(&corners[0], &corners[1], &corners[2], &corners[3], &band->xy0, &band->xy1,
                            &band->xy2, &band->xy3, &SKY_SCRATCH->p, &SKY_SCRATCH->flag);
        if (SKY_SCRATCH->flag >= 0) {
            addPrim(&D_8009BE3C->ot[otz >> D_80050100], band);
        }
        offset += sizeof(D_8009D194[0]);
        corners += 4;
    }
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800737EC);
#endif

/* Initialise the four textured horizon quads and the two texture windows. */
void func_800739B8(void) {
    RECT window;
    PolyFT4 *quad;
    u16 tpage;
    u16 clut;
    s32 i;

    tpage = GetTPage(0, 1, 0x380, 0x100);
    clut = GetClut(0x110, 0x1FE);
    quad = D_8009C744;
    for (i = 0; i < 4; i++, quad++) {
        ((u8 *)quad)[3] = 9;
        quad->code = 0x2C;
        quad->r0 = 0x30;
        quad->g0 = 0x30;
        quad->b0 = 0x30;
        quad->u0 = 0;
        quad->v0 = 0;
        quad->u1 = 0xFF;
        quad->v1 = 0;
        quad->u2 = 0;
        quad->v2 = 0x3F;
        quad->u3 = 0xFF;
        quad->v3 = 0x3F;
        quad->tpage = tpage;
        quad->clut = clut;
        SetSemiTrans(quad, 1);
    }
    window.x = 0;
    window.y = 0;
    window.w = 0x80;
    window.h = 0;
    SetTexWindow(&D_8009D3D8[0], &window);
    window.x = 0;
    window.y = 0;
    window.w = 0;
    window.h = 0;
    SetTexWindow(&D_8009D3D8[1], &window);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80073B04);

/* Initialise the overlay picture quad (both buffers), its texture page, eight
 * red Gouraud triangles and 64 small tiles. */
#ifdef NON_MATCHING /* x3 store addressed from the copy base register */
void func_80073E30(void) {
    PolyG3 *triangle;
    Tile *tile;
    s32 i;

    setPrimLen(&D_8009C5C0[0], 9);
    D_8009C5C0[0].code = 0x2C;
    D_8009C5C0[0].x0 = 0xD0;
    D_8009C5C0[0].y0 = 0x78;
    D_8009C5C0[0].x1 = 0x137;
    D_8009C5C0[0].y1 = 0x78;
    D_8009C5C0[0].x2 = 0xD0;
    D_8009C5C0[0].y2 = 0xD7;
    D_8009C5C0[0].x3 = 0x137;
    D_8009C5C0[0].y3 = 0xD7;
    D_8009C5C0[0].u0 = 0;
    D_8009C5C0[0].v0 = 0x80;
    D_8009C5C0[0].u1 = 0x7F;
    D_8009C5C0[0].v1 = 0x80;
    D_8009C5C0[0].u2 = 0;
    D_8009C5C0[0].v2 = 0xFF;
    D_8009C5C0[0].u3 = 0x7F;
    D_8009C5C0[0].v3 = 0xFF;
    D_8009C5C0[0].r0 = 0x80;
    D_8009C5C0[0].g0 = 0x80;
    D_8009C5C0[0].b0 = 0x80;
    D_8009C5C0[0].tpage = GetTPage(0, 0, 0x380, 0x100);
    D_8009C5C0[0].clut = GetClut(0x100, 0x1FE);
    SetSemiTrans(&D_8009C5C0[0], 1);
    D_8009C5C0[1] = D_8009C5C0[0];
    SetDrawTPage(&D_8009C5A0, 1, 0, GetTPage(0, 1, 0x380, 0x100));
    triangle = D_8009C664;
    for (i = 0; i < 8; i++) {
        ((u8 *)triangle)[3] = 6;
        triangle->code = 0x30;
        triangle->r0 = 0xFF;
        triangle->g0 = 0x40;
        triangle->b0 = 0x40;
        triangle->r1 = 0;
        triangle->g1 = 0;
        triangle->b1 = 0;
        triangle->r2 = 0;
        triangle->g2 = 0;
        triangle->b2 = 0;
        SetSemiTrans(triangle, 1);
        triangle++;
    }
    tile = D_8009C898;
    for (i = 0; i < 0x40; i++) {
        ((u8 *)tile)[3] = 3;
        tile->code = 0x60;
        tile->r0 = 0x80;
        tile->g0 = 0x80;
        tile->b0 = 0x10;
        tile->w = 2;
        tile->h = 2;
        tile++;
    }
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80073E30);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800740B8);

/* Allocate the recent-position ring and the two buffers of 16 footprint quads,
 * and initialise them. */
void func_80074594(void) {
    WorldmapSpot *spot;
    PolyFT4 *quad;
    s32 i;

    D_8009D30C = func_80031BDC(0x80, 0);
    D_8009BE14 = func_80031BDC(0x280, 0);
    D_8009BE18 = func_80031BDC(0x280, 0);
    spot = D_8009D30C;
    for (i = 15; i != -1; i--) {
        spot->z = 0;
        spot->id = 0;
        spot->x = 0;
        spot++;
    }
    quad = D_8009BE14;
    for (i = 15; i != -1; i--) {
        setPolyFT4(quad);
        setRGB0(quad, 0x40, 0x40, 0x48);
        quad->u0 = 0x80;
        quad->v0 = 0xF0;
        quad->u1 = 0x8F;
        quad->v1 = 0xF0;
        quad->u2 = 0x80;
        quad->v2 = 0xFF;
        quad->u3 = 0x8F;
        quad->v3 = 0xFF;
        quad->clut = GetClut(0x120, 0x1FE);
        quad->tpage = GetTPage(0, 0, 0x380, 0x100);
        SetSemiTrans(quad, 1);
        quad++;
    }
    *(QuadSet *)D_8009BE18 = *(QuadSet *)D_8009BE14;
    D_8009BE38 = 0;
}

/* Free the position ring and two work buffers. */
void func_8007474C(void) {
    func_800320E8(D_8009BE18);
    func_800320E8(D_8009BE14);
    func_800320E8(D_8009D30C);
}

/* Record a position (in world units) with an id in the 16-entry ring. */
void func_80074794(s16 id, VECTOR *position) {
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
            LoadImage(&anim->slot->rect, anim->images + anim->slot->frames[anim->frame].image * 16);
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

/* Advance the second texture animations; images are rect-sized. */
void func_80075104(void) {
    TexAnim *anim;
    RECT *rect;
    s32 size;
    s32 i;

    anim = D_8009D7D0;
    for (i = 0; i < D_8009CD64; i++, anim++) {
        if (--anim->timer == 0) {
            anim->frame++;
            anim->timer = anim->slot->frames[anim->frame].duration;
            if (anim->timer < 0) {
                anim->frame = 0;
                anim->timer = anim->slot->frames[0].duration;
            }
            rect = &anim->slot->rect;
            size = rect->h * rect->w * 2;
            LoadImage(rect, anim->images + anim->slot->frames[anim->frame].image * size);
        }
    }
}

/* Reset the movement state; vehicles move twice as fast as on foot. */
void func_80075228(void) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        D_8009C854[i] = 0;
    }
    D_8009D64C = 1;
    if (D_8006EE54.flags & 0x4000) {
        D_8009BE40 = 0x300;
    } else {
        D_8009BE40 = 0x180;
    }
    D_8009BCC4 = 1;
    D_8009D80C = 0;
}

/* Every D_8009BE40 frames give each of D_8009BCC4 timers a distinct random
 * delay (1..D_8009BE40); count down the timers and count those expiring. */
void func_8007528C(void) {
    s32 i;
    s32 j;
    s32 value;

    if (--D_8009D64C == 0) {
        for (i = 0; i < D_8009BCC4; i++) {
            if (i != 0) {
                do {
                    value = rand() % D_8009BE40 + 1;
                    for (j = 0; j < i; j++) {
                        if (D_8009C854[j] == value) {
                            break;
                        }
                    }
                } while (j < i);
            } else {
                value = rand() % D_8009BE40 + 1;
            }
            D_8009C854[i] = value;
        }
        D_8009D64C = D_8009BE40;
    }
    D_8009D80C = 0;
    for (i = 0; i < D_8009BCC4; i++) {
        if (--D_8009C854[i] == 0) {
            D_8009D80C++;
        }
    }
}

/* Save the world-map state to the resident save area. */
void func_80075460(void) {
    WorldmapSave *save;

    save = &D_8005A4E4;
    save->actors = *(ActorSet *)D_8009BE24;
    save->position.vx = D_8009D55C.target.vx;
    save->position.vy = D_8009D55C.target.vy;
    save->position.vz = D_8009D55C.target.vz;
    save->unk2010 = (s16)D_8009D52C;
    save->timer_period = D_8009BE40;
    save->timer_count = D_8009BCC4;
    save->timer_countdown = D_8009D64C;
    save->timers = *(TimerSet *)D_8009C854;
    save->queue = D_8009CEC4;
    save->queue_count = D_8009D154;
    save->camera_angle[0] = ((s32 *)&D_8009BD38)[0];
    save->camera_angle[1] = ((s32 *)&D_8009BD38)[1];
    save->camera_distance = D_8009D3F0;
    save->unk22D0 = D_8009BE0C;
    save->unk22E4[0] = ((s32 *)&D_8009C838)[0];
    save->unk22E4[1] = ((s32 *)&D_8009C838)[1];
    save->unk22D4.vx = ((VECTOR *)D_8009BBB4)->vx;
    save->unk22D4.vy = ((VECTOR *)D_8009BBB4)->vy;
    save->unk22D4.vz = ((VECTOR *)D_8009BBB4)->vz;
    save->camera_target.vx = D_8009BE28.target.vx;
    save->camera_target.vy = D_8009BE28.target.vy;
    save->camera_target.vz = D_8009BE28.target.vz;
}

/* Restore the world-map state from the resident save area. */
void func_8007565C(void) {
    WorldmapSave *save;

    save = &D_8005A4E4;
    *(ActorSet *)D_8009BE24 = save->actors;
    D_8009C5AC = save->position;
    D_8009D55C.target = save->position;
    *(TimerSet *)D_8009C854 = save->timers;
    D_8009D52C = save->unk2010;
    D_8009BE40 = save->timer_period;
    D_8009BCC4 = save->timer_count;
    D_8009D64C = save->timer_countdown;
    D_8009CEC4 = save->queue;
    D_8009D154 = save->queue_count;
    D_8009BD38 = *(SVECTOR *)save->camera_angle;
    D_8009D3F0 = save->camera_distance;
    D_8009BE0C = save->unk22D0;
    *(VECTOR *)D_8009BBB4 = save->unk22D4;
    D_8009C838 = *(SVECTOR *)save->unk22E4;
    D_8009BE28.target = save->camera_target;
}

/* Suspend the world map for another scene: record the return state, release
 * the area and save the VRAM areas the other scene overwrites. */
void func_800758C0(void) {
    RECT rect;
    void *block;
    s32 i;

    D_8006EE54.unk6A = 1;
    D_8006F950 = (D_8009BD38.vy + 0x2000) & 0x3FFF;
    for (i = 0; i < 3; i++) {
        (&D_8006EE54.unk70)[i] = (&D_8006F8E5)[i];
    }
    D_8009D14C = D_80059179;
    if (func_80093F18(&D_8009D55C.target) == 4) {
        D_80059179 = 1;
    }
    func_80096694();
    func_80086124();
    func_800866C8();
    func_80089128();
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
    block = func_80031BDC(4, 1);
    func_800320E8(block);
    block = (void *)((u32)block & 0xFFFFFF);
    D_8009C7E4 = func_80031BDC((u32)block - 0x1C4FFC, 1);
    func_80071FEC();
    D_8009C800 = func_80031BDC(0x10000, 0);
    D_8009C890 = func_80031BDC(0xC800, 0);
    rect.x = 0x180;
    rect.y = 0x100;
    rect.w = 0x80;
    rect.h = 0x100;
    StoreImage(&rect, D_8009C800);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x140;
    rect.h = 0x50;
    StoreImage(&rect, D_8009C890);
    if (D_8009D7F0 == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x10, 0, 8, 2);
    while (func_800286CC() >= 2) {
    }
    func_80032EB4(D_8009D528, D_8009C7E4);
    func_800320E8(D_8009D528);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0xE0);
    DrawSync(0);
    func_80028A60(0);
}

/* Resume the world map after another scene: reload the area, restore the saved
 * VRAM areas and bring the systems back up. */
void func_80075B58(void) {
    RECT rect;

    func_80032498(3, 0);
    func_80028470(0x24, 0);
    func_800320E8(D_8005945C);
    func_800320E8(D_8009C7E4);
    func_80072BB0();
    D_8009BD20 = func_80031BDC(func_800288EC(D_8009C174), 1);
    func_800295D8(D_8009C174, D_8009BD20, 0, 0);
    func_80072DB4(0x10, 0x80, -8, 2);
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0);
    MoveImage(&rect, 0, 0xD8);
    ClearOTagR(D_8009BE3C->ot, 0x400);
    func_80028A60(0);
    func_8008440C();
    rect.x = 0x180;
    rect.y = 0x100;
    rect.w = 0x80;
    rect.h = 0x100;
    LoadImage(&rect, D_8009C800);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x140;
    rect.h = 0x50;
    LoadImage(&rect, D_8009C890);
    DrawSync(0);
    func_800320E8(D_8009C800);
    func_800320E8(D_8009C890);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80085FE0();
    func_80033698(0x130, 0x1E0);
    VSync(0);
    func_80035DB0();
    D_8009D804 = 0;
    D_8006EE54.unk6A = 0;
    D_80059179 = D_8009D14C;
    func_80075D4C();
}

/* Apply party slots that joined or left since the last update, then pick the
 * movement mode from the members present. */
void func_80075D4C(void) {
    WorldmapActor *actors;
    u8 *applied;  /* state last applied per slot (2-byte records) */
    s16 *timers;  /* per-slot timer (6-byte records) */
    s32 i;
    s32 count;
    u8 state;

    i = 0;
    actors = D_8009BE24;
    applied = (u8 *)&D_8006EE54.unk70;
    timers = (s16 *)(applied + 0x11E);
    do {
        state = (&D_8006F8E5)[i];
        if (state != applied[i * 2]) {
            if (state == 0) {
                actors[i + 1].position.vx = actors[i + 4].position.vx;
                actors[i + 1].position.vy = actors[i + 4].position.vy;
                actors[i + 1].position.vz = actors[i + 4].position.vz;
                actors[i + 1].unk58 = actors[i + 4].unk58;
            } else {
                timers[i * 3] = 0x400;
                actors[i + 4].unk24 = 0;
                actors[i + 4].position.vx = actors[i + 1].position.vx;
                actors[i + 4].position.vy = actors[i + 1].position.vy;
                actors[i + 4].position.vz = actors[i + 1].position.vz;
                actors[i + 4].unk58 = actors[i + 1].unk58;
            }
        }
        i++;
    } while (i < 3);
    count = 0;
    for (i = 0; i < 3; i++) {
        if (D_8006F368[i] != 0xFF && (&D_8006F8E5)[i] == 1) {
            count++;
        }
    }
    if (!(D_8006EE54.flags & 0x4000)) {
        D_8009BE10 = count != 0 ? 2 : 1;
    }
}

/* Roll an encounter for the terrain at a position and a party level: pick a
 * formation by the bracket's weights and copy the terrain's encounter set.
 * Returns 0 when the bracket has no formations. */
#ifdef NON_MATCHING /* formation search: original keeps a second copy of the weight pointer */
s32 func_80075E7C(VECTOR *position, s32 level) {
    u8 weights[16];
    s32 kind;
    s32 bracket;
    s32 total;
    s32 roll;
    s32 formation;
    s32 result;
    s32 i;
    u8 *weight;
    u8 *row;

    kind = (s16)func_80094028(position);
    if (func_80093F18(position) == 4) {
        kind = D_8009A3A0[kind];
    }
    bracket = 1;
    while (level >= D_8009B578[bracket]) {
        bracket++;
    }
    bracket--;
    total = 0;
    row = (u8 *)D_8009D73C[kind] + 0x200; /* weights: 16 per bracket */
    row += bracket * 16;
    for (i = 0; i < 16; i++) {
        weights[i] = row[i];
        total += row[i];
    }
    result = 0;
    if (total > 0) {
        roll = rand() % total + 1;
        formation = 0;
        weight = weights;
        do {
            roll--;
            while (*weight == 0) {
                weight++;
                formation++;
            }
            (*weight)--;
        } while (roll > 0);
        D_800658DC = *(EncounterSet *)D_8009D73C[kind];
        D_80059508 = formation;
        result = 1;
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075E7C);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076098);

/* Reset the GPU and sound state before leaving. */
void func_800762FC(void) {
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    DrawSync(0);
    VSync(0);
    FlushCache();
    ExitCriticalSection();
}

/* Pause: show the pause screen on the other buffer until button 0x800 is
 * pressed, then restore the display. */
void func_8007634C(void) {
    RECT rect;
    s32 saved;

    saved = D_80059488;
    DrawSync(0);
    VSync(0);
    if (D_8009D7F0 == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    PutDispEnv(&D_8009BBC8[1].disp);
    PutDrawEnv(&D_8009BBC8[1].draw);
    func_80037EE4();
    do {
        DrawSync(0);
        VSync(0);
        func_8001FAB4(0x88, 0x64);
        D_8009BD1C = 0;
        D_8009BD14 = 0;
        D_8009CD50 = 0;
        D_8009BD18 = 0;
        D_8009BD10 = 0;
        D_8009CD4C = 0;
        while (func_80035CDC() != 0) {
            D_8009CD4C |= D_80059570;
            D_8009CD50 |= D_80059574;
            D_8009BD10 |= D_8005948C;
            D_8009BD14 |= D_80059490;
            D_8009BD18 |= D_800594A4;
            D_8009BD1C |= D_800594A8;
        }
    } while (!(D_8009BD10 & 0x800));
    func_80037E8C();
    DrawSync(0);
    VSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0xD8);
    PutDispEnv(&D_8009BBC8[D_8009D7F0].disp);
    PutDrawEnv(&D_8009BBC8[D_8009D7F0].draw);
    D_80059488 = saved;
}

/* Wait on the other buffer until the pad check (func_80035734) succeeds,
 * then restore the display. */
void func_80076594(void) {
    RECT rect;
    s32 saved;

    saved = D_80059488;
    DrawSync(0);
    VSync(0);
    if (D_8009D7F0 == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    PutDispEnv(&D_8009BBC8[1].disp);
    PutDrawEnv(&D_8009BBC8[1].draw);
    func_80037EE4();
    do {
        DrawSync(0);
        VSync(0);
        func_8001FAB4(0x88, 0x64);
        D_8009BD1C = 0;
        D_8009BD14 = 0;
        D_8009CD50 = 0;
        D_8009BD18 = 0;
        D_8009BD10 = 0;
        D_8009CD4C = 0;
        while (func_80035CDC() != 0) {
            D_8009CD4C |= D_80059570;
            D_8009CD50 |= D_80059574;
            D_8009BD10 |= D_8005948C;
            D_8009BD14 |= D_80059490;
            D_8009BD18 |= D_800594A4;
            D_8009BD1C |= D_800594A8;
        }
    } while (func_80035734(0) == 0);
    func_80037E8C();
    DrawSync(0);
    VSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0xD8);
    PutDispEnv(&D_8009BBC8[D_8009D7F0].disp);
    PutDrawEnv(&D_8009BBC8[D_8009D7F0].draw);
    D_80059488 = saved;
}

/* Replace the music: stop the current sequence and start `data` (the
 * contents of disc file `file`). */
void func_800767D4(void *data, s32 file) {
    func_80039CC4();
    func_800399D4(D_80062528);
    memcpy(D_80062648, data, func_800288EC(file));
    D_80062528 = func_80039850(D_80062648);
    func_80039A80(D_80062528, 0x7F, 0);
}

/* Quadratic Bezier point at t (0..0x1000) through three control points. */
void func_80076858(s32 t, SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, VECTOR *out) {
    s32 w0;
    s32 w1;
    s32 w2;
    s32 s;

    s = 0x1000 - t;
    w0 = (s * s * 8) >> 12;
    w1 = ((s * t) >> 8) + 0x8000;
    w2 = (t * t * 8) >> 12;
    out->vx = p0->vx * w0 + p1->vx * w1 + p2->vx * w2;
    out->vy = p0->vy * w0 + p1->vy * w1 + p2->vy * w2;
    out->vz = p0->vz * w0 + p1->vz * w1 + p2->vz * w2;
}

/* Unpack a replacement area file and resolve its sections (no spots or
 * models). */
void func_80076954(void) {
    void *block;

    block = D_8009C180;
    D_8009C180 = func_80032E88(block, 0);
    func_800320E8(block);
    D_8009CD48 = (u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off8;
    D_8009D308 = (u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->offC;
    D_8009BD30 = (u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off10;
    D_8009C7EC = (TerrainTexture *)((u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off14);
    D_8009BCC0 = (AreaObject *)((u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off18);
    D_8009D77C = (s32 *)((u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off20);
    D_8009D7C8 = (s32 *)((u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off24);
}

/* Mode step that has nothing to do; always reports done. */
s32 func_80076A14(void) {
    return 1;
}

/* One world-map frame of a scripted scene (no player input). */
s32 func_80076A1C(void) {
    if (D_8009D144 == 0) {
        func_80097440(D_8009BD40);
    } else {
        func_80097244(D_8009BD40);
    }
    func_80089748();
    func_80089C78();
    func_8008615C();
    func_800848F4();
    func_800980D4(D_8009BBB4);
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
    return 1;
}

/* Run an actor's script until an opcode yields. */
s32 func_80076B34(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    s32 step;
    s32 word;

    actor = &D_8009BE24[index];
    step = 0;
    do {
        actor->u.script += step;
        word = *(s32 *)actor->u.script;
        step = D_8009A3C0[word & 0xFFFF](actor, word >> 16, actor->u.script[2], actor->u.script[3]);
    } while (step != 0);
    return 1;
}

/* Script opcode 0: end the world map. */
s32 func_80076BC4(void) {
    D_8009D554 = 0;
    D_8009D7CC = 0;
    return 0;
}

/* Script opcode 1: wait `frames` frames. */
s32 func_80076BDC(WorldmapActor *actor, s16 frames) {
    if (actor->wait == 0) {
        actor->wait = frames;
        return 0;
    }
    if (--actor->wait <= 0) {
        return 2;
    }
    return 0;
}

/* Script opcode 2. */
s32 func_80076C18(WorldmapActor *actor, s32 a, s32 b) {
    func_80097770(a, b);
    return 4;
}

/* Script opcode 3: place the player (world units). */
s32 func_80076C3C(WorldmapActor *actor, s32 x, s32 y, s32 z) {
    D_8009C5AC.vx = x << 12;
    D_8009C5AC.vy = y << 12;
    D_8009C5AC.vz = z << 12;
    return 4;
}

/* Script opcode 4: set the script vector. */
s32 func_80076C68(WorldmapActor *actor, s16 x, s16 y, s16 z) {
    SCRIPT_VECTOR->vx = x;
    SCRIPT_VECTOR->vy = y;
    SCRIPT_VECTOR->vz = z;
    return 4;
}

/* Script opcode 5. */
s32 func_80076C88(WorldmapActor *actor, s32 a) {
    func_80089160(a, SCRIPT_VECTOR, 0);
    return 2;
}

/* Script opcode 6. */
s32 func_80076CB4(WorldmapActor *actor, s32 a) {
    func_800894C8(a);
    return 2;
}

/* Script opcode 7. */
s32 func_80076CD4(WorldmapActor *actor, s32 a) {
    func_80089514(a);
    return 2;
}

/* Script opcode 8: music control. */
s32 func_80076CF4(WorldmapActor *actor, s32 a, s32 b) {
    func_8003A89C(D_80062528, a, b);
    return 4;
}

/* Script opcode 9: play a sound of the area bank. */
s32 func_80076D1C(WorldmapActor *actor, s32 sound) {
    func_80039E60((D_8006259C->id << 16) | sound);
    return 2;
}

/* Script opcode 10: play a sound of the area bank with parameters. */
s32 func_80076D50(WorldmapActor *actor, s32 sound, s32 b, s32 c) {
    func_8003A3B8((D_8006259C->id << 16) | sound, b, c);
    return 4;
}

/* Script opcode 11. */
s32 func_80076D8C(WorldmapActor *actor, s32 a, s32 b) {
    D_8009CCA4 = a;
    D_8009D3CC = b;
    return 4;
}

/* Move the actor's current point an eighth of the way to its target (snapping
 * when close) and aim the camera from it. */
void func_80076DA4(WorldmapActor *actor, VECTOR *work) {
    if ((actor->motion.vx != actor->u.step) | (actor->motion.vy != actor->unk54) |
        (actor->motion.vz != actor->unk58)) {
        work[0].vx = actor->u.step - actor->motion.vx;
        work[0].vy = actor->unk54 - actor->motion.vy;
        work[0].vz = actor->unk58 - actor->motion.vz;
        func_80093484(&work[0]);
        work[1].vx = work[0].vx >> 3;
        work[1].vy = work[0].vy >> 3;
        work[1].vz = work[0].vz >> 3;
        if (ABS(work[1].vx) < 0x40) {
            actor->motion.vx = actor->u.step;
        } else {
            actor->motion.vx += work[1].vx;
        }
        if (ABS(work[1].vy) < 0x40) {
            actor->motion.vy = actor->unk54;
        } else {
            actor->motion.vy += work[1].vy;
        }
        if (ABS(work[1].vz) < 0x40) {
            actor->motion.vz = actor->unk58;
        } else {
            actor->motion.vz += work[1].vz;
        }
    }
    D_8009BD38.vx = actor->motion.vx >> 12;
    D_8009BD38.vy = actor->motion.vy >> 12;
    D_8009BD38.vz = actor->motion.vz >> 12;
}

/* Ease the camera distance towards the actor's, snapping when close. */
void func_80076F54(WorldmapActor *actor) {
    s32 target;
    s32 step;

    target = actor->unk5C;
    if (target != D_8009D3F0) {
        step = (target - D_8009D3F0) >> 3;
        if ((step < 0 ? -step : step) < 0x40) {
            D_8009D3F0 = target;
        } else {
            D_8009D3F0 += step;
        }
    }
}

/* Move the actor an eighth of the way to the saved camera target (snapping when
 * close), scrolling the ground offset with it, and look at the actor. */
void func_80076FA8(WorldmapActor *actor, VECTOR *work) {
    if ((actor->position.vx != D_8009D55C.target.vx) | (actor->position.vy != D_8009D55C.target.vy) |
        (actor->position.vz != D_8009D55C.target.vz)) {
        work[0].vx = D_8009D55C.target.vx - actor->position.vx;
        work[0].vy = D_8009D55C.target.vy - actor->position.vy;
        work[0].vz = D_8009D55C.target.vz - actor->position.vz;
        func_80093484(&work[0]);
        work[1].vx = work[0].vx >> 3;
        work[1].vy = work[0].vy >> 3;
        work[1].vz = work[0].vz >> 3;
        if (ABS(work[1].vx) < 0x200) {
            GROUND_SCROLL[0] += work[0].vx;
            actor->position.vx = D_8009D55C.target.vx;
        } else {
            GROUND_SCROLL[0] += work[1].vx;
            actor->position.vx += work[1].vx;
        }
        if (ABS(work[1].vy) < 0x200) {
            actor->position.vy = D_8009D55C.target.vy;
        } else {
            actor->position.vy += work[1].vy;
        }
        if (ABS(work[1].vz) < 0x200) {
            GROUND_SCROLL[2] += work[0].vz;
            actor->position.vz = D_8009D55C.target.vz;
        } else {
            GROUND_SCROLL[2] += work[1].vz;
            actor->position.vz += work[1].vz;
        }
    }
    D_8009BE28.target = actor->position;
}

/* Step `value` towards `target` by `delta`, stopping on it. */
#ifdef NON_MATCHING /* first branch delay slot filled with the delta copy */
s32 func_800771D8(s32 value, s32 target, s32 delta) {
    s32 distance;
    s32 size;

    if (value != target) {
        distance = target - value;
        if (distance < 0) {
            distance = -distance;
        }
        size = ABS(delta);
        value += delta;
        if (distance < size) {
            value = target;
        }
    }
    return value;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800771D8);
#endif

/* Set up the scripted flight scene: display, terrain loader, scene objects and
 * its four actors. */
void func_80077214(void) {
    RECT rect;

    func_80072BB0();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 2);
    while (func_800286CC() >= 3) {
    }
    func_80073530();
    func_8009766C();
    D_8009BE4C = D_8009A180;
    D_8009CCA4 = 2;
    D_8009D3CC = 4;
    D_8009D804 = 0;
    D_8009D144 = 0;
    D_8009CD40 = func_80086700;
    func_80098044();
    func_80028A60(0);
    D_8009C5AC.vx = 0x7702000;
    D_8009C5AC.vy = -0x300000;
    D_8009C5AC.vz = 0x27C0000;
    func_80084580();
    func_8008440C();
    func_800979C8();
    func_800736DC();
    func_80085F58();
    func_800863E0();
    func_80074E58();
    func_80075030();
    func_800739B8();
    func_80088F64();
    func_80028470(0x24, 0);
    func_80097BC0(&D_8009C5AC);
    do {
        func_800967E4();
        VSync(0);
    } while (func_80096668() > 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_8007756C, (s32)func_800776E0);
    func_80097718((s32)func_80087710, (s32)func_80087734);
    func_80097718((s32)func_80071A50, (s32)func_80071A58);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80085FE0();
    func_80075228();
    func_80089160(0xE, NULL, NULL);
}

/* Leave for scene 0x11: shut down the subsystems and free the area. */
void func_80077480(void) {
    func_80084818();
    func_80086124();
    func_80086568();
    func_800866C8();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    func_800320E8(D_8009BC38[0]);
    func_800320E8(D_8009BCB0[0]);
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006F94E = 0x11;
    D_8006F954[0] = 7;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Start a scripted camera looking along the player's heading from above. */
s32 func_8007756C(s32 index) {
    WorldmapActor *actor;

    D_8009BD38.vx = -0x80;
    D_8009BD38.vy = 0x200;
    D_8009D3F0 = 0x400000;
    D_8009BD38.vz = 0;
    actor = &D_8009BE24[index];
    actor->position.vx = -0x80000;
    D_8009BE0C = 0x78;
    actor->position.vy = D_8009BD38.vy << 12;
    actor->motion = actor->position;
    D_8009D144 = 0;
    D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009BE28.target.vz = D_8009C5AC.vz;
    func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    *SCRIPT_VECTOR = VIEW_VECTORS[0];
    VIEW_VECTORS[0] = VIEW_VECTORS[1];
    VIEW_VECTORS[1] = *SCRIPT_VECTOR;
    return 1;
}

/* Steer the scripted camera with the pad (clamped yaw range, pitch), ease its
 * angles and swap the view vectors. */
s32 func_800776E0(s32 index) {
    CameraScratch *scratch;
    WorldmapActor *actor;

    scratch = (CameraScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    if (D_8009BD10 & 0x40) {
        D_8009D554 = 0;
        D_8009D7CC = 0;
    }
    if (D_8009CD4C & 0x1000) {
        actor->position.vx += 0x8000;
    }
    if (D_8009CD4C & 0x4000) {
        actor->position.vx -= 0x8000;
    }
    if (actor->position.vx < -0x180000) {
        actor->position.vx = -0x180000;
    } else if (actor->position.vx > 0x18000) {
        actor->position.vx = 0x18000;
    }
    if (D_8009CD4C & 0x8004) {
        actor->position.vy -= 0x10000;
    }
    if (D_8009CD4C & 0x2008) {
        actor->position.vy += 0x10000;
    }
    if ((actor->motion.vx != actor->position.vx) | (actor->motion.vy != actor->position.vy)) {
        scratch->delta.vx = actor->position.vx - actor->motion.vx;
        scratch->delta.vy = actor->position.vy - actor->motion.vy;
        actor->motion.vx += scratch->delta.vx >> 3;
        actor->motion.vy += scratch->delta.vy >> 3;
    }
    D_8009BD38.vx = actor->motion.vx >> 12;
    D_8009BD38.vy = actor->motion.vy >> 12;
    func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    scratch->view = VIEW_VECTORS[0];
    VIEW_VECTORS[0] = VIEW_VECTORS[1];
    VIEW_VECTORS[1] = scratch->view;
    D_8009BD38.vy = (D_8009BD38.vy + 0x800) & 0xFFF;
    SetGeomScreen(D_8009BCDC);
    return 1;
}

/* Mode step that has nothing to do; always reports done. */
s32 func_80077954(void) {
    return 1;
}

/* One world-map frame of a scripted scene without actor updates. */
s32 func_8007795C(void) {
    if (D_8009D144 == 0) {
        func_80097440(D_8009BD40);
    } else {
        func_80097244(D_8009BD40);
    }
    func_8008615C();
    func_800848F4();
    func_800980D4(D_8009BBB4);
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
    return 1;
}

/* Set up the scene mode: display, terrain loader, scene objects and its four
 * actors. */
void func_80077A64(void) {
    RECT rect;

    func_80072BB0();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 1);
    while (func_800286CC() >= 3) {
    }
    func_80076954();
    func_8009766C();
    D_8009BE4C = D_8009A180;
    D_8009CCA4 = 1;
    D_8009D3CC = 4;
    D_8009D804 = 0;
    D_8009D144 = 0;
    D_8009CD40 = func_80086700;
    func_80098044();
    func_80028A60(0);
    func_800721E4();
    D_8009C5AC.vx = 0x2000000;
    D_8009C5AC.vy = -0x200000;
    D_8009C5AC.vz = 0x2000000;
    func_80084580();
    func_8008440C();
    func_800979C8();
    func_800736DC();
    func_800863E0();
    func_80074E58();
    func_80075030();
    func_800739B8();
    func_80088F64();
    func_80028A60(0);
    func_80038428(D_8006259C);
    func_80028470(0x24, 0);
    func_80097BC0(&D_8009C5AC);
    do {
        func_800967E4();
        VSync(0);
    } while (func_80096668() > 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_80077DC8, (s32)func_80077E68);
    func_80097718((s32)func_8007828C, (s32)func_800783E8);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
}

/* Leave for scene 0x10E: stop the sound bank, shut down and free the area. */
void func_80077CC0(void) {
    func_80039FF8();
    func_8003852C(D_8006259C);
    func_800320E8(D_8006259C);
    func_80084818();
    func_80086568();
    func_800866C8();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    func_800320E8(D_8009BC38[0]);
    func_800320E8(D_8009BCB0[0]);
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006F94E = 0x10E;
    D_8006F954[0] = 0;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Start a scripted camera: reset the actor and camera, play a sound. */
s32 func_80077DC8(s32 index) {
    WorldmapActor *actor;

    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x200000;
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->unk58 = 0;
    actor->unk54 = 0;
    actor->u.script = NULL;
    D_8009BD38.vz = 0;
    D_8009BD38.vy = 0;
    D_8009BD38.vx = 0;
    D_8009D144 = 1;
    func_80039E60((D_8006259C->id << 16) | 0xA4);
    actor->wait = 0x18;
    return 1;
}
