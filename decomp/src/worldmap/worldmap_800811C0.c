#include "worldmap.h"

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 37 u16 states and 37 u16 waits (started by
 * func_80081174; tools/analysis/overlay_scripts.py decodes it). The waits'
 * 0x4C bytes end with the stray halfword (0x2E07) the original object keeps
 * in its alignment padding. */
INCLUDE_ASSET(".data", D_8009A6C0, 0x8009A6C0, 0x4A);
INCLUDE_ASSET(".data", D_8009A70C, 0x8009A70C, 0x4C);

/* Actor script (func_80076B34 commands) given by func_800827C8: 434 signed
 * halfwords, user-supplied (an asset in worldmap.classification.txt;
 * tools/analysis/overlay_scripts.py decodes it). */
INCLUDE_ASSET(".data", D_8009A758, 0x8009A758, 0x364);

/* The pulsing effect's settings for its commands 1, 2 and 4 (func_80083264):
 * five slots of 14, the position and then the actor's parameters. */
s16 D_8009AABC[5 * 14] = {
    2673, -876, 18618, 64, 128, 255, 2, 1, 0, -64, 0, 0, 20, 0,
    2673, -844, 18682, 96, 96, 255, -4, -2, 0, -32, 1024, 0, 18, 0,
    2673, -748, 18746, 128, 64, 255, 8, 8, 0, 0, 2048, 0, 24, 0,
    2673, -860, 18874, 96, 96, 255, -6, -4, 0, 32, 3072, 0, 22, 0,
    2673, -876, 18938, 64, 128, 255, 2, 1, 0, 64, 1536, 0, 16, 0,
};
s16 D_8009AB48[5 * 14] = {
    28280, -1248, 10200, 64, 128, 255, 2, 1, 0, -640, 0, 0, 20, 0,
    28280, -1216, 10264, 96, 96, 255, -4, -2, 0, -608, 1024, 0, 18, 0,
    28280, -1120, 10328, 128, 64, 255, 8, 8, 0, -576, 2048, 0, 24, 0,
    28280, -1232, 10456, 96, 96, 255, -6, -4, 0, -544, 3072, 0, 22, 0,
    28280, -1248, 10520, 64, 128, 255, 2, 1, 0, -512, 1536, 0, 16, 0,
};
s16 D_8009ABD4[5 * 14] = {
    20344, -760, 6474, 64, 128, 255, 2, 1, 0, -64, 0, 0, 20, 0,
    20344, -728, 6538, 96, 96, 255, -4, -2, 0, -32, 1024, 0, 18, 0,
    20344, -632, 6602, 128, 64, 255, 8, 8, 0, 0, 2048, 0, 24, 0,
    20344, -744, 6730, 96, 96, 255, -6, -4, 0, 32, 3072, 0, 22, 0,
    20344, -760, 6794, 64, 128, 255, 2, 1, 0, 64, 1536, 0, 16, 0,
};

/* Actor script (func_80076B34 commands) given by func_800838E8: 102 signed
 * halfwords, user-supplied like D_8009A758. */
INCLUDE_ASSET(".data", D_8009AC60, 0x8009AC60, 0xCC);

/* Heat-haze scene director (mode 16): func_8007A9F8's cue sequencer on
 * D_8009A6C0/D_8009A70C. Actor slots (func_80080D00): 0 the screen fade, 2
 * the camera (func_80081470), 3 object 2's fade (func_80081868), 4 objects
 * 0 and 1 (func_80081B24), 6 the heat-haze strength (func_80081FD8). The
 * fade with rate 2 subtracts the fade quad (black). */
