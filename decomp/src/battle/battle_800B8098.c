/* Battle unit from 800B8098 to the end of the overlay text, built like
 * battle_800B15D8.c by the Cygnus CDK GCC 2.7.2. Its jump tables sit at 0
 * mod 8 (800B8098's at 0x80070A10) where the previous unit's sit at 4
 * (800B7870's at 0x800709FC), so a unit starts between the two; the
 * functions from 800B7C28 to 800B8068 have no rodata, and the boundary is
 * placed at the first function that has. Later tables flip phase again
 * (0x80070AB8 -> 0x80070ADC -> 0x80070B08, 0x80070BB0 -> 0x80070C14), so
 * this unit holds further boundaries not yet placed. */
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
    func_80021B04(&D_800D30A0[0], D_800658C8->eye[0], D_800658C8->eye[1], D_800658C8->eye[2]);
    func_80021B04(&D_800D3354, D_800658C8->eye[0], D_800658C8->eye[1], D_800658C8->eye[2]);
    func_80021B04(&D_800D30A0[1], D_800658C8->lookAt[0], D_800658C8->lookAt[1], D_800658C8->lookAt[2]);
    func_80021B04(&D_800D335C, D_800658C8->lookAt[0], D_800658C8->lookAt[1], D_800658C8->lookAt[2]);
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B838C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B853C);

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
    func_8003852C(D_8005919C);
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

#ifdef NON_MATCHING
/* Start the first frame: the frame skip from the gear enemies present,
 * draw into the second buffer with the first one's background colour. */
void func_800B88C4(void) {
    s32 i;
    s32 skip;
    FrameBuffer *buffer;

    D_800C3D58 = 0;
    for (i = 3; i != 11; i++) {
        if (BATTLE_AREA.slots[i].field2 < 0x11 && BATTLE_AREA.slots[i].gear != 0) {
            D_800C3D58++;
        }
    }
    D_80059198 = D_800C3D58 / 2 - 1;
    if (D_80059198 < 0) {
        D_80059198 = 0;
    }
    skip = D_80059198;
    D_80059198 = 0;
    D_80050100 = 2;
    BATTLE_AREA.frameTicks = skip;
    buffer = &BATTLE_AREA.buffers[0];
    if (BATTLE_AREA.current == buffer) {
        buffer = &BATTLE_AREA.buffers[1];
    }
    BATTLE_AREA.current = buffer;
    BATTLE_AREA.ot = buffer->ot;
    ClearOTagR(buffer->ot, 0x1000);
    BATTLE_AREA.buffer = 1;
    BATTLE_AREA.current = &BATTLE_AREA.buffers[1];
    BATTLE_AREA.field8DA8 = 0;
    BATTLE_AREA.buffers[1].drawEnv.isbg = BATTLE_AREA.buffers[0].drawEnv.isbg;
    BATTLE_AREA.buffers[1].drawEnv.r0 = BATTLE_AREA.buffers[0].drawEnv.r0;
    BATTLE_AREA.buffers[1].drawEnv.g0 = BATTLE_AREA.buffers[0].drawEnv.g0;
    BATTLE_AREA.buffers[1].drawEnv.b0 = BATTLE_AREA.buffers[0].drawEnv.b0;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B88C4);
#endif

void func_800B89F4(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B89FC);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B8DA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B8EBC);

/* Return the sprite to its idle motion. */
void func_800B9020(BattleSprite *sprite) {
    func_800245D8(sprite, sprite->idle.mode);
    func_80021BF8(sprite, NULL);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B905C);

