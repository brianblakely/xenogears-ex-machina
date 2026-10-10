/* World map unit 80077E68-8007A9F8 (rodata 8006FB40-8006FBC4, data
 * 8009A3F0-8009A450): the actors of scene modes 9 (the camera flight and the
 * rig) and 10 (its script, the landing rig and the growing sprites), the
 * frame step of most scene modes, the set-up and leave handlers of modes 10
 * and 14 and the sequence start of mode 14's director.
 *
 * func_80073398's seven-entry table ends at 8006fb40 and func_80077E68's
 * follows at once, 0 mod 8: within one unit an odd-length table followed by
 * another keeps its phase with a pad word, so this unit's rodata starts at
 * 8006fb40 and its text after func_80073398, at or before func_80077E68. */
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

/* The scene camera's flight path: control points, the last marked by pad -1. */
SVECTOR D_8009A3F0[12] = {
    {0, -228, -425}, {301, -228, -301}, {490, -364, 0}, {358, -437, 358}, {0, -512, 512},
    {-400, -625, 400}, {-577, -675, -577}, {0, -904, -946}, {96, -608, 0}, {0, -1157, 1106},
    {-793, -899, -771}, {0, 0, 0, -1},
};

/* Scratchpad work area of the camera path. */
typedef struct {
    VECTOR at;        /* 0x00: path point, then look-at */
    VECTOR eye;       /* 0x10 */
    s32 distance;     /* 0x20 */
    u8 pad24[0x7C];
    SVECTOR points[3]; /* 0xA0 */
} PathScratch;

#define PATH_SCRATCH ((PathScratch *)0x1F800000)

/* Scene camera flight along the path: follow the path points, speed up and
 * slow down by state, fade out at the end, and set the engine volume from the
 * eye's distance. */
s32 func_80077E68(s32 index) {
    WorldmapActor *actor;
    SVECTOR *points;
    s32 segment;
    s32 distance;
    PathScratch *scratch;

    actor = &D_8009BE24[index];
    scratch = PATH_SCRATCH;
    points = D_8009A3F0;
    segment = actor->u.step >> 12;
    points += segment;
    actor->unk54 = segment;
    if (points[2].pad != -1) {
        PATH_SCRATCH->points[0].vx = points[0].vx;
        PATH_SCRATCH->points[0].vz = points[0].vz;
        PATH_SCRATCH->points[1].vx = points[1].vx;
        PATH_SCRATCH->points[1].vz = points[1].vz;
        PATH_SCRATCH->points[2].vx = points[2].vx;
        PATH_SCRATCH->points[2].vz = points[2].vz;
        PATH_SCRATCH->points[0].vy = points[0].vy;
        PATH_SCRATCH->points[1].vy = points[1].vy;
        PATH_SCRATCH->points[2].vy = points[2].vy;
        func_80076858(actor->u.step & 0xFFF, &PATH_SCRATCH->points[0], &PATH_SCRATCH->points[1],
                      &PATH_SCRATCH->points[2], &scratch->at);
        VIEW.at.vz = 0;
        VIEW.at.vx = 0;
        VIEW.up.vz = 0;
        VIEW.up.vx = 0;
        VIEW.up.vy = -0x1000;
        VIEW.eye.vx = scratch->at.vx >> 16;
        VIEW.eye.vz = -(scratch->at.vz >> 16);
        VIEW.eye.vy = scratch->at.vy >> 16;
        VIEW.at.vy = D_8009BE28.target.vy >> 12;
    }
    func_80097244(&D_8009BD40);
    func_80097070(&D_8009C808, &D_8009BD38);
    switch (actor->state) {
    case 0:
        actor->unk58 += 5;
        if (--actor->wait < 0) {
            actor->unk58 = 0x80;
            actor->state++;
            scratch->points[0].vy = -0x20D;
            scratch->points[0].vx = (D_8009BE28.target.vx >> 12) + 0x32;
            scratch->points[0].vz = (D_8009BE28.target.vz >> 12) - 0x70;
            func_80089160(8, &scratch->points[0], NULL);
        }
        break;
    case 1:
        if (actor->u.step >= 0x6000) {
            actor->state++;
        }
        break;
    case 2:
        actor->unk58 -= 2;
        if (actor->unk58 < 0x19) {
            actor->unk58 = 0x18;
            actor->state++;
        }
        break;
    case 3:
        if (actor->u.step > 0x7FFF) {
            actor->state++;
        }
        break;
    case 4:
        actor->unk58 -= 1;
        if (actor->unk58 < 5) {
            actor->unk58 = 4;
            actor->state++;
        }
        break;
    case 5:
        if (actor->u.step > 0x83FF) {
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0xA4, 0, 0x100);
            func_80097770(0, 0xD);
            D_8009CCA4 = 2;
            D_8009D3CC = 4;
            actor->wait = 0x50;
            actor->unk58 = 0;
            actor->state++;
        }
        break;
    case 6:
        if (--actor->wait <= 0) {
            D_8009D554 = 0;
            D_8009D7CC = 0;
        }
        break;
    }
    actor->u.step += actor->unk58;
    if (actor->state < 6) {
        scratch->at.vx = VIEW.at.vx << 12;
        scratch->at.vy = VIEW.at.vy << 12;
        scratch->at.vz = VIEW.at.vz << 12;
        scratch->eye.vx = VIEW.eye.vx << 12;
        scratch->eye.vy = VIEW.eye.vy << 12;
        scratch->eye.vz = VIEW.eye.vz << 12;
        distance = func_80094154(&scratch->at, &scratch->eye) >> 3;
        scratch->distance = distance;
        sound_set_effect_volume((sound_effect_bank->id << 16) | 0xA4, 0x87 - distance);
    }
    return 1;
}

