/* Resident sprite engine, first unit (0x8001c8dc-0x80022090): task lists,
 * sprite motion and the animation script interpreter. Compiled by the CDK
 * GCC at -G8; positive `li` are assembled as `addiu` (ASPSX 2.50+). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/area.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/task.h"
#include "resident/text.h"
#include "battle_overlay.h"
#include "own_declarations.h"

TexturePosition sprite_cell_page_positions[8] = { /* 8004FAB8 */
    {0x300, 0}, {0x340, 0}, {0x380, 0}, {0x3C0, 0},
    {0x300, 0x100}, {0x340, 0x100}, {0x380, 0x100}, {0x3C0, 0x100},
};
SVECTOR sprite_shadow_corners[4] = {0}; /* 8004FAD8 */
/* Single-bit masks: halfwords (the facing group masks) and words. */
u16 sprite_halfword_bit_masks[16] = { /* 8004FAF8 */
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
};
u32 sprite_word_bit_masks[32] = { /* 8004FB18 */
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000,
    0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000,
};
SVECTOR sprite_quad_corners[4] = {0}; /* 8004FB98 */
/* The sprite camera: identity rotation, no translation. */
MATRIX sprite_view_matrix = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}}; /* 8004FBB8 */
/* The packed image 8001fab4 uploads: its unpacked size (664 bytes) and the
 * LZSS stream 80032e88 decodes. */
INCLUDE_ASSET(".data", sprite_packed_pause_image, 0x8004FBD8, 0xE8);

/* The unit's own small data, statics and small commons, all $gp-relative.
 * The statics take the unit's .sbss (800592e4); the commons merge with
 * commons/commons_small.c's definitions. */
u32 task_next_serial = 0;                  /* 80059184 */
s32 task_main_count = 0;                   /* 80059188 */
s32 task_draw_count = 0;                   /* 8005918C */
Sprite *sprite_pending_list = NULL;        /* 80059190 */
s16 sprite_texture_area_row = 0;           /* 80059194: texture area row (0-2) of the next image */
s16 sprite_texture_area_column = 0;        /* 80059196: texture area column of the next image */
static s32 *sprite_image_list;             /* 800592E4: image list for 8001fb30 */
static DVECTOR sprite_image_list_position; /* 800592E8: its position */
Task *task_unread_current_node;
Task *task_main_list;
Task *task_next_node;
Task *task_draw_list;

/* 8001C8DC: Destroy every task of both lists. */
void task_destroy_all(void) {
    Task *task;

    while ((task = task_main_list) != NULL) {
        task->destroy(task);
    }
    while ((task = task_draw_list) != NULL) {
        task->destroy(task);
    }
}

/* 8001C944: Empty both task lists. */
void task_clear_lists(void) {
    task_main_list = NULL;
    task_draw_list = NULL;
    task_main_count = 0;
    task_draw_count = 0;
    task_main_pause_timer = 0;
}

/* 8001C964: Run the main task list, unless it is paused (count the pause down). */
void task_run_main_list(void) {
    Task *task;

    if (task_main_pause_timer != 0) {
        if (--task_main_pause_timer == 0) {
            task_catch_up_frame_count = 0;
        }
        return;
    }
    task_next_node = task_main_list;
    while (task_next_node != NULL) {
        task = task_next_node;
        task_unread_current_node = task;
        task_next_node = task->next;
        if (task->update != NULL) {
            task->update(task);
        }
    }
}

/* 8001C9F8: Run the second task list. */
void task_run_draw_list(void) {
    Task *task;

    task_next_node = task_draw_list;
    while (task_next_node != NULL) {
        task = task_next_node;
        task_unread_current_node = task;
        task_next_node = task->next;
        if (task->update != NULL) {
            task->update(task);
        }
    }
}

/* 8001CA58: Link `node` at the head of the second task list under `owner`. */
void task_link_draw_node(Task *owner, Task *node) {
    TaskLink link;
    Task *head = task_draw_list;

    node->owner = owner;
    node->next = head;
    task_draw_list = node;
    link.word = node->link.word;
    link.bits.owner_serial = owner->id.bits.serial;
    node->id.bits.serial = task_next_serial++;
    link.bits.flag29 = 0;
    link.bits.flag30 = 0;
    link.bits.active = 0;
    node->link = link;
    node->update = NULL;
    node->destroy = task_unlink_draw_node;
    task_draw_count++;
}

/* 8001CAF0: Allocate a task with `size` bytes after its node on the second list. */
Task *task_alloc_draw_task(Task *owner, s32 size) {
    Task *node = heap_alloc(size + sizeof(Task), task_alloc_mode);

    task_link_draw_node(owner, node);
    node->destroy = task_destroy_draw_task;
    return node;
}

/* 8001CB48: Unlink a task from the second list. */
void task_unlink_draw_node(Task *task) {
    Task *prev = NULL;
    Task *current;

    for (current = task_draw_list; current != NULL; current = current->next) {
        if (current == task) {
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                task_draw_list = current->next;
            }
            if (task_next_node == task) {
                task_next_node = task->next;
            }
            break;
        }
        prev = current;
    }
    if (current == NULL) {
        task_draw_count++;
    }
    task_draw_count--;
}

/* 8001CBE8: Destroy callback of an allocated second-list task: unlink and free it. */
void task_destroy_draw_task(Task *task) {
    task_unlink_draw_node(task);
    heap_free(task);
}

/* 8001CC18: Link `node` at the head of the main task list under `owner`; it counts as
 * active while the active flag (800591ac) is set. */
/* The node is written through a second pointer to it (the original keeps
 * the node in $t3 and the inactive branch's copy in $v1); the owner's
 * serial is read once the owner is linked. */
void task_link_main_node(Task *owner, Task *node) {
    Task *self = &node[0];
    u32 serial;

    self->owner = owner;
    serial = owner->id.bits.serial;
    self->destroy = task_unlink_main_node;
    self->update = NULL;
    self->next = task_main_list;
    task_main_list = self;
    self->link.bits.owner_serial = serial;
    self->link.bits.flag29 = 0;
    self->link.bits.flag30 = 0;
    self->link.bits.active = 0;
    self->id.bits.serial = task_next_serial++;
    if (task_new_tasks_active != 0) {
        task_active_main_count++;
        self->link.bits.active = 1;
    } else {
        self->link.bits.active = 0;
    }
    task_main_count++;
}

/* 8001CD08: Allocate a task with `size` bytes after its node on the main list. */
Task *task_alloc_main_task(Task *owner, s32 size) {
    Task *node = heap_alloc(size + sizeof(Task), task_alloc_mode);

    task_link_main_node(owner, node);
    node->destroy = task_destroy_main_task;
    node->data = NULL;
    return node;
}

/* 8001CD64: Set a task's update callback. */
void task_set_draw_callback(Task *task, void (*update)(Task *)) {
    task->update = update;
}

/* 8001CD6C: Set a task's update callback (second entry). */
void task_set_update_callback(Task *task, void (*update)(Task *)) {
    task->update = update;
}

/* 8001CD74: Set a task's destroy callback. */
void task_set_destroy_callback(Task *task, void (*destroy)(Task *)) {
    task->destroy = destroy;
}

/* 8001CD7C: A task's update callback. */
void *task_get_update_callback(Task *task) {
    return task->update;
}

/* 8001CD88: A task's destroy callback. */
void *task_get_destroy_callback(Task *task) {
    return task->destroy;
}

/* 8001CD94: Unlink a task from the main list. */
void task_unlink_main_node(Task *task) {
    Task *prev = NULL;
    Task *current;

    for (current = task_main_list; current != NULL; current = current->next) {
        if (current == task) {
            if (prev != NULL) {
                prev->next = task->next;
            } else {
                task_main_list = task->next;
            }
            if (task_next_node == task) {
                task_next_node = task->next;
            }
            break;
        }
        prev = current;
    }
    if (task->link.bits.active) {
        task_active_main_count--;
    }
    task_main_count--;
}

/* 8001CE44: Destroy callback of an allocated main-list task: unlink and free it. */
void task_destroy_main_task(Task *task) {
    task_unlink_main_node(task);
    heap_free(task);
}

/* 8001CE74: Destroy every task `owner` created (both lists). */
void task_destroy_owned_by(Task *owner) {
    Task *prev;
    Task *task;

    prev = NULL;
    for (task = task_draw_list; task != NULL; task = task->next) {
        if (task->owner == owner && !((task->link.word >> 30) & 1) && task->link.bits.owner_serial == owner->id.bits.serial) {
            if (prev != NULL) {
                prev->next = task->next;
            } else {
                task_draw_list = task->next;
            }
            if (task_next_node == task) {
                task_next_node = task->next;
            }
            if (task->destroy != NULL) {
                task->destroy(task);
            }
        } else {
            prev = task;
        }
    }
    prev = NULL;
    for (task = task_main_list; task != NULL; task = task->next) {
        if (task->owner == owner && !((task->link.word >> 30) & 1) && task->link.bits.owner_serial == owner->id.bits.serial) {
            if (prev != NULL) {
                prev->next = task->next;
            } else {
                task_main_list = task->next;
            }
            if (task_next_node == task) {
                task_next_node = task->next;
            }
            if (task->destroy != NULL) {
                task->destroy(task);
            }
        } else {
            prev = task;
        }
    }
}

/* 8001D034: Clear word 0x70 of the sprite of every flag-29 task `owner` created. */
void task_clear_child_sprite_parents(Task *owner) {
    Task *task;

    for (task = task_main_list; task != NULL; task = task->next) {
        if (task->owner == owner && task->link.bits.owner_serial == owner->id.bits.serial && ((task->link.word >> 29) & 1)) {
            ((Sprite *)task->data)->parent = NULL;
        }
    }
}

/* 8001D0A4: The main-list task `owner` created with update callback `update`, or NULL. */
Task *task_find_owned_with_update(Task *owner, void (*update)(Task *)) {
    Task *task;

    for (task = task_main_list; task != NULL; task = task->next) {
        if (task->owner == owner && task->link.bits.owner_serial == owner->id.bits.serial && task->update == update) {
            return task;
        }
    }
    return NULL;
}

/* 8001D10C: The first main-list task `owner` created, or NULL. */
Task *task_find_first_owned_by(Task *owner) {
    Task *task;

    for (task = task_main_list; task != NULL; task = task->next) {
        if (task->owner == owner && task->link.bits.owner_serial == owner->id.bits.serial) {
            return task;
        }
    }
    return NULL;
}

/* 8001D164: The first main-list task with update callback `update`, or NULL. */
Task *task_find_by_update(void (*update)(Task *)) {
    Task *task;

    for (task = task_main_list; task != NULL; task = task->next) {
        if (task->update == update) {
            return task;
        }
    }
    return NULL;
}

/* 8001D19C: Destroy callback of a two-node task: unlink both nodes and free it. */
void task_destroy_two_node_task(Task *task) {
    task_unlink_draw_node(task + 1);
    task_unlink_main_node(task);
    heap_free(task);
}

/* 8001D1D8: Allocate a `size`-byte task that starts with two nodes: the first on the main
 * list under `owner` with `update`, the second on the second list with
 * `update2`; both nodes' data is the task itself. */
Task *task_alloc_two_node_task(s32 size, Task *owner, void (*update)(Task *), void (*update2)(Task *),
                    void (*destroy)(Task *)) {
    Task *node = heap_alloc(size, task_alloc_mode);

    task_link_main_node(owner, node);
    task_link_draw_node(node, node + 1);
    task_set_update_callback(node, update);
    task_set_draw_callback(node + 1, update2);
    if (destroy != NULL) {
        task_set_destroy_callback(node, destroy);
    } else {
        task_set_destroy_callback(node, task_destroy_two_node_task);
    }
    node->data = node;
    node[1].data = node;
    return node;
}

/* 8001D298: Empty the pending sprite list (the sprite queues' set-up 80024f64 and
 * release 80024fb8 call one each). */
void sprite_clear_pending_list(void) {
    sprite_pending_list = NULL;
}

/* 8001D2A4 */
void sprite_clear_pending_list_on_release(void) {
    sprite_pending_list = NULL;
}

/* 8001D2B0: Request frame `frame` for a one-sided sprite: it joins the pending list
 * (drawn by 8001d468), or, already pending, first draws its previous one. */