/* Count a step of the battle menu (field34), when there is one. */
void func_800B9258(void) {
    if (D_800C3610 != NULL) {
        D_800C3610->field34++;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B9284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B9508);

/* Mark the battle menu (field48) with its state. */
void func_800B9B30(void) {
    D_800C3610->field48 = 1;
    D_800C3610->field49 = D_800C3610->state;
}

/* Turn two sprites to face each other (the second not while its motion
 * mode is 0x15). */
void func_800B9B54(BattleSprite *sprite, BattleSprite *other) {
    if (sprite != other) {
        func_800223B0(sprite, func_800BEF24(sprite, other));
        func_80021FE0(sprite, func_800BEF24(sprite, other));
        if (other->motion.bytes[3] != 0x15) {
            func_800223B0(other, func_800BEF24(other, sprite));
            func_80021FE0(other, func_800BEF24(other, sprite));
        }
    }
}

/* Put the sprite at its target, idle, facing other. Defined without a
 * prototype: 800BF0C4 calls it with the sprite alone. */
void func_800B9C00(sprite, other)
    BattleSprite *sprite;
    BattleSprite *other;
{
    sprite->x.fixed = sprite->target[0] << 16;
    sprite->z.fixed = sprite->target[2] << 16;
    D_800C3610->field48 = 1;
    func_800BF0B4(4);
    func_800245D8(sprite, sprite->idle.mode);
    func_800B9B54(sprite, other);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B9C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800B9F78);

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
void func_800BA59C(BattleSprite *sprite, s16 direction) {
    s32 speed;

    sprite->direction = direction;
    speed = sprite->speed >> 3;
    sprite->velocity[0] = (func_8003F8CC(direction) >> 1) * speed >> 8;
    sprite->velocity[2] = -((func_8003F8B0(sprite->direction) >> 1) * speed) >> 8;
}

/* Aim sprite's jump at its target: turn it towards the target and set the
 * rising speed that lands it on the ground there (or the target's height
 * when that is higher). */
void func_800BA614(BattleSprite *sprite) {
    VECTOR delta;
    SVECTOR point;
    VECTOR out;
    s32 triangle;
    s32 height;
    s16 angle;
    s32 distance;

    func_80021B04(&point, sprite->target[0], sprite->target[1], sprite->target[2]);
    triangle = func_800A5914(&point, sprite->triangle, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    if (point.vy > sprite->target[1]) {
        point.vy = sprite->target[1];
    }
    height = ((point.vy << 16) - sprite->y.fixed) >> 16;
    delta.vx = sprite->target[0] - (sprite->x.fixed >> 16);
    delta.vz = sprite->target[2] - (sprite->z.fixed >> 16);
    angle = -ratan2(delta.vz, delta.vx);
    func_8004A414(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz);
    sprite->velocity[1] = -sprite->gravity * distance * 16 / (sprite->speed >> 11) + sprite->speed * height / distance;
    func_800BA59C(sprite, angle);
}

/* Aim sprite's jump at its target keeping its rising speed: snap it to
 * whole units, turn it towards the target and set the speed that covers the
 * distance (and the height difference) in the jump's frames. */
void func_800BA768(BattleSprite *sprite) {
    VECTOR delta;
    SVECTOR point;
    VECTOR out;
    s32 frames;
    s32 triangle;
    s32 angle;
    s32 distance;
    s32 height;

    frames = -(sprite->velocity[1] * 2 / sprite->gravity);
    sprite->x.fixed &= 0xFFFF0000;
    sprite->y.fixed &= 0xFFFF0000;
    sprite->z.fixed &= 0xFFFF0000;
    delta.vx = sprite->target[0] - (sprite->x.fixed >> 16);
    delta.vz = sprite->target[2] - (sprite->z.fixed >> 16);
    delta.vy = 0;
    angle = -ratan2(delta.vz, delta.vx);
    func_8004A414(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz) << 16;
    if (frames != 0) {
        sprite->speed = distance / frames;
    } else {
        sprite->speed = 0;
    }
    func_80021B04(&point, sprite->target[0], sprite->target[1], sprite->target[2]);
    triangle = func_800A5914(&point, sprite->triangle, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    if (point.vy > sprite->target[1]) {
        point.vy = sprite->target[1];
    }
    height = (point.vy << 16) - sprite->y.fixed;
    if (frames != 0) {
        sprite->velocity[1] += height / frames;
    }
    func_800BA59C(sprite, angle);
    func_80022B2C(sprite);
}

/* Put sprite on the scene's ground: its triangle and ground height. */
void func_800BA8F4(BattleSprite *sprite) {
    SVECTOR point;
    VECTOR out;
    s32 triangle;

    point.vx = sprite->x.fixed >> 16;
    point.vy = sprite->y.fixed >> 16;
    point.vz = sprite->z.fixed >> 16;
    triangle = func_800A5914(&point, sprite->triangle, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    sprite->ground = point.vy;
    sprite->triangle = triangle;
}

/* Create a sprite task (updated by 800BAC50, drawn by 800BAB0C) at x, y, z
 * facing direction, running animation. */
ActorTask *func_800BA984(s32 resource, s16 a, s16 b, s16 c, s16 d, s16 e, s16 x, s16 y, s16 z, s16 animation,
                         s16 direction, s32 unused11, s32 unused12, s32 g) {
    ActorTask *task;
    BattleSprite *sprite;

    task = func_8001D1D8(0x19C, NULL, func_800BAC50, func_800BAB0C, func_800BABDC);
    sprite = (BattleSprite *)(task + 1);
    task->data = sprite;
    task->draw.data = sprite;
    task->draw.owner = NULL;
    func_800242F4(sprite, resource, a, b, c, d, e, g);
    sprite->task = task;
    sprite->x.fixed = x << 16;
    sprite->y.fixed = y << 16;
    sprite->z.fixed = z << 16;
    sprite->idle.mode = animation;
    sprite->render.word |= 4;
    sprite->direction = direction;
    func_80022000(&sprite->x.fixed, 0x2000);
    sprite->field82 = 0x2000;
    sprite->triangle = 0;
    func_800245D8(sprite, animation);
    return task;
}

/* Draw a sprite task: its depth in the view, and its parts when visible. */
void func_800BAB0C(ActorTask *task) {
    SVECTOR point;
    s32 result[2]; /* screen position, then the GTE flags */
    VECTOR unused;
    BattleSprite *sprite;
    s32 depth;

    if (D_800C3664 == 0) {
        sprite = task->data;
        point.vx = sprite->x.fixed >> 16;
        point.vy = sprite->y.fixed >> 16;
        point.vz = sprite->z.fixed >> 16;
        SetRotMatrix(&D_800D30BC);
        SetTransMatrix(&D_800D30BC);
        depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> D_80050100) + sprite->depthBias;
        if (result[1] & 0x8000) {
            depth = 0;
        }
        sprite->depth = depth;
        if ((u32)(depth - 1) < 0xFFF) {
            func_8001E298(sprite, D_8005956C + depth);
        }
    }
}

#ifdef NON_MATCHING
/* Destroy a sprite task: its part block, children, sprite and node.
 * Nonmatching: the original computes the sprite from $a0 before copying the
 * task to $s0. */
void func_800BABDC(ActorTask *task) {
    BattleSprite *sprite = (BattleSprite *)(task + 1);
    void *parts = sprite->view->parts;

    if (parts != NULL) {
        func_800320E8(parts);
    }
    func_8001CE74(task);
    func_8001D3F4(sprite);
    func_8001CB48(&task->draw);
    func_8001CD94(task);
    func_800320E8(task);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BABDC);
#endif

/* Update a sprite task (twice with double steps) unless paused. */
void func_800BAC50(ActorTask *task) {
    BattleSprite *sprite = task->data;

    if (D_800C3664 == 0) {
        func_80023210(sprite);
        func_80022CDC(sprite);
        if (sprite->motion.bits.doubleStep) {
            func_80023210(sprite);
            func_80022CDC(sprite);
        }
    }
}

/* Party slot's sprite on screen: its position, depth and a box around it. */
void func_800BACBC(s32 slot, s16 *x, s16 *y, s16 *depth, s16 *left, s16 *width, s16 *centre) {
    SVECTOR point;
    s16 sxy[2];
    s32 p;
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];

    point.vx = sprite->x.fixed >> 16;
    point.vy = sprite->y.fixed >> 16;
    point.vz = sprite->z.fixed >> 16;
    PushMatrix();
    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    *depth = RotTransPers(&point, (s32 *)sxy, &p, &p) >> 4;
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
    ActorTask *task = BATTLE_AREA.tasks[slot];

    if (task != NULL) {
        func_800BFC80((BattleSprite *)task, 0, 2); /* the slot's task where a sprite is taken */
        if (slot < 3) {
            if (BATTLE_AREA.sources[slot].data != NULL) {
                func_800320E8(BATTLE_AREA.sources[slot].data);
            }
            BATTLE_AREA.sources[slot].data = NULL;
        }
        task->destroy(task);
        func_8001CE74(task);
        BATTLE_AREA.sprites[slot] = NULL;
        BATTLE_AREA.tasks[slot] = NULL;
    }
}

/* Face slot's sprite along its side (turned for a nonzero target code),
 * unless it runs animation 0x15. */
void func_800BAEB8(s32 slot) {
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];
    s32 direction;

    if (sprite->motion.bytes[3] != 0x15) {
        direction = (BATTLE_AREA.slots[slot].targetCode != 0) << 11;
        func_800223B0(&sprite->x.fixed, direction);
        func_80021FE0(&sprite->x.fixed, direction);
    }
}

void func_800BAF40(void) {
}

#ifdef NON_MATCHING
/* Send party slot's sprite off: select it (800BC404), run its exit
 * animation 0x16 and wait for it and its tasks, then remove the sprite and
 * load the slot's gear object in its place (800BB760), waiting for it.
 * Nonmatching: battle_core.h declares slot u8 and 800BC404's mask u16; the
 * original takes and passes words, unextended. */
void func_800BAF48(u8 slot) {
    BattleSprite *sprite;
    s32 tasks;

    func_800BC404(1 << slot);
    func_800BC404(0);
    tasks = D_80059188;
    sprite = BATTLE_AREA.sprites[slot];
    func_800B8D7C();
    func_800245D8(sprite, 0x16);
    while (sprite->countdown != 0 && sprite->motion.bytes[3] == 0x16) {
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
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BAF48);
#endif

/* Destroy the party members' sprites other than keep's that are not in
 * use, then end their stage objects (800B14CC). */
void func_800BB080(s32 keep) {
    s32 i;
    BattleSprite *sprite;

    for (i = 0; i != 3; i++) {
        if (i != keep) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL && sprite->field48 == 0) {
                sprite->task->destroy(sprite->task);
                BATTLE_AREA.sprites[i] = NULL;
                BATTLE_AREA.tasks[i] = NULL;
            }
        }
    }
    func_800B14CC(keep);
}

/* Update of a sprite following its slot's stage object: step its animation
 * while it runs, then put it at the object's position. */
