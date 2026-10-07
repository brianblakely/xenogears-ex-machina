/* Battle unit from 800B7870 to 800B8098 (Cygnus CDK GCC 2.7.2).
 * 800B7870's jump table at 0x800709FC sits at 4 mod 8 directly after
 * 800B3F04's odd-length table at 0 mod 8, so a unit starts between the two
 * (placed at the first function with rodata); its own 5-entry table is
 * followed directly by 800B8098's at 0x80070A10 (0 mod 8). */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "effect.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
#include "actor.h"
#include "popup.h"
#include "frame.h"
#include "stage.h"
#include "action_file.h"

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
    BattleSprite *sprite;

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
    func_800295D8(file, (s32)D_800594F0, 0, 0x80);
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
    BattleTask wait;
    SpriteResource saved;
    BattleSprite *actor;
    BattleSprite *runner;
    s32 own;
    SpriteResource *resource;

    actor = D_800C3E1C;
    func_8001CC18(0, &wait);
    wait.update = NULL;
    func_800B8354();
    resource = (SpriteResource *)D_8005A474;
    at.x = 0x380;
    at.y = 0x100;
    clut.x = 0;
    clut.y = 0x1F4;
    D_800C3624 = 0;
    func_80022224(resource, D_800594F0, at, clut, 0);
    own = 0;
    func_800BEB04();
    if (func_8001EE68(resource->frames)) {
        saved = *(SpriteResource *)D_800C3E1C->base;
        runner = func_80023B84(D_800C3E1C, (void *)(resource->motions[D_800C3DF0 + 1] + (s32)resource->motions), resource);
    } else {
        own = 1;
        runner = D_800C3E1C;
        func_80021BF0(runner, D_800594F0);
        func_800245D8(runner, -1);
    }
    actor->sound = runner->sound = func_800C0FAC(D_800594F0);
    D_800D3350 = 1;
    D_800C35D4 = 1;
    func_8003A89C(D_800C3E54, 0x60, 0x78);
    func_8001CD94(&wait);
    return own;
}

/* Set the acting sprite of a single action. */
void func_800B8048(BattleSprite *sprite) {
    D_800C3E1C = sprite;
}

/* Request sound (run by the frame loop, 800B8068). */
void func_800B8054(s32 sound) {
    D_800591B4 = sound;
    D_800591B1 = 0;
}

/* Run a requested single action: load its command file (800B7C34) and
 * start it (800B7E94); mark it done. */
void func_800B8068(s32 sound) {
    func_800B7C34(sound);
    func_800B7E94();
    D_800591B1 = 1;
}
