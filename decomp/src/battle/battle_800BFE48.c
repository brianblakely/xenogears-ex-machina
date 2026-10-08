/* Battle unit from 800BFE48 to 800C11CC (Cygnus CDK GCC 2.7.2).
 * 800BFE48's tables start at 0x80070B08 (0 mod 8) directly after
 * 800BD3AC's odd-length one at 4 mod 8; the functions from 800BD7A0 to
 * 800BFDA8 have no rodata, and the boundary is placed at the first function
 * that has. 800C0564's 25-entry table at 0x80070BB0 is followed directly by
 * 800C11CC's at 0x80070C14 (4 mod 8), so the unit ends before 800C11CC. */
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
#include "settle.h"
#include "curve.h"

/* The cubic B-spline weights (1.0 = 0x4000) of the curve cells: five rows
 * of eight steps, four weights each (func_800C0D18). */
s32 D_800C37E8[40][4] = {
    {0x4000, 0, 0, 0}, {0x2AE0, 0x13B8, 0x162, 0x5}, {0x1B00, 0x1FC0, 0x515, 0x2A},
    {0xFA0, 0x2568, 0xA68, 0x90}, {0x800, 0x2600, 0x10AA, 0x155}, {0x360, 0x22D8, 0x172D, 0x29A},
    {0x100, 0x1D40, 0x1D40, 0x480}, {0x20, 0x1688, 0x2232, 0x725}, {0x1000, 0x2555, 0xAAA, 0},
    {0xAB8, 0x2628, 0xF1A, 0x5}, {0x6C0, 0x24EA, 0x142A, 0x2A}, {0x3E8, 0x220D, 0x197A, 0x90},
    {0x200, 0x1DFF, 0x1EAA, 0x155}, {0xD8, 0x1932, 0x235A, 0x29A}, {0x40, 0x1415, 0x272A, 0x480},
    {0x8, 0xF17, 0x29BA, 0x725}, {0xAAA, 0x2AAA, 0xAAA, 0}, {0x725, 0x29BA, 0xF1A, 0x5},
    {0x480, 0x272A, 0x142A, 0x2A}, {0x29A, 0x235A, 0x197A, 0x90}, {0x155, 0x1EAA, 0x1EAA, 0x155},
    {0x90, 0x197A, 0x235A, 0x29A}, {0x2A, 0x142A, 0x272A, 0x480}, {0x5, 0xF1A, 0x29BA, 0x725},
    {0xAAA, 0x2AAA, 0xAAA, 0}, {0x725, 0x29BA, 0xF17, 0x8}, {0x480, 0x272A, 0x1415, 0x40},
    {0x29A, 0x235A, 0x1932, 0xD8}, {0x155, 0x1EAA, 0x1DFF, 0x200}, {0x90, 0x197A, 0x220D, 0x3E8},
    {0x2A, 0x142A, 0x24EA, 0x6C0}, {0x5, 0xF1A, 0x2628, 0xAB8}, {0xAAA, 0x2555, 0x1000, 0},
    {0x725, 0x2232, 0x1688, 0x20}, {0x480, 0x1D40, 0x1D40, 0x100}, {0x29A, 0x172D, 0x22D8, 0x360},
    {0x155, 0x10AA, 0x2600, 0x800}, {0x90, 0xA68, 0x2568, 0xFA0}, {0x2A, 0x515, 0x1FC0, 0x1B00},
    {0x5, 0x162, 0x13B8, 0x2AE0},
};
s32 (*D_800C3A68)[4] = D_800C37E8;
s32 D_800C3A6C = 0;

/* Return the slots' sprites to their places after an action: sprites in a
 * hit motion leave it, sprites away from their slot walk back, then each
 * takes its condition's idle motion and gains or loses the status effect
 * sprites of its changed status bits (bits 13-15). */
