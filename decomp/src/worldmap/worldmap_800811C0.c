#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800811C0);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081470);

/* Build `count` semi-transparent textured quads on page 0x180,0. */
void func_800816DC(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x180, 0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}

/* Start the actor above the player and build scene object 2 there. */
#ifdef NON_MATCHING /* scene-object pointer loaded at a different point */
s32 func_800817A0(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position.vx = D_8009C5AC.vx;
    objects = D_8009C620;
    actor->position.vy = D_8009C5AC.vy - 0x100000;
    actor->u.step = 0;
    actor->unk54 = 0;
    actor->unk58 = 0;
    actor->position.vz = D_8009C5AC.vz;
    func_800816DC(&objects[2], objects[2].prims, objects[2].def->count, 1);
    objects[2].position.vx = actor->position.vx >> 12;
    objects[2].position.vy = actor->position.vy >> 12;
    objects[2].position.vz = actor->position.vz >> 12;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800817A0);
#endif

/* Fade scene object 2 in (command 1) or reset it to opaque grey (command 2). */
#ifdef NON_MATCHING /* loop pointer biased to b0 instead of the code byte */
s32 func_80081868(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    PolyFT4 *quad;
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
        for (i = 0; i < object->def->count; i++) {
            setRGB0(quad, 0x80, 0x80, 0x80);
            setSemiTrans(quad, 0);
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
    func_800809EC((&object->prims)[D_8009D7F0], object->def->count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081868);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800819C8);

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
    func_800809EC((&object->prims)[D_8009D7F0], object->def->count, actor->u.step, actor->unk54, actor->unk58);
    object++;
    func_800809EC((&object->prims)[D_8009D7F0], object->def->count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* Allocate the shared quad pool (two display copies) and mark every quad free. */
s32 func_80081C3C(void) {
    PolyFT4 *quads;
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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081D80);

/* Reset an actor to state 0, step 1. */
s32 func_80081FB4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->u.step = 1;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80081FD8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80082324);

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
    func_800320E8(D_8009BC38[0]);
    func_800320E8(D_8009BCB0[0]);
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006F94E = 0x269;
    D_8006F954[0] = 2;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Give an actor its script. */
s32 func_800827C8(s32 index) {
    D_8009BE24[index].u.script = D_8009A758;
    return 1;
}

/* Start a scripted camera on the player: pitch -0x220 at distance 0x50. */
#ifdef NON_MATCHING /* actor pointer and distance constant swap registers */
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
    D_8009D144 = 0;
    D_8009D3F0 = 0x500000;
    actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
    D_8009BE0C = 0x78;
    actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800827EC);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_800828DC);

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
void func_80083108(SceneObject *object, PolyFT3 *prims, s32 count, s32 abr) {
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
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT3));
}

/* Set the colour of `count` textured triangles. */
void func_800831D8(PolyFT3 *prims, s32 count, s32 r, s32 g, s32 b) {
    for (count--; count != -1; count--) {
        setRGB0(prims, r, g, b);
        prims++;
    }
}

/* Rebuild the triangles of scene effect `index`. */
s32 func_80083214(s32 index) {
    SceneObject *object;

    object = &D_8009C620[78 + index];
    func_80083108(object, object->prims, object->def->count, 3);
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_80083264);

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

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_8008355C);

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
    func_800320E8(D_8009BC38[0]);
    func_800320E8(D_8009BCB0[0]);
    func_800320E8(D_8009BC38[1]);
    func_800320E8(D_8009BCB0[1]);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006F94E = 0x269;
    D_8006F954[0] = 4;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Give an actor its script. */
s32 func_800838E8(s32 index) {
    D_8009BE24[index].u.script = D_8009AC60;
    return 1;
}

/* Start a scripted camera on the player: pitch -0x20, yaw 0x400 at distance 0x96. */
#ifdef NON_MATCHING /* actor pointer and distance constant swap registers */
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
    D_8009D144 = 0;
    D_8009D3F0 = 0x960000;
    actor->motion.vy = actor->unk54 = D_8009BD38.vy << 12;
    D_8009BE0C = 0x78;
    actor->motion.vz = actor->unk58 = D_8009BD38.vz << 12;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_800811C0", func_8008390C);
#endif
