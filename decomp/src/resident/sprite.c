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

/* The unit's own small globals ($gp-relative; the assembler knows them as
 * this unit's small commons). */
u32 D_80059184;
s32 D_80059188;
s32 D_8005918C;
Sprite *D_80059190;
s16 D_80059194; /* texture area row (0-2) of the next image */
s16 D_80059196; /* texture area column of the next image */
s32 D_800591B8;
s32 D_800592EC;
s32 D_800592F8;               /* the queue being filled (0 or 1) */
s32 D_800592FC;               /* bytes of the queue entry block / 2 */
SpriteQueueEntry *D_80059300[2]; /* the two queues */
u8 *D_800594B4;               /* the queue entry block */
s32 D_800594C4;
SpriteQueueEntry *D_80059580; /* the next free queue entry */
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
    D_80059428[0] = 0;
}

/* Run the main task list, unless it is paused (count the pause down). */
/* Nonmatching: the original addresses the pause count absolutely each time; as a
 * sizeless extern (needed to keep it off $gp here) GCC keeps its address in a register. */
#ifdef NON_MATCHING
void func_8001C964(void) {
    Task *task;

    if (D_80059428[0] != 0) {
        if (--D_80059428[0] == 0) {
            D_80059494[0] = 0;
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001C964);
#endif

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
/* Nonmatching: the original loads the serial counter and list head first and builds the id words in another order. */
#ifdef NON_MATCHING
void func_8001CA58(Task *owner, Task *node) {
    node->owner = owner;
    node->next = D_80059594;
    D_80059594 = node;
    node->link.bits.owner_serial = owner->id.bits.serial;
    node->id.bits.serial = D_80059184++;
    node->link.bits.flag29 = 0;
    node->link.bits.flag30 = 0;
    node->link.bits.active = 0;
    node->update = NULL;
    node->destroy = func_8001CB48;
    D_8005918C++;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CA58);
#endif

/* Allocate a task with `size` bytes after its node on the second list. */
Task *func_8001CAF0(Task *owner, s32 size) {
    Task *node = func_80031BDC(size + sizeof(Task), D_800591AF[0]);

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
    if (D_800591AC[0] != 0) {
        D_80059464[0]++;
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
    Task *node = func_80031BDC(size + sizeof(Task), D_800591AF[0]);

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
/* Nonmatching: the active count, addressed absolutely, is kept in a register here. */
#ifdef NON_MATCHING
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
        D_80059464[0]--;
    }
    D_80059188--;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CD94);
#endif

/* Destroy callback of an allocated main-list task: unlink and free it. */
void func_8001CE44(Task *task) {
    func_8001CD94(task);
    func_800320E8(task);
}

/* Destroy every task `owner` created (both lists). */
/* Nonmatching: the original keeps the owner in $s3 and the serial mask in $s2; GCC swaps them. */
#ifdef NON_MATCHING
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001CE74);
#endif

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
    Task *node = func_80031BDC(size, D_800591AF[0]);

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
/* Nonmatching: in the pending-list scan the original builds the two image addresses through $v0 and walks the list in $v1. */
#ifdef NON_MATCHING
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
                    func_8001F8E8(sprite, sprite->frame);
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D2B0);
#endif

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

    for (sprite = D_80059190; sprite != NULL; sprite = sprite->renderer->next_pending) {
        if (sprite->frame == 0) {
            sprite->flags &= ~0xFC;
        } else {
            func_8001DAE8(sprite, sprite->frame, sprite->image);
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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001D53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001DAE8);

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

    if (D_800591AD[0] != 0 || D_800591AE[0] != 0) {
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

/* Build frame `frame` with 8001e3d8 (and 8001e9bc for render flag 2). */
void func_8001E298(Sprite *sprite, s32 frame) {
    func_8001E148(sprite);
    func_8001E3D8(sprite, frame);
    if ((sprite->render.word >> 2) & 1) {
        func_8001E9BC(sprite, frame);
    }
}

/* Build frame `frame` from `image` with 8001ee88 (and 8001e9bc for render flag 2). */
void func_8001E2F8(Sprite *sprite, s32 frame, void *image) {
    func_8001E148(sprite);
    func_8001EE88(sprite, frame, image);
    if ((sprite->render.word >> 2) & 1) {
        func_8001E9BC(sprite, frame);
    }
}

/* Build frame `frame` from `image` with 8001f1d4 (and 8001e9bc for render flag 2). */
void func_8001E368(Sprite *sprite, s32 frame, void *image) {
    func_8001E148(sprite);
    func_8001F1D4(sprite, frame, image);
    if ((sprite->render.word >> 2) & 1) {
        func_8001E9BC(sprite, frame);
    }
}

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

/* Reserve `width` columns of the sprite texture area (three 64-line rows from
 * (0x300, 0x140), 0x40 columns each) and return their position. */
/* Nonmatching: the original copies the width argument to $a3 and stores the column after the result copy. */
#ifdef NON_MATCHING
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F530);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F5BC);

/* Recolour a one-sided sprite's parts: the sprite's colour word and blend
 * mode (its blend rate - 1). */
