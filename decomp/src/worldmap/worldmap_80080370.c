/* World map unit 80080370-800811C0 (rodata 800701E0-800702E4, data
 * 8009A698-8009A6C0): the director of scene mode 13 and its actors (the
 * camera, the effects and the growing objects), the set-up and leave
 * handlers of mode 16 and the sequence start of its director.
 *
 * func_8007F968's five-entry table ends at 800701e0 and func_80080370's
 * follows at once, 0 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after func_8007F968, at or before
 * func_80080370. Its data opens with the cue sequence that func_8008032C,
 * left in the preceding unit by the split, starts. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 9 u16 states and 9 u16 waits (started by
 * func_8008032C; tools/analysis/overlay_scripts.py decodes it). The waits'
 * 0x14 bytes end with the stray halfword (0x7542) that follows the nine
 * waits at the end of the unit's data. */
INCLUDE_ASSET(".data", D_8009A698, 0x8009A698, 0x12);
INCLUDE_ASSET(".data", D_8009A6AC, 0x8009A6AC, 0x14);

/* Flight scene director (mode 13): func_8007A9F8's cue sequencer on
 * D_8009A698/D_8009A6AC; its starter does not step, so entry 0 runs twice.
 * Actor slots (func_8007FF70): 0 the screen fade, 2 the camera
 * (func_80080600), 3 effects 0x28-0x2A (func_80080944), 4 the growing
 * objects 0 and 1 (func_80080AC4). A fade with rate 1 adds the fade quad
 * (white), with rate 2 subtracts it (black). */