void func_800BB13C(ActorTask *task) {
    SVECTOR unused; /* the original's frame has this unused local */
    BattleSprite *sprite = task->data;
    u32 low = sprite->frameBits.bits.slotLow;
    BattleObject *object = D_800D3368[sprite->motion.bits.slotHigh << 2 | low];

    if (object != NULL) {
        if (sprite->countdown == 0) {
            sprite->framesLeft = 0;
        }
        if (sprite->framesLeft != 0) {
            func_80023210(sprite);
            func_80022CDC(sprite);
            if (sprite->motion.bits.doubleStep) {
                func_80023210(sprite);
                func_80022CDC(sprite);
            }
        }
        sprite->x.fixed = object->hierarchy->translation[0] << 16;
        sprite->y.fixed = object->hierarchy->translation[1] << 16;
        sprite->z.fixed = object->hierarchy->translation[2] << 16;
    }
}

/* Draw of a slot-following sprite: its size from the slot's object and its
 * depth in the view. */
void func_800BB248(ActorTask *task) {
    SVECTOR point;
    s32 result[2]; /* screen position, then the GTE flags */
    BattleSprite *sprite = task->data;
    s32 depth;
    u32 low;

    low = sprite->frameBits.bits.slotLow;
    sprite->size = func_800AA600(sprite->motion.bits.slotHigh << 2 | low);
    sprite->halfSize = sprite->size / 2;
    point.vx = sprite->x.fixed >> 16;
    point.vy = sprite->y.fixed >> 16;
    point.vz = sprite->z.fixed >> 16;
    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> D_80050100) + sprite->depthBias;
    if (result[1] & 0x8000) {
        depth = 0;
    }
    sprite->depth = depth;
}

/* Destroy a task node. */
void func_800BB314(ActorTask *task) {
    SVECTOR unused; /* the original's frame has this unused local */

    func_8001CB48(&task->draw);
    func_8001CD94(task);
    func_800320E8(task);
}

/* Create slot's sprite following its stage object (800BB13C, 800BB248),
 * unless it has one. */
void func_800BB350(u32 slot) {
    ActorTask *task;
    BattleSprite *sprite;
    u8 saved;

    if (BATTLE_AREA.sprites[slot] == NULL) {
        saved = D_800591AC;
        D_800591AC = 0;
        task = func_8001D1D8(0x19C, NULL, func_800BB13C, func_800BB248, func_800BB314);
        sprite = (BattleSprite *)(task + 1);
        sprite->task = task;
        task->data = sprite;
        task->draw.data = sprite;
        func_80023804(sprite);
        func_800239A0(sprite);
        sprite->flags.bits.group = 4;
        sprite->task = task;
        sprite->render.word &= ~3;
        sprite->frameBits.bits.sequencerOwned = 0;
        sprite->resource->field8 = 0;
        sprite->resource->fieldC = 0;
        sprite->field82 = D_800591A8;
        sprite->x.fixed = (u16)BATTLE_AREA.slots[slot].x << 16;
        sprite->z.fixed = (u16)BATTLE_AREA.slots[slot].z << 16;
        sprite->y.fixed = 0;
        sprite->size = func_800AA600(slot);
        sprite->field82 = 0x2000;
        sprite->halfSize = sprite->size >> 1;
        func_80022000(&sprite->x.fixed, 0x2000);
        sprite->base = D_8006BE10;
        BATTLE_AREA.sprites[slot] = sprite;
        BATTLE_AREA.tasks[slot] = task;
        sprite->field4C = 0;
        sprite->field48 = 0;
        D_800591AC = saved;
        sprite->frameBits.bits.slotLow = slot;
        sprite->motion.bits.slotHigh = slot >> 2;
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
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BB620);
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
    task = func_8001CD08(NULL, 4);
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BB6E0);
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
    func_80048D7C(&v, &forward);
    func_8004A480(&upward, &forward, &v);
    func_80048D7C(&v, &right);
    func_8004A480(&forward, &right, &v);
    func_80048D7C(&v, &upward);
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
    VECTOR unused[2]; /* the original's frame has these unused locals */
    VECTOR delta;
    VECTOR unused2;
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
        ((SVECTOR *)&step)[1].vx = ((SVECTOR *)&step)[0].vx = D_800D39EC->x.fixed >> 16;
        ((SVECTOR *)&step)[0].vy = D_800D39EC->y.fixed >> 16;
        ((SVECTOR *)&step)[0].vz = D_800D39EC->z.fixed >> 16;
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
    func_8004A414(&delta, &square);
    horizontal = SquareRoot0(square.vx + square.vz);
    D_800D309C.range = SquareRoot0(square.vx + square.vy + square.vz);
    D_800D309C.rot.vy = -ratan2(delta.vz, delta.vx);
    D_800D309C.rot.vx = -ratan2(delta.vy, horizontal);
    D_800D309C.rot.vz = 0;
}

/* Destroy of a camera sprite task: release its camera role (restoring the
 * saved point unless effects are off), free it, and when the last one ends
 * return to camera mode 800c367c. */
