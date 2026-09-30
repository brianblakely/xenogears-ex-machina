#include "worldmap.h"

/* Scene director: state 1 steps the timed sequence at D_8009A450/D_8009A46C;
 * the other states start effects, sounds and actor commands and return to 1. */
s32 func_8007A9F8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (actor->state) {
    case 0:
        break;
    case 1:
        if (--actor->wait < 0) {
            actor->state = (&D_8009A450)[actor->u.step];
            actor->wait = D_8009A46C[actor->u.step];
            actor->u.step++;
        }
        break;
    case 2:
        func_80089160(0x11, NULL, NULL);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 1);
        break;
    case 3:
        func_80097770(2, 2);
        actor->state = 1;
        func_80089160(0xF, NULL, NULL);
        func_80089160(0x10, NULL, NULL);
        func_80039E60((D_8006259C->id << 16) | 4);
        func_80039E60((D_8006259C->id << 16) | 5);
        func_80039E60((D_8006259C->id << 16) | 6);
        break;
    case 4:
        func_80097770(2, 3);
        func_80097770(3, 1);
        actor->state = 1;
        break;
    case 5:
        func_80097770(2, 6);
        func_80097770(4, 1);
        func_80097770(5, 1);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 7);
        func_80039E60((D_8006259C->id << 16) | 8);
        func_80039E60((D_8006259C->id << 16) | 9);
        break;
    case 6:
        func_80097770(2, 4);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 0xA);
        func_80039E18((D_8006259C->id << 16) | 0xB);
        func_80039E18((D_8006259C->id << 16) | 0xC);
        break;
    case 7:
        func_80097770(2, 5);
        actor->state = 1;
        break;
    case 8:
        func_80097770(2, 1);
        actor->state = 1;
        break;
    case 9:
        func_80097770(6, 1);
        func_80097770(2, 7);
        actor->state = 1;
        break;
    case 10:
        func_80097770(0, 0xD);
        D_8009D3CC = 4;
        actor->state = 1;
        break;
    case 11:
        D_8009D554 = 0;
        D_8009D7CC = 0;
        actor->state = 1;
        break;
    }
    return 1;
}

/* Start a scripted camera looking at the player from yaw 0x480. */
#ifdef NON_MATCHING /* return value loaded before the stores */
s32 func_8007AD34(s32 index) {
    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x1C0000;
    D_8009BD38.vx = 0x40;
    D_8009BD38.vy = 0x480;
    D_8009BD38.vz = 0;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    D_8009BE24[index].u.step = 0x1000;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007AD34);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007ADD4);

/* Rebuild scene objects 4-5 and show objects 6-7 at zero scale. */
s32 func_8007B200(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    ScaleScratch *scratch;
    s32 i;

    scratch = SCALE_SCRATCH;
    i = 0;
    actor = &D_8009BE24[index];
    object = &D_8009C620[4];
    do {
        func_8007A06C(object, object->prims, object->def->count);
        object++;
        i++;
    } while (i < 2);
    actor->unk54 = 0;
    actor->u.step = 0;
    scratch->matrix[0] = D_8009A180;
    scratch->scale[0].vx = scratch->scale[0].vz = actor->u.step;
    scratch->scale[0].vy = 0x1000;
    ScaleMatrix(&scratch->matrix[0], &scratch->scale[0]);
    object[0].matrix = scratch->matrix[0];
    object[1].matrix = scratch->matrix[0];
    object[1].visible = 1;
    object[0].visible = 1;
    return 3;
}

/* Grow scene objects 4 and 5 at the player: widen their scale each frame up to 0x7F00. */
s32 func_8007B394(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    s32 x;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        objects[5].visible = 0;
        objects[4].visible = 0;
    }
    x = D_8009C5AC.vx >> 12;
    objects[4].position.vy = objects[5].position.vy = -0x40;
    objects[4].position.vx = objects[5].position.vx = x;
    objects[4].position.vz = objects[5].position.vz = D_8009C5AC.vz >> 12;
    if ((actor->u.step += 0x180) > 0x800) {
        actor->unk54 += 0x180;
    }
    if (actor->u.step > 0x7F00) {
        actor->u.step = 0x7F00;
    }
    if (actor->unk54 > 0x7F00) {
        actor->unk54 = 0x7F00;
    }
    SCALE_SCRATCH->matrix[0] = D_8009A180;
    SCALE_SCRATCH->matrix[1] = SCALE_SCRATCH->matrix[0];
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&SCALE_SCRATCH->matrix[0], &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&SCALE_SCRATCH->matrix[1], &SCALE_SCRATCH->scale[1]);
    objects[4].matrix = SCALE_SCRATCH->matrix[0];
    objects[5].matrix = SCALE_SCRATCH->matrix[1];
    return 1;
}

