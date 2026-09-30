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
/* Nonmatching: the original loads the serial counter and list head before the link word. */
#ifdef NON_MATCHING
void func_8001CA58(Task *owner, Task *node) {
    TaskLink link;
    Task *head = D_80059594;

    node->owner = owner;
    node->next = head;
    D_80059594 = node;
    link = node->link;
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CA58);
#endif

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
/* Nonmatching: id-word scheduling as in 8001ca58, and the active count (absolute) is kept in a register. */
#ifdef NON_MATCHING
void func_8001CC18(Task *owner, Task *node) {
    node->owner = owner;
    node->destroy = func_8001CD94;
    node->update = NULL;
    node->next = D_8005958C;
    D_8005958C = node;
    node->id.bits.serial = D_80059184++;
    node->link.bits.owner_serial = owner->id.bits.serial;
    node->link.bits.flag29 = 0;
    node->link.bits.flag30 = 0;
    node->link.bits.active = 0;
    if (D_800591AC != 0) {
        D_80059464++;
        node->link.bits.active = 1;
    } else {
        node->link.bits.active = 0;
    }
    D_80059188++;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CC18);
#endif

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
            ((Sprite *)task->data)->word70 = 0;
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
/* Nonmatching: the original loads the resident page table address after the abr argument's sign extension (lui/addiu into one register); here it is split and scheduled first. */
#ifdef NON_MATCHING
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
            cell++;
            kind |= *cell << 8;
            pages = D_8004FAB8;
            page = (TexturePosition *)(((kind << 1) & 0x1C) + (s32)pages);
            parts[i].tpage = GetTPage(kind & 1, rate, page->x, page->y);
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D53C);
#endif

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
/* Nonmatching: the original keeps the view matrix address in $s1 and reuses the sprite register for the renderer matrix; GCC folds the address into each access. */
#ifdef NON_MATCHING
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
    offset_y = sprite->renderer->offset_y << shift;
    offset_x = sprite->renderer->offset_x << shift;
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001E148);
#endif

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
/* Nonmatching: the original frame has 12 more bytes of locals between the angles and the RotTransPers4 outputs (an 8-byte aggregate and an addressable word no code uses), and it keeps the part width in $t0 and the column span in $a3 (swapped here). */
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
    origin_y = renderer->offset_y;
    origin_x = renderer->offset_x;
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
u8 func_8001EE68(u8 *frame) {
    return frame[1] >> 7;
}

/* The part count of a frame header (bits 9-14). */
s32 func_8001EE74(u16 *header) {
    return (*header >> 9) & 0x3F;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001EE88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F1D4);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FBE4);

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