void func_800BBEE0(ActorTask *task) {
    SVECTOR *point;
    BattleSprite *sprite = task->data;

    if (sprite->flags.bits.group == 0xA) {
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
    if (sprite->motion.bits.owned) {
        func_8001CE74(task);
    }
    func_8001CD94(task);
    func_8001CB48(&task->draw);
    func_800320E8(task);
    if (--D_800C3CC4 == 0) {
        func_800BC2F0(D_800C367C);
    }
}

/* Update of a camera sprite task: step its animation (twice when double
 * stepping), make its position the camera eye (group 0xA) or look-at point,
 * and destroy it when its animation ends. */
void func_800BC018(ActorTask *task) {
    BattleSprite *sprite = task->data;

    func_80023210(sprite);
    func_80022CDC(sprite);
    if (sprite->flags.bits.group == 0xA) {
        D_8006F99C.vx = sprite->x.fixed;
        D_8006F99C.vy = sprite->y.fixed;
        D_8006F99C.vz = sprite->z.fixed;
    } else {
        D_8006F9AC.vx = sprite->x.fixed;
        D_8006F9AC.vy = sprite->y.fixed;
        D_8006F9AC.vz = sprite->z.fixed;
    }
    if (sprite->framesLeft != 0) {
        if (sprite->motion.bits.doubleStep) {
            func_80023210(sprite);
            func_80022CDC(sprite);
            if (sprite->flags.bits.group == 0xA) {
                D_8006F99C.vx = sprite->x.fixed;
                D_8006F99C.vy = sprite->y.fixed;
                D_8006F99C.vz = sprite->z.fixed;
            } else {
                D_8006F9AC.vx = sprite->x.fixed;
                D_8006F9AC.vy = sprite->y.fixed;
                D_8006F9AC.vz = sprite->z.fixed;
            }
            if (sprite->framesLeft == 0) {
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
void func_800BC158(ActorTask *task) {
    SVECTOR *point;
    BattleSprite *sprite = (BattleSprite *)(task + 1);

    if (sprite->flags.bits.group == 0xA) {
        if (D_800C3680 != NULL) {
            if (((BattleSprite *)(D_800C3680 + 1))->frame != 1 && sprite->frame == 1) {
                D_800C3680->destroy(D_800C3680);
                D_800C3680 = task;
            } else {
                sprite->countdown = 0;
                sprite->framesLeft = 0;
            }
        } else {
            D_800C3680 = task;
            D_800C3CCC.vx = D_800D30A0[0].vx;
            D_800C3CCC.vy = D_800D30A0[0].vy;
            D_800C3CCC.vz = D_800D30A0[0].vz;
        }
    } else if (D_800C3684 != NULL) {
        if (((BattleSprite *)(D_800C3684 + 1))->frame != 1 && sprite->frame == 1) {
            D_800C3684->destroy(D_800C3684);
            D_800C3684 = task;
        } else {
            sprite->countdown = 0;
            sprite->framesLeft = 0;
        }
    } else {
        D_800C3684 = task;
        point = &D_800D30A0[1];
        D_800C3CD4.vx = point->vx;
        D_800C3CD4.vy = point->vy;
        D_800C3CD4.vz = point->vz;
    }
    D_800C3CC4++;
    if (sprite->motion.bits.flip) {
        sprite->direction = 0x800;
    } else {
        sprite->direction = 0;
    }
    func_8001CD74(task, func_800BBEE0);
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BC018);
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
            D_800C3680->destroy(D_800C3680);
            D_800C3680 = NULL;
        }
        if (D_800C3684 != NULL) {
            D_800C3684->destroy(D_800C3684);
            D_800C3684 = NULL;
        }
        break;
    }
}

/* Set D_800C367C. */
void func_800BC3F8(s32 value) {
    D_800C367C = value;
}

#ifdef NON_MATCHING
/* Start camera move (800BC460) unless effects are off; restore D_80059454.
 * Nonmatching: battle_core.h declares mask u16, which the original passes on
 * unextended (its own parameter is a word). */
void func_800BC404(u16 mask) {
    if (D_800C37C8 == 0) {
        func_800BC2F0(1);
        func_800BC460(mask);
    }
    D_80059454 = D_800C3CDC;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BC404);
#endif

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
    s32 screen[2];
    SVECTOR v;
    s32 result[2];
    MATRIX m2;
    VECTOR out;
    SVECTOR v2;
    MATRIX m3;
    VECTOR unused;
    SVECTOR v3;
    BattleSprite *sprite;
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
            center.vx += sprite->x.fixed >> 1;
            center.vy += sprite->y.fixed >> 1;
            center.vz += sprite->z.fixed >> 1;
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
                if (maxX < sprite->x.fixed) {
                    maxX = sprite->x.fixed;
                }
                if (sprite->x.fixed < minX) {
                    minX = sprite->x.fixed;
                }
                if (maxZ < sprite->z.fixed) {
                    maxZ = sprite->z.fixed;
                }
                if (sprite->z.fixed < minZ) {
                    minZ = sprite->z.fixed;
                }
                if (maxY < sprite->y.fixed) {
                    maxY = sprite->y.fixed;
                }
                if (sprite->y.fixed < minY) {
                    minY = sprite->y.fixed;
                }
            }
        }
        center.vx = (minX + maxX) / 2;
        center.vy = (maxY + minY) / 2;
        center.vz = (minZ + maxZ) / 2;
        center.vx >>= 16;
        center.vy >>= 16;
        center.vz >>= 16;
        func_8004ABBC(&D_800C3740, &m);
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
                point.vx = sprite->x.fixed >> 16;
                point.vy = sprite->y.fixed >> 16;
                point.vz = sprite->z.fixed >> 16;
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
                    point.vy -= sprite->size;
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
            func_8004ABBC(&D_800C3740, &m2);
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
            func_8004ABBC(&D_800C3740, &m3);
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
void func_800BCAFC(BattleSprite *sprite, s32 angle) {
    s32 level = func_8003F8B0(angle << 6) + 0x1000;

    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->colour[0] = level;
    sprite->colour[1] = level;
    sprite->colour[2] = level;
    func_8001F6B0(sprite);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BCB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BCBB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BCC60);

/* Clear the highlighted slots. */
void func_800BCD8C(void) {
    D_800C3D14 = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BCD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BCEAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BCFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD024);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD1FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD2E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD3AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD7A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD810);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BD974);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDB74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDC14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDC78);

/* End the effect task D_800D2D68 (task): its draw task and itself. */
void func_800BDCF8(ActorTask *task) {
    D_800D2D68 = NULL;
    func_8001CB48(&task->draw);
    func_8001CD94(task);
}

void func_800BDD34(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDD3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDE58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BDF1C);

/* End the effect task D_800D2D68 (800BDCF8), if any. */
void func_800BE0DC(void) {
    if (D_800D2D68 != NULL) {
        func_800BDCF8(D_800D2D68);
    }
}

/* Clear D_800D2D68 and D_800C374C. */
void func_800BE108(void) {
    D_800D2D68 = 0;
    D_800C374C = 0;
}

/* Popup update: spin and shrink it, fade its colour, end it with its life. */
void func_800BE11C(NumberPopup *popup) {
    popup->angle.vz += popup->spin;
    popup->scale.vx += 0x330;
    popup->scale.vy += 0x330;
    popup->scale.vz += 0x330;
    popup->colour.rgbc[0] = func_80021AD8(popup->colour.rgbc[0], -4);
    popup->colour.rgbc[1] = func_80021AD8(popup->colour.rgbc[1], -4);
    popup->colour.rgbc[2] = func_80021AD8(popup->colour.rgbc[2], -4);
    if (--popup->life == 0) {
        popup->destroy(popup);
    }
}

/* Popup drawing: its glyphs six times, each copy turned back 5 more and
 * shrunk by 0x330, centred on the screen from the geometry offset. */
void func_800BE1C4(PopupTask *task) {
    MATRIX m;
    SVECTOR unused; /* allocated in the original frame */
    SVECTOR angle;
    VECTOR offset;
    VECTOR scale;
    s32 x;
    s32 y;
    NumberPopup *popup = task->popup;
    s32 i;
    s32 j;
    PopupGlyph *glyph;

    ReadGeomOffset(&x, &y);
    offset.vx = (0xA0 - x) * 2;
    offset.vy = (0x46 - y) * 2;
    offset.vz = ReadGeomScreen();
    D_800C377C = ReadGeomScreen();
    func_80021B24(&angle, &popup->angle);
    scale.vx = popup->scale.vx;
    scale.vy = popup->scale.vy;
    scale.vz = popup->scale.vz;
    for (i = 0; i != 6; i++) {
        func_8003F738(&angle, &m);
        TransMatrix(&m, &offset);
        CompMatrix(&D_800C3760, &m, &m);
        ScaleMatrix(&m, &scale);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (j = 0, glyph = popup->glyphs; j != popup->glyphCount; j++, glyph++) {
            func_800BD810(glyph, popup->colour.word);
        }
        angle.vz -= 5;
        scale.vx -= 0x330;
        scale.vy -= 0x330;
        scale.vz -= 0x330;
    }
}

/* Show value as a number popup, coloured by the popup kind D_800D3630 (2
 * green, 3 magenta, 11 blue, else white), spinning one way at random. */
