/* World map unit 8007A9F8-8007C3B8 (rodata 8006FBC4-8006FC50, data
 * 8009A450-8009A4D8): the director of scene mode 14 and its actors (camera
 * shake, growing objects, exhaust trail, the rig's flight), the set-up and
 * leave handlers of mode 12 and the sequence start of its director.
 *
 * func_80079778's five-entry table ends at 8006fbc4 and func_8007A9F8's
 * follows at once, 4 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after func_80079778, at or before
 * func_8007A9F8. Its data opens with the cue sequence that func_8007A9B4,
 * left in the preceding unit by the split, starts. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 14 u16 states and 14 u16 waits (started by
 * func_8007A9B4; tools/analysis/overlay_scripts.py decodes it). */
INCLUDE_ASSET(".data", D_8009A450, 0x8009A450, 0x1C);
INCLUDE_ASSET(".data", D_8009A46C, 0x8009A46C, 0x1C);

/* Exhaust effect angle. */
SVECTOR D_8009A488 = {0, 128, 0};

/* The rig's flight path control points. */
SVECTOR D_8009A490[9] = {
    {22989, -240, 25600}, {23421, -200, 25192}, {23749, -192, 24992}, {24445, -168, 24816},
    {24709, -232, 25456}, {24245, -360, 26024}, {23797, -160, 25648}, {23389, -48, 25192},
    {23251, -248, 24648},
};

/* Scene director (mode 14), a cue sequencer: state 1 counts the wait down
 * and, once it drops below 0, loads the next entry's state and wait from
 * D_8009A450/D_8009A46C; each other state runs its cue on the next update
 * and returns to 1. Requests (func_80097770) go to the setup's actor slots
 * (func_8007A5DC): 0 the screen fade, 2 the camera (func_8007ADD4), 3 and 4
 * the growing objects (func_8007B394, func_8007B798), 5 the exhaust trail,
 * 6 the rig's flight. tools/analysis/overlay_scripts.py decodes the
 * sequence. */
s32 func_8007A9F8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = D_8009A450[actor->u.step];
            actor->wait = D_8009A46C[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: start emitter group 0x11; area sound 1. */
    case 2:
        func_80089160(0x11, NULL, NULL);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 1);
        break;
    /* 3: slot 2 request 2; emitter groups 0xF and 0x10; area sounds 4-6. */
    case 3:
        func_80097770(2, 2);
        actor->state = 1;
        func_80089160(0xF, NULL, NULL);
        func_80089160(0x10, NULL, NULL);
        func_80039E60((D_8006259C->id << 16) | 4);
        func_80039E60((D_8006259C->id << 16) | 5);
        func_80039E60((D_8006259C->id << 16) | 6);
        break;
    /* 4: slot 2 request 3, slot 3 request 1. */
    case 4:
        func_80097770(2, 3);
        func_80097770(3, 1);
        actor->state = 1;
        break;
    /* 5: slot 2 request 6, slots 4 and 5 request 1; area sounds 7-9. */
    case 5:
        func_80097770(2, 6);
        func_80097770(4, 1);
        func_80097770(5, 1);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 7);
        func_80039E60((D_8006259C->id << 16) | 8);
        func_80039E60((D_8006259C->id << 16) | 9);
        break;
    /* 6: slot 2 request 4; area sound 0xA, and 0xB and 0xC on voices 12-13. */
    case 6:
        func_80097770(2, 4);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 0xA);
        func_80039E18((D_8006259C->id << 16) | 0xB);
        func_80039E18((D_8006259C->id << 16) | 0xC);
        break;
    /* 7: slot 2 request 5. */
    case 7:
        func_80097770(2, 5);
        actor->state = 1;
        break;
    /* 8: slot 2 request 1. */
    case 8:
        func_80097770(2, 1);
        actor->state = 1;
        break;
    /* 9: slot 6 request 1, slot 2 request 7. */
    case 9:
        func_80097770(6, 1);
        func_80097770(2, 7);
        actor->state = 1;
        break;
    /* 10: fade out (slot 0 request 13) at 4 per frame. */
    case 10:
        func_80097770(0, 0xD);
        D_8009D3CC = 4;
        actor->state = 1;
        break;
    /* 11: end the world-map loop with exit 0. */
    case 11:
        D_8009D554 = 0;
        D_8009D7CC = 0;
        actor->state = 1;
        break;
    }
    return 1;
}

