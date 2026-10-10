/* Resident sprite engine, second unit (0x80022090-0x800248d4): sprite
 * orientation, resources, construction and child sprites. Built like the
 * first unit (sprite.c). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/sprite.h"
#include "resident/task.h"
#include "battle_overlay.h"
#include "own_declarations.h"

/* Lengths in bytes of the frame script commands 0x80-0xff, including the
 * command byte. */
u8 sprite_vm_command_lengths[0x80] = { /* 8004FCC0 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
};

/* The unit's own small data and statics ($gp-relative; the statics take
 * the unit's .sbss, 800592ec). */
s32 sprite_palette_bank = 0; /* 800591B8 */
s16 sprite_unused_halfwords[4] = {0x400, -0x200, 0, 0}; /* 800591BC: nothing references it */
static s32 sprite_unread_kind7_update_count; /* 800592EC */
static RECT *sprite_load_image_rect;      /* 800592F0: LoadImage area for 80022a0c */
static u_long *sprite_load_image_pixels;    /* 800592F4: LoadImage pixels for 80022a0c */

/* 80022090: Rebuild a sprite renderer's matrix: rotation by its angles, scaled by
 * its scales (flag bit 0: scale before rotating), then halved by the
 * sprite's speed factor when it has one. */
void sprite_rebuild_orientation(Sprite *sprite) {
    VECTOR unused; /* the frame reserves 16 bytes no code uses */
    VECTOR scale;
    VECTOR size;
    MATRIX rotation;

    if (!(sprite->flags & 1)) {
        scale.vx = sprite->renderer->scale_x;
        scale.vy = sprite->renderer->scale_y;
        scale.vz = sprite->renderer->scale_z;
        gpu_build_rotation_matrix((SVECTOR *)sprite->renderer, &sprite->renderer->matrix);
        ScaleMatrixL(&sprite->renderer->matrix, &scale);
    } else {
        MATRIX scaling = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};

        size.vx = sprite->renderer->scale_x;
        size.vy = sprite->renderer->scale_y;
        size.vz = sprite->renderer->scale_z;
        ScaleMatrixL(&scaling, &size);
        gpu_build_rotation_matrix((SVECTOR *)sprite->renderer, &rotation);
        MulMatrix0(&rotation, &scaling, &sprite->renderer->matrix);
    }
    if (sprite->rate != 0) {
        scale.vx = sprite->rate >> 1;
        scale.vy = sprite->rate >> 1;
        scale.vz = sprite->rate >> 1;
        ScaleMatrix(&sprite->renderer->matrix, &scale);
    }
}

/* 80022224: Resolve a resource block's section offsets into `resource`; with flag
 * 800591ad set, a nonzero field of the first section's first halfword (bits
 * 6-11) goes to 800591b3. The mode argument is unused. */
void sprite_resolve_resource(SpriteResource *resource, s32 *data, SVECTOR origin, s32 mode) {
    s32 value;

    /* Preserve the two halfword-aligned coordinate words. */
    ((DVECTOR *)&resource->origin)[0] = ((DVECTOR *)&origin)[0];
    ((DVECTOR *)&resource->origin)[1] = ((DVECTOR *)&origin)[1];
    resource->section3 = (u8 *)(data[3] + (s32)data);
    resource->section2 = (u8 *)(data[2] + (s32)data);
    resource->section1 = (u16 *)(data[1] + (s32)data);
    sprite_battle_module_loaded = 0;
    if (sprite_in_battle != 0) {
        value = (*resource->section1 >> 6) & 0x3F;
        if (value != 0) {
            sprite_requested_battle_module = value;
        }
    }
}

/* 800222BC: Bind a sprite's image to resource block `data` (unless it already is: its
 * sections through 80022224, render bit 30 set); with 800591ad set, the image
 * size comes from the sequencer for sequencer frames, else 768 x 256. */
void sprite_bind_resource(Sprite *sprite, s32 *data) {
    SpriteResource *resource = sprite->image;
    s32 unused[2]; /* the frame reserves 8 bytes no code uses */

    if (data == NULL) {
        return;
    }
    if (data != sprite->resource_block) {
        sprite_resolve_resource(resource, data, resource->origin, (sprite->render.word >> 20) & 0xF);
        sprite->resource_block = data;
        sprite->render.word |= 0x40000000;
    }
    if (sprite_in_battle != 0) {
        if (!sprite_is_cell_directory(resource->section2)) {
            ((SpriteImage *)resource)->size.height = 0x100;
            ((SpriteImage *)resource)->size.width = 0x300;
        } else {
            ((SpriteImage *)resource)->size = ((SpriteSequencer *)sprite->sequencer)->size;
        }
    }
}

/* 800223B0: Face a sprite at `angle`: with an animation, choose the frame table of
 * the angle's facing group (one, four or eight groups; the others mirror
 * one), and when the group changes replay the animation's commands from its
 * start up to the current command (80022660), keeping the countdown. */