void func_800BE330(s32 value) {
    NumberPopup *popup;
    u8 text[8];
    s32 i;
    s32 x;

    popup = func_8001D1D8(sizeof(NumberPopup), 0, func_800BE11C, func_800BE1C4, 0);
    popup->spin = -(((rand() & 3) - 2) * 8);
    if (popup->spin == 0) {
        popup->spin = 6;
    }
    popup->life = 32;
    popup->colour.rgbc[3] = 0x2E;
    popup->scale.vx = 0x2000;
    popup->scale.vy = 0x2000;
    popup->scale.vz = 0x2000;
    popup->glyphCount = 0;
    popup->angle.vx = 0;
    popup->angle.vy = 0;
    popup->angle.vz = 0;
    switch (D_800D3630) {
    case 11:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 3:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 2:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0;
        break;
    default:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0x80;
        break;
    }
    func_800BE6E8(value, text, 5, 0, 0);
    x = D_800C3752[text[0]];
    popup->glyphCount = 0;
    for (i = 0; i != text[0]; x += 10) {
        popup->glyphCount += func_80026DCC(D_800D2F5C, text[i + 1] + 0x72, &popup->glyphs[popup->glyphCount], x, -8);
        i++;
    }
    for (i = 0; i != popup->glyphCount; i++) {
        popup->glyphs[i].w--;
        popup->glyphs[i].h--;
    }
}

/* Run up to three commands (kinds 0, 1 and 10) on slot's sprite outside the
 * battle menu and wait frames until they are done. */
void func_800BE538(s32 slot, s32 first, s32 second, s32 third) {
    BattleSprite *sprite;
    s32 mode;
    BattleMenu *menu;

    D_80059464 = 0;
    D_800591AC = 1;
    sprite = BATTLE_AREA.sprites[slot];
    D_800C3780 = 1;
    if (sprite != NULL) {
        mode = sprite->motion.bytes[3];
        func_800245D8(sprite, 10);
        menu = D_800C3610;
        D_800C3610 = (BattleMenu *)1;
        if (first) {
            func_800BD3AC(sprite, first, 0);
        }
        if (second) {
            func_800BD3AC(sprite, second, 1);
        }
        if (third) {
            func_800BD3AC(sprite, third, 10);
        }
        D_800C3610 = menu;
        while (func_800BF6F8()) {
            func_800BE790();
        }
        while (sprite->motion.bytes[3] == 10) {
            func_800BE790();
        }
        func_800245D8(sprite, mode);
    }
    D_800C3780 = 0;
    func_800BE0DC();
    D_80059464 = 0;
    D_800591AC = 0;
}

/* Write value as digits hexadecimal glyphs (D_800C3784, plus base) after
 * the count in text. */
void func_800BE6A0(s32 value, u8 *text, s32 digits, s32 base) {
    s32 i;
    s32 last;

    i = 0;
    if (digits != 0) {
        last = digits - 1;
        do {
            text[i + 1] = D_800C3784[(value >> ((last - i) * 4)) & 0xF] + base;
        } while (++i != digits);
    }
    text[0] = digits;
}

/* Write value in decimal after the count in text: a '-' for a negative
 * value, then its last digits + 1 digits (plus base), leading zeros only
 * when leading is set. */
void func_800BE6E8(s32 value, u8 *text, s32 digits, u8 leading, s32 base) {
    s32 count = 0;
    u8 *out = text + 1;
    u8 digit;

    if (value < 0) {
        count = 1;
        text[1] = '-';
        out = text + 2;
        value = -value;
        digits--;
    }
    while (value %= D_800C37A4[digits], digits != 0) {
        digits--;
        digit = value / D_800C37A4[digits];
        if (digit) {
            leading = 1;
        }
        if (leading) {
            *out++ = digit + base;
            count++;
        }
    }
    *out = value + base;
    text[0] = count + 1;
}

#ifdef NON_MATCHING
/* Run one battle frame: swap the display buffers, read the controllers,
 * update the sprites, the stage and the effects (the skipped frames once
 * more each) with the stack in the scratchpad, draw, time the frame and
 * present it; the outermost frame also runs the battle menu, a pending sound
 * request and the deferred free of the objects' extra files. Nonmatching:
 * the original rematerialises the frame's address at each use instead of
 * keeping it in a saved register. */
void func_800BE790(void) {
    BattleArea *frame = &BATTLE_AREA;
    FrameBuffer *buffer;
    s32 skipped;

    D_800C37D0++;
    func_80019CA0();
    D_800D309C.start = VSync(-1);
    buffer = &frame->buffers[0];
    if (frame->current == buffer) {
        buffer = &frame->buffers[1];
    }
    frame->current = buffer;
    frame->ot = buffer->ot;
    ClearOTagR(buffer->ot, 0x1000);
    frame->buffer = 1 - frame->buffer;
    if (D_80010000 != -1) {
        func_800BEBC4();
        __asm__ volatile(".word 0x0001000D"); /* break 1: the debugger breakpoint */
        func_80280A9C();
    }
    func_800250E0(frame->buffer);
    func_800BBAB8();
    func_800BB9D4();
    func_80024FF4(&D_800D309C.matrix);
    func_80024FE4(frame->ot);
    if (D_80010000 != -1) {
        func_80037324(frame->ot);
    }
    func_800A9A50(&D_800D309C.matrix, (s32)D_800CCB94, D_8005956C, frame->buffer);
    SPAD_STACK_ENTER();
    func_8001D468();
    func_8001C9F8();
    func_8001C964();
    for (skipped = D_80059494 - 1; skipped != -1; skipped--) {
        func_800BBAB8();
        func_8001C964();
    }
    SPAD_STACK_LEAVE();
    func_80076544();
    func_8008A9C0(0);
    while (--D_80059494 != -1) {
        func_8008A9C0(1);
    }
    D_800D309C.drawn = VSync(1);
    DrawSync(0);
    D_800D309C.synced = VSync(1);
    D_80059494 = VSync(-1) - D_800D309C.start - D_80059198;
    if (D_80059494 < 0) {
        D_80059494 = 0;
    }
    if (D_80059494 >= 5) {
        D_80059494 = 4;
    }
    frame->frameTicks = D_80059494 + D_80059198;
    VSync(D_80059198 != 0 ? D_80059198 + 1 : 0);
    PutDispEnv(&frame->current->dispEnv);
    PutDrawEnv(&frame->current->drawEnv);
    func_80025044();
    DrawOTag(&frame->current->ot[0xFFF]);
    func_800BEB04();
    if (D_800C37D0 == 1) {
        if (D_800C3610 != NULL) {
            D_800C3610->update(D_800C3610);
        }
        if (D_800591B4 != 0) {
            u16 sound = D_800591B4;

            D_800591B4 = 0;
            func_800B8068(sound);
        }
        if (D_800C37CC != 0 && func_800286CC() == 0) {
            D_800C37CC = 0;
            func_800B136C();
        }
    }
    D_800C37D0--;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BE790);
#endif

/* Load the requested battle module (D_800591B3) into 0x801FC000 when it
 * changed, around the module switch 800B8354, and mark it loaded. */
void func_800BEB04(void) {
    s32 saved0;
    s32 saved1;
    u8 module;

    if (D_800591B2 != (module = D_800591B3)) {
        D_800591B2 = D_800591B3;
        func_800B8354();
        func_800284B4(&saved0, &saved1);
        func_80028470(0xC, 2);
        func_800295D8(module + 2, 0x801FC000, 0, 0x80);
        func_800B8354();
        func_80028470(saved0, saved1);
        DrawSync(0);
        VSync(0);
        EnterCriticalSection();
        FlushCache();
        ExitCriticalSection();
    }
    D_800591B0 = 1;
}

