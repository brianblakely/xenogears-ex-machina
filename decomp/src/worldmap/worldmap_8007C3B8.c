#include "worldmap.h"

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 8 u16 states and 8 u16 waits (started by
 * func_8007C36C; tools/analysis/overlay_scripts.py decodes it). */
INCLUDE_ASSET(".data", D_8009A4D8, 0x8009A4D8, 0x10);
INCLUDE_ASSET(".data", D_8009A4E8, 0x8009A4E8, 0x10);

/* Camera shot paths: control points. */
SVECTOR D_8009A4F8[14] = {
    {1629, -160, 1629}, {0, -160, 2304}, {-1629, -160, 1629}, {-2304, -160, 0},
    {-1621, -386, -1621}, {0, -525, -1623}, {814, -160, -814}, {1022, -110, 0},
    {515, -401, 516}, {0, -260, 503}, {-267, -235, 267}, {-383, -125, -6}, {-244, -55, -251},
    {6, -55, -352},
};
SVECTOR D_8009A568[7] = {
    {4, -387, -369}, {-252, -507, -257}, {-272, -637, -2}, {-176, -892, 172}, {-209, -991, 360},
    {-495, -1063, 852}, {-703, -707, 1647},
};

/* Vehicle scene director (mode 12): func_8007A9F8's cue sequencer on
 * D_8009A4D8/D_8009A4E8, whose starter runs entry 0 at once. Actor slots
 * (func_8007BF50): 0 the screen fade, 2 the camera shots (func_8007C7D8),
 * 3-8 the drifting scene objects (func_8007CD20-func_8007D690), 9 object 16
 * (func_8007D7FC). A fade with rate 1 adds the fade quad (white), with
 * rate 2 subtracts it (black). */