void sprite_request_frame(Sprite *sprite, s32 frame) {
    Sprite *pending;

    if ((sprite->render.word & 3) != 1) {
        sprite->frame = 0;
        return;
    }
    if ((sprite->flags >> 20) & 1) {
        sprite->flags &= ~0x100000;
        if (sprite->renderer->pointer34 != NULL) {
            sprite_clear_group_entries(sprite);
        }
    }
    if ((sprite->flags >> 17) & 1) {
        for (pending = sprite_pending_list; pending != NULL; pending = pending->renderer->next_pending) {
            if (pending == sprite) {
                if (sprite->image != sprite_effect_source && sprite->image != sprite_shared_source && !((sprite->flags >> 19) & 1)) {
                    sprite_apply_frame_controls(sprite, sprite->frame, sprite->image);
                }
                sprite->frame = frame;
                return;
            }
        }
    }
    sprite->frame = frame;
    sprite->flags |= 0x20000;
    sprite->renderer->next_pending = sprite_pending_list;
    sprite_pending_list = sprite;
}

/* 8001D3F4: Remove a sprite from the pending list. */
void sprite_remove_pending(Sprite *sprite) {
    Sprite *prev = NULL;
    Sprite *pending;

    for (pending = sprite_pending_list; pending != NULL; pending = pending->renderer->next_pending) {
        if (pending == sprite) {
            if (prev != NULL) {
                prev->renderer->next_pending = pending->renderer->next_pending;
            } else {
                sprite_pending_list = pending->renderer->next_pending;
            }
        } else {
            prev = pending;
        }
    }
}

/* 8001D468: Draw the pending frame of every pending sprite and empty the list. */
void sprite_build_pending_frames(void) {
    Sprite *sprite;
    s32 frame;

    for (sprite = sprite_pending_list; sprite != NULL; sprite = sprite->renderer->next_pending) {
        frame = sprite->frame;
        if (frame == 0) {
            sprite->flags &= ~0xFC;
        } else {
            sprite_build_frame(sprite, frame, sprite->image);
        }
    }
    sprite_pending_list = NULL;
}

/* 8001D4E8: Give a sprite's renderer its 0x40-byte block (once). */
void sprite_alloc_group_entries(Sprite *sprite) {
    if (sprite->renderer->pointer34 == NULL) {
        sprite->renderer->pointer34 = heap_alloc(0x40, 0);
        sprite_clear_group_entries(sprite);
    }
}

/* 8001D53C: Build frame `frame` of a cell-directory source (directory bit 15): each
 * part takes a cell already in VRAM with its own texture position and size,
 * from the page of a resident cell kind (two-byte kinds), of the sprite's
 * sequencer or of the source, after the control bytes before it. */
void sprite_build_cell_frame(Sprite *sprite, s32 frame, SpriteSource *source) {
    u16 *table;
    u8 *record;
    u16 *cells;
    u16 v_base;
    u16 blend;
    u32 colour;
    s32 count;
    u8 wide;
    Sprite *self;
    u8 *p;
    SpritePart *parts;
    s32 group;
    s32 i;
    u8 control;
    u8 *cell;
    s32 kind;
    u16 u_base;
    s16 rate;
    s32 palette;
    TexturePosition *page;
    TexturePosition *pages;

    table = source->frames;
    record = (u8 *)(table[frame] + (s32)table);
    cells = (u16 *)(record + 4);
    wide = *record & 0x80;
    count = *record & 0x3F;
    p = record + (count * 2 + 4);
    parts = sprite->renderer->parts[1];
    self = sprite;
    sprite->height = record[3] * sprite->scale / 4096;
    sprite->extent_depth = record[1] * sprite->scale / 4096;
    v_base = source->origin.vy & 0xFF;
    blend = sprite->render.bits.blend;
    group = 4;
    colour = *(u32 *)&sprite->red;
    for (i = 0; i != count; i++, p += 3) {
        parts[i].byte9 = 0;
        parts[i].byte8 = 0;
        parts[i].flags &= ~0x20;
    next:
        control = *p;
        if (control & 0x80) {
            p++;
            if (control & 0x40) {
                group = control & 7;
                if (sprite->renderer->pointer34 == NULL) {
                    sprite->renderer->pointer34 = heap_alloc(0x40, 0);
                    sprite_clear_group_entries(sprite);
                }
                if (control & 0x20) {
                    sprite->renderer->pointer34[group].byte0 = *p++;
                    sprite->renderer->pointer34[group].byte1 = *p++;
                }
                if (control & 0x10) {
                    s16 depth = *p++ << 4;

                    sprite->renderer->pointer34[group].half6 = depth;
                } else {
                    sprite->renderer->pointer34[group].half6 = 0;
                }
            } else {
                if (control & 4) {
                    parts[i].flags |= 0x20;
                }
                if (control & 1) {
                    parts[i].byte8 = *p++;
                }
                if (control & 2) {
                    parts[i].byte9 = *p++;
                }
            }
            goto next;
        }
        cell = (u8 *)(*cells + (s32)table);
        kind = *cell;
        cells++;
        if (kind & 1) {
            parts[i].flags |= 8;
            u_base = (source->origin.vx & 0x3F) >> 1;
        } else {
            parts[i].flags &= ~8;
            u_base = (source->origin.vx & 0x3F) >> 2;
        }
        rate = (*p >> 4) & 3;
        palette = *p & 0xF;
        parts[i].colour = colour;
        if (rate != 0 || (rate = blend) != 0) {
            rate--;
            ((u8 *)&parts[i].colour)[3] |= 2;
        }
        if ((kind >> 4) & 1) {
            TexturePosition *resident_pages;
            TexturePosition *resident_page;

            cell++;
            kind |= *cell << 8;
            resident_pages = sprite_cell_page_positions;
            resident_page = (TexturePosition *)(((kind << 1) & 0x1C) + (s32)resident_pages);
            parts[i].tpage = GetTPage(kind & 1, rate, resident_page->x, resident_page->y);
            parts[i].clut = GetClut((kind >> 1) & 0xF0, ((kind >> 9) & 0xF) + 0x1CC);
        } else {
            if (self->frame_bits.sequencer_owned == 1 &&
                (pages = (TexturePosition *)((SpriteSequencer *)self->sequencer)->buffer) != NULL) {
                page = (TexturePosition *)((kind << 1 & 0x1C) + (s32)pages);
                v_base = page->y & 0xFF;
                u_base = (page->x & 0x3F) >> 2;
                parts[i].tpage = getTPage(kind & 1, rate, page->x, page->y);
            } else {
                parts[i].tpage = getTPage(kind & 1, rate, source->origin.vx + ((kind << 5) & 0x1C0), source->origin.vy);
            }
            parts[i].clut = getClut(source->clut_x + palette * 16, source->clut_y);
        }
        parts[i].flags = (parts[i].flags & ~7) | group;
        parts[i].u = u_base + cell[1];
        parts[i].v = v_base + cell[2];
        parts[i].w = cell[3];
        parts[i].h = cell[4];
        parts[i].flags = (parts[i].flags & ~0x10) | ((*p >> 2) & 0x10);
        if (wide) {
            parts[i].x = p[1] | ((s8)p[2] << 8);
            parts[i].y = p[3] | ((s8)p[4] << 8);
            p += 2;
        } else {
            parts[i].x = (s8)p[1];
            parts[i].y = (s8)p[2];
        }
    }
    sprite->flags = (sprite->flags & ~0xFC) | ((i & 0x3F) << 2);
}

/* 8001DAE8: Build frame `frame` of a sprite's source into its parts and queue the
 * uploads of the cells it uses (to the source's texture position, or a
 * reserved texture area column for facing group 14), after its palette
 * when the render flag asks for it; cell-directory sources go to 8001d53c. */
void sprite_build_frame(Sprite *sprite, s32 frame, SpriteSource *source) {
    u16 *table;
    SpritePart *parts;
    u16 *palette;
    DVECTOR position;
    RECT rect; /* the palette area, then a cell's place in the image */
    u8 *record;
    u16 *cells;
    u32 blend;
    u32 colour;
    s32 u_base4;           /* texture column of the image in 4-bit texels */
    s32 u_base8;           /* ... in 8-bit texels */
    u32 v_base;
    s32 count;
    u8 wide;
    u8 *p;
    s32 group;
    s32 i;
    u8 control;
    SpriteCell *cell;
    u16 kind;
    u8 u;
    u8 words;
    s16 rate;
    s32 clut;

    sprite->flags &= ~0x20000;
    sprite->flags &= ~0x80000;
    table = source->frames;
    parts = sprite->renderer->parts[1];
    if (frame < (*table & 0x1FF) + 1) {
        if ((sprite->render.word >> 30) & 1) {
            sprite->render.word &= ~0x40000000;
            palette = source->palette;
            if (*palette != 0) {
                rect.x = source->clut_x;
                rect.y = source->clut_y;
                rect.w = *palette * 16;
                rect.h = 1;
                sprite_queue_upload((u_long *)(palette + (*palette * ((sprite->render.word >> 16) & 0xF0) + 2)),
                              source->clut_x, source->clut_y, *palette * 16, 1);
            }
        }
        if (*table & 0x8000) {
            sprite_build_cell_frame(sprite, frame, source);
            return;
        }
        record = (u8 *)(table[frame] + (s32)table);
        position = ((SpriteSource *)sprite->image)->origin;
        if (((sprite->flags >> 13) & 0xF) == 0xE) {
            position = sprite_reserve_texture_columns(record[4]);
        }
        cells = (u16 *)(record + 6);
        wide = *record & 0x80;
        count = *record & 0x3F;
        p = record + (count * 4 + 6);
        sprite->height = record[3] * sprite->scale / 4096;
        sprite->extent_depth = record[1] * sprite->scale / 4096;
        blend = sprite->render.bits.blend;
        u_base4 = (position.vx & 0x3F) * 4;
        u_base8 = (position.vx & 0x3F) * 2;
        v_base = position.vy & 0xFF;
        group = 4;
        colour = *(u32 *)&sprite->red;
        for (i = 0; i != count; i++, p += 3) {
            parts[i].byte9 = 0;
            parts[i].byte8 = 0;
            parts[i].flags &= ~0x20;
        next:
            control = *p;
            if (control & 0x80) {
                p++;
                if (control & 0x40) {
                    if (sprite->renderer->pointer34 == NULL) {
                        sprite->renderer->pointer34 = heap_alloc(0x40, 0);
                        sprite_clear_group_entries(sprite);
                    }
                    group = control & 7;
                    if (control & 0x20) {
                        sprite->renderer->pointer34[group].byte0 = *p++;
                        sprite->renderer->pointer34[group].byte1 = *p++;
                    }
                    if (control & 0x10) {
                        s16 depth = *p++ << 4;

                        sprite->renderer->pointer34[group].half6 = depth;
                    } else {
                        sprite->renderer->pointer34[group].half6 = 0;
                    }
                } else {
                    if (control & 4) {
                        parts[i].flags |= 0x20;
                    }
                    if (control & 1) {
                        parts[i].byte8 = *p++;
                    }
                    if (control & 2) {
                        parts[i].byte9 = *p++;
                    }
                }
                goto next;
            }
            cell = (SpriteCell *)(cells[0] * 4 + (s32)table);
            rect.x = cells[1] & 0x1F;
            rect.y = (cells[1] >> 5) & 0x3F;
            kind = cell->kind;
            cells += 2;
            if (kind & 1) {
                u = u_base8 + rect.x * 2;
                words = cell->w >> 1;
                parts[i].flags |= 8;
            } else {
                u = u_base4 + rect.x * 4;
                words = cell->w >> 2;
                parts[i].flags &= ~8;
            }
            parts[i].u = u;
            parts[i].flags = (parts[i].flags & ~7) | group;
            parts[i].v = rect.y + v_base;
            parts[i].w = cell->w;
            parts[i].h = cell->h;
            parts[i].flags = (parts[i].flags & ~0x10) | ((*p >> 2) & 0x10);
            rate = (*p >> 4) & 3;
            clut = *p & 0xF;
            parts[i].colour = colour;
            if (rate != 0 || (rate = blend) != 0) {
                rate--;
                ((u8 *)&parts[i].colour)[3] |= 2;
            }
            parts[i].tpage = getTPage(kind & 1, rate, position.vx, position.vy);
            parts[i].clut = GetClut(source->clut_x + clut * 16, source->clut_y);
            sprite_queue_upload((u_long *)(cell + 1), position.vx + rect.x, position.vy + rect.y, words, cell->h);
            if (wide) {
                parts[i].x = p[1] | ((s8)p[2] << 8);
                parts[i].y = p[3] | ((s8)p[4] << 8);
                p += 2;
            } else {
                parts[i].x = (s8)p[1];
                parts[i].y = (s8)p[2];
            }
        }
        sprite->flags = (sprite->flags & ~0xFC) | ((i & 0x3F) << 2);
        sprite_queue_upload(NULL, position.vx, position.vy, record[4], record[5]);
    }
}

