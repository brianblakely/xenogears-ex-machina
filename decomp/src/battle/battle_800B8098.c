/* Battle unit from 800B8098 to 800BD3AC: the battle's start and close and the
 * loads it waits for, the acting slot's turn run through the battle menu
 * (walks, sounds, the turn cancel), the slots' sprites and gear objects, the
 * battle camera, and the slot highlights and results. It is built like
 * battle_800B15D8.c by the Cygnus CDK GCC 2.7.2. Its jump tables sit at
 * 0 mod 8 (800B8098's at 0x80070A10) where the previous unit's sit at 4
 * (800B7870's at 0x800709FC), so a unit starts between the two; the functions
 * from 800B7C28 to 800B8068 have no rodata, and the boundary is placed at the
 * first function that has. Its last table (800B9F78's, 9 entries at
 * 0x80070AB8) is followed directly by 800BD3AC's at 0x80070ADC (4 mod 8), so
 * the unit ends before 800BD3AC. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/command.h"
#include "battle/effect_script.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/highlight.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/sprite.h"
#include "battle/stage.h"
#include "battle/turn.h"
#include "battle/windows.h"
#include "files.h"
#include "overlays.h"
#include "own_declarations.h"
#include "popup.h"
#include "resident_views.h"
#include "settle.h"
#include "sprite_effect.h"

/* Functions of other units declared as this unit calls them, which differs
 * from their definitions. */
void func_800A5E9C(u8 *first, u8 *second); /* the two buffers' background colours (two words there) */
void func_800AA760(s32 index, s32 value);  /* set stage object index's byte 0x2A (a u8 there) */
/* Defined without a return value: 800B89FC takes what it leaves in v0, the
 * new acting sprite. */
Sprite *func_800BEFF4(s32 slot);

/* This unit's functions, declared before their first use. */
void func_800B8284(void);
void func_800B8840(void);
void func_800B88C4(void);
void func_800B89F4(void);
void func_800B9508(Sprite *sprite);
void func_800B9B54(Sprite *sprite, Sprite *other);
void func_800BADD4(s32 slot);
void func_800BB7F8(void);
void func_800BC454(s16 value);
void func_800BC460(u32 mask);
void func_800BCD8C(void);
void func_800BCFAC(Task *task);
void func_800BD098(SpriteTask *owner);

/* The unit's own uninitialized variables (its .bss, after
 * battle_800B7134.c's; ASPSX 2.56 aligns each by its size up to a word:
 * decomp/Makefile). */
static u8 D_800C3CB8;      /* gear file reads running */
static s32 D_800C3CBC;
static s32 D_800C3CC0;     /* camera mode */
static u8 D_800C3CC4;      /* eye and look-at sprites running */
static s32 D_800C3CC8;     /* unreferenced */
static SVECTOR D_800C3CCC; /* the eye point saved while the camera sprites run */
static SVECTOR D_800C3CD4; /* the look-at point saved while they run */
static s32 D_800C3CDC;     /* the framed camera range */
static s32 D_800C3CE0[2];  /* unreferenced */

s32 D_800C35D8 = 0;
BattleSound D_800C35DC[] = {
    {0x1F, 0x0F}, {0x26, 0x13}, {0x2F, 0x12}, {0x36, 0x0D}, {0x3E, 0x11}, {0x44, 0x1B},
    {0x4A, 0x16}, {0x50, 0x12}, {0x57, 0x01}, {0x59, 0x01}, {0x5B, 0x02},
};
u16 D_800C3608 = 0;
s32 D_800C360C = 0;
BattleMenu *D_800C3610 = NULL;
s16 D_800C3614 = 0;
void *D_800C3618 = NULL;
s32 D_800C361C = 0;
u8 D_800C3620 = 0;
u8 D_800C3621 = 0;
u8 D_800C3622 = 0;
u8 D_800C3623 = 0;
u8 D_800C3624 = 0;
u16 D_800C3626 = 0;
s32 D_800C3628 = 0;
u8 D_800C362C = 0;
s16 D_800C3630[] = {0, 0x16, 0x2F, 0x4A, 0x5E, 0x71, 0x7C, 0x8F, 0x97, 0x2F, 0x7C};
s16 D_800C3648[] = {6, 0x1A, 0x37, 0x4E, 0x61, 0x74, 0x83, 0x8F, 0x97, 0x37, 0x83};
s32 D_800C3660 = 0;
u8 D_800C3664 = 0;
u16 D_800C3666 = 0;
ImagePlace D_800C3668[3] = {{0x340, 0}, {0x2C0, 0x100}, {0x300, 0x100}};
s32 D_800C3674 = 0x200;
s32 D_800C3678 = -1;
s32 D_800C367C = 1;
SpriteTask *D_800C3680 = NULL;
SpriteTask *D_800C3684 = NULL;
u8 D_800C3688 = 0;
/* A box's corners and its twelve edges; nothing reads them. */
SVECTOR D_800C368C[8] = {
    {-1500, -768, 0},    {1500, -768, 0},    {1500, 0, 0},    {-1500, 0, 0},
    {-1500, -768, 1536}, {1500, -768, 1536}, {1500, 0, 1536}, {-1500, 0, 1536},
};
SVECTOR *D_800C36CC[12][2] = {
    {&D_800C368C[3], &D_800C368C[7]}, {&D_800C368C[2], &D_800C368C[3]}, {&D_800C368C[2], &D_800C368C[6]},
    {&D_800C368C[0], &D_800C368C[1]}, {&D_800C368C[1], &D_800C368C[2]}, {&D_800C368C[0], &D_800C368C[3]},
    {&D_800C368C[6], &D_800C368C[7]}, {&D_800C368C[4], &D_800C368C[5]}, {&D_800C368C[5], &D_800C368C[6]},
    {&D_800C368C[4], &D_800C368C[7]}, {&D_800C368C[1], &D_800C368C[5]}, {&D_800C368C[0], &D_800C368C[4]},
};
u8 D_800C372C = 0;
SVECTOR D_800C3730 = {0, 0x1000, 0};
s32 D_800C3738 = 0x200;
s16 D_800C373C = 0xC0;
SVECTOR D_800C3740 = {0xC0, 0, 0};
SlotPulse *D_800C3748 = NULL;

/* Start the battle in mode (1-4 the battle module's intros, 801E8588..;
 * others 800B7870): the display, the frame state and the formation's
 * background colour. */
void func_800B8098(s32 mode) {
    D_800D36B8 = mode;
    func_800B8284();
    func_8001BBAC();
    switch (mode) {
    case 1:
        func_80028A60(0);
        func_801E8588();
        break;
    case 2:
        func_80028A60(0);
        func_801E91E8();
        break;
    case 3:
        func_80028A60(0);
        func_801E9594();
        break;
    case 4:
        func_80028A60(0);
        func_801E893C();
        break;
    case 0:
    case 5:
    default:
        func_800B7870();
        break;
    }
    func_80028A60(0);
    func_800A8B0C();
    BATTLE_AREA.buffers[0].drawEnv.isbg = func_801E7210(&D_8005949C, D_80059520, D_80059470, D_800CCB94,
                                                        D_800CCB94 + 0x20, &D_800C4A39);
    func_800A5E9C(&D_800C4A39, &BATTLE_AREA.buffers[1].drawEnv.r0);
}

/* Enter the battle: the first frame buffer (800B88C4), the frame state
 * (800B8840), the battle module (801E62E0) and the scene's camera. */
void func_800B81BC(s32 arg0) {
    func_800B88C4();
    func_800B8840();
    func_801E62E0(arg0);
    func_80038310(D_800595AC);
    func_80021B04(&D_800D30A0[0], SCENE_DATA->cameras[0].eye[0], SCENE_DATA->cameras[0].eye[1], SCENE_DATA->cameras[0].eye[2]);
    func_80021B04(&D_800D3354, SCENE_DATA->cameras[0].eye[0], SCENE_DATA->cameras[0].eye[1], SCENE_DATA->cameras[0].eye[2]);
    func_80021B04(&D_800D30A0[1], SCENE_DATA->cameras[0].lookAt[0], SCENE_DATA->cameras[0].lookAt[1], SCENE_DATA->cameras[0].lookAt[2]);
    func_80021B04(&D_800D335C, SCENE_DATA->cameras[0].lookAt[0], SCENE_DATA->cameras[0].lookAt[1], SCENE_DATA->cameras[0].lookAt[2]);
    SetDispMask(1);
}

/* Set up the two display buffers: 320 x 224 at y 224 and 0 (drawn at 0
 * and 224), shown at (0, 10) as 256 x 216. */
void func_800B8284(void) {
    SetGeomOffset(160, 164);
    SetDefDispEnv(&BATTLE_AREA.buffers[0].dispEnv, 0, 224, 320, 224);
    SetDefDispEnv(&BATTLE_AREA.buffers[1].dispEnv, 0, 0, 320, 224);
    SetDefDrawEnv(&BATTLE_AREA.buffers[0].drawEnv, 0, 0, 320, 224);
    SetDefDrawEnv(&BATTLE_AREA.buffers[1].drawEnv, 0, 224, 320, 224);
    BATTLE_AREA.buffers[1].dispEnv.screen.y = 10;
    BATTLE_AREA.buffers[0].dispEnv.screen.y = 10;
    BATTLE_AREA.buffers[1].dispEnv.screen.w = 256;
    BATTLE_AREA.buffers[0].dispEnv.screen.w = 256;
    BATTLE_AREA.buffers[1].dispEnv.screen.x = 0;
    BATTLE_AREA.buffers[0].dispEnv.screen.x = 0;
    BATTLE_AREA.buffers[1].dispEnv.screen.h = 216;
    BATTLE_AREA.buffers[0].dispEnv.screen.h = 216;
}

/* Run frames while the disc is busy. */
void func_800B8354(void) {
    while (func_800286CC() != 0) {
        func_800BE790();
    }
}

/* Play battle sound index to its end: load its sound bank and its wave bank
 * (the next bank, plus variant except for sound 8), start sound (plus
 * variant) and run frames while it plays, then free both banks. */