s32 func_800811C0(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = D_8009A6C0[actor->u.step];
            actor->wait = D_8009A70C[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: slot 6 request 1. */
    case 0x2:
        func_80097770(6, 1);
        actor->state = 1;
        break;
    /* 3: slot 6 request 0, which no case of func_80081FD8 takes. */
    case 0x3:
        func_80097770(6, 0);
        actor->state = 1;
        break;
    /* 4: slot 6 request 2. */
    case 0x4:
        func_80097770(6, 2);
        actor->state = 1;
        break;
    /* 5: slot 6 request 3. */
    case 0x5:
        func_80097770(6, 3);
        actor->state = 1;
        break;
    /* 6: slot 6 request 4. */
    case 0x6:
        func_80097770(6, 4);
        actor->state = 1;
        break;
    /* 7: slot 6 request 5. */
    case 0x7:
        func_80097770(6, 5);
        actor->state = 1;
        break;
    /* 8: area sounds 0x2E-0x30. */
    case 0x8:
        func_80039E60((D_8006259C->id << 16) | 0x2E);
        func_80039E60((D_8006259C->id << 16) | 0x2F);
        func_80039E60((D_8006259C->id << 16) | 0x30);
        actor->state = 1;
        break;
    /* 0x10: slots 3 and 4 request 1. */
    case 0x10:
        func_80097770(3, 1);
        func_80097770(4, 1);
        actor->state = 1;
        break;
    /* 0x11: slot 3 request 2. */
    case 0x11:
        func_80097770(3, 2);
        actor->state = 1;
        break;
    /* 0x3F: fade the music out over 0xF0 frames; fade out at rate 2, 4 per
     * frame. */
    case 0x3F:
        func_8003A89C((SoundSeq *)D_80062528, 0, 0xF0);
        func_80097770(0, 0xD);
        D_8009CCA4 = 2;
        D_8009D3CC = 4;
        actor->state = 1;
        break;
    /* 0x40: end the world-map loop with exit 0; idle. */
    case 0x40:
        D_8009D554 = 0;
        D_8009D7CC = 0;
        actor->state = 0;
        break;
    }
    return 1;
}

/* Start a scripted camera at the player position. */
s32 func_800813E8(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk4 = 1;
    actor->state = 0;
    return 1;
}

/* Scripted zoom-in camera with a random vertical shake (command 1 starts the zoom). */
s32 func_80081470(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    s32 delta;

    scratch = (ActorScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 2:
        break;
    case 3:
        break;
    case 1:
        actor->state = 1;
        actor->unk4 = 0;
        D_8009BD38.vx = -0x80;
        D_8009BD38.vy = -0x200;
        D_8009BD38.vz = 0;
        actor->u.step = 0x980000;
        actor->unk54 = actor->unk58 = D_8009BD38.vx << 12;
        D_8009D3F0 = 0x980000;
        actor->unk5C = actor->unk60 = D_8009BD38.vy << 12;
        break;
    }
    if (D_8009D144 == 0) {
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    if (actor->state == 1) {
        if ((actor->u.step -= 0x10000) < 0x630000) {
            actor->u.step = 0x630000;
        }
        delta = actor->u.step - D_8009D3F0;
        if (delta != 0) {
            D_8009D3F0 += delta >> 5;
        }
        if ((actor->unk54 += 0x1000) > 0x10000) {
            actor->unk54 = 0x10000;
        }
        delta = actor->unk54 - actor->unk58;
        if (delta != 0) {
            actor->unk58 += delta >> 4;
            D_8009BD38.vx = actor->unk58 >> 12;
        }
        if ((actor->unk5C += 0x10000) > 0x4B0000) {
            actor->unk5C = 0x4B0000;
        }
        delta = actor->unk5C - actor->unk60;
        if (delta != 0) {
            actor->unk60 += delta >> 5;
            D_8009BD38.vy = actor->unk60 >> 12;
        }
    }
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    ((s16 *)D_8009BD40)[1] += scratch->position.vy; /* VIEW_VECTORS[0].vy */
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* Build `count` semi-transparent textured quads on page 0x180,0. */
void func_800816DC(SceneObject *object, POLY_FT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x180, 0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT4));
}

