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

/* Scene director: state 1 steps a timed state sequence (states at unk54,
 * durations at unk58); the other states start sounds and actor commands and
 * return to 1. */
s32 func_8007DE98(s32 index) {
    WorldmapActor *actor;
    s32 unused[4]; /* unreferenced; the original frame reserves it */

    actor = &D_8009BE24[index];
    switch (actor->state) {
    case 0:
        break;
    case 1:
        if (--actor->wait < 0) {
            actor->state = ((u16 *)actor->unk54)[actor->u.step];
            actor->wait = ((u16 *)actor->unk58)[actor->u.step];
            actor->u.step++;
        }
        break;
    case 2:
        func_80039E60((D_8006259C->id << 16) | 0x1C);
        func_80039E60((D_8006259C->id << 16) | 0x1D);
        func_80039E60((D_8006259C->id << 16) | 0x1E);
        func_80097770(2, 2);
        func_80097770(3, 2);
        func_80097770(4, 3);
        func_80097770(5, 3);
        func_80097770(6, 3);
        func_80097770(7, 3);
        func_80097770(8, 3);
        actor->state = 1;
        break;
    case 3:
        func_80089514(0x22);
        func_80089514(0x23);
        func_80089514(0x24);
        func_80097770(2, 3);
        func_80097770(3, 3);
        func_80097770(4, 3);
        func_80097770(5, 3);
        func_80097770(6, 3);
        func_80097770(7, 3);
        func_80097770(8, 3);
        actor->state = 1;
        break;
    case 4:
        func_80097770(2, 4);
        func_80097770(3, 4);
        func_80097770(4, 3);
        func_80097770(5, 3);
        func_80097770(6, 3);
        func_80097770(7, 3);
        func_80097770(8, 3);
        actor->state = 1;
        break;
    case 5:
        func_80097770(2, 5);
        actor->state = 1;
        break;
    case 6:
        func_80097770(0, 0xD);
        D_8009CCA4 = 1;
        D_8009D3CC = 0x40;
        actor->state = 1;
        break;
    case 7:
        func_80097770(3, 5);
        func_80097770(4, 4);
        func_80097770(5, 4);
        func_80097770(6, 4);
        func_80097770(7, 4);
        func_80097770(8, 4);
        func_80097770(0, 0xC);
        D_8009CCA4 = 1;
        D_8009D3CC = 0x40;
        actor->state = 1;
        break;
    case 8:
        func_80097770(2, 6);
        actor->state = 1;
        break;
    case 9:
        func_80097770(0, 0xD);
        D_8009CCA4 = 1;
        D_8009D3CC = 0x80;
        actor->state = 1;
        break;
    case 10:
        func_80097770(0, 0xC);
        D_8009CCA4 = 1;
        D_8009D3CC = 0x80;
        func_80097770(9, 1);
        func_80097770(2, 7);
        actor->state = 1;
        break;
    case 16:
        func_80097770(2, 4);
        func_80097770(3, 0x10);
        func_80097770(4, 3);
        func_80097770(5, 3);
        func_80097770(6, 3);
        func_80097770(7, 3);
        func_80097770(8, 3);
        actor->state = 1;
        break;
    case 17:
        func_80097770(2, 0x10);
        actor->state = 1;
        break;
    case 18:
        func_80097770(2, 0x11);
        actor->state = 1;
        break;
    case 24:
        func_80097770(2, 4);
        func_80097770(3, 0x18);
        func_80097770(4, 3);
        func_80097770(5, 3);
        func_80097770(6, 3);
        func_80097770(7, 3);
        func_80097770(8, 3);
        actor->state = 1;
        break;
    case 25:
        func_80097770(2, 0x18);
        actor->state = 1;
        break;
    case 61:
        func_80039E60((D_8006259C->id << 16) | D_8009A5A0[D_8009D3D4][0]);
        func_80039E60((D_8006259C->id << 16) | D_8009A5A0[D_8009D3D4][1]);
        func_80039E60((D_8006259C->id << 16) | D_8009A5A0[D_8009D3D4][2]);
        actor->state = 1;
        break;
    case 62:
        func_80039E60((D_8006259C->id << 16) | 0x19);
        func_80039E60((D_8006259C->id << 16) | 0x1A);
        func_80039E60((D_8006259C->id << 16) | 0x1B);
        actor->state = 1;
        break;
    case 63:
        func_80097770(0, 0xD);
        D_8009CCA4 = 2;
        D_8009D3CC = 4;
        actor->state = 1;
        break;
    case 64:
        D_8009D554 = 0;
        D_8009D7CC = 0;
        actor->state = 0;
        break;
    }
    return 1;
}

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