/* Link scene objects 1-13 to object 0 and put the scene camera on the player. */
s32 func_8007828C(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 1);
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    func_800848B4(0, 4);
    func_800848B4(0, 5);
    func_800848B4(0, 6);
    func_800848B4(0, 7);
    func_800848B4(0, 8);
    func_800848B4(0, 9);
    func_800848B4(0, 0xA);
    func_800848B4(0, 0xB);
    func_800848B4(0, 0xC);
    func_800848B4(0, 0xD);
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position = D_8009C5AC;
    actor->unk54 = 0x40;
    actor->unk58 = 0x200;
    actor->unk5C = 0x60;
    actor->unk60 = 0x300;
    actor->u.script = NULL;
    actor->unk64 = 0x50;
    D_8009BE28.target = actor->position;
    return 1;
}

/* Scratchpad work area of the scene rigs. */
typedef struct {
    VECTOR position;   /* 0x00 */
    u8 pad10[0x90];
    SVECTOR angle[4];  /* 0xA0 */
    u8 padC0[0x30];
    MATRIX matrix[4];  /* 0xF0 */
} RigScratch;

#define RIG_SCRATCH ((RigScratch *)0x1F800000)

/* Fly the scene rig forward: move the actor and the ground scroll, place scene
 * object 0 and the camera on it, spin the three rotors and share their
 * matrices across the rig's objects. */
s32 func_800783E8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vz -= 0x4000;
    GROUND_SCROLL[2] -= 0x4000;
    func_80093354(&actor->position);
    RIG_SCRATCH->position.vx = actor->position.vx >> 12;
    RIG_SCRATCH->position.vy = actor->position.vy >> 12;
    RIG_SCRATCH->position.vz = actor->position.vz >> 12;
    D_8009C620[0].position = RIG_SCRATCH->position;
    D_8009BE28.target = actor->position;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    actor->unk60 = (actor->unk60 + actor->unk64) & 0xFFF;
    RIG_SCRATCH->angle[0].vx = RIG_SCRATCH->angle[0].vz = RIG_SCRATCH->angle[1].vx =
        RIG_SCRATCH->angle[1].vz = RIG_SCRATCH->angle[2].vx = RIG_SCRATCH->angle[2].vz = 0;
    RIG_SCRATCH->angle[0].vy = actor->u.step;
    RIG_SCRATCH->angle[1].vy = actor->unk58;
    RIG_SCRATCH->angle[2].vy = actor->unk60;
    RIG_SCRATCH->angle[3].vx = RIG_SCRATCH->angle[3].vy = 0;
    RIG_SCRATCH->angle[3].vz = actor->unk60;
    RotMatrixYXZ(&RIG_SCRATCH->angle[0], &RIG_SCRATCH->matrix[0]);
    RotMatrixYXZ(&RIG_SCRATCH->angle[1], &RIG_SCRATCH->matrix[1]);
    RotMatrixYXZ(&RIG_SCRATCH->angle[2], &RIG_SCRATCH->matrix[2]);
    RotMatrixYXZ(&RIG_SCRATCH->angle[3], &RIG_SCRATCH->matrix[3]);
    D_8009C620[1].matrix = D_8009C620[2].matrix = D_8009C620[3].matrix = RIG_SCRATCH->matrix[3];
    D_8009C620[4].matrix = D_8009C620[5].matrix = RIG_SCRATCH->matrix[2];
    D_8009C620[6].matrix = D_8009C620[10].matrix = D_8009C620[8].matrix = D_8009C620[12].matrix =
        RIG_SCRATCH->matrix[0];
    D_8009C620[7].matrix = D_8009C620[11].matrix = D_8009C620[9].matrix = D_8009C620[13].matrix =
        RIG_SCRATCH->matrix[1];
    return 1;
}

