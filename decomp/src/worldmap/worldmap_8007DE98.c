#include "worldmap.h"

/* Actor handlers this unit installs by address (see func_80097718). */
s32 func_8008032C();
s32 func_80080370();
s32 func_80080578();
s32 func_80080600();
s32 func_80080900();
s32 func_80080944();
s32 func_80080A28();
s32 func_80080AC4();
s32 func_80076A14();
s32 func_80076A1C();

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007DE98);

/* Start a scripted camera at the player position (step 0, speed 0x40). */
s32 func_8007E450(s32 index) {
    WorldmapActor *actor;

    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BE0C = 0x78;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk4 = 1;
    actor->unk58 = 0x40;
    actor->state = 0;
    actor->u.step = 0;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007E4E4);

/* Set up `count` translucent blue textured quads of a scene object and copy them to its second buffer. */
#ifdef NON_MATCHING /* loop pointer biased to b0 instead of the code byte */
void func_8007EBBC(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x300, 0x100);
        quads->clut = GetClut(0, 0x1FF);
        setRGB0(quads, 0x3C, 0x3C, 0xC0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007EBBC);
#endif

/* Start the flight: link objects 2-3 to 1, build their quads, hide 1 and place the actor behind the player on its entry path. */
#ifdef NON_MATCHING /* motion reload not hoisted into the load delay slot */
s32 func_8007ECA4(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    func_800848B4(1, 2);
    func_800848B4(1, 3);
    objects = D_8009C620;
    actor = &D_8009BE24[index];
    func_8007EBBC(&objects[1], objects[1].prims, objects[1].def->count, 3);
    func_8007EBBC(&objects[2], objects[2].prims, objects[2].def->count, 3);
    func_8007EBBC(&objects[3], objects[3].prims, objects[3].def->count, 1);
    D_8009C620[1].visible = 0;
    D_8009C620[1].angle.vz = 0;
    D_8009C620[1].angle.vy = 0;
    D_8009C620[1].angle.vx = 0;
    func_8004A92C(&D_8009C620[1].angle, &D_8009C620[1].matrix);
    actor->motion.vx = -0x85A;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->motion.vz = 0xDA6;
    actor->u.step = D_8009A674[D_8009D3D4].vx << 12;
    actor->unk54 = D_8009A674[D_8009D3D4].vz << 12;
    actor->position.vx = D_8009C5AC.vx - actor->motion.vx * 0x3680;
    actor->position.vy = D_8009C5AC.vy;
    actor->position.vz = D_8009C5AC.vz - actor->motion.vz * 0x3680;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007ECA4);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007EE34);

/* Build scene object `index` and start its fall. */
s32 func_8007F8AC(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 slot;
    s32 abr;

    slot = index - 4;
    actor = &D_8009BE24[index];
    object = &D_8009C620[slot + 4];
    abr = 3;
    if (slot == 4) {
        abr = 1;
    }
    func_8007EBBC(object, object->prims, object->def->count, abr);
    actor->motion.vx = -0x85A;
    actor->motion.vz = 0xDA6;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->unk5C = D_8009A68C[slot];
    actor->wait = 0x3C;
    return 1;
}

/* Exhaust flame on scene object `index`: commands 1-5 stop, start or restart
 * it; it follows actor 3, emits effects 0x23/0x24 and shrinks away; done (3)
 * once its size runs out. */
