/* Battle unit from 800B15D8 to the end of the overlay text, built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (docs/matching.md): stores to
 * globals take a register for %hi, positive `li` becomes `addiu`, and some
 * epilogues (800B8090, 800B88BC, 800BEF84, 800BEFEC, 800BF718) carry the
 * stack adjustment in the `jr $ra` delay slot. The unit starts at 800B15D8,
 * the first function whose global stores take a register for %hi (800B14CC's
 * take $at); its rodata starts at 0x800707DC, after 800B12D0's jump table. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "effect.h"
#include "objects.h"
#include "actor.h"

/* Select script index of an effect script file: copy its entry into
 * D_800C3BD0 (relocating its offsets to addresses unless the file is already
 * relocated) and start its commands; its command count. */
s32 func_800B15D8(ScriptFile *file, s32 index) {
    ScriptEntry *entry = &file->entries[index];

    D_800C3BD0 = *entry;
    if (!(file->flags & 1)) {
        D_800C3BD0.data0 += (u32)entry;
        D_800C3BD0.data8 += (u32)entry;
        D_800C3BD0.commands += (u32)entry;
    }
    D_800C3BF0 = 0;
    D_800C3BEC = D_800C3BD0.commands;
    return entry->count;
}

/* Address of entry index (0x1C bytes each) of a table with a 0xC-byte
 * header. */
u8 *func_800B168C(u8 *table, s32 index) {
    return table + (index * 0x1C + 0xC);
}

/* The total size of an unrelocated script entry's commands (each command's
 * first byte + 1 words). */
s32 func_800B16A4(ScriptEntry *entry) {
    s32 i = 0;
    s32 size = 0;
    u8 *command = entry->commands + (u32)entry;
    s32 count = entry->count;

    while (i != count) {
        i++;
        size += (command[0] + 1) * 4;
        command += (command[1] + 1) * 4;
    }
    return size;
}

/* Step the effect script cursor to the next command (its second byte + 1
 * words further). */
void func_800B16F0(void) {
    D_800C3BEC += (D_800C3BEC[1] + 1) * 4;
    D_800C3BF0++;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1EA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1F6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B2AEC);

void func_800B3348(void) {
}

void func_800B3350(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3358);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B35C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3658);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B36BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B383C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3878);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B397C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B39C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3B6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3B94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3C2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3C74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3CD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3E04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3F04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B4EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B4F88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B50D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B51B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B56E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B572C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B57E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5924);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B59BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5C18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5CC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5DC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5DF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6004);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B61B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B61F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B626C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B62C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B639C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B63F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6464);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B64D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B65B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6808);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6930);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B69E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6B98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6BFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6C98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6CEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6DC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6E84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B73A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B73EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7424);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7C28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7E94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8048);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8054);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8068);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B81BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B838C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B853C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8774);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B88C4);

void func_800B89F4(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B89FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8D04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8D7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8DA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8EBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9020);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B905C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9B30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9B54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9C00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9F78);

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
    sprite->speedX = (func_8003F8CC(direction) >> 1) * speed >> 8;
    sprite->speedZ = -((func_8003F8B0(sprite->direction) >> 1) * speed) >> 8;
}

/* Aim sprite's jump at its target: turn it towards the target and set the
 * rising speed that lands it on the ground there (or the target's height
 * when that is higher). */
void func_800BA614(BattleSprite *sprite) {
    Vector delta;
    SVector point;
    Vector out;
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
    height = ((point.vy << 16) - sprite->y) >> 16;
    delta.vx = sprite->target[0] - (sprite->x >> 16);
    delta.vz = sprite->target[2] - (sprite->z >> 16);
    angle = -ratan2(delta.vz, delta.vx);
    func_8004A414(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz);
    sprite->speedY = -sprite->gravity * distance * 16 / (sprite->speed >> 11) + sprite->speed * height / distance;
    func_800BA59C(sprite, angle);
}

/* Aim sprite's jump at its target keeping its rising speed: snap it to
 * whole units, turn it towards the target and set the speed that covers the
 * distance (and the height difference) in the jump's frames. */
void func_800BA768(BattleSprite *sprite) {
    Vector delta;
    SVector point;
    Vector out;
    s32 frames;
    s32 triangle;
    s32 angle;
    s32 distance;
    s32 height;

    frames = -(sprite->speedY * 2 / sprite->gravity);
    sprite->x &= 0xFFFF0000;
    sprite->y &= 0xFFFF0000;
    sprite->z &= 0xFFFF0000;
    delta.vx = sprite->target[0] - (sprite->x >> 16);
    delta.vz = sprite->target[2] - (sprite->z >> 16);
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
    height = (point.vy << 16) - sprite->y;
    if (frames != 0) {
        sprite->speedY += height / frames;
    }
    func_800BA59C(sprite, angle);
    func_80022B2C(sprite);
}

/* Put sprite on the scene's ground: its triangle and ground height. */
void func_800BA8F4(BattleSprite *sprite) {
    SVector point;
    Vector out;
    s32 triangle;

    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
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
    task->field20 = sprite;
    task->field1C = 0;
    func_800242F4(sprite, resource, a, b, c, d, e, g);
    sprite->task = task;
    sprite->x = x << 16;
    sprite->y = y << 16;
    sprite->z = z << 16;
    sprite->fieldB0 = animation;
    sprite->render |= 4;
    sprite->direction = direction;
    func_80022000(&sprite->x, 0x2000);
    sprite->field82 = 0x2000;
    sprite->triangle = 0;
    func_800245D8(sprite, animation);
    return task;
}

