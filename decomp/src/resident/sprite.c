/* Resident sprite engine, first unit (0x8001c8dc-0x80022090): task lists,
 * sprite motion and the animation script interpreter. Compiled by the CDK
 * GCC at -G8; positive `li` are assembled as `addiu` (ASPSX 2.50+). */
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
u32 D_80059184;
s32 D_80059188;
s32 D_8005918C;
Sprite *D_80059190;
s16 D_80059194; /* texture area row (0-2) of the next image */
s16 D_80059196; /* texture area column of the next image */
s32 *D_800592E4;              /* image list for 8001fb30 */
s16 D_800592E8;               /* its position */
s16 D_800592EA;
Task *D_800594C0;
Task *D_8005958C;
Task *D_80059590;
Task *D_80059594;

/* Destroy every task of both lists. */
void func_8001C8DC(void) {
    Task *task;

    while ((task = D_8005958C) != NULL) {
        task->destroy(task);
    }
    while ((task = D_80059594) != NULL) {
        task->destroy(task);
    }
}

/* Empty both task lists. */
void func_8001C944(void) {
    D_8005958C = NULL;
    D_80059594 = NULL;
    D_80059188 = 0;
    D_8005918C = 0;
    D_80059428 = 0;
}

/* Run the main task list, unless it is paused (count the pause down). */
void func_8001C964(void) {
    Task *task;

    if (D_80059428 != 0) {
        if (--D_80059428 == 0) {
            D_80059494 = 0;
        }
        return;
    }
    D_80059590 = D_8005958C;
    while (D_80059590 != NULL) {
        task = D_80059590;
        D_800594C0 = task;
        D_80059590 = task->next;
        if (task->update != NULL) {
            task->update(task);
        }
    }
}

/* Run the second task list. */
void func_8001C9F8(void) {
    Task *task;

    D_80059590 = D_80059594;
    while (D_80059590 != NULL) {
        task = D_80059590;
        D_800594C0 = task;
        D_80059590 = task->next;
        if (task->update != NULL) {
            task->update(task);
        }
    }
}

/* Link `node` at the head of the second task list under `owner`. */
void func_8001CA58(Task *owner, Task *node) {
    TaskLink link;
    Task *head = D_80059594;

    node->owner = owner;
    node->next = head;
    D_80059594 = node;
    link.word = node->link.word;
    link.bits.owner_serial = owner->id.bits.serial;
    node->id.bits.serial = D_80059184++;
    link.bits.flag29 = 0;
    link.bits.flag30 = 0;
    link.bits.active = 0;
    node->link = link;
    node->update = NULL;
    node->destroy = func_8001CB48;
    D_8005918C++;
}

/* Allocate a task with `size` bytes after its node on the second list. */
Task *func_8001CAF0(Task *owner, s32 size) {
    Task *node = func_80031BDC(size + sizeof(Task), D_800591AF);

    func_8001CA58(owner, node);
    node->destroy = func_8001CBE8;
    return node;
}

/* Unlink a task from the second list. */
void func_8001CB48(Task *task) {
    Task *prev = NULL;
    Task *current;

    for (current = D_80059594; current != NULL; current = current->next) {
        if (current == task) {
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                D_80059594 = current->next;
            }
            if (D_80059590 == task) {
                D_80059590 = task->next;
            }
            break;
        }
        prev = current;
    }
    if (current == NULL) {
        D_8005918C++;
    }
    D_8005918C--;
}

/* Destroy callback of an allocated second-list task: unlink and free it. */
void func_8001CBE8(Task *task) {
    func_8001CB48(task);
    func_800320E8(task);
}

/* Link `node` at the head of the main task list under `owner`; it counts as
 * active while the active flag (800591ac) is set. */
/* The node is written through a second pointer to it (the original keeps
 * the node in $t3 and the inactive branch's copy in $v1); the owner's
 * serial is read once the owner is linked. */
void func_8001CC18(Task *owner, Task *node) {
    Task *self = &node[0];
    u32 serial;

    self->owner = owner;
    serial = owner->id.bits.serial;
    self->destroy = func_8001CD94;
    self->update = NULL;
    self->next = D_8005958C;
    D_8005958C = self;
    self->link.bits.owner_serial = serial;
    self->link.bits.flag29 = 0;
    self->link.bits.flag30 = 0;
    self->link.bits.active = 0;
    self->id.bits.serial = D_80059184++;
    if (D_800591AC != 0) {
        D_80059464++;
        self->link.bits.active = 1;
    } else {
        self->link.bits.active = 0;
    }
    D_80059188++;
}

/* Allocate a task with `size` bytes after its node on the main list. */
Task *func_8001CD08(Task *owner, s32 size) {
    Task *node = func_80031BDC(size + sizeof(Task), D_800591AF);

    func_8001CC18(owner, node);
    node->destroy = func_8001CE44;
    node->data = NULL;
    return node;
}

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

/* Unlink a task from the main list. */
void func_8001CD94(Task *task) {
    Task *prev = NULL;
    Task *current;

    for (current = D_8005958C; current != NULL; current = current->next) {
        if (current == task) {
            if (prev != NULL) {
                prev->next = task->next;
            } else {
                D_8005958C = task->next;
            }
            if (D_80059590 == task) {
                D_80059590 = task->next;
            }
            break;
        }
        prev = current;
    }
    if (task->link.bits.active) {
        D_80059464--;
    }
    D_80059188--;
}

/* Destroy callback of an allocated main-list task: unlink and free it. */
void func_8001CE44(Task *task) {
    func_8001CD94(task);
    func_800320E8(task);
}

/* Destroy every task `owner` created (both lists). */
void func_8001CE74(Task *owner) {
    Task *prev;
    Task *task;

    prev = NULL;
    for (task = D_80059594; task != NULL; task = task->next) {
        if (task->owner == owner && !((task->link.word >> 30) & 1) && task->link.bits.owner_serial == owner->id.bits.serial) {
            if (prev != NULL) {
                prev->next = task->next;
            } else {
                D_80059594 = task->next;
            }
            if (D_80059590 == task) {
                D_80059590 = task->next;
            }
            if (task->destroy != NULL) {
                task->destroy(task);
            }
        } else {
            prev = task;
        }
    }
    prev = NULL;
    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->owner == owner && !((task->link.word >> 30) & 1) && task->link.bits.owner_serial == owner->id.bits.serial) {
            if (prev != NULL) {
                prev->next = task->next;
            } else {
                D_8005958C = task->next;
            }
            if (D_80059590 == task) {
                D_80059590 = task->next;
            }
            if (task->destroy != NULL) {
                task->destroy(task);
            }
        } else {
            prev = task;
        }
    }
}