void sprite_set_facing(Sprite *sprite, s16 angle) {
    s32 old = sprite->frame_bits.step;
    s32 countdown;

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
        if ((s16)((angle + 0x400) & 0xFFF) > 0x800) {
            sprite->motion.word |= 4;
        } else {
            sprite->motion.word &= ~4;
        }
        sprite->frame_table = (u16 *)(sprite->animation[2] + (s32)&sprite->animation[2]);
        sprite->frame_bits.step = 0;
        sprite->facings = sprite->animation + 3;
        break;
    case 1:
        angle = ((angle + 0x600) >> 10) & 3;
        if (angle < 3) {
            sprite->frame_table = (u16 *)(sprite->animation[angle + 2] + (s32)&sprite->animation[angle + 2]);
            sprite->motion.word &= ~4;
        } else {
            sprite->frame_table = (u16 *)(sprite->animation[3] + (s32)&sprite->animation[3]);
            sprite->motion.word |= 4;
        }
        sprite->frame_bits.step = angle;
        break;
    case 2:
        angle = ((angle + 0x500) >> 9) & 7;
        if (angle < 5) {
            sprite->frame_table = (u16 *)(sprite->animation[angle + 2] + (s32)&sprite->animation[angle + 2]);
            sprite->motion.word &= ~4;
        } else {
            angle = (angle - 5) ^ 3;
            sprite->frame_table = (u16 *)(sprite->animation[angle + 2] + (s32)&sprite->animation[angle + 2]);
            sprite->motion.word |= 4;
        }
        sprite->frame_bits.step = angle;
        break;
    }
    if (old != sprite->frame_bits.step) {
        u8 *target;
        s32 count = sprite->frame_bits.field22;

        target = sprite->script;
        sprite->script = (u8 *)(sprite->animation[1] + (s32)&sprite->animation[1]);
        sprite->frame_bits.frame = 0x3F;
        sprite->frame_bits.field22 = 0;
        countdown = sprite->countdown;
        sprite_vm_replay_frames(sprite, target, count);
        sprite->countdown = countdown;
    }
    sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
}

/* 80022660: Replay a sprite's frame commands without timing until the script reaches
 * `target` with `count` commands run: frame commands step or look up the
 * frame and add their duration (commands 40-7f repeat the last one), b3
 * sets the frame index, be shows a frame with its flip and duration, e2
 * pushes its return point and jumps; 80-82 end, and 86, 87 and 97 end at
 * the target. Others are skipped by their length (sprite_vm_command_lengths). */
void sprite_vm_replay_frames(Sprite *sprite, u8 *target, s32 count) {
    u8 *script;
    u8 op;
    s32 duration;
    s32 value;
    s16 offset;
    u8 *args;

next:
    script = sprite->script;
    if (script == target && sprite->frame_bits.field22 == count) {
        return;
    }
    {
        op = *script;
        args = script + 1;
        /* 00-7f: the frames of 800248d4, unscaled; 40-7f add the last frame command's
         * duration again (unset before one ran). */
        if (op < 0x80) {
            sprite->script = args;
            if (op < 0x10) {
                sprite_request_frame(sprite, sprite->frame + 1);
                duration = (op & 0xF) + 1;
            } else if (op < 0x20) {
                sprite->frame_bits.frame++;
                sprite_show_indexed_frame(sprite);
                duration = (op & 0xF) + 1;
            } else if (op < 0x30) {
                sprite_request_frame(sprite, sprite->frame - 1);
                duration = (op & 0xF) + 1;
            }
            if (op < 0x40) {
                duration = (op & 0xF) + 1;
            }
            sprite->countdown += duration;
            if (++sprite->frame_bits.field22 == 0) {
                sprite->frame_bits.field22--;
            }
            goto next;
        }
        switch (op) {
        /* be: frame with flip and unscaled wait; advanced by the width table's 2. */
        case 0xBE:
            value = script[1] | (script[2] << 8);
            sprite->motion.bits.frame_flip = value >> 9;
            sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
            if (sprite->frame != (value & 0x1FF)) {
                sprite_request_frame(sprite, value & 0x1FF);
            }
            sprite->countdown += ((value >> 11) & 0xF) + 1;
            break;
        /* e2 s16: call (push the return point, jump). */
        case 0xE2:
            offset = script[1] + ((s8)args[1] << 8);
            sprite_stack_push_three_bytes(sprite, (s32)(script + 3));
            sprite->script += offset;
            goto next;
        /* b3 s8: frame index = s8. */
        case 0xB3:
            sprite->frame_bits.frame = (s8)script[1];
            break;
        /* 80-82: stop. */
        case 0x80:
        case 0x81:
        case 0x82:
            return;
        /* 86: stop at the target. */
        case 0x86:
            if (script == target) {
                return;
            }
            break;
        /* 87: stop at the target. */
        case 0x87:
            if (script == target) {
                return;
            }
            break;
        /* 97: stop at the target. */
        case 0x97:
            if (script == target) {
                return;
            }
            break;
        }
        sprite->script += sprite_vm_command_lengths[op - 0x80];
        goto next;
    }
}