/* Mode step that has nothing to do; always reports done. */
s32 func_80078948(void) {
    return 1;
}

/* Per-frame update and draw of the scene mode. */
s32 func_80078950(void) {
    if (D_8009D144 == 0) {
        func_80097440(&D_8009BD40);
    } else {
        func_80097244(&D_8009BD40);
    }
    func_80089748();
    func_80089C78();
    func_800848F4();
    func_800980D4(&D_8009BBB4);
    if (D_8009D558 != 0) {
        func_800981C8(&D_8009BE28);
        func_80096130();
        func_80098CC0();
    }
    func_800983A0(&D_8009BE28);
    func_8009932C(D_8009BE3C->ot, (s32)D_8009BE3C->packets, &D_8009BE28);
    D_8009C5BC += 0x40;
    func_80073B04();
    func_800737EC();
    func_80086798();
    return 1;
}

/* Set up the first cutscene mode: display, terrain loader, scene objects and
 * its actors. */
void func_80078A60(void) {
    RECT rect;

    func_80072BB0();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 2);
    while (cd_get_pending_read_count() >= 3) {
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
    cd_sync_reads(0);
    func_800721E4();
    D_8009C5AC.vx = 0x2000000;
    D_8009C5AC.vy = -0x300000;
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
    cd_sync_reads(0);
    sound_add_effect_bank(sound_effect_bank);
    cd_select_directory(0x24, 0);
    func_80097BC0(&D_8009C5AC);
    do {
        func_800967E4();
        VSync(0);
    } while (func_80096668() > 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_80078E2C, (s32)func_80078EA4);
    func_80097718((s32)func_800795E4, (s32)func_80079778);
    func_80097718((s32)func_8007A144, (s32)func_8007A1B4);
    func_80097718((s32)func_8007A410, (s32)func_8007A430);
    func_80097718((s32)func_8007A568, (s32)func_8007A570);
    func_80097718((s32)func_800794D8, (s32)func_80079538);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80075228();
}

/* Leave the scene: release its resources and continue in scene 0x110. */
void func_80078D24(void) {
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
    func_80084818();
    func_80086568();
    func_800866C8();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    heap_free(D_8009BBC8[0].ot);
    heap_free(D_8009BBC8[1].ot);
    heap_free(D_8009BBC8[0].packets);
    heap_free(D_8009BBC8[1].packets);
    heap_free(D_8009C180);
    func_800976A0();
    game_data.map = 0x110;
    game_data.entry[2] = 0;
    D_8009BBC4 = 1;
    game_data.entry[0] = D_8009BD38.vy;
}

/* Start a scripted camera looking down from yaw 0x680. */
s32 func_80078E2C(s32 index) {
    WorldmapActor *actor;

    D_8009BE0C = 0x78;
    D_8009D3F0 = 0x200000;
    D_8009D144 = 0;
    actor = &D_8009BE24[index];
    actor->state = 0x10;
    actor->unk58 = 0;
    actor->unk54 = 0;
    actor->u.script = NULL;
    D_8009BD38.vx = -0xC0;
    D_8009BD38.vy = 0x680;
    D_8009BD38.vz = 0;
    actor->wait = 0x40;
    actor->unk74 = 0;
    return 1;
}

/* Scene script: start the effects in turn, shake the view while the engines
 * run, then fade out; states 16-18 are the scene's opening. */