void func_800B838C(s32 index, s32 variant) {
    FileRequest banks[4]; /* the sound bank, the wave bank, the end, one unused */
    SoundBank *system;
    void *waves;
    SoundSequence *waveBank;
    s32 sound;

    func_800C0F70();
    func_800B8354();
    func_80028470(0x2C, 1);
    banks[0].file = D_800C35DC[index].bank;
    system = func_80031BDC(func_800288EC(banks[0].file), 0);
    banks[0].destination = system;
    if (index == 8) {
        banks[1].file = D_800C35DC[8].bank + 1;
    } else {
        banks[1].file = D_800C35DC[index].bank + 1;
        banks[1].file += variant;
    }
    waves = func_80031BDC(func_800288EC(banks[1].file), 0);
    banks[1].destination = waves;
    banks[2].destination = NULL;
    banks[2].file = 0;
    func_80029AFC(banks, 0, 0);
    func_800B8354();
    func_80038428(system);
    waveBank = func_80037FD8(waves, 0);
    while (func_8003BDFC(0) != 0) {
        func_800BE790();
    }
    sound = D_800C35DC[index].sound + variant + (system->id << 16);
    func_80039E60(sound);
    while (func_8003A5D0(sound) != 0) {
        func_800BE790();
    }
    func_8003852C(system);
    func_80038310(waveBank);
    func_800320E8(waves);
    func_800320E8(system);
}

/* Close the battle: for mode 0 the party (on foot) turns to its pose 0x18,
 * for mode 2 a white fade and pose 5 over 40 frames; then remove the enemy
 * slots, load wave bank 5 as the battle's (D_800595AC) and free the enemy
 * set data. */
void func_800B853C(s32 mode) {
    s32 i;
    s32 slot;
    Sprite *sprite;
    Sprite *member;
    void *waves;

    switch (mode) {
    case 0:
        func_800BC404(-1);
        for (i = 0; i != 3; i++) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL && (s8)sprite->motion.bytes[3] != 0x15) {
                func_800BFC80(sprite, 0, 2);
                func_800245D8(sprite, 0x18);
            }
        }
        break;
    case 2:
        func_800BC404(0);
        func_800B39C0(0x28, 2, 0xFF, 0xFF, 0xFF);
        for (slot = 0; slot != 3; slot++) {
            member = BATTLE_AREA.sprites[slot];
            if (member != NULL && (s8)member->motion.bytes[3] != 0x15) {
                func_800245D8(member, 5);
            }
        }
        slot = 0x28;
        do {
            func_800BE790();
            slot--;
        } while (slot > 0);
    case 1:
        break;
    }
    for (slot = 3; slot != 11; slot++) {
        func_800A9FF0(slot);
        func_800BADD4(slot);
    }
    func_800C0F70();
    func_800B8354();
    func_80028470(0x2C, 0);
    waves = func_80031BDC(func_800288EC(5), 1);
    func_800295D8(5, waves, 0, 0x80);
    func_800B8354();
    D_800595AC = func_80037FD8(waves, 0);
    while (func_8003BDFC(0) != 0) {
        func_800BE790();
    }
    func_800320E8(waves);
    func_800320E8(D_800D39C8);
    BATTLE_AREA.field8DA8 = 0;
}

/* Leave the battle: finish drawing, remove the slots' sprites, the
 * resident sprites and tasks, the sound bank and the scene. */
void func_800B8774(void) {
    s32 slot;

    if (BATTLE_AREA.buffer == 0) {
        func_800BE790();
    }
    DrawSync(0);
    for (slot = 0; slot != 11; slot++) {
        func_800BADD4(slot);
    }
    func_80024FB8();
    func_8001C8DC();
    func_8003852C((SoundBank *)D_8005919C);
    func_800320E8(D_8005919C);
    D_800591AD = 0;
    DrawSync(0);
    func_800A9F94();
    func_800A4820();
    func_800320E8(*(void **)D_800D2D54); /* a heap block here */
}

/* Reset the battle's frame state, sprites, camera and effects. */
void func_800B8840(void) {
    D_800591AD = 1;
    D_80059464 = 0;
    D_800591AC = 0;
    D_800591A8 = 0x2000;
    func_800BED30();
    func_800BE108();
    func_8001C944();
    func_800BB7F8();
    func_80024F64(0x5000, 0);
    func_800BCD8C();
    func_800B7C28();
    func_800B89F4();
    D_80050104 = 0;
}

/* Start the first frame: the frame skip from the gear enemies present,
 * draw into the second buffer with the first one's background colour. */
void func_800B88C4(void) {
    s32 i = 3;
    s32 skip;
    FrameBuffer *buffer;

    D_800C3D58 = 0;
    for (; i != 11; i++) {
        if (BATTLE_AREA.slots[i].field2 < 0x11 && BATTLE_AREA.slots[i].gear != 0) {
            D_800C3D58++;
        }
    }
    D_80059198 = D_800C3D58 / 2 - 1;
    if (D_80059198 < 0) {
        D_80059198 = 0;
    }
    skip = D_80059198;
    BATTLE_AREA.frameTicks = skip;
    D_80059198 = 0;
    D_80050100 = 2;
    buffer = &BATTLE_AREA.buffers[0];
    if (BATTLE_AREA.current == buffer) {
        buffer = &BATTLE_AREA.buffers[1];
    }
    BATTLE_AREA.current = buffer;
    BATTLE_AREA.ot = buffer->ot;
    ClearOTagR((u_long *)buffer->ot, 0x1000);
    BATTLE_AREA.buffer = 1;
    BATTLE_AREA.current = &BATTLE_AREA.buffers[1];
    BATTLE_AREA.field8DA8 = 0;
    BATTLE_AREA.buffers[1].drawEnv.isbg = BATTLE_AREA.buffers[0].drawEnv.isbg;
    BATTLE_AREA.buffers[1].drawEnv.r0 = BATTLE_AREA.buffers[0].drawEnv.r0;
    BATTLE_AREA.buffers[1].drawEnv.g0 = BATTLE_AREA.buffers[0].drawEnv.g0;
    BATTLE_AREA.buffers[1].drawEnv.b0 = BATTLE_AREA.buffers[0].drawEnv.b0;
}

/* Empty; the frame state's reset (800B8840) calls it last. */
void func_800B89F4(void) {
}

/* Open the battle menu for slot's turn: make it the acting slot facing its
 * event's first target. On foot, load wave bank 7 once for a gear frame or
 * return the sprite to its state; a gear turns to targets (800AA320 0x1A).
 * Mode 0 then walks the sprite (a gear turns to the event's targets),
 * otherwise the menu goes to state 4 (9 for a gear). */
void func_800B89FC(s32 mode, s32 slot, s32 targets, s32 arg3) {
    BattleMenu *menu;
    Sprite *sprite;
    void *waves;

    func_800BC3F8(0);
    if (D_800C3610 != NULL) {
        for (;;) {
            __asm__ volatile(".word 0x0001000D"); /* break 1 */
        }
    }
    func_800BE790();
    func_800BE790();
    D_800C3624 = 0;
    D_800C3608 &= ~(1 << slot);
    menu = func_800BED4C();
    D_800C3610 = menu;
    menu->field40 = arg3;
    menu->turnSlot = slot;
    func_800BF3E8(func_800BEFF4(slot));
    D_800C3DF0 = 0;
    sprite = D_800C3610->sprite;
    if (!BATTLE_AREA.slots[slot].gear) {
        func_800C0F70();
        D_800C3620 = 0;
        func_800BC454(0xC0);
        if (!func_8001EE68(*(u8 **)sprite->image)) {
            D_800C3622 = 0;
            if (D_800C3618 == NULL || SPRITE_SLOT(sprite) != D_800C361C) {
                func_800BF2B8(sprite);
            }
        } else {
            if (!D_800C3622) {
                func_800B8354();
                func_80028470(0x2C, 0);
                waves = func_80031BDC(func_800288EC(7), 0);
                func_800295D8(7, waves, 0, 0x80);
                func_800B8354();
                func_800C0F70();
                D_800C3A6C = func_80037FD8(waves, 0);
                while (func_8003BDFC(0) != 0) {
                    func_800BE790();
                }
                func_800320E8(waves);
            }
            D_800C3622 = 1;
        }
    } else {
        func_800B8D04();
        func_800BFBA0();
        func_800BF0B4(8);
        func_800BEE2C(slot, targets, 0x1A);
        func_800BC454(0xC0);
    }
    if (mode == 0) {
        if (BATTLE_AREA.slots[slot].gear) {
            func_800BEE2C(slot, BATTLE_AREA.events[D_800C360C].targetMask, 2);
        } else {
            func_80021BF8(sprite, func_800B9B30);
            func_800BF0C4(sprite);
        }
    } else {
        func_800BF0B4(BATTLE_AREA.slots[slot].gear ? 9 : 4);
    }
}

/* Finish the battle's loads: wait for the disc (800B8354), start the
 * requested loads (800BF9EC), run frames until D_80059464 is reached,
 * then free the command file. */
void func_800B8D04(void) {
    func_800B8354();
    func_800BF9EC();
    while (D_80059464 != func_800BF720()) {
        func_800BE790();
    }
    func_800BF3A4();
    if (D_800C3618 != NULL) {
        func_800320E8(D_800C3618);
        D_800C3618 = NULL;
    }
}

/* Stop 8002A498 and finish the loads (800B8D04). */
void func_800B8D7C(void) {
    func_8002A498(0);
    func_800B8D04();
}

/* Cancel the turn: the turn's slot acts again; a gear turns back (800AA320
 * 0x1F), a party member returns to its place, ground and idle motion; close
 * the battle menu. */
void func_800B8DA4(void) {
    Sprite *sprite;

    if (D_800C3610 != NULL) {
        func_800BEFF4(D_800C3610->turnSlot);
        sprite = D_800C3610->sprite;
        if (BATTLE_AREA.slots[SPRITE_SLOT(sprite)].gear) {
            func_800BEE2C(SPRITE_SLOT(sprite), 0, 0x1F);
        } else {
            sprite->x = (u16)BATTLE_AREA.slots[D_800C3610->slot].x << 16;
            sprite->z = (u16)BATTLE_AREA.slots[D_800C3610->slot].z << 16;
            func_800BA8F4(sprite);
            sprite->y = sprite->ground << 16;
            func_800245D8(sprite, (s8)sprite->b0.byteb0);
            func_80021BF8(sprite, NULL);
        }
        func_800BEDE8();
    }
}