void func_800BFE48(void) {
    BattleSprite *sprite;
    s32 slot;
    s32 motion;
    s32 bit;
    u32 bits;
    u32 removed;
    u32 status;
    u32 old;

    for (slot = 0; slot != 11; slot++) {
        if (BATTLE_AREA.slots[slot].gear) {
            continue;
        }
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite == NULL || func_8009A0DC(slot) == 8) {
            continue;
        }
        switch (sprite->motion.bytes[3]) {
        case 5:
        case 7:
        case 14:
        case 15:
        case 21:
            if (sprite->motion.bytes[3] != D_800C37D4[func_8009A0DC(slot)]) {
                func_800245D8(sprite, 0x10);
                D_800D2E54 &= ~(1 << SPRITE_SLOT(sprite));
            }
            break;
        }
    }
    func_800C0564();

    for (slot = 0; slot != 11; slot++) {
        if (BATTLE_AREA.slots[slot].gear) {
            continue;
        }
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite == NULL || func_8009A0DC(slot) == 8 || sprite->motion.bytes[3] == 0x15) {
            continue;
        }
        if (DISTANCE(sprite->x.part.whole, (u16)BATTLE_AREA.slots[slot].x) >= 9) {
            goto walk;
        }
        if (DISTANCE(sprite->z.part.whole, (u16)BATTLE_AREA.slots[slot].z) < 9) {
            continue;
        }
    walk:
        sprite->motion.bits.doubleStep = 1;
        sprite->target[0] = BATTLE_AREA.slots[slot].x;
        sprite->target[2] = BATTLE_AREA.slots[slot].z;
        sprite->target[1] = 0;
        func_800245D8(sprite, 3);
    }
    func_800C0564();

    for (slot = 0; slot != 11; slot++) {
        if (BATTLE_AREA.sprites[slot] != NULL) {
            BATTLE_AREA.sprites[slot]->motion.bits.doubleStep = 0;
        }
    }

    for (slot = 0; slot != 11; slot++) {
        if (BATTLE_AREA.slots[slot].gear) {
            continue;
        }
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite == NULL) {
            continue;
        }
        if (sprite->motion.bytes[3] != 0x15) {
            func_800BAEB8(slot);
        }
        func_800C0314();
        if (func_8009A0DC(slot) != 8) {
            motion = D_800C37D4[func_8009A0DC(slot)];
            if (!BATTLE_AREA.slots[slot].gear
                && (motion != 0x15 || (D_800C3608 >> SPRITE_SLOT(sprite)) & 1)) {
                if (motion == 1) {
                    motion = sprite->idle.mode;
                }
                if (!BATTLE_AREA.slots[slot].hidden && sprite->motion.bytes[3] != motion) {
                    func_800245D8(sprite, motion);
                }
            }
        }
        status = func_8009A1AC(slot);
        old = (u16)sprite->resource->fieldC;
        sprite->resource->fieldC = status;
        removed = old & ~status;
        bits = status & ~old;
        for (bit = 0; bit != 16; bit++, bits = (bits & 0xFFFF) >> 1) {
            if (bits & 1) {
                switch (bit) {
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                    break;
                case 13:
                    func_800BFDA8(sprite, 9);
                    break;
                case 14:
                    func_800BFDA8(sprite, 10);
                    break;
                case 15:
                    func_800BFDA8(sprite, 11);
                    break;
                }
            }
        }
        bits = removed;
        for (bit = 0; bit != 16; bit++, bits = (bits & 0xFFFF) >> 1) {
            if (bits & 1) {
                switch (bit) {
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                    break;
                case 13:
                    func_800BFD88(sprite, 9);
                    break;
                case 14:
                    func_800BFD88(sprite, 10);
                    break;
                case 15:
                    func_800BFD88(sprite, 11);
                    break;
                }
            }
        }
    }
}

/* Knock down the slots of the area's down mask not yet down: each slot
 * sprite (unless its slot still counts while down) plays motion 0x15 (gears
 * through their stage object), then frames run until their countdowns end;
 * the number knocked down. */
s32 func_800C0314(void) {
    BattleArea *area = &BATTLE_AREA;
    BattleSprite *list[12];
    BattleSprite *sprite;
    s32 i;
    s32 slot;
    s32 downed;
    u8 gear;
    u8 busy;

    downed = 0;
    gear = downed;
    i = func_800BEEB4(area->knockedOut & (area->knockedOut ^ (u16)D_800D2E54), list, D_800C3E1C);
    if (i != 0) {
        for (i--; i >= 0; i--) {
            sprite = list[i];
            if ((D_800C3608 >> SPRITE_SLOT(sprite)) & 1) {
                list[i] = NULL;
                continue;
            }
            sprite->countdown = 0;
            slot = SPRITE_SLOT(sprite);
            if (BATTLE_AREA.slots[slot].gear) {
                gear = 1;
                D_800D3368[slot]->field38 = gear;
                func_800BEE2C(SPRITE_SLOT(sprite), SPRITE_SLOT(sprite), 0x15);
            } else if (sprite->motion.bytes[3] != 0x15) {
                func_800245D8(sprite, 0x15);
            }
            downed++;
            D_800D2E54 |= 1 << SPRITE_SLOT(sprite);
        }
    }
    do {
        busy = 0;
        for (i = 0; i != downed; i++) {
            if (list[i] != NULL && list[i]->field48 != 0 && list[i]->countdown != 0) {
                busy = 1;
            }
        }
        if (busy && downed != 0) {
            func_800BF9EC();
        }
        if (busy && downed != 0) {
            func_800BE790();
        }
    } while (busy);
    if (gear) {
        func_800B136C();
    }
    return downed;
}

