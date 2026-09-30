/* Resident sprite/actor engine (0x8001c8dc-0x8002709c): task lists,
 * sprite motion and animation scripts. A -G8 unit whose positive `li`
 * are assembled as `addiu` (ASPSX 2.50+). */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "mode.h"
#include "menu.h"
#include "sprite.h"
#include "cd.h"
#include "stream.h"
#include "model.h"
#include "heap.h"
#include "text.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001C8DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001C944);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001C964);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001C9F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CA58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CAF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CB48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CBE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CC18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CD08);

/* Set a task's update callback. */
void func_8001CD64(Task *task, void (*update)(Task *)) {
    task->update = update;
}

/* Set a task's update callback (second entry). */
void func_8001CD6C(Task *task, void (*update)(Task *)) {
    task->update = update;
}

/* Set a task's destroy callback. */
void func_8001CD74(Task *task, void (*destroy)(Task *)) {
    task->destroy = destroy;
}

/* A task's update callback. */
void *func_8001CD7C(Task *task) {
    return task->update;
}

/* A task's destroy callback. */
void *func_8001CD88(Task *task) {
    return task->destroy;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CD94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CE44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CE74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D034);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D0A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D10C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D164);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D19C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D1D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D2A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D2B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D3F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D4E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001DAE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E148);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E2F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E368);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E3D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E9BC);

/* Whether a frame table entry takes its image from the sequencer (second byte bit 7). */
u8 func_8001EE68(u8 *frame) {
    return frame[1] >> 7;
}

/* The part count of a frame header (bits 9-14). */
u16 func_8001EE74(u16 *header) {
    return (*header >> 9) & 0x3F;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001EE88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F1D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F6B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F8E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FAB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FB30);

/* The operand a script byte names: a frame table entry (bit 7 set) or a byte on the sprite's stack. */
/* Nonmatching under GCC 2.7.2 and 2.6.3: register choice for the stack index and operand byte. */
#ifdef NON_MATCHING
u8 *func_8001FBA4(Sprite *sprite, u8 *code) {
    u8 *operand;

    if (*code & 0x80) {
        operand = sprite->frames + (*code & 0x7F);
    } else {
        operand = &sprite->stack[(s8)*code + sprite->stack_top];
    }
    return operand;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FBA4);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FBE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021AD8);