s32 func_80080370(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = D_8009A698[actor->u.step];
            actor->wait = D_8009A6AC[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: slot 3 request 1. */
    case 2:
        func_80097770(3, 1);
        actor->state = 1;
        break;
    /* 3: slot 2 request 2. */
    case 3:
        func_80097770(2, 2);
        actor->state = 1;
        break;
    /* 4: fade out at rate 1, 0x80 per frame. */
    case 4:
        func_80097770(0, 0xD);
        D_8009CCA4 = 1;
        D_8009D3CC = 0x80;
        actor->state = 1;
        break;
    /* 5: fade in at rate 1, 0x80 per frame; slot 4 request 1. */
    case 5:
        func_80097770(0, 0xC);
        func_80097770(4, 1);
        D_8009CCA4 = 1;
        D_8009D3CC = 0x80;
        actor->state = 1;
        break;
    /* 6: slot 2 request 3. */
    case 6:
        func_80097770(2, 3);
        actor->state = 1;
        break;
    /* 7: fade out at rate 2, 4 per frame. */
    case 7:
        func_80097770(0, 0xD);
        D_8009CCA4 = 2;
        D_8009D3CC = 4;
        actor->state = 1;
        break;
    /* 8: area sounds 0x16-0x18. */
    case 8:
        func_80039E60((D_8006259C->id << 16) | 0x16);
        func_80039E60((D_8006259C->id << 16) | 0x17);
        func_80039E60((D_8006259C->id << 16) | 0x18);
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
s32 func_80080578(s32 index) {
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

/* Scripted camera: pull in and tilt (command 1), shake (2) or settle the shake (3). */
s32 func_80080600(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    s32 delta;

    scratch = (ActorScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 1:
        actor->state = 1;
        actor->unk4 = 0;
        D_8009BD38.vx = 0x10;
        D_8009BD38.vy = 0x8E0;
        D_8009BD38.vz = 0;
        actor->u.step = 0x800000;
        actor->unk54 = actor->unk58 = D_8009BD38.vx << 12;
        D_8009D3F0 = 0x800000;
        actor->unk5C = actor->unk60 = D_8009BD38.vy << 12;
        break;
    case 2:
        actor->state = 2;
        actor->unk4 = 0;
        actor->unk7C = 0x8000;
        break;
    case 3:
        actor->state = 3;
        actor->unk4 = 0;
        actor->unk7C = 0x80000;
        break;
    }
    if (D_8009D144 == 0) {
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    switch (actor->state) {
    case 0:
    case 2:
        break;
    case 1:
        if ((actor->u.step -= 0x10000) < 0x300000) {
            actor->u.step = 0x300000;
        }
        delta = actor->u.step - D_8009D3F0;
        if (delta != 0) {
            D_8009D3F0 += delta >> 5;
        }
        if ((actor->unk54 -= 0x4000) < -0x100000) {
            actor->unk54 = -0x100000;
        }
        delta = actor->unk54 - actor->unk58;
        if (delta != 0) {
            actor->unk58 += delta >> 4;
            D_8009BD38.vx = actor->unk58 >> 12;
        }
        if ((actor->unk5C -= 0x10000) < 0x2E0000) {
            actor->unk5C = 0x2E0000;
        }
        delta = actor->unk5C - actor->unk60;
        if (delta != 0) {
            actor->unk60 += delta >> 5;
            D_8009BD38.vy = actor->unk60 >> 12;
        }
        break;
    case 3:
        if ((actor->unk7C -= 0x4000) < 0x1000) {
            actor->unk7C = 0x1000;
            actor->state = 0;
        }
        break;
    }
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    VIEW.eye.vy += scratch->position.vy;
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* Copy the player position into the actor. */
s32 func_80080900(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = D_8009C5AC.vx;
    actor->position.vy = D_8009C5AC.vy;
    actor->position.vz = D_8009C5AC.vz;
    return 1;
}

/* On command, emit effects 0x28-0x2A at the actor. */
s32 func_80080944(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(0x28, SCRIPT_VECTOR, 0);
        func_80089160(0x29, SCRIPT_VECTOR, 0);
        func_80089160(0x2A, SCRIPT_VECTOR, 0);
    }
    return 1;
}

/* Set the colour of `count` textured quads. */
void func_800809EC(POLY_FT4 *quads, s32 count, s32 r, s32 g, s32 b) {
    s32 i;

    for (i = 0; i < count; i++, quads++) {
        setRGB0(quads, r, g, b);
    }
}

/* Start a descent at the player and rebuild scene objects 0 and 1. */
s32 func_80080A28(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = D_8009C620;
    actor = &D_8009BE24[index];
    actor->position.vx = D_8009C5AC.vx;
    actor->position.vy = -0x80000;
    actor->position.vz = D_8009C5AC.vz;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    func_8007A06C(object, object->prims, object->def->primitive_count);
    object++;
    func_8007A06C(object, object->prims, object->def->primitive_count);
    return 3;
}

/* Grow and fade scene objects 0 and 1 at the actor; ends the step when faded out. */
s32 func_80080AC4(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = D_8009C620;
    actor = &D_8009BE24[index];
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
    func_800809EC((&object->prims)[D_8009D7F0], object->def->primitive_count, actor->unk58, actor->unk58, actor->unk58);
    object++;
    func_800809EC((&object->prims)[D_8009D7F0], object->def->primitive_count, actor->unk58, actor->unk58, actor->unk58);
    if ((actor->unk58 -= 4) < 0) {
        actor->unk58 = 0;
        return 3;
    }
    return 1;
}

/* Set up the heat-haze scene: fixed start position, music, its director and effect actors. */
void func_80080D00(void) {
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
    D_8009C5AC.vx = 0x4000000;
    D_8009C5AC.vy = -0xC0000;
    D_8009C5AC.vz = 0x4000000;
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
    sequence = func_80039850((SoundSeqHeader *)D_80062648);
    D_80062528 = (s32)sequence;
    func_80039A80(sequence, 0x7F, 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_80081174, (s32)func_800811C0);
    func_80097718((s32)func_800813E8, (s32)func_80081470);
    func_80097718((s32)func_800817A0, (s32)func_80081868);
    func_80097718((s32)func_800819C8, (s32)func_80081B24);
    func_80097718((s32)func_80081C3C, (s32)func_80081D80);
    func_80097718((s32)func_80081FB4, (s32)func_80081FD8);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80075228();
}

/* Leave the world map for scene 0x1FA (flag word 0). */
void func_8008106C(void) {
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
    D_8006D634.map = 0x1FA;
    D_8006D634.entry[2] = 0;
    D_8009BBC4 = 1;
    D_8006D634.entry[0] = D_8009BD38.vy;
}

/* Start an actor's timed sequence: first state and its duration. */
s32 func_80081174(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A6C0[0];
    actor->wait = D_8009A70C[actor->u.step];
    actor->u.step++;
    return 1;
}
