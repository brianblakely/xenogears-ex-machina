/* Field unit 800A9274-end: particle effects, model instance bounds,
 * sprites, the map picture, the overlay sprites and text glyphs, the text
 * roll and sequence, event operands and party slot swaps.
 *
 * Its rodata starts 0 mod 8 after 800a5c40's 4-mod-8 table, at 0x2d8 (the
 * first rodata of 800a9688) or at 0x2e8 (800ab748's jump table); this file
 * takes 0x2d8 with the text from 800a9274, where the particle effect slots
 * start (see field_800A4748.c). */
#include "common.h"
#include "field.h"
#include "field_anim.h"
#include "field_gte.h"
#include "field_motion.h"
#include "field_script.h"
#include "field_actor_events.h"
#include "field_screen.h"
#include "field_movie.h"
#include "field_panel.h"


extern s16 D_800B0108[64]; /* effect slot owners, -1 free */
extern u8 D_800B14B0[64];  /* effect slot states */

/* Free all 64 effect slots. */
void func_800A9274(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        D_800B14B0[i] = 0;
        D_800B0108[i] = -1;
    }
}

extern Record78 *D_800C3918[64]; /* effect slot emitters */

/* Release effect slot `slot` and its particles. */
void func_800A92AC(s32 slot) {
    Record78 *emitter;
    s32 i;

    if (D_800B14B0[slot] == 1) {
        emitter = D_800C3918[slot];
        for (i = 0; i < 8; i++) {
            if (emitter->count != 0) {
                func_800320E8(emitter->particles);
            }
            emitter++;
        }
        func_800320E8(D_800C3918[slot]);
    }
    D_800B14B0[slot] = 0;
    D_800B0108[slot] = -1;
}

/* Stop the emitters of effect slot `slot`. */
void func_800A9374(s32 slot) {
    Record78 *emitter;
    s32 i;

    if (D_800B14B0[slot] == 1) {
        emitter = D_800C3918[slot];
        for (i = 0; i < 8; i++) {
            if (emitter->count != 0) {
                emitter->unk04 = 0;
            }
            emitter++;
        }
    }
}

/* Stop the emitters of effect slot `slot` and release their particles. */
void func_800A93CC(s32 slot) {
    Record78 *emitter;
    s32 i;
    s32 j;
    Particle *particle;

    if (D_800B14B0[slot] == 1) {
        emitter = D_800C3918[slot];
        for (i = 0; i < 8; i++) {
            if (emitter->count != 0) {
                emitter->unk04 = 0;
                for (j = 0; j < emitter->count; j++) {
                    particle = &emitter->particles[j];
                    particle->unk04 = 1;
                }
            }
            emitter++;
        }
    }
}

/* Release all effect slots. */
void func_800A9460(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        func_800A92AC(i);
    }
    func_800775F8();
}

extern s32 D_800B0044;

/* Reset the eight particle emitters at 800b02cc with parameter `value`. */
void func_800A94A4(s32 value) {
    s32 i;
    s32 j;

    D_800B0044 = 0;
    for (i = 0; i < 8; i++) {
        D_800B02CC[i].unk52 = value;
        D_800B02CC[i].unk00 = 0;
        D_800B02CC[i].unk02 = 0;
        D_800B02CC[i].unk04 = 0x80;
        D_800B02CC[i].count = 0;
        setVector(&D_800B02CC[i].unk0C, 0, 0, 0);
        setVector(&D_800B02CC[i].unk14, 0, -1000, 0);
        D_800B02CC[i].unk08 = 0x8000;
        D_800B02CC[i].unk50 = 0x800;
        D_800B02CC[i].unk24 = 1;
        setVector(&D_800B02CC[i].unk1C, 0, 0, 0);
        D_800B02CC[i].unk28 = 0x100;
        D_800B02CC[i].unk58 = 0x1C;
        D_800B02CC[i].unk26 = 0;
        D_800B02CC[i].flags = 0;
        D_800B02CC[i].unk76 = 0;
        D_800B02CC[i].unk56 = 1;
        D_800B02CC[i].unk54 = 0;
        setVector(&D_800B02CC[i].unk5A, 0x1C8, 0x1C8, 0x1C8);
        setVector(&D_800B02CC[i].unk62, 0x20, 0x20, 0x20);
        D_800B02CC[i].unk6A = 0x80;
        D_800B02CC[i].unk6B = 0x20;
        D_800B02CC[i].unk6C = 0;
        D_800B02CC[i].unk6E = -4;
        D_800B02CC[i].unk6F = -1;
        D_800B02CC[i].unk70 = 0;
        for (j = 0; j < 8; j++) {
            D_800B02CC[i].unk30[j][0] = 0;
            D_800B02CC[i].unk30[j][1] = 0;
        }
    }
}

#include "field_effect.h"

/* Run the effect slots for a frame: count down emitter delays, spawn and
 * draw particles, count down emitter lifetimes (7fff lasts), and release
 * the slots with nothing left alive. */
void func_800A9688(void) {
    MATRIX view;
    s32 spawned;
    Record78 *emitter;
    s32 alive;
    s32 slot;
    s32 i;
    s32 j;

    if (D_800ADB34 != 0) {
        return;
    }
    view = *(MATRIX *)D_800AFA64;
    for (slot = 0; slot < 64; slot++) {
        alive = 0;
        if (D_800B14B0[slot] == 1) {
            emitter = D_800C3918[slot];
            for (i = 0; i < 8; i++) {
                spawned = 0;
                if (emitter->count != 0) {
                    if (emitter->unk02 == 0) {
                        for (j = 0; j < emitter->count; j++) {
                            if (emitter->particles[j].unk00 == 0) {
                                if (emitter->unk04 != 0) {
                                    func_800AA6B4(emitter, &emitter->particles[j], &spawned);
                                    func_800A9F18(emitter, &emitter->particles[j], &view);
                                    alive = 1;
                                }
                            } else {
                                func_800A9F18(emitter, &emitter->particles[j], &view);
                                alive = 1;
                            }
                        }
                        if (emitter->unk04 != 0) {
                            if (emitter->unk04 != 0x7FFF) {
                                emitter->unk04--;
                            }
                            alive = 1;
                        }
                    } else {
                        alive = 1;
                        emitter->unk02--;
                    }
                }
                emitter++;
            }
            if (alive == 0) {
                func_800A92AC(slot);
            }
        }
    }
    if (D_800C268C == 0) {
        func_80281B00("PARTICLE  ");
    }
}

/* A random number in 0..range. */
s32 func_800A987C(s32 range) {
    return (rand() * range + 1) >> 15;
}