/* Nonmatching: same instructions; the original numbers its registers differently (sprite in $t1, part in $a0). */
#ifdef NON_MATCHING
void func_8001F6B0(Sprite *sprite) {
    SpriteImageSize size;
    SpritePart *part;
    u32 colour;
    s32 blend;
    s32 i;

    if ((sprite->render.word & 3) != 1) {
        return;
    }
    blend = (sprite->render.word >> 5) & 7;
    if (blend != 0) {
        blend--;
    }
    size = ((SpriteImage *)sprite->image)->size;
    colour = *(u32 *)&sprite->red;
    part = sprite->renderer->part_cursor;
    for (i = 0; i != (u8)sprite->flags >> 2; i++) {
        part[i].colour = colour;
        part[i].tpage = (part[i].tpage & 0xFF9F) | (blend << 5);
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F6B0);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001F8E8);

/* Unpack and upload the image at 8004fbd8 to (x, y). */
/* Nonmatching: the original keeps x in $s1 and the image in $s0; GCC swaps them. */
#ifdef NON_MATCHING
void func_8001FAB4(s32 x, s32 y) {
    void *image = func_80032E88(D_8004FBD8, 0);

    func_8002DDE4(image, 1, x, y, 0, 0, 0);
    DrawSync(0);
    func_800320E8(image);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FAB4);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8001FB30);

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

/* Restore a sprite from a snapshot: its state, then advance its animation
 * (moving it) until the saved step, then its saved position; frame skipping
 * is off meanwhile. */
/* Nonmatching: the original addresses the frame-skip word absolutely for the load and the store (GCC keeps its address in a register) and reads the two saved bytes with lbu. */
#ifdef NON_MATCHING
void func_80021D50(Sprite *sprite, SpriteState *state) {
    s32 skip = D_80059198.skip;

    D_80059198.skip = 0;
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
    D_80059198.skip = skip;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80021D50);
#endif

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

/* Resolve a resource block's section offsets into `resource`; with flag
 * 800591ad set, a nonzero field of the first section's first halfword (bits
 * 6-11) goes to 800591b3. */
/* Nonmatching: the original schedules the flag load before the stores and copies the origin through one register. */
#ifdef NON_MATCHING
void func_80022224(SpriteResource *resource, u8 *data, SVECTOR origin) {
    s32 value;

    resource->origin = origin;
    resource->section3 = data + ((s32 *)data)[3];
    resource->section2 = data + ((s32 *)data)[2];
    D_800591B0[0] = 0;
    resource->section1 = (u16 *)(data + ((s32 *)data)[1]);
    if (D_800591AD[0] != 0) {
        value = (*resource->section1 >> 6) & 0x3F;
        if (value != 0) {
            D_800591B3[0] = value;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80022224);
#endif

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

    if (sprite->renderer != NULL && sprite->renderer->parts != NULL) {
        func_80025180((u32)sprite->renderer->parts);
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
    if (sprite->renderer->parts != NULL) {
        func_800320E8(sprite->renderer->parts);
    }
    sprite->renderer->part_cursor = sprite->renderer->parts = func_80031BDC(count * 24, from_top);
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
/* Nonmatching: the original compares the bit with 1 (li/bne); GCC tests it against zero. */
#ifdef NON_MATCHING
void func_800230A8(Sprite *sprite) {
    if (sprite->frame_bits.sequencer_owned == 1 && ((SpriteSequencer *)sprite->sequencer)->buffer != NULL) {
        func_800320E8(((SpriteSequencer *)sprite->sequencer)->buffer);
    }
    func_8001D3F4(sprite);
    func_800320E8(sprite->renderer->parts);
    func_800320E8(sprite);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800230A8);
#endif

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

/* A property (0, 1 or 2) of kind `kind`; `fallback` for kind 3 and the rest. */
/* Nonmatching: the original loads the jump table address with la and adds the index; GCC 2.7.2 indexes the symbol through $at (as in 80025224). */
#ifdef NON_MATCHING
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023468);
#endif

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
    sprite->renderer->pointer34 = (SpriteRendererEntry *)((u8 *)sprite + 0x124);
    sprite->image = (u8 *)sprite + 0x110;
    sprite->renderer->next_pending = NULL;
}

/* Give a sprite its inline renderer with an inline part list. */
void func_800239F4(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->renderer->part_cursor = (SpritePart *)((u8 *)sprite + 0xF4);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->next_pending = NULL;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023A48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023B84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80023FD8);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_8002435C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024524);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800245D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800248D4);

/* Reset the sprite engine: flags, the value at 800591a8, the task lists and
 * the pending-frame list. */
void func_80024F20(void) {
    D_800591AD[0] = 0;
    D_800591AE[0] = 0;
    D_800591A8[0] = 0x2000;
    func_8001C944();
    func_8001D298();
}

/* Allocate the queue entry block (`size` * 2 bytes) and empty both queues. */
void func_80024F64(s32 size, s32 mode) {
    D_800592FC = size;
    D_800594B4 = func_80031BDC(size * 2, mode);
    D_800594B8[0] = D_800594B4 + size;
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
    D_8005956C[0] = value;
}

/* Copy the current light settings. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
void func_80024FF4(MATRIX *view) {
    D_8004FBB8 = *view;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80024FF4);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025044);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_800250E0);

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
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite", func_80025180);
#endif

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