/* End the turn: wait for the command's loads and sprites, restore the view
 * and the camera, close the battle menu, end the turn's presentation
 * (800BA4E0), fade a pending sound and clear every sprite's bit 6. */
void func_800B8EBC(void) {
    Sprite *sprite = D_800C3610->sprite;
    s32 slot;
    s32 i;

    if (D_800C3610 != NULL) {
        D_800C3624 = 0;
        D_800C3DF0 = 0;
        D_800C3622 = 0;
        func_80080BD0();
        DrawSync(0);
        slot = D_800C3610->field40;
        func_80021BF8(sprite, NULL);
        func_800C0314();
        while (D_80059464 != func_800BF720()) {
            func_800BE790();
        }
        func_800C0564();
        func_800B8D7C();
        func_800BC3F8(1);
        func_800BE0DC();
        func_800BF9EC();
        func_800BFA9C();
        D_800C3688 = 0;
        func_800BC454(0xC0);
        func_800BEDE8();
        func_800BA4E0(slot);
        if (D_800C35D4) {
            func_8003A89C((SoundSeq *)D_800C3E54, 0x7F, 0x50);
        }
        D_800C35D4 = 0;
        for (i = 0; i != 11; i++) {
            if (BATTLE_AREA.sprites[i] != NULL) {
                BATTLE_AREA.sprites[i]->motion.word &= ~0x40;
            }
        }
    }
}

/* Return the sprite to its idle motion. */
void func_800B9020(Sprite *sprite) {
    func_800245D8(sprite, (s8)sprite->b0.byteb0);
    func_80021BF8(sprite, NULL);
}

/* Start the current event: its actor acts, facing its first target. For
 * a partner action both sprites get bit 6 and face each other, the camera
 * frames both, and the actor steps 0x50 beside its partner (to the side it
 * came from, or away from the partner's place when it stands there); then
 * start the loads and walk (800B9508). */
void func_800B905C(void) {
    Sprite *sprite;
    Sprite *partner;
    u16 mask;
    s16 x;

    func_800BEFF4(BATTLE_AREA.events[D_800C360C].actor);
    sprite = D_800C3610->sprite;
    func_800BF3E8(sprite);
    if (AREA_PARTNER_ACTION) {
        partner = sprite->partner;
        partner->motion.word |= 0x40;
        sprite->motion.word |= 0x40;
        func_800B9B54(sprite, partner);
        partner = sprite->partner;
        mask = (1 << SPRITE_SLOT(sprite)) | (1 << SPRITE_SLOT(partner));
        func_800BC460(mask);
        x = partner->x >> 16;
        if (x != (u16)BATTLE_AREA.slots[SPRITE_SLOT(partner)].x
            || (partner->z >> 16) != (u16)BATTLE_AREA.slots[SPRITE_SLOT(partner)].z) {
            if (x < (u16)BATTLE_AREA.slots[SPRITE_SLOT(partner)].x) {
                sprite->target_x = x - 0x50;
            } else {
                sprite->target_x = x + 0x50;
            }
        } else if ((sprite->x >> 16) < x) {
            sprite->target_x = x - 0x50;
        } else {
            sprite->target_x = x + 0x50;
        }
        sprite->target_z = partner->z >> 16;
        sprite->target_y = 0;
        func_800BC460(mask);
        copyVector(&D_800D3354, &D_800D30A0[0]);
        copyVector(&D_800D335C, &D_800D30A0[1]);
        func_800B9B54(sprite, partner);
    }
    func_800BF9EC();
    func_800B9508(sprite);
}

/* Count a step of the battle menu (field34), when there is one. */
void func_800B9258(void) {
    if (D_800C3610 != NULL) {
        D_800C3610->field34++;
    }
}

/* Run a control event (type 0xF3-0xFA) of the current event for sprite:
 * the event's parameter shows or hides a message window, delays the next
 * event, runs a command (with motion 0x11, for 0xF5 also pose 0x13 and
 * D_800C3623), frames its targets, or adds slots that count while down. */
void func_800B9284(Sprite *sprite, s32 type) {
    s32 parameter;

    switch (type) {
    case 0xFA:
        D_800C3610->field48 = 1;
        func_80021BF8(sprite, NULL);
        func_80079E18(BATTLE_AREA.events[D_800C360C].parameter);
        break;
    case 0xF8:
        D_800C3610->field48 = 1;
        func_80021BF8(sprite, NULL);
        func_80079E4C(BATTLE_AREA.events[D_800C360C].parameter);
        break;
    case 0xF7:
        D_800C3610->field48 = 1;
        func_80021BF8(sprite, NULL);
        D_800C3614 = BATTLE_AREA.events[D_800C360C].parameter;
        break;
    case 0xF5:
        func_800B8048(sprite);
        parameter = BATTLE_AREA.events[D_800C360C].parameter;
        D_800C3DF0 = (parameter >> 9) & 0x3F;
        sprite->motion.bytes[3] = 0x11;
        func_800BF600(parameter & 0x1FF, sprite);
        func_800BF730((s32)sprite);
        func_800245D8(sprite, 0x13);
        D_800C3623 = 1;
        break;
    case 0xF4:
        func_800B8048(sprite);
        parameter = BATTLE_AREA.events[D_800C360C].parameter;
        D_800C3DF0 = (parameter >> 9) & 0x3F;
        sprite->motion.bytes[3] = 0x11;
        func_800BF600(parameter & 0x1FF, sprite);
        break;
    case 0xF6:
        func_800BC404(BATTLE_AREA.events[D_800C360C].targetMask);
        break;
    case 0xF3:
        D_800C3610->field48 = 1;
        D_800C3608 |= BATTLE_AREA.events[D_800C360C].parameter;
        break;
    }
}

/* Step the current event for the acting sprite (after any delay): control
 * events 0xF3-0xFA (800B9284; 0xF9 ends with pose 5), 0xFB takes another
 * slot's sprite images, 0xFC sets the idle mode, 0xFD walks on, 0xFE (and
 * 0xFF once 800C0314 is done) returns the turn's sprite to its place;
 * other types are commands: a motion on foot (from 0x10 a command of the
 * sprite's slot's field2), or a gear's pose framing it and its partner. */
void func_800B9508(Sprite *sprite) {
    s32 motion;
    Sprite *other;
    Sprite *partner;
    s32 type;
    s32 command;

    if (D_800C3614 != 0) {
        D_800C3614--;
        return;
    }
    type = BATTLE_AREA.events[D_800C360C].type;
    switch (type) {
    case 0xF3:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xFA:
        func_800B9284(sprite, type);
        break;
    case 0xF9:
        func_800245D8(sprite, 5);
        func_800BC404(0);
        return;
    case 0xFF:
        func_80021BF8(sprite, NULL);
        D_800C3610->field48 = 1;
        if (func_800C0314() == 0) {
            if (sprite->frame_bits.field28 == 0 && !BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
                func_800245D8(sprite, (s8)sprite->b0.byteb0);
            }
            return;
        }
    case 0xFE:
        D_800C3610->field48 = 0;
        sprite = func_800BEFF4(D_800C3610->turnSlot);
        func_800BF3E8(sprite);
        func_80021BF8(sprite, func_800B9B30);
        func_800BF0B4(5);
        if (FIXED_WHOLE(sprite->x) == (u16)BATTLE_AREA.slots[D_800C3610->slot].x
            && FIXED_WHOLE(sprite->z) == (u16)BATTLE_AREA.slots[D_800C3610->slot].z) {
            func_800B9B30(sprite);
        } else {
            sprite->target_x = BATTLE_AREA.slots[D_800C3610->slot].x;
            sprite->target_z = BATTLE_AREA.slots[D_800C3610->slot].z;
            sprite->target_y = 0;
            func_800245D8(sprite, 4);
        }
        return;
    case 0xFD:
        D_800C3610->field48 = 1;
        if (BATTLE_AREA.events[D_800C360C].parameter == 0) {
            D_800C3610->field48 = 0;
            func_800BF0C4(sprite);
            func_80021BF8(sprite, func_800B9B30);
        }
        D_800C360C++;
        return;
    case 0xFC:
        D_800C3610->field48 = 1;
        func_80021BF8(sprite, NULL);
        func_80021FB8(sprite, BATTLE_AREA.events[D_800C360C].parameter);
        D_800C360C++;
        return;
    case 0xFB:
        D_800C3610->field48 = 1;
        func_80021BF8(sprite, NULL);
        other = BATTLE_AREA.sprites[BATTLE_AREA.events[D_800C360C].parameter];
        sprite->image = other->image;
        ((SpriteSequencer *)sprite->sequencer)->size = ((SpriteSequencer *)other->sequencer)->size;
        sprite->render.word |= 0x40000000;
        func_800320E8(sprite->renderer->parts[0]);
        sprite->renderer->parts[1] = sprite->renderer->parts[0] = func_80031BDC(func_80031894((u8 *)other->renderer->parts[0]), 0);
        D_800C360C++;
        return;
    default:
        partner = sprite->partner;
        D_800C3626 = 0;
        func_800B8048(sprite);
        func_80021BF8(sprite, func_800B9B30);
        ((SpriteSequencer *)sprite->sequencer)->word8 = BATTLE_AREA.events[D_800C360C].codes[SPRITE_SLOT(partner)];
        if (!func_8001EE68(*(u8 **)sprite->image)) {
            if (type >= 0x10) {
                command = type - 0x10;
                command += D_800C3630[BATTLE_AREA.slots[SPRITE_SLOT(sprite)].field2];
                if (command < D_800C3648[BATTLE_AREA.slots[SPRITE_SLOT(sprite)].field2]) {
                    sprite->motion.bytes[3] = 0x1C;
                } else {
                    sprite->motion.bytes[3] = 0x11;
                }
                func_800BF600(command, sprite);
                func_800BF730((s32)sprite);
            } else {
                motion = type;
                if (!D_800D3350 && D_800C3618 != NULL) {
                    func_800B8354();
                    sprite->word50 = func_800BF354();
                    func_80021BF0(sprite, (s32)D_800C3618);
                }
                if (D_800C3610->turnSlot == SPRITE_SLOT(sprite)) {
                    motion = ~motion;
                    func_800245D8(sprite, motion);
                } else {
                    func_800245D8(sprite, motion);
                }
            }
        } else {
            func_800245D8(sprite, type);
            func_800BC460((1 << SPRITE_SLOT(sprite)) | (1 << SPRITE_SLOT(sprite->partner)));
        }
        D_800C3610->field48 = 0;
        D_800C360C++;
        return;
    }
    D_800C360C++;
}

