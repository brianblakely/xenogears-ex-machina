/* Resident sprite engine, third unit (0x800248d4-0x8002709c): the
 * animation script interpreter, the image queues and sprite drawing. Built
 * like the first unit (sprite.c). Its jump table (0x800186e0) directly
 * follows the second unit's 15-entry table at 0x800186a4 without the pad
 * GCC's `.align 3` would give within one unit, and the small globals below
 * are addressed through $gp only from here, the second unit's only there. */
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
s32 D_800592F8;               /* the queue being filled (0 or 1) */
s32 D_800592FC;               /* bytes of the queue entry block / 2 */
SpriteQueueEntry *D_80059300[2]; /* the two queues */
u8 *D_800594B4;               /* the first queue's entry block (the second's follows) */
ImageUpload *D_800594C4;      /* the upload list of the first queue (the second follows) */
SpriteQueueEntry *D_80059580; /* the next free queue entry */
u8 *D_80059524;                /* the queue block being filled */
u8 *D_80059534;                /* its end */

/* Run a sprite's animation script until a command takes time: frame
 * commands (below 80) show a frame for their duration scaled by the gravity
 * divisor; be shows a frame with its flip bits and duration; the others
 * end, loop or restart the animation, wait on the sprite's motion or its
 * creator, branch, call and return (e2, 85), count loops (e4) or run
 * through 8001fbe4. In battle (800591ad) the battle overlay runs it. */
