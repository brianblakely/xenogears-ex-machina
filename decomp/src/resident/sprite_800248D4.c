/* Resident sprite engine, third unit (0x800248d4-0x80025c04): the
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
#include "resident/mode.h"
#include "resident/menu.h"
#include "resident/sprite.h"
#include "resident/cd.h"
#include "resident/stream.h"
#include "resident/model.h"
#include "resident/heap.h"
#include "resident/text.h"
#include "resident/pad.h"
#include "resident/console.h"
#include "resident/sound.h"
#include "own_declarations.h"

void func_80025258(Task *task);
void func_8002541C(Task *task);
void func_80025544(Task *task);
void func_80025710(Task *task);
void func_80025718(Task *task);
void func_800257F0(Task *task);

/* Task update callbacks by kind (80025224). */
void (*D_8004FD40[16])(Task *) = {
    func_80025258, func_80025710, func_80025718, NULL,
    NULL,          func_80025258, func_80025258, func_80025718,
    func_8002541C, func_80025544, NULL,          NULL,
    NULL,          NULL,          func_80025258, func_800257F0,
};
/* Light colour and direction matrices of lit sprite models. */
MATRIX D_8004FD80 = {{{0xC00, 0, 0}, {0xC00, 0, 0}, {0xC00, 0, 0}}, {0, 0, 0}};
MATRIX D_8004FDA0 = {{{0x1000, 0x1000, 0x1000}, {0, 0, 0}, {0, 0, 0}}, {0, 0, 0}};

/* The unit's own small variables, $gp-relative: its statics, which take
 * the unit's .sbss (800592f8), and its small commons, which merge with
 * commons/common_80059404.c's definitions. */
static s32 D_800592F8;        /* the queue being filled (0 or 1) */
static s32 D_800592FC;        /* bytes of the queue entry block / 2 */
static SpriteQueueEntry *D_80059300[2]; /* the two queues */
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
    /* 00-7f, one byte: 00-0f show the next frame, 10-1f the next frame-table entry
     * (80022d44), 20-2f the previous frame, 30-3f keep the frame; each waits (op & f) + 1
     * frames scaled by the divisor / 256 (at least 1) and counts a step (frame bits 22-27,
     * held at 63). 40-7f set no duration: the original reads a stale register. */
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
    /* 80-ff: the cases below, the rest through 8001fbe4; a handler that keeps the script
     * pointer then advances by the command's length D_8004FCC0[op - 0x80] (80-9f: 1,
     * a0-c7: 2, c8-f0: 3, f1-ff: 4). See tools/analysis/sprite_vm.py. */
    switch (op) {
    /* be s16 (three bytes; the width table says two): frame bits 0-8, wait bits 11-14 + 1
     * (scaled); one-sided sprites also take flip x (bit 9) and y (bit 10), and bit 15 maps a
     * nonzero frame through the frame map (+60). */
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
    /* 8e: stop: no script. */
    case 0x8E:
        sprite->script = NULL;
        return;
    /* e2 s16: call: push the 3-byte return point (the next command) and jump by s16 from
     * this command. */
    case 0xE2:
        offset = args[0] + ((s8)args[1] << 8);
        func_80021CF8(sprite, (s32)(sprite->script + 3));
        sprite->script += offset;
        goto next;
    /* 85: return to the 3 bytes popped from the stack (the pointer's top byte kept). */
    case 0x85:
        sprite->script = (u8 *)(((u32)sprite->script & 0xFF000000) | func_80021C6C(sprite));
        goto next;
    /* fa var s16: jump by s16 from this command when the variable is nonzero. */
    case 0xFA:
        if (*func_8001FBA4(sprite, args) == 0) {
            break;
        }
        sprite->script += (s16)(args[1] | ((s8)args[2] << 8));
        goto next;
    /* d4 s16: jump by s16 from this command, then run the completion callback. */
    case 0xD4:
        sprite->script += (s16)(args[0] | ((s8)args[1] << 8));
        if (sprite->callback != NULL) {
            sprite->callback(sprite);
        }
        goto next;
    /* 86: wait while rising (vertical speed below 0), retrying each frame. */
    case 0x86:
        if (sprite->speed_y >= 0) {
            break;
        }
        sprite->countdown = 1;
        return;
    /* 87: wait while above the ground (y < ground), retrying each frame. */
    case 0x87:
        if ((s16)(sprite->y >> 16) >= sprite->ground) {
            break;
        }
        sprite->countdown = 1;
        return;
    /* 80: end: state 0, then the completion callback, or else the idle animation (byte b0)
     * when it is not negative. */
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
    /* 98: with a creator (+70) running this sprite's wait animation (+8d) in state 2, retry
     * each frame; then go on after a frame (at once without a creator). */
    case 0x98:
        creator = (Sprite *)sprite->parent;
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
    /* 82: restart: completion callback, then the current animation again (800245d8, keeping
     * the vertical speed), run at once. */
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
    /* 81: hold: animation 3f ends (80); others stop here (countdown 0) after the completion
     * callback, in state 1. */
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
    /* e4 s16: loop: pop a count; when nonzero, push it less one and jump by s16 from this
     * command. */
    case 0xE4:
        count = func_80021C20(sprite);
        if (count == 0) {
            break;
        }
        count--;
        func_80021CA0(sprite, count);
        sprite->script += (s16)(args[0] | ((s8)args[1] << 8));
        goto next;
    /* e1 s16: jump by s16 from this command. */
    case 0xE1:
        sprite->script += (s16)(args[0] | ((s8)args[1] << 8));
        goto next;
    /* a7 u8: bit 7 pauses the main task list (80059428) for (u8 & 7f) + 1 frames and waits a
     * frame; else wait u8 + 2 frames (scaled, at least 1). */
    case 0xA7:
        sprite->script += D_8004FCC0[op - 0x80];
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
    /* c8 op var: generic command op (8001fbe4) on the bytes at the variable (8001fba4). */
    case 0xC8:
        func_8001FBE4(sprite, args[0], func_8001FBA4(sprite, args + 1));
        break;
    /* Others: 8001fbe4 on the bytes after the command (no effect without a case there,
     * as for 83 and 84). */
    default:
        func_8001FBE4(sprite, op, args);
        break;
    }
    sprite->script += D_8004FCC0[op - 0x80];
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
void func_800250E0(s32 queue) {
    s32 offset = queue * sizeof(SpriteQueueEntry *);
    SpriteQueueEntry *entry = *(SpriteQueueEntry **)((u8 *)D_80059300 + offset);

    D_800592F8 = queue;
    D_80059580 = (SpriteQueueEntry *)(&D_800594B4)[queue];
    D_80059524 = (u8 *)D_80059580;
    D_80059534 = D_80059524 + D_800592FC;
    for (; entry != NULL; entry = entry->next) {
        func_800320E8((void *)entry->value);
    }
    D_80059300[queue] = NULL;
}

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
            depth = ((Sprite *)sprite->parent)->depth;
        }
        if (depth > 0 && depth < 0x1000) {
            func_8001E298(sprite, (u_long *)(D_8005956C + depth * 4));
        }
    }
}