/* Read the controllers; holding select (0x100) slows the frame down. */
void func_800BEBC4(void) {
    func_800BEC18();
    if (BATTLE_AREA.held & 0x100) {
        VSync(8);
        D_80059494 = 0;
    }
}

/* Read both controllers: held, newly pressed and released buttons, and a
 * history of the first controller's last changes. */
void func_800BEC18(void) {
    s32 held;
    u16 old;

    held = func_8003569C(0) & 0xFFFF;
    old = BATTLE_AREA.held;
    BATTLE_AREA.held = held;
    BATTLE_AREA.pressed = ~old & held;
    BATTLE_AREA.released = old & ~held;
    held = func_8003569C(1);
    BATTLE_AREA.pressed2 = ~BATTLE_AREA.held2 & held;
    BATTLE_AREA.held2 = held;
    BATTLE_AREA.heldOnly = BATTLE_AREA.held & ~held;
    if (BATTLE_AREA.held != BATTLE_AREA.history[0].held) {
        BATTLE_AREA.history[3] = BATTLE_AREA.history[2];
        BATTLE_AREA.history[2] = BATTLE_AREA.history[1];
        BATTLE_AREA.history[1] = BATTLE_AREA.history[0];
        BATTLE_AREA.history[0].held = BATTLE_AREA.held;
        BATTLE_AREA.history[0].pressed = BATTLE_AREA.pressed;
        BATTLE_AREA.history[0].released = BATTLE_AREA.released;
        BATTLE_AREA.history[0].time = D_800D30E4;
    }
}

/* Clear the battle menu state. */
void func_800BED30(void) {
    D_800C3E20 = 0;
    D_800C3610 = NULL;
    D_800D2E54 = 0;
}

/* Open the battle menu (800B9F78 its update). */
BattleMenu *func_800BED4C(void) {
    BattleMenu *menu = func_80031BDC(sizeof(BattleMenu), 0);

    D_800C3610 = menu;
    menu->update = func_800B9F78;
    D_800C360C = 0;
    menu->field4A = 0;
    D_800C3610->field30 = 0;
    D_800C3610->field48 = 1;
    D_800C3610->field2C = 1;
    D_800C3610->sprite = NULL;
    D_800C3610->field49 = 0;
    D_800C3610->field34 = 0;
    func_800BF0B4(0);
    D_80059464 = 0;
    D_800591AC = 1;
    return D_800C3610;
}

/* Close the battle menu. */
void func_800BEDE8(void) {
    func_800320E8(D_800C3610);
    D_80059464 = 0;
    D_800C3610 = NULL;
    D_800591AC = 0;
}

/* Run 800AA320 on a 4 KB stack of its own. */
void func_800BEE2C(s32 index, s32 mask, s32 arg2) {
    u8 *stack = func_80031BDC(0x1000, 1);

    STACK_ENTER(stack + 0xF9C);
    func_800AA320(index, mask, arg2);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* List the slot sprites of the slots in mask (up to 11, NULL-terminated),
 * setting their target; their count. */
s32 func_800BEEB4(u32 mask, BattleSprite **list, BattleSprite *target) {
    s32 i;
    s32 count;
    BattleSprite *sprite;

    i = 0;
    count = i;

    for (; i != 11; i++, mask = (mask & 0xFFFF) >> 1) {
        if (mask & 1) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL) {
                sprite->partner = target;
                list[count] = sprite;
                count++;
            }
        }
    }
    list[count] = NULL;
    return count;
}


/* The direction from sprite from to sprite to on the ground. */
s16 func_800BEF24(BattleSprite *from, BattleSprite *to) {
    GroundPoint a;
    GroundPoint b;

    a.x = from->x.fixed >> 16;
    a.z = from->z.fixed >> 16;
    b.x = to->x.fixed >> 16;
    b.z = to->z.fixed >> 16;
    return func_80023124(b, a);
}

/* The direction from sprite to its target point on the ground. */
s16 func_800BEF8C(BattleSprite *sprite) {
    GroundPoint a;
    GroundPoint b;

    a.x = sprite->x.fixed >> 16;
    a.z = sprite->z.fixed >> 16;
    b.x = sprite->target[0];
    b.z = sprite->target[2];
    return func_80023124(b, a);
}

/* Make slot the acting slot, returning the previous acting sprite to idle. */
void func_800BEFF4(s32 slot) {
    BattleSprite *sprite = D_800C3610->sprite;

    if (sprite != NULL && D_800C3610->slot != slot && !BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
        func_800245D8(sprite, sprite->idle.mode);
    }
    D_800C3610->slot = slot;
    D_800C3610->sprite = BATTLE_AREA.sprites[slot];
}

/* Set the battle menu's state. */
void func_800BF0B4(s32 state) {
    D_800C3610->state = state;
}

/* Walk sprite to the next point of the path, or at its end, to its target. */
void func_800BF0C4(BattleSprite *sprite) {
    if (BATTLE_AREA.path[D_800C3610->field2C].x == 0xFFFF && BATTLE_AREA.path[D_800C3610->field2C].z == 0xFFFF) {
        sprite->target[1] = 0;
        sprite->target[0] = sprite->x.fixed >> 16;
        sprite->target[2] = sprite->z.fixed >> 16;
        func_800BF4F0(sprite, sprite->partner);
        return;
    }
    sprite->target[0] = BATTLE_AREA.path[D_800C3610->field2C].x;
    sprite->target[2] = BATTLE_AREA.path[D_800C3610->field2C].z;
    sprite->target[1] = 0;
    func_800BF1EC(sprite, BATTLE_AREA.path[D_800C3610->field2C].run ? 3 : 2);
    D_800C3610->field2C++;
}

/* Start sprite moving to its target point with motion mode. */
void func_800BF1EC(BattleSprite *sprite, s32 mode) {
    GroundPoint from;
    GroundPoint to;

    from.x = sprite->x.fixed >> 16;
    from.z = sprite->z.fixed >> 16;
    to.x = sprite->target[0];
    to.z = sprite->target[2];
    D_800C3610->field44 = func_800C07CC(from, to);
    func_80021FE0((s32 *)sprite, func_800BEF8C(sprite));
    func_800223B0((s32 *)sprite, func_800BEF8C(sprite));
    func_800245D8(sprite, mode);
    func_800BF0B4(6);
}

/* Load the file of sprite's resource for its slot's command. */
void func_800BF2B8(BattleSprite *sprite) {
    s32 file;
    void *block;

    func_800B8D7C();
    func_80028470(0x2C, 1);
    file = sprite->resource->file;
    block = func_80031BDC(func_800288EC(file), 1);
    func_800295D8(file, (s32)block, 0, 0x80);
    D_800C3618 = block;
    D_800C361C = SPRITE_SLOT(sprite);
}

/* Start the loaded command file once. */
s32 func_800BF354(void) {
    s32 result;

    if (D_800D3350 == 0) {
        result = (s32)func_800C0FAC(D_800C3618);
        D_800D3350 = 1;
    }
    return result;
}

/* Stop the started command file. */
void func_800BF3A4(void) {
    if (D_800D3350 != 0) {
        func_800C1140(D_800C3618);
        D_800D3350 = 0;
    }
}