void func_800248D4(Sprite *sprite) {
    u8 *script;
    u8 *args;
    u8 op;
    s32 duration;
    s32 value;
    s32 time;
    s32 wait;
    s16 offset;
    s32 frame;
    s32 speed;
    Sprite *creator;
    u8 count;

    if (D_800591AD != 0) {
        func_800C11CC(sprite);
        return;
    }
next:
    if (sprite->countdown != 0) {
        return;
    }
    script = sprite->script;
    op = *script;
    args = script + 1;
    if (op < 0x80) {
        sprite->script = args;
        if (op < 0x10) {
            func_8001D2B0(sprite, sprite->frame + 1);
            duration = (op & 0xF) + 1;
        } else if (op < 0x20) {
            sprite->frame_bits.frame++;
            func_80022D44(sprite);
            duration = (op & 0xF) + 1;
        } else if (op < 0x30) {
            func_8001D2B0(sprite, sprite->frame - 1);
            duration = (op & 0xF) + 1;
        }
        if (op < 0x40) {
            duration = (op & 0xF) + 1;
        }
        duration = duration * sprite->motion.bits.divisor / 256;
        if (duration == 0) {
            duration = 1;
        }
        sprite->countdown += duration;
        if (++sprite->frame_bits.field22 == 0) {
            sprite->frame_bits.field22--;
        }
        return;
    }
    switch (op) {
    case 0xBE:
        value = args[0] | ((s8)args[1] << 8);
        time = ((value >> 11) & 0xF) + 1;
        frame = value & 0x1FF;
        time = time * sprite->motion.bits.divisor / 256;
        if (time == 0) {
            time = 1;
        }
        if (sprite->render.bits.sides != 1) {
            sprite->frame = frame;
            sprite->countdown += time;
            sprite->script += 3;
            return;
        }
        if (value < 0 && frame != 0) {
            frame = ((u8 *)sprite->word60)[frame - 1];
        }
        sprite->motion.bits.frame_flip = value >> 9;
        sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
        if ((value >> 10) & 1) {
            sprite->render.bits.flip_y = 1;
        } else {
            sprite->render.bits.flip_y = 0;
        }
        func_8001D2B0(sprite, frame);
        sprite->countdown += time;
        sprite->script += 3;
        return;
    case 0x8E:
        sprite->script = NULL;
        return;
    case 0xE2:
        offset = args[0] + ((s8)args[1] << 8);
        func_80021CF8(sprite, (s32)(sprite->script + 3));
        sprite->script += offset;
        goto next;
    case 0x85:
        sprite->script = (u8 *)(((u32)sprite->script & 0xFF000000) | func_80021C6C(sprite));
        goto next;
    case 0xFA:
        if (*func_8001FBA4(sprite, args) == 0) {
            break;
        }
        sprite->script += (s16)(args[1] | ((s8)args[2] << 8));
        goto next;
    case 0xD4:
        sprite->script += (s16)(args[0] | ((s8)args[1] << 8));
        if (sprite->callback != NULL) {
            sprite->callback(sprite);
        }
        goto next;
    case 0x86:
        if (sprite->speed_y >= 0) {
            break;
        }
        sprite->countdown = 1;
        return;
    case 0x87:
        if ((s16)(sprite->y >> 16) >= sprite->ground) {
            break;
        }
        sprite->countdown = 1;
        return;
    case 0x80:
    end:
        sprite->frame_bits.field28 = 0;
        if (sprite->callback != NULL) {
            sprite->callback(sprite);
            return;
        }
        if ((s8)sprite->b0.byteb0 >= 0) {
            func_800245D8(sprite, (s8)sprite->b0.byteb0);
        }
        sprite->frame_bits.field28 = 0;
        return;
    case 0x98:
        creator = (Sprite *)sprite->word70;
        if (creator == NULL) {
            break;
        }
        sprite->countdown = 1;
        if ((s8)creator->motion.bytes[3] != (s8)sprite->unknown8d) {
            break;
        }
        if (creator->frame_bits.field28 == 2) {
            return;
        }
        break;
    case 0x82:
        if (sprite->callback != NULL) {
            sprite->callback(sprite);
        }
        speed = sprite->speed_y;
        func_800245D8(sprite, (s8)sprite->motion.bytes[3]);
        sprite->speed_y = speed;
        sprite->countdown = 0;
        func_800248D4(sprite);
        return;
    case 0x81:
        if ((s8)sprite->motion.bytes[3] == 0x3F) {
            goto end;
        }
        sprite->countdown = 0;
        if (sprite->callback != NULL) {
            sprite->callback(sprite);
        }
        sprite->frame_bits.field28 = 1;
        return;
    case 0xE4:
        count = func_80021C20(sprite);
        if (count == 0) {
            break;
        }
        count--;
        func_80021CA0(sprite, count);
        sprite->script += (s16)(args[0] | ((s8)args[1] << 8));
        goto next;
    case 0xE1:
        sprite->script += (s16)(args[0] | ((s8)args[1] << 8));
        goto next;
    case 0xA7:
        sprite->script += D_8004FC40[op];
        if (args[0] & 0x80) {
            D_80059428 = (args[0] & 0x7F) + 1;
            sprite->countdown++;
            return;
        }
        wait = (args[0] + 2) * sprite->motion.bits.divisor / 256;
        if (wait == 0) {
            wait = 1;
        }
        sprite->countdown += wait;
        return;
    case 0xC8:
        func_8001FBE4(sprite, args[0], func_8001FBA4(sprite, args + 1));
        break;
    default:
        func_8001FBE4(sprite, op, args);
        break;
    }
    sprite->script += D_8004FC40[op];
    goto next;
}

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
    D_800594C4 = NULL;
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

/* Run the uploads queued on the queue being filled and empty its list. */
void func_80025044(void) {
    ImageUpload *upload;

    for (upload = (&D_800594C4)[D_800592F8]; upload != NULL; upload = upload->next) {
        if (upload->pixels != NULL) {
            LoadImage(&upload->rect, upload->pixels);
        } else {
            ClearImage(&upload->rect, 0, 0, 0);
        }
    }
    (&D_800594C4)[D_800592F8] = NULL;
}

/* Start filling queue `queue`: its entry block becomes the free space, and
 * the blocks its entries hold are released. */
/* Nonmatching: the original computes the index before the entry array's address. */
#ifdef NON_MATCHING
void func_800250E0(s32 queue) {
    SpriteQueueEntry *entry = D_80059300[queue];

    D_800592F8 = queue;
    D_80059580 = (SpriteQueueEntry *)(D_80059524 = (&D_800594B4)[queue]);
    D_80059534 = D_80059524 + D_800592FC;
    for (; entry != NULL; entry = entry->next) {
        func_800320E8((void *)entry->value);
    }
    D_80059300[queue] = NULL;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_800250E0);