/* Sprite task draw for a sprite without a frame: a point primitive at its
 * screen position (its colour word) and a draw-mode primitive with its
 * blend bits, both from the queue entry block, at its depth. */
void func_8002541C(Task *task) {
    SVECTOR position;
    s32 flag;
    Sprite *sprite = task->data;
    PointPrim *point;
    P_TAG *primitive;
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
        primitive = (P_TAG *)point;
        *(u32 *)&primitive->r0 = *(u32 *)&sprite->red;
        AddPrim((u32 *)D_8005956C + depth, primitive);
        mode = (ModePrim *)D_80059580;
        if ((u8 *)(mode + 1) < D_80059534) {
            D_80059580 = (SpriteQueueEntry *)(mode + 1);
            mode->len = 1;
            mode->code = (sprite->render.word & 0x60) | 0xE1000000;
            AddPrim((u32 *)D_8005956C + depth, mode);
        }
    }
}

/* Sprite task draw for a sprite without a frame drawn as a square: a tile
 * primitive (its colour word) centred on its screen position, as wide as
 * the projection of its size (halfword 0x36) at its distance, and a
 * draw-mode primitive with its blend bits, both from the queue entry
 * block, at its depth. */
void func_80025544(Task *task) {
    SVECTOR centre;
    SVECTOR edge;
    s32 edge_xy[2];
    s32 unused_xy;
    s32 flag;
    Sprite *sprite = task->data;
    s32 size;
    TILE *tile;
    ModePrim *mode;
    s32 depth;

    if (sprite->frame != 0) {
        return;
    }
    size = sprite->height;
    tile = (TILE *)D_80059580;
    if ((u8 *)(tile + 1) < D_80059534) {
        centre.vx = sprite->x >> 16;
        centre.vy = sprite->y >> 16;
        centre.vz = sprite->z >> 16;
        D_80059580 = (SpriteQueueEntry *)(tile + 1);
        SetRotMatrix(&D_8004FBB8);
        SetTransMatrix(&D_8004FBB8);
        edge = centre;
        edge.vx += size;
        depth = RotTransPers3(&centre, &edge, &centre, (long *)&tile->x0, edge_xy, &unused_xy, &flag, &flag) >> D_80050100;
        sprite->depth = depth;
        size = (s16)edge_xy[0] - tile->x0;
        if (size == 0) {
            size = 1;
        }
        size = abs(size);
        tile->x0 -= size / 2;
        tile->y0 -= size / 2;
        *(u32 *)&tile->r0 = *(u32 *)&sprite->red;
        setlen(tile, 3);
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

void func_80025710(Task *task) {
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
    if (((SpriteModelRenderer *)sprite->renderer)->model != NULL) {
        position.vx = sprite->x >> 16;
        position.vy = sprite->y >> 16;
        position.vz = sprite->z >> 16;
        TransMatrix(&sprite->renderer->matrix, &position);
        if (!sprite->render.bits.no_view) {
            CompMatrix(&D_8004FBB8, &sprite->renderer->matrix, &matrix);
        }
        SetRotMatrix(&matrix);
        SetTransMatrix(&matrix);
        func_8002C700((SpriteModel *)((SpriteModelRenderer *)sprite->renderer)->model,
                      (RenderPacket *)((SpriteModelRenderer *)sprite->renderer)->packets[D_800592F8],
                      (u32 *)D_8005956C, ((u16 *)&sprite->flags)[1] & 4); /* flags bit 18, as a halfword */
    }
}

/* Sprite task draw for lit models: rebuild the orientation; with a model
 * (renderer block), light it when flag bit 1 is set (its light angles and
 * colour from the renderer), place it at the sprite's position (in view
 * space unless render bit 24 is set; render bit 31 draws it around the
 * screen centre 160,112) and draw it with the battle overlay's renderer
 * (800b1f6c), at the back with a 16-bit depth shift for render bit 25. */
void func_800257F0(Task *task) {
    MATRIX view;
    VECTOR position;
    MATRIX rotation;
    MATRIX light;
    long offset_x;
    long offset_y;
    Sprite *sprite = task->data;
    s32 shift;

    func_80022038(sprite);
    if (((SpriteModelRenderer *)sprite->renderer)->model == NULL) {
        return;
    }
    if ((sprite->flags >> 1) & 1) {
        PushMatrix();
        D_8004FD80.m[0][0] = sprite->renderer->light_colour[0];
        D_8004FD80.m[1][0] = sprite->renderer->light_colour[1];
        D_8004FD80.m[2][0] = sprite->renderer->light_colour[2];
        func_8003F738(&sprite->renderer->light_angles, &rotation);
        MulMatrix0(&rotation, &sprite->renderer->matrix, &rotation);
        MulMatrix0(&D_8004FDA0, &rotation, &light);
        SetBackColor(0x20, 0x20, 0x20);
        SetColorMatrix(&D_8004FD80);
        SetLightMatrix(&light);
        PopMatrix();
    }
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    TransMatrix(&sprite->renderer->matrix, &position);
    if (!sprite->render.bits.no_view) {
        CompMatrix(&D_8004FBB8, &sprite->renderer->matrix, &view);
        SetRotMatrix(&view);
        SetTransMatrix(&view);
    } else {
        SetRotMatrix(&sprite->renderer->matrix);
        SetTransMatrix(&sprite->renderer->matrix);
    }
    if ((s32)sprite->render.word < 0) {
        ReadGeomOffset(&offset_x, &offset_y);
        SetGeomOffset(160, 112);
    }
    if ((sprite->render.word >> 25) & 1) {
        shift = D_80050100;
        D_80050100 = 16;
        func_800B1F6C(((SpriteModelRenderer *)sprite->renderer)->model,
                      ((SpriteModelRenderer *)sprite->renderer)->packets[D_800592F8],
                      (u32 *)D_8005956C, 0, 0xFEC, sprite->render.bits.blend);
        D_80050100 = shift;
    } else {
        func_800B1F6C(((SpriteModelRenderer *)sprite->renderer)->model,
                      ((SpriteModelRenderer *)sprite->renderer)->packets[D_800592F8],
                      (u32 *)D_8005956C, 0, sprite->half30, sprite->render.bits.blend);
    }
    if ((s32)sprite->render.word < 0) {
        SetGeomOffset(offset_x, offset_y);
    }
}

/* Sprite task draw for unlit models: rebuild the orientation and, with a
 * model, move its matrix to the sprite's position in view space (keeping
 * its own rotation) and draw it with the battle overlay's renderer
 * (800b1f6c), at the back with a 16-bit depth shift for render bit 25. */
void func_80025A88(Task *task) {
    MATRIX unused; /* the frame reserves 32 bytes no code uses */
    VECTOR translated;
    SVECTOR position;
    Sprite *sprite = task->data;
    s32 shift;

    func_80022038(sprite);
    if (((SpriteModelRenderer *)sprite->renderer)->model == NULL) {
        return;
    }
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    ApplyMatrix(&D_8004FBB8, &position, &translated);
    sprite->renderer->matrix.t[0] = D_8004FBB8.t[0] + translated.vx;
    sprite->renderer->matrix.t[1] = D_8004FBB8.t[1] + translated.vy;
    sprite->renderer->matrix.t[2] = D_8004FBB8.t[2] + translated.vz;
    SetRotMatrix(&sprite->renderer->matrix);
    SetTransMatrix(&sprite->renderer->matrix);
    if ((sprite->render.word >> 25) & 1) {
        shift = D_80050100;
        D_80050100 = 16;
        func_800B1F6C(((SpriteModelRenderer *)sprite->renderer)->model,
                      ((SpriteModelRenderer *)sprite->renderer)->packets[D_800592F8],
                      (u32 *)D_8005956C, 0, 0xFEC, sprite->render.bits.blend);
        D_80050100 = shift;
    } else {
        func_800B1F6C(((SpriteModelRenderer *)sprite->renderer)->model,
                      ((SpriteModelRenderer *)sprite->renderer)->packets[D_800592F8],
                      (u32 *)D_8005956C, 0, sprite->half30, sprite->render.bits.blend);
    }
}