s32 func_80078EA4(s32 index) {
    CameraScratch *scratch;
    WorldmapActor *actor;
    s16 trigger;

    actor = &D_8009BE24[index];
    scratch = (CameraScratch *)0x1F800000;
    switch (actor->state) {
    case 0:
        if (--actor->wait <= 0) {
            actor->wait = 0x20;
            actor->state++;
            func_80097770(5, 1);
            sound_play_effect((sound_effect_bank->id << 16) | 0x62);
            sound_play_effect((sound_effect_bank->id << 16) | 0x63);
        }
        break;
    case 1:
        if (--actor->wait <= 0) {
            actor->wait = 0x20;
            actor->state++;
            func_80097770(3, 1);
        }
        break;
    case 2:
        if (--actor->wait <= 0) {
            actor->wait = 0x30;
            actor->state++;
            func_80097770(4, 1);
            func_80097770(2, 1);
            func_80097770(6, 1);
        }
        break;
    case 3:
        if (--actor->wait <= 0) {
            actor->wait = 0x28;
            actor->state++;
        }
        break;
    case 4:
        actor->wait--;
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        scratch->view.vx = rand() % 12 - 6;
        scratch->view.vy = rand() % 12 - 6;
        VIEW_VECTORS[0].vx += scratch->view.vx;
        VIEW_VECTORS[1].vx += scratch->view.vx;
        VIEW_VECTORS[0].vy += scratch->view.vy;
        VIEW_VECTORS[1].vy += scratch->view.vy;
        if (actor->wait <= 0) {
            actor->wait = 0x78;
            actor->state++;
            sound_play_effect((sound_effect_bank->id << 16) | 0x79);
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x62, 0, 0x100);
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x63, 0, 0x100);
        }
        break;
    case 5:
        actor->wait--;
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        scratch->view.vx = rand() % 4 - 2;
        scratch->view.vy = rand() % 4 - 2;
        VIEW_VECTORS[0].vx += scratch->view.vx;
        VIEW_VECTORS[0].vy += scratch->view.vy;
        VIEW_VECTORS[1].vx += scratch->view.vx;
        VIEW_VECTORS[1].vy += scratch->view.vy;
        if (actor->wait <= 0) {
            actor->wait = 0x5A;
            actor->state++;
        }
        break;
    case 6:
        if (--actor->wait <= 0) {
            actor->wait = 0x50;
            actor->state++;
            func_80097770(0, 0xD);
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x79, 0, 0x100);
            D_8009CCA4 = 2;
            D_8009D3CC = 4;
        }
        break;
    case 7:
        if (--actor->wait <= 0) {
            actor->wait = 0;
            D_8009D554 = 0;
            D_8009D7CC = 0;
        }
        break;
    case 16:
        trigger = actor->unk4;
        if (trigger == 1) {
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x36, 0, 8);
            func_80097770(0, 0xD);
            D_8009D3CC = 0x20;
            D_8009CCA4 = trigger;
            actor->wait = 8;
            actor->unk4 = 0;
            actor->state++;
        }
        break;
    case 17:
        if (--actor->wait <= 0) {
            D_8009D3F0 = 0x960000;
            D_8009BD38.vx = -0x30;
            D_8009BD38.vy = 0x40;
            D_8009BD38.vz = 0;
            actor->wait = 0x10;
            actor->state++;
        }
        break;
    case 18:
        if (--actor->wait <= 0) {
            func_80097770(0, 0xC);
            D_8009CCA4 = 1;
            D_8009D3CC = 0x80;
            actor->state = 0;
            actor->wait = 1;
        }
        break;
    }
    if (actor->state != 4 && actor->state != 5) {
        func_80096F18(&D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
    }
    return 1;
}

/* Place the scene camera target and set the scene yaw. */
s32 func_800794D8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->position.vx = 0x2000000;
    actor->position.vz = 0x1500000;
    actor->state = 0;
    D_8009C620[16].angle.vx = 0;
    D_8009C620[16].angle.vy = 0x780;
    D_8009C620[16].angle.vz = 0;
    RotMatrixYXZ(&D_8009C620[16].angle, &D_8009C620[16].matrix);
    return 1;
}

/* Keep the actor on the ground and scene object 16 at it. */
s32 func_80079538(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        D_8009C620[16].visible = 1;
    }
    func_80093354(&actor->position);
    actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) + 0x18000;
    D_8009C620[16].position.vx = actor->position.vx >> 12;
    D_8009C620[16].position.vy = actor->position.vy >> 12;
    D_8009C620[16].position.vz = actor->position.vz >> 12;
    return 1;
}