/* Mark the battle menu (field48) with its state. */
void func_800B9B30(void) {
    D_800C3610->field48 = 1;
    D_800C3610->field49 = D_800C3610->state;
}

/* Turn two sprites to face each other (the second not while its motion
 * mode is 0x15). */
void func_800B9B54(Sprite *sprite, Sprite *other) {
    if (sprite != other) {
        func_800223B0(sprite, func_800BEF24(sprite, other));
        func_80021FE0(sprite, func_800BEF24(sprite, other));
        if ((s8)other->motion.bytes[3] != 0x15) {
            func_800223B0(other, func_800BEF24(other, sprite));
            func_80021FE0(other, func_800BEF24(other, sprite));
        }
    }
}

/* Put the sprite at its target, idle, facing other. Defined without a
 * prototype: 800BF0C4 calls it with the sprite alone. */
void func_800B9C00(sprite, other)
    Sprite *sprite;
    Sprite *other;
{
    sprite->x = sprite->target_x << 16;
    sprite->z = sprite->target_z << 16;
    D_800C3610->field48 = 1;
    func_800BF0B4(4);
    func_800245D8(sprite, (s8)sprite->b0.byteb0);
    func_800B9B54(sprite, other);
}

/* Step the current event of a gear's turn (after any delay): control events
 * (800B9284), 0xF4 sets the command sound and step, 0xFB swaps stage
 * objects, 0xFC sets a stage object's byte, 0xFD/0xF9 and commands run
 * 800AA320 on the event's targets, 0xFE turns the turn's slot to them and
 * ends (state 10), 0xFF ends (state 9). */
void func_800B9C78(void) {
    s32 slot;
    Sprite *sprite;
    s32 type;
    s32 parameter;
    s32 event;

    if (D_800C3614 != 0) {
        D_800C3614--;
        return;
    }
    func_800BF9EC();
    slot = BATTLE_AREA.events[D_800C360C].actor;
    func_800BEFF4(slot);
    func_800BF3E8(D_800C3610->sprite);
    slot = D_800C3610->slot;
    sprite = BATTLE_AREA.sprites[slot];
    type = BATTLE_AREA.events[D_800C360C].type;
    func_800BF0B4(8);
    if (D_800C3610->field4A) {
        AREA_BYTE_A73 = 0;
    }
    D_800C3610->field4A = 0;
    switch (type) {
    case 0xFF:
        func_800BF0B4(9);
        return;
    case 0xFE:
        func_800B8354();
        func_800BEE2C(D_800C3610->turnSlot, BATTLE_AREA.events[D_800C360C].targetMask, 4);
        func_800BF0B4(10);
        return;
    case 0xFC:
        func_800AA760(slot, BATTLE_AREA.events[D_800C360C].parameter);
        func_800BF0B4(9);
        D_800C360C++;
        return;
    case 0xFD:
        D_800C360C++;
        func_800BEE2C(slot, BATTLE_AREA.events[D_800C360C - 1].targetMask, 2);
        return;
    case 0xF3:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xFA:
        func_800B9284(sprite, type);
        func_800BF0B4(9);
        D_800C360C++;
        return;
    case 0xFB:
        func_800BF0B4(9);
        func_800AA79C(SPRITE_SLOT(sprite), BATTLE_AREA.events[D_800C360C].parameter);
        D_800C360C++;
        return;
    case 0xF4:
        func_800BF0B4(9);
        event = D_800C360C;
        D_800C360C = event + 1;
        parameter = BATTLE_AREA.events[event].parameter;
        D_800C3DF0 = (parameter >> 9) & 0x3F;
        D_800D39E4 = parameter & 0x1FF;
        return;
    default:
        D_800C3610->field4A = 1;
        func_800B8048(sprite);
        D_800C360C++;
        func_800BEE2C(slot, BATTLE_AREA.events[D_800C360C - 1].targetMask, type);
        return;
    }
}

/* The battle menu's update (not reentered): finish a requested sound
 * command, step a gear's events, run the menu's pending action (field49:
 * 2 put the sprite at its target, 4 a party target's hit pose 0x1B,
 * 5 return to idle and end the slot's turn, 6 walk to the partner), then
 * run its state: 2/6 walk towards the target until the distance grows,
 * 4 start the event, 7/8/10 wait for the popups, 9 step a gear's event. */
void func_800B9F78(BattleMenu *menu) {
    Sprite *sprite;
    Sprite *target;
    Sprite *first;
    GroundPoint from;
    GroundPoint to;
    GroundPoint toPartner;
    s32 distance;

    if (D_800C3660 != 0) {
        return;
    }
    D_800C3660 = 1;
    D_800C3610 = menu;
    sprite = menu->sprite;
    target = menu->target;
    if (D_800C3628 != 0) {
        D_800C3628 = 0;
        if (func_800B7E94()) {
            func_80021BF8(sprite, func_800B9B30);
        } else {
            if (SPRITE_SLOT(sprite) < 3) {
                if (D_800C3623) {
                    func_800245D8(sprite, 0x13);
                } else {
                    func_800245D8(sprite, 0x12);
                }
                D_800C3623 = 0;
            }
            if (sprite->animations != 0) {
                func_800BF0B4(7);
            }
        }
    }
    if (D_800C3610->field34 != 0) {
        func_800B9C78();
        D_800C3610->field34--;
    }
    if (D_800C3610->field49 != 0) {
        switch (D_800C3610->field49) {
        case 4:
            first = D_800D363C[0];
            if (SPRITE_SLOT(first) < 3 && !BATTLE_AREA.slots[SPRITE_SLOT(first)].gear
                && BATTLE_AREA.events[D_800C360C - 1].codes[SPRITE_SLOT(first)] == 7) {
                func_800B8048(first);
                func_800245D8(first, 0x1B);
                while ((s8)first->motion.bytes[3] == 0x1B) {
                    func_800BE790();
                }
            }
            break;
        case 6:
            func_800BF4F0(sprite, sprite->partner);
            break;
        case 2:
            func_800B9C00(sprite, sprite->partner);
            break;
        case 5:
            if (!BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
                func_800245D8(sprite, (s8)sprite->b0.byteb0);
            }
            func_800BAEB8(D_800C3610->slot);
            func_800BF0B4(10);
            break;
        }
        D_800C3610->field49 = 0;
    }
    switch (D_800C3610->state) {
    case 7:
        if (D_80059464 == func_800BF720()) {
            func_800BF0B4(4);
            D_800C3610->field48 = 1;
            func_800BF9EC();
        }
        break;
    case 10:
        if (D_80059464 == func_800BF720()) {
            func_800B8EBC();
        }
        break;
    case 9:
        func_800B9C78();
        break;
    case 8:
        if (D_800C362C && D_80059464 == func_800BF720()) {
            if (!D_800C3624) {
                break;
            }
            if (D_800C362C == 1) {
                D_800C3610->field34++;
            }
            func_800BFA9C();
        }
        break;
    case 2:
        from.x = sprite->x >> 16;
        from.z = sprite->z >> 16;
        to.x = sprite->target_x;
        to.z = sprite->target_z;
        distance = func_800C07CC(from, to);
        if (D_800C3610->field44 < distance) {
            func_800B9C00(sprite, target);
        } else {
            D_800C3610->field44 = distance;
        }
        break;
    case 6:
        from.x = sprite->x >> 16;
        from.z = sprite->z >> 16;
        toPartner.x = sprite->target_x;
        toPartner.z = sprite->target_z;
        distance = func_800C07CC(from, toPartner);
        if (D_800C3610->field44 < distance) {
            func_800BF4F0(sprite, target);
        } else {
            D_800C3610->field44 = distance;
        }
        break;
    case 4:
        if (D_800C3610->field48) {
            func_800B905C();
        }
        break;
    }
    D_800C3660 = 0;
}

/* End slot's turn presentation: wait for the stage objects (800B136C), then
 * restore the view (800B8D7C) and, for a gear, 800BFBA0; for a party member
 * on foot its sprite's state (800BF2B8). */
void func_800BA4E0(s32 slot) {
    D_80059464 = 0;
    D_800591AC = 0;
    func_800B136C();
    if (BATTLE_AREA.slots[slot].gear) {
        func_800B8D7C();
        func_800BFBA0();
    } else if (slot < 3) {
        if (BATTLE_AREA.sprites[slot] != NULL) {
            func_800BF2B8(BATTLE_AREA.sprites[slot]);
        }
    } else {
        func_800B8D7C();
    }
    func_800BC454(0xC0);
}

/* Turn sprite to direction, its horizontal speed a quarter of its speed
 * along it. */
void func_800BA59C(Sprite *sprite, s16 direction) {
    s32 speed;

    sprite->direction = direction;
    speed = sprite->speed >> 3;
    sprite->speed_x = (func_8003F8CC(direction) >> 1) * speed >> 8;
    sprite->speed_z = -((func_8003F8B0(sprite->direction) >> 1) * speed) >> 8;
}

/* Aim sprite's jump at its target: turn it towards the target and set the
 * rising speed that lands it on the ground there (or the target's height
 * when that is higher). */