s32 func_8007C3B8(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;

    actor = &D_8009BE24[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->state) {
    /* 0: idle (no entry holds it). */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = D_8009A4D8[actor->u.step];
            actor->wait = D_8009A4E8[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: slot 2 request 1; area sounds 0xD-0xF. */
    case 2:
        func_80097770(2, 1);
        actor->state = 1;
        func_80039E60((D_8006259C->id << 16) | 0xD);
        func_80039E60((D_8006259C->id << 16) | 0xE);
        func_80039E60((D_8006259C->id << 16) | 0xF);
        break;
    /* 0x10: start emitter group 0x14 at the camera target's x, z; area
     * sounds 0x10-0x12. */
    case 0x10:
        scratch->position.vy = 0;
        scratch->position.vx = D_8009BE28.target.vx >> 12;
        scratch->position.vz = D_8009BE28.target.vz >> 12;
        func_80089160(0x14, &scratch->position, NULL);
        func_80039E60((D_8006259C->id << 16) | 0x10);
        func_80039E60((D_8006259C->id << 16) | 0x11);
        func_80039E60((D_8006259C->id << 16) | 0x12);
        actor->state = 1;
        break;
    /* 0x11: slot 2 request 2; fade out at rate 1, 0x40 per frame; area
     * sounds 0x13-0x15. */
    case 0x11:
        func_80097770(2, 2);
        func_80097770(0, 0xD);
        D_8009CCA4 = 1;
        actor->state = 1;
        D_8009D3CC = 0x40;
        func_80039E60((D_8006259C->id << 16) | 0x13);
        func_80039E60((D_8006259C->id << 16) | 0x14);
        func_80039E60((D_8006259C->id << 16) | 0x15);
        break;
    /* 0x12: slot 2 request 3, slots 3-7 request 1; fade in at rate 1, 1 per
     * frame. */
    case 0x12:
        func_80097770(2, 3);
        func_80097770(3, 1);
        func_80097770(4, 1);
        func_80097770(5, 1);
        func_80097770(6, 1);
        func_80097770(7, 1);
        func_80097770(0, 0xC);
        D_8009CCA4 = 1;
        D_8009D3CC = 1;
        actor->state = 1;
        break;
    /* 0x13: slot 2 request 4, slot 9 request 1. */
    case 0x13:
        func_80097770(2, 4);
        func_80097770(9, 1);
        actor->state = 1;
        break;
    /* 0x14: slot 2 request 6, slots 3 and 9 request 2. */
    case 0x14:
        func_80097770(2, 6);
        func_80097770(3, 2);
        func_80097770(9, 2);
        actor->state = 1;
        break;
    /* 0x16: fade out at rate 2, 4 per frame. */
    case 0x16:
        func_80097770(0, 0xD);
        D_8009CCA4 = 2;
        D_8009D3CC = 4;
        actor->state = 1;
        break;
    /* 0x40: end the world-map loop with exit 0 (the state stays 1). */
    case 0x40:
        D_8009D554 = 0;
        D_8009D7CC = 0;
        actor->state = 1;
        break;
    }
    return 1;
}

/* Start a scripted camera close behind the player. */
s32 func_8007C724(s32 index) {
    WorldmapActor *actor;

    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x400000;
    D_8009D144 = 1;
    actor = &D_8009BE24[index];
    actor->unk7C = 0x1000;
    D_8009BD38.vx = -0x40;
    D_8009BD38.vy = 0;
    D_8009BD38.vz = 0;
    D_8009D55C.target.vx = D_8009BE28.target.vx = D_8009C5AC.vx;
    D_8009D55C.target.vy = D_8009BE28.target.vy = D_8009C5AC.vy;
    D_8009D55C.target.vz = D_8009BE28.target.vz = D_8009C5AC.vz;
    actor->unk58 = 0x40;
    actor->u.step = 0;
    return 1;
}

/* Camera shot director: move the camera along a shot path (u.step, speed
 * unk58), keep the saved camera target, and shake the view by unk7C. */
s32 func_8007C7D8(s32 index) {
    WorldmapActor *actor;
    ShotScratch *scratch;
    SVECTOR *points;
    s32 shake;

    actor = &D_8009BE24[index];
    scratch = (ShotScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->state = 1;
        actor->unk5C = 0x8200;
        actor->unk4 = 0;
        actor->unk60 = 1;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 3;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 4;
        break;
    case 4:
        actor->state = 5;
        actor->unk5C = 0xA800;
        actor->unk4 = 0;
        actor->unk60 = 8;
        break;
    case 6:
        actor->state = 6;
        actor->unk4 = 0;
        actor->u.step = 0;
        actor->unk58 = 0x40;
        break;
    }
    if (D_8009D144 == 0) {
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    switch (actor->state) {
    case 2:
        actor->unk58 -= actor->unk60;
        if (actor->unk58 < 0) {
            actor->unk58 = 0;
            actor->state = 0;
        }
    case 1:
        actor->u.step += actor->unk58;
        if (actor->u.step > actor->unk5C) {
            actor->state = 2;
        }
    case 0:
    follow:
        scratch->point.vy = 0;
        scratch->point.vx = 0;
        scratch->point.vz = D_8009D55C.target.vz - D_8009BE28.target.vz;
        func_80093484(&scratch->point);
        GROUND_SCROLL[2] += scratch->point.vz;
        D_8009BE28.target.vz = D_8009D55C.target.vz;
        points = &D_8009A4F8[actor->u.step >> 12];
        if (points[2].pad != -1) {
            func_80076858(actor->u.step & 0xFFF, &points[0], &points[1], &points[2], &scratch->point);
            VIEW.at.vz = 0;
            VIEW.at.vx = 0;
            VIEW.up.vz = 0;
            VIEW.up.vx = 0;
            VIEW.eye.vx = scratch->point.vx >> 16;
            VIEW.up.vy = -0x1000;
            VIEW.eye.vz = -(scratch->point.vz >> 16);
            VIEW.eye.vy = scratch->point.vy >> 16;
            VIEW.at.vy = D_8009BE28.target.vy >> 12;
        }
        func_80097244(D_8009BD40);
        func_80097070(&D_8009C808, &D_8009BD38);
        break;
    case 3:
        actor->unk7C = 0xF000;
        actor->state = 0;
        goto follow;
    case 4:
        scratch->spot.vy = 0;
        scratch->spot.vx = D_8009BE28.target.vx >> 12;
        scratch->spot.vz = D_8009BE28.target.vz >> 12;
        func_80089160(0x1A, &scratch->spot, NULL);
        func_80089160(0x1B, &scratch->spot, NULL);
        actor->unk7C -= 0x120;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
            actor->state = 0;
            func_800894C8(0x1A);
            func_800894C8(0x1B);
        }
        goto follow;
    case 5:
        actor->unk58 = 0x100;
        actor->state = 1;
        goto follow;
    case 6:
        scratch->point.vy = 0;
        scratch->point.vx = 0;
        scratch->point.vz = D_8009D55C.target.vz - D_8009BE28.target.vz;
        func_80093484(&scratch->point);
        GROUND_SCROLL[2] += scratch->point.vz;
        D_8009BE28.target.vx = D_8009D55C.target.vx;
        D_8009BE28.target.vy = D_8009D55C.target.vy;
        D_8009BE28.target.vz = D_8009D55C.target.vz;
        actor->u.step += actor->unk58;
        if (actor->u.step > 0x4800) {
            actor->unk58--;
            if (actor->unk58 < 0) {
                actor->unk58 = 0;
            }
        }
        points = &D_8009A568[actor->u.step >> 12];
        if (points[2].pad != -1) {
            func_80076858(actor->u.step & 0xFFF, &points[0], &points[1], &points[2], &scratch->point);
            VIEW.at.vz = 0;
            VIEW.at.vx = 0;
            VIEW.up.vz = 0;
            VIEW.up.vx = 0;
            VIEW.eye.vx = scratch->point.vx >> 16;
            VIEW.up.vy = -0x1000;
            VIEW.eye.vz = -(scratch->point.vz >> 16);
            VIEW.eye.vy = scratch->point.vy >> 16;
            VIEW.at.vy = D_8009BE28.target.vy >> 12;
        }
        func_80097244(D_8009BD40);
        func_80097070(&D_8009C808, &D_8009BD38);
        break;
    }
    shake = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    scratch->spot.vy = shake;
    ((s16 *)D_8009BD40)[1] += shake; /* VIEW.eye.vy */
    VIEW.at.vy += scratch->spot.vy;
    return 1;
}

/* Link scene objects 0-3 to 4, hide 4 and reset its rotation; place the actor. */
s32 func_8007CC6C(s32 index) {
    WorldmapActor *actor;

    func_800848B4(4, 0);
    func_800848B4(4, 2);
    func_800848B4(4, 1);
    func_800848B4(4, 3);
    D_8009C620[4].visible = 0;
    D_8009C620[4].angle.vz = 0;
    D_8009C620[4].angle.vy = 0;
    D_8009C620[4].angle.vx = 0;
    func_8004A92C(&D_8009C620[4].angle, &D_8009C620[4].matrix);
    actor = &D_8009BE24[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xD00000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x400000;
    return 1;
}

/* Drift scene object 4 with the actor along z; commands 1/2 start its effects (state 1 also follows with the camera). */
s32 func_8007CD20(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    ActorScratch *scratch;

    actor = &D_8009BE24[index];
    object = &D_8009C620[4];
    scratch = (ActorScratch *)0x1F800000;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->state = 1;
    } else if (actor->unk4 == 2) {
        actor->unk4 = 0;
        actor->state = 2;
    }
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    scratch->position.vx = actor->position.vx >> 12;
    scratch->position.vy = actor->position.vy >> 12;
    scratch->position.vz = actor->position.vz >> 12;
    func_80089160(0x13, &scratch->position, NULL);
    switch (actor->state) {
    case 1:
        func_80089160(0x1F, &scratch->position, NULL);
    case 0:
        D_8009D55C.target.vz = actor->position.vz;
        break;
    case 2:
        func_80089160(0x1F, &scratch->position, NULL);
        break;
    }
    return 1;
}

/* Link scene object 6 to 5, hide 5 and reset its rotation; place the actor. */
s32 func_8007CE84(s32 index) {
    WorldmapActor *actor;

    func_800848B4(5, 6);
    D_8009C620[5].visible = 0;
    D_8009C620[5].angle.vz = 0;
    D_8009C620[5].angle.vy = 0;
    D_8009C620[5].angle.vx = 0;
    func_8004A92C(&D_8009C620[5].angle, &D_8009C620[5].matrix);
    actor = &D_8009BE24[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xB00000;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x200000;
    return 1;
}

/* Drift scene object 5 with the actor over the terrain along z; command 1 starts its trail effect. */
s32 func_8007CF18(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    SVECTOR *vector;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->state = 1;
    }
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) - 0x4000;
    objects[5].position.vx = actor->position.vx >> 12;
    objects[5].position.vy = actor->position.vy >> 12;
    objects[5].position.vz = actor->position.vz >> 12;
    ACTOR_SCRATCH->position.vx = actor->position.vx >> 12;
    vector = SCRIPT_VECTOR;
    ACTOR_SCRATCH->position.vy = actor->position.vy >> 12;
    ACTOR_SCRATCH->position.vz = actor->position.vz >> 12;
    func_80089160(0x15, SCRIPT_VECTOR, NULL);
    if (actor->state == 1) {
        vector->vx = actor->position.vx >> 12;
        vector->vy = actor->position.vy >> 12;
        vector->vz = actor->position.vz >> 12;
        func_80089160(0x1E, SCRIPT_VECTOR, NULL);
    }
    return 1;
}

