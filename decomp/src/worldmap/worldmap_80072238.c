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
    func_8004931C(&D_8009C808, &SKY_SCRATCH->rotation, &SKY_SCRATCH->view);
    func_80049EFC(&SKY_SCRATCH->view);
    func_80049F8C(&SKY_SCRATCH->view);
    for (i = 0; i < 4; i++) {
        band = (PolyG4 *)((u8 *)&D_8009D194[0][D_8009D7F0] + offset);
        otz = func_8004A73C(&corners[0], &corners[1], &corners[2], &corners[3], &band->xy0, &band->xy1,
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

    tpage = func_80043A1C(0, 1, 0x380, 0x100);
    clut = func_80043A58(0x110, 0x1FE);
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
        func_80043BFC(quad, 1);
    }
    window.x = 0;
    window.y = 0;
    window.w = 0x80;
    window.h = 0;
    func_800453AC(&D_8009D3D8[0], &window);
    window.x = 0;
    window.y = 0;
    window.w = 0;
    window.h = 0;
    func_800453AC(&D_8009D3D8[1], &window);
}

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
            func_80044894(rect, anim->images + anim->slot->frames[anim->frame].image * size);
        }
    }
}

/* Reset the movement state; vehicles move twice as fast as on foot. */
#ifdef NON_MATCHING /* constant 1 is reused after the branch instead of reloaded */
void func_80075228(void) {
    s32 i;
    s32 speed;

    for (i = 15; i >= 0; i--) {
        D_8009C854[i] = 0;
    }
    D_8009D64C = 1;
    speed = 0x300;
    if (!(D_8006EE54.flags & 0x4000)) {
        speed = 0x180;
    }
    D_8009BE40 = speed;
    D_8009BCC4 = 1;
    D_8009D80C = 0;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075228);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007528C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075460);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007565C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800758C0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075B58);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075D4C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80075E7C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076098);

/* Reset the GPU and sound state before leaving. */
void func_800762FC(void) {
    func_800445D0(0);
    func_8004B54C(0);
    func_800404D4();
    func_800445D0(0);
    func_8004B54C(0);
    func_80040454();
    func_800404E4();
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007634C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076594);

/* Replace the music: stop the current sequence and start disc file `file`. */
void func_800767D4(s32 mode, s32 file) {
    func_80039CC4();
    func_800399D4(D_80062528);
    func_8003F968(D_80062648, mode, func_800288EC(file));
    D_80062528 = func_80039850(D_80062648);
    func_80039A80(D_80062528, 0x7F, 0);
}

/* Quadratic Bezier point at t (0..0x1000) through three control points. */
void func_80076858(s32 t, SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, Vec3 *out) {
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
    D_8009C7EC = (u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off14;
    D_8009BCC0 = (u8 *)D_8009C180 + ((AreaHeader *)D_8009C180)->off18;
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
        func_800981C8(D_8009BE28);
        func_80096130();
        func_80098CC0();
    }
    func_800983A0(D_8009BE28);
    func_8009932C(D_8009BE3C->ot, D_8009BE3C->unk74, D_8009BE28);
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
        actor->script += step;
        word = *(s32 *)actor->script;
        step = D_8009A3C0[word & 0xFFFF](actor, word >> 16, actor->script[2], actor->script[3]);
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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076DA4);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80076FA8);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077214);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077480);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007756C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_800776E0);

/* Mode step that has nothing to do; always reports done. */
s32 func_80077954(void) {
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_8007795C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077A64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80072238", func_80077CC0);

/* Start a scripted camera: reset the actor and camera, play a sound. */
s32 func_80077DC8(s32 index) {
    WorldmapActor *actor;

    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x200000;
    actor = &D_8009BE24[index];
    actor->unk20 = 0;
    actor->unk58 = 0;
    actor->unk54 = 0;
    actor->script = NULL;
    D_8009BD38.vz = 0;
    D_8009BD38.vy = 0;
    D_8009BD38.vx = 0;
    D_8009D144 = 1;
    func_80039E60((D_8006259C->id << 16) | 0xA4);
    actor->wait = 0x18;
    return 1;
}