/* 8001E148: Set the GTE rotation and translation for drawing a sprite: its position
 * through the view matrix plus its scaled screen offset. */
void sprite_set_draw_matrix(Sprite *sprite) {
    SVECTOR position;
    VECTOR view;
    s32 shift;
    s32 offset_y;
    s32 offset_x;
    MATRIX *matrix;

    if (sprite_in_battle != 0 || sprite_in_worldmap != 0) {
        sprite_update_orientation(sprite);
    }
    shift = (sprite->flags >> 8) & 0x1F;
    offset_y = sprite->renderer->offset.y;
    offset_x = sprite->renderer->offset.x;
    offset_y <<= shift;
    offset_x <<= shift;
    if ((sprite->motion.word >> 2) & 1) {
        offset_x = -offset_x;
    }
    offset_y = offset_y * sprite->scale / 4096;
    offset_x = offset_x * sprite->scale / 4096;
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    ApplyMatrix(&sprite_view_matrix, &position, &view);
    matrix = &sprite->renderer->matrix;
    matrix->t[0] = sprite_view_matrix.t[0] + view.vx + offset_x;
    matrix->t[1] = sprite_view_matrix.t[1] + view.vy + offset_y;
    matrix->t[2] = sprite_view_matrix.t[2] + view.vz;
    SetRotMatrix(matrix);
    SetTransMatrix(matrix);
}

/* 8001E298: Draw a sprite's parts at `ot` with 8001e3d8 (and 8001e9bc for render flag 2). */
void sprite_draw(Sprite *sprite, u_long *ot) {
    sprite_set_draw_matrix(sprite);
    sprite_draw_parts(sprite, ot);
    if ((sprite->render.word >> 2) & 1) {
        sprite_draw_shadow(sprite, ot);
    }
}

/* 8001E2F8: Draw a sprite's parts at `ot` with 8001ee88 (and 8001e9bc for render flag 2). */
void sprite_draw_cut_below(Sprite *sprite, u_long *ot, s32 height) {
    sprite_set_draw_matrix(sprite);
    sprite_draw_parts_cut_below(sprite, ot, height);
    if ((sprite->render.word >> 2) & 1) {
        sprite_draw_shadow(sprite, ot);
    }
}

/* 8001E368: Draw a sprite's parts at `ot` with 8001f1d4 (and 8001e9bc for render flag 2). */
void sprite_draw_cut_above(Sprite *sprite, u_long *ot, s32 height) {
    sprite_set_draw_matrix(sprite);
    sprite_draw_parts_cut_above(sprite, ot, height);
    if ((sprite->render.word >> 2) & 1) {
        sprite_draw_shadow(sprite, ot);
    }
}

/* 8001E3D8: Draw a sprite's parts as textured quads (POLY_FT4 from the queue block)
 * linked at `ot` (or, with render bit 27, at `ot` minus the part's group).
 * Parts of one group share a matrix: the renderer's, or its product with the
 * group entry's rotation and offset; groups masked by render byte 1
 * (8004faf8) are skipped. */
