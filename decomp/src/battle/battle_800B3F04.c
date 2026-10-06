/* Battle unit from 800B3F04 to 800B7870 (Cygnus CDK GCC 2.7.2, like
 * battle_800B15D8.c). 800B1F6C's odd-length table ends at 0x80070850 and
 * 800B3F04's follows unpadded at 0 mod 8, so a unit starts between the two;
 * the functions from 800B2AEC to 800B3E04 have no rodata, and the boundary
 * is placed at the first function that has. Its 107-entry table is followed
 * directly by 800B7870's at 0x800709FC (4 mod 8): the unit ends before it. */
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
#include "sprite_script.h"

/* Run battle sprite script command (1-107) on sprite with its argument
 * bytes: motion, velocity and gravity settings, render flags, camera and
 * display switches, sounds, target highlights and the helpers 800B4EDC-
 * 800B6E84 and the battle module's 801FC6FC/801FC7B0/801FC898. */
void func_800B3F04(BattleSprite *sprite, s32 command, u8 *args) {
    s32 unused[10]; /* allocated in the original frame */
    VECTOR eye;
    VECTOR target;
    BattleSprite *other;
    BattleTask *task;
    s32 distance;
    s32 scale;
    s32 kind;
    u8 isbg, r, g, b;
    u16 played;

    switch (command) {
    case 0x6B:
        func_800B3E04();
        break;
    case 0x6A:
        D_800C492A = 1;
        D_800C35D4 = 0;
        func_8003A89C(D_800C3E54, 0, 0x78);
        break;
    case 0x69:
        if ((sprite->idle.word >> 10) & 1) {
            func_800222BC(sprite, sprite->field4C);
            sprite->idle.word |= 0x400;
        } else {
            func_800222BC(sprite, sprite->field48);
            sprite->idle.word &= ~0x400;
        }
        break;
    case 0x68:
        if (sprite->motion.bytes[3] < 0) {
            func_800222BC(sprite, sprite->field4C);
            sprite->idle.word |= 0x400;
        } else {
            func_800222BC(sprite, sprite->field48);
            sprite->idle.word &= ~0x400;
        }
        break;
    case 0x66:
        D_800C3688 = 1;
        break;
    case 0x64:
        sprite->idle.word |= 0x200;
    case 0x63:
        if ((s16)D_80059454 > 0x200) {
            sprite->field3A = D_80059454;
            sprite->flags.word = (sprite->flags.word & ~0x1F00) | 0x300;
            sprite->render.word |= 0x10000000;
        }
        break;
    case 0x62:
        func_800BD1FC(SPRITE_SLOT(sprite));
        break;
    case 0x61:
        sprite->y.fixed = (sprite->ground - 1) << 16;
        other = sprite->partner;
        distance = (s8)args[0] * sprite->scale / 4096;
        if (other->x.part.whole == (u16)BATTLE_AREA.slots[SPRITE_SLOT(other)].x) {
            if (!sprite->motion.bits.flip) {
                distance = -distance;
            }
        } else if (other->x.part.whole < (u16)BATTLE_AREA.slots[SPRITE_SLOT(other)].x) {
            distance = -distance;
        }
        sprite->target[0] = (other->x.fixed >> 16) + distance;
        sprite->target[2] = other->z.fixed >> 16;
        sprite->target[1] = 0;
        func_800BA768(sprite);
        break;
    case 0x5F:
        D_800C3564 = 1;
        break;
    case 0x60:
        D_800C3564 = SPRITE_SLOT(sprite) + 2;
        break;
    case 0x59:
        D_800D3638 = 0;
        break;
    case 0x5A:
        D_800D3638 = 1;
        break;
    case 0x57:
        func_800AA788(1);
        break;
    case 0x58:
        func_800AA788(0);
        break;
    case 0x56: {
        s32 slot;
        s32 low_slot;
        if (D_800C3622) {
            low_slot = sprite->frameBits.bits.slotLow;
            slot = sprite->motion.bits.slotHigh << 2 | low_slot;
            kind = BATTLE_AREA.slots[slot].field2;
            if (kind == 9) {
                kind = 2;
            }
            if (kind == 10) {
                kind = 6;
            }
            if (kind == 8) {
                kind = 15;
            }
            played = D_800C3626;
            if (!((played >> slot) & 1)) {
                D_800C3626 = played | (1 << slot);
                func_80039E60((kind + 0x52) | (D_8005919C->bank << 16));
            }
        }
        break;
    }
    case 0x54:
        func_801FC898();
        break;
    case 0x4F:
        if (sprite->sound != NULL) {
            func_80039DB8(args[0] | (((SoundSystem *)sprite->sound)->bank << 16));
        }
        break;
    case 0x52:
        if (sprite->sound != NULL) {
            func_80039EC4(args[0] | (((SoundSystem *)sprite->sound)->bank << 16), args[1]);
        }
        break;
    case 0x4D:
        if (sprite->sound != NULL) {
            func_8003A14C(args[0] | (((SoundSystem *)sprite->sound)->bank << 16));
        }
        break;
    case 0x4E:
        if (D_8005919C != NULL) {
            func_8003A14C(args[0] | (D_8005919C->bank << 16));
        }
        break;
    case 0x4C:
        sprite->flags.word |= 2;
        break;
    case 0x48:
        sprite->flags.word |= 1;
        break;
    case 0x67:
        sprite->countdown++;
        eye.vx = D_800D309C.eye.vx - D_800D3354.vx;
        eye.vy = D_800D309C.eye.vy - D_800D3354.vy;
        eye.vz = D_800D309C.eye.vz - D_800D3354.vz;
        func_8004A414(&eye, &eye);
        target.vx = D_800D309C.target.vx - D_800D335C.vx;
        target.vy = D_800D309C.target.vy - D_800D335C.vy;
        target.vz = D_800D309C.target.vz - D_800D335C.vz;
        func_8004A414(&target, &target);
        if (SquareRoot0(eye.vx + eye.vy + eye.vz) < 4 && SquareRoot0(target.vx + target.vy + target.vz) < 4) {
            break;
        }
        sprite->script -= 2;
        break;
    case 0x50:
        sprite->countdown++;
        if (D_800D36BC != 0) {
            D_800D36BC--;
            break;
        }
        sprite->script -= 2;
        break;
    case 0x46:
        func_800B61F8(sprite, args);
        D_800D2D4C = 0;
        break;
    case 0x51:
        func_800B626C(sprite, args);
        break;
    case 0x47:
        func_800B62C8(sprite, args);
        break;
    case 0x44:
        if (*(s32 *)0x80010000 != -1) {
            D_800C3568 = sprite;
        }
        break;
    case 0x45:
        if (*(s32 *)0x80010000 != -1) {
            D_800C3568 = NULL;
        }
        break;
    case 0x43:
        sprite->render.word |= 0x80000000;
        break;
    case 0x3E:
        func_800BF8CC(sprite);
        break;
    case 0x3D:
        func_800B4EDC(sprite);
        break;
    case 0x3A:
        func_800B639C(sprite, args);
        break;
    case 0x42:
        sprite->motion.word &= ~0x20;
        break;
    case 0x38:
        func_800B63F0(sprite, args);
        break;
    case 0x37:
        D_800C3621 = 1;
        break;
    case 0x34:
        sprite->gravity = (((s8)args[0] << 1) * sprite->field82 / 4096) << 5;
        sprite->gravity *= (D_80059198 + 1) * (D_80059198 + 1);
        break;
    case 0x33:
        if (sprite->field48 != 0) {
            func_800245D8(sprite, args[0]);
        }
        break;
    case 0x31:
        SetGeomOffset(0xA0, 0x70);
        break;
    case 0x32:
        SetGeomOffset(0xA0, 0xA4);
        break;
    case 0x30:
        if (sprite->view != NULL && (sprite->render.word & 3) == 1 && sprite->view->anchors != NULL) {
            sprite->view->field3D = sprite->view->anchors[args[0]].y;
            sprite->view->field3C = sprite->view->anchors[args[0]].x;
        }
        break;
    case 0x2F:
        func_800BF730((s32)sprite);
        break;
    case 0x2C:
        func_800B6438(sprite, args);
        break;
    case 0x27:
        func_800B6464(sprite, args);
        break;
    case 0x26:
        func_800B64D4(sprite, args);
        break;
    case 0x28:
        sprite->velocity[0] = (((s8)args[0] << 4) * sprite->field82 / 4096) << 8;
        break;
    case 0x29:
        sprite->velocity[0] += (((s8)args[0] << 4) * sprite->field82 / 4096) << 8;
        break;
    case 0x21:
        sprite->velocity[2] = (((s8)args[0] << 4) * sprite->field82 / 4096) << 12;
        break;
    case 0x22:
        sprite->velocity[2] += (((s8)args[0] << 4) * sprite->field82 / 4096) << 8;
        break;
    case 0x2A:
        sprite->velocity[1] = (((s8)args[0] << 4) * sprite->field82 / 4096) << 12;
        break;
    case 0x2B:
        sprite->velocity[1] += (((s8)args[0] << 4) * sprite->field82 / 4096) << 8;
        break;
    case 0x1F:
        sprite->render.word &= ~0x04000000;
        func_800BA8F4(sprite);
        break;
    case 0x3F:
        sprite->render.word |= 0x04000000;
        break;
    case 0x3B:
        sprite->render.word |= 0x20000000;
        break;
    case 0x3C:
        sprite->render.word &= ~0x20000000;
        break;
    case 0x35:
        sprite->render.word |= 0x08000000;
        break;
    case 0x36:
        sprite->render.word &= ~0x08000000;
        break;
    case 0x23:
        func_800B6518(sprite, args);
        break;
    case 0x40:
        func_800B65B0(sprite, args);
        break;
    case 0x1C:
        func_800B6808(sprite, args);
        break;
    case 0x1B:
        func_800B6A50(sprite, args);
        break;
    case 0x1A:
        func_8001CE74(sprite->task);
        break;
    case 0x2D:
        func_800B6930(sprite, args);
        break;
    case 0x2E:
        func_800B6990(sprite->target, args);
        break;
    case 0x5B:
        func_800B6990(sprite->view->field44, args);
        break;
    case 0x5C:
        func_800B6990(sprite->view->field4C, args);
        break;
    case 0x5D:
        func_800B69E4(sprite->view->field44, args);
        break;
    case 0x5E:
        func_800B69E4(sprite->view->field4C, args);
        break;
    case 0x1D:
        func_800B6A7C(sprite, args);
        break;
    case 0x19:
        func_800B6B98(sprite, args);
        break;
    case 0x18:
        func_800B6BFC(sprite, args);
        break;
    case 0x17:
        scale = (s8)args[0] << 2;
        sprite->velocity[0] = sprite->velocity[0] * scale / 256;
        sprite->velocity[1] = sprite->velocity[1] * scale / 256;
        sprite->velocity[2] = sprite->velocity[2] * scale / 256;
        break;
    case 0x16:
        sprite->render.word |= 0x02000000;
        break;
    case 0x15:
        sprite->render.word |= 0x01000000;
        break;
    case 0x11:
        D_800C3664 = 1;
        break;
    case 0x12:
        D_800C372C = 1;
        isbg = BATTLE_AREA.buffers[0].drawEnv.isbg;
        r = BATTLE_AREA.buffers[0].drawEnv.r0;
        g = BATTLE_AREA.buffers[0].drawEnv.g0;
        b = BATTLE_AREA.buffers[0].drawEnv.b0;
        BATTLE_AREA.buffers[1].drawEnv.isbg = 1;
        BATTLE_AREA.buffers[0].drawEnv.isbg = 1;
        BATTLE_AREA.buffers[0].drawEnv.r0 = 0;
        BATTLE_AREA.buffers[1].drawEnv.r0 = 0;
        BATTLE_AREA.buffers[0].drawEnv.g0 = 0;
        BATTLE_AREA.buffers[1].drawEnv.g0 = 0;
        BATTLE_AREA.buffers[0].drawEnv.b0 = 0;
        BATTLE_AREA.buffers[1].drawEnv.b0 = 0;
        D_800C3CAC = isbg;
        D_800C3CB0[0] = r;
        D_800C3CB0[1] = g;
        D_800C3CB0[2] = b;
        break;
    case 0x13:
        D_800C3664 = 0;
        break;
    case 0x14:
        D_800C372C = 0;
        BATTLE_AREA.buffers[1].drawEnv.isbg = D_800C3CAC;
        BATTLE_AREA.buffers[0].drawEnv.isbg = D_800C3CAC;
        BATTLE_AREA.buffers[0].drawEnv.r0 = D_800C3CB0[0];
        BATTLE_AREA.buffers[1].drawEnv.r0 = D_800C3CB0[0];
        BATTLE_AREA.buffers[0].drawEnv.g0 = D_800C3CB0[1];
        BATTLE_AREA.buffers[1].drawEnv.g0 = D_800C3CB0[1];
        BATTLE_AREA.buffers[0].drawEnv.b0 = D_800C3CB0[2];
        BATTLE_AREA.buffers[1].drawEnv.b0 = D_800C3CB0[2];
        break;
    case 0x10:
        sprite->x.fixed = D_800D335C.vx << 16;
        sprite->y.fixed = D_800D335C.vy << 16;
        sprite->z.fixed = D_800D335C.vz << 16;
        break;
    case 0x53:
        sprite->motion.word &= ~8;
        sprite->motion.word &= ~4;
        sprite->render.word &= ~8;
        sprite->render.word &= ~0x10;
        break;
    case 0xF:
        sprite->motion.word &= ~8;
        sprite->motion.word &= ~4;
        sprite->render.word &= ~8;
        sprite->render.word &= ~0x10;
        func_80021FE0(sprite, 0);
        break;
    case 0xE:
        func_80021FE0(sprite, (s8)args[0] * 16);
        break;
    case 0xD:
        func_800B6E84(sprite, args);
        break;
    case 0xA:
        func_800B6C44(sprite, args);
        break;
    case 0xB:
        func_800B6C98(sprite, args);
        break;
    case 0x49:
        if (sprite->view != NULL) {
            sprite->view->angle[0] += (s8)args[0];
            sprite->render.word |= 0x10000000;
        }
        break;
    case 0x4A:
        if (sprite->view != NULL) {
            sprite->view->angle[1] += (s8)args[0];
            sprite->render.word |= 0x10000000;
        }
        break;
    case 0x4B:
        if (sprite->view != NULL) {
            sprite->view->angle[2] += (s8)args[0];
            sprite->render.word |= 0x10000000;
        }
        break;
    case 0xC:
        func_800B6CEC(sprite, args);
        break;
    case 0x9:
        sprite->direction += 0x800;
        sprite->velocity[0] = -sprite->velocity[0];
        sprite->velocity[1] = -sprite->velocity[1];
        sprite->velocity[2] = -sprite->velocity[2];
        break;
    case 0x1:
        func_8001D4E8(sprite);
        func_800B572C(sprite, func_8001FBA4(sprite, args));
        break;
    case 0x24:
        func_801FC7B0(sprite, args);
        break;
    case 0x25:
        func_801FC6FC(sprite, args);
        break;
    case 0x2:
        task = func_8001D0A4(sprite->task, func_800B5588);
        if (task != NULL) {
            task->destroy(task);
        }
        break;
    case 0x3:
        func_800B5C18(sprite, args);
        break;
    case 0x4:
        while ((task = func_8001D164(func_800B5B3C)) != NULL) {
            task->destroy(task);
        }
        break;
    case 0x1E:
        func_800B61B0(sprite, args);
        break;
    case 0x20:
        func_800B5DC4(sprite);
        break;
    case 0x5:
        func_800B5FBC(sprite, args);
        break;
    case 0x6:
        func_800BC404(1 << SPRITE_SLOT(D_800C3E1C));
        break;
    case 0x55:
        func_800BC404((1 << SPRITE_SLOT(D_800C3E1C)) | D_800D3634);
        break;
    case 0x7:
        func_800BC404(1 << SPRITE_SLOT(sprite->partner));
        break;
    case 0x65:
        func_800BC404(D_800D3634);
        break;
    case 0x41: {
        BattleSprite **active = &D_800C3E1C;
        s32 a = SPRITE_SLOT(sprite->partner);
        func_800BC404((1 << a) | (1 << SPRITE_SLOT(*active)));
        break;
    }
    case 0x8:
        sprite->flags.word |= 0x80000;
        func_800B6DC0(sprite, args);
        break;
    }
}