/* Scene camera director: commands set up camera shots; the states fly the
 * camera target across the map, zoom and shake the view by unk7C. */
s32 func_8007E4E4(s32 index) {
    WorldmapActor *actor;
    CameraScratch *scratch;
    s32 shake;

    actor = &D_8009BE24[index];
    scratch = (CameraScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        D_8009D3F0 = 0x500000;
        actor->unk4 = 0;
        D_8009BD38.vx = 0x150;
        D_8009BD38.vy = 0xF50;
        D_8009BD38.vz = 0;
        break;
    case 2:
        D_8009D3F0 = 0x4D0000;
        actor->unk4 = 0;
        D_8009BD38.vx = 0xC60;
        D_8009BD38.vy = 0x380;
        D_8009BD38.vz = 0;
        break;
    case 3:
        D_8009D3F0 = 0x400000;
        actor->unk4 = 0;
        D_8009BD38.vx = 0x30;
        D_8009BD38.vy = 0x160;
        D_8009BD38.vz = 0;
        break;
    case 4:
        D_8009D3F0 = 0x400000;
        actor->unk4 = 0;
        D_8009BD38.vx = 0x30;
        D_8009BD38.vy = 0x960;
        D_8009BD38.vz = 0;
        break;
    case 5:
        actor->state = 1;
        actor->unk4 = 0;
        actor->unk5C = -0x8000;
        break;
    case 6:
        actor->state = 3;
        actor->unk7C = 0x1000;
        D_8009D3F0 = 0x200000;
        actor->unk4 = 0;
        D_8009BD38.vx = -0x58;
        D_8009BD38.vy = 0xE58;
        D_8009BD38.vz = 0;
        break;
    case 7:
        actor->state = 5;
        actor->unk4 = 0;
        actor->unk7C = 0x40000;
        break;
    case 16:
        actor->state = 0x10;
        actor->unk4 = 0;
        actor->unk5C = -0x8000;
        break;
    case 17:
        actor->state = 0x10;
        actor->unk7C = 0x1000;
        D_8009D3F0 = 0x400000;
        actor->unk4 = 0;
        D_8009BD38.vx = 0xE0;
        D_8009BD38.vy = 0x798;
        D_8009BD38.vz = 0;
        break;
    case 24:
        actor->state = 0x18;
        actor->unk4 = 0;
        actor->unk5C = -0x8000;
        break;
    }
    if (D_8009D144 == 0) {
        func_80093354(&actor->position);
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        D_8009BE28.target.vx -= 0x42D00;
        D_8009BE28.target.vz += 0x6D300;
        if (D_8009BE28.target.vx < 0x1F9E000 && D_8009BE28.target.vz > 0x5998000) {
            D_8009BE28.target.vx = 0x1F9E000;
            D_8009BE28.target.vz = 0x5998000;
            actor->state = 2;
            actor->unk7C = 0x10000;
        } else {
            GROUND_SCROLL[0] -= 0x42D00;
            GROUND_SCROLL[2] += 0x6D300;
        }
        func_80093354(&actor->position);
        D_8009BD38.vy -= 8;
        D_8009BD38.vx += actor->unk5C >> 12;
        actor->unk5C += 0x300;
        D_8009D3F0 += 0x8000;
        if (D_8009BD38.vx > 0x80) {
            D_8009BD38.vx = 0x80;
        }
        break;
    case 2:
        actor->unk7C -= 0x80;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
        }
        break;
    case 3:
        D_8009BE28.target.vx -= 0x4C400;
        D_8009BE28.target.vy += 0xFF80;
        D_8009BE28.target.vz -= 0x65580;
        if (D_8009BE28.target.vx < 0x1498000 && D_8009BE28.target.vz < 0x4AF2000) {
            D_8009BE28.target.vx = 0x1498000;
            D_8009BE28.target.vz = 0x4AF2000;
            actor->state = 4;
            actor->unk5C = 0x20000;
        } else {
            GROUND_SCROLL[0] -= 0x4C400;
            GROUND_SCROLL[2] -= 0x65580;
        }
        func_80093354(&actor->position);
        D_8009D3F0 += 0x20000;
        D_8009BD38.vy += 0x20;
        D_8009BD38.vx -= 4;
        break;
    case 4:
        D_8009BD38.vy += actor->unk5C >> 12;
        actor->unk5C -= 0x800;
        if (actor->unk5C < 0) {
            actor->unk5C = 0;
            actor->state = 0;
        }
        break;
    case 5:
        actor->unk7C -= 0x2000;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
            actor->state = 0;
        }
        break;
    case 16:
        D_8009BE28.target.vx -= 0x42D00;
        D_8009BE28.target.vz += 0x6D300;
        if (D_8009BE28.target.vx < 0x1F9E000 && D_8009BE28.target.vz > 0x5998000) {
            D_8009BE28.target.vx = 0x1F9E000;
            D_8009BE28.target.vz = 0x5998000;
            actor->state = 0x11;
            actor->unk7C = 0x8000;
        } else {
            GROUND_SCROLL[0] -= 0x42D00;
            GROUND_SCROLL[2] += 0x6D300;
        }
        func_80093354(&actor->position);
        D_8009BD38.vy -= 8;
        D_8009BD38.vx += actor->unk5C >> 12;
        actor->unk5C += 0x300;
        D_8009D3F0 += 0x8000;
        if (D_8009BD38.vx > 0x80) {
            D_8009BD38.vx = 0x80;
        }
        break;
    case 17:
        actor->unk7C -= 0x200;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
        }
        break;
    case 24:
        D_8009BE28.target.vx -= 0x42D00;
        D_8009BE28.target.vz += 0x6D300;
        if (D_8009BE28.target.vx < 0x2152000 && D_8009BE28.target.vz > 0x5AA3000) {
            D_8009BE28.target.vx = 0x2152000;
            D_8009BE28.target.vz = 0x5AA3000;
            actor->state = 0x19;
            actor->unk60 = 0x10000;
        } else {
            GROUND_SCROLL[0] -= 0x42D00;
            GROUND_SCROLL[2] += 0x6D300;
        }
        func_80093354(&actor->position);
        D_8009BD38.vy -= 8;
        D_8009BD38.vx += actor->unk5C >> 12;
        actor->unk5C += 0x300;
        D_8009D3F0 += 0x8000;
        if (D_8009BD38.vx > 0x80) {
            D_8009BD38.vx = 0x80;
        }
        break;
    case 25:
        D_8009BD38.vy -= actor->unk60 >> 12;
        actor->unk60 -= 0x200;
        if (actor->unk60 < 0) {
            actor->unk60 = 0;
        }
        break;
    }
    shake = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    scratch->view.vy = shake;
    ((s16 *)D_8009BD40)[1] += shake; /* VIEW_VECTORS[0].vy */
    VIEW_VECTORS[1].vy += scratch->view.vy;
    return 1;
}