void sprite_draw_parts(Sprite *sprite, u_long *ot) {
    SpriteRenderer *renderer;
    u32 flags;
    s32 mirror;
    s32 shift;
    s32 origin_y;
    s32 origin_x;
    SpritePart *parts;
    s32 count;
    s32 group;
    s32 i;
    u8 visible;
    s32 offset_x;
    s32 offset_y;
    MATRIX m;
    SVECTOR angles;
    VECTOR depth;
    long flag;
    POLY_FT4 *poly;
    s16 w, h, x, y;
    u8 u;
    s32 v, du, dv;

    renderer = sprite->renderer;
    flags = sprite->flags;
    mirror = (sprite->motion.word >> 2) & 1;
    shift = (flags >> 8) & 0x1F;
    origin_y = renderer->offset.y;
    origin_x = renderer->offset.x;
    parts = renderer->parts[1];
    origin_y <<= shift;
    origin_x <<= shift;
    if (mirror) {
        origin_x = -origin_x;
    }
    count = (flags >> 2) & 0x3F;
    group = -1;
    if ((u8 *)sprite_queue_next_free + count * sizeof(POLY_FT4) < sprite_queue_block_end) {
        for (i = 0; i != (sprite->flags >> 2 & 0x3F); i++) {
            if (group != (parts[i].flags & 7)) {
                group = parts[i].flags & 7;
                visible = (sprite_halfword_bit_masks[group] & ((u8 *)&sprite->render)[1]) == 0;
                if (sprite->renderer->pointer34 != NULL &&
                    (*(u16 *)&sprite->renderer->pointer34[group] != 0 ||
                     sprite->renderer->pointer34[group].half6 != 0)) {
                    offset_y = sprite->renderer->pointer34[group].byte1;
                    offset_x = sprite->renderer->pointer34[group].byte0;
                    offset_y <<= (sprite->flags >> 8) & 0x1F;
                    offset_x <<= (sprite->flags >> 8) & 0x1F;
                    if ((sprite->render.word >> 3) & 1) {
                        offset_x = -offset_x;
                    }
                    offset_y = offset_y * sprite->scale / 4096;
                    offset_x = offset_x * sprite->scale / 4096;
                    angles.vx = sprite->renderer->pointer34[group].half2;
                    angles.vy = sprite->renderer->pointer34[group].half4;
                    angles.vz = sprite->renderer->pointer34[group].half6;
                    if ((sprite->render.word >> 3) & 1) {
                        angles.vz = -angles.vz;
                    }
                    gpu_build_rotation_matrix(&angles, &m);
                    m.t[0] = sprite->renderer->matrix.t[0] + offset_x;
                    m.t[1] = sprite->renderer->matrix.t[1] + offset_y;
                    m.t[2] = sprite->renderer->matrix.t[2];
                    SetMulMatrix(&sprite->renderer->matrix, &m);
                    SetTransMatrix(&m);
                } else {
                    SetRotMatrix(&sprite->renderer->matrix);
                    SetTransMatrix(&sprite->renderer->matrix);
                }
            }
            if (visible) {
                poly = (POLY_FT4 *)sprite_queue_next_free;
                sprite_queue_next_free = (SpriteQueueEntry *)(poly + 1);
                setlen(poly, 9);
                *(u32 *)&poly->r0 = parts[i].colour;
                poly->tpage = parts[i].tpage;
                poly->clut = parts[i].clut;
                w = parts[i].w + (s8)parts[i].byte8;
                h = parts[i].h + (s8)parts[i].byte9;
                w <<= ((sprite->flags >> 8) & 0x1F);
                h <<= ((sprite->flags >> 8) & 0x1F);
                x = parts[i].x << ((sprite->flags >> 8) & 0x1F);
                y = parts[i].y << ((sprite->flags >> 8) & 0x1F);
                if ((sprite->render.word >> 3) & 1) {
                    w = -w;
                    x = -x;
                }
                if ((sprite->render.word >> 4) & 1) {
                    h = -h;
                    y = -y;
                }
                if (!((parts[i].flags >> 4) & 1)) {
                    sprite_quad_corners[0].vx = x;
                    sprite_quad_corners[1].vx = x + w;
                    sprite_quad_corners[2].vx = x + w;
                    sprite_quad_corners[3].vx = x;
                } else {
                    sprite_quad_corners[0].vx = x + w;
                    sprite_quad_corners[1].vx = x;
                    sprite_quad_corners[2].vx = x;
                    sprite_quad_corners[3].vx = x + w;
                }
                if (!((parts[i].flags >> 5) & 1)) {
                    sprite_quad_corners[0].vy = y;
                    sprite_quad_corners[1].vy = y;
                    sprite_quad_corners[2].vy = y + h;
                    sprite_quad_corners[3].vy = y + h;
                } else {
                    sprite_quad_corners[0].vy = y + h;
                    sprite_quad_corners[1].vy = y + h;
                    sprite_quad_corners[2].vy = y;
                    sprite_quad_corners[3].vy = y;
                }
                sprite_quad_corners[0].vy -= origin_y;
                sprite_quad_corners[1].vy -= origin_y;
                sprite_quad_corners[2].vy -= origin_y;
                sprite_quad_corners[3].vy -= origin_y;
                sprite_quad_corners[0].vx -= origin_x;
                sprite_quad_corners[1].vx -= origin_x;
                sprite_quad_corners[2].vx -= origin_x;
                sprite_quad_corners[3].vx -= origin_x;
                RotTransPers4(&sprite_quad_corners[0], &sprite_quad_corners[1], &sprite_quad_corners[2], &sprite_quad_corners[3], (long *)&poly->x0,
                              (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &depth.pad, &flag);
                u = parts[i].u;
                v = parts[i].v;
                du = parts[i].w - 1;
                dv = parts[i].h - 1;
                if (poly->x3 < poly->x0) {
                    s32 cropped_u = u - 1;
                    if (cropped_u >= 0) {
                        u = cropped_u;
                    } else {
                        u = 0;
                        du = parts[i].w - 2;
                    }
                }
                setUV4(poly, u, v, u + du, v, u, v + dv, u + du, v + dv);
                if ((sprite->render.word >> 27) & 1) {
                    addPrim(ot - group, poly);
                } else {
                    addPrim(ot, poly);
                }
            }
        }
    }
}

/* 8001E9BC: Draw a sprite's shadow: its parts as black quads (POLY_FT4 from the queue
 * block) flattened onto its floor height, the view matrix scaled by the
 * sprite scale (half height), linked at `ot`. */
void sprite_draw_shadow(Sprite *sprite, u_long *ot) {
    MATRIX m;
    SVECTOR position;
    VECTOR view;
    VECTOR scale;
    long depth;
    long flag;
    SpritePart *parts;
    s32 count;
    s32 group;
    s32 i;
    u8 visible;
    u32 part_flags;
    u32 render;
    POLY_FT4 *poly;
    s16 w, h, x, y;

    m = sprite_view_matrix;
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    scale.vx = sprite->scale;
    scale.vy = sprite->scale / 2;
    scale.vz = 0;
    ScaleMatrixL(&m, &scale);
    position.vy = sprite->ground;
    ApplyMatrix(&sprite_view_matrix, &position, &view);
    m.t[0] += view.vx;
    m.t[1] += view.vy;
    m.t[2] += view.vz;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    count = (u8)sprite->flags >> 2;
    group = -1;
    parts = sprite->renderer->parts[1];
    if ((u8 *)sprite_queue_next_free + count * sizeof(POLY_FT4) < sprite_queue_block_end) {
        for (i = 0; i != (u8)sprite->flags >> 2; i++) {
            part_flags = parts[i].flags;
            if (group != (part_flags & 7)) {
                group = part_flags & 7;
                visible = (sprite_halfword_bit_masks[group] & ((u8 *)&sprite->render)[1]) == 0;
            }
            if (visible) {
                w = parts[i].w + (s8)parts[i].byte8;
                h = parts[i].h + (s8)parts[i].byte9;
                w <<= (sprite->flags >> 8) & 0x1F;
                h <<= (sprite->flags >> 8) & 0x1F;
                x = parts[i].x << ((sprite->flags >> 8) & 0x1F);
                y = parts[i].y << ((sprite->flags >> 8) & 0x1F);
                render = sprite->render.word;
                if ((render >> 3) & 1) {
                    w = -w;
                    x = -x;
                }
                if (((render >> 4) & 1) != ((part_flags >> 5) & 1)) {
                    h = -h;
                    y = -y;
                }
                if (!((part_flags >> 4) & 1)) {
                    sprite_shadow_corners[0].vx = x;
                    sprite_shadow_corners[1].vx = x + w;
                    sprite_shadow_corners[2].vx = x + w;
                    sprite_shadow_corners[3].vx = x;
                } else {
                    sprite_shadow_corners[0].vx = x + w;
                    sprite_shadow_corners[1].vx = x;
                    sprite_shadow_corners[2].vx = x;
                    sprite_shadow_corners[3].vx = x + w;
                }
                if (!((parts[i].flags >> 5) & 1)) {
                    sprite_shadow_corners[2].vz = y + h;
                    sprite_shadow_corners[3].vz = y + h;
                    sprite_shadow_corners[0].vz = y;
                    sprite_shadow_corners[1].vz = y;
                } else {
                    sprite_shadow_corners[2].vz = y;
                    sprite_shadow_corners[3].vz = y;
                    sprite_shadow_corners[0].vz = y + h;
                    sprite_shadow_corners[1].vz = y + h;
                }
                poly = (POLY_FT4 *)sprite_queue_next_free;
                sprite_queue_next_free = (SpriteQueueEntry *)(poly + 1);
                setlen(poly, 9);
                poly->code = 0x2C;
                poly->r0 = 0;
                poly->g0 = 0;
                poly->b0 = 0;
                RotAverage4(&sprite_shadow_corners[0], &sprite_shadow_corners[1], &sprite_shadow_corners[2], &sprite_shadow_corners[3], (long *)&poly->x0,
                            (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &depth, &flag);
                poly->y0 = poly->y1 = (s16)(poly->y0 + poly->y1) / 2;
                poly->y2 = poly->y3 = (s16)(poly->y2 + poly->y3) / 2;
                poly->tpage = parts[i].tpage;
                poly->clut = parts[i].clut;
                poly->u0 = parts[i].u;
                poly->v0 = parts[i].v;
                poly->u1 = parts[i].w + (u8)(parts[i].u - 1);
                poly->v1 = parts[i].v;
                poly->u2 = parts[i].u;
                poly->v2 = parts[i].h + (u8)(parts[i].v - 1);
                poly->u3 = parts[i].w + (u8)(parts[i].u - 1);
                poly->v3 = parts[i].h + (u8)(parts[i].v - 1);
                addPrim(ot, poly);
            }
        }
    }
}

/* 8001EE68: Whether a frame table entry takes its image from the sequencer (second byte bit 7). */
s32 sprite_is_cell_directory(u8 *frame) {
    return frame[1] >> 7;
}

/* 8001EE74: The part count of a frame header (bits 9-14). */
s32 sprite_get_part_count(u16 *header) {
    return (*header >> 9) & 0x3F;
}

/* 8001EE88: Draw a sprite's parts as textured quads (POLY_FT4 from the queue block)
 * cut off below `height` (in the parts' units before the sprite's shift):
 * parts entirely past it are skipped, parts crossing it lose the rows past
 * it, texture included. Linked at `ot`. */
void sprite_draw_parts_cut_below(Sprite *sprite, u_long *ot, s32 height) {
    u32 flags;
    s32 count;
    SpritePart *parts;
    s32 i;
    POLY_FT4 *poly;
    s32 w, h, x, y;
    s32 top, bottom;
    s32 cut;
    long depth;
    long flag;
    u8 u;
    s32 v, du, dv;

    height <<= (sprite->flags >> 8) & 0x1F;
    count = (sprite->flags >> 2) & 0x3F;
    parts = sprite->renderer->parts[1];
    if ((u8 *)sprite_queue_next_free + count * sizeof(POLY_FT4) < sprite_queue_block_end) {
        for (i = 0; i != ((sprite->flags >> 2) & 0x3F); i++) {
            poly = (POLY_FT4 *)sprite_queue_next_free;
            sprite_queue_next_free = (SpriteQueueEntry *)(poly + 1);
            setlen(poly, 9);
            *(u32 *)&poly->r0 = parts[i].colour;
            poly->tpage = parts[i].tpage;
            poly->clut = parts[i].clut;
            x = parts[i].x;
            w = parts[i].w + (s8)parts[i].byte8;
            y = parts[i].y;
            h = parts[i].h + (s8)parts[i].byte9;
            w <<= (sprite->flags >> 8) & 0x1F;
            h <<= (sprite->flags >> 8) & 0x1F;
            x <<= (sprite->flags >> 8) & 0x1F;
            y <<= (sprite->flags >> 8) & 0x1F;
            if ((sprite->render.word >> 3) & 1) {
                w = -w;
                x = -x;
            }
            if ((sprite->render.word >> 4) & 1) {
                h = -h;
                y = -y;
            }
            if (!((parts[i].flags >> 4) & 1)) {
                sprite_quad_corners[0].vx = x;
                sprite_quad_corners[1].vx = x + w;
                sprite_quad_corners[2].vx = x + w;
                sprite_quad_corners[3].vx = x;
            } else {
                sprite_quad_corners[0].vx = x + w;
                sprite_quad_corners[1].vx = x;
                sprite_quad_corners[2].vx = x;
                sprite_quad_corners[3].vx = x + w;
            }
            if (h > 0) {
                top = y;
                bottom = y + h;
            } else {
                top = y + h;
                bottom = y;
            }
            if (height < top) {
                continue;
            }
            cut = 0;
            if (height < bottom) {
                cut = bottom - height;
            }
            h -= cut;
            if (h < 0) {
                y -= cut;
            }
            if (!((parts[i].flags >> 5) & 1)) {
                sprite_quad_corners[0].vy = y;
                sprite_quad_corners[1].vy = y;
                sprite_quad_corners[2].vy = y + h;
                sprite_quad_corners[3].vy = y + h;
            } else {
                sprite_quad_corners[0].vy = y + h;
                sprite_quad_corners[1].vy = y + h;
                sprite_quad_corners[2].vy = y;
                sprite_quad_corners[3].vy = y;
            }
            RotAverage4(&sprite_quad_corners[0], &sprite_quad_corners[1], &sprite_quad_corners[2], &sprite_quad_corners[3], (long *)&poly->x0,
                        (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &depth, &flag);
            cut >>= (sprite->flags >> 8) & 0x1F;
            v = parts[i].v;
            if (h > 0) {
                dv = parts[i].h;
                dv -= cut;
            } else {
                v -= cut;
                dv = parts[i].h;
                dv -= cut;
            }
            u = parts[i].u;
            du = parts[i].w;
            if (poly->x3 < poly->x0) {
                if (u - 1 >= 0) {
                    u--;
                } else {
                    u = 0;
                    du--;
                }
            }
            setUV4(poly, u, v, u + du, v, u, v + dv, u + du, v + dv);
            addPrim(ot, poly);
        }
    }
}

/* 8001F1D4: Draw a sprite's parts as 8001ee88 does, cut off above `height` instead:
 * parts entirely above it are skipped, parts crossing it lose the rows
 * above it, texture included. Linked at `ot`. */
void sprite_draw_parts_cut_above(Sprite *sprite, u_long *ot, s32 height) {
    u32 flags;
    s32 count;
    SpritePart *parts;
    s32 i;
    POLY_FT4 *poly;
    s32 w, h, x, y;
    s32 top, bottom;
    s32 cut;
    long depth;
    long flag;
    u16 u;
    s32 v, du, dv;

    height <<= (sprite->flags >> 8) & 0x1F;
    count = (sprite->flags >> 2) & 0x3F;
    parts = sprite->renderer->parts[1];
    if ((u8 *)sprite_queue_next_free + count * sizeof(POLY_FT4) < sprite_queue_block_end) {
        for (i = 0; i != ((sprite->flags >> 2) & 0x3F); i++) {
            poly = (POLY_FT4 *)sprite_queue_next_free;
            sprite_queue_next_free = (SpriteQueueEntry *)(poly + 1);
            setlen(poly, 9);
            *(u32 *)&poly->r0 = parts[i].colour;
            poly->tpage = parts[i].tpage;
            poly->clut = parts[i].clut;
            x = parts[i].x;
            w = parts[i].w + (s8)parts[i].byte8;
            y = parts[i].y;
            h = parts[i].h + (s8)parts[i].byte9;
            w <<= (sprite->flags >> 8) & 0x1F;
            h <<= (sprite->flags >> 8) & 0x1F;
            x <<= (sprite->flags >> 8) & 0x1F;
            y <<= (sprite->flags >> 8) & 0x1F;
            if ((sprite->render.word >> 3) & 1) {
                w = -w;
                x = -x;
            }
            if ((sprite->render.word >> 4) & 1) {
                h = -h;
                y = -y;
            }
            if (!((parts[i].flags >> 4) & 1)) {
                sprite_quad_corners[0].vx = x;
                sprite_quad_corners[1].vx = x + w;
                sprite_quad_corners[2].vx = x + w;
                sprite_quad_corners[3].vx = x;
            } else {
                sprite_quad_corners[0].vx = x + w;
                sprite_quad_corners[1].vx = x;
                sprite_quad_corners[2].vx = x;
                sprite_quad_corners[3].vx = x + w;
            }
            if (h > 0) {
                top = y;
                bottom = y + h;
            } else {
                top = y + h;
                bottom = y;
            }
            if (bottom < height) {
                continue;
            }
            cut = 0;
            if (top < height) {
                cut = height - top;
            }
            if (h > 0) {
                y += cut;
                h -= cut;
            } else {
                y += cut;
                h += cut;
            }
            if (!((parts[i].flags >> 5) & 1)) {
                sprite_quad_corners[0].vy = y;
                sprite_quad_corners[1].vy = y;
                sprite_quad_corners[2].vy = y + h;
                sprite_quad_corners[3].vy = y + h;
            } else {
                sprite_quad_corners[0].vy = y + h;
                sprite_quad_corners[1].vy = y + h;
                sprite_quad_corners[2].vy = y;
                sprite_quad_corners[3].vy = y;
            }
            RotAverage4(&sprite_quad_corners[0], &sprite_quad_corners[1], &sprite_quad_corners[2], &sprite_quad_corners[3], (long *)&poly->x0,
                        (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &depth, &flag);
            cut >>= (sprite->flags >> 8) & 0x1F;
            if (h > 0) {
                v = parts[i].v;
                v += cut;
                dv = parts[i].h;
            } else {
                v = parts[i].v;
                v -= cut;
                dv = parts[i].h;
            }
            dv -= cut;
            u = parts[i].u;
            du = parts[i].w;
            if (poly->x3 < poly->x0) {
                s16 column = u;

                column--;
                if (column >= 0) {
                    u = column;
                } else {
                    u = 0;
                    du--;
                }
            }
            setUV4(poly, u, v, u + du, v, u, v + dv, u + du, v + dv);
            addPrim(ot, poly);
        }
    }
}

/* 8001F530: Reserve `width` columns of the sprite texture area (three 64-line rows from
 * (0x300, 0x140), 0x40 columns each) and return their position. */
DVECTOR sprite_reserve_texture_columns(s32 width) {
    DVECTOR position;

    if (sprite_texture_area_column + width > 0x40) {
        sprite_texture_area_column = 0;
        if (++sprite_texture_area_row >= 3) {
            sprite_texture_area_row = 0;
        }
    }
    position.vx = sprite_texture_area_column + 0x300;
    position.vy = sprite_texture_area_row * 64 + 0x140;
    sprite_texture_area_column += width;
    return position;
}

/* 8001F5BC: A sprite's extent (width, height, depth) at its scale, from the frame
 * record its first animation's byte 4 selects (less one; the first record
 * when the directory entry is below the index). */
void sprite_get_extent(Sprite *sprite, s32 unused, s32 *width, s32 *height, s32 *depth) {
    SpriteSource *source = sprite->image;
    u8 *block = (u8 *)(source->animations[1] + (s32)source->animations);
    u8 *animation = (u8 *)(((u16 *)block)[2] + (s32)block);
    s32 index = animation[4];
    u16 *frames = source->frames;
    u8 *record;

    if (index != 0) {
        index--;
    }
    if (frames[index] < index) {
        index = 0;
    }
    record = (u8 *)(frames[index + 1] + (s32)frames);
    *height = record[3] * sprite->scale / 4096;
    *depth = record[1] * sprite->scale / 4096;
    *width = record[2] * sprite->scale / 4096;
}

/* 8001F6B0: Recolour a one-sided sprite's parts: the sprite's colour word and blend
 * mode (its blend rate - 1). */
void sprite_recolor_parts(Sprite *sprite) {
    SpriteImageSize size; /* copied, unused */
    SpritePart *part;
    u32 colour;
    s32 blend;
    s32 i;

    if ((sprite->render.word & 3) != 1) {
        return;
    }
    blend = sprite->render.bits.blend;
    if (blend != 0) {
        blend--;
    }
    size = ((SpriteImage *)sprite->image)->size;
    part = sprite->renderer->parts[1];
    colour = *(u32 *)&sprite->red;
    for (i = 0; i != (u8)sprite->flags >> 2; i++) {
        part[i].colour = colour;
        part[i].tpage = (part[i].tpage & 0xFF9F) | (blend << 5);
    }
}

/* 8001F750: Apply the control bytes of frame `frame`'s parts: for each part, bytes
 * with bit 7 set precede it; with bit 6 they set entry (bits 0-2) of the
 * renderer's 0x40-byte block (bit 5: two bytes, bit 4: a depth byte * 16,
 * else depth 0), otherwise bits 0-1 skip a byte each. Parts are 3 bytes (5
 * when the frame's bit 7 is set). */
void sprite_apply_cell_frame_controls(Sprite *sprite, s32 frame, SpriteSource *source) {
    u16 *frames = source->frames;
    u8 *record = (u8 *)(frames[frame] + (s32)frames);
    u8 wide = *record & 0x80;
    s32 count = *record & 0x3F;
    u8 *p = record + (count * 2 + 4);
    s32 i;
    u8 control;
    s32 slot;

    for (i = 0; i != count; i++) {
    next:
        control = *p;
        if (control & 0x80) {
            p++;
            if (control & 0x40) {
                slot = control & 7;
                if (sprite->renderer->pointer34 == NULL) {
                    sprite->renderer->pointer34 = heap_alloc(0x40, 0);
                    sprite_clear_group_entries(sprite);
                }
                if (control & 0x20) {
                    sprite->renderer->pointer34[slot].byte0 = *p++;
                    sprite->renderer->pointer34[slot].byte1 = *p++;
                }
                if (control & 0x10) {
                    s16 depth = *p++ << 4;

                    sprite->renderer->pointer34[slot].half6 = depth;
                } else {
                    sprite->renderer->pointer34[slot].half6 = 0;
                }
            } else {
                if (control & 1) {
                    p++;
                }
                if (control & 2) {
                    p++;
                }
            }
            goto next;
        }
        if (wide) {
            p += 2;
        }
        p += 3;
    }
}

/* 8001F8E8: Apply the control bytes of frame `frame`'s parts (as 8001f750 does, which
 * handles directories with bit 15 set): here the part count is followed by
 * four bytes per part. Frames beyond the directory's count (bits 0-8) are
 * ignored. */
void sprite_apply_frame_controls(Sprite *sprite, s32 frame, SpriteSource *source) {
    u16 *frames = source->frames;
    u8 *record;
    u8 wide;
    s32 count;
    u8 *p;
    s32 i;
    u8 control;
    s32 slot;
    s32 unused[4]; /* the frame reserves 16 bytes no code uses */

    if (frame >= (*frames & 0x1FF) + 1) {
        return;
    }
    if (*frames & 0x8000) {
        sprite_apply_cell_frame_controls(sprite, frame, source);
        return;
    }
    record = (u8 *)(frames[frame] + (s32)frames);
    wide = *record & 0x80;
    count = *record & 0x3F;
    p = record + (count * 4 + 6);
    for (i = 0; i != count; i++) {
    next:
        control = *p;
        if (control & 0x80) {
            p++;
            if (control & 0x40) {
                if (sprite->renderer->pointer34 == NULL) {
                    sprite->renderer->pointer34 = heap_alloc(0x40, 0);
                    sprite_clear_group_entries(sprite);
                }
                slot = control & 7;
                if (control & 0x20) {
                    sprite->renderer->pointer34[slot].byte0 = *p++;
                    sprite->renderer->pointer34[slot].byte1 = *p++;
                }
                if (control & 0x10) {
                    s16 depth = *p++ << 4;

                    sprite->renderer->pointer34[slot].half6 = depth;
                } else {
                    sprite->renderer->pointer34[slot].half6 = 0;
                }
            } else {
                if (control & 1) {
                    p++;
                }
                if (control & 2) {
                    p++;
                }
            }
            goto next;
        }
        if (wide) {
            p += 2;
        }
        p += 3;
    }
}

/* 8001FAB4: Unpack and upload the image at 8004fbd8 to (x, y). */
void sprite_upload_pause_image(s32 x, s32 y) {
    void *image = text_unpack_lzss_alloc(sprite_packed_pause_image, 0);

    model_load_image_list(image, 1, x, y, 0, 0, 0);
    DrawSync(0);
    heap_free(image);
}

/* 8001FB30: Upload the image list at sprite_image_list to sprite_image_list_position, running
 * the upload on an 8 KB heap block as its stack. */
void sprite_upload_image_list(void) {
    u8 *stack = heap_alloc(0x2000, 1);

    STACK_ENTER(stack + 0x1F00);
    model_load_image_list(sprite_image_list, 1, sprite_image_list_position.vx, sprite_image_list_position.vy, 0, 0, 0);
    STACK_LEAVE();
    heap_free(stack);
}

/* 8001FBA4: The operand a script byte names: a frame table entry (bit 7 set) or a byte on the sprite's stack. */
u8 *sprite_vm_resolve_variable(Sprite *sprite, u8 *code) {
    u8 *operand;
    s32 offset;

    if (*code & 0x80) {
        operand = sprite->frames + (*code & 0x7F);
    } else {
        offset = (s8)*code;
        operand = &sprite->stack[sprite->stack_top + offset];
    }
    return operand;
}

/* Apply the script's direction offset, in units of sixteen angle steps. */
#define SPRITE_OFFSET_DIRECTION(sprite_, offset_) \
    do { \
        sprite_set_direction((sprite_), (sprite_)->direction + (offset_) * 16); \
    } while (0)

/* 8001FBE4: Run the script command `op` (0x8a-0xfc) of a sprite on its operand bytes
 * `code`: motion, placement, colour, renderer angles and scales, byte
 * arithmetic on the sprite's stack and frame variables, sounds, models. */
void sprite_vm_run_generic_command(Sprite *sprite, u8 op, u8 *code) {
    SVECTOR vector;
    VECTOR sum;
    SVECTOR angles;
    MATRIX m;
    DVECTOR from;
    DVECTOR to;
    long flag;
    Sprite *other;
    u8 *stack;
    s32 n;
    s32 count;
    s32 distance;
    s32 angle;
    s32 value;
    u16 bits;
    s32 x, y;
    s32 group;
    u8 arg;
    u8 transform;
    u8 *p;
    SpriteModelRenderer *model;
    SpriteModel *loaded; /* never set: 0xf7 clears a word through whatever the register holds */
    s32 buffer;
    s16 direction;

    switch (op) {
    /* 8d: model texture pages take the page at the image origin (8002cc10). */
    case 0x8D:
        /* called without a prototype: the coordinates pass as ints */
        model_set_tpage_override(((SpriteSource *)sprite->image)->origin.vx, ((SpriteSource *)sprite->image)->origin.vy);
        break;
    /* c6 u8: sequencer value (+c) = u8 when the sequencer is the sprite's own. */
    case 0xC6:
        if (sprite->frame_bits.sequencer_owned == 1) {
            ((SpriteSequencer *)sprite->sequencer)->halfc = code[0];
        }
        break;
    /* c9 s16: sequencer value (+c) = s16 when the sequencer is the sprite's own. */
    case 0xC9: {
        s32 value;

        if (sprite->frame_bits.sequencer_owned == 1) {
            value = code[0] | (s16)(code[1] << 8);
            ((SpriteSequencer *)sprite->sequencer)->halfc = value;
        }
        break;
    }
    /* c5 u8: move horizontally (80022cdc) u8 / (skip + 1) times. */
    case 0xC5: {
        s32 n;

        n = code[0];
        n /= sprite_frame_skip + 1;
        while (--n != -1) {
            sprite_move(sprite);
        }
        break;
    }
    /* b9 u8: play sound u8 of the sprite's own bank (+50), if it has one. */
    case 0xB9:
        if ((SpriteVoice *)sprite->word50 != NULL) {
            sound_play_effect(code[0] | (((SpriteVoice *)sprite->word50)->bank << 16));
        }
        break;
    /* b0 u8: play sound u8 of the scripts' bank (8005919c), if any. */
    case 0xB0:
        if (sprite_script_sound_bank != NULL) {
            sound_play_effect(code[0] | (sprite_script_sound_bank->bank << 16));
        }
        break;
    /* cc s16: variable table (+88) = this command + s16. */
    case 0xCC:
        sprite->frames = sprite->script + ((s16)(code[1] << 8) | code[0]);
        break;
    /* 8c: turn (direction and facing) to the target (+74) on the ground plane. */
    case 0x8C:
        other = sprite->partner;
        from.vx = sprite->x >> 16;
        from.vy = sprite->z >> 16;
        to.vx = other->x >> 16;
        to.vy = other->z >> 16;
        direction = sprite_get_ground_direction(to, from);
        sprite_set_direction(sprite, direction);
        sprite_set_facing(sprite, direction);
        break;
    /* 94: model sprites (render kind 2): renderer y angle = the creator's direction. */
    case 0x94:
        other = sprite->parent;
        if ((sprite->render.word & 3) == 2) {
            sprite->renderer->angle_y = other->direction;
            sprite->render.bits.dirty = 1;
        }
        break;
    /* a7 u8 (c8 only: the interpreters run a7 themselves): bit 7 pauses the main task list
     * for (u8 & 7f) + 1 frames, else wait u8 + 1 frames (scaled, at least 1). */
    case 0xA7: {
        s32 n;

        if (code[0] & 0x80) {
            n = (code[0] & 0x7F) + 1;
            task_main_pause_timer = n;
        } else {
            n = (code[0] + 1) * sprite->motion.bits.divisor / 256;
            if (n == 0) {
                n = 1;
            }
            sprite->countdown += n;
        }
        break;
    }
    /* fc s24: upload the image list at the operand + s24 at the image origin (8001fb30, on
     * an 8 KB heap stack). */
    case 0xFC:
        stack = heap_alloc(0x2000, 0);
        STACK_ENTER(stack + 0x1F00);
        {
            s32 image_x, image_y;

            sprite_image_list = (s32 *)((((s8)code[2] << 16) + (code[1] << 8) + code[0]) + (s32)code);
            image_x = ((SpriteSource *)sprite->image)->origin.vx;
            image_y = ((SpriteSource *)sprite->image)->origin.vy;
            sprite_image_list_position.vx = image_x;
            sprite_image_list_position.vy = image_y;
        }
        sprite_upload_image_list();
        STACK_LEAVE();
        heap_free(stack);
        break;
    /* bf u8: height (+36) = u8. */
    case 0xBF:
        sprite->height = code[0];
        break;
    /* 96: destroy the tasks the sprite's block created (8001ce74). */
    case 0x96:
        task_destroy_owned_by(sprite->block);
        break;
    /* a2 u8: render byte 1 = u8: the part groups not drawn (their sprite_halfword_bit_masks bits). */
    case 0xA2:
        ((u8 *)&sprite->render)[1] = code[0];
        break;
    /* cd u16: with a renderer, x angle (part group bits 9-11: that group's +2) set (bit 12)
     * or added: bits 0-8 * 8. */
    case 0xCD: {
        s32 value;
        u16 bits;
        s32 angle;
        s32 group;

        if (sprite->renderer != NULL) {
            value = code[0] | (s16)(code[1] << 8);
            bits = value;
            angle = (value & 0x1FF) << 3;
            group = (bits >> 9) & 7;
            if (!((bits >> 12) & 1)) {
                if (group != 0) {
                    if (sprite->renderer->pointer34 != NULL) {
                        sprite->renderer->pointer34[group].half2 += angle;
                    }
                } else {
                    sprite->renderer->angle_x += angle;
                    sprite->render.bits.dirty = 1;
                }
            } else if (group != 0) {
                if (sprite->renderer->pointer34 != NULL) {
                    sprite->renderer->pointer34[group].half2 = angle;
                }
            } else {
                sprite->renderer->angle_x = angle;
                sprite->render.bits.dirty = 1;
            }
        }
        break;
    }
    /* ce u16: as cd for the y angle (group +4), negated when mirrored. */
    case 0xCE: {
        s32 value;
        u16 bits;
        s32 angle;
        s32 group;

        value = code[0] | (s16)(code[1] << 8);
        bits = value;
        if (sprite->renderer != NULL) {
            angle = (value & 0x1FF) * 8;
            group = (bits >> 9) & 7;
            if ((sprite->motion.word >> 2) & 1) {
                angle = -angle;
            }
            if (!((bits >> 12) & 1)) {
                if (group != 0) {
                    if (sprite->renderer->pointer34 != NULL) {
                        sprite->renderer->pointer34[group].half4 += angle;
                    }
                } else {
                    sprite->renderer->angle_y += angle;
                    sprite->render.bits.dirty = 1;
                }
            } else if (group != 0) {
                if (sprite->renderer->pointer34 != NULL) {
                    sprite->renderer->pointer34[group].half4 = angle;
                }
            } else {
                sprite->renderer->angle_y = angle;
                sprite->render.bits.dirty = 1;
            }
        }
        break;
    }
    /* cf u16: as cd for the z angle (group +6), negated when mirrored. */
    case 0xCF: {
        s32 value;
        u16 bits;
        s32 angle;
        s32 group;

        value = code[0] | (s16)(code[1] << 8);
        bits = value;
        if (sprite->renderer != NULL) {
            angle = (value & 0x1FF) * 8;
            group = (bits >> 9) & 7;
            if ((sprite->motion.word >> 2) & 1) {
                angle = -angle;
            }
            if (!((bits >> 12) & 1)) {
                if (group != 0) {
                    if (sprite->renderer->pointer34 != NULL) {
                        sprite->renderer->pointer34[group].half6 += angle;
                    }
                } else {
                    sprite->renderer->angle_z += angle;
                    sprite->render.bits.dirty = 1;
                }
            } else if (group != 0) {
                if (sprite->renderer->pointer34 != NULL) {
                    sprite->renderer->pointer34[group].half6 = angle;
                }
            } else {
                sprite->renderer->angle_z = angle;
                sprite->render.bits.dirty = 1;
            }
        }
        break;
    }
    /* c0 u8: move a random distance below u8 (scaled) in a random ground direction. */
    case 0xC0: {
        s32 distance;
        s32 angle;

        distance = rand() & 0xFF;
        distance *= code[0];
        distance >>= 8;
        distance = distance * sprite->scale / 4096;
        angle = rand();
        sprite->x += sprite_scale_by_rate(sprite, gpu_get_cos(angle)) * distance * 16;
        sprite->z -= sprite_scale_by_rate(sprite, gpu_get_sin(angle)) * distance * 16;
        break;
    }
    /* c1 u8: move to a random point a random distance below u8 (scaled) away. */
    case 0xC1: {
        s32 distance;

        distance = rand() & 0xFF;
        distance *= code[0];
        distance >>= 8;
        distance = distance * sprite->scale / 4096;
        sprite_set_svector(&vector, sprite_scale_by_rate(sprite, distance), 0, 0);
        sprite_set_svector(&angles, rand(), rand(), 0);
        sprite_set_vector(&sum, sprite->x >> 16, sprite->y >> 16, sprite->z >> 16);
        TransMatrix(&m, &sum);
        SetTransMatrix(&m);
        gpu_build_rotation_matrix(&angles, &m);
        SetRotMatrix(&m);
        RotTransSV(&vector, &vector, &flag);
        sprite->x = vector.vx << 16;
        sprite->y = vector.vy << 16;
        sprite->z = vector.vz << 16;
        break;
    }
    /* bc sel: bit 7 set: place at selector bits 0-5 (cases below), into the target (+a0)
     * with bit 6, else the position, view-transformed with render bit 24 except for 22-24
     * and 36-37; bit 7 clear: at the creator's part offset sel (one-sided). */
    case 0xBC: {
        u8 arg;
        u8 transform;
        s32 index;

        arg = code[0];
        index = arg & 0x3F;
        if (arg & 0x80) {
            transform = sprite->render.bits.no_view;
            switch (index) {
            /* 38: the formation place of its side and slot, y 0. */
            case 38: {
                u32 slot, side;

                slot = sprite->frame_bits.unknown30;
                side = sprite->motion.word & 3;
                vector.vx = battle_area.slots[(side << 2) | slot].x;
                slot = sprite->frame_bits.unknown30;
                side = sprite->motion.word & 3;
                vector.vz = battle_area.slots[(side << 2) | slot].z;
                vector.vy = 0;
                break;
            }
            /* 36: set the block task's link bit 30, then its own position. */
            case 36:
                ((Task *)sprite->block)->link.word |= 0x40000000;
                goto own_position;
            /* 37: clear that bit, then its own position. */
            case 37:
                ((Task *)sprite->block)->link.word &= ~0x40000000;
                goto own_position;
            /* 23: twice the screen centre's offset from the geometry offset, its own z. */
            case 23:
                ReadGeomOffset(&sum.vx, &sum.vy);
                vector.vx = (0xA0 - sum.vx) * 2;
                vector.vy = (0x70 - sum.vy) * 2;
                transform = 0;
                vector.vz = sprite->z >> 16;
                break;
            /* 24: its own position. */
            case 24:
            own_position:
                vector.vx = sprite->x >> 16;
                vector.vy = sprite->y >> 16;
                transform = 0;
                vector.vz = sprite->z >> 16;
                break;
            /* 22: the creator's position. */
            case 22:
                other = sprite->parent;
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                transform = 0;
                vector.vz = other->z >> 16;
                break;
            /* 32-35: as 18-21 for the acting sprite (800c3e1c). */
            case 32:
                other = battle_acting_sprite;
                goto focus_top;
            case 33:
                other = battle_acting_sprite;
                goto focus_middle;
            case 34:
                other = battle_acting_sprite;
                goto focus_depth;
            case 35:
                other = battle_acting_sprite;
                goto focus_half_depth;
            /* 18: the target's top (y - height). */
            case 18:
                other = sprite->partner;
            focus_top:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->height;
                break;
            /* 19: the target's middle (y - height + (height - depth) / 2). */
            case 19:
                other = sprite->partner;
            focus_middle:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->height - (other->height - other->extent_depth) / 2;
                break;
            /* 20: the target's y - depth. */
            case 20:
                other = sprite->partner;
            focus_depth:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->extent_depth;
                break;
            /* 21: the target's y - (depth - depth / 2). */
            case 21:
                other = sprite->partner;
            focus_half_depth:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->extent_depth - (u16)other->extent_depth / 2;
                break;
            /* 6, 7: the points 8006f99c and 8006f9ac. */
            case 6:
                vector.vx = sprite_camera_eye.vx >> 16;
                vector.vy = sprite_camera_eye.vy >> 16;
                vector.vz = sprite_camera_eye.vz >> 16;
                break;
            case 7:
                vector.vx = sprite_camera_look_at.vx >> 16;
                vector.vy = sprite_camera_look_at.vy >> 16;
                vector.vz = sprite_camera_look_at.vz >> 16;
                break;
            /* 1: the acting sprite's position. */
            case 1:
                other = battle_acting_sprite;
                goto focus_position;
            /* 9, 8, 10: the target's part offsets 1, 2 and 3. */
            case 9:
                index = 11;
                goto own_group;
            case 8:
                index = 12;
                goto own_group;
            case 10:
                index = 13;
                goto own_group;
            /* 11-17: the creator's part offset 1-7 (none without a creator). */
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
                other = sprite->parent;
                if (other == NULL) {
                    break;
                }
                goto group_place;
            own_group:
                other = sprite->partner;
            group_place:
                if (other->renderer == NULL) {
                    break;
                }
                if ((other->render.word & 3) != 1) {
                    break;
                }
                if (other->renderer->pointer34 != NULL) {
                    s32 entry = index - 10;

                    x = other->renderer->pointer34[entry].byte0;
                    y = other->renderer->pointer34[entry].byte1;
                } else {
                    x = 0;
                    y = 0;
                }
                if ((other->motion.word >> 2) & 1) {
                    x = -x;
                }
                x = x * other->scale / 4096;
                y = y * other->scale / 4096;
                sprite_set_svector(&vector, x + (other->x >> 16), y + (other->y >> 16), other->z >> 16);
                break;
            /* 25-31: the acting sprite's part offsets 1-7. */
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
                other = battle_acting_sprite;
                index -= 14;
                goto group_place;
            /* 0: the target's position. */
            case 0:
                other = sprite->partner;
            focus_position:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                break;
            /* 2: the centre of the group 800d363c. */
            case 2: {
                s32 members;

                sprite_set_vector(&sum, 0, 0, 0);
                for (members = 0; (other = battle_area_event_target_sprites[members]) != NULL; members++) {
                    sum.vx += other->x;
                    sum.vy += other->y;
                    sum.vz += other->z;
                }
                sum.vx /= members;
                sum.vy /= members;
                sum.vz /= members;
                vector.vx = sum.vx >> 16;
                vector.vy = sum.vy >> 16;
                vector.vz = sum.vz >> 16;
                break;
            }
            /* 3: halfway to the acting sprite. */
            case 3:
                other = battle_acting_sprite;
                sum.vx = other->x;
                sum.vy = other->y;
                sum.vz = other->z;
                sum.vx += sprite->x;
                sum.vy += sprite->y;
                sum.vz += sprite->z;
                sum.vx /= 2;
                sum.vy /= 2;
                sum.vz /= 2;
                vector.vx = sum.vx >> 16;
                vector.vy = sum.vy >> 16;
                vector.vz = sum.vz >> 16;
                break;
            /* 4: the centre of the group and this sprite. */
            case 4: {
                s32 members;

                sprite_set_vector(&sum, 0, 0, 0);
                for (members = 0; (other = battle_area_event_target_sprites[members]) != NULL; members++) {
                    sum.vx += other->x;
                    sum.vy += other->y;
                    sum.vz += other->z;
                }
                members++;
                sum.vx += sprite->x;
                sum.vy += sprite->y;
                sum.vz += sprite->z;
                sum.vx /= members;
                sum.vy /= members;
                sum.vz /= members;
                vector.vx = sum.vx >> 16;
                vector.vy = sum.vy >> 16;
                vector.vz = sum.vz >> 16;
                break;
            }
            /* 5: a zero sum divided by an unset count. */
            case 5:
                sprite_set_vector(&sum, 0, 0, 0);
                sum.vx /= count;
                sum.vy /= count;
                sum.vz /= count;
                vector.vx = sum.vx >> 16;
                vector.vy = sum.vy >> 16;
                vector.vz = sum.vz >> 16;
                break;
            }
            if (transform) {
                ApplyMatrixSV(&sprite_view_matrix, &vector, &vector);
                vector.vx += sprite_view_matrix.t[0];
                vector.vy += sprite_view_matrix.t[1];
                vector.vz += sprite_view_matrix.t[2];
            }
            if (arg & 0x40) {
                sprite->target_x = vector.vx;
                sprite->target_y = vector.vy;
                sprite->target_z = vector.vz;
            } else {
                sprite->x = vector.vx << 16;
                sprite->y = vector.vy << 16;
                sprite->z = vector.vz << 16;
            }
        } else {
            s32 dx, dy;

            other = sprite->parent;
            if (other != NULL && other->renderer != NULL && (other->render.word & 3) == 1) {
                if (other->renderer->pointer34 != NULL) {
                    dy = other->renderer->pointer34[arg].byte1;
                    dx = other->renderer->pointer34[arg].byte0;
                } else {
                    dx = 0;
                    dy = 0;
                }
                if ((other->render.word >> 3) & 1) {
                    dx = -dx;
                }
                dy = (dy * other->scale / 4096) << 16;
                dx = (dx * other->scale / 4096) << 16;
                sprite->z = other->z;
                sprite->x = other->x + dx;
                sprite->y = other->y + dy;
            }
        }
        break;
    }
    /* d1 var var: a *= b (bytes, 8001fba4). */
    case 0xD1:
        *sprite_vm_resolve_variable(sprite, code) *= *sprite_vm_resolve_variable(sprite, code + 1);
        break;
    /* d2, d5 var var: a /= b. */
    case 0xD2:
    case 0xD5:
        *sprite_vm_resolve_variable(sprite, code) /= *sprite_vm_resolve_variable(sprite, code + 1);
        break;
    /* e5 var u8: a = (rand & ff) * u8 >> 8. */
    case 0xE5:
        p = sprite_vm_resolve_variable(sprite, code);
        *p = (s32)((rand() & 0xFF) * code[1]) >> 8;
        break;
    /* d6 var u8: a += u8. */
    case 0xD6:
        *sprite_vm_resolve_variable(sprite, code) += code[1];
        break;
    /* d7 var s8: a *= s8. */
    case 0xD7:
        *sprite_vm_resolve_variable(sprite, code) *= (s8)code[1];
        break;
    /* d8 var s8: a /= s8. */
    case 0xD8:
        *sprite_vm_resolve_variable(sprite, code) /= (s8)code[1];
        break;
    /* d9 var s8: a <<= s8. */
    case 0xD9:
        *sprite_vm_resolve_variable(sprite, code) <<= (s8)code[1];
        break;
    /* da var s8: a >>= s8, signed. */
    case 0xDA:
        *(s8 *)sprite_vm_resolve_variable(sprite, code) >>= (s8)code[1];
        break;
    /* db var s8: the halfword at a <<= s8. */
    case 0xDB:
        {
            u8 *half = sprite_vm_resolve_variable(sprite, code);
            s32 value;

            value = ((half[1] << 8) | half[0]) << (s8)code[1];
            half[0] = value;
            half[1] = value >> 8;
        }
        break;
    /* dc var s8: the halfword at a >>= s8. */
    case 0xDC:
        {
            u8 *half = sprite_vm_resolve_variable(sprite, code);
            s32 value;

            value = ((half[1] << 8) | half[0]) >> (s8)code[1];
            half[0] = value;
            half[1] = value >> 8;
        }
        break;
    /* d0, d3, dd, de var var: a += b. */
    case 0xD0:
    case 0xD3:
    case 0xDD:
    case 0xDE:
        *sprite_vm_resolve_variable(sprite, code) += *sprite_vm_resolve_variable(sprite, code + 1);
        break;
    /* a4 s8: the target (+74) plays animation s8 (800245d8). */
    case 0xA4:
        sprite_start_animation(sprite->partner, (s8)code[0]);
        break;
    /* df var u8: a = u8. */
    case 0xDF:
        *sprite_vm_resolve_variable(sprite, code) = code[1];
        break;
    /* e6 var u8: the halfword at a = u8. */
    case 0xE6:
        {
            u8 *half = sprite_vm_resolve_variable(sprite, code);

            half[0] = code[1];
            half[1] = 0;
        }
        break;
    /* 91: coloured parts (colour flag off, 8001f6b0). */
    case 0x91:
        sprite->colour_flags &= ~1;
        sprite_recolor_parts(sprite);
        break;
    /* 92: uncoloured parts (colour flag on, 8001f6b0). */
    case 0x92:
        sprite->colour_flags |= 1;
        sprite_recolor_parts(sprite);
        break;
    /* bb s8: depth bias (+30) += s8. */
    case 0xBB:
        sprite->half30 += (s8)code[0];
        break;
    /* 93: with a creator and a render kind: type e (flags bits 13-16) unless its frames
     * come from the sequencer; with a renderer, the creator's eight part-group entries
     * and screen offset; then show the frame again (8001d2b0). */
    case 0x93: {
        s32 i;

        other = sprite->parent;
        if (other != NULL && (sprite->render.word & 3)) {
            if (sprite_is_cell_directory((u8 *)((SpriteSource *)sprite->image)->frames) == 0) {
                sprite->flags = (sprite->flags & ~0x1E000) | 0x1C000;
            }
            if (sprite->renderer != NULL && other->renderer->pointer34 != NULL) {
                sprite_alloc_group_entries(sprite);
                for (i = 0; i != 8; i++) {
                    sprite->renderer->pointer34[i] = other->renderer->pointer34[i];
                }
                sprite->renderer->offset = other->renderer->offset;
            }
            sprite_request_frame(sprite, sprite->frame);
        }
        break;
    }
    /* ba u8: blend rate u8 (80023290). */
    case 0xBA:
        sprite_set_blend_rate(sprite, code[0]);
        break;
    /* f1 r g b: colour = r, g, b (model sprites: the model's too); one-sided parts are
     * recoloured. */
    case 0xF1: {
        SpriteModelRenderer *model;

        model = (SpriteModelRenderer *)sprite->renderer;
        sprite->red = code[0];
        sprite->green = code[1];
        sprite->blue = code[2];
        if ((sprite->render.word & 3) == 2) {
            model->red = code[0];
            model->green = code[1];
            model->blue = code[2];
        }
        if ((sprite->render.word & 3) == 1) {
            sprite_recolor_parts(sprite);
        }
        break;
    }
    /* f2 s8 s8 s8: colour += r, g, b, clamped to 0-255; model sprites add to the model's
     * and, as type f with an unlit model, tint it (800b2aec). */
    case 0xF2:
        model = (SpriteModelRenderer *)sprite->renderer;
        sprite->red = sprite_add_clamp_byte(sprite->red, (s8)code[0]);
        sprite->green = sprite_add_clamp_byte(sprite->green, (s8)code[1]);
        sprite->blue = sprite_add_clamp_byte(sprite->blue, (s8)code[2]);
        if ((sprite->render.word & 3) == 2) {
            model->red += (s8)code[0];
            model->green += (s8)code[1];
            model->blue += (s8)code[2];
        }
        if ((sprite->render.word & 3) == 1) {
            sprite_recolor_parts(sprite);
        }
        if (((sprite->flags >> 13) & 0xF) == 0xF && ((SpriteModelRenderer *)sprite->renderer)->model != NULL &&
            !((sprite->flags >> 1) & 1)) {
            battle_tmd_tint_packets(((SpriteModelRenderer *)sprite->renderer)->model,
                          ((SpriteModelRenderer *)sprite->renderer)->packets[0],
                          ((SpriteModelRenderer *)sprite->renderer)->packets[1], model->red, model->green, model->blue);
        }
        break;
    /* 90: rebind the image to the resource block (+4c, b0 bit 10 set) while the animation
     * block (+48) is bound, else back to the animation block. */
    case 0x90:
        if (sprite->resource_block == sprite->animations) {
            sprite_bind_resource(sprite, (s32 *)sprite->resource);
            sprite->b0.wordb0 |= 0x400;
        } else {
            sprite_bind_resource(sprite, sprite->animations);
            sprite->b0.wordb0 &= ~0x400;
        }
        break;
    /* f5 s24: bind the model at the operand + s24 (relocated) with new packet buffers (heap
     * tag 5). */
    case 0xF5: {
        s32 offset;
        ModelBuffer *data;

        heap_select_owner_tag(5, 0);
        offset = (s8)code[2];
        offset <<= 16;
        offset += code[1] << 8;
        offset += code[0];
        offset += (s32)code;
        data = (ModelBuffer *)offset;
        model_relocate_sprite_model((SpriteModel *)offset);
        if (((SpriteModelRenderer *)sprite->renderer)->packets[0] != NULL) {
            heap_free(((SpriteModelRenderer *)sprite->renderer)->packets[0]);
        }
        model_alloc_packet_buffers(data, &((SpriteModelRenderer *)sprite->renderer)->packets[0],
                      &((SpriteModelRenderer *)sprite->renderer)->packets[1]);
        model_build_packets((SpriteModel *)data, (RenderPacket *)((SpriteModelRenderer *)sprite->renderer)->packets[0], 0);
        memcpy(((SpriteModelRenderer *)sprite->renderer)->packets[1],
               ((SpriteModelRenderer *)sprite->renderer)->packets[0], data->size);
        ((SpriteModelRenderer *)sprite->renderer)->model = data;
        break;
    }
    /* f6 s24: as f5 for the model group at the operand + s24, its model 16 bytes on. */
    case 0xF6:
        heap_select_owner_tag(5, 0);
        buffer = (s8)code[2];
        buffer <<= 16;
        buffer += code[1] << 8;
        buffer += code[0];
        buffer += (s32)code;
        model_relocate_group((ModelGroup *)buffer);
        buffer += 0x10;
        if (((SpriteModelRenderer *)sprite->renderer)->packets[0] != NULL) {
            heap_free(((SpriteModelRenderer *)sprite->renderer)->packets[0]);
        }
        model_alloc_packet_buffers((ModelBuffer *)buffer, &((SpriteModelRenderer *)sprite->renderer)->packets[0],
                      &((SpriteModelRenderer *)sprite->renderer)->packets[1]);
        model_build_packets((SpriteModel *)buffer, (RenderPacket *)((SpriteModelRenderer *)sprite->renderer)->packets[0], 0);
        memcpy(((SpriteModelRenderer *)sprite->renderer)->packets[1],
               ((SpriteModelRenderer *)sprite->renderer)->packets[0], ((ModelBuffer *)buffer)->size);
        ((SpriteModelRenderer *)sprite->renderer)->model = (ModelBuffer *)buffer;
        break;
    /* f7 s24: as f6 without the heap tag, then a word is cleared through an unset pointer. */
    case 0xF7:
        buffer = (s8)code[2];
        buffer <<= 16;
        buffer += code[1] << 8;
        buffer += code[0];
        buffer += (s32)code;
        model_relocate_group((ModelGroup *)buffer);
        buffer += 0x10;
        if (((SpriteModelRenderer *)sprite->renderer)->packets[0] != NULL) {
            heap_free(((SpriteModelRenderer *)sprite->renderer)->packets[0]);
        }
        model_alloc_packet_buffers((ModelBuffer *)buffer, &((SpriteModelRenderer *)sprite->renderer)->packets[0],
                      &((SpriteModelRenderer *)sprite->renderer)->packets[1]);
        model_build_packets((SpriteModel *)buffer, (RenderPacket *)((SpriteModelRenderer *)sprite->renderer)->packets[0], 0);
        memcpy(((SpriteModelRenderer *)sprite->renderer)->packets[1],
               ((SpriteModelRenderer *)sprite->renderer)->packets[0], ((ModelBuffer *)buffer)->size);
        *(s32 *)(loaded->unk10 + 4) = 0;
        ((SpriteModelRenderer *)sprite->renderer)->model = (ModelBuffer *)buffer;
        break;
    /* b5 u8: scale = u8 << 8 (sprites with a render kind). */
    case 0xB5:
        {
            s16 scale;

            angle = code[0];
            scale = angle << 8;

            if (sprite->render.word & 3) {
                sprite_set_scale(sprite, scale);
            }
        }
        break;
    /* e7 s16: scale += s16 * 2 (80022000). */
    case 0xE7:
        sprite_set_scale(sprite, sprite->scale + (s16)((code[0] | (s16)(code[1] << 8)) * 2));
        break;
    /* e9 s16: renderer x scale += s16 * 2. */
    case 0xE9: {
        s32 value;

        value = (code[0] | (s16)(code[1] << 8)) * 2;
        if (sprite->renderer != NULL) {
            sprite->renderer->scale_x += value;
            sprite->render.bits.dirty = 1;
        }
        break;
    }
    /* ea s16: renderer y scale += s16 * 2. */
    case 0xEA: {
        s32 value;

        value = (code[0] | (s16)(code[1] << 8)) * 2;
        if (sprite->renderer != NULL) {
            sprite->renderer->scale_y += value;
            sprite->render.bits.dirty = 1;
        }
        break;
    }
    /* eb s16: renderer z scale += s16 * 2. */
    case 0xEB: {
        s32 value;

        value = (code[0] | (s16)(code[1] << 8)) * 2;
        if (sprite->renderer != NULL) {
            sprite->renderer->scale_z += value;
            sprite->render.bits.dirty = 1;
        }
        break;
    }
    /* bd u8: child sprite running animation u8 of the shared block (8006be10) with this
     * sprite's image (80023b84). */
    case 0xBD: {
        SpriteSource *source = (SpriteSource *)sprite_shared_source;

        sprite_create_child(sprite, (u16 *)(source->animations[code[0] + 1] + (s32)source->animations), sprite->image);
        break;
    }
    /* e0 s16: child sprite running the animation header at the operand + s16 with this
     * sprite's image (80023b84). */
    case 0xE0:
        sprite_create_child(sprite, (u16 *)(code + (((s8)code[1] << 8) + code[0])), sprite->image);
        break;
    /* ad u8: bounce (frame bits 1-10) = u8. */
    case 0xAD:
        sprite->frame_bits.bounce = code[0];
        break;
    /* b4 u8: push u8 (an e4 loop count). */
    case 0xB4:
        sprite_stack_push_byte(sprite, code[0]);
        break;
    /* b8 s8: stack index -= s8 (reserves s8 variable bytes; negative releases). */
    case 0xB8:
        angle = (s8)code[0];
        sprite->stack_top -= angle;
        break;
    /* b3 s8: frame index (frame bits 11-16) = s8. */
    case 0xB3:
        sprite->frame_bits.frame = (s8)code[0];
        break;
    /* ae s8: renderer z angle += s8 * 16, negated when mirrored. */
    case 0xAE:
        angle = (s8)code[0] * 16;
        if ((sprite->motion.word >> 2) & 1) {
            angle = -angle;
        }
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_z += angle;
            sprite->render.bits.dirty = 1;
        }
        break;
    /* b6 s8: renderer x angle += s8 * 16. */
    case 0xB6:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_x += (s16)(code[0] << 8) >> 4;
            sprite->render.bits.dirty = 1;
        }
        break;
    /* b7 s8: renderer y angle += s8 * 16. */
    case 0xB7:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_y += (s16)(code[0] << 8) >> 4;
            sprite->render.bits.dirty = 1;
        }
        break;
    /* af s8: renderer z angle = s8 * 16, negated when mirrored. */
    case 0xAF:
        angle = (s8)code[0] * 16;
        if ((sprite->motion.word >> 2) & 1) {
            angle = -angle;
        }
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_z = angle;
            sprite->render.bits.dirty = 1;
        }
        break;
    /* c4 u8: turn the velocity about z by a random step in [-u8 / 2, u8 / 2) * 16. */
    case 0xC4: {
        s32 offset;

        offset = (rand() & 0xFF) * code[0] / 256;
        offset -= code[0] >> 1;
        sprite_set_svector(&vector, 0, 0, offset * 16);
        gpu_build_rotation_matrix(&vector, &m);
        ApplyMatrixLV(&m, (VECTOR *)&sprite->speed_x, &sum);
        sprite->speed_x = sum.vx;
        sprite->speed_y = sum.vy;
        sprite->speed_z = sum.vz;
        break;
    }
    /* ac u8: direction += a random step in [-u8 / 2, u8 / 2) * 16. */
    case 0xAC: {
        s32 offset;

        offset = (rand() & 0xFF) * code[0] / 256;
        offset -= code[0] >> 1;
        SPRITE_OFFSET_DIRECTION(sprite, offset);
        break;
    }
    /* a9 s8: x += s8 * scale / 4096 (by the speed factor), negated when mirrored. */
    case 0xA9:
        value = sprite_scale_by_rate(sprite, (s8)code[0] * sprite->scale / 4096) << 16;
        if ((sprite->motion.word >> 2) & 1) {
            value = -value;
        }
        sprite->x += value;
        break;
    /* aa s8: y += s8 * scale / 4096 (by the speed factor). */
    case 0xAA:
        sprite->y += sprite_scale_by_rate(sprite, (s8)code[0] * sprite->scale / 4096) << 16;
        break;
    /* ab s8: z += s8 * scale / 4096 (by the speed factor). */
    case 0xAB:
        sprite->z += sprite_scale_by_rate(sprite, (s8)code[0] * sprite->scale / 4096) << 16;
        break;
    /* a8 s8: direction += s8 * 16; the velocity follows (80022974). */
    case 0xA8:
        sprite->direction += (s16)(code[0] << 8) >> 4;
        sprite_update_velocity(sprite);
        break;
    /* 8a: stop: x, z and walking speeds 0. */
    case 0x8A:
        sprite->speed_x = 0;
        sprite->speed_z = 0;
        sprite->speed = 0;
        break;
    /* a3 s8: gravity (+1c) = (s8 * 64 * (+82) / 4096 << 5) * (65536 / divisor)^2 / 256 /
     * 256 * (skip + 1)^2, or the own sequencer's word 4 when nonzero. */
    case 0xA3:
        if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->word4 != 0) {
            sprite->gravity = ((SpriteSequencer *)sprite->sequencer)->word4;
        } else {
            sprite->gravity = (((s8)code[0] << 6) * (s16)sprite->word82 / 4096) << 5;
            n = 0x10000 / sprite->motion.bits.divisor;
            sprite->gravity *= n * n / 256;
            sprite->gravity /= 256;
            sprite->gravity *= (sprite_frame_skip + 1) * (sprite_frame_skip + 1);
        }
        break;
    /* a5 s8: walking speed += s8 * 16 * (skip + 1) * (+82) / 4096 << 8; the velocity
     * follows. */
    case 0xA5:
        sprite->speed += (((s8)code[0] << 4) * (sprite_frame_skip + 1) * (s16)sprite->word82 / 4096) << 8;
        sprite_update_velocity(sprite);
        break;
    /* a6 s8: vertical speed += (s8 * 16 * (skip + 1) * (+82) / 4096 << 16) / divisor,
     * unless the sequencer is the sprite's own. */
    case 0xA6:
        if (sprite->frame_bits.sequencer_owned != 1) {
            sprite->speed_y +=
                ((((s8)code[0] << 4) * (sprite_frame_skip + 1) * (s16)sprite->word82 / 4096) << 16) / sprite->motion.bits.divisor;
        }
        break;
    /* a0 s8: walking speed = s8 * 16 * (skip + 1) * (+82) / 4096 << 8; the velocity
     * follows. */
    case 0xA0:
        sprite->speed = (((s8)code[0] << 4) * (sprite_frame_skip + 1) * (s16)sprite->word82 / 4096) << 8;
        sprite_update_velocity(sprite);
        break;
    /* a1 s8: vertical speed = s8 * 16 * (skip + 1) * (+82) / 4096 << 8 (or the own
     * sequencer's word 0 when nonzero), then << 8 / divisor. */
    case 0xA1:
        if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->word0 != 0) {
            sprite->speed_y = ((SpriteSequencer *)sprite->sequencer)->word0;
        } else {
            sprite->speed_y = (((s8)code[0] << 4) * (sprite_frame_skip + 1) * (s16)sprite->word82 / 4096) << 8;
        }
        sprite->speed_y <<= 8;
        sprite->speed_y /= sprite->motion.bits.divisor;
        break;
    /* ed s16: x = s16. */
    case 0xED:
        sprite->x = (code[0] | ((s8)code[1] << 8)) << 16;
        break;
    /* ee s16: y = ground + s16 * scale / 4096. */
    case 0xEE:
        sprite->y = (sprite->ground + (code[0] | ((s8)code[1] << 8)) * sprite->scale / 4096) << 16;
        break;
    /* ef s16: z = s16. */
    case 0xEF:
        sprite->z = (code[0] | ((s8)code[1] << 8)) << 16;
        break;
    }
}