/* Link scene objects 1-13 to object 0, put the camera on the player, tilt
 * object 0 and play sound 0x36 of the area bank. */
s32 func_800795E4(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 1);
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    func_800848B4(0, 4);
    func_800848B4(0, 5);
    func_800848B4(0, 6);
    func_800848B4(0, 7);
    func_800848B4(0, 8);
    func_800848B4(0, 9);
    func_800848B4(0, 0xA);
    func_800848B4(0, 0xB);
    func_800848B4(0, 0xC);
    func_800848B4(0, 0xD);
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position = D_8009C5AC;
    actor->unk54 = 0x40;
    actor->unk58 = 0x200;
    actor->unk5C = 0x60;
    actor->unk60 = 0x300;
    actor->u.script = NULL;
    actor->unk64 = 0x50;
    D_8009BE28.target = actor->position;
    D_8009C620[0].angle.vx = -0x100;
    D_8009C620[0].angle.vy = 0;
    D_8009C620[0].angle.vz = -0x40;
    sound_play_effect((sound_effect_bank->id << 16) | 0x36);
    return 1;
}

/* Scene rig landing: drop and brake the rig with exhaust effects, then show all
 * its objects; every frame place it and spin its rotors. */
s32 func_80079778(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    RigScratch *scratch;

    actor = &D_8009BE24[index];
    objects = D_8009C620;
    scratch = (RigScratch *)0x1F800000;
    switch (actor->state) {
    case 0:
        scratch->angle[0].vx = actor->position.vx >> 12;
        scratch->angle[0].vy = actor->position.vy >> 12;
        scratch->angle[0].vz = actor->position.vz >> 12;
        func_80089160(0xB, &scratch->angle[0], NULL);
        actor->position.vy += 0x4000;
        actor->position.vz -= 0x8000;
        GROUND_SCROLL[2] -= 0x8000;
        if (actor->position.vy >= -0x18000) {
            actor->motion.vy = -0x4000;
            actor->state++;
            func_80089160(0xC, &scratch->angle[0], NULL);
            sound_play_effect((sound_effect_bank->id << 16) | 0x71);
            actor->wait = 0x20;
        }
        break;
    case 1:
        scratch->angle[0].vx = actor->position.vx >> 12;
        scratch->angle[0].vy = actor->position.vy >> 12;
        scratch->angle[0].vz = actor->position.vz >> 12;
        func_80089160(0xB, &scratch->angle[0], NULL);
        func_80089160(0xC, &scratch->angle[0], NULL);
        if (--actor->wait <= 0) {
            func_800894C8(0xC);
        }
        objects[0].angle.vx += 4;
        objects[0].angle.vz += 2;
        actor->position.vy += actor->motion.vy;
        actor->motion.vy += 0x100;
        actor->position.vz -= 0x8000;
        GROUND_SCROLL[2] -= 0x8000;
        if (actor->position.vy >= -0x18000) {
            actor->motion.vz = -0x8000;
            actor->state++;
            func_80089160(0xC, &scratch->angle[0], NULL);
            sound_play_effect((sound_effect_bank->id << 16) | 0x71);
        }
        break;
    case 2:
        scratch->angle[0].vx = actor->position.vx >> 12;
        scratch->angle[0].vy = actor->position.vy >> 12;
        scratch->angle[0].vz = actor->position.vz >> 12;
        func_80089160(0xB, &scratch->angle[0], NULL);
        func_80089160(0xC, &scratch->angle[0], NULL);
        objects[0].angle.vx -= 2;
        objects[0].angle.vz -= 1;
        actor->position.vz += actor->motion.vz;
        GROUND_SCROLL[2] += actor->motion.vz;
        actor->motion.vz += 0x100;
        if (actor->position.vz <= 0x1580000) {
            actor->wait = 8;
            actor->state++;
            func_800894C8(0xC);
            func_80097770(1, 1);
        }
        break;
    case 3:
        objects[0].angle.vx -= 2;
        objects[0].angle.vz -= 1;
        actor->position.vz += actor->motion.vz;
        GROUND_SCROLL[2] += actor->motion.vz;
        actor->motion.vz += 0x100;
        if (--actor->wait <= 0) {
            func_80097770(1, 1);
            actor->state++;
        }
        break;
    case 4:
        if (actor->unk4 != 0) {
            actor->unk4 = 0;
            objects[0].visible = objects[1].visible = objects[2].visible = objects[3].visible =
                objects[4].visible = objects[5].visible = objects[6].visible = objects[7].visible =
                    objects[8].visible = objects[9].visible = objects[10].visible = objects[11].visible =
                        objects[12].visible = objects[13].visible = 1;
            func_800894C8(0xB);
        }
        break;
    }
    func_80093354(&actor->position);
    D_8009C620[0].position.vx = actor->position.vx >> 12;
    D_8009C620[0].position.vy = actor->position.vy >> 12;
    D_8009C620[0].position.vz = actor->position.vz >> 12;
    D_8009BE28.target = actor->position;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    actor->unk60 = (actor->unk60 + actor->unk64) & 0xFFF;
    scratch->angle[0].vx = scratch->angle[0].vz = scratch->angle[1].vx = scratch->angle[1].vz =
        scratch->angle[2].vx = scratch->angle[2].vz = 0;
    scratch->angle[0].vy = actor->u.step;
    scratch->angle[1].vy = actor->unk58;
    scratch->angle[2].vy = actor->unk60;
    scratch->angle[3].vx = scratch->angle[3].vy = 0;
    scratch->angle[3].vz = actor->unk60;
    RotMatrixYXZ(&scratch->angle[0], &scratch->matrix[0]);
    RotMatrixYXZ(&scratch->angle[1], &scratch->matrix[1]);
    RotMatrixYXZ(&scratch->angle[2], &scratch->matrix[2]);
    RotMatrixYXZ(&scratch->angle[3], &scratch->matrix[3]);
    D_8009C620[1].matrix = D_8009C620[2].matrix = D_8009C620[3].matrix = scratch->matrix[3];
    D_8009C620[4].matrix = D_8009C620[5].matrix = scratch->matrix[2];
    D_8009C620[6].matrix = D_8009C620[10].matrix = D_8009C620[8].matrix = D_8009C620[12].matrix =
        scratch->matrix[0];
    D_8009C620[7].matrix = D_8009C620[11].matrix = D_8009C620[9].matrix = D_8009C620[13].matrix =
        scratch->matrix[1];
    RotMatrixYXZ(&D_8009C620[0].angle, &scratch->matrix[0]);
    D_8009C620[0].matrix = scratch->matrix[0];
    return 1;
}