/* 80022974: Derive a sprite's horizontal velocity from its walking speed, gravity divisor and direction. */
void sprite_update_velocity(Sprite *sprite) {
    s32 speed = ((sprite->speed >> 4) << 8) / sprite->motion.bits.divisor;

    sprite->speed_x = ((gpu_get_cos(sprite->direction) >> 2) * speed) >> 6;
    sprite->speed_z = -((gpu_get_sin(sprite->direction) >> 2) * speed) >> 6;
}

/* 80022A00: Read a word. */
s32 sprite_read_word(s32 *word) {
    return *word;
}

/* 80022A0C: Upload the image at sprite_load_image_pixels to sprite_load_image_rect, running LoadImage on an 8 KB
 * heap block as its stack. */
void sprite_load_image_on_heap_stack(void) {
    u8 *stack = heap_alloc(0x2000, 1);

    STACK_ENTER(stack + 0x1F00);
    LoadImage(sprite_load_image_rect, sprite_load_image_pixels);
    STACK_LEAVE();
    heap_free(stack);
}

/* 80022A70: Upload a list of images side by side from (x, y), 64 columns apart: the
 * list is a count followed by offsets (from the list) of images that start
 * with their width and height. */
void sprite_upload_images_side_by_side(s32 *list, s32 x, s16 y) {
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
        sprite_load_image_rect = &rect;
        sprite_load_image_pixels = (u_long *)image;
        sprite_load_image_on_heap_stack();
    }
}

/* 80022B2C: Move a sprite vertically by its speed (scaled by its speed factor) under
 * its gravity (word 0x1c): unless render bit 26 is set, the battle stage
 * floor (800ba8f4) stops a falling sprite, which rebounds by its bounce
 * factor and settles once the rebound is below the gravity. */
void sprite_move_vertically(Sprite *sprite) {
    s32 speed;
    s32 gravity;

    if (!((sprite->render.word >> 26) & 1)) {
        battle_sprite_update_ground(sprite);
        if (sprite->speed_y > 0 && sprite->gravity > 0) {
            if ((sprite->y >> 16) == sprite->ground) {
                return;
            }
            sprite->y += sprite_scale_by_rate(sprite, sprite->speed_y >> 4) << 4;
            if ((sprite->y >> 16) >= sprite->ground) {
                speed = -sprite->speed_y * sprite->frame_bits.bounce;
                sprite->y = sprite->ground << 16;
                /* Round the signed rebound toward zero before scaling. */
                if (speed < 0) {
                    speed += 0xFF;
                }
                speed >>= 8;
                sprite->speed_y = speed;
                gravity = sprite->gravity;
                if (abs(speed) < abs(gravity)) {
                    sprite->speed_y = 0;
                }
                return;
            }
        } else {
            sprite->y += sprite_scale_by_rate(sprite, sprite->speed_y >> 4) << 4;
            if ((sprite->y >> 16) >= sprite->ground) {
                sprite->y = sprite->ground << 16;
            }
        }
        sprite->speed_y += sprite->gravity;
    } else {
        sprite->y += sprite_scale_by_rate(sprite, sprite->speed_y >> 4) << 4;
        sprite->speed_y += sprite->gravity;
    }
}

/* 80022CAC: Scale a value by the sprite's speed factor (1024 = 1) when it has one. */
s32 sprite_scale_by_rate(Sprite *sprite, s32 value) {
    s32 result = value;
    u16 rate = sprite->rate;

    if (rate != 0) {
        result *= rate;
        result /= 1024;
    }
    return result;
}

/* 80022CDC: Move a sprite horizontally by its speed (scaled by its speed factor), then 80022b2c. */
void sprite_move(Sprite *sprite) {
    sprite->x += sprite_scale_by_rate(sprite, sprite->speed_x >> 4) << 4;
    sprite->z += sprite_scale_by_rate(sprite, sprite->speed_z >> 4) << 4;
    sprite_move_vertically(sprite);
}

/* 80022D44: Show the frame the frame index selects, mirrored when its flip bit and the sprite's mirror flag differ. */
void sprite_show_indexed_frame(Sprite *sprite) {
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
    sprite_request_frame(sprite, frame);
}

/* 80022DF4: Sprite task update: advance the animation and move (twice with double_step); destroy the task when the frames run out. */
void sprite_task_update(Task *task) {
    Sprite *sprite = task->data;

    sprite_vm_tick(sprite);
    sprite_move(sprite);
    if (sprite->script != NULL) {
        if (!((sprite->motion.word >> 6) & 1)) {
            return;
        }
        sprite_vm_tick(sprite);
        sprite_move(sprite);
        if (sprite->script != NULL) {
            return;
        }
    }
    task->destroy(task);
}

/* 80022E8C: Count a sprite task update, then run it. */
void sprite_task_update_counted(Task *task) {
    sprite_unread_kind7_update_count++;
    sprite_task_update(task);
}

/* 80022EB8: Sprite task destroy callback: queue the part list, release the renderer
 * block of a one-sided sprite, destroy the tasks it created (motion bit 5)
 * and clear its flag-29 children's word (bit 11 at 0xb0), leave the pending
 * list, unlink both nodes and free the task. */