/* Draw a sprite task: its depth in the view, and its parts when visible. */
void func_800BAB0C(ActorTask *task) {
    SVector point;
    s32 result[2]; /* screen position, then the GTE flags */
    Vector unused;
    BattleSprite *sprite;
    s32 depth;

    if (D_800C3664 == 0) {
        sprite = task->data;
        point.vx = sprite->x >> 16;
        point.vy = sprite->y >> 16;
        point.vz = sprite->z >> 16;
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
    void *parts = sprite->renderer->parts;

    if (parts != NULL) {
        func_800320E8(parts);
    }
    func_8001CE74(task);
    func_8001D3F4(sprite);
    func_8001CB48(&task->field1C);
    func_8001CD94(task);
    func_800320E8(task);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BABDC);
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
    SVector point;
    s16 sxy[2];
    s32 p;
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];

    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
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
        func_800BFC80(task, 0, 2);
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
        func_800223B0(&sprite->x, direction);
        func_80021FE0(&sprite->x, direction);
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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BAF48);
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
    SVector unused; /* the original's frame has this unused local */
    BattleSprite *sprite = task->data;
    u32 low = sprite->frame.bits.slotLow;
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
        sprite->x = object->hierarchy->translation[0] << 16;
        sprite->y = object->hierarchy->translation[1] << 16;
        sprite->z = object->hierarchy->translation[2] << 16;
    }
}

/* Draw of a slot-following sprite: its size from the slot's object and its
 * depth in the view. */
void func_800BB248(ActorTask *task) {
    SVector point;
    s32 result[2]; /* screen position, then the GTE flags */
    BattleSprite *sprite = task->data;
    s32 depth;
    u32 low;

    low = sprite->frame.bits.slotLow;
    sprite->size = func_800AA600(sprite->motion.bits.slotHigh << 2 | low);
    sprite->halfSize = sprite->size / 2;
    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
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
    SVector unused; /* the original's frame has this unused local */

    func_8001CB48(&task->field1C);
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
        task->field20 = sprite;
        func_80023804(sprite);
        func_800239A0(sprite);
        sprite->flags.bits.group = 4;
        sprite->task = task;
        sprite->render &= ~3;
        sprite->frame.bits.sequencerOwned = 0;
        sprite->sequencer->field8 = 0;
        sprite->sequencer->fieldC = 0;
        sprite->field82 = D_800591A8;
        sprite->x = (u16)BATTLE_AREA.slots[slot].x << 16;
        sprite->z = (u16)BATTLE_AREA.slots[slot].z << 16;
        sprite->y = 0;
        sprite->size = func_800AA600(slot);
        sprite->field82 = 0x2000;
        sprite->halfSize = sprite->size >> 1;
        func_80022000(&sprite->x, 0x2000);
        sprite->resource = D_8006BE10;
        BATTLE_AREA.sprites[slot] = sprite;
        BATTLE_AREA.tasks[slot] = task;
        sprite->field4C = 0;
        sprite->field48 = 0;
        D_800591AC = saved;
        sprite->frame.bits.slotLow = slot;
        sprite->motion.bits.slotHigh = slot >> 2;
    }
}

/* Task step: take a free stage place (of three), create the gear object of
 * the task's slot there, its sprite (800BB350), and end the task; the last
 * one sets D_800C37CC. */
void func_800BB540(ActorTask *task) {
    s32 i;
    s32 bit;

    for (i = 0, bit = 1; i != 3; i++, bit <<= 1) {
        if (!(D_800C3666 & bit)) {
            D_800C3666 |= bit;
            break;
        }
    }
    func_800A979C(task->field1C, D_800C3668[i].x, D_800C3668[i].y, 0, task->field1C + 0x1C0);
    D_800C3CB8--;
    func_800BB350(task->field1C);
    task->destroy(task);
    if (--D_800C35D8 == 0) {
        D_800C37CC = 1;
    }
}

/* Task step: once the disc is idle, run 800BB540 on a separate stack. */
void func_800BB620(ActorTask *task) {
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
void func_800BB690(ActorTask *task) {
    func_800A9540(task->field1C);
    D_800C3CB8++;
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BB620);
}

/* Task step: once the disc and the file reads are idle, run 800BB690 on a
 * separate stack. */
void func_800BB6E0(ActorTask *task) {
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
    ActorTask *task;

    D_800591AC = 0;
    D_800591AF = 1;
    task = func_8001CD08(NULL, 4);
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BB6E0);
    task->field1C = slot;
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
void func_800BB844(Matrix *m, SVector *eye, SVector *target, SVector *up) {
    Vector v;
    Vector forward;
    Vector right;
    Vector upward;

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BBAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BBEE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC158);

/* Set the camera mode: 2 puts the eye and look-at sprites at the saved
 * points, 4 sets D_800C3CBC to 5, others release them. */
void func_800BC2F0(s32 mode) {
    SVector *point;

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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC404);
#endif

/* Set D_800C3740. */
void func_800BC454(s16 value) {
    D_800C3740 = value;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC460);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCAA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCAD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCAFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCBB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCC60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCD8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCEAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD024);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD1FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD2E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD3AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD7A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD810);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD974);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDB74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDC14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDC78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDCF8);

void func_800BDD34(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDD3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDE58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE11C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE1C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE538);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE6A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE6E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE790);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEB04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEC18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BED30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BED4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEDE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEE2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEEB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEF24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEF8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEFF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF0B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF0C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF1EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF2B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF3A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF3E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF4F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF5E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF6CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF6F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF730);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF73C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF7C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF85C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF8CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF954);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF9EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFA9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFBA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFC80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFD88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFDA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C06E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0758);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C07CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0828);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C08CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0D18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0F70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0FAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C1140);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C11CC);