/* Copy the sprite's current part's 8 x 8 texture block and its 16-colour
 * CLUT row to VRAM (0x3F0, 0x1F0) and (0x3F0, 0x1EE). */
void func_800B4EDC(BattleSprite *sprite) {
    SpriteImagePart *part = sprite->view->part;
    RECT rect;
    u16 tpage;
    u16 clut;

    tpage = part->tpage;
    rect.x = (part->u >> 2) + ((tpage & 0xF) << 6);
    rect.y = part->v + ((tpage << 4) & 0x100);
    rect.w = 8;
    rect.h = 8;
    MoveImage(&rect, 0x3F0, 0x1F0);
    clut = part->clut;
    rect.w = 16;
    rect.h = 1;
    rect.x = clut & 0x3F;
    rect.y = (clut >> 6) & 0x1FF;
    MoveImage(&rect, 0x3F0, 0x1EE);
}

/* The matrix of the sprite's anchor index: its angles, at the anchor's
 * offset (mirrored with the sprite, scaled) from the sprite's position, in
 * the sprite's screen matrix. */
void func_800B4F88(BattleSprite *sprite, s32 index, MATRIX *m) {
    SpriteAnchor *anchor;
    s32 x;
    s32 y;
    SVECTOR angles;

    if (sprite->view != NULL) {
        anchor = (SpriteAnchor *)(index * sizeof(SpriteAnchor) + (s32)sprite->view->anchors);
        y = anchor->y;
        x = anchor->x;
        if ((sprite->motion.word >> 2) & 1) {
            x = -x;
        }
        y = y * sprite->scale / 4096;
        x = x * sprite->scale / 4096;
        angles.vx = anchor->angle[0];
        angles.vy = sprite->view->anchors[index].angle[1];
        angles.vz = sprite->view->anchors[index].angle[2];
        func_8003F738(&angles, m);
        m->t[0] = sprite->view->matrix.t[0] + x;
        m->t[1] = sprite->view->matrix.t[1] + y;
        m->t[2] = sprite->view->matrix.t[2];
        SetMulMatrix(m, &sprite->view->matrix);
    }
}

