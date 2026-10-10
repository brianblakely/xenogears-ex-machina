/* Field unit 800A9274-end: particle effects, model instance bounds,
 * sprites, the map picture, the overlay sprites and text glyphs, the text
 * roll and sequence, event operands and party slot swaps.
 *
 * Its rodata starts 0 mod 8 after 800a5c40's 4-mod-8 table, at 0x2d8 (the
 * first rodata of 800a9688) or at 0x2e8 (800ab748's jump table); this file
 * takes 0x2d8 with the text from 800a9274, where the particle effect slots
 * start (see field_screen.c). */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "field/monitor.h"
#include "field.h"
#include "field_camera.h"
#include "field_draw.h"
#include "field_effect.h"
#include "field_event.h"
#include "field_glyph.h"
#include "field_layer.h"
#include "field_mode.h"
#include "field_motion.h"
#include "field_pad.h"
#include "field_party.h"
#include "field_picture.h"
#include "field_screen.h"

/* Particle spawn offset per view octant. */
u8 field_effect_octant_offset_table[8] = {2, 3, 4, 5, 6, 7, 0, 1}; /* 800AF474 */

/* The picture table (field_picture.h): per picture its map, the words for
 * 800c3914 and 800c3a18, the file (0x7fb +), the item shown while held, the
 * position and whether the flagged pieces are added; ended by map 0xffff. */
s32 field_picture_table[10 * 8 + 1] = { /* 800AF47C */
    0xEF,  0xA8B, 0xA72, 3,   0x61, 0xA0, 0x70, 0,
    0xF0,  0xA8B, 0xA72, 4,   0x61, 0xA8, 0x87, 0,
    0xF1,  0xA8B, 0xA72, 5,   0x61, 0x98, 0x7C, 0,
    0x2C2, 0x374, 0x342, 6,   0x62, 0x98, 0x7A, 1,
    0x2BF, 0x3EB, 0x3E7, 8,   0x62, 0x87, 0x7B, 0,
    0x2C3, 0x569, 0x560, 0xA, 0x62, 0xBC, 0x50, 0,
    0x2C4, 0x425, 0x425, 9,   0x62, 0x8D, 0x92, 0,
    0x1BD, 0xB25, 0xAE9, 0xB, 0x63, 0xF3, 0x46, 0,
    0x1BE, 0xB25, 0xAE9, 0xB, 0x63, 0xF3, 0x46, 0,
    0x1BF, 0xB25, 0xAE9, 0xB, 0x63, 0x51, 0x6D, 0,
    0xFFFF,
};

/* The pieces of file 0x802's 320-wide image. */
RECT field_picture_flagged_piece_rects[5] = { /* 800AF5C0 */
    {0x98, 0x46, 0x20, 0x20}, {0xA8, 0x8C, 0x2C, 0x2C}, {0x78, 0x48, 0x18, 0x1C},
    {0x70, 0x5C, 0x24, 0x20}, {0xBC, 0x32, 0x2C, 0x2C},
};

/* 800A9274: Free all 64 effect slots. */
void field_effect_clear_slots(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        field_effect_slot_states[i] = 0;
        field_effect_slot_owners[i] = -1;
    }
}

/* 800A92AC: Release effect slot `slot` and its particles. */
void field_effect_release_slot(s32 slot) {
    Record78 *emitter;
    s32 i;

    if (field_effect_slot_states[slot] == 1) {
        emitter = field_effect_slot_emitters[slot];
        for (i = 0; i < 8; i++) {
            if (emitter->count != 0) {
                heap_free(emitter->particles);
            }
            emitter++;
        }
        heap_free(field_effect_slot_emitters[slot]);
    }
    field_effect_slot_states[slot] = 0;
    field_effect_slot_owners[slot] = -1;
}

/* 800A9374: Stop the emitters of effect slot `slot`. */
void field_effect_stop_slot_emitters(s32 slot) {
    Record78 *emitter;
    s32 i;

    if (field_effect_slot_states[slot] == 1) {
        emitter = field_effect_slot_emitters[slot];
        for (i = 0; i < 8; i++) {
            if (emitter->count != 0) {
                emitter->unk04 = 0;
            }
            emitter++;
        }
    }
}