/* Face sprite and the first target of the current event at each other. */
void func_800BF3E8(BattleSprite *sprite) {
    BattleSprite *first;
    s32 slot;
    BattleSprite *target;

    D_800D3634 = BATTLE_AREA.events[D_800C360C].targetMask;
    if ((D_800D3678 = func_800BEEB4(BATTLE_AREA.events[D_800C360C].targetMask, D_800D363C, sprite)) == 0) {
        D_800D363C[0] = sprite;
    }
    target = D_800D363C[0];
    sprite->partner = target;
    target->partner = sprite;
    first = D_800D363C[0];
    slot = SPRITE_SLOT(target);
    D_800C3610->target = first;
    D_800C3610->targetSlot = slot;
    func_800223B0((s32 *)sprite, func_800BEF24(sprite, sprite->partner));
    if (target->motion.bytes[3] != 0x15) {
        func_800223B0((s32 *)target, func_800BEF24(sprite->partner, sprite));
    }
}

/* At the path's end, step sprite beside target; else walk the path on. */
void func_800BF4F0(BattleSprite *sprite, BattleSprite *target) {
    s16 x;

    if (BATTLE_AREA.path[D_800C3610->field2C].x == 0xFFFF && BATTLE_AREA.path[D_800C3610->field2C].z == 0xFFFF) {
        sprite->x.fixed = sprite->target[0] << 16;
        sprite->z.fixed = sprite->target[2] << 16;
        x = target->x.fixed >> 16;
        sprite->target[0] = (s16)(sprite->x.fixed >> 16) >= x ? x + 0x50 : x - 0x50;
        sprite->target[2] = target->z.fixed >> 16;
        sprite->target[1] = 0;
        if (sprite->target[0] == (s16)(sprite->x.fixed >> 16) && sprite->target[2] == (s16)(sprite->z.fixed >> 16)) {
            func_800B9C00(sprite);
            return;
        }
        func_800BF1EC(sprite, 3);
        func_800BF0B4(2);
        return;
    }
    func_800BF0C4(sprite);
}

/* Count a finished sprite motion. */
void func_800BF5E8(void) {
    D_800C3CE8++;
}

/* Run command with sprite playing its motion, then wait for the motion's end. */
void func_800BF600(s32 command, BattleSprite *sprite) {
    if (sprite->field48 == 0) {
        func_800B7C34(command);
        return;
    }
    D_800C3CE8 = 0;
    if (sprite->motion.bytes[3] != 0) {
        func_80021BF8(sprite, func_800BF5E8);
        func_800245D8(sprite, sprite->motion.bytes[3]);
    }
    func_800B7C34(command);
    if (sprite->motion.bytes[3] != 0) {
        while (D_800C3CE8 == 0) {
            func_800BE790();
        }
        func_80021BF8(sprite, NULL);
    }
}

/* Update the battle menu when it is open. */
void func_800BF6CC(void) {
    if (D_800C3610 != NULL) {
        func_800BD2E4();
    }
}

/* D_80059464 less one while a number popup shows. */
s32 func_800BF6F8(void) {
    return D_80059464 - func_800BF720();
}

/* Whether a number popup shows. */
s32 func_800BF720(void) {
    return D_800D2D68 != 0;
}

/* Set D_800C3628. */
void func_800BF730(s32 value) {
    D_800C3628 = value;
}

/* Watch a sprite's value; on a rise or a fall under the threshold call back
 * and end. */
void func_800BF73C(EffectSprite *task) {
    SlotWatch *watch = (SlotWatch *)task;
    s32 last = watch->value;

    watch->value = func_800B57E4(watch->sprite);
    if (last < watch->value || watch->value < watch->threshold) {
        watch->callback(watch->sprite);
        watch->destroy(watch);
    }
}

/* Start watching sprite's value against threshold with callback. */
void func_800BF7C8(BattleSprite *sprite, s32 threshold, void (*callback)(BattleSprite *sprite)) {
    SlotWatch *watch = func_8001CD08(sprite->task, sizeof(SlotWatch) - 0x1C);

    func_8001CD6C((EffectSprite *)watch, func_800BF73C);
    watch->callback = callback;
    watch->sprite = sprite;
    watch->mode = sprite->motion.bytes[3];
    watch->value = func_800B57E4(sprite);
    watch->threshold = threshold;
    sprite->motion.word |= 0x20;
}

#ifdef NON_MATCHING
/* Make slot's sprite act on the sprite of slot target alone. Nonmatching:
 * the original keeps the store of D_800D3634 before the sprite's, and
 * allocates the registers otherwise. */
void func_800BF85C(s32 slot, s32 target) {
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];

    if (sprite != NULL) {
        D_800C3E1C = sprite;
        D_800D3634 = 1 << target;
        sprite->partner = BATTLE_AREA.sprites[target];
        D_800D363C[1] = NULL;
        D_800D363C[0] = BATTLE_AREA.sprites[target];
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BF85C);
#endif

/* Move sprite's target to the next of the event's targets. */
void func_800BF8CC(BattleSprite *sprite) {
    s32 i;

    for (i = 0; i != D_800D3678; i++) {
        if (D_800D363C[i] == sprite->partner) {
            break;
        }
    }
    if (i >= D_800D3678) {
        sprite->partner = D_800D363C[0];
    } else {
        sprite->partner = D_800D363C[i + 1];
    }
}

/* The index of sprite among the event's targets. */
s32 func_800BF954(BattleSprite *sprite) {
    s32 i;

    for (i = 0; i != D_800D3678; i++) {
        if (D_800D363C[i] == sprite) {
            break;
        }
    }
    return i;
}

/* Count an effect hit; at the second, signal event 11. */
void func_800BF998(void) {
    D_800D2D4C++;
    D_800D36BC++;
    if (D_800D2D4C == 2) {
        func_800A9FF0(0xB);
    }
}

/* Upload the images of file 1 of directory 0x2C once requested. */
void func_800BF9EC(void) {
    void *file;

    if (D_800C3621 != 0) {
        func_800B8354();
        func_80028470(0x2C, 0);
        file = func_80031BDC(func_800288EC(1), 0);
        func_800295D8(1, (s32)file, 0, 0x80);
        func_800B8354();
        func_8002DDE4(file, 0, 0, 0, 0, 0, 0);
        func_800BE790();
        func_800320E8(file);
        D_800C3621 = 0;
    }
}

/* Once requested, restart the party slots in gears (in mode 2 all but the
 * acting one) and wait for them to stop moving. */
void func_800BFA9C(void) {
    s32 i;
    s32 acting;

    if (D_800C362C != 0) {
        func_800B8D04();
        func_800BE790();
        func_800BE790();
        acting = SPRITE_SLOT(D_800C3E1C);
        for (i = 0; i != 3; i++) {
            if (BATTLE_AREA.slots[i].gear != 0 && (D_800C362C != 2 || i != acting) && BATTLE_AREA.slots[i].field2 < 0x11) {
                func_800BB760(i);
            }
        }
        while (D_800C35D8 != 0) {
            func_800BE790();
        }
        D_800C362C = 0;
    }
}