/* Build `count` semi-transparent textured quads for a scene sprite. */
void func_8007A06C(SceneObject *object, POLY_FT4 *quads, s32 count) {
    POLY_FT4 *quad;
    s32 i;

    quad = quads;
    for (i = 0; i < count; i++) {
        setPolyFT4(quad);
        quad->tpage = GetTPage(1, 3, 0x340, 0x100);
        quad->clut = GetClut(0x100, 0x1FF);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT4));
}

/* Rebuild both scene sprites' quads. */
s32 func_8007A144(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->unk70 = 0;
    actor->unk6C = 0;
    func_8007A06C(&D_8009C620[14], D_8009C620[14].prims, D_8009C620[14].def->primitive_count);
    func_8007A06C(&D_8009C620[15], D_8009C620[15].prims, D_8009C620[15].def->primitive_count);
    return 3;
}

/* Grow scene sprites 14 and 15 at scene object 0: sprite 14 widens each frame,
 * sprite 15 steps once 14 is full; done (3) when 15 reaches 0x6000. */
s32 func_8007A1B4(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;
    s32 result;

    objects = D_8009C620;
    actor = &D_8009BE24[index];
    objects[14].position = objects[0].position;
    objects[15].position = objects[14].position;
    actor->unk6C += 0xC0;
    result = 1;
    if (actor->unk6C >= 0x800) {
        actor->unk70 = (actor->unk70 + 0x100) & 0x7FFF;
    }
    if (actor->unk70 >= 0x6000) {
        result = 3;
        actor->unk4 = 0;
        actor->unk70 = 0;
        actor->unk6C = 0;
    }
    SCALE_SCRATCH->matrix[0] = D_8009A180;
    SCALE_SCRATCH->matrix[1] = SCALE_SCRATCH->matrix[0];
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->unk6C;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk70;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&SCALE_SCRATCH->matrix[0], &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&SCALE_SCRATCH->matrix[1], &SCALE_SCRATCH->scale[1]);
    objects[14].matrix = SCALE_SCRATCH->matrix[0];
    objects[15].matrix = SCALE_SCRATCH->matrix[1];
    return result;
}