/* The first free effect slot, or -1. */
s32 func_800A98B4(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (D_800B14B0[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* Stop the effect slots owned by `owner`; with `release` also release
 * their particles. */
void func_800A98E8(s32 owner, s32 release) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (D_800B0108[i] == owner) {
            if (release == 0) {
                D_800C3918[i]->unk04 = 0;
                D_800C3918[i]->unk02 = 0;
                func_800A9374(i);
            } else {
                D_800C3918[i]->unk04 = 0;
                D_800C3918[i]->unk02 = 0;
                func_800A93CC(i);
            }
        }
    }
}

typedef struct {
    Record78 emitters[8];
} EmitterSet;
extern s32 D_800ADB44; /* last effect owner */
void func_800A8EAC(Particle *particle, s32 a, s32 b);

/* Start an effect for `owner` in a free slot: copy the eight template
 * emitters and allocate and set up their particles. -1 when no slot. */
s32 func_800A99A8(s32 owner) {
    Record78 *emitters;
    Record78 *emitter;
    s32 slot;
    s32 j;

    slot = func_800A98B4();
    if (slot == -1) {
        return -1;
    }
    func_80032498(8, 0);
    D_800ADB44 = owner;
    D_800B14B0[slot] = 1;
    D_800B0108[slot] = owner;
    emitters = func_80031BDC(0x3C0, 0);
    D_800C3918[slot] = emitters;
    *(EmitterSet *)emitters = *(EmitterSet *)D_800B02CC;
    emitter = emitters;
    for (slot = 0; slot < 8; slot++) {
        if (emitter->count != 0) {
            emitter->particles = func_80031BDC(emitter->count * sizeof(Particle), 0);
            for (j = 0; j < emitter->count; j++) {
                emitter->particles[j].unk00 = 0;
                func_800A8EAC(&emitter->particles[j], emitter->unk54, ((s16)emitter->flags >> 8) + 1 & 3);
            }
        }
        emitter++;
    }
    return 1;
}

/* `value` + `delta`, clamped to 0..255. */
s32 func_800A9B1C(s32 value, s32 delta) {
    if (delta < 0) {
        value += delta;
        if (value < 0) {
            value = 0;
        }
    } else {
        value += delta;
        if (value >= 0x100) {
            value = 0xFF;
        }
    }
    return value;
}

/* Transform a particle's quad: rotate it by `angle` at its position under
 * `view` (mode 3 scales the view by `scale` and then the quad again),
 * scale it by its size, tint it and link it at its depth (depth mode 0
 * front, 1 nearer, 2 as is, 3 farther). */
void func_800A9B54(Particle *particle, MATRIX *view, s16 angle, s32 depth_mode, VECTOR *scale, s32 mode) {
    MATRIX m;
    MATRIX local;
    MATRIX scaled_view;
    SVECTOR rotation;
    VECTOR size;
    s32 otz;
    s32 p;

    rotation.vx = 0;
    rotation.vy = 0;
    rotation.vz = angle;
    func_8003F738(&rotation, &local);
    local.t[0] = particle->position.vx >> 12;
    local.t[1] = particle->position.vy >> 12;
    local.t[2] = particle->position.vz >> 12;
    if (mode == 3) {
        scaled_view = *view;
        ScaleMatrix(&scaled_view, scale);
        CompMatrix(&scaled_view, &local, &m);
        func_8007409C(&m, &local);
        size.vx = particle->unk38.vx;
        size.vy = particle->unk38.vy;
        size.vz = particle->unk38.vz;
        ScaleMatrix(&m, &size);
        (particle->quads + D_800ADB08)->r0 = particle->unk48[0];
        (particle->quads + D_800ADB08)->g0 = particle->unk48[1];
        (particle->quads + D_800ADB08)->b0 = particle->unk48[2];
        SetTransMatrix(&m);
        ScaleMatrix(&m, scale);
    } else {
        CompMatrix(view, &local, &m);
        func_8007409C(&m, &local);
        size.vx = particle->unk38.vx;
        size.vy = particle->unk38.vy;
        size.vz = particle->unk38.vz;
        ScaleMatrix(&m, &size);
        (particle->quads + D_800ADB08)->r0 = particle->unk48[0];
        (particle->quads + D_800ADB08)->g0 = particle->unk48[1];
        (particle->quads + D_800ADB08)->b0 = particle->unk48[2];
        SetTransMatrix(&m);
    }
    SetRotMatrix(&m);
    otz = RotAverage4(&particle->corners[0], &particle->corners[1], &particle->corners[2], &particle->corners[3],
                      (long *)&particle->quads[D_800ADB08].x0, (long *)&particle->quads[D_800ADB08].x1,
                      (long *)&particle->quads[D_800ADB08].x2, (long *)&particle->quads[D_800ADB08].x3, &p, &p) >>
          D_80050100;
    switch (depth_mode) {
    case 0:
        p = 1;
        break;
    case 1:
        p = otz - 0x10;
        break;
    case 2:
        p = otz;
        break;
    case 3:
        p = otz + 0x10;
        break;
    }
    if (p > 0 && p < 0x1000) {
        addPrim(&D_800C426C->ot[p], &particle->quads[D_800ADB08]);
    }
}

/* Step a particle: while delayed count down and at launch place it and
 * its velocity in the emitter's frame (0 owner-facing, 1 801e module, 2
 * owner's transform, 3 owner-facing and scaled); afterwards move it,
 * fade its colour, draw it and count its life down. */
void func_800A9F18(Record78 *emitter, Particle *particle, MATRIX *view) {
    VECTOR v;
    SVECTOR sv;
    MATRIX m;
    MATRIX camera;
    VECTOR origin;
    VECTOR up;
    VECTOR rotated;
    VECTOR scale;
    s32 flag;
    s32 scaled;

    if (particle->unk02 != 0) {
        if (--particle->unk02 == 0) {
            m.t[0] = m.t[1] = m.t[2] = 0;
            scaled = 0;
            switch ((emitter->flags >> 4) & 3) {
            case 3:
                sv.vx = 0;
                sv.vy = D_800AF880.components.descriptors[emitter->unk52].actor->unk108;
                sv.vz = 0;
                func_8003F738(&sv, &m);
                origin.vx = WHOLE(D_800AF880.components.descriptors[emitter->unk52].actor->position[0]);
                origin.vy = WHOLE(D_800AF880.components.descriptors[emitter->unk52].actor->position[1]);
                origin.vz = WHOLE(D_800AF880.components.descriptors[emitter->unk52].actor->position[2]);
                emitter->unk50 = D_800AF880.components.descriptors[emitter->unk52].actor->scale[0];
                scaled = 1;
                break;
            case 0:
                sv.vx = 0;
                sv.vy = D_800AF880.components.descriptors[emitter->unk52].actor->unk108;
                sv.vz = 0;
                func_8003F738(&sv, &m);
                origin.vx = WHOLE(D_800AF880.components.descriptors[emitter->unk52].actor->position[0]);
                origin.vy = WHOLE(D_800AF880.components.descriptors[emitter->unk52].actor->position[1]);
                origin.vz = WHOLE(D_800AF880.components.descriptors[emitter->unk52].actor->position[2]);
                emitter->unk50 = 0x1000;
                break;
            case 1:
                func_801E72CC(&m, &camera, emitter->unk72, emitter->unk74);
                SetRotMatrix(&m);
                SetTransMatrix(&m);
                sv.vx = emitter->unk0C.vx;
                sv.vy = emitter->unk0C.vy;
                sv.vz = emitter->unk0C.vz;
                func_8004A6DC(&sv, &origin.vx, &flag);
                emitter->unk50 = 0x1000;
                break;
            case 2:
                m = D_800AF880.components.descriptors[emitter->unk52].transform;
                SetRotMatrix(&m);
                SetTransMatrix(&m);
                sv.vx = emitter->unk0C.vx;
                sv.vy = emitter->unk0C.vy;
                sv.vz = emitter->unk0C.vz;
                func_8004A6DC(&sv, &origin.vx, &flag);
                emitter->unk50 = 0x1000;
                break;
            }
            m.t[0] = m.t[1] = m.t[2] = 0;
            SetRotMatrix(&m);
            SetTransMatrix(&m);
            sv.vx = particle->velocity.vx;
            sv.vy = particle->velocity.vy;
            sv.vz = particle->velocity.vz;
            func_800495DC(&sv, &v);
            VectorNormal(&v, &particle->velocity);
            particle->velocity.vx = (particle->velocity.vx * emitter->unk08 >> 12) * emitter->unk24;
            particle->velocity.vy = (particle->velocity.vy * emitter->unk08 >> 12) * emitter->unk24;
            particle->velocity.vz = (particle->velocity.vz * emitter->unk08 >> 12) * emitter->unk24;
            if (scaled == 1) {
                particle->position.vx = particle->position.vx * emitter->unk50 >> 12;
                particle->position.vy = particle->position.vy * emitter->unk50 >> 12;
                particle->position.vz = particle->position.vz * emitter->unk50 >> 12;
            }
            SetRotMatrix(&m);
            SetTransMatrix(&m);
            sv.vx = particle->position.vx;
            sv.vy = particle->position.vy;
            sv.vz = particle->position.vz;
            func_8004A6DC(&sv, &v.vx, &flag);
            if (scaled == 1) {
                sv.vz = 0;
                sv.vx = D_800B00B4 - 0x400;
                sv.vy = -D_800AF880.view_angle;
                func_8004ABBC(&sv, &camera);
                SetRotMatrix(&camera);
                SetTransMatrix(&camera);
                up.vx = 0;
                up.vz = 0;
                up.vy = v.vy;
                func_8004998C(&up, &rotated);
                v.vx += rotated.vx;
                v.vz += rotated.vz;
                v.vy = rotated.vy;
                particle->position.vx = (origin.vx + v.vx) * (0x1000000 / emitter->unk50);
                particle->position.vy = (origin.vy + v.vy) * (0x1000000 / emitter->unk50);
                particle->position.vz = (origin.vz + v.vz) * (0x1000000 / emitter->unk50);
                return;
            }
            particle->position.vx = (origin.vx + v.vx) << 12;
            particle->position.vy = (origin.vy + v.vy) << 12;
            particle->position.vz = (origin.vz + v.vz) << 12;
        }
    } else {
        particle->velocity.vx += particle->unk28.vx;
        particle->velocity.vy += particle->unk28.vy;
        particle->velocity.vz += particle->unk28.vz;
        particle->position.vx += particle->velocity.vx;
        particle->position.vy += particle->velocity.vy;
        particle->position.vz += particle->velocity.vz;
        particle->unk38.vx += particle->unk40.vx;
        particle->unk38.vy += particle->unk40.vy;
        particle->unk38.vz += particle->unk40.vz;
        particle->unk48[0] = func_800A9B1C(particle->unk48[0], particle->unk4C[0]);
        particle->unk48[1] = func_800A9B1C(particle->unk48[1], particle->unk4C[1]);
        particle->unk48[2] = func_800A9B1C(particle->unk48[2], particle->unk4C[2]);
        scale.vx = emitter->unk50;
        scale.vy = emitter->unk50;
        scale.vz = emitter->unk50;
        if (particle->unk04 != 1) {
            func_800A9B54(particle, view, particle->angle, (emitter->flags >> 1) & 3, &scale,
                          (emitter->flags >> 4) & 3);
        }
        if (--particle->unk04 == 0) {
            particle->unk00 = 0;
        }
    }
}

extern u8 D_800AF474[8]; /* spawn offset per view octant */

/* Spawn `particle` of `emitter`: its delay after the previous spawn, a
 * random start within the spawn radius around the emitter (offset by the
 * owner's view octant) and a velocity toward a random point of the
 * target spread. */
void func_800AA6B4(Record78 *emitter, Particle *particle, s32 *spawned) {
    VECTOR unused; /* the original frame reserves an unused 16-byte local */
    VECTOR start;
    VECTOR end;
    s32 radius;
    s32 angle;
    u32 facing;
    s32 k;

    particle->unk00 = 1;
    particle->unk02 = emitter->unk56 + *spawned;
    *spawned += emitter->unk56;
    particle->unk04 = emitter->unk58;
    if (emitter->flags & 1) {
        particle->angle = rand() & 0xFFF;
    } else {
        particle->angle = emitter->unk76;
    }
    if (!(emitter->flags & 0x80)) {
        radius = func_800A987C(emitter->unk26);
    } else {
        radius = emitter->unk26;
    }
    angle = func_800A987C(0xFFF);
    start.vx = func_8003F8CC(angle) * radius >> 12;
    if (!(emitter->flags & 0x40)) {
        start.vz = func_8003F8B0(angle) * radius >> 12;
    } else {
        start.vz = 0;
    }
    facing = (D_800AF880.view_angle + D_800AF880.components.descriptors[emitter->unk52].actor->unk108) & 0xFFF;
    k = D_800AF474[facing >> 9];
    start.vx += emitter->unk0C.vx + emitter->unk30[k][0];
    start.vz += emitter->unk0C.vz + emitter->unk30[k][1];
    start.vy = emitter->unk0C.vy;
    particle->position.vx = start.vx;
    particle->position.vz = start.vz;
    particle->position.vy = start.vy;
    radius = func_800A987C(emitter->unk28);
    end.vx = emitter->unk14.vx + (func_8003F8CC(angle) * radius >> 12);
    end.vz = emitter->unk14.vz + (func_8003F8B0(angle) * radius >> 12);
    end.vy = emitter->unk14.vy;
    particle->velocity.vx = end.vx - start.vx;
    particle->velocity.vy = end.vy - start.vy;
    particle->velocity.vz = end.vz - start.vz;
    particle->unk28.vx = emitter->unk1C.vx;
    particle->unk28.vy = emitter->unk1C.vy;
    particle->unk28.vz = emitter->unk1C.vz;
    particle->unk38.vx = emitter->unk5A.vx;
    particle->unk38.vy = emitter->unk5A.vy;
    particle->unk38.vz = emitter->unk5A.vz;
    particle->unk40.vx = emitter->unk62.vx;
    particle->unk40.vy = emitter->unk62.vy;
    particle->unk40.vz = emitter->unk62.vz;
    particle->unk48[0] = emitter->unk6A;
    particle->unk48[1] = emitter->unk6B;
    particle->unk48[2] = emitter->unk6C;
    particle->unk4C[0] = emitter->unk6E;
    particle->unk4C[1] = emitter->unk6F;
    particle->unk4C[2] = emitter->unk70;
}

/* Set an instance's bounding centre and radius from its mesh bounds. */
void func_800AA9DC(FieldInstance *instance) {
    s32 min_x;
    s32 min_y;
    s32 min_z;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 size;

    dx = size = instance->mesh->max[0] - (min_x = instance->mesh->min[0]);
    dy = instance->mesh->max[1] - (min_y = instance->mesh->min[1]);
    dz = instance->mesh->max[2] - (min_z = instance->mesh->min[2]);
    if (size < dy) {
        size = dy;
    }
    if (size < dz) {
        size = dz;
    }
    instance->center[0] = dx / 2 + min_x;
    instance->center[1] = dy / 2 + min_y;
    instance->center[2] = dz / 2 + min_z;
    instance->radius = size * 2 + 1;
}

extern MATRIX D_800B00E8; /* instance view: the rotation with its translation */

/* 0 when an instance's bounding square (its radius around its centre) is
 * on screen, else -1. */
s32 func_800AAA74(FieldInstance *instance) {
    VECTOR position;
    SVECTOR corner;
    s32 flag;
    s32 sxy;
    s32 depth;
    s32 radius;
    s32 top;
    s32 left;
    s32 bottom;
    s32 right;

    func_8004A6DC((SVECTOR *)instance->center, &position.vx, &flag);
    D_800B00E8.t[0] = position.vx;
    D_800B00E8.t[1] = position.vy;
    D_800B00E8.t[2] = position.vz;
    SetRotMatrix(&D_800B00E8);
    SetTransMatrix(&D_800B00E8);
    radius = instance->radius;
    corner.vy = corner.vx = -radius;
    corner.vz = 0;
    RotTransPers(&corner, &sxy, &depth, &flag);
    top = sxy >> 16;
    left = (s16)sxy;
    corner.vy = corner.vx = radius;
    corner.vz = 0;
    RotTransPers(&corner, &sxy, &depth, &flag);
    bottom = sxy >> 16;
    right = (s16)sxy;
    if (top < D_800C3A60 + 0xE0 && -D_800C3A60 < bottom && left < D_800C3A5C + 0x140 && -D_800C3A5C < right) {
        return 0;
    }
    return -1;
}

typedef struct {
    DR_MODE modes[33][2];
    SPRT sprites[33][2];
} FieldSprites;
extern FieldSprites *D_800AFC68;

/* Release the sprite block. */
void func_800AABD8(void) {
    func_800320E8(D_800AFC68);
    DrawSync(0);
}

/* Allocate the 33 sprites (the first 16x16, the rest 8x8), each with a
 * draw mode per buffer. */
void func_800AAC08(void) {
    RECT window;
    SPRT *sprite;
    SPRT *copy;
    s32 i;

    D_800AFC68 = func_80031BDC(0x840, 0);
    window.x = 0;
    window.y = 0;
    window.w = 0xFF;
    window.h = 0xFF;
    for (i = 0; i < 33; i++) {
        SetDrawMode(&D_800AFC68->modes[i][0], 0, 0, GetTPage(0, 0, 0x3C0, 0x100), &window);
        SetDrawMode(&D_800AFC68->modes[i][1], 0, 0, GetTPage(0, 0, 0x3C0, 0x140), &window);
        sprite = D_800AFC68->sprites[i];
        SetSprt(sprite);
        copy = sprite + 1;
        sprite->r0 = 0x80;
        sprite->g0 = 0x80;
        sprite->b0 = 0x80;
        if (i == 0) {
            sprite->u0 = 0xE0;
            sprite->v0 = 0x70;
            sprite->h = sprite->w = 0x10;
        } else {
            sprite->v0 = 0x60;
            sprite->u0 = 0xE0;
            sprite->h = sprite->w = 8;
        }
        sprite->x0 = 0xA0;
        sprite->y0 = 0x70;
        sprite->clut = GetClut(0x100, 0xF7);
        *copy = *sprite;
    }
}

/* Set sprite `index`'s colour in both buffers. */
void func_800AADC8(s32 index, s32 r, s32 g, s32 b) {
    (D_800AFC68->sprites[index] + 0)->r0 = r;
    (D_800AFC68->sprites[index] + 0)->g0 = g;
    (D_800AFC68->sprites[index] + 0)->b0 = b;
    (D_800AFC68->sprites[index] + 1)->r0 = r;
    (D_800AFC68->sprites[index] + 1)->g0 = g;
    (D_800AFC68->sprites[index] + 1)->b0 = b;
}

/* Place sprite `index` at (x, y) (anchor 0: 4, 12 above-left; 1: 4, 4)
 * and link it with its draw mode into the overlay ordering table. */
void func_800AAE4C(s32 index, s32 x, s32 y, s32 anchor) {
    switch (anchor) {
    case 0:
        y -= 12;
        x -= 4;
        break;
    case 1:
        y -= 4;
        x -= 4;
        break;
    }
    D_800AFC68->sprites[index][D_800ADB08].x0 = x;
    D_800AFC68->sprites[index][D_800ADB08].y0 = y;
    addPrim(&D_800C426C->overlay_ot[0], &D_800AFC68->sprites[index][D_800ADB08]);
    addPrim(&D_800C426C->overlay_ot[0], &D_800AFC68->modes[index][D_800ADB08]);
}

#include "field_picture.h"

/* Set up the picture: four marker sprites (the first 16x16, the rest
 * 8x8) and the three 128x224 picture pieces from the 8-bit pages at
 * (300, 100). */
void func_800AAF80(void) {
    RECT window;
    SPRT *sprite;
    SPRT *copy;
    POLY_FT4 *quad;
    POLY_FT4 *quad_copy;
    s32 i;

    D_800C3A3C = func_80031BDC(sizeof(ScreenPieces), 0);
    D_800B1DF0 = func_80031BDC(sizeof(PictureMarks), 0);
    setRECT(&window, 0, 0, 0xFF, 0xFF);
    for (i = 0; i < 4; i++) {
        SetDrawMode(&D_800B1DF0->modes[i][0], 0, 0, GetTPage(0, 0, 0x3C0, 0x140), &window);
        SetDrawMode(&D_800B1DF0->modes[i][1], 0, 0, GetTPage(0, 0, 0x3C0, 0x140), &window);
        sprite = &D_800B1DF0->sprites[i][0];
        SetSprt(sprite);
        copy = sprite + 1;
        setRGB0(sprite, 0x80, 0x80, 0x80);
        sprite->x0 = 0xA0;
        sprite->y0 = 0x70;
        if (i == 0) {
            sprite->u0 = 0xE0;
            sprite->v0 = 0x70;
            sprite->h = sprite->w = 0x10;
        } else {
            sprite->v0 = 0x60;
            sprite->u0 = 0xE0;
            sprite->h = sprite->w = 8;
        }
        sprite->clut = GetClut(0x100, 0xF7);
        *copy = *sprite;
    }
    for (i = 0; i < 3; i++) {
        quad = &D_800C3A3C->quads[i][0];
        quad_copy = &D_800C3A3C->quads[i][1];
        SetPolyFT4(quad);
        quad->x0 = i << 7;
        quad->y2 = 0xDF;
        quad->x2 = i << 7;
        quad->y0 = 0;
        quad->x1 = (i << 7) + 0x80;
        quad->y1 = 0;
        quad->x3 = (i << 7) + 0x80;
        quad->y3 = 0xDF;
        setRECT(&D_800C3A3C->windows[i][0], 0, 0, 0xFF, 0xFF);
        setRECT(&D_800C3A3C->windows[i][1], 0, 0, 0xFF, 0xFF);
        SetDrawMode(&D_800C3A3C->modes[i][0], 0, 0, GetTPage(1, 0, 0x300 + i * 0x40, 0x100),
                    &D_800C3A3C->windows[i][0]);
        SetDrawMode(&D_800C3A3C->modes[i][1], 0, 0, GetTPage(1, 0, 0x300 + i * 0x40, 0x100),
                    &D_800C3A3C->windows[i][1]);
        setRGB0(quad, 0x80, 0x80, 0x80);
        SetSemiTrans(quad, 1);
        quad->v2 = 0xDF;
        quad->u0 = 0;
        quad->v0 = 0;
        quad->u1 = 0x80;
        quad->v1 = 0;
        quad->u2 = 0;
        quad->u3 = 0x80;
        quad->v3 = 0xDF;
        quad->tpage = GetTPage(1, 0, 0x300 + i * 0x40, 0x100);
        quad->clut = GetClut(0, 0xF6);
        *quad_copy = *quad;
    }
}

/* 0 when item `item` is held in inventory list 0, else -1. */
s32 func_800AB328(s32 item) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->id0[i] == item && D_8005A39C->count0[i] != 0) {
            return 0;
        }
    }
    return -1;
}