/* 80021AD8: `value + delta` clamped to 0-255. */
s32 sprite_add_clamp_byte(s32 value, s32 delta) {
    value += delta;
    if (value >= 0x100) {
        value = 0xFF;
    } else if (value < 0) {
        value = 0;
    }
    return value;
}

/* 80021B04: Vector helpers: set a short or a long vector, copy one. */
void sprite_set_svector(SVECTOR *vector, s16 x, s16 y, s16 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

/* 80021B14 */
void sprite_set_vector(VECTOR *vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

/* 80021B24 */
void sprite_copy_svector(SVECTOR *to, SVECTOR *from) {
    to->vx = from->vx;
    to->vy = from->vy;
    to->vz = from->vz;
}

/* 80021B48 */
void sprite_copy_vector(VECTOR *to, VECTOR *from) {
    to->vx = from->vx;
    to->vy = from->vy;
    to->vz = from->vz;
}

/* 80021B6C: Drop a sprite's part colour. */
void sprite_disable_part_color(Sprite *sprite) {
    sprite->colour_flags |= 1;
    sprite_recolor_parts(sprite);
}

/* 80021B98: Colour a sprite's one-sided parts. */
void sprite_set_part_color(Sprite *sprite, u8 red, u8 green, u8 blue) {
    sprite->red = red;
    sprite->green = green;
    sprite->blue = blue;
    sprite->colour_flags &= ~1;
    sprite_recolor_parts(sprite);
}

/* 80021BCC: Set a sprite's gravity divisor. */
void sprite_set_gravity_divisor(Sprite *sprite, s32 divisor) {
    sprite->motion.bits.divisor = divisor;
}

/* 80021BF0: Set a sprite's resource word (+0x4c). */
void sprite_set_alternate_resource(Sprite *sprite, s32 resource) {
    sprite->resource = resource;
}

/* 80021BF8: Set a sprite's completion callback. */
void sprite_set_completion_callback(Sprite *sprite, void *callback) {
    sprite->callback = callback;
}

/* 80021C00: Set a sprite's scale shift (flags bits 8-12, SpriteFlagBits.shift: the
 * shift 8001e148 applies to its parts' offsets and sizes). */
void sprite_set_scale_shift(Sprite *sprite, s32 shift) {
    sprite->flags = (sprite->flags & ~0x1F00) | ((shift & 0x1F) << 8);
}

/* 80021C20: Pop a byte from a sprite's stack. */
u8 sprite_stack_pop_byte(Sprite *sprite) {
    u8 value = sprite->stack[sprite->stack_top];

    sprite->stack_top += 1;
    return value;
}

/* 80021C3C: Pop a halfword from a sprite's stack. */
s16 sprite_stack_pop_halfword(Sprite *sprite) {
    s16 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8);

    sprite->stack_top += 2;
    return value;
}

/* 80021C6C: Pop three bytes from a sprite's stack. */
s32 sprite_stack_pop_three_bytes(Sprite *sprite) {
    s32 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8) +
                (sprite->stack[sprite->stack_top + 2] << 16);

    sprite->stack_top += 3;
    return value;
}