s32 func_8007F968(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *leader;
    SceneObject *object;
    FlameScratch *scratch;
    s32 unused[2]; /* unreferenced; the original frame reserves it */

    actor = &D_8009BE24[index];
    leader = &D_8009BE24[3];
    object = &D_8009C620[index];
    scratch = (FlameScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 3:
        actor->wait = 0x3C;
        actor->motion.vx = -0x85A;
        actor->motion.vz = 0xDA6;
        actor->unk4 = 0;
        actor->state = 0;
        actor->motion.vy = 0;
        actor->unk5C = D_8009A684[index];
        object->visible = 0;
        object->angle.vx = object->angle.vy = object->angle.vz = 0;
        func_8004A92C(&object->angle, &object->matrix);
        func_800894C8(0x23);
        func_800894C8(0x24);
        break;
    case 4:
        actor->wait = 4;
        actor->state = 1;
        actor->unk4 = 0;
        actor->unk5C = D_8009A684[index];
        object->visible = 0;
        object->angle.vx = object->angle.vy = object->angle.vz = 0;
        func_8004A92C(&object->angle, &object->matrix);
        func_800894C8(0x23);
        func_800894C8(0x24);
        break;
    case 5:
        actor->state = 1;
        actor->unk4 = 0;
        actor->wait = 0x1E;
        break;
    }
    switch (actor->state) {
    case 0:
        actor->position.vx = leader->position.vx;
        actor->position.vy = leader->position.vy;
        actor->position.vz = leader->position.vz;
        scratch->position.vx = (actor->position.vx + 0x42D000) >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = (actor->position.vz - 0x6D3000) >> 12;
        scratch->angle.vx = scratch->angle.vz = 0;
        scratch->angle.vy = ratan2(0x85A, 0xDA6) & 0xFFF;
        func_80089160(0x23, &scratch->position, &scratch->angle);
        func_80089160(0x24, &scratch->position, &scratch->angle);
        break;
    case 1:
        if (--actor->wait < 0) {
            actor->wait = 0;
            actor->unk5C -= 0x80;
        }
        break;
    }
    scratch->base.m[0][0] = 0xDA6;
    scratch->base.m[0][1] = 0;
    scratch->base.m[0][2] = 0x85A;
    scratch->base.m[1][0] = 0;
    scratch->base.m[1][1] = -0x1000;
    scratch->base.m[1][2] = 0;
    scratch->base.m[2][0] = -0x85A;
    scratch->base.m[2][1] = 0;
    scratch->base.m[2][2] = 0xDA6;
    object->angle.vz = (object->angle.vz + 0x100) & 0xFFF;
    func_8004A92C(&object->angle, &scratch->rotation);
    MulMatrix0(&scratch->base, &scratch->rotation, &object->matrix);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    scratch->scale.vx = scratch->scale.vy = actor->unk5C;
    scratch->scale.vz = 0x2000;
    ScaleMatrix(&object->matrix, &scratch->scale);
    if (actor->unk5C < 0) {
        actor->unk5C = 0;
        object->visible = 1;
        return 3;
    }
    return 1;
}

/* Rebuild scene objects 9 and 10 and start a descent at a fixed point. */
#ifdef NON_MATCHING /* return value loaded before the stores */
s32 func_8007FC8C(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    func_8007A06C(&objects[9], objects[9].prims, objects[9].def->count);
    func_8007A06C(&objects[10], objects[10].prims, objects[10].def->count);
    actor->position.vx = 0x1498000;
    actor->position.vy = -0x80000;
    actor->position.vz = 0x4AF2000;
    actor->state = 0;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    return 3;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FC8C);
#endif

/* Grow and fade scene objects 9 and 10 at the actor; ends the step when faded out. */
#ifdef NON_MATCHING /* the two object pointers swap s2/s3 */
s32 func_8007FD30(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;
    SceneObject *object;
    SceneObject *object2;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    object = &objects[9];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
    }
    objects[9].position.vx = objects[10].position.vx = actor->position.vx >> 12;
    objects[9].position.vy = objects[10].position.vy = actor->position.vy >> 12;
    objects[9].position.vz = objects[10].position.vz = actor->position.vz >> 12;
    objects[10].matrix = D_8009A180;
    objects[9].matrix = objects[10].matrix;
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&objects[9].matrix, &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&objects[10].matrix, &SCALE_SCRATCH->scale[1]);
    object2 = &objects[10];
    if ((actor->u.step += 0x180) > 0x7FFF) {
        actor->u.step = 0x7FFF;
    }
    if ((actor->unk54 += 0x180) > 0x7FFF) {
        actor->unk54 = 0x7FFF;
    }
    func_800809EC((&object->prims)[D_8009D7F0], objects[9].def->count, actor->unk58, actor->unk58, actor->unk58);
    func_800809EC((&object2->prims)[D_8009D7F0], objects[10].def->count, actor->unk58, actor->unk58, actor->unk58);
    if ((actor->unk58 -= 3) < 0) {
        actor->unk58 = 0;
        return 3;
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007DE98", func_8007FD30);
#endif

/* Set up the third cutscene mode: display, terrain loader, scene objects and
 * its actors. */
void func_8007FF70(void) {
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
    D_8009C5AC.vx = 0x4100000;
    D_8009C5AC.vy = -0x80000;
    D_8009C5AC.vz = 0x13C0000;
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
    func_80028A60(0);
    func_80038428(D_8006259C);
    func_80028470(0x24, 0);
    func_80097BC0(&D_8009C5AC);
    do {
        func_800967E4();
        VSync(0);
    } while (func_80096668() > 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_8008032C, (s32)func_80080370);
    func_80097718((s32)func_80080578, (s32)func_80080600);
    func_80097718((s32)func_80080900, (s32)func_80080944);
    func_80097718((s32)func_80080A28, (s32)func_80080AC4);
    func_80097718((s32)func_80076A14, (s32)func_80076A1C);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80085FE0();
    func_80075228();
}

/* Leave the world map for scene 0x84 (flag word 2). */
void func_80080218(void) {
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
    D_8006F94E = 0x84;
    D_8006F954[0] = 2;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Restart an actor's timed sequence at its first step. */
s32 func_8008032C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A698[0];
    actor->wait = D_8009A6AC[actor->u.step];
    return 1;
}