void func_800BA614(Sprite *sprite) {
    VECTOR delta;
    SVECTOR point;
    VECTOR out;
    s32 triangle;
    s32 height;
    s16 angle;
    s32 distance;

    func_80021B04(&point, sprite->target_x, sprite->target_y, sprite->target_z);
    triangle = func_800A5914(&point, sprite->word78, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    if (point.vy > sprite->target_y) {
        point.vy = sprite->target_y;
    }
    height = ((point.vy << 16) - sprite->y) >> 16;
    delta.vx = sprite->target_x - (sprite->x >> 16);
    delta.vz = sprite->target_z - (sprite->z >> 16);
    angle = -ratan2(delta.vz, delta.vx);
    Square0(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz);
    sprite->speed_y = -sprite->gravity * distance * 16 / (sprite->speed >> 11) + sprite->speed * height / distance;
    func_800BA59C(sprite, angle);
}

/* Aim sprite's jump at its target keeping its rising speed: snap it to
 * whole units, turn it towards the target and set the speed that covers the
 * distance (and the height difference) in the jump's frames. */
void func_800BA768(Sprite *sprite) {
    VECTOR delta;
    SVECTOR point;
    VECTOR out;
    s32 frames;
    s32 triangle;
    s32 angle;
    s32 distance;
    s32 height;

    frames = -(sprite->speed_y * 2 / sprite->gravity);
    sprite->x &= 0xFFFF0000;
    sprite->y &= 0xFFFF0000;
    sprite->z &= 0xFFFF0000;
    delta.vx = sprite->target_x - (sprite->x >> 16);
    delta.vz = sprite->target_z - (sprite->z >> 16);
    delta.vy = 0;
    angle = -ratan2(delta.vz, delta.vx);
    Square0(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz) << 16;
    if (frames != 0) {
        sprite->speed = distance / frames;
    } else {
        sprite->speed = 0;
    }
    func_80021B04(&point, sprite->target_x, sprite->target_y, sprite->target_z);
    triangle = func_800A5914(&point, sprite->word78, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    if (point.vy > sprite->target_y) {
        point.vy = sprite->target_y;
    }
    height = (point.vy << 16) - sprite->y;
    if (frames != 0) {
        sprite->speed_y += height / frames;
    }
    func_800BA59C(sprite, angle);
    func_80022B2C(sprite);
}

/* Put sprite on the scene's ground: its triangle and ground height. */
void func_800BA8F4(Sprite *sprite) {
    SVECTOR point;
    VECTOR out;
    s32 triangle;

    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
    triangle = func_800A5914(&point, sprite->word78, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    sprite->ground = point.vy;
    sprite->word78 = triangle;
}

/* Create a sprite task (updated by 800BAC50, drawn by 800BAB0C) at x, y, z
 * facing direction, running animation. */
SpriteTask *func_800BA984(s32 resource, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused5, s16 x,
                          s16 y, s16 z, s16 animation, s16 direction, s32 unused11, s32 unused12,
                          s32 palette_bank) {
    SpriteTask *task;
    Sprite *sprite;

    task = (SpriteTask *)func_8001D1D8(0x19C, NULL, func_800BAC50, func_800BAB0C, func_800BABDC);
    sprite = &task->sprite;
    task->task.data = sprite;
    task->auxiliary.data = sprite;
    task->auxiliary.owner = NULL;
    func_800242F4(sprite, resource, clut_x, clut_y, texture_x, texture_y, unused5, palette_bank);
    sprite->block = task;
    sprite->x = x << 16;
    sprite->y = y << 16;
    sprite->z = z << 16;
    sprite->b0.byteb0 = animation;
    sprite->render.word |= 4;
    sprite->direction = direction;
    func_80022000(sprite, 0x2000);
    sprite->word82 = 0x2000;
    sprite->word78 = 0;
    func_800245D8(sprite, animation);
    return task;
}

/* Draw a sprite task: its depth in the view, and its parts when visible. */
void func_800BAB0C(Task *task) {
    SVECTOR point;
    long result[2]; /* screen position, then the GTE flags */
    VECTOR unused;
    Sprite *sprite;
    s32 depth;

    if (D_800C3664 == 0) {
        sprite = task->data;
        point.vx = sprite->x >> 16;
        point.vy = sprite->y >> 16;
        point.vz = sprite->z >> 16;
        SetRotMatrix(&D_800D30BC);
        SetTransMatrix(&D_800D30BC);
        depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> D_80050100) + sprite->half30;
        if (result[1] & 0x8000) {
            depth = 0;
        }
        sprite->depth = depth;
        if ((u32)(depth - 1) < 0xFFF) {
            func_8001E298(sprite, (u_long *)D_8005956C + depth);
        }
    }
}

/* Destroy a sprite task: its part block, children, sprite and node. */
void func_800BABDC(Task *node) {
    SpriteTask *task = (SpriteTask *)node;
    Sprite *sprite = &task->sprite;
    void *parts = sprite->renderer->parts[0];

    if (parts != NULL) {
        func_800320E8(parts);
    }
    func_8001CE74(&task->task);
    func_8001D3F4(sprite);
    func_8001CB48(&task->auxiliary);
    func_8001CD94(&task->task);
    func_800320E8(task);
}

/* Update a sprite task (twice with double steps) unless paused. */
void func_800BAC50(Task *task) {
    Sprite *sprite = task->data;

    if (D_800C3664 == 0) {
        func_80023210(sprite);
        func_80022CDC(sprite);
        if (sprite->motion.bits.double_step) {
            func_80023210(sprite);
            func_80022CDC(sprite);
        }
    }
}

/* Party slot's sprite on screen: its position, depth and a box around it. */
void func_800BACBC(s32 slot, s16 *x, s16 *y, s16 *depth, s16 *left, s16 *width, s16 *centre) {
    SVECTOR point;
    s16 sxy[2];
    long p;
    Sprite *sprite = BATTLE_AREA.sprites[slot];

    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
    PushMatrix();
    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    *depth = RotTransPers(&point, (long *)sxy, &p, &p) >> 4;
    *x = sxy[0];
    *y = sxy[1];
    *left = sxy[0] - 0x30;
    *width = 0x30;
    *centre = sxy[0] - 0x18;
    PopMatrix();
}

/* Remove party slot's sprite task: stop its effects (800BFC80), free its
 * sprite source, destroy the task and clear the slot's sprite. */
void func_800BADD4(s32 slot) {
    SpriteTask *task = BATTLE_AREA.tasks[slot];

    if (task != NULL) {
        func_800BFC80((Sprite *)task, 0, 2); /* the slot's task where a sprite is taken */
        if (slot < 3) {
            if (BATTLE_AREA.sources[slot].data != NULL) {
                func_800320E8(BATTLE_AREA.sources[slot].data);
            }
            BATTLE_AREA.sources[slot].data = NULL;
        }
        task->task.destroy(&task->task);
        func_8001CE74(&task->task);
        BATTLE_AREA.sprites[slot] = NULL;
        BATTLE_AREA.tasks[slot] = NULL;
    }
}

/* Face slot's sprite along its side (turned for a nonzero target code),
 * unless it runs animation 0x15. */
void func_800BAEB8(s32 slot) {
    Sprite *sprite = BATTLE_AREA.sprites[slot];
    s32 direction;

    if ((s8)sprite->motion.bytes[3] != 0x15) {
        direction = (BATTLE_AREA.slots[slot].targetCode != 0) << 11;
        func_800223B0(sprite, direction);
        func_80021FE0(sprite, direction);
    }
}

/* Empty; its callers (the combo and technique commits) pass a slot and a
 * mode it ignores. */
void func_800BAF40(void) {
}

/* Send party slot's sprite off: select it (800BC404), run its exit
 * animation 0x16 and wait for it and its tasks, then remove the sprite and
 * load the slot's gear object in its place (800BB760), waiting for it. */
void func_800BAF48(s32 slot) {
    Sprite *sprite;
    s32 tasks;

    func_800BC404(1 << slot);
    func_800BC404(0);
    tasks = D_80059188;
    sprite = BATTLE_AREA.sprites[slot];
    func_800B8D7C();
    func_800245D8(sprite, 0x16);
    while (sprite->countdown != 0 && (s8)sprite->motion.bytes[3] == 0x16) {
        func_800BE790();
    }
    while (D_80059188 != tasks) {
        func_800BE790();
    }
    func_800BADD4(slot);
    func_800BE790();
    func_800BE790();
    func_800BB760(slot);
    while (D_800C35D8 != 0) {
        func_800BE790();
    }
}

/* Destroy the party members' sprites other than keep's that are not in
 * use, then end their stage objects (800B14CC). */
void func_800BB080(s32 keep) {
    s32 i;
    Sprite *sprite;

    for (i = 0; i != 3; i++) {
        if (i != keep) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL && sprite->animations == 0) {
                ((Task *)sprite->block)->destroy(sprite->block);
                BATTLE_AREA.sprites[i] = NULL;
                BATTLE_AREA.tasks[i] = NULL;
            }
        }
    }
    func_800B14CC(keep);
}

/* Update of a sprite following its slot's stage object: step its animation
 * while it runs, then put it at the object's position. */
void func_800BB13C(Task *task) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    Sprite *sprite = task->data;
    u32 low = sprite->frame_bits.unknown30;
    BattleObject *object = D_800D3368[sprite->motion.bits.unknown0 << 2 | low];

    if (object != NULL) {
        if (sprite->countdown == 0) {
            sprite->script = 0;
        }
        if (sprite->script != 0) {
            func_80023210(sprite);
            func_80022CDC(sprite);
            if (sprite->motion.bits.double_step) {
                func_80023210(sprite);
                func_80022CDC(sprite);
            }
        }
        sprite->x = object->hierarchy->translation[0] << 16;
        sprite->y = object->hierarchy->translation[1] << 16;
        sprite->z = object->hierarchy->translation[2] << 16;
    }
}

/* Draw of a slot-following sprite: its size from the slot's object and its
 * depth in the view. */
void func_800BB248(Task *task) {
    SVECTOR point;
    long result[2]; /* screen position, then the GTE flags */
    Sprite *sprite = task->data;
    s32 depth;
    u32 low;

    low = sprite->frame_bits.unknown30;
    sprite->height = func_800AA600(sprite->motion.bits.unknown0 << 2 | low);
    sprite->extent_depth = sprite->height / 2;
    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> D_80050100) + sprite->half30;
    if (result[1] & 0x8000) {
        depth = 0;
    }
    sprite->depth = depth;
}

/* Destroy a task node. */
void func_800BB314(Task *task) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */

    func_8001CB48(task + 1);
    func_8001CD94(task);
    func_800320E8(task);
}

/* Create slot's sprite following its stage object (800BB13C, 800BB248),
 * unless it has one. */
