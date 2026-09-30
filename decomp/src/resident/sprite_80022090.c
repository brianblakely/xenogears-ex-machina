/* Resident sprite engine, second unit (0x80022090-0x8002709c): sprite
 * orientation, resources, construction, frame commands and the image
 * queues. Built like the first unit (sprite.c). */
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

/* The unit's own small globals ($gp-relative; the assembler knows them as
 * this unit's small commons). */
s32 D_800591B8;
s32 D_800592EC;
RECT *D_800592F0;             /* LoadImage area for 80022a0c */
u_long *D_800592F4;           /* LoadImage pixels for 80022a0c */
s32 D_800592F8;               /* the queue being filled (0 or 1) */
s32 D_800592FC;               /* bytes of the queue entry block / 2 */
SpriteQueueEntry *D_80059300[2]; /* the two queues */
u8 *D_800594B4;               /* the queue entry block */
s32 D_800594C4;
SpriteQueueEntry *D_80059580; /* the next free queue entry */

/* Rebuild a sprite renderer's matrix: rotation by its angles, scaled by
 * its scales (flag bit 0: scale before rotating), then halved by the
 * sprite's speed factor when it has one. */
void func_80022090(Sprite *sprite) {
    VECTOR unused; /* the frame reserves 16 bytes no code uses */
    VECTOR scale;
    VECTOR size;
    MATRIX rotation;

    if (!(sprite->flags & 1)) {
        scale.vx = sprite->renderer->scale_x;
        scale.vy = sprite->renderer->scale_y;
        scale.vz = sprite->renderer->scale_z;
        func_8003F738((SVECTOR *)sprite->renderer, &sprite->renderer->matrix);
        ScaleMatrixL(&sprite->renderer->matrix, &scale);
    } else {
        MATRIX scaling = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};

        size.vx = sprite->renderer->scale_x;
        size.vy = sprite->renderer->scale_y;
        size.vz = sprite->renderer->scale_z;
        ScaleMatrixL(&scaling, &size);
        func_8003F738((SVECTOR *)sprite->renderer, &rotation);
        MulMatrix0(&rotation, &scaling, &sprite->renderer->matrix);
    }
    if (sprite->rate != 0) {
        scale.vx = sprite->rate >> 1;
        scale.vy = sprite->rate >> 1;
        scale.vz = sprite->rate >> 1;
        ScaleMatrix(&sprite->renderer->matrix, &scale);
    }
}

/* Resolve a resource block's section offsets into `resource`; with flag
 * 800591ad set, a nonzero field of the first section's first halfword (bits
 * 6-11) goes to 800591b3. */