void sprite_task_destroy(Task *task) {
    Sprite *sprite = task->data;
    SpriteTask *node = (SpriteTask *)task;

    if (sprite->renderer != NULL && sprite->renderer->parts[0] != NULL) {
        sprite_queue_free_later((u32)sprite->renderer->parts[0]);
    }
    if ((sprite->render.word & 3) == 1 && sprite->renderer->pointer34 != NULL) {
        heap_free(sprite->renderer->pointer34);
    }
    if ((sprite->motion.word >> 5) & 1) {
        task_destroy_owned_by(&node->task);
    }
    if ((sprite->b0.wordb0 >> 11) & 1) {
        task_clear_child_sprite_parents(&node->task);
    }
    if ((sprite->render.word & 3) == 1) {
        sprite_remove_pending(sprite);
    }
    task_unlink_main_node(&node->task);
    task_unlink_draw_node(&node->auxiliary);
    heap_free(task);
}

/* 80022FC4: Replace a sprite renderer's part list with room for `count` parts. */
void sprite_realloc_parts(Sprite *sprite, s32 count, s32 from_top) {
    if (sprite->renderer->parts[0] != NULL) {
        heap_free(sprite->renderer->parts[0]);
    }
    sprite->renderer->parts[1] = sprite->renderer->parts[0] = heap_alloc(count * 24, from_top);
}

/* 8002303C: Allocate a sprite sequencer's buffer of `count` words and store the image's
 * two header halfwords in its first entry. */
void sprite_alloc_sequencer_buffer(Sprite *sprite, s32 count, s32 mode) {
    ((SpriteSequencer *)sprite->sequencer)->buffer = heap_alloc(count * 4, mode);
    ((SpriteSequencer *)sprite->sequencer)->buffer[1] = ((SpriteImage *)sprite->image)->size.height;
    ((SpriteSequencer *)sprite->sequencer)->buffer[0] = ((SpriteImage *)sprite->image)->size.width;
}

/* 800230A8: Destroy a sprite: release its sequencer buffer (frame bit 0), leave the
 * pending list and release its part list and itself. */
void sprite_destroy(Sprite *sprite) {
    if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->buffer != NULL) {
        heap_free(((SpriteSequencer *)sprite->sequencer)->buffer);
    }
    sprite_remove_pending(sprite);
    heap_free(sprite->renderer->parts[0]);
    heap_free(sprite);
}

/* 80023124: The direction (0-0xfff) from one ground point to another. */
s32 sprite_get_ground_direction(DVECTOR from, DVECTOR to) {
    VECTOR delta;

    delta.vx = from.vx - to.vx;
    delta.vz = from.vy - to.vy;
    return -ratan2(delta.vz, delta.vx) & 0xFFF;
}

/* 80023170: Show a frame with the given mirroring, restarting the frame countdown and step. */
void sprite_show_frame_flipped(Sprite *sprite, s32 frame, s32 flip, s32 flip_y) {
    sprite->countdown = 0;
    sprite->frame_bits.phase = 0;
    sprite->frame_bits.step = 0;
    sprite->render.bits.flip_y = flip_y;
    sprite->render.bits.flip = flip;
    sprite_request_frame(sprite, frame);
}

/* 800231E0: The first word of the section a block's fourth word locates. */
s32 sprite_read_palette_word0(s32 *block) {
    return *(s32 *)(block[3] + (s32)block);
}

/* 800231F8: One more than the second word of that section. */
s32 sprite_read_palette_word1_plus_one(s32 *block) {
    return ((s32 *)(block[3] + (s32)block))[1] + 1;
}

/* 80023210: Count down the frame timer once per displayed frame, running the next command when it expires. */
void sprite_vm_tick(Sprite *sprite) {
    s32 i;

    for (i = 0; i != sprite_frame_skip + 1; i++) {
        if (sprite->countdown != 0) {
            if (--sprite->countdown == 0) {
                sprite_vm_run(sprite);
            }
        }
    }
}

/* 80023290: Set a sprite's blend rate; type 8 and 9 sprites keep it one lower, the others recolour their parts. */
void sprite_set_blend_rate(Sprite *sprite, s32 rate) {
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
        sprite_recolor_parts(sprite);
    }
}

/* 80023340: Replace a sprite renderer's part list with room for `count` parts (from the heap bottom). */
void sprite_realloc_parts_from_bottom(Sprite *sprite, s32 count) {
    heap_free(sprite->renderer->parts[0]);
    sprite->renderer->parts[0] = sprite->renderer->parts[1] = heap_alloc(count * 24, 0);
}