#endif

/* Queue `value` on the queue being filled. */
void func_80025180(u32 value) {
    SpriteQueueEntry *entry = D_80059580;

    D_80059580 = entry + 1;
    if (entry != NULL) {
        entry->value = value;
        entry->next = D_80059300[D_800592F8];
        D_80059300[D_800592F8] = entry;
    }
}

/* Queue an upload of `pixels` to (x, y, w, h) on the queue being filled
 * (dropped when its block is full). */
void func_800251C8(u_long *pixels, s16 x, s16 y, s16 w, s16 h) {
    ImageUpload *upload = (ImageUpload *)D_80059580;

    if ((u8 *)(upload + 1) < D_80059534) {
        upload->rect.h = h;
        upload->rect.x = x;
        upload->rect.y = y;
        upload->rect.w = w;
        upload->pixels = pixels;
        D_80059580 = (SpriteQueueEntry *)(upload + 1);
        upload->next = (&D_800594C4)[D_800592F8];
        (&D_800594C4)[D_800592F8] = upload;
    }
}

/* Set a task's update callback from the table at 8004fd40. */
void func_80025224(Task *task, s32 kind) {
    func_8001CD64(task, D_8004FD40[kind]);
}

/* Sprite task draw: its depth from its position under the view matrix
 * plus its depth bias (0 when the projection overflows); unless it is a
 * passive child while 800c3664 is set, draw it into the ordering table
 * (8001e298), or with render bit 24 in its own placement (8001e3d8) at
 * the bias or, with bit 25, at the back; render bit 29 takes the creator's
 * depth. Only depths 1-0xfff are drawn. */
void func_80025258(Task *task) {
    SVECTOR position;
    VECTOR translation;
    s32 xy;
    s32 flag;
    Sprite *sprite = task->data;
    s32 depth;

    if (sprite->b0.bits.passive_children && D_800C3664 != 0) {
        return;
    }
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    SetRotMatrix(&D_8004FBB8);
    SetTransMatrix(&D_8004FBB8);
    depth = (RotTransPers(&position, &xy, &xy, &flag) >> D_80050100) + sprite->half30;
    if (flag & 0x8000) {
        depth = 0;
    }
    sprite->depth = depth;
    if ((sprite->render.word >> 24) & 1) {
        func_80022038(sprite);
        translation.vx = sprite->x >> 16;
        translation.vy = sprite->y >> 16;
        translation.vz = sprite->z >> 16;
        TransMatrix(&sprite->renderer->matrix, &translation);
        SetRotMatrix(&sprite->renderer->matrix);
        SetTransMatrix(&sprite->renderer->matrix);
        if ((sprite->render.word >> 25) & 1) {
            depth = 0xFFF;
        } else {
            depth = sprite->half30;
        }
        if (depth > 0 && depth < 0x1000) {
            func_8001E3D8(sprite, D_8005956C + depth * 4);
        }
    } else {
        if ((sprite->render.word >> 29) & 1) {
            depth = ((Sprite *)sprite->word70)->depth;
        }
        if (depth > 0 && depth < 0x1000) {
            func_8001E298(sprite, D_8005956C + depth * 4);
        }
    }
}

/* Sprite task draw for a sprite without a frame: a point primitive at its
 * screen position (its colour word) and a draw-mode primitive with its
 * blend bits, both from the queue entry block, at its depth. */