/* Nonmatching: the original copies the origin a word at a time through one register (the destination copied to $t0); this build's block move uses two. The mode argument is unused. */
#ifdef NON_MATCHING
void func_80022224(SpriteResource *resource, s32 *data, SVECTOR origin, s32 mode) {
    s32 value;

    resource->origin = origin;
    resource->section3 = (u8 *)(data[3] + (s32)data);
    resource->section2 = (u8 *)(data[2] + (s32)data);
    D_800591B0 = 0;
    resource->section1 = (u16 *)(data[1] + (s32)data);
    if (D_800591AD != 0) {
        value = (*resource->section1 >> 6) & 0x3F;
        if (value != 0) {
            D_800591B3 = value;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80022224);
#endif

/* Bind a sprite's image to resource block `data` (unless it already is: its
 * sections through 80022224, render bit 30 set); with 800591ad set, the image
 * size comes from the sequencer for sequencer frames, else 768 x 256. */
void func_800222BC(Sprite *sprite, s32 *data) {
    SpriteResource *resource = sprite->image;
    s32 unused[2]; /* the frame reserves 8 bytes no code uses */

    if (data == NULL) {
        return;
    }
    if (data != sprite->resource_block) {
        func_80022224(resource, data, resource->origin, (sprite->render.word >> 20) & 0xF);
        sprite->resource_block = data;
        sprite->render.word |= 0x40000000;
    }
    if (D_800591AD != 0) {
        if (!func_8001EE68(resource->section2)) {
            ((SpriteImage *)resource)->size.height = 0x100;
            ((SpriteImage *)resource)->size.width = 0x300;
        } else {
            ((SpriteImage *)resource)->size = ((SpriteSequencer *)sprite->sequencer)->size;
        }
    }
}

/* Face a sprite at `angle`: with an animation, choose the frame table of
 * the angle's facing group (one, four or eight groups; the others mirror
 * one), and when the group changes replay the animation's commands from its
 * start up to the current command (80022660), keeping the countdown. */
/* Nonmatching: the original keeps the angle unextended in $a3 (extending it only for the shifts) and orders the facing-group tests differently. */
#ifdef NON_MATCHING
void func_800223B0(Sprite *sprite, s16 angle) {
    s32 old = sprite->frame_bits.step;
    s16 countdown;
    s32 flip;

    sprite->word80 = angle;
    if ((angle + 0x400) & 1) {
        sprite->motion.word |= 4;
    } else {
        sprite->motion.word &= ~4;
    }
    if (sprite->animations == NULL) {
        return;
    }
    switch (sprite->frame_bits.phase) {
    case 0:
        if (((angle + 0x400) & 0xFFF) > 0x800) {
            sprite->motion.word |= 4;
        } else {
            sprite->motion.word &= ~4;
        }
        sprite->frame_bits.step = 0;
        sprite->facings = sprite->animation + 3;
        sprite->frame_table = (u16 *)(sprite->animation[2] + 4 + (s32)sprite->animation);
        break;
    case 1:
        angle = ((angle + 0x600) >> 10) & 3;
        if (angle >= 3) {
            sprite->motion.word |= 4;
            sprite->frame_table = (u16 *)(sprite->animation[3] + 6 + (s32)sprite->animation);
        } else {
            sprite->motion.word &= ~4;
            sprite->frame_table = (u16 *)(*(u16 *)((u8 *)sprite->animation + angle * 2 + 4) + ((u8 *)sprite->animation + (angle * 2 + 4)));
        }
        sprite->frame_bits.step = angle;
        break;
    case 2:
        angle = ((angle + 0x500) >> 9) & 7;
        if (angle >= 5) {
            angle = (angle - 5) ^ 3;
            sprite->motion.word |= 4;
            sprite->frame_table = (u16 *)(*(u16 *)((u8 *)sprite->animation + angle * 2 + 4) + ((u8 *)sprite->animation + (angle * 2 + 4)));
        } else {
            sprite->motion.word &= ~4;
            sprite->frame_table = (u16 *)(*(u16 *)((u8 *)sprite->animation + angle * 2 + 4) + ((u8 *)sprite->animation + (angle * 2 + 4)));
        }
        sprite->frame_bits.step = angle;
        break;
    }
    if (old != sprite->frame_bits.step) {
        u8 *target = sprite->script;
        s32 count = sprite->frame_bits.field22;

        countdown = sprite->countdown;
        sprite->frame_bits.frame = 0x3F;
        sprite->frame_bits.field22 = 0;
        sprite->script = (u8 *)(sprite->animation[1] + 2 + (s32)sprite->animation);
        func_80022660(sprite, target, count);
        sprite->countdown = countdown;
    }
    sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_800223B0);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80022660);

/* Derive a sprite's horizontal velocity from its walking speed, gravity divisor and direction. */
void func_80022974(Sprite *sprite) {
    s32 speed = ((sprite->speed >> 4) << 8) / sprite->motion.bits.divisor;

    sprite->speed_x = ((func_8003F8CC(sprite->direction) >> 2) * speed) >> 6;
    sprite->speed_z = -((func_8003F8B0(sprite->direction) >> 2) * speed) >> 6;
}

s32 func_80022A00(s32 *word) {
    return *word;
}

/* Upload the image at D_800592F4 to D_800592F0, running LoadImage on an 8 KB
 * heap block as its stack. */
void func_80022A0C(void) {
    u8 *stack = func_80031BDC(0x2000, 1);

    STACK_ENTER(stack + 0x1F00);
    LoadImage(D_800592F0, D_800592F4);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* Upload a list of images side by side from (x, y), 64 columns apart: the
 * list is a count followed by offsets (from the list) of images that start
 * with their width and height. */
void func_80022A70(s32 *list, s32 x, s16 y) {
    RECT rect;
    s32 *offsets;
    s32 count;
    s32 column;
    u16 *image;

    offsets = list;
    count = *offsets++;
    column = 0;
    rect.y = y;
    while (count-- > 0) {
        image = (u16 *)(*offsets++ + (s32)list);
        rect.w = *image++;
        rect.h = *image++;
        rect.x = x + column;
        column += 0x40;
        D_800592F0 = &rect;
        D_800592F4 = (u_long *)image;
        func_80022A0C();
    }
}

/* Move a sprite vertically by its speed (scaled by its speed factor) under
 * its gravity (word 0x1c): unless render bit 26 is set, the battle stage
 * floor (800ba8f4) stops a falling sprite, which rebounds by its bounce
 * factor and settles once the rebound is below the gravity. */
/* Nonmatching: the rebound division reuses the product's register and the floor store follows mflo in the original; GCC copies the product first. */
#ifdef NON_MATCHING
void func_80022B2C(Sprite *sprite) {
    s32 speed;
    s32 gravity;

    if (!((sprite->render.word >> 26) & 1)) {
        func_800BA8F4(sprite);
        if (sprite->speed_y > 0 && sprite->word1c > 0) {
            if (WHOLE(sprite->y) == sprite->ground) {
                return;
            }
            sprite->y += func_80022CAC(sprite, sprite->speed_y >> 4) << 4;
            if ((sprite->y >> 16) >= sprite->ground) {
                speed = -sprite->speed_y * sprite->frame_bits.bounce;
                sprite->y = sprite->ground << 16;
                sprite->speed_y = speed = speed / 256;
                gravity = sprite->word1c;
                if (speed < 0) {
                    speed = -speed;
                }
                if (gravity < 0) {
                    gravity = -gravity;
                }
                if (speed < gravity) {
                    sprite->speed_y = 0;
                }
                return;
            }
        } else {
            sprite->y += func_80022CAC(sprite, sprite->speed_y >> 4) << 4;
            if ((sprite->y >> 16) >= sprite->ground) {
                sprite->y = sprite->ground << 16;
            }
        }
        sprite->speed_y += sprite->word1c;
    } else {
        sprite->y += func_80022CAC(sprite, sprite->speed_y >> 4) << 4;
        sprite->speed_y += sprite->word1c;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80022B2C);
#endif

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
    if (sprite->script != NULL) {
        if (!((sprite->motion.word >> 6) & 1)) {
            return;
        }
        func_80023210(sprite);
        func_80022CDC(sprite);
        if (sprite->script != NULL) {
            return;
        }
    }
    task->destroy(task);
}

/* Count a sprite task update, then run it. */
void func_80022E8C(Task *task) {
    D_800592EC++;
    func_80022DF4(task);
}

/* Sprite task destroy callback: queue the part list, release the renderer
 * block of a one-sided sprite, destroy the tasks it created (motion bit 5)
 * and clear its flag-29 children's word (bit 11 at 0xb0), leave the pending
 * list, unlink both nodes and free the task. */
void func_80022EB8(Task *task) {
    Sprite *sprite = task->data;
    SpriteTask *node = (SpriteTask *)task;

    if (sprite->renderer != NULL && sprite->renderer->parts[0] != NULL) {
        func_80025180((u32)sprite->renderer->parts[0]);
    }
    if ((sprite->render.word & 3) == 1 && sprite->renderer->pointer34 != NULL) {
        func_800320E8(sprite->renderer->pointer34);
    }
    if ((sprite->motion.word >> 5) & 1) {
        func_8001CE74(&node->task);
    }
    if ((sprite->b0.wordb0 >> 11) & 1) {
        func_8001D034(&node->task);
    }
    if ((sprite->render.word & 3) == 1) {
        func_8001D3F4(sprite);
    }
    func_8001CD94(&node->task);
    func_8001CB48(&node->auxiliary);
    func_800320E8(task);
}

/* Replace a sprite renderer's part list with room for `count` parts. */
void func_80022FC4(Sprite *sprite, s32 count, s32 from_top) {
    if (sprite->renderer->parts[0] != NULL) {
        func_800320E8(sprite->renderer->parts[0]);
    }
    sprite->renderer->parts[1] = sprite->renderer->parts[0] = func_80031BDC(count * 24, from_top);
}

/* Allocate a sprite sequencer's buffer of `count` words and store the image's
 * two header halfwords in its first entry. */
void func_8002303C(Sprite *sprite, s32 count, s32 mode) {
    ((SpriteSequencer *)sprite->sequencer)->buffer = func_80031BDC(count * 4, mode);
    ((SpriteSequencer *)sprite->sequencer)->buffer[1] = ((SpriteImage *)sprite->image)->size.height;
    ((SpriteSequencer *)sprite->sequencer)->buffer[0] = ((SpriteImage *)sprite->image)->size.width;
}

/* Destroy a sprite: release its sequencer buffer (frame bit 0), leave the
 * pending list and release its part list and itself. */
void func_800230A8(Sprite *sprite) {
    if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->buffer != NULL) {
        func_800320E8(((SpriteSequencer *)sprite->sequencer)->buffer);
    }
    func_8001D3F4(sprite);
    func_800320E8(sprite->renderer->parts[0]);
    func_800320E8(sprite);
}

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
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80023170);
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

    for (i = 0; i != D_80059198 + 1; i++) {
        if (sprite->countdown != 0) {
            if (--sprite->countdown == 0) {
                func_800248D4(sprite);
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80023210);
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
    func_800320E8(sprite->renderer->parts[0]);
    sprite->renderer->parts[0] = sprite->renderer->parts[1] = func_80031BDC(count * 24, 0);
}

/* Create a sprite task (with `extra` bytes after the sprite) under `owner`: its auxiliary node, default sprite and update/destroy callbacks. */
SpriteTask *func_800233A4(Task *owner, s32 extra) {
    SpriteTask *node = func_80031BDC(extra + sizeof(SpriteTask), D_800591AF);
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

/* A frame entry's image index: bits 8-10, plus 8 when bit 14 is set. */
s32 func_80023440(u16 *entry) {
    s32 index = (*entry >> 8) & 7;

    if ((*entry >> 14) & 1) {
        index += 8;
    }
    return index;
}

/* A property (0, 1 or 2) of kind `kind`; `fallback` for kind 3 and the rest. */
s32 func_80023468(s32 kind, s32 fallback) {
    switch (kind) {
    case 0:
    case 5:
    case 6:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        fallback = 1;
        break;
    case 1:
    case 4:
    case 8:
    case 9:
        fallback = 0;
        break;
    case 2:
    case 7:
    case 15:
        fallback = 2;
        break;
    }
    return fallback;
}

/* Clear the eight entries of a sprite renderer's 0x40-byte block. */
void func_800234AC(Sprite *sprite) {
    s32 i;

    for (i = 0; i != 8; i++) {
        sprite->renderer->pointer34[i].byte0 = 0;
        sprite->renderer->pointer34[i].byte1 = 0;
        sprite->renderer->pointer34[i].half2 = 0;
        sprite->renderer->pointer34[i].half4 = 0;
        sprite->renderer->pointer34[i].half6 = 0;
    }
}

/* Apply an animation header: its command script and frame table, facing
 * groups (bits 0-1), gravity from its signed weight (bits 2-7), the frame
 * skip, the sprite's weight and gravity divisor; unless kept (bits 11, 12,
 * 13) clear the speeds, reset the angles and rebuild the orientation, and
 * rescale; then restart the command state. */
void func_80023538(Sprite *sprite, u16 *animation) {
    s32 weight;
    s32 skip;
    s32 step;
    s32 offset;
    SpriteRenderer *renderer;

    sprite->animation = animation;
    offset = animation[1] + 2;
    sprite->script = (u8 *)(offset + (s32)animation);
    sprite->frame_bits.phase = animation[0] & 3;
    offset = animation[2] + 4;
    sprite->frame_table = (u16 *)(offset + (s32)animation);
    weight = (animation[0] >> 2) & 0x3F;
    if (weight & 0x20) {
        weight |= ~0x3F;
    }
    skip = D_80059198 + 1;
    sprite->word1c = weight << 10;
    sprite->word1c *= skip * skip * (s16)sprite->word82 / 4096;
    step = 0x10000 / sprite->motion.bits.divisor;
    sprite->word1c *= step * step / 256;
    sprite->word1c /= 256;
    if (!((animation[0] >> 11) & 1)) {
        sprite->speed_z = 0;
        sprite->speed_y = 0;
        sprite->speed_x = 0;
        sprite->speed = 0;
    }
    renderer = sprite->renderer;
    if (renderer != NULL) {
        if (!((animation[0] >> 12) & 1)) {
            renderer->angle_z = 0;
            renderer->angle_x = 0;
            renderer->angle_y = 0;
            func_80022090(sprite);
        }
        if (!((animation[0] >> 13) & 1)) {
            if (D_800591AD == 0) {
                goto one_sided;
            }
            func_80022000(sprite, D_800591A8);
        }
        if (D_800591AD != 0) {
            func_80022090(sprite);
        }
    one_sided:
        if ((sprite->render.word & 3) == 1) {
            sprite->renderer->offset_x = sprite->renderer->offset_y = 0;
            if (!((sprite->flags >> 20) & 1) && sprite->renderer->pointer34 != NULL) {
                func_800234AC(sprite);
            }
        }
    }
    sprite->stack_top = 0x10;
    sprite->frame_bits.bounce = 0;
    sprite->countdown = 1;
    sprite->frame_bits.field22 = 0;
    sprite->frame_bits.field28 = 2;
    sprite->frame_bits.frame = 0x3F;
    sprite->half30 = 0;
    if (sprite->sequencer != NULL && sprite->frame_bits.sequencer_owned == 1) {
        ((SpriteSequencer *)sprite->sequencer)->word0 = ((SpriteSequencer *)sprite->sequencer)->word4 = 0;
        ((SpriteSequencer *)sprite->sequencer)->halfc = 0;
    }
}

/* Reset a sprite's state to the defaults: no flags, frame or animation,
 * blend 0x2d, gravity divisor 256, gravity from the frame skip and the
 * sprite's weight (word 0x82), an empty byte stack. */
void func_80023804(Sprite *sprite) {
    s32 gravity;
    u32 *words;

    sprite->render.word = 0;
    sprite->colour_flags = 0x2D;
    sprite->flags = 0;
    sprite->rate = 0;
    sprite->half30 = 0;
    sprite->direction = 0;
    sprite->frame = 0;
    *(u32 *)&sprite->frame_bits = 0;
    sprite->render.bits.unknown2 = 0;
    sprite->render.bits.flip = 0;
    sprite->render.bits.flip_y = 0;
    sprite->render.bits.mode = 0;
    sprite->render.bits.no_view = 0;
    sprite->render.bits.field16 = 0;
    sprite->flags &= ~0xFC;
    sprite->flags &= ~0x1F00;
    sprite->flags &= ~0x1E000;
    words = (u32 *)&sprite->frame_bits; /* the motion and 0xb0 words follow */
    words[1] = 0;
    words[2] = 0;
    sprite->b0.byteb0 = 0;
    sprite->motion.bytes[3] = 0;
    gravity = D_80059198 + 1;
    sprite->frame_bits.step = 0;
    sprite->motion.bits.divisor = 0x100;
    sprite->word1c = gravity * (gravity << 14) * (s16)sprite->word82 / 4096;
    sprite->script = NULL;
    sprite->word70 = 0;
    sprite->resource_block = NULL;
    sprite->callback = NULL;
    sprite->word80 = 0;
    sprite->stack_top = 0x10;
    sprite->ground = 0;
    sprite->block = NULL;
    sprite->word50 = 0;
}

/* Clear a renderer's angles and part list. */
void func_8002393C(SpriteRenderer *renderer) {
    renderer->angle_x = 0;
    renderer->angle_y = 0;
    renderer->angle_z = 0;
    renderer->parts[0] = NULL;
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
    sprite->renderer->pointer34 = (SpriteRendererEntry *)((u8 *)sprite + 0x124);
    sprite->image = (u8 *)sprite + 0x110;
    sprite->renderer->next_pending = NULL;
}

/* Give a sprite its inline renderer with an inline part list. */
void func_800239F4(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->renderer->parts[1] = (SpritePart *)((u8 *)sprite + 0xF4);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->next_pending = NULL;
}

/* Create a sprite task under `owner` with `extra` bytes and its storage
 * (mode 0: no renderer, 1: inline renderer and part list sized for the
 * image's first frame (kinds 5 and 6 use the 8006be10 and 8005a474 images),
 * 2: inline renderer); records the allocation, its size and the image. */
SpriteTask *func_80023A48(s32 kind, s32 mode, SpriteSource *source, s32 extra, Task *owner) {
    SpriteTask *task;
    s32 size;
    Sprite *sprite;

    switch (mode) {
    case 0:
        size = 0;
        task = func_800233A4(owner, extra);
        func_80023950(&task->sprite);
        break;
    case 1:
        if (kind == 5) {
            source = (SpriteSource *)D_8006BE10;
        }
        if (kind == 6) {
            source = (SpriteSource *)D_8005A474;
        }
        size = (func_8001EE74(source->frames) - 1) * 24 + 0x58;
        task = func_800233A4(owner, size + extra);
        func_800239F4(&task->sprite);
        break;
    case 2:
        size = 0x54;
        task = func_800233A4(owner, extra + 0x54);
        func_80023958(&task->sprite);
        break;
    }
    sprite = &task->sprite;
    sprite->block = task;
    sprite->size = size + 0xEC;
    sprite->image = source;
    return task;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80023B84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80023FD8);

/* 80024524 with `extra` in 800591b8 for the call. */
void func_80024294(void *a0, s16 a1, s16 a2, s16 a3, s16 a4, s16 a5, s32 extra) {
    D_800591B8 = extra;
    func_80024524(a0, a1, a2, a3, a4, a5);
    D_800591B8 = 0;
}

/* 8002435c with `extra` in 800591b8 for the call. */
void func_800242F4(void *a0, s32 a1, s16 a2, s16 a3, s16 a4, s16 a5, s16 a6, s32 extra) {
    D_800591B8 = extra;
    func_8002435C(a0, a1, a2, a3, a4, a5, a6);
    D_800591B8 = 0;
}

/* Construct a sprite from resource block `data`: defaults, inline storage,
 * unit scale, one-sided rendering, the sequencer (reset, or owned when
 * 800591ad is clear), render modes from 800591b8, a part list for its
 * first frame, the image origin (width, height, x, y), the resource binding
 * and animation 0. */
/* Nonmatching: the prologue copies the data argument before the sprite argument in the original; GCC copies them in order. */
#ifdef NON_MATCHING
Sprite *func_8002435C(Sprite *sprite, s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused) {
    s32 *block = data;

    func_80023804(sprite);
    func_800239A0(sprite);
    func_80022000(sprite, 0x1000);
    sprite->flags &= ~0x1E000;
    sprite->render.bits.sides = 1;
    if (D_800591AD != 0) {
        sprite->frame_bits.sequencer_owned = 0;
        ((SpriteSequencer *)sprite->sequencer)->word8 = 0;
        ((SpriteSequencer *)sprite->sequencer)->halfc = 0;
    } else {
        sprite->frame_bits.sequencer_owned = 1;
        ((SpriteSequencer *)sprite->sequencer)->buffer = NULL;
    }
    sprite->block = sprite;
    sprite->render.bits.mode = D_800591B8;
    sprite->render.bits.field16 = D_800591B8;
    sprite->renderer->parts[1] = sprite->renderer->parts[0] = func_80031BDC(func_8001EE74((u16 *)(block[2] + (s32)block)) * 24, 0);
    ((SpriteResource *)sprite->image)->origin.vx = width;
    ((SpriteResource *)sprite->image)->origin.vy = height;
    ((SpriteResource *)sprite->image)->origin.vz = x;
    ((SpriteResource *)sprite->image)->origin.pad = y;
    sprite->animations = data;
    func_800222BC(sprite, data);
    sprite->word60 = (u16 *)((s32)((SpriteResource *)sprite->image)->section1 + ((*((SpriteResource *)sprite->image)->section1 & 0x3F) + 1) * 2);
    func_800245D8(sprite, 0);
    return sprite;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_8002435C);
#endif

/* Allocate a sprite (356 bytes, its inline storage included) and construct
 * it from resource block `data` (8002435c). */
Sprite *func_80024524(s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused) {
    Sprite *sprite = func_80031BDC(0x164, 0);

    sprite->size = 0x164;
    return func_8002435C(sprite, data, x, y, width, height, unused);
}

/* Select animation `animation` (negative: ~animation from the alternate
 * resource): bind the resource (image size from the sequencer or 768 x 256
 * with 800591ad set), store the animation number, apply the animation's
 * header (80023538) and face the current angle again. */
void func_800245D8(Sprite *sprite, s32 animation) {
    u16 *directory;
    u16 *table;

    if (sprite->animations == NULL) {
        sprite->script = NULL;
        return;
    }
    if (sprite->resource_block == sprite->animations) {
        sprite->b0.wordb0 &= ~0x400;
    } else {
        sprite->b0.wordb0 |= 0x400;
    }
    if (animation < 0) {
        func_800222BC(sprite, (s32 *)sprite->resource);
        if (D_800591AD != 0 && !func_8001EE68(((SpriteResource *)sprite->image)->section2)) {
            ((SpriteImage *)sprite->image)->size.height = 0x100;
            ((SpriteImage *)sprite->image)->size.width = 0x300;
        }
    } else {
        func_800222BC(sprite, sprite->animations);
        if (D_800591AD != 0) {
            ((SpriteImage *)sprite->image)->size = ((SpriteSequencer *)sprite->sequencer)->size;
        }
    }
    sprite->motion.bytes[3] = animation;
    if (animation < 0) {
        animation = ~animation;
    }
    directory = ((SpriteResource *)sprite->image)->section1;
    table = (u16 *)(directory[animation + 1] + (s32)directory);
    sprite->flags |= 0x100000;
    sprite->animation = table;
    func_80023538(sprite, table);
    func_800223B0(sprite, sprite->word80);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80024730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_800248D4);

/* Reset the sprite engine: flags, the value at 800591a8, the task lists and
 * the pending-frame list. */
void func_80024F20(void) {
    D_800591AD = 0;
    D_800591AE = 0;
    D_800591A8 = 0x2000;
    func_8001C944();
    func_8001D298();
}

/* Allocate the queue entry block (`size` * 2 bytes) and empty both queues. */
void func_80024F64(s32 size, s32 mode) {
    D_800592FC = size;
    D_800594B4 = func_80031BDC(size * 2, mode);
    D_800594B8 = D_800594B4 + size;
    D_80059300[1] = NULL;
    D_80059300[0] = NULL;
    D_800594C4 = 0;
    func_8001D298();
}

/* Release the queue entry block. */
void func_80024FB8(void) {
    func_800320E8(D_800594B4);
    func_8001D2A4();
}

void func_80024FE4(s32 value) {
    D_8005956C = value;
}

/* Copy the current light settings. */
void func_80024FF4(MATRIX *view) {
    D_8004FBB8 = *view;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025044);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_800250E0);

/* Queue `value` on the queue being filled. */
/* Nonmatching: the original takes the queue array's address with lui/addiu; GNU as makes that la $gp-relative (maspsx leaves la of small data to the assembler). */
#ifdef NON_MATCHING
void func_80025180(u32 value) {
    SpriteQueueEntry *entry = D_80059580;

    D_80059580 = entry + 1;
    if (entry != NULL) {
        entry->value = value;
        entry->next = D_80059300[D_800592F8];
        D_80059300[D_800592F8] = entry;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025180);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_800251C8);

/* Set a task's update callback from the table at 8004fd40. */
void func_80025224(Task *task, s32 kind) {
    func_8001CD64(task, D_8004FD40[kind]);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025258);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_8002541C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025544);