/* Start a scripted camera looking at the player from yaw 0x480. */
s32 func_8007AD34(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x1C0000;
    D_8009BD38.vx = 0x40;
    D_8009BD38.vy = 0x480;
    D_8009BD38.vz = 0;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->u.step = 0x1000;
    return 1;
}

/* Scene camera shake: commands pick a shot or a shake ramp (u.step is the
 * shake amplitude, 20.12); every frame jitter both view vectors by it. */
s32 func_8007ADD4(s32 index) {
    WorldmapActor *actor;
    CameraScratch *scratch;
    s32 originZ;

    actor = &D_8009BE24[index];
    scratch = (CameraScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 2:
        actor->state = 2;
        actor->unk4 = 0;
        actor->u.step = 0x40000;
        break;
    case 3:
        D_8009D3F0 = 0x640000;
        actor->unk4 = 0;
        actor->state = 0;
        D_8009BD38.vx = -0x1E0;
        D_8009BD38.vy = 0x480;
        D_8009BD38.vz = 0;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 0;
        D_8009D3F0 = 0x1E0000;
        D_8009BD38.vx = 0x40;
        D_8009BD38.vy = 0x418;
        D_8009BD38.vz = 0;
        D_8009D144 = 0;
        D_8009BE28.target.vz = D_8009C5AC.vz;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 3;
        break;
    case 6:
        actor->unk4 = 0;
        actor->state = 4;
        actor->unk58 = D_8009C5AC.vx + 0xC7C00;
        D_8009D144 = 1;
        actor->unk5C = D_8009C5AC.vz + 0x3EC400;
        break;
    case 7:
        actor->unk4 = 0;
        actor->state = 5;
        break;
    }
    switch (actor->state) {
    case 0:
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    case 1:
        actor->u.step += 0x200;
        if (actor->u.step > 0x8000) {
            actor->u.step = 0x8000;
            actor->state = 0;
        }
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    case 2:
        actor->u.step -= 0x200;
        if (actor->u.step < 0x8000) {
            actor->u.step = 0x8000;
            actor->state = 0;
        }
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    case 3:
        actor->u.step -= 0x100;
        if (actor->u.step < 0x1000) {
            actor->u.step = 0x1000;
            actor->state = 0;
        }
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    case 4:
        D_8009BE28.target.vz += 0x3A000;
        if (D_8009BE28.target.vz > 0x77FFFFF) {
            actor->state = 0;
        }
        VIEW.eye.vx = (actor->unk58 - D_8009BE28.target.vx) >> 12;
        originZ = actor->unk5C;
        VIEW.at.vx = VIEW.at.vz = 0;
        VIEW.at.vy = D_8009BE28.target.vy >> 12;
        VIEW.eye.vy = (D_8009BE28.target.vy >> 12) - 0x40;
        VIEW.eye.vz = (D_8009BE28.target.vz - originZ) >> 12;
        func_80097244(&D_8009BD40);
        func_80097070(&D_8009C808, &D_8009BD38);
        break;
    case 5:
        D_8009BD38.vy += 4;
        if (D_8009BD38.vy > 0x600) {
            actor->state = 0;
        }
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    }
    scratch->view.vx = rand() % (actor->u.step >> 12) - (actor->u.step >> 13);
    scratch->view.vy = rand() % (actor->u.step >> 12) - (actor->u.step >> 13);
    VIEW.eye.vx += scratch->view.vx;
    VIEW.at.vx += scratch->view.vx;
    VIEW.eye.vy += scratch->view.vy;
    VIEW.at.vy += scratch->view.vy;
    return 1;
}

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
        func_8007A06C(object, object->prims, object->def->primitive_count);
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
        func_8007A06C(object, object->prims, object->def->primitive_count);
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
    RotMatrixYXZ(&D_8009C620[0].angle, &D_8009C620[0].matrix);
    return 3;
}

/* Scratchpad work area of the rig path follower. */
typedef struct {
    VECTOR axis[4];   /* 0x00 */
    u8 pad40[0x60];
    SVECTOR angle;    /* 0xA0 */
    SVECTOR heading;  /* 0xA8 */
    u8 padB0[0x40];
    MATRIX frame;     /* 0xF0 */
} FollowScratch;

/* Fly scene object 0 along the rig path (speeding up, braking, then rolling
 * out); orient it to the path and emit exhaust while low. Done (3) at the end
 * of the path. */