/* Link scene objects 7 and 8 to 9, hide 9 and reset its rotation; place the actor. */
s32 func_8007D078(s32 index) {
    WorldmapActor *actor;

    func_800848B4(9, 7);
    func_800848B4(9, 8);
    D_8009C620[9].visible = 0;
    D_8009C620[9].angle.vz = 0;
    D_8009C620[9].angle.vy = 0;
    D_8009C620[9].angle.vx = 0;
    func_8004A92C(&D_8009C620[9].angle, &D_8009C620[9].matrix);
    actor = &D_8009BE24[index];
    actor->motion.vz = -0x4000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vx = 0xC00000;
    actor->position.vy = 0;
    actor->position.vz = 0;
    return 1;
}

/* Drift scene object 9 with the actor over the terrain; command 1 shows objects 7-9 and ends the step. */
s32 func_8007D110(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        objects[9].visible = 1;
        objects[7].visible = 1;
        objects[8].visible = 1;
        func_800894C8(0x16);
        return 3;
    }
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) - 0x4000;
    objects[9].position.vx = actor->position.vx >> 12;
    objects[9].position.vy = actor->position.vy >> 12;
    objects[9].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    func_80089160(0x16, SCRIPT_VECTOR, NULL);
    return 1;
}

/* Link scene object 11 to 10, hide 10 and reset its rotation; place the actor. */
s32 func_8007D228(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0xA, 0xB);
    D_8009C620[10].visible = 0;
    D_8009C620[10].angle.vz = 0;
    D_8009C620[10].angle.vy = 0;
    D_8009C620[10].angle.vx = 0;
    func_8004A92C(&D_8009C620[10].angle, &D_8009C620[10].matrix);
    actor = &D_8009BE24[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xE00000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x100000;
    return 1;
}

