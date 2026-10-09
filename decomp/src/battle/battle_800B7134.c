/* Battle unit from 800B7134 to 800B8098: the shattered screen's draw and
 * set-up, the battle's intro swirl and the single actions' command files
 * (800B7870-800B8068), built by the Cygnus CDK GCC 2.7.2. 800B7870's jump
 * table at 0x800709FC sits at 4 mod 8 directly after 800B3F04's odd-length
 * table at 0 mod 8, so a unit starts between the two functions (that table is
 * all of 800B3F04's unit's rodata). The screen shatter's draw (800B7134,
 * 800B7160) and the intro swirl 800B7870 share the unit's own variable
 * D_800C3CB4, which follows 800B3F04's .bss, so the unit starts at 800B7134
 * at the latest; that is where it is placed. Its own 5-entry table is
 * followed directly by 800B8098's at 0x80070A10 (0 mod 8). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/stream.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/sprite.h"
#include "battle/stage.h"
#include "overlays.h"
#include "own_declarations.h"
#include "resident_views.h"
#include "sprite_effect.h"

/* This unit's functions, declared before their first use. */
void func_800B7160(Task *draw);
ScreenShatter *func_800B7424(ScreenShatter *shatter);

/* The unit's own uninitialized variable (its .bss, after
 * battle_800B3F04.c's). */
static u32 *D_800C3CB4; /* the ordering table the shatter draws into */

/* Shatter draw: into the ordering table (800B7160). */
void func_800B7134(Task *draw) {
    D_800C3CB4 = (u32 *)D_8005956C;
    func_800B7160(draw);
}

/* Shatter draw: each shard that has fallen in front of the screen (z at
 * least 64), its layer's triangle turned and placed, projected at the
 * screen centre and distance 512. */
void func_800B7160(Task *draw) {
    ScreenShatter *shatter = draw->data;
    s32 offsetX;
    s32 offsetY;
    s32 screen;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVECTOR *triangle;

    ReadGeomOffset(&offsetX, &offsetY);
    screen = ReadGeomScreen();
    SetGeomOffset(160, 112);
    SetGeomScreen(512);
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                poly = &shard->poly[BATTLE_AREA.buffer];
                if (shard->position.vz >= 64) {
                    MATRIX m;
                    s32 p;
                    s32 flag;
                    s32 depth;

                    func_8003F738(&shard->angles, &m);
                    TransMatrix(&m, &shard->position);
                    SetRotMatrix(&m);
                    SetTransMatrix(&m);
                    if (layer == 0) {
                        triangle = D_800C3594;
                    } else {
                        triangle = D_800C35AC;
                    }
                    depth = RotTransPers3(&triangle[0], &triangle[1], &triangle[2], (u32 *)&poly->x0,
                                          (u32 *)&poly->x1, (u32 *)&poly->x2, &p, &flag) >> 6;
                    AddPrim(D_800C3CB4 + depth, poly);
                }
            }
        }
    }
    SetGeomOffset(offsetX, offsetY);
    SetGeomScreen(screen);
}

/* Free a heap block once drawing is done. */
void func_800B7330(void *block) {
    DrawSync(0);
    func_800320E8(block);
}

/* Shatter destroy: end the draw task, the task and its sprites. */
void func_800B7364(ScreenShatter *shatter) {
    func_8001CB48(&shatter->draw);
    func_8001CD94(&shatter->task);
    func_80025180((u32)shatter);
}

/* Shatter the screen copied to VRAM (0x2C0, 0x100). */
void func_800B73A0(void) {
    func_800B7424((ScreenShatter *)func_8001D1D8(sizeof(ScreenShatter), NULL, func_800B6F0C, func_800B7134,
                                                 (void (*)(Task *))func_800B7364));
}

/* Set up a shattered screen in a heap block (not run as a task). */
ScreenShatter *func_800B73EC(void) {
    ScreenShatter *shatter = func_80031BDC(sizeof(ScreenShatter), 1);

    shatter->task.data = shatter;
    shatter->draw.data = shatter;
    return func_800B7424(shatter);
}

