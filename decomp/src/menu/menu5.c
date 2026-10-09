#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "resident/window.h"
#include "actor.h"
#include "bout.h"
#include "brain.h"
#include "camera.h"
#include "debug.h"
#include "display.h"
#include "effects.h"
#include "glow.h"
#include "gte.h"
#include "helpers.h"
#include "hud.h"
#include "menus.h"
#include "mode.h"
#include "node.h"
#include "resident_views.h"
#include "script.h"
#include "select.h"
#include "sound.h"
#include "stage.h"
#include "task.h"
#include "text.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static SVECTOR D_80092768; /* stored map position */
static s32 D_80092770;
static s32 D_80092774;
static s32 D_80092778; /* unreferenced */
static s32 D_8009277C; /* framing heading */
static s16 D_80092780; /* fade level */
static s32 D_80092784;
static POLY_FT4 *D_80092788[2]; /* floor quad pools: template, working copy */
static s32 D_80092790;
static s32 D_80092794; /* scene mode */
static s32 D_80092798; /* first actor's model id */
static s32 D_8009279C; /* second actor's model id */
static u16 D_800927A0; /* floor palette */
static u16 D_800927A4; /* floor texture page */
static u16 D_800927A8; /* floor texture row */
static s32 D_800927AC; /* orbit angle */
static s32 D_800927B0; /* orbit speed */
static void *D_800927B4[2]; /* loaded model of each actor slot */
static s32 D_800927BC[2]; /* unreferenced */
static s32 D_800927C4;
static s32 D_800927C8; /* unreferenced */
static u8 *D_800927CC; /* per map row: right edge of the drawn span */
static u8 *D_800927D0; /* per map row: left edge of the drawn span */
static u16 D_800927D4; /* backdrop texture page */
static u16 D_800927D8; /* backdrop palette */
static u8 D_800927DC; /* backdrop texel u */
static u8 D_800927E0; /* backdrop texel v */
static s32 D_800927E4[2]; /* unreferenced */
static u8 D_800927EC;

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk). Nothing addresses the words
 * marked unreferenced; each is the size of one more per-buffer pair of the
 * array before it. */
static POLY_G4 D_80095580[2]; /* sky gradient, one per buffer */
static DR_TPAGE D_800955C8[4]; /* backdrop texture pages: two, one per buffer each */
static s32 D_800955E8[4]; /* unreferenced */
static SPRT D_800955F8[6]; /* backdrop sprites: three parts, one per buffer each */
static s32 D_80095670[10]; /* unreferenced */
static Hud D_80095698;
static DR_TPAGE D_80095918[4]; /* HUD texture page modes, two per buffer */
static Line3D D_80095938[100];

/* Stage colours, read by func_80082A70 alone. */
Environment D_8009178C[] = {
    { { 0x10, 0x60, 0x80 }, 0, 0x38, 0x38, 0x38, 0, { 0x70, 0x70, 0x70 }, 0, { 0x80, 0x80, 0x80 }, 0, 1 },
    { { 0x30, 0x60, 0x40 }, 0, 0x40, 0x40, 0x40, 0, { 0xE0, 0xB0, 0x70 }, 0, { 0x90, 0x90, 0x90 }, 0, 1 },
    { { 0x08, 0x30, 0x3F }, 0, 0x20, 0x20, 0x30, 0, { 0x40, 0x40, 0x50 }, 0, { 0x30, 0x30, 0x38 }, 0, 0 },
};

/* Files of the menu mode, loaded by func_80029AFC up to the zero file. */
FileRequest D_800917C0[6] = { { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 0 } };

s32 D_800917F0 = 0;

DVECTOR D_800917F4[8] = {
    { 0, 1 }, { 0x7F, 1 }, { 0x77, 0x10 }, { 0x40, 0x10 },
    { 0x34, 8 }, { -4, 8 }, { 0x48, 1 }, { 0x38, 1 },
};

u16 D_80091814[16] = {
    0x8000, 0x8421, 0x8842, 0x8C63, 0x9084, 0x94A5, 0x98C6, 0x9CE7,
    0xA108, 0xA529, 0xA94A, 0xAD6B, 0xB18C, 0xB5AD, 0xB9CE, 0x8000,
};

/* The round map: per row, the leftmost and rightmost allowed columns. */
u8 D_80091834[128] = {
    0x32, 0x2E, 0x2B, 0x28, 0x26, 0x23, 0x21, 0x20, 0x1E, 0x1C, 0x1B, 0x1A, 0x18, 0x17, 0x16, 0x15,
    0x14, 0x12, 0x11, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0C, 0x0B, 0x0A, 0x09, 0x09, 0x08, 0x08,
    0x07, 0x06, 0x06, 0x05, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x07, 0x08,
    0x08, 0x09, 0x09, 0x0A, 0x0B, 0x0C, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x11, 0x12, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x1A, 0x1B, 0x1C, 0x1E, 0x20, 0x21, 0x23, 0x26, 0x28, 0x2B, 0x2E, 0x32, 0x38,
};
u8 D_800918B4[128] = {
    0x4C, 0x50, 0x53, 0x56, 0x58, 0x5B, 0x5D, 0x5E, 0x60, 0x62, 0x63, 0x64, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x6C, 0x6D, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x72, 0x73, 0x74, 0x75, 0x75, 0x76, 0x76,
    0x77, 0x78, 0x78, 0x79, 0x79, 0x79, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C, 0x7D, 0x7D,
    0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7D, 0x7D, 0x7D,
    0x7D, 0x7C, 0x7C, 0x7C, 0x7B, 0x7B, 0x7B, 0x7A, 0x7A, 0x79, 0x79, 0x79, 0x78, 0x78, 0x77, 0x76,
    0x76, 0x75, 0x75, 0x74, 0x73, 0x72, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6D, 0x6C, 0x6A, 0x69,
    0x68, 0x67, 0x66, 0x64, 0x63, 0x62, 0x60, 0x5E, 0x5D, 0x5B, 0x58, 0x56, 0x53, 0x50, 0x4C, 0x46,
};

/* Map tile texture positions per orientation; the icon pages are set at
 * run time. */
MapTable D_80091934 = {
    {
        { 0x0000, 0x000F, 0x0F00, 0x0F0F },
        { 0x000F, 0x0000, 0x0F0F, 0x0F00 },
        { 0x0F00, 0x0F0F, 0x0000, 0x000F },
        { 0x0F0F, 0x0F00, 0x000F, 0x0000 },
    },
};

/* Build the menu backdrop packets: the sky gradient quads, the backdrop
 * texture pages, the six backdrop sprites; scale the map heights and set
 * up the map drawing pools. */
void func_80081ECC(void) {
    POLY_G4 *sky;
    s16 *height;
    s32 i;

    func_800875EC();
    sky = &D_80095580[0];
    setlen(sky, 8);
    sky->code = 0x38;
    sky->r0 = 0x10;
    sky->g0 = 0x60;
    sky->b0 = 0x7F;
    *(u16 *)&sky->r1 = 0x6010;
    sky->b1 = 0x7F;
    *(u16 *)&sky->r2 = 0x7F7F;
    sky->b2 = 0x7F;
    *(u16 *)&sky->r3 = 0x7F7F;
    sky->b3 = 0x7F;
    *(u32 *)&sky->x0 = 0;
    *(u32 *)&sky->x1 = 0x140;
    *(u32 *)&sky->x2 = 0x600000;
    *(u32 *)&sky->x3 = 0x600140;
    D_80095580[1] = D_80095580[0];
    SetDrawTPage(&D_800955C8[0], 0, 0, GetTPage(2, 2, 0, 0x100));
    SetDrawTPage(&D_800955C8[1], 0, 0, GetTPage(2, 2, 0, 0));
    SetDrawTPage(&D_800955C8[2], 0, 0, GetTPage(2, 2, 0x100, 0x100));
    SetDrawTPage(&D_800955C8[3], 0, 0, GetTPage(2, 2, 0x100, 0));
    setlen(&D_800955F8[0], 4);
    *(u32 *)&D_800955F8[0].r0 = 0x64707070;
    D_800955F8[0].code &= ~1; /* texture not shaded */
    D_800955F8[0].code |= 2;  /* semi-transparent */
    *(u32 *)&D_800955F8[0].x0 = 0;
    *(u16 *)&D_800955F8[0].u0 = 0;
    *(u32 *)&D_800955F8[0].w = 0xDB0080;
    func_800732AC(&D_800955F8[1], &D_800955F8[0], sizeof(SPRT) * 5);
    D_800955F8[3].u0 = 0x80;
    D_800955F8[2].u0 = 0x80;
    D_800955F8[3].x0 = 0x80;
    D_800955F8[2].x0 = 0x80;
    D_800955F8[5].x0 = 0x100;
    D_800955F8[4].x0 = 0x100;
    D_800955F8[5].w = 0x40;
    D_800955F8[4].w = 0x40;
    height = (s16 *)D_800928DC;
    for (i = 0; i < 0x4000; i++) {
        *height *= 12;
        height += 2;
    }
    func_80087830();
}