/* Set up `count` translucent blue textured quads of a scene object and copy them to its second buffer. */
void func_8007EBBC(SceneObject *object, PolyFT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x300, 0x100);
        quads->clut = GetClut(0, 0x1FF);
        setSemiTrans(quads, 1);
        setRGB0(quads, 0x3C, 0x3C, 0xC0);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(PolyFT4));
}

/* Start the flight: link objects 2-3 to 1, build their quads, hide 1 and place the actor behind the player on its entry path. */
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

/* Flying vehicle (scene object 1): commands place it on its approach track;
 * it flies along its motion vector, stops at the landing point, trails
 * effect 0x22 and faces its direction. */
s32 func_8007EE34(s32 index) {
    s32 result;
    WorldmapActor *actor;
    SceneObject *object;
    TrackScratch *scratch;

    result = 1;
    actor = &D_8009BE24[index];
    object = &D_8009C620[1];
    scratch = (TrackScratch *)0x1F800000;
    if (actor->unk4 != 0) {
        actor->motion.vx = -0x85A;
        actor->motion.vy = 0;
        actor->motion.vz = 0xDA6;
        actor->u.step = D_8009A674[D_8009D3D4].vx << 12;
        actor->unk54 = D_8009A674[D_8009D3D4].vz << 12;
    }
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 0;
        actor->position.vx = D_8009C5AC.vx - actor->motion.vx * 0x3680;
        actor->position.vy = D_8009C5AC.vy;
        actor->position.vz = D_8009C5AC.vz - actor->motion.vz * 0x3680;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0;
        actor->position.vx = D_8009C5AC.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x2D00;
        actor->position.vz = D_8009C5AC.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x2D00;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 0;
        actor->position.vx = D_8009C5AC.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x1E00;
        actor->position.vz = D_8009C5AC.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x1E00;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 1;
        actor->position.vx = D_8009C5AC.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x3300;
        actor->position.vz = D_8009C5AC.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x3300;
        break;
    case 5:
        actor->state = 2;
        actor->motion.vx = -0x988;
        actor->motion.vy = 0x227;
        actor->unk4 = 0;
        actor->motion.vz = -0xCAB;
        actor->position.vx = 0x1F9E000;
        actor->position.vz = 0x5998000;
        break;
    case 16:
        actor->unk4 = 0;
        actor->state = 0x10;
        actor->position.vx = actor->motion.vx * 0x3300 + 0x56F2000;
        actor->position.vz = actor->motion.vz * 0x3300 + 0x7F2C000;
        break;
    case 24:
        actor->unk4 = 0;
        actor->state = 0x18;
        actor->position.vx = D_8009C5AC.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x3300;
        actor->position.vz = D_8009C5AC.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x3300;
        break;
    }
    actor->position.vx += actor->motion.vx << 7;
    actor->position.vy += actor->motion.vy << 7;
    actor->position.vz += actor->motion.vz << 7;
    switch (actor->state) {
    case 0:
        if (actor->position.vx < actor->u.step && actor->position.vz > actor->unk54) {
            actor->position.vx = actor->u.step;
            actor->position.vz = actor->unk54;
            func_80097770(4, 2);
            func_80097770(5, 2);
            func_80097770(6, 2);
            func_80097770(7, 2);
            func_80097770(8, 2);
        }
        break;
    case 1:
        if (actor->position.vx < 0x1F9E000 && actor->position.vz > 0x5998000) {
            actor->position.vx = 0x1F9E000;
            actor->position.vz = 0x5998000;
            actor->state = 0x40;
            scratch->position.vx = actor->position.vx >> 12;
            scratch->position.vy = actor->position.vy >> 12;
            scratch->position.vz = actor->position.vz >> 12;
            func_80089160(0x20, &scratch->position, NULL);
        }
        break;
    case 2:
        if (actor->position.vy > -0x80000) {
            scratch->position.vx = 0x1498;
            scratch->position.vz = 0x4AF2;
            scratch->position.vy = func_80093978(0x1498000, 0x4AF2000) >> 12;
            func_80089160(0x25, &scratch->position, NULL);
            func_80089160(0x26, &scratch->position, NULL);
            func_80089160(0x27, &scratch->position, NULL);
            actor->state = 3;
        }
        break;
    case 3:
        if (actor->position.vy > 0x100000) {
            actor->position.vx -= actor->motion.vx << 7;
            actor->position.vy -= actor->motion.vy << 7;
            actor->position.vz -= actor->motion.vz << 7;
            object[0].visible = object[1].visible = object[2].visible = 1;
            actor->state = 0x41;
        }
        break;
    case 16:
        if (actor->position.vx < 0x1F9E000 && actor->position.vz > 0x5998000) {
            actor->state = 0x11;
            scratch->position.vx = 0x1F9E;
            scratch->position.vz = 0x5998;
            scratch->position.vy = actor->position.vy >> 12;
            func_80089160(0x20, &scratch->position, NULL);
            func_80089160(0x21, &scratch->position, NULL);
        }
        break;
    case 17:
        if (actor->position.vx < 0x199D000 && actor->position.vz > 0x6367000) {
            actor->position.vx = 0x199D000;
            actor->position.vz = 0x6367000;
            object[0].visible = object[1].visible = object[2].visible = 1;
            func_80097770(4, 5);
            func_80097770(5, 5);
            func_80097770(6, 5);
            func_80097770(7, 5);
            func_80097770(8, 5);
            result = 3;
        }
        break;
    case 24:
        if (actor->position.vx < 0x1EB5000 && actor->position.vz > 0x5EE6000) {
            object[0].visible = object[1].visible = object[2].visible = 1;
            func_80097770(4, 2);
            func_80097770(5, 2);
            func_80097770(6, 2);
            func_80097770(7, 2);
            func_80097770(8, 2);
            actor->state = 0x41;
        }
        break;
    case 0x40:
        actor->position.vx = 0x1F9E000;
        actor->position.vz = 0x5998000;
        break;
    case 0x41:
        result = 3;
        break;
    }
    func_80093354(&actor->position);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    switch (actor->state) {
    case 0:
    case 1:
    case 16:
    case 24:
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
        scratch->axis[0].vx = scratch->axis[0].vy = 0x800;
        scratch->axis[0].vz = 0x1800;
        ScaleMatrix(&object->matrix, &scratch->axis[0]);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->angle.vx = scratch->angle.vz = 0;
        scratch->angle.vy = ratan2(0x85A, 0xDA6) & 0xFFF;
        func_80089160(0x22, &scratch->position, &scratch->angle);
        break;
    case 4:
    case 17:
    case 0x41:
        func_800894C8(0x22);
        break;
    case 2:
    case 3:
        scratch->axis[0].vx = 0x988;
        scratch->axis[0].vy = -0x227;
        scratch->axis[0].vz = -0xCAB;
        scratch->axis[1].vx = scratch->axis[1].vz = 0;
        scratch->axis[1].vy = 0x1000;
        func_8004A480(&scratch->axis[1], &scratch->axis[0], &scratch->axis[2]);
        VectorNormal(&scratch->axis[2], &scratch->axis[2]);
        func_8004A480(&scratch->axis[0], &scratch->axis[2], &scratch->axis[1]);
        VectorNormal(&scratch->axis[1], &scratch->axis[1]);
        scratch->frame.m[0][0] = scratch->axis[2].vx;
        scratch->frame.m[0][1] = scratch->axis[2].vy;
        scratch->frame.m[0][2] = scratch->axis[2].vz;
        scratch->frame.m[1][0] = scratch->axis[1].vx;
        scratch->frame.m[1][1] = scratch->axis[1].vy;
        scratch->frame.m[1][2] = scratch->axis[1].vz;
        scratch->frame.m[2][0] = scratch->axis[0].vx;
        scratch->frame.m[2][1] = scratch->axis[0].vy;
        scratch->frame.m[2][2] = scratch->axis[0].vz;
        func_80097070(&scratch->frame, &scratch->angle);
        func_8004A8EC(&scratch->frame, &scratch->base);
        object->angle.vz = (object->angle.vz + 0x100) & 0xFFF;
        func_8004A92C(&object->angle, &scratch->rotation);
        MulMatrix0(&scratch->base, &scratch->rotation, &object->matrix);
        scratch->axis[0].vx = scratch->axis[0].vy = 0x800;
        scratch->axis[0].vz = 0x1800;
        ScaleMatrix(&object->matrix, &scratch->axis[0]);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        func_80089160(0x22, &scratch->position, &scratch->angle);
        break;
    }
    return result;
}

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
s32 func_8007FC8C(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    func_8007A06C(&objects[9], objects[9].prims, objects[9].def->count);
    func_8007A06C(&objects[10], objects[10].prims, objects[10].def->count);
    actor->state = 0;
    actor->position.vx = 0x1498000;
    actor->position.vy = -0x80000;
    actor->position.vz = 0x4AF2000;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    return 3;
}

/* Grow and fade scene objects 9 and 10 at the actor; ends the step when faded out. */
s32 func_8007FD30(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    object = &D_8009C620[9];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
    }
    object[0].position.vx = object[1].position.vx = actor->position.vx >> 12;
    object[0].position.vy = object[1].position.vy = actor->position.vy >> 12;
    object[0].position.vz = object[1].position.vz = actor->position.vz >> 12;
    object[1].matrix = D_8009A180;
    object[0].matrix = object[1].matrix;
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&object[0].matrix, &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&object[1].matrix, &SCALE_SCRATCH->scale[1]);
    if ((actor->u.step += 0x180) > 0x7FFF) {
        actor->u.step = 0x7FFF;
    }
    if ((actor->unk54 += 0x180) > 0x7FFF) {
        actor->unk54 = 0x7FFF;
    }
    func_800809EC((&object->prims)[D_8009D7F0], object->def->count, actor->unk58, actor->unk58, actor->unk58);
    object++;
    func_800809EC((&object->prims)[D_8009D7F0], object->def->count, actor->unk58, actor->unk58, actor->unk58);
    if ((actor->unk58 -= 3) < 0) {
        actor->unk58 = 0;
        return 3;
    }
    return 1;
}

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