void func_800BB350(u32 slot) {
    SpriteTask *task;
    Sprite *sprite;
    u8 saved;

    if (BATTLE_AREA.sprites[slot] == NULL) {
        saved = D_800591AC;
        D_800591AC = 0;
        task = (SpriteTask *)func_8001D1D8(0x19C, NULL, func_800BB13C, func_800BB248, func_800BB314);
        sprite = &task->sprite;
        sprite->block = task;
        task->task.data = sprite;
        task->auxiliary.data = sprite;
        func_80023804(sprite);
        func_800239A0(sprite);
        ((SpriteFlagBits *)&sprite->flags)->type = 4;
        sprite->block = task;
        sprite->render.word &= ~3;
        sprite->frame_bits.sequencer_owned = 0;
        ((SpriteSequencer *)sprite->sequencer)->word8 = 0;
        ((SpriteSequencer *)sprite->sequencer)->halfc = 0;
        sprite->word82 = D_800591A8;
        sprite->x = (u16)BATTLE_AREA.slots[slot].x << 16;
        sprite->z = (u16)BATTLE_AREA.slots[slot].z << 16;
        sprite->y = 0;
        sprite->height = func_800AA600(slot);
        sprite->word82 = 0x2000;
        sprite->extent_depth = sprite->height >> 1;
        func_80022000(sprite, 0x2000);
        sprite->image = D_8006BE10;
        BATTLE_AREA.sprites[slot] = sprite;
        BATTLE_AREA.tasks[slot] = task;
        sprite->resource = 0;
        sprite->animations = 0;
        D_800591AC = saved;
        sprite->frame_bits.unknown30 = slot;
        sprite->motion.bits.unknown0 = slot >> 2;
    }
}

/* Task step: take a free stage place (of three), create the gear object of
 * the task's slot there, its sprite (800BB350), and end the task; the last
 * one sets D_800C37CC. */
void func_800BB540(SlotTask *task) {
    s32 i;
    s32 bit;

    for (i = 0, bit = 1; i != 3; i++, bit <<= 1) {
        if (!(D_800C3666 & bit)) {
            D_800C3666 |= bit;
            break;
        }
    }
    func_800A979C(task->slot, D_800C3668[i].x, D_800C3668[i].y, 0, task->slot + 0x1C0);
    D_800C3CB8--;
    func_800BB350(task->slot);
    task->task.destroy(&task->task);
    if (--D_800C35D8 == 0) {
        D_800C37CC = 1;
    }
}

/* Task step: once the disc is idle, run 800BB540 on a separate stack. */
void func_800BB620(SlotTask *task) {
    u8 *stack;

    if (func_800286CC() == 0) {
        stack = func_80031BDC(0x1000, 1);
        STACK_ENTER(stack + 0xF00);
        func_800BB540(task);
        STACK_LEAVE();
        func_800320E8(stack);
    }
}

/* Task step: read the gear files of the task's slot (800A9540), then
 * continue with 800BB620. */
void func_800BB690(SlotTask *task) {
    func_800A9540(task->slot);
    D_800C3CB8++;
    func_8001CD6C(&task->task, (void (*)(Task *))func_800BB620);
}

/* Task step: once the disc and the file reads are idle, run 800BB690 on a
 * separate stack. */
void func_800BB6E0(SlotTask *task) {
    u8 *stack;

    if (func_800286CC() == 0 && D_800C3CB8 == 0) {
        stack = func_80031BDC(0x1000, 1);
        STACK_ENTER(stack + 0xF00);
        func_800BB690(task);
        STACK_LEAVE();
        func_800320E8(stack);
    }
}

/* Start a task loading slot's gear object (800BB6E0). */
void func_800BB760(s32 slot) {
    u8 saved = D_800591AC;
    SlotTask *task;

    D_800591AC = 0;
    D_800591AF = 1;
    task = (SlotTask *)func_8001CD08(NULL, 4);
    func_8001CD6C(&task->task, (void (*)(Task *))func_800BB6E0);
    task->slot = slot;
    D_800591AF = 0;
    D_800C35D8++;
    D_800591AC = saved;
}

/* Reset the camera modes. */
void func_800BB7F8(void) {
    D_800C3674 = 0x200;
    D_800C3678 = -1;
    D_800C3CC4 = 0;
    D_800C3CBC = 1;
    func_800BC2F0(0);
}

/* Build view matrix m looking from eye at target with up vector up. */
void func_800BB844(MATRIX *m, SVECTOR *eye, SVECTOR *target, SVECTOR *up) {
    VECTOR v;
    VECTOR forward;
    VECTOR right;
    VECTOR upward;

    func_80021B14(&v, target->vx - eye->vx, target->vy - eye->vy, target->vz - eye->vz);
    upward.vx = up->vx;
    upward.vy = up->vy;
    upward.vz = up->vz;
    VectorNormal(&v, &forward);
    OuterProduct12(&upward, &forward, &v);
    VectorNormal(&v, &right);
    OuterProduct12(&forward, &right, &v);
    VectorNormal(&v, &upward);
    m->m[0][0] = right.vx;
    m->m[0][1] = right.vy;
    m->m[0][2] = right.vz;
    m->m[1][0] = upward.vx;
    m->m[1][1] = upward.vy;
    m->m[1][2] = upward.vz;
    m->m[2][0] = forward.vx;
    m->m[2][1] = forward.vy;
    m->m[2][2] = forward.vz;
    PushMatrix();
    ApplyMatrix(m, eye, &v);
    m->t[0] = -v.vx;
    m->t[1] = -v.vy;
    m->t[2] = -v.vz;
    PopMatrix();
}

/* Set the battle view from the camera points, shaken by 800c354c, and draw
 * the stage unless that is off. */
void func_800BB9D4(void) {
    func_800BB844(&D_800D309C.matrix, &D_800D3354, &D_800D335C, &D_800C3730);
    D_800D309C.matrix.t[0] += D_800C354C.vx;
    D_800D309C.matrix.t[1] += D_800C354C.vy;
    D_800D309C.matrix.t[2] += D_800C354C.vz;
    if (D_800C372C == 0) {
        func_800A4654(&D_800D309C.matrix, NULL, 0, BATTLE_AREA.ot, BATTLE_AREA.buffer, &D_800D3354, &D_800D335C,
                      0x1000);
    }
}

/* Step the battle camera: take its wanted points from the camera mode, move
 * the eye and look-at points a fraction (800c3674) of the way there, and
 * derive its angles and range. */
void func_800BBAB8(void) {
    SVECTOR *point;
    VECTOR step;
    VECTOR unused[2]; /* unused in the original; reserves 32 bytes */
    VECTOR delta;
    VECTOR unused2; /* unused in the original; reserves 16 bytes */
    VECTOR square;
    s32 horizontal;

    switch (D_800C3CC0) {
    case 0:
        break;
    case 1:
        func_800BC460(D_800C3678);
        break;
    case 2:
        D_800D30A0[0].vx = D_8006F99C.vx >> 16;
        D_800D30A0[0].vy = D_8006F99C.vy >> 16;
        D_800D30A0[0].vz = D_8006F99C.vz >> 16;
        point = &D_800D30A0[1];
        point->vx = D_8006F9AC.vx >> 16;
        point->vy = D_8006F9AC.vy >> 16;
        point->vz = D_8006F9AC.vz >> 16;
        break;
    case 3:
        /* step holds the wanted look-at, then eye point */
        ((SVECTOR *)&step)[1].vx = ((SVECTOR *)&step)[0].vx = D_800D39EC->x >> 16;
        ((SVECTOR *)&step)[0].vy = D_800D39EC->y >> 16;
        ((SVECTOR *)&step)[0].vz = D_800D39EC->z >> 16;
        ((SVECTOR *)&step)[1].vz = ((SVECTOR *)&step)[0].vz - func_8003F8CC(D_800C373C) * D_800C3738 / 4096;
        ((SVECTOR *)&step)[1].vy = ((SVECTOR *)&step)[0].vy - func_8003F8B0(D_800C373C) * D_800C3738 / 4096;
        D_800D309C.eye = ((SVECTOR *)&step)[1];
        D_800D309C.target = ((SVECTOR *)&step)[0];
        break;
    }
    if (D_800C3CBC == 1) {
        gte_lddp(D_800C3674);
        step.vx = D_800D309C.eye.vx - D_800D3354.vx;
        step.vy = D_800D309C.eye.vy - D_800D3354.vy;
        step.vz = D_800D309C.eye.vz - D_800D3354.vz;
        gte_ldlvl(&step);
        gte_gpf12();
        gte_stlvl(&step);
        if (step.vx | step.vz | step.vy) {
            D_800D3354.vx += step.vx;
            D_800D3354.vy += step.vy;
            D_800D3354.vz += step.vz;
        } else {
            SVECTOR *wanted = &D_800D309C.eye;

            D_800D3354.vx = wanted->vx;
            D_800D3354.vy = wanted->vy;
            D_800D3354.vz = wanted->vz;
        }
        step.vx = D_800D309C.target.vx - D_800D335C.vx;
        step.vy = D_800D309C.target.vy - D_800D335C.vy;
        step.vz = D_800D309C.target.vz - D_800D335C.vz;
        gte_ldlvl(&step);
        gte_gpf12();
        gte_stlvl(&step);
        if (step.vx | step.vz | step.vy) {
            D_800D335C.vx += step.vx;
            D_800D335C.vy += step.vy;
            D_800D335C.vz += step.vz;
        } else {
            SVECTOR *wanted = &D_800D309C.target;

            D_800D335C.vx = wanted->vx;
            D_800D335C.vy = wanted->vy;
            D_800D335C.vz = wanted->vz;
        }
    }
    delta.vx = D_800D335C.vx - D_800D3354.vx;
    delta.vy = D_800D335C.vy - D_800D3354.vy;
    delta.vz = D_800D335C.vz - D_800D3354.vz;
    Square0(&delta, &square);
    horizontal = SquareRoot0(square.vx + square.vz);
    D_800D309C.range = SquareRoot0(square.vx + square.vy + square.vz);
    D_800D309C.rot.vy = -ratan2(delta.vz, delta.vx);
    D_800D309C.rot.vx = -ratan2(delta.vy, horizontal);
    D_800D309C.rot.vz = 0;
}