/* Draw the large direction arrow at a map position (8.8 fixed point). */
void func_80082178(s32 x, s32 z, s32 direction) {
    s32 start_x;
    s32 start_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    x >>= 8;
    z >>= 8;
    last_x = start_x = x + ((func_8003F8B0(direction + 0x280) * 10) >> 12);
    last_z = start_z = z + ((func_8003F8CC(direction + 0x280) * 10) >> 12);
    angle = direction + 0x580;
    for (i = 0; i < 6; i++) {
        next_x = x + ((func_8003F8B0(angle) * 24) >> 12);
        next_z = z + ((func_8003F8CC(angle) * 24) >> 12);
        func_80087698(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x100;
    }
    next_x = x + ((func_8003F8B0(direction - 0x280) * 10) >> 12);
    next_z = z + ((func_8003F8CC(direction - 0x280) * 10) >> 12);
    func_80087698(last_x, last_z, next_x, next_z);
    func_80087698(start_x, start_z, next_x, next_z);
}

/* Draw the small direction arrow at a map position (8.8 fixed point). */
void func_80082300(s32 x, s32 z, s32 direction) {
    s32 start_x;
    s32 start_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    x >>= 8;
    z >>= 8;
    last_x = start_x = x + ((func_8003F8B0(direction + 0x100) * 16) >> 12);
    last_z = start_z = z + ((func_8003F8CC(direction + 0x100) * 16) >> 12);
    angle = direction + 0x78A;
    for (i = 0; i < 3; i++) {
        next_x = x + ((func_8003F8B0(angle) * 32) >> 12);
        next_z = z + ((func_8003F8CC(angle) * 32) >> 12);
        func_80087698(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x75;
    }
    next_x = x + ((func_8003F8B0(direction - 0x100) * 16) >> 12);
    next_z = z + ((func_8003F8CC(direction - 0x100) * 16) >> 12);
    func_80087698(last_x, last_z, next_x, next_z);
    func_80087698(start_x, start_z, next_x, next_z);
}

/* Copy the stored map position. */
void func_80082458(SVECTOR *out) {
    *out = D_80092768;
}

/* Raise a ground corner by its square's kind: 1 by 0x100, 3 by 0x40. */
#define GROUND_KIND_LIFT(corner, x, z)                                          \
    switch (((u32 *)D_800928DC)[(z) * 128 + (x)] & 0x3000000) {                \
    case 0x1000000:                                                            \
        (corner).vy += 0xC0;                                                   \
    case 0x3000000:                                                            \
        (corner).vy += 0x40;                                                   \
    }

/* Ground height under a position: the plane through the triangle of its
 * 256-unit square that contains it (corners optionally raised by their
 * square's kind); the plane's normal is kept in D_80092768.
 * Once the triangle is copied, its wide plane point reuses the last
 * corner's scratch slot and the following eight bytes. */
s32 func_80082488(VECTOR *pos, s32 lift) {
    struct {
        s32 unused0[2];
        union {
            SVECTOR corner[5]; /* four corners and room for the later VECTOR */
            struct {
                SVECTOR unused[3];
                VECTOR point;
            } plane;
        } geometry;
        SVECTOR tri[3];
        s32 unused1[2];
    } scratch;
    GroundSquare *square;
    s32 x;
    s32 z;
    s32 x0;
    s32 z0;

    x = pos->vx;
    z = pos->vz;
    x0 = x & ~0xFF;
    x >>= 8;
    z0 = z & ~0xFF;
    z >>= 8;
    square = (GroundSquare *)((z * 128 + x) * sizeof(GroundSquare) +
                             (s32)D_800928DC);
    scratch.geometry.corner[0].vx = x0;
    scratch.geometry.corner[0].vy = square[0].height;
    scratch.geometry.corner[0].vz = z0;
    scratch.geometry.corner[1].vx = x0 + 0x100;
    scratch.geometry.corner[1].vy = square[129].height;
    scratch.geometry.corner[1].vz = z0 + 0x100;
    scratch.geometry.corner[2].vx = x0 + 0x100;
    scratch.geometry.corner[2].vy = square[1].height;
    scratch.geometry.corner[2].vz = z0;
    scratch.geometry.corner[3].vx = x0;
    scratch.geometry.corner[3].vy = square[128].height;
    scratch.geometry.corner[3].vz = z0 + 0x100;
    if (lift) {
        GROUND_KIND_LIFT(scratch.geometry.corner[0], x, z);
        GROUND_KIND_LIFT(scratch.geometry.corner[1], x + 1, z + 1);
        GROUND_KIND_LIFT(scratch.geometry.corner[2], x + 1, z);
        GROUND_KIND_LIFT(scratch.geometry.corner[3], x, z + 1);
    }
    if ((scratch.geometry.corner[0].vz - scratch.geometry.corner[1].vz) * pos->vx +
            (scratch.geometry.corner[1].vx - scratch.geometry.corner[0].vx) * pos->vz +
            scratch.geometry.corner[0].vx * scratch.geometry.corner[1].vz -
            scratch.geometry.corner[1].vx * scratch.geometry.corner[0].vz < 0) {
        scratch.tri[0] = scratch.geometry.corner[0];
        scratch.tri[1] = scratch.geometry.corner[1];
        scratch.tri[2] = scratch.geometry.corner[2];
    } else {
        scratch.tri[0] = scratch.geometry.corner[0];
        scratch.tri[1] = scratch.geometry.corner[3];
        scratch.tri[2] = scratch.geometry.corner[1];
    }
    func_8002DB84(&scratch.tri[0], &scratch.tri[1], &scratch.tri[2], &D_80092768);
    {
        scratch.geometry.plane.point.vx = scratch.tri[0].vx;
        scratch.geometry.plane.point.vy = scratch.tri[0].vy;
        scratch.geometry.plane.point.vz = scratch.tri[0].vz;
        return pos->vy +
               (scratch.geometry.plane.point.vx * D_80092768.vx +
                scratch.geometry.plane.point.vy * D_80092768.vy +
                scratch.geometry.plane.point.vz * D_80092768.vz -
                (pos->vx * D_80092768.vx + pos->vy * D_80092768.vy +
                 pos->vz * D_80092768.vz)) / D_80092768.vy;
    }
}

/* Ground height of the map cell under a position (cells of 256 units). */
s32 func_80082880(SVECTOR *pos) {
    VECTOR unused[3]; /* the original frame has 0x30 unused bytes */
    s16 x, z;

    x = pos->vx >> 8;
    z = pos->vz >> 8;
    return *(s16 *)&((s32 *)D_800928DC)[x + z * 128];
}

/* The map cell word under a position (cells of 256 units). */
s32 func_800828C4(VECTOR *pos) {
    s32 x = pos->vx >> 8;
    s32 z = pos->vz >> 8;

    return ((s32 *)D_800928DC)[z * 128 + x];
}

/* Keep a moving position inside the circular arena of the given radius
 * around the scene centre: when the step would leave it, turn the step
 * along the rim and shorten it until the end point is inside. */
void func_800828F8(VECTOR *pos, VECTOR *step, s32 radius) {
    VECTOR local;
    VECTOR next;
    VECTOR square;
    MATRIX rim;
    MATRIX back;
    SVECTOR dir;
    s32 distance;

    local.vx = pos->vx + step->vx - 0x3F80;
    local.vz = pos->vz + step->vz - 0x3F80;
    func_8004A414(&local, &square);
    if (radius < SquareRoot0(square.vx + square.vz)) {
        VectorNormalS(&local, &dir);
        rim.m[2][1] = 0;
        rim.m[1][2] = 0;
        rim.m[1][0] = 0;
        rim.m[0][1] = 0;
        rim.m[1][1] = 0x1000;
        rim.m[2][2] = dir.vz;
        rim.m[0][0] = dir.vz;
        rim.m[0][2] = -dir.vx;
        rim.m[2][0] = dir.vx;
        ApplyMatrixLV(&rim, step, &local);
        func_8004A8EC(&rim, &back);
        SetRotMatrix(&back);
        local.vz = 0;
        for (;;) {
            func_8004998C(&local, step);
            next.vx = pos->vx + step->vx - 0x3F80;
            next.vz = pos->vz + step->vz - 0x3F80;
            func_8004A414(&next, &square);
            distance = SquareRoot0(square.vx + square.vz);
            if (radius >= distance) {
                break;
            }
            local.vz -= distance - radius - 8;
        }
    }
}

/* Apply the current stage's colours: sky gradient (top and bottom), back
 * and far (fog) colours, fade tiles and the GTE primitive colour. */
void func_80082A70(void) {
    Environment *env;
    s32 top_r;
    s32 top_g;
    s32 top_b;
    s32 bottom_r;
    s32 bottom_g;
    s32 bottom_b;

    env = &D_8009178C[D_800928B4];
    D_8009288C = env;
    top_r = env->top[0];
    top_g = env->top[1];
    top_b = env->top[2];
    D_8009291C = env->unk4;
    D_80092910 = env->unk5;
    D_80092908 = env->unk6;
    bottom_r = env->bottom[0];
    bottom_g = env->bottom[1];
    bottom_b = env->bottom[2];
    func_8002C6E0(env->back[0], env->back[1], env->back[2]);
    func_8004A10C(bottom_r, bottom_g, bottom_b);
    D_80095580[0].r0 = top_r;
    D_80095580[1].r0 = top_r;
    D_80095580[0].g0 = top_g;
    D_80095580[1].g0 = top_g;
    D_80095580[0].b0 = top_b;
    D_80095580[1].b0 = top_b;
    *(u16 *)&D_80095580[0].r1 = top_r | (top_g << 8);
    D_80095580[0].b1 = top_b;
    *(u16 *)&D_80095580[1].r1 = top_r | (top_g << 8);
    D_80095580[1].b1 = top_b;
    *(u16 *)&D_80095580[0].r2 = bottom_r | (bottom_g << 8);
    D_80095580[0].b2 = bottom_b;
    *(u16 *)&D_80095580[1].r2 = bottom_r | (bottom_g << 8);
    D_80095580[1].b2 = bottom_b;
    *(u16 *)&D_80095580[0].r3 = bottom_r | (bottom_g << 8);
    D_80095580[0].b3 = bottom_b;
    *(u16 *)&D_80095580[1].r3 = bottom_r | (bottom_g << 8);
    D_80095580[1].b3 = bottom_b;
    D_8009A0D8[0].background.r0 = bottom_r;
    D_8009A0D8[0].background.g0 = bottom_g;
    D_8009A0D8[0].background.b0 = bottom_b;
    D_8009A0D8[1].background.r0 = bottom_r;
    D_8009A0D8[1].background.g0 = bottom_g;
    D_8009A0D8[1].background.b0 = bottom_b;
    SetFogNearFar(0x800, 0x1800, 0xC0);
    D_80059598 = (D_80059598 & 0xFFFFFF) | 0x28000000;
    gte_ldrgb(&D_80059598);
}

/* Load the stage's floor texture (a TIM, palette made semi-transparent)
 * and build the two pools of 64 textured floor quads, alternating the two
 * halves of the texture. */
void func_80082C4C(MenuImages *files) {
    TIM_IMAGE tim;
    POLY_FT4 *quad;
    s16 *clut;
    s32 i;

    OpenTIM(files->floor);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    for (i = 0; i < 0x100; i++) {
        *clut++ |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_800927A0 = GetClut(tim.crect->x, tim.crect->y);
    D_800927A4 = GetTPage(1, 0, tim.prect->x, tim.prect->y);
    D_800927A8 = (u8)tim.prect->y;
    D_80092788[0] = func_80031BDC(0xA00, 0);
    D_80092788[1] = func_80031BDC(0xA00, 0);
    quad = D_80092788[0];
    for (i = 0; i < 0x40; i += 2) {
        setlen(&quad[0], 9);
        quad[0].code = 0x2C;
        setlen(&quad[1], 9);
        quad[1].code = 0x2C;
        quad->clut = D_800927A0;
        quad->tpage = D_800927A4;
        quad->u0 = 0x7F;
        quad->v0 = D_800927A8 + 0x3F;
        quad->u1 = 0x7F;
        quad->v1 = D_800927A8;
        quad->u2 = 0x3F;
        quad->v2 = D_800927A8 + 0x3F;
        quad->u3 = 0x3F;
        quad->v3 = D_800927A8;
        quad++;
        quad->clut = D_800927A0;
        quad->tpage = D_800927A4;
        quad->u0 = 0x3F;
        quad->v0 = D_800927A8 + 0x3F;
        quad->u1 = 0x3F;
        quad->v1 = D_800927A8;
        quad->u2 = 0;
        quad->v2 = D_800927A8 + 0x3F;
        quad->u3 = 0;
        quad->v3 = D_800927A8;
        quad++;
    }
    func_800732AC(D_80092788[1], D_80092788[0], 0xA00);
}

/* Draw the arena wall: a ring of 32 two-storey textured segments around
 * the scene centre, starting behind the given position, depth-cued and
 * skipped when too far away. The wall's corners are taken relative to the
 * camera as 16-bit offsets. */
void func_80082E60(u32 *ot, VECTOR *pos) {
    VECTOR centre;
    SVECTOR base0;
    SVECTOR base1;
    SVECTOR mid0;
    SVECTOR mid1;
    SVECTOR top0;
    SVECTOR top1;
    s32 z[4];
    POLY_FT4 *quad;
    POLY_FT4 *next;
    s32 angle;
    s32 depth;
    s32 i;

    centre = *pos;
    i = 0;
    quad = D_80092788[D_800928A0];
    centre.vx -= 0x3F80;
    centre.vz -= 0x3F80;
    angle = ratan2(centre.vx, centre.vz) & 0xFFF0;
    angle -= 0x100;
    mid0.vy = mid1.vy = -0x290;
    base0.vy = base1.vy = 0;
    top0.vy = top1.vy = -0x520;
    base0.vx = ((func_8003F8B0(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vx - 0x3F80);
    base0.vz = ((func_8003F8CC(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vz - 0x3F80);
    angle += 0x10;
    for (; i < 32; i++) {
        top0.vx = mid0.vx = base0.vx;
        top0.vz = mid0.vz = base0.vz;
        top1.vx = mid1.vx = base1.vx = ((func_8003F8B0(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vx - 0x3F80);
        top1.vz = mid1.vz = base1.vz = ((func_8003F8CC(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vz - 0x3F80);
        gte_ldv3(&base0, &base1, &mid0);
        gte_rtpt();
        gte_dpcs();
        gte_stsxy3(&quad[0].x0, &quad[0].x1, &quad[0].x2);
        gte_stsz3(&z[0], &z[1], &z[2]);
        gte_ldv3(&mid1, &top0, &top1);
        gte_rtpt();
        next = &quad[1];
        depth = z[0];
        if (depth < z[1]) {
            depth = z[1];
        }
        if (depth <= z[2]) {
            depth = z[2];
        }
        *(u32 *)&next->x0 = *(u32 *)&quad[0].x2;
        gte_stsxy(&quad[0].x3);
        gte_stsxy3(&quad[0].x3, &next->x2, &next->x3);
        gte_stsz(&z[3]);
        *(u32 *)&next->x1 = *(u32 *)&quad[0].x3;
        if (depth <= z[3]) {
            depth = z[3];
        }
        if (depth < 0x1C00) {
            depth >>= 4;
            gte_strgb(&quad[0].r0);
            gte_strgb(&next->r0);
            setlen(&quad[0], 9);
            quad[0].code = 0x2C;
            setlen(next, 9);
            next->code = 0x2C;
            AddPrim(&ot[depth], &quad[0]);
            AddPrim(&ot[depth], next);
        }
        quad += 2;
        angle += 0x10;
        base0.vx = base1.vx;
        base0.vz = base1.vz;
    }
}

/* Put the look-at point somewhere random around the scene centre and set
 * the idle camera motion parameters. */
void func_800831C8(void) {
    s32 radius;
    s32 angle;

    radius = (rand() & 0x1FFF) + 0x800;
    angle = rand() % 0x600 + 0x500;
    D_8009871C.vx = ((func_8003F8B0(angle) * radius) >> 12) + 0x4000;
    D_8009871C.vz = ((func_8003F8CC(angle) * radius) >> 12) + 0x4000;
    D_8009871C.vy = -((rand() & 0x7FF) + 0x400);
    D_80092770 = 0x100;
    D_80092774 = 0x40;
    D_8009287C = 0x40;
    D_8009290C = 0x400;
}

/* Turn the idle camera with the left/right buttons. */
void func_800832C0(s32 buttons) {
    if (buttons & 0x8000) {
        D_800927AC += 0x20;
    }
    if (buttons & 0x2000) {
        D_800927AC -= 0x20;
    }
}

/* Idle orbit camera: move the eye toward a point between the two actors
 * (further toward the other actor late in the orbit, a third of the way
 * when smoothing) and swing the look-at point around it, kept inside the
 * arena and above the ground. */
void func_80083310(s32 smooth) {
    VECTOR look;
    VECTOR step;
    VECTOR offset;
    VECTOR unused;   /* the original frame has 0x18 unused bytes */
    SVECTOR unused2;
    Actor *subject;
    Actor *other;
    s32 value; /* the other actor's share, then the orbit angle, then the ground */

    if (D_80092890 != 0) {
        subject = &D_80097010;
        other = &D_8009872C;
    } else {
        subject = &D_8009872C;
        other = &D_80097010;
    }
    ratan2(subject->pos.vx - other->pos.vx, subject->pos.vz - other->pos.vz);
    if (D_800928AC > 0xB0) {
        value = 0x100;
    } else if (D_800928AC > 0xA0) {
        value = (D_800928AC - 0xA0) << 4;
    } else {
        value = 0;
    }
    offset.vx = other->pos.vx;
    offset.vy = other->pos.vy;
    offset.vz = other->pos.vz;
    offset.vx -= subject->pos.vx;
    offset.vy -= subject->pos.vy;
    offset.vz -= subject->pos.vz;
    offset.vx *= value;
    offset.vy *= value;
    offset.vz *= value;
    offset.vx /= 256;
    offset.vy /= 256;
    offset.vz /= 256;
    offset.vx += subject->pos.vx;
    offset.vy += subject->pos.vy;
    offset.vz += subject->pos.vz;
    offset.vy -= 0xA0;
    offset.vx -= D_8009867C.vx;
    offset.vy -= D_8009867C.vy;
    offset.vz -= D_8009867C.vz;
    if (smooth) {
        offset.vx /= 3;
        offset.vy /= 3;
        offset.vz /= 3;
    }
    D_80092770 = 0xC00;
    D_8009867C.vx += offset.vx;
    D_8009867C.vy += offset.vy;
    D_8009867C.vz += offset.vz;
    value = D_800927AC + D_800928AC * D_800927B0;
    look.vx = (func_8003F8B0(value) * D_80092770) >> 12;
    look.vz = (func_8003F8CC(value) * D_80092770) >> 12;
    look.vy = -(D_800928AC * 6 + 0x200);
    look.vx += D_8009867C.vx;
    look.vy += D_8009867C.vy;
    look.vz += D_8009867C.vz;
    step.vx = look.vx - D_8009871C.vx;
    step.vz = look.vz - D_8009871C.vz;
    func_800828F8(&D_8009871C, &step, 0x3D00);
    D_8009871C.vx += step.vx;
    D_8009871C.vz += step.vz;
    value = func_80082488(&D_8009871C, 0);
    if (value < look.vy) {
        look.vy = value;
    }
    D_8009871C.vy = look.vy;
}

/* Start an idle camera orbit at a random angle, speed and direction. */
void func_8008369C(void) {
    D_800927AC = rand();
    D_800927B0 = rand() % 12 + 4;
    if (rand() & 1) {
        D_800927B0 = -D_800927B0;
    }
    func_80083310(0);
}

/* Frame two actors: put the eye between them, pick the side of the pair
 * the look-at point is nearer to, and move the look-at point toward a spot
 * beside the pair (further back when they are far apart), kept inside the
 * arena and above the ground. */
void func_80083738(Actor *first, Actor *second) {
    VECTOR side;
    VECTOR other_side;
    VECTOR unused[2]; /* the original frame has 0x20 unused bytes */
    s32 heading;
    s32 distance;
    s32 angle;
    s32 value; /* the second angle, then a side's distance, then the ground */

    heading = ratan2(first->pos.vx - second->pos.vx, first->pos.vz - second->pos.vz);
    distance = func_800887A4(&first->pos, &second->pos);
    angle = heading - 0x400;
    D_80092770 = distance * 2 / 3 + 0xC0;
    D_8009867C.vx = (first->pos.vx + second->pos.vx) / 2;
    D_8009867C.vy = (first->pos.vy + second->pos.vy) / 2 - 0xA0;
    D_8009867C.vz = (first->pos.vz + second->pos.vz) / 2;
    side.vx = D_8009867C.vx + ((func_8003F8B0(angle) * D_80092770) >> 12);
    side.vz = D_8009867C.vz + ((func_8003F8CC(angle) * D_80092770) >> 12);
    value = heading + 0x400;
    other_side.vx = D_8009867C.vx + ((func_8003F8B0(value) * D_80092770) >> 12);
    other_side.vz = D_8009867C.vz + ((func_8003F8CC(value) * D_80092770) >> 12);
    side.vx -= D_8009871C.vx;
    side.vy -= D_8009871C.vy;
    side.vz -= D_8009871C.vz;
    other_side.vx -= D_8009871C.vx;
    other_side.vy -= D_8009871C.vy;
    other_side.vz -= D_8009871C.vz;
    value = func_80088754(&side);
    if (func_80088754(&other_side) < value) {
        D_8009290C = 0x400;
        D_800928F4 = 0;
    } else {
        D_8009290C = -0x400;
        D_800928F4 = 1;
    }
    distance /= 4;
    if (distance > 0x300) {
        distance = 0x300;
    }
    side.vy = D_8009867C.vy - D_80092774 - distance;
    side.vx = D_8009867C.vx + ((func_8003F8B0(heading + D_8009290C) * D_80092770) >> 12);
    side.vz = D_8009867C.vz + ((func_8003F8CC(heading + D_8009290C) * D_80092770) >> 12);
    value = func_80082488(&side, 0) - 0x100;
    if (value < side.vy) {
        side.vy = value;
    }
    side.vx = (side.vx - D_8009871C.vx) / D_8009287C;
    side.vy = (side.vy - D_8009871C.vy) / D_8009287C;
    side.vz = (side.vz - D_8009871C.vz) / D_8009287C;
    D_8009277C = heading;
    func_800828F8(&D_8009871C, &side, 0x3D00);
    D_8009287C = 100;
    D_8009871C.vx += side.vx;
    D_8009871C.vy += side.vy;
    D_8009871C.vz += side.vz;
}

/* Read the camera's look-at point and eye. */
void func_80083B54(VECTOR *look, VECTOR *eye) {
    *look = D_8009871C;
    *eye = D_8009867C;
}

/* Clear the display area (one or both 320-wide buffers) and wait. */
void func_80083BB4(s32 both) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    if (both) {
        rect.w = 0x280;
    } else {
        rect.w = 0x140;
    }
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Enter a camera/scene mode, running its setup. */
void func_80083C0C(s32 mode) {
    D_80092794 = mode;
    switch (mode) {
    case 3:
        func_80081E6C();
        break;
    case 4:
        func_8007A21C(D_8009294C);
        break;
    case 8:
        func_8007AC3C();
        break;
    case 6:
        if (D_8009872C.unkF2 < D_80097010.unkF2) {
            func_800725B0(&D_80097010);
        } else {
            func_800725B0(&D_8009872C);
        }
        break;
    }
}

/* The scene state word. */
s32 func_80083CD8(void) {
    return D_80092790;
}

/* Draw the elapsed time (frames at 30 per second) as minutes, seconds and
 * hundredths. */
void func_80083CE8(void) {
    char text[32];
    s32 minutes;
    s32 seconds;

    seconds = D_80092944 % 1800;
    minutes = D_80092944 / 1800;
    sprintf(text, "%02d'%02d''%02d", minutes, seconds / 30, D_80092944 % 30 * 99 / 30);
    func_8007EBE0((s32)text);
}

/* Update an actor's glow light (fading it) at its position relative to its
 * opponent, and the spot light at its position relative to the camera. */
void func_80083DCC(LightRig *rig, Actor *actor, s32 index) {
    MATRIX unused; /* unused in the original; reserves 32 bytes */
    Node *light = rig->lights[index];
    u8 glow = actor->glow;
    s32 level = glow;

    if (level != 0) {
        actor->glow = glow - 0x18;
        if (level < actor->glow) {
            actor->glow = 0;
        }
    }
    if (actor->unkD4 & 0x20) {
        NODE_LIGHT(light)->colour[0] = actor->opponent->colour.r * level / 16;
        NODE_LIGHT(light)->colour[1] = actor->opponent->colour.g * level / 16;
        NODE_LIGHT(light)->colour[2] = actor->opponent->colour.b * level / 16;
    } else {
        NODE_LIGHT(light)->colour[0] = level << 4;
        NODE_LIGHT(light)->colour[1] = level << 3;
        NODE_LIGHT(light)->colour[2] = 0;
    }
    if (D_8009288C->dim) {
        NODE_LIGHT(light)->colour[0] /= 2;
        NODE_LIGHT(light)->colour[1] /= 2;
        NODE_LIGHT(light)->colour[2] /= 2;
    }
    NODE_LIGHT(light)->direction[0] = actor->pos.vx;
    NODE_LIGHT(light)->direction[1] = actor->pos.vy;
    NODE_LIGHT(light)->direction[2] = actor->pos.vz;
    NODE_LIGHT(light)->direction[0] -= actor->opponent->pos.vx;
    NODE_LIGHT(light)->direction[1] -= actor->opponent->pos.vy;
    NODE_LIGHT(light)->direction[2] -= actor->opponent->pos.vz;
    func_80030A30(index, NODE_LIGHT(light));
    light = rig->lights[2];
    NODE_LIGHT(light)->colour[0] = NODE_LIGHT(light)->colour[1] = NODE_LIGHT(light)->colour[2] = 0;
    NODE_LIGHT(light)->direction[0] = actor->pos.vx;
    NODE_LIGHT(light)->direction[1] = actor->pos.vy;
    NODE_LIGHT(light)->direction[2] = actor->pos.vz;
    NODE_LIGHT(light)->direction[0] -= D_8009871C.vx;
    NODE_LIGHT(light)->direction[1] -= D_8009871C.vy;
    NODE_LIGHT(light)->direction[2] -= D_8009871C.vz;
    func_80030A30(2, NODE_LIGHT(light));
}

/* Draw the 3D arena: aim the camera, pose the actors, then draw the floor,
 * the actors and their shadows, the look-at marker and the sky. */
s32 func_800840CC(LightRig *rig) {
    MATRIX floor;
    MATRIX camera;
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    OtPair *layer = rig->layer;
    Light *light;

    func_8008AC0C(layer);
    func_80089A98(rig, &D_8009871C, &D_8009867C);
    if (D_80092790 != 4 && D_800928C8 != 4 && D_80092790 != 8) {
        func_80087068(&D_8009872C, &D_80097010);
    } else if (D_800928C8 == 4) {
        func_8007661C(&D_8009872C);
        if (D_800911D4 != 0) {
            func_8007661C(&D_80097010);
        }
    }
    func_8007E574(D_80092938);
    func_80080D20(D_80092938);
    camera = rig->camera->view;
    ((Node *)D_8009872C.object)->view = ((Node *)D_80097010.object)->view = camera;
    light = NODE_LIGHT(rig->lights[0]);
    light->colour[0] = light->colour[1] = light->colour[2] = 0x800;
    func_80030A30(0, NODE_LIGHT(rig->lights[0]));
    gte_SetBackColor(D_8009291C, D_80092910, D_80092908);
    func_80083DCC(rig, &D_8009872C, 1);
    func_8008A7E0(D_8009872C.node);
    func_80083DCC(rig, &D_80097010, 1);
    func_8008A7E0(D_80097010.node);
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    func_8008E8B0();
    func_8007CF78(&camera, layer->ot[D_800928A0]);
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    func_8007334C(layer->ot[D_800928A0], &rig->camera->view);
    floor = D_80091C0C;
    floor.t[1] = -D_80096FA8.vy;
    CompMatrix(&camera, &floor, &floor);
    gte_SetRotMatrix(&floor);
    gte_SetTransMatrix(&floor);
    func_80082A70();
    func_80082E60(layer->ot[D_800928A0], &D_8009871C);
    func_80087B74(&D_8009872C, layer->ot[D_800928A0], &floor);
    func_80087B74(&D_80097010, layer->ot[D_800928A0], &floor);
    gte_SetRotMatrix(&floor);
    gte_SetTransMatrix(&floor);
    func_80087650();
    if (D_800928B0 != 0) {
        func_80082300(D_8009871C.vx, D_8009871C.vz,
                      ratan2(D_8009871C.vx - D_8009867C.vx, D_8009871C.vz - D_8009867C.vz));
    } else {
        func_80082178(D_8009871C.vx, D_8009871C.vz,
                      ratan2(D_8009871C.vx - D_8009867C.vx, D_8009871C.vz - D_8009867C.vz));
    }
    func_8008779C(layer->ot[D_800928A0], D_8009867C.vx, D_8009867C.vz);
    func_8008AE1C(layer);
    func_80086E24();
    func_80031678(D_80092938, &D_80095580[D_800928A0]);
    return 0;
}

/* Draw the 3D scene: aim the camera, give both actors the camera matrix,
 * light and draw them, then the view's layer and the backdrop sprites. */
s32 func_800846A0(LightRig *rig) {
    MATRIX unused0; /* unused in the original; reserves 32 bytes */
    MATRIX camera;
    VECTOR unused1; /* unused in the original; reserves 16 bytes */
    OtPair *layer = rig->layer;
    Light *light;

    func_8008AC0C(layer);
    func_80089A98(rig, &D_8009871C, &D_8009867C);
    func_80080D10();
    camera = rig->camera->view;
    ((Node *)D_8009872C.object)->view = ((Node *)D_80097010.object)->view = camera;
    func_8008A62C();
    light = NODE_LIGHT(rig->lights[0]);
    light->colour[0] = light->colour[1] = light->colour[2] = 0x800;
    func_80030A30(0, NODE_LIGHT(rig->lights[0]));
    gte_SetBackColor(D_8009291C, D_80092910, D_80092908);
    func_80083DCC(rig, &D_8009872C, 1);
    func_8008A7E0(D_8009872C.node);
    func_80083DCC(rig, &D_80097010, 1);
    func_8008A7E0(D_80097010.node);
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    func_8007334C(layer->ot[D_800928A0], &rig->camera->view);
    func_8007D068(layer->ot[D_800928A0]);
    func_8008AE1C(layer);
    func_80086E24();
    AddPrim(D_80092938, &D_800955F8[4 + D_800928A0]);
    AddPrim(D_80092938, &D_800955C8[2 + D_800928A0]);
    AddPrim(D_80092938, &D_800955F8[2 + D_800928A0]);
    AddPrim(D_80092938, &D_800955F8[D_800928A0]);
    AddPrim(D_80092938, &D_800955C8[D_800928A0]);
    return 0;
}

/* Draw a 3D view: update its layer, link this buffer's ordering table, finish. */
s32 func_800849E0(LightRig *rig) {
    OtPair *layer = rig->layer;

    func_8008AC0C(layer);
    func_80080D20(&layer->ot[D_800928A0][1]);
    func_8008AE1C(layer);
    func_80086E24();
    return 0;
}

/* Update a 3D view's layer without drawing it. */
void func_80084A40(LightRig *rig) {
    func_8008AC0C(rig->layer);
}

/* Draw a 3D view with its shading packet at brightness 0xC0. */
s32 func_80084A64(LightRig *rig) {
    OtPair *layer = rig->layer;

    func_8008E3CC(&layer->ot[D_800928A0][2], 0xC0, 0);
    func_80080D20(&layer->ot[D_800928A0][1]);
    func_8008AE1C(layer);
    func_80086E24();
    return 0;
}

/* Draw the fading overlay while a fade is running. */
void func_80084AE0(void) {
    if (D_80092780 != 0) {
        func_8008E120();
        func_8007F258(D_80092938, 0);
        func_80080D20(D_80092938);
        func_8008E3CC(D_80092938, 0xC0, 1);
        func_8008BC04();
    }
}

/* Step the overlay fade down by 4; when it ends, reset it. */
void func_80084B48(void) {
    if (D_80092780 != 0) {
        D_80092780 -= 4;
        if (D_80092780 <= 0) {
            D_80092780 = 0;
            D_80092784 = 0;
            func_8003A838(D_80092948, 0x100, 0);
            D_8009292C = 0x100;
            func_8008E064();
        } else {
            func_8008E3CC(D_80092938, D_80092780, 1);
        }
    } else {
        D_80092784 = 0;
    }
}

/* Attach an extra object (model D_80091FB0) to the actor's model, turned
 * by (0, 0xC00, 0x400). */
void func_80084BEC(Actor *actor) {
    Node *parent = ((ModelSet *)actor->node->data)->nodes[12];
    Node *object = func_80089C54();
    NodeModel *part = func_80089FC4();

    func_80089E2C(object, part);
    func_8008A184(part, &D_80091FB0);
    func_80089C88(parent, object);
    object->angles.vy = 0xC00;
    object->angles.vx = 0;
    object->angles.vz = 0x400;
}

/* Set up an actor from its loaded model file on one side of the scene:
 * opponent link, model object, kind flags from the model id, part counts,
 * and the palette/emblem images in VRAM (mirrored for side 0). */
void func_80084C88(Actor *actor, ModelFile *data, s32 side) {
    RECT rect;
    void *block; /* the model object, later the mirrored emblem */
    SceneHeader *header;
    u8 *source;
    s32 i;
    s32 j;
    u8 *out;
    s32 row;

    actor->flags = (actor->flags & ~0x08000000) | ((side & 1) << 27);
    if (side) {
        func_8008A140(0x380, 0, 0, 0x1FE);
        actor->opponent = &D_8009872C;
    } else {
        func_8008A140(0x3C0, 0, 0, 0x1FF);
        actor->opponent = &D_80097010;
    }
    func_8008AF6C(data);
    block = func_8008B38C(data);
    actor->object = func_80089C54();
    func_80089C88(actor->object, block);
    actor->node = (Node *)block;
    actor->moves = &D_80092874[actor->model_id];
    actor->kind = 0;
    switch (actor->model_id) {
    case 36:
    case 37:
        actor->kind |= 1;
    case 38:
        actor->kind |= 2;
        break;
    case 3:
    case 14:
    case 27:
    case 34:
    case 35:
    case 39:
    case 41:
    case 42:
        actor->kind |= 4;
        break;
    case 13:
        func_8008A168();
        func_80084BEC(actor);
        break;
    }
    func_8008E6F8(actor);
    header = data->header;
    actor->header = header;
    actor->unk7C = data->unk14;
    actor->move_slots = data->slots;
    actor->unk900 = (u8 *)header + 0x34;
    actor->visible = (u8 *)(header->unk30 + (s32)header);
    actor->visible_count = header->unkE;
    actor->colour.r = header->unk10[0];
    actor->colour.g = header->unk10[1];
    actor->colour.b = header->unk10[2];
    actor->move_count = 0;
    actor->parts_b = 0;
    for (i = 0; i < 14; i++) {
        if (actor->moves->learned[i]) {
            if (actor->move_slots[i].usable) {
                actor->move_count++;
            } else {
                actor->parts_b++;
            }
        }
    }
    row = 0x100;
    rect.x = 0;
    rect.y = side + 0x1F6;
    rect.w = row;
    rect.h = 1;
    LoadImage(&rect, (u_long *)data->image);
    rect.x = side * 16 + 0x380;
    rect.y = row;
    rect.w = 0xB;
    rect.h = 0x16;
    if (side) {
        LoadImage(&rect, (u_long *)(data->image + 0x200));
    } else {
        source = data->image + 0x200;
        block = func_80031BDC(0x1E4, 0);
        out = block;
        for (i = 0; i < 0x16; i++) {
            for (j = 0; j < 0x16; j++) {
                *out++ = source[0x15 - j];
            }
            source += 0x16;
        }
        LoadImage(&rect, block);
        func_80032C18(block, 1);
    }
    rect.x = 0x3A0;
    rect.y = side * 8 + 0x100;
    rect.w = 0x10;
    rect.h = 8;
    LoadImage(&rect, (u_long *)(data->image + 0x3E4));
}

/* The vertical-blank hook, run by the resident handler 8003634c: while the
 * overlay fade runs, step the glow field (8008e120, which calls rand) on odd
 * blank counts. */
void func_80084FD0(void) {
    if (D_80092784 != 0 && (D_80059488 & 1)) {
        func_8008E120();
    }
}

/* Leave the menu screen for scene mode 5. */
void func_80085014(void) {
    D_80092920 &= ~1;
    func_80083BB4(0);
    func_80083C0C(5);
    D_80099D98.driven = 1;
    D_80099D98.com1 = 1;
    D_800928C8 = 6;
}

/* Return from scene mode 5 to the menu screen. */
void func_80085070(void) {
    D_80099D98.driven = 0;
    D_80099D98.com1 = 0;
    D_80092920 |= 1;
}

/* Give actor slot `which` (1 = D_80097010, 0 = D_8009872C) a new model
 * id and load its model, replacing the previous one. */
void func_8008509C(s32 which, s32 id) {
    if (which != 0) {
        D_80097010.model_id = id;
    } else {
        D_8009872C.model_id = id;
    }
    if (D_800927B4[which] != NULL) {
        func_800320E8(D_800927B4[which]);
        D_800927B4[which] = NULL;
    }
    func_80028470(0x30, 1);
    D_800927B4[which] = func_800891C0(id + 2);
    func_80028470(0x30, 0);
}

/* Release actor slot `which`'s model. */
void func_80085134(s32 which) {
    func_80028A60(0);
    if (D_800927B4[which] != NULL) {
        func_800320E8(D_800927B4[which]);
        D_800927B4[which] = NULL;
    }
}

/* Load a resource by its file number. */
void func_8008518C(FileRequest *resource, s32 arg) {
    resource->destination = func_80031BDC(func_800288EC(resource->file), arg);
}

/* Leave the menu mode: stop its sound and streams, wait for drawing and
 * dispatch the next mode. */
void func_800851D4(void) {
    func_8003852C(D_800927C4);
    if (D_800917F0 != 0) {
        func_80039C4C(D_80092948);
        func_800399D4(D_80092948);
    }
    func_80088A40();
    func_8001996C(1);
    DrawSync(0);
    VSync(2);
    D_8005061C = 1;
    func_80019ACC(0);
}

/* Whether the scene is in a state that ends the menu mode. */
s32 func_80085264(void) {
    s32 done = 0;

    if (D_80092794 == 5 || D_80092794 == 7 ||
        (D_80092794 == 1 && D_800928C8 == 4 && D_8005061C == 0)) {
        done = 1;
    }
    return done;
}

/* The menu mode: load its resources, then run the title/options screens
 * (until a choice starts a bout or the demo idles out) and the bouts
 * themselves, one scene mode (D_80092790) per frame. */
void func_800852C4(s32 arg) {
    LightRig *rig;
    void *file;
    void *model;
    s32 sequence;
    s32 step;
    s32 idle;
    s32 hold; /* never initialised: the first held frame counts from garbage */

    D_800928D0 = 0;
    D_80092920 |= 1;
    D_800928DC = func_80031BDC(0x10010, 0);
    rig = func_8008A3E0(func_8008A2B8(0x1000));
    func_8002C59C(&D_80091FB0);
    func_8008518C(&D_800917C0[0], 0);
    func_8008518C(&D_800917C0[1], 0);
    func_8008518C(&D_800917C0[2], 1);
    func_8008518C(&D_800917C0[3], 1);
    func_8008518C(&D_800917C0[4], 1);
    func_80029AFC(D_800917C0, 0, 0);
    sequence = (s32)D_800917C0[0].destination;
    D_800927C4 = (s32)D_800917C0[1].destination;
    func_8008976C(0x140, 0xDA);
    func_80088308();
    func_80030988(1, 1, 0x40, 0x40);
    D_80092784 = 0;
    func_800363F0(func_80084FD0);
    func_80036E4C(0x7FFF, 0x8000);
    func_80033698(0x140, 0xFF);
    func_800814AC();
    func_80079A8C();
    func_8008DF30();
    D_800927B4[1] = NULL;
    D_800927B4[0] = NULL;
    D_80097010.object = NULL;
    D_8009872C.object = NULL;
    func_80080AA0(1);
    func_80028A60(0);
    func_80038428(D_800927C4);
    if (D_800917F0 != 0) {
        D_80092948 = func_80039850(sequence);
        func_80039A80(D_80092948, 0x7F, 0);
    } else {
        D_80092948 = D_80062528;
    }
    func_80032EB4(D_800917C0[3].destination, D_800928DC);
    func_800320E8(D_800917C0[3].destination);
    func_80081ECC();
    file = func_80032E88(D_800917C0[2].destination, 0);
    func_800320E8(D_800917C0[2].destination);
    func_8003342C(file);
    D_80092880 = ((s32 *)file)[1];
    D_80092874 = (MoveList *)((s32 *)file)[2];
    func_8007EEE8(D_8005061C == 1);
    file = func_80032E88(D_800917C0[4].destination, 1);
    func_800320E8(D_800917C0[4].destination);
    func_8003342C(file);
    func_80082C4C(file);
    func_8007B388(file);
    func_8007E634(file);
    func_800878DC(file);
    func_800868E0(file);
    func_80071794(file);
    func_800320E8(file);
    func_8007EFB4();
    func_800718C0();
    func_8008BC04();
    D_8009289C = 0;
restart:
    idle = 0x4650;
    func_80032D60();
    func_80089D5C(D_8009872C.object);
    func_80089D5C(D_80097010.object);
    func_80032D60();
    func_800809D8();
    D_80092780 = 0xFF;
    D_80092784 = 0;
    func_8008DF50();
    func_80079B0C();
    func_800346A4(&D_8009868C);
    D_80092794 = -1;
    D_800928C4 = 0;
    D_800928B4 = 0;
    if (D_8005061C == 1) {
        D_80092898 = 0;
        step = 0;
    } else {
        if (D_8005061C == 2) {
            D_800928C4 = 1;
            D_80092884 = 0;
        }
        D_8005061C = 0;
        D_80099D98.level = D_80050621;
        D_80099D98.option6 = D_80050620;
        func_80080A58();
        D_80099D98.com1 = 0;
        D_80092798 = D_8005061E;
        D_8009279C = D_8005061F;
        switch (D_8005061D) {
        case 0:
            D_80099D98.driven = 1;
            func_80083C0C(1);
            D_800928C8 = 1;
            func_80080644(D_80092798, D_8009279C);
            if (D_8009279C == 5) {
                D_800928B4 = 1;
            }
            break;
        case 1:
            D_80099D98.driven = 1;
            func_80083C0C(1);
            D_800928C8 = 4;
            break;
        case 2:
            D_80099D98.driven = 0;
            func_80083C0C(7);
            D_800928C8 = 5;
            func_800719F0();
            break;
        }
        func_8008509C(0, D_80092798);
        step = 1;
        func_8008509C(1, D_8009279C);
        D_80092924 = 0;
        D_80092898 = 2;
    }
    if (func_80085264()) {
        func_80028A60(0);
        step = 10;
    }
    if (step != 10) {
        do {
            if ((D_80059570 & ~1) || (D_80059574 & ~1)) {
                idle = 0x4650;
            }
            if ((D_80059570 & 1) && func_800809BC()) {
                if (++hold == 0x78) {
                    idle = 0;
                }
            } else {
                hold = 0;
            }
            if (idle != 0) {
                idle--;
            } else if (D_80092780 != 0) {
                func_80080570();
                D_80092780 = 0;
                func_80028A60(0);
                func_80085014();
                break;
            }
            func_80084A40(rig);
            switch (step) {
            case 0:
                func_80081D2C();
                if (D_80092924 != 0) {
                    D_80092898 = 2;
                    step = 1;
                }
                if (D_800928E8 & 1) {
                    func_8008E120();
                }
                break;
            case 1:
                func_80036420();
                if (!func_80085264()) {
                    func_8008E120();
                    func_8007F258(D_80092938, 0);
                }
                if (!func_80028A60(1)) {
                    step = 10;
                }
                break;
            }
            func_80084A64(rig);
            func_8008BC04();
        } while (step != 10);
    }
    D_80092898 = 0;
    if (func_80085264()) {
        D_80092780 = 0;
        D_80092784 = 0;
        func_8008E064();
        func_80080A58();
    }
    model = func_80032E88(D_800927B4[0], 0);
    func_800320E8(D_800927B4[0]);
    D_800927B4[0] = model;
    func_80084AE0();
    model = func_80032E88(D_800927B4[1], 0);
    func_800320E8(D_800927B4[1]);
    D_800927B4[1] = model;
    func_80084AE0();
    func_80084C88(&D_8009872C, D_800927B4[0], 0);
    func_80084AE0();
    func_80084C88(&D_80097010, D_800927B4[1], 1);
    func_80084AE0();
    func_8007B270(&D_8009872C.colour, &D_80097010.colour);
    func_80081E00();
    if (D_80092780 != 0) {
        D_80092784 = 1;
        func_8003A838(D_80092948, 0x158, 0);
        D_8009292C = 0x200;
    }
    D_80092890 = 2;
new_bout:
    func_80079B44();
    D_8009872C.pos.vx = 0x3E80;
    D_8009872C.pos.vy = 0;
    D_8009872C.pos.vz = 0x3F80;
    D_80097010.pos.vx = 0x4080;
    D_80097010.pos.vy = 0;
    D_80097010.pos.vz = 0x3F80;
    for (;;) {
        func_80036DC8(0, 0xFF, 0);
        D_80092898 = 2;
        D_800928B0 = 0;
        D_80092790 = D_80092794;
        func_80084B48();
        func_80034888(&D_8009868C, D_80092938, D_800928A0);
        switch (D_80092790) {
        case 1:
            func_80079DF0(&D_8009872C, &D_80097010);
            SetGeomScreen(0xC0);
            if (D_80092794 == 1) {
                func_800796B8(&D_8009872C, &D_80097010);
            }
            func_800840CC(rig);
            break;
        case 6:
            func_80072858(rig);
            break;
        case 7:
            func_80071AD0();
            SetGeomScreen(0xC0);
            func_800840CC(rig);
            if (D_80092794 == 3) {
                D_80092898 = 0;
                func_8008BC04();
                goto leave;
            }
            break;
        case 4:
            SetGeomScreen(0x800);
            func_8007A344(&D_8009872C, &D_80097010);
            if (D_80092794 != 8) {
                func_80083310(1);
                D_800928B0 = 1;
            }
            func_800840CC(rig);
            break;
        case 8:
            SetGeomScreen(0x200);
            func_8007AE10(&D_8009872C, &D_80097010);
            func_800840CC(rig);
            break;
        case 0:
            func_80081D2C();
            func_800849E0(rig);
            D_80092898 = 0;
            break;
        case 5:
            func_80036420();
            if (D_8005948C != 0) {
                D_8005948C = 0;
                D_800594A4 = 0;
                func_80085070();
                D_80092898 = 0;
                func_80081E6C();
                func_8008BC04();
                goto restart;
            }
            func_80079DF0(&D_8009872C, &D_80097010);
            SetGeomScreen(0xC0);
            func_800796B8(&D_8009872C, &D_80097010);
            func_800846A0(rig);
            break;
        case 3:
        leave:
            if (D_8005061C == 0) {
                func_800851D4();
            }
            func_80081E6C();
            goto restart;
        case 2:
            if (D_80099D98.option6 != 0) {
                if (D_8009872C.unkF2 == D_80099D98.option6) {
                    if (D_80099D98.com1) {
                        goto leave;
                    }
                    func_80083C0C(6);
                    break;
                }
                if (D_80097010.unkF2 == D_80099D98.option6) {
                    if (D_80099D98.driven) {
                        goto leave;
                    }
                    func_80083C0C(6);
                    break;
                }
            }
            func_80083C0C(1);
            goto new_bout;
        }
        func_8003708C(0x9E, 0);
        func_80036DC8(0xFF, 0xFF, 0);
        func_8008BC04();
    }
}

/* Screen position of the left-hand gauge for a layout point. */
void func_80085E34(DVECTOR *point, DVECTOR *out) {
    out->vx = point->vx + 0x18;
    out->vy = point->vy + 6;
    out->vx += 0x4F;
}

/* Screen position of the right-hand (mirrored) gauge for a layout point. */
void func_80085E60(DVECTOR *point, DVECTOR *out) {
    out->vx = 0x8B - point->vx;
    out->vy = 0x20 - point->vy;
    out->vx += 0x4F;
}

/* Gauge x for a side (nonzero = mirrored). */
void func_80085E90(s32 mirrored, s16 *out, s32 x) {
    if (mirrored) {
        *out = 0xDA - x;
    } else {
        *out = x + 0x67;
    }
}

/* Gauge y for a side (nonzero = mirrored). */
void func_80085EAC(s32 mirrored, s16 *out, s32 y) {
    if (mirrored) {
        *out = 0x20 - y;
    } else {
        *out = y + 6;
    }
}

/* Build one buffer's overlay packets: texture page modes, the frame
 * outlines and gauge quads of both sides (left from the corner layout,
 * right mirrored), the arrow triangles and the marks. The mirror loop runs
 * over six arrows and so also writes three past the array into the marks,
 * which are set afterwards; the second mirrored arrow then gets its first
 * corner one pixel up and its third one pixel left. Both black marks' heads
 * come first, then the first two marks' coordinates, one shared value at a
 * time (x0 = x2, x1, x3, y0 = y1, y2 = y3). So their shared code word
 * 0x28000000 is dead before any coordinate and takes $v0, while the length 5
 * (all four marks) and the 0x1E (marks[0].x0/x2, marks[1].y2/y3) live across
 * the coordinates and take $a0/$v1; the two decrement loads, register births
 * that sched1 puts late, take $v0/$v1, and sched2 lifts each load to just
 * after the last use of its register. */
void func_80085EC8(OverlayBuffer *buf) {
    s32 i;

    SetDrawTPage(&buf->tpage[0], 0, 1, GetTPage(0, 2, 0, 0));
    SetDrawTPage(&buf->tpage[1], 0, 0, GetTPage(0, 1, 0, 0));
    setlen(&buf->frame[0], 6);
    *(u32 *)&buf->frame[0].r0 = 0x4C000000;
    buf->frame[0].pad = 0x55555555;
    setlen(&buf->frame[1], 6);
    *(u32 *)&buf->frame[1].r0 = 0x4C000000;
    buf->frame[1].pad = 0x55555555;
    setlen(&buf->frame[2], 6);
    *(u32 *)&buf->frame[2].r0 = 0x4C000000;
    buf->frame[2].pad = 0x55555555;
    setlen(&buf->frame[3], 6);
    *(u32 *)&buf->frame[3].r0 = 0x4C000000;
    buf->frame[3].pad = 0x55555555;
    func_80085E34(&D_800917F4[0], (DVECTOR *)&buf->frame[0].x0);
    func_80085E34(&D_800917F4[1], (DVECTOR *)&buf->frame[0].x1);
    func_80085E34(&D_800917F4[2], (DVECTOR *)&buf->frame[0].x2);
    func_80085E34(&D_800917F4[3], (DVECTOR *)&buf->frame[0].x3);
    func_80085E34(&D_800917F4[3], (DVECTOR *)&buf->frame[1].x0);
    func_80085E34(&D_800917F4[4], (DVECTOR *)&buf->frame[1].x1);
    func_80085E34(&D_800917F4[5], (DVECTOR *)&buf->frame[1].x2);
    func_80085E34(&D_800917F4[0], (DVECTOR *)&buf->frame[1].x3);
    func_80085E60(&D_800917F4[0], (DVECTOR *)&buf->frame[2].x0);
    func_80085E60(&D_800917F4[1], (DVECTOR *)&buf->frame[2].x1);
    func_80085E60(&D_800917F4[2], (DVECTOR *)&buf->frame[2].x2);
    func_80085E60(&D_800917F4[3], (DVECTOR *)&buf->frame[2].x3);
    func_80085E60(&D_800917F4[3], (DVECTOR *)&buf->frame[3].x0);
    func_80085E60(&D_800917F4[4], (DVECTOR *)&buf->frame[3].x1);
    func_80085E60(&D_800917F4[5], (DVECTOR *)&buf->frame[3].x2);
    func_80085E60(&D_800917F4[0], (DVECTOR *)&buf->frame[3].x3);
    MargePrim(&buf->frame[0], &buf->frame[1]);
    MargePrim(&buf->frame[2], &buf->frame[3]);
    buf->frame[0].x0 = 0x1D;
    buf->frame[2].x0 = 0x121;
    setlen(&buf->bars[0], 5);
    *(u32 *)&buf->bars[0].r0 = 0x280000FF;
    setlen(&buf->bars[1], 5);
    *(u32 *)&buf->bars[1].r0 = 0x280000FF;
    setlen(&buf->bars[2], 5);
    *(u32 *)&buf->bars[2].r0 = 0x280000FF;
    setlen(&buf->bars[3], 5);
    *(u32 *)&buf->bars[3].r0 = 0x280000FF;
    setlen(&buf->bars[4], 5);
    *(u32 *)&buf->bars[4].r0 = 0x280000FF;
    setlen(&buf->bars[5], 5);
    *(u32 *)&buf->bars[5].r0 = 0x280000FF;
    func_80085E34(&D_800917F4[0], (DVECTOR *)&buf->bars[0].x0);
    func_80085E34(&D_800917F4[7], (DVECTOR *)&buf->bars[0].x1);
    func_80085E34(&D_800917F4[5], (DVECTOR *)&buf->bars[0].x2);
    func_80085E34(&D_800917F4[4], (DVECTOR *)&buf->bars[0].x3);
    func_80085E34(&D_800917F4[7], (DVECTOR *)&buf->bars[1].x0);
    func_80085E34(&D_800917F4[6], (DVECTOR *)&buf->bars[1].x1);
    func_80085E34(&D_800917F4[4], (DVECTOR *)&buf->bars[1].x2);
    func_80085E34(&D_800917F4[3], (DVECTOR *)&buf->bars[1].x3);
    func_80085E34(&D_800917F4[6], (DVECTOR *)&buf->bars[2].x0);
    func_80085E34(&D_800917F4[1], (DVECTOR *)&buf->bars[2].x1);
    func_80085E34(&D_800917F4[3], (DVECTOR *)&buf->bars[2].x2);
    func_80085E34(&D_800917F4[2], (DVECTOR *)&buf->bars[2].x3);
    func_80085E60(&D_800917F4[0], (DVECTOR *)&buf->bars[3].x0);
    func_80085E60(&D_800917F4[7], (DVECTOR *)&buf->bars[3].x1);
    func_80085E60(&D_800917F4[5], (DVECTOR *)&buf->bars[3].x2);
    func_80085E60(&D_800917F4[4], (DVECTOR *)&buf->bars[3].x3);
    func_80085E60(&D_800917F4[7], (DVECTOR *)&buf->bars[4].x0);
    func_80085E60(&D_800917F4[6], (DVECTOR *)&buf->bars[4].x1);
    func_80085E60(&D_800917F4[4], (DVECTOR *)&buf->bars[4].x2);
    func_80085E60(&D_800917F4[3], (DVECTOR *)&buf->bars[4].x3);
    func_80085E60(&D_800917F4[6], (DVECTOR *)&buf->bars[5].x0);
    func_80085E60(&D_800917F4[1], (DVECTOR *)&buf->bars[5].x1);
    func_80085E60(&D_800917F4[3], (DVECTOR *)&buf->bars[5].x2);
    func_80085E60(&D_800917F4[2], (DVECTOR *)&buf->bars[5].x3);
    SetDrawTPage(&buf->bar_tpage, 0, 1, GetTPage(0, 1, 0, 0));
    func_800732AC(buf->bars_dim, buf->bars, sizeof(buf->bars));
    func_800732AC(buf->bars_lit, buf->bars, sizeof(buf->bars));
    for (i = 0; i < 6; i++) {
        setlen(&buf->bars_dim[i], 5);
        *(u32 *)&buf->bars_dim[i].r0 = 0x28806060;
    }
    for (i = 0; i < 6; i++) {
        setlen(&buf->bars_lit[i], 5);
        *(u32 *)&buf->bars_lit[i].r0 = 0x280000FF;
    }
    *(u32 *)&buf->arrows[0][0].x0 = 0x200014;
    *(u32 *)&buf->arrows[0][0].x1 = 0x20001C;
    *(u32 *)&buf->arrows[0][0].x2 = 0x28001C;
    *(u32 *)&buf->arrows[0][1].x0 = 0x200013;
    *(u32 *)&buf->arrows[0][1].x1 = 0x290013;
    *(u32 *)&buf->arrows[0][1].x2 = 0x29001B;
    *(u32 *)&buf->arrows[0][2].x0 = 0x320013;
    *(u32 *)&buf->arrows[0][2].x1 = 0x2A0013;
    *(u32 *)&buf->arrows[0][2].x2 = 0x2A001B;
    for (i = 0; i < 6; i++) {
        setlen(&buf->arrows[0][i], 4);
        *(u32 *)&buf->arrows[0][i].r0 = 0x2000FF00;
        setlen(&buf->arrows[1][i], 4);
        *(u32 *)&buf->arrows[1][i].r0 = 0x2000FF00;
        buf->arrows[1][i].y0 = buf->arrows[0][i].y0;
        buf->arrows[1][i].y1 = buf->arrows[0][i].y1;
        buf->arrows[1][i].y2 = buf->arrows[0][i].y2;
        buf->arrows[1][i].x0 = 0x140 - buf->arrows[0][i].x0;
        buf->arrows[1][i].x1 = 0x140 - buf->arrows[0][i].x1;
        buf->arrows[1][i].x2 = 0x140 - buf->arrows[0][i].x2;
    }
    buf->arrows[1][1].y0--;
    buf->arrows[1][1].x2--;
    setlen(&buf->marks[0], 5);
    *(u32 *)&buf->marks[0].r0 = 0x28000000;
    setlen(&buf->marks[1], 5);
    *(u32 *)&buf->marks[1].r0 = 0x28000000;
    buf->marks[0].x0 = buf->marks[0].x2 = 0x1E;
    buf->marks[0].x1 = 0x63;
    buf->marks[0].x3 = 0x5E;
    buf->marks[0].y0 = buf->marks[0].y1 = 9;
    buf->marks[0].y2 = buf->marks[0].y3 = 0x14;
    buf->marks[1].x0 = buf->marks[1].x2 = 0x122;
    buf->marks[1].x1 = 0xE3;
    buf->marks[1].x3 = 0xDE;
    buf->marks[1].y0 = buf->marks[1].y1 = 0x13;
    buf->marks[1].y2 = buf->marks[1].y3 = 0x1E;
    setlen(&buf->marks[2], 5);
    *(u32 *)&buf->marks[2].r0 = 0x280000FF;
    *(u32 *)&buf->marks[2].x0 = 0x320006;
    *(u32 *)&buf->marks[2].x1 = 0x36000A;
    *(u32 *)&buf->marks[2].x2 = 0x4C0006;
    *(u32 *)&buf->marks[2].x3 = 0x48000A;
    setlen(&buf->marks[3], 5);
    *(u32 *)&buf->marks[3].r0 = 0x280000FF;
    *(u32 *)&buf->marks[3].x0 = 0x32013A;
    *(u32 *)&buf->marks[3].x1 = 0x360136;
    *(u32 *)&buf->marks[3].x2 = 0x4C013A;
    *(u32 *)&buf->marks[3].x3 = 0x480136;
}

/* Build a textured quad (and its second-buffer copy) showing a whole TIM
 * image at (x, y); `depth` is the TIM colour mode (0 = 4-bit, 1 = 8-bit,
 * 2 = 16-bit), which sets how many pixels one VRAM word holds. */
void func_800864B4(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth) {
    s32 scale;
    s32 right;

    switch (depth) {
    case 0:
        scale = 4;
        break;
    case 1:
        scale = 2;
        break;
    case 2:
        scale = 1;
        break;
    }
    setlen(quad, 9);
    quad->code = 0x2D;
    quad->clut = GetClut(tim->crect->x, tim->crect->y);
    quad->tpage = GetTPage(depth, 0, tim->prect->x, tim->prect->y);
    quad->x0 = quad->x2 = x;
    right = x + tim->prect->w * scale;
    if (scale == 2) {
        quad->x3 = right + 1;
    } else {
        quad->x3 = right;
    }
    quad->y0 = quad->y1 = y;
    quad->x1 = quad->x3 = quad->x3; /* the original stores x3 again */
    quad->y2 = quad->y3 = tim->prect->h + y;
    quad->u0 = quad->u2 = tim->prect->x * scale;
    quad->u1 = quad->u3 = (tim->prect->x + tim->prect->w) * scale;
    quad->v0 = quad->v1 = tim->prect->y;
    quad->v2 = quad->v3 = tim->prect->y + tim->prect->h;
    quad[1] = quad[0];
}

/* The same quad mirrored horizontally (texture u runs right to left). */
void func_800866D4(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth) {
    s32 scale;
    s32 right;

    switch (depth) {
    case 0:
        scale = 4;
        break;
    case 1:
        scale = 2;
        break;
    case 2:
        scale = 1;
        break;
    }
    setlen(quad, 9);
    quad->code = 0x2D;
    quad->clut = GetClut(tim->crect->x, tim->crect->y);
    quad->tpage = GetTPage(depth, 0, tim->prect->x, tim->prect->y);
    right = x + tim->prect->w * scale;
    quad->x1 = quad->x3 = x;
    quad->x0 = quad->x2 = right;
    quad->u0 = quad->u2 = tim->prect->x * scale - 1;
    quad->u1 = quad->u3 = (tim->prect->x + tim->prect->w) * scale - 1;
    quad->y0 = quad->y1 = y;
    quad->y2 = quad->y3 = tim->prect->h + y;
    quad->v0 = quad->v1 = tim->prect->y;
    quad->v2 = quad->v3 = tim->prect->y + tim->prect->h;
    quad[1] = quad[0];
}

/* Build the HUD packets: the two name plates (left and mirrored right)
 * from the name TIM, the icon and gauge sprites, the gauge bar quads from
 * the bar TIM, the HUD texture page modes and the gauge palette. One sprite
 * pair pointer walks the icons and then the gauges (the original keeps it
 * in $s2). Each bar quad gets its colour/code word and then its length, and
 * the gauge sprite its length, code, size and texture position. */
void func_800868E0(MenuImages *files) {
    TIM_IMAGE tim;
    RECT rect;
    s16 *clut;
    Hud *hud = &D_80095698;
    SpritePair *pair;
    POLY_FT4 *bar;

    OpenTIM(files->name);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    clut[1] = 0x8000;
    LoadImage(tim.crect, tim.caddr);
    pair = hud->icon;
    LoadImage(tim.prect, tim.paddr);
    func_800864B4(&tim, 6, 7, hud->name_l, 0);
    func_800866D4(&tim, 0x13A - tim.prect->w * 4, 7, hud->name_r, 0);
    SetDrawTPage(&D_80095918[0], 0, 1, GetTPage(1, 0, 0x380, 0x100));
    D_80095918[1] = D_80095918[0];
    SetDrawTPage(&D_80095918[2], 0, 1, GetTPage(0, 0, 0x380, 0x100));
    D_80095918[3] = D_80095918[2];
    setlen(&pair[0].s[0], 4);
    pair[0].s[0].code = 0x65;
    *(u32 *)&pair[0].s[0].x0 = 0x90007;
    *(u16 *)&pair[0].s[0].u0 = 0;
    *(u32 *)&pair[0].s[0].w = 0x160016;
    pair[0].s[0].clut = GetClut(0, 0x1F6);
    D_80095698.icon[2] = D_80095698.icon[1] = pair[0];
    pair = &D_80095698.icon[2];
    pair[0].s[0].x0 = 0x123;
    pair[0].s[0].u0 = 0x20;
    pair[0].s[0].clut = GetClut(0, 0x1F7);
    pair[1] = pair[0];
    OpenTIM(files->bar);
    ReadTIM(&tim);
    bar = D_80095698.bar_l;
    pair = D_80095698.gauge;
    LoadImage(tim.prect, tim.paddr);
    func_800864B4(&tim, 6, 0x20, bar, 0);
    func_800866D4(&tim, 0x13A - tim.prect->w * 4, 0x20, D_80095698.bar_r, 0);
    D_80092860 = D_80095698.bar_l[0].v0;
    D_80092864 = D_80095698.bar_r[0].v0;
    *(u32 *)&D_80095698.bar_l[0].r0 = 0x2C000080;
    setlen(&D_80095698.bar_l[0], 9);
    *(u32 *)&D_80095698.bar_l[1].r0 = 0x2C000080;
    setlen(&D_80095698.bar_l[1], 9);
    *(u32 *)&D_80095698.bar_r[0].r0 = 0x2C000080;
    setlen(&D_80095698.bar_r[0], 9);
    *(u32 *)&D_80095698.bar_r[1].r0 = 0x2C000080;
    setlen(&D_80095698.bar_r[1], 9);
    setlen(&pair[0].s[0], 4);
    pair[0].s[0].code = 0x65;
    *(u32 *)&pair[0].s[0].w = 0x80040;
    *(u16 *)&pair[0].s[0].u0 = 0x80;
    D_80095698.bar_l[1].clut = D_80095698.name_l[0].clut;
    D_80095698.bar_l[0].clut = D_80095698.name_l[0].clut;
    D_80095698.bar_r[1].clut = D_80095698.name_l[0].clut;
    D_80095698.bar_r[0].clut = D_80095698.name_l[0].clut;
    pair[0].s[0].clut = GetClut(0x3A0, 0x110);
    *(u32 *)&pair[0].s[0].x0 = 0xB001E;
    pair[2] = pair[0];
    pair = &D_80095698.gauge[2];
    *(u16 *)&pair[0].s[0].u0 = 0x880;
    *(u32 *)&pair[0].s[0].x0 = 0x1500E3;
    pair[-1] = pair[-2];
    D_80095698.gauge[3] = D_80095698.gauge[2];
    rect.x = 0x3A0;
    rect.y = 0x110;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, (u_long *)D_80091814);
}

/* Link this buffer's overlay packets into the overlay ordering table. */
void func_80086E24(void) {
    AddPrim(D_80092938, &D_8009A2F8[D_800928A0].tpage[1]);
}

/* Link a gauge bar filled to `value`: the first part up to 0x38, a sloped
 * second part up to 0x48, then the third part. */
void func_80086E70(void *ot, GaugeBar *bar, s32 value, s32 mirrored) {
    s32 over;

    if (value >= 0x38) {
        func_80085E90(mirrored, &bar->parts[0].x1, 0x38);
        func_80085E90(mirrored, &bar->parts[0].x3, 0x34);
        AddPrim(ot, &bar->parts[0]);
    } else {
        func_80085E90(mirrored, &bar->parts[0].x1, value);
        func_80085E90(mirrored, &bar->parts[0].x3, value - 4);
        AddPrim(ot, &bar->parts[0]);
        return;
    }
    if (value >= 0x48) {
        func_80085E90(mirrored, &bar->parts[1].x1, 0x48);
        func_80085E90(mirrored, &bar->parts[1].x3, 0x40);
        func_80085EAC(mirrored, &bar->parts[1].y3, 0x10);
        AddPrim(ot, &bar->parts[1]);
    } else {
        func_80085E90(mirrored, &bar->parts[1].x1, value);
        over = value - 0x38;
        func_80085E90(mirrored, &bar->parts[1].x3, value - (over / 4 + 4));
        func_80085EAC(mirrored, &bar->parts[1].y3, over / 2 + 8);
        AddPrim(ot, &bar->parts[1]);
        return;
    }
    func_80085E90(mirrored, &bar->parts[2].x1, value);
    func_80085E90(mirrored, &bar->parts[2].x3, value - 8);
    AddPrim(ot, &bar->parts[2]);
}

/* Colour a marker packet by an actor's state: none (returns 0),
 * yellow when set, red otherwise. */
s32 func_80086FF8(Actor *actor, POLY_F4 *packet) {
    if (func_8008F530(actor, 0)) {
        return 0;
    }
    if (func_8008F530(actor, 1)) {
        setlen(packet, 5);
        *(u32 *)&packet->r0 = 0x2800FFFF;
    } else {
        setlen(packet, 5);
        *(u32 *)&packet->r0 = 0x280000FF;
    }
    return 1;
}

/* Link the HUD and map overlay for this frame: each side's arrows (one per
 * point), state marks, name plates, icons, gauges, the charge bars (flashing
 * when nearly full) and the level bars (tinted by the level). The level
 * tints are written as loop-invariant expressions inside the loops (the
 * original's moved invariants include a copy of the repeated green term). */
void func_80087068(Actor *left, Actor *right) {
    OverlayBuffer *buf;
    POLY_FT4 *bar;
    s32 n;
    s32 level;

    buf = &D_8009A2F8[D_800928A0];
    n = left->unkF2;
    if (n > 0) {
        AddPrim(D_80092938, &buf->arrows[0][0]);
    }
    if (n > 1) {
        AddPrim(D_80092938, &buf->arrows[0][1]);
    }
    if (n > 2) {
        AddPrim(D_80092938, &buf->arrows[0][2]);
    }
    n = right->unkF2;
    if (n > 0) {
        AddPrim(D_80092938, &buf->arrows[1][0]);
    }
    if (n > 1) {
        AddPrim(D_80092938, &buf->arrows[1][1]);
    }
    if (n > 2) {
        AddPrim(D_80092938, &buf->arrows[1][2]);
    }
    if (func_80086FF8(left, &buf->marks[2])) {
        AddPrim(D_80092938, &buf->marks[2]);
    }
    if (func_80086FF8(right, &buf->marks[3])) {
        AddPrim(D_80092938, &buf->marks[3]);
    }
    AddPrim(D_80092938, &D_80095698.name_l[D_800928A0]);
    AddPrim(D_80092938, &D_80095698.name_r[D_800928A0]);
    AddPrim(D_80092938, &D_80095698.icon[D_800928A0]);
    AddPrim(D_80092938, &D_80095698.icon[2 + D_800928A0]);
    AddPrim(D_80092938, &D_80095918[D_800928A0]);
    AddPrim(D_80092938, &D_80095698.gauge[D_800928A0]);
    AddPrim(D_80092938, &D_80095698.gauge[2 + D_800928A0]);
    AddPrim(D_80092938, &D_80095918[2 + D_800928A0]);
    AddPrim(D_80092938, &D_80095698.bar_r[D_800928A0]);
    AddPrim(D_80092938, &D_80095698.bar_l[D_800928A0]);
    AddPrim(D_80092938, &buf->marks[0]);
    AddPrim(D_80092938, &buf->marks[1]);
    AddPrim(D_80092938, &buf->frame[0]);
    AddPrim(D_80092938, &buf->frame[2]);
    func_80086E70(D_80092938, (GaugeBar *)&buf->bars[0], D_8009872C.unkC0, 0);
    func_80086E70(D_80092938, (GaugeBar *)&buf->bars[3], D_80097010.unkC0, 1);
    n = left->charge >> 6;
    bar = &D_80095698.bar_l[D_800928A0];
    if (n > 0x38) {
        bar->r0 = D_80059488 << 3;
    } else {
        bar->r0 = 0xFF;
    }
    bar->y0 = bar->y1 = 0x60 - n;
    bar->v0 = bar->v1 = D_80092860 - (n - 0x40);
    n = right->charge >> 6;
    bar = &D_80095698.bar_r[D_800928A0];
    if (n > 0x38) {
        bar->r0 = D_80059488 << 3;
    } else {
        bar->r0 = 0xFF;
    }
    bar->y0 = bar->y1 = 0x60 - n;
    bar->v0 = bar->v1 = D_80092860 - (n - 0x40);
    if (left->level != 0) {
        level = 0x100 - left->level;
        for (n = 0; n < 3; n++) {
            buf->bars_lit[n].r0 = ((level * 3) >> 3) + ((left->level * 255) >> 8);
            buf->bars_lit[n].g0 = (level * 3) >> 3;
            buf->bars_lit[n].b0 = level >> 1;
        }
        func_80086E70(D_80092938, (GaugeBar *)&buf->bars_lit[0], left->unkC1, 0);
    }
    if (right->level != 0) {
        level = 0x100 - right->level;
        for (n = 3; n < 6; n++) {
            buf->bars_lit[n].r0 = ((level * 3) >> 3) + ((right->level * 255) >> 8);
            buf->bars_lit[n].g0 = (level * 3) >> 3;
            buf->bars_lit[n].b0 = level >> 1;
        }
        func_80086E70(D_80092938, (GaugeBar *)&buf->bars_lit[3], right->unkC1, 1);
    }
    for (n = 0; n < 6; n++) {
        AddPrim(D_80092938, &buf->bars_dim[n]);
    }
    AddPrim(D_80092938, buf);
}

/* Build the overlay packets for buffer 0 and copy them to buffer 1. */
void func_800875EC(void) {
    func_80085EC8(&D_8009A2F8[0]);
    D_8009A2F8[1] = D_8009A2F8[0];
}

/* Empty the map's row spans (left 0xFF, right 0). */
void func_80087650(void) {
    s32 row;

    for (row = 0; row < 0x80; row++) {
        D_800927CC[row] = 0;
        D_800927D0[row] = 0xFF;
    }
}

/* Widen the map's row spans along a line, clamped to each row's limits. */
void func_80087698(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 x;
    s32 step;
    s32 row;
    s32 column;
    s32 swap;

    if (y0 == y1) {
        return;
    }
    if (y1 < y0) {
        swap = x1;
        x1 = x0;
        x0 = swap;
        swap = y1;
        y1 = y0;
        y0 = swap;
    }
    x = x0 << 8;
    step = ((x1 - x0) << 8) / (y1 - y0);
    for (row = y0; row < y1; row++, x += step) {
        if (row < 0) {
            continue;
        }
        if (row >= 0x80) {
            return;
        }
        column = x >> 8;
        if (column < D_800927D0[row]) {
            if (column < D_80091834[row]) {
                column = D_80091834[row];
            }
            D_800927D0[row] = column;
        }
        if (D_800927CC[row] < column) {
            if (D_800918B4[row] + 1 < column) {
                column = D_800918B4[row] + 1;
            }
            D_800927CC[row] = column;
        }
    }
}

/* Draw the map triangles: load the map colour (as a textured-triangle
 * code) into the GTE, copy the 0x30-byte map table into the scratchpad and
 * run the triangle loop. */
void func_8008779C(u32 *ot, s32 originX, s32 originZ) {
    D_80059598 = (D_80059598 & 0xFFFFFF) | 0x24000000;
    gte_ldrgb(&D_80059598);
    func_800732AC((void *)0x1F800120, &D_80091934, sizeof(MapTable));
    func_80072D18(ot, originX, originZ);
}

/* Set up the map row spans in the scratchpad and the two textured
 * triangle packet pools (0x708 triangles each). */
void func_80087830(void) {
    POLY_FT3 *poly;
    s32 i;

    D_800927CC = (u8 *)0x1F800000;
    D_800927D0 = (u8 *)0x1F800080;
    D_80092854[0] = func_80031BDC(0xE100, 0);
    D_80092854[1] = func_80031BDC(0xE100, 0);
    poly = D_80092854[0];
    for (i = 0; i < 0x708; i++) {
        setlen(poly, 7);
        poly->code = 0x24;
        poly++;
    }
    func_800732AC(D_80092854[1], D_80092854[0], 0xE100);
}

/* Load the stage's icon, backdrop and extra TIM images into VRAM, noting
 * the icon and backdrop palettes and texture pages; the backdrop palette's
 * first entry is transparent and the rest semi-transparent. */
void func_800878DC(MenuImages *files) {
    TIM_IMAGE tim;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s16 *clut;
    s32 i;

    for (i = 0; i < 4; i++) {
        OpenTIM(files->icons[i]);
        ReadTIM(&tim);
        D_80091934.icons[i * 2 + 1] = GetClut(tim.crect->x, tim.crect->y);
        D_80091934.icons[i * 2] = GetTPage(1, 1, tim.prect->x, tim.prect->y);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
    OpenTIM(files->backdrop);
    ReadTIM(&tim);
    D_800927D8 = GetClut(tim.crect->x, tim.crect->y);
    D_800927D4 = GetTPage(0, 2, tim.prect->x, tim.prect->y);
    D_800927DC = tim.prect->x * 4;
    D_800927E0 = tim.prect->y;
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    for (i = 0x1C; i < 0x25; i++) {
        OpenTIM(files->extra[i - 0x1C]);
        ReadTIM(&tim);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
}

/* Build an actor's textured backdrop quad (64x64 texels) for both buffers. */
void func_80087AB0(Actor *actor) {
    POLY_FT4 *quad = &actor->backdrop[0];

    *(u32 *)&quad->r0 = 0x2C101010;
    setlen(quad, 9);
    quad->code |= 2;
    quad->clut = D_800927D8;
    quad->tpage = D_800927D4;
    *(u16 *)&quad->u0 = D_800927DC | (D_800927E0 << 8);
    *(u16 *)&quad->u1 = (D_800927DC + 0x3F) | (D_800927E0 << 8);
    *(u16 *)&quad->u2 = D_800927DC | ((D_800927E0 + 0x3F) << 8);
    *(u16 *)&quad->u3 = (D_800927DC + 0x3F) | ((D_800927E0 + 0x3F) << 8);
    actor->backdrop[1] = actor->backdrop[0];
}

/* Draw an actor's ground shadow: a square sized by its height, centred
 * under it and tilted to the ground normal there. */
void func_80087B74(Actor *actor, u32 *ot, MATRIX *view) {
    SVECTOR corners[4];
    VECTOR centre;
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    SVECTOR normal;
    MATRIX m;
    s32 otz;
    s32 z0, z1, z2, z3;
    POLY_FT4 *quad;
    s32 size;
    s32 min;

    if ((actor->flags & 0x60000000) != 0x20000000) {
        size = (actor->home.vy + actor->unk92C.vy) / 32 + 0x90;
        if (size > 0) {
            centre.vx = (actor->home.vx + actor->unk92C.vx) / 2;
            centre.vz = (actor->home.vz + actor->unk92C.vz) / 2;
            centre.vy = 0;
            corners[0].vx = corners[1].vx = corners[0].vz = corners[2].vz = -size;
            corners[3].vx = corners[2].vx = corners[1].vz = corners[3].vz = size;
            corners[0].vy = corners[1].vy = corners[2].vy = corners[3].vy = 0;
            quad = &actor->backdrop[D_800928A0];
            m.t[0] = centre.vx - D_80096FA8.vx;
            m.t[1] = func_80082488(&centre, 0);
            m.t[2] = centre.vz - D_80096FA8.vz;
            func_80082458(&normal);
            m.m[0][0] = 0x1000;
            m.m[0][1] = 0;
            m.m[0][2] = 0;
            m.m[1][0] = normal.vx;
            m.m[1][1] = normal.vy;
            m.m[1][2] = normal.vz;
            m.m[2][0] = 0;
            m.m[2][1] = 0;
            m.m[2][2] = 0x1000;
            CompMatrix(view, &m, &m);
            gte_SetRotMatrix(&m);
            gte_SetTransMatrix(&m);
            gte_ldv3c(corners);
            gte_rtpt();
            gte_nclip();
            gte_stopz(&otz);
            if (otz >= 0) {
                gte_stsz3(&z0, &z1, &z2);
                gte_stsxy3_ft4(quad);
                gte_ldv0(&corners[3]);
                gte_rtps();
                gte_stsz(&z3);
                gte_stsxy(&quad->x3);
                min = z0;
                if (z1 < min) {
                    min = z1;
                }
                if (z2 < min) {
                    min = z2;
                }
                if (z3 < min) {
                    min = z3;
                }
                otz = min >> 4;
                AddPrim(ot + otz, quad);
            }
        }
    }
}

/* Record a position in the path list (up to 31 entries). */
void func_80087E38(VECTOR *pos) {
    s32 count = D_800928F8;
    s16 *base;
    s16 *at;

    if (count < 0x1F) {
        base = &D_8009A928[0].x;
        at = base + count * (sizeof(PathMarker) / sizeof(s16));
        at[0] = pos->vx;
        at[1] = pos->vy;
        D_800928F8 = count + 1;
        at[2] = pos->vz;
    }
}

/* Draw the recorded path points as axis crosses (64 units long), then
 * clear the list. */
void func_80087EA0(u32 *ot) {
    SVECTOR ends[6];
    PathMarker *point;
    s32 i;
    s32 j;

    for (i = 0; i < D_800928F8; i++) {
        point = &D_8009A928[i];
        ends[0].vx = point->x - 0x20;
        ends[0].vy = point->y;
        ends[0].vz = point->z;
        ends[1].vx = point->x + 0x20;
        ends[1].vy = point->y;
        ends[1].vz = point->z;
        ends[2].vx = point->x;
        ends[2].vy = point->y - 0x20;
        ends[2].vz = point->z;
        ends[3].vx = point->x;
        ends[3].vy = point->y + 0x20;
        ends[3].vz = point->z;
        ends[4].vx = point->x;
        ends[4].vy = point->y;
        ends[4].vz = point->z - 0x20;
        ends[5].vx = point->x;
        ends[5].vy = point->y;
        ends[5].vz = point->z + 0x20;
        for (j = 0; j < 6; j++) {
            ends[j].vx -= D_80096FA8.vx;
            ends[j].vy -= D_80096FA8.vy;
            ends[j].vz -= D_80096FA8.vz;
        }
        gte_ldv3(&ends[0], &ends[1], &ends[2]);
        gte_rtpt();
        gte_stsxy3(&point->axes[D_800928A0][0].x0, &point->axes[D_800928A0][0].x1,
                   &point->axes[D_800928A0][1].x0);
        gte_ldv3(&ends[3], &ends[4], &ends[5]);
        gte_rtpt();
        gte_stsxy3(&point->axes[D_800928A0][1].x1, &point->axes[D_800928A0][2].x0,
                   &point->axes[D_800928A0][2].x1);
        setlen(&point->axes[D_800928A0][0], 3);
        *(u32 *)&point->axes[D_800928A0][0].r0 = 0x400000FF;
        setlen(&point->axes[D_800928A0][1], 3);
        *(u32 *)&point->axes[D_800928A0][1].r0 = 0x4000FF00;
        setlen(&point->axes[D_800928A0][2], 3);
        *(u32 *)&point->axes[D_800928A0][2].r0 = 0x40FF0000;
        func_800316C0(ot, &point->axes[D_800928A0][0]);
        func_800316C0(ot, &point->axes[D_800928A0][1]);
        func_800316C0(ot, &point->axes[D_800928A0][2]);
    }
    D_800928F8 = 0;
}

/* Start a debug line between two points in one of eight colours (bit 0
 * blue, bit 1 red, bit 2 green). Returns the line, or NULL when all 100
 * are in use. */
Line3D *func_8008820C(VECTOR *from, VECTOR *to, s32 colour) {
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        line = &D_80095938[i];
        if (line->timer == 0) {
            line->timer = 1;
            line->from.vx = from->vx;
            line->from.vy = from->vy;
            line->from.vz = from->vz;
            line->to.vx = to->vx;
            line->to.vy = to->vy;
            line->to.vz = to->vz;
            line->packets[0].r0 = (colour & 2) * 0x7F;
            line->packets[0].g0 = (colour & 4) * 0x3F;
            line->packets[0].b0 = (colour & 1) * 0xFF;
            line->packets[1].r0 = (colour & 2) * 0x7F;
            line->packets[1].g0 = (colour & 4) * 0x3F;
            line->packets[1].b0 = (colour & 1) * 0xFF;
            return line;
        }
    }
    return NULL;
}

/* Start a debug line that stays for the given number of frames. */
void func_800882D4(VECTOR *from, VECTOR *to, s32 colour, s32 frames) {
    Line3D *line = func_8008820C(from, to, colour);

    if (line != NULL) {
        line->timer = frames;
    }
}

/* Stop every debug line. */
void func_80088308(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        D_80095938[i].timer = 0;
    }
}

/* Project and link every live debug line, counting its frames down. */
void func_8008832C(void *ot) {
    SVECTOR ends[2];
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_80095938[i].timer != 0) {
            line = &D_80095938[i];
            line->timer--;
            ends[0] = line->from;
            ends[1] = line->to;
            ends[0].vx -= D_80096FA8.vx;
            ends[0].vy -= D_80096FA8.vy;
            ends[0].vz -= D_80096FA8.vz;
            ends[1].vx -= D_80096FA8.vx;
            ends[1].vy -= D_80096FA8.vy;
            ends[1].vz -= D_80096FA8.vz;
            gte_ldv01(&ends[0], &ends[1]);
            gte_rtpt();
            gte_stsxy01(&line->packets[D_800928A0].x0, &line->packets[D_800928A0].x1);
            setlen(&line->packets[D_800928A0], 3);
            setcode(&line->packets[D_800928A0], 0x40);
            func_800316C0(ot, &line->packets[D_800928A0]);
        }
    }
}

/* Scale a vector down by the square root of its (absolute) length measure
 * and pass it on. */
void func_800884E0(VECTOR *vector, void *out) {
    VECTOR scaled = *vector;
    s32 square;
    s32 length;

    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormal(&scaled, out);
}

/* Scale a vector down by the square root of its (absolute) length measure
 * and pass it to VectorNormalS. */
void func_8008859C(VECTOR *vector, void *out) {
    VECTOR scaled = *vector;
    s32 square;
    s32 length;

    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormalS(&scaled, out);
}

/* The same for a short vector. */
void func_80088658(SVECTOR *vector, void *out) {
    VECTOR scaled;
    s32 square;
    s32 length;

    scaled.vx = vector->vx;
    scaled.vy = vector->vy;
    scaled.vz = vector->vz;
    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormalS(&scaled, out);
}

/* Length of a vector. */
s32 func_800886FC(VECTOR *vector) {
    VECTOR square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return SquareRoot0(square.vx + square.vy + square.vz);
}

/* Horizontal (x/z) length of a vector. */
s32 func_80088754(VECTOR *vector) {
    VECTOR square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return SquareRoot0(square.vx + square.vz);
}

/* Distance between two points. */
s32 func_800887A4(VECTOR *from, VECTOR *to) {
    VECTOR delta;

    delta.vx = to->vx - from->vx;
    delta.vy = to->vy - from->vy;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return SquareRoot0(delta.vx + delta.vy + delta.vz);
}

/* Horizontal (x/z) distance between two points. */
s32 func_80088838(VECTOR *from, VECTOR *to) {
    VECTOR delta;

    delta.vx = to->vx - from->vx;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return SquareRoot0(delta.vx + delta.vz);
}

/* Set a bit of the resident flag array. */
void func_800888B0(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    D_8006D634.progress[flag >> 3] |= bit;
}

/* Test a bit of the resident flag array. */
s32 func_800888E4(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    return D_8006D634.progress[flag >> 3] & bit;
}

/* Clear a bit of the resident flag array. */
void func_80088908(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    D_8006D634.progress[flag >> 3] &= ~bit;
}

/* Set bit 16 of the resident state word. */
void func_80088940(void) {
    D_8006D634.options.complete = 1;
}

/* Once bit 16 of the resident state word is set, queue list entry 22
 * (ARGENTO, only once). */
void func_8008895C(void) {
    if (D_8006D634.options.complete && D_800927EC == 0) {
        D_800927EC = 1;
        D_800928EC[D_80092888++] = &D_80091964[22];
    }
}

/* Once every progress flag 0..48 except 22 is set, mark the options
 * complete and apply the unlock. */
s32 func_800889C8(void) {
    s32 flag;

    for (flag = 0; flag < 49; flag++) {
        if (flag != 22 && !func_800888E4(flag)) {
            return 0;
        }
    }
    D_8006D634.options.complete = 1;
    func_8008895C();
    return 0;
}

/* Store the current option settings in the saved options word. */
void func_80088A40(void) {
    if (D_8005061C) {
        D_8006D634.options.version = 1;
        D_8006D634.options.option4 = D_80099D98.option4;
        D_8006D634.options.option5 = D_80099D98.option5;
        D_8006D634.options.option6 = D_80099D98.option6;
        D_8006D634.options.option13 = D_80099D98.level;
    }
}

/* Load the option settings from the saved options word, or write the
 * defaults when it was never written; a completed word clears the flags. */
void func_80088AF8(void) {
    s32 i;

    if (D_8005061C) {
        D_800927EC = 0;
        if (D_8006D634.options.version == 1) {
            D_80099D98.option4 = D_8006D634.options.option4;
            D_80099D98.option5 = D_8006D634.options.option5;
            D_80099D98.option6 = D_8006D634.options.option6;
            D_80099D98.level = D_8006D634.options.option13;
            if (D_8006D634.options.complete) {
                for (i = 0; i < 8; i++) {
                    D_8006D634.progress[i] = 0;
                }
            }
        } else {
            D_80099D98.option4 = 0;
            D_80099D98.option5 = 0;
            D_80099D98.option6 = 2;
            D_80099D98.level = 0;
            func_80088A40();
        }
    }
}

/* Set a progress flag, then check for completion. */
void func_80088BD4(s32 flag) {
    func_800888B0(flag);
    func_800889C8();
}