/* Draw the picture at brightness `level`: the marker at the controlled
 * actor's scaled map position, then the three picture pieces. */
void func_800AB378(s32 level) {
    FieldActor *actor;
    s32 x;
    s32 y;
    s32 i;

    actor = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    x = WHOLE(actor->position[0]) * D_800C3914 >> 16;
    y = -(WHOLE(actor->position[2]) * D_800C3A18) >> 16;
    for (i = 0; i < 1; i++) {
        if (i == 0) {
            y -= 12;
            x -= 4;
        }
        D_800B1DF0->sprites[i][D_800ADB08].x0 = x + D_800AFE78;
        D_800B1DF0->sprites[i][D_800ADB08].y0 = y + D_800AFE7C;
        (D_800B1DF0->sprites[i] + (D_800ADB08 & 1))->r0 = level;
        (D_800B1DF0->sprites[i] + (D_800ADB08 & 1))->g0 = level;
        (D_800B1DF0->sprites[i] + (D_800ADB08 & 1))->b0 = level;
        addPrim(&D_800C426C->overlay_ot[0], &D_800B1DF0->sprites[i][D_800ADB08]);
        addPrim(&D_800C426C->overlay_ot[0], &D_800B1DF0->modes[i][D_800ADB08]);
    }
    for (i = 0; i < 3; i++) {
        (D_800C3A3C->quads[i] + (D_800ADB08 & 1))->r0 = level;
        (D_800C3A3C->quads[i] + (D_800ADB08 & 1))->g0 = level;
        (D_800C3A3C->quads[i] + (D_800ADB08 & 1))->b0 = level;
        addPrim(&D_800C426C->overlay_ot[0], &D_800C3A3C->quads[i][D_800ADB08]);
        addPrim(&D_800C426C->overlay_ot[0], &D_800C3A3C->modes[i][D_800ADB08]);
    }
}

