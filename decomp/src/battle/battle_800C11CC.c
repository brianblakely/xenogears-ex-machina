/* Battle unit from 800C11CC to the end of the overlay text (Cygnus CDK
 * GCC 2.7.2). 800C11CC's tables start at 0x80070C14 (4 mod 8) directly
 * after 800C0564's odd-length one at 0 mod 8; the functions from 800C06E4
 * to 800C1140 have no rodata, and the boundary is placed at the first
 * function that has. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "effect.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
#include "actor.h"
#include "popup.h"
#include "frame.h"
#include "stage.h"
#include "item_command.h"
#include "sprite_vm.h"

/* Run a sprite's animation script until it waits: commands below 80 show a
 * frame (00-0F the next, 10-1F the next of the facing, 20-2F the previous,
 * 30-3F none) and wait (op & 0xF) + 1 frames scaled by its divisor; the
 * others are the battle's commands, falling back to the resident VM's. */
void func_800C11CC(BattleSprite *sprite) {
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
    BattleSprite *partner;
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
                SPRITE_FRAME_BITS(sprite).frame++;
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
            duration = duration * SPRITE_MOTION_BITS(sprite).divisor / 256;
            if (duration == 0) {
                duration = 1;
            }
            sprite->countdown += duration;
            if (++SPRITE_FRAME_BITS(sprite).commands == 0) {
                SPRITE_FRAME_BITS(sprite).commands--;
            }
            return;
        }
        switch (op) {
        case 0xE8:
            func_800B3F04(sprite, args[0], func_8001FBA4(sprite, args + 1));
            break;
        case 0xCA:
            {
                u8 *effect = SCRIPT_DATA(args);

                func_800B39C0(effect[3] >> 1, effect[4] != 0 ? effect[4] - 1 : func_800B3B6C(), effect[0], effect[1], effect[2]);
            }
            break;
        case 0xCB:
            {
                u8 *quake = SCRIPT_DATA(args);

                func_80021B04(&v, quake[0], quake[1], quake[2]);
                func_800B3658(&v, quake[3]);
            }
            break;
        case 0xE3:
            {
                BattleSprite *target = sprite->partner;
                u8 *animation = sprite->script + VM_S16(args, 0);

                target->motion.bytes[3] = 0x3F;
                func_80023538(target, animation);
            }
            break;
        case 0xFB:
            func_800B5924(sprite, args[2] * 2, sprite->script + VM_S16(args, 0));
            break;
        case 0xC3:
        case 0xEC:
        case 0xF9:
            func_800B3F04(sprite, args[0], args + 1);
            break;
        case 0x9D:
            if (sprite->flags.bits.group == 10) {
                D_800D3354.vx = sprite->x.fixed >> 16;
                D_800D3354.vy = sprite->y.fixed >> 16;
                D_800D3354.vz = sprite->z.fixed >> 16;
            } else {
                D_800D335C.vx = sprite->x.fixed >> 16;
                D_800D335C.vy = sprite->y.fixed >> 16;
                D_800D335C.vz = sprite->z.fixed >> 16;
            }
            break;
        case 0x99:
            sprite->view->angle[0] = D_800D30B0.vx;
            sprite->view->angle[1] = D_800D30B0.vy;
            sprite->view->angle[2] = D_800D30B0.vz;
            break;
        case 0x9A:
            angles = (SVECTOR *)sprite->view;
            v.vx = D_800D30B8;
            v.vy = 0;
            v.vz = 0;
            func_8003F738(angles, &m);
            ApplyMatrix(&m, &v, &out);
            if (sprite->flags.bits.group == 10) {
                sprite->x.fixed = D_8006F9AC.vx - (out.vx << 16);
                sprite->y.fixed = D_8006F9AC.vy - (out.vy << 16);
                sprite->z.fixed = D_8006F9AC.vz - (out.vz << 16);
            }
            if (sprite->flags.bits.group == 11) {
                sprite->x.fixed = D_8006F99C.vx + (out.vx << 16);
                sprite->y.fixed = D_8006F99C.vy + (out.vy << 16);
                sprite->z.fixed = D_8006F99C.vz + (out.vz << 16);
            }
            break;
        case 0x9B:
            sprite->scale = ((s16)D_80059454 << 12) / D_800D30B8;
            break;
        case 0x9C:
            v.vx = ((s16)D_80059454 << 12) / sprite->scale;
            angles = &D_800D30B0;
            v.vy = 0;
            v.vz = 0;
            func_8003F738(angles, &m);
            ApplyMatrix(&m, &v, &out);
            if (sprite->flags.bits.group == 10) {
                sprite->x.fixed = D_8006F9AC.vx - (out.vx << 16);
                sprite->y.fixed = D_8006F9AC.vy - (out.vy << 16);
                sprite->z.fixed = D_8006F9AC.vz - (out.vz << 16);
            }
            if (sprite->flags.bits.group == 11) {
                sprite->x.fixed = D_8006F99C.vx + (out.vx << 16);
                sprite->y.fixed = D_8006F99C.vy + (out.vy << 16);
                sprite->z.fixed = D_8006F99C.vz + (out.vz << 16);
            }
            break;
        case 0xC2:
            {
                BattleSprite *target;
                s32 distance;

                sprite->y.fixed = (sprite->ground - 1) << 16;
                target = sprite->partner;
                distance = (s8)args[0] * sprite->scale / 4096;
                if (!SPRITE_MOTION_BITS(sprite).mirror) {
                    distance = -distance;
                }
                sprite->target[0] = (target->x.fixed >> 16) + distance;
                sprite->target[2] = target->z.fixed >> 16;
                sprite->target[1] = 0;
                func_800BA768(sprite);
            }
            break;
        case 0x97:
            if ((sprite->y.fixed >> 16) < sprite->ground) {
                sprite->countdown = 1;
                return;
            }
            partner = sprite->partner;
            func_80021FC0(sprite, 0);
            a.x = sprite->x.fixed >> 16;
            a.z = sprite->z.fixed >> 16;
            b.x = partner->x.fixed >> 16;
            b.z = partner->z.fixed >> 16;
            func_80021FE0(sprite, func_80023124(b, a));
            func_800223B0(sprite, func_80023124(b, a));
            func_80021FE0(partner, func_80023124(a, b));
            func_800223B0(partner, func_80023124(a, b));
            break;
        case 0xA4:
            {
                BattleSprite *target = sprite->partner;
                s32 motion = (s8)args[0];

                if (target->field48 == 0) {
                    func_800AA454(SPRITE_SLOT(target), 1 << SPRITE_SLOT(sprite), motion);
                } else {
                    func_800245D8(target, motion);
                }
            }
            break;
        case 0x9E:
            sprite->countdown = 1;
            if (!(BATTLE_AREA.held & 0x100)) {
                return;
            }
            break;
        case 0x95:
            sprite->countdown = 1;
            if (func_800286CC() != 0) {
                return;
            }
            break;
        case 0x89:
            func_800BA768(sprite);
            break;
        case 0x88:
            func_800BA614(sprite);
            break;
        case 0x8F:
            D_800C3624 = 1;
            sprite->script = NULL;
            SPRITE_FRAME_BITS(sprite).state = 0;
            return;
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
        case 0xF3:
            if (sprite->view->parts != NULL) {
                func_80025180(sprite->view->parts);
            }
            offset = ((s8)args[2] << 16) + (args[1] << 8) + args[0];
            data = (u8 *)(offset + (s32)args);
            if (offset != 0) {
                size = func_800B16A4(func_800B168C(data, 0));
                buffer = func_80031BDC(size * 2, 0);
                if (sprite->field3A != 0) {
                    func_800B1EA0(func_800B168C(data, 0), 3);
                }
                func_800B1720(func_800B168C(data, 0), buffer, sprite->render.bytes[0] >> 5, sprite->colourFlags & 1);
                part = buffer + size;
                memcpy(part, buffer, size);
                sprite->view->parts = buffer;
                sprite->view->part = (SpriteImagePart *)part;
                sprite->view->anchors = (SpriteAnchor *)func_800B168C(data, 0);
            } else {
                sprite->view->parts = NULL;
                sprite->view->anchors = NULL;
            }
            break;
        case 0x8B:
            func_800BD2E4();
            break;
        case 0xBE:
            argument = args[0] | ((s8)args[1] << 8);
            wait = ((argument >> 11) & 0xF) + 1;
            frame = argument & 0x1FF;
            wait = wait * SPRITE_MOTION_BITS(sprite).divisor / 256;
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
                    frame = SPRITE_FRAME_MAP(sprite)[frame - 1];
                }
                SPRITE_MOTION_BITS(sprite).frameFlip = (argument >> 9) & 1;
                SPRITE_RENDER_BITS(sprite).flip = SPRITE_MOTION_BITS(sprite).frameFlip ^ SPRITE_MOTION_BITS(sprite).mirror;
                if ((argument >> 10) & 1) {
                    SPRITE_RENDER_BITS(sprite).flipY = 1;
                } else {
                    SPRITE_RENDER_BITS(sprite).flipY = 0;
                }
                func_8001D2B0(sprite, frame);
            }
            sprite->countdown += wait;
            sprite->script += 3;
            return;
        case 0x8E:
            sprite->script = NULL;
            return;
        case 0xE2:
            {
                s32 jump = args[0] + (s16)(args[1] << 8);

                func_80021CF8(sprite, sprite->script + 3);
                sprite->script += (s16)jump;
            }
            goto next;
        case 0x85:
            sprite->script = (u8 *)(((u32)sprite->script & 0xFF000000) | func_80021C6C(sprite));
            goto next;
        case 0xFA:
            if (*func_8001FBA4(sprite, args) == 0) {
                break;
            }
            sprite->script += VM_S16(args, 1);
            goto next;
        case 0xD4:
            sprite->script += VM_S16(args, 0);
            if (SPRITE_CALLBACK(sprite) != NULL) {
                SPRITE_CALLBACK(sprite)(sprite);
            }
            goto next;
        case 0x86:
            if (sprite->velocity[1] < 0) {
                sprite->countdown = 1;
                return;
            }
            break;
        case 0x87:
            if ((sprite->y.fixed >> 16) < sprite->ground) {
                sprite->countdown = 1;
                return;
            }
            break;
        case 0x80:
        end:
            SPRITE_FRAME_BITS(sprite).state = 0;
            if (SPRITE_CALLBACK(sprite) != NULL) {
                SPRITE_CALLBACK(sprite)(sprite);
                return;
            }
            if (sprite->idle.mode >= 0) {
                func_800245D8(sprite, sprite->idle.mode);
            }
            SPRITE_FRAME_BITS(sprite).state = 0;
            return;
        case 0x98:
            {
                BattleSprite *parent = sprite->parent;

                if (parent == NULL) {
                    break;
                }
                sprite->countdown = 1;
                if (parent->motion.bytes[3] != SPRITE_WAIT_MOTION(sprite)) {
                    break;
                }
                if (SPRITE_FRAME_BITS(parent).state == 2) {
                    return;
                }
            }
            break;
        case 0x82:
            if (SPRITE_CALLBACK(sprite) != NULL) {
                SPRITE_CALLBACK(sprite)(sprite);
            }
            velocity = sprite->velocity[1];
            func_800245D8(sprite, sprite->motion.bytes[3]);
            sprite->velocity[1] = velocity;
            sprite->countdown = 0;
            func_800C11CC(sprite);
            return;
        case 0x81:
            if (sprite->motion.bytes[3] == 0x3F) {
                goto end;
            }
            sprite->countdown = 0;
            if (SPRITE_CALLBACK(sprite) != NULL) {
                SPRITE_CALLBACK(sprite)(sprite);
            }
            SPRITE_FRAME_BITS(sprite).state = 1;
            return;
        case 0xE4:
            count = func_80021C20(sprite);
            if ((u8)count == 0) {
                break;
            }
            count--;
            func_80021CA0(sprite, count);
            /* fall through */
        case 0xE1:
            sprite->script += VM_S16(args, 0);
            goto next;
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
                s32 frames = (args[0] + 2) * SPRITE_MOTION_BITS(sprite).divisor / 256;

                if (frames == 0) {
                    frames = 1;
                }
                sprite->countdown += frames;
            }
            return;
        case 0xC8:
            func_8001FBE4(sprite, args[0], func_8001FBA4(sprite, args + 1));
            break;
        default:
            func_8001FBE4(sprite, op, args);
            break;
        case 0x9F:
            break;
        }
        sprite->script += D_8004FC40[op];
        goto next;
    }
}