/* 80021CA0: Push a byte onto a sprite's stack. */
void sprite_stack_push_byte(Sprite *sprite, u8 value) {
    sprite->stack[--sprite->stack_top] = value;
}

/* 80021CC4: Push a halfword onto a sprite's stack. */
void sprite_stack_push_halfword(Sprite *sprite, u16 value) {
    sprite->stack_top -= 2;
    sprite->stack[sprite->stack_top] = value;
    sprite->stack[sprite->stack_top + 1] = value >> 8;
}

/* 80021CF8: Push three bytes onto a sprite's stack. */
void sprite_stack_push_three_bytes(Sprite *sprite, s32 value) {
    sprite->stack_top -= 3;
    sprite->stack[sprite->stack_top] = value;
    sprite->stack[sprite->stack_top + 1] = value >> 8;
    sprite->stack[sprite->stack_top + 2] = value >> 16;
}

/* 80021D3C: Set a position's x and z from whole units (16.16). */
void sprite_set_position_xz(VECTOR *position, s32 x, s32 z) {
    position->vz = z << 16;
    position->vx = x << 16;
}

/* 80021D50: Restore a sprite from a snapshot: its state, then advance its animation
 * (moving it) until the saved step, then its saved position; frame skipping
 * is off meanwhile. */
void sprite_restore_state(Sprite *sprite, SpriteState *state) {
    s32 skip = sprite_frame_skip;

    sprite_frame_skip = 0;
    sprite->word80 = state->word80;
    sprite->motion.bytes[3] = state->byteaf;
    sprite->b0.byteb0 = state->byteb0;
    sprite->renderer->scale_x = state->scale_x;
    sprite->renderer->scale_y = state->scale_y;
    sprite->renderer->scale_z = state->scale_z;
    sprite->word82 = state->word82;
    sprite->scale = state->scale;
    sprite_start_animation(sprite, (s8)sprite->motion.bytes[3]);
    while (sprite->frame_bits.field22 != state->field22) {
        sprite_vm_tick(sprite);
        sprite->x += sprite->speed_x;
        sprite->z += sprite->speed_z;
        sprite->y += sprite->speed_y;
        sprite->speed_y += sprite->gravity;
    }
    sprite->x = state->x;
    sprite->y = state->y;
    sprite->z = state->z;
    ((SpriteSequencer *)sprite->sequencer)->word0 = state->sequencer0;
    ((SpriteSequencer *)sprite->sequencer)->word4 = state->sequencer4;
    sprite_frame_skip = skip;
}