/* Drift scene object 10 with the actor over the terrain; command 1 shows objects 10-11, bursts and ends the step. */
s32 func_8007D2B8(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        objects[10].visible = 1;
        objects[11].visible = 1;
        func_800894C8(0x17);
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(0x1C, SCRIPT_VECTOR, NULL);
        return 3;
    }
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) - 0x4000;
    objects[10].position.vx = actor->position.vx >> 12;
    objects[10].position.vy = actor->position.vy >> 12;
    objects[10].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    func_80089160(0x17, SCRIPT_VECTOR, NULL);
    return 1;
}

/* Link scene object 15 to 12, hide 12 and reset its rotation; place the actor. */
s32 func_8007D414(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0xC, 0xF);
    D_8009C620[12].visible = 0;
    D_8009C620[12].angle.vz = 0;
    D_8009C620[12].angle.vy = 0;
    D_8009C620[12].angle.vx = 0;
    func_8004A92C(&D_8009C620[12].angle, &D_8009C620[12].matrix);
    actor = &D_8009BE24[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xE80000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x280000;
    return 1;
}

/* Drift scene object 12 with the actor over the terrain; command 1 shows objects 12 and 15, bursts and ends the step. */
s32 func_8007D4A4(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        objects[12].visible = 1;
        objects[15].visible = 1;
        func_800894C8(0x18);
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(0x1D, SCRIPT_VECTOR, NULL);
        return 3;
    }
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) - 0x4000;
    objects[12].position.vx = actor->position.vx >> 12;
    objects[12].position.vy = actor->position.vy >> 12;
    objects[12].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    func_80089160(0x18, SCRIPT_VECTOR, NULL);
    return 1;
}

/* Link scene object 14 to 13, hide 13 and reset its rotation; place the actor. */
s32 func_8007D600(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0xD, 0xE);
    D_8009C620[13].visible = 0;
    D_8009C620[13].angle.vz = 0;
    D_8009C620[13].angle.vy = 0;
    D_8009C620[13].angle.vx = 0;
    func_8004A92C(&D_8009C620[13].angle, &D_8009C620[13].matrix);
    actor = &D_8009BE24[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xF80000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x380000;
    return 1;
}