/* Cut the screen copied to VRAM (0x2C0, 0x100) into shards: per 16 x 16
 * cell an upper-left and a lower-right triangle, each starting further out
 * the later it moves, launched outwards at a random speed with a random spin
 * and fall. */
ScreenShatter *func_800B7424(ScreenShatter *shatter) {
    ScreenShard *shard;
    POLY_FT3 *poly;
    VECTOR square;
    SVECTOR angles;
    MATRIX m;
    s32 radius;
    s32 row;
    s32 layer;
    s32 column;
    s32 i;
    s32 distance;
    s32 turn;
    s32 tilt;
    s32 r;
    s32 base;
    s32 yaw;

    shatter->frame = 0;
    radius = SquareRoot0(160 * 160 + 112 * 112) << 10;
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                shard->angles.vx = 0;
                shard->angles.vy = 0;
                shard->angles.vz = 0;
                if (layer == 0) {
                    shard->position.vx = (column * 16 - 155) * 32;
                    shard->position.vy = (row * 16 - 107) * 32;
                    shard->position.vz = 0x4000;
                } else {
                    shard->position.vx = (column * 16 - 149) * 32;
                    shard->position.vy = (row * 16 - 101) * 32;
                    shard->position.vz = 0x4000;
                }
                D_800C35C4.vz = -500 << 16;
                D_800C35C4.vz = D_800C35C4.vz + (-(rand() % 1000) << 16);
                func_8004A414(&shard->position, &square);
                distance = SquareRoot0(square.vx + square.vy);
                shard->delay = (radius / 32 - distance) / 2048; /* overwritten */
                shard->delay = distance / 1024;
                /* Turn outwards, a little at random; tilt by the distance. */
                turn = ratan2(shard->position.vy, shard->position.vx);
                r = rand();
                yaw = (turn += 0x600) + r % 1024;
                tilt = (distance << 11) / radius;
                r = rand();
                base = tilt - 0x20;
                tilt = base + r % 64;
                angles.vx = 0;
                angles.vy = tilt;
                angles.vz = yaw;
                func_8004ABBC(&angles, &m);
                ApplyMatrixLV(&m, &D_800C35C4, &shard->velocity);
                shard->fall = 0x70800 - ((rand() % 1600) << 8);
                shard->spin.vx = (rand() & 0xFF) - 0x7F;
                shard->spin.vy = (rand() & 0xFF) - 0x7F;
                shard->spin.vz = (rand() & 0x1FF) - 0xFF;
                for (i = 0; i != 2; i++) {
                    poly = &shard->poly[i];
                    SetPolyFT3(poly);
                    SetShadeTex(poly, 0);
                    poly->r0 = 0xFF;
                    poly->g0 = 0xFF;
                    poly->b0 = 0xFF;
                    setSemiTrans(poly, 0);
                    poly->tpage = GetTPage(2, 1, column * 16 + 0x2C0, 0x100);
                    if (layer == 0) {
                        poly->u0 = column * 16 & 0x3F;
                        poly->v0 = row * 16;
                        poly->u1 = (column * 16 & 0x3F) + 16;
                        poly->v1 = row * 16;
                        poly->u2 = column * 16 & 0x3F;
                        poly->v2 = row * 16 + 16;
                    } else {
                        poly->u0 = (column * 16 & 0x3F) + 16;
                        poly->v0 = row * 16;
                        poly->u1 = (column * 16 & 0x3F) + 16;
                        poly->v1 = row * 16 + 16;
                        poly->u2 = column * 16 & 0x3F;
                        poly->v2 = row * 16 + 16;
                    }
                }
            }
        }
    }
    return shatter;
}

/* The battle's intro swirl: the screen shatters (800B73EC) while the
 * battle module's set-up phases 0-2 and the scene files load, one step per
 * frame once the disc is idle, for at least 86 frames; then phase 3. The
 * screen copied to VRAM (0x2C0, 0x100) with its pixels made opaque feeds
 * the shards, and the background fades from white. */