/* Start the actor above the player and build scene object 2 there. */
s32 func_800817A0(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    actor->state = 0;
    actor->position.vx = D_8009C5AC.vx;
    actor->position.vy = D_8009C5AC.vy - 0x100000;
    actor->position.vz = D_8009C5AC.vz;
    actor->u.step = 0;
    actor->unk54 = 0;
    actor->unk58 = 0;
    func_800816DC(&objects[2], objects[2].prims, objects[2].def->primitive_count, 1);
    objects[2].position.vx = actor->position.vx >> 12;
    objects[2].position.vy = actor->position.vy >> 12;
    objects[2].position.vz = actor->position.vz >> 12;
    return 1;
}

/* Fade scene object 2 in (command 1) or reset it to opaque grey (command 2). */
s32 func_80081868(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    POLY_FT4 *quad;
    s32 i;

    actor = &D_8009BE24[index];
    object = &D_8009C620[2];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0;
        quad = (&object->prims)[D_8009D7F0];
        for (i = 0; i < object->def->primitive_count; i++) {
            setSemiTrans(quad, 0);
            setRGB0(quad, 0x80, 0x80, 0x80);
            quad++;
        }
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->u.step += 1;
        actor->unk54 += 1;
        actor->unk58 += 1;
        if (actor->u.step >= 0xFF) {
            actor->unk58 = 0xFF;
            actor->unk54 = 0xFF;
            actor->u.step = 0xFF;
            actor->state = 0;
        }
        break;
    }
    func_800809EC((&object->prims)[D_8009D7F0], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* Place the actor at the player and rebuild scene objects 0-1 stretched 7x in height. */
s32 func_800819C8(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position.vx = D_8009C5AC.vx;
    actor->position.vy = D_8009C5AC.vy;
    actor->position.vz = D_8009C5AC.vz;
    actor->u.step = 0;
    actor->unk54 = 0;
    actor->unk58 = 0;
    objects[0].matrix = D_8009A180;
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = 0x1000;
    SCALE_SCRATCH->scale[0].vy = 0x7000;
    ScaleMatrix(&objects[0].matrix, &SCALE_SCRATCH->scale[0]);
    objects[1].matrix = objects[0].matrix;
    func_800816DC(objects, objects->prims, objects->def->primitive_count, 3);
    objects++;
    func_800816DC(objects, objects->prims, objects->def->primitive_count, 3);
    return 1;
}

/* Fade scene objects 0 and 1 in (command 1 starts it). */
s32 func_80081B24(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    actor = &D_8009BE24[index];
    object = D_8009C620;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->state = 1;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->u.step += 4;
        actor->unk54 += 2;
        actor->unk58 += 1;
        if (actor->u.step >= 0xFC) {
            actor->u.step = 0xFC;
            actor->state = 0;
        }
        break;
    }
    func_800809EC((&object->prims)[D_8009D7F0], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    object++;
    func_800809EC((&object->prims)[D_8009D7F0], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* Allocate the shared quad pool (two display copies) and mark every quad free. */
s32 func_80081C3C(void) {
    POLY_FT4 *quads;
    s32 i;
    s16 *flags;

    D_8009D158[0] = func_80031BDC(sizeof(QuadBuffer), 0);
    D_8009D158[1] = func_80031BDC(sizeof(QuadBuffer), 0);
    D_8009D148 = func_80031BDC(0xC0 * sizeof(s16), 0);
    quads = D_8009D158[0]->quads;
    for (i = 0; i < 0xC0; i++) {
        setPolyFT4(quads);
        setRGB0(quads, 0x80, 0x80, 0x80);
        setShadeTex(quads, 1);
        quads->tpage = GetTPage(2, 0, 0x280, 0x100);
        quads++;
    }
    *D_8009D158[1] = *D_8009D158[0];
    flags = D_8009D148;
    for (i = 0; i < 0xC0; i++) {
        *flags++ = 1;
    }
    return 1;
}

/* Heat haze: offset each of 192 one-pixel rows by a random amount and copy the result back to the frame. */
s32 func_80081D80(void) {
    RECT rect;
    POLY_FT4 *quad;
    u16 *spread;
    s32 row;
    s32 next;
    s32 offset;

    row = 0;
    quad = D_8009D158[D_8009D7F0]->quads;
    spread = D_8009D148;
    do {
        offset = rand() % *spread - (*spread >> 1);
        spread++;
        quad->y0 = row;
        quad->y1 = row;
        quad->v0 = row;
        quad->v1 = row;
        quad->u0 = 0;
        quad->u1 = 0xC0;
        quad->u2 = 0;
        quad->u3 = 0xC0;
        quad->x0 = offset + 0x40;
        quad->x1 = offset + 0x100;
        quad->x2 = offset + 0x40;
        quad->x3 = offset + 0x100;
        next = row + 1;
        quad->y2 = next;
        quad->y3 = next;
        quad->v2 = next;
        quad->v3 = next;
        row = next;
        addPrim(D_8009BE3C->ot, quad);
        quad++;
    } while (row < 0xC0);
    rect.x = 0x40;
    rect.w = 0xC0;
    rect.h = 0xD8;
    rect.y = D_8009D7F0 * 0xD8;
    SetDrawMove(&D_8009D164[D_8009D7F0], &rect, 0x280, 0x100);
    addPrim(D_8009BE3C->ot, &D_8009D164[D_8009D7F0]);
    return 1;
}

/* Reset an actor to state 0, step 1. */
s32 func_80081FB4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->u.step = 1;
    return 1;
}

/* Heat-haze strength: set every row (1-3, 5) or scatter random rows (1, 4) per command. */
s32 func_80081FD8(s32 index) {
    WorldmapActor *actor;
    s32 i;
    s32 start;
    u16 *flags;

    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 3;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 4;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 5;
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        for (i = 0, flags = D_8009D148; i < 0xC0; i++) {
            *flags++ = actor->u.value;
        }
        start = rand() & 0x3F;
        flags = &D_8009D148[start];
        for (i = start; i < start + (rand() & 0x1F) + 1; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        start = (rand() & 0x3F) + 0x40;
        flags = &D_8009D148[start];
        for (i = start; i < start + (rand() & 0x1F) + 1; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        start = (rand() & 0x1F) + 0x80;
        flags = &D_8009D148[start];
        for (i = start; i < start + (rand() & 0x1F) + 1; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        break;
    case 2:
        flags = D_8009D148;
        if (++actor->u.step > 0x40) {
            actor->u.step = 0x40;
        }
        for (i = 0; i < 0xC0; i++) {
            *flags++ = actor->u.value;
        }
        break;
    case 3:
        flags = D_8009D148;
        if (--actor->u.step < 2) {
            actor->u.step = 2;
        }
        for (i = 0; i < 0xC0; i++) {
            *flags++ = actor->u.value;
        }
        break;
    case 4:
        flags = D_8009D148;
        for (i = 0; i < 0xC0; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        break;
    case 5:
        for (i = 0, flags = D_8009D148; i < 0xC0; i++) {
            *flags++ = 2;
        }
        break;
    }
    return 1;
}

/* Set up the pulsing-effect scene: fixed start position, music, its camera and five effect slots. */
void func_80082324(void) {
    RECT rect;
    SoundSeq *sequence;
    void *data;
    u16 debug;

    func_80072BB0();
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 2);
    while (func_800286CC() >= 3) {
    }
    func_80076954();
    func_8009766C();
    D_8009BE4C = D_8009A180;
    D_8009CCA4 = 2;
    D_8009D3CC = 0x10;
    D_8009D804 = 0;
    D_8009D144 = 0;
    D_8009CD40 = func_80086700;
    func_80098044();
    func_80028A60(0);
    func_8001B66C();
    D_8009C5AC.vx = 0x70A9000;
    D_8009C5AC.vy = -0x148000;
    D_8009C5AC.vz = 0x42AA000;
    func_80084580();
    func_8008440C();
    func_800979C8();
    func_80072090();
    func_800736DC();
    func_80085F58();
    func_800863E0();
    func_80074E58();
    func_80075030();
    func_800739B8();
    func_80088F64();
    func_80028A60(0);
    D_8006258C = func_80037FD8(D_8009C88C, 0);
    func_80028470(0x24, 0);
    func_80097BC0(&D_8009C5AC);
    do {
        func_800967E4();
        VSync(0);
    } while (func_80096668() > 0);
    debug = D_8005957C & 0x10;
    if (debug) {
        while (debug) {
        }
    }
    func_800320E8(D_8009C88C);
    func_80038428(D_8006259C);
    data = D_8009C884;
    memcpy(D_80062648, data, func_800288EC(D_8009D3D0));
    sequence = func_80039850((SoundSeqHeader *)D_80062648);
    D_80062528 = (s32)sequence;
    func_80039A80(sequence, 0x7F, 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_800827C8, (s32)func_80076B34);
    func_80097718((s32)func_800827EC, (s32)func_800828DC);
    func_80097718((s32)func_80083214, (s32)func_80083264);
    func_80097718((s32)func_80083214, (s32)func_80083264);
    func_80097718((s32)func_80083214, (s32)func_80083264);
    func_80097718((s32)func_80083214, (s32)func_80083264);
    func_80097718((s32)func_80083214, (s32)func_80083264);
    func_80097718((s32)func_800834D0, (s32)func_800834D8);
    func_80097718((s32)func_80076A14, (s32)func_80076A1C);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80085FE0();
    func_80075228();
}

/* Leave the world map for scene 0x269 (flag word 2). */
void func_800826B4(void) {
    func_80039FF8();
    func_8003852C(D_8006259C);
    func_800320E8(D_8006259C);
    func_80084818();
    func_80086124();
    func_80086568();
    func_800866C8();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    func_800320E8(D_8009BBC8[0].ot);
    func_800320E8(D_8009BBC8[1].ot);
    func_800320E8(D_8009BBC8[0].packets);
    func_800320E8(D_8009BBC8[1].packets);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006D634.map = 0x269;
    D_8006D634.entry[2] = 2;
    D_8009BBC4 = 1;
    D_8006D634.entry[0] = D_8009BD38.vy;
}

/* Give an actor its script. */
s32 func_800827C8(s32 index) {
    D_8009BE24[index].u.script = D_8009A758;
    return 1;
}

/* Start a scripted camera on the player: pitch -0x220 at distance 0x50. */
s32 func_800827EC(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    actor->position.vx = D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    actor->position.vy = D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    actor->position.vz = D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->state = 0;
    actor->unk4 = 0;
    actor->unk5C = 0x500000;
    D_8009BD38.vx = -0x220;
    D_8009BD38.vy = 0;
    D_8009BD38.vz = 0;
    actor->motion.vx = actor->u.step = -0x220 << 12;
    D_8009BE0C = 0x78;
    D_8009D144 = 0;
    D_8009D3F0 = 0x500000;
    actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
    actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
    return 1;
}

/* Pulsing-effect scene camera: commands pick camera shots and moves; each state eases distance, pitch and yaw; adds a vertical shake. */
s32 func_800828DC(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;

    actor = &D_8009BE24[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        actor->u.step = actor->motion.vx;
        actor->unk54 = actor->motion.vy;
        actor->unk58 = actor->motion.vz;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0;
        actor->unk5C = 0x3E0000;
        D_8009BD38.vx = -0x20;
        D_8009BD38.vy = 0xD70;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x20 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x3E0000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 2;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 3;
        D_8009D55C.target.vx = 0x7499000;
        D_8009D55C.target.vy = -0x18C000;
        D_8009D55C.target.vz = 0x408A000;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 4;
        break;
    case 6:
        actor->state = 5;
        actor->unk7C = 0x1000;
        actor->unk4 = 0;
        actor->unk5C = 0x320000;
        D_8009BD38.vx = 0x40;
        D_8009BD38.vy = 0xD40;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = 0x40 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x320000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vx = D_8009D55C.target.vx = 0x1379000;
        actor->position.vy = D_8009D55C.target.vy = -0x120000;
        actor->position.vz = D_8009D55C.target.vz = 0x4B2A000;
        break;
    case 7:
        actor->unk4 = 0;
        actor->state = 6;
        break;
    case 8:
        actor->state = 7;
        actor->unk4 = 0;
        actor->unk5C = 0x100000;
        D_8009BD38.vx = -0x90;
        D_8009BD38.vy = -0x510;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x90 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x100000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vx = D_8009D55C.target.vx = 0x7529000;
        actor->position.vy = D_8009D55C.target.vy = -0x18C000;
        actor->position.vz = D_8009D55C.target.vz = 0x2A3A000;
        break;
    case 9:
        actor->unk4 = 0;
        actor->state = 8;
        break;
    case 10:
        actor->state = 9;
        actor->unk4 = 0;
        actor->unk5C = 0x320000;
        D_8009BD38.vx = -0x160;
        D_8009BD38.vy = 0xDE0;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x160 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x320000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vx = D_8009D55C.target.vx = 0x2659000;
        actor->position.vy = D_8009D55C.target.vy = -0x110000;
        actor->position.vz = D_8009D55C.target.vz = 0x6D8A000;
        break;
    case 11:
        actor->state = 10;
        actor->unk4 = 0;
        actor->unk5C = 0x290000;
        D_8009BD38.vx = -0x1A0;
        D_8009BD38.vy = 0x160;
        D_8009BD38.vz = 0;
        actor->motion.vx = actor->u.step = -0x1A0 << 12;
        actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
        D_8009D3F0 = 0x290000;
        actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
        actor->position.vx = D_8009D55C.target.vx = 0x4BA9000;
        actor->position.vy = D_8009D55C.target.vy = -0x288000;
        actor->position.vz = D_8009D55C.target.vz = 0x1DDA000;
        break;
    case 12:
        actor->unk4 = 0;
        actor->state = 11;
        break;
    }
    if (D_8009D144 == 0) {
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->u.step = func_800771D8(actor->u.step, -0x20000, 0x5D17);
        actor->unk54 = func_800771D8(actor->unk54, 0x580000, 0x10000);
        break;
    case 2:
        actor->u.step = func_800771D8(actor->u.step, -0x1F0000, -0x10000);
        actor->unk54 = func_800771D8(actor->unk54, 0x1290000, 0x10000);
        actor->unk5C = func_800771D8(actor->unk5C, 0x490000, 0x10000);
        break;
    case 3:
        actor->u.step = func_800771D8(actor->u.step, -0x10000, 0x10000);
        actor->unk54 = func_800771D8(actor->unk54, 0x1290000, 0x10000);
        actor->unk5C = func_800771D8(actor->unk5C, 0x2B0000, -0x10000);
        break;
    case 4:
        actor->unk7C += 0x200;
        if (actor->unk7C > 0x8000) {
            actor->unk7C = 0x8000;
            actor->state = 0;
        }
        break;
    case 6:
        actor->unk54 = func_800771D8(actor->unk54, 0xF40000, 0x4000);
        break;
    case 8:
        actor->u.step = func_800771D8(actor->u.step, 0x50000, 0x2000);
        actor->unk54 = func_800771D8(actor->unk54, 0x10000, 0x8000);
        break;
    case 11:
        actor->u.step = func_800771D8(actor->u.step, -0x80000, 0x2400);
        actor->unk54 = func_800771D8(actor->unk54, 0x2C0000, 0x2C00);
        actor->unk5C = func_800771D8(actor->unk5C, 0x100000, -0x3200);
        D_8009D55C.target.vx = func_800771D8(D_8009D55C.target.vx, 0x4CD9000, 0x2600);
        D_8009D55C.target.vy = func_800771D8(D_8009D55C.target.vy, -0x15C000, 0x2580);
        D_8009D55C.target.vz = func_800771D8(D_8009D55C.target.vz, 0x1C8A000, -0x2A00);
        func_80093354(&D_8009D55C.target);
        break;
    }
    func_80076DA4(actor, scratch);
    func_80076F54(actor, scratch);
    func_80076FA8(actor, scratch);
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    ((s16 *)D_8009BD40)[1] += scratch->position.vy; /* VIEW_VECTORS[0].vy */
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* Pulse a scene object: spin it, stretch its x scale and bounce its tint between limits. */
void func_80082F64(WorldmapActor *actor, SceneObject *object, ScaleScratch *scratch) {
    switch (actor->state) {
    case 0:
        break;
    case 1:
        scratch->angle.vz = 0;
        scratch->angle.vx = 0;
        scratch->angle.vy = actor->unk68;
        scratch->scale[0].vx = actor->unk70 & 0x7FFF;
        scratch->scale[0].vy = 0x1000;
        scratch->scale[0].vz = func_8003F8B0(actor->unk6C) * 6;
        func_8003F738(&scratch->angle, &scratch->matrix[0]);
        ScaleMatrix(&scratch->matrix[0], &scratch->scale[0]);
        object->matrix = scratch->matrix[0];
        actor->u.step += actor->unk5C;
        actor->unk54 += actor->unk60;
        if (actor->u.step >= 0x100) {
            actor->u.step = 0xFF;
            actor->unk5C = -actor->unk5C;
        } else if (actor->u.step < 0x40) {
            actor->u.step = 0x40;
            actor->unk5C = -actor->unk5C;
        }
        if (actor->unk54 >= 0x100) {
            actor->unk54 = 0xFF;
            actor->unk60 = -actor->unk60;
        } else if (actor->unk54 < 0x80) {
            actor->unk54 = 0x80;
            actor->unk60 = -actor->unk60;
        }
        actor->unk6C = (actor->unk6C + 8) & 0xFFF;
        actor->unk70 += actor->unk74;
        break;
    }
}

/* Build `count` semi-transparent black textured triangles on page 0x2C0,0x100. */
void func_80083108(SceneObject *object, POLY_FT3 *prims, s32 count, s32 abr) {
    s32 i;

    for (i = count - 1; i != -1; i--) {
        ((u8 *)prims)[3] = 7;
        prims->code = 0x24;
        prims->tpage = GetTPage(0, abr, 0x2C0, 0x100);
        prims->r0 = 0;
        prims->g0 = 0;
        prims->b0 = 0;
        prims->code |= 2;
        prims++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT3));
}

/* Set the colour of `count` textured triangles. */
void func_800831D8(POLY_FT3 *prims, s32 count, s32 r, s32 g, s32 b) {
    for (count--; count != -1; count--) {
        setRGB0(prims, r, g, b);
        prims++;
    }
}

/* Rebuild the triangles of scene effect `index`. */
s32 func_80083214(s32 index) {
    SceneObject *object;

    object = &D_8009C620[78 + index];
    func_80083108(object, object->prims, object->def->primitive_count, 3);
    return 1;
}

/* Pulsing effect slot: commands 1/2/4 load one of three settings, 3 stops it; then pulse and tint its object. */
s32 func_80083264(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 *params;
    s32 slot;
    s32 i;

    slot = index - 3;
    actor = &D_8009BE24[index];
    object = &D_8009C620[81 + slot];
    switch (actor->unk4) {
    case 0:
        break;
    case 1:
        params = &actor->u.step;
        actor->state = 1;
        actor->unk4 = 0;
        object->position.vx = D_8009AABC[slot * 14];
        object->position.vy = D_8009AABC[slot * 14 + 1];
        object->position.vz = D_8009AABC[slot * 14 + 2];
        for (i = 3; i < 14; i++) {
            *params++ = D_8009AABC[slot * 14 + i];
        }
        break;
    case 2:
        params = &actor->u.step;
        actor->state = 1;
        actor->unk4 = 0;
        object->position.vx = D_8009AB48[slot * 14];
        object->position.vy = D_8009AB48[slot * 14 + 1];
        object->position.vz = D_8009AB48[slot * 14 + 2];
        for (i = 3; i < 14; i++) {
            *params++ = D_8009AB48[slot * 14 + i];
        }
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 0;
        break;
    case 4:
        params = &actor->u.step;
        actor->state = 1;
        actor->unk4 = 0;
        object->position.vx = D_8009ABD4[slot * 14];
        object->position.vy = D_8009ABD4[slot * 14 + 1];
        object->position.vz = D_8009ABD4[slot * 14 + 2];
        for (i = 3; i < 14; i++) {
            *params++ = D_8009ABD4[slot * 14 + i];
        }
        break;
    }
    func_80082F64(actor, object, (ScaleScratch *)0x1F800000);
    func_800831D8((&object->prims)[D_8009D7F0], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* Mode step that has nothing to do; always reports done. */
s32 func_800834D0(void) {
    return 1;
}

/* On command, reload the terrain around the player and drain the frames. */
s32 func_800834D8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        DrawSync(0);
        VSync(0);
        func_80097D64();
        func_80097BC0(&D_8009C5AC);
        do {
            func_800967E4();
            VSync(0);
        } while (func_80096668() > 0);
    }
    return 1;
}

/* Set up the effect scene: fixed start position, its director and effect actors. */
void func_8008355C(void) {
    RECT rect;

    func_80072BB0();
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 2);
    while (func_800286CC() >= 3) {
    }
    func_80076954();
    func_8009766C();
    D_8009BE4C = D_8009A180;
    D_8009CCA4 = 2;
    D_8009D3CC = 0x10;
    D_8009D804 = 0;
    D_8009D144 = 0;
    D_8009CD40 = func_80086700;
    func_80098044();
    func_80028A60(0);
    func_800721E4();
    D_8009C5AC.vx = 0x1800000;
    D_8009C5AC.vy = -0x100000;
    D_8009C5AC.vz = 0x1A00000;
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
    func_80097718((s32)func_800838E8, (s32)func_80076B34);
    func_80097718((s32)func_8008390C, (s32)func_80083A00);
    func_80097718((s32)func_80083FE4, (s32)func_80084068);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80075228();
}

/* Leave the world map for scene 0x269 (flag word 4). */
void func_800837DC(void) {
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
    func_800320E8(D_8009BBC8[0].ot);
    func_800320E8(D_8009BBC8[1].ot);
    func_800320E8(D_8009BBC8[0].packets);
    func_800320E8(D_8009BBC8[1].packets);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006D634.map = 0x269;
    D_8006D634.entry[2] = 4;
    D_8009BBC4 = 1;
    D_8006D634.entry[0] = D_8009BD38.vy;
}

/* Give an actor its script. */
s32 func_800838E8(s32 index) {
    D_8009BE24[index].u.script = D_8009AC60;
    return 1;
}

/* Start a scripted camera on the player: pitch -0x20, yaw 0x400 at distance 0x96. */
s32 func_8008390C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    actor->position.vx = D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    actor->position.vy = D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    actor->position.vz = D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->state = 0;
    actor->unk4 = 0;
    actor->unk5C = 0x960000;
    D_8009BD38.vx = -0x20;
    D_8009BD38.vy = 0x400;
    D_8009BD38.vz = 0;
    actor->motion.vx = actor->u.step = -0x20 << 12;
    D_8009BE0C = 0x78;
    D_8009D144 = 0;
    D_8009D3F0 = 0x960000;
    actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
    actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
    return 1;
}