#ifdef NON_MATCHING
/* The offsets of the sprite's five trail anchors (D_800C356C), mirrored with
 * the sprite and scaled, as points (x, y, 0) of out, when it is drawn one
 * sided. */
void func_800B50D4(BattleSprite *sprite, SVECTOR *out) {
    SpriteAnchor *anchor;
    s32 i;
    s32 x;
    s32 y;

    if ((sprite->render.word & 3) == 1 && sprite->view != NULL && sprite->view->anchors != NULL) {
        for (i = 0; i != 5; i++) {
            anchor = &sprite->view->anchors[D_800C356C[i]];
            x = anchor->x;
            y = anchor->y;
            if ((sprite->motion.word >> 2) & 1) {
                x = -x;
            }
            x = x * sprite->scale / 8192;
            y = y * sprite->scale / 8192;
            out[i].vx = x;
            out[i].vy = y;
            out[i].vz = 0;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B3F04", func_800B50D4);
#endif

/* Draw one segment of a sprite trail (800C08CC) from point a to point b: a
 * light line, and when on screen a quad as wide as the trail (D_800C3E9C)
 * textured from the sprite's copied 8 x 8 block (800B4EDC), joined to the
 * previous segment's far corners; counts the segments in D_800D2FCC. */
void func_800B51B0(VECTOR *a, VECTOR *b) {
    SVECTOR from;
    SVECTOR to;
    long scratch;
    long flag;
    LINE_F2 *line;
    POLY_FT4 *quad;
    s32 abr;
    s32 depth;
    s32 width;
    s32 angle;
    s32 dx;
    s32 dy;

    if (a->vx == b->vx && a->vy == b->vy && a->vz == b->vz) {
        return;
    }
    from.vx = a->vx;
    from.vy = a->vy;
    from.vz = a->vz;
    to.vx = b->vx;
    to.vy = b->vy;
    to.vz = b->vz;
    if (D_80059580 + sizeof(LINE_F2) >= D_80059534) {
        return;
    }
    line = (LINE_F2 *)D_80059580;
    D_80059580 += sizeof(LINE_F2);
    SetLineF2(line);
    abr = D_800C3D4C;
    if (abr != 0) {
        abr--;
        SetSemiTrans(line, 1);
    } else {
        SetSemiTrans(line, 0);
    }
    line->r0 = 0xF0;
    line->g0 = 0xF0;
    line->b0 = 0xF0;
    if (D_800D2FD8[D_800D2FCC / 8] != 0) {
        depth = 2;
    } else {
        depth = -2;
    }
    depth += D_800D3334;
    RotTransPers3(&from, &to, &from, (long *)&line->x0, (long *)&line->x1, &scratch, &scratch, &flag);
    if (depth <= 0 || depth >= 0x1000) {
        return;
    }
    dx = line->x1 - line->x0;
    dy = line->y1 - line->y0;
    width = D_800C3E9C;
    angle = ratan2(dy, dx) + 0x400; /* across the line */
    dx = func_8003F8CC(angle) * width / 8192;
    dy = func_8003F8B0(angle) * width / 8192;
    if (D_80059580 + sizeof(POLY_FT4) >= D_80059534) {
        return;
    }
    quad = (POLY_FT4 *)D_80059580;
    D_80059580 += sizeof(POLY_FT4);
    SetPolyFT4(quad);
    SetShadeTex(quad, 1);
    quad->u0 = 0xC0 + (D_800D2FCC & 7);
    quad->v0 = 0xF0;
    quad->u1 = 0xC1 + (D_800D2FCC & 7);
    quad->v1 = 0xF0;
    quad->u2 = 0xC0 + (D_800D2FCC & 7);
    quad->v2 = 0xF7;
    quad->u3 = 0xC1 + (D_800D2FCC & 7);
    quad->v3 = 0xF7;
    quad->tpage = GetTPage(0, abr, 0x3F0, 0x1F0);
    quad->clut = GetClut(0x3F0, 0x1EE);
    quad->r0 = 0xF0;
    quad->g0 = 0xF0;
    quad->b0 = 0xF0;
    if (D_800D2FCC == 0) {
        quad->x0 = line->x0 - dx;
        quad->y0 = line->y0 - dy;
        quad->x1 = line->x0 + dx;
        quad->y1 = line->y0 + dy;
    } else {
        quad->x0 = D_800C3CA4;
        quad->y0 = D_800C3CA6;
        quad->x1 = D_800C3CA8;
        quad->y1 = D_800C3CAA;
    }
    D_800C3CA4 = quad->x2 = line->x1 - dx;
    D_800C3CA6 = quad->y2 = line->y1 - dy;
    D_800C3CA8 = quad->x3 = line->x1 + dx;
    D_800C3CAA = quad->y3 = line->y1 + dy;
    AddPrim(D_8005956C + depth, quad);
    D_800D2FCC++;
}

/* Trail update: publish its colours and blend for drawing, refresh the
 * anchors when the sprite's frame changed and ease the trail towards them;
 * end once the sprite's motion changes or its phase is 0 or 1. */
void func_800B5588(BattleTask *task) {
    SpriteTrail *trail = task->data;
    BattleSprite *sprite;
    BattleSprite *current;
    s32 phase;
    s32 i;

    D_800D2FD8 = trail->colours;
    sprite = trail->sprite;
    D_800C3E9C = trail->count;
    D_800C3D4C = trail->blend;
    D_800D3334 = sprite->depth;
    if (sprite->frame != trail->frame) {
        trail->frame = sprite->frame;
        func_800B50D4(sprite, trail->anchors);
    }
    for (i = 1; i != 5; i++) {
        trail->trail[i].vx += (trail->anchors[i].vx - trail->trail[i].vx) / 2;
        trail->trail[i].vy += (trail->anchors[i].vy - trail->trail[i].vy) / 2;
        trail->trail[i].vz += (trail->anchors[i].vz - trail->trail[i].vz) / 2;
    }
    trail->trail[0].vx = trail->anchors[0].vx;
    trail->trail[0].vy = trail->anchors[0].vy;
    trail->trail[0].vz = trail->anchors[0].vz;
    current = trail->sprite;
    if (trail->motion != current->motion.bytes[3] || (phase = (current->frameBits.word >> 28) & 3) == 0 || phase == 1) {
        trail->task.destroy(&trail->task);
    }
}

/* Trail draw: the sprite, then the trail (800C08CC). */
void func_800B56E4(BattleTask *draw) {
    SpriteTrail *trail = draw->data;

    func_8001E148(trail->sprite);
    func_800C08CC(5, trail->trail, func_800B51B0);
}

/* Give sprite a trail in colours (the first byte the colour count, 0 for
 * 4). */
void func_800B572C(BattleSprite *sprite, u8 *colours) {
    SpriteTrail *trail = func_8001D1D8(0xB8, sprite->task, func_800B5588, func_800B56E4, NULL);

    trail->sprite = sprite;
    trail->frame = sprite->frame;
    trail->motion = sprite->motion.bytes[3];
    trail->colours = colours;
    trail->blend = sprite->render.bytes[0] >> 5;
    if (colours[0] == 0) {
        trail->count = 4;
    } else {
        trail->count = colours[0];
    }
    func_800B50D4(sprite, trail->trail);
    func_800B50D4(sprite, trail->anchors);
}

/* The distance from the sprite to its target. */
s32 func_800B57E4(BattleSprite *sprite) {
    VECTOR delta;
    VECTOR squares;

    delta.vx = sprite->target[0] - sprite->x.part.whole;
    delta.vy = sprite->target[1] - sprite->y.part.whole;
    delta.vz = sprite->target[2] - sprite->z.part.whole;
    func_8004A414(&delta, &squares);
    return SquareRoot0(squares.vx + squares.vz + squares.vy);
}

/* Approach watch update: resume the sprite at the given script on its next
 * tick once it passes its target or comes near it; end once redirected,
 * its countdown reaches zero, or its motion changes. */
void func_800B5854(BattleTask *task) {
    SpriteApproach *approach = (SpriteApproach *)task;
    u8 done = 0;
    BattleSprite *sprite = approach->sprite;
    s32 last = approach->distance;
    s32 distance = func_800B57E4(sprite);
    u8 *resume;

    approach->distance = distance;
    if (last < distance || distance < approach->near) {
        done = 1;
        resume = approach->resume;
        sprite->countdown = 1;
        sprite->script = resume;
    }
    if (sprite->countdown == 0) {
        done = 1;
    }
    if (sprite->motion.bytes[3] != approach->motion) {
        done = 1;
    }
    if (done) {
        approach->task.destroy(&approach->task);
    }
}

/* Watch sprite approach its target (800B5854), resuming at the given script. */
SpriteApproach *func_800B5924(BattleSprite *sprite, s32 near, u8 *resume) {
    SpriteApproach *approach = func_8001CD08(sprite->task, sizeof(SpriteApproach) - sizeof(BattleTask));

    func_8001CD6C(approach, func_800B5854);
    approach->sprite = sprite;
    approach->distance = func_800B57E4(sprite);
    approach->near = near;
    approach->resume = resume;
    approach->motion = sprite->motion.bytes[3];
    sprite->motion.word |= 0x20;
    return approach;
}

/* The offset of the sprite's anchor index (mirrored with the sprite,
 * scaled), when it is drawn one sided. */
Point2 func_800B59BC(BattleSprite *sprite, s32 index) {
    Point2 offset;

    if (sprite->view != NULL && (sprite->render.word & 3) == 1 && sprite->view->anchors != NULL) {
        offset.y = sprite->view->anchors[index].y;
        offset.x = sprite->view->anchors[index].x;
        if ((sprite->motion.word >> 2) & 1) {
            offset.x = -offset.x;
        }
        offset.y = offset.y * sprite->scale / 4096;
        offset.x = offset.x * sprite->scale / 4096;
        return offset;
    }
}

/* The screen position of the sprite's anchor index. */
Point2 func_800B5AC4(BattleSprite *sprite, s32 index) {
    Point2 point = func_800B59BC(sprite, index);

    point.x += sprite->x.fixed >> 16;
    point.y += sprite->y.fixed >> 16;
    return point;
}

/* Link update: move the partner so that its anchor meets the sprite's;
 * end once the sprite's motion changes or its phase is 0 or 1. */
void func_800B5B3C(SpriteLink *link) {
    BattleSprite *partner = link->sprite->partner;
    Point2 a = func_800B5AC4(link->sprite, link->anchor);
    Point2 b = func_800B5AC4(partner, link->partnerAnchor);
    Point2 delta;
    BattleSprite *sprite;
    s32 phase;

    delta.x = a.x - b.x;
    delta.y = a.y - b.y;
    partner->x.fixed += delta.x << 16;
    partner->y.fixed += delta.y << 16;
    sprite = link->sprite;
    if (link->motion != sprite->motion.bytes[3] || (phase = (sprite->frameBits.word >> 28) & 3) == 0 || phase == 1) {
        link->task.destroy(&link->task);
    }
}

/* Link sprite's partner to it at anchors (low nibble the sprite's, high
 * nibble the partner's). */
SpriteLink *func_800B5C18(BattleSprite *sprite, u8 *anchors) {
    SpriteLink *link = func_8001CD08(sprite->task, sizeof(SpriteLink) - sizeof(BattleTask));

    func_8001CD6C(link, func_800B5B3C);
    link->sprite = sprite;
    link->partner = sprite->partner;
    link->frame = sprite->frame;
    link->motion = sprite->motion.bytes[3];
    link->anchor = *anchors & 0xF;
    link->partnerAnchor = *anchors >> 4;
    func_800B5B3C(link);
    return link;
}

/* Sprite orbit update: place the sprite around its target by its speeds
 * (radius and angles); end with its frames. */
void func_800B5CC0(BattleTask *task) {
    BattleSprite *sprite = task->data;
    MATRIX m;
    SVECTOR offset;
    SVECTOR angles;
    VECTOR position;

    func_80023210(sprite);
    angles.vy = sprite->velocity[1] >> 13;
    angles.vz = sprite->velocity[2] >> 13;
    angles.vx = 0;
    offset.vx = func_80022CAC(sprite, sprite->velocity[0] >> 13);
    offset.vy = 0;
    offset.vz = 0;
    func_8003F738(&angles, &m);
    ApplyMatrix(&m, &offset, &position);
    position.vx += sprite->target[0];
    position.vy += sprite->target[1];
    position.vz += sprite->target[2];
    sprite->x.fixed = position.vx << 16;
    sprite->y.fixed = position.vy << 16;
    sprite->z.fixed = position.vz << 16;
    if (sprite->script == 0) {
        task->destroy(task);
    }
}

/* Start sprite's orbit (800B5CC0). */
void func_800B5DC4(BattleSprite *sprite) {
    sprite->frame = 1;
    func_8001CD6C(sprite->task, func_800B5CC0);
}

/* Draw the sprite as a streak: a line in its colour from its position back
 * along its velocity (scaled down by its size), blended by its render mode;
 * sets its depth. */
void func_800B5DF4(BattleTask *draw) {
    BattleSprite *sprite = draw->data;
    u8 *cursor;
    LINE_F2 *line;
    DR_TPAGE *tpage;
    SVECTOR position;
    long p;
    s32 depth;
    s32 shift;
    u32 blend;

    if (sprite->frame != 0) {
        return;
    }
    cursor = D_80059580;
    if (cursor + sizeof(LINE_F2) >= D_80059534) {
        return;
    }
    position.vx = sprite->x.fixed >> 16;
    position.vy = sprite->y.fixed >> 16;
    D_80059580 = cursor + sizeof(LINE_F2);
    position.vz = sprite->z.fixed >> 16;
    line = (LINE_F2 *)cursor;
    if (sprite->render.bytes[3] & 1) {
        SetRotMatrix(&D_800C3574);
        SetTransMatrix(&D_800C3574);
    } else {
        SetRotMatrix(&D_8004FBB8);
        SetTransMatrix(&D_8004FBB8);
    }
    depth = RotTransPers(&position, (long *)&line->x0, &p, &p) >> D_80050100;
    shift = sprite->size + 8;
    sprite->depth = depth;
    position.vx -= sprite->velocity[0] >> shift;
    position.vy -= sprite->velocity[1] >> shift;
    position.vz -= sprite->velocity[2] >> shift;
    RotTransPers(&position, (long *)&line->x1, &p, &p);
    setlen(line, 3);
    *(u32 *)&line->r0 = *(u32 *)sprite->colour;
    AddPrim(D_8005956C + depth, line);
    tpage = (DR_TPAGE *)D_80059580;
    if (D_80059580 + sizeof(DR_TPAGE) < D_80059534) {
        blend = sprite->render.bytes[0] >> 5;
        if (blend != 0) {
            D_80059580 += sizeof(DR_TPAGE);
            setlen(tpage, 1);
            tpage->code[0] = 0xE1000000 | (((blend - 1) & 3) << 5);
            AddPrim(D_8005956C + depth, tpage);
        }
    }
}

/* Draw sprite with 800B5DF4, uncoloured. */
void func_800B5FBC(BattleSprite *sprite) {
    sprite->frame = 1;
    func_8001CD64(&sprite->task->draw, func_800B5DF4);
    sprite->colourFlags = 0x40;
}

/* Draw the sprite as a line in its colour from it to its parent, blended by
 * its render mode; sets its depth. */
void func_800B6004(BattleTask *draw) {
    BattleSprite *sprite = draw->data;
    BattleSprite *parent;
    LINE_F2 *line;
    DR_TPAGE *tpage;
    SVECTOR position;
    long p;
    s32 depth;
    u32 blend;

    if (sprite->frame != 0) {
        return;
    }
    line = (LINE_F2 *)D_80059580;
    parent = sprite->parent;
    if ((u8 *)(line + 1) >= D_80059534) {
        return;
    }
    position.vx = sprite->x.fixed >> 16;
    position.vy = sprite->y.fixed >> 16;
    position.vz = sprite->z.fixed >> 16;
    D_80059580 = (u8 *)(line + 1);
    SetRotMatrix(&D_8004FBB8);
    SetTransMatrix(&D_8004FBB8);
    depth = RotTransPers(&position, (long *)&line->x0, &p, &p) >> D_80050100;
    sprite->depth = depth;
    position.vx = sprite->x.fixed >> 16;
    position.vy = sprite->y.fixed >> 16;
    position.vz = sprite->z.fixed >> 16;
    position.vx = parent->x.fixed >> 16;
    position.vy = parent->y.fixed >> 16;
    position.vz = parent->z.fixed >> 16;
    RotTransPers(&position, (long *)&line->x1, &p, &p);
    setlen(line, 3);
    *(u32 *)&line->r0 = *(u32 *)sprite->colour;
    AddPrim(D_8005956C + depth, line);
    tpage = (DR_TPAGE *)D_80059580;
    if (D_80059580 + sizeof(DR_TPAGE) < D_80059534) {
        blend = sprite->render.bytes[0] >> 5;
        if (blend != 0) {
            D_80059580 += sizeof(DR_TPAGE);
            setlen(tpage, 1);
            tpage->code[0] = 0xE1000000 | (((blend - 1) & 3) << 5);
            AddPrim(D_8005956C + depth, tpage);
        }
    }
}

/* Draw sprite with 800B6004, uncoloured. */
void func_800B61B0(BattleSprite *sprite) {
    sprite->frame = 1;
    func_8001CD64(&sprite->task->draw, func_800B6004);
    sprite->colourFlags = 0x40;
}

/* Script command: reset D_800D36BC and load stage object set args[0]
 * (800A96B4, on a stack in a heap block). */
void func_800B61F8(BattleSprite *sprite, u8 *args) {
    u8 *stack = func_80031BDC(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    D_800D36BC = 0;
    func_800A96B4(args[0]);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* Script command: free stage object 11 (800A9FF0, on a stack in a heap
 * block). */
void func_800B626C(void) {
    u8 *stack = func_80031BDC(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    func_800A9FF0(0xB);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* Script command: show stage object 11 in mode args[0] (800BEE2C); mode 2
 * first places it and shows it to the side of D_800C3E1C only. */
void func_800B62C8(BattleSprite *sprite, u8 *args) {
    u8 *stack = func_80031BDC(0x4000, 1);
    u32 frame_bits;

    STACK_ENTER(stack + 0x3E00);
    if (args[0] == 2) {
        func_800A979C(0xB, 0x300, 0x100, 0, 0x1DB);
        frame_bits = D_800C3E1C->frameBits.word >> 30;
        func_800BEE2C(0xB, 1 << (((D_800C3E1C->motion.word & 3) << 2) | frame_bits), args[0]);
    } else {
        func_800BEE2C(0xB, D_800D3634, args[0]);
    }
    STACK_LEAVE();
    func_800320E8(stack);
}

/* Script command: fade the lights (800B3CD4) with the parameters at the
 * relative offset in args. */
void func_800B639C(BattleSprite *sprite, u8 *args) {
    s8 *fade = (s8 *)SCRIPT_DATA(args);

    func_800B3CD4((u8)fade[5], (u8)fade[3], (u8)fade[4], fade[0], fade[1], fade[2]);
}

/* Script command: upload the images at the relative offset in args. */
void func_800B63F0(BattleSprite *sprite, u8 *args) {
    func_8002DDE4(SCRIPT_DATA(args), 0, 0, 0, 0, 0, 0);
}

/* Script command: draw sprite with the resident sprite drawer (80025A88). */
void func_800B6438(BattleSprite *sprite) {
    func_8001CD64(&sprite->task->draw, func_80025A88);
}

/* Script command: move the CLUTs of the sprite's parts by (args[0],
 * args[1]). */
void func_800B6464(BattleSprite *sprite, u8 *args) {
    SpriteImagePart *part = sprite->view->part;
    s16 count = sprite->flags.bytes[0] >> 2;
    s16 i = 0;
    s32 x;
    s32 y;

    if (count != 0) {
        do {
            i++;
            x = part->clut & 0x3F;
            y = (part->clut >> 6) & 0x1FF;
            x += args[0];
            y += args[1];
            part->clut = x | (y << 6);
            part++;
        } while (i != count);
    }
}

/* Script command: set battle sprite args[1]'s value (800245D8) to args[0]. */
void func_800B64D4(BattleSprite *sprite, u8 *args) {
    func_800245D8(BATTLE_AREA.sprites[args[1]], args[0]);
}

/* Script command: turn the sprite towards its target (on the x-z plane). */
void func_800B6518(BattleSprite *sprite) {
    GroundPoint position;
    GroundPoint target;
    s16 direction;

    position.x = sprite->x.fixed >> 16;
    position.z = sprite->z.fixed >> 16;
    target.x = sprite->target[0];
    target.z = sprite->target[2];
    direction = func_80023124(target, position);
    func_80021FE0(sprite, direction);
    func_800223B0(sprite, direction);
}

/* Script command: turn the sprite's speed towards its target by at most
 * args[0] * 4 (of 4096) in each angle, keeping its length. */
void func_800B65B0(BattleSprite *sprite, u8 *args) {
    SVECTOR want;
    SVECTOR angles;
    VECTOR delta;
    VECTOR squares;
    VECTOR velocity;
    SVECTOR length;
    MATRIX m;
    VECTOR speed;
    s16 distance;
    s16 speedLength;
    s32 step;
    s16 turn;
    s16 difference;
    SVECTOR *length_vector;

    delta.vx = sprite->target[0] - sprite->x.part.whole;
    delta.vy = sprite->target[1] - sprite->y.part.whole;
    delta.vz = sprite->target[2] - sprite->z.part.whole;
    func_8004A414(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    want.vy = -ratan2(delta.vz, delta.vx);
    want.vz = ratan2(delta.vy, distance);
    want.vx = 0;
    velocity.vx = sprite->velocity[0] >> 7;
    velocity.vy = sprite->velocity[1] >> 7;
    velocity.vz = sprite->velocity[2] >> 7;
    func_8004A414(&velocity, &squares);
    speedLength = SquareRoot0(squares.vy + squares.vx + squares.vz);
    distance = SquareRoot0(squares.vx + squares.vz);
    angles.vy = -ratan2(velocity.vz, velocity.vx);
    angles.vz = ratan2(velocity.vy, distance);
    angles.vx = 0;
    step = args[0] * 4;
    difference = ((s32)((u16)want.vy - (u16)angles.vy) << 20) >> 20;
    turn = difference;
    if (step < abs(difference)) {
        turn = step;
        if (difference < 0) {
            turn = -step;
        }
    }
    angles.vy += turn;
    difference = ((s32)((u16)want.vz - (u16)angles.vz) << 20) >> 20;
    turn = difference;
    length_vector = &length;
    if (step < abs(difference)) {
        turn = step;
        if (difference < 0) {
            turn = -step;
        }
    }
    angles.vz += turn;
    func_80021B04(length_vector, speedLength, 0, 0);
    func_8003F738(&angles, &m);
    ApplyMatrix(&m, length_vector, &speed);
    sprite->velocity[0] = speed.vx << 7;
    sprite->velocity[1] = speed.vy << 7;
    sprite->velocity[2] = speed.vz << 7;
}

/* Script command: aim the sprite's speed (its speed setting, field18) at
 * its target and face it that way. */
void func_800B6808(BattleSprite *sprite) {
    SVECTOR angles;
    VECTOR delta;
    VECTOR squares;
    SVECTOR length;
    MATRIX m;
    VECTOR speed;
    s32 distance;

    delta.vx = sprite->target[0] - sprite->x.part.whole;
    delta.vy = sprite->target[1] - sprite->y.part.whole;
    delta.vz = sprite->target[2] - sprite->z.part.whole;
    func_8004A414(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    angles.vy = -ratan2(delta.vz, delta.vx);
    angles.vz = ratan2(delta.vy, distance);
    angles.vx = 0;
    sprite->direction = angles.vy;
    func_80021B04(&length, (sprite->speed << 9) >> 16, 0, 0);
    func_8003F738(&angles, &m);
    ApplyMatrix(&m, &length, &speed);
    sprite->velocity[0] = speed.vx << 7;
    sprite->velocity[1] = speed.vy << 7;
    sprite->velocity[2] = speed.vz << 7;
}

/* Script command: set a 16.16 point to the script's three s16s. */
void func_800B6930(Fixed16 *point, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    point[0].fixed = SCRIPT_S16(data, 0) << 16;
    point[1].fixed = SCRIPT_S16(data, 2) << 16;
    point[2].fixed = SCRIPT_S16(data, 4) << 16;
}

/* Script command: set a vector to the script's three s16s. */
void func_800B6990(s16 *vector, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 value;

    value = SCRIPT_S16(data, 0);
    vector[0] = value;
    value = SCRIPT_S16(data, 2);
    vector[1] = value;
    value = SCRIPT_S16(data, 4);
    vector[2] = value;
}

/* Script command: add the script's three s16s to a vector. */
void func_800B69E4(s16 *vector, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 value;

    value = SCRIPT_S16(data, 0);
    vector[0] += value;
    value = SCRIPT_S16(data, 2);
    vector[1] += value;
    value = SCRIPT_S16(data, 4);
    vector[2] += value;
}

/* Script command: take the speed of the sprite's parent. */
void func_800B6A50(BattleSprite *sprite) {
    BattleSprite *parent = sprite->parent;

    sprite->velocity[0] = parent->velocity[0];
    sprite->velocity[1] = parent->velocity[1];
    sprite->velocity[2] = parent->velocity[2];
}

/* Script command: break the sprite's image into pieces (801FC4C4) with the
 * script's parameters (scaled with the sprite when field3A is set), leaving
 * it without an image. */
void func_800B6A7C(BattleSprite *sprite, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 a;
    s32 b;
    s32 c;
    SpriteView *view;

    if (sprite->field3A != 0) {
        a = func_80022CAC(sprite, (((s8 *)data)[1] << 3) | data[0]) << 8;
        b = func_80022CAC(sprite, data[2]) << 16;
        c = func_80022CAC(sprite, data[3]) << 16;
    } else {
        a = ((((s8 *)data)[1] << 3) | data[0]) << 5;
        b = data[2] << 13;
        c = data[3] << 13;
    }
    view = sprite->view;
    func_801FC4C4(view->anchors, view->parts, &view->matrix, a, b, c, data[4] * 16, data[5] * 4);
    sprite->view->anchors = NULL;
    sprite->view->parts = NULL;
    sprite->view->part = NULL;
}

/* Script command: start a burst from the sprite (801FC53C) with the
 * script's parameters. */
void func_800B6B98(BattleSprite *sprite, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    func_801FC53C(sprite, data[0] * 16, data[1], ((s8 *)data)[2] * 8, data[3] * 8, ((s8 *)data)[4] * 8, data[5]);
}

/* Script command: copy the screen to VRAM (0x2C0, 0x100) and shatter it
 * (800B73A0). */
void func_800B6BFC(void) {
    RECT rect;

    rect.w = 320;
    rect.x = 0;
    rect.y = 0;
    rect.h = 224;
    MoveImage(&rect, 0x2C0, 0x100);
    func_800B73A0();
}

/* Script command: turn the sprite about z to its speed's direction in x-y. */
void func_800B6C44(BattleSprite *sprite) {
    sprite->view->angle[2] = ratan2(sprite->velocity[1] >> 8, sprite->velocity[0] >> 8);
    sprite->render.word |= 0x10000000;
}

/* Script command: turn the sprite about x to its speed's direction in x-z. */
void func_800B6C98(BattleSprite *sprite) {
    sprite->view->angle[0] = ratan2(sprite->velocity[2] >> 8, sprite->velocity[0] >> 8);
    sprite->render.word |= 0x10000000;
}

/* Script command: turn the sprite to its speed's direction. */
void func_800B6CEC(BattleSprite *sprite) {
    VECTOR speed;
    VECTOR squares;
    s32 distance;

    speed.vx = sprite->velocity[0] >> 8;
    speed.vy = sprite->velocity[1] >> 8;
    speed.vz = sprite->velocity[2] >> 8;
    if (speed.vz == 0) {
        speed.vz = 4;
    }
    func_8004A414(&speed, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    sprite->view->angle[1] = -ratan2(speed.vz, speed.vx);
    sprite->view->angle[2] = ratan2(speed.vy, distance);
    sprite->view->angle[0] = 0;
    sprite->render.word |= 0x10000000;
}

/* Script command: clear the sprite's eight anchors. */
void func_800B6DC0(BattleSprite *sprite) {
    s32 i;

    if (sprite->view != NULL && sprite->view->anchors != NULL) {
        for (i = 0; i != 8; i++) {
            sprite->view->anchors[i].x = 0;
            sprite->view->anchors[i].y = 0;
            sprite->view->anchors[i].angle[0] = 0;
            sprite->view->anchors[i].angle[1] = 0;
            sprite->view->anchors[i].angle[2] = 0;
        }
        sprite->view->field3C = 0;
        sprite->view->field3D = 0;
    }
}

/* Script command: turn the sprite's speed about z by args[0] * 16. */
void func_800B6E84(BattleSprite *sprite, s8 *args) {
    SVECTOR angles;
    MATRIX m;
    VECTOR velocity;

    func_80021B04(&angles, 0, 0, args[0] * 16);
    func_8003F738(&angles, &m);
    ApplyMatrixLV(&m, (VECTOR *)sprite->velocity, &velocity);
    sprite->velocity[0] = velocity.vx;
    sprite->velocity[1] = velocity.vy;
    sprite->velocity[2] = velocity.vz;
}

/* Shatter update: after its delay each shard fades, moves, turns and
 * falls, its velocity easing out. */
void func_800B6F0C(BattleTask *task) {
    ScreenShatter *shatter = task->data;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVECTOR step;

    shatter->frame++;
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                if (shard->delay != 0) {
                    shard->delay--;
                } else {
                    poly = &shard->poly[BATTLE_AREA.buffer];
                    poly->r0 = func_80021AD8(poly->r0, -6);
                    poly->g0 = func_80021AD8(poly->g0, -6);
                    poly->b0 = func_80021AD8(poly->b0, -6);
                    step.vx = shard->velocity.vx >> 16;
                    step.vy = shard->velocity.vy >> 16;
                    step.vz = shard->velocity.vz >> 16;
                    shard->position.vx += step.vx;
                    shard->position.vy += step.vy;
                    shard->position.vz += step.vz;
                    shard->angles.vx += shard->spin.vx;
                    shard->angles.vy += shard->spin.vy;
                    shard->angles.vz += shard->spin.vz;
                    shard->velocity.vy -= shard->velocity.vy / 16;
                    shard->velocity.vx -= shard->velocity.vx / 16;
                    shard->velocity.vz -= shard->velocity.vz / 16;
                    shard->velocity.vy += shard->fall;
                }
            }
        }
    }
}

/* Shatter draw: into the ordering table (800B7160). */
void func_800B7134(BattleTask *draw) {
    D_800C3CB4 = D_8005956C;
    func_800B7160(draw);
}

#ifdef NON_MATCHING
/* Shatter draw: each shard that has fallen in front of the screen (z at
 * least 64), its layer's triangle turned and placed, projected at the
 * screen centre and distance 512. */
void func_800B7160(BattleTask *draw) {
    ScreenShatter *shatter = draw->data;
    s32 offsetX;
    s32 offsetY;
    MATRIX m;
    s32 p;
    s32 flag;
    s32 screen;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVECTOR *triangle;

    ReadGeomOffset(&offsetX, &offsetY);
    screen = ReadGeomScreen();
    SetGeomOffset(160, 112);
    SetGeomScreen(512);
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                poly = &shard->poly[BATTLE_AREA.buffer];
                if (shard->position.vz >= 64) {
                    func_8003F738(&shard->angles, &m);
                    TransMatrix(&m, &shard->position);
                    SetRotMatrix(&m);
                    SetTransMatrix(&m);
                    if (layer == 0) {
                        triangle = D_800C3594;
                    } else {
                        triangle = D_800C35AC;
                    }
                    AddPrim(D_800C3CB4 + (RotTransPers3(&triangle[0], &triangle[1], &triangle[2], (u32 *)&poly->x0,
                                                        (u32 *)&poly->x1, (u32 *)&poly->x2, &p, &flag) >> 6),
                            poly);
                }
            }
        }
    }
    SetGeomOffset(offsetX, offsetY);
    SetGeomScreen(screen);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B3F04", func_800B7160);
#endif

/* Free a heap block once drawing is done. */
void func_800B7330(void *block) {
    DrawSync(0);
    func_800320E8(block);
}

/* Shatter destroy: end the draw task, the task and its sprites. */
void func_800B7364(ScreenShatter *shatter) {
    func_8001CB48(&shatter->draw);
    func_8001CD94(shatter);
    func_80025180(shatter);
}

/* Shatter the screen copied to VRAM (0x2C0, 0x100). */
void func_800B73A0(void) {
    func_800B7424(func_8001D1D8(sizeof(ScreenShatter), 0, func_800B6F0C, func_800B7134, func_800B7364));
}

/* Set up a shattered screen in a heap block (not run as a task). */
ScreenShatter *func_800B73EC(void) {
    ScreenShatter *shatter = func_80031BDC(sizeof(ScreenShatter), 1);

    shatter->task.data = shatter;
    shatter->draw.data = shatter;
    return func_800B7424(shatter);
}

/* Cut the screen copied to VRAM (0x2C0, 0x100) into shards: per 16 x 16
 * cell an upper-left and a lower-right triangle, each starting further out
 * the later it moves, launched outwards at a random speed with a random spin
 * and fall. */
ScreenShatter *func_800B7424(ScreenShatter *shatter) {
    ScreenShard *shard;
    POLY_FT3 *poly;
    VECTOR square;
    SVECTOR angles;
    MATRIX m;
    s32 radius;
    s32 row;
    s32 layer;
    s32 column;
    s32 i;
    s32 distance;
    s32 turn;
    s32 tilt;
    s32 r;
    s32 base;
    s32 yaw;

    shatter->frame = 0;
    radius = SquareRoot0(160 * 160 + 112 * 112) << 10;
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                shard->angles.vx = 0;
                shard->angles.vy = 0;
                shard->angles.vz = 0;
                if (layer == 0) {
                    shard->position.vx = (column * 16 - 155) * 32;
                    shard->position.vy = (row * 16 - 107) * 32;
                    shard->position.vz = 0x4000;
                } else {
                    shard->position.vx = (column * 16 - 149) * 32;
                    shard->position.vy = (row * 16 - 101) * 32;
                    shard->position.vz = 0x4000;
                }
                D_800C35C4.vz = -500 << 16;
                D_800C35C4.vz = D_800C35C4.vz + (-(rand() % 1000) << 16);
                func_8004A414(&shard->position, &square);
                distance = SquareRoot0(square.vx + square.vy);
                shard->delay = (radius / 32 - distance) / 2048; /* overwritten */
                shard->delay = distance / 1024;
                /* Turn outwards, a little at random; tilt by the distance. */
                turn = ratan2(shard->position.vy, shard->position.vx);
                r = rand();
                yaw = (turn += 0x600) + r % 1024;
                tilt = (distance << 11) / radius;
                r = rand();
                base = tilt - 0x20;
                tilt = base + r % 64;
                angles.vx = 0;
                angles.vy = tilt;
                angles.vz = yaw;
                func_8004ABBC(&angles, &m);
                ApplyMatrixLV(&m, &D_800C35C4, &shard->velocity);
                shard->fall = 0x70800 - ((rand() % 1600) << 8);
                shard->spin.vx = (rand() & 0xFF) - 0x7F;
                shard->spin.vy = (rand() & 0xFF) - 0x7F;
                shard->spin.vz = (rand() & 0x1FF) - 0xFF;
                for (i = 0; i != 2; i++) {
                    poly = &shard->poly[i];
                    SetPolyFT3(poly);
                    SetShadeTex(poly, 0);
                    poly->r0 = 0xFF;
                    poly->g0 = 0xFF;
                    poly->b0 = 0xFF;
                    setSemiTrans(poly, 0);
                    poly->tpage = GetTPage(2, 1, column * 16 + 0x2C0, 0x100);
                    if (layer == 0) {
                        poly->u0 = column * 16 & 0x3F;
                        poly->v0 = row * 16;
                        poly->u1 = (column * 16 & 0x3F) + 16;
                        poly->v1 = row * 16;
                        poly->u2 = column * 16 & 0x3F;
                        poly->v2 = row * 16 + 16;
                    } else {
                        poly->u0 = (column * 16 & 0x3F) + 16;
                        poly->v0 = row * 16;
                        poly->u1 = (column * 16 & 0x3F) + 16;
                        poly->v1 = row * 16 + 16;
                        poly->u2 = column * 16 & 0x3F;
                        poly->v2 = row * 16 + 16;
                    }
                }
            }
        }
    }
    return shatter;
}