/* 80021EBC: Save a sprite's position and animation state. */
void sprite_save_state(Sprite *sprite, SpriteState *state) {
    state->x = sprite->x;
    state->y = sprite->y;
    state->z = sprite->z;
    state->word80 = sprite->word80;
    state->frame = sprite->frame_bits.frame;
    state->byteaf = (s8)sprite->motion.bytes[3];
    state->byteb0 = (s8)sprite->b0.byteb0;
    state->field22 = sprite->frame_bits.field22;
    state->sequencer0 = ((SpriteSequencer *)sprite->sequencer)->word0;
    state->sequencer4 = ((SpriteSequencer *)sprite->sequencer)->word4;
    state->scale_x = sprite->renderer->scale_x;
    state->scale_y = sprite->renderer->scale_y;
    state->scale_z = sprite->renderer->scale_z;
    state->word82 = sprite->word82;
    state->scale = sprite->scale;
}

/* 80021FB8: Set the low byte of a sprite's word +0xb0. */
void sprite_set_idle_animation(Sprite *sprite, u8 value) {
    sprite->b0.byteb0 = value;
}

/* 80021FC0: Set a sprite's walking speed (its velocity follows). */
void sprite_set_walk_speed(Sprite *sprite, s32 speed) {
    sprite->speed = speed;
    sprite_update_velocity(sprite);
}

/* 80021FE0: Turn a sprite (its speed follows the direction). */
void sprite_set_direction(Sprite *sprite, s16 direction) {
    sprite->direction = direction;
    sprite_update_velocity(sprite);
}

/* 80022000: Set a sprite's uniform scale (and its renderer's), marking the orientation dirty. */
void sprite_set_scale(Sprite *sprite, s32 scale) {
    SpriteRenderer *renderer = sprite->renderer;

    if (renderer != NULL) {
        renderer->scale_x = renderer->scale_y = renderer->scale_z = sprite->scale = scale;
        sprite->render.bits.dirty = 1;
    }
}

/* 80022038: Rebuild a sprite's orientation if it is marked dirty. */
void sprite_update_orientation(Sprite *sprite) {
    if ((sprite->render.word >> 28) & 1) {
        sprite_rebuild_orientation(sprite);
        sprite->render.bits.dirty = 0;
    }
}