void func_800B7870(void) {
    RECT rect;
    u16 *pixels;
    u16 *pixel;
    s32 i;
    s32 frames;
    s32 step;
    s32 phase;
    FrameBuffer *buffer;
    BattleArea *area;
    BattleArea *frame;
    FrameBuffer *first;
    FrameBuffer *next;
    ScreenShatter *shatter;

    frames = 86;
    func_8001C944();
    step = 1;
    phase = 0;
    pixels = func_80031BDC(0x30000, 1);
    pixel = pixels;
    rect.x = 0;
    rect.y = 0;
    rect.w = 320;
    rect.h = 224;
    StoreImage(&rect, (u_long *)pixels);
    DrawSync(0);
    for (i = 0; i != 320 * 256; i++) {
        *pixel++ |= 0x8000;
    }
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 320;
    rect.h = 224;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    func_800320E8(pixels);
    area = &BATTLE_AREA;
    buffer = &area->buffers[0];
    if (area->current == (first = buffer)) {
        buffer = &area->buffers[1];
    }
    area->current = buffer;
    area->ot = buffer->ot;
    ClearOTagR(buffer->ot, 0x1000);
    area->buffer = 0;
    area->current = first;
    area->buffers[0].drawEnv.isbg = 1;
    area->buffers[1].drawEnv.isbg = 1;
    area->buffers[0].drawEnv.r0 = 0xFF;
    area->buffers[1].drawEnv.r0 = 0xFF;
    area->buffers[0].drawEnv.g0 = 0xFF;
    area->buffers[1].drawEnv.g0 = 0xFF;
    area->buffers[0].drawEnv.b0 = 0xFF;
    area->buffers[1].drawEnv.b0 = 0xFF;
    shatter = func_800B73EC();
    while (frames != 0 || step != 5) {
        if (frames > 0) {
            frames--;
        }
        frame = &BATTLE_AREA;
        next = &frame->buffers[0];
        if (frame->current == next) {
            next = &frame->buffers[1];
        }
        frame->current = next;
        frame->ot = next->ot;
        ClearOTagR(next->ot, 0x1000);
        frame->buffer = 1 - frame->buffer;
        D_800C3CB4 = frame->ot;
        if (func_800286CC() == 0) {
            switch (step) {
            case 0:
                break;
            case 2:
                func_8001BB0C();
                step++;
                break;
            case 1:
            case 3:
            case 4:
                func_801E5840(phase);
                phase++;
                step++;
                break;
            }
        }
        func_80019CA0();
        SPAD_STACK_ENTER();
        func_800B6F0C(&shatter->task);
        func_800B7160(&shatter->task);
        SPAD_STACK_LEAVE();
        DrawSync(0);
        VSync(2);
        BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.r0 = func_80021AD8(BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.r0, -12);
        BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.g0 = func_80021AD8(BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.g0, -12);
        BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.b0 = func_80021AD8(BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.b0, -12);
        PutDispEnv(&BATTLE_AREA.current->dispEnv);
        PutDrawEnv(&BATTLE_AREA.current->drawEnv);
        DrawOTag((u_long *)&BATTLE_AREA.current->ot[0xFFF]);
    }
    func_800B7330(shatter);
    SetDispMask(0);
    func_80028A60(0);
    func_801E5840(3);
}

/* Clear D_800D2FDC. */
void func_800B7C28(void) {
    D_800D2FDC = 0;
}

/* Load single action index's command file (0x22 + 2 * index) and play its
 * stream (0x23 + 2 * index); action 0xE3 first puts back the VRAM columns
 * saved by 800B3E04. The file's first part says which gears restart: all
 * (1) or all but the acting sprite's (2). */