/* Run frames until every slot's sprite has settled: gear slots until their
 * sprite's frames run out, others (unless hidden or out of action) until
 * they are back in their condition's idle motion or their idle mode. */
void func_800C0564(void) {
    BattleSprite *sprite;
    s32 slot;
    s8 motion;
    u8 busy;

    while (1) {
        busy = 0;
        for (slot = 0; slot != 11; slot++) {
            sprite = BATTLE_AREA.sprites[slot];
            if (sprite == NULL) {
                continue;
            }
            if (BATTLE_AREA.slots[slot].gear) {
                if (sprite->script != 0) {
                    busy = 1;
                }
                continue;
            }
            if (func_8009A0DC(slot) == 8) {
                continue;
            }
            switch (sprite->motion.bytes[3]) {
            case 0:
            case 1:
            case 5:
            case 7:
            case 14:
            case 15:
            case 20:
            case 21:
            case 22:
            case 24:
                break;
            default:
                if (!BATTLE_AREA.slots[slot].hidden) {
                    motion = sprite->motion.bytes[3];
                    if (motion != D_800C37D4[func_8009A0DC(slot)] && sprite->motion.bytes[3] != sprite->idle.mode) {
                        busy = 1;
                    }
                }
                break;
            }
        }
        if (!busy) {
            break;
        }
        func_800BE790();
    }
}

/* The distance between two points. */
s32 func_800C06E4(VECTOR *a, VECTOR *b) {
    VECTOR d;

    d.vx = a->vx - b->vx;
    d.vy = a->vy - b->vy;
    d.vz = a->vz - b->vz;
    func_8004A414(&d, &d);
    return SquareRoot0(d.vy + d.vz + d.vx);
}

/* The distance between two short points. */
s32 func_800C0758(SVECTOR *a, SVECTOR *b) {
    VECTOR d;

    d.vx = a->vx - b->vx;
    d.vy = a->vy - b->vy;
    d.vz = a->vz - b->vz;
    func_8004A414(&d, &d);
    return SquareRoot0(d.vy + d.vz + d.vx);
}

/* The distance between two points on the ground. */
s32 func_800C07CC(GroundPoint a, GroundPoint b) {
    VECTOR d;

    d.vx = a.x - b.x;
    d.vz = a.z - b.z;
    func_8004A414(&d, &d);
    return SquareRoot0(d.vx + d.vz);
}

/* The direction angles from point to to point from (no roll). */
void func_800C0828(SVECTOR *from, SVECTOR *to, SVECTOR *angles) {
    VECTOR unused[2];
    VECTOR d;
    VECTOR squares;
    s32 ground;

    d.vx = from->vx - to->vx;
    d.vy = from->vy - to->vy;
    d.vz = from->vz - to->vz;
    func_8004A414(&d, &squares);
    ground = SquareRoot0(squares.vx + squares.vz);
    angles->vy = ratan2(d.vz, d.vx);
    angles->vz = ratan2(d.vy, ground);
    angles->vx = 0;
}

/* One span of a curve: the eight points of cell row row over the four
 * control points from p, each drawn as a segment from the previous point
 * (a and b alternate as the current point). */
#define CURVE_SPAN(row, p)                                   \
    for (column = 1; column != 8; column++) {                \
        if (column & 1) {                                    \
            func_800C0D18(row, column, p, &b);               \
            draw(&a, &b);                                    \
        } else {                                             \
            func_800C0D18(row, column, p, &a);               \
            draw(&b, &a);                                    \
        }                                                    \
    }

/* Draw a smooth curve through count points (at least 2; 3 and 4 are padded
 * to 5 by repeating the last point) as segments draw(from, to): a line for
 * 2, otherwise spans of eight segments ending at the last point. */