/* Rebuild scene objects 6-7 and show objects 8-9 at zero scale. */
s32 func_8007B604(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    ScaleScratch *scratch;
    s32 i;

    scratch = SCALE_SCRATCH;
    i = 0;
    actor = &D_8009BE24[index];
    object = &D_8009C620[6];
    do {
        func_8007A06C(object, object->prims, object->def->count);
        object++;
        i++;
    } while (i < 2);
    actor->unk54 = 0;
    actor->u.step = 0;
    scratch->matrix[0] = D_8009A180;
    scratch->scale[0].vx = scratch->scale[0].vz = actor->u.step;
    scratch->scale[0].vy = 0x1000;
    ScaleMatrix(&scratch->matrix[0], &scratch->scale[0]);
    object[0].matrix = scratch->matrix[0];
    object[1].matrix = scratch->matrix[0];
    object[1].visible = 1;
    object[0].visible = 1;
    return 3;
}

/* Grow scene objects 6 and 7 at the player (see func_8007B394). */
s32 func_8007B798(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    s32 x;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        objects[7].visible = 0;
        objects[6].visible = 0;
    }
    x = D_8009C5AC.vx >> 12;
    objects[6].position.vy = objects[7].position.vy = -0x40;
    objects[6].position.vx = objects[7].position.vx = x;
    objects[6].position.vz = objects[7].position.vz = D_8009C5AC.vz >> 12;
    if ((actor->u.step += 0x180) > 0x800) {
        actor->unk54 += 0x180;
    }
    if (actor->u.step > 0x7F00) {
        actor->u.step = 0x7F00;
    }
    if (actor->unk54 > 0x7F00) {
        actor->unk54 = 0x7F00;
    }
    SCALE_SCRATCH->matrix[0] = D_8009A180;
    SCALE_SCRATCH->matrix[1] = SCALE_SCRATCH->matrix[0];
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&SCALE_SCRATCH->matrix[0], &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&SCALE_SCRATCH->matrix[1], &SCALE_SCRATCH->scale[1]);
    objects[6].matrix = SCALE_SCRATCH->matrix[0];
    objects[7].matrix = SCALE_SCRATCH->matrix[1];
    return 1;
}

/* Scene step with nothing to do. */
s32 func_8007BA08(void) {
    return 3;
}

/* Exhaust trail: move the emitter along its path for 60 frames, then reset to the player and end the step. */
s32 func_8007BA10(s32 index) {
    WorldmapActor *actor;
    s32 result;

    result = 1;
    actor = &D_8009BE24[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->wait = 0x3C;
        actor->position.vx = D_8009C5AC.vx;
        actor->position.vy = D_8009C5AC.vy;
        actor->position.vz = D_8009C5AC.vz;
    }
    if (--actor->wait > 0) {
        actor->position.vx -= 0x95D0;
        actor->position.vz += 0x2F130;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(9, SCRIPT_VECTOR, &D_8009A488);
    } else {
        actor->wait = 0x3C;
        actor->position = D_8009C5AC;
        func_800894C8(9);
        result = 3;
    }
    return result;
}

/* Link scene objects 1-3 to object 0 and reset its rotation. */
s32 func_8007BB60(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 1);
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->unk54 = 0x100;
    actor->unk58 = 0x100;
    D_8009C620[0].angle.vx = 0;
    D_8009C620[0].angle.vy = 0;
    D_8009C620[0].angle.vz = 0;
    func_8004A92C(&D_8009C620[0].angle, &D_8009C620[0].matrix);
    return 3;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8007A9F8", func_8007BBEC);

/* Set up the second vehicle scene: fixed start position, its director and object actors. */
void func_8007BF50(void) {
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
    D_8009C5AC.vx = 0xD00000;
    D_8009C5AC.vy = -0xA0000;
    D_8009C5AC.vz = 0x400000;
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
    func_80097718((s32)func_8007C36C, (s32)func_8007C3B8);
    func_80097718((s32)func_8007C724, (s32)func_8007C7D8);
    func_80097718((s32)func_8007CC6C, (s32)func_8007CD20);
    func_80097718((s32)func_8007CE84, (s32)func_8007CF18);
    func_80097718((s32)func_8007D078, (s32)func_8007D110);
    func_80097718((s32)func_8007D228, (s32)func_8007D2B8);
    func_80097718((s32)func_8007D414, (s32)func_8007D4A4);
    func_80097718((s32)func_8007D600, (s32)func_8007D690);
    func_80097718((s32)func_8007D774, (s32)func_8007D7FC);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80075228();
}

/* Leave the world map for scene 0x111 (flag word 2), releasing its sound, subsystems and buffers. */
void func_8007C260(void) {
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
    D_8006F94E = 0x111;
    D_8006F954[0] = 2;
    D_8009BBC4 = 1;
    D_8006F950 = D_8009BD38.vy;
}

/* Start an actor's timed sequence: first state and its duration. */
s32 func_8007C36C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A4D8[0];
    actor->wait = D_8009A4E8[actor->u.step];
    actor->u.step++;
    return 1;
}