/* 0 when game flag `which` (bits 3-6 of +1a16) is set; for 4, when bit 7
 * is clear; else -1. */
s32 func_800AB748(u32 which) {
    switch (which) {
    case 0:
        if (D_8005A39C->vars[0x73] & 8) {
            return 0;
        }
        break;
    case 1:
        if (D_8005A39C->vars[0x73] & 0x10) {
            return 0;
        }
        break;
    case 2:
        if (D_8005A39C->vars[0x73] & 0x20) {
            return 0;
        }
        break;
    case 3:
        if (D_8005A39C->vars[0x73] & 0x40) {
            return 0;
        }
        break;
    case 4:
        if (!(D_8005A39C->vars[0x73] & 0x80)) {
            return 0;
        }
        break;
    }
    return -1;
}

extern RECT D_800AF5C0[5]; /* pieces of file 0x802's 320-wide image */

#ifdef NON_MATCHING
/* Upload the pieces of file 0x802's image whose game flag (800ab748) is
 * clear to the 8-bit page area at (300, 100).
 * NON_MATCHING (score 171 -> 139): the original indexes the piece table
 * with i * 8 against bases held in registers (constants CSE reuses as
 * related values: &x in t0 rematerialized, h = (i * 8) + (sym + 6) hoisted
 * out of the row loop, y and w as copies of the outer field pointers) and
 * spills file, pixels, i and the x pointer. Reading h in the row test and
 * the outer w and h through a flat halfword view gives the original's outer
 * pointers (h, w, y from one sym + 6 base); the row loop still reads x, y
 * and w symbol-relative, so nothing else is hoisted and nothing spills.
 * Every struct/flat/pointer mix of the nine reads was scored (best 139). */