/* Drift scene object 13 with the actor over the terrain. */
s32 func_8007D690(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) - 0x4000;
    objects[13].position.vx = actor->position.vx >> 12;
    objects[13].position.vy = actor->position.vy >> 12;
    objects[13].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    func_80089160(0x19, SCRIPT_VECTOR, NULL);
    return 1;
}

/* Show scene object 16, reset its rotation and place the actor. */
s32 func_8007D774(s32 index) {
    WorldmapActor *actor;

    D_8009C620[16].visible = 1;
    D_8009C620[16].angle.vz = 0;
    D_8009C620[16].angle.vy = 0;
    D_8009C620[16].angle.vx = 0;
    func_8004A92C(&D_8009C620[16].angle, &D_8009C620[16].matrix);
    actor = &D_8009BE24[index];
    actor->position.vx = 0xD00000;
    actor->position.vz = 0x400000;
    actor->position.vy = -0x280000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->motion.vz = -0x6000;
    return 3;
}

/* Drift scene object 16 with the actor; command 1 hides it and moves ahead of the camera, command 2 makes the camera follow. */
s32 func_8007D7FC(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = &D_8009C620[16];
    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        D_8009C620[16].visible = 0;
        actor->position.vx = D_8009BE28.target.vx;
        actor->position.vz = D_8009BE28.target.vz + 0x400000;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        break;
    }
    actor->position.vz += actor->motion.vz;
    func_80093354(&actor->position);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    if (actor->state == 2) {
        D_8009D55C.target.vx = actor->position.vx;
        D_8009D55C.target.vy = actor->position.vy;
        D_8009D55C.target.vz = actor->position.vz;
    }
    return 1;
}

/* Set up the vehicle scene: load its area, place the player at the entry, start music and its scripted actors. */
void func_8007D918(void) {
    RECT rect;
    void *sequence;
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
    D_8009C5AC.vx = D_8009A5B4[D_8009D3D4].vx << 12;
    D_8009C5AC.vy = D_8009A5B4[D_8009D3D4].vy << 12;
    D_8009C5AC.vz = D_8009A5B4[D_8009D3D4].vz << 12;
    func_80084580();
    func_8008440C();
    func_800979C8();
    func_80072090();
    func_800736DC();
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
    sequence = func_80039850(D_80062648);
    D_80062528 = sequence;
    func_80039A80(sequence, 0x7F, 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_8007DE14, (s32)func_8007DE98);
    func_80097718((s32)func_8007E450, (s32)func_8007E4E4);
    func_80097718((s32)func_8007ECA4, (s32)func_8007EE34);
    func_80097718((s32)func_8007F8AC, (s32)func_8007F968);
    func_80097718((s32)func_8007F8AC, (s32)func_8007F968);
    func_80097718((s32)func_8007F8AC, (s32)func_8007F968);
    func_80097718((s32)func_8007F8AC, (s32)func_8007F968);
    func_80097718((s32)func_8007F8AC, (s32)func_8007F968);
    func_80097718((s32)func_8007FC8C, (s32)func_8007FD30);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80075228();
}

/* Leave the world map: release its sound, subsystems and buffers, and request scene 0x1A1 with the exit's flag word. */
void func_8007DCE0(void) {
    func_8003A89C(D_80062528, 0, 0xF0);
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
    D_8006F94E.scene = 0x1A1;
    D_8006F954[0] = D_8009A5CC[D_8009D3D4];
    D_8006F94E.heading = D_8009BD38.vy;
    D_8009BBC4 = 1;
}

/* Start the selected timed sequence on an actor. */
s32 func_8007DE14(s32 index) {
    s32 unused[2]; /* unreferenced local: the original frame reserves it */
    WorldmapActor *actor;
    s32 sequence;

    sequence = D_8009D3D4;
    actor = &D_8009BE24[index];
    actor->unk54 = (s32)D_8009A65C[sequence].states;
    actor->unk58 = (s32)D_8009A65C[sequence].durations;
    actor->u.step = 0;
    actor->state = *(s16 *)actor->unk54;
    actor->wait = ((u16 *)actor->unk58)[actor->u.step];
    actor->u.step++;
    return 1;
}