/* 800233A4: Create a sprite task (with `extra` bytes after the sprite) under `owner`: its auxiliary node, default sprite and update/destroy callbacks. */
SpriteTask *sprite_task_alloc(Task *owner, s32 extra) {
    SpriteTask *node = heap_alloc(extra + sizeof(SpriteTask), task_alloc_mode);
    Task *auxiliary;
    Sprite *sprite;

    task_link_main_node(owner, &node->task);
    auxiliary = &node->auxiliary;
    task_link_draw_node(&node->task, auxiliary);
    sprite = &node->sprite;
    sprite_reset_defaults(sprite);
    node->task.data = sprite;
    auxiliary->data = sprite;
    task_set_update_callback(&node->task, sprite_task_update);
    task_set_destroy_callback(&node->task, sprite_task_destroy);
    return node;
}

/* 80023440: A frame entry's image index: bits 8-10, plus 8 when bit 14 is set. */
s32 sprite_get_header_kind(u16 *entry) {
    s32 index = (*entry >> 8) & 7;

    if ((*entry >> 14) & 1) {
        index += 8;
    }
    return index;
}

/* 80023468: A property (0, 1 or 2) of kind `kind`; `fallback` for kind 3 and the rest. */
s32 sprite_get_render_kind(s32 kind, s32 fallback) {
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

/* 800234AC: Clear the eight entries of a sprite renderer's 0x40-byte block. */
void sprite_clear_group_entries(Sprite *sprite) {
    s32 i;

    for (i = 0; i != 8; i++) {
        sprite->renderer->pointer34[i].byte0 = 0;
        sprite->renderer->pointer34[i].byte1 = 0;
        sprite->renderer->pointer34[i].half2 = 0;
        sprite->renderer->pointer34[i].half4 = 0;
        sprite->renderer->pointer34[i].half6 = 0;
    }
}

/* 80023538: Apply an animation header: its command script and frame table, facing
 * groups (bits 0-1), gravity from its signed weight (bits 2-7), the frame
 * skip, the sprite's weight and gravity divisor; unless kept (bits 11, 12,
 * 13) clear the speeds, reset the angles and rebuild the orientation, and
 * rescale; then restart the command state. */
void sprite_apply_animation_header(Sprite *sprite, u16 *animation) {
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
    skip = sprite_frame_skip + 1;
    sprite->gravity = weight << 10;
    sprite->gravity *= skip * skip * (s16)sprite->word82 / 4096;
    step = 0x10000 / sprite->motion.bits.divisor;
    sprite->gravity *= step * step / 256;
    sprite->gravity /= 256;
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
            sprite_rebuild_orientation(sprite);
        }
        if (!((animation[0] >> 13) & 1)) {
            if (sprite_in_battle == 0) {
                goto one_sided;
            }
            sprite_set_scale(sprite, sprite_default_scale);
        }
        if (sprite_in_battle != 0) {
            sprite_rebuild_orientation(sprite);
        }
    one_sided:
        if ((sprite->render.word & 3) == 1) {
            sprite->renderer->offset.x = sprite->renderer->offset.y = 0;
            if (!((sprite->flags >> 20) & 1) && sprite->renderer->pointer34 != NULL) {
                sprite_clear_group_entries(sprite);
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

/* 80023804: Reset a sprite's state to the defaults: no flags, frame or animation,
 * blend 0x2d, gravity divisor 256, gravity from the frame skip and the
 * sprite's weight (word 0x82), an empty byte stack. */
void sprite_reset_defaults(Sprite *sprite) {
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
    gravity = sprite_frame_skip + 1;
    sprite->frame_bits.step = 0;
    sprite->motion.bits.divisor = 0x100;
    sprite->gravity = gravity * (gravity << 14) * (s16)sprite->word82 / 4096;
    sprite->script = NULL;
    sprite->parent = 0;
    sprite->resource_block = NULL;
    sprite->callback = NULL;
    sprite->word80 = 0;
    sprite->stack_top = 0x10;
    sprite->ground = 0;
    sprite->block = NULL;
    sprite->word50 = 0;
}

/* 8002393C: Clear a renderer's angles and part list. */
void sprite_reset_renderer(SpriteRenderer *renderer) {
    renderer->angle_x = 0;
    renderer->angle_y = 0;
    renderer->angle_z = 0;
    renderer->parts[0] = NULL;
}

/* 80023950: Detach a sprite's renderer. */
void sprite_detach_renderer(Sprite *sprite) {
    sprite->renderer = NULL;
}

/* 80023958: Give a sprite the renderer stored right after it. */
void sprite_attach_inline_renderer(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    sprite_reset_renderer(sprite->renderer);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->word40 = 0;
}

/* 800239A0: Give a sprite its inline renderer, sequencer and image storage (after the sprite). */
void sprite_attach_inline_storage(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    sprite_reset_renderer(sprite->renderer);
    sprite->sequencer = (u8 *)sprite + 0xF4;
    sprite->renderer->pointer34 = (SpriteRendererEntry *)((u8 *)sprite + 0x124);
    sprite->image = (u8 *)sprite + 0x110;
    sprite->renderer->next_pending = NULL;
}

/* 800239F4: Give a sprite its inline renderer with an inline part list. */
void sprite_attach_inline_parts(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    sprite_reset_renderer(sprite->renderer);
    sprite->renderer->parts[1] = (SpritePart *)((u8 *)sprite + 0xF4);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->next_pending = NULL;
}

/* 80023A48: Create a sprite task under `owner` with `extra` bytes and its storage
 * (mode 0: no renderer, 1: inline renderer and part list sized for the
 * image's first frame (kinds 5 and 6 use the 8006be10 and 8005a474 images),
 * 2: inline renderer); records the allocation, its size and the image. */
SpriteTask *sprite_task_create(s32 kind, s32 mode, SpriteSource *source, s32 extra, Task *owner) {
    SpriteTask *task;
    s32 size;
    Sprite *sprite;

    switch (mode) {
    case 0:
        size = 0;
        task = sprite_task_alloc(owner, extra);
        sprite_detach_renderer(&task->sprite);
        break;
    case 1:
        if (kind == 5) {
            source = (SpriteSource *)sprite_shared_source;
        }
        if (kind == 6) {
            source = (SpriteSource *)sprite_effect_source;
        }
        size = (sprite_get_part_count(source->frames) - 1) * 24 + 0x58;
        task = sprite_task_alloc(owner, size + extra);
        sprite_attach_inline_parts(&task->sprite);
        break;
    case 2:
        size = 0x54;
        task = sprite_task_alloc(owner, extra + 0x54);
        sprite_attach_inline_renderer(&task->sprite);
        break;
    }
    sprite = &task->sprite;
    sprite->block = task;
    sprite->size = size + 0xEC;
    sprite->image = source;
    return task;
}

/* 80023B84: Create a child sprite of `parent` running animation `header` from
 * `source`: its kind from the frame entry (3: the parent's), its storage
 * mode from the kind; it inherits the parent's facing, blending, speeds,
 * frame, position, resources and (with a mode) renderer angles and scales,
 * then takes the animation and its kind's callbacks. With the parent's
 * passive_children flag the task starts inactive. */
Sprite *sprite_create_child(Sprite *parent, u16 *header, SpriteSource *source) {
    u8 active = task_new_tasks_active;
    s32 kind;
    s32 mode;
    SpriteTask *task;
    Sprite *child;
    u32 split;
    u32 frame_bits;
    u32 divisor;
    u32 motion;
    u32 owner_word;

    parent->b0.wordb0 |= 0x800;
    if (parent->b0.bits.passive_children) {
        task_new_tasks_active = 0;
    }
    kind = sprite_get_header_kind(header);
    if (kind == 3) {
        kind = ((SpriteFlagBits *)&parent->flags)->type;
    }
    mode = ((s32 (*)())sprite_get_render_kind)(kind); /* the fallback argument is not passed */
    task = sprite_task_create(kind, mode, source, 0, parent->block);
    task->task.link.bits.flag29 = 1;
    child = &task->sprite;
    /* The original reads this pointer once without using the value. */
    source = ((volatile Sprite *)child)->image;
    ((SpriteFlagBits *)&child->flags)->type = kind;
    child->render.bits.sides = mode;
    child->flags = (child->flags & ~0x1F00) | (parent->flags & 0x1F00);
    child->render.word = (child->render.word & ~8) | (parent->render.word & 8);
    child->render.word = (child->render.word & ~0x10) | (parent->render.word & 0x10);
    child->render.bits.unknown8 = parent->render.bits.unknown8;
    ((SpriteFlagBits *)&child->flags)->flag18 = ((SpriteFlagBits *)&parent->flags)->flag18;
    child->render.word = (child->render.word | 0x4000000) & ~4;
    child->speed = parent->speed;
    child->direction = parent->direction;
    child->scale = parent->scale;
    child->frame = parent->frame;
    if ((child->b0.bits.share_rate = parent->b0.bits.share_rate)) {
        child->rate = parent->rate;
        child->flags = (child->flags & ~0x1F00) | 0x300;
    }
    frame_bits = parent->frame_bits.unknown30;
    split = (parent->motion.bits.unknown0 << 2) | frame_bits;
    child->frame_bits.unknown30 = split;
    child->motion.bits.unknown0 = split >> 2;
    child->b0.bits.passive_children = parent->b0.bits.passive_children;
    child->motion.word = (child->motion.word & ~0x40) | (parent->motion.word & 0x40);
    divisor = parent->motion.word & 0x7FF80;
    child->motion.word = (child->motion.word & ~0x7FF80) | divisor;
    motion = child->motion.word & ~4;
    owner_word = *(u32 *)&child->frame_bits & ~1;
    motion |= parent->motion.word & 4;
    *(u32 *)&child->frame_bits = owner_word;
    child->motion.word = motion;
    if (!parent->frame_bits.sequencer_owned) {
        child->sequencer = parent->sequencer;
    } else {
        child->sequencer = NULL;
    }
    child->parent = parent;
    child->resource_block = parent->resource_block;
    child->animations = parent->animations;
    child->partner = parent->partner;
    child->word82 = parent->word82;
    child->word50 = parent->word50;
    child->unknown8d = parent->motion.bytes[3];
    child->word78 = parent->word78;
    child->x = parent->x;
    child->y = parent->y;
    child->z = parent->z;
    child->speed_x = parent->speed_x;
    child->speed_y = parent->speed_y;
    child->speed_z = parent->speed_z;
    if (mode != 0) {
        child->renderer->angle_x = parent->renderer->angle_x;
        child->renderer->angle_y = parent->renderer->angle_y;
        child->renderer->angle_z = parent->renderer->angle_z;
        child->renderer->scale_x = parent->renderer->scale_x;
        child->renderer->scale_y = parent->renderer->scale_y;
        child->renderer->scale_z = parent->renderer->scale_z;
    }
    sprite_apply_animation_header(child, header);
    sprite_task_init_by_kind(task);
    task_new_tasks_active = active;
    return child;
}

/* 80023FD8: Create an effect sprite task running animation `index` of `source` at
 * `position` (whole units), with `extra` bytes after the sprite: its kind
 * and storage mode come from the animation's frame entry; in battle (flag
 * 800591ad) it takes the facing, blending, speed, scale and resources of
 * the battle's current actor (800c3e1c) before its own resources are
 * cleared. */
SpriteTask *sprite_create_effect(s32 index, SpriteSource *source, SVECTOR *position, s32 extra) {
    u16 *table = source->animations;
    u16 *header = (u16 *)(table[index + 1] + (s32)table);
    s32 kind;
    s32 mode;
    SpriteTask *task;
    Sprite *child;
    Sprite *actor;
    u32 split;
    u32 frame_bits;

    kind = sprite_get_header_kind(header);
    mode = ((s32 (*)())sprite_get_render_kind)(kind); /* the fallback argument is not passed */
    task = sprite_task_create(kind, mode, source, extra, NULL);
    task->task.link.bits.flag29 = 1;
    child = &task->sprite;
    source = child->image; /* kept across the actor copy */
    child->parent = 0;
    child->partner = 0;
    if (sprite_in_battle != 0 && (actor = battle_acting_sprite) != NULL) {
        child->resource_block = actor->resource_block;
        child->animations = actor->animations;
        child->partner = actor->partner;
        child->speed = actor->speed;
        child->direction = actor->direction;
        child->flags = (child->flags & ~0x1F00) | (actor->flags & 0x1F00);
        child->render.word = (child->render.word & ~8) | (actor->render.word & 8);
        child->render.word = (child->render.word & ~0x10) | (actor->render.word & 0x10);
        child->render.bits.unknown8 = actor->render.bits.unknown8;
        child->scale = actor->scale;
        child->render.word = (child->render.word | 0x4000000) & ~4;
        child->motion.bits.mirror = actor->motion.bits.mirror;
        child->motion.bits.divisor = actor->motion.bits.divisor;
        /* Both pointer reads and stores are present in the original. */
        child->sequencer = actor->sequencer;
        child->sequencer = actor->sequencer;
        frame_bits = actor->frame_bits.unknown30;
        split = (actor->motion.bits.unknown0 << 2) | frame_bits;
        child->frame_bits.unknown30 = split;
        child->motion.bits.unknown0 = split >> 2;
        child->word50 = actor->word50;
        child->unknown8d = actor->motion.bytes[3];
    }
    child->resource_block = NULL;
    child->animations = NULL;
    child->frame = 0;
    child->image = source;
    ((SpriteFlagBits *)&child->flags)->type = kind;
    child->render.bits.sides = mode;
    child->word82 = sprite_default_scale;
    child->x = position->vx << 16;
    child->y = position->vy << 16;
    child->z = position->vz << 16;
    sprite_apply_animation_header(child, header);
    sprite_task_init_by_kind(task);
    return task;
}

/* 80024294: 80024524 with `extra` in 800591b8 for the call. */
void sprite_create_with_palette_bank(void *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused, s32 extra) {
    sprite_palette_bank = extra;
    sprite_create(data, clut_x, clut_y, texture_x, texture_y, unused);
    sprite_palette_bank = 0;
}

/* 800242F4: 8002435c with `extra` in 800591b8 for the call. */
void sprite_construct_with_palette_bank(void *sprite, s32 data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused, s32 extra) {
    sprite_palette_bank = extra;
    sprite_construct(sprite, data, clut_x, clut_y, texture_x, texture_y, unused);
    sprite_palette_bank = 0;
}

/* 8002435C: Construct a sprite from resource block `data`: defaults, inline storage,
 * unit scale, one-sided rendering, the sequencer (reset, or owned when
 * 800591ad is clear), render modes from 800591b8, a part list for its
 * first frame, the image origin (width, height, x, y), the resource binding
 * and animation 0. */
Sprite *sprite_construct(Sprite *self, s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused) {
    s32 *block = data;
    Sprite *sprite = self;

    sprite_reset_defaults(sprite);
    sprite_attach_inline_storage(sprite);
    sprite_set_scale(sprite, 0x1000);
    sprite->flags &= ~0x1E000;
    sprite->render.bits.sides = 1;
    if (sprite_in_battle != 0) {
        sprite->frame_bits.sequencer_owned = 0;
        ((SpriteSequencer *)sprite->sequencer)->word8 = 0;
        ((SpriteSequencer *)sprite->sequencer)->halfc = 0;
    } else {
        sprite->frame_bits.sequencer_owned = 1;
        ((SpriteSequencer *)sprite->sequencer)->buffer = NULL;
    }
    sprite->block = sprite;
    sprite->render.bits.mode = sprite_palette_bank;
    sprite->render.bits.field16 = sprite_palette_bank;
    sprite->renderer->parts[1] = sprite->renderer->parts[0] = heap_alloc(sprite_get_part_count((u16 *)(block[2] + (s32)block)) * 24, 0);
    ((SpriteResource *)sprite->image)->origin.vx = width;
    ((SpriteResource *)sprite->image)->origin.vy = height;
    ((SpriteResource *)sprite->image)->origin.vz = x;
    ((SpriteResource *)sprite->image)->origin.pad = y;
    sprite->animations = data;
    sprite_bind_resource(sprite, data);
    sprite->word60 = (u16 *)((s32)((SpriteResource *)sprite->image)->section1 + ((*((SpriteResource *)sprite->image)->section1 & 0x3F) + 1) * 2);
    sprite_start_animation(sprite, 0);
    return sprite;
}

/* 80024524: Allocate a sprite (356 bytes, its inline storage included) and construct
 * it from resource block `data` (8002435c). */
Sprite *sprite_create(s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused) {
    Sprite *sprite = heap_alloc(0x164, 0);

    sprite->size = 0x164;
    return sprite_construct(sprite, data, x, y, width, height, unused);
}

/* 800245D8: Select animation `animation` (negative: ~animation from the alternate
 * resource): bind the resource (image size from the sequencer or 768 x 256
 * with 800591ad set), store the animation number, apply the animation's
 * header (80023538) and face the current angle again. */
void sprite_start_animation(Sprite *sprite, s32 animation) {
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
        sprite_bind_resource(sprite, (s32 *)sprite->resource);
        if (sprite_in_battle != 0 && !sprite_is_cell_directory(((SpriteResource *)sprite->image)->section2)) {
            ((SpriteImage *)sprite->image)->size.height = 0x100;
            ((SpriteImage *)sprite->image)->size.width = 0x300;
        }
    } else {
        sprite_bind_resource(sprite, sprite->animations);
        if (sprite_in_battle != 0) {
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
    sprite_apply_animation_header(sprite, table);
    sprite_set_facing(sprite, sprite->word80);
}

/* 80024730: Finish a new sprite task by its kind: 7 counts its updates, 8 and 9 set
 * their blending and show a frame, 10-13 are camera markers the battle
 * overlay registers (800bc158) at the eye (10, 12) or look-at (11, 13)
 * position, 12 and 13 becoming 10 and 11 with a frame shown; then the
 * auxiliary node gets its kind's update callback. */
/* The sprite and auxiliary node are reached through the task's first node;
 * the 800bc158 calls take the task itself, which the original keeps in its
 * own register. */
void sprite_task_init_by_kind(SpriteTask *task) {
    SpriteTask *self = (SpriteTask *)&task->task;
    Sprite *sprite = &self->sprite;
    Task *auxiliary = &self->auxiliary;
    s32 kind = ((SpriteFlagBits *)&sprite->flags)->type;

    switch (kind) {
    case 12:
        sprite->frame = 1;
        kind = ((SpriteFlagBits *)&sprite->flags)->type -= 2;
        battle_camera_register_sprite(task);
        sprite->x = sprite_camera_eye.vx;
        sprite->y = sprite_camera_eye.vy;
        sprite->z = sprite_camera_eye.vz;
        break;
    case 10:
        sprite->frame = 0;
        battle_camera_register_sprite(task);
        sprite->x = sprite_camera_eye.vx;
        sprite->y = sprite_camera_eye.vy;
        sprite->z = sprite_camera_eye.vz;
        break;
    case 13:
        sprite->frame = 1;
        kind = ((SpriteFlagBits *)&sprite->flags)->type -= 2;
        battle_camera_register_sprite(task);
        sprite->x = sprite_camera_eye.vx;
        sprite->y = sprite_camera_eye.vy;
        sprite->z = sprite_camera_eye.vz;
        break;
    case 11:
        sprite->frame = 0;
        battle_camera_register_sprite(task);
        sprite->x = sprite_camera_look_at.vx;
        sprite->y = sprite_camera_look_at.vy;
        sprite->z = sprite_camera_look_at.vz;
        break;
    case 7:
        task_set_update_callback(&self->task, sprite_task_update_counted);
        break;
    case 8:
        sprite->colour_flags = 0x68;
        sprite->frame = 1;
        break;
    case 9:
        sprite->height = 3;
        sprite->colour_flags = 0x60;
        sprite->frame = 1;
        break;
    case 0:
    case 14:
        break;
    }
    sprite_task_set_draw_by_kind(auxiliary, kind);
}