/* Wait 0x60 frames. */
s32 func_8007A410(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->wait = 0x60;
    return 3;
}

/* Carry effect 9 forward from scene object 0 for `wait` frames, then stop it. */
s32 func_8007A430(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (actor->unk4 == result) {
        actor->unk4 = 0;
        actor->position.vx = D_8009C620[0].position.vx << 12;
        actor->position.vy = D_8009C620[0].position.vy << 12;
        actor->position.vz = D_8009C620[0].position.vz << 12;
    }
    if (--actor->wait > 0) {
        actor->position.vz += 0x20000;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        func_80089160(9, SCRIPT_VECTOR, NULL);
    } else {
        actor->wait = 0x60;
        actor->position = D_8009C5AC;
        func_800894C8(9);
        result = 3;
    }
    return result;
}

/* Scene step with nothing to do. */
s32 func_8007A568(void) {
    return 3;
}

/* Emit effect 0xA at the scene position. */
s32 func_8007A570(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->unk4 = 0;
    SCRIPT_VECTOR->vx = D_8009C620[0].position.vx;
    SCRIPT_VECTOR->vy = D_8009C620[0].position.vy;
    SCRIPT_VECTOR->vz = D_8009C620[0].position.vz;
    func_80089160(0xA, SCRIPT_VECTOR, 0);
    return 3;
}

/* Set up the second cutscene mode: display, terrain loader, scene objects and
 * its actors. */
void func_8007A5DC(void) {
    RECT rect;

    func_80072BB0();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    func_80072DB4(0x40, 0, 4, 2);
    while (cd_get_pending_read_count() >= 3) {
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
    cd_sync_reads(0);
    func_800721E4();
    D_8009C5AC.vx = 0x5BED000;
    D_8009C5AC.vy = -0xA0000;
    D_8009C5AC.vz = 0x62A8000;
    func_80084580();
    func_8008440C();
    func_800979C8();
    func_800736DC();
    func_800863E0();
    func_80074E58();
    func_80075030();
    func_800739B8();
    func_80088F64();
    cd_sync_reads(0);
    sound_add_effect_bank(sound_effect_bank);
    cd_select_directory(0x24, 0);
    func_80097BC0(&D_8009C5AC);
    do {
        func_800967E4();
        VSync(0);
    } while (func_80096668() > 0);
    func_80097718((s32)func_800923A8, (s32)func_800925A0);
    func_80097718((s32)func_8007A9B4, (s32)func_8007A9F8);
    func_80097718((s32)func_8007AD34, (s32)func_8007ADD4);
    func_80097718((s32)func_8007B200, (s32)func_8007B394);
    func_80097718((s32)func_8007B604, (s32)func_8007B798);
    func_80097718((s32)func_8007BA08, (s32)func_8007BA10);
    func_80097718((s32)func_8007BB60, (s32)func_8007BBEC);
    func_80097718((s32)func_80078948, (s32)func_80078950);
    func_800978FC();
    func_8008901C();
    func_800865A0();
    func_80075228();
}

/* Leave the scene: release its resources and continue in scene 0x11A. */
void func_8007A8AC(void) {
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
    func_80084818();
    func_80086568();
    func_800866C8();
    func_80074F04();
    func_800750DC();
    func_80088FF4();
    func_80089128();
    func_80097D64();
    heap_free(D_8009BBC8[0].ot);
    heap_free(D_8009BBC8[1].ot);
    heap_free(D_8009BBC8[0].packets);
    heap_free(D_8009BBC8[1].packets);
    heap_free(D_8009C180);
    func_800976A0();
    game_data.map = 0x11A;
    game_data.entry[2] = 0;
    D_8009BBC4 = 1;
    game_data.entry[0] = D_8009BD38.vy;
}

/* Restart an actor's timed sequence at its first step. */
s32 func_8007A9B4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0;
    actor->state = D_8009A450[0];
    actor->wait = D_8009A46C[actor->u.step];
    return 1;
}