/* 800A93CC: Stop the emitters of effect slot `slot` and release their particles. */
void field_effect_stop_slot_and_particles(s32 slot) {
    Record78 *emitter;
    s32 i;
    s32 j;
    Particle *particle;

    if (field_effect_slot_states[slot] == 1) {
        emitter = field_effect_slot_emitters[slot];
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

/* 800A9460: Release all effect slots. */
void field_effect_release_all_slots(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        field_effect_release_slot(i);
    }
    field_sync_draw_and_vsync();
}

/* 800A94A4: Reset the eight particle emitters at 800b02cc with parameter `value`. */
void field_effect_reset_templates(s32 value) {
    s32 i;
    s32 j;

    field_effect_edited_template = 0;
    for (i = 0; i < 8; i++) {
        field_effect_templates[i].unk52 = value;
        field_effect_templates[i].unk00 = 0;
        field_effect_templates[i].unk02 = 0;
        field_effect_templates[i].unk04 = 0x80;
        field_effect_templates[i].count = 0;
        setVector(&field_effect_templates[i].unk0C, 0, 0, 0);
        setVector(&field_effect_templates[i].unk14, 0, -1000, 0);
        field_effect_templates[i].unk08 = 0x8000;
        field_effect_templates[i].unk50 = 0x800;
        field_effect_templates[i].unk24 = 1;
        setVector(&field_effect_templates[i].unk1C, 0, 0, 0);
        field_effect_templates[i].unk28 = 0x100;
        field_effect_templates[i].unk58 = 0x1C;
        field_effect_templates[i].unk26 = 0;
        field_effect_templates[i].flags = 0;
        field_effect_templates[i].unk76 = 0;
        field_effect_templates[i].unk56 = 1;
        field_effect_templates[i].unk54 = 0;
        setVector(&field_effect_templates[i].unk5A, 0x1C8, 0x1C8, 0x1C8);
        setVector(&field_effect_templates[i].unk62, 0x20, 0x20, 0x20);
        field_effect_templates[i].unk6A = 0x80;
        field_effect_templates[i].unk6B = 0x20;
        field_effect_templates[i].unk6C = 0;
        field_effect_templates[i].unk6E = -4;
        field_effect_templates[i].unk6F = -1;
        field_effect_templates[i].unk70 = 0;
        for (j = 0; j < 8; j++) {
            field_effect_templates[i].unk30[j][0] = 0;
            field_effect_templates[i].unk30[j][1] = 0;
        }
    }
}

/* 800A9688: Run the effect slots for a frame: count down emitter delays, spawn and
 * draw particles, count down emitter lifetimes (7fff lasts), and release
 * the slots with nothing left alive. */
void field_effect_update_slots(void) {
    MATRIX view;
    s32 spawned;
    Record78 *emitter;
    s32 alive;
    s32 slot;
    s32 i;
    s32 j;

    if (field_vram_column_saved != 0) {
        return;
    }
    view = field_view.scaled_world;
    for (slot = 0; slot < 64; slot++) {
        alive = 0;
        if (field_effect_slot_states[slot] == 1) {
            emitter = field_effect_slot_emitters[slot];
            for (i = 0; i < 8; i++) {
                spawned = 0;
                if (emitter->count != 0) {
                    if (emitter->unk02 == 0) {
                        for (j = 0; j < emitter->count; j++) {
                            if (emitter->particles[j].unk00 == 0) {
                                if (emitter->unk04 != 0) {
                                    field_effect_spawn_particle(emitter, &emitter->particles[j], &spawned);
                                    field_effect_step_particle(emitter, &emitter->particles[j], &view);
                                    alive = 1;
                                }
                            } else {
                                field_effect_step_particle(emitter, &emitter->particles[j], &view);
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
                field_effect_release_slot(slot);
            }
        }
    }
    if (field_monitor_absent == 0) {
        field_debug_mark_cpu_time("PARTICLE  ");
    }
}

/* 800A987C: A random number in 0..range. */
s32 field_effect_get_random(s32 range) {
    return (rand() * range + 1) >> 15;
}

/* 800A98B4: The first free effect slot, or -1. */
s32 field_effect_find_free_slot(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (field_effect_slot_states[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* 800A98E8: Stop the effect slots owned by `owner`; with `release` also release
 * their particles. */
void field_effect_stop_by_owner(s32 owner, s32 release) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (field_effect_slot_owners[i] == owner) {
            if (release == 0) {
                field_effect_slot_emitters[i]->unk04 = 0;
                field_effect_slot_emitters[i]->unk02 = 0;
                field_effect_stop_slot_emitters(i);
            } else {
                field_effect_slot_emitters[i]->unk04 = 0;
                field_effect_slot_emitters[i]->unk02 = 0;
                field_effect_stop_slot_and_particles(i);
            }
        }
    }
}

typedef struct {
    Record78 emitters[8];
} EmitterSet;

/* 800A99A8: Start an effect for `owner` in a free slot: copy the eight template
 * emitters and allocate and set up their particles. -1 when no slot. */
s32 field_effect_start(s32 owner) {
    Record78 *emitters;
    Record78 *emitter;
    s32 slot;
    s32 j;

    slot = field_effect_find_free_slot();
    if (slot == -1) {
        return -1;
    }
    heap_select_owner_tag(8, 0);
    field_unread_effect_last_owner = owner;
    field_effect_slot_states[slot] = 1;
    field_effect_slot_owners[slot] = owner;
    emitters = heap_alloc(0x3C0, 0);
    field_effect_slot_emitters[slot] = emitters;
    *(EmitterSet *)emitters = *(EmitterSet *)field_effect_templates;
    emitter = emitters;
    for (slot = 0; slot < 8; slot++) {
        if (emitter->count != 0) {
            emitter->particles = heap_alloc(emitter->count * sizeof(Particle), 0);
            for (j = 0; j < emitter->count; j++) {
                emitter->particles[j].unk00 = 0;
                field_effect_init_particle_quads(&emitter->particles[j], emitter->unk54, ((s16)emitter->flags >> 8) + 1 & 3);
            }
        }
        emitter++;
    }
    return 1;
}

/* 800A9B1C: `value` + `delta`, clamped to 0..255. */
s32 field_effect_add_clamped_u8(s32 value, s32 delta) {
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

/* 800A9B54: Transform a particle's quad: rotate it by `angle` at its position under
 * `view` (mode 3 scales the view by `scale` and then the quad again),
 * scale it by its size, tint it and link it at its depth (depth mode 0
 * front, 1 nearer, 2 as is, 3 farther). */
void field_effect_draw_particle(Particle *particle, MATRIX *view, s16 angle, s32 depth_mode, VECTOR *scale, s32 mode) {
    MATRIX m;
    MATRIX local;
    MATRIX scaled_view;
    SVECTOR rotation;
    VECTOR size;
    s32 otz;
    long p;

    rotation.vx = 0;
    rotation.vy = 0;
    rotation.vz = angle;
    gpu_build_rotation_matrix(&rotation, &local);
    local.t[0] = particle->position.vx >> 12;
    local.t[1] = particle->position.vy >> 12;
    local.t[2] = particle->position.vz >> 12;
    if (mode == 3) {
        scaled_view = *view;
        ScaleMatrix(&scaled_view, scale);
        CompMatrix(&scaled_view, &local, &m);
        field_matrix_copy_rotation(&m, &local);
        size.vx = particle->unk38.vx;
        size.vy = particle->unk38.vy;
        size.vz = particle->unk38.vz;
        ScaleMatrix(&m, &size);
        (particle->quads + field_draw_buffer_index)->r0 = particle->unk48[0];
        (particle->quads + field_draw_buffer_index)->g0 = particle->unk48[1];
        (particle->quads + field_draw_buffer_index)->b0 = particle->unk48[2];
        SetTransMatrix(&m);
        ScaleMatrix(&m, scale);
    } else {
        CompMatrix(view, &local, &m);
        field_matrix_copy_rotation(&m, &local);
        size.vx = particle->unk38.vx;
        size.vy = particle->unk38.vy;
        size.vz = particle->unk38.vz;
        ScaleMatrix(&m, &size);
        (particle->quads + field_draw_buffer_index)->r0 = particle->unk48[0];
        (particle->quads + field_draw_buffer_index)->g0 = particle->unk48[1];
        (particle->quads + field_draw_buffer_index)->b0 = particle->unk48[2];
        SetTransMatrix(&m);
    }
    SetRotMatrix(&m);
    otz = RotAverage4(&particle->corners[0], &particle->corners[1], &particle->corners[2], &particle->corners[3],
                      (long *)&particle->quads[field_draw_buffer_index].x0, (long *)&particle->quads[field_draw_buffer_index].x1,
                      (long *)&particle->quads[field_draw_buffer_index].x2, (long *)&particle->quads[field_draw_buffer_index].x3, &p, &p) >>
          model_ot_depth_shift;
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
        addPrim(&field_current_draw_block->ot[p], &particle->quads[field_draw_buffer_index]);
    }
}

/* 800A9F18: Step a particle: while delayed count down and at launch place it and
 * its velocity in the emitter's frame (0 owner-facing, 1 801e module, 2
 * owner's transform, 3 owner-facing and scaled); afterwards move it,
 * fade its colour, draw it and count its life down. */
void field_effect_step_particle(Record78 *emitter, Particle *particle, MATRIX *view) {
    VECTOR v;
    SVECTOR sv;
    MATRIX m;
    MATRIX camera;
    VECTOR origin;
    VECTOR up;
    VECTOR rotated;
    VECTOR scale;
    long flag;
    s32 scaled;

    if (particle->unk02 != 0) {
        if (--particle->unk02 == 0) {
            m.t[0] = m.t[1] = m.t[2] = 0;
            scaled = 0;
            switch ((emitter->flags >> 4) & 3) {
            case 3:
                sv.vx = 0;
                sv.vy = field_view.components.descriptors[emitter->unk52].actor->unk108;
                sv.vz = 0;
                gpu_build_rotation_matrix(&sv, &m);
                origin.vx = WHOLE(field_view.components.descriptors[emitter->unk52].actor->position[0]);
                origin.vy = WHOLE(field_view.components.descriptors[emitter->unk52].actor->position[1]);
                origin.vz = WHOLE(field_view.components.descriptors[emitter->unk52].actor->position[2]);
                emitter->unk50 = field_view.components.descriptors[emitter->unk52].actor->scale[0];
                scaled = 1;
                break;
            case 0:
                sv.vx = 0;
                sv.vy = field_view.components.descriptors[emitter->unk52].actor->unk108;
                sv.vz = 0;
                gpu_build_rotation_matrix(&sv, &m);
                origin.vx = WHOLE(field_view.components.descriptors[emitter->unk52].actor->position[0]);
                origin.vy = WHOLE(field_view.components.descriptors[emitter->unk52].actor->position[1]);
                origin.vz = WHOLE(field_view.components.descriptors[emitter->unk52].actor->position[2]);
                emitter->unk50 = 0x1000;
                break;
            case 1:
                gear_model_get_node_matrix(&m, &camera, emitter->unk72, emitter->unk74);
                SetRotMatrix(&m);
                SetTransMatrix(&m);
                sv.vx = emitter->unk0C.vx;
                sv.vy = emitter->unk0C.vy;
                sv.vz = emitter->unk0C.vz;
                RotTrans(&sv, &origin, &flag);
                emitter->unk50 = 0x1000;
                break;
            case 2:
                m = field_view.components.descriptors[emitter->unk52].transform;
                SetRotMatrix(&m);
                SetTransMatrix(&m);
                sv.vx = emitter->unk0C.vx;
                sv.vy = emitter->unk0C.vy;
                sv.vz = emitter->unk0C.vz;
                RotTrans(&sv, &origin, &flag);
                emitter->unk50 = 0x1000;
                break;
            }
            m.t[0] = m.t[1] = m.t[2] = 0;
            SetRotMatrix(&m);
            SetTransMatrix(&m);
            sv.vx = particle->velocity.vx;
            sv.vy = particle->velocity.vy;
            sv.vz = particle->velocity.vz;
            libgte_rotate_svector(&sv, &v);
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
            RotTrans(&sv, &v, &flag);
            if (scaled == 1) {
                sv.vz = 0;
                sv.vx = field_camera_pitch - 0x400;
                sv.vy = -field_view.view_angle;
                RotMatrix(&sv, &camera);
                SetRotMatrix(&camera);
                SetTransMatrix(&camera);
                up.vx = 0;
                up.vz = 0;
                up.vy = v.vy;
                libgte_rotate_vector(&up, &rotated);
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
        particle->unk48[0] = field_effect_add_clamped_u8(particle->unk48[0], particle->unk4C[0]);
        particle->unk48[1] = field_effect_add_clamped_u8(particle->unk48[1], particle->unk4C[1]);
        particle->unk48[2] = field_effect_add_clamped_u8(particle->unk48[2], particle->unk4C[2]);
        scale.vx = emitter->unk50;
        scale.vy = emitter->unk50;
        scale.vz = emitter->unk50;
        if (particle->unk04 != 1) {
            field_effect_draw_particle(particle, view, particle->angle, (emitter->flags >> 1) & 3, &scale,
                          (emitter->flags >> 4) & 3);
        }
        if (--particle->unk04 == 0) {
            particle->unk00 = 0;
        }
    }
}

/* 800AA6B4: Spawn `particle` of `emitter`: its delay after the previous spawn, a
 * random start within the spawn radius around the emitter (offset by the
 * owner's view octant) and a velocity toward a random point of the
 * target spread. */
void field_effect_spawn_particle(Record78 *emitter, Particle *particle, s32 *spawned) {
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
        radius = field_effect_get_random(emitter->unk26);
    } else {
        radius = emitter->unk26;
    }
    angle = field_effect_get_random(0xFFF);
    start.vx = gpu_get_cos(angle) * radius >> 12;
    if (!(emitter->flags & 0x40)) {
        start.vz = gpu_get_sin(angle) * radius >> 12;
    } else {
        start.vz = 0;
    }
    facing = (field_view.view_angle + field_view.components.descriptors[emitter->unk52].actor->unk108) & 0xFFF;
    k = field_effect_octant_offset_table[facing >> 9];
    start.vx += emitter->unk0C.vx + emitter->unk30[k][0];
    start.vz += emitter->unk0C.vz + emitter->unk30[k][1];
    start.vy = emitter->unk0C.vy;
    particle->position.vx = start.vx;
    particle->position.vz = start.vz;
    particle->position.vy = start.vy;
    radius = field_effect_get_random(emitter->unk28);
    end.vx = emitter->unk14.vx + (gpu_get_cos(angle) * radius >> 12);
    end.vz = emitter->unk14.vz + (gpu_get_sin(angle) * radius >> 12);
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

/* 800AA9DC: Set an instance's bounding centre and radius from its mesh bounds. */
void field_instance_set_bounds(FieldInstance *instance) {
    s32 min_x;
    s32 min_y;
    s32 min_z;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 size;

    dx = size = instance->mesh->box_max.vx - (min_x = instance->mesh->box_min.vx);
    dy = instance->mesh->box_max.vy - (min_y = instance->mesh->box_min.vy);
    dz = instance->mesh->box_max.vz - (min_z = instance->mesh->box_min.vz);
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

/* 800AAA74: 0 when an instance's bounding square (its radius around its centre) is
 * on screen, else -1. */
s32 field_instance_is_off_screen(FieldInstance *instance) {
    VECTOR position;
    SVECTOR corner;
    long flag;
    long sxy;
    long depth;
    s32 radius;
    s32 top;
    s32 left;
    s32 bottom;
    s32 right;

    RotTrans((SVECTOR *)instance->center, &position, &flag);
    field_instance_cull_matrix.t[0] = position.vx;
    field_instance_cull_matrix.t[1] = position.vy;
    field_instance_cull_matrix.t[2] = position.vz;
    SetRotMatrix(&field_instance_cull_matrix);
    SetTransMatrix(&field_instance_cull_matrix);
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
    if (top < field_model_cull_margin_y + 0xE0 && -field_model_cull_margin_y < bottom && left < field_model_cull_margin_x + 0x140 && -field_model_cull_margin_x < right) {
        return 0;
    }
    return -1;
}

/* 800AABD8: Release the sprite block. */
void field_overlay_sprite_release_all(void) {
    heap_free(field_overlay_sprites);
    DrawSync(0);
}

/* 800AAC08: Allocate the 33 sprites (the first 16x16, the rest 8x8), each with a
 * draw mode per buffer. */
void field_overlay_sprite_alloc_all(void) {
    RECT window;
    SPRT *sprite;
    SPRT *copy;
    s32 i;

    field_overlay_sprites = heap_alloc(0x840, 0);
    window.x = 0;
    window.y = 0;
    window.w = 0xFF;
    window.h = 0xFF;
    for (i = 0; i < 33; i++) {
        SetDrawMode(&field_overlay_sprites->modes[i][0], 0, 0, GetTPage(0, 0, 0x3C0, 0x100), &window);
        SetDrawMode(&field_overlay_sprites->modes[i][1], 0, 0, GetTPage(0, 0, 0x3C0, 0x140), &window);
        sprite = field_overlay_sprites->sprites[i];
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

/* 800AADC8: Set sprite `index`'s colour in both buffers. */
void field_overlay_sprite_set_color(s32 index, s32 r, s32 g, s32 b) {
    (field_overlay_sprites->sprites[index] + 0)->r0 = r;
    (field_overlay_sprites->sprites[index] + 0)->g0 = g;
    (field_overlay_sprites->sprites[index] + 0)->b0 = b;
    (field_overlay_sprites->sprites[index] + 1)->r0 = r;
    (field_overlay_sprites->sprites[index] + 1)->g0 = g;
    (field_overlay_sprites->sprites[index] + 1)->b0 = b;
}

/* 800AAE4C: Place sprite `index` at (x, y) (anchor 0: 4, 12 above-left; 1: 4, 4)
 * and link it with its draw mode into the overlay ordering table. */
void field_overlay_sprite_place(s32 index, s32 x, s32 y, s32 anchor) {
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
    field_overlay_sprites->sprites[index][field_draw_buffer_index].x0 = x;
    field_overlay_sprites->sprites[index][field_draw_buffer_index].y0 = y;
    addPrim(&field_current_draw_block->overlay_ot[0], &field_overlay_sprites->sprites[index][field_draw_buffer_index]);
    addPrim(&field_current_draw_block->overlay_ot[0], &field_overlay_sprites->modes[index][field_draw_buffer_index]);
}

/* 800AAF80: Set up the picture: four marker sprites (the first 16x16, the rest
 * 8x8) and the three 128x224 picture pieces from the 8-bit pages at
 * (300, 100). */
void field_picture_init(void) {
    RECT window;
    SPRT *sprite;
    SPRT *copy;
    POLY_FT4 *quad;
    POLY_FT4 *quad_copy;
    s32 i;

    field_picture_pieces = heap_alloc(sizeof(ScreenPieces), 0);
    field_picture_marker_sprites = heap_alloc(sizeof(PictureMarks), 0);
    setRECT(&window, 0, 0, 0xFF, 0xFF);
    for (i = 0; i < 4; i++) {
        SetDrawMode(&field_picture_marker_sprites->modes[i][0], 0, 0, GetTPage(0, 0, 0x3C0, 0x140), &window);
        SetDrawMode(&field_picture_marker_sprites->modes[i][1], 0, 0, GetTPage(0, 0, 0x3C0, 0x140), &window);
        sprite = &field_picture_marker_sprites->sprites[i][0];
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
        quad = &field_picture_pieces->quads[i][0];
        quad_copy = &field_picture_pieces->quads[i][1];
        SetPolyFT4(quad);
        quad->x0 = i << 7;
        quad->y2 = 0xDF;
        quad->x2 = i << 7;
        quad->y0 = 0;
        quad->x1 = (i << 7) + 0x80;
        quad->y1 = 0;
        quad->x3 = (i << 7) + 0x80;
        quad->y3 = 0xDF;
        setRECT(&field_picture_pieces->windows[i][0], 0, 0, 0xFF, 0xFF);
        setRECT(&field_picture_pieces->windows[i][1], 0, 0, 0xFF, 0xFF);
        SetDrawMode(&field_picture_pieces->modes[i][0], 0, 0, GetTPage(1, 0, 0x300 + i * 0x40, 0x100),
                    &field_picture_pieces->windows[i][0]);
        SetDrawMode(&field_picture_pieces->modes[i][1], 0, 0, GetTPage(1, 0, 0x300 + i * 0x40, 0x100),
                    &field_picture_pieces->windows[i][1]);
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

/* 800AB328: 0 when item `item` is held in inventory list 0, else -1. */
s32 field_inventory_is_item_not_held(s32 item) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (game_current_data->itemIds[i] == item && game_current_data->itemCounts[i] != 0) {
            return 0;
        }
    }
    return -1;
}

/* 800AB378: Draw the picture at brightness `level`: the marker at the controlled
 * actor's scaled map position, then the three picture pieces. */
void field_picture_draw(s32 level) {
    FieldActor *actor;
    s32 x;
    s32 y;
    s32 i;

    actor = field_view.components.descriptors[field_work.controlled].actor;
    x = WHOLE(actor->position[0]) * field_picture_marker_scale_x >> 16;
    y = -(WHOLE(actor->position[2]) * field_picture_marker_scale_z) >> 16;
    for (i = 0; i < 1; i++) {
        if (i == 0) {
            y -= 12;
            x -= 4;
        }
        field_picture_marker_sprites->sprites[i][field_draw_buffer_index].x0 = x + field_picture_marker_origin_x;
        field_picture_marker_sprites->sprites[i][field_draw_buffer_index].y0 = y + field_picture_marker_origin_y;
        (field_picture_marker_sprites->sprites[i] + (field_draw_buffer_index & 1))->r0 = level;
        (field_picture_marker_sprites->sprites[i] + (field_draw_buffer_index & 1))->g0 = level;
        (field_picture_marker_sprites->sprites[i] + (field_draw_buffer_index & 1))->b0 = level;
        addPrim(&field_current_draw_block->overlay_ot[0], &field_picture_marker_sprites->sprites[i][field_draw_buffer_index]);
        addPrim(&field_current_draw_block->overlay_ot[0], &field_picture_marker_sprites->modes[i][field_draw_buffer_index]);
    }
    for (i = 0; i < 3; i++) {
        (field_picture_pieces->quads[i] + (field_draw_buffer_index & 1))->r0 = level;
        (field_picture_pieces->quads[i] + (field_draw_buffer_index & 1))->g0 = level;
        (field_picture_pieces->quads[i] + (field_draw_buffer_index & 1))->b0 = level;
        addPrim(&field_current_draw_block->overlay_ot[0], &field_picture_pieces->quads[i][field_draw_buffer_index]);
        addPrim(&field_current_draw_block->overlay_ot[0], &field_picture_pieces->modes[i][field_draw_buffer_index]);
    }
}

/* 800AB748: 0 when game flag `which` (bits 3-6 of +1a16) is set; for 4, when bit 7
 * is clear; else -1. */
s32 field_picture_is_piece_flag_off(u32 which) {
    switch (which) {
    case 0:
        if (game_current_data->vars[0x73] & 8) {
            return 0;
        }
        break;
    case 1:
        if (game_current_data->vars[0x73] & 0x10) {
            return 0;
        }
        break;
    case 2:
        if (game_current_data->vars[0x73] & 0x20) {
            return 0;
        }
        break;
    case 3:
        if (game_current_data->vars[0x73] & 0x40) {
            return 0;
        }
        break;
    case 4:
        if (!(game_current_data->vars[0x73] & 0x80)) {
            return 0;
        }
        break;
    }
    return -1;
}

/* 800AB808: Upload the pieces of file 0x802's image whose game flag (800ab748) is
 * clear to the 8-bit page area at (300, 100): each piece's rows (80 words
 * per image row) are gathered into a buffer, then loaded at half the x and
 * width. A piece RECT is four signed halfwords (x, y, width, height); each
 * row advances the buffer by the width rounded down to words, rereading the
 * width after the copy.
 *
 * PsyQ's MEMORY.H (and psyq/libc.h) declares memcpy without a prototype,
 * which keeps GCC's built-in: it expands the source address as a sum, giving
 * the original's `addu a1,a1,v0`. The guard tests the zeroed row counter,
 * not the height: combine folds that compare into the branch and leaves its
 * result a stack slot nothing accesses, the original's unused sp+64. */
void field_picture_add_flagged_pieces(void) {
    TIM_IMAGE tim;
    u_long *file;
    u8 *pixels;
    u8 *row_pixels;
    s32 i;
    s32 row;
    s32 column;
    const s16 *height_base;

    file = heap_alloc(cd_get_aligned_file_size(0x802), 0);
    cd_read_file(0x802, file, 0, 0x80);
    cd_sync_reads(0);
    pixels = heap_alloc(0xF20, 0);
    OpenTIM(file);
    if (ReadTIM(&tim) != NULL) {
        height_base = &field_picture_flagged_piece_rects[0].h;
        for (i = 0; i < 5; i++) {
            if (field_picture_is_piece_flag_off(i) == -1 && tim.paddr != NULL) {
                row_pixels = pixels;
                row = 0;
                if (row < height_base[i * 4]) {
                    do {
                        column = ((s16 *)field_picture_flagged_piece_rects)[i * 4] / 4;
                        memcpy(
                            row_pixels,
                            tim.paddr + ((((s16 *)field_picture_flagged_piece_rects)[i * 4 + 1] + row) * 0x50 + column),
                            ((s16 *)field_picture_flagged_piece_rects)[i * 4 + 2]);
                        row_pixels += ((s16 *)field_picture_flagged_piece_rects)[i * 4 + 2] / 4 * 4;
                    } while (++row < ((s16 *)field_picture_flagged_piece_rects)[i * 4 + 3]);
                }
                tim.prect->x = ((s16 *)field_picture_flagged_piece_rects)[i * 4] / 2 + 0x300;
                tim.prect->y = ((s16 *)field_picture_flagged_piece_rects)[i * 4 + 1] + 0x100;
                tim.prect->w = ((s16 *)field_picture_flagged_piece_rects)[i * 4 + 2] / 2;
                tim.prect->h = height_base[i * 4];
                LoadImage(tim.prect, (u_long *)pixels);
                DrawSync(0);
            }
        }
    }
    heap_free(file);
    heap_free(pixels);
}

/* 800ABA98: Show the current map's picture while its item is held: park the VRAM
 * at (300, 100), load the picture, fade it in, hold until the button, fade
 * it out and restore the VRAM. */
void field_picture_show(void) {
    RECT rect;
    u8 *saved;
    u32 *file;
    s32 picture;
    s32 i;

    for (i = 0;; i++) {
        if (field_picture_table[i * PICTURE_WORDS + PICTURE_MAP] == 0xFFFF) {
            return;
        }
        if ((mode_field_map_id & 0x3FFF) == field_picture_table[i * PICTURE_WORDS + PICTURE_MAP]) {
            break;
        }
    }
    if (field_inventory_is_item_not_held(field_picture_table[i * PICTURE_WORDS + PICTURE_ITEM]) == -1) {
        return;
    }
    field_picture_marker_scale_x = field_picture_table[i * PICTURE_WORDS + PICTURE_UNK04];
    field_picture_marker_scale_z = field_picture_table[i * PICTURE_WORDS + PICTURE_UNK08];
    picture = field_picture_table[i * PICTURE_WORDS + PICTURE_FILE];
    field_picture_marker_origin_x = field_picture_table[i * PICTURE_WORDS + PICTURE_X];
    field_picture_marker_origin_y = field_picture_table[i * PICTURE_WORDS + PICTURE_Y];
    setRECT(&rect, 0x300, 0x100, 0xA0, 0x100);
    saved = heap_alloc(0x14000, 0);
    StoreImage(&rect, (u_long *)saved);
    DrawSync(0);
    cd_sync_reads(0);
    cd_select_directory(4, 0);
    file = heap_alloc(cd_get_aligned_file_size(picture + 0x7FB), 0);
    cd_read_file(picture + 0x7FB, file, 0, 0x80);
    cd_sync_reads(0);
    field_load_tim_at(file, 0x300, 0x100, 0, 0xF6, 0, 0);
    heap_free(file);
    if (field_picture_table[i * PICTURE_WORDS + PICTURE_PIECES] == 1) {
        field_picture_add_flagged_pieces();
    }
    field_picture_init();
    for (i = 0; i < 16; i++) {
        field_draw_swap_and_clear_ots();
        field_picture_draw(i * 8);
        field_draw_present_overlay();
    }
    do {
        field_draw_swap_and_clear_ots();
        field_picture_draw(0x80);
        field_draw_present_overlay();
        field_pad_drain_queue();
    } while (!(field_pad_port0_repeated & 0x100));
    for (i = 16; i > 0; i--) {
        field_draw_swap_and_clear_ots();
        field_picture_draw(i * 8);
        field_draw_present_overlay();
    }
    DrawSync(0);
    heap_free(field_picture_pieces);
    heap_free(field_picture_marker_sprites);
    LoadImage(&rect, (u_long *)saved);
    DrawSync(0);
    heap_free(saved);
}

/* 800ABD18: Set up the five 128x224 overlay sprites (8-bit pages from x 280) with
 * their draw modes in both buffers. */
void field_wide_overlay_init(void) {
    RECT window;
    SPRT *sprite;
    s32 i;

    setRECT(&window, 0, 0, 0xFF, 0xFF);
    for (i = 0; i < 5; i++) {
        SetDrawMode(&field_wide_overlay_sprites.modes[i][0], 0, 0, GetTPage(1, 0, 0x280 + i * 0x40, 0), &window);
        SetDrawMode(&field_wide_overlay_sprites.modes[i][1], 0, 0, GetTPage(1, 0, 0x280 + i * 0x40, 0), &window);
        sprite = &field_wide_overlay_sprites.sprites[i][0];
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
        field_wide_overlay_sprites.sprites[i][1] = *sprite;
    }
}

/* 800ABEC8: Link the five overlay sprites and their draw modes of the current
 * buffer into the overlay ordering table. */
void field_wide_overlay_draw(void) {
    s32 i;

    if (field_wide_overlay_shown != 0) {
        for (i = 0; i < 5; i++) {
            addPrim(&field_current_draw_block->overlay_ot[0], &field_wide_overlay_sprites.sprites[i][field_draw_buffer_index]);
            addPrim(&field_current_draw_block->overlay_ot[0], &field_wide_overlay_sprites.modes[i][field_draw_buffer_index]);
        }
    }
}

/* The unit's own uninitialized variables, which only the text sequence
 * below reads: the field BSS 800af76c-800af858, after field_event's and
 * ahead of the commons (field_common.c). */
static void *field_staff_roll_text;           /* 800AF76C: file 0xab */
static TextRollLine *field_staff_roll_line_sprites;   /* 800AF770: 16 lines */
static u8 *field_staff_roll_text_position;             /* 800AF774: sequence text position */
static s32 field_staff_roll_vram_row; /* 800AF778 */
static s32 field_staff_roll_pass_count; /* 800AF77C */
static s32 field_staff_roll_bytes_left;             /* 800AF780: file 0xab bytes left */
static void *field_staff_roll_font_tim;           /* 800AF784: file 0xac */
static POLY_GT4 field_staff_roll_fade_quads[2][2];  /* 800AF788: top and bottom fade per buffer */

/* 800ABFDC: The glyph of the big-endian two-byte code at `text`: codes 8540..887f give
 * overlay font cell code - 8540 (*own 1), any other the kanji ROM bitmap
 * address from Krom2RawAdd (*own 0). */
s32 field_staff_roll_get_glyph(u8 *text, s32 *own) {
    u16 code;

    code = text[1] | (text[0] << 8);
    if ((u16)(code - GLYPH_OWN_FIRST) < GLYPH_OWN_COUNT) {
        *own = 1;
        return code - GLYPH_OWN_FIRST;
    }
    *own = 0;
    return Krom2RawAdd(code);
}

/* 800AC03C: Expand a 16x15 1-bit ROM glyph into an 8-bit glyph cell (0xff set, 0
 * clear); a -1 glyph fills the cell. */
void field_staff_roll_expand_rom_glyph(u8 *cell, u16 *rows) {
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

/* 800AC0F0: Draw the next text line into VRAM row `row` at x `left`, blanking the rest
 * of its 28 cells; return the text after it. A line is up to 28 two-byte
 * codes ending at a CR, which it consumes; the CR test comes before each
 * code, so a line of 28 codes leaves its CR to the next line, an empty one.
 * Once no bytes of file 0xab are left (field_staff_roll_bytes_left) it only blanks the row. */
u8 *field_staff_roll_write_line(u8 *text, s32 left, s32 row) {
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
    if (field_staff_roll_bytes_left <= 0) {
        count = 0;
    } else {
        for (count = 0, x = left; count < GLYPH_LINE_CELLS; count++, x += 9) {
            if (line[used] == '\r') {
                used++;
                break;
            }
            glyph = field_staff_roll_get_glyph(&line[used], &own);
            used += 2;
            if (own == 1) {
                source.w = 9;
                source.h = 16;
                source.x = glyph % 7 * 9 + 0x380;
                source.y = glyph / 7 * 16 + 0x100;
                MoveImage(&source, x, row * 16);
            } else {
                field_staff_roll_expand_rom_glyph(cell.pixels[0], (u16 *)glyph);
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
    field_staff_roll_bytes_left -= used;
    return text + used;
}

/* 800AC308: Load files 0xab and 0xac. */
void field_staff_roll_load_files(void) {
    cd_select_directory(4, 0);
    field_staff_roll_bytes_left = cd_get_file_size(0xAB);
    field_staff_roll_text = heap_alloc(cd_get_aligned_file_size(0xAB), 1);
    cd_read_file(0xAB, field_staff_roll_text, 0, 0x80);
    cd_sync_reads(0);
    field_staff_roll_font_tim = heap_alloc(cd_get_aligned_file_size(0xAC), 1);
    cd_read_file(0xAC, field_staff_roll_font_tim, 0, 0x80);
    cd_sync_reads(0);
}

/* 800AC3AC: Set up the text roll: the white top and bottom fades (640 wide, 24
 * tall at 0 and c8) and the 16 lines of sprites. */
void field_staff_roll_init(void) {
    SPRT *sprite;
    s32 i;
    s32 k;

    SetPolyGT4(&field_staff_roll_fade_quads[0][0]);
    SetPolyGT4(&field_staff_roll_fade_quads[1][0]);
    field_staff_roll_fade_quads[0][0].r0 = field_staff_roll_fade_quads[0][0].g0 = field_staff_roll_fade_quads[0][0].b0 = 0xFF;
    field_staff_roll_fade_quads[0][0].r1 = field_staff_roll_fade_quads[0][0].g1 = field_staff_roll_fade_quads[0][0].b1 = 0xFF;
    field_staff_roll_fade_quads[1][0].r2 = field_staff_roll_fade_quads[1][0].g2 = field_staff_roll_fade_quads[1][0].b2 = 0xFF;
    field_staff_roll_fade_quads[1][0].r3 = field_staff_roll_fade_quads[1][0].g3 = field_staff_roll_fade_quads[1][0].b3 = 0xFF;
    field_staff_roll_fade_quads[0][0].r2 = field_staff_roll_fade_quads[0][0].g2 = field_staff_roll_fade_quads[0][0].b2 = 0;
    field_staff_roll_fade_quads[0][0].r3 = field_staff_roll_fade_quads[0][0].g3 = field_staff_roll_fade_quads[0][0].b3 = 0;
    field_staff_roll_fade_quads[1][0].r0 = field_staff_roll_fade_quads[1][0].g0 = field_staff_roll_fade_quads[1][0].b0 = 0;
    field_staff_roll_fade_quads[1][0].r1 = field_staff_roll_fade_quads[1][0].g1 = field_staff_roll_fade_quads[1][0].b1 = 0;
    setXY4(&field_staff_roll_fade_quads[0][0], 0, 0, 0x280, 0, 0, 0x18, 0x280, 0x18);
    setXY4(&field_staff_roll_fade_quads[1][0], 0, 0xC8, 0x280, 0xC8, 0, 0xE0, 0x280, 0xE0);
    field_staff_roll_fade_quads[0][0].u0 = 0;
    field_staff_roll_fade_quads[0][0].v0 = 0;
    field_staff_roll_fade_quads[0][0].u1 = 2;
    field_staff_roll_fade_quads[0][0].v1 = 0;
    field_staff_roll_fade_quads[0][0].u2 = 0;
    field_staff_roll_fade_quads[0][0].v2 = 2;
    field_staff_roll_fade_quads[0][0].u3 = 2;
    field_staff_roll_fade_quads[0][0].v3 = 2;
    setUV4(&field_staff_roll_fade_quads[1][0], 0, 0, 2, 0, 0, 2, 2, 2);
    field_staff_roll_fade_quads[0][0].tpage = GetTPage(1, 2, 0x3C0, 0x100);
    field_staff_roll_fade_quads[1][0].tpage = GetTPage(1, 2, 0x3C0, 0x100);
    field_staff_roll_fade_quads[0][0].clut = GetClut(0, 0x1FF);
    field_staff_roll_fade_quads[1][0].clut = GetClut(0, 0x1FF);
    SetSemiTrans(&field_staff_roll_fade_quads[0][0], 1);
    SetSemiTrans(&field_staff_roll_fade_quads[1][0], 1);
    field_staff_roll_fade_quads[0][1] = field_staff_roll_fade_quads[0][0];
    field_staff_roll_fade_quads[1][1] = field_staff_roll_fade_quads[1][0];
    field_staff_roll_line_sprites = heap_alloc(16 * sizeof(TextRollLine), 1);
    for (i = 0; i < 16; i++) {
        sprite = &field_staff_roll_line_sprites[i].sprites[0][0];
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
            field_staff_roll_line_sprites[i].sprites[0][k].x0 = 0x40 + k * 0x80;
            field_staff_roll_line_sprites[i].sprites[1][k].x0 = 0x40 + k * 0x80;
            SetDrawMode(&field_staff_roll_line_sprites[i].modes[0][k], 0, 0, GetTPage(1, 0, 0x300 + k * 0x40, 0), NULL);
            SetDrawMode(&field_staff_roll_line_sprites[i].modes[1][k], 0, 0, GetTPage(1, 0, 0x300 + k * 0x40, 0), NULL);
        }
    }
}

/* 800AC99C: Draw the text roll: its two fades, then each line scrolled up a pixel
 * from the other buffer's position. */
void field_staff_roll_draw(void) {
    s32 y;
    s32 i;
    s32 k;

    addPrim(&field_current_draw_block->overlay_ot[0], &field_staff_roll_fade_quads[1][field_draw_buffer_index]);
    addPrim(&field_current_draw_block->overlay_ot[0], &field_staff_roll_fade_quads[0][field_draw_buffer_index]);
    for (i = 0; i < 16; i++) {
        y = (field_staff_roll_line_sprites[i].sprites[(field_draw_buffer_index + 1) & 1][0].y0 - 1) & 0xFF;
        field_staff_roll_line_sprites[i].sprites[field_draw_buffer_index][0].y0 = y;
        field_staff_roll_line_sprites[i].sprites[field_draw_buffer_index][1].y0 = y;
        field_staff_roll_line_sprites[i].sprites[field_draw_buffer_index][2].y0 = y;
        field_staff_roll_line_sprites[i].sprites[field_draw_buffer_index][3].y0 = y;
        for (k = 0; k < 4; k++) {
            addPrim(&field_current_draw_block->overlay_ot[0], &field_staff_roll_line_sprites[i].sprites[field_draw_buffer_index][k]);
            addPrim(&field_current_draw_block->overlay_ot[0], &field_staff_roll_line_sprites[i].modes[field_draw_buffer_index][k]);
        }
    }
}

/* field.c's TIM upload, declared here: the call in 800aaf80 above passes it
 * unnarrowed ints, as field_event.c's do (no header declares it). */
void field_load_tim_at(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

/* 800ACB90: Upload file 0xac's image to (380, 100) with its CLUT at (0, 1ff), then
 * fill the 64x4 VRAM block at (3c0, 100) with ones. */
void field_staff_roll_upload_font(void) {
    RECT rect;
    u_long *pixels;
    s32 i;

    field_load_tim_at(field_staff_roll_font_tim, 0x380, 0x100, 0, 0x1FF, 0, 0);
    DrawSync(0);
    heap_free(field_staff_roll_font_tim);
    pixels = heap_alloc(0x200, 1);
    for (i = 0; i < 0x80; i++) {
        pixels[i] = -1;
    }
    rect.x = 0x3C0;
    rect.y = 0x100;
    rect.w = 0x40;
    rect.h = 4;
    LoadImage(&rect, pixels);
    DrawSync(0);
    heap_free(pixels);
}

void field_staff_roll_init(void);

/* 800ACC58: Start the sequence from file 0xab when enabled. */
void field_staff_roll_start(void) {
    RECT unused; /* the original frame reserves an unused 8-byte local */

    if (mode_staff_roll_enabled != 0) {
        field_staff_roll_load_files();
        field_staff_roll_init();
        field_staff_roll_pass_count = 0;
        field_staff_roll_vram_row = 15;
        field_staff_roll_text_position = field_staff_roll_text;
    }
}

/* 800ACCB0: Release the sequence buffers when enabled. */
void field_staff_roll_release(void) {
    if (mode_staff_roll_enabled != 0) {
        heap_free(field_staff_roll_text);
        heap_free(field_staff_roll_line_sprites);
    }
}

/* 800ACCF4: Advance the sequence one pass; every 16th pass, from the first, draw the
 * next line of file 0xab at x 0x300 into VRAM row field_staff_roll_vram_row & 15 (from 15). */
void field_staff_roll_advance(void) {
    if (mode_staff_roll_enabled != 0) {
        if ((field_staff_roll_pass_count & 0xF) == 0) {
            field_staff_roll_text_position = field_staff_roll_write_line(field_staff_roll_text_position, 0x300, field_staff_roll_vram_row & 0xF);
            field_staff_roll_vram_row++;
        }
        field_staff_roll_pass_count++;
    }
}

/* 800ACD7C: The signed halfword operand at byte `offset` from the working PC. */
s32 field_event_read_s16(s32 offset) {
    u8 *code;

    code = &field_event_bytecode[field_current_event_actor->pc + offset];
    return (s16)(code[0] + (code[1] << 8));
}

/* 800ACDB8: The raw halfword operand at byte `offset` from the working PC. */
s32 field_event_read_u16(s32 offset) {
    u8 *code;

    code = &field_event_bytecode[field_current_event_actor->pc + offset];
    return code[0] | (code[1] << 8);
}

/* 800ACDEC: The operand at `offset`: bit 15 marks a 15-bit immediate, else it names
 * an event variable. */
s32 field_event_read_imm_or_var(s32 offset) {
    s32 operand;

    operand = field_event_read_u16(offset);
    if (operand & 0x8000) {
        return operand & 0x7FFF;
    }
    return field_event_read_variable(operand & 0xFFFF);
}

void field_party_board_or_leave_gears(s32 mode);

/* 800ACE24: Mark which party slots changed character, then refresh the party. */
void field_party_apply_gear_changes(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (mode_party_gear_refresh_flags[i] == game_current_data->inGear[i]) {
            mode_party_gear_refresh_flags[i] = 0;
        } else {
            mode_party_gear_refresh_flags[i] = 1;
        }
    }
    field_party_board_or_leave_gears(0);
}

/* 800ACE90: Flag the party slots whose members are present for 800ad978 (mode 1),
 * by the first slot's state. */
void field_party_toggle_gear_riding(void) {
    s32 i;

    mode_party_gear_refresh_flags[0] = mode_party_gear_refresh_flags[1] = mode_party_gear_refresh_flags[2] = 0;
    if (game_current_data->inGear[0] == 0) {
        for (i = 0; i < 3; i++) {
            if (game_current_data->inGear[i] == 0 && mode_get_character_gear_id(mode_party_members[i]) != 0xFF) {
                mode_party_gear_refresh_flags[i] = 1;
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (game_current_data->inGear[i] == 1 && mode_get_character_gear_id(mode_party_members[i]) != 0xFF) {
                mode_party_gear_refresh_flags[i] = 1;
            }
        }
    }
    field_party_board_or_leave_gears(1);
}

#define DESCRIPTOR(index) (&field_view.components.descriptors[index])

/* 800ACFD0: Return party slot `slot` to its member: swap the models back, hand the
 * stand-in's heading over, and restart both animations. */
void field_party_leave_gear(s32 slot) {
    Sprite *stand_in;
    Sprite *model;

    game_current_data->inGear[slot] = 0;
    stand_in = DESCRIPTOR(mode_party_stand_in_actors[slot])->model;
    model = DESCRIPTOR(mode_party_actors[slot])->model;
    DESCRIPTOR(mode_party_actors[slot])->model = stand_in;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->model = model;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->flags = (DESCRIPTOR(mode_party_stand_in_actors[slot])->flags & 0xF07F) | 0x200;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->flags &= 0xFFDF;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->flags &= ~1;
    field_actor_copy_position_state(mode_party_stand_in_actors[slot], mode_party_actors[slot]);
    DESCRIPTOR(mode_party_actors[slot])->model->x = DESCRIPTOR(mode_party_actors[slot])->actor->position[0];
    DESCRIPTOR(mode_party_actors[slot])->model->y = DESCRIPTOR(mode_party_actors[slot])->actor->position[1];
    DESCRIPTOR(mode_party_actors[slot])->model->z = DESCRIPTOR(mode_party_actors[slot])->actor->position[2];
    DESCRIPTOR(mode_party_actors[slot])->actor->flags |= 0x400;
    DESCRIPTOR(mode_party_actors[slot])->actor->flags &= ~0x300;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(mode_party_actors[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->unk108 = DESCRIPTOR(mode_party_actors[slot])->actor->unk108;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->heading_goal = DESCRIPTOR(mode_party_actors[slot])->actor->heading_goal;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->unkE8 = DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->unkE6;
    DESCRIPTOR(mode_party_actors[slot])->actor->unkE8 = DESCRIPTOR(mode_party_actors[slot])->actor->unkE6;
    field_actor_start_animation(DESCRIPTOR(mode_party_stand_in_actors[slot])->model, 6, DESCRIPTOR(mode_party_stand_in_actors[slot]));
    field_actor_start_animation(DESCRIPTOR(mode_party_actors[slot])->model, DESCRIPTOR(mode_party_actors[slot])->actor->unkE6,
                  DESCRIPTOR(mode_party_actors[slot]));
    field_party_record_slot_position(slot);
    field_effect_stop_by_owner(mode_party_stand_in_actors[slot], 0);
}

/* 800AD4D4: Put the current actor in for party slot `slot`: swap its model with the
 * member's, mark the slot taken, and restart both animations. */
void field_party_board_gear(s32 slot) {
    Sprite *model;

    model = DESCRIPTOR(field_current_event_actor_index)->model;
    DESCRIPTOR(field_current_event_actor_index)->model = DESCRIPTOR(mode_party_actors[slot])->model;
    DESCRIPTOR(mode_party_actors[slot])->model = model;
    DESCRIPTOR(mode_party_actors[slot])->model->x = DESCRIPTOR(mode_party_actors[slot])->actor->position[0];
    DESCRIPTOR(mode_party_actors[slot])->model->y = DESCRIPTOR(mode_party_actors[slot])->actor->position[1];
    DESCRIPTOR(mode_party_actors[slot])->model->z = DESCRIPTOR(mode_party_actors[slot])->actor->position[2];
    DESCRIPTOR(field_current_event_actor_index)->flags |= 0x20;
    DESCRIPTOR(mode_party_actors[slot])->actor->flags |= 0x200;
    DESCRIPTOR(mode_party_actors[slot])->actor->flags &= ~0x500;
    game_current_data->inGear[slot] = 1;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(mode_party_actors[slot])->actor->flags &= ~0x1800;
    DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->unkE8 = DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->unkE6;
    DESCRIPTOR(mode_party_actors[slot])->actor->unkE8 = DESCRIPTOR(mode_party_actors[slot])->actor->unkE6;
    field_actor_start_animation(DESCRIPTOR(mode_party_stand_in_actors[slot])->model, DESCRIPTOR(mode_party_stand_in_actors[slot])->actor->unkE6,
                  DESCRIPTOR(mode_party_stand_in_actors[slot]));
    field_actor_start_animation(DESCRIPTOR(mode_party_actors[slot])->model, DESCRIPTOR(mode_party_actors[slot])->actor->unkE6,
                  DESCRIPTOR(mode_party_actors[slot]));
    field_effect_stop_by_owner(mode_party_stand_in_actors[slot], 0);
    field_party_record_slot_position(slot);
}

/* 800AD898: For party members in slot state 1, set actor flag 0x200 and clear
 * 0x500. */
void field_party_set_gear_rider_flags(void) {
    s32 i;

    if (field_work.unk2268 != 0) {
        for (i = 0; i < 3; i++) {
            if (mode_party_actors[i] != 0xFF && game_current_data->inGear[i] == 1) {
                field_view.components.descriptors[mode_party_actors[i]].actor->flags |= 0x200;
                field_view.components.descriptors[mode_party_actors[i]].actor->flags &= ~0x500;
            }
        }
    }
}

/* 800AD978: For each party member flagged in 8006be2c, run 800ad4d4 when `mode`
 * disagrees with its slot state (+22b1) and 800acfd0 otherwise. */
void field_party_board_or_leave_gears(s32 mode) {
    s32 i;

    if (field_work.unk2268 != 0) {
        for (i = 0; i < 3; i++) {
            if (mode_party_actors[i] != 0xFF && mode_party_gear_refresh_flags[i] == 1) {
                field_current_event_actor_index = mode_party_stand_in_actors[i];
                if ((game_current_data->inGear[i] == 0 && mode != 0) || (game_current_data->inGear[i] != 0 && mode == 0)) {
                    field_party_board_gear(i);
                } else {
                    field_party_leave_gear(i);
                }
            }
        }
    }
}