void func_80021B04(SVECTOR *vector, s16 x, s16 y, s16 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

void func_80021B14(VECTOR *vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

void func_80021B24(SVECTOR *to, SVECTOR *from) {
    to->vx = from->vx;
    to->vy = from->vy;
    to->vz = from->vz;
}

void func_80021B48(VECTOR *to, VECTOR *from) {
    to->vx = from->vx;
    to->vy = from->vy;
    to->vz = from->vz;
}

/* Drop a sprite's part colour. */
void func_80021B6C(Sprite *sprite) {
    sprite->colour_flags |= 1;
    func_8001F6B0(sprite);
}

/* Colour a sprite's one-sided parts. */
void func_80021B98(Sprite *sprite, u8 red, u8 green, u8 blue) {
    sprite->red = red;
    sprite->green = green;
    sprite->blue = blue;
    sprite->colour_flags &= ~1;
    func_8001F6B0(sprite);
}

/* Set a sprite's gravity divisor. */
void func_80021BCC(Sprite *sprite, s32 divisor) {
    sprite->motion.bits.divisor = divisor;
}

void func_80021BF0(Sprite *sprite, s32 resource) {
    sprite->resource = resource;
}

/* Set a sprite's completion callback. */
void func_80021BF8(Sprite *sprite, void *callback) {
    sprite->callback = callback;
}

/* Set a sprite's facing group (flags bits 8-12). */
void func_80021C00(Sprite *sprite, s32 group) {
    sprite->flags = (sprite->flags & ~0x1F00) | ((group & 0x1F) << 8);
}

/* Pop a byte from a sprite's stack. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
u8 func_80021C20(Sprite *sprite) {
    u8 value = sprite->stack[sprite->stack_top];

    sprite->stack_top += 1;
    return value;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021C20);
#endif

/* Pop a halfword from a sprite's stack. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
s16 func_80021C3C(Sprite *sprite) {
    s16 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8);

    sprite->stack_top += 2;
    return value;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021C3C);
#endif

/* Pop three bytes from a sprite's stack. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
s32 func_80021C6C(Sprite *sprite) {
    s32 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8) +
                (sprite->stack[sprite->stack_top + 2] << 16);

    sprite->stack_top += 3;
    return value;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021C6C);
#endif

/* Push a byte onto a sprite's stack. */
void func_80021CA0(Sprite *sprite, u8 value) {
    sprite->stack[--sprite->stack_top] = value;
}

/* Push a halfword onto a sprite's stack. */
void func_80021CC4(Sprite *sprite, u16 value) {
    sprite->stack_top -= 2;
    sprite->stack[sprite->stack_top] = value;
    sprite->stack[sprite->stack_top + 1] = value >> 8;
}

/* Push three bytes onto a sprite's stack. */
void func_80021CF8(Sprite *sprite, s32 value) {
    sprite->stack_top -= 3;
    sprite->stack[sprite->stack_top] = value;
    sprite->stack[sprite->stack_top + 1] = value >> 8;
    sprite->stack[sprite->stack_top + 2] = value >> 16;
}

/* Set a position's x and z from whole units (16.16). */
void func_80021D3C(VECTOR *position, s32 x, s32 z) {
    position->vz = z << 16;
    position->vx = x << 16;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021D50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021EBC);

void func_80021FB8(Sprite *sprite, u8 value) {
    sprite->byteb0 = value;
}

/* Set a sprite's walking speed (its velocity follows). */
void func_80021FC0(Sprite *sprite, s32 speed) {
    sprite->speed = speed;
    func_80022974(sprite);
}

/* Turn a sprite (its speed follows the direction). */
void func_80021FE0(Sprite *sprite, s16 direction) {
    sprite->direction = direction;
    func_80022974(sprite);
}

/* Set a sprite's uniform scale (and its renderer's), marking the orientation dirty. */
void func_80022000(Sprite *sprite, s16 scale) {
    SpriteRenderer *renderer = sprite->renderer;

    if (renderer != NULL) {
        renderer->scale_x = renderer->scale_y = renderer->scale_z = sprite->scale = scale;
        sprite->render.bits.dirty = 1;
    }
}

/* Rebuild a sprite's orientation if it is marked dirty. */
void func_80022038(Sprite *sprite) {
    if ((sprite->render.word >> 28) & 1) {
        func_80022090(sprite);
        sprite->render.bits.dirty = 0;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022090);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022224);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800222BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800223B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022660);

/* Derive a sprite's horizontal velocity from its walking speed, gravity divisor and direction. */
void func_80022974(Sprite *sprite) {
    s32 speed = ((sprite->speed >> 4) << 8) / sprite->motion.bits.divisor;

    sprite->speed_x = ((func_8003F8CC(sprite->direction) >> 2) * speed) >> 6;
    sprite->speed_z = -((func_8003F8B0(sprite->direction) >> 2) * speed) >> 6;
}

s32 func_80022A00(s32 *word) {
    return *word;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022A0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022A70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022B2C);

/* Scale a value by the sprite's speed factor (1024 = 1) when it has one. */
s32 func_80022CAC(Sprite *sprite, s32 value) {
    s32 result = value;
    u16 rate = sprite->rate;

    if (rate != 0) {
        result *= rate;
        result /= 1024;
    }
    return result;
}

/* Move a sprite horizontally by its speed (scaled by its speed factor), then 80022b2c. */
void func_80022CDC(Sprite *sprite) {
    sprite->x += func_80022CAC(sprite, sprite->speed_x >> 4) << 4;
    sprite->z += func_80022CAC(sprite, sprite->speed_z >> 4) << 4;
    func_80022B2C(sprite);
}

/* Show the frame the frame index selects, mirrored when its flip bit and the sprite's mirror flag differ. */
void func_80022D44(Sprite *sprite) {
    s32 index = sprite->frame_bits.frame;
    u16 entry;
    s32 frame;

    if (index < 0) {
        index = 0;
    }
    entry = sprite->frame_table[index];
    frame = entry & 0x1FF;
    if (entry & 0x200) {
        sprite->motion.bits.frame_flip = 1;
    } else {
        sprite->motion.bits.frame_flip = 0;
    }
    sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
    func_8001D2B0(sprite, frame);
}

/* Sprite task update: advance the animation and move (twice with double_step); destroy the task when the frames run out. */
void func_80022DF4(Task *task) {
    Sprite *sprite = task->data;

    func_80023210(sprite);
    func_80022CDC(sprite);
    if (sprite->frames_left != 0) {
        if (!((sprite->motion.word >> 6) & 1)) {
            return;
        }
        func_80023210(sprite);
        func_80022CDC(sprite);
        if (sprite->frames_left != 0) {
            return;
        }
    }
    task->destroy(task);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022E8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022EB8);

/* Replace a sprite renderer's part list with room for `count` parts. */
void func_80022FC4(Sprite *sprite, s32 count, s32 from_top) {
    if (sprite->renderer->parts != NULL) {
        func_800320E8(sprite->renderer->parts);
    }
    sprite->renderer->part_cursor = sprite->renderer->parts = func_80031BDC(count * 24, from_top);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8002303C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800230A8);

/* The direction (0-0xfff) from one ground point to another. */
s32 func_80023124(DVECTOR from, DVECTOR to) {
    VECTOR delta;

    delta.vx = from.vx - to.vx;
    delta.vz = from.vy - to.vy;
    return -ratan2(delta.vz, delta.vx) & 0xFFF;
}

/* Show a frame with the given mirroring, restarting the frame countdown and step. */
/* Nonmatching: same operations, different register allocation and scheduling of the two bitfield updates. */
#ifdef NON_MATCHING
void func_80023170(Sprite *sprite, s32 frame, s32 flip, s32 flip_y) {
    sprite->countdown = 0;
    sprite->render.bits.flip_y = flip_y;
    sprite->render.bits.flip = flip;
    sprite->frame_bits.phase = 0;
    sprite->frame_bits.step = 0;
    func_8001D2B0(sprite, frame);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023170);
#endif

/* The first word of the section a block's fourth word locates. */
s32 func_800231E0(s32 *block) {
    return *(s32 *)(block[3] + (s32)block);
}

/* One more than the second word of that section. */
s32 func_800231F8(s32 *block) {
    return ((s32 *)(block[3] + (s32)block))[1] + 1;
}

/* Count down the frame timer once per displayed frame, running the next command when it expires. */
/* Nonmatching: the original reads the countdown twice (lh to test, lhu to decrement), which GCC 2.7.2/2.6.3 do not reproduce here. */
#ifdef NON_MATCHING
void func_80023210(Sprite *sprite) {
    s32 i;

    for (i = 0; i != D_80059198.skip + 1; i++) {
        if (sprite->countdown != 0) {
            if (--sprite->countdown == 0) {
                func_800248D4(sprite);
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023210);
#endif

/* Set a sprite's blend rate; type 8 and 9 sprites keep it one lower, the others recolour their parts. */
void func_80023290(Sprite *sprite, s32 rate) {
    s32 type;
    s32 blend;

    rate &= 7;
    sprite->render.bits.blend = rate;
    if (rate != 0) {
        sprite->colour_flags |= 2;
    } else {
        sprite->colour_flags &= ~2;
    }
    type = (sprite->flags >> 13) & 0xF;
    if (type == 8 || type == 9) {
        blend = (sprite->render.word >> 5) & 7;
        if (blend != 0) {
            sprite->render.bits.blend = blend - 1;
        }
    } else {
        func_8001F6B0(sprite);
    }
}

/* Replace a sprite renderer's part list with room for `count` parts (from the heap bottom). */
void func_80023340(Sprite *sprite, s32 count) {
    func_800320E8(sprite->renderer->parts);
    sprite->renderer->parts = sprite->renderer->part_cursor = func_80031BDC(count * 24, 0);
}

/* Create a sprite task (with `extra` bytes after the sprite) under `owner`: its auxiliary node, default sprite and update/destroy callbacks. */
/* Nonmatching: the original keeps the auxiliary node pointer in its own register (s2); GCC folds its offset. */
#ifdef NON_MATCHING
SpriteTask *func_800233A4(Task *owner, s32 extra) {
    SpriteTask *node = func_80031BDC(extra + sizeof(SpriteTask), D_800591AF[0]);
    Task *auxiliary;
    Sprite *sprite;

    func_8001CC18(owner, &node->task);
    auxiliary = &node->auxiliary;
    func_8001CA58(&node->task, auxiliary);
    sprite = &node->sprite;
    func_80023804(sprite);
    node->task.data = sprite;
    auxiliary->data = sprite;
    func_8001CD6C(&node->task, func_80022DF4);
    func_8001CD74(&node->task, func_80022EB8);
    return node;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800233A4);
#endif

/* A frame entry's image index: bits 8-10, plus 8 when bit 14 is set. */
s32 func_80023440(u16 *entry) {
    s32 index = (*entry >> 8) & 7;

    if ((*entry >> 14) & 1) {
        index += 8;
    }
    return index;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800234AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023538);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023804);

/* Clear a renderer's angles and part list. */
void func_8002393C(SpriteRenderer *renderer) {
    renderer->angle_x = 0;
    renderer->angle_y = 0;
    renderer->angle_z = 0;
    renderer->parts = NULL;
}

void func_80023950(Sprite *sprite) {
    sprite->renderer = NULL;
}

/* Give a sprite the renderer stored right after it. */
void func_80023958(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->word40 = 0;
}

/* Give a sprite its inline renderer, sequencer and image storage (after the sprite). */
void func_800239A0(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->sequencer = (u8 *)sprite + 0xF4;
    sprite->renderer->pointer34 = (u8 *)sprite + 0x124;
    sprite->image = (u8 *)sprite + 0x110;
    sprite->renderer->pointer38 = NULL;
}

/* Give a sprite its inline renderer with an inline part list. */
void func_800239F4(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->renderer->part_cursor = (u8 *)sprite + 0xF4;
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->pointer38 = NULL;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023A48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023B84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023FD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024294);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800242F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8002435C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024524);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800245D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800248D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024F20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024F64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024FB8);

void func_80024FE4(s32 value) {
    D_8005956C[0] = value;
}

/* Copy the current light settings. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
void func_80024FF4(SpriteLight *light) {
    D_8004FBB8 = *light;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024FF4);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025044);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800250E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800251C8);

/* Set a task's update callback from the table at 8004fd40. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
void func_80025224(Task *task, s32 kind) {
    func_8001CD64(task, D_8004FD40[kind]);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025224);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025258);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8002541C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025544);

void func_80025710(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025718);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800257F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025A88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025C04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025D4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025FA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80026338);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800263E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8002675C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80026A0C);

void func_80026B9C(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80026BA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80026DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80026F44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80026FE8);