/* Destroy of a camera sprite task: release its camera role (restoring the
 * saved point unless effects are off), free it, and when the last one ends
 * return to camera mode 800c367c. */
void func_800BBEE0(Task *node) {
    SpriteTask *task = (SpriteTask *)node;
    SVECTOR *point;
    Sprite *sprite = task->task.data;

    if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
        if (D_800C3680 == task) {
            D_800C3680 = NULL;
            if (D_800C37C8 == 0) {
                D_800D30A0[0].vx = D_800C3CCC.vx;
                D_800D30A0[0].vy = D_800C3CCC.vy;
                D_800D30A0[0].vz = D_800C3CCC.vz;
            }
        }
    } else if (D_800C3684 == task) {
        D_800C3684 = NULL;
        if (D_800C37C8 == 0) {
            point = &D_800D30A0[1];
            point->vx = D_800C3CD4.vx;
            point->vy = D_800C3CD4.vy;
            point->vz = D_800C3CD4.vz;
        }
    }
    if (sprite->motion.bits.owns_children) {
        func_8001CE74(&task->task);
    }
    func_8001CD94(&task->task);
    func_8001CB48(&task->auxiliary);
    func_800320E8(task);
    if (--D_800C3CC4 == 0) {
        func_800BC2F0(D_800C367C);
    }
}

/* Update of a camera sprite task: step its animation (twice when double
 * stepping), make its position the camera eye (group 0xA) or look-at point,
 * and destroy it when its animation ends. */
void func_800BC018(Task *task) {
    Sprite *sprite = task->data;

    func_80023210(sprite);
    func_80022CDC(sprite);
    if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
        D_8006F99C.vx = sprite->x;
        D_8006F99C.vy = sprite->y;
        D_8006F99C.vz = sprite->z;
    } else {
        D_8006F9AC.vx = sprite->x;
        D_8006F9AC.vy = sprite->y;
        D_8006F9AC.vz = sprite->z;
    }
    if (sprite->script != 0) {
        if (sprite->motion.bits.double_step) {
            func_80023210(sprite);
            func_80022CDC(sprite);
            if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
                D_8006F99C.vx = sprite->x;
                D_8006F99C.vy = sprite->y;
                D_8006F99C.vz = sprite->z;
            } else {
                D_8006F9AC.vx = sprite->x;
                D_8006F9AC.vy = sprite->y;
                D_8006F9AC.vz = sprite->z;
            }
            if (sprite->script == 0) {
                task->destroy(task);
            }
        }
    } else {
        task->destroy(task);
    }
}

/* Make sprite task a camera sprite: the eye (group 0xA) or look-at sprite,
 * saving the camera point or taking over (field34 1) from a running one,
 * else stopping the new one; then camera mode 2 follows the sprites. */
void func_800BC158(SpriteTask *task) {
    SVECTOR *point;
    Sprite *sprite = &task->sprite;

    if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
        if (D_800C3680 != NULL) {
            if (D_800C3680->sprite.frame != 1 && sprite->frame == 1) {
                D_800C3680->task.destroy(&D_800C3680->task);
                D_800C3680 = task;
            } else {
                sprite->countdown = 0;
                sprite->script = 0;
            }
        } else {
            D_800C3680 = task;
            D_800C3CCC.vx = D_800D30A0[0].vx;
            D_800C3CCC.vy = D_800D30A0[0].vy;
            D_800C3CCC.vz = D_800D30A0[0].vz;
        }
    } else if (D_800C3684 != NULL) {
        if (D_800C3684->sprite.frame != 1 && sprite->frame == 1) {
            D_800C3684->task.destroy(&D_800C3684->task);
            D_800C3684 = task;
        } else {
            sprite->countdown = 0;
            sprite->script = 0;
        }
    } else {
        D_800C3684 = task;
        point = &D_800D30A0[1];
        D_800C3CD4.vx = point->vx;
        D_800C3CD4.vy = point->vy;
        D_800C3CD4.vz = point->vz;
    }
    D_800C3CC4++;
    if (sprite->motion.bits.mirror) {
        sprite->direction = 0x800;
    } else {
        sprite->direction = 0;
    }
    func_8001CD74(&task->task, func_800BBEE0);
    func_8001CD6C(&task->task, func_800BC018);
    func_800BC2F0(2);
}

/* Set the camera mode: 2 puts the eye and look-at sprites at the saved
 * points, 4 sets D_800C3CBC to 5, others release them. */
void func_800BC2F0(s32 mode) {
    SVECTOR *point;

    D_800C3CC0 = mode;
    D_800C3CBC = 1;
    switch (mode) {
    case 4:
        D_800C3CBC = 5;
        break;
    case 2:
        D_8006F99C.vx = D_800D30A0[0].vx << 16;
        D_8006F99C.vy = D_800D30A0[0].vy << 16;
        D_8006F99C.vz = D_800D30A0[0].vz << 16;
        point = &D_800D30A0[1];
        D_8006F9AC.vx = point->vx << 16;
        D_8006F9AC.vy = point->vy << 16;
        D_8006F9AC.vz = point->vz << 16;
        break;
    default:
        if (D_800C3680 != NULL) {
            D_800C3680->task.destroy(&D_800C3680->task);
            D_800C3680 = NULL;
        }
        if (D_800C3684 != NULL) {
            D_800C3684->task.destroy(&D_800C3684->task);
            D_800C3684 = NULL;
        }
        break;
    }
}

/* Set D_800C367C. */
void func_800BC3F8(s32 value) {
    D_800C367C = value;
}

/* Start camera move (800BC460) unless effects are off; restore D_80059454. */
void func_800BC404(s32 mask) {
    if (D_800C37C8 == 0) {
        func_800BC2F0(1);
        func_800BC460(mask);
    }
    D_80059454 = D_800C3CDC;
}

/* Set the camera framing pitch. */
void func_800BC454(s16 value) {
    D_800C3740.vx = value;
}

/* Frame the camera on the party slots in mask: look at the middle of their
 * sprites from the framing angles, at a range that keeps the farthest sprite
 * (and its gear top) on screen; the points go to the camera's wanted eye and
 * look-at points. */
void func_800BC460(u32 mask) {
    VECTOR center;
    SVECTOR eye;
    SVECTOR target;
    MATRIX m;
    VECTOR offset;
    SVECTOR point;
    long screen[2];
    SVECTOR v;
    long result[2];
    MATRIX m2;
    VECTOR out;
    SVECTOR v2;
    MATRIX m3;
    VECTOR unused;
    SVECTOR v3;
    Sprite *sprite;
    s32 i;
    s32 count;
    s32 farthest;
    s32 minX, maxX, minY, maxY, minZ, maxZ;
    s32 distance;
    s32 range;
    u32 bits;

    memset(&center, 0, sizeof(center));
    farthest = 0;
    D_800C3678 = mask;
    i = 0;
    count = 0;
    for (bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
        if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
            count++;
            center.vx += sprite->x >> 1;
            center.vy += sprite->y >> 1;
            center.vz += sprite->z >> 1;
        }
    }
    if (count != 0) {
        center.vx = center.vx / count * 2;
        center.vy = center.vy / count * 2;
        center.vz = center.vz / count * 2;
        maxX = minX = center.vx;
        maxZ = minZ = center.vz;
        maxY = minY = center.vy;
        for (i = 0, bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
            if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
                if (maxX < sprite->x) {
                    maxX = sprite->x;
                }
                if (sprite->x < minX) {
                    minX = sprite->x;
                }
                if (maxZ < sprite->z) {
                    maxZ = sprite->z;
                }
                if (sprite->z < minZ) {
                    minZ = sprite->z;
                }
                if (maxY < sprite->y) {
                    maxY = sprite->y;
                }
                if (sprite->y < minY) {
                    minY = sprite->y;
                }
            }
        }
        center.vx = (minX + maxX) / 2;
        center.vy = (maxY + minY) / 2;
        center.vz = (minZ + maxZ) / 2;
        center.vx >>= 16;
        center.vy >>= 16;
        center.vz >>= 16;
        RotMatrix(&D_800C3740, &m);
        v.vx = 0;
        v.vy = 0;
        v.vz = ReadGeomScreen() * 8;
        ApplyMatrix(&m, &v, &offset);
        eye.vx = center.vx;
        eye.vy = center.vy;
        eye.vz = center.vz;
        target.vx = center.vx;
        target.vy = center.vy;
        target.vz = center.vz;
        eye.vx -= offset.vx;
        eye.vy += offset.vy;
        eye.vz -= offset.vz;
        func_800BB844(&m, &eye, &target, &D_800C3730);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (i = 0, bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
            if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
                point.vx = sprite->x >> 16;
                point.vy = sprite->y >> 16;
                point.vz = sprite->z >> 16;
                RotTransPers(&point, screen, &result[0], &result[1]);
                ((s16 *)screen)[0] -= 160;
                ((s16 *)screen)[1] -= 164;
                ((s16 *)screen)[0] <<= 2;
                ((s16 *)screen)[1] <<= 2;
                distance = ((s16 *)screen)[0] * ((s16 *)screen)[0];
                distance += ((s16 *)screen)[1] * ((s16 *)screen)[1];
                if (farthest < distance) {
                    farthest = distance;
                }
                if (BATTLE_AREA.slots[i].gear && D_800C3688 == 0) {
                    point.vy -= sprite->height;
                    RotTransPers(&point, screen, &result[0], &result[1]);
                    ((s16 *)screen)[0] -= 160;
                    ((s16 *)screen)[1] -= 164;
                    ((s16 *)screen)[0] <<= 2;
                    ((s16 *)screen)[1] <<= 2;
                    distance = ((s16 *)screen)[0] * ((s16 *)screen)[0];
                    distance += ((s16 *)screen)[1] * ((s16 *)screen)[1];
                    if (farthest < distance) {
                        farthest = distance;
                    }
                }
            }
        }
        farthest = SquareRoot0(farthest);
        if (farthest < 120) {
            RotMatrix(&D_800C3740, &m2);
            v2.vx = 0;
            v2.vy = 0;
            v2.vz = ReadGeomScreen() * 2;
            D_800C3CDC = ReadGeomScreen() * 2;
            ApplyMatrix(&m2, &v2, (VECTOR *)&point);
            eye.vx = center.vx;
            eye.vy = center.vy;
            eye.vz = center.vz;
            target.vx = center.vx;
            target.vy = center.vy;
            target.vz = center.vz;
            eye.vx -= (*(VECTOR *)&point).vx;
            eye.vy += (*(VECTOR *)&point).vy;
            eye.vz -= (*(VECTOR *)&point).vz;
            D_800D30A0[0].vx = eye.vx;
            D_800D30A0[0].vy = eye.vy;
            D_800D30A0[0].vz = eye.vz;
            {
                SVECTOR *p = &D_800D30A0[1];

                p->vx = target.vx;
                p->vy = target.vy;
                p->vz = target.vz;
            }
        } else {
            range = (farthest << 14) / 120;
            range = (range << 1) * ReadGeomScreen();
            range >>= 14;
            D_800C3CDC = range;
            RotMatrix(&D_800C3740, &m3);
            v3.vx = 0;
            v3.vy = 0;
            v3.vz = range;
            ApplyMatrix(&m3, &v3, &out);
            eye.vx = center.vx;
            eye.vy = center.vy;
            eye.vz = center.vz;
            target.vx = center.vx;
            target.vy = center.vy;
            target.vz = center.vz;
            eye.vx -= out.vx;
            eye.vy += out.vy;
            eye.vz -= out.vz;
            D_800D30A0[0].vx = eye.vx;
            D_800D30A0[0].vy = eye.vy;
            D_800D30A0[0].vz = eye.vz;
            {
                SVECTOR *p = &D_800D30A0[1];

                p->vx = target.vx;
                p->vy = target.vy;
                p->vz = target.vz;
            }
        }
    }
}