/* Load and transfer the sound bank of file 5 of directory 0x2C once. */
void func_800BFBA0(void) {
    u8 *file;

    if (D_800C3620 == 0) {
        func_800B8354();
        func_80028470(0x2C, 0);
        file = func_80031BDC(func_800288EC(5), 0);
        func_800295D8(5, (s32)file, 0, 0x80);
        func_800B8354();
        if (func_800383EC(*(u16 *)(file + 0x20)) == 0) {
            func_800C0F70();
            D_800C3A6C = func_80037FD8(file, 0);
            while (func_8003BDFC(0) != 0) {
                func_800BE790();
            }
            D_800C3620 = 1;
            D_800C3622 = 0;
        }
        func_800320E8(file);
    }
}

/* Find the resident effect sprites of sprite with motion mode (any with
 * action 2): action 0 returns the first, the others destroy them. */
BattleSprite *func_800BFC80(BattleSprite *sprite, s32 mode, s32 action) {
    ActorTask *owner = sprite->task;
    ActorTask *task;
    BattleSprite *child;

    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->owner == owner && (task->link & 0x1FFFFFFF) == (owner->id & 0x1FFFFFFF) && (task->link >> 29 & 1)) {
            child = task->data;
            if (child->base == D_8006BE10) {
                if (action != 2) {
                    if (child->motion.bytes[3] != mode) {
                        continue;
                    }
                    if (action == 0) {
                        return child;
                    }
                }
                child->task->destroy(child->task);
            }
        }
    }
    return NULL;
}

/* Destroy the resident effect sprites of sprite with motion mode. */
void func_800BFD88(BattleSprite *sprite, s32 mode) {
    func_800BFC80(sprite, mode, 1);
}

/* Give sprite a resident effect sprite playing motion mode, unless it has. */
void func_800BFDA8(BattleSprite *sprite, s32 mode) {
    u8 saved;
    BattleSprite *child;

    if (sprite->field48 != 0 && func_800BFC80(sprite, mode, 0) == NULL) {
        void *motion = (void *)(SPRITE_RESOURCE->motions[mode + 1] + (s32)SPRITE_RESOURCE->motions);
        saved = D_800591AC;
        D_800591AC = 0;
        child = func_80023B84(sprite, motion, D_8006BE10);
        child->motion.bytes[3] = mode;
        child->partner = sprite;
        D_800591AC = saved;
        child->idle.word |= 0x100;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800C0564);

/* The distance between two points. */
s32 func_800C06E4(VECTOR *a, VECTOR *b) {
    VECTOR d;

    d.vx = a->vx - b->vx;
    d.vy = a->vy - b->vy;
    d.vz = a->vz - b->vz;
    func_8004A414(&d, &d);
    return SquareRoot0(d.vy + d.vz + d.vx);
}

/* The distance between two short points. */
s32 func_800C0758(SVECTOR *a, SVECTOR *b) {
    VECTOR d;

    d.vx = a->vx - b->vx;
    d.vy = a->vy - b->vy;
    d.vz = a->vz - b->vz;
    func_8004A414(&d, &d);
    return SquareRoot0(d.vy + d.vz + d.vx);
}

/* The distance between two points on the ground. */
s32 func_800C07CC(GroundPoint a, GroundPoint b) {
    VECTOR d;

    d.vx = a.x - b.x;
    d.vz = a.z - b.z;
    func_8004A414(&d, &d);
    return SquareRoot0(d.vx + d.vz);
}

/* The direction angles from point to to point from (no roll). */
void func_800C0828(SVECTOR *from, SVECTOR *to, SVECTOR *angles) {
    VECTOR unused[2];
    VECTOR d;
    VECTOR squares;
    s32 ground;

    d.vx = from->vx - to->vx;
    d.vy = from->vy - to->vy;
    d.vz = from->vz - to->vz;
    func_8004A414(&d, &squares);
    ground = SquareRoot0(squares.vx + squares.vz);
    angles->vy = ratan2(d.vz, d.vx);
    angles->vz = ratan2(d.vy, ground);
    angles->vx = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800C08CC);

/* The average of the four points weighted by the weights of cell
 * (row, column). */
void func_800C0D18(s32 row, s32 column, SVECTOR *points, VECTOR *out) {
    VECTOR v;
    s32 cell = row * 8 + column;

    gte_lddp(D_800C3A68[cell][0]);
    v.vx = points[0].vx;
    v.vy = points[0].vy;
    v.vz = points[0].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(out);

    gte_lddp(D_800C3A68[cell][1]);
    v.vx = points[1].vx;
    v.vy = points[1].vy;
    v.vz = points[1].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(&v);
    out->vx += v.vx;
    out->vy += v.vy;
    out->vz += v.vz;

    gte_lddp(D_800C3A68[cell][2]);
    v.vx = points[2].vx;
    v.vy = points[2].vy;
    v.vz = points[2].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(&v);
    out->vx += v.vx;
    out->vy += v.vy;
    out->vz += v.vz;

    gte_lddp(D_800C3A68[cell][3]);
    v.vx = points[3].vx;
    v.vy = points[3].vy;
    v.vz = points[3].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(&v);
    out->vx += v.vx;
    out->vy += v.vy;
    out->vz += v.vz;

    out->vx >>= 2;
    out->vy >>= 2;
    out->vz >>= 2;
}

/* Release the transferred sound bank. */
void func_800C0F70(void) {
    if (D_800C3A6C != 0) {
        func_80038310(D_800C3A6C);
    }
    D_800C3A6C = 0;
}

#ifdef NON_MATCHING
/* Set up a command file's parts: transfer its wave bank (freeing the file
 * from it when last), link its sound bank and upload its images; its sound
 * bank. Nonmatching: the original keeps D_800C3A6C's address in a saved
 * register (and reloads the debugger word's) where this is the reverse. */
SoundSystem *func_800C0FAC(s32 *file) {
    s32 *offsets = file;
    s32 *entry;
    s32 n;
    SoundSystem *bank = NULL;
    VramPoint image;
    VramPoint clut;

    for (n = *offsets - 3, offsets += 4; n > 0; n--, offsets++) {
        entry = (s32 *)(*offsets + (s32)file);
        switch (*entry) {
        case 0x73646573: /* "seds" */
            bank = (SoundSystem *)entry;
            func_80038428(bank);
            break;
        case 0x20736477: /* "wds " */
            func_800C0F70();
            D_800C3620 = 0;
            D_800C3622 = 0;
            D_800C3A6C = func_80037FD8(entry, 0);
            while (func_8003BDFC(0) != 0) {
                if (D_80010000 != -1) {
                    __asm__ volatile(".word 0x0001000D"); /* break 1 */
                }
            }
            if (n == 1) {
                func_80031F70(file, *offsets);
            }
            break;
        default:
            image.x = 0x380;
            image.y = 0x100;
            clut.x = 0;
            clut.y = 0x1F4;
            func_80022224(D_8005A474, entry, image, clut, 0);
            break;
        }
    }
    return bank;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800C0FAC);
#endif

/* Free the sound bank of a command file. */
void func_800C1140(s32 *file) {
    s32 *offsets = file;
    s32 *entry;
    s32 n;

    for (n = *offsets - 3, offsets += 4; n > 0; n--, offsets++) {
        entry = (s32 *)(*offsets + (s32)file);
        if (*entry == 0x73646573) { /* "seds" */
            func_8003852C(entry);
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B8098", func_800C11CC);
