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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800BFE48", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800BFE48", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800BFE48", func_800C0564);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800BFE48", func_800C08CC);

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

#ifdef NON_MATCHING
/* Set up a command file's parts: transfer its wave bank (freeing the file
 * from it when last), link its sound bank and upload its images; its sound
 * bank. Nonmatching: the original keeps D_800C3A6C's address in a saved
 * register (and reloads the debugger word's) where this is the reverse. */
SoundSystem *func_800C0FAC(s32 *file) {
    s32 *offsets = file;
    s32 *entry;
    s32 n;
    SoundSystem *bank = NULL;
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
            D_800C3A6C = func_80037FD8(entry, 0);
            while (func_8003BDFC(0) != 0) {
                if (D_80010000 != -1) {
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
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800BFE48", func_800C0FAC);
#endif

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