#define PIECE(n, field) (((s16 *)D_800AF5C0)[(n) * 4 + (field)])
enum { PIECE_X, PIECE_Y, PIECE_W, PIECE_H };
void func_800AB808(void) {
    TIM_IMAGE tim;
    u_long *file;
    u8 *pixels;
    u8 *row_pixels;
    s32 i;
    s32 row;

    file = func_80031BDC(func_800288EC(0x802), 0);
    func_800295D8(0x802, file, 0, 0x80);
    func_80028A60(0);
    pixels = func_80031BDC(0xF20, 0);
    OpenTIM(file);
    if (ReadTIM(&tim) != NULL) {
        for (i = 0; i < 5; i++) {
            if (func_800AB748(i) == -1 && tim.paddr != NULL) {
                row_pixels = pixels;
                for (row = 0; row < PIECE(i, PIECE_H); row++) {
                    memcpy(row_pixels, tim.paddr + (D_800AF5C0[i].y + row) * 0x50 + D_800AF5C0[i].x / 4,
                           D_800AF5C0[i].w);
                    row_pixels += D_800AF5C0[i].w / 4 * 4;
                }
                tim.prect->x = D_800AF5C0[i].x / 2 + 0x300;
                tim.prect->y = D_800AF5C0[i].y + 0x100;
                tim.prect->w = PIECE(i, PIECE_W) / 2;
                tim.prect->h = PIECE(i, PIECE_H);
                LoadImage(tim.prect, (u_long *)pixels);
                DrawSync(0);
            }
        }
    }
    func_800320E8(file);
    func_800320E8(pixels);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800A9274", func_800AB808);
#endif

/* Show the current map's picture while its item is held: park the VRAM
 * at (300, 100), load the picture, fade it in, hold until the button, fade
 * it out and restore the VRAM. */
void func_800ABA98(void) {
    RECT rect;
    u8 *saved;
    u32 *file;
    s32 picture;
    s32 i;

    for (i = 0;; i++) {
        if (D_800AF47C[i * PICTURE_WORDS + PICTURE_MAP] == 0xFFFF) {
            return;
        }
        if ((D_8004F34C & 0x3FFF) == D_800AF47C[i * PICTURE_WORDS + PICTURE_MAP]) {
            break;
        }
    }
    if (func_800AB328(D_800AF47C[i * PICTURE_WORDS + PICTURE_ITEM]) == -1) {
        return;
    }
    D_800C3914 = D_800AF47C[i * PICTURE_WORDS + PICTURE_UNK04];
    D_800C3A18 = D_800AF47C[i * PICTURE_WORDS + PICTURE_UNK08];
    picture = D_800AF47C[i * PICTURE_WORDS + PICTURE_FILE];
    D_800AFE78 = D_800AF47C[i * PICTURE_WORDS + PICTURE_X];
    D_800AFE7C = D_800AF47C[i * PICTURE_WORDS + PICTURE_Y];
    setRECT(&rect, 0x300, 0x100, 0xA0, 0x100);
    saved = func_80031BDC(0x14000, 0);
    StoreImage(&rect, (u_long *)saved);
    DrawSync(0);
    func_80028A60(0);
    func_80028470(4, 0);
    file = func_80031BDC(func_800288EC(picture + 0x7FB), 0);
    func_800295D8(picture + 0x7FB, file, 0, 0x80);
    func_80028A60(0);
    func_80070340(file, 0x300, 0x100, 0, 0xF6, 0, 0);
    func_800320E8(file);
    if (D_800AF47C[i * PICTURE_WORDS + PICTURE_PIECES] == 1) {
        func_800AB808();
    }
    func_800AAF80();
    for (i = 0; i < 16; i++) {
        func_80073FE0();
        func_800AB378(i * 8);
        func_800A6924();
    }
    do {
        func_80073FE0();
        func_800AB378(0x80);
        func_800A6924();
        func_80074700();
    } while (!(D_800C3900 & 0x100));
    for (i = 16; i > 0; i--) {
        func_80073FE0();
        func_800AB378(i * 8);
        func_800A6924();
    }
    DrawSync(0);
    func_800320E8(D_800C3A3C);
    func_800320E8(D_800B1DF0);
    LoadImage(&rect, (u_long *)saved);
    DrawSync(0);
    func_800320E8(saved);
}

typedef struct {
    DR_MODE modes[5][2];
    SPRT sprites[5][2];
} OverlaySprites;
extern OverlaySprites D_800B0188; /* per sprite and draw buffer */

/* Set up the five 128x224 overlay sprites (8-bit pages from x 280) with
 * their draw modes in both buffers. */
void func_800ABD18(void) {
    RECT window;
    SPRT *sprite;
    s32 i;

    setRECT(&window, 0, 0, 0xFF, 0xFF);
    for (i = 0; i < 5; i++) {
        SetDrawMode(&D_800B0188.modes[i][0], 0, 0, GetTPage(1, 0, 0x280 + i * 0x40, 0), &window);
        SetDrawMode(&D_800B0188.modes[i][1], 0, 0, GetTPage(1, 0, 0x280 + i * 0x40, 0), &window);
        sprite = &D_800B0188.sprites[i][0];
        SetSprt(sprite);
        sprite->x0 = i << 7;
        setRGB0(sprite, 0x80, 0x80, 0x80);
        sprite->y0 = 0;
        sprite->u0 = 0;
        sprite->v0 = 0;
        sprite->w = 0x80;
        sprite->h = 0xE0;
        SetSemiTrans(sprite, 0);
        sprite->clut = GetClut(0, 0xE8);
        D_800B0188.sprites[i][1] = *sprite;
    }
}

/* Link the five overlay sprites and their draw modes of the current
 * buffer into the overlay ordering table. */
void func_800ABEC8(void) {
    s32 i;

    if (D_800ADB54 != 0) {
        for (i = 0; i < 5; i++) {
            addPrim(&D_800C426C->overlay_ot[0], &D_800B0188.sprites[i][D_800ADB08]);
            addPrim(&D_800C426C->overlay_ot[0], &D_800B0188.modes[i][D_800ADB08]);
        }
    }
}

#include "field_glyph.h"

extern s32 D_800AF780; /* file 0xab bytes left */

/* The glyph of the two-byte code at `text`: an index into the overlay font
 * (*own 1) or the ROM font bitmap (*own 0). */
s32 func_800ABFDC(u8 *text, s32 *own) {
    u16 code;

    code = text[1] | (text[0] << 8);
    if ((u16)(code - GLYPH_OWN_FIRST) < GLYPH_OWN_COUNT) {
        *own = 1;
        return code - GLYPH_OWN_FIRST;
    }
    *own = 0;
    return Krom2RawAdd(code);
}

/* Expand a 16x15 1-bit ROM glyph into an 8-bit glyph cell (0xff set, 0
 * clear); a -1 glyph fills the cell. */
void func_800AC03C(u8 *cell, u16 *rows) {
    s32 i;
    s32 bit;

    if (rows == (u16 *)-1) {
        for (i = 0; i < GLYPH_CELL_BYTES; i++) {
            *cell++ = 0xFF;
        }
        return;
    }
    for (i = 0; i < 15; i++) {
        for (bit = 7; bit >= 0; bit--) {
            *cell++ = (*rows >> bit) & 1 ? 0xFF : 0;
        }
        for (bit = 15; bit >= 8; bit--) {
            *cell++ = (*rows >> bit) & 1 ? 0xFF : 0;
        }
        *cell++ = 0;
        *cell++ = 0;
        rows++;
    }
}

/* Draw the next text line (ending at CR, up to 28 glyphs) into VRAM row
 * `row` at x `left`, blanking the rest; return the text after it. */
u8 *func_800AC0F0(u8 *text, s32 left, s32 row) {
    GlyphCell cell;
    RECT dest;
    RECT source;
    u8 line[0x40];
    s32 own;
    s32 used;
    s32 count;
    s32 glyph;
    s32 x;
    s32 i;

    dest.w = 9;
    dest.h = 16;
    dest.y = row * 16;
    for (i = 0; i < 0x40; i++) {
        line[i] = text[i];
    }
    for (i = GLYPH_CELL_BYTES - 1; i >= 0; i--) {
        cell.pixels[0][i] = 0;
    }
    used = 0;
    if (D_800AF780 <= 0) {
        count = 0;
    } else {
        for (count = 0, x = left; count < GLYPH_LINE_CELLS; count++, x += 9) {
            if (line[used] == '\r') {
                used++;
                break;
            }
            glyph = func_800ABFDC(&line[used], &own);
            used += 2;
            if (own == 1) {
                source.w = 9;
                source.h = 16;
                source.x = glyph % 7 * 9 + 0x380;
                source.y = glyph / 7 * 16 + 0x100;
                MoveImage(&source, x, row * 16);
            } else {
                func_800AC03C(cell.pixels[0], (u16 *)glyph);
                dest.x = x;
                LoadImage(&dest, (u_long *)&cell);
            }
            DrawSync(0);
        }
    }
    for (i = GLYPH_CELL_BYTES - 1; i >= 0; i--) {
        cell.pixels[0][i] = 0;
    }
    for (; count < GLYPH_LINE_CELLS; count++) {
        dest.x = count * 9 + left;
        LoadImage(&dest, (u_long *)&cell);
        DrawSync(0);
    }
    D_800AF780 -= used;
    return text + used;
}

extern s32 func_80028738(s32 file);
extern void *D_800AF76C; /* file 0xab */
extern void *D_800AF784; /* file 0xac */

/* Load files 0xab and 0xac. */
void func_800AC308(void) {
    func_80028470(4, 0);
    D_800AF780 = func_80028738(0xAB);
    D_800AF76C = func_80031BDC(func_800288EC(0xAB), 1);
    func_800295D8(0xAB, D_800AF76C, 0, 0x80);
    func_80028A60(0);
    D_800AF784 = func_80031BDC(func_800288EC(0xAC), 1);
    func_800295D8(0xAC, D_800AF784, 0, 0x80);
    func_80028A60(0);
}

/* Set up the text roll: the white top and bottom fades (640 wide, 24
 * tall at 0 and c8) and the 16 lines of sprites. */
void func_800AC3AC(void) {
    SPRT *sprite;
    s32 i;
    s32 k;

    SetPolyGT4(&D_800AF788[0][0]);
    SetPolyGT4(&D_800AF788[1][0]);
    D_800AF788[0][0].r0 = D_800AF788[0][0].g0 = D_800AF788[0][0].b0 = 0xFF;
    D_800AF788[0][0].r1 = D_800AF788[0][0].g1 = D_800AF788[0][0].b1 = 0xFF;
    D_800AF788[1][0].r2 = D_800AF788[1][0].g2 = D_800AF788[1][0].b2 = 0xFF;
    D_800AF788[1][0].r3 = D_800AF788[1][0].g3 = D_800AF788[1][0].b3 = 0xFF;
    D_800AF788[0][0].r2 = D_800AF788[0][0].g2 = D_800AF788[0][0].b2 = 0;
    D_800AF788[0][0].r3 = D_800AF788[0][0].g3 = D_800AF788[0][0].b3 = 0;
    D_800AF788[1][0].r0 = D_800AF788[1][0].g0 = D_800AF788[1][0].b0 = 0;
    D_800AF788[1][0].r1 = D_800AF788[1][0].g1 = D_800AF788[1][0].b1 = 0;
    setXY4(&D_800AF788[0][0], 0, 0, 0x280, 0, 0, 0x18, 0x280, 0x18);
    setXY4(&D_800AF788[1][0], 0, 0xC8, 0x280, 0xC8, 0, 0xE0, 0x280, 0xE0);
    D_800AF788[0][0].u0 = 0;
    D_800AF788[0][0].v0 = 0;
    D_800AF788[0][0].u1 = 2;
    D_800AF788[0][0].v1 = 0;
    D_800AF788[0][0].u2 = 0;
    D_800AF788[0][0].v2 = 2;
    D_800AF788[0][0].u3 = 2;
    D_800AF788[0][0].v3 = 2;
    setUV4(&D_800AF788[1][0], 0, 0, 2, 0, 0, 2, 2, 2);
    D_800AF788[0][0].tpage = GetTPage(1, 2, 0x3C0, 0x100);
    D_800AF788[1][0].tpage = GetTPage(1, 2, 0x3C0, 0x100);
    D_800AF788[0][0].clut = GetClut(0, 0x1FF);
    D_800AF788[1][0].clut = GetClut(0, 0x1FF);
    SetSemiTrans(&D_800AF788[0][0], 1);
    SetSemiTrans(&D_800AF788[1][0], 1);
    D_800AF788[0][1] = D_800AF788[0][0];
    D_800AF788[1][1] = D_800AF788[1][0];
    D_800AF770 = func_80031BDC(16 * sizeof(TextRollLine), 1);
    for (i = 0; i < 16; i++) {
        sprite = &D_800AF770[i].sprites[0][0];
        SetSprt(sprite);
        setRGB0(sprite, 0x80, 0x80, 0x80);
        SetSemiTrans(sprite, 0);
        sprite->clut = GetClut(0, 0x1FF);
        sprite->u0 = 0;
        sprite->v0 = i * 16;
        sprite->w = 0x80;
        sprite->h = 0x10;
        sprite->x0 = 0x40;
        sprite->y0 = i * 16;
        sprite[4] = *sprite;
        sprite[1] = *sprite;
        sprite[5] = *sprite;
        sprite[2] = *sprite;
        sprite[6] = *sprite;
        sprite[3] = *sprite;
        sprite[7] = *sprite;
        for (k = 0; k < 4; k++) {
            D_800AF770[i].sprites[0][k].x0 = 0x40 + k * 0x80;
            D_800AF770[i].sprites[1][k].x0 = 0x40 + k * 0x80;
            SetDrawMode(&D_800AF770[i].modes[0][k], 0, 0, GetTPage(1, 0, 0x300 + k * 0x40, 0), NULL);
            SetDrawMode(&D_800AF770[i].modes[1][k], 0, 0, GetTPage(1, 0, 0x300 + k * 0x40, 0), NULL);
        }
    }
}

/* Draw the text roll: its two fades, then each line scrolled up a pixel
 * from the other buffer's position. */
void func_800AC99C(void) {
    s32 y;
    s32 i;
    s32 k;

    addPrim(&D_800C426C->overlay_ot[0], &D_800AF788[1][D_800ADB08]);
    addPrim(&D_800C426C->overlay_ot[0], &D_800AF788[0][D_800ADB08]);
    for (i = 0; i < 16; i++) {
        y = (D_800AF770[i].sprites[(D_800ADB08 + 1) & 1][0].y0 - 1) & 0xFF;
        D_800AF770[i].sprites[D_800ADB08][0].y0 = y;
        D_800AF770[i].sprites[D_800ADB08][1].y0 = y;
        D_800AF770[i].sprites[D_800ADB08][2].y0 = y;
        D_800AF770[i].sprites[D_800ADB08][3].y0 = y;
        for (k = 0; k < 4; k++) {
            addPrim(&D_800C426C->overlay_ot[0], &D_800AF770[i].sprites[D_800ADB08][k]);
            addPrim(&D_800C426C->overlay_ot[0], &D_800AF770[i].modes[D_800ADB08][k]);
        }
    }
}

void func_80070340(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

/* Upload file 0xac's image to (380, 100) with its CLUT at (0, 1ff), then
 * fill the 64x4 VRAM block at (3c0, 100) with ones. */
void func_800ACB90(void) {
    RECT rect;
    u32 *pixels;
    s32 i;

    func_80070340(D_800AF784, 0x380, 0x100, 0, 0x1FF, 0, 0);
    DrawSync(0);
    func_800320E8(D_800AF784);
    pixels = func_80031BDC(0x200, 1);
    for (i = 0; i < 0x80; i++) {
        pixels[i] = -1;
    }
    rect.x = 0x3C0;
    rect.y = 0x100;
    rect.w = 0x40;
    rect.h = 4;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_800320E8(pixels);
}

extern u8 *D_800AF774; /* sequence text position */
extern s32 D_800AF778;
extern s32 D_800AF77C;
void func_800AC3AC(void);

/* Start the sequence from file 0xab when enabled. */
void func_800ACC58(void) {
    RECT unused; /* the original frame reserves an unused 8-byte local */

    if (D_8004F300 != 0) {
        func_800AC308();
        func_800AC3AC();
        D_800AF77C = 0;
        D_800AF778 = 15;
        D_800AF774 = D_800AF76C;
    }
}


/* Release the sequence buffers when enabled. */
void func_800ACCB0(void) {
    if (D_8004F300 != 0) {
        func_800320E8(D_800AF76C);
        func_800320E8(D_800AF770);
    }
}


/* Advance the sequence one frame; every 16th frame decode the next step. */
void func_800ACCF4(void) {
    if (D_8004F300 != 0) {
        if ((D_800AF77C & 0xF) == 0) {
            D_800AF774 = func_800AC0F0(D_800AF774, 0x300, D_800AF778 & 0xF);
            D_800AF778++;
        }
        D_800AF77C++;
    }
}

/* The signed halfword operand at byte `offset` from the working PC. */
s32 func_800ACD7C(s32 offset) {
    u8 *code;

    code = &D_800ADC00[D_800B0078->pc + offset];
    return (s16)(code[0] + (code[1] << 8));
}

/* The raw halfword operand at byte `offset` from the working PC. */
s32 func_800ACDB8(s32 offset) {
    u8 *code;

    code = &D_800ADC00[D_800B0078->pc + offset];
    return code[0] | (code[1] << 8);
}

/* The operand at `offset`: bit 15 marks a 15-bit immediate, else it names
 * an event variable. */
s32 func_800ACDEC(s32 offset) {
    s32 operand;

    operand = func_800ACDB8(offset);
    if (operand & 0x8000) {
        return operand & 0x7FFF;
    }
    return func_800A3018(operand & 0xFFFF);
}

extern s16 D_8006BE2C[3];
void func_800AD978(s32 mode);

/* Mark which party slots changed character, then refresh the party. */
void func_800ACE24(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8006BE2C[i] == D_8005A39C->unk22B1[i]) {
            D_8006BE2C[i] = 0;
        } else {
            D_8006BE2C[i] = 1;
        }
    }
    func_800AD978(0);
}