/* Camera mode 4 (800BC2F0), unless effects are disabled. */
void func_800BCAA4(void) {
    if (D_800C37C8 == 0) {
        func_800BC2F0(4);
    }
}

/* Camera mode 1 (800BC2F0), unless effects are disabled. */
void func_800BCAD0(void) {
    if (D_800C37C8 == 0) {
        func_800BC2F0(1);
    }
}

/* Shade the sprite by the cosine of angle (0x80 plus half, at most 0xFF)
 * and update it (8001F6B0). */
void func_800BCAFC(Sprite *sprite, s32 angle) {
    s32 level = func_8003F8B0(angle << 6) + 0x1000;

    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->red = level;
    sprite->green = level;
    sprite->blue = level;
    func_8001F6B0(sprite);
}

/* End the acting slot's pulse: restore its sprite's colour. */
void func_800BCB54(Task *task) {
    SlotPulse *pulse = D_800C3748;

    if (pulse != NULL) {
        pulse->sprite->colour_flags |= 1;
        func_8001CD94(&pulse->task);
        func_800320E8(pulse);
        D_800C3748 = NULL;
    }
}

/* Pulse update: the sprite's red and green-blue follow two phases of the
 * tick (0x80 plus half, at most 0xFF). */
void func_800BCBB4(Task *task) {
    SlotPulse *pulse = (SlotPulse *)task;
    s32 angle = pulse->tick << 6;
    Sprite *sprite = pulse->sprite;
    s32 level;

    level = func_8003F8B0(angle) + 0x1000;
    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->red = level;
    level = func_8003F8CC(angle) + 0x1000;
    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->green = level;
    sprite->blue = level;
    func_8001F6B0(sprite);
    pulse->tick++;
}

/* Start the acting slot's pulse (unless it already runs for that slot). */
void func_800BCC60(void) {
    SlotPulse *pulse;
    Sprite *sprite;

    if (D_800C3748 != NULL) {
        if (D_800C3748->slot == D_800C4922) {
            return;
        }
        func_800BCB54(NULL);
    }
    if (BATTLE_AREA.tasks[AREA_ACTING_SLOT] == NULL) {
        return;
    }
    pulse = (SlotPulse *)func_8001CD08(&BATTLE_AREA.tasks[AREA_ACTING_SLOT]->task, sizeof(SlotPulse) - sizeof(Task));
    D_800C3748 = pulse;
    func_8001CD6C(&pulse->task, func_800BCBB4);
    func_8001CD74(&pulse->task, func_800BCB54);
    if (D_800591AC) {
        D_80059464--;
    }
    pulse->task.link.word &= 0x7FFFFFFF;
    pulse->sprite = sprite = BATTLE_AREA.sprites[AREA_ACTING_SLOT];
    pulse->slot = AREA_ACTING_SLOT;
    sprite->colour_flags &= ~1;
}

/* Clear the highlighted slots. */
void func_800BCD8C(void) {
    D_800C3D14 = 0;
}

/* Highlight the slots of mask (bit per slot): pulse the acting slot while
 * any is set, and give each highlighted slot's sprite a ring (ending the
 * others'). */
void func_800BCD98(u16 mask) {
    s32 slot;
    SpriteTask *task;
    Task *ring;

    D_800C3D14 = mask;
    if (mask) {
        func_800BCC60();
    } else {
        func_800BCB54(NULL);
    }
    for (slot = 0; slot != 11; slot++, mask >>= 1) {
        if (mask & 1) {
            task = BATTLE_AREA.tasks[slot];
            if (task != NULL && func_8001D0A4(&task->task, func_800BCFAC) == NULL) {
                func_800BD098(task);
            }
        } else {
            task = BATTLE_AREA.tasks[slot];
            if (task != NULL) {
                ring = func_8001D0A4(&task->task, func_800BCFAC);
                if (ring != NULL) {
                    ring->destroy(ring);
                }
            }
        }
    }
}

/* Ring draw: place, spin and scale it (to a constant screen size) and draw
 * its script into this buffer's vertices. */
void func_800BCEAC(Task *draw) {
    SlotRing *ring = draw->data;
    MATRIX m;
    VECTOR scale;
    s32 size;

    TransMatrix(&m, &ring->pos);
    func_8003F738(&ring->angle, &m);
    CompMatrix(&D_8004FBB8, &m, &m);
    size = ReadGeomScreen() << 12;
    if (m.t[2] != 0) {
        size /= m.t[2];
    }
    size = 0x1000000 / size / 2;
    func_80021B14(&scale, size, size, size);
    ScaleMatrixL(&m, &scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    func_800B1F6C(ring->script, ring->vertices[BATTLE_AREA.buffer], (u32 *)D_8005956C, 0, 0, 0);
}

/* Ring update: follow the sprite (0x20 above its top), shade it and spin. */
void func_800BCFAC(Task *task) {
    SlotRing *ring = task->data;
    Sprite *sprite = ring->sprite;

    ring->pos.vx = FIXED_WHOLE(sprite->x);
    ring->pos.vy = FIXED_WHOLE(sprite->y);
    ring->pos.vz = FIXED_WHOLE(sprite->z);
    ring->pos.vy = ring->pos.vy - ring->height - 0x20;
    func_800BCAFC(sprite, ++ring->tick);
    ring->angle.vy += 0x10;
}

/* Ring destroy: restore the sprite's colour and free the ring. */
void func_800BD024(Task *task) {
    SlotRing *ring = task->data;
    Sprite *sprite = ring->sprite;

    sprite->colour_flags |= 1;
    func_8001F6B0(sprite);
    func_80025180((u32)ring->vertices[0]);
    func_8001CE74(task);
    func_8001CD94(task);
    func_800320E8(ring);
}

/* Give an actor task's sprite a ring. */
void func_800BD098(SpriteTask *owner) {
    Sprite *sprite = owner->task.data;
    SlotRing *ring = (SlotRing *)func_8001D1D8(sizeof(SlotRing), &owner->task, func_800BCFAC, func_800BCEAC, func_800BD024);
    s32 size;
    u8 *vertices;

    ring->sprite = sprite;
    ring->height = sprite->height;
    if (D_800591AC) {
        D_80059464--;
    }
    ring->task.link.word &= 0x7FFFFFFF;
    func_80021B04(&ring->angle, 0, 0, 0);
    size = func_800B16A4((ScriptEntry *)func_800B168C(D_8001C76C, 0));
    vertices = func_80031BDC(size * 2, 0);
    func_800B1720(func_800B168C(D_8001C76C, 0), vertices, 0, 1);
    memcpy(vertices + size, vertices, size);
    ring->vertices[0] = vertices;
    ring->vertices[1] = vertices + size;
    ring->script = func_800B168C(D_8001C76C, 0);
    sprite->colour_flags &= ~1;
    func_8001F6B0(sprite);
    func_800BCFAC(&ring->task);
}

/* Show the current event's result on slot's sprite (800BD3AC), with the
 * running total and colour kind, once. */
void func_800BD1FC(s32 slot) {
    Sprite *sprite;
    s32 code;
    s32 total;
    s32 colour;
    s32 amount;

    if (BATTLE_AREA.events[D_800C360C - 1].codes[slot] != 0xFF && BATTLE_AREA.events[D_800C360C - 1].amounts[slot] != 0xFFFF) {
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite != NULL) {
            code = BATTLE_AREA.events[D_800C360C - 1].codes[slot];
            total = BATTLE_AREA.events[D_800C360C - 1].accumulated[slot];
            colour = BATTLE_AREA.events[D_800C360C - 1].accumulatedCodes[slot];
            amount = BATTLE_AREA.events[D_800C360C - 1].amounts[slot];
            D_800C3D38 = total;
            D_800D3630 = colour;
            func_800BD3AC(sprite, amount, code);
            BATTLE_AREA.events[D_800C360C - 1].amounts[slot] = 0xFFFF;
        }
    }
}

/* Show the current event's results on every slot (800BD1FC); once a slot
 * has code 7, not on the acting sprite. */
void func_800BD2E4(void) {
    u8 skipActor = 0;
    s32 slot;
    Sprite *sprite;

    for (slot = 0; slot != 11; slot++) {
        if (BATTLE_AREA.events[D_800C360C - 1].codes[slot] == 7) {
            skipActor = 1;
        }
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite != NULL && (D_800C3E1C != sprite || !skipActor)) {
            func_800BD1FC(slot);
        }
    }
}