/* Clear word 0x70 of the sprite of every flag-29 task `owner` created. */
void func_8001D034(Task *owner) {
    Task *task;

    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->owner == owner && task->link.bits.owner_serial == owner->id.bits.serial && ((task->link.word >> 29) & 1)) {
            ((Sprite *)task->data)->word70 = NULL;
        }
    }
}

/* The main-list task `owner` created with update callback `update`, or NULL. */
Task *func_8001D0A4(Task *owner, void (*update)(Task *)) {
    Task *task;

    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->owner == owner && task->link.bits.owner_serial == owner->id.bits.serial && task->update == update) {
            return task;
        }
    }
    return NULL;
}

/* The first main-list task `owner` created, or NULL. */
Task *func_8001D10C(Task *owner) {
    Task *task;

    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->owner == owner && task->link.bits.owner_serial == owner->id.bits.serial) {
            return task;
        }
    }
    return NULL;
}

/* The first main-list task with update callback `update`, or NULL. */
Task *func_8001D164(void (*update)(Task *)) {
    Task *task;

    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->update == update) {
            return task;
        }
    }
    return NULL;
}

/* Destroy callback of a two-node task: unlink both nodes and free it. */
void func_8001D19C(Task *task) {
    func_8001CB48(task + 1);
    func_8001CD94(task);
    func_800320E8(task);
}

/* Allocate a `size`-byte task that starts with two nodes: the first on the main
 * list under `owner` with `update`, the second on the second list with
 * `update2`; both nodes' data is the task itself. */
Task *func_8001D1D8(s32 size, Task *owner, void (*update)(Task *), void (*update2)(Task *),
                    void (*destroy)(Task *)) {
    Task *node = func_80031BDC(size, D_800591AF);

    func_8001CC18(owner, node);
    func_8001CA58(node, node + 1);
    func_8001CD6C(node, update);
    func_8001CD64(node + 1, update2);
    if (destroy != NULL) {
        func_8001CD74(node, destroy);
    } else {
        func_8001CD74(node, func_8001D19C);
    }
    node->data = node;
    node[1].data = node;
    return node;
}

void func_8001D298(void) {
    D_80059190 = NULL;
}

void func_8001D2A4(void) {
    D_80059190 = NULL;
}

/* Request frame `frame` for a one-sided sprite: it joins the pending list
 * (drawn by 8001d468), or, already pending, first draws its previous one. */
void func_8001D2B0(Sprite *sprite, s32 frame) {
    Sprite *pending;

    if ((sprite->render.word & 3) != 1) {
        sprite->frame = 0;
        return;
    }
    if ((sprite->flags >> 20) & 1) {
        sprite->flags &= ~0x100000;
        if (sprite->renderer->pointer34 != NULL) {
            func_800234AC(sprite);
        }
    }
    if ((sprite->flags >> 17) & 1) {
        for (pending = D_80059190; pending != NULL; pending = pending->renderer->next_pending) {
            if (pending == sprite) {
                if (sprite->image != D_8005A474 && sprite->image != D_8006BE10 && !((sprite->flags >> 19) & 1)) {
                    func_8001F8E8(sprite, sprite->frame, sprite->image);
                }
                sprite->frame = frame;
                return;
            }
        }
    }
    sprite->frame = frame;
    sprite->flags |= 0x20000;
    sprite->renderer->next_pending = D_80059190;
    D_80059190 = sprite;
}

/* Remove a sprite from the pending list. */
void func_8001D3F4(Sprite *sprite) {
    Sprite *prev = NULL;
    Sprite *pending;

    for (pending = D_80059190; pending != NULL; pending = pending->renderer->next_pending) {
        if (pending == sprite) {
            if (prev != NULL) {
                prev->renderer->next_pending = pending->renderer->next_pending;
            } else {
                D_80059190 = pending->renderer->next_pending;
            }
        } else {
            prev = pending;
        }
    }
}

/* Draw the pending frame of every pending sprite and empty the list. */
void func_8001D468(void) {
    Sprite *sprite;
    s32 frame;

    for (sprite = D_80059190; sprite != NULL; sprite = sprite->renderer->next_pending) {
        frame = sprite->frame;
        if (frame == 0) {
            sprite->flags &= ~0xFC;
        } else {
            func_8001DAE8(sprite, frame, sprite->image);
        }
    }
    D_80059190 = NULL;
}

/* Give a sprite's renderer its 0x40-byte block (once). */
void func_8001D4E8(Sprite *sprite) {
    if (sprite->renderer->pointer34 == NULL) {
        sprite->renderer->pointer34 = func_80031BDC(0x40, 0);
        func_800234AC(sprite);
    }
}

/* Build frame `frame` of a cell-directory source (directory bit 15): each
 * part takes a cell already in VRAM with its own texture position and size,
 * from the page of a resident cell kind (two-byte kinds), of the sprite's
 * sequencer or of the source, after the control bytes before it. */
void func_8001D53C(Sprite *sprite, s32 frame, SpriteSource *source) {
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
                    sprite->renderer->pointer34 = func_80031BDC(0x40, 0);
                    func_800234AC(sprite);
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
            resident_pages = D_8004FAB8;
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

/* Build frame `frame` of a sprite's source into its parts and queue the
 * uploads of the cells it uses (to the source's texture position, or a
 * reserved texture area column for facing group 14), after its palette
 * when the render flag asks for it; cell-directory sources go to 8001d53c. */
void func_8001DAE8(Sprite *sprite, s32 frame, SpriteSource *source) {
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
                func_800251C8((u_long *)(palette + (*palette * ((sprite->render.word >> 16) & 0xF0) + 2)),
                              source->clut_x, source->clut_y, *palette * 16, 1);
            }
        }
        if (*table & 0x8000) {
            func_8001D53C(sprite, frame, source);
            return;
        }
        record = (u8 *)(table[frame] + (s32)table);
        position = ((SpriteSource *)sprite->image)->origin;
        if (((sprite->flags >> 13) & 0xF) == 0xE) {
            position = func_8001F530(record[4]);
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
                        sprite->renderer->pointer34 = func_80031BDC(0x40, 0);
                        func_800234AC(sprite);
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
            func_800251C8((u_long *)(cell + 1), position.vx + rect.x, position.vy + rect.y, words, cell->h);
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
        func_800251C8(NULL, position.vx, position.vy, record[4], record[5]);
    }
}

/* Set the GTE rotation and translation for drawing a sprite: its position
 * through the view matrix plus its scaled screen offset. */