void func_800C08CC(s32 count, SVECTOR *points, void (*draw)()) {
    VECTOR a;
    VECTOR b;
    SVECTOR unused; /* 8 bytes of the frame that no code touches */
    s32 column;
    s32 first;

    D_800D2FCC = 0;
    if (count < 5) {
        if (count < 3) {
            if (count < 2) {
                return;
            }
            a.vx = points[0].vx;
            a.vy = points[0].vy;
            a.vz = points[0].vz;
            b.vx = points[1].vx;
            b.vy = points[1].vy;
            b.vz = points[1].vz;
            draw(&a, &b);
            return;
        }
        switch (count) {
        case 3:
            points[3].vx = points[2].vx;
            points[3].vy = points[2].vy;
            points[3].vz = points[2].vz;
            points[4].vx = points[2].vx;
            points[4].vy = points[2].vy;
            points[4].vz = points[2].vz;
            break;
        case 4:
            points[4].vx = points[3].vx;
            points[4].vy = points[3].vy;
            points[4].vz = points[3].vz;
            break;
        }
        count = 5;
    }

    first = 0;
    func_800C0D18(0, 0, &points[first], &a);
    CURVE_SPAN(0, &points[first]);
    if (count >= 6) {
        first = 1;
        func_800C0D18(1, 0, &points[first], &a);
        draw(&b, &a);
        CURVE_SPAN(1, &points[first]);
    }
    for (first = 2; first < count - 5; first++) {
        func_800C0D18(2, 0, &points[first], &a);
        draw(&b, &a);
        CURVE_SPAN(2, &points[first]);
    }
    if (count >= 7) {
        first = count - 5;
        func_800C0D18(3, 0, &points[first], &a);
        draw(&b, &a);
        CURVE_SPAN(3, &points[first]);
    }
    first = count - 4;
    func_800C0D18(4, 0, &points[first], &a);
    draw(&b, &a);
    CURVE_SPAN(4, &points[first]);
    if (count >= 5) {
        a.vx = points[count - 1].vx;
        a.vy = points[count - 1].vy;
        a.vz = points[count - 1].vz;
        draw(&b, &a);
    }
}

/* The average of the four points weighted by the weights of cell
 * (row, column). */
void func_800C0D18(s32 row, s32 column, SVECTOR *points, VECTOR *out) {
    VECTOR v;
    s32 cell = row * 8 + column;

    gte_lddp(D_800C3A68[cell][0]);
    v.vx = points[0].vx;
    v.vy = points[0].vy;
    v.vz = points[0].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(out);

    gte_lddp(D_800C3A68[cell][1]);
    v.vx = points[1].vx;
    v.vy = points[1].vy;
    v.vz = points[1].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(&v);
    out->vx += v.vx;
    out->vy += v.vy;
    out->vz += v.vz;

    gte_lddp(D_800C3A68[cell][2]);
    v.vx = points[2].vx;
    v.vy = points[2].vy;
    v.vz = points[2].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(&v);
    out->vx += v.vx;
    out->vy += v.vy;
    out->vz += v.vz;

    gte_lddp(D_800C3A68[cell][3]);
    v.vx = points[3].vx;
    v.vy = points[3].vy;
    v.vz = points[3].vz;
    gte_ldlvl(&v);
    gte_gpf12();
    gte_stlvl(&v);
    out->vx += v.vx;
    out->vy += v.vy;
    out->vz += v.vz;

    out->vx >>= 2;
    out->vy >>= 2;
    out->vz >>= 2;
}

/* Release the transferred sound bank. */
void func_800C0F70(void) {
    if (D_800C3A6C != 0) {
        func_80038310(D_800C3A6C);
    }
    D_800C3A6C = 0;
}

/* Set up a command file's parts: transfer its wave bank (freeing the file
 * from it when last), link its sound bank and upload its images; its sound
 * bank. The wave bank handle is stored through its address taken before
 * the transfer call, and the debugger word is read at its fixed address,
 * as in 800B3F04. */
SoundSystem *func_800C0FAC(s32 *file) {
    SoundSystem *bank = NULL;
    s32 *offsets = file;
    s32 *entry;
    s32 n;
    VramPoint image;
    VramPoint clut;

    for (n = *offsets - 3, offsets += 4; n > 0; n--, offsets++) {
        entry = (s32 *)(*offsets + (s32)file);
        switch (*entry) {
        case 0x73646573: /* "seds" */
            bank = (SoundSystem *)entry;
            func_80038428(bank);
            break;
        case 0x20736477: /* "wds " */
            func_800C0F70();
            D_800C3620 = 0;
            D_800C3622 = 0;
            {
                s32 *waves = &D_800C3A6C;

                *waves = func_80037FD8(entry, 0);
            }
            while (func_8003BDFC(0) != 0) {
                if (*(s32 *)0x80010000 != -1) {
                    __asm__ volatile(".word 0x0001000D"); /* break 1 */
                }
            }
            if (n == 1) {
                func_80031F70(file, *offsets);
            }
            break;
        default:
            image.x = 0x380;
            image.y = 0x100;
            clut.x = 0;
            clut.y = 0x1F4;
            func_80022224(D_8005A474, entry, image, clut, 0);
            break;
        }
    }
    return bank;
}

/* Free the sound bank of a command file. */
void func_800C1140(s32 *file) {
    s32 *offsets = file;
    s32 *entry;
    s32 n;

    for (n = *offsets - 3, offsets += 4; n > 0; n--, offsets++) {
        entry = (s32 *)(*offsets + (s32)file);
        if (*entry == 0x73646573) { /* "seds" */
            func_8003852C(entry);
        }
    }
}