/* Flag the party slots whose members are present for 800ad978 (mode 1),
 * by the first slot's state. */
void func_800ACE90(void) {
    s32 i;

    D_8006BE2C[0] = D_8006BE2C[1] = D_8006BE2C[2] = 0;
    if (D_8005A39C->unk22B1[0] == 0) {
        for (i = 0; i < 3; i++) {
            if (D_8005A39C->unk22B1[i] == 0 && func_8001ACF0(D_80062590[i]) != 0xFF) {
                D_8006BE2C[i] = 1;
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (D_8005A39C->unk22B1[i] == 1 && func_8001ACF0(D_80062590[i]) != 0xFF) {
                D_8006BE2C[i] = 1;
            }
        }
    }
    func_800AD978(1);
}

#include "field_party.h"

#define DESCRIPTOR(index) (&D_800AF880.components.descriptors[index])

/* Return party slot `slot` to its member: swap the models back, hand the
 * stand-in's heading over, and restart both animations. */
void func_800ACFD0(s32 slot) {
    FieldModel *stand_in;
    FieldModel *model;

    D_8005A39C->unk22B1[slot] = 0;
    stand_in = DESCRIPTOR(D_8006F990[slot])->model;
    model = DESCRIPTOR(D_8005A444[slot])->model;
    DESCRIPTOR(D_8005A444[slot])->model = stand_in;
    DESCRIPTOR(D_8006F990[slot])->model = model;
    DESCRIPTOR(D_8006F990[slot])->flags = (DESCRIPTOR(D_8006F990[slot])->flags & 0xF07F) | 0x200;
    DESCRIPTOR(D_8006F990[slot])->flags &= 0xFFDF;
    DESCRIPTOR(D_8006F990[slot])->actor->flags &= ~1;
    func_800A0524(D_8006F990[slot], D_8005A444[slot]);
    DESCRIPTOR(D_8005A444[slot])->model->position[0] = DESCRIPTOR(D_8005A444[slot])->actor->position[0];
    DESCRIPTOR(D_8005A444[slot])->model->position[1] = DESCRIPTOR(D_8005A444[slot])->actor->position[1];
    DESCRIPTOR(D_8005A444[slot])->model->position[2] = DESCRIPTOR(D_8005A444[slot])->actor->position[2];
    DESCRIPTOR(D_8005A444[slot])->actor->flags |= 0x400;
    DESCRIPTOR(D_8005A444[slot])->actor->flags &= ~0x300;
    DESCRIPTOR(D_8006F990[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(D_8005A444[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(D_8006F990[slot])->actor->unk108 = DESCRIPTOR(D_8005A444[slot])->actor->unk108;
    DESCRIPTOR(D_8006F990[slot])->actor->heading_goal = DESCRIPTOR(D_8005A444[slot])->actor->heading_goal;
    DESCRIPTOR(D_8006F990[slot])->actor->unkE8 = DESCRIPTOR(D_8006F990[slot])->actor->unkE6;
    DESCRIPTOR(D_8005A444[slot])->actor->unkE8 = DESCRIPTOR(D_8005A444[slot])->actor->unkE6;
    func_800821F4(DESCRIPTOR(D_8006F990[slot])->model, 6, DESCRIPTOR(D_8006F990[slot]));
    func_800821F4(DESCRIPTOR(D_8005A444[slot])->model, DESCRIPTOR(D_8005A444[slot])->actor->unkE6,
                  DESCRIPTOR(D_8005A444[slot]));
    func_8009FEE4(slot);
    func_800A98E8(D_8006F990[slot], 0);
}

/* Put the current actor in for party slot `slot`: swap its model with the
 * member's, mark the slot taken, and restart both animations. */
void func_800AD4D4(s32 slot) {
    FieldModel *model;

    model = DESCRIPTOR(D_800AFD1C)->model;
    DESCRIPTOR(D_800AFD1C)->model = DESCRIPTOR(D_8005A444[slot])->model;
    DESCRIPTOR(D_8005A444[slot])->model = model;
    DESCRIPTOR(D_8005A444[slot])->model->position[0] = DESCRIPTOR(D_8005A444[slot])->actor->position[0];
    DESCRIPTOR(D_8005A444[slot])->model->position[1] = DESCRIPTOR(D_8005A444[slot])->actor->position[1];
    DESCRIPTOR(D_8005A444[slot])->model->position[2] = DESCRIPTOR(D_8005A444[slot])->actor->position[2];
    DESCRIPTOR(D_800AFD1C)->flags |= 0x20;
    DESCRIPTOR(D_8005A444[slot])->actor->flags |= 0x200;
    DESCRIPTOR(D_8005A444[slot])->actor->flags &= ~0x500;
    D_8005A39C->unk22B1[slot] = 1;
    DESCRIPTOR(D_8006F990[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(D_8005A444[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(D_8006F990[slot])->actor->unkE8 = DESCRIPTOR(D_8006F990[slot])->actor->unkE6;
    DESCRIPTOR(D_8005A444[slot])->actor->unkE8 = DESCRIPTOR(D_8005A444[slot])->actor->unkE6;
    func_800821F4(DESCRIPTOR(D_8006F990[slot])->model, DESCRIPTOR(D_8006F990[slot])->actor->unkE6,
                  DESCRIPTOR(D_8006F990[slot]));
    func_800821F4(DESCRIPTOR(D_8005A444[slot])->model, DESCRIPTOR(D_8005A444[slot])->actor->unkE6,
                  DESCRIPTOR(D_8005A444[slot]));
    func_800A98E8(D_8006F990[slot], 0);
    func_8009FEE4(slot);
}

/* For party members in slot state 1, set actor flag 0x200 and clear
 * 0x500. */
void func_800AD898(void) {
    s32 i;

    if (D_800B2078.unk2268 != 0) {
        for (i = 0; i < 3; i++) {
            if (D_8005A444[i] != 0xFF && D_8005A39C->unk22B1[i] == 1) {
                D_800AF880.components.descriptors[D_8005A444[i]].actor->flags |= 0x200;
                D_800AF880.components.descriptors[D_8005A444[i]].actor->flags &= ~0x500;
            }
        }
    }
}

/* For each party member flagged in 8006be2c, run 800ad4d4 when `mode`
 * disagrees with its slot state (+22b1) and 800acfd0 otherwise. */
void func_800AD978(s32 mode) {
    s32 i;

    if (D_800B2078.unk2268 != 0) {
        for (i = 0; i < 3; i++) {
            if (D_8005A444[i] != 0xFF && D_8006BE2C[i] == 1) {
                D_800AFD1C = D_8006F990[i];
                if ((D_8005A39C->unk22B1[i] == 0 && mode != 0) || (D_8005A39C->unk22B1[i] != 0 && mode == 0)) {
                    func_800AD4D4(i);
                } else {
                    func_800ACFD0(i);
                }
            }
        }
    }
}