void func_8001E148(Sprite *sprite) {
    SVECTOR position;
    VECTOR view;
    s32 shift;
    s32 offset_y;
    s32 offset_x;
    MATRIX *matrix;

    if (D_800591AD != 0 || D_800591AE != 0) {
        func_80022038(sprite);
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
    ApplyMatrix(&D_8004FBB8, &position, &view);
    matrix = &sprite->renderer->matrix;
    matrix->t[0] = D_8004FBB8.t[0] + view.vx + offset_x;
    matrix->t[1] = D_8004FBB8.t[1] + view.vy + offset_y;
    matrix->t[2] = D_8004FBB8.t[2] + view.vz;
    SetRotMatrix(matrix);
    SetTransMatrix(matrix);
}

/* Draw a sprite's parts at `ot` with 8001e3d8 (and 8001e9bc for render flag 2). */
void func_8001E298(Sprite *sprite, u_long *ot) {
    func_8001E148(sprite);
    func_8001E3D8(sprite, ot);
    if ((sprite->render.word >> 2) & 1) {
        func_8001E9BC(sprite, ot);
    }
}

/* Draw a sprite's parts at `ot` with 8001ee88 (and 8001e9bc for render flag 2). */
void func_8001E2F8(Sprite *sprite, u_long *ot, s32 height) {
    func_8001E148(sprite);
    func_8001EE88(sprite, ot, height);
    if ((sprite->render.word >> 2) & 1) {
        func_8001E9BC(sprite, ot);
    }
}

/* Draw a sprite's parts at `ot` with 8001f1d4 (and 8001e9bc for render flag 2). */
void func_8001E368(Sprite *sprite, u_long *ot, s32 height) {
    func_8001E148(sprite);
    func_8001F1D4(sprite, ot, height);
    if ((sprite->render.word >> 2) & 1) {
        func_8001E9BC(sprite, ot);
    }
}

/* Draw a sprite's parts as textured quads (POLY_FT4 from the queue block)
 * linked at `ot` (or, with render bit 27, at `ot` minus the part's group).
 * Parts of one group share a matrix: the renderer's, or its product with the
 * group entry's rotation and offset; groups masked by render byte 1
 * (8004faf8) are skipped. */
/* Nonmatching: the original frame has 12 more bytes of locals between the
 * angles and the RotTransPers4 outputs (0x50-0x5B, never accessed), and it
 * keeps the part width in $t0 and the column span in $a3 (swapped here).
 * One unused 12-byte aggregate (long[3], s16[6], DVECTOR[3], u8[12]) is
 * BLKmode, so GCC aligns it to 8 and reserves 16 bytes: the outputs then sit
 * 4 bytes too high (score 22, with only that offset and the $t0/$a3 swap
 * left); an 8-byte aggregate plus an unused word would fit, but an unused
 * word scalar is not allowed as padding. */
#ifdef NON_MATCHING
void func_8001E3D8(Sprite *sprite, u_long *ot) {
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
    long depth;
    long flag;
    POLY_FT4 *poly;
    s16 w, h, x, y;
    s32 width, height, left, top;
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
    if ((u8 *)D_80059580 + count * sizeof(POLY_FT4) < D_80059534) {
        for (i = 0; i != (sprite->flags >> 2 & 0x3F); i++) {
            if (group != (parts[i].flags & 7)) {
                group = parts[i].flags & 7;
                visible = (D_8004FAF8[group] & ((u8 *)&sprite->render)[1]) == 0;
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
                    func_8003F738(&angles, &m);
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
                poly = (POLY_FT4 *)D_80059580;
                D_80059580 = (SpriteQueueEntry *)(poly + 1);
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
                    D_8004FB98[0].vx = x;
                    D_8004FB98[1].vx = x + w;
                    D_8004FB98[2].vx = x + w;
                    D_8004FB98[3].vx = x;
                } else {
                    D_8004FB98[0].vx = x + w;
                    D_8004FB98[1].vx = x;
                    D_8004FB98[2].vx = x;
                    D_8004FB98[3].vx = x + w;
                }
                if (!((parts[i].flags >> 5) & 1)) {
                    D_8004FB98[0].vy = y;
                    D_8004FB98[1].vy = y;
                    D_8004FB98[2].vy = y + h;
                    D_8004FB98[3].vy = y + h;
                } else {
                    D_8004FB98[0].vy = y + h;
                    D_8004FB98[1].vy = y + h;
                    D_8004FB98[2].vy = y;
                    D_8004FB98[3].vy = y;
                }
                D_8004FB98[0].vy -= origin_y;
                D_8004FB98[1].vy -= origin_y;
                D_8004FB98[2].vy -= origin_y;
                D_8004FB98[3].vy -= origin_y;
                D_8004FB98[0].vx -= origin_x;
                D_8004FB98[1].vx -= origin_x;
                D_8004FB98[2].vx -= origin_x;
                D_8004FB98[3].vx -= origin_x;
                RotTransPers4(&D_8004FB98[0], &D_8004FB98[1], &D_8004FB98[2], &D_8004FB98[3], (long *)&poly->x0,
                              (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &depth, &flag);
                u = parts[i].u;
                v = parts[i].v;
                du = parts[i].w - 1;
                dv = parts[i].h - 1;
                if (poly->x3 < poly->x0) {
                    if (u - 1 >= 0) {
                        u--;
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E3D8);
#endif

/* Draw a sprite's shadow: its parts as black quads (POLY_FT4 from the queue
 * block) flattened onto its floor height, the view matrix scaled by the
 * sprite scale (half height), linked at `ot`. */
void func_8001E9BC(Sprite *sprite, u_long *ot) {
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

    m = D_8004FBB8;
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    scale.vx = sprite->scale;
    scale.vy = sprite->scale / 2;
    scale.vz = 0;
    ScaleMatrixL(&m, &scale);
    position.vy = sprite->ground;
    ApplyMatrix(&D_8004FBB8, &position, &view);
    m.t[0] += view.vx;
    m.t[1] += view.vy;
    m.t[2] += view.vz;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    count = (u8)sprite->flags >> 2;
    group = -1;
    parts = sprite->renderer->parts[1];
    if ((u8 *)D_80059580 + count * sizeof(POLY_FT4) < D_80059534) {
        for (i = 0; i != (u8)sprite->flags >> 2; i++) {
            part_flags = parts[i].flags;
            if (group != (part_flags & 7)) {
                group = part_flags & 7;
                visible = (D_8004FAF8[group] & ((u8 *)&sprite->render)[1]) == 0;
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
                    D_8004FAD8[0].vx = x;
                    D_8004FAD8[1].vx = x + w;
                    D_8004FAD8[2].vx = x + w;
                    D_8004FAD8[3].vx = x;
                } else {
                    D_8004FAD8[0].vx = x + w;
                    D_8004FAD8[1].vx = x;
                    D_8004FAD8[2].vx = x;
                    D_8004FAD8[3].vx = x + w;
                }
                if (!((parts[i].flags >> 5) & 1)) {
                    D_8004FAD8[2].vz = y + h;
                    D_8004FAD8[3].vz = y + h;
                    D_8004FAD8[0].vz = y;
                    D_8004FAD8[1].vz = y;
                } else {
                    D_8004FAD8[2].vz = y;
                    D_8004FAD8[3].vz = y;
                    D_8004FAD8[0].vz = y + h;
                    D_8004FAD8[1].vz = y + h;
                }
                poly = (POLY_FT4 *)D_80059580;
                D_80059580 = (SpriteQueueEntry *)(poly + 1);
                setlen(poly, 9);
                poly->code = 0x2C;
                poly->r0 = 0;
                poly->g0 = 0;
                poly->b0 = 0;
                RotAverage4(&D_8004FAD8[0], &D_8004FAD8[1], &D_8004FAD8[2], &D_8004FAD8[3], (long *)&poly->x0,
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

/* Whether a frame table entry takes its image from the sequencer (second byte bit 7). */
s32 func_8001EE68(u8 *frame) {
    return frame[1] >> 7;
}

/* The part count of a frame header (bits 9-14). */
s32 func_8001EE74(u16 *header) {
    return (*header >> 9) & 0x3F;
}

/* Draw a sprite's parts as textured quads (POLY_FT4 from the queue block)
 * cut off below `height` (in the parts' units before the sprite's shift):
 * parts entirely past it are skipped, parts crossing it lose the rows past
 * it, texture included. Linked at `ot`. */
void func_8001EE88(Sprite *sprite, u_long *ot, s32 height) {
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
    if ((u8 *)D_80059580 + count * sizeof(POLY_FT4) < D_80059534) {
        for (i = 0; i != ((sprite->flags >> 2) & 0x3F); i++) {
            poly = (POLY_FT4 *)D_80059580;
            D_80059580 = (SpriteQueueEntry *)(poly + 1);
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
                D_8004FB98[0].vx = x;
                D_8004FB98[1].vx = x + w;
                D_8004FB98[2].vx = x + w;
                D_8004FB98[3].vx = x;
            } else {
                D_8004FB98[0].vx = x + w;
                D_8004FB98[1].vx = x;
                D_8004FB98[2].vx = x;
                D_8004FB98[3].vx = x + w;
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
                D_8004FB98[0].vy = y;
                D_8004FB98[1].vy = y;
                D_8004FB98[2].vy = y + h;
                D_8004FB98[3].vy = y + h;
            } else {
                D_8004FB98[0].vy = y + h;
                D_8004FB98[1].vy = y + h;
                D_8004FB98[2].vy = y;
                D_8004FB98[3].vy = y;
            }
            RotAverage4(&D_8004FB98[0], &D_8004FB98[1], &D_8004FB98[2], &D_8004FB98[3], (long *)&poly->x0,
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

/* Draw a sprite's parts as 8001ee88 does, cut off above `height` instead:
 * parts entirely above it are skipped, parts crossing it lose the rows
 * above it, texture included. Linked at `ot`. */
void func_8001F1D4(Sprite *sprite, u_long *ot, s32 height) {
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
    if ((u8 *)D_80059580 + count * sizeof(POLY_FT4) < D_80059534) {
        for (i = 0; i != ((sprite->flags >> 2) & 0x3F); i++) {
            poly = (POLY_FT4 *)D_80059580;
            D_80059580 = (SpriteQueueEntry *)(poly + 1);
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
                D_8004FB98[0].vx = x;
                D_8004FB98[1].vx = x + w;
                D_8004FB98[2].vx = x + w;
                D_8004FB98[3].vx = x;
            } else {
                D_8004FB98[0].vx = x + w;
                D_8004FB98[1].vx = x;
                D_8004FB98[2].vx = x;
                D_8004FB98[3].vx = x + w;
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
                D_8004FB98[0].vy = y;
                D_8004FB98[1].vy = y;
                D_8004FB98[2].vy = y + h;
                D_8004FB98[3].vy = y + h;
            } else {
                D_8004FB98[0].vy = y + h;
                D_8004FB98[1].vy = y + h;
                D_8004FB98[2].vy = y;
                D_8004FB98[3].vy = y;
            }
            RotAverage4(&D_8004FB98[0], &D_8004FB98[1], &D_8004FB98[2], &D_8004FB98[3], (long *)&poly->x0,
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

/* Reserve `width` columns of the sprite texture area (three 64-line rows from
 * (0x300, 0x140), 0x40 columns each) and return their position. */
DVECTOR func_8001F530(s32 width) {
    DVECTOR position;

    if (D_80059196 + width > 0x40) {
        D_80059196 = 0;
        if (++D_80059194 >= 3) {
            D_80059194 = 0;
        }
    }
    position.vx = D_80059196 + 0x300;
    position.vy = D_80059194 * 64 + 0x140;
    D_80059196 += width;
    return position;
}

/* A sprite's extent (width, height, depth) at its scale, from the frame
 * record its first animation's byte 4 selects (less one; the first record
 * when the directory entry is below the index). */
void func_8001F5BC(Sprite *sprite, s32 unused, s32 *width, s32 *height, s32 *depth) {
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

/* Recolour a one-sided sprite's parts: the sprite's colour word and blend
 * mode (its blend rate - 1). */
void func_8001F6B0(Sprite *sprite) {
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

/* Apply the control bytes of frame `frame`'s parts: for each part, bytes
 * with bit 7 set precede it; with bit 6 they set entry (bits 0-2) of the
 * renderer's 0x40-byte block (bit 5: two bytes, bit 4: a depth byte * 16,
 * else depth 0), otherwise bits 0-1 skip a byte each. Parts are 3 bytes (5
 * when the frame's bit 7 is set). */
void func_8001F750(Sprite *sprite, s32 frame, SpriteSource *source) {
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
                    sprite->renderer->pointer34 = func_80031BDC(0x40, 0);
                    func_800234AC(sprite);
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

/* Apply the control bytes of frame `frame`'s parts (as 8001f750 does, which
 * handles directories with bit 15 set): here the part count is followed by
 * four bytes per part. Frames beyond the directory's count (bits 0-8) are
 * ignored. */
void func_8001F8E8(Sprite *sprite, s32 frame, SpriteSource *source) {
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
        func_8001F750(sprite, frame, source);
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
                    sprite->renderer->pointer34 = func_80031BDC(0x40, 0);
                    func_800234AC(sprite);
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

/* Unpack and upload the image at 8004fbd8 to (x, y). */
void func_8001FAB4(s32 x, s32 y) {
    void *image = func_80032E88(D_8004FBD8, 0);

    func_8002DDE4(image, 1, x, y, 0, 0, 0);
    DrawSync(0);
    func_800320E8(image);
}

/* Upload the image list at D_800592E4 to (D_800592E8, D_800592EA), running
 * the upload on an 8 KB heap block as its stack. */
void func_8001FB30(void) {
    u8 *stack = func_80031BDC(0x2000, 1);

    STACK_ENTER(stack + 0x1F00);
    func_8002DDE4(D_800592E4, 1, D_800592E8, D_800592EA, 0, 0, 0);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* The operand a script byte names: a frame table entry (bit 7 set) or a byte on the sprite's stack. */
u8 *func_8001FBA4(Sprite *sprite, u8 *code) {
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

/* Run the script command `op` (0x8a-0xfc) of a sprite on its operand bytes
 * `code`: motion, placement, colour, renderer angles and scales, byte
 * arithmetic on the sprite's stack and frame variables, sounds, models. */
/* Nonmatching: same size, frame, case layout and tail sharing as the
 * original; left (about 40 instructions): 0xCD keeps the angle in a second
 * register for the angle_x store, case 38 loads the motion word before the
 * frame bits, 0xF5 accumulates the model address in $s0 (the original in
 * $a0, copied to $s0 in the call's delay slot), and 0xBD, 0xB8, 0xC4, 0xAC
 * and 0xA3 order or allocate one load or operand differently. */
#ifdef NON_MATCHING
void func_8001FBE4(Sprite *sprite, u8 op, u8 *code) {
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
    case 0x8D:
        /* called without a prototype: the coordinates pass as ints */
        func_8002CC10(((SpriteSource *)sprite->image)->origin.vx, ((SpriteSource *)sprite->image)->origin.vy);
        break;
    case 0xC6:
        if (sprite->frame_bits.sequencer_owned == 1) {
            ((SpriteSequencer *)sprite->sequencer)->halfc = code[0];
        }
        break;
    case 0xC9: {
        s32 value;

        if (sprite->frame_bits.sequencer_owned == 1) {
            value = code[0] | (s16)(code[1] << 8);
            ((SpriteSequencer *)sprite->sequencer)->halfc = value;
        }
        break;
    }
    case 0xC5: {
        s32 n;

        n = code[0];
        n /= D_80059198 + 1;
        while (--n != -1) {
            func_80022CDC(sprite);
        }
        break;
    }
    case 0xB9:
        if ((SpriteVoice *)sprite->word50 != NULL) {
            func_80039E60(code[0] | (((SpriteVoice *)sprite->word50)->bank << 16));
        }
        break;
    case 0xB0:
        if (D_8005919C != NULL) {
            func_80039E60(code[0] | (D_8005919C->bank << 16));
        }
        break;
    case 0xCC:
        sprite->frames = sprite->script + ((s16)(code[1] << 8) | code[0]);
        break;
    case 0x8C:
        other = sprite->word74;
        from.vx = sprite->x >> 16;
        from.vy = sprite->z >> 16;
        to.vx = other->x >> 16;
        to.vy = other->z >> 16;
        direction = func_80023124(to, from);
        func_80021FE0(sprite, direction);
        func_800223B0(sprite, direction);
        break;
    case 0x94:
        other = sprite->word70;
        if ((sprite->render.word & 3) == 2) {
            sprite->renderer->angle_y = other->direction;
            sprite->render.bits.dirty = 1;
        }
        break;
    case 0xA7: {
        s32 n;

        if (code[0] & 0x80) {
            n = (code[0] & 0x7F) + 1;
            D_80059428 = n;
        } else {
            n = (code[0] + 1) * sprite->motion.bits.divisor / 256;
            if (n == 0) {
                n = 1;
            }
            sprite->countdown += n;
        }
        break;
    }
    case 0xFC:
        stack = func_80031BDC(0x2000, 0);
        STACK_ENTER(stack + 0x1F00);
        {
            s32 image_x, image_y;

            D_800592E4 = (s32 *)((((s8)code[2] << 16) + (code[1] << 8) + code[0]) + (s32)code);
            image_x = ((SpriteSource *)sprite->image)->origin.vx;
            image_y = ((SpriteSource *)sprite->image)->origin.vy;
            D_800592E8 = image_x;
            D_800592EA = image_y;
        }
        func_8001FB30();
        STACK_LEAVE();
        func_800320E8(stack);
        break;
    case 0xBF:
        sprite->height = code[0];
        break;
    case 0x96:
        func_8001CE74(sprite->block);
        break;
    case 0xA2:
        ((u8 *)&sprite->render)[1] = code[0];
        break;
    case 0xCD: {
        s32 value;
        u16 bits;
        s32 angle;
        s32 group;

        if (sprite->renderer != NULL) {
            value = code[0] | (s16)(code[1] << 8);
            bits = value;
            angle = (value & 0x1FF) * 8;
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
    case 0xC0: {
        s32 distance;
        s32 angle;

        distance = rand() & 0xFF;
        distance *= code[0];
        distance >>= 8;
        distance = distance * sprite->scale / 4096;
        angle = rand();
        sprite->x += func_80022CAC(sprite, func_8003F8CC(angle)) * distance * 16;
        sprite->z -= func_80022CAC(sprite, func_8003F8B0(angle)) * distance * 16;
        break;
    }
    case 0xC1: {
        s32 distance;

        distance = rand() & 0xFF;
        distance *= code[0];
        distance >>= 8;
        distance = distance * sprite->scale / 4096;
        func_80021B04(&vector, func_80022CAC(sprite, distance), 0, 0);
        func_80021B04(&angles, rand(), rand(), 0);
        func_80021B14(&sum, sprite->x >> 16, sprite->y >> 16, sprite->z >> 16);
        TransMatrix(&m, &sum);
        SetTransMatrix(&m);
        func_8003F738(&angles, &m);
        SetRotMatrix(&m);
        RotTransSV(&vector, &vector, &flag);
        sprite->x = vector.vx << 16;
        sprite->y = vector.vy << 16;
        sprite->z = vector.vz << 16;
        break;
    }
    case 0xBC: {
        u8 arg;
        u8 transform;
        s32 index;

        arg = code[0];
        index = arg & 0x3F;
        if (arg & 0x80) {
            transform = sprite->render.bits.no_view;
            switch (index) {
            case 38:
                vector.vx = D_800C3EB0[((sprite->motion.word & 3) << 2) | sprite->frame_bits.unknown30].x;
                vector.vz = D_800C3EB0[((sprite->motion.word & 3) << 2) | sprite->frame_bits.unknown30].z;
                vector.vy = 0;
                break;
            case 36:
                ((Task *)sprite->block)->link.word |= 0x40000000;
                goto own_position;
            case 37:
                ((Task *)sprite->block)->link.word &= ~0x40000000;
                goto own_position;
            case 23:
                ReadGeomOffset(&sum.vx, &sum.vy);
                vector.vx = (0xA0 - sum.vx) * 2;
                vector.vy = (0x70 - sum.vy) * 2;
                transform = 0;
                vector.vz = sprite->z >> 16;
                break;
            case 24:
            own_position:
                vector.vx = sprite->x >> 16;
                vector.vy = sprite->y >> 16;
                transform = 0;
                vector.vz = sprite->z >> 16;
                break;
            case 22:
                other = sprite->word70;
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                transform = 0;
                vector.vz = other->z >> 16;
                break;
            case 32:
                other = D_800C3E1C;
                goto focus_top;
            case 33:
                other = D_800C3E1C;
                goto focus_middle;
            case 34:
                other = D_800C3E1C;
                goto focus_depth;
            case 35:
                other = D_800C3E1C;
                goto focus_half_depth;
            case 18:
                other = sprite->word74;
            focus_top:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->height;
                break;
            case 19:
                other = sprite->word74;
            focus_middle:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->height - (other->height - other->extent_depth) / 2;
                break;
            case 20:
                other = sprite->word74;
            focus_depth:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->extent_depth;
                break;
            case 21:
                other = sprite->word74;
            focus_half_depth:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                vector.vy -= other->extent_depth - (u16)other->extent_depth / 2;
                break;
            case 6:
                vector.vx = D_8006F99C.vx >> 16;
                vector.vy = D_8006F99C.vy >> 16;
                vector.vz = D_8006F99C.vz >> 16;
                break;
            case 7:
                vector.vx = D_8006F9AC.vx >> 16;
                vector.vy = D_8006F9AC.vy >> 16;
                vector.vz = D_8006F9AC.vz >> 16;
                break;
            case 1:
                other = D_800C3E1C;
                goto focus_position;
            case 9:
                index = 11;
                goto own_group;
            case 8:
                index = 12;
                goto own_group;
            case 10:
                index = 13;
                goto own_group;
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
                other = sprite->word70;
                if (other == NULL) {
                    break;
                }
                goto group_place;
            own_group:
                other = sprite->word74;
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
                func_80021B04(&vector, x + (other->x >> 16), y + (other->y >> 16), other->z >> 16);
                break;
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
                other = D_800C3E1C;
                index -= 14;
                goto group_place;
            case 0:
                other = sprite->word74;
            focus_position:
                vector.vx = other->x >> 16;
                vector.vy = other->y >> 16;
                vector.vz = other->z >> 16;
                break;
            case 2: {
                s32 members;

                func_80021B14(&sum, 0, 0, 0);
                for (members = 0; (other = D_800D363C[members]) != NULL; members++) {
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
            case 3:
                other = D_800C3E1C;
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
            case 4: {
                s32 members;

                func_80021B14(&sum, 0, 0, 0);
                for (members = 0; (other = D_800D363C[members]) != NULL; members++) {
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
            case 5:
                func_80021B14(&sum, 0, 0, 0);
                sum.vx /= count;
                sum.vy /= count;
                sum.vz /= count;
                vector.vx = sum.vx >> 16;
                vector.vy = sum.vy >> 16;
                vector.vz = sum.vz >> 16;
                break;
            }
            if (transform) {
                ApplyMatrixSV(&D_8004FBB8, &vector, &vector);
                vector.vx += D_8004FBB8.t[0];
                vector.vy += D_8004FBB8.t[1];
                vector.vz += D_8004FBB8.t[2];
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

            other = sprite->word70;
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
    case 0xD1:
        *func_8001FBA4(sprite, code) *= *func_8001FBA4(sprite, code + 1);
        break;
    case 0xD2:
    case 0xD5:
        *func_8001FBA4(sprite, code) /= *func_8001FBA4(sprite, code + 1);
        break;
    case 0xE5:
        p = func_8001FBA4(sprite, code);
        *p = (s32)((rand() & 0xFF) * code[1]) >> 8;
        break;
    case 0xD6:
        *func_8001FBA4(sprite, code) += code[1];
        break;
    case 0xD7:
        *func_8001FBA4(sprite, code) *= (s8)code[1];
        break;
    case 0xD8:
        *func_8001FBA4(sprite, code) /= (s8)code[1];
        break;
    case 0xD9:
        *func_8001FBA4(sprite, code) <<= (s8)code[1];
        break;
    case 0xDA:
        *(s8 *)func_8001FBA4(sprite, code) >>= (s8)code[1];
        break;
    case 0xDB:
        {
            u8 *half = func_8001FBA4(sprite, code);
            s32 value;

            value = ((half[1] << 8) | half[0]) << (s8)code[1];
            half[0] = value;
            half[1] = value >> 8;
        }
        break;
    case 0xDC:
        {
            u8 *half = func_8001FBA4(sprite, code);
            s32 value;

            value = ((half[1] << 8) | half[0]) >> (s8)code[1];
            half[0] = value;
            half[1] = value >> 8;
        }
        break;
    case 0xD0:
    case 0xD3:
    case 0xDD:
    case 0xDE:
        *func_8001FBA4(sprite, code) += *func_8001FBA4(sprite, code + 1);
        break;
    case 0xA4:
        func_800245D8(sprite->word74, (s8)code[0]);
        break;
    case 0xDF:
        *func_8001FBA4(sprite, code) = code[1];
        break;
    case 0xE6:
        {
            u8 *half = func_8001FBA4(sprite, code);

            half[0] = code[1];
            half[1] = 0;
        }
        break;
    case 0x91:
        sprite->colour_flags &= ~1;
        func_8001F6B0(sprite);
        break;
    case 0x92:
        sprite->colour_flags |= 1;
        func_8001F6B0(sprite);
        break;
    case 0xBB:
        sprite->half30 += (s8)code[0];
        break;
    case 0x93: {
        s32 i;

        other = sprite->word70;
        if (other != NULL && (sprite->render.word & 3)) {
            if (func_8001EE68((u8 *)((SpriteSource *)sprite->image)->frames) == 0) {
                sprite->flags = (sprite->flags & ~0x1E000) | 0x1C000;
            }
            if (sprite->renderer != NULL && other->renderer->pointer34 != NULL) {
                func_8001D4E8(sprite);
                for (i = 0; i != 8; i++) {
                    sprite->renderer->pointer34[i] = other->renderer->pointer34[i];
                }
                sprite->renderer->offset = other->renderer->offset;
            }
            func_8001D2B0(sprite, sprite->frame);
        }
        break;
    }
    case 0xBA:
        func_80023290(sprite, code[0]);
        break;
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
            func_8001F6B0(sprite);
        }
        break;
    }
    case 0xF2:
        model = (SpriteModelRenderer *)sprite->renderer;
        sprite->red = func_80021AD8(sprite->red, (s8)code[0]);
        sprite->green = func_80021AD8(sprite->green, (s8)code[1]);
        sprite->blue = func_80021AD8(sprite->blue, (s8)code[2]);
        if ((sprite->render.word & 3) == 2) {
            model->red += (s8)code[0];
            model->green += (s8)code[1];
            model->blue += (s8)code[2];
        }
        if ((sprite->render.word & 3) == 1) {
            func_8001F6B0(sprite);
        }
        if (((sprite->flags >> 13) & 0xF) == 0xF && ((SpriteModelRenderer *)sprite->renderer)->model != NULL &&
            !((sprite->flags >> 1) & 1)) {
            func_800B2AEC(((SpriteModelRenderer *)sprite->renderer)->model,
                          ((SpriteModelRenderer *)sprite->renderer)->packets[0],
                          ((SpriteModelRenderer *)sprite->renderer)->packets[1], model->red, model->green, model->blue);
        }
        break;
    case 0x90:
        if (sprite->resource_block == sprite->animations) {
            func_800222BC(sprite, (s32 *)sprite->resource);
            sprite->b0.wordb0 |= 0x400;
        } else {
            func_800222BC(sprite, sprite->animations);
            sprite->b0.wordb0 &= ~0x400;
        }
        break;
    case 0xF5:
        func_80032498(5, 0);
        buffer = (s8)code[2] << 16;
        buffer += code[1] << 8;
        buffer += code[0];
        buffer += (s32)code;
        func_8002C59C((SpriteModel *)buffer);
        if (((SpriteModelRenderer *)sprite->renderer)->packets[0] != NULL) {
            func_800320E8(((SpriteModelRenderer *)sprite->renderer)->packets[0]);
        }
        func_8002CB54((ModelBuffer *)buffer, &((SpriteModelRenderer *)sprite->renderer)->packets[0],
                      &((SpriteModelRenderer *)sprite->renderer)->packets[1]);
        func_8002C8CC((ModelBuffer *)buffer, ((SpriteModelRenderer *)sprite->renderer)->packets[0], 0);
        memcpy(((SpriteModelRenderer *)sprite->renderer)->packets[1],
               ((SpriteModelRenderer *)sprite->renderer)->packets[0], ((ModelBuffer *)buffer)->size);
        ((SpriteModelRenderer *)sprite->renderer)->model = (ModelBuffer *)buffer;
        break;
    case 0xF6:
        func_80032498(5, 0);
        buffer = (s8)code[2] << 16;
        buffer += code[1] << 8;
        buffer += code[0];
        buffer += (s32)code;
        func_8002C3E8((ModelGroup *)buffer);
        buffer += 0x10;
        if (((SpriteModelRenderer *)sprite->renderer)->packets[0] != NULL) {
            func_800320E8(((SpriteModelRenderer *)sprite->renderer)->packets[0]);
        }
        func_8002CB54((ModelBuffer *)buffer, &((SpriteModelRenderer *)sprite->renderer)->packets[0],
                      &((SpriteModelRenderer *)sprite->renderer)->packets[1]);
        func_8002C8CC((ModelBuffer *)buffer, ((SpriteModelRenderer *)sprite->renderer)->packets[0], 0);
        memcpy(((SpriteModelRenderer *)sprite->renderer)->packets[1],
               ((SpriteModelRenderer *)sprite->renderer)->packets[0], ((ModelBuffer *)buffer)->size);
        ((SpriteModelRenderer *)sprite->renderer)->model = (ModelBuffer *)buffer;
        break;
    case 0xF7:
        buffer = (s8)code[2] << 16;
        buffer += code[1] << 8;
        buffer += code[0];
        buffer += (s32)code;
        func_8002C3E8((ModelGroup *)buffer);
        buffer += 0x10;
        if (((SpriteModelRenderer *)sprite->renderer)->packets[0] != NULL) {
            func_800320E8(((SpriteModelRenderer *)sprite->renderer)->packets[0]);
        }
        func_8002CB54((ModelBuffer *)buffer, &((SpriteModelRenderer *)sprite->renderer)->packets[0],
                      &((SpriteModelRenderer *)sprite->renderer)->packets[1]);
        func_8002C8CC((ModelBuffer *)buffer, ((SpriteModelRenderer *)sprite->renderer)->packets[0], 0);
        memcpy(((SpriteModelRenderer *)sprite->renderer)->packets[1],
               ((SpriteModelRenderer *)sprite->renderer)->packets[0], ((ModelBuffer *)buffer)->size);
        *(s32 *)(loaded->unk10 + 4) = 0;
        ((SpriteModelRenderer *)sprite->renderer)->model = (ModelBuffer *)buffer;
        break;
    case 0xB5:
        {
            s16 scale = code[0] << 8;

            if (sprite->render.word & 3) {
                func_80022000(sprite, scale);
            }
        }
        break;
    case 0xE7:
        func_80022000(sprite, sprite->scale + (s16)((code[0] | (s16)(code[1] << 8)) * 2));
        break;
    case 0xE9: {
        s32 value;

        value = (code[0] | (s16)(code[1] << 8)) * 2;
        if (sprite->renderer != NULL) {
            sprite->renderer->scale_x += value;
            sprite->render.bits.dirty = 1;
        }
        break;
    }
    case 0xEA: {
        s32 value;

        value = (code[0] | (s16)(code[1] << 8)) * 2;
        if (sprite->renderer != NULL) {
            sprite->renderer->scale_y += value;
            sprite->render.bits.dirty = 1;
        }
        break;
    }
    case 0xEB: {
        s32 value;

        value = (code[0] | (s16)(code[1] << 8)) * 2;
        if (sprite->renderer != NULL) {
            sprite->renderer->scale_z += value;
            sprite->render.bits.dirty = 1;
        }
        break;
    }
    case 0xBD:
        func_80023B84(sprite, (u8 *)(((u16 *)D_8006BE20)[code[0] + 1] + (s32)D_8006BE20), sprite->image);
        break;
    case 0xE0:
        func_80023B84(sprite, code + (((s8)code[1] << 8) + code[0]), sprite->image);
        break;
    case 0xAD:
        sprite->frame_bits.bounce = code[0];
        break;
    case 0xB4:
        func_80021CA0(sprite, code[0]);
        break;
    case 0xB8:
        sprite->stack_top -= (s8)code[0];
        break;
    case 0xB3:
        sprite->frame_bits.frame = (s8)code[0];
        break;
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
    case 0xB6:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_x += (s16)(code[0] << 8) >> 4;
            sprite->render.bits.dirty = 1;
        }
        break;
    case 0xB7:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_y += (s16)(code[0] << 8) >> 4;
            sprite->render.bits.dirty = 1;
        }
        break;
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
    case 0xC4:
        n = (rand() & 0xFF) * code[0] / 256;
        func_80021B04(&vector, 0, 0, (n - (code[0] >> 1)) * 16);
        func_8003F738(&vector, &m);
        ApplyMatrixLV(&m, (VECTOR *)&sprite->speed_x, &sum);
        sprite->speed_x = sum.vx;
        sprite->speed_y = sum.vy;
        sprite->speed_z = sum.vz;
        break;
    case 0xAC:
        func_80021FE0(sprite, sprite->direction + ((rand() & 0xFF) * code[0] / 256 - (code[0] >> 1)) * 16);
        break;
    case 0xA9:
        value = func_80022CAC(sprite, (s8)code[0] * sprite->scale / 4096) << 16;
        if ((sprite->motion.word >> 2) & 1) {
            value = -value;
        }
        sprite->x += value;
        break;
    case 0xAA:
        sprite->y += func_80022CAC(sprite, (s8)code[0] * sprite->scale / 4096) << 16;
        break;
    case 0xAB:
        sprite->z += func_80022CAC(sprite, (s8)code[0] * sprite->scale / 4096) << 16;
        break;
    case 0xA8:
        sprite->direction += (s16)(code[0] << 8) >> 4;
        func_80022974(sprite);
        break;
    case 0x8A:
        sprite->speed_x = 0;
        sprite->speed_z = 0;
        sprite->speed = 0;
        break;
    case 0xA3:
        if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->word4 != 0) {
            sprite->word1c = ((SpriteSequencer *)sprite->sequencer)->word4;
        } else {
            sprite->word1c = (((s8)code[0] << 6) * (s16)sprite->word82 / 4096) << 5;
            n = 0x10000 / sprite->motion.bits.divisor;
            sprite->word1c *= n * n / 256;
            sprite->word1c /= 256;
            sprite->word1c *= (D_80059198 + 1) * (D_80059198 + 1);
        }
        break;
    case 0xA5:
        sprite->speed += (((s8)code[0] << 4) * (D_80059198 + 1) * (s16)sprite->word82 / 4096) << 8;
        func_80022974(sprite);
        break;
    case 0xA6:
        if (sprite->frame_bits.sequencer_owned != 1) {
            sprite->speed_y +=
                ((((s8)code[0] << 4) * (D_80059198 + 1) * (s16)sprite->word82 / 4096) << 16) / sprite->motion.bits.divisor;
        }
        break;
    case 0xA0:
        sprite->speed = (((s8)code[0] << 4) * (D_80059198 + 1) * (s16)sprite->word82 / 4096) << 8;
        func_80022974(sprite);
        break;
    case 0xA1:
        if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->word0 != 0) {
            sprite->speed_y = ((SpriteSequencer *)sprite->sequencer)->word0;
        } else {
            sprite->speed_y = (((s8)code[0] << 4) * (D_80059198 + 1) * (s16)sprite->word82 / 4096) << 8;
        }
        sprite->speed_y <<= 8;
        sprite->speed_y /= sprite->motion.bits.divisor;
        break;
    case 0xED:
        sprite->x = (code[0] | ((s8)code[1] << 8)) << 16;
        break;
    case 0xEE:
        sprite->y = (sprite->ground + (code[0] | ((s8)code[1] << 8)) * sprite->scale / 4096) << 16;
        break;
    case 0xEF:
        sprite->z = (code[0] | ((s8)code[1] << 8)) << 16;
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FBE4);
#endif

/* `value + delta` clamped to 0-255. */
s32 func_80021AD8(s32 value, s32 delta) {
    value += delta;
    if (value >= 0x100) {
        value = 0xFF;
    } else if (value < 0) {
        value = 0;
    }
    return value;
}

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
u8 func_80021C20(Sprite *sprite) {
    u8 value = sprite->stack[sprite->stack_top];

    sprite->stack_top += 1;
    return value;
}

/* Pop a halfword from a sprite's stack. */
s16 func_80021C3C(Sprite *sprite) {
    s16 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8);

    sprite->stack_top += 2;
    return value;
}

/* Pop three bytes from a sprite's stack. */
s32 func_80021C6C(Sprite *sprite) {
    s32 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8) +
                (sprite->stack[sprite->stack_top + 2] << 16);

    sprite->stack_top += 3;
    return value;
}

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

/* Restore a sprite from a snapshot: its state, then advance its animation
 * (moving it) until the saved step, then its saved position; frame skipping
 * is off meanwhile. */
void func_80021D50(Sprite *sprite, SpriteState *state) {
    s32 skip = D_80059198;

    D_80059198 = 0;
    sprite->word80 = state->word80;
    sprite->motion.bytes[3] = state->byteaf;
    sprite->b0.byteb0 = state->byteb0;
    sprite->renderer->scale_x = state->scale_x;
    sprite->renderer->scale_y = state->scale_y;
    sprite->renderer->scale_z = state->scale_z;
    sprite->word82 = state->word82;
    sprite->scale = state->scale;
    func_800245D8(sprite, (s8)sprite->motion.bytes[3]);
    while (sprite->frame_bits.field22 != state->field22) {
        func_80023210(sprite);
        sprite->x += sprite->speed_x;
        sprite->z += sprite->speed_z;
        sprite->y += sprite->speed_y;
        sprite->speed_y += sprite->word1c;
    }
    sprite->x = state->x;
    sprite->y = state->y;
    sprite->z = state->z;
    ((SpriteSequencer *)sprite->sequencer)->word0 = state->sequencer0;
    ((SpriteSequencer *)sprite->sequencer)->word4 = state->sequencer4;
    D_80059198 = skip;
}

/* Save a sprite's position and animation state. */
void func_80021EBC(Sprite *sprite, SpriteState *state) {
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

void func_80021FB8(Sprite *sprite, u8 value) {
    sprite->b0.byteb0 = value;
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
void func_80022000(Sprite *sprite, s32 scale) {
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