void func_80025710(void) {
}

/* Sprite task draw: rebuild the orientation if needed and, with a renderer
 * block, place the model at the sprite's position (in view space unless
 * render bit 24 is set) and draw it (8002c700) with the part list of the
 * queue being filled. */
void func_80025718(Task *task) {
    MATRIX matrix;
    VECTOR position;
    Sprite *sprite = task->data;

    func_80022038(sprite);
    if (sprite->renderer->pointer34 != NULL) {
        position.vx = WHOLE(sprite->x);
        position.vy = WHOLE(sprite->y);
        position.vz = WHOLE(sprite->z);
        TransMatrix(&sprite->renderer->matrix, &position);
        if (!sprite->render.bits.no_view) {
            CompMatrix(&D_8004FBB8, &sprite->renderer->matrix, &matrix);
        }
        SetRotMatrix(&matrix);
        SetTransMatrix(&matrix);
        func_8002C700(sprite->renderer->pointer34, sprite->renderer->parts[D_800592F8], D_8005956C, ((u16 *)&sprite->flags)[1] & 4); /* flags bit 18, as a halfword */
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_800257F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025A88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025C04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025D4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80025FA8);

/* The texture of sheet entry `id`: its first word, mode, CLUT position and
 * the VRAM position of its pixels (page plus column and row). */
void func_80026338(u16 *sheet, s32 id, s32 *first, s32 *mode, s32 *clut_x, s32 *clut_y, s32 *x, s32 *y) {
    s16 *entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    SheetPart *part = (SheetPart *)(entry + 2);
    s32 u;
    s32 column;

    *first = entry[0];
    u = part->u << 16;
    if (part->mode != 0) {
        column = u >> 18;
    } else {
        column = u >> 20;
    }
    *mode = part->mode;
    *clut_x = part->clut_x;
    *clut_y = part->clut_y;
    *x = (s16)(part->page_x & 0xFFC0) + column;
    *y = (s16)(part->page_y & 0xFF00) + part->v;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_800263E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_8002675C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80026A0C);

void func_80026B9C(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80026BA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80026DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80026F44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_80022090", func_80026FE8);