void func_800B7C34(s32 index) {
    RECT rect;
    s32 file;
    s32 stream;
    s32 restart;
    Sprite *sprite;

    if (index == 0xE3) {
        rect.w = 0x40;
        rect.h = 0x100;
        rect.x = D_800C3668[0].x;
        rect.y = D_800C3668[0].y;
        MoveImage(&rect, 0x280, 0x100);
        rect.w = 0x40;
        rect.h = 0x100;
        rect.x = D_800C3668[1].x;
        rect.y = D_800C3668[1].y;
        MoveImage(&rect, 0x240, 0x100);
        rect.w = 0x40;
        rect.h = 0x100;
        rect.x = D_800C3668[2].x;
        rect.y = D_800C3668[2].y;
        MoveImage(&rect, 0x200, 0x100);
        DrawSync(0);
    }
    func_800B8D7C();
    func_80028470(0xC, 2);
    file = index * 2 + 0x22;
    stream = index * 2 + 0x23;
    D_800C3CEC = 1;
    D_800D2FDC = 1;
    D_800594F0 = func_80031BDC(func_800288EC(file), 0);
    func_800295D8(file, D_800594F0, 0, 0x80);
    func_800B8354();
    restart = (*(u16 *)(D_800594F0[1] + (s32)D_800594F0) >> 12) & 3;
    if (restart != 0) {
        D_800C362C = restart;
        sprite = D_800C3E1C;
        func_800BC404(0);
        if (restart == 1) {
            func_800BB080(-1);
            D_800C3666 = 0;
        } else {
            D_800C3666 &= ~(1 << SPRITE_SLOT(sprite));
            func_800BB080(SPRITE_SLOT(sprite));
        }
    }
    D_800594BC = func_8002A260(8, 0);
    if (func_800288EC(stream) > 0x10) {
        func_80029EB0(stream, D_800594BC, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    func_800B8354();
    DrawSync(0);
    D_800C3618 = D_800594F0;
    func_800320E8(D_800594BC);
}

/* Start the loaded command file for the acting sprite: upload its images
 * and, when its frames take their image from the sequencer, give the
 * sprite an effect sprite running its command motion (else the file becomes
 * the sprite's own resource); start its sound bank. 1 when the sprite runs
 * it itself. */
u8 func_800B7E94(void) {
    VramPoint at;
    VramPoint clut;
    Task wait;
    SpriteSource saved;
    Sprite *actor;
    Sprite *runner;
    s32 own;
    SpriteSource *resource;

    actor = D_800C3E1C;
    func_8001CC18(0, &wait);
    wait.update = NULL;
    func_800B8354();
    resource = (SpriteSource *)D_8005A474;
    at.x = 0x380;
    at.y = 0x100;
    clut.x = 0;
    clut.y = 0x1F4;
    D_800C3624 = 0;
    func_80022224(resource, D_800594F0, at, clut, 0);
    own = 0;
    func_800BEB04();
    if (func_8001EE68((u8 *)resource->frames)) {
        saved = *(SpriteSource *)D_800C3E1C->image;
        runner = func_80023B84(D_800C3E1C, (void *)(resource->animations[D_800C3DF0 + 1] + (s32)resource->animations), resource);
    } else {
        own = 1;
        runner = D_800C3E1C;
        func_80021BF0(runner, (s32)D_800594F0);
        func_800245D8(runner, -1);
    }
    actor->word50 = runner->word50 = (s32)func_800C0FAC(D_800594F0);
    D_800D3350 = 1;
    D_800C35D4 = 1;
    func_8003A89C((SoundSeq *)D_800C3E54, 0x60, 0x78);
    func_8001CD94(&wait);
    return own;
}

/* Set the acting sprite of a single action. */
void func_800B8048(Sprite *sprite) {
    D_800C3E1C = sprite;
}

/* Request single action `action`; the frame loop runs it (800B8068). */
void func_800B8054(s32 action) {
    D_800591B4 = action;
    D_800591B1 = 0;
}

/* Run a requested single action: load its command file (800B7C34) and
 * start it (800B7E94); mark it done. */
void func_800B8068(s32 action) {
    func_800B7C34(action);
    func_800B7E94();
    D_800591B1 = 1;
}
