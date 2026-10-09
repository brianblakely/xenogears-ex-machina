/* Battle unit from 800C11CC to the end of the overlay text (Cygnus CDK
 * GCC 2.7.2). 800C11CC's tables start at 0x80070C14 (4 mod 8) directly
 * after 800C0564's odd-length one at 0 mod 8; the functions from 800C06E4
 * to 800C1140 have no rodata, and the boundary is placed at the first
 * function that has. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/effect_script.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/highlight.h"
#include "battle/item_command.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/sprite.h"
#include "battle/sprite_script.h"
#include "own_declarations.h"
#include "resident_views.h"
#include "sprite_effect.h"

/* A little-endian s16 at index i of a command's arguments, as the VM
 * forms it (the high byte shifted, then narrowed). */
#define VM_S16(p, i) ((s16)((p)[(i) + 1] << 8) | (p)[i])


/* The screen fade's blend mode (800B3B6C): a u8, taken as int. */
s32 func_800B3B6C();


/* Run a sprite's animation script until it waits: commands below 80 show a
 * frame (00-0F the next, 10-1F the next of the facing, 20-2F the previous,
 * 30-3F none) and wait (op & 0xF) + 1 frames scaled by its divisor; the
 * others are the battle's commands, falling back to the resident VM's. */
void func_800C11CC(Sprite *sprite) {
    u8 *args;
    u8 op;
    s32 duration;
    s32 wait;
    s32 argument;
    s32 frame;
    s32 cond;
    s32 offset;
    s32 size;
    u8 *data;
    u8 *buffer;
    u8 *part;
    Sprite *partner;
    SVECTOR *angles;
    SVECTOR v;
    MATRIX m;
    VECTOR out;
    GroundPoint a;
    GroundPoint b;
    s16 code;
    s32 velocity;
    s32 count;
    u8 value;

next:
    if (sprite->countdown == 0) {
        op = *sprite->script;
        args = sprite->script + 1;
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
            /* 40-7F leave duration unset (the original reads a stale register). */
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
        /* 80-ff: as 800248d4 (a handler that keeps the script pointer then advances by
         * the resident length table, D_8004FC40[op] = D_8004FCC0[op - 0x80]), with the
         * battle's cases. */
        switch (op) {
        /* e8 cmd var: battle command cmd (800b3f04) on the bytes at the variable. */
        case 0xE8:
            func_800B3F04(sprite, args[0], func_8001FBA4(sprite, args + 1));
            break;
        /* ca s16: fade the screen with the five bytes at the operand + s16: to r, g, b over
         * (byte 3 >> 1) * 2 frames, blend byte 4 - 1 (0: the running fade's, 1 without
         * one, 800b3b6c) (800b39c0). */
        case 0xCA:
            {
                u8 *effect = SCRIPT_DATA(args);

                func_800B39C0(effect[3] >> 1, effect[4] != 0 ? effect[4] - 1 : func_800B3B6C(), effect[0], effect[1], effect[2]);
            }
            break;
        /* cb s16: quake the view with the four bytes at the operand + s16: amplitude x, y,
         * z, then the time, byte 3 * 2 frames (800b3658). */
        case 0xCB:
            {
                u8 *quake = SCRIPT_DATA(args);

                func_80021B04(&v, quake[0], quake[1], quake[2]);
                func_800B3658(&v, quake[3]);
            }
            break;
        /* e3 s16: the partner runs the animation header at this command + s16, as animation
         * 3f (80023538). */
        case 0xE3:
            {
                Sprite *target = sprite->partner;
                u8 *animation = sprite->script + VM_S16(args, 0);

                target->motion.bytes[3] = 0x3F;
                func_80023538(target, (u16 *)animation);
            }
            break;
        /* fb s16 u8: go on; once the sprite comes nearer its target point (+a0) than u8 * 2,
         * or moves away from it, resume at this command + s16 (800b5924 watches). */
        case 0xFB:
            func_800B5924(sprite, args[2] * 2, sprite->script + VM_S16(args, 0));
            break;
        /* c3 cmd, ec cmd a, f9 cmd a b: battle command cmd (800b3f04) on the bytes after it:
         * none, one or two of its own. */
        case 0xC3:
        case 0xEC:
        case 0xF9:
            func_800B3F04(sprite, args[0], args + 1);
            break;
        /* 9d: mark the camera eye point (800d3354, group 10) or look-at point (800d335c,
         * others) at the sprite's position. */
        case 0x9D:
            if (((SpriteFlagBits *)&sprite->flags)->type == 10) {
                D_800D3354.vx = sprite->x >> 16;
                D_800D3354.vy = sprite->y >> 16;
                D_800D3354.vz = sprite->z >> 16;
            } else {
                D_800D335C.vx = sprite->x >> 16;
                D_800D335C.vy = sprite->y >> 16;
                D_800D335C.vz = sprite->z >> 16;
            }
            break;
        /* 99: view angles = the camera's angles (800d30b0). */
        case 0x99:
            sprite->renderer->angle_x = D_800D30B0.vx;
            sprite->renderer->angle_y = D_800D30B0.vy;
            sprite->renderer->angle_z = D_800D30B0.vz;
            break;
        /* 9a: v = (camera distance 800d30b8, 0, 0) by the view angles; group 10 moves to
         * 8006f9ac - v, group 11 to 8006f99c + v. */
        case 0x9A:
            angles = (SVECTOR *)sprite->renderer;
            v.vx = D_800D30B8;
            v.vy = 0;
            v.vz = 0;
            func_8003F738(angles, &m);
            ApplyMatrix(&m, &v, &out);
            if (((SpriteFlagBits *)&sprite->flags)->type == 10) {
                sprite->x = D_8006F9AC.vx - (out.vx << 16);
                sprite->y = D_8006F9AC.vy - (out.vy << 16);
                sprite->z = D_8006F9AC.vz - (out.vz << 16);
            }
            if (((SpriteFlagBits *)&sprite->flags)->type == 11) {
                sprite->x = D_8006F99C.vx + (out.vx << 16);
                sprite->y = D_8006F99C.vy + (out.vy << 16);
                sprite->z = D_8006F99C.vz + (out.vz << 16);
            }
            break;
        /* 9b: scale = (s16)80059454 << 12 / the camera distance. */
        case 0x9B:
            sprite->scale = ((s16)D_80059454 << 12) / D_800D30B8;
            break;
        /* 9c: as 9a with ((s16)80059454 << 12 / scale, 0, 0) by the camera's angles. */
        case 0x9C:
            v.vx = ((s16)D_80059454 << 12) / sprite->scale;
            angles = &D_800D30B0;
            v.vy = 0;
            v.vz = 0;
            func_8003F738(angles, &m);
            ApplyMatrix(&m, &v, &out);
            if (((SpriteFlagBits *)&sprite->flags)->type == 10) {
                sprite->x = D_8006F9AC.vx - (out.vx << 16);
                sprite->y = D_8006F9AC.vy - (out.vy << 16);
                sprite->z = D_8006F9AC.vz - (out.vz << 16);
            }
            if (((SpriteFlagBits *)&sprite->flags)->type == 11) {
                sprite->x = D_8006F99C.vx + (out.vx << 16);
                sprite->y = D_8006F99C.vy + (out.vy << 16);
                sprite->z = D_8006F99C.vz + (out.vz << 16);
            }
            break;
        /* c2 s8: y = ground - 1; aim the jump (800ba768) at the target point (+a0) = the
         * partner's x + s8 (scaled, negated unless mirrored), y 0 and z. */
        case 0xC2:
            {
                Sprite *target;
                s32 distance;

                sprite->y = (sprite->ground - 1) << 16;
                target = sprite->partner;
                distance = (s8)args[0] * sprite->scale / 4096;
                if (!sprite->motion.bits.mirror) {
                    distance = -distance;
                }
                sprite->target_x = (target->x >> 16) + distance;
                sprite->target_z = target->z >> 16;
                sprite->target_y = 0;
                func_800BA768(sprite);
            }
            break;
        /* 97: wait until landed (retrying each frame), then stop walking and turn this
         * sprite and its partner to face each other. */
        case 0x97:
            if ((sprite->y >> 16) < sprite->ground) {
                sprite->countdown = 1;
                return;
            }
            partner = sprite->partner;
            func_80021FC0(sprite, 0);
            a.x = sprite->x >> 16;
            a.z = sprite->z >> 16;
            b.x = partner->x >> 16;
            b.z = partner->z >> 16;
            func_80021FE0(sprite, func_80023124(b, a));
            func_800223B0(sprite, func_80023124(b, a));
            func_80021FE0(partner, func_80023124(a, b));
            func_800223B0(partner, func_80023124(a, b));
            break;
        /* a4 s8: the partner plays animation s8: through its stage object (800aa454) without
         * an animation block, else 800245d8. */
        case 0xA4:
            {
                Sprite *target = sprite->partner;
                s32 motion = (s8)args[0];

                if (target->animations == 0) {
                    func_800AA454(SPRITE_SLOT(target), 1 << SPRITE_SLOT(sprite), motion);
                } else {
                    func_800245D8(target, motion);
                }
            }
            break;
        /* 9e: retry each frame until pad bit 0x100 is held, then wait a frame. */
        case 0x9E:
            sprite->countdown = 1;
            if (!(BATTLE_AREA.held & 0x100)) {
                return;
            }
            break;
        /* 95: retry each frame while the disc is busy (800286cc), then wait a frame. */
        case 0x95:
            sprite->countdown = 1;
            if (func_800286CC() != 0) {
                return;
            }
            break;
        /* 89: aim the jump at the target point (+a0) keeping the rising speed (800ba768). */
        case 0x89:
            func_800BA768(sprite);
            break;
        /* 88: aim the jump at the target point (+a0) (800ba614). */
        case 0x88:
            func_800BA614(sprite);
            break;
        /* 8f: finish: flag 800c3624, no script, state 0. */
        case 0x8F:
            D_800C3624 = 1;
            sprite->script = NULL;
            sprite->frame_bits.field28 = 0;
            return;
        /* f8 s16 c: jump by s16 from this command when condition c (bits 0-6, bit 7 negates)
         * holds: 0-6 the current event's code for the partner's slot (4: 4 or 7), 7 more
         * event targets follow the partner (then the next one), 8 flag 800c4928, 9 the
         * partner is the acting sprite; others never. */
        case 0xF8:
            code = BATTLE_AREA.events[D_800C360C - 1].codes[SPRITE_SLOT(sprite->partner)];
            switch (args[2] & 0x7F) {
            case 8:
                cond = (u8)D_800C4928 != 0;
                break;
            case 9:
                cond = sprite->partner == D_800C3E1C;
                break;
            case 0:
                cond = code == 0;
                break;
            case 1:
                cond = code == 1;
                break;
            case 2:
                cond = code == 2;
                break;
            case 3:
                cond = code == 3;
                break;
            case 4:
                if (code == 4 || code == 7) {
                    cond = 1;
                } else {
                    cond = 0;
                }
                break;
            case 5:
                cond = code == 5;
                break;
            case 6:
                cond = code == 6;
                break;
            case 7:
                cond = D_800D3678;
                cond = func_800BF954(sprite->partner) + 1 < cond;
                func_800BF8CC(sprite);
                break;
            default:
                cond = 0;
                break;
            }
            if (args[2] & 0x80) {
                cond = !cond;
            }
            if (!cond) {
                break;
            }
            sprite->script += VM_S16(args, 0);
            goto next;
        /* f3 s24: queue the old part list; with s24 nonzero, build the parts and anchors of
         * the effect entry at the operand + s24, else drop them. */
        case 0xF3:
            if (sprite->renderer->parts[0] != NULL) {
                func_80025180((u32)sprite->renderer->parts[0]);
            }
            offset = ((s8)args[2] << 16) + (args[1] << 8) + args[0];
            data = (u8 *)(offset + (s32)args);
            if (offset != 0) {
                size = func_800B16A4(func_800B168C(data, 0));
                buffer = func_80031BDC(size * 2, 0);
                if (sprite->rate != 0) {
                    func_800B1EA0(func_800B168C(data, 0), 3);
                }
                func_800B1720(func_800B168C(data, 0), buffer, ((u8 *)&sprite->render)[0] >> 5, sprite->colour_flags & 1);
                part = buffer + size;
                memcpy(part, buffer, size);
                sprite->renderer->parts[0] = (SpritePart *)buffer;
                sprite->renderer->parts[1] = (SpritePart *)part;
                sprite->renderer->pointer34 = (SpriteRendererEntry *)func_800B168C(data, 0);
            } else {
                sprite->renderer->parts[0] = NULL;
                sprite->renderer->pointer34 = NULL;
            }
            break;
        /* 8b: show the current event's results (800bd2e4). */
        case 0x8B:
            func_800BD2E4();
            break;
        /* be s16 (three bytes; the width table says two): frame bits 0-8, wait bits 11-14 +
         * 1 (scaled); one-sided sprites also take flip x (bit 9) and y (bit 10), and bit 15
         * maps a nonzero frame through the frame map (+60). */
        case 0xBE:
            argument = args[0] | ((s8)args[1] << 8);
            wait = ((argument >> 11) & 0xF) + 1;
            frame = argument & 0x1FF;
            wait = wait * sprite->motion.bits.divisor / 256;
            if (wait == 0) {
                wait = 1;
            }
            if ((sprite->render.word & 3) != 1) {
                sprite->frame = frame;
                sprite->countdown += wait;
                sprite->script += 3;
                return;
            } else {
                if (argument < 0 && frame != 0) {
                    frame = ((u8 *)sprite->word60)[frame - 1];
                }
                sprite->motion.bits.frame_flip = (argument >> 9) & 1;
                sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
                if ((argument >> 10) & 1) {
                    sprite->render.bits.flip_y = 1;
                } else {
                    sprite->render.bits.flip_y = 0;
                }
                func_8001D2B0(sprite, frame);
            }
            sprite->countdown += wait;
            sprite->script += 3;
            return;
        /* 8e: stop: no script. */
        case 0x8E:
            sprite->script = NULL;
            return;
        /* e2 s16: call: push the 3-byte return point (the next command) and jump by s16 from
         * this command. */
        case 0xE2:
            {
                s32 jump = args[0] + (s16)(args[1] << 8);

                func_80021CF8(sprite, (s32)(sprite->script + 3));
                sprite->script += (s16)jump;
            }
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
            sprite->script += VM_S16(args, 1);
            goto next;
        /* d4 s16: jump by s16 from this command, then run the completion callback. */
        case 0xD4:
            sprite->script += VM_S16(args, 0);
            if (sprite->callback != NULL) {
                sprite->callback(sprite);
            }
            goto next;
        /* 86: wait while rising (vertical speed below 0), retrying each frame. */
        case 0x86:
            if (sprite->speed_y < 0) {
                sprite->countdown = 1;
                return;
            }
            break;
        /* 87: wait while above the ground (y < ground), retrying each frame. */
        case 0x87:
            if ((sprite->y >> 16) < sprite->ground) {
                sprite->countdown = 1;
                return;
            }
            break;
        /* 80: end: state 0, then the completion callback, or else the idle animation (byte
         * b0) when it is not negative. */
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
        /* 98: with a creator (+70) running this sprite's wait animation (+8d) in state 2,
         * retry each frame; then go on after a frame (at once without a creator). */
        case 0x98:
            {
                Sprite *parent = sprite->parent;

                if (parent == NULL) {
                    break;
                }
                sprite->countdown = 1;
                if ((s8)parent->motion.bytes[3] != (s8)sprite->unknown8d) {
                    break;
                }
                if (parent->frame_bits.field28 == 2) {
                    return;
                }
            }
            break;
        /* 82: restart: completion callback, then the current animation again (800245d8,
         * keeping the vertical speed), run at once. */
        case 0x82:
            if (sprite->callback != NULL) {
                sprite->callback(sprite);
            }
            velocity = sprite->speed_y;
            func_800245D8(sprite, (s8)sprite->motion.bytes[3]);
            sprite->speed_y = velocity;
            sprite->countdown = 0;
            func_800C11CC(sprite);
            return;
        /* 81: hold: animation 3f ends (80); others stop here (countdown 0) after the
         * completion callback, in state 1. */
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
        /* e4 s16: loop: pop a count; when nonzero, push it less one and jump (e1). */
        case 0xE4:
            count = func_80021C20(sprite);
            if ((u8)count == 0) {
                break;
            }
            count--;
            func_80021CA0(sprite, count);
            /* fall through */
        /* e1 s16: jump by s16 from this command. */
        case 0xE1:
            sprite->script += VM_S16(args, 0);
            goto next;
        /* a7 u8: bit 7 pauses the main task list (80059428) for (u8 & 7f) + 1 frames and
         * waits a frame; else wait u8 + 2 frames (scaled, at least 1). */
        case 0xA7:
            sprite->script += D_8004FC40[op];
            value = args[0];
            if (value & 0x80) {
                s32 repeat = (value & 0x7F) + 1;

                D_80059428 = repeat;
                sprite->countdown++;
                return;
            }
            {
                s32 frames = (args[0] + 2) * sprite->motion.bits.divisor / 256;

                if (frames == 0) {
                    frames = 1;
                }
                sprite->countdown += frames;
            }
            return;
        /* c8 op var: generic command op (8001fbe4) on the bytes at the variable (8001fba4). */
        case 0xC8:
            func_8001FBE4(sprite, args[0], func_8001FBA4(sprite, args + 1));
            break;
        /* Others: 8001fbe4 on the bytes after the command (no effect without a case
         * there, as for 83 and 84). */
        default:
            func_8001FBE4(sprite, op, args);
            break;
        /* 9f: no effect. */
        case 0x9F:
            break;
        }
        sprite->script += D_8004FC40[op];
        goto next;
    }
}