/* Nonmatching: the original copies the point to $a1 for the call before storing its colour through that copy, and its first AddPrim argument is set up later. */
#ifdef NON_MATCHING
void func_8002541C(Task *task) {
    SVECTOR position;
    s32 flag;
    Sprite *sprite = task->data;
    PointPrim *point;
    ModePrim *mode;
    s32 depth;

    if (sprite->frame != 0) {
        return;
    }
    point = (PointPrim *)D_80059580;
    if ((u8 *)(point + 1) < D_80059534) {
        position.vx = sprite->x >> 16;
        position.vy = sprite->y >> 16;
        D_80059580 = (SpriteQueueEntry *)(point + 1);
        position.vz = sprite->z >> 16;
        SetRotMatrix(&D_8004FBB8);
        SetTransMatrix(&D_8004FBB8);
        depth = RotTransPers(&position, (long *)&point->xy, &flag, &flag) >> D_80050100;
        sprite->depth = depth;
        point->len = 2;
        point->colour = *(u32 *)&sprite->red;
        AddPrim((u32 *)D_8005956C + depth, point);
        mode = (ModePrim *)D_80059580;
        if ((u8 *)(mode + 1) < D_80059534) {
            D_80059580 = (SpriteQueueEntry *)(mode + 1);
            mode->len = 1;
            mode->code = (sprite->render.word & 0x60) | 0xE1000000;
            AddPrim((u32 *)D_8005956C + depth, mode);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_8002541C);
#endif

/* Sprite task draw for a sprite without a frame drawn as a square: a tile
 * primitive (its colour word) centred on its screen position, as wide as
 * the projection of its size (halfword 0x36) at its distance, and a
 * draw-mode primitive with its blend bits, both from the queue entry
 * block, at its depth. */
/* Nonmatching: the original leaves the delay slot of the absolute-value branch empty and stores the colour word right after the length; this build fills the slot with the tile pointer copy and stores the colour in the AddPrim delay slot. */
#ifdef NON_MATCHING
void func_80025544(Task *task) {
    SVECTOR centre;
    SVECTOR edge;
    s32 edge_xy[2];
    s32 unused_xy;
    s32 flag;
    Sprite *sprite = task->data;
    s32 size;
    TilePrim *tile;
    ModePrim *mode;
    s32 depth;

    if (sprite->frame != 0) {
        return;
    }
    size = *(u16 *)sprite->unknown36;
    tile = (TilePrim *)D_80059580;
    if ((u8 *)(tile + 1) < D_80059534) {
        centre.vx = sprite->x >> 16;
        centre.vy = sprite->y >> 16;
        centre.vz = sprite->z >> 16;
        D_80059580 = (SpriteQueueEntry *)(tile + 1);
        SetRotMatrix(&D_8004FBB8);
        SetTransMatrix(&D_8004FBB8);
        edge = centre;
        edge.vx += size;
        depth = RotTransPers3(&centre, &edge, &centre, (long *)&tile->x, edge_xy, &unused_xy, &flag, &flag) >> D_80050100;
        sprite->depth = depth;
        size = (s16)edge_xy[0] - tile->x;
        if (size == 0) {
            size = 1;
        }
        if (size < 0) {
            size = -size;
        }
        tile->x -= size / 2;
        tile->y -= size / 2;
        tile->colour = *(u32 *)&sprite->red;
        tile->len = 3;
        tile->h = size;
        tile->w = size;
        AddPrim((u32 *)D_8005956C + depth, tile);
        mode = (ModePrim *)D_80059580;
        if ((u8 *)(mode + 1) < D_80059534) {
            D_80059580 = (SpriteQueueEntry *)(mode + 1);
            mode->len = 1;
            mode->code = (sprite->render.word & 0x60) | 0xE1000000;
            AddPrim((u32 *)D_8005956C + depth, mode);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80025544);
#endif

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
        position.vx = sprite->x >> 16;
        position.vy = sprite->y >> 16;
        position.vz = sprite->z >> 16;
        TransMatrix(&sprite->renderer->matrix, &position);
        if (!sprite->render.bits.no_view) {
            CompMatrix(&D_8004FBB8, &sprite->renderer->matrix, &matrix);
        }
        SetRotMatrix(&matrix);
        SetTransMatrix(&matrix);
        func_8002C700(sprite->renderer->pointer34, sprite->renderer->parts[D_800592F8], D_8005956C, ((u16 *)&sprite->flags)[1] & 4); /* flags bit 18, as a halfword */
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_800257F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80025A88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80025C04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80025D4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80025FA8);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_800263E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_8002675C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80026A0C);

void func_80026B9C(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80026BA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80026DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80026F44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sprite_800248D4", func_80026FE8);