s32 func_8007BBEC(s32 index) {
    s32 result;
    WorldmapActor *actor;
    FollowScratch *scratch;
    SVECTOR *points;
    SceneObject *object;

    result = 1;
    actor = &D_8009BE24[index];
    scratch = (FollowScratch *)0x1F800000;
    switch (actor->u.step >> 12) {
    case 0:
    case 1:
        actor->u.step += actor->unk58;
        actor->unk54 += 4;
        break;
    case 2:
    case 3:
    case 4:
        actor->u.step += actor->unk58;
        actor->unk58 -= 8;
        if (actor->unk58 < 0x80) {
            actor->unk58 = 0x80;
        }
        actor->unk54 += 4;
        break;
    case 5:
    case 6:
    case 7:
        actor->u.step += actor->unk58;
        actor->unk58 += 8;
        if (actor->unk58 > 0x100) {
            actor->unk58 = 0x100;
        }
        actor->unk54 -= 0x10;
        break;
    case 8:
        result = 3;
        break;
    }
    points = &D_8009A490[actor->u.step >> 12];
    if (points[2].pad != -1) {
        func_80076858(actor->u.step & 0xFFF, &points[0], &points[1], &points[2], &scratch->axis[0]);
    }
    points = &D_8009A490[(actor->u.step + 0x80) >> 12];
    func_80076858((actor->u.step + 0x80) & 0xFFF, &points[0], &points[1], &points[2], &scratch->axis[1]);
    object = D_8009C620;
    object->position.vx = scratch->axis[0].vx >> 16;
    object->position.vy = scratch->axis[0].vy >> 16;
    object->position.vz = scratch->axis[0].vz >> 16;
    scratch->axis[1].vx = (scratch->axis[1].vx - scratch->axis[0].vx) >> 12;
    scratch->axis[1].vy = (scratch->axis[1].vy - scratch->axis[0].vy) >> 12;
    scratch->axis[1].vz = -((scratch->axis[1].vz - scratch->axis[0].vz) >> 12);
    VectorNormal(&scratch->axis[1], &scratch->axis[0]);
    scratch->angle.vx = 0;
    scratch->angle.vy = ratan2(scratch->axis[0].vx, scratch->axis[0].vz) & 0xFFF;
    scratch->angle.vz = actor->unk54;
    RotMatrixYXZ(&scratch->angle, &scratch->frame);
    scratch->angle.vx = 0;
    scratch->angle.vy = -0x1000;
    scratch->angle.vz = 0;
    ApplyMatrix(&scratch->frame, &scratch->angle, &scratch->axis[1]);
    OuterProduct12(&scratch->axis[0], &scratch->axis[1], &scratch->axis[3]);
    VectorNormal(&scratch->axis[3], &scratch->axis[2]);
    OuterProduct12(&scratch->axis[0], &scratch->axis[2], &scratch->axis[3]);
    VectorNormal(&scratch->axis[3], &scratch->axis[1]);
    scratch->frame.m[0][0] = scratch->axis[2].vx;
    scratch->frame.m[0][1] = scratch->axis[2].vy;
    scratch->frame.m[0][2] = scratch->axis[2].vz;
    scratch->frame.m[1][0] = scratch->axis[1].vx;
    scratch->frame.m[1][1] = scratch->axis[1].vy;
    scratch->frame.m[1][2] = scratch->axis[1].vz;
    scratch->frame.m[2][0] = scratch->axis[0].vx;
    scratch->frame.m[2][1] = scratch->axis[0].vy;
    scratch->frame.m[2][2] = scratch->axis[0].vz;
    libgte_transpose_matrix(&scratch->frame, &object->matrix);
    func_80097070(&scratch->frame, &scratch->heading);
    if (object->position.vy >= -0x7F) {
        scratch->angle.vx = object->position.vx;
        scratch->angle.vy = object->position.vy;
        scratch->angle.vz = object->position.vz;
        scratch->heading.vz = -scratch->heading.vz;
        func_80089160(0x12, &scratch->angle, &scratch->heading);
    }
    return result;
}

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
    func_800320E8(D_8009BBC8[0].ot);
    func_800320E8(D_8009BBC8[1].ot);
    func_800320E8(D_8009BBC8[0].packets);
    func_800320E8(D_8009BBC8[1].packets);
    func_800320E8(D_8009C180);
    func_800976A0();
    D_8006D634.map = 0x111;
    D_8006D634.entry[2] = 2;
    D_8009BBC4 = 1;
    D_8006D634.entry[0] = D_8009BD38.vy;
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
