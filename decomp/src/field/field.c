#include "common.h"
#include "field.h"

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8006FDEC);

/* Decode compressed `source` data into `destination`. */
void func_8007008C(s32 unused, void *source, void *destination) {
    func_80032EB4(source, destination);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800700B0);

/* Load a TIM's image at (x, y) and its CLUT at (clut_x, clut_y) with the
 * given size; a CLUT y of -1 or a zero size keeps the TIM's own. */
void func_80070340(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h) {
    TIM_IMAGE image;

    OpenTIM(tim);
    if (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            if (clut_y != -1) {
                image.crect->x = clut_x;
                image.crect->y = clut_y;
            }
            if (clut_w != 0) {
                image.crect->w = clut_w;
            }
            if (clut_h != 0) {
                image.crect->h = clut_h;
            }
            LoadImage(image.crect, image.caddr);
        }
        /* Only the x placement is guarded; the image is always loaded. */
        if (image.paddr != NULL) {
            image.prect->x = x;
        }
        image.prect->y = y;
        LoadImage(image.prect, image.paddr);
    }
}

/* Start the map's own stream (file 0xb9 + 2 * map) into a four-sector ring,
 * unless one already runs. */
void func_80070488(void) {
    void *ring;

    if (D_800ADB60 == 0) {
        D_800ADB60 = 1;
        D_800ADC14 = ring = func_8002A260(4, 1);
        func_80029EB0((D_8004F34C & 0xFFF) * 2 + 0xB9, ring, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

/* Stop the field stream and release its ring, then continue with 80078c5c. */
void func_80070508(void) {
    if (D_800ADB60 == 1) {
        func_80028A60(0);
        DrawSync(0);
        func_800320E8(D_800ADC14);
        D_800ADB60 = 0;
    }
    func_80078C5C();
}

/* Widen a short vector to 16.16 fixed point. */
void func_80070560(VECTOR *out, SVECTOR *in) {
    out->vx = in->vx << 16;
    out->vy = in->vy << 16;
    out->vz = in->vz << 16;
}

/* Identity rotation with a zero translation. */
void func_80070594(MATRIX *m) {
    SVECTOR angles;

    angles.vx = 0;
    angles.vy = 0;
    angles.vz = 0;
    func_8003F738(&angles, m);
    m->t[2] = 0;
    m->t[1] = 0;
    m->t[0] = 0;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800705DC);

/* Reset the three slots at 800b06a4 and clear 800adb0c. */
void func_80070C84(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800B06A4[i].a = 0xFF;
        D_800B06A4[i].b = 0xFF;
    }
    D_800ADB0C = 0;
}

INCLUDE_RODATA(".local/decomp/field/asm/nonmatchings/field", D_8006FAF0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80070CC8);

/* Initialise both fade channels' primitives. */
void func_80071A64(void) {
    func_8007D93C(0);
    func_8007D93C(1);
}

/* Advance one fade channel a step; a finished channel whose levels reached
 * zero turns off unless a fade-out is still in progress. */
void func_80071A8C(s32 channel) {
    if (D_800B2078.fades[channel].active != 0) {
        if (D_800B2078.fades[channel].steps <= 0) {
            D_800B2078.fades[channel].steps = 0;
            if (D_800ADC08 != 1 && D_800B2078.fades[channel].level[0] == 0 &&
                D_800B2078.fades[channel].level[1] == 0 && D_800B2078.fades[channel].level[2] == 0) {
                D_800B2078.fades[channel].active = 0;
            }
        } else {
            D_800B2078.fades[channel].level[0] = D_800B2078.fades[channel].level[0] + D_800B2078.fades[channel].step[0];
            if (D_800B2078.fades[channel].level[0] >> 8 >= 0x100) {
                D_800B2078.fades[channel].level[0] = 0xFF00;
            }
            if (D_800B2078.fades[channel].level[0] < 0) {
                D_800B2078.fades[channel].level[0] = 0;
            }
            D_800B2078.fades[channel].level[1] = D_800B2078.fades[channel].level[1] + D_800B2078.fades[channel].step[1];
            if (D_800B2078.fades[channel].level[1] >> 8 >= 0x100) {
                D_800B2078.fades[channel].level[1] = 0xFF00;
            }
            if (D_800B2078.fades[channel].level[1] < 0) {
                D_800B2078.fades[channel].level[1] = 0;
            }
            D_800B2078.fades[channel].level[2] = D_800B2078.fades[channel].level[2] + D_800B2078.fades[channel].step[2];
            if (D_800B2078.fades[channel].level[2] >> 8 >= 0x100) {
                D_800B2078.fades[channel].level[2] = 0xFF00;
            }
            if (D_800B2078.fades[channel].level[2] < 0) {
                D_800B2078.fades[channel].level[2] = 0;
            }
            D_800B2078.fades[channel].steps = D_800B2078.fades[channel].steps - 1;
        }
    }
}

/* Step both fade channels while fading, then draw them into `ot`. */
void func_80071CB4(void *ot) {
    if (D_800ADC04 == 2) {
        func_80071A8C(0);
        func_80071A8C(1);
    }
    func_8007DA44(ot, D_800ADB08);
}

/* Start a fade on `channel` towards (red, green, blue) over `steps` frames. */
void func_80071D08(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr) {
    s32 red_step = ((red << 8) - D_800B2078.fades[channel].level[0]) / steps;
    s32 green_step = ((green << 8) - D_800B2078.fades[channel].level[1]) / steps;
    s32 blue_step = ((blue << 8) - D_800B2078.fades[channel].level[2]) / steps;

    D_800B2078.fades[channel].steps = steps;
    D_800B2078.fades[channel].active = 1;
    D_800B2078.fades[channel].abr = abr;
    D_800B2078.fades[channel].step[0] = red_step;
    D_800B2078.fades[channel].step[1] = green_step;
    D_800B2078.fades[channel].step[2] = blue_step;
}

/* Fade channel 0 out to white over `steps` frames, once. */
void func_80071DCC(s32 steps) {
    s32 rate;

    if (D_800ADC08 != 1) {
        D_800ADC08 = 1;
        if (D_800ADC04 == 2) {
            rate = 0xFF00 / steps;
            D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0;
            D_800B2078.fades[0].steps = steps;
            D_800B2078.fades[0].active = 1;
            D_800B2078.fades[0].abr = 2;
            D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = rate;
        }
    }
}

/* Fade channel 0 back in from full over `steps` frames, once. */
void func_80071E58(s32 steps) {
    s32 rate;

    if (D_800ADC08 != 0) {
        D_800ADC08 = 0;
        if (D_800ADC04 == 2) {
            rate = -0x10000 / steps;
            D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0xFF00;
            D_800B2078.fades[0].active = 1;
            D_800B2078.fades[0].steps = steps;
            D_800B2078.fades[0].abr = 2;
            D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = rate;
        }
    }
}

/* Pointer setup: pad buffers, divisors 3 and 4, bounds, both ports' starts. */
void func_80071EE8(void) {
    func_8007AD8C(D_800625FC[0], D_800625FC[1]);
    func_8007AE14(3, 4);
    func_8007ADA4(0, 0x140, 0, 0xE0);
    func_8007AE2C(0, 0x50, 0x64);
    func_8007AE2C(1, 0xFA, 0x64);
    func_8007ADA4(0, 0x12C, 0xA, 0xDC);
}

/* Set both draw buffers' clip areas; the second sits 0x100 lines lower. */
void func_80071F64(s32 x, s32 y, s32 w, s32 h) {
    D_800B249C[0].draw.clip.x = x;
    D_800B249C[0].draw.clip.y = y;
    D_800B249C[0].draw.clip.w = w;
    D_800B249C[0].draw.clip.h = h;
    D_800B249C[1].draw.clip.x = x;
    D_800B249C[1].draw.clip.y = y + 0x100;
    D_800B249C[1].draw.clip.w = w;
    D_800B249C[1].draw.clip.h = h;
}

/* Display setup: geometry defaults and offset, both draw blocks'
 * environments, clip areas and screens, black backgrounds, then show the
 * second block and set the renderer's limits. */
void func_80071FB0(void) {
    D_80059198 = 1;
    DrawSync(0);
    VSync(0);
    InitGeom();
    SetGeomOffset(0xA0, 0x70);
    SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x140, 0xE0);
    SetDefDrawEnv(&D_800B249C[0].draw2, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800B249C[1].draw2, 0, 0x100, 0x140, 0xE0);
    SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x140, 0xE0);
    SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x140, 0xE0);
    func_80071F64(0, 0, 0x140, 0xE0);
    func_80086D8C();
    D_800B249C[0].draw.r0 = 0;
    D_800B249C[0].draw.g0 = 0;
    D_800B249C[0].draw.b0 = 0;
    D_800B249C[1].draw.r0 = 0;
    D_800B249C[1].draw.g0 = 0;
    D_800B249C[1].draw.b0 = 0;
    D_800B249C[0].draw.dtd = 1;
    D_800B249C[1].draw.dtd = 1;
    VSync(0);
    PutDispEnv(&D_800B249C[1].disp);
    PutDrawEnv(&D_800B249C[1].draw);
    func_8002DFF0(0x140, 0xF0);
}

/* Clear a matrix's translation. */
void func_80072140(MATRIX *m) {
    m->t[2] = 0;
    m->t[1] = 0;
    m->t[0] = 0;
}

/* Compose the view (orbit under the previous view), the world matrices, and
 * leave the scaled world matrix loaded for drawing. */
void func_80072150(void) {
    VECTOR scale;
    MATRIX unused;
    MATRIX composed;
    s32 flag;

    func_8003F738(&D_800AF880.orbit_angles, &D_800AF880.orbit);
    func_80072140(&D_800AF880.orbit);
    CompMatrix(&D_800AF880.orbit, &D_800AF880.previous_view, &composed);
    func_80074038(&D_800AF880.previous_view, &composed);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.world_matrix);
    func_80072140(&D_800AF880.world_matrix);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.scaled_world);
    func_80049BDC(&D_800AF880.previous_view, &D_800AF880.scaled_world);
    SetRotMatrix(&D_800AF880.previous_view);
    SetTransMatrix(&D_800AF880.previous_view);
    func_8004A6DC(&D_800AF880.anchor, D_800AF880.scaled_world.t, &flag);
    scale.vx = D_800AF880.scale;
    scale.vy = D_800AF880.scale;
    scale.vz = D_800AF880.scale;
    ScaleMatrix(&D_800AF880.scaled_world, &scale);
    SetRotMatrix(&D_800AF880.scaled_world);
    SetTransMatrix(&D_800AF880.scaled_world);
}

/* Rebuild a descriptor's matrix from its rotation, scaled by its actor. */
void func_80072254(s32 index) {
    VECTOR scale;

    scale.vx = D_800AF880.components.descriptors[index].actor->scale[0];
    scale.vy = D_800AF880.components.descriptors[index].actor->scale[1];
    scale.vz = D_800AF880.components.descriptors[index].actor->scale[2];
    func_8003F738(&D_800AF880.components.descriptors[index].rotation, &D_800AF880.components.descriptors[index].matrix);
    ScaleMatrix(&D_800AF880.components.descriptors[index].matrix, &scale);
}

/* Compose the view and reload the scaled world matrix; 802815b0 runs unless
 * 800c268c is set. */
void func_800722F4(void) {
    func_80072150();
    SetRotMatrix(&D_800AF880.scaled_world);
    SetTransMatrix(&D_800AF880.scaled_world);
    if (D_800C268C == 0) {
        func_802815B0();
    }
}

/* Count the blocked octants from `start` upward; zero when all are blocked. */
s32 func_8007234C(s32 mask, s32 start) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 8; i++, count++) {
        if (!(mask & D_800ADC1C[start++ & 7])) {
            return count;
        }
    }
    return 0;
}

/* Count the blocked octants from `start` downward; zero when all are blocked. */
s32 func_80072398(s32 mask, s32 start) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 8; i++, count++) {
        if (!(mask & D_800ADC1C[start-- & 7])) {
            return count;
        }
    }
    return 0;
}

#ifdef NON_MATCHING
/* The intersection of the lines through segments `a` and `b` (X/Z points);
 * `b`'s start when they are parallel. Differs only in the register of the
 * second product of each cross product (t0/v1 in the original). */
void func_800723E4(DVECTOR *a, DVECTOR *b, DVECTOR *out) {
    VECTOR ua;
    VECTOR ub;
    VECTOR d;
    s32 cross;
    s32 t;

    d.vx = a[1].vx - a[0].vx;
    d.vy = 0;
    d.vz = a[1].vy - a[0].vy;
    func_80048D7C(&d, &ua);
    d.vx = b[1].vx - b[0].vx;
    d.vy = 0;
    d.vz = b[1].vy - b[0].vy;
    func_80048D7C(&d, &ub);
    cross = (ub.vx * ua.vz - ub.vz * ua.vx) >> 12;
    if (cross == 0) {
        t = 0;
    } else {
        t = ((b[0].vy - a[0].vy) * ua.vx - (b[0].vx - a[0].vx) * ua.vz) / cross;
    }
    out->vx = b[0].vx + ((t * ub.vx) >> 12);
    out->vy = b[0].vy + ((t * ub.vz) >> 12);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800723E4);
#endif

#ifdef NON_MATCHING
/* The camera's initial state. */
void func_8007254C(void) {
    D_800AF880.target_a = 8;
    D_800AF880.target_b = 8;
    D_800AF880.heading_velocity = 0x400000;
    D_800AF880.heading_high = 0x08000000;
    D_800AF880.heading_angles.vy = 0x800;
    D_800AF880.shake_amplitude[2] = 0;
    D_800AF880.shake_amplitude[1] = 0;
    D_800AF880.shake_amplitude[0] = 0;
    D_800AF880.shake_step[2] = 0;
    D_800AF880.shake_step[1] = 0;
    D_800AF880.shake_step[0] = 0;
    D_800AF880.shake = 0;
    D_800AF880.shake_stop = 0;
    D_800AF880.flags = 0;
    D_800AF880.view_angle = 0;
    D_800AF880.angle = 0;
    D_800AF880.heading_blocks[0] = 0;
    D_800AF880.heading_blocks[1] = 0;
    D_800AF880.heading_steps = 0;
    D_800AF880.heading_angles.vx = 0;
    D_800AF880.heading_angles.vz = 0;
    D_800AF880.heading = 0x800;
    D_800AF880.orbit_angles.vx = 0;
    D_800AF880.orbit_angles.vy = 0;
    D_800AF880.orbit_angles.vz = 0;
    func_80070594(&D_800AF880.orbit);
    D_800AF880.eye.vx = 0;
    D_800AF880.eye.vy = 0;
    D_800AF880.eye.vz = 0;
    D_800AF880.up.vy = 0x10000000;
    D_800AF880.unk050.vy = 0x10000000;
    D_800AF880.elevation = 0x1E;
    D_800AF880.projection = 0x200;
    D_800AF880.target.vx = 0;
    D_800AF880.target.vy = 0;
    D_800AF880.target.vz = 0;
    D_800AF880.up.vx = 0;
    D_800AF880.up.vz = 0;
    D_800AF880.shake_offset.vx = 0;
    D_800AF880.shake_offset.vy = 0;
    D_800AF880.shake_offset.vz = 0;
    D_800AF880.eye_goal.vx = 0;
    D_800AF880.eye_goal.vy = 0;
    D_800AF880.eye_goal.vz = 0;
    D_800AF880.target_goal.vx = 0;
    D_800AF880.target_goal.vy = 0;
    D_800AF880.target_goal.vz = 0;
    D_800AF880.unk050.vx = 0;
    D_800AF880.unk050.vz = 0;
    D_800AF880.elevation_steps = 0;
    D_800AF880.projection_steps = 0;
    D_800AF880.steps = 0;
    D_800AF880.distance = 0x1000;
    D_800AF880.mode = 0;
    D_800AF880.scripted = 0;
    D_800AF880.target_steps = 0;
    D_800AF880.eye_steps = 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007254C);
#endif

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800726E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80072A38);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80072D74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80073230);

/* Rotate `point` in X/Z about `center` by the camera heading angles. */
void func_80073684(VECTOR *point, VECTOR *center) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;

    PushMatrix();
    func_8003F738(&D_800AF880.heading_angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    ApplyMatrixLV(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    PopMatrix();
}

/* Truncate a 16.16 vector to its integer parts. */
void func_80073734(VECTOR *v) {
    v->vx = ((s16 *)&v->vx)[1];
    v->vy = ((s16 *)&v->vy)[1];
    v->vz = ((s16 *)&v->vz)[1];
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80073750);

/* Turn angle `angle` towards `goal` by `step` the short way round, stopping
 * at the goal; 12-bit angles. */
s32 func_80073930(s32 angle, s32 goal, s32 step) {
    if (((angle - goal) & 0xFFF) < 0x800) {
        angle -= step;
        if (((angle - goal) & 0xFFF) >= 0x800) {
            angle = goal;
        }
    } else {
        angle += step;
        if (((angle - goal) & 0xFFF) < 0x800) {
            angle = goal;
        }
    }
    return angle & 0xFFF;
}

/* Turn towards `goal` by `step`, or jump there when 800adc18 is set. */
s32 func_80073988(s32 angle, s32 goal, s32 step) {
    s32 result;

    if (D_800ADC18 == 0) {
        result = func_80073930(angle, goal, step);
    } else {
        result = goal & 0xFFF;
    }
    return result;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800739C0);

/* Refresh each shown model instance's bounds (800aa9dc) and choose its
 * drawing mode from its descriptor flags. */
void func_80073E38(void) {
    s32 i;
    FieldDescriptor *descriptor;
    FieldInstance *instance;
    u16 flags;

    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        if (!(descriptor->flags & 0x40)) {
            instance = descriptor->instance;
            func_800AA9DC(instance);
            if (D_800B2078.sprite_gate != 0) {
                if (descriptor->flags & 0x10) {
                    instance->mode = 5;
                } else {
                    instance->mode = 4;
                }
            } else {
                flags = descriptor->flags;
                if (flags & 0xC) {
                    instance->mode = 1;
                } else if (flags & 0x4000) {
                    instance->mode = 3;
                } else if (flags & 0x10) {
                    instance->mode = 2;
                } else {
                    instance->mode = 0;
                }
            }
        }
    }
}

/* Switch to the other draw block and clear its overlay ordering table.
 * Breaks (code 1 in the high field; maspsx places `break N` in the low
 * field, so it is written as 1024) when 800c268c is clear. */
void func_80073F50(void) {
    if (D_800C268C == 0) {
        __asm__ volatile("break 1024");
    }
    D_800ADB08 = (D_800ADB08 + 1) % 2;
    D_800C426C = &D_800B249C[D_800ADB08];
    ClearOTagR(D_800C426C->overlay_ot, 8);
}

/* Swap the draw buffer and clear its ordering tables. */
void func_80073FE0(void) {
    func_80073F50();
    ClearOTagR(D_800C426C->ot, 0x1000);
    if (D_800ADB4C != 0) {
        ClearOTagR(D_800C426C->ot2, 0x1000);
    }
}

/* Copy a matrix's rotation and translation. */
void func_80074038(MATRIX *to, MATRIX *from) {
    func_8007409C(to, from);
    func_80074078(to, from);
}

/* Copy a matrix's translation. */
void func_80074078(MATRIX *to, MATRIX *from) {
    to->t[0] = from->t[0];
    to->t[1] = from->t[1];
    to->t[2] = from->t[2];
}

/* Copy a matrix's rotation. */
void func_8007409C(MATRIX *to, MATRIX *from) {
    to->m[0][0] = from->m[0][0];
    to->m[0][1] = from->m[0][1];
    to->m[0][2] = from->m[0][2];
    to->m[1][0] = from->m[1][0];
    to->m[1][1] = from->m[1][1];
    to->m[1][2] = from->m[1][2];
    to->m[2][0] = from->m[2][0];
    to->m[2][1] = from->m[2][1];
    to->m[2][2] = from->m[2][2];
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80074108);

/* Whether any shown descriptor (flag 0x40 clear) has flag 0x8000. */
s32 func_8007469C(void) {
    s32 i;
    u16 flags;

    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        flags = D_800AF880.components.descriptors[i].flags;
        if (!(flags & 0x40)) {
            if (flags & 0x8000) {
                return 1;
            }
        }
    }
    return 0;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80074700);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800748E8);

/* Draw the 801e module's layer (with the emitters updated and its back
 * colour set) when enabled, then the debug "GEAR" timer. */
void func_8007520C(void) {
    if (D_8004F380 == 0) {
        if (D_800B2078.unk2264 != 0) {
            func_80086BA8();
            SetBackColor(D_800B2078.unk225C[0], D_800B2078.unk225C[1], D_800B2078.unk225C[2]);
            func_801E7D14(&D_800AF880.scaled_world, D_800B2078.unk221C, D_800C426C->ot, D_800ADB08, 1);
        }
        if (D_800C268C == 0) {
            func_80281B00("GEAR      ");
        }
    }
}

extern u8 D_800ADB05; /* 1 while character drawing is off */
extern u8 D_800AFA64[];
void func_800250E0(s32 buffer);
void func_80024FE4(u32 *ot);
void func_80024FF4(void *p);
void func_8001D468(void);
void func_8001C9F8(void);
void func_8001C964(void);
void func_80023210(FieldModel *model);
void func_80075B44(u32 *ot, s32 buffer);
void func_800764B4(u32 *ot, s32 buffer);

/* Draw the field characters: set up the model renderer for this buffer, then
 * draw each actor's model (shown ones unless their layer is hidden or they
 * are flagged off, others only with layer flag 0x1000000), then the debug
 * "CHAR" timer. The descriptor flags are read as a whole word here. */
void func_800752C8(void) {
    s32 i;

    if (D_800ADB05 == 1) {
        return;
    }
    func_800250E0(D_800ADB08);
    func_80024FE4(D_800C426C->ot);
    func_80024FF4(D_800AFA64);
    func_8001D468();
    func_8001C9F8();
    func_8001C964();
    func_80075B44(D_800C426C->ot, D_800ADB08);
    for (i = 0; i < D_800ADBFC; i++) {
        if ((*(u32 *)&D_800AF880.components.descriptors[i].flags & 0x60) == 0x40) {
            if ((D_800AF880.components.descriptors[i].actor->layer_flags & 0x600) != 0x200
                && !(D_800AF880.components.descriptors[i].actor->layer_flags & 0x1000)
                && !(D_800AF880.components.descriptors[i].actor->flags & 1)) {
                func_80023210(D_800AF880.components.descriptors[i].model);
            }
        } else if (D_800AF880.components.descriptors[i].actor->layer_flags & 0x1000000) {
            func_80023210(D_800AF880.components.descriptors[i].model);
        }
    }
    func_800764B4(D_800C426C->ot, D_800ADB08);
    if (D_800C268C == 0) {
        func_80281B00("CHAR      ");
    }
}

/* Link a table's primitives into `ot` (AddPrims). */
void func_80075458(void *ot, u32 *table, s32 depth) {
    AddPrims(ot, table + depth, table);
}

/* Draw the 801e-layer object from the camera eye and target when event
 * parameters are enabled. */
void func_80075484(void) {
    SVECTOR eye;
    SVECTOR target;

    if (D_800B0080.enabled != 0 && D_800ADB50 == 0) {
        eye.vx = D_800AF880.eye.vx >> 16;
        eye.vy = D_800AF880.eye.vy >> 16;
        eye.vz = D_800AF880.eye.vz >> 16;
        target.vx = D_800AF880.target.vx >> 16;
        target.vy = D_800AF880.target.vy >> 16;
        target.vz = D_800AF880.target.vz >> 16;
        func_800273C4(D_800B007C, &eye, &target, &D_800AF880.scaled_world,
                      D_800C426C->ot + D_800B2078.unk21D4 + 0x1000, D_800ADB08);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007554C);

/* Finish the frame: draw the overlays, flush, sync, copy the shown half,
 * then put this block's environments and draw its overlay table. */
void func_80075910(void) {
    RECT rect;

    func_80074700();
    func_800A2030();
    func_800805F4();
    func_8008004C(D_800C426C->overlay_ot, D_800ADB08);
    DrawSync(0);
    VSync(0);
    rect.x = 0x140;
    rect.w = 0x140;
    rect.h = 0xE0;
    rect.y = ((D_800ADB78 + 1) & 1) << 8;
    MoveImage(&rect, 0, D_800ADB08 << 8);
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    DrawOTag(&D_800C426C->overlay_ot[7]);
}

void func_8004A480(VECTOR *a, VECTOR *b, VECTOR *out); /* OuterProduct12 */

/* The rotation matrix whose second row is `axis`: the first row is the unit
 * vector perpendicular to world up and `axis`, the third completes the basis. */
void func_800759E4(MATRIX *m, VECTOR *axis) {
    VECTOR up = { 0, 0, 0x1000, 0 };
    VECTOR side;
    VECTOR cross;

    func_8004A480(&up, axis, &cross);
    func_80048D7C(&cross, &side);
    func_8004A480(&side, axis, &cross);
    func_80048D7C(&cross, &up);
    m->m[0][0] = side.vx;
    m->m[0][1] = side.vy;
    m->m[0][2] = side.vz;
    m->m[1][0] = axis->vx;
    m->m[1][1] = axis->vy;
    m->m[1][2] = axis->vz;
    m->m[2][0] = up.vx;
    m->m[2][1] = up.vy;
    m->m[2][2] = up.vz;
}

/* Pass a colour on to resident 80021b98 unless 800b218e is set. */
void func_80075B08(void *target, u8 *color) {
    if (D_800B2078.sprite_gate == 0) {
        func_80021B98(target, color[0], color[1], color[2]);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80075B44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800764B4);

/* Sprite completion callback: flag the sprite's actor (layer bit 16). */
void func_80076A74(FieldSprite *sprite) {
    D_800AF880.components.descriptors[sprite->sequencer->actor].actor->layer_flags |= 0x10000;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80076AC0);

/* Set the semi-transparency bit of each of `count` 16-bit pixels. */
void func_800771B0(u32 *pixels, s32 count) {
    s32 i;

    i = count / 2;
    while (--i != -1) {
        *pixels++ |= 0x80008000;
    }
}

/* Load every image (and CLUT) of a TIM list into VRAM where it lies. */
void func_800771F8(u32 *tim) {
    TIM_IMAGE image;

    OpenTIM(tim);
    while (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            LoadImage(image.crect, image.caddr);
        }
        if (image.paddr != NULL) {
            LoadImage(image.prect, image.paddr);
        }
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077268);

/* Load the text palette, with the debug font first when enabled. */
void func_80077544(void) {
    if (D_800C268C == 0) {
        func_8003747C(0x80270000);
        func_800374E8(0x10, 0x10, 0x130, 0xE0, 0x400, 4, 0x3C0, 0x100, 0x100, 0x1FF, 0);
    }
    func_80033698(0x100, 0xF0);
}

/* Select the field's heap tag and directory, then set up the pointer. */
void func_800775C0(void) {
    func_80032498(8, 0);
    func_80028470(4, 0);
    func_80071EE8();
}

/* Wait for drawing to finish (DrawSync), then VSync. */
void func_800775F8(void) {
    DrawSync(0);
    VSync(0);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077620);

/* Stop the stream, then read the map's data ahead until it is in. */
void func_800777DC(void) {
    func_80028A60(0);
    while (func_8001B484((D_8004F34C & 0xFFF) * 2, 0) != 0) {
    }
}

/* Record the VSync counter. */
void func_8007781C(void) {
    D_800ADBA4 = VSync(1);
}

/* Set a matrix's nine rotation elements. */
void func_80077844(MATRIX *m, s32 m00, s32 m01, s32 m02, s32 m10, s32 m11, s32 m12, s32 m20,
                   s32 m21, s32 m22) {
    m->m[0][0] = m00;
    m->m[0][1] = m01;
    m->m[0][2] = m02;
    m->m[1][0] = m10;
    m->m[1][1] = m11;
    m->m[1][2] = m12;
    m->m[2][0] = m20;
    m->m[2][1] = m21;
    m->m[2][2] = m22;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077884);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077AB4);

/* Run 80077884 then 80077ab4. */
void func_80077C60(void) {
    func_80077884();
    func_80077AB4();
}

/* Allocate and keep the three party sprite blocks (0x14000 bytes each). */
void func_80077C88(void) {
    func_80032498(8, 0);
    D_8005A414[0] = func_80031BDC(0x14000, 0);
    D_8005A414[1] = func_80031BDC(0x14000, 0);
    D_8005A414[2] = func_80031BDC(0x14000, 0);
    func_800320A4(D_8005A414[0]);
    func_800320A4(D_8005A414[1]);
    func_800320A4(D_8005A414[2]);
}

/* Unlink, then release, the three blocks at 8005a414..8005a41c. */
void func_80077D2C(void) {
    func_800320B8(D_8005A414[0]);
    func_800320B8(D_8005A414[1]);
    func_800320B8(D_8005A414[2]);
    func_800320E8(D_8005A414[0]);
    func_800320E8(D_8005A414[1]);
    func_800320E8(D_8005A414[2]);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077DAC);

#ifdef NON_MATCHING
/* -1 when the field may leave (800adbd0 is 1, 800b2344 clear, and the
 * controlled actor has flag 0x800), else 0. */
s32 func_80077E10(void) {
    s32 result = 0;

    if (D_800ADBD0 == 1 && D_800B2078.jump_mode == 0) {
        result = -((D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags & 0x800) != 0);
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077E10);
#endif

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077E88);

/* Field post-frame work: 8003fa38, resolve a pending sound, count down the
 * instant-turn frames. Declared int but returns nothing (the return register
 * stays live, so the final branch keeps an empty delay slot). */
s32 func_80078B5C(void) {
    rand();
    if (D_8004F308 == -1) {
        D_8004F308 = func_80085C90(D_8004F324);
    }
    if (D_800ADC18 != 0) {
        D_800ADC18--;
    }
}

#ifdef NON_MATCHING
/* -1 while a battle menu, the disc, the music, a battle request or a pending
 * transition is busy; otherwise -1 only when 800adbc4 is not 0xff. */
s32 func_80078BC8(void) {
    s32 result;

    if (D_800ADB2C != 0) {
        return -1;
    }
    result = -1;
    if (func_800286CC() == 0 && D_8004F308 == 0 && D_800ADB90 == 0 && D_800ADB34 == 0) {
        result = -(D_800ADBC4 != 0xFF);
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80078BC8);
#endif

/* With 800b2344 set, brighten the 256x32 text strip at (0, 1e0) in VRAM
 * (every non-transparent pixel gains 0x0c63) and disable both buffers'
 * dithering. */
void func_80078C5C(void) {
    RECT rect;
    u32 *pixels;
    s32 i;

    if (D_800B2078.jump_mode != 0) {
        D_800B249C[0].draw.dtd = 0;
        D_800B249C[1].draw.dtd = 0;
        pixels = func_80031BDC(0x4000, 0);
        rect.y = 0x1E0;
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0x20;
        StoreImage(&rect, pixels);
        DrawSync(0);
        for (i = 0; i < 0x1000; i++) {
            if (pixels[i] & 0xFFFF) {
                pixels[i] |= 0xC63;
            }
            if (pixels[i] & 0xFFFF0000) {
                pixels[i] |= 0x0C630000;
            }
        }
        LoadImage(&rect, pixels);
        DrawSync(0);
        func_800320E8(pixels);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80078D44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80079288);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007954C);

void func_800796F4(void) {
}

/* Switch to the other draw block and put its display and draw environments. */
void func_800796FC(void) {
    D_800ADB08 = (D_800ADB08 + 1) % 2;
    D_800C426C = &D_800B249C[D_800ADB08];
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
}

extern DR_MODE D_800AFE24[2]; /* fade draw mode per buffer */
extern RECT D_800AFE4C;       /* fade copy source */
extern TILE D_800AFE54[2];    /* fade tile per buffer */

/* Present the current buffer under a full-screen tile of brightness
 * `level * 4`: link the tile and its draw mode, copy the display area, then
 * put the environments and draw. */
void func_80079784(s32 level) {
    func_80073FE0();
    D_800AFE54[D_800ADB08].r0 = D_800AFE54[D_800ADB08].g0 = D_800AFE54[D_800ADB08].b0 = level * 4;
    addPrim(D_800C426C->ot, &D_800AFE54[D_800ADB08]);
    addPrim(D_800C426C->ot, &D_800AFE24[D_800ADB08]);
    func_800775F8();
    MoveImage(&D_800AFE4C, 0, D_800ADB08 << 8);
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    DrawOTag(&D_800C426C->ot[1]);
}

/* Set the battle-entry flag (80059179): clear only while the controlled
 * actor has neither bit 0x40 nor 0x80 of +14; 800b234c overrides it. */
void func_800798BC(void) {
    if (D_800B2078.unk2268 != 0 && !(D_800AF880.components.descriptors[D_800B2078.controlled].actor->unk014 & 0xC0)) {
        D_80059179 = 0;
    } else {
        D_80059179 = 1;
    }
    if (D_800B2078.battle_override != 0xFF) {
        D_80059179 = D_800B2078.battle_override;
    }
}

/* Move a VRAM rectangle (MoveImage) and wait for it. */
void func_8007995C(s32 w, s32 h, s32 x, s32 y, s32 to_x, s32 to_y) {
    RECT rect;

    rect.w = w;
    rect.h = h;
    rect.x = x;
    rect.y = y;
    MoveImage(&rect, to_x, to_y);
    DrawSync(0);
}

/* Sync, then flush the instruction cache inside a critical section. */
void func_8007999C(void) {
    func_800775F8();
    EnterCriticalSection();
    FlushCache();
    ExitCriticalSection();
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800799D4);

/* Set a quad's texture coordinates, each clamped to 0..255. */
void func_8007A44C(POLY_FT4 *poly, s16 u0, s16 v0, s16 u1, s16 v1, s16 u2, s16 v2, s16 u3, s16 v3) {
    if (u0 < 0) {
        u0 = 0;
    }
    if (u1 < 0) {
        u1 = 0;
    }
    if (u2 < 0) {
        u2 = 0;
    }
    if (u3 < 0) {
        u3 = 0;
    }
    if (v0 < 0) {
        v0 = 0;
    }
    if (v1 < 0) {
        v1 = 0;
    }
    if (v2 < 0) {
        v2 = 0;
    }
    if (v3 < 0) {
        v3 = 0;
    }
    if (u0 >= 0x100) {
        u0 = 0xFF;
    }
    if (u1 >= 0x100) {
        u1 = 0xFF;
    }
    if (u2 >= 0x100) {
        u2 = 0xFF;
    }
    if (u3 >= 0x100) {
        u3 = 0xFF;
    }
    if (v0 >= 0x100) {
        v0 = 0xFF;
    }
    if (v1 >= 0x100) {
        v1 = 0xFF;
    }
    if (v2 >= 0x100) {
        v2 = 0xFF;
    }
    if (v3 >= 0x100) {
        v3 = 0xFF;
    }
    poly->u0 = u0;
    poly->v0 = v0;
    poly->u1 = u1;
    poly->v1 = v1;
    poly->u2 = u2;
    poly->v2 = v2;
    poly->u3 = u3;
    poly->v3 = v3;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007A5C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007A7F4);

#ifdef NON_MATCHING
/* Set up a pointer marker: a 48x48 quad around the origin and its
 * semi-transparent textured primitive, copied for the second buffer. */
void func_8007AA44(FieldMarker *m) {
    SetPolyFT4(&m->poly[0]);
    m->v[3].vx = -0x18;
    m->v[3].vy = 0;
    m->v[3].vz = -0x18;
    m->v[2].vx = 0x18;
    m->v[2].vy = 0;
    m->v[2].vz = -0x18;
    m->v[1].vx = -0x18;
    m->v[1].vy = 0;
    m->v[1].vz = 0x18;
    m->v[0].vx = 0x18;
    m->v[0].vy = 0;
    m->v[0].vz = 0x18;
    m->poly[0].r0 = 0x80;
    m->poly[0].g0 = 0x80;
    m->poly[0].b0 = 0x80;
    m->poly[0].tpage = GetTPage(0, 2, 0x280, 0x1E0);
    m->poly[0].clut = GetClut(0x100, 0xF3);
    SetSemiTrans(&m->poly[0], 1);
    m->poly[0].u0 = 0;
    m->poly[0].v0 = 0xE0;
    m->poly[0].u1 = 0xF;
    m->poly[0].v1 = 0xE0;
    m->poly[0].u2 = 0;
    m->poly[0].v2 = 0xEF;
    m->poly[0].u3 = 0xF;
    m->poly[0].v3 = 0xEF;
    m->poly[1] = m->poly[0];
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007AA44);
#endif

/* Project a marker quad's four corners with the given matrix into its buffer's
 * textured polygon and link that polygon into the ordering table entry. */
void func_8007AB6C(u32 *ot, FieldMarker *marker, MATRIX *m, s32 buffer) {
    POLY_FT4 *poly = &marker->poly[buffer];
    s32 p;
    s32 flag;

    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotAverage4(&marker->v[0], &marker->v[1], &marker->v[2], &marker->v[3],
                &poly->x0, &poly->x1, &poly->x2, &poly->x3, &p, &flag);
    addPrim(ot + 1, poly);
    PopMatrix();
}

/* Project a quad, then replace it with a 16x10 screen-aligned sprite standing
 * on the midpoint of its projected bottom edge, and link it into the ordering
 * table entry. */
void func_8007AC58(u32 *ot, FieldMarker *marker, MATRIX *m, s32 buffer) {
    POLY_FT4 *poly = &marker->poly[buffer];
    s32 p;
    s32 flag;
    s32 x;
    s32 y;
    s32 right;

    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotAverage4(&marker->v[0], &marker->v[1], &marker->v[2], &marker->v[3],
                &poly->x0, &poly->x1, &poly->x2, &poly->x3, &p, &flag);
    x = (poly->x3 + poly->x2) / 2;
    right = x + 8;
    x -= 8;
    y = poly->y3;
    setXY4(poly, x, y - 10, right, y - 10, x, y, right, y);
    addPrim(ot + 1, poly);
    PopMatrix();
}

/* Set the pointer's two pad buffers. */
void func_8007AD8C(void *pad0, void *pad1) {
    D_800B0054[0] = pad0;
    D_800B0054[1] = pad1;
}

/* Set the pointer bounds, scaled by the divisors. */
void func_8007ADA4(s32 left, s32 right, s32 top, s32 bottom) {
    D_800C3A44 = left * D_800B005C;
    D_800C3A50 = right * D_800B005C;
    D_800C3A4C = top * D_800B0060;
    D_800C3A54 = bottom * D_800B0060;
}

/* Set the pointer's X and Y divisors. */
void func_8007AE14(s32 x_divisor, s32 y_divisor) {
    D_800B005C = x_divisor;
    D_800B0060 = y_divisor;
}

/* Set a port's pointer position, scaled by the divisors. */
void func_8007AE2C(s32 port, s32 x, s32 y) {
    D_800B0068[port] = x * D_800B005C;
    D_800B0070[port] = y * D_800B0060;
}

/* Read a port's pointer into out[0..4]: x and y (unscaled), buttons (mouse
 * pads only, else -0x100), x and y motion. Declared int but returns nothing. */
s32 func_8007AE78(s32 port, s32 *out) {
    func_8007AF74(port);
    out[0] = D_800B0068[port] / D_800B005C;
    out[1] = D_800B0070[port] / D_800B0060;
    out[2] = -0x100;
    out[3] = D_800B0054[port][4];
    out[4] = D_800B0054[port][5];
    if (D_800B0054[port][0] == 0 && D_800B0054[port][1] == 0x12) {
        out[2] = ~D_800B0054[port][3] & 0xC;
    }
}

/* Move a mouse port's pointer by its motion, kept inside the bounds. */
void func_8007AF74(s32 port) {
    if (D_800B0054[port][0] == 0 && D_800B0054[port][1] == 0x12) {
        D_800B0068[port] += D_800B0054[port][4];
        D_800B0070[port] += D_800B0054[port][5];
        if (D_800B0068[port] > D_800C3A50) {
            D_800B0068[port] = D_800C3A50;
        } else if (D_800B0068[port] < D_800C3A44) {
            D_800B0068[port] = D_800C3A44;
        }
        if (D_800B0070[port] > D_800C3A54) {
            D_800B0070[port] = D_800C3A54;
        } else if (D_800B0070[port] < D_800C3A4C) {
            D_800B0070[port] = D_800C3A4C;
        }
    }
}

#ifdef NON_MATCHING
/* The height of `p` on the plane through triangle a, b, c (0 for a vertical
 * plane); the plane normal is left in `normal`. Differs only in the register
 * of the second product (t1 in the original). */
void func_8007B07C(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *p, VECTOR *normal) {
    VECTOR edge_b;
    VECTOR edge_c;
    VECTOR d;

    d.vx = b->vx - a->vx;
    d.vy = b->vy - a->vy;
    d.vz = b->vz - a->vz;
    func_80048D7C(&d, &edge_b);
    d.vx = c->vx - a->vx;
    d.vy = c->vy - a->vy;
    d.vz = c->vz - a->vz;
    func_80048D7C(&d, &edge_c);
    func_8004A480(&edge_b, &edge_c, normal);
    if (normal->vy == 0) {
        p->vy = 0;
        return;
    }
    p->vy = a->vy + (-(normal->vx * (p->vx - a->vx)) - normal->vz * (p->vz - a->vz)) / normal->vy;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B07C);
#endif

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B1C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B478);

/* The X/Z offset `distance` away at `angle`, scaled by 800b218c. */
void func_8007B614(VECTOR *out, s32 distance, s32 angle) {
    s32 heading;

    distance *= 16;
    distance = (distance * D_800B2078.scale) >> 12;
    heading = angle & 0xFFF;
    out->vx = func_8003F8CC(heading) * distance;
    out->vz = -(func_8003F8B0(heading) * distance);
    out->vy = 0;
}

/* The heading of an X/Z offset. */
s32 func_8007B694(VECTOR *v) {
    return -ratan2(v->vz, v->vx) & 0xFFF;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B6C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B814);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007BAC0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007BEF4);

/* The floor range: `high` is `low` raised by a nonnegative extent. */
void func_8007C670(s32 *low, s32 *high, s32 extent) {
    s32 base = *low;

    if (extent >= 0) {
        *low = base;
        base += extent;
    } else {
        *low = base;
    }
    *high = base;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007C694);

/* Allocate `words` words of the scratchpad. */
u32 *func_8007CD3C(s32 words) {
    u32 *p = (u32 *)0x1F800000 + D_800ADC10;

    D_800ADC10 += words;
    return p;
}

/* Release `words` words of the scratchpad. */
void func_8007CD60(s32 words) {
    D_800ADC10 -= words;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007CD80);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007D3D4);

/* Normalise a 20.12 vector, pointing it along its largest component. */
void func_8007D818(VECTOR *v, VECTOR *out) {
    s32 largest = func_8007D8B4(v->vx, v->vy, v->vz);

    v->vx >>= 12;
    v->vy >>= 12;
    v->vz >>= 12;
    if (largest < 0) {
        v->vx = -v->vx;
        v->vy = -v->vy;
        v->vz = -v->vz;
    }
    func_80048D7C(v, out);
}

/* The component of largest magnitude (0 when none is strictly ahead). */
s32 func_8007D8B4(s32 x, s32 y, s32 z) {
    s32 ax = x;
    s32 ay = y;
    s32 az = z;

    if (ax < 0) {
        ax = -ax;
    }
    if (ay < 0) {
        ay = -ay;
    }
    if (az < 0) {
        az = -az;
    }
    if (ax >= ay && ax >= az) {
        return x;
    }
    if (ay >= ax && ay >= az) {
        return y;
    }
    if (az >= ax && az >= ay) {
        return z;
    }
    return 0;
}

/* Prepare a fade channel: a full-screen semi-transparent tile in both
 * buffers, inactive, levels zero, additive blend. */
void func_8007D93C(s32 channel) {
    SetTile(&D_800B2078.fades[channel].tiles[0]);
    SetSemiTrans(&D_800B2078.fades[channel].tiles[0], 1);
    D_800B2078.fades[channel].tiles[0].w = 0x140;
    D_800B2078.fades[channel].tiles[0].h = 0xE0;
    D_800B2078.fades[channel].tiles[0].x0 = 0;
    D_800B2078.fades[channel].tiles[0].y0 = 0;
    D_800B2078.fades[channel].tiles[1] = D_800B2078.fades[channel].tiles[0];
    D_800B2078.fades[channel].active = 0;
    D_800B2078.fades[channel].steps = 0;
    D_800B2078.fades[channel].level[0] = D_800B2078.fades[channel].level[1] = D_800B2078.fades[channel].level[2] = 0;
    D_800B2078.fades[channel].abr = 2;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007DA44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007DCF8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007DECC);

/* Set a dialogue window's rectangle. */
void func_8007E114(s32 window, s32 x, s32 y, s32 w, s32 h) {
    D_800C2698[window].text.rect.x = x;
    D_800C2698[window].text.rect.y = y;
    D_800C2698[window].text.rect.w = w;
    D_800C2698[window].text.rect.h = h;
}

/* Place a quad's corners at (x, y) with size (w, h), optionally mirrored. */
void func_8007E16C(POLY_FT4 *poly, s32 x, s32 y, s32 w, s32 h, s32 mirror) {
    if (mirror == 0) {
        x--;
        poly->x0 = x;
        poly->x1 = x + w;
        poly->x2 = x;
        poly->x3 = x + w;
    } else {
        poly->x1 = x;
        poly->x0 = x + w;
        poly->x3 = x;
        poly->x2 = x + w;
    }
    poly->y0 = y;
    poly->y1 = y;
    poly->y2 = y + h;
    poly->y3 = y + h;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007E1C0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007EE0C);

extern DVECTOR D_800ADF34[]; /* icon texture origin per frame */

#ifdef NON_MATCHING
/* Point both buffers' icon of `window` at frame `frame`: a 64x64 texture
 * square and the frame's CLUT row. Differs only in the order of the first
 * two independent instructions (sll before lui). */
void func_8007F5AC(s32 window, s32 frame) {
    DialogueWindow *w;
    s16 *u;
    s16 *v;

    w = &D_800C2698[window];
    u = &D_800ADF34[frame].vx;
    v = &D_800ADF34[frame].vy;
    D_800C2698[window].icon[1].u0 = w->icon[0].u0 = *u;
    D_800C2698[window].icon[1].v0 = w->icon[0].v0 = *v;
    D_800C2698[window].icon[1].u1 = w->icon[0].u1 = *u + 0x40;
    D_800C2698[window].icon[1].v1 = w->icon[0].v1 = *v;
    D_800C2698[window].icon[1].u2 = w->icon[0].u2 = *u;
    D_800C2698[window].icon[1].v2 = w->icon[0].v2 = *v + 0x40;
    D_800C2698[window].icon[1].u3 = w->icon[0].u3 = *u + 0x40;
    D_800C2698[window].icon[1].v3 = w->icon[0].v3 = *v + 0x40;
    D_800C2698[window].icon[1].clut = w->icon[0].clut = GetClut(0, frame + 0xE0);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007F5AC);
#endif

/* Close dialogue window `window` unless it is busy; -1 when busy. */
s32 func_8007F6F8(s16 window) {
    if (D_800C2698[window].busy == 0) {
        func_80034614(&D_800C2698[window].text);
        func_800345E0(&D_800C2698[window].text);
        func_800346D4(&D_800C2698[window].text);
        D_800C2698[window].status = -1;
        D_800C2698[window].busy = -1;
        D_800C2698[window].cleared = -1;
        D_800C2698[window].age = 0xFFFF;
        D_800B068C[window] = -1;
        D_800B2078.open_windows &= (1 << window) ^ 0xFF;
        D_800C2698[window].owner = 0xFF;
        D_800C2698[window].unk412 = 0;
        return 0;
    }
    return -1;
}

/* The screen position of a point `height` above descriptor `index`. */
void func_8007F814(s32 index, s32 *x, s32 *y, s32 height) {
    SVECTOR point;
    MATRIX m;
    s32 screen;
    s32 depth;
    s32 flag;

    CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[index].matrix, &m);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    point.vx = 0;
    point.vy = height;
    point.vz = 0;
    RotTransPers(&point, &screen, &depth, &flag);
    *y = screen >> 16;
    *x = (s16)screen;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007F8DC);

/* Close every dialogue window that is not busy. */
void func_8007FFE8(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].busy == 0) {
            func_8007F6F8(window);
        }
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008004C);

/* Close each idle dialogue window whose timer ran out (unless flag 4 keeps
 * it) or that was cleared, and count the timers down. */
void func_800805F4(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].busy == 0) {
            if (D_800C2698[window].timer == 0 && !(D_800C2698[window].text.flags & 4)) {
                func_8007F6F8(window);
            }
            if (D_800C2698[window].cleared == 0) {
                func_8007F6F8(window);
            }
            if (D_800C2698[window].timer != 0) {
                D_800C2698[window].timer--;
            }
        }
    }
}

/* The first dialogue window whose age is zero, or 0xffff. */
s32 func_800806E4(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age == 0) {
            return window;
        }
    }
    return 0xFFFF;
}

/* 0 when a dialogue window is free, else -1. */
s32 func_80080720(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age == 0xFFFF) {
            return 0;
        }
    }
    return -1;
}

/* The oldest dialogue window in use, or 0xffff. */
s32 func_80080760(void) {
    s32 oldest_age = 0;
    s32 oldest = 0xFFFF;
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age != 0xFFFF && D_800C2698[window].age >= oldest_age) {
            oldest_age = D_800C2698[window].age;
            oldest = window;
        }
    }
    return oldest;
}

/* Age the windows in use and take the first free one (age 0); 0xffff when
 * all are in use. */
s32 func_800807B4(void) {
    s32 window;

    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age != 0xFFFF) {
            D_800C2698[window].age++;
        }
    }
    for (window = 0; window < 4; window++) {
        if (D_800C2698[window].age == 0xFFFF) {
            D_800C2698[window].age = 0;
            return window;
        }
    }
    return 0xFFFF;
}

/* Release an event actor's blocks, its record, its descriptor block and
 * its model. */
void func_8008083C(s32 index) {
    FieldActor *actor;

    if (index < D_800ADBFC) {
        actor = D_800AF880.components.descriptors[index].actor;
        if (actor->unk134 & 0x80) {
            func_800320E8(actor->unk110);
        }
        if (actor->state.word & 0x1000) {
            func_800320E8(actor->unk114);
        }
        if (D_800AF880.components.descriptors[index].flags & 0x2000) {
            func_800320E8(actor->list);
        }
        if (actor->unk124 != -1) {
            func_800320E8(actor->unk120);
        }
        func_800320E8(actor);
        func_800320E8(D_800AF880.components.descriptors[index].unk08);
        func_800230A8(D_800AF880.components.descriptors[index].model);
    }
}

/* The collision attribute under an actor on its layer, or 0 when the layer
 * is switched off for it. */
u32 func_80080968(FieldActor *actor) {
    s16 layer = actor->layer;

    if ((actor->layer_flags >> (layer + 3)) & 1) {
        return 0;
    }
    return D_800AF880.components.collision_attributes[D_800AF880.components.collision_triangles[layer][actor->triangle[layer]].attribute].word;
}

/* Frames (in 800b14ac, two per step) and height of a jump under the actor's
 * gravity from the fixed launch speed. */
s32 func_800809D0(FieldActor *actor) {
    s32 speed = -0x14D000;
    s32 height = 0;

    D_800B14AC = 0;
    do {
        height += speed;
        speed += actor->gravity.value;
        D_800B14AC += 2;
    } while (speed <= 0);
    return height >> 16;
}

/* The next word of the current descriptor's actor list. */
s32 func_80080A18(void) {
    return D_800AF880.components.descriptors[D_800ADB58].actor->list[D_800ADB5C++];
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80080A74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80080F44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008110C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800815F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80081C54);

/* -1 when the actor's bits 9-10 meet bits 3-4 of +14, else 0. */
s32 func_80081F5C(FieldActor *actor) {
    u32 bits = (actor->flags >> 9) & 3;

    return -((bits & (actor->unk014 >> 3)) != 0);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80081F80);

extern void func_800245D8(void *model, s32 animation);
extern u8 D_800ADFB8[];
/* Start animation `animation` on a descriptor's model (flag 0x40 set):
 * clears the actor's flag 0x800 outside jumps or on a change; 801e layer
 * actors (layer bit 13) set their layer frame instead (below 0x10 through
 * the 800adfb8 table). */
void func_800821F4(void *model, s32 animation, FieldDescriptor *descriptor) {
    if (!(descriptor->flags & 0x40)) {
        return;
    }
    if (animation != 3 && D_800B2078.jump_mode == 0) {
        descriptor->actor->flags &= ~0x800;
    }
    if (animation == 0xFF) {
        animation = 0;
    }
    if (animation != D_800B2078.animation_mode) {
        descriptor->actor->flags &= ~0x800;
    }
    if (!(descriptor->actor->layer_flags & 0x2000)) {
        if (!(descriptor->actor->layer_flags & 0x1000000)) {
            func_800245D8(model, animation);
        }
    } else if (animation < 0x10) {
        func_801E8330(descriptor->actor->state.bits.layer, 0, D_800ADFB8[animation]);
        D_800B2078.unk21E4[descriptor->actor->state.bits.layer] = D_800ADFB8[animation];
    } else {
        animation -= 0x10;
        func_801E8330(descriptor->actor->state.bits.layer, 0, animation);
        D_800B2078.unk21E4[descriptor->actor->state.bits.layer] = animation;
    }
}

typedef struct {
    u8 unk00[0x18];
    u16 half_x;  /* 18 */
    u8 unk1A[2];
    u16 half_z;  /* 1C */
    u8 unk1E[4];
    s16 x;       /* 22 */
    u8 unk24[6];
    s16 z;       /* 2A */
} FieldBox;
extern void func_80281678(FieldBox *box);
/* -1 unless point (x, z) lies inside `box` grown by `margin`; inside, run
 * 80281678 on it (unless 800c268c is set) and return 0. */
s32 func_8008237C(s32 x, s32 z, FieldBox *box, s32 margin) {
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    point = (x << 16) + z;
    a = ((box->x - box->half_x - margin) << 16) + (box->z + box->half_z + margin);
    b = ((box->x + box->half_x + margin) << 16) + (box->z + box->half_z + margin);
    c = ((box->x + box->half_x + margin) << 16) + (box->z - box->half_z - margin);
    d = ((box->x - box->half_x - margin) << 16) + (box->z - box->half_z - margin);
    if (func_8004A70C(a, b, point) < 0 || func_8004A70C(b, c, point) < 0 ||
        func_8004A70C(c, d, point) < 0 || func_8004A70C(d, a, point) < 0) {
        return -1;
    }
    if (D_800C268C == 0) {
        func_80281678(box);
    }
    return 0;
}

/* 0 when the actor has no quad (state bit 12); else -1 unless its position
 * plus `offset` lies inside the quad at +114. */
s32 func_80082494(s32 *offset, FieldActor *actor) {
    s16 *quad;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    if (!(actor->state.word & 0x1000)) {
        return 0;
    }
    point = (((actor->position[0] + offset[0]) >> 16) << 16) + ((actor->position[2] + offset[2]) >> 16);
    quad = actor->unk114;
    a = (quad[0] << 16) + quad[1];
    b = (quad[2] << 16) + quad[3];
    c = (quad[4] << 16) + quad[5];
    d = (quad[6] << 16) + quad[7];
    if (func_8004A70C(a, b, point) < 0 || func_8004A70C(b, c, point) < 0 ||
        func_8004A70C(c, d, point) < 0) {
        return -1;
    }
    return func_8004A70C(d, a, point) >> 31;
}

/* Planar distance between two descriptors' actors (integer positions). */
s32 func_800825AC(s32 from, s32 to) {
    s32 to_x = D_800AF880.components.descriptors[to].actor->position[0] >> 16;
    s32 to_z = D_800AF880.components.descriptors[to].actor->position[2] >> 16;
    s32 from_x = D_800AF880.components.descriptors[from].actor->position[0] >> 16;
    s32 from_z = D_800AF880.components.descriptors[from].actor->position[2] >> 16;

    return func_80099A4C(to_x - from_x, to_z - from_z);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80082620);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80082BB8);

/* Ease `value` 0x4000 towards zero, bounded by +-`limit`. */
s32 func_80083178(s32 value, s32 limit) {
    if (value < 0) {
        value += 0x4000;
        if (value < -limit) {
            value = -limit;
        }
        if (value > 0) {
            value = 0;
        }
    } else {
        value -= 0x4000;
        if (value > limit) {
            value = limit;
        }
        if (value < 0) {
            value = 0;
        }
    }
    return value;
}

/* The integer parts of a 16.16 vector. */
void func_800831D0(SVECTOR *out, VECTOR *in) {
    out->vx = in->vx >> 16;
    out->vy = in->vy >> 16;
    out->vz = in->vz >> 16;
}

#ifdef NON_MATCHING
/* While the actor moves, turn its heading a quarter (left with flag bit 0,
 * else right) once, apply it, and mark the heading as turned. */
void func_800831F4(void *owner, FieldActor *actor, s32 unused, s32 flags) {
    s32 heading;

    if (actor->unk030[0] != 0 || actor->unk030[2] != 0) {
        heading = actor->heading_goal;
        if (!(heading & 0x8000)) {
            if (!(flags & 1)) {
                heading += 0x400;
            } else {
                heading -= 0x400;
            }
            actor->heading = actor->heading_goal = heading & 0xFFF;
            func_80081F80(owner, actor->heading);
            actor->heading = actor->heading_goal = actor->heading_goal | 0x8000;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800831F4);
#endif

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80083288);

void func_80083994(void) {
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008399C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80084158);

#ifdef NON_MATCHING
/* -1 when the actor's motion, collision state or layer prevents idling. */
s32 func_8008492C(FieldActor *actor) {
    if ((actor->unk014 & 0x420000) || D_800ADB98 != 0 || actor->unk030[0] != 0 ||
        actor->unk030[1] != 0 || actor->unk030[2] != 0 || D_800ADC0C != 1 || actor->unk074 != 0xFF ||
        (actor->flags & 0x401800)) {
        return -1;
    }
    if ((actor->layer_flags & 1) && actor->layer == 0) {
        return -1;
    }
    if ((actor->layer_flags & 2) && actor->layer == 1) {
        return -1;
    }
    if (actor->layer_flags & 4) {
        return -(actor->layer == 2);
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008492C);
#endif

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80084A40);

/* One music-wave stream step: pass arrivals to the chunk callback; -1 once
 * the stream finished and its ring is released. */
s32 func_800854D0(void) {
    s32 arrived = func_80028B14();

    D_800ADBBC = arrived;
    if (arrived != 0) {
        D_800AFEA4(arrived);
        return 0;
    }
    if (func_800286CC() != 0) {
        return 0;
    }
    if (D_800ADBBC != 0) {
        return 0;
    }
    func_800320E8(D_800ADBB8);
    D_800ADB2C = 0;
    return -1;
}

/* Start streaming music-wave `file` into an eight-sector ring with a chunk
 * callback. */
void func_80085560(s32 file, s32 unused, void (*callback)(s32)) {
    void *ring;

    D_800ADB2C = 1;
    D_800ADBB8 = ring = func_8002A260(8, unused);
    func_800295D8(file, ring, 0, 0x100);
    D_800AFEA4 = callback;
}

#ifdef NON_MATCHING
/* Play sound effect `id` on voice pair `channel` at a volume and pan. */
void func_800855C8(s32 id, s32 volume, s32 pan, s32 channel) {
    channel &= 7;
    func_8003A20C(channel * 2);
    func_80039F9C(id, channel * 2, volume, pan);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800855C8);
#endif

/* Play sound effect `id` on `channel` at full volume and centre pan; id 0
 * stops the channel. */
void func_80085634(s32 id, s32 channel) {
    channel &= 7;
    if (id == 0) {
        func_8003A20C(channel * 2);
    } else {
        D_800B2078.last_sound_effect = id;
        func_800855C8(id, 0x7F, 0x40, channel);
    }
}

/* Play the movie sound effects whose time (from 800c3a2c) has come: each
 * timeline entry holds a time and a sound (id in the low byte, voice pair
 * in bits 8-10). */
void func_80085678(void) {
    u16 *times;
    u16 *sounds;
    s32 sound;

    if (D_800C3A38 == 0xFF) {
        return;
    }
    times = &D_800AE060[0][0];
    sounds = &D_800AE060[0][1];
    for (;;) {
        if (D_800B06A0 < times[D_800C3A64 * 2] + D_800C3A2C) {
            return;
        }
        sound = sounds[D_800C3A64 * 2];
        func_80039EC4((sound & 0xFF) | (D_800B235C->id << 16), ((sound >> 8) & 7) * 2);
        D_800C3A64++;
    }
}

/* Release a movie's sound-effect bank, when one is loaded. */
void func_80085738(void) {
    if (D_800C3A38 != 0xFF) {
        func_80039FF8();
        func_8003852C(D_800B235C);
        func_800320E8(D_800B235C);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085788);

/* Load the field's sound-effect bank (file 0xa8), from the disc or from the
 * copy at 8005a4bc, and open it. */
void func_80085890(void) {
    s32 size;

    func_80028470(4, 0);
    size = func_800288EC(0xA8);
    D_8006259C = func_80031BDC(size, 0);
    func_800320A4(D_8006259C);
    if (D_8004F32C == -1) {
        func_800295D8(0xA8, D_8006259C, 0, 0x80);
        func_80028A60(0);
    } else {
        memcpy(D_8006259C, D_8005A4BC, size);
        func_800320B8(D_8005A4BC);
        func_800320E8(D_8005A4BC);
    }
    func_80038428(D_8006259C);
    func_8003BDFC(0x10);
    func_80028470(4, 0);
    D_8004F32C = -1;
}

/* Unlink and release the field's sound-effect bank. */
void func_80085988(void) {
    func_8003852C(D_8006259C);
    func_800320B8(D_8006259C);
    func_800320E8(D_8006259C);
    D_8004F32C = -1;
}

#ifdef NON_MATCHING
/* Music-wave chunk callback: gather four 2 KiB chunks and open them as a
 * wave bank; later chunks feed the bank. */
void func_800859DC(WaveChunk *chunk) {
    if (D_800B2078.wave_chunks < 0) {
        return;
    }
    if (D_800B2078.wave_chunks < 4) {
        ((WaveChunk *)D_800C3A1C)[D_800B2078.wave_chunks] = *chunk;
        D_800B2078.wave_chunks++;
        func_8002945C(chunk);
        if (D_800B2078.wave_chunks == 4) {
            D_8006258C = func_800380D0(D_800C3A1C, 0x2000, 0);
        }
    } else if (D_800B2078.wave_chunks == 4) {
        func_8003BDFC(0x10);
        *(WaveChunk *)D_800C3A1C = *chunk;
        func_8003827C(D_800C3A1C, 0x800);
        func_8002945C(chunk);
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800859DC);
#endif

#ifdef NON_MATCHING
/* Change the field music to `music` (0xff: none): release the shared wave
 * bank when the entry asks, then stream its wave file through 800859dc. */
void func_80085B20(s32 music) {
    u8 wave;

    func_80028A60(0);
    func_8001B66C();
    if (music == 0xFF) {
        D_8004F308 = 0;
        return;
    }
    func_80028470(0x1C, 0);
    if (D_800ADFCC[music][1] == 1) {
        func_80086024();
    }
    wave = D_800ADFCC[music][0];
    if (wave != 0xFF && D_8004F33C != wave) {
        func_80085560(wave * 2 + 0x13, 1, (void (*)(s32))func_800859DC);
        D_8004F354 = 1;
        D_800B2078.wave_chunks = 0;
        D_800C3A1C = func_80031BDC(0x2000, 1);
    }
    func_80028470(4, 0);
    D_8004F308 = -1;
    D_800AFC54 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085B20);
#endif

/* Run up to five stream steps; 0 once the stream finished, else -1. */
s32 func_80085C3C(void) {
    s32 steps;

    for (steps = 0; steps < 5; steps++) {
        if (func_800854D0() == -1) {
            return 0;
        }
    }
    return -1;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085C90);

/* Stop and release the cached sequence. */
void func_80085EEC(void) {
    if (D_8004F2FC != 0) {
        func_80039C4C(D_8004F2FC);
        func_800399D4(D_8004F2FC);
        D_8004F2FC = 0;
    }
}

/* Once the disc is idle, open the shared wave bank from its buffer and
 * release the buffer; -1 while still reading. */
s32 func_80085F30(void) {
    s32 bank;

    if (func_800286CC() != 0) {
        return -1;
    }
    bank = func_80037FD8(D_800B00E0, 0);
    D_8006251C = bank;
    D_80059560 = bank;
    func_8003BDFC(0x10);
    func_800320E8(D_800B00E0);
    D_8004F364 = 1;
    D_8004F384 = 0;
    D_8004F368 = 0;
    return 0;
}

/* Start reading the shared wave bank (file 3 of directory 0x1c). */
void func_80085FB8(void) {
    void *buffer;

    func_80028470(0x1C, 0);
    D_800B00E0 = buffer = func_80031BDC(func_800288EC(3), 1);
    func_800295D8(3, buffer, 0, 0x80);
    func_80028470(4, 0);
    D_8004F364 = 0x80;
}

/* Release the shared wave bank once and mark it unloaded. */
void func_80086024(void) {
    if (D_8004F368 == 0) {
        D_8004F384 = 1;
        func_80038310(D_8006251C);
        D_8004F368 = 1;
    }
    D_8004F364 = 0;
}

/* A positional emitter's volume at `distance`: full at the source, falling
 * linearly to half at the range 800b21ac. */
void func_80086078(s32 distance, u32 *out, s32 volume) {
    s32 level;

    if (distance > D_800B2078.emitter_range) {
        distance = D_800B2078.emitter_range;
    }
    level = 0x80 - (((0x7F0000 / D_800B2078.emitter_range) * distance) >> 16);
    *out = ((u32)(level << 16) / 127 * volume) >> 16;
}

/* Update the voices of the emitter following descriptor `id`: volume by
 * distance, pan by the descriptor's screen X. */
void func_800860F0(s32 unused0, s32 volume, s32 unused2, s32 distance, s32 id) {
    s32 i;
    s32 voice;
    s32 pan;
    u32 level;
    s32 x;
    s32 y;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].actor == id) {
            voice = i * 2;
            func_80086078(distance, &level, volume);
            func_80086200(id, &x, &y);
            if (x > 0x140) {
                x = 0x13F;
            }
            if (x < 0) {
                x = 0;
            }
            pan = (x * 0x6666) >> 16;
            func_8003A344(voice, level);
            func_8003A55C(voice, pan);
        }
    }
}

#ifdef NON_MATCHING
/* The screen position of descriptor `index`. */
void func_80086200(s32 index, s32 *x, s32 *y) {
    SVECTOR point;
    MATRIX m;
    s32 screen;
    s32 depth;
    s32 flag;

    func_8009CDB4(1);
    CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[index].matrix, &m);
    point.vx = 0;
    point.vy = 0;
    point.vz = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTransPers(&point, &screen, &depth, &flag);
    *y = screen >> 16;
    *x = (s16)screen;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086200);
#endif

#ifdef NON_MATCHING
/* Start `sound` on the first free emitter, following descriptor `actor`,
 * with volume by distance and pan by screen X. */
void func_800862CC(u16 sound, s32 volume, s32 unused, s32 distance, s32 actor) {
    s32 i;
    u32 level;
    s32 x;
    s32 y;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].sound == 0xFFFF) {
            D_800AFE88[i].sound = sound;
            D_800AFE88[i].actor = actor;
            func_80086078(distance, &level, volume);
            func_80086200(actor, &x, &y);
            if (x > 0x140) {
                x = 0x13F;
            }
            if (x < 0) {
                x = 0;
            }
            func_8003A20C(i * 2);
            func_80039F9C(sound, i * 2, level, (x * 0x6666) >> 16);
            return;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800862CC);
#endif

/* Stop the emitter following descriptor `id`, freeing its slot. */
void func_800863E8(s32 id) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].actor == id) {
            func_8003A20C(i * 2);
            D_800AFE88[i].sound = 0xFFFF;
            D_800AFE88[i].actor = 0xFFFF;
            return;
        }
    }
}

/* The emitter slot following descriptor `id`, or -1. */
s32 func_80086470(s32 owner, s32 id) {
    s32 i;

    if (owner == -1) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].actor == id) {
            return i;
        }
    }
    return -1;
}

/* Clear the emitter slots. */
void func_800864B4(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800AFE88[i].actor = 0xFFFF;
        D_800AFE88[i].sound = 0xFFFF;
    }
}

/* Clear the emitter slots and stop the voices of the emitters in use. */
void func_800864F0(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800AFE88[i].sound = 0xFFFF;
        D_800AFE88[i].actor = 0xFFFF;
    }
    for (i = 0; i < 4; i++) {
        if (!(D_800B2078.effects_kept & 1)) {
            func_8003A20C(i * 2);
        }
        D_800B2078.effects_kept >>= 1;
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086590);

/* Point the listener (80086590) at the controlled actor, the camera eye or
 * the camera target, as 800b22e0 selects. */
void func_80086908(void) {
    switch (D_800B2078.unk22E0) {
    case 0:
        func_80086590((VECTOR *)D_800AF880.components.descriptors[D_800B2078.controlled].actor->position);
        break;
    case 1:
        func_80086590(&D_800AF880.eye);
        break;
    case 2:
        func_80086590(&D_800AF880.target);
        break;
    }
}

/* Event opcode fe: run the extended instruction named by the next byte. */
void func_800869B8(void) {
    D_800AE6A0[D_800ADC00[++D_800B0078->pc]]();
}

/* Scale emitter `emitter`'s per-step delta ((22e8 - 2300) / 2318) by the
 * steps left once its distance from `position` is covered, into 223c. */
void func_80086A1C(s32 emitter, s32 *position) {
    s32 dx;
    s32 dy;
    s32 dz;
    s32 left;

    dx = ((D_800B2078.unk22E8[emitter][0] - D_800B2078.unk2300[emitter][0]) << 16) / D_800B2078.unk2318[emitter];
    dy = ((D_800B2078.unk22E8[emitter][1] - D_800B2078.unk2300[emitter][1]) << 16) / D_800B2078.unk2318[emitter];
    dz = ((D_800B2078.unk22E8[emitter][2] - D_800B2078.unk2300[emitter][2]) << 16) / D_800B2078.unk2318[emitter];
    left = D_800B2078.unk2318[emitter] - func_80099A04(D_800B2078.emitter_position[emitter][0] - WHOLE(position[0]),
                                                        D_800B2078.emitter_position[emitter][1] - WHOLE(position[1]),
                                                        D_800B2078.emitter_position[emitter][2] - WHOLE(position[2]));
    D_800B2078.unk223C[0][emitter] = (dx * left) >> 16;
    D_800B2078.unk223C[1][emitter] = (dy * left) >> 16;
    D_800B2078.unk223C[2][emitter] = (dz * left) >> 16;
}

/* Update the three positional emitters from their actors' positions. */
void func_80086BA8(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800B2078.emitter_descriptor[i] != -1) {
            func_80086A1C(i, D_800AF880.components.descriptors[D_800B2078.emitter_descriptor[i]].actor->position);
        }
    }
}

/* Event: selector byte 1: 0 skips; 1 starts emitter record 0 on the current
 * actor with operand 4 (0x27 selects mode 0x20, else 0x22). */
void func_80086C34(void) {
    s32 kind;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        D_800B0078->pc += 2;
        break;
    case 1:
        func_800ACDEC(2);
        kind = func_800ACDEC(4);
        func_800ACDEC(6);
        func_800A94A4(D_800AFD1C);
        D_800B02CC[0].flags = 0x14;
        D_800B02CC[0].unk00 = 1;
        D_800B02CC[0].count = 0x10;
        D_800B02CC[0].unk72 = 0;
        D_800B02CC[0].unk74 = kind;
        if (kind == 0x27) {
            D_800B02CC[0].unk74 = 0x20;
        } else {
            D_800B02CC[0].unk74 = 0x22;
        }
        D_800B02CC[0].unk04 = 0x1000;
        func_800A99A8(D_800AFD1C);
        D_800B0078->pc += 8;
        break;
    }
}

/* Event opcode e2: call resident 80019cd0, yield and step over the opcode. */
void func_80086D4C(void) {
    func_80019CD0();
    D_800B00C0 = 1;
    D_800B0078->pc++;
}

/* Set both draw blocks' display screen rectangles (0, 10, 256, 216). */
void func_80086D8C(void) {
    D_800B249C[0].disp.screen.x = 0;
    D_800B249C[0].disp.screen.y = 10;
    D_800B249C[0].disp.screen.w = 0x100;
    D_800B249C[0].disp.screen.h = 0xD8;
    D_800B249C[1].disp.screen.x = 0;
    D_800B249C[1].disp.screen.y = 10;
    D_800B249C[1].disp.screen.w = 0x100;
    D_800B249C[1].disp.screen.h = 0xD8;
}

/* Event opcode e0: set 800b2358 from its byte operand. */
void func_80086DE0(void) {
    D_800B2078.unk2358 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: switch to the 640-wide display (op1 0), or set (1) / clear (2)
 * 800adb54. */
void func_80086E1C(void) {
    RECT rect;

    switch (func_800ACDEC(1)) {
    case 0:
        rect.w = 0x500;
        rect.x = 0;
        rect.y = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x280, 0xE0);
        SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x280, 0xE0);
        func_80086D8C();
        break;
    case 1:
        D_800ADB54 = 1;
        break;
    case 2:
        D_800ADB54 = 0;
        break;
    }
    D_800B0078->pc += 3;
}

/* Event: set the current actor's +128 to (op1 << 12) | op3. */
void func_80086F7C(void) {
    s32 high = func_800ACDEC(1);

    D_800B0078->unk128 = (high << 12) | func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: effect control by selector byte: stop (0), start with four
 * operands (1), pause (2) or restart with four operands (3). */
void func_80086FD0(void) {
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        func_800AAC08();
        D_800B0078->pc += 2;
        break;
    case 2:
        func_800AABD8();
        D_800B0078->pc += 2;
        break;
    case 1:
        func_800AAE4C(func_800ACDEC(2), func_800ACDEC(4), func_800ACDEC(6), func_800ACDEC(8));
        D_800B0078->pc += 10;
        break;
    case 3:
        func_800AADC8(func_800ACDEC(2), func_800ACDEC(4), func_800ACDEC(6), func_800ACDEC(8));
        D_800B0078->pc += 10;
        break;
    }
}

/* Event: set bits op3 in the flags of game record op1. */
void func_80087148(void) {
    s32 record = func_800ACDEC(1);
    s32 bits = func_800ACDEC(3);

    D_8005A39C->records[record].flags |= bits;
    D_800B0078->pc += 5;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800871B0);

/* Event: store op3 in byte op1 of the table at 800b225f. */
void func_800873C4(void) {
    s32 index = func_800ACDEC(1);

    D_800B2078.unk225F[index] = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: variables op13 and op15 receive op1 * op9 / op5 and
 * op3 * op11 / op7 (16.16 intermediate). */
void func_80087420(void) {
    s32 a = func_800ACDEC(1);
    s32 b = func_800ACDEC(3);
    s32 c = func_800ACDEC(5);
    s32 d = func_800ACDEC(7);
    s32 e = func_800ACDEC(9);
    s32 f = func_800ACDEC(11);
    s32 first = (((e << 16) / c) * a) >> 16;
    s32 second = (((f << 16) / d) * b) >> 16;

    func_800A3074(func_800ACDB8(13) & 0xFFFF, first);
    func_800A3074(func_800ACDB8(15) & 0xFFFF, second);
    D_800B0078->pc += 17;
}

/* Event: skip a two-byte operand. */
void func_8008752C(void) {
    D_800B0078->pc += 3;
}

/* Event: set game flag 0x4000 of +22b6. */
void func_8008754C(void) {
    D_8005A39C->unk22B6 |= 0x4000;
    D_800B0078->pc++;
}

/* Event: copy character op1 over character op3. */
void func_80087580(void) {
    s32 from = func_800ACDEC(1);

    D_8005A39C->gears[func_800ACDEC(3)] = D_8005A39C->gears[from];
    D_800B0078->pc += 5;
}

#ifdef NON_MATCHING
/* Event: copy character slot and record op1 over op3; slots 9 and 10 set
 * game flags 0x2000 / 0x1000. */
void func_8008764C(void) {
    s32 from = func_800ACDEC(1);
    s32 to = func_800ACDEC(3);

    D_8005A39C->characters[to] = D_8005A39C->characters[from];
    D_8005A39C->records[to] = D_8005A39C->records[from];
    if (to == 9) {
        D_8005A39C->unk22B6 |= 0x2000;
    }
    if (to == 10) {
        D_8005A39C->unk22B6 |= 0x1000;
    }
    D_800B0078->pc += 5;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008764C);
#endif

/* Event: store 80050622 in variable op1. */
void func_80087800(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_80050622);
    D_800B0078->pc += 3;
}

/* Event: once sound is idle, stop it and set the six sound bytes at
 * 8005061c from operands; wait otherwise. */
void func_80087848(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1) {
        D_800B00C0 = 1;
        D_800B0078->pc--;
        return;
    }
    func_800379B4(0);
    D_8005061C[0] = func_800ACDEC(1);
    D_8005061C[1] = func_800ACDEC(3);
    D_8005061C[2] = func_800ACDEC(5);
    D_8005061C[3] = func_800ACDEC(7);
    D_8005061C[4] = func_800ACDEC(9);
    D_8005061C[5] = func_800ACDEC(11);
    D_800ADB88 = 1;
    D_800ADBE8 = 0;
    D_800B0078->pc += 13;
}

/* Event: store the game's +1844 and +1846 in variables op1 and op3. */
void func_80087960(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk1844);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk1846);
    D_800B0078->pc += 5;
}

/* Event: store the game's +184e and +1852 in variables op1 and op3. */
void func_800879D0(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk184E);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk1852);
    D_800B0078->pc += 5;
}

/* Event: set 800b2357 from its byte operand. */
void func_80087A40(void) {
    D_800B2078.unk2357 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set 800b2354 from its byte operand. */
void func_80087A7C(void) {
    D_800B2078.unk2354 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set the game's +184e and +1852 from operands 1 and 3 (immediate by
 * flags 0x80/0x40 of byte 9) and reset +1850/+1854, setting +1856. */
void func_80087AB8(void) {
    D_8005A39C->unk184E = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk1852 = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk1854 = 0;
    D_8005A39C->unk1850 = 0;
    D_8005A39C->unk1856 = 1;
    D_800B0078->pc += 6;
}

/* Event: store the game's four halfwords at +182c in variables op1..op7. */
void func_80087B5C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk182C[0]);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk182C[1]);
    func_800A3074(func_800ACDB8(5) & 0xFFFF, D_8005A39C->unk182C[2]);
    func_800A3074(func_800ACDB8(7) & 0xFFFF, D_8005A39C->unk182C[3]);
    D_800B0078->pc += 9;
}

/* Event: set 8004f300. */
void func_80087C0C(void) {
    D_8004F300 = 1;
    D_800B0078->pc++;
}

/* Event: set the game's four halfwords at +182c from operands 1..7
 * (immediate by flags 0x80/0x40/0x20/0x10 of byte 9). */
void func_80087C34(void) {
    D_8005A39C->unk182C[0] = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk182C[1] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk182C[2] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk182C[3] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: store the game's +1834 in variable op1. */
void func_80087D30(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk1834);
    D_800B0078->pc += 3;
}

/* Event: set the game's +1834 from operand 1 (immediate when flag 0x80 of
 * byte 3 is set). */
void func_80087D80(void) {
    D_8005A39C->unk1834 = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    D_800B0078->pc += 4;
}

/* Event: set 800b2355 (selector byte 0) or 800b2356 from operand 2. */
void func_80087DE0(void) {
    s32 value = func_800ACDEC(2);

    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        D_800B2078.unk2355 = value;
    } else {
        D_800B2078.unk2356 = value;
    }
    D_800B0078->pc += 4;
}

/* Event: set the battle-entry override (800b234c) from operand 1. */
void func_80087E5C(void) {
    D_800B2078.battle_override = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: make the selected actor the controlled one (clearing every actor's
 * control flags first). */
void func_80087E98(void) {
    s32 index = func_8009CDB4(1);
    s32 i;

    if (index != 0xFF) {
        if (index == D_8005A444[0]) {
            D_800B2078.followers_idle = 0;
        } else {
            D_800B2078.followers_idle = 1;
        }
        D_800B2078.controlled = index;
        D_800B2078.unk233E = index;
        for (i = 0; i < D_800ADBFC; i++) {
            D_800AF880.components.descriptors[i].actor->flags &= ~0x01004000;
        }
        D_800AF880.components.descriptors[index].actor->flags |= 0x4000;
    }
    D_800B0078->pc += 2;
}

/* Event: count 800b2348 up. */
void func_80087FA4(void) {
    D_800B2078.unk2348++;
    D_800B0078->pc++;
}

/* Event: run 800a8ba4. */
void func_80087FD4(void) {
    func_800A8BA4();
    D_800B0078->pc++;
}

extern void func_801E72CC(MATRIX *m, MATRIX *work, s32 a, s32 b);
/* Event: rotate the vector operands 5/7/9 by the rotation built (801e72cc)
 * from operands 1 and 3 and store the result in variables 0xc, 0xe, 0x10. */
void func_8008800C(void) {
    MATRIX m;
    MATRIX work;
    SVECTOR in;
    SVECTOR out;
    s32 flag;
    s32 a;

    m.t[0] = m.t[1] = m.t[2] = 0;
    a = func_8009CF78(1, EVENT_OPERAND_BYTE(0xB));
    func_801E72CC(&m, &work, a, func_8009CFBC(3, EVENT_OPERAND_BYTE(0xB)));
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    in.vx = func_8009D000(5, EVENT_OPERAND_BYTE(0xB));
    in.vy = func_8009D044(7, EVENT_OPERAND_BYTE(0xB));
    in.vz = func_8009D088(9, EVENT_OPERAND_BYTE(0xB));
    RotTransSV(&in, &out, &flag);
    func_800A3074(func_800ACDB8(0xC) & 0xFFFF, out.vx);
    func_800A3074(func_800ACDB8(0xE) & 0xFFFF, out.vy);
    func_800A3074(func_800ACDB8(0x10) & 0xFFFF, out.vz);
    D_800B0078->pc += 0x12;
}

/* Event: restore every character's two gauges to their maxima. */
void func_80088198(void) {
    s32 i;
    GameState *state = D_8005A39C;

    for (i = 0; i < 20; i++) {
        state->gears[i].points = state->gears[i].points_max;
        state->gears[i].gauge = state->gears[i].gauge_max;
    }
    D_800B0078->pc++;
}

/* Event: pause (selector 0) or resume the particles' VRAM. */
void func_800881E8(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        func_800A915C();
    } else {
        func_800A91F0();
    }
    D_800B0078->pc += 2;
}

/* Event: wait until the pending sound is resolved, yielding each time. */
void func_8008825C(void) {
    if (D_8004F308 == -1) {
        D_800B0078->pc--;
    } else {
        D_800B0078->pc++;
    }
    D_800B00C0 = 1;
}

/* Event: store character op1's slot byte +2c (0xff for none) in variable
 * op3. */
void func_800882B8(void) {
    s32 character = func_8008CF3C(func_800ACDEC(1));

    if (character != 0xFF) {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->characters[character].unkA0);
    } else {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, 0xFF);
    }
    D_800B0078->pc += 5;
}

/* Event: set byte +4 of party slot op1 to op3. */
void func_80088360(void) {
    s32 slot = func_800ACDEC(1);

    D_8005A39C->characters[slot].unkA0 = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: set (selector 0) or clear character op2's bit of the game's +2318. */
void func_800883D4(void) {
    s32 character = func_8008CF3C(func_800ACDEC(2));

    if (character != 0xFF) {
        if (D_800ADC00[D_800B0078->pc + 1] == 0) {
            D_8005A39C->unk2318 |= 1 << character;
        } else {
            D_8005A39C->unk2318 &= ~(1 << character);
        }
    }
    D_800B0078->pc += 4;
}

#include "field_script.h"

/* Event: set 800b236c to the inverse of its byte operand's low bit. */
void func_8008848C(void) {
    D_800B236C = EVENT_OPERAND_BYTE(1) ^ 1;
    D_800B0078->pc += 2;
}

/* Event: set the sound-emitter range from operand 1. */
void func_800884CC(void) {
    D_800B2078.emitter_range = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: for party member operand 5 (0xff none), store its model's
 * animation +0c in variable operand 1 and its descriptor in variable
 * operand 3, clearing +0c unless it is 1; four batch steps. */
void func_80088508(void) {
    s32 member;
    FieldModel *model;

    member = D_8005A444[func_800ACDEC(5)];
    D_800AFC7C += 4;
    if (member != 0xFF) {
        model = D_800AF880.components.descriptors[member].model;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, model->animation->unk0C);
        func_800A3074(func_800ACDB8(3) & 0xFFFF, member);
        if (model->animation->unk0C != 1) {
            model->animation->unk0C = 0;
        }
    } else {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, 0);
        func_800A3074(func_800ACDB8(3) & 0xFFFF, 0);
    }
    D_800B0078->pc += 7;
}

/* Clear the eight pairs at +30 of the current emitter record. */
void func_8008861C(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        D_800B02CC[D_800B2078.unk2384].unk30[i][0] = D_800B02CC[D_800B2078.unk2384].unk30[i][1] = 0;
    }
}

/* Event: start effect op3 (0..3 map to 0, 0x10, 0x20, 0x30) with op5 and op7
 * for actor op1, using four batch steps. */
void func_80088674(void) {
    s32 actor = func_800ACDEC(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    D_800B2078.unk2374 = func_800ACDEC(1);
    D_800B2078.unk2378 = func_800ACDEC(3);
    D_800B2078.unk237C = func_800ACDEC(5);
    D_800B2078.unk2380 = func_800ACDEC(7);
    D_800B0078->pc += 9;
    func_800A94A4(actor);
    switch (D_800B2078.unk2378) {
    case 0:
        D_800B2078.unk2378 = 0;
        break;
    case 1:
        D_800B2078.unk2378 = 0x10;
        break;
    case 2:
        D_800B2078.unk2378 = 0x20;
        break;
    case 3:
        D_800B2078.unk2378 = 0x30;
        break;
    }
    D_800AFC7C += 4;
}

/* Event: start effect op2 (0..3 map to 0, 0x10, 0x20, 0x30) with op4 and op6
 * on the selected actor, using four batch steps. */
void func_80088790(void) {
    s32 actor = func_8009CDB4(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    D_800B2078.unk2374 = actor;
    D_800B2078.unk2378 = func_800ACDEC(2);
    D_800B2078.unk237C = func_800ACDEC(4);
    D_800B2078.unk2380 = func_800ACDEC(6);
    D_800B0078->pc += 8;
    func_800A94A4(actor);
    switch (D_800B2078.unk2378) {
    case 0:
        D_800B2078.unk2378 = 0;
        break;
    case 1:
        D_800B2078.unk2378 = 0x10;
        break;
    case 2:
        D_800B2078.unk2378 = 0x20;
        break;
    case 3:
        D_800B2078.unk2378 = 0x30;
        break;
    }
    D_800AFC7C += 4;
}

extern void func_8002303C(FieldModel *model, s32, s32);
/* Event: set the current actor's model frame from operands 1 and 3. */
void func_800888A4(void) {
    FieldModel *model;
    u16 low;
    s32 frame;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    low = func_800ACDEC(1) & 0xF;
    frame = ((func_800ACDEC(1) >> 4) << 8) + func_800ACDEC(3);
    func_8002303C(model, 2, 0);
    model->animation->unk18[2] = low << 6;
    D_800B0078->state.bits.unk18 = low << 6;
    model->animation->unk18[3] = frame;
    D_800B0078->unk130 = frame;
    D_800B0078->state.bits.unk16 = 1;
    D_800B0078->pc += 5;
}

/* Event: set the current actor's two model frames from operands 1/3 and
 * 5/7. */
void func_800889BC(void) {
    FieldModel *model;
    u16 low0;
    s32 frame0;
    u16 low1;
    s32 frame1;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    low0 = func_800ACDEC(1) & 0xF;
    frame0 = ((func_800ACDEC(1) >> 4) << 8) + func_800ACDEC(3);
    low1 = func_800ACDEC(5) & 0xF;
    frame1 = ((func_800ACDEC(5) >> 4) << 8) + func_800ACDEC(7);
    func_8002303C(model, 3, 0);
    model->animation->unk18[2] = low0 << 6;
    D_800B0078->state.bits.unk18 = low0 << 6;
    model->animation->unk18[3] = frame0;
    D_800B0078->unk130 = frame0;
    model->animation->unk18[4] = low1 << 6;
    D_800B0078->unk130_9 = low1 << 6;
    model->animation->unk18[5] = frame1;
    D_800B0078->unk130_19 = frame1;
    D_800B0078->pc += 9;
    D_800B0078->state.bits.unk16 = 2;
}

/* Event: set flag 0x80 (op1 1) or 0x40 (op1 2) of the current record's +2a,
 * using four batch steps. */
void func_80088B68(void) {
    s32 bits = 0;

    switch (func_800ACDEC(1)) {
    case 1:
        bits = 0x80;
        break;
    case 2:
        bits = 0x40;
        break;
    }
    D_800B02CC[D_800B2078.unk2384].flags |= bits;
    D_800AFC7C += 4;
    D_800B0078->pc += 7;
}

/* Event: set the current record's +24, high flag byte and +76 from operands
 * 1, 3 and 5, using four batch steps. */
void func_80088C1C(void) {
    D_800B02CC[D_800B2078.unk2384].unk24 = func_800ACDEC(1);
    D_800B02CC[D_800B2078.unk2384].flags |= func_800ACDEC(3) << 8;
    D_800B02CC[D_800B2078.unk2384].unk76 = func_800ACDEC(5);
    D_800AFC7C += 4;
    D_800B0078->pc += 7;
}

/* 80088d38 with 0. */
void func_80088CF8(void) {
    func_80088D38(0);
}

/* 80088d38 with 4. */
void func_80088D18(void) {
    func_80088D38(4);
}

/* Set the current emitter record's four +30 pairs from index first on to the
 * selected operands 1..15 (flags byte 0x11); four batch steps. */
void func_80088D38(s32 first) {
    D_800B02CC[D_800B2078.unk2384].unk30[first][0] = func_8009CF78(1, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first][1] = func_8009CFBC(3, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 1][0] = func_8009D000(5, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 1][1] = func_8009D044(7, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 2][0] = func_8009D088(9, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 2][1] = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 3][0] = func_8009D110(0xD, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 3][1] = func_8009D154(0xF, EVENT_OPERAND_BYTE(0x11));
    D_800AFC7C += 4;
    D_800B0078->pc += 0x12;
}

extern s32 D_800ADB40;
/* Event: select emitter record operand 1 and start it with the effect
 * actor, count operand 3, +02 operand 5 and +04 operand 7; four batch
 * steps. */
void func_80089004(void) {
    s32 index;

    D_800B2078.unk2384 = index = func_800ACDEC(1);
    D_800B02CC[index].unk24 = 1;
    D_800B02CC[D_800B2078.unk2384].unk52 = D_800B2078.unk2374;
    D_800ADB40 = D_800B02CC[D_800B2078.unk2384].unk52;
    D_800B02CC[D_800B2078.unk2384].unk00 = 0;
    D_800B02CC[D_800B2078.unk2384].unk76 = 0;
    D_800B02CC[D_800B2078.unk2384].count = func_800ACDEC(3);
    D_800B02CC[D_800B2078.unk2384].unk02 = func_800ACDEC(5);
    D_800B02CC[D_800B2078.unk2384].unk04 = func_800ACDEC(7);
    func_8008861C();
    D_800AFC7C += 4;
    D_800B0078->pc += 9;
}

/* Event: set the current emitter record's +0c and +14 vectors from the
 * selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089174(void) {
    D_800B02CC[D_800B2078.unk2384].unk0C.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk0C.vy = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk0C.vz = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk14.vx = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk14.vy = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk14.vz = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: set the current emitter record's +08, +1c vector, +26 and +28 from
 * the selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089374(void) {
    D_800B02CC[D_800B2078.unk2384].unk08 = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk1C.vx = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk1C.vy = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk1C.vz = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk26 = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk28 = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: set the current record's +56, +58 and +54 from operands 1..5, its
 * flags from op7, op9 and the effect kind, and +72/+74 from the effect
 * parameters, using four batch steps. */
void func_80089574(void) {
    s16 flags;

    D_800B02CC[D_800B2078.unk2384].unk56 = func_800ACDEC(1);
    D_800B02CC[D_800B2078.unk2384].unk58 = func_800ACDEC(3);
    D_800B02CC[D_800B2078.unk2384].unk54 = func_800ACDEC(5);
    flags = func_800ACDEC(7);
    D_800B02CC[D_800B2078.unk2384].flags = flags | (func_800ACDEC(9) * 2) | D_800B2078.unk2378;
    D_800B02CC[D_800B2078.unk2384].unk72 = D_800B2078.unk237C;
    D_800B02CC[D_800B2078.unk2384].unk74 = D_800B2078.unk2380;
    D_800AFC7C += 4;
    D_800B0078->pc += 11;
}

/* Event: set the current emitter record's +5a and +62 vectors from the
 * selected operands 1/3 and 5/7 (flags byte 9); four batch steps. */
void func_800896D4(void) {
    D_800B02CC[D_800B2078.unk2384].unk5A.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk5A.vy = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk5A.vz = 0;
    D_800B02CC[D_800B2078.unk2384].unk62.vx = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk62.vy = func_8009D044(7, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk62.vz = 0;
    D_800AFC7C += 4;
    D_800B0078->pc += 10;
}

/* Event: set the current emitter record's bytes +6a..+6c and +6e..+70 from
 * the selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089880(void) {
    D_800B02CC[D_800B2078.unk2384].unk6A = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6B = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6C = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6E = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6F = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk70 = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: use four batch steps and, unless 800adb8c is set, 800a99a8 for the
 * current actor. */
void func_80089A80(void) {
    D_800AFC7C += 4;
    if (D_800ADB8C == 0) {
        func_800A99A8(D_800AFD1C);
    }
    D_800B0078->pc++;
}

/* Event: use four batch steps and run 800a98e8 for the current actor with
 * its byte operand. */
void func_80089AE4(void) {
    D_800AFC7C += 4;
    func_800A98E8(D_800AFD1C, D_800ADC00[D_800B0078->pc + 1]);
    D_800B0078->pc += 2;
}

/* Event: store the current actor's party position (or 0xff) in variable op1. */
void func_80089B54(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8005A444[i] == D_800AFD1C) {
            func_800A3074(func_800ACDB8(1) & 0xFFFF, i);
            goto done;
        }
    }
    func_800A3074(func_800ACDB8(1) & 0xFFFF, 0xFF);
done:
    D_800B0078->pc += 3;
}

/* Event: set emitter operand 1's start (22e8) and end (2300) points and its
 * step count (2318) from the selected operands 3-15 (flags byte 0x11). */
void func_80089BF0(void) {
    s32 emitter;

    emitter = func_8009CF78(1, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk22E8[emitter][0] = func_8009CFBC(3, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk22E8[emitter][1] = func_8009D000(5, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk22E8[emitter][2] = func_8009D044(7, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2300[emitter][0] = func_8009D088(9, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2300[emitter][1] = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2300[emitter][2] = func_8009D110(0xD, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2318[emitter] = func_8009D154(0xF, EVENT_OPERAND_BYTE(0x11));
    D_800B0078->pc += 0x12;
}

/* Event: place sound emitter op1 at (op3, op7, op5) and attach it to the
 * actor byte 10 selects (-1 for none). */
void func_80089DCC(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    s32 actor;

    D_800B2078.emitter_position[index][0] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.emitter_position[index][2] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.emitter_position[index][1] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    actor = func_8009CDB4(10);
    if (actor != 0xFF) {
        D_800B2078.emitter_descriptor[index] = actor;
    } else {
        D_800B2078.emitter_descriptor[index] = -1;
    }
    D_800B0078->pc += 11;
}

/* Event: set 800b22e0 from operand 1. */
void func_80089F18(void) {
    D_800B2078.unk22E0 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set 800b21d2 to operand 1 less 0x80. */
void func_80089F54(void) {
    D_800B2078.piece_drift_mode = func_800ACDEC(1) - 0x80;
    D_800B0078->pc += 3;
}

/* Event: store operand byte 1 in 800afe84. */
void func_80089F94(void) {
    FieldActor *actor;

    actor = D_800B0078;
    D_800AFE84 = D_800ADC00[actor->pc + 1];
    actor->pc += 2;
}

/* Event: set the eight halfwords at 800b0080 from raw operands (+88
 * cleared; +84 at least 1). */
void func_80089FD0(void) {
    s16 value;

    D_800B0080.unk80[0] = func_800ACDB8(1);
    D_800B0080.unk80[1] = func_800ACDB8(3);
    value = func_800ACDB8(5);
    D_800B0080.unk80[2] = value;
    if (value == 0) {
        D_800B0080.unk80[2] = value + 1;
    }
    D_800B0080.unk80[3] = func_800ACDB8(7);
    D_800B0080.unk80[4] = 0;
    D_800B0080.unk80[5] = func_800ACDB8(9);
    D_800B0080.unk80[6] = func_800ACDB8(11);
    D_800B0080.unk80[7] = func_800ACDB8(13);
    D_800B0078->pc += 15;
}

/* Event: set 800b0090, 800b0098 and 800b0094 from operands 1, 3 and 5
 * (immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008A08C(void) {
    D_800B0080.unk90 = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0080.unk98 = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0080.unk94 = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

/* Event: set the object parameters at 800b00a0 from twelve operands and
 * enable it (800b00b2). */
void func_8008A148(void) {
    D_800B0080.unkA0[0] = func_800ACDEC(1);
    D_800B0080.unkA0[1] = func_800ACDEC(3);
    D_800B0080.unkA0[2] = func_800ACDEC(5);
    D_800B0080.unkA4[0] = func_800ACDEC(7);
    D_800B0080.unkA4[1] = func_800ACDEC(9);
    D_800B0080.unkA4[2] = func_800ACDEC(11);
    D_800B0080.unkA8[0] = func_800ACDEC(13);
    D_800B0080.unkA8[1] = func_800ACDEC(15);
    D_800B0080.unkA8[2] = func_800ACDEC(17);
    D_800B0080.unkAC = func_800ACDEC(19);
    D_800B0080.unkAE = func_800ACDEC(21);
    D_800B0080.unkB0 = func_800ACDEC(23);
    D_800B0078->pc += 25;
    D_800B0080.enabled = 1;
}

/* Event: wait while 800adb88 is set, yielding each time. */
void func_8008A244(void) {
    if (D_800ADB88 == 0) {
        D_800B0078->pc++;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: store 800b06a0 in variable op1. */
void func_8008A2A0(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B06A0);
    D_800B0078->pc += 3;
}

#ifdef NON_MATCHING
/* Event 0x77: once the display is idle, load TIM file 0x7fb + op5 (mode 0),
 * upload it to VRAM at op4/op6 with CLUT row op8 + 0xe8 (mode 1) or free
 * it (other modes). */
void func_8008A2E8(void) {
    s32 index;
    s32 file;
    s32 clut_y;
    s32 x;
    s32 y;
    s32 row;
    u32 *image;

    if (func_8008A558() == -1) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    if (EVENT_OPERAND_BYTE(1) == 0) {
        index = func_8009CF78(5, EVENT_OPERAND_BYTE(0xD));
        func_80028470(4, 0);
        file = index + 0x7FB;
        image = func_80031BDC(func_800288EC(file), 0);
        D_800B1F74 = image;
        func_800295D8(file, image, 0, 0x80);
        D_800B0078->pc += 2;
    } else if (EVENT_OPERAND_BYTE(1) == 1) {
        row = func_8009D044(8, EVENT_OPERAND_BYTE(0xA));
        clut_y = row + 0xE8;
        if (row == 0xFF) {
            clut_y = -1;
        }
        x = func_8009CFBC(4, EVENT_OPERAND_BYTE(0xA));
        y = func_8009D000(6, EVENT_OPERAND_BYTE(0xA));
        if (y >= 0x100 && x >= 0x2C0) {
            func_800A915C();
        }
        func_80070340(D_800B1F74, x, y, 0, clut_y, 0, 0);
        D_800B0078->pc += 0xB;
    } else {
        func_800320E8(D_800B1F74);
        D_800B0078->pc += 2;
    }
    D_800B00C0 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A2E8);
#endif

void func_8008A4E0(void) {
}

void func_8008A4E8(void) {
}

void func_8008A4F0(void) {
}

void func_8008A4F8(void) {
}

void func_8008A500(void) {
}

void func_8008A508(void) {
}

void func_8008A510(void) {
}

void func_8008A518(void) {
}

/* Wait (VSync) until the disc is idle and the stream is stopped. */
void func_8008A520(void) {
    while (func_8008A558() != 0) {
        VSync(0);
    }
}

/* Stop the stream once no music-wave read runs and the disc is idle; -1
 * while busy. */
s32 func_8008A558(void) {
    if (D_800ADB2C == 0) {
        if (func_800286CC() == 0) {
            func_80028A60(0);
            return 0;
        }
    }
    return -1;
}

/* Event: call 8003633c(0) when its byte operand is zero. */
void func_8008A5A0(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        func_8003633C(0);
    }
    D_800B0078->pc++;
}

/* Event: set 800b21d4 from operand 1. */
void func_8008A604(void) {
    D_800B2078.unk21D4 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set character op3's slot byte +4 to op1 less its byte +3 (at
 * least 0). */
void func_8008A640(void) {
    s32 character = func_8008CF3C(func_800ACDEC(3));
    s32 value;

    if (character != 0xFF) {
        value = func_800ACDEC(1) - D_8005A39C->characters[character].unk77;
        if (value < 0) {
            value = 0;
        }
        D_8005A39C->characters[character].unk78 = value;
    }
    D_800B0078->pc += 5;
}

/* Event: store character op3's slot bytes +3 and +4 summed (0 for none) in
 * variable op1. */
void func_8008A6E0(void) {
    s32 character = func_8008CF3C(func_800ACDEC(3));
    s32 sum;

    if (character != 0xFF) {
        sum = D_8005A39C->characters[character].unk77 + D_8005A39C->characters[character].unk78;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, sum);
    } else {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, 0);
    }
    D_800B0078->pc += 5;
}

/* Find a free (0xff) slot of the table at 80062590 for `id`; -1 when `id`
 * is already there or no slot is free. */
s32 func_8008A790(s32 id, s32 *slot) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_80062590[i] == id) {
            break;
        }
        if (D_80062590[i] == 0xFF) {
            *slot = i;
            return 0;
        }
    }
    return -1;
}

/* Start reading party member `member`'s sprite file for slot `slot` (a
 * character substitute while 8004f34c has 0xc000). */
void func_8008A7DC(s32 member, s32 slot) {
    s32 file;
    s32 size;
    s32 sprite;

    D_800ADBCC = slot;
    D_800ADBC8 = member;
    func_80028470(4, 0);
    if (D_800ADB1C == 0) {
        func_80028A60(0);
    }
    if (!(D_8004F34C & 0xC000)) {
        file = member + 5;
        size = func_800288EC(file);
        D_8006FABC[D_800ADBCC] = member;
        D_800ADBC0 = func_80031BDC(size, 0);
        func_800295D8(file, D_800ADBC0, 0, 0x80);
    } else {
        sprite = func_8001ACF0(member);
        if (sprite == 0xFF) {
            sprite = 0;
        }
        sprite += 0x10;
        file = sprite + 5;
        D_800ADBC0 = func_80031BDC(func_800288EC(file), 0);
        D_8006FABC[D_800ADBCC] = sprite;
        func_800295D8(file, D_800ADBC0, 0, 0x80);
    }
    if (D_800ADB1C == 0) {
        func_80028A60(0);
    }
    D_800ADBC4 = 1;
}

/* Event: set actor field EA to the complement of operand byte 1. */
void func_8008A93C(void) {
    FieldActor *actor;

    actor = D_800B0078;
    actor->unk0EA = ~D_800ADC00[actor->pc + 1];
    actor->pc += 2;
}

/* Event: as 8008a93c, and clear the actor's layer bit 16. */
void func_8008A974(void) {
    func_8008A93C();
    D_800B0078->layer_flags &= ~0x10000;
}

/* Event: once the stream is stopped, hand the actor's block to its model
 * (80021bf0) and continue; otherwise wait. Yields either way. */
void func_8008A9AC(void) {
    if (func_8008A558() == 0) {
        D_800ADB90 = 0;
        func_80021BF0(D_800AF880.components.descriptors[D_800AFD1C].model, D_800B0078->unk120);
        D_800B0078->pc++;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: release the actor's block at +120 once, then yield. */
void func_8008AA60(void) {
    if (D_800B0078->unk124 != -1) {
        func_800320E8(D_800B0078->unk120);
        D_800B0078->unk124 = -1;
    }
    D_800B00C0 = 1;
    D_800B0078->pc++;
}

/* Event 0xb0: load wave-bank file op4 into slot op2 (releasing the old bank)
 * or, with mode byte 1, register the loaded bank; waits while the display
 * is busy. */
void func_8008AACC(void) {
    s32 file;
    s32 slot;
    void *data;

    if (func_8008A558() == 0) {
        if (EVENT_OPERAND_BYTE(1) == 1) {
            D_80062518[D_800AFD18] = func_80037FD8(D_800AFD08, 0);
            func_8003BDFC(0x10);
            func_800320E8(D_800AFD08);
            if (D_800AFD18 == 3) {
                D_800595AC = D_80062524;
            }
            D_800B00C0 = 1;
            D_800B0078->pc += 2;
        } else {
            slot = func_800ACDEC(2);
            D_800AFD18 = slot;
            func_80038310(D_80062518[slot]);
            file = func_800ACDEC(4);
            D_800AFD0C = file;
            if (!(file & 0x80)) {
            standard:
                func_80028470(0x1C, 0);
                D_800AFD0C += 2;
            } else if (D_8004F370 == 1) {
                D_800AFD0C = 4;
                goto standard;
            } else {
                D_800AFD0C = (file & 0x7F) + 0x1F;
                func_80028470(0x2C, 1);
            }
            data = func_80031BDC(func_800288EC(D_800AFD0C), 0);
            D_800AFD08 = data;
            func_800295D8(D_800AFD0C, data, 0, 0x80);
            func_80028470(4, 0);
            D_800B0078->pc += 6;
        }
    } else {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
    }
}

/* Event: load file op1 + 0x77a as the actor's block (+120), once the
 * stream and disc are idle; yields. */
void func_8008ACE8(void) {
    s32 number;
    s32 file;
    s32 size;

    if (D_800ADB90 == 0 && D_800ADB2C == 0) {
        if (func_8008A558() != 0) {
            D_800B00C0 = 1;
            D_800B0078->pc--;
            return;
        }
        if (D_800B0078->unk124 != -1) {
            func_800320E8(D_800B0078->unk120);
            D_800B0078->unk124 = -1;
        }
        number = func_800ACDEC(1);
        func_80028470(4, 0);
        file = number + 0x77A;
        size = func_800288EC(file) + 8;
        D_800B0078->unk124 = file;
        D_800B0078->unk120 = func_80031BDC(size, 0);
        func_800295D8(file, D_800B0078->unk120, 0, 0x80);
        if (D_800ADB1C == 0) {
            func_80028A60(0);
        }
        D_800ADB90 = 1;
        D_800B0078->pc += 3;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: set (selector 0) or clear the actor's layer bit 17. */
void func_8008AE5C(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        D_800B0078->layer_flags |= 0x20000;
    } else {
        D_800B0078->layer_flags &= ~0x20000;
    }
    D_800B0078->pc += 2;
}

/* Event: set entry op1 of the 800b221c triples from operands 3, 5 and 7
 * (immediate by flags of byte 9). */
void func_8008AEC8(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);

    D_800B2078.unk221C[index][0] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk221C[index][1] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk221C[index][2] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: set column op1 of the 800b223c table from operands 3, 5 and 7
 * (immediate by flags of byte 9). */
void func_8008AFD8(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);

    D_800B2078.unk223C[0][index] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk223C[1][index] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk223C[2][index] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: set the three bytes at 800b225c from operands 1, 3 and 5. */
void func_8008B0E8(void) {
    D_800B2078.unk225C[0] = func_800ACDEC(1);
    D_800B2078.unk225C[1] = func_800ACDEC(3);
    D_800B2078.unk225C[2] = func_800ACDEC(5);
    D_800B0078->pc += 7;
}

/* Event: set 800b21b4 from operand 1. */
void func_8008B144(void) {
    D_800B2078.unk21B4 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: when the 801e module is loaded, pass (op1, op3) to 801e8330 and
 * keep op3 in the table at 800b21e4. */
void func_8008B180(void) {
    s32 index;

    if (D_800ADB1C != 0) {
        index = func_800ACDEC(1) & 0xFFFF;
        func_801E8330(index, 0, func_800ACDEC(3));
        D_800B2078.unk21E4[func_800ACDEC(1)] = func_800ACDEC(3);
    }
    D_800B0078->pc += 5;
}

/* Event: set the actor's +11e from operand 1. */
void func_8008B210(void) {
    D_800B0078->unk11E = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: fade channel 1 towards (op3, op5, op7) over op9 frames with blend
 * op1. */
void func_8008B248(void) {
    s32 steps = func_800ACDEC(9);
    s32 red = func_800ACDEC(3);
    s32 green = func_800ACDEC(5);
    s32 blue = func_800ACDEC(7);

    func_80071D08(1, steps, red, green, blue, func_800ACDEC(1));
    D_800B0078->pc += 11;
}

/* Event: run 800a484c(0) and skip fourteen operand bytes. */
void func_8008B2F0(void) {
    func_800A484C(0);
    D_800B0078->pc += 15;
}

/* Event: 801e effect control by selector byte: start with operand 2 (0),
 * wait for 800b2078 to clear (1), clear it (2) or stop (3); yields. */
void func_8008B328(void) {
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        func_800A4CC4(0, 0, 0, 0, 0, 0, func_800ACDEC(2));
        D_800B2078.unk207A = 1;
        D_800B0078->pc += 4;
        break;
    case 1:
        if (D_800B2078.unk2078 == 0) {
            D_800B0078->pc += 2;
        } else {
            D_800B0078->pc--;
        }
        break;
    case 2:
        D_800B2078.unk2078 = 0;
        D_800B0078->pc += 2;
        break;
    case 3:
        func_800A47D4();
        D_800B0078->pc += 2;
        break;
    }
    D_800B00C0 = 1;
}

/* Event: set the sprite view rotation from operands 1, 3 and 5 (X, Z, Y;
 * immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008B45C(void) {
    D_800B2078.sprite_angles.vx = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800B2078.sprite_angles.vz = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800B2078.sprite_angles.vy = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

/* Event: set the camera orbit angles from operands 1, 3 and 5 (X, Z, Y;
 * immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008B518(void) {
    D_800AF880.orbit_angles.vx = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800AF880.orbit_angles.vz = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800AF880.orbit_angles.vy = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

#ifdef NON_MATCHING
/* Event 0x1b: scroll the current actor's textured polygons by (op1, op3)
 * texels in both draw buffers. */
void func_8008B5D4(void) {
    FieldInstance *instance;
    POLY_FT3 *ft3;
    POLY_FT3 *ft3_other;
    POLY_FT4 *ft4;
    POLY_FT4 *ft4_other;
    u8 *prims;
    u8 *other;
    FieldMesh *mesh;
    u32 *group;
    s32 du;
    s32 dv;
    s32 groups;
    u32 header;
    s32 count;
    s32 code;
    s32 i;

    instance = D_800AF880.components.descriptors[D_800AFD1C].instance;
    prims = instance->prims[D_800ADB08];
    mesh = instance->mesh;
    other = instance->prims[(D_800ADB08 + 1) & 1];
    group = mesh->groups;
    du = (s16)func_800ACD7C(1);
    dv = (s16)func_800ACD7C(3);
    for (groups = mesh->group_count; groups > 0; groups--) {
        header = *group;
        code = header & 0xFF;
        count = header >> 16;
        if (code == 0xC4 || code == 0xC8) {
            group++;
        } else {
            group++;
            if (!(header & 8)) {
                ft3 = (POLY_FT3 *)prims;
                ft3_other = (POLY_FT3 *)other;
                for (i = 0; i < count; i++) {
                    ft3->u0 += du;
                    ft3->u1 += du;
                    ft3->u2 += du;
                    ft3->v0 += dv;
                    ft3->v1 += dv;
                    ft3->v2 += dv;
                    ft3_other->u0 = ft3->u0;
                    ft3_other->u1 = ft3->u1;
                    ft3_other->u2 = ft3->u2;
                    ft3_other->v0 = ft3->v0;
                    ft3_other->v1 = ft3->v1;
                    ft3_other->v2 = ft3->v2;
                    group += 2;
                    ft3++;
                    ft3_other++;
                }
                prims = (u8 *)ft3;
                other = (u8 *)ft3_other;
            } else {
                ft4 = (POLY_FT4 *)prims;
                ft4_other = (POLY_FT4 *)other;
                for (i = 0; i < count; i++) {
                    ft4->u0 += du;
                    ft4->u1 += du;
                    ft4->u2 += du;
                    ft4->u3 += du;
                    ft4->v0 += dv;
                    ft4->v1 += dv;
                    ft4->v2 += dv;
                    ft4->v3 += dv;
                    ft4_other->u0 = ft4->u0;
                    ft4_other->u1 = ft4->u1;
                    ft4_other->u2 = ft4->u2;
                    ft4_other->u3 = ft4->u3;
                    ft4_other->v0 = ft4->v0;
                    ft4_other->v1 = ft4->v1;
                    ft4_other->v2 = ft4->v2;
                    ft4_other->v3 = ft4->v3;
                    group += 2;
                    ft4++;
                    ft4_other++;
                }
                prims = (u8 *)ft4;
                other = (u8 *)ft4_other;
            }
        }
    }
    D_800B0078->pc += 5;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B5D4);
#endif

/* Event: once the disc is idle, stop the stream, decode the pending party
 * sprite into its block, release the buffer and apply it; wait otherwise. */
void func_8008B894(void) {
    if (D_800ADBC4 != 0xFF && func_800286CC() == 0) {
        func_80028A60(0);
        func_80032EB4(D_800ADBC0, D_8005A414[D_800ADBCC]);
        func_800320E8(D_800ADBC0);
        func_8008B978(D_800ADBC8);
        D_800ADBC4 = 0xFF;
        D_800B00C0 = 1;
        D_800B0078->pc++;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Run the join event of party member `member`: the first event actor whose
 * event 0 starts with instruction 0x16 for that member is initialised and
 * run (with the member's own actor while 8004f34c has 0xc000); the running
 * actor's context is restored afterwards. */
void func_8008B978(s32 member) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 yield;
    s32 current;
    s32 steps;
    s32 pc;
    s32 i;
    s32 entry;
    u8 *code;
    s32 pc_new;

    D_8005A39C->unk1D30 |= 1 << D_800ADBC8;
    descriptor = D_800B06B8;
    actor = D_800B0078;
    if (D_800ADB1C != 0) {
        pc = actor->pc;
        yield = D_800B00C0;
        steps = D_800AFC7C;
        current = D_800AFD1C;
        for (i = 0; i < D_800ADBFC; i++) {
            entry = func_800A3090(i, 0);
            code = &D_800ADC00[entry];
            if (code[0] == 0x16 && code[1] == member) {
                D_800B06B8 = &D_800AF880.components.descriptors[i];
                D_800B0078 = D_800B06B8->actor;
                func_80080A74(i);
                D_800AFD1C = i;
                D_800AF880.components.descriptors[i].actor->pc = entry;
                pc_new = func_800A3090(i, 0);
                D_800AFFEC = 0;
                D_800B0078->pc = pc_new;
                entry = func_800A3090(i, 0);
                func_8008D380(i, D_800B2078.controlled);
                func_800A1EC8(0xFFFF);
                func_80077268();
                D_8005A39C->unk1D30 |= 1 << D_800ADBC8;
                if (D_8004F34C & 0xC000) {
                    D_800B06B8 = &D_800AF880.components.descriptors[D_8006F990[D_800ADBCC]];
                    D_800B0078 = D_800B06B8->actor;
                    func_80080A74(D_8006F990[D_800ADBCC]);
                    D_800AF880.components.descriptors[i].actor->pc = entry;
                    D_800AFD1C = D_8006F990[D_800ADBCC];
                    pc_new = func_800A3090(D_8006F990[D_800ADBCC], 0);
                    D_800AFFEC = 0;
                    D_800B0078->pc = pc_new;
                    func_800A1EC8(0xFFFF);
                }
                break;
            }
        }
        D_800B06B8 = descriptor;
        D_800B0078 = actor;
        D_800B00C0 = yield;
        D_800AFD1C = current;
        D_800AFC7C = steps;
        actor->pc = pc;
    }
}

/* Event: once idle, add character op1 to the party (a free slot starts its
 * sprite load) or mark it waiting (+1d30); yields while busy. */
void func_8008BC80(void) {
    s32 pending = D_800ADBC4;
    s32 member;
    s32 slot;

    if (pending == 0xFF && D_800ADB2C == 0 && func_8008A558() == 0) {
        func_80028A60(0);
        member = func_800ACDEC(1);
        if (member != pending) {
            if (func_8008A790(member, &slot) == 0) {
                D_8005A39C->unk22B1[slot] = 0;
                D_80062590[slot] = member;
                func_8008A7DC(member, slot);
                D_800B0078->pc += 3;
                return;
            }
            D_8005A39C->unk1D30 |= 1 << member;
            D_800B0078->pc += 5;
            return;
        }
        D_800B0078->pc += 5;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Event: once idle, add the character in its byte operand to the party (a
 * free slot starts its sprite load) or mark it waiting; yields while busy. */
void func_8008BDD8(void) {
    s32 slot;
    s32 member;

    if (D_800ADBC4 == 0xFF && D_800ADB2C == 0 && func_8008A558() == 0) {
        func_80028A60(0);
        if (func_8008A790(D_800ADC00[D_800B0078->pc + 1], &slot) == 0) {
            D_8005A39C->unk22B1[slot] = 0;
            member = D_800ADC00[D_800B0078->pc + 1];
            D_80062590[slot] = member;
            func_8008A7DC(member, slot);
            D_800B0078->pc += 2;
            return;
        }
        D_8005A39C->unk1D30 |= 1 << D_800ADC00[D_800B0078->pc + 1];
        D_800B0078->pc += 4;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Close party slot `slot` up: the next slot's member, sprite data and ids
 * move down (its actor's sprite is set up again for this slot) and the next
 * slot is emptied. */
void func_8008BF38(s32 slot) {
    s32 member;
    s32 kind;
    s32 *sprites;

    member = D_8005A444[slot + 1];
    if (member == 0xFF) {
        D_8006FABC[slot] = D_8006FABC[slot + 1];
        D_80062590[slot] = D_80062590[slot + 1];
        D_8005A444[slot] = D_8005A444[slot + 1];
        D_80062590[slot + 1] = member;
        D_8006FABC[slot + 1] = member;
        D_8005A444[slot + 1] = member;
        return;
    }
    *(PartySprite *)D_8005A414[slot] = *(PartySprite *)D_8005A414[slot + 1];
    D_8006FABC[slot] = D_8006FABC[slot + 1];
    D_80062590[slot] = D_80062590[slot + 1];
    D_8005A444[slot] = D_8005A444[slot + 1];
    kind = D_800AF880.components.descriptors[member].actor->unk126[0];
    if (!(kind & 0x80)) {
        func_80076AC0(member, slot, D_8005A414[slot], 1, 0, slot, 1);
    } else {
        sprites = D_800AF880.components.sprites;
        func_80076AC0(member, D_800AF880.components.descriptors[member].actor->unk126[1],
                      (u8 *)(sprites[(kind & 0x7F) + 1] + (s32)sprites),
                      D_800AF880.components.descriptors[member].actor->unk130_28,
                      D_800AF880.components.descriptors[member].actor->unk134 & 0xF,
                      D_800AF880.components.descriptors[member].actor->unk126[0],
                      (D_800AF880.components.descriptors[member].actor->unk134 >> 4) & 1);
    }
    D_80062590[slot + 1] = 0xFF;
    D_8006FABC[slot + 1] = 0xFF;
    D_8005A444[slot + 1] = 0xFF;
}

#ifdef NON_MATCHING
/* Take party slot `slot`'s member out of the party: its actor gets the
 * lead's sprite and is hidden, and the slot's ids are cleared. */
void func_8008C180(s32 slot) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    FieldActor *member;
    s32 current;
    u16 pc;
    s32 index;

    if (D_8005A444[slot] != 0xFF) {
        descriptor = D_800B06B8;
        actor = D_800B0078;
        current = D_800AFD1C;
        pc = actor->pc;
        D_800B06B8 = &D_800AF880.components.descriptors[D_8005A444[slot]];
        D_800B0078 = D_800B06B8->actor;
        func_80080A74(D_8005A444[slot]);
        index = D_8005A444[slot];
        D_800AFD1C = index;
        D_800AF880.components.descriptors[index].flags = (D_800AF880.components.descriptors[index].flags & 0xF07F) | 0x200;
        func_80076AC0(index, 0, D_8005A414[0], 1, 0, 0, 1);
        member = D_800B0078;
        member->flags |= 1;
        member->layer_flags |= 0x100000;
        D_800B00C0 = 0;
        D_800B0078 = actor;
        D_800B06B8 = descriptor;
        D_800AFD1C = current;
        member->pc = pc;
        member->flags |= 0x20000;
        member->layer_flags |= 0x400;
        D_8005A444[slot] = 0xFF;
    }
    D_80062590[slot] = 0xFF;
    D_8006FABC[slot] = 0xFF;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008C180);
#endif

/* Event 0x19: remove character op1 from the party once no sprite load is
 * pending. Before the field is set up only the slot tables and sprite data
 * close up; afterwards the member's actor is also released and the lead
 * actor becomes the party leader again. */
void func_8008C334(void) {
    s32 slot;

    if (D_800ADBC4 != 0xFF) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    DrawSync(0);
    slot = func_8009FA00(func_8008CF3C(EVENT_OPERAND_BYTE(1)));
    if (slot != -1) {
        if (D_800ADB1C == 0) {
            switch (slot) {
            case 0:
                if (D_80062590[1] == 0xFF) {
                    D_80062590[0] = 0xFF;
                    D_8006FABC[0] = 0xFF;
                    D_8005A39C->unk22B1[0] = 0;
                } else {
                    *(PartySprite *)D_8005A414[0] = *(PartySprite *)D_8005A414[1];
                    D_80062590[0] = D_80062590[1];
                    D_8006FABC[0] = D_8006FABC[1];
                    D_80062590[1] = 0xFF;
                    D_8006FABC[1] = 0xFF;
                    D_8005A39C->unk22B1[0] = D_8005A39C->unk22B1[1];
                    if (D_80062590[2] != 0xFF) {
                        *(PartySprite *)D_8005A414[1] = *(PartySprite *)D_8005A414[2];
                        D_80062590[1] = D_80062590[2];
                        D_8006FABC[1] = D_8006FABC[2];
                        D_80062590[2] = 0xFF;
                        D_8006FABC[2] = 0xFF;
                        D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                    }
                }
                break;
            case 1:
                if (D_80062590[2] == 0xFF) {
                    D_80062590[1] = 0xFF;
                    D_8006FABC[1] = 0xFF;
                    D_8005A39C->unk22B1[1] = 0;
                } else {
                    *(PartySprite *)D_8005A414[1] = *(PartySprite *)D_8005A414[2];
                    D_80062590[1] = D_80062590[2];
                    D_8006FABC[1] = D_8006FABC[2];
                    D_80062590[2] = 0xFF;
                    D_8006FABC[2] = 0xFF;
                    D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                    D_8005A39C->unk22B1[2] = 0;
                }
                break;
            case 2:
                D_80062590[2] = 0xFF;
                D_8006FABC[2] = 0xFF;
                D_8005A39C->unk22B1[2] = 0;
                break;
            }
        } else {
            switch (slot) {
            case 0:
                D_8005A39C->unk22B1[0] = D_8005A39C->unk22B1[1];
                D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                D_8005A39C->unk22B1[2] = 0;
                func_8008C180(0);
                func_8008BF38(0);
                func_8008BF38(1);
                break;
            case 1:
                D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                D_8005A39C->unk22B1[2] = 0;
                func_8008C180(1);
                func_8008BF38(1);
                break;
            case 2:
                D_8005A39C->unk22B1[2] = 0;
                func_8008C180(2);
                break;
            }
            if (D_80062590[0] != 0xFF && D_8005A444[0] != 0xFF) {
                D_800B2078.controlled = D_8005A444[0];
                D_800AF880.components.descriptors[D_8005A444[0]].actor->flags =
                    (D_800AF880.components.descriptors[D_8005A444[0]].actor->flags | 0x4400) & ~0x80;
            } else {
                D_800B2078.controlled = 0;
            }
        }
    }
    D_800B0078->pc += 2;
}

/* Event: release the actor's block at +114 when flag 0x1000 of +12c says it
 * holds one. */
void func_8008C7D8(void) {
    if (D_800B0078->state.word & 0x1000) {
        func_800320E8(D_800B0078->unk114);
        D_800B0078->state.word &= ~0x1000;
    }
    D_800B0078->pc++;
}

/* Event: with the sequence playing, pass op1/op3 to 8003a89c; wait while a
 * sound is still loading into the 801e module. */
void func_8008C84C(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_800ACDEC(1);
        func_8003A89C(D_80062528, a, func_800ACDEC(3));
        D_800B0078->pc += 5;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 5;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: as 8008c84c through 8003a948 with selected operands 1 and 3. */
void func_8008C938(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 5]);
        func_8003A948(D_80062528, a, func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 5]));
        D_800B0078->pc += 6;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 6;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 6;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: as 8008c84c through 8003a838. */
void func_8008CA60(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_800ACDEC(1);
        func_8003A838(D_80062528, a, func_800ACDEC(3));
        D_800B0078->pc += 5;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 5;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: as 8008c938 through 8003a9bc. */
void func_8008CB4C(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 5]);
        func_8003A9BC(D_80062528, a, func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 5]));
        D_800B0078->pc += 6;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 6;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 6;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: as 8008c84c through 8003aac4 with one operand. */
void func_8008CC74(void) {
    if (D_8004F36C != 0) {
        func_8003AAC4(D_80062528, func_800ACDEC(1));
        D_800B0078->pc += 3;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 3;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 3;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: set the actor's sound (op1, op3) with mode 0, stopping its current
 * one; a zero sound turns the mode off. */
void func_8008CD48(void) {
    D_800B0078->sound = func_800ACDEC(1);
    D_800B0078->sound_mode = 0;
    D_800B0078->sound_volume = func_800ACDEC(3);
    D_800B0078->pc += 5;
    func_800863E8(D_800AFD1C);
    if (D_800B0078->sound == 0) {
        D_800B0078->sound_mode = 0xFF;
    }
}

/* Event: as 8008cd48 with mode 0x80. */
void func_8008CDD4(void) {
    D_800B0078->sound = func_800ACDEC(1);
    D_800B0078->sound_mode = 0x80;
    D_800B0078->sound_volume = func_800ACDEC(3);
    D_800B0078->pc += 5;
    func_800863E8(D_800AFD1C);
    if (D_800B0078->sound == 0) {
        D_800B0078->sound_mode = 0xFF;
    }
}

/* Event: clear party member op1's bit of the game's +1d32. */
void func_8008CE64(void) {
    s32 member = func_8008CF3C(func_800ACDEC(1));

    if (member != 0xFF) {
        D_8005A39C->unk1D32 &= ~(1 << member);
    }
    D_800B0078->pc += 3;
}

/* Event: set party member op1's bit of the game's +1d32. */
void func_8008CED0(void) {
    s32 member = func_8008CF3C(func_800ACDEC(1));

    if (member != 0xFF) {
        D_8005A39C->unk1D32 |= 1 << member;
    }
    D_800B0078->pc += 3;
}

/* Resolve a character id: 0xfd..0xff name the party members, 0xfc none
 * (0xff). */
s32 func_8008CF3C(s32 id) {
    if (id == 0xFF) {
        return D_80062590[2];
    }
    if (id == 0xFE) {
        return D_80062590[1];
    }
    if (id == 0xFD) {
        return D_80062590[0];
    }
    if (id == 0xFC) {
        return 0xFF;
    }
    return id;
}

/* Event: set the actor's character (+80) from operand 1. */
void func_8008CF9C(void) {
    D_800B0078->character = func_8008CF3C(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

/* Event: set the six halfwords at 800b21a0 from raw operands. */
void func_8008CFEC(void) {
    D_800B2078.unk21A0[0] = func_800ACDB8(1);
    D_800B2078.unk21A0[2] = func_800ACDB8(3);
    D_800B2078.unk21A0[1] = func_800ACDB8(5);
    D_800B2078.unk21A0[3] = func_800ACDB8(7);
    D_800B2078.unk21A0[5] = func_800ACDB8(9);
    D_800B2078.unk21A0[4] = func_800ACDB8(11);
    D_800B0078->pc += 13;
}

/* Event: clear (op1 zero) or set the actor's layer bit 11. */
void func_8008D078(void) {
    if (func_800ACDEC(1) == 0) {
        D_800B0078->layer_flags &= ~0x800;
    } else {
        D_800B0078->layer_flags |= 0x800;
    }
    D_800B0078->pc += 3;
}

/* Event: scale the current actor by op1 and rebuild its matrix. */
void func_8008D0F4(void) {
    s32 scale = func_800ACDEC(1);

    D_800AF880.components.descriptors[D_800AFD1C].model->unk2C = (u32)(scale * 3) >> 2;
    D_800B0078->scale[0] = scale;
    D_800B0078->scale[1] = scale;
    D_800B0078->scale[2] = scale;
    func_80072254(D_800AFD1C);
    D_800B0078->pc += 3;
}

/* Event: scale the current actor by (op1, op3, op5) and rebuild its matrix. */
void func_8008D180(void) {
    s16 x = func_800ACDEC(1);
    s16 y = func_800ACDEC(3);
    s16 z = func_800ACDEC(5);

    D_800AF880.components.descriptors[D_800AFD1C].model->unk2C = 0xC00;
    D_800B0078->scale[0] = x;
    D_800B0078->scale[1] = y;
    D_800B0078->scale[2] = z;
    func_80072254(D_800AFD1C);
    D_800B0078->pc += 7;
}

/* Event: set 800b218c from operand 1. */
void func_8008D230(void) {
    D_800B2078.scale = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set the current model's +82 to twice operand 1. */
void func_8008D26C(void) {
    D_800AF880.components.descriptors[D_800AFD1C].model->unk82 = func_800ACDEC(1) * 2;
    D_800B0078->pc += 3;
}

void func_8008D2D8(void) {
}

/* Write a halfword into the event bytecode at `offset`. */
void func_8008D2E0(s32 value, s32 offset) {
    D_800ADC00[offset + 1] = value >> 8;
    D_800ADC00[offset] = value;
}

/* -1 when two descriptors are at least 16 apart in X/Z, else 0. */
s32 func_8008D30C(s32 a, s32 b) {
    return -(func_80099A4C(D_800AF880.components.descriptors[a].matrix.t[0] - D_800AF880.components.descriptors[b].matrix.t[0],
                           D_800AF880.components.descriptors[a].matrix.t[2] - D_800AF880.components.descriptors[b].matrix.t[2]) >= 0x10);
}

/* Copy descriptor `from`'s actor placement (collision triangles, layer,
 * position, +50, +14, +72, +ec), model position and +84, and matrix
 * translation onto descriptor `to`. */
void func_8008D380(s32 to, s32 from) {
    FieldActor *target;
    FieldActor *source;
    s32 i;

    source = D_800AF880.components.descriptors[from].actor;
    target = D_800AF880.components.descriptors[to].actor;
    for (i = 0; i < 4; i++) {
        target->triangle[i] = source->triangle[i];
    }
    target->layer = source->layer;
    target->unk50[0] = source->unk50[0];
    target->unk50[1] = source->unk50[1];
    target->unk50[2] = source->unk50[2];
    target->position[0] = source->position[0];
    target->position[1] = source->position[1];
    target->position[2] = source->position[2];
    target->unkEC = source->unkEC;
    target->unk72 = source->unk72;
    target->unk014 = source->unk014;
    D_800AF880.components.descriptors[to].model->unk84 = D_800AF880.components.descriptors[from].model->unk84;
    D_800AF880.components.descriptors[to].model->position[0] = D_800AF880.components.descriptors[from].model->position[0];
    D_800AF880.components.descriptors[to].model->position[1] = D_800AF880.components.descriptors[from].model->position[1];
    D_800AF880.components.descriptors[to].model->position[2] = D_800AF880.components.descriptors[from].model->position[2];
    D_800AF880.components.descriptors[to].matrix.t[0] = D_800AF880.components.descriptors[from].matrix.t[0];
    D_800AF880.components.descriptors[to].matrix.t[1] = D_800AF880.components.descriptors[from].matrix.t[1];
    D_800AF880.components.descriptors[to].matrix.t[2] = D_800AF880.components.descriptors[from].matrix.t[2];
}

/* Clear the current actor's party bit in 800b219f. */
void func_8008D570(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8005A444[i] == D_800AFD1C) {
            D_800B2078.party_bits &= ~(1 << i);
        }
    }
}

/* Event: set 800b21cd from its byte operand. */
void func_8008D5C8(void) {
    D_800B2078.camera_floor_fixed = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: clear (selector 0) or set (selector 1) the actor's layer bit 10. */
void func_8008D604(void) {
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        D_800B0078->layer_flags &= ~0x400;
        break;
    case 1:
        D_800B0078->layer_flags |= 0x400;
        break;
    }
    D_800B0078->pc += 2;
}

/* Event: set flag bit op1 (variable op1 >> 4, bit op1 & 15). */
void func_8008D684(void) {
    u16 reference = func_800ACDB8(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (func_800ACDB8(1) & 0xF);

    func_800A3074(variable, func_800A3018(variable) | bit);
    D_800B0078->pc += 3;
}

/* Event: clear flag bit op1 (variable op1 >> 4, bit op1 & 15). */
void func_8008D700(void) {
    u16 reference = func_800ACDB8(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (func_800ACDB8(1) & 0xF);

    func_800A3074(variable, func_800A3018(variable) & ~bit);
    D_800B0078->pc += 3;
}

/* Event: continue when flag bit op1 is set, else jump to op3. */
void func_8008D780(void) {
    u16 reference = func_800ACDB8(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (func_800ACDB8(1) & 0xF);

    if (func_800A3018(variable) & bit) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Write a 0x57 (arc move, mode 0x81) instruction with operands `a`, `b`,
 * `c` and 0x0c at the pc, followed by ff 57 8f 26 01 80 57 0f. */
void func_8008D808(s32 a, s32 b, s32 c) {
    EVENT_OPERAND_BYTE(0) = 0x57;
    EVENT_OPERAND_BYTE(1) = 0x81;
    func_8008D2E0(a, D_800B0078->pc + 2);
    func_8008D2E0(b, D_800B0078->pc + 4);
    func_8008D2E0(c, D_800B0078->pc + 6);
    func_8008D2E0(0xC, D_800B0078->pc + 8);
    EVENT_OPERAND_BYTE(0xA) = 0xFF;
    EVENT_OPERAND_BYTE(0xB) = 0x57;
    EVENT_OPERAND_BYTE(0xC) = 0x8F;
    EVENT_OPERAND_BYTE(0xD) = 0x26;
    EVENT_OPERAND_BYTE(0xE) = 1;
    EVENT_OPERAND_BYTE(0xF) = 0x80;
    EVENT_OPERAND_BYTE(0x10) = 0x57;
    EVENT_OPERAND_BYTE(0x11) = 0xF;
}

/* Write a 0x4b instruction with operands `a` and `b` into the event code at
 * pc+0xc (followed by ff ff 80) and advance the pc by 0xc. */
void func_8008DA04(s32 a, s32 b) {
    EVENT_OPERAND_BYTE(0xC) = 0x4B;
    func_8008D2E0(a, D_800B0078->pc + 0xD);
    func_8008D2E0(b, D_800B0078->pc + 0xF);
    EVENT_OPERAND_BYTE(0x11) = 0xFF;
    EVENT_OPERAND_BYTE(0x12) = 0xFF;
    EVENT_OPERAND_BYTE(0x13) = 0x80;
    D_800B0078->pc += 0xC;
}

/* Event: clear the actor's +75 (0xff). */
void func_8008DAFC(void) {
    D_800B0078->unk075 = 0xFF;
    D_800B0078->pc++;
}

/* Event: set 800b233e to the selected actor. */
void func_8008DB2C(void) {
    D_800B2078.unk233E = func_8009CDB4(1);
    D_800B0078->pc += 2;
}

/* Add to party member `member`'s points, capped at its maximum. Declared
 * int but returns nothing. */
s32 func_8008DB68(s32 member, s32 amount) {
    s32 character = func_8001ACF0(D_80062590[member]);

    if (character != 0xFF) {
        D_8005A39C->gears[character].points += amount;
        if (D_8005A39C->gears[character].points_max < D_8005A39C->gears[character].points) {
            D_8005A39C->gears[character].points = D_8005A39C->gears[character].points_max;
        }
    }
}

/* Take from party member `member`'s points, leaving at least one. Declared
 * int but returns nothing. */
s32 func_8008DBF0(s32 member, s32 amount) {
    s32 character = func_8001ACF0(D_80062590[member]);
    s32 points;

    if (character != 0xFF) {
        points = D_8005A39C->gears[character].points - amount;
        if (points <= 0) {
            points = 1;
        }
        D_8005A39C->gears[character].points = points;
    }
}

/* Event: add operand 1 to the points of the party members the byte-3 mask
 * names. */
void func_8008DC74(void) {
    s32 i;
    s32 amount = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    s32 mask = D_800AEA2C[D_800ADC00[D_800B0078->pc + 3] & 3];

    for (i = 0; i < 3; i++) {
        if (D_80062590[i] != 0xFF && (mask & 1)) {
            func_8008DB68(i, amount);
        }
        mask >>= 1;
    }
    D_800B0078->pc += 4;
}

/* Event: take operand 1 from the points of the party members the byte-3
 * mask names. */
void func_8008DD6C(void) {
    s32 i;
    s32 amount = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    s32 mask = D_800AEA2C[D_800ADC00[D_800B0078->pc + 3] & 3];

    for (i = 0; i < 3; i++) {
        if (D_80062590[i] != 0xFF && (mask & 1)) {
            func_8008DBF0(i, amount);
        }
        mask >>= 1;
    }
    D_800B0078->pc += 4;
}

/* Event: set the actor's +75 to the selected actor, if any. */
void func_8008DE64(void) {
    s32 actor = func_8009CDB4(1);

    if (actor != 0xFF) {
        D_800B0078->unk075 = actor;
    }
    D_800B0078->pc += 2;
}

/* Event: store the selected actor's flags in variable op1. */
void func_8008DEBC(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, actor->flags);
    }
    D_800B0078->pc += 3;
}

/* Event: store the selected actor's flag halfword 1 in variable op1. */
void func_8008DF44(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 1));
    }
    D_800B0078->pc += 3;
}

/* Event: store the selected actor's layer flags in variable op1. */
void func_8008DFCC(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, actor->layer_flags);
    }
    D_800B0078->pc += 3;
}

/* Event: store the selected actor's flag halfword 3 in variable op1. */
void func_8008E054(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 3));
    }
    D_800B0078->pc += 3;
}

/* Continue past a 5-byte instruction when `flags` has any bit of op1, else
 * jump to op4. */
void func_8008E0DC(s32 flags) {
    if (func_800ACDB8(1) & flags & 0xFFFF) {
        D_800B0078->pc += 6;
    } else {
        D_800B0078->pc = func_800ACDB8(4);
    }
}

/* Continue past a 4-byte instruction when `flags` has any bit of op1, else
 * jump to op3. */
void func_8008E148(s32 flags) {
    if (func_800ACDB8(1) & flags & 0xFFFF) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Event: store the planar distance between the actors selected by bytes 3
 * and 4 (0 when either is missing) in variable op1. */
void func_8008E1B4(void) {
    s32 distance = 0;
    s32 a = func_8009CDB4(3);
    s32 b = func_8009CDB4(4);
    s32 ax, az, bx, bz;

    if (a != 0xFF && b != 0xFF) {
        ax = D_800AF880.components.descriptors[a].actor->position[0] >> 16;
        bx = D_800AF880.components.descriptors[b].actor->position[0] >> 16;
        az = D_800AF880.components.descriptors[a].actor->position[2] >> 16;
        bz = D_800AF880.components.descriptors[b].actor->position[2] >> 16;
        distance = func_80099A4C(ax - bx, az - bz);
    }
    func_800A3074(func_800ACDB8(1) & 0xFFFF, distance);
    D_800B0078->pc += 5;
}

/* Event: test the selected actor's flag halfword 0 (8008e0dc). */
void func_8008E298(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 0));
}

/* Event: test the selected actor's flag halfword 1 (8008e0dc). */
void func_8008E2EC(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 1));
}

/* Event: test the selected actor's flag halfword 2 (8008e0dc). */
void func_8008E340(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 2));
}

/* Event: test the selected actor's flag halfword 3 (8008e0dc). */
void func_8008E394(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 3));
}

/* Event: test the current actor's flag halfword 0 (8008e148). */
void func_8008E3E8(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 0));
}

/* Event: test the current actor's flag halfword 1 (8008e148). */
void func_8008E414(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 1));
}

/* Event: test the current actor's flag halfword 2 (8008e148). */
void func_8008E440(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 2));
}

/* Event: test the current actor's flag halfword 3 (8008e148). */
void func_8008E46C(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 3));
}

/* Store `value` in variable op1. */
void func_8008E498(s32 value) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value & 0xFFFF);
    D_800B0078->pc += 3;
}

/* Event: store the current actor's flag halfword 0 in variable op1. */
void func_8008E4EC(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 0));
}

/* Event: store the current actor's flag halfword 1 in variable op1. */
void func_8008E518(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 1));
}

/* Event: store the current actor's flag halfword 2 in variable op1. */
void func_8008E544(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 2));
}

/* Event: store the current actor's flag halfword 3 in variable op1. */
void func_8008E570(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 3));
}

/* Event: by selector byte 1, set (0-3) or clear (4-7) raw operand 2 in the
 * low or high half of the actor's flag word or layer flag word. */
void func_8008E59C(void) {
    u32 bits = func_800ACDB8(2) & 0xFFFF;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        D_800B0078->flags |= bits;
        break;
    case 1:
        D_800B0078->flags |= bits << 16;
        break;
    case 2:
        D_800B0078->layer_flags |= bits;
        break;
    case 3:
        D_800B0078->layer_flags |= bits << 16;
        break;
    case 4:
        D_800B0078->flags &= ~bits;
        break;
    case 5:
        D_800B0078->flags &= ~(bits << 16);
        break;
    case 6:
        D_800B0078->layer_flags &= ~bits;
        break;
    case 7:
        D_800B0078->layer_flags &= ~(bits << 16);
        break;
    }
    D_800B0078->pc += 4;
}

/* Draw 800b229c distinct random numbers 1..(800b2298 + 1) into 800b22a0
 * (none when the count is zero, which also clears the range). */
void func_8008E718(void) {
    s32 i;
    s32 j;
    s32 pick;

    D_800B2078.unk2294 = D_800B2078.unk2298;
    if (D_800B2078.unk229C == 0) {
        D_800B2078.unk2298 = 0;
        return;
    }
    for (i = 0; i < 32; i++) {
        D_800B2078.unk22A0[i] = 0xFFFF;
    }
    for (i = 0; i < D_800B2078.unk229C; i++) {
    retry:
        pick = (rand() * (D_800B2078.unk2298 + 1)) >> 15 & 0xFFFF;
        for (j = 0; j < 32; j++) {
            if (D_800B2078.unk22A0[j] == pick) {
                goto retry;
            }
        }
        D_800B2078.unk22A0[i] = pick;
    }
    for (i = 0; i < D_800B2078.unk229C; i++) {
        D_800B2078.unk22A0[i]++;
    }
}

/* Event: set 800b2298 and the 800b229c count (at most 32) from operands 1
 * and 3, then apply them (8008e718). */
void func_8008E85C(void) {
    D_800B2078.unk2298 = func_800ACDEC(1);
    D_800B2078.unk229C = func_800ACDEC(3);
    if (D_800B2078.unk229C > 0x20) {
        D_800B2078.unk229C = 0x20;
    }
    func_8008E718();
    D_800B0078->pc += 5;
}

/* Event: by selector byte, clear (0) or set (1, keeping the heading goal)
 * flag 0x8000, or set layer bit 19 (2); clearing also stops a model moving
 * under bit 19. */
void func_8008E8C8(void) {
    FieldModel *model;

    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        if (D_800B0078->flags & 0x8000) {
            D_800B0078->flags &= ~0x8000;
        }
        if (D_800B0078->layer_flags & 0x80000) {
            model = D_800AF880.components.descriptors[D_800AFD1C].model;
            model->unk18 = 0;
            model->unk14 = 0;
            model->unk0C = 0;
            D_800B0078->layer_flags &= ~0x80000;
        }
        break;
    case 1:
        D_800B0078->flags |= 0x8000;
        D_800B0078->unk11C = D_800B0078->heading_goal;
        break;
    case 2:
        D_800B0078->layer_flags |= 0x80000;
        break;
    }
    D_800B0078->pc += 2;
}

/* Event: wait for 800adb7c, clearing it once seen; yield each time. */
void func_8008E9F8(void) {
    if (D_800ADB7C == 0) {
        D_800B0078->pc--;
    } else {
        D_800ADB7C = 0;
        D_800B0078->pc++;
    }
    D_800B00C0 = 1;
}

#ifdef NON_MATCHING
/* Event 0xa0: once sound is available, request a movie: file op1, the
 * parameters at 800c3a2a/2c/2e from op3/op5/op7 and its sound bank op9
 * (selected operands, flags byte 11), with the default window and fade. */
void func_8008EA58(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    D_800C3A20 = func_8009CF78(1, EVENT_OPERAND_BYTE(0xB));
    D_800C3A2A = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xB));
    D_800C3A2C = func_8009D000(5, EVENT_OPERAND_BYTE(0xB));
    D_800C3A2E = func_8009D044(7, EVENT_OPERAND_BYTE(0xB));
    D_800C3A38 = func_8009D088(9, EVENT_OPERAND_BYTE(0xB));
    D_800C3A32 = 0x140;
    D_800ADB80 = 0x40;
    D_800C3A36 = 1;
    D_800C3A34 = 0x100;
    D_800C3A26 = 0;
    D_800C3A24 = 0;
    D_800C3A22 = 0;
    D_800C3A28 = 0x100;
    D_800C3A3A = 0;
    D_800C3A30 &= 0xF;
    D_800ADB74 = 0;
    D_800ADB70 = 1;
    D_800B00C0 = 1;
    D_800B0078->pc += 0xC;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EA58);
#endif

#ifdef NON_MATCHING
/* Event 0x60: once sound is available, request movie op1 with parameters
 * op3/op5; op7's low nibble picks the display layout (0: half-width at
 * x 0x140, 1: full 16-bit, 2: full 24-bit) and its 0xc0 bits the fade. */
void func_8008EC30(void) {
    s32 mode;
    s32 layout;

    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    D_800C3A20 = func_800ACDEC(1);
    D_800C3A2A = func_800ACDEC(3);
    D_800C3A2E = func_800ACDEC(5);
    mode = func_800ACDEC(7);
    layout = mode & 0xF;
    D_800C3A30 = mode;
    D_800ADB80 = mode & 0xC0;
    D_800C3A32 = 0x140;
    D_800C3A34 = 0x100;
    D_800C3A30 = layout;
    D_800C3A2C = 1;
    switch (layout) {
    case 0:
        D_800C3A22 = 0x140;
        D_800C3A24 = 0;
        D_800C3A26 = 0x140;
        D_800C3A28 = 0x100;
        D_800ADB74 = 1;
        D_800C3A36 = 0;
        break;
    case 1:
        D_800C3A26 = 0;
        D_800C3A24 = 0;
        D_800C3A22 = 0;
        D_800C3A28 = 0x100;
        D_800ADB74 = 0;
        D_800C3A36 = 0;
        break;
    case 2:
        D_800C3A26 = 0;
        D_800C3A24 = 0;
        D_800C3A22 = 0;
        D_800C3A28 = 0x100;
        D_800ADB74 = 0;
        D_800C3A36 = 1;
        break;
    }
    D_800C3A38 = 0xFF;
    D_800C3A3A = 0;
    D_800ADB70 = 1;
    D_800B00C0 = 1;
    D_800B0078->pc += 9;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EC30);
#endif

#ifdef NON_MATCHING
/* Event 0x67: request movie op1 with parameters op3/op5/op7, mode op9 (0xfe
 * marks 800c3a3a, 0x40 the fade), window position op11/op13 and size
 * op15/op17, without a sound bank. */
void func_8008EE14(void) {
    s32 mode;
    u16 x;
    u16 y;

    D_800C3A20 = func_800ACDEC(1);
    D_800C3A2A = func_800ACDEC(3);
    D_800C3A2C = func_800ACDEC(5);
    D_800C3A2E = func_800ACDEC(7);
    D_800C3A30 = func_800ACDEC(9);
    mode = D_800C3A30;
    if (mode == 0xFE) {
        D_800C3A3A = 1;
    } else {
        D_800C3A3A = 0;
    }
    x = func_800ACDEC(0xB);
    D_800C3A22 = x;
    D_800C3A26 = x;
    y = func_800ACDEC(0xD);
    D_800C3A24 = y;
    D_800C3A28 = y;
    D_800C3A32 = func_800ACDEC(0xF);
    D_800C3A34 = func_800ACDEC(0x11);
    D_800ADB80 = mode & 0x40;
    D_800C3A38 = 0xFF;
    D_800ADB74 = 2;
    D_800C3A36 = 0;
    D_800C3A30 &= 0xF;
    D_800ADB70 = 1;
    D_800B0078->pc += 0x13;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EE14);
#endif

/* Event: request transition 1 with operand 1 (800adb38/800adb3c). */
void func_8008EF5C(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 1;
    D_800B0078->pc += 3;
}

/* Event: request transition 2 with operand 1 (800adb38/800adb3c). */
void func_8008EFA0(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 2;
    D_800B0078->pc += 3;
}

/* Event: set both draw buffers' clip areas from four operands. */
void func_8008EFE4(void) {
    s32 x = func_800ACDEC(1);
    s32 y = func_800ACDEC(3);
    s32 w = func_800ACDEC(5);

    func_80071F64(x, y, w, func_800ACDEC(7));
    D_800B0078->pc += 9;
}

extern s32 D_800ADB38;
extern s32 D_800ADB3C;

/* Store an operand in D_800ADB3C and set D_800ADB38 to 3. */
void func_8008F070(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 3;
    D_800B0078->pc += 3;
}

/* Set a selected actor's colour triples; mode bits 1/2 select each triple. */
void func_8008F0B4(void) {
    FieldActor *actor;
    s32 index;

    index = func_8009CDB4(2);
    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        if (EVENT_OPERAND_BYTE(1) & 1) {
            actor->color0[0] = func_800ACDEC(3);
            actor->color0[1] = func_800ACDEC(5);
            actor->color0[2] = func_800ACDEC(7);
        }
        if (EVENT_OPERAND_BYTE(1) & 2) {
            actor->color1[0] = func_800ACDEC(3);
            actor->color1[1] = func_800ACDEC(5);
            actor->color1[2] = func_800ACDEC(7);
        }
    }
    D_800B0078->pc += 9;
}

/* Set the current actor's colour triples; mode bits 1/2 select each triple. */
void func_8008F1C8(void) {
    if (EVENT_OPERAND_BYTE(1) & 1) {
        D_800B0078->color0[0] = func_800ACDEC(2);
        D_800B0078->color0[1] = func_800ACDEC(4);
        D_800B0078->color0[2] = func_800ACDEC(6);
    }
    if (EVENT_OPERAND_BYTE(1) & 2) {
        D_800B0078->color1[0] = func_800ACDEC(2);
        D_800B0078->color1[1] = func_800ACDEC(4);
        D_800B0078->color1[2] = func_800ACDEC(6);
    }
    D_800B0078->pc += 8;
}

void func_80023290(FieldModel *model, s32 value);

/* Pass an operand to resident 80023290 with the current descriptor's word 04. */
void func_8008F2D8(void) {
    s32 value = func_800ACDEC(1);

    func_80023290(D_800AF880.components.descriptors[D_800AFD1C].model, value);
    D_800B0078->pc += 3;
}

extern s32 D_800C3A5C;
extern s32 D_800C3A60;

/* Store two operands in D_800C3A5C/D_800C3A60. */
void func_8008F348(void) {
    D_800C3A5C = func_800ACDEC(1);
    D_800C3A60 = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Set which field effects are kept across a reload. */
void func_8008F394(void) {
    D_800B2078.effects_kept = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

void func_8003A450(s32 a, s32 b, s32 c);

/* Resident 8003A450(2 * op3, op1, op5). */
void func_8008F3D0(void) {
    s32 a;
    s32 b;

    a = func_800ACDEC(3) * 2;
    b = func_800ACDEC(1);
    func_8003A450(a, b, func_800ACDEC(5));
    D_800B0078->pc += 7;
}

void func_8003A344(s32 a, s32 b);

/* Resident 8003A344(2 * op3, op1). */
void func_8008F444(void) {
    s32 a;

    a = func_800ACDEC(3) * 2;
    func_8003A344(a, func_800ACDEC(1));
    D_800B0078->pc += 5;
}

void func_8003A55C(s32 a, s32 b);

/* Resident 8003A55C(2 * op3, op1). */
void func_8008F4A0(void) {
    s32 a;

    a = func_800ACDEC(3) * 2;
    func_8003A55C(a, func_800ACDEC(1));
    D_800B0078->pc += 5;
}

/* Field 80085634(op1, op3). */
void func_8008F4FC(void) {
    s32 a;

    a = func_800ACDEC(1);
    func_80085634(a, func_800ACDEC(3));
    D_800B0078->pc += 5;
}

void func_800855C8(s32 a, s32 b, s32 c, s32 d);

/* Field 800855C8(op1, op5, op3, op7). */
void func_8008F558(void) {
    s32 a;
    s32 b;
    s32 c;

    a = func_800ACDEC(1);
    b = func_800ACDEC(5);
    c = func_800ACDEC(3);
    func_800855C8(a, b, c, func_800ACDEC(7));
    D_800B0078->pc += 9;
}

s32 func_8003A5D0(s32 mask);

/* Re-run this opcode each frame while any operand button (<< 8) is held. */
void func_8008F5E4(void) {
    s32 buttons;

    buttons = func_8003A5D0(-1);
    if (!(buttons & (func_800ACDEC(1) << 8))) {
        D_800B0078->pc += 3;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

/* Field 80085634(op1, 3). */
void func_8008F668(void) {
    func_80085634(func_800ACDEC(1), 3);
    D_800B0078->pc += 3;
}

/* Field 800855C8(op1, op5, op3, 3). */
void func_8008F6AC(void) {
    s32 a;
    s32 b;

    a = func_800ACDEC(1);
    b = func_800ACDEC(5);
    func_800855C8(a, b, func_800ACDEC(3), 3);
    D_800B0078->pc += 7;
}

extern s32 D_800ADBDC;
extern s32 D_8004F340;
void func_8008F7B8(void);

/* Request field music with D_8004F340 = 0, or yield while music is disabled. */
void func_8008F724(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        return;
    }
    D_8004F340 = 0;
    func_8008F7B8();
}

/* Request field music with D_8004F340 = -1, or yield while music is disabled. */
void func_8008F76C(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        return;
    }
    D_8004F340 = -1;
    func_8008F7B8();
}

extern s32 D_8004F308;
extern s32 D_8004F324;
extern s32 D_8004F354;
void func_80085EEC(void);
void func_8001B66C(void);
s32 func_8008A558(void);
void func_80085B20(s32 track, s32 arg1);

#ifdef NON_MATCHING
/* Select the field music track (operand 1). Without D_800ADB1C the track is
 * only recorded; otherwise yield until the music system can take a change. */
void func_8008F7B8(void) {
    s32 track;

    track = func_800ACDEC(1);
    if (D_800ADB1C == 0) {
        func_80085EEC();
        if (track != D_8004F324) {
            func_8001B66C();
            D_8004F308 = -1;
        }
        D_8004F324 = track;
        D_800B0078->pc += 3;
    } else if (func_8008A558() != 0 || D_800ADBDC == 0) {
        D_800B00C0 = 1;
    } else if (D_8004F354 != 1 && D_8004F308 != -1) {
        if (track != D_8004F324) {
            func_8001B66C();
            D_8004F324 = track;
            D_8004F308 = -1;
            func_80085B20(track, 0);
        }
        D_800B0078->pc += 3;
    } else {
        D_800B00C0 = 1;
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F7B8);
#endif

/* Start a camera shake toward three amplitudes over a frame count. */
void func_8008F90C(void) {
    s32 x;
    s32 y;
    s32 z;
    s32 frames;

    x = func_800ACDEC(1);
    y = func_800ACDEC(3);
    z = func_800ACDEC(5);
    frames = func_800ACDEC(7);
    if (frames == 0) {
        frames = 1;
    }
    D_800B0078->pc += 9;
    D_800AF880.shake = 1;
    D_800AF880.shake_time = frames;
    D_800AF880.shake_step[0] = ((x << 16) - D_800AF880.shake_amplitude[0]) / frames;
    D_800AF880.shake_step[1] = ((z << 16) - D_800AF880.shake_amplitude[1]) / frames;
    D_800AF880.shake_step[2] = ((y << 16) - D_800AF880.shake_amplitude[1]) / frames;
    if (x == 0 && z == 0 && y == 0) {
        D_800AF880.shake_time = frames + 2;
        D_800AF880.shake_stop = 1;
        return;
    }
    D_800AF880.shake_stop = 0;
}

/* Yield; advance once the requested camera moves (bit 1: target, bit 0: eye)
 * have no steps left. */
void func_8008FA38(void) {
    s32 mask;
    s32 wanted;

    wanted = func_800ACDEC(1);
    mask = 3;
    if (D_800AF880.target_steps == 0) {
        mask = 2;
    }
    if (D_800AF880.eye_steps == 0) {
        mask &= 1;
    }
    D_800B00C0 = 1;
    if (!(mask & wanted)) {
        D_800B0078->pc += 3;
    }
}

/* Set the camera heading from a selected operand. */
void func_8008FABC(void) {
    s16 heading;

    heading = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    D_800AF880.heading_angles.vy = heading;
    D_800AF880.heading = heading;
    D_800B0078->pc += 4;
}

/* Copy the working elevation/heading/zoom into the scripted camera. */
void func_8008FB28(void) {
    D_800AF880.scripted_scale = 0x1000;
    D_800AF880.scripted_heading = D_800AF880.heading_angles.vy;
    D_800AF880.scripted_elevation = D_800AF880.elevation;
    D_800AF880.scripted_zoom = (D_800AF880.projection * D_800AF880.distance) >> 12;
    D_800B0078->pc += 1;
}

/* Switch to the scripted camera now, from the working parameters. */
void func_8008FB98(void) {
    D_800AF880.mode = 1;
    D_800AFC7C += 4;
    D_800B0078->pc += 1;
    D_800AF880.scripted_scale = 0x1000;
    D_800AF880.target_a = 12;
    D_800AF880.target_b = 12;
    D_800AF880.flags |= 0x8000;
    D_800AF880.scripted_heading = D_800AF880.heading_angles.vy;
    D_800AF880.scripted_elevation = D_800AF880.elevation;
    D_800AF880.scripted_zoom = (D_800AF880.projection * D_800AF880.distance) >> 12;
}


/* Leave the scripted camera: mode 0 just clears the hold flag; mode 1 either
 * ends at once (operand 0, also skipping the next opcode) or blends back over
 * the operand's frame count (mode 2). */
void func_8008FC4C(void) {
    s32 frames;

    switch (D_800AF880.mode) {
    case 0:
        D_800AF880.flags &= 0x7FFF;
        D_800B0078->pc += 3;
        break;
    case 1:
        frames = func_800ACDEC(1);
        if (frames == 0) {
            D_800AF880.mode = 0;
            D_800AF880.flags &= 0x7FFF;
            D_800B0078->pc += 3;
            D_800B2078.camera_counter = 2;
        } else {
            D_800AF880.mode = 2;
            D_800AF880.target_a = frames;
            D_800AF880.target_b = frames;
        }
        D_800B0078->pc += 3;
        break;
    case 2:
        break;
    }
}

/* Set the scripted camera's two blend frame counts (at least 1). */
void func_8008FD40(void) {
    s32 frames;

    D_800AF880.target_a = func_800ACDEC(1);
    frames = func_800ACDEC(3);
    D_800AF880.target_b = frames;
    if (D_800AF880.target_a == 0) {
        D_800AF880.target_a = 1;
    }
    if (frames == 0) {
        D_800AF880.target_b = 1;
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 5;
}

/* Save the target goal. */
void func_8008FDD0(void) {
    D_800AF880.saved_target.vx = D_800AF880.target_goal.vx;
    D_800AF880.saved_target.vy = D_800AF880.target_goal.vy;
    D_800AF880.saved_target.vz = D_800AF880.target_goal.vz;
    D_800AFC7C += 1;
    D_800B0078->pc += 1;
}

/* Set the saved target from three selected operands (whole units). */
void func_8008FE2C(void) {
    D_800AF880.saved_target.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_target.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_target.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Point A follows a selected actor's position. */
void func_8008FF04(void) {
    FieldActor *actor;

    actor = D_800AF880.components.descriptors[func_8009CD7C(1)].actor;
    D_800AF880.point_actor_a.vx = actor->position[0];
    D_800AF880.point_actor_a.vy = actor->position[1];
    D_800AF880.point_actor_a.vz = actor->position[2];
    D_800AFC7C += 1;
    D_800B0078->pc += 2;
}

/* Set point A from three selected operands (whole units). */
void func_8008FF90(void) {
    D_800AF880.point_actor_a.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_a.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_a.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Save the eye goal. */
void func_80090068(void) {
    D_800AF880.saved_eye.vx = D_800AF880.eye_goal.vx;
    D_800AF880.saved_eye.vy = D_800AF880.eye_goal.vy;
    D_800AF880.saved_eye.vz = D_800AF880.eye_goal.vz;
    D_800AFC7C += 1;
    D_800B0078->pc += 1;
}

/* Set the saved eye from three selected operands (whole units). */
void func_800900C4(void) {
    D_800AF880.saved_eye.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_eye.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_eye.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Point B follows a selected actor's position. */
void func_8009019C(void) {
    FieldActor *actor;

    actor = D_800AF880.components.descriptors[func_8009CD7C(1)].actor;
    D_800AF880.point_actor_b.vx = actor->position[0];
    D_800AF880.point_actor_b.vy = actor->position[1];
    D_800AF880.point_actor_b.vz = actor->position[2];
    D_800AFC7C += 1;
    D_800B0078->pc += 2;
}

/* Set point B from three selected operands (whole units). */
void func_80090228(void) {
    D_800AF880.point_actor_b.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_b.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_b.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Reset the saved/point targets to the target goal and eyes to the eye goal. */
void func_80090300(void) {
    D_800AF880.point_actor_a.vx = D_800AF880.saved_target.vx = D_800AF880.target_goal.vx;
    D_800AF880.point_actor_a.vy = D_800AF880.saved_target.vy = D_800AF880.target_goal.vy;
    D_800AF880.point_actor_a.vz = D_800AF880.saved_target.vz = D_800AF880.target_goal.vz;
    D_800AF880.saved_eye.vx = D_800AF880.eye_goal.vx;
    D_800AF880.saved_eye.vy = D_800AF880.eye_goal.vy;
    D_800AF880.saved_eye.vz = D_800AF880.eye_goal.vz;
    D_800AF880.point_actor_b.vx = D_800AF880.eye_goal.vx;
    D_800AF880.point_actor_b.vy = D_800AF880.eye_goal.vy;
    D_800AF880.point_actor_b.vz = D_800AF880.eye_goal.vz;
    D_800AFC7C += 1;
    D_800B0078->pc += 1;
}

#ifdef NON_MATCHING
/* Event 0xac: start a scripted camera move. Mode 0 (1) moves the target
 * (eye) from its saved point to point A (B) over op2 steps; mode 2 (3) moves
 * it along the same line at op2 units per step. Byte-1 bit 0x80 also snaps
 * the live target (eye) to the start. */
void func_800903BC(void) {
    s32 mode;
    VECTOR delta;
    VECTOR direction;
    s32 distance;
    s32 speed;

    mode = EVENT_OPERAND_BYTE(1) & 0xF;
    switch (mode) {
    case 0:
        D_800AF880.target_steps = func_800ACDEC(2);
        if (D_800AF880.target_steps == 0) {
            D_800AF880.target_steps++;
            D_800AF880.target_a = 1;
        }
        D_800AF880.scripted_target.vx = D_800AF880.saved_target.vx;
        D_800AF880.scripted_target.vy = D_800AF880.saved_target.vy;
        D_800AF880.scripted_target.vz = D_800AF880.saved_target.vz;
        D_800AF880.scripted |= 1;
        D_800AF880.target_step.vx = (D_800AF880.point_actor_a.vx - D_800AF880.saved_target.vx) / D_800AF880.target_steps;
        D_800AF880.target_step.vy = (D_800AF880.point_actor_a.vy - D_800AF880.saved_target.vy) / D_800AF880.target_steps;
        D_800AF880.target_step.vz = (D_800AF880.point_actor_a.vz - D_800AF880.saved_target.vz) / D_800AF880.target_steps;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.target.vx = D_800AF880.saved_target.vx;
            D_800AF880.target.vy = D_800AF880.saved_target.vy;
            D_800AF880.target.vz = D_800AF880.saved_target.vz;
        }
        break;
    case 2:
        delta.vx = (D_800AF880.saved_target.vx - D_800AF880.point_actor_a.vx) >> 16;
        delta.vy = (D_800AF880.saved_target.vy - D_800AF880.point_actor_a.vy) >> 16;
        delta.vz = (D_800AF880.saved_target.vz - D_800AF880.point_actor_a.vz) >> 16;
        func_80048D7C(&delta, &direction);
        distance = func_80099A04((D_800AF880.saved_target.vx - D_800AF880.point_actor_a.vx) >> 16,
                                 (D_800AF880.saved_target.vy - D_800AF880.point_actor_a.vy) >> 16,
                                 (D_800AF880.saved_target.vz - D_800AF880.point_actor_a.vz) >> 16);
        speed = func_800ACDEC(2);
        D_800AF880.scripted_target.vx = D_800AF880.saved_target.vx;
        D_800AF880.scripted_target.vy = D_800AF880.saved_target.vy;
        D_800AF880.scripted_target.vz = D_800AF880.saved_target.vz;
        D_800AF880.scripted |= 1;
        D_800AF880.target_step.vx = -(direction.vx * speed) * 16;
        D_800AF880.target_step.vy = -(direction.vy * speed) * 16;
        D_800AF880.target_step.vz = -(direction.vz * speed) * 16;
        D_800AF880.target_steps = distance / speed;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.target.vx = D_800AF880.saved_target.vx;
            D_800AF880.target.vy = D_800AF880.saved_target.vy;
            D_800AF880.target.vz = D_800AF880.saved_target.vz;
        }
        break;
    case 3:
        delta.vx = (D_800AF880.saved_eye.vx - D_800AF880.point_actor_b.vx) >> 16;
        delta.vy = (D_800AF880.saved_eye.vy - D_800AF880.point_actor_b.vy) >> 16;
        delta.vz = (D_800AF880.saved_eye.vz - D_800AF880.point_actor_b.vz) >> 16;
        func_80048D7C(&delta, &direction);
        distance = func_80099A04((D_800AF880.saved_eye.vx - D_800AF880.point_actor_b.vx) >> 16,
                                 (D_800AF880.saved_eye.vy - D_800AF880.point_actor_b.vy) >> 16,
                                 (D_800AF880.saved_eye.vz - D_800AF880.point_actor_b.vz) >> 16);
        speed = func_800ACDEC(2);
        D_800AF880.scripted_eye[0] = D_800AF880.saved_eye.vx;
        D_800AF880.scripted_eye[1] = D_800AF880.saved_eye.vy;
        D_800AF880.scripted_eye[2] = D_800AF880.saved_eye.vz;
        D_800AF880.scripted |= 2;
        D_800AF880.eye_step[0] = -(direction.vx * speed) * 16;
        D_800AF880.eye_step[1] = -(direction.vy * speed) * 16;
        D_800AF880.eye_step[2] = -(direction.vz * speed) * 16;
        D_800AF880.eye_steps = distance / speed;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.eye.vx = D_800AF880.saved_eye.vx;
            D_800AF880.eye.vy = D_800AF880.saved_eye.vy;
            D_800AF880.eye.vz = D_800AF880.saved_eye.vz;
        }
        break;
    case 1:
        D_800AF880.eye_steps = func_800ACDEC(2);
        if (D_800AF880.eye_steps == 0) {
            D_800AF880.eye_steps++;
            D_800AF880.target_b = 1;
        }
        D_800AF880.scripted_eye[0] = D_800AF880.saved_eye.vx;
        D_800AF880.scripted_eye[1] = D_800AF880.saved_eye.vy;
        D_800AF880.scripted_eye[2] = D_800AF880.saved_eye.vz;
        D_800AF880.scripted |= 2;
        D_800AF880.eye_step[0] = (D_800AF880.point_actor_b.vx - D_800AF880.saved_eye.vx) / D_800AF880.eye_steps;
        D_800AF880.eye_step[1] = (D_800AF880.point_actor_b.vy - D_800AF880.saved_eye.vy) / D_800AF880.eye_steps;
        D_800AF880.eye_step[2] = (D_800AF880.point_actor_b.vz - D_800AF880.saved_eye.vz) / D_800AF880.eye_steps;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.eye.vx = D_800AF880.saved_eye.vx;
            D_800AF880.eye.vy = D_800AF880.saved_eye.vy;
            D_800AF880.eye.vz = D_800AF880.saved_eye.vz;
        }
        break;
    }
    D_800B0078->pc += 4;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800903BC);
#endif

/* Store the camera target's whole x, z, y in three variables. */
void func_80090A10(void) {
    func_800A3074(func_800ACDB8(1), WHOLE(D_800AF880.target.vx));
    func_800A3074(func_800ACDB8(3), WHOLE(D_800AF880.target.vz));
    func_800A3074(func_800ACDB8(5), WHOLE(D_800AF880.target.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the camera eye's whole x, z, y in three variables. */
void func_80090A94(void) {
    func_800A3074(func_800ACDB8(1), WHOLE(D_800AF880.eye.vx));
    func_800A3074(func_800ACDB8(3), WHOLE(D_800AF880.eye.vz));
    func_800A3074(func_800ACDB8(5), WHOLE(D_800AF880.eye.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the target goal's whole x, z, y in three variables. */
void func_80090B18(void) {
    func_800A3074(func_800ACDB8(1), WHOLE(D_800AF880.target_goal.vx));
    func_800A3074(func_800ACDB8(3), WHOLE(D_800AF880.target_goal.vz));
    func_800A3074(func_800ACDB8(5), WHOLE(D_800AF880.target_goal.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the eye goal's whole x, z, y in three variables. */
void func_80090B9C(void) {
    func_800A3074(func_800ACDB8(1), WHOLE(D_800AF880.eye_goal.vx));
    func_800A3074(func_800ACDB8(3), WHOLE(D_800AF880.eye_goal.vz));
    func_800A3074(func_800ACDB8(5), WHOLE(D_800AF880.eye_goal.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Mode 0: store the scripted heading in a variable; otherwise set it from
 * the raw operand. */
void func_80090C20(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1), D_800AF880.scripted_heading);
    } else {
        D_800AF880.scripted_heading = func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Mode 0: store the scripted elevation in a variable; otherwise set it from
 * the raw operand. */
void func_80090CB8(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1), D_800AF880.scripted_elevation);
    } else {
        D_800AF880.scripted_elevation = func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Mode 0: store the scripted zoom in a variable; otherwise set it from the
 * raw operand. */
void func_80090D50(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1), D_800AF880.scripted_zoom);
    } else {
        D_800AF880.scripted_zoom = (u16)func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Store the scripted heading, elevation and zoom in three variables. */
void func_80090DEC(void) {
    func_800A3074(func_800ACDB8(1), D_800AF880.scripted_heading);
    func_800A3074(func_800ACDB8(3), D_800AF880.scripted_elevation);
    func_800A3074(func_800ACDB8(5), D_800AF880.scripted_zoom);
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}


/* Derive a scripted heading, pitch and zoom from point A looking at point B
 * and store them in three variables. */
void func_80090E70(void) {
    VECTOR a;
    VECTOR b;
    s32 heading;
    s32 pitch;
    s32 zoom;

    a.vx = D_800AF880.point_actor_a.vx;
    a.vy = D_800AF880.point_actor_a.vy;
    a.vz = D_800AF880.point_actor_a.vz;
    b.vx = D_800AF880.point_actor_b.vx;
    b.vy = D_800AF880.point_actor_b.vy;
    b.vz = D_800AF880.point_actor_b.vz;
    zoom = func_80099A04((b.vx - a.vx) >> 16, (b.vy - a.vy) >> 16,
                              (b.vz - a.vz) >> 16) / 2;
    D_800AF880.scripted_scale = 0x1000;
    heading = ((-ratan2(a.vz - b.vz, a.vx - b.vx) & 0xFFFF) - 0x400) & 0xFFF;
    pitch = ((-ratan2(func_80099A4C((b.vx - a.vx) >> 16, (b.vz - a.vz) >> 16),
                             (a.vy - b.vy) >> 16) * 360) >> 12) + 91;
    func_800A3074(func_800ACDB8(1), heading);
    func_800A3074(func_800ACDB8(3), pitch);
    func_800A3074(func_800ACDB8(5), zoom);
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Rotate `point` about `center` in the XZ plane by `angle` (result mirrored
 * through the center, as the original subtracts center - point). */
void func_80091008(VECTOR *point, VECTOR *center, s32 angle) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;
    SVECTOR angles;

    angles.vx = 0;
    angles.vy = angle;
    angles.vz = 0;
    PushMatrix();
    func_8003F738(&angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    ApplyMatrixLV(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    PopMatrix();
}

void func_80091008(VECTOR *point, VECTOR *center, s32 angle);

/* Place a point at a heading/elevation/distance from a centre given by
 * selected operands and store its whole x, z, y in three variables. */
void func_800910C0(void) {
    VECTOR center;
    VECTOR point;
    s32 heading;
    s32 elevation;
    s32 angle;
    s32 distance;

    center.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(13)) << 16;
    center.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(13)) << 16;
    center.vy = func_8009D000(5, EVENT_OPERAND_BYTE(13)) << 16;
    heading = func_8009D044(7, EVENT_OPERAND_BYTE(13));
    elevation = func_8009D088(9, EVENT_OPERAND_BYTE(13));
    distance = func_8009D0CC(11, EVENT_OPERAND_BYTE(13));
    angle = ((elevation * 0xB60) >> 8) + 0xC00;
    point.vy = ((-((func_8003F8CC(angle) * distance) << 5)) >> 16) * D_800AF880.scripted_scale * 16 + center.vy;
    point.vz = (((func_8003F8B0(angle) * distance) << 5) >> 16) * D_800AF880.scripted_scale * 16 + center.vz;
    point.vx = center.vx;
    func_80091008(&point, &center, heading);
    func_800A3074(func_800ACDB8(14), WHOLE(point.vx));
    func_800A3074(func_800ACDB8(16), WHOLE(point.vz));
    func_800A3074(func_800ACDB8(18), WHOLE(point.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 20;
}

/* As func_800910C0, around camera point operand 1 (saved target, point A,
 * saved eye, point B). */
void func_80091318(void) {
    VECTOR center;
    VECTOR point;
    s32 heading;
    s32 elevation;
    s32 angle;
    s32 distance;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        center.vx = D_800AF880.saved_target.vx;
        center.vy = D_800AF880.saved_target.vy;
        center.vz = D_800AF880.saved_target.vz;
        break;
    case 1:
        center.vx = D_800AF880.point_actor_a.vx;
        center.vy = D_800AF880.point_actor_a.vy;
        center.vz = D_800AF880.point_actor_a.vz;
        break;
    case 2:
        center.vx = D_800AF880.saved_eye.vx;
        center.vy = D_800AF880.saved_eye.vy;
        center.vz = D_800AF880.saved_eye.vz;
        break;
    case 3:
        center.vx = D_800AF880.point_actor_b.vx;
        center.vy = D_800AF880.point_actor_b.vy;
        center.vz = D_800AF880.point_actor_b.vz;
        break;
    }
    heading = func_8009CF78(2, EVENT_OPERAND_BYTE(8));
    elevation = func_8009CFBC(4, EVENT_OPERAND_BYTE(8));
    distance = func_8009D000(6, EVENT_OPERAND_BYTE(8));
    angle = ((elevation * 0xB60) >> 8) + 0xC00;
    point.vy = ((-((func_8003F8CC(angle) * distance) << 5)) >> 16) * D_800AF880.scripted_scale * 16 + center.vy;
    point.vz = (((func_8003F8B0(angle) * distance) << 5) >> 16) * D_800AF880.scripted_scale * 16 + center.vz;
    point.vx = center.vx;
    func_80091008(&point, &center, heading);
    func_800A3074(func_800ACDB8(9), WHOLE(point.vx));
    func_800A3074(func_800ACDB8(11), WHOLE(point.vz));
    func_800A3074(func_800ACDB8(13), WHOLE(point.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 15;
}

/* Store camera point operand 1's whole x, z, y in three variables. */
void func_800915C4(void) {
    VECTOR point;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        point.vx = D_800AF880.saved_target.vx;
        point.vy = D_800AF880.saved_target.vy;
        point.vz = D_800AF880.saved_target.vz;
        break;
    case 1:
        point.vx = D_800AF880.point_actor_a.vx;
        point.vy = D_800AF880.point_actor_a.vy;
        point.vz = D_800AF880.point_actor_a.vz;
        break;
    case 2:
        point.vx = D_800AF880.saved_eye.vx;
        point.vy = D_800AF880.saved_eye.vy;
        point.vz = D_800AF880.saved_eye.vz;
        break;
    case 3:
        point.vx = D_800AF880.point_actor_b.vx;
        point.vy = D_800AF880.point_actor_b.vy;
        point.vz = D_800AF880.point_actor_b.vz;
        break;
    }
    func_800A3074(func_800ACDB8(2), WHOLE(point.vx));
    func_800A3074(func_800ACDB8(4), WHOLE(point.vz));
    func_800A3074(func_800ACDB8(6), WHOLE(point.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Copy camera point operand 1 into camera point operand 2. */
void func_80091720(void) {
    VECTOR point;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        point.vx = D_800AF880.saved_target.vx;
        point.vy = D_800AF880.saved_target.vy;
        point.vz = D_800AF880.saved_target.vz;
        break;
    case 1:
        point.vx = D_800AF880.point_actor_a.vx;
        point.vy = D_800AF880.point_actor_a.vy;
        point.vz = D_800AF880.point_actor_a.vz;
        break;
    case 2:
        point.vx = D_800AF880.saved_eye.vx;
        point.vy = D_800AF880.saved_eye.vy;
        point.vz = D_800AF880.saved_eye.vz;
        break;
    case 3:
        point.vx = D_800AF880.point_actor_b.vx;
        point.vy = D_800AF880.point_actor_b.vy;
        point.vz = D_800AF880.point_actor_b.vz;
        break;
    }
    switch (EVENT_OPERAND_BYTE(2)) {
    case 0:
        D_800AF880.saved_target.vx = point.vx;
        D_800AF880.saved_target.vy = point.vy;
        D_800AF880.saved_target.vz = point.vz;
        break;
    case 1:
        D_800AF880.point_actor_a.vx = point.vx;
        D_800AF880.point_actor_a.vy = point.vy;
        D_800AF880.point_actor_a.vz = point.vz;
        break;
    case 2:
        D_800AF880.saved_eye.vx = point.vx;
        D_800AF880.saved_eye.vy = point.vy;
        D_800AF880.saved_eye.vz = point.vz;
        break;
    case 3:
        D_800AF880.point_actor_b.vx = point.vx;
        D_800AF880.point_actor_b.vy = point.vy;
        D_800AF880.point_actor_b.vz = point.vz;
        break;
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 3;
}

void func_80073E38(void);

/* Set two colour triples and two ranges from operands, then apply them. */
void func_80091944(void) {
    D_800B2078.fog_color[0] = func_800ACDEC(1);
    D_800B2078.fog_color[1] = func_800ACDEC(3);
    D_800B2078.fog_color[2] = func_800ACDEC(5);
    D_800B2078.far_color[0] = func_800ACDEC(7);
    D_800B2078.far_color[1] = func_800ACDEC(9);
    D_800B2078.far_color[2] = func_800ACDEC(11);
    D_800B2078.fog_range[0] = func_800ACDEC(13);
    D_800B2078.fog_range[1] = func_800ACDEC(15);
    D_800B2078.sprite_gate = 1;
    func_80073E38();
    D_800B0078->pc += 17;
}

/* Set the four camera bounds from signed operands (the last negated). */
void func_80091A08(void) {
    D_800AF880.bounds[0] = func_800ACD7C(1);
    D_800AF880.bounds[1] = func_800ACD7C(3);
    D_800AF880.bounds[2] = func_800ACD7C(5);
    D_800AF880.bounds[3] = -func_800ACD7C(7);
    D_800B0078->pc += 9;
}


/* Set a colour triple from three operands. */
void func_80091A78(void) {
    D_800B2078.clear_color[0] = func_800ACDEC(1);
    D_800B2078.clear_color[1] = func_800ACDEC(3);
    D_800B2078.clear_color[2] = func_800ACDEC(5);
    D_800B0078->pc += 7;
}

void func_80091AD4(void) {
}

extern RECT D_800AF5E8[32];
extern s16 D_800AF6E8[32];
extern s16 D_800AF728[32];
extern u16 D_800AF768;

/* Record a VRAM rectangle in the 32-slot ring at 800af5e8 and move it to
 * (dx, dy), or clear it to black when `clear` is set. */
void func_80091ADC(s32 x, s32 y, s32 w, s32 h, s32 dx, s32 dy, s32 clear) {
    s32 slot;

    slot = D_800AF768 & 0x1F;
    D_800AF5E8[slot].y = y;
    D_800AF5E8[slot].x = x;
    D_800AF5E8[slot].w = w;
    D_800AF5E8[slot].h = h;
    D_800AF6E8[slot] = dx;
    D_800AF728[slot] = dy;
    if (clear == 0) {
        MoveImage(&D_800AF5E8[slot], D_800AF6E8[slot], (s16)dy);
    } else {
        ClearImage(&D_800AF5E8[slot], 0, 0, 0);
    }
    D_800AF768++;
}

/* Event 0xe1: with op1 and op3 both zero, clear the VRAM rectangle
 * (op5, op7, op9, op11) to black; otherwise move the rectangle at (op1, op3)
 * of size op5 x op7 to (op9, op11). Selected operands, flags byte 13. */
void func_80091BBC(void) {
    s32 x;
    s32 y;
    RECT unused; /* the original frame holds an unused 8-byte local */

    x = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    y = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    if (x == 0 && y == 0) {
        func_80091ADC(func_8009D000(5, EVENT_OPERAND_BYTE(0xD)), func_8009D044(7, EVENT_OPERAND_BYTE(0xD)),
                      func_8009D088(9, EVENT_OPERAND_BYTE(0xD)), func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD)), 0, 0, 1);
    } else {
        func_80091ADC(x, y, func_8009D000(5, EVENT_OPERAND_BYTE(0xD)), func_8009D044(7, EVENT_OPERAND_BYTE(0xD)),
                      func_8009D088(9, EVENT_OPERAND_BYTE(0xD)), func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD)), 0);
    }
    D_800B0078->pc += 0xE;
}

/* Set the current actor's two-bit mode (flags bits 5-6) and value EE from
 * selected operands. */
void func_80091E00(void) {
    D_800B0078->unk134 = (D_800B0078->unk134 & ~0x60) | ((func_8009CF78(1, EVENT_OPERAND_BYTE(5)) & 3) << 5);
    D_800B0078->unkEE = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    D_800B0078->pc += 6;
}

/* As func_80091E00, for a selected actor. */
void func_80091E98(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->unk134 = (actor->unk134 & ~0x60) | ((func_8009CF78(2, EVENT_OPERAND_BYTE(6)) & 3) << 5);
        actor->unkEE = func_8009CFBC(4, EVENT_OPERAND_BYTE(6));
    }
    D_800B0078->pc += 7;
}

/* Store an operand (clamped to 0xFFF) in slot operand 1 of the current
 * actor's word table when its descriptor has flag 0x2000. */
void func_80091F84(void) {
    s32 index;
    s32 value;

    index = func_800ACDEC(1);
    value = func_800ACDEC(3);
    if (value >= 0x1000) {
        value = 0xFFF;
    }
    if (D_800AF880.components.descriptors[D_800AFD1C].flags & 0x2000) {
        D_800B0078->list[index] = value;
    }
    D_800B0078->pc += 5;
}

/* Swap two variables. */
void func_80092044(void) {
    s32 first;
    s32 second;

    first = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    second = func_800A3018(func_800ACDB8(3) & 0xFFFF);
    func_800A3074(func_800ACDB8(3), first);
    func_800A3074(func_800ACDB8(1), second);
    D_800B0078->pc += 5;
}

/* Up to 32 entries created by func_800921E8. */
typedef struct {
    s16 count;
    s32 handles[32];
    u8 *buffers[32];
    s16 lengths[32];
} WindowList;
extern WindowList D_800AFEA8;
void func_80027EAC(s32 handle);

/* Pass every handle in the list at 800afea8 to resident 80027EAC. */
void func_800920D8(void) {
    s32 i;

    for (i = 0; i < D_800AFEA8.count; i++) {
        func_80027EAC(D_800AFEA8.handles[i]);
    }
}

typedef struct {
    u8 *data[32];
    s16 size[32];
} ByteTables;
extern ByteTables D_800AFF2C;

/* Store a raw operand byte at index operand 3 of table operand 1, within its
 * size. */
void func_80092148(void) {
    s32 table;
    s32 index;
    s32 value;

    table = func_800ACDEC(1);
    index = func_800ACDEC(3);
    value = func_800ACDB8(5) & 0xFFFF;
    if (index < D_800AFF2C.size[table]) {
        D_800AFF2C.data[table][index] = value;
    }
    D_800B0078->pc += 7;
}

extern s32 D_800ADB8C;
void *func_80031BDC(s32 size, s32 flags);
void func_80027D64(s32 handle, s16 x, s16 y, s16 width, s16 height, s16 length, s16 a, s16 b, u8 *buffer);

/* Open a new entry in the list at 800afea8: allocate its handle and a
 * filled buffer, then create it through resident 80027D64. */
void func_800921E8(void) {
    s32 length;
    s32 fill;
    s32 i;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 a;

    if (D_800ADB8C == 0 && D_800AFEA8.count < 32) {
        length = func_800ACDB8(9) & 0xFFFF;
        D_800AFEA8.lengths[D_800AFEA8.count] = length;
        D_800AFEA8.buffers[D_800AFEA8.count] = func_80031BDC(length + 1, 0);
        D_800AFEA8.handles[D_800AFEA8.count] = (s32)func_80031BDC(0x18, 0);
        fill = func_800ACDB8(15) & 0xFFFF;
        for (i = 0; i < length; i++) {
            D_800AFEA8.buffers[D_800AFEA8.count][i] = fill;
        }
        x = (s16)func_800ACDB8(1);
        y = (s16)func_800ACDB8(3);
        width = (s16)func_800ACDB8(5);
        height = (s16)func_800ACDB8(7);
        a = (s16)func_800ACDB8(11);
        func_80027D64(D_800AFEA8.handles[D_800AFEA8.count], x, y, width, height, length, a,
                      func_800ACDB8(13), D_800AFEA8.buffers[D_800AFEA8.count]);
        D_800AFEA8.count++;
    }
    D_800B0078->pc += 17;
}

/* No operation. */
void func_800923E4(void) {
    D_800B0078->pc += 1;
}

/* No operation. */
void func_80092404(void) {
    D_800B0078->pc += 1;
}


/* Return byte `which` of collision attribute `index`. */
s32 func_80092424(s32 index, s32 which) {
    switch (which) {
    case 0:
        return D_800AF880.components.collision_attributes[index].bytes[0];
    case 1:
        return D_800AF880.components.collision_attributes[index].bytes[1];
    case 2:
        return D_800AF880.components.collision_attributes[index].bytes[2];
    case 3:
        return D_800AF880.components.collision_attributes[index].bytes[3];
    }
    return 0;
}

/* Replace byte `which` of collision attribute `index`. */
void func_800924D4(s32 index, s32 which, s32 value) {
    switch (which) {
    case 0:
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & ~0xFF) | value;
        break;
    case 1:
        value <<= 8;
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & 0xFFFF00FF) | value;
        break;
    case 2:
        value <<= 16;
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & 0xFF00FFFF) | value;
        break;
    case 3:
        value <<= 24;
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & 0x00FFFFFF) | value;
        break;
    }
}


/* Select mode 0..2 (unk17C) and set the text speed to 8, 6 or 4. */
void func_800925A0(void) {
    s32 mode;

    mode = func_800ACDEC(1);
    D_800B2078.unk217C = mode;
    switch (mode) {
    case 0:
        D_800B2078.text_speed = 8;
        break;
    case 1:
        D_800B2078.text_speed = 6;
        break;
    case 2:
        D_800B2078.text_speed = 4;
        break;
    }
    D_800B0078->pc += 3;
}


/* Set the input mask from a raw operand. */
void func_80092628(void) {
    D_800B2078.input_mask = func_800ACDB8(1);
    D_800B0078->pc += 3;
}

/* Set a collision attribute byte (operands: index, byte, value). */
void func_80092664(void) {
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), func_800ACDEC(3));
    D_800B0078->pc += 5;
}

/* OR a value into a collision attribute byte. */
void func_800926C8(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value |= func_800ACDEC(3);
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    D_800B0078->pc += 5;
}

/* AND a value into a collision attribute byte. */
void func_80092768(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value &= func_800ACDEC(3);
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    D_800B0078->pc += 5;
}

extern FieldDescriptor *D_800B06B8;

/* Give the current actor a step along the published descriptor's facing
 * and set its layer flag 0x800. */
void func_80092808(void) {
    FieldActor *actor;
    s32 step;

    D_800B0078->unk60 = (func_8003F8CC(D_800B06B8->rotation.vy) * 36) >> 12;
    step = -(func_8003F8B0(D_800B06B8->rotation.vy) * 36) >> 12;
    actor = D_800B0078;
    actor->unk64 = step;
    actor->layer_flags |= 0x800;
    actor->pc++;
}

#ifdef NON_MATCHING
/* Walk the controlled actor one step toward (x, z); with `mode` 0 the goal
 * is 40 units along direction `angle` from the running actor (publishing
 * the field id first when 800adbec asks). Returns -1 while walking (mode 1
 * retries the instruction) and 0 once it has arrived or is stuck, when it
 * stops, turns and the instruction continues. */
s32 func_80092894(s32 angle, s32 mode, s32 x, s32 z) {
    FieldDescriptor *descriptor;
    FieldActor *player;
    FieldModel *model;
    s16 from_x;
    s16 from_z;
    s32 reach;
    s32 dx;
    s32 dz;
    s32 value;
    s32 field;
    VECTOR delta;
    u16 heading;

    D_800B2078.encounter_inhibition = -1;
    descriptor = &D_800AF880.components.descriptors[D_800B2078.controlled];
    player = descriptor->actor;
    model = descriptor->model;
    player->layer_flags |= 0x38;
    model->unk18 = 0x80000;
    from_x = WHOLE(player->position[0]);
    from_z = WHOLE(player->position[2]);
    reach = func_80099A8C(8) * 2;
    if (mode == 0) {
        if (D_800ADBDC == 0 || D_800ADBE4 == 0) {
            D_800B00C0 = 1;
        }
        if (D_800ADBEC != 0) {
            value = func_800ACDEC(4);
            field = func_800ACDEC(2);
            func_80092F44();
            D_800ADBEC = 0;
            func_800A3074(2, value);
            D_8004F34C = field;
        }
        angle = D_800B06B8->rotation.vy + angle - 0x400;
        x = D_800B0078->unk60 + WHOLE(D_800B0078->position[0]) + ((func_8003F8CC(angle) * 40) >> 12);
        z = D_800B0078->unk64 + WHOLE(D_800B0078->position[2]) + (-(func_8003F8B0(angle) * 40) >> 12);
    }
    dx = x - from_x;
    dz = z - from_z;
    delta.vx = dx;
    delta.vy = 0;
    delta.vz = dz;
    if (reach < func_80099A4C(dx, dz)) {
        if (player->last_position[0] == WHOLE(player->position[0]) &&
            player->last_position[1] == WHOLE(player->position[1]) &&
            player->last_position[2] == WHOLE(player->position[2])) {
            player->stuck++;
        } else {
            player->stuck = 0;
        }
        if ((s16)player->stuck <= 0x40) {
            player->heading_goal = player->heading = func_8007B694(&delta);
            D_800B00C0 = 1;
            if (mode != 0) {
                D_800B0078->pc -= 1;
            }
            return -1;
        }
    }
    player->heading_goal = player->heading = player->heading_goal | 0x8000;
    model->unk18 = 0;
    player->unkE8 = 0;
    func_800821F4(model, 0, &D_800AF880.components.descriptors[D_800B2078.controlled]);
    D_800B00C0 = 1;
    player->slots[player->slot].value = 0xFFFF;
    player->slots[player->slot].move_mode = 0;
    player->flags &= ~0x200000;
    if (mode == 1) {
        player->layer_flags &= ~0x38;
    }
    player->stuck = 0;
    D_800B0078->pc += 6;
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092894);
#endif

/* Walk the player to the selected x/z (800a0c4c/80092894) when the field
 * is idle, restoring its flag 0x80 on arrival; otherwise retry. */
void func_80092C20(void) {
    s32 x;
    s32 z;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
        D_800B0078->pc--;
        return;
    }
    x = func_8009CF78(1, EVENT_OPERAND_BYTE(5));
    z = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    if (D_800B2078.unk2350 == 0) {
        D_800B2078.unk2350 = D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags;
    }
    func_800A0C4C();
    if (func_80092894(0, 1, x, z) == 0) {
        if (!(D_800B2078.unk2350 & 0x80)) {
            D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags &= ~0x80;
        }
        D_800B2078.unk2350 = 0;
    }
}

extern s32 D_800ADBE4;
extern s32 D_800ADB2C;
extern s32 D_800ADB90;
s32 func_80092894(s32 a, s32 b, s32 c, s32 d);

/* Start transition 80092894(0, ...) once field control allows it; yield
 * until then. */
void func_80092DFC(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800A0C4C();
        func_80092894(0, 0, 0, 0);
    }
}

/* As func_80092DFC with first argument 0x3E0. */
void func_80092EA0(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800A0C4C();
        func_80092894(0x3E0, 0, 0, 0);
    }
}

extern s32 D_8004F34C;
s32 func_8009744C(void);
s32 func_8009A514(void);

/* Publish the current field id and two values in variables 4, 6 and 8 and
 * count variable 0x12 up. */
void func_80092F44(void) {
    func_800A3074(4, D_8004F34C & 0x3FFF);
    func_800A3074(6, func_8009744C() & 0xFFFF);
    func_800A3074(8, func_8009A514() & 0xFFFF);
    func_800A3074(0x12, (s16)(func_800A3018(0x12) + 1));
}

extern s32 D_800ADBD8;
extern s32 D_800B0064;

/* When D_800ADBD8 is set, consume it and store an operand in D_800B0064. */
void func_80092FB4(void) {
    if (D_800ADBD8 != 0) {
        D_800B2078.encounter_inhibition = -1;
        D_800ADBD8 = 0;
        D_800B0064 = func_800ACDEC(1);
    }
    D_800B0078->pc += 3;
}

extern u8 D_800B02C8;

/* Request a map change (map, entry, heading or keep the camera's, and
 * operand 7) when the field is idle. Yields. */
void func_80093014(void) {
    s32 heading;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
        return;
    }
    func_800A31E8();
    D_800B2078.encounter_inhibition = -1;
    D_800ADBE4 = 0;
    D_8005A39C->unk231A = func_8009CF78(1, EVENT_OPERAND_BYTE(9));
    D_8005A39C->unk231E = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    heading = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    if (((heading & 0xFFFF) == 0xFFFF) | (heading == -1)) {
        D_8005A39C->unk231C = (D_800AF880.heading_angles.vy + 0x800) & 0xFFF;
    } else {
        D_8005A39C->unk231C = (heading + 0x800) & 0xFFF;
    }
    D_8005A39C->unk2320 = func_8009D044(7, EVENT_OPERAND_BYTE(9));
    func_800931F8();
    D_800B02C8 = 1;
    D_800B00C0 = 1;
    D_800B0078->pc += 10;
}

void func_800931F8(void) {
}

extern s32 D_800B0048;
extern s32 D_800AFD14;
void func_800932D0(void);

/* Once field control allows it, run func_800932D0 and store two operands. */
void func_80093200(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800932D0();
        func_800931F8();
        D_800B0048 = func_800ACDEC(0);
        D_800AFD14 = func_800ACDEC(2);
        D_800B0078->pc += 4;
    }
}

extern s32 D_800ADB70;
extern s32 D_800ADBEC;
void func_80092F44(void);

/* Request a field change (operands: field id, entry) once field control
 * allows it, then yield. */
void func_800932D0(void) {
    s32 entry;
    s32 field;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0 ||
        D_800ADB70 != 0) {
        D_800B00C0 = 1;
    } else {
        D_800B2078.encounter_inhibition = -1;
        if (D_800ADBEC != 0) {
            entry = func_800ACDEC(3);
            field = func_800ACDEC(1);
            func_80092F44();
            D_800ADBEC = 0;
            func_800A3074(2, entry);
            D_8004F34C = field;
            func_800931F8();
        }
        D_800B00C0 = 1;
        D_800B0078->pc += 5;
    }
}

extern u8 D_8005954C;
extern u8 D_80059508;
extern u8 D_800594F8;
extern s32 D_800ADBE0;
extern s32 D_800ADB88;
extern s32 D_800ADB18;

/* Leave the field for another module (operand 1), optionally requesting a
 * field change (operands 5, 7; 0x7FFF: none). Re-runs until allowed. */
void func_800933F8(void) {
    s32 field;
    s32 entry;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0 || D_800ADB2C != 0 || D_8004F308 == -1 ||
        D_800ADB90 != 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    D_8005954C = D_800B2078.unk2356;
    D_80059508 = func_800ACDEC(1);
    D_800594F8 = 0;
    D_800ADBDC = 0;
    D_800ADBE0 = 0;
    D_800ADB88 = 1;
    field = func_800ACDEC(5);
    if (field != 0x7FFF) {
        entry = func_800ACDEC(7);
        func_80092F44();
        func_800A3074(2, entry);
        D_8004F34C = field;
        D_800ADB18 = 1;
    }
    D_800B00C0 = 1;
    D_800B0078->pc += 9;
}

/* Leave the field for another module (operand 1) once allowed. */
void func_80093568(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0 || D_800ADB2C != 0 || D_8004F308 == -1 ||
        D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        D_8005954C = D_800B2078.unk2356;
        D_80059508 = func_800ACDEC(1);
        D_800594F8 = 0;
        D_800ADBDC = 0;
        D_800ADBE0 = 0;
        D_800ADB88 = 1;
        D_800B00C0 = 1;
        D_800B0078->pc += 3;
    }
}

/* Store a collision attribute byte in a variable. */
void func_80093664(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    func_800A3074(func_800ACDB8(3), value);
    D_800B0078->pc += 5;
}

extern s32 D_8004F350;

/* Yield; advance only once D_8004F350 is zero. */
void func_800936E4(void) {
    if (D_8004F350 == 0) {
        D_800B0078->pc += 1;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

extern u8 D_80059171;
extern s32 D_800ADB64;

/* Request field action 0 with parameter D_800B2078.unk236C. */
void func_80093740(void) {
    D_800B00C0 = 1;
    D_800ADB64 = 0;
    D_80059171 = D_800B2078.unk236C;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request field action 6 with parameter 1. */
void func_80093790(void) {
    D_80059171 = 1;
    D_800ADB64 = 6;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request field action 2. */
void func_800937E0(void) {
    D_800ADB64 = 2;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request field action 3 with an operand parameter. */
void func_80093824(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 3;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request a field change (operands: field id, entry) as field action 1. */
void func_80093888(void) {
    s32 entry;
    s32 field;

    D_800B2078.encounter_inhibition = -1;
    entry = func_800ACDEC(3);
    field = func_800ACDEC(1);
    func_80092F44();
    func_800A3074(2, entry);
    D_8004F34C = field;
    func_800931F8();
    D_800ADB64 = 1;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 5;
}

/* Field action 1 with an entry operand also stored in game state and
 * variable 2. */
void func_80093930(void) {
    s16 entry;

    entry = func_800ACDEC(1);
    D_800ADB64 = 1;
    D_800B00C0 = 1;
    D_8005A39C->vars[1] = entry;
    D_8005A39C->unk2320 = entry;
    D_800C3A68[1] = entry;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request field action 4 with an operand parameter. */
void func_800939A0(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 4;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request field action 5 with an operand parameter. */
void func_80093A04(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 5;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Clear the camera hold flag (0x8000). */
void func_80093A68(void) {
    D_800AF880.flags &= 0x7FFF;
    D_800B0078->pc += 1;
}

/* Set the camera hold flag (0x8000). */
void func_80093A98(void) {
    D_800AF880.flags |= 0x8000;
    D_800B0078->pc += 1;
}


/* Release script control: clear the encounter inhibition, both control
 * bytes and the camera hold flags. */
void func_80093AC8(void) {
    D_800B2078.encounter_inhibition = 0;
    D_800B2078.script_control[0] = 0;
    D_800B2078.script_control[1] = 0;
    D_800AF880.flags &= 0x3FFF;
    D_800B0078->pc += 1;
}

/* Take script control (both control bytes, camera hold flags); re-runs
 * while the field is not ready. */
void func_80093B10(void) {
    D_800B2078.encounter_inhibition = -1;
    D_800B2078.script_control[0] = 1;
    D_800B2078.script_control[1] = 1;
    D_800AF880.flags |= 0xC000;
    if (D_800ADBDC == 0 || D_800ADBE4 == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    D_800B0078->pc += 1;
}

/* Clear script control byte 0. */
void func_80093BB0(void) {
    D_800B2078.script_control[0] = 0;
    D_800B0078->pc += 1;
}

/* Set script control byte 0. */
void func_80093BD4(void) {
    D_800B2078.script_control[0] = 1;
    D_800B0078->pc += 1;
}

/* Clear script control byte 1. */
void func_80093BFC(void) {
    D_800B2078.script_control[1] = 0;
    D_800B0078->pc += 1;
}

/* Set script control byte 1. */
void func_80093C20(void) {
    D_800B2078.script_control[1] = 1;
    D_800B0078->pc += 1;
}

/* Clear the encounter inhibition. */
void func_80093C48(void) {
    D_800B2078.encounter_inhibition = 0;
    D_800B0078->pc += 1;
}

/* Inhibit encounters once the field is ready; yield until then. */
void func_80093C6C(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0) {
        D_800B00C0 = 1;
    } else {
        D_800B2078.encounter_inhibition = -1;
        D_800B0078->pc += 1;
    }
}

/* Load a bytecode table byte (table offset + index) into a variable. */
void func_80093CD0(void) {
    u16 offset;

    offset = func_800ACDB8(1);
    offset += func_800ACDEC(5);
    func_800A3074(func_800ACDB8(3), D_800ADC00[offset]);
    D_800B0078->pc += 7;
}

/* Load a bytecode table halfword (unsigned when operand 7 is zero, else
 * signed) into a variable. */
void func_80093D48(void) {
    u16 offset;

    offset = func_800ACDB8(1);
    offset += func_800ACDEC(5);
    if (EVENT_OPERAND_BYTE(7) == 0) {
        func_800A3074(func_800ACDB8(3), D_800ADC00[offset] | (D_800ADC00[offset + 1] << 8));
    } else {
        func_800A3074(func_800ACDB8(3), (s16)(D_800ADC00[offset] + (D_800ADC00[offset + 1] << 8)));
    }
    D_800B0078->pc += 8;
}

/* Door-style swing: while flag 0x100000 is clear, turn the current
 * descriptor by 0x20 per frame (direction operand 1) for 31 frames, then set
 * the flag and advance. */
void func_80093E30(void) {
    FieldActor *actor;

    if (!(D_800B0078->flags & 0x100000)) {
        if (!(D_800B0078->state.word & 0x20)) {
            D_800B0078->state.word |= 0x20;
            D_800B0078->unkE2 = 0;
            func_80085634(8, 3);
        } else {
            D_800B0078->unkE2++;
            actor = D_800B0078;
            if (actor->unkE2 < 31) {
                if (D_800ADC00[actor->pc + 1] == 0) {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += 0x20;
                } else {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= 0x20;
                }
            } else {
                actor->unkE2 = 0;
                actor->flags |= 0x100000;
                actor->state.word &= ~0x20;
                D_800B0078->pc += 2;
            }
        }
    } else {
        D_800B0078->pc += 2;
    }
    func_80072254(D_800AFD1C);
}

/* The reverse swing: while flag 0x100000 is set, turn back over 31 frames,
 * then clear it and advance. */
void func_80093FC0(void) {
    FieldActor *actor;

    if (D_800B0078->flags & 0x100000) {
        if (!(D_800B0078->state.word & 0x20)) {
            D_800B0078->state.word |= 0x20;
            D_800B0078->unkE2 = 0;
            func_80085634(8, 3);
        } else {
            D_800B0078->unkE2++;
            actor = D_800B0078;
            if (actor->unkE2 < 31) {
                if (D_800ADC00[actor->pc + 1] == 0) {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= 0x20;
                } else {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += 0x20;
                }
            } else {
                actor->unkE2 = 0;
                actor->flags &= ~0x100000;
                actor->state.word &= ~0x20;
                D_800B0078->pc += 2;
            }
        }
    } else {
        D_800B0078->pc += 2;
    }
    func_80072254(D_800AFD1C);
}

/* Event 0xe8: shake the actor for op3 frames (0x100000 set once done): op5
 * 0x1000/0x1001 moves the target down/up by op1 * 16, other values move it
 * op1 along direction op5 relative to the actor's heading. */
void func_80094158(void) {
    s32 angle;
    s32 sine;
    s32 cosine;
    FieldActor *actor;

    if (!(D_800B0078->flags & 0x100000)) {
        if (!(D_800B0078->state.word & 0x20)) {
            D_800B0078->state.word |= 0x20;
            D_800B0078->unkE2 = 0;
            func_80085634(8, 3);
            D_800B0078->target[0] = D_800B0078->position[0];
            D_800B0078->target[1] = D_800B0078->position[1];
            D_800B0078->target[2] = D_800B0078->position[2];
        } else {
            D_800B0078->unkE2++;
            if (D_800B0078->unkE2 < func_800ACDEC(3)) {
                switch (func_800ACDEC(5)) {
                case 0x1000:
                    D_800B0078->target[1] -= func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                case 0x1001:
                    D_800B0078->target[1] += func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                default:
                    angle = D_800B06B8->rotation.vy + func_800ACDEC(5) - 0x400;
                    sine = func_8003F8CC(angle);
                    D_800B0078->target[0] += sine * func_800ACDEC(1);
                    cosine = func_8003F8B0(angle);
                    D_800B0078->target[2] -= cosine * func_800ACDEC(1);
                    D_800B06B8->matrix.t[0] = WHOLE(D_800B0078->target[0]);
                    D_800B06B8->matrix.t[2] = WHOLE(D_800B0078->target[2]);
                    break;
                }
            } else {
                actor = D_800B0078;
                actor->unkE2 = 0;
                actor->flags |= 0x100000;
                actor->state.word &= ~0x20;
                D_800B0078->pc += 7;
            }
        }
    } else {
        D_800B0078->pc += 7;
    }
    func_80072254(D_800AFD1C);
}

#ifdef NON_MATCHING
/* Event 0xe9: the reverse shake of 0xe8, run while flag 0x100000 is set
 * (cleared once op3 frames have passed); the direction offsets are negated
 * and the target is not reset first. */
void func_800943AC(void) {
    FieldActor *actor;
    FieldActor *done;
    s32 angle;
    s32 sine;
    s32 cosine;

    actor = D_800B0078;
    if (actor->flags & 0x100000) {
        if (!(actor->state.word & 0x20)) {
            actor->state.word |= 0x20;
            actor->unkE2 = 0;
            func_80085634(8, 3);
        } else {
            actor->unkE2++;
            if (D_800B0078->unkE2 < func_800ACDEC(3)) {
                switch (func_800ACDEC(5)) {
                case 0x1000:
                    D_800B0078->target[1] -= func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                case 0x1001:
                    D_800B0078->target[1] += func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                default:
                    angle = D_800B06B8->rotation.vy + func_800ACDEC(5) - 0x400;
                    sine = func_8003F8CC(angle);
                    D_800B0078->target[0] -= sine * func_800ACDEC(1);
                    cosine = func_8003F8B0(angle);
                    D_800B0078->target[2] += cosine * func_800ACDEC(1);
                    D_800B06B8->matrix.t[0] = WHOLE(D_800B0078->target[0]);
                    D_800B06B8->matrix.t[2] = WHOLE(D_800B0078->target[2]);
                    break;
                }
            } else {
                done = D_800B0078;
                done->unkE2 = 0;
                done->flags &= ~0x100000;
                done->state.word &= ~0x20;
                actor = D_800B0078;
                actor->pc += 7;
            }
        }
    } else {
        actor->pc += 7;
    }
    func_80072254(D_800AFD1C);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800943AC);
#endif

extern s32 D_8004F318;
extern s32 D_8004F328;

/* Reset D_8004F318/D_8004F328 and store (op1 << 8 | op3) in variable 10. */
void func_800945D4(void) {
    s32 high;

    D_8004F318 = 0;
    D_8004F328 = 0xFF;
    high = func_800ACDEC(1);
    func_800A3074(10, ((high << 8) & 0xFF00) | (func_800ACDEC(3) & 0xFF));
    D_800B0078->pc += 5;
}

/* Set D_8004F328 from an operand byte. */
void func_80094650(void) {
    D_8004F328 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Reset D_8004F318 and D_8004F328. */
void func_8009468C(void) {
    D_8004F318 = 0;
    D_8004F328 = 0xFF;
    D_800B0078->pc += 1;
}

/* Set the current actor's two-bit mode (state) to 1 with value unk70. */
void func_800946BC(void) {
    D_800B0078->state.word = (D_800B0078->state.word & ~3) | 1;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set the current actor's two-bit mode (state) to 2 with value unk70. */
void func_80094710(void) {
    D_800B0078->state.word = (D_800B0078->state.word & ~3) | 2;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set the current actor's two-bit mode (state) to 3 with value unk70. */
void func_80094764(void) {
    D_800B0078->state.word |= 3;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Turn a selected actor's descriptor: operand 1 picks axis and sign
 * (0/1: x+/-, 2/3: y+/-, 4/5: z+/-). */
void func_800947B0(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(2) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(2)];
        switch (EVENT_OPERAND_BYTE(1)) {
        case 0:
            descriptor->rotation.vx += func_800ACDEC(3);
            break;
        case 1:
            descriptor->rotation.vx -= func_800ACDEC(3);
            break;
        case 2:
            descriptor->rotation.vy += func_800ACDEC(3);
            break;
        case 3:
            descriptor->rotation.vy -= func_800ACDEC(3);
            break;
        case 4:
            descriptor->rotation.vz += func_800ACDEC(3);
            break;
        case 5:
            descriptor->rotation.vz -= func_800ACDEC(3);
            break;
        }
        func_80072254(func_8009CDB4(2));
    }
    D_800B0078->pc += 5;
}

/* Set one rotation axis (operand 3) of the current descriptor. */
void func_80094918(void) {
    switch (EVENT_OPERAND_BYTE(3)) {
    case 0:
        D_800AF880.components.descriptors[D_800AFD1C].rotation.vx = func_800ACDEC(1);
        break;
    case 1:
        D_800AF880.components.descriptors[D_800AFD1C].rotation.vy = func_800ACDEC(1);
        break;
    case 2:
        D_800AF880.components.descriptors[D_800AFD1C].rotation.vz = func_800ACDEC(1);
        break;
    }
    D_800B0078->pc += 4;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about x by an operand and reapply it. */
void func_80094A5C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vx += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about x by minus an operand. */
void func_80094ACC(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vx -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about y by an operand. */
void func_80094B3C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about y by minus an operand. */
void func_80094BAC(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about z by an operand. */
void func_80094C1C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vz += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about z by minus an operand. */
void func_80094C8C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vz -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* First free slot of inventory list 0, or -1. */
s32 func_80094CFC(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->count0[i] == 0 || D_8005A39C->id0[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 1, or -1. */
s32 func_80094D4C(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->count1[i] == 0 || D_8005A39C->id1[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 2, or -1. */
s32 func_80094D9C(void) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (D_8005A39C->count2[i] == 0 || D_8005A39C->id2[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 3, or -1. */
s32 func_80094DEC(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->count3[i] == 0 || D_8005A39C->id3[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 4, or -1. */
s32 func_80094E3C(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->count4[i] == 0 || D_8005A39C->id4[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 0, or -1. */
s32 func_80094E8C(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->id0[i] == id && D_8005A39C->count0[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 2, or -1. */
s32 func_80094EDC(s32 id) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (D_8005A39C->id2[i] == id && D_8005A39C->count2[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 1, or -1. */
s32 func_80094F2C(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->id1[i] == id && D_8005A39C->count1[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 3, or -1. */
s32 func_80094F7C(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->id3[i] == id && D_8005A39C->count3[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 4, or -1. */
s32 func_80094FCC(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->id4[i] == id && D_8005A39C->count4[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Id array of the inventory list selected by item >> 8. */
u8 *func_8009501C(s32 item) {
    switch (item >> 8) {
    case 0:
        return D_8005A39C->id0;
    case 1:
        return D_8005A39C->id1;
    case 2:
        return D_8005A39C->id2;
    case 3:
        return D_8005A39C->id3;
    case 4:
        return D_8005A39C->id4;
    }
    return NULL;
}

/* Count array of the inventory list selected by item >> 8. */
u8 *func_800950A0(s32 item) {
    switch (item >> 8) {
    case 0:
        return D_8005A39C->count0;
    case 1:
        return D_8005A39C->count1;
    case 2:
        return D_8005A39C->count2;
    case 3:
        return D_8005A39C->count3;
    case 4:
        return D_8005A39C->count4;
    }
    return NULL;
}

/* Slot holding `item` (list in the high byte), or -1; 0 for no list. */
s32 func_80095124(s32 item) {
    switch (item >> 8) {
    case 0:
        return func_80094E8C(item);
    case 1:
        return func_80094F2C(item - 0x100);
    case 2:
        return func_80094EDC(item - 0x200);
    case 3:
        return func_80094F7C(item - 0x300);
    case 4:
        return func_80094FCC(item - 0x400);
    }
    return 0;
}

/* First free slot of the list selected by item >> 8, or -1; 0 for no list. */
s32 func_800951B8(s32 item) {
    switch (item >> 8) {
    case 0:
        return func_80094CFC();
    case 1:
        return func_80094D4C();
    case 2:
        return func_80094D9C();
    case 3:
        return func_80094DEC();
    case 4:
        return func_80094E3C();
    }
    return 0;
}

void func_80095284(void);

/* Stop the current actor (func_80095284) and advance. */
void func_8009524C(void) {
    func_80095284();
    D_800B0078->pc += 1;
}

/* Stop the current actor: clear its motion words and its model's, mark
 * 0x8000 in unk104/unk106, and yield. */
void func_80095284(void) {
    FieldModel *model;
    u16 state;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800B00C0 = 1;
    D_800B0078->unk030[0] = 0;
    D_800B0078->unk030[1] = 0;
    D_800B0078->unk030[2] = 0;
    D_800B0078->unk40[0] = 0;
    D_800B0078->unk40[1] = 0;
    D_800B0078->unk40[2] = 0;
    state = D_800B0078->heading | 0x8000;
    D_800B0078->heading_goal = state;
    D_800B0078->heading = state;
    model->unk0C = 0;
    model->unk14 = 0;
    model->unk18 = 0;
}

/* Set the terrain angle from an operand. */
void func_80095300(void) {
    D_800B2078.terrain_angle = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

#ifdef NON_MATCHING
/* Call (operand 2) when the controlled actor stands inside trigger zone
 * operand 1 and the call stack has room; otherwise skip. */
void func_8009533C(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    zone = &D_800ADBF4[EVENT_OPERAND_BYTE(1)];
    a = (zone->corner[0].z << 16) + zone->corner[0].x;
    b = (zone->corner[1].z << 16) + zone->corner[1].x;
    c = (zone->corner[2].z << 16) + zone->corner[2].x;
    d = (zone->corner[3].z << 16) + zone->corner[3].x;
    if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
        func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0 &&
        (D_800B0078->state.word & 0x1C0) != 0x100) {
        D_800B0078->call_stack[(D_800B0078->state.word >> 6) & 7] = D_800B0078->pc + 4;
        D_800B0078->pc = func_800ACDB8(2);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | (((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6);
        return;
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009533C);
#endif

#ifdef NON_MATCHING
/* As func_8009533C, also requiring the zone's height within the
 * controlled actor's vertical extent. */
void func_80095520(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    zone = &D_800ADBF4[EVENT_OPERAND_BYTE(1)];
    if (zone->corner[0].y < WHOLE(player->position[1]) &&
        WHOLE(player->position[1]) - player->height < zone->corner[0].y) {
        a = (zone->corner[0].z << 16) + zone->corner[0].x;
        b = (zone->corner[1].z << 16) + zone->corner[1].x;
        point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
        c = (zone->corner[2].z << 16) + zone->corner[2].x;
        d = (zone->corner[3].z << 16) + zone->corner[3].x;
        if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
            func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0 &&
            (D_800B0078->state.word & 0x1C0) != 0x100) {
            D_800B0078->call_stack[(D_800B0078->state.word >> 6) & 7] = D_800B0078->pc + 4;
            D_800B0078->pc = func_800ACDB8(2);
            D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | (((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6);
            return;
        }
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095520);
#endif

#ifdef NON_MATCHING
/* Continue when the controlled actor is inside trigger zone operand 1,
 * else jump to operand 2. */
void func_80095734(void) {
    u8 *operand;
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    operand = &D_800ADC00[D_800B0078->pc];
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    zone = &D_800ADBF4[operand[1]];
    a = (zone->corner[0].z << 16) + zone->corner[0].x;
    b = (zone->corner[1].z << 16) + zone->corner[1].x;
    c = (zone->corner[2].z << 16) + zone->corner[2].x;
    d = (zone->corner[3].z << 16) + zone->corner[3].x;
    if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
        func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0) {
        D_800B0078->pc += 4;
        return;
    }
    D_800B0078->pc = func_800ACDB8(2);
    D_800AFC7C += 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095734);
#endif

#ifdef NON_MATCHING
/* Continue when the controlled actor is inside trigger zone operand 1 and
 * the zone's height lies within the actor's body, else jump to operand 2. */
void func_800958C0(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    zone = &D_800ADBF4[EVENT_OPERAND_BYTE(1)];
    if (zone->corner[0].y < WHOLE(player->position[1]) &&
        WHOLE(player->position[1]) - player->height < zone->corner[0].y) {
        a = (zone->corner[0].z << 16) + zone->corner[0].x;
        b = (zone->corner[1].z << 16) + zone->corner[1].x;
        point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
        c = (zone->corner[2].z << 16) + zone->corner[2].x;
        d = (zone->corner[3].z << 16) + zone->corner[3].x;
        if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
            func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0) {
            D_800B0078->pc += 4;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(2);
    D_800AFC7C += 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800958C0);
#endif

/* Project a selected actor's origin to the screen. */
void func_80095A7C(s32 *x, s32 *y) {
    SVECTOR origin;
    MATRIX m;
    union {
        s32 word;
        DVECTOR xy;
    } screen;
    s32 depth;
    s32 flag;

    CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[func_8009CD7C(1)].transform, &m);
    origin.vx = 0;
    origin.vy = 0;
    origin.vz = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTransPers(&origin, &screen.word, &depth, &flag);
    *y = screen.xy.vy;
    *x = screen.xy.vx;
}

extern s32 D_800ADC18;

/* Yield; continue while a selected actor is well inside the screen,
 * otherwise jump to operand 2. */
void func_80095B3C(void) {
    s32 x;
    s32 y;

    func_80095A7C(&x, &y);
    if (D_800ADC18 != 0) {
        D_800B0078->pc += 4;
        return;
    }
    if (y > 32 && y < 192 && x > 32 && x < 288) {
        D_800B0078->pc += 4;
    } else {
        D_800B0078->pc = func_800ACDB8(2);
    }
    D_800B00C0 = 1;
}

/* Yield; continue while a selected actor is on screen, otherwise jump to
 * operand 2. */
void func_80095C00(void) {
    s32 x;
    s32 y;

    func_80095A7C(&x, &y);
    if (D_800ADC18 != 0) {
        D_800B0078->pc += 4;
        return;
    }
    if (y > 0 && y < 224 && x > 0 && x < 320) {
        D_800B0078->pc += 4;
    } else {
        D_800B0078->pc = func_800ACDB8(2);
    }
    D_800B00C0 = 1;
}

/* Continue when a selected actor is on collision layer operand 2,
 * otherwise jump to operand 4. */
void func_80095CC4(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        if (func_800ACDEC(2) == actor->layer) {
            D_800B0078->pc += 6;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(4);
}

/* Continue when the triangle a selected actor stands on has attribute
 * operand 2, otherwise jump to operand 4. */
void func_80095D6C(void) {
    FieldActor *actor;
    u8 attribute;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        attribute = D_800AF880.components.collision_triangles[actor->layer][actor->triangle[actor->layer]].attribute;
        if (func_800ACDEC(2) == attribute) {
            D_800B0078->pc += 6;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(4);
}

/* Continue when a selected actor is nearer than operand 2 to the published
 * actor, otherwise jump to operand 4. */
void func_80095E48(void) {
    FieldActor *other;
    FieldDescriptor *descriptor;
    s32 distance;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        other = descriptor->actor;
        distance = func_80099A04(WHOLE(D_800B06B8->actor->position[0]) - WHOLE(other->position[0]),
                                 WHOLE(D_800B06B8->actor->position[1]) - WHOLE(other->position[1]),
                                 WHOLE(D_800B06B8->actor->position[2]) - WHOLE(other->position[2]));
        if (distance < func_800ACDEC(2)) {
            D_800B0078->pc += 6;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(4);
}

/* Continue when the party's gold is at least the 32-bit operand 1,
 * otherwise jump to operand 5. */
void func_80095F24(void) {
    u8 *operand;

    operand = &D_800ADC00[D_800B0078->pc];
    if ((u32)D_8005A39C->gold >=
        operand[1] + (operand[2] << 8) + (operand[3] << 16) + (operand[4] << 24)) {
        D_800B0078->pc += 7;
        return;
    }
    D_800B0078->pc = func_800ACDB8(5);
}

/* Add gold, capped at 9999999. */
void func_80095FB8(void) {
    s32 gold;

    gold = D_8005A39C->gold + func_800ACDEC(1);
    if (gold > 9999999) {
        gold = 9999999;
    }
    D_8005A39C->gold = gold;
    D_800B0078->pc += 3;
}

/* Remove gold, not below zero. */
void func_8009601C(void) {
    s32 amount;
    s32 gold;

    amount = func_800ACDEC(1);
    gold = D_8005A39C->gold;
    gold -= amount;
    if (gold < 0) {
        gold = 0;
    }
    D_8005A39C->gold = gold;
    D_800B0078->pc += 3;
}

/* Continue when raw operand 1 shares a bit with `bits`, otherwise jump to
 * operand 3. */
void func_80096078(s32 bits) {
    if (func_800ACDB8(1) & bits & 0xFFFF) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue when raw operand 1 equals `value`, otherwise jump to operand 3. */
void func_800960E4(s32 value) {
    if ((func_800ACDB8(1) & 0xFFFF) == (value & 0xFFFF)) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

extern u16 D_800AFE9C;
extern u16 D_800AFC6C;
void func_80096078(s32 bits);
void func_800960E4(s32 value);

/* Branch unless the held buttons equal raw operand 1. */
void func_80096150(void) {
    func_800960E4(D_800AFE9C);
}

/* Branch unless D_800AFC6C equals raw operand 1. */
void func_80096178(void) {
    func_800960E4(D_800AFC6C);
}

/* Branch unless a held button is among raw operand 1. */
void func_800961A0(void) {
    func_80096078(D_800AFE9C);
}

/* Branch unless D_800AFC6C shares a bit with raw operand 1. */
void func_800961C8(void) {
    func_80096078(D_800AFC6C);
}

/* Clear D_800AFC6C. */
void func_800961F0(void) {
    D_800AFC6C = 0;
    D_800B0078->pc += 1;
}

s32 func_80095124(s32 item);
u8 *func_800950A0(s32 item);
u8 *func_8009501C(s32 item);

/* Store the carried count of item operand 1 in a variable. */
void func_80096214(void) {
    s32 item;
    s32 slot;
    u8 *counts;

    item = func_800ACDEC(1);
    slot = func_80095124(item);
    counts = func_800950A0(item);
    func_8009501C(item);
    if (slot != -1) {
        func_800A3074(func_800ACDB8(3), counts[slot]);
    } else {
        func_800A3074(func_800ACDB8(3), 0);
    }
    D_800B0078->pc += 5;
}

/* Continue when item operand 1 is carried, otherwise jump to operand 3. */
void func_800962C0(void) {
    if (func_80095124(func_800ACDEC(1)) != -1) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

void func_8009635C(s32 item);

/* Give one of item operand 1. */
void func_8009631C(void) {
    func_8009635C(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

s32 func_800951B8(s32 item);


#ifdef NON_MATCHING
/* Add one of `item` (list in the high byte) up to 99, or take a free slot. */
void func_8009635C(s32 item) {
    s32 slot;
    u8 *counts;
    u8 *ids;
    u8 *count;

    slot = func_80095124(item);
    counts = func_800950A0(item);
    ids = func_8009501C(item);
    if (slot != -1) {
        count = &counts[slot];
        if (*count < 99) {
            *count += 1;
        }
    } else {
        slot = func_800951B8(item);
        count = &counts[slot];
        if (slot != -1) {
            ids[slot] = item;
            *count = 1;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009635C);
#endif

/* Take one of item operand 1; an emptied slot's id becomes 0xFF. */
void func_8009640C(void) {
    s32 item;
    s32 slot;
    u8 *ids;
    u8 *counts;

    item = func_800ACDEC(1);
    slot = func_80095124(item);
    if (slot != -1) {
        ids = func_8009501C(item);
        counts = func_800950A0(item);
        if (--counts[slot] == 0) {
            ids[slot] = 0xFF;
        }
    }
    D_800B0078->pc += 3;
}

/* Continue when character operand 1 is in the party, otherwise jump to
 * operand 2. */
void func_800964B0(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (EVENT_OPERAND_BYTE(1) == D_80062590[i]) {
            D_800B0078->pc += 4;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(2);
}

/* Continue when game flag bit operand 1 (of unk1D30) is set, otherwise
 * jump to operand 2. */
void func_80096534(void) {
    if ((D_8005A39C->unk1D30 >> EVENT_OPERAND_BYTE(1)) & 1) {
        D_800B0078->pc += 4;
        return;
    }
    D_800B0078->pc = func_800ACDB8(2);
}

/* Set game flag bit operand 1 of unk1D30. */
void func_800965A8(void) {
    D_8005A39C->unk1D30 |= 1 << EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Clear game flag bit operand 1 of unk1D30. */
void func_800965F4(void) {
    D_8005A39C->unk1D30 &= ~(1 << EVENT_OPERAND_BYTE(1));
    D_800B0078->pc += 2;
}

/* Continue when variable 0 is below operand 1, otherwise jump. */
void func_80096644(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (func_800A3018(0) < value) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue when variable 0 is above operand 1, otherwise jump. */
void func_800966B4(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (value < func_800A3018(0)) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue when variable 0 equals operand 1, otherwise jump. */
void func_80096724(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (func_800A3018(0) == value) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Store an operand in variable 0 (extending the batch limit). */
void func_80096790(void) {
    D_800AFC7C += 32;
    func_800A3074(0, func_800ACDEC(1));
    D_800B0078->pc += 3;
}

/* Copy variable 0 into a variable. */
void func_800967E8(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference, func_800A3018(0));
    D_800B0078->pc += 3;
}

/* Restore HP of party slot `slot`, up to its maximum. */
void func_80096844(s32 slot, s32 amount) {
    D_8005A39C->characters[D_80062590[slot]].hp += amount;
    if (D_8005A39C->characters[D_80062590[slot]].max_hp < D_8005A39C->characters[D_80062590[slot]].hp) {
        D_8005A39C->characters[D_80062590[slot]].hp = D_8005A39C->characters[D_80062590[slot]].max_hp;
    }
}

/* Reduce HP of party slot `slot`, leaving at least 1. */
void func_800968CC(s32 slot, s32 amount) {
    s32 hp;

    hp = D_8005A39C->characters[D_80062590[slot]].hp - amount;
    if (hp <= 0) {
        hp = 1;
    }
    D_8005A39C->characters[D_80062590[slot]].hp = hp;
}

/* Restore EP of party slot `slot`, up to its maximum. */
void func_80096920(s32 slot, s32 amount) {
    D_8005A39C->characters[D_80062590[slot]].ep += amount;
    if (D_8005A39C->characters[D_80062590[slot]].max_ep < D_8005A39C->characters[D_80062590[slot]].ep) {
        D_8005A39C->characters[D_80062590[slot]].ep = D_8005A39C->characters[D_80062590[slot]].max_ep;
    }
}

/* Reduce EP of party slot `slot`, leaving at least 1. */
void func_800969A8(s32 slot, s32 amount) {
    s32 ep;

    ep = D_8005A39C->characters[D_80062590[slot]].ep - amount;
    if (ep <= 0) {
        ep = 1;
    }
    D_8005A39C->characters[D_80062590[slot]].ep = ep;
}

extern s16 D_800AEA2C[4];

/* Reduce the HP of the party members selected by mask table entry
 * (operand 3 & 3) by a selected operand. */
void func_800969FC(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_800968CC(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Set the jump mode, animation mode and repeat delay. */
void func_80096AF4(void) {
    D_800B2078.jump_mode = func_800ACDEC(1);
    D_800B2078.animation_mode = func_800ACDEC(3);
    D_800B2078.repeat_delay = func_800ACDEC(5);
    D_800B2078.repeat_remaining = 0;
    D_800B0078->pc += 7;
}

/* Store the HP of party slot operand 3 in a variable. */
void func_80096B58(void) {
    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        func_800A3074(func_800ACDB8(1), D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(3)]].hp);
    }
    D_800B0078->pc += 4;
}

/* Store the EP of party slot operand 3 in a variable. */
void func_80096C40(void) {
    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        func_800A3074(func_800ACDB8(1), D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(3)]].ep);
    }
    D_800B0078->pc += 4;
}

/* Set the HP of party slot operand 1 (capped at its maximum); operand 3
 * selects the slot that must be occupied. */
void func_80096D28(void) {
    s32 hp;

    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        hp = func_800ACDEC(2);
        if (D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_hp < hp) {
            hp = D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_hp;
        }
        D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].hp = hp;
    }
    D_800B0078->pc += 4;
}

/* Set the EP of party slot operand 1 (capped at its maximum). */
void func_80096E20(void) {
    s32 ep;

    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        ep = func_800ACDEC(2);
        if (D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_ep < ep) {
            ep = D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_ep;
        }
        D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].ep = ep;
    }
    D_800B0078->pc += 4;
}

/* Restore the EP of the masked party members by a selected operand (as
 * func_80097108). */
void func_80096F18(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_80096920(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Reduce the EP of the masked party members by a selected operand. */
void func_80097010(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_800969A8(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Restore the EP of the masked party members by a selected operand. */
void func_80097108(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_80096920(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Fully restore HP and EP of character operand 1. */
void func_80097200(void) {
    s32 id;

    id = func_800ACDEC(1);
    D_8005A39C->characters[id].hp = D_8005A39C->characters[id].max_hp;
    D_8005A39C->characters[id].ep = D_8005A39C->characters[id].max_ep;
    D_800B0078->pc += 3;
}

/* Fully restore every character's HP. */
void func_80097264(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        D_8005A39C->characters[i].hp = D_8005A39C->characters[i].max_hp;
    }
    D_800B0078->pc += 1;
}

/* Fully restore every character's EP. */
void func_800972AC(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        D_8005A39C->characters[i].ep = D_8005A39C->characters[i].max_ep;
    }
    D_800B0078->pc += 1;
}

/* Yield once. */
void func_800972F4(void) {
    D_800B00C0 = 1;
    D_800B0078->pc += 1;
}

void func_8007D93C(s32 a);
void func_80071E58(s32 a);

/* Field 8007D93C(0), then 80071E58(operand 1). */
void func_8009731C(void) {
    func_8007D93C(0);
    func_80071E58(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

void func_80071DCC(s32 a);

/* Field 80071DCC(operand 1). */
void func_80097364(void) {
    func_80071DCC(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

s32 func_8001B484(s32 a, s32 b);

/* Re-run until resident 8001B484(raw operand 2, operand byte 1) returns 0. */
void func_800973A4(void) {
    if (func_8001B484(func_800ACDB8(2) & 0xFFFF, EVENT_OPERAND_BYTE(1)) == 0) {
        D_800B0078->pc += 4;
    }
}

/* Skip operand-1 three-byte entries (and this opcode). */
void func_80097410(void) {
    D_800B0078->pc += func_800ACDEC(1) * 3 + 3;
}

/* Facing octant (0..7) of the controlled actor. */
s32 func_8009744C(void) {
    return (((D_800AF880.components.descriptors[D_800B2078.controlled].actor->heading_goal + 0x100) >> 9) + 2) & 7;
}

s32 func_80097A50(s32 speed);

/* Move the current actor toward actor operand 1 (speed operand 5, latched
 * in the slot); advances once arrived. */
void func_8009749C(void) {
    FieldActor *other;

    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    other = D_800AF880.components.descriptors[EVENT_OPERAND_BYTE(1)].actor;
    D_800B0078->target[0] = WHOLE(other->position[0]);
    D_800B0078->target[2] = WHOLE(other->position[2]);
    D_800B0078->target[1] = WHOLE(other->position[1]);
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(5);
    }
    if (func_80097A50(func_800ACDEC(5)) == 0) {
        D_800B0078->pc += 7;
    }
}

/* Move toward actor operand 1 at the default speed. */
void func_800975C0(void) {
    FieldActor *other;

    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    other = D_800AF880.components.descriptors[EVENT_OPERAND_BYTE(1)].actor;
    D_800B0078->target[0] = WHOLE(other->position[0]);
    D_800B0078->target[2] = WHOLE(other->position[2]);
    D_800B0078->target[1] = WHOLE(other->position[1]);
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80097A50(0xFFFF) == 0) {
        D_800B0078->pc += 5;
    }
}

/* Start a relative move (mode 1) from the current position, speed
 * operand 8; advances once arrived. */
void func_800976A8(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(8);
    }
    if (func_80097A50(func_800ACDEC(8)) == 0) {
        D_800B0078->pc += 10;
    }
}

/* Relative move (mode 1) at the default speed. */
void func_800977A4(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80097A50(0xFFFF) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Move in mode 3 from the current position, speed operand 5. */
void func_80097864(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 3;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(5);
    }
    if (func_80097A50(func_800ACDEC(5)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Continue a move at speed operand 8. */
void func_80097954(void) {
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(8);
    }
    if (func_80097A50(func_800ACDEC(8)) == 0) {
        D_800B0078->pc += 10;
    }
}

/* Continue a move at the default speed. */
void func_800979F0(void) {
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80097A50(0xFFFF) == 0) {
        D_800B0078->pc += 8;
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097A50);

s32 func_80099AC0(s32 speed);

/* Turn-move (mode 2) with speed operand 2 latched in the slot. */
void func_80098038(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(2);
    }
    if (func_80099AC0(func_800ACDEC(2)) == 0) {
        D_800B0078->pc += 4;
    }
}

/* Turn-move (mode 2) at the default speed. */
void func_800980FC(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 2;
    }
}

/* Turn-move in mode 3 from the current position, speed operand 3. */
void func_80098184(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 3;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(3);
    }
    if (func_80099AC0(func_800ACDEC(3)) == 0) {
        D_800B0078->pc += 5;
    }
}

/* Turn-move in mode 1 from the current position, speed operand 6. */
void func_80098274(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(6);
    }
    if (func_80099AC0(func_800ACDEC(6)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Turn-move in mode 1 at the default speed. */
void func_80098370(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 6;
    }
}

/* Turn-move in mode 0, speed operand 6. */
void func_80098430(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 0;
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(6);
    }
    if (func_80099AC0(func_800ACDEC(6)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Set the piece drift vector from selected operands and enable it. */
void func_800984EC(void) {
    D_800B2078.piece_drift[0] = func_8009CF78(1, EVENT_OPERAND_BYTE(7));
    D_800B2078.piece_drift[1] = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    D_800B2078.piece_drift[2] = func_8009D000(5, EVENT_OPERAND_BYTE(7));
    D_800B2078.piece_drift_mode |= 0x80;
    D_800B0078->pc += 8;
}

/* Event 0x74 (debug): print variable op1 unless 800c268c is set. */
void func_800985BC(void) {
    s32 value;

    if (D_800C268C == 0) {
        value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
        func_800379C8("DEB=%xh %d \n", value, value);
    }
    D_800B0078->pc += 3;
}

/* Store the planar length of (x2 - x1, z2 - z1) from selected operands in
 * a variable. */
void func_8009861C(void) {
    s32 x1;
    s32 z1;
    s32 x2;
    s32 z2;

    x1 = func_8009CFBC(3, EVENT_OPERAND_BYTE(11));
    z1 = func_8009D000(5, EVENT_OPERAND_BYTE(11));
    x2 = func_8009D044(7, EVENT_OPERAND_BYTE(11));
    z2 = func_8009D088(9, EVENT_OPERAND_BYTE(11));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, func_80099A4C(x2 - x1, z2 - z1));
    D_800B0078->pc += 12;
}

/* Store the distance between two points from selected operands in a
 * variable. */
void func_80098738(void) {
    s32 x1;
    s32 y1;
    s32 z1;
    s32 x2;
    s32 y2;
    s32 z2;

    x1 = func_8009CFBC(3, EVENT_OPERAND_BYTE(15));
    y1 = func_8009D000(5, EVENT_OPERAND_BYTE(15));
    z1 = func_8009D000(7, EVENT_OPERAND_BYTE(15));
    x2 = func_8009D044(9, EVENT_OPERAND_BYTE(15));
    y2 = func_8009D088(11, EVENT_OPERAND_BYTE(15));
    z2 = func_8009D088(13, EVENT_OPERAND_BYTE(15));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, func_80099A04(x2 - x1, z2 - z1, y2 - y1));
    D_800B0078->pc += 16;
}

s32 func_80073930(s32 a, s32 b, s32 c);

/* Store field 80073930(three selected operands) in a variable. */
void func_800988B8(void) {
    s32 a;
    s32 b;
    s32 value;

    a = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    b = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    value = func_80073930(a, b, func_8009D044(7, EVENT_OPERAND_BYTE(9)));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 10;
}

/* Store the current actor's facing (12 bits) in a variable. */
void func_8009899C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B0078->heading_goal & 0xFFF);
    D_800B0078->pc += 3;
}

/* Store a selected actor's facing (12 bits) in a variable. */
void func_800989F0(void) {
    s32 index;
    FieldActor *actor;

    index = func_8009CDB4(1);
    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(2) & 0xFFFF, actor->heading_goal & 0xFFF);
    }
    D_800B0078->pc += 4;
}

/* Place the current actor at a selected position (whole units), marking
 * flags 0x10000 / layer 0x200000, and mirror it to its descriptor and
 * model. */
void func_80098A7C(void) {
    FieldModel *model;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800B0078->flags |= 0x10000;
    D_800B0078->layer_flags |= 0x200000;
    D_800B0078->position[0] = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800B0078->position[2] = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800B0078->position[1] = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = WHOLE(D_800B0078->position[0]);
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = WHOLE(D_800B0078->position[1]);
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = WHOLE(D_800B0078->position[2]);
    model->position[0] = D_800B0078->position[0];
    model->position[1] = D_800B0078->position[1];
    model->position[2] = D_800B0078->position[2];
    D_800B0078->pc += 8;
}

void func_80098CAC(s32 mode);

/* Walk mode 0 at the default speed. */
void func_80098C00(void) {
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    func_80098CAC(0);
}

/* Walk mode 1 with speed operand 11 latched in the slot. */
void func_80098C3C(void) {
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(11);
    }
    func_80098CAC(1);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098CAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099214);

/* Turn-move in mode 0 at the default speed. */
void func_80099980(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 0;
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 6;
    }
}

/* Length of (dx, dy, dz). */
s32 func_80099A04(s32 dx, s32 dy, s32 dz) {
    VECTOR v;
    VECTOR squares;

    v.vx = dx;
    v.vy = dy;
    v.vz = dz;
    func_8004A414(&v, &squares);
    return SquareRoot0(squares.vx + squares.vy + squares.vz);
}

/* Length of (dx, dz). */
s32 func_80099A4C(s32 dx, s32 dz) {
    VECTOR v;
    VECTOR squares;

    v.vx = dx;
    v.vy = dz;
    v.vz = 0;
    func_8004A414(&v, &squares);
    return SquareRoot0(squares.vx + squares.vy);
}

/* Absolute value through the GTE square and square root. */
s32 func_80099A8C(s32 x) {
    VECTOR v;
    VECTOR squares;

    v.vx = x;
    func_8004A414(&v, &squares);
    return SquareRoot0(squares.vx);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099AC0);

/* Store the current actor's unkE4 in a variable. */
void func_80099EF8(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B0078->unkE4);
    D_800B0078->pc += 3;
}

/* Store the controlled actor's unkE4 in a variable. */
void func_80099F48(void) {
    FieldActor *player;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, player->unkE4);
    D_800B0078->pc += 3;
}

/* Store the current actor's facing octant in a variable. */
void func_80099FC4(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, (((D_800B0078->heading_goal + 0x100) >> 9) + 2) & 7);
    D_800B0078->pc += 3;
}

/* Store a selected descriptor's translation x, z, y in three variables. */
void func_8009A024(void) {
    s32 index;

    index = func_8009CDB4(1);
    if (index != 0xFF) {
        func_800A3074(func_800ACDB8(2) & 0xFFFF, D_800AF880.components.descriptors[index].transform.t[0]);
        func_800A3074(func_800ACDB8(4) & 0xFFFF, D_800AF880.components.descriptors[index].transform.t[2]);
        func_800A3074(func_800ACDB8(6) & 0xFFFF, D_800AF880.components.descriptors[index].transform.t[1]);
    }
    D_800B0078->pc += 8;
}

/* Set the current actor's unkE6 from an operand byte. */
void func_8009A0FC(void) {
    D_800B0078->unkE6 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Clear layer flag 0x1000000 and set unkEA from an operand byte. */
void func_8009A130(void) {
    D_800B0078->layer_flags &= ~0x1000000;
    D_800B0078->unk0EA = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* As func_8009A130, also clearing layer flag 0x10000. */
void func_8009A174(void) {
    func_8009A130();
    D_800B0078->layer_flags &= ~0x10000;
}

/* Once layer flag 0x10000 is set, set unkEA to 0xFF and advance. */
void func_8009A1AC(void) {
    if (D_800B0078->layer_flags & 0x10000) {
        D_800B0078->unk0EA = 0xFF;
        D_800B0078->pc += 1;
    }
}

/* Face the actor of party slot operand 1. */
void func_8009A1E4(void) {
    s32 index;
    FieldActor *other;
    s16 facing;

    index = D_8005A444[EVENT_OPERAND_BYTE(1)];
    if (index != 0xFF) {
        other = D_800AF880.components.descriptors[index].actor;
        facing = -ratan2(other->position[2] - D_800B0078->position[2],
                                other->position[0] - D_800B0078->position[0]) | 0x8000;
        D_800B0078->heading = facing;
        D_800B0078->heading_goal = facing;
    }
    D_800B0078->pc += 2;
}

/* Face a selected actor. */
void func_8009A2A8(void) {
    s32 index;
    FieldActor *other;
    s16 facing;

    index = func_8009CDB4(1);
    if (index != 0xFF) {
        other = D_800AF880.components.descriptors[index].actor;
        facing = -ratan2(other->position[2] - D_800B0078->position[2],
                                other->position[0] - D_800B0078->position[0]) | 0x8000;
        D_800B0078->heading = facing;
        D_800B0078->heading_goal = facing;
    }
    D_800B0078->pc += 2;
}

/* Blend the camera distance toward operand 1 over operand-3 frames. */
void func_8009A34C(void) {
    s32 step;

    D_800AF880.steps = EVENT_OPERAND_BYTE(3);
    if (D_800AF880.steps == 0) {
        D_800AF880.steps++;
        D_800B2078.camera_counter += 2;
    }
    step = -((D_800AF880.distance - func_800ACDEC(1)) << 16) / D_800AF880.steps;
    D_800AF880.start = D_800AF880.distance << 16;
    D_800AF880.flags |= 1;
    D_800AF880.step = step;
    D_800B0078->pc += 4;
}

/* Blend the camera elevation toward `target` over `steps` frames. */
void func_8009A420(s32 target, s32 steps) {
    if (steps == 0) {
        D_800B2078.camera_counter = 2;
        steps = 1;
    }
    D_800AF880.elevation_steps = steps;
    D_800AF880.elevation_value = (s16)D_800AF880.elevation << 16;
    D_800AF880.flags |= 8;
    D_800AF880.elevation_step = -(((s16)D_800AF880.elevation - target) << 16) / steps;
}

/* Blend the camera elevation (selected operand 1, steps operand 3 & 0x7F). */
void func_8009A490(void) {
    func_8009A420(func_8009CF78(1, EVENT_OPERAND_BYTE(3)), EVENT_OPERAND_BYTE(3) & 0x7F);
    D_800B0078->pc += 4;
}

/* Camera angle octant (0..7). */
s32 func_8009A514(void) {
    return (7 - ((D_800AF880.angle - 0x100) >> 9)) & 7;
}

/* Store the camera angle octant in a variable. */
void func_8009A534(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference, func_8009A514() & 0xFFFF);
    D_800B0078->pc += 3;
}

/* Yield while any camera flag in operand byte 1 is set. */
void func_8009A58C(void) {
    if (!(D_800AF880.flags & EVENT_OPERAND_BYTE(1))) {
        D_800B0078->pc += 2;
        return;
    }
    D_800B00C0 = 1;
}

/* Yield while any camera flag in operand byte 1 is set (second opcode). */
void func_8009A5E0(void) {
    if (!(D_800AF880.flags & EVENT_OPERAND_BYTE(1))) {
        D_800B0078->pc += 2;
        return;
    }
    D_800B00C0 = 1;
}

/* Set camera heading block 0. */
void func_8009A634(void) {
    D_800AF880.heading_blocks[0] = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set camera heading block 1. */
void func_8009A670(void) {
    D_800AF880.heading_blocks[1] = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Store (sin(selected angle) * selected length) >> 12 in a variable. */
void func_8009A6AC(void) {
    s32 reference;
    s32 angle;
    s32 length;

    reference = func_800ACDB8(1) & 0xFFFF;
    angle = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    length = func_8009D000(5, EVENT_OPERAND_BYTE(7));
    func_800A3074(reference, (func_8003F8B0(angle) * length) >> 12);
    D_800B0078->pc += 8;
}

/* Store (cos(selected angle) * selected length) >> 12 in a variable. */
void func_8009A768(void) {
    s32 reference;
    s32 angle;
    s32 length;

    reference = func_800ACDB8(1) & 0xFFFF;
    angle = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    length = func_8009D000(5, EVENT_OPERAND_BYTE(7));
    func_800A3074(reference, (func_8003F8CC(angle) * length) >> 12);
    D_800B0078->pc += 8;
}

/* Store atan2(selected y, selected x) in a variable. */
void func_8009A824(void) {
    s32 reference;
    s32 y;

    reference = func_800ACDB8(1) & 0xFFFF;
    y = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    func_800A3074(reference, (s16)ratan2(y, func_8009D000(5, EVENT_OPERAND_BYTE(7))));
    D_800B0078->pc += 8;
}

/* The current actor's facing octant (0..7). */
s32 func_8009A8DC(void) {
    return (((D_800B0078->heading_goal + 0x100) >> 9) + 2) & 7;
}

/* Face `angle`; outside D_800ADB1C also sets the rest facing. */
void func_8009A904(u16 angle) {
    if (D_800ADB1C == 0) {
        D_800B0078->heading = angle | 0x8000;
        D_800B0078->heading_goal = angle | 0x8000;
        D_800B0078->unk108 = angle | 0x8000;
    }
    D_800B0078->heading = angle | 0x8000;
    D_800B0078->heading_goal = angle | 0x8000;
    D_800B0078->pc += 3;
}

/* A selected actor faces `angle`. */
void func_8009A958(u16 angle) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        if (D_800ADB1C == 0) {
            actor->heading = angle | 0x8000;
            actor->heading_goal = angle | 0x8000;
            actor->unk108 = angle | 0x8000;
        }
        actor->heading = angle | 0x8000;
        actor->heading_goal = angle | 0x8000;
    }
    D_800B0078->pc += 4;
}

/* Selected actor operand 1 faces selected actor operand 2. */
void func_8009AA00(void) {
    FieldActor *actor;
    FieldActor *other;
    s16 facing;

    if (func_8009CDB4(1) != 0xFF && func_8009CDB4(2) != 0xFF) {
        other = D_800AF880.components.descriptors[func_8009CDB4(2)].actor;
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        facing = -ratan2(other->position[2] - actor->position[2],
                                other->position[0] - actor->position[0]) | 0x8000;
        if (D_800ADB1C == 0) {
            actor->heading = facing;
            actor->heading_goal = facing;
            actor->unk108 = facing;
        }
        actor->heading = facing;
        actor->heading_goal = facing;
    }
    D_800B0078->pc += 3;
}

/* Face `angle` relative to the camera. */
void func_8009AB08(u16 angle) {
    s16 facing;

    facing = ((angle - D_800AF880.angle) & 0xFFF) | 0x8000;
    D_800B0078->heading = facing;
    D_800B0078->heading_goal = facing;
    if (D_800ADB1C == 0) {
        D_800B0078->unk108 = facing;
    }
    D_800B0078->pc += 3;
}

extern s16 D_800AEA34[8];

/* Turn clockwise by operand-1 octants. */
void func_8009AB5C(void) {
    s32 turn;

    turn = func_800ACDEC(1);
    func_8009A904(D_800AEA34[(turn + func_8009A8DC()) & 7]);
}

/* Turn counter-clockwise by operand-1 octants. */
void func_8009ABAC(void) {
    s32 turn;

    turn = func_800ACDEC(1);
    func_8009A904(D_800AEA34[(func_8009A8DC() - turn) & 7]);
}

void func_8009A958(u16 angle);

/* A selected actor faces direction table entry operand 2. */
void func_8009ABFC(void) {
    func_8009A958(D_800AEA34[func_800ACDEC(2)]);
}

/* A selected actor faces direction entry operand 2, camera-relative. */
void func_8009AC34(void) {
    func_8009A958((D_800AEA34[func_800ACDEC(2)] - D_800AF880.angle) & 0xFFF);
}

/* Face direction table entry operand 1. */
void func_8009AC7C(void) {
    func_8009A904(D_800AEA34[func_800ACDEC(1)]);
}

void func_8009AB08(u16 angle);

/* Face direction table entry operand 1, camera-relative. */
void func_8009ACB4(void) {
    func_8009AB08(D_800AEA34[func_800ACDEC(1)]);
}

extern s16 D_800AEA44[8];

/* Face second-table direction operand byte 1, camera-relative. */
void func_8009ACEC(void) {
    s16 facing;

    facing = ((D_800AEA44[EVENT_OPERAND_BYTE(1)] - D_800AF880.angle) & 0xFFF) | 0x8000;
    D_800B0078->heading = facing;
    D_800B0078->heading_goal = facing;
    if (D_800ADB1C == 0) {
        D_800B0078->unk108 = facing;
    }
    D_800B0078->pc += 2;
}

extern s16 D_800AEA54[8];

/* Face third-table direction operand byte 1. */
void func_8009AD6C(void) {
    s16 facing;

    facing = D_800AEA54[EVENT_OPERAND_BYTE(1)] | 0x8000;
    D_800B0078->heading = facing;
    D_800B0078->heading_goal = facing;
    if (D_800ADB1C == 0) {
        D_800B0078->unk108 = facing;
    }
    D_800B0078->pc += 2;
}

/* Set camera flag 0x4000. */
void func_8009ADDC(void) {
    D_800AF880.flags |= 0x4000;
    D_800B0078->pc += 1;
}

/* Clear camera flag 0x4000 (and the upper half). */
void func_8009AE0C(void) {
    D_800AF880.flags &= 0xBFFF;
    D_800B0078->pc += 1;
}

#ifdef NON_MATCHING
/* Blend the camera projection toward `target` over `steps` frames (at
 * once when zero). */
void func_8009AE3C(s32 target, s32 steps) {
    if (steps != 0) {
        D_800AF880.projection_steps = steps;
        D_800AF880.flags |= 0x10;
        D_800AF880.projection_value = D_800AF880.projection << 16;
        D_800AF880.projection_step = -((D_800AF880.projection - target) << 16) / steps;
    } else {
        D_800AF880.projection = target;
        D_800AF880.projection_steps = 0;
        D_800B2078.camera_counter += 2;
    }
    D_800AF880.flags &= 0xDFFF;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AE3C);
#endif

#ifdef NON_MATCHING
/* Walk party slot `slot`'s member one gather step toward (x, z). Returns 0
 * when the member is absent, disabled or has arrived (it is then placed at
 * (x, z) facing `facing`, or its own heading for 0xff) and -1 while it is
 * still walking; a member stuck for 0x40 steps or an override warps. */
s32 func_8009AEE0(s32 slot, s32 x, s32 z, s32 facing) {
    FieldDescriptor *descriptor;
    FieldModel *model;
    FieldActor *actor;
    FieldActor *saved;
    s32 saved_index;
    s32 member;
    s32 step;
    s32 dx;
    s32 distance;
    s32 dz;
    VECTOR delta;
    u16 heading;

    member = D_8005A444[slot];
    if (member == 0xFF) {
        return 0;
    }
    descriptor = &D_800AF880.components.descriptors[member];
    if (descriptor->flags & 0x20) {
        return 0;
    }
    model = descriptor->model;
    actor = descriptor->actor;
    if (model->unk18 == 0) {
        model->unk18 = 0x4000000 / (u16)actor->unk76;
    }
    step = func_80099A8C(model->unk18 >> 15) + 1;
    dx = x - WHOLE(actor->position[0]);
    dz = z - WHOLE(actor->position[2]);
    delta.vx = dx;
    delta.vy = 0;
    delta.vz = dz;
    distance = func_80099A4C(dx, dz);
    actor->flags |= 0x400000;
    if (step >= distance) {
    arrive:
        if (!(actor->flags & 0x8000)) {
            if (facing == 0xFF) {
                heading = actor->heading_goal | 0x8000;
            } else {
                heading = D_800AEA34[facing] | 0x8000;
            }
        } else {
            heading = actor->unk11C | 0x8000;
        }
        actor->heading = heading;
        actor->heading_goal = heading;
        actor->position[0] = x << 16;
        actor->position[2] = z << 16;
        actor->stuck = 0;
        actor->flags &= 0xFDDFF7FF;
        return 0;
    }
    if (actor->last_position[0] == WHOLE(actor->position[0]) &&
        actor->last_position[1] == WHOLE(actor->position[1]) &&
        actor->last_position[2] == WHOLE(actor->position[2])) {
        actor->stuck++;
    } else {
        actor->stuck = 0;
    }
    actor->heading_goal = actor->heading = func_8007B694(&delta);
    if ((s16)actor->stuck > 0x40 || (s16)D_800B2078.unk2348 != 0) {
        saved = D_800B0078;
        saved_index = D_800AFD1C;
        D_800B0078 = actor;
        D_800AFD1C = D_8005A444[slot];
        func_8009E574(x, z);
        D_800B0078 = saved;
        D_800AFD1C = saved_index;
        goto arrive;
    }
    return -1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AEE0);
#endif

/* Force the party position. */
void func_8009B15C(void) {
    D_800B2078.forced_position = 1;
    D_800B0078->pc += 1;
}

void func_80081C54(s32 index);

/* Release forced positioning and resettle the controlled actor. */
void func_8009B184(void) {
    s32 i;

    i = 0;
    D_800B2078.forced_position = 0;
    D_800B2078.party_processing_mode = 0;
    D_800B2078.unk2368 = 0;
    D_800B2078.unk2364 = 0;
    D_800B2078.unk2360 = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    do {
        i++;
        func_80081C54(D_800B2078.controlled);
    } while (i < 32);
    D_800B0078->pc += 1;
}

s32 func_8009AEE0(s32 member, s32 x, s32 z, s32 range);
void func_8009B338(void);

/* Yield until all three party members are near the leader, then release
 * party processing and continue. */
void func_8009B210(void) {
    FieldActor *leader;
    s32 x;
    s32 z;
    s32 near;

    leader = D_800AF880.components.descriptors[D_8005A444[0]].actor;
    x = WHOLE(leader->position[0]);
    z = WHOLE(leader->position[2]);
    near = func_8009AEE0(0, x, z, 0xFF) == 0;
    if (func_8009AEE0(1, x, z, 0xFF) == 0) {
        near |= 2;
    }
    if (func_8009AEE0(2, x, z, 0xFF) == 0) {
        near |= 4;
    }
    D_800B00C0 = 1;
    if (near == 7) {
        D_800B0078->pc++;
        D_800B2078.party_processing_mode = 0;
        D_800B2078.unk2348 = 0;
        func_8009B338();
    } else {
        D_800B2078.party_processing_mode = 1;
        D_800B0078->pc--;
    }
}

/* Release the party motion overrides and resettle the controlled actor. */
void func_8009B338(void) {
    s32 i;

    i = 0;
    D_800B2078.unk2368 = 0;
    D_800B2078.unk2364 = 0;
    D_800B2078.unk2360 = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    do {
        i++;
        func_80081C54(D_800B2078.controlled);
    } while (i < 32);
}

/* Event 0x23: gather the party at (op1, op3), (op5, op7) and (op9, op11)
 * facing op15/op17/op19, retrying until all three have arrived; op1 0x7fff
 * instead marks every member's heading as turned and continues. */
void func_8009B398(void) {
    FieldActor *actor;
    s32 i;
    s32 near;

    if (func_8009CF78(1, EVENT_OPERAND_BYTE(0xD)) == 0x7FFF) {
        D_800B0078->pc += 0x14;
        D_800B2078.preserve_nonplayer_motion = 1;
        i = 0;
        do {
            if (D_8005A444[i] != 0xFF) {
                actor = D_800AF880.components.descriptors[D_8005A444[i]].actor;
                actor->heading_goal = actor->heading = actor->heading_goal | 0x8000;
            }
            i++;
        } while (i < 3);
        return;
    }
    near = func_8009AEE0(0, func_8009CF78(1, EVENT_OPERAND_BYTE(0xD)), func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD)),
                         func_800ACDEC(0xE)) == 0;
    if (func_8009AEE0(1, func_8009D000(5, EVENT_OPERAND_BYTE(0xD)), func_8009D044(7, EVENT_OPERAND_BYTE(0xD)),
                      func_800ACDEC(0x10)) == 0) {
        near |= 2;
    }
    if (func_8009AEE0(2, func_8009D088(9, EVENT_OPERAND_BYTE(0xD)), func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD)),
                      func_800ACDEC(0x12)) == 0) {
        near |= 4;
    }
    D_800B00C0 = 1;
    if (near == 7) {
        D_800B2078.unk2348 = 0;
        D_800B0078->pc += 0x14;
        D_800B2078.party_processing_mode = 0;
    } else {
        D_800B2078.party_processing_mode = 1;
        D_800B0078->pc -= 1;
    }
    D_800B2078.preserve_nonplayer_motion = 1;
}

/* Store the camera projection in a variable. */
void func_8009B664(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.projection);
    D_800B0078->pc += 3;
}

void func_8009AE3C(s32 target, s32 steps);

/* Blend the camera projection (operands: target, frames). */
void func_8009B6AC(void) {
    s32 target;

    target = func_800ACDEC(1);
    func_8009AE3C(target, func_800ACDEC(3));
    D_800B0078->pc += 5;
}

extern s16 D_800AEA64[64]; /* octant turn table [from * 8 + to] */

/* Turn the camera to octant `octant` over `steps` frames. */
void func_8009B708(s32 octant, s32 steps) {
    s32 current;
    s32 velocity;

    current = func_8009A514() & 0xFFFF;
    if (steps == 0) {
        steps = 1;
        D_800B2078.camera_counter += 2;
    }
    velocity = (D_800AEA64[current * 8 + octant] << 25) / steps;
    D_800AF880.heading_steps = steps;
    D_800AF880.heading = ((octant + 4) & 7) << 9;
    D_800AF880.heading_velocity = velocity;
}

/* Turn the camera one octant (direction 0: positive) over `steps` frames. */
void func_8009B7A8(s32 direction, s32 steps) {
    s32 velocity;
    s32 heading;

    if (steps == 0) {
        steps = 1;
        D_800B2078.camera_counter += 2;
    }
    if (direction == 0) {
        D_800AF880.heading_velocity = 0x2000000 / steps;
        D_800AF880.heading += 0x200;
    } else {
        D_800AF880.heading_velocity = (s32)0xFE000000 / steps;
        D_800AF880.heading -= 0x200;
    }
    D_800AF880.heading_steps = steps;
}

/* Once the camera is idle, turn one octant over operand-1 frames. */
void func_8009B824(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B7A8(0, func_800ACDEC(1));
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

/* As func_8009B824 (the original also passes direction 0). */
void func_8009B884(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B7A8(0, func_800ACDEC(1));
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

void func_8009B708(s32 octant, s32 steps);

/* Turn the camera to octant operand 1 over operand-3 frames (at once
 * outside D_800ADB1C). */
void func_8009B8E4(void) {
    s32 octant;
    s32 steps;
    s16 heading;

    octant = func_800ACDEC(1);
    steps = func_800ACDEC(3);
    if (D_800ADB1C == 0) {
        heading = (octant + 4) & 7;
        D_800AF880.heading_angles.vy = heading << 9;
        D_800AF880.heading = heading << 9;
        D_800B0078->pc += 5;
    } else if (D_800AF880.heading_steps == 0) {
        func_8009B708(octant, steps);
        D_800B0078->pc += 5;
    }
    D_800B00C0 = 1;
}

/* Once the camera is idle, save its octant, projection and elevation. */
void func_8009B9A0(void) {
    if (D_800AF880.heading_steps == 0) {
        D_800AF880.saved_view.octant = func_8009A514();
        D_800AF880.saved_view.projection = D_800AF880.projection;
        D_800AF880.saved_view.elevation = D_800AF880.elevation;
        D_800B0078->pc += 1;
    }
}

/* Once the camera is idle, blend back to the saved view over 32 frames. */
void func_8009BA0C(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B708(D_800AF880.saved_view.octant, 32);
        func_8009AE3C(D_800AF880.saved_view.projection, 32);
        func_8009A420(D_800AF880.saved_view.elevation, 32);
        D_800B0078->pc += 1;
    }
}



#ifdef NON_MATCHING
/* Set the camera elevation, octant and projection at once. */
void func_8009BA7C(void) {
    s16 heading;
    s32 angle;

    D_800AF880.elevation = func_800ACDEC(3);
    heading = (func_800ACDEC(1) + 4) & 7;
    angle = heading << 9;
    D_800AF880.heading = angle;
    D_800AF880.heading_angles.vy = heading << 9;
    D_800AF880.heading_high = angle << 16;
    D_800AF880.projection = func_800ACDEC(5);
    SetGeomScreen(D_800AF880.projection);
    D_800B0078->pc += 7;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BA7C);
#endif

/* Wait on the actor's dialogue window: with none, store the actor's +81
 * byte in variable 14 and continue; otherwise once its speaker has layer
 * flag 0x200 and the actor's low wait bit is clear, end the waiting script
 * slot and release the window. Yields. */
void func_8009BB0C(void) {
    s32 window;
    u32 value;
    s32 bits;

    if (func_8009CD18(&window) == -1) {
        D_800AFC7C += 8;
        func_800A3074(0x14, D_800B0078->unk081);
        D_800B0078->pc++;
        return;
    }
    if (D_800AF880.components.descriptors[D_800C2698[window].unk418].actor->layer_flags & 0x200) {
        value = D_800B0078->unk84;
        if (value >> 16) {
            bits = (value >> 16) & 0xFFFF;
        } else {
            bits = value & 0xFFFF;
        }
        if (!(bits & 1)) {
            if (D_800B0078->slots[D_800B0078->slot].priority != 7) {
                func_800A1B70();
            }
            D_800C2698[window].cleared = 0;
        }
    }
    D_800B00C0 = 1;
}

#ifdef NON_MATCHING
/* Event 0xa9: once this actor's dialogue window has its answer (or is
 * still being typed), highlight lines op1 >> 4 .. op1 & 0xf as a choice. */
void func_8009BC98(void) {
    s32 window;
    u32 first;

    if (func_8009CD18(&window) == 0) {
        D_800AFC7C += 8;
        if (func_80033CD0(&D_800C2698[window].text) == 1 ||
            (D_800C2698[window].text.unk84 != 0 && D_800C2698[window].text.unk6C != 0)) {
            D_800C2698[window].status = 0;
            D_800B0078->unk081 = 0xFF;
            first = EVENT_OPERAND_BYTE(1) >> 4;
            D_800C2698[window].unk37E = first;
            D_800C2698[window].unk382 = 0;
            D_800C2698[window].unk380 = (EVENT_OPERAND_BYTE(1) & 0xF) - first + 1;
            func_80034800(&D_800C2698[window].text, 0xEF, 0x1E, 0xF0);
            D_800B0078->pc += 2;
        }
    } else {
        D_800B0078->pc += 2;
    }
    D_800B00C0 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BC98);
#endif

/* Whether the current actor's octant (state bits 9-11) is within four
 * octants past the camera's. */
s32 func_8009BE58(void) {
    return ((((s32)(D_800B0078->state.word >> 9) & 7) - (func_8009A514() & 0xFFFF)) & 7) < 5;
}

s32 func_8009CD18(s32 *window);


/* Close this actor's dialogue window (operand 1 zero) or reset its
 * speech state; yields. */
void func_8009BE9C(void) {
    FieldActor *actor;
    s32 window;

    actor = D_800B0078;
    if (D_800ADC00[actor->pc + 1] == 0) {
        if (func_8009CD18(&window) == 0) {
            D_800C2698[window].cleared = 0;
            D_800B0078->pc += 2;
        } else {
            D_800B0078->pc += 2;
        }
    } else {
        actor->unk82 = 0;
        actor->unk88 = 0;
        actor->unk8A = 0;
        D_800B0078->unk83 = 0;
        D_800B0078->unk84 = 0;
        D_800B0078->pc += 2;
    }
    D_800B00C0 = 1;
}

void func_8009C01C(void);

/* Copy a selected actor's unk80 and open its message (func_8009C01C). */
void func_8009BF8C(void) {
    if (func_8009CDB4(1) != 0xFF) {
        D_800B0078->character = D_800AF880.components.descriptors[func_8009CDB4(1)].actor->character;
        func_8009C01C();
        return;
    }
    D_800B0078->pc += 6;
}

s32 func_8009C5A8(s32 index, s32 mode);

/* Show a message for a selected actor; re-runs until a window is free. */
void func_8009C01C(void) {
    if (func_8009CDB4(1) != 0xFF) {
        s32 index = func_8009CDB4(1);

        D_800B0078->pc += 1;
        if (func_8009C5A8(index, 0) == -1) {
            D_800B0078->pc -= 1;
        }
    } else {
        D_800B0078->pc += 6;
    }
}

/* Show a message for the current actor (mode 0). */
void func_8009C0B4(void) {
    func_8009C5A8(D_800AFD1C, 0);
}

/* Show a message for the current actor (mode 1). */
void func_8009C0DC(void) {
    func_8009C5A8(D_800AFD1C, 1);
}

/* Show a message for the current actor (mode 2). */
void func_8009C104(void) {
    func_8009C5A8(D_800AFD1C, 2);
}

/* Show a message for the current actor (mode 3). */
void func_8009C12C(void) {
    func_8009C5A8(D_800AFD1C, 3);
}

#ifdef NON_MATCHING
/* Show the dialogue portrait of `character`: finish a pending slot first
 * (upload loaded images, or release shown ones) and return -1; a slot
 * already holding it is selected (bits 2-4 of the actor state) and 0
 * returned; otherwise the next free slot starts loading its image files and
 * -1 is returned. */
s32 func_8009C154(s16 character) {
    s32 i;
    s32 tries;
    s32 found;
    s32 count;

    for (i = 0; i < 3; i++) {
        if (D_800B06A4[i].b == 1) {
            if (func_80028A60(1) != 0) {
                return -1;
            }
            D_800B06A4[i].b = 2;
            func_80070340(D_800ADB10, D_800AEAE4[i][0].x, D_800AEAE4[i][0].y, D_800AEAE4[i][0].clut_x,
                          D_800AEAE4[i][0].clut_y, 0x100, 1);
            if (D_800B06A4[i].c == 0) {
                func_80070340(D_800ADB10, D_800AEAE4[i][1].x, D_800AEAE4[i][1].y, D_800AEAE4[i][1].clut_x,
                              D_800AEAE4[i][1].clut_y, 0x100, 1);
            } else {
                func_80070340(D_800ADB14, D_800AEAE4[i][1].x, D_800AEAE4[i][1].y, D_800AEAE4[i][1].clut_x,
                              D_800AEAE4[i][1].clut_y, 0x100, 1);
            }
            return -1;
        }
        if (D_800B06A4[i].b == 2) {
            D_800B06A4[i].b = 0;
            func_800320E8(D_800ADB10);
            if (D_800B06A4[i].c == 1) {
                func_800320E8(D_800ADB14);
            }
            return -1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (D_800B06A4[i].a == character) {
            D_800B0078->state.bits.unk2 = i;
            return 0;
        }
    }
    found = 0;
    for (tries = 0; tries < 3; tries++) {
        D_800ADB0C++;
        if (D_800ADB0C >= 3) {
            D_800ADB0C = 0;
        }
        if (func_8009C538(D_800B06A4[D_800ADB0C].a) == 0) {
            found = 1;
            break;
        }
    }
    if (found == 0) {
        return -1;
    }
    D_800B0078->state.bits.unk2 = D_800ADB0C;
    func_80028470(4, 0);
    D_800B06A4[D_800ADB0C].a = character;
    D_800B06A4[D_800ADB0C].b = 1;
    D_800B06A4[D_800ADB0C].c = 0;
    D_800B00C8[0].file = D_800AE1E0[character][0] + 0x46;
    D_800ADB10 = D_800B00C8[0].data = func_80031BDC(func_800288EC(D_800B00C8[0].file), 0);
    count = 1;
    if (D_800AE1E0[character][1] != D_800AE1E0[character][0]) {
        D_800B06A4[D_800ADB0C].c = 1;
        D_800B00C8[1].file = D_800AE1E0[character][1] + 0x46;
        count = 2;
        D_800ADB14 = D_800B00C8[1].data = func_80031BDC(func_800288EC(D_800B00C8[1].file), 0);
    }
    D_800B00C8[count].file = 0;
    D_800B00C8[count].data = NULL;
    func_80029AFC(D_800B00C8, 0, 0);
    return -1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C154);
#endif

/* -1 when an idle window shows message kind 1 for `id`, else 0. */
s32 func_8009C538(s32 id) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].busy == 0 && D_800C2698[i].unk494 == 1 && D_800C2698[i].unk495 == id) {
            return -1;
        }
    }
    return 0;
}

#ifdef NON_MATCHING
/* Open this actor's dialogue window for message op1 above/below speaker
 * `speaker` (mode 0 follows the speaker, mode 3 is centred, others use the
 * fixed full-width box); op3 overrides the style byte. Returns -1 while the
 * window cannot open yet (the instruction is retried) and 0 once opened. */
s32 func_8009C5A8(s32 speaker, s32 mode) {
    s32 owned;
    s32 x;
    s32 y;
    u16 message;
    s32 window;
    s32 i;
    s32 idle;
    s32 combined;
    s32 columns;
    u8 rows;
    s32 progress;
    s32 low;
    s32 style;
    s32 top;
    s16 left;
    s32 flags;

    D_800AFC7C += 0x20;
    if (D_800ADB2C != 0 || D_800AFD04 != 0 || D_800C4268 != 0 || D_800ADB64 != 0xFF ||
        (D_800ADB70 == 0 && func_8008A558() != 0) ||
        ((u8)D_800B0078->character != 0xFF && func_8009C154((u8)D_800B0078->character) == -1)) {
        D_800B00C0 = 1;
        return -1;
    }
    D_800C4268++;
    if (func_8009CD18(&owned) != -1) {
        D_800B00C0 = 1;
        D_800C2698[owned].cleared = 0;
        return -1;
    }
    D_800AFC7C += 8;
    message = func_800ACDB8(1);
    if (func_80080720() != 0) {
        window = func_80080760();
        if (window != 0xFFFF) {
            D_800C2698[window].cleared = 0;
            D_800B00C0 = 1;
            return -1;
        }
    } else {
        window = func_800807B4();
    }
    idle = 0;
    combined = 0;
    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].busy == 0) {
            idle++;
            combined |= D_800C2698[i].layout;
        }
    }
    columns = func_8003373C(D_800ADBF0, message);
    rows = func_80033760(D_800ADBF0, message);
    if (mode == 0 || mode == 3) {
        if (D_800B0078->unk82 != 0) {
            columns = D_800B0078->unk82;
        }
        if (D_800B0078->unk83 != 0) {
            rows = D_800B0078->unk83;
        }
    }
    progress = D_800B0078->unk84;
    low = progress & 0xFFFF;
    D_800B0078->unk84 = low;
    style = low;
    if (EVENT_OPERAND_BYTE(3) != 0) {
        style = (progress & 0xFF00) | EVENT_OPERAND_BYTE(3);
        D_800B0078->unk84 = low | (style << 16);
    }
    top = 0x10;
    switch ((style >> 4) & 3) {
    case 1:
        goto above;
    case 0:
        if (((((D_800B0078->state.word >> 9) & 7) - func_8009A514()) & 7) >= 5) {
            if (!(combined & 0x80) && idle == 0) {
                goto above;
            }
        } else if (combined & 0x80) {
            goto above;
        }
        /* fall through */
    case 2:
        D_800C2698[window].layout = 0x81;
        if (mode == 0 || mode == 3) {
            func_8007F814(speaker, &x, &y, -0x40);
            top = 0x94;
            if (mode == 0) {
                top = y + 0x30;
            } else {
                x = 0xA0;
            }
            if ((u8)D_800B0078->character != 0xFF && !(style & 2)) {
                columns += 0x11;
                if (columns < 0x18) {
                    columns = 0x29;
                }
                rows = 4;
                top = 0x94;
            }
        } else {
            columns = 0x48;
            rows = 4;
            top = 0x94;
            x = 0xA0;
        }
        break;
    above:
        D_800C2698[window].layout = 1;
        if (mode == 0 || mode == 3) {
            func_8007F814(speaker, &x, &y, -0x40);
            top = 0x14;
            if (mode == 0) {
                top = y - rows * 14 - 0x24;
            } else {
                x = 0xA0;
            }
            if ((u8)D_800B0078->character != 0xFF && !(style & 2)) {
                rows = 4;
                if (columns < 0x18) {
                    columns = 0x18;
                }
                columns += 0x11;
                top = 0x10;
            }
        } else {
            columns = 0x48;
            rows = 4;
            top = 0x10;
            x = 0xA0;
        }
        break;
    }
    left = x - 8 - columns * 2;
    if (left < 0xC) {
        left = 0xC;
    }
    if (left + 0x10 + columns * 4 >= 0x135) {
        left = 0x124 - columns * 4;
    }
    if (top < 0x10) {
        top = 0x10;
    }
    if (top + 8 + rows * 14 >= 0xD5) {
        top = 0xCC - rows * 14;
    }
    if (mode == 0 || mode == 3) {
        if (D_800B0078->unk88 != 0) {
            left = D_800B0078->unk88;
        }
        if (D_800B0078->unk8A != 0) {
            top = D_800B0078->unk8A;
        }
        if (D_800B0078->unk82 != 0) {
            columns = D_800B0078->unk82;
        }
        if (D_800B0078->unk83 != 0) {
            rows = D_800B0078->unk83;
        }
        if ((u8)D_800B0078->character != 0xFF && !(style & 2)) {
            rows = 4;
        }
    }
    if (style & 0x40) {
        D_800C2698[window].layout |= 0x40;
    }
    flags = 0;
    if (!(style & 0xC)) {
        flags = (((((s16)D_800AF880.components.descriptors[speaker].actor->heading_goal >> 9) - func_8009A514()) + 1) &
                 7) >= 4;
        flags <<= 10;
    } else if (style & 4) {
        flags = 0x400;
    }
    func_8007F8DC(left, top, message, window, columns, rows, D_800AFD1C, speaker, mode, flags, style);
    func_8009CCF8(window);
    D_800B0078->heading |= 0x8000;
    D_800B0078->pc += 4;
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C5A8);
#endif

/* Set talk-inhibit bit `bit`. */
void func_8009CCF8(s32 bit) {
    D_800B2078.open_windows |= 1 << bit;
}

/* Find the idle window owned by the current actor. */
s32 func_8009CD18(s32 *window) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].owner == D_800AFD1C && D_800C2698[i].busy == 0) {
            *window = i;
            return 0;
        }
    }
    return -1;
}

/* Actor selector at `offset`, defaulting to the party leader. */
s32 func_8009CD7C(s32 offset) {
    s32 index;

    index = func_8009CDB4(offset);
    if (index == 0xFF) {
        return D_8005A444[0];
    }
    return index;
}

/* Actor selector at `offset`: 0xFF/0xFE/0xFD pick party slots, 0xFB the
 * current actor. */
s32 func_8009CDB4(s32 offset) {
    s32 index;

    index = D_800ADC00[D_800B0078->pc + offset];
    if (index == 0xFF) {
        index = D_8005A444[0];
    } else if (index == 0xFE) {
        index = D_8005A444[1];
    } else if (index == 0xFD) {
        index = D_8005A444[2];
    } else if (index == 0xFB) {
        index = D_800AFD1C;
    }
    return index;
}

/* Set the current actor's speech parameters from operand bytes. */
void func_8009CE48(void) {
    D_800B0078->unk88 = EVENT_OPERAND_BYTE(1) * 2;
    D_800B0078->unk8A = EVENT_OPERAND_BYTE(2);
    D_800B0078->unk82 = EVENT_OPERAND_BYTE(3) * 3;
    D_800B0078->unk83 = EVENT_OPERAND_BYTE(4);
    D_800B0078->pc += 5;
}

/* Set the current actor's speech parameters from operands. */
void func_8009CEE0(void) {
    D_800B0078->unk88 = func_800ACDEC(1);
    D_800B0078->unk8A = func_800ACDEC(3);
    D_800B0078->unk82 = func_800ACDEC(5) * 3;
    D_800B0078->unk83 = func_800ACDEC(7);
    D_800B0078->unk84 = func_800ACDEC(9);
    D_800B0078->pc += 11;
}

void func_8009CF70(void) {
}

/* Selected operand, immediate when flags bit 0x80 is set. */
s32 func_8009CF78(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x80) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x40 is set. */
s32 func_8009CFBC(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x40) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x20 is set. */
s32 func_8009D000(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x20) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x10 is set. */
s32 func_8009D044(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x10) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x08 is set. */
s32 func_8009D088(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x08) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x04 is set. */
s32 func_8009D0CC(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x04) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x02 is set. */
s32 func_8009D110(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x02) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x01 is set. */
s32 func_8009D154(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x01) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}


/* Store a random number in a variable. */
void func_8009D198(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference, rand());
    D_800B0078->pc += 3;
}

/* Store a random number in 0..operand in a variable. */
void func_8009D1F0(void) {
    s32 value;

    value = (rand() * (func_800ACDEC(3) + 1)) >> 15;
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 5;
}

/* Shift a variable right by an operand. */
void func_8009D260(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value = value >> func_800ACDEC(3);
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 5;
}

/* Shift a variable left by an operand. */
void func_8009D2D0(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value = value << func_800ACDEC(3);
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 5;
}

/* Increment a variable. */
void func_8009D340(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF) + 1;
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 3;
}

/* Decrement a variable. */
void func_8009D3A4(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF) - 1;
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 3;
}

/* Clear bit (selected operand) of a variable. */
void func_8009D408(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value &= ~(1 << func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* XOR a variable with a selected operand. */
void func_8009D4A0(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value ^= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* OR a variable with a selected operand. */
void func_8009D52C(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value |= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* AND a variable with a selected operand. */
void func_8009D5B8(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value &= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* Set bit (selected operand) of a variable. */
void func_8009D644(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value |= 1 << func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* Multiply a variable by a selected operand. */
void func_8009D6D8(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value *= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* Divide a variable by a selected operand (zero treated as one). */
void func_8009D768(void) {
    s32 value;
    s32 divisor;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    divisor = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    if (divisor == 0) {
        divisor = 1;
    }
    value /= divisor;
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* Subtract a selected operand from a variable. */
void func_8009D804(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value -= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* Add a selected operand to a variable. */
void func_8009D890(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value += func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1), value);
    D_800B0078->pc += 6;
}

/* Clear a variable. */
void func_8009D91C(void) {
    func_800A3074(func_800ACDB8(1), 0);
    D_800B0078->pc += 3;
}

/* Set a variable to one. */
void func_8009D960(void) {
    func_800A3074(func_800ACDB8(1), 1);
    D_800B0078->pc += 3;
}

/* Set a variable from a selected operand. */
void func_8009D9A4(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference, func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    D_800B0078->pc += 6;
}

/* Set actor flag 0x20000. */
void func_8009DA1C(void) {
    FieldActor *actor = D_800B0078;

    actor->flags |= 0x20000;
    actor->pc++;
}

/* Clear actor flag 0x20000. */
void func_8009DA44(void) {
    FieldActor *actor = D_800B0078;

    actor->flags &= ~0x20000;
    actor->pc++;
}

/* Set actor flag 0x800000. */
void func_8009DA70(void) {
    FieldActor *actor = D_800B0078;

    actor->flags |= 0x800000;
    actor->pc++;
}

/* Clear actor flag 0x800000. */
void func_8009DA98(void) {
    FieldActor *actor = D_800B0078;

    actor->flags &= ~0x800000;
    actor->pc++;
}

/* Hide a selected actor, disable its descriptor and release the current
 * actor's idle dialogue window. */
void func_8009DAC4(void) {
    FieldActor *actor;
    FieldDescriptor *descriptor;
    s32 window;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->flags |= 1;
        actor->layer_flags |= 0x100000;
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->flags |= 0x20;
        if (func_8009CD18(&window) == 0) {
            D_800C2698[window].cleared = 0;
        }
    }
    D_800B0078->pc += 2;
}

/* Show a selected actor again. */
void func_8009DBC8(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->flags &= ~1;
    }
    D_800B0078->pc += 2;
}

/* Stop and hide a selected actor and release the current actor's idle
 * dialogue window. */
void func_8009DC4C(void) {
    FieldActor *actor;
    u16 state;
    s32 window;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->unk030[0] = 0;
        actor->unk030[1] = 0;
        actor->unk030[2] = 0;
        actor->unk40[0] = 0;
        actor->unk40[1] = 0;
        actor->unk40[2] = 0;
        state = actor->heading | 0x8000;
        actor->flags |= 1;
        actor->heading_goal = state;
        actor->heading = state;
        if (func_8009CD18(&window) == 0) {
            D_800C2698[window].cleared = 0;
        }
    }
    D_800B0078->pc += 2;
}

/* Wait operand-1 frames (counted in the slot), yielding each frame. */
void func_8009DD34(void) {
    if (D_800B0078->slots[D_800B0078->slot].countdown == 0) {
        D_800B0078->slots[D_800B0078->slot].countdown = func_800ACDEC(1);
    } else {
        D_800B0078->slots[D_800B0078->slot].countdown--;
    }
    if (D_800B0078->slots[D_800B0078->slot].countdown == 0) {
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

/* Re-enable a selected, still visible actor's descriptor. */
void func_8009DDEC(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        if (!(descriptor->actor->layer_flags & 0x100000)) {
            descriptor->flags &= 0xFFDF;
            descriptor->actor->layer_flags &= ~0x2000000;
        }
    }
    D_800B0078->pc += 2;
}

/* Disable a selected actor's descriptor (flag 0x20). */
void func_8009DE94(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->flags |= 0x20;
    }
    D_800B0078->pc += 2;
}

/* Re-enable the current descriptor. */
void func_8009DF10(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags &= 0xFFDF;
    actor = D_800B0078;
    actor->unkE8 = 0xFF;
    actor->layer_flags &= ~0x2000000;
    actor->pc++;
}

/* Set layer flags 0x2000000 and 0x800 on a selected actor. */
void func_8009DF78(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->actor->layer_flags |= 0x2000000;
        descriptor->actor->layer_flags |= 0x800;
    }
    D_800B0078->pc += 2;
}

/* Set layer flags 0x2000000 and 0x800 on the current actor. */
void func_8009E014(void) {
    FieldActor *actor = D_800B0078;

    actor->layer_flags |= 0x2000800;
    actor->pc++;
}

/* Disable the current descriptor (flag 0x20). */
void func_8009E040(void) {
    FieldDescriptor *descriptor;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags |= 0x20;
    D_800B0078->pc += 1;
}

void func_80021BCC(FieldModel *model, u16 value);

/* Set the current actor's unk76 and pass it to its model. */
void func_8009E094(void) {
    s16 value;

    value = func_800ACDEC(1);
    D_800B0078->unk76 = value;
    func_80021BCC(D_800AF880.components.descriptors[D_800AFD1C].model, value);
    D_800B0078->pc += 3;
}

/* Map operand bits onto actor flags (1->0x80, 4->0x20, 8->0x10, 0x10->8,
 * 0x20->4, 0x40->0x8000000). */
void func_8009E10C(void) {
    s32 bits;
    s32 flags;

    bits = func_800ACDEC(1);
    flags = (bits & 1) << 7;
    if (bits & 4) {
        flags |= 0x20;
    }
    if (bits & 8) {
        flags |= 0x10;
    }
    if (bits & 0x10) {
        flags |= 8;
    }
    if (bits & 0x20) {
        flags |= 4;
    }
    if (bits & 0x40) {
        flags |= 0x8000000;
    }
    D_800B0078->flags = (D_800B0078->flags & 0xF7FFFF43) | flags;
    D_800B0078->pc += 3;
}

#include "field_actor_events.h"

#ifdef NON_MATCHING
void func_8009E1A0(void) {
    FieldActor *actor = D_800B0078;
    u8 *code = D_800ADC00;

    actor->layer_flags = (actor->layer_flags & ~7) | (code[actor->pc + 1] & 7);
    actor->layer_flags = (actor->layer_flags & ~0x38) | ((code[actor->pc + 1] >> 1) & 0x38);
    actor->pc += 2;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E1A0);
#endif

/* Enter mode 0x400000 (clearing 0x40000) from the current height. */
void func_8009E208(void) {
    FieldActor *actor = D_800B0078;
    s32 y;

    y = WHOLE(actor->position[1]);
    actor->unkEC = 0;
    actor->flags = (actor->flags & ~0x40000) | 0x400000;
    actor->pc++;
    actor->unk72 = y;
}

void func_8009E574(s32 x, s32 z);
void func_8009E810(s32 y);

/* Start a jump (func_8009E574 / func_8009E810 from signed operands). */
void func_8009E248(void) {
    s32 a;

    a = (s16)func_800ACD7C(1);
    func_8009E574(a, (s16)func_800ACD7C(3));
    func_8009E810((s16)func_800ACD7C(5));
    D_800B0078->flags |= 0x40000;
    D_800B0078->pc += 7;
}

/* Start func_8009E810 from a selected operand. */
void func_8009E2C8(void) {
    func_8009E810(func_8009CF78(1, EVENT_OPERAND_BYTE(3)));
    D_800B0078->flags |= 0x40000;
    D_800B0078->pc += 4;
}

/* Signed halfword of the bytecode at `offset`. */
s16 func_8009E330(s32 offset) {
    return D_800ADC00[offset] + (D_800ADC00[offset + 1] << 8);
}

/* Place the current actor on layer operand 5 at a selected x/z. */
void func_8009E35C(void) {
    s32 x;

    D_800B0078->layer = EVENT_OPERAND_BYTE(5);
    x = func_8009CF78(1, EVENT_OPERAND_BYTE(6));
    func_8009E574(x, func_8009CFBC(3, EVENT_OPERAND_BYTE(6)));
    D_800B0078->layer_flags &= ~0x200000;
    D_800B0078->flags &= ~0x10000;
    D_800B0078->pc += 7;
}

/* Move the current actor to layer operand 1 at its own x/z. */
void func_8009E428(void) {
    FieldActor *actor;

    D_800B0078->layer = EVENT_OPERAND_BYTE(1);
    actor = D_800AF880.components.descriptors[D_800AFD1C].actor;
    func_8009E574(WHOLE(actor->position[0]), WHOLE(actor->position[2]));
    D_800B0078->pc += 2;
}

/* Place the current actor at a selected x/z. */
void func_8009E4BC(void) {
    s32 x;

    x = func_8009CF78(1, EVENT_OPERAND_BYTE(5));
    func_8009E574(x, func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    D_800B0078->layer_flags &= ~0x200000;
    D_800B0078->flags &= ~0x10000;
    D_800B0078->pc += 6;
}

/* Place the current actor at integer (x, z) on the floor of its layer:
 * locate the floor triangle of every layer, take its terrain, normal and
 * height, move the descriptor and model there and clear the motion. */
void func_8009E574(s32 x, s32 z) {
    VECTOR normals[4];
    SVECTOR points[4];
    FieldModel *model;
    s32 layer;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    for (layer = 0; layer < D_800AF880.components.layer_count - 1; layer++) {
        D_800B0078->triangle[layer] = func_8007B1C4(x, z, layer, &points[layer], &normals[layer]);
    }
    D_800B0078->unk014 = func_80080968(D_800B0078);
    D_800B0078->unk50[0] = (normals + D_800B0078->layer)->vx;
    D_800B0078->unk50[1] = (normals + D_800B0078->layer)->vy;
    D_800B0078->unk50[2] = (normals + D_800B0078->layer)->vz;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[0] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = x;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[1] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = points[D_800B0078->layer].vy;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[2] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = z;
    model->unk84 = points[D_800B0078->layer].vy;
    D_800B0078->position[0] = x << 16;
    D_800B0078->position[1] = points[D_800B0078->layer].vy << 16;
    D_800B0078->position[2] = z << 16;
    D_800B0078->unk72 = points[D_800B0078->layer].vy;
    model->position[0] = D_800B0078->position[0];
    model->position[1] = D_800B0078->position[1];
    model->position[2] = D_800B0078->position[2];
    D_800B0078->unk40[0] = 0;
    D_800B0078->unk40[1] = 0;
    D_800B0078->unk40[2] = 0;
    D_800B0078->unk030[0] = 0;
    D_800B0078->unk030[1] = 0;
    D_800B0078->unk030[2] = 0;
    D_800B0078->target[0] = 0;
    D_800B0078->target[1] = 0;
    D_800B0078->target[2] = 0;
    *(s16 *)D_800B0078->unk062 = 0;
    D_800B0078->unk60 = 0;
    D_800B0078->unk64 = 0;
    model->unk0C = 0;
    model->unk10 = 0;
    model->unk14 = 0;
    *(s32 *)D_800B0078->unk0F0 = 0;
    D_800B0078->unkEC = 0;
    D_800B0078->unk72 = D_800B0078->position[1] >> 16;
    D_800B0078->flags = (D_800B0078->flags & ~0x40000) | 0x400000;
}

#ifdef NON_MATCHING
/* Set the current actor's height `y` (whole units). */
void func_8009E810(s32 y) {
    D_800B0078->position[1] = y << 16;
    D_800B0078->unkEC = y;
    D_800B0078->unk72 = y;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E810);
#endif

/* Set the current actor's extents from non-zero operand bytes (doubled). */
void func_8009E83C(void) {
    if (EVENT_OPERAND_BYTE(1) != 0) {
        D_800B0078->unk18 = EVENT_OPERAND_BYTE(1) * 2;
    }
    if (EVENT_OPERAND_BYTE(2) != 0) {
        D_800B0078->gravity.s.fraction = EVENT_OPERAND_BYTE(2) * 2;
    }
    if (EVENT_OPERAND_BYTE(3) != 0) {
        D_800B0078->height = EVENT_OPERAND_BYTE(3) * 2;
    }
    if (EVENT_OPERAND_BYTE(4) != 0) {
        D_800B0078->gravity.s.whole = EVENT_OPERAND_BYTE(4) * 2;
    }
    D_800B0078->pc += 5;
}

/* Event: give the current actor a boundary quadrilateral (+114, allocated
 * once and marked by state bit 12) of four selected x/z corners. */
void func_8009E91C(void) {
    if (!(D_800B0078->state.word & 0x1000)) {
        D_800B0078->unk114 = func_80031BDC(0x10, 0);
    }
    D_800B0078->state.word |= 0x1000;
    ((ActorBoundary *)D_800B0078->unk114)->corners[0].x = func_8009CF78(1, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[0].z = func_8009CFBC(3, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[1].x = func_8009D000(5, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[1].z = func_8009D044(7, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[2].x = func_8009D088(9, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[2].z = func_8009D0CC(11, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[3].x = func_8009D110(13, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[3].z = func_8009D154(15, EVENT_OPERAND_BYTE(17));
    D_800B0078->pc += 18;
}

/* -1 when one of the actor's slots carries event tag `tag`, else 0. */
s32 func_8009EB48(FieldActor *actor, s32 tag) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (tag == actor->slots[i].tag) {
            return -1;
        }
    }
    return 0;
}

/* Event: request event (tag low five bits of operand 2, priority its high
 * three) on a selected actor, in its first free slot; retried while none is
 * free. An actor in a request handshake (+04 bit 20) instead releases both
 * linked slots. */
void func_8009EB78(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (func_8009CDB4(1) != 0xFF) {
        index = func_8009CDB4(1);
        other = D_800AF880.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            D_800B0078->slots[D_800B0078->slot].unk16 = 0;
            other->slots[D_800B0078->unk0CF].unk22 = 0;
        } else if (func_8009EB48(other, EVENT_OPERAND_BYTE(2) & 0x1F) != -1) {
            for (i = 0; i < 8; i++) {
                if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                    other->slots[i].resume_pc = func_800A3090(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                    other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                    other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                    goto done;
                }
            }
            return;
        }
    }
done:
    D_800B0078->pc += 3;
}

/* Event: request event operand 2 on a selected actor and wait until it has
 * started: phase 0 (the current slot's bits 16-17) requests it, linking the
 * two slots through +cf, and phase 1 waits for the target to run it. */
void func_8009ED68(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (func_8009CDB4(1) != 0xFF) {
        index = func_8009CDB4(1);
        other = D_800AF880.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            D_800B0078->slots[D_800B0078->slot].unk16 = 0;
            other->slots[D_800B0078->unk0CF].unk22 = 0;
        } else {
            switch (D_800B0078->slots[D_800B0078->slot].unk16) {
            case 0:
                if (func_8009EB48(other, EVENT_OPERAND_BYTE(2) & 0x1F) == -1) {
                    break;
                }
                for (i = 0; i < 8; i++) {
                    if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                        other->slots[i].resume_pc = func_800A3090(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                        other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                        other->slots[D_800B0078->unk0CF].unk22 = 1;
                        other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                        D_800B0078->unk0CF = i;
                        D_800B0078->slots[D_800B0078->slot].unk16 = 1;
                        return;
                    }
                }
                return;
            case 1:
                if (other->slot == D_800B0078->unk0CF || other->slots[D_800B0078->unk0CF].priority == 0xF) {
                    D_800B0078->pc += 3;
                    D_800B0078->slots[D_800B0078->slot].unk16 = 0;
                    other->slots[D_800B0078->unk0CF].unk22 = 0;
                    return;
                }
                D_800B00C0 = 1;
                return;
            default:
                return;
            }
        }
    }
    D_800B0078->pc += 3;
}

/* Event: request event operand 2 on a selected actor and wait until it has
 * started (phase 1 of the current slot's bits 16-17) and then finished
 * (phase 2, its slot free again). */
void func_8009F0A0(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (func_8009CDB4(1) != 0xFF) {
        index = func_8009CDB4(1);
        other = D_800AF880.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            D_800B0078->slots[D_800B0078->slot].unk16 = 0;
            other->slots[D_800B0078->unk0CF].unk22 = 0;
        } else {
            switch (D_800B0078->slots[D_800B0078->slot].unk16) {
            case 0:
                if (func_8009EB48(other, EVENT_OPERAND_BYTE(2) & 0x1F) == -1) {
                    break;
                }
                for (i = 0; i < 8; i++) {
                    if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                        other->slots[i].resume_pc = func_800A3090(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                        other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                        other->slots[D_800B0078->unk0CF].unk22 = 1;
                        D_800B0078->unk0CF = i;
                        D_800B0078->slots[D_800B0078->slot].unk16 = 1;
                        other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                        return;
                    }
                }
                return;
            case 1:
                if (other->slot == D_800B0078->unk0CF || other->slots[D_800B0078->unk0CF].priority == 0xF) {
                    D_800B0078->slots[D_800B0078->slot].unk16 = 2;
                    return;
                }
                D_800B00C0 = 1;
                return;
            case 2:
                if (other->slots[D_800B0078->unk0CF].priority == 0xF) {
                    D_800B0078->slots[D_800B0078->slot].unk16 = 0;
                    other->slots[D_800B0078->unk0CF].unk22 = 0;
                    D_800B0078->pc += 3;
                    return;
                }
                D_800B00C0 = 1;
                return;
            default:
                return;
            }
        }
    }
    D_800B0078->pc += 3;
}

/* Wander: every 16 frames turn the facing target by +/- an octant. */
void func_8009F424(void) {
    s32 facing;

    facing = D_800B0078->heading_goal;
    if ((++D_800B0078->unk102 & 0xF) == 0) {
        if (!(rand() & 1)) {
            facing = (D_800B0078->heading_goal + 0x200) & 0xFFF;
        } else {
            facing = (D_800B0078->heading_goal - 0x200) & 0xFFF;
        }
    }
    D_800B00C0 = 1;
    D_800B0078->heading = facing;
    D_800B0078->pc += 1;
}

/* Wander with pauses: as func_8009F424, sometimes holding the facing. */
void func_8009F4CC(void) {
    s32 facing;
    s32 random;

    facing = D_800B0078->heading_goal;
    if ((++D_800B0078->unk102 & 0xF) == 0) {
        random = rand();
        if (random & 0x30) {
            facing = D_800B0078->heading_goal |= 0x8000;
        } else if (!(random & 1)) {
            facing = (D_800B0078->heading_goal + 0x200) & 0xFFF;
        } else {
            facing = (D_800B0078->heading_goal - 0x200) & 0xFFF;
        }
    }
    D_800B00C0 = 1;
    D_800B0078->heading = facing;
    D_800B0078->pc += 1;
}

void func_8009F5F4(void);

/* Run player control for this frame and repeat this opcode. */
void func_8009F5A8(void) {
    u16 pc;

    pc = D_800B0078->pc;
    func_8009F5F4();
    D_800B00C0 = 1;
    D_800B0078->pc = pc;
}

/* Event a7: player control. With dialogue closed and encounters allowed,
 * poll the pad, count frames stuck against terrain, start a jump (0x800)
 * on the jump button or after 32 stuck frames, and face the d-pad
 * direction relative to the camera (0x8000 when none). Non-player actors
 * are marked 0x1000000 instead. */
void func_8009F5F4(void) {
    u8 unused[0x48]; /* the original frame holds 0x48 unused bytes */
    s32 i;
    s32 idle;
    s32 direction = 0;

    if (D_800B0078->flags & 0x4000) {
        for (i = 0; i < 4; i++) {
            if (D_800C2698[i].status == 0) {
                break;
            }
        }
        idle = (i == 4) ? -1 : 0;
        if (idle == -1 && D_800B2078.encounter_inhibition == 0) {
            if (D_800AFE9C >> 12) {
                func_80079288();
            }
            D_800ADB68 = 1;
            if (D_800B0078->unk014 & 0x400000) {
                if (ACTOR_CACHED_POSITION(D_800B0078)[0] == WHOLE(D_800B0078->position[0])
                    && ACTOR_CACHED_POSITION(D_800B0078)[1] == WHOLE(D_800B0078->position[1])
                    && ACTOR_CACHED_POSITION(D_800B0078)[2] == WHOLE(D_800B0078->position[2])) {
                    D_800ADB02++;
                }
            } else {
                D_800ADB02 = 0;
            }
            if (D_800ADB02 > 32 && (D_800ADB02 = 32, D_800AFE9C & 0x80) && !(D_800B0078->flags & 0x1800) && D_800ADB64 == 0xFF) {
                goto jump;
            }
            if (D_800B2078.jump_mode == 0) {
                if ((D_800C2694 & 0x80) && !(D_800B0078->flags & 0x1800) && !(D_800B0078->unk014 & 0x400000) && D_800ADB64 == 0xFF) {
                jump:
                    if (func_80081F5C(D_800B0078) == 0) {
                        D_800B0078->flags |= 0x800;
                        D_800ADB28 = D_800B2078.unk2360;
                    }
                }
            } else {
                if (D_800C2694 & 0x80) {
                    if (D_800B2078.repeat_remaining != 0) {
                        goto count;
                    }
                    if (D_800ADB64 == 0xFF && func_80081F5C(D_800B0078) == 0) {
                        D_800B0078->flags |= 0x800;
                        D_800ADB28 = D_800B2078.unk2360;
                        D_800B0078->unkE8 = 0xFF;
                        D_800B2078.repeat_remaining = D_800B2078.repeat_delay;
                    }
                }
                if (D_800B2078.repeat_remaining != 0) {
                count:
                    D_800B2078.repeat_remaining--;
                }
            }
            if (D_800B2078.unk2354 == 0) {
                direction = D_800ADF68[(D_800AFE9C >> 12) ^ 0xF];
            } else {
                direction = D_800ADF88[(D_800AFE9C >> 12) ^ 0xF];
            }
            if (!(direction & 0x8000)) {
                direction = (direction - D_800AF880.angle) & 0xFFF;
            }
            D_800B0078->heading = direction;
        } else {
            D_800B0078->heading = direction | 0x8000;
        }
    } else if (D_800B2078.preserve_nonplayer_motion == 0) {
        D_800B0078->flags |= 0x1000000;
    }
    D_800B0078->pc += 1;
}

/* Party slot of character `id`, or -1. */
s32 func_8009FA00(s32 id) {
    s32 i;

    if (id == 0xFF) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (D_80062590[i] == 0xFF) {
            return -1;
        }
        if (D_80062590[i] == id) {
            return i;
        }
    }
    return -1;
}

s16 func_8009E330(s32 offset);


#ifdef NON_MATCHING
/* Place the current actor at entry point `entry` of the bytecode's entry
 * table (when present): layer, x/z, camera octant and facing (0xFF: from
 * variables 8 and 6). */
s32 func_8009FA54(s32 entry) {
    s32 marker;
    s32 record;
    s32 x;
    s16 heading;
    s32 facing;

    marker = D_800ADC00[0];
    if (marker == 0xFF) {
        record = entry * 7;
        D_800B0078->layer = D_800ADC00[record + 5];
        x = func_8009E330(record + 1);
        func_8009E574(x, func_8009E330(record + 3));
        heading = ((D_800ADC00[record + 6] + 4) & 7) << 9;
        if (D_800ADC00[record + 6] == marker) {
            heading = ((func_800A3018(8) + 4) & 7) << 9;
        }
        D_800AF880.heading_angles.vy = heading;
        D_800AF880.heading = heading;
        D_800AF880.heading_high = heading << 16;
        facing = (((D_800ADC00[record + 7] - 2) & 7) << 9) | 0x8000;
        if (D_800ADC00[record + 7] == marker) {
            facing = (((func_800A3018(6) - 2) & 7) << 9) | 0x8000;
        }
        D_800B0078->heading = facing;
        D_800B0078->heading_goal = facing;
        D_800B0078->unk108 = facing;
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FA54);
#endif

extern s32 D_8004F34C;
void func_8001AD1C(void);
void func_8001B044(void);
void func_8001B3A8(void);

/* Mark the field id (0xC000) and reset the resident services; record
 * operand byte 1 in D_800B2268. */
void func_8009FB98(void) {
    D_8004F34C |= 0xC000;
    func_8001AD1C();
    func_8001B044();
    func_8001B3A8();
    D_800B2078.unk2268 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

extern s32 D_8006F990[3];

/* Party slot whose field actor is `index`, or 0xFF. */
s32 func_8009FC10(s32 index) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8006F990[i] == index) {
            return i;
        }
    }
    return 0xFF;
}

void func_8009FD10(s32 slot);

/* Set party slot flag (unk22B1) for slot operand 1 (max 2). */
void func_8009FC48(void) {
    s32 slot;

    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    D_8005A39C->unk22B1[slot] = 1;
    func_8009FD10(slot);
    D_800B0078->pc += 3;
}

/* Clear party slot flag (unk22B1) for slot operand 1 (max 2). */
void func_8009FCAC(void) {
    s32 slot;

    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    D_8005A39C->unk22B1[slot] = 0;
    func_8009FD10(slot);
    D_800B0078->pc += 3;
}

/* Record the field id in the slot's variable triple and clear the rest. */
void func_8009FD10(s32 slot) {
    switch (slot) {
    case 0:
        func_800A3074(0x2A, D_8004F34C & 0xFFF);
        func_800A3074(0x2C, 0);
        func_800A3074(0x2E, 0);
        break;
    case 1:
        func_800A3074(0x30, D_8004F34C & 0xFFF);
        func_800A3074(0x32, 0);
        func_800A3074(0x34, 0);
        break;
    case 2:
        func_800A3074(0x36, D_8004F34C & 0xFFF);
        func_800A3074(0x38, 0);
        func_800A3074(0x3A, 0);
        break;
    }
}

void func_800AD4D4(s32 slot);

/* Run func_800AD4D4 for the current actor's party slot unless flagged. */
void func_8009FDD4(void) {
    s32 slot;

    slot = func_8009FC10(D_800AFD1C);
    if (slot != 0xFF && D_8005A39C->unk22B1[slot] == 0) {
        func_800AD4D4(slot);
    }
    D_800B0078->pc += 1;
}

void func_800ACFD0(s32 slot);

/* Run func_800ACFD0 for party slot operand 1 when occupied and flagged. */
void func_8009FE4C(void) {
    u8 slot;

    slot = EVENT_OPERAND_BYTE(1);
    if (D_80062590[slot] != 0xFF && D_8005A39C->unk22B1[slot] != 0) {
        func_800ACFD0(slot);
    }
    D_800B0078->pc += 2;
}

/* Record party slot `slot`'s map (its layer in bits 14 up) and integer x/z
 * in event variables 2a/2c/2e, 30/32/34 or 36/38/3a. Declared int without
 * a return value: the original keeps $v0 live on exit. */
s32 func_8009FEE4(s32 slot) {
    s32 layer;

    if (D_8005A444[slot] != 0xFF) {
        layer = D_800AF880.components.descriptors[D_8005A444[slot]].actor->layer << 14;
        switch (slot) {
        case 0:
            func_800A3074(0x2A, (D_8004F34C & 0xFFF) | layer);
            func_800A3074(0x2C, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[0]));
            func_800A3074(0x2E, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[2]));
            break;
        case 1:
            func_800A3074(0x30, (D_8004F34C & 0xFFF) | layer);
            func_800A3074(0x32, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[0]));
            func_800A3074(0x34, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[2]));
            break;
        case 2:
            func_800A3074(0x36, (D_8004F34C & 0xFFF) | layer);
            func_800A3074(0x38, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[0]));
            func_800A3074(0x3A, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[2]));
            break;
        }
    }
}

/* Read party slot `slot`'s variable triple (see func_8009FD10). */
void func_800A0158(s32 slot, s32 *a, s32 *b, s32 *c) {
    switch (slot) {
    case 0:
        *a = func_800A3018(0x2A);
        *b = func_800A3018(0x2C);
        *c = func_800A3018(0x2E);
        break;
    case 1:
        *a = func_800A3018(0x30);
        *b = func_800A3018(0x32);
        *c = func_800A3018(0x34);
        break;
    case 2:
        *a = func_800A3018(0x36);
        *b = func_800A3018(0x38);
        *c = func_800A3018(0x3A);
        break;
    }
}

/* Event 5c: the current actor becomes party slot operand 1 (at most 2):
 * take its member's sprite and shown it at the slot's recorded map
 * position (variables 2a..3a) when that is this map; with no member, show
 * the field's first sprite instead. Members away from this map stay hidden
 * (descriptor flag 0x20). */
void func_800A0228(void) {
    FieldDescriptor *descriptor;
    s32 slot;
    s32 shown;
    s32 map;
    s32 x;
    s32 z;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    shown = 1;
    D_8006F990[slot] = D_800AFD1C;
    if (D_80062590[slot] != 0xFF && func_8001ACF0(D_80062590[slot]) != 0xFF) {
        func_800A0158(slot, &map, &x, &z);
        D_800B0078->layer = (map >> 14) & 3;
        if ((D_8004F34C & 0xFFF) != (map & 0x3FFF)) {
            shown = 0;
            x = 0;
            z = 0;
            D_800B0078->layer = 0;
        }
        if (D_8005A39C->unk22B1[slot] != 0) {
            shown = 0;
        } else if (D_80062590[slot] == 7) {
            shown = 0;
        }
        descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
        func_80076AC0(D_800AFD1C, slot, D_8005A414[slot], 1, 0, slot, 1);
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
        if ((D_8004F34C & 0xFFF) != (map & 0x3FFF)) {
            D_800B0078->layer = 0;
        }
        func_8009E574(x, z);
        func_800A0C94();
        D_800B0078->flags = (D_800B0078->flags | 0x400) & ~0x300;
        if (shown == 0) {
            D_800AF880.components.descriptors[D_800AFD1C].flags |= 0x20;
        }
    } else {
        func_800A0D3C();
        D_800B0078->pc += 2;
        D_800B0078->layer_flags |= 0x800;
        return;
    }
    if (D_800AF880.components.layer_count - 1 < D_800B0078->layer) {
        D_800B0078->layer = 0;
    }
    D_800B0078->flags |= 0x20000;
    D_800B0078->layer_flags |= 0xC00;
    D_800B0078->pc += 3;
}

/* Copy actor `from`'s collision state, height, +50 words, position and
 * matrix to actor `to` and move `to`'s model to it. */
void func_800A0524(s32 to, s32 from) {
    FieldActor *target;
    FieldActor *source;
    s32 i;

    target = D_800AF880.components.descriptors[to].actor;
    source = D_800AF880.components.descriptors[from].actor;
    for (i = 0; i < 4; i++) {
        target->triangle[i] = source->triangle[i];
    }
    target->layer = source->layer;
    target->unkEC = source->unkEC;
    target->unk72 = source->unk72;
    target->unk50[0] = source->unk50[0];
    target->unk50[1] = source->unk50[1];
    target->unk50[2] = source->unk50[2];
    target->position[0] = source->position[0];
    target->position[1] = source->position[1];
    target->position[2] = source->position[2];
    func_8007409C(&D_800AF880.components.descriptors[to].matrix, &D_800AF880.components.descriptors[from].matrix);
    func_80074078(&D_800AF880.components.descriptors[to].matrix, &D_800AF880.components.descriptors[from].matrix);
    D_800AF880.components.descriptors[to].model->position[0] = D_800AF880.components.descriptors[from].actor->position[0];
    D_800AF880.components.descriptors[to].model->position[1] = D_800AF880.components.descriptors[from].actor->position[1];
    D_800AF880.components.descriptors[to].model->position[2] = D_800AF880.components.descriptors[from].actor->position[2];
}

extern s32 D_800AFFEC;
s32 func_8009FA00(s32 character);
extern s16 D_800AFD20;

/* Give the current actor the sprite of party member operand 1, or hide it
 * (flag 1, layer flag 0x100000) and end its script when absent. */
void func_800A06E8(void) {
    FieldDescriptor *descriptor;
    s32 slot;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    slot = func_8009FA00(func_8008CF3C(func_800ACDEC(1)));
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    if (slot != -1) {
        func_80076AC0(D_800AFD1C, slot, D_8005A414[slot], 2, 0, slot, 1);
        D_800AFD20 = -0xC0;
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
        func_800A0C94();
        D_800B0078->flags = (D_800B0078->flags | 0x100) & ~0x80;
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
    } else {
        func_80076AC0(D_800AFD1C, 0, D_8005A414[0], 1, 0, 0, 1);
        D_800B0078->flags |= 1;
        D_800AFFEC = 1;
        D_800B00C0 = 1;
        D_800B0078->layer_flags |= 0x100000;
    }
    D_800B0078->pc += 3;
}

#ifdef NON_MATCHING
/* Event 16: the current actor becomes party character operand 1 (ff, fe, fd: party slots 2, 1, 0). A party member takes its slot (slot 0 becomes the controlled actor), its sprite (or sprite 800ae294[character] of the alternate set 800b2268) and map entry variable 2; others hide and end their script. The sprite-table address keeps 800b2268 in a different register order (a1*4+4 first). */
void func_800A08B8(void) {
    FieldDescriptor *descriptor;
    s32 character;
    s32 slot;
    FieldModel *model;
    s32 *sprites;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    character = func_8008CF3C(func_800ACDEC(1));
    slot = func_8009FA00(character);
    D_800B0078->unkE4 = character;
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    if (slot != -1) {
        if (slot == 0) {
            D_800B2078.controlled = D_800AFD1C;
            D_800B2078.unk233E = D_800AFD1C;
            D_800B0078->flags = (D_800B0078->flags | 0x4400) & ~0x80;
        }
        D_8005A444[slot] = D_800AFD1C;
        if (D_800B2078.unk2268 != 0) {
            sprites = D_800AF880.components.sprites;
            func_80076AC0(D_800AFD1C, D_800AE294[character] + D_800B2078.unk2268,
                          (u8 *)(sprites[D_800AE294[character] + D_800B2078.unk2268 + 1] + (s32)sprites),
                          0, 0, (D_800AE294[character] + D_800B2078.unk2268) | 0x80, 1);
            D_800B0078->flags = (D_800B0078->flags | 0x400) & ~0x300;
            if (D_8005A39C->unk22B1[slot] != 0) {
                model = D_800AF880.components.descriptors[D_800AFD1C].model;
                D_800AF880.components.descriptors[D_800AFD1C].model = D_800AF880.components.descriptors[D_8006F990[slot]].model;
                D_800AF880.components.descriptors[D_8006F990[slot]].model = model;
                D_800B0078->flags = (D_800B0078->flags | 0x200) & ~0x500;
            }
        } else {
            func_80076AC0(D_800AFD1C, slot, D_8005A414[slot], 1, 0, slot, 1);
            D_800B0078->flags = (D_800B0078->flags | 0x400) & ~0x300;
        }
        D_800AFD20 = -0xC0;
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
        func_8009FA54(func_800A3018(2));
        func_800A0C94();
        D_800B0078->layer_flags &= ~0x800;
    } else {
        func_80076AC0(D_800AFD1C, 0, D_8005A414[0], 1, 0, 0, 1);
        D_800B0078->flags |= 1;
        D_800AFFEC = 1;
        D_800B00C0 = 1;
        D_800B0078->layer_flags |= 0x100000;
    }
    D_800B0078->flags |= 0x20000;
    D_800B0078->layer_flags |= 0x400;
    D_800B0078->pc += 3;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A08B8);
#endif

/* Set flag 0x80 on the controlled actor. */
void func_800A0C4C(void) {
    FieldActor *player;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    player->flags |= 0x80;
}

/* Mirror the current actor's position into its descriptor and model. */
void func_800A0C94(void) {
    FieldModel *model;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[0] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] =
        WHOLE(D_800B0078->position[0]);
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[1] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] =
        WHOLE(D_800B0078->position[1]);
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[2] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] =
        WHOLE(D_800B0078->position[2]);
    model->position[0] = D_800B0078->position[0];
    model->position[1] = D_800B0078->position[1];
    model->position[2] = D_800B0078->position[2];
    model->unk10 = 0;
    D_800B0078->unk72 = model->unk84 = WHOLE(D_800B0078->position[1]);
}


/* Give the current actor the field's first sprite and show it. */
void func_800A0D3C(void) {
    FieldActor *actor;
    s32 *sprites;

    sprites = D_800AF880.components.sprites;
    func_80076AC0(D_800AFD1C, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 1);
    func_800A0C94();
    actor = D_800B0078;
    actor->flags |= 0x100;
    actor->layer_flags |= 0x800;
    actor->pc++;
}

/* Set D_800B2078.unk234A (mode block unk34A) from an operand. */
void func_800A0DC0(void) {
    D_800B2078.unk234A = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

s32 func_80028530(void);

/* Store resident 80028530() in a variable. */
void func_800A0DFC(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference, func_80028530());
    D_800B0078->pc += 3;
}


/* Yield; advance once D_800ADB74 is zero. */
void func_800A0E54(void) {
    if (D_800ADB74 == 0) {
        D_800B0078->pc += 1;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

extern s32 D_800ADB84;

/* Count D_800ADB84 up and yield. */
void func_800A0EB0(void) {
    D_800B00C0 = 1;
    D_800ADB84 += 1;
    D_800B0078->pc += 1;
}

extern u8 *D_801E8670[]; /* 801e module layers; +34 is a byte flag */
extern void func_801E8030(s32 layer);

/* Close the current actor's 801e layer: mode 0 clears its flag, mode 1
 * releases it. Yields. */
void func_800A0EE8(void) {
    FieldActor *actor;
    s32 layer;

    actor = D_800B0078;
    layer = actor->state.bits.layer;
    actor->layer_flags &= ~0x2000;
    switch (D_800ADC00[actor->pc + 1]) {
    case 0:
        D_801E8670[layer][0x34] = 0;
        D_800B0078->pc += 2;
        break;
    case 1:
        func_801E8030(actor->state.bits.layer);
        D_800B2078.unk2264--;
        D_800B0078->pc += 2;
        break;
    }
    D_800B00C0 = 1;
}

#ifdef NON_MATCHING
void func_800A0FD8(void) {
    s32 layer;

    if (D_800ADB2C != 0 || func_8008A558() != 0) {
        D_800B00C0 = 1;
        D_800B0078->pc--;
        return;
    }
    layer = D_800B0078->state.bits.layer;
    D_800B0078->layer_flags &= ~0x2000;
    func_80028470(4, 0);
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        D_801E8670[layer][0x34] = 0;
        D_800B0078->pc += 2;
        break;
    case 1:
        func_801E8030(D_800B0078->state.bits.layer);
        D_800B2078.unk21DC[layer] = func_800ACDEC(5) * 2;
        D_800B2394[0].file = D_800B2078.unk21DC[layer] + 0x6BA;
        D_800B2394[0].destination = D_8005A420[layer] = func_80031BDC(func_800288EC(D_800B2078.unk21DC[layer] + 0x6BA), 0);
        D_800B2394[1].file = D_800B2078.unk21DC[layer] + 0x6BB;
        D_800B2394[1].destination = D_8005A450[layer] = func_80031BDC(func_800288EC(D_800B2078.unk21DC[layer] + 0x6BB), 1);
        D_800B2394[2].file = 0;
        D_800B2394[2].destination = 0;
        func_80029AFC(D_800B2394, 0, 0);
        D_800B0078->pc += 2;
        break;
    case 2:
        if (func_80028A60(1) != 0) {
            D_800B0078->pc--;
            break;
        }
        func_800ACDEC(2);
        func_801E742C(layer, 0, D_8005A420[layer], D_8005A450[layer],
                      0x240 - ((layer + D_800B2078.unk225F[layer]) << 6), 0x100, 0, layer + 0xFC,
                      LAYER_STATE.motion[layer]);
        LAYER_STATE.scale[layer] = ((EffectLayer *)D_801E8670[layer])->scale;
        func_800320E8(D_8005A450[layer]);
        D_800B0078->pc += 4;
        D_800B0078->layer_flags |= 0x2000;
        ((EffectLayer *)D_801E8670[layer])->scale = (D_800B0078->scale[0] * 5) >> 6;
        ((EffectLayer *)D_801E8670[layer])->y = D_800B0078->position[1] >> 16;
        ((EffectLayer *)D_801E8670[layer])->model->x = WHOLE(D_800B0078->position[0]);
        ((EffectLayer *)D_801E8670[layer])->model->z = WHOLE(D_800B0078->position[2]);
        break;
    }
    D_800B00C0 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0FD8);
#endif

/* Give the current actor the field's first sprite on the next free 801e
 * layer (operand 1: layer parameter) and show it. */
void func_800A1364(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 *sprites;
    s32 value;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    value = func_800ACDEC(1);
    sprites = D_800AF880.components.sprites;
    func_80076AC0(D_800AFD1C, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 1);
    func_800A0C94();
    actor = D_800B0078;
    actor->pc += 3;
    actor->flags |= 0x100;
    D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
    actor->layer_flags = (actor->layer_flags | 0x2000) & ~0x800;
    D_800B2078.unk21DC[D_800B2078.unk2264] = value * 2;
    D_800B2078.unk225F[D_800B2078.unk2264] = 0;
    D_800B0078->state.bits.layer = D_800B2078.unk2264;
    D_800B2078.unk2264++;
}

/* Give the current actor sprite operand 1 (parameter operand 3), mirror its
 * position and show it. */
void func_800A14F0(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 sprite;
    s32 *sprites;
    u8 *data;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    sprite = func_800ACDEC(1);
    sprites = D_800AF880.components.sprites;
    data = (u8 *)(sprites[sprite + 1] + (s32)sprites);
    func_80076AC0(D_800AFD1C, sprite, data, 0, func_800ACDEC(3), sprite | 0x80, 1);
    func_800A0C94();
    actor = D_800B0078;
    actor->pc += 5;
    actor->flags = (actor->flags | 0x100) & ~0x80;
    actor->layer_flags &= ~0x800;
    D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
}

/* As func_800A14F0 with parameter 0. */
void func_800A1624(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 sprite;
    s32 *sprites;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    sprite = func_800ACDEC(1);
    sprites = D_800AF880.components.sprites;
    func_80076AC0(D_800AFD1C, sprite, (u8 *)(sprites[sprite + 1] + (s32)sprites), 0, 0, sprite | 0x80, 0);
    func_800A0C94();
    actor = D_800B0078;
    actor->pc += 3;
    actor->flags = (actor->flags | 0x100) & ~0x80;
    actor->layer_flags &= ~0x800;
    D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
}

extern char D_8006FD44[]; /* "STACKERR ACT=%d\n" */
extern void func_800379C8(char *format, ...);

/* Call the script at operand 1, pushing the return PC (after the 5-byte
 * instruction); with the four-entry call stack full, report and yield. */
void func_800A1730(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 5;
        D_800B0078->pc = func_800ACDB8(1);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | ((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (D_800C268C == 0) {
            func_800379C8(D_8006FD44, D_800AFD1C);
        }
        D_800B00C0 = 1;
    }
}

/* As func_800A1730 for a 3-byte instruction. */
void func_800A17F4(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 3;
        D_800B0078->pc = func_800ACDB8(1);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | ((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (D_800C268C == 0) {
            func_800379C8(D_8006FD44, D_800AFD1C);
        }
        D_800B00C0 = 1;
    }
}

extern s32 D_800AFFEC;

/* Return from a script call; with the call stack empty, report, end the
 * current script slot (priority 15, tag 0xff) and yield. */
void func_800A18B8(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) == 0) {
        if (D_800C268C == 0) {
            func_800379C8(D_8006FD44, D_800AFD1C);
        }
        D_800B0078->slots[D_800B0078->slot].priority = 15;
        D_800B0078->slots[D_800B0078->slot].tag = 0xFF;
        D_800AFFEC = 1;
        D_800B00C0 = 1;
    } else {
        actor->state.word = (actor->state.word & ~0x1C0) | ((((actor->state.word >> 6) & 7) - 1) & 7) << 6;
        actor->pc = actor->call_stack[(actor->state.word >> 6) & 7];
    }
}

/* Reset the current actor's eight script slots and call stack; yields. */
void func_800A19B0(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        D_800B0078->slots[i].countdown = 0;
        D_800B0078->slots[i].unk16 = 0;
        D_800B0078->slots[i].priority = 15;
        D_800B0078->slots[i].resume_pc = 0xFFFF;
        D_800B0078->slots[i].unk22 = 0;
        D_800B0078->slots[i].tag = 0xFF;
        D_800B0078->slots[i].value = 0xFFFF;
        D_800B0078->slots[i].move_mode = 0;
    }
    D_800B0078->slot = 0;
    D_800B0078->unk0CF = 0;
    D_800B00C0 = 1;
    D_800B0078->unk84 = 0;
    D_800B0078->state.bits.depth = 0;
}

s32 func_800A3090(s32 actor, s32 event);

/* Point every priority-7 script slot at the actor's script 1, end the
 * current slot and yield. */
void func_800A1A8C(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800B0078->slots[i].priority == 7) {
            D_800B0078->slots[i].resume_pc = func_800A3090(D_800AFD1C, 1);
        }
    }
    D_800B0078->slots[D_800B0078->slot].priority = 15;
    D_800B0078->slots[D_800B0078->slot].tag = 0xFF;
    D_800B00C0 = 1;
}

extern s32 D_800AFFEC;

/* End the current script slot and yield. */
void func_800A1B70(void) {
    D_800B0078->slots[D_800B0078->slot].priority = 15;
    D_800B0078->slots[D_800B0078->slot].tag = 0xFF;
    D_800AFFEC = 1;
    D_800B00C0 = 1;
}

INCLUDE_RODATA(".local/decomp/field/asm/nonmatchings/field", D_8006FD44);

/* Event 02: compare two halfword operands (bits 7/6 of operand byte 5
 * select an event variable or a signed immediate; variables compare
 * unsigned when flagged so) by condition bits 0-3 of byte 5, and jump to
 * operand 6 unless it holds. */
void func_800A1BD0(void) {
    s32 left;
    s32 right;
    s32 result;

    right = 0;
    left = 0;
    switch (EVENT_OPERAND_BYTE(5) & 0xF0) {
    case 0x00:
        left = func_800A3018(func_800ACDB8(1) & 0xFFFF);
        right = func_800A3018(func_800ACDB8(3) & 0xFFFF);
        if (func_800A2FE0(func_800ACDB8(1) & 0xFFFF) != 0) {
            right &= 0xFFFF;
        } else {
            right = (s16)right;
        }
        break;
    case 0x40:
        left = func_800A3018(func_800ACDB8(1) & 0xFFFF);
        right = (s16)func_800ACD7C(3);
        if (func_800A2FE0(func_800ACDB8(1) & 0xFFFF) != 0) {
            right &= 0xFFFF;
        }
        break;
    case 0x80:
        left = (s16)func_800ACD7C(1);
        right = func_800A3018(func_800ACDB8(3) & 0xFFFF);
        if (func_800A2FE0(func_800ACDB8(3) & 0xFFFF) != 0) {
            left &= 0xFFFF;
        }
        break;
    case 0xC0:
        left = (s16)func_800ACD7C(1);
        right = (s16)func_800ACD7C(3);
        break;
    }
    result = 0;
    switch (EVENT_OPERAND_BYTE(5) & 0xF) {
    case 0:
        if (left == right) {
            result++;
        }
        break;
    case 1:
        if (left != right) {
            result++;
        }
        break;
    case 2:
        if (left > right) {
            result++;
        }
        break;
    case 3:
        if (left < right) {
            result++;
        }
        break;
    case 4:
        if (left >= right) {
            result++;
        }
        break;
    case 5:
        if (left <= right) {
            result++;
        }
        break;
    case 6:
        if (left & right) {
            result++;
        }
        break;
    case 7:
        if (left != right) {
            result++;
        }
        break;
    case 8:
        if (left | right) {
            result++;
        }
        break;
    case 9:
        if (left & right) {
            result++;
        }
        break;
    case 10:
        if (~left & right) {
            result++;
        }
        break;
    }
    if (result == 1) {
        D_800B0078->pc += 8;
    } else {
        D_800B0078->pc = func_800ACDB8(6);
    }
}

/* Jump to operand 1. */
void func_800A1E74(void) {
    D_800B0078->pc = func_800ACDB8(1);
}

/* Advance, raising the batch limit by 32. */
void func_800A1E9C(void) {
    D_800AFC7C += 32;
    D_800B0078->pc++;
}

extern void (*D_800AE2A0[])(void); /* event instructions */
extern s32 D_800ADBE0;
extern s32 D_800ADBEC;
extern s32 D_800AFFEC;

/* Run the current actor's event instructions until one yields, its script
 * slot ends, the field starts a transition or `limit` (raised by some
 * instructions) runs out; 1024 is an error. Declared int without a
 * value, as the original keeps $v0 live (its loop delay slot stays empty). */
s32 func_800A1EC8(s32 limit) {
    s32 count;

    D_800B00C0 = 0;
    D_800AFC7C = limit;
    for (count = 0; count < D_800AFC7C; count++) {
        if (count > 0x400) {
            if (D_800C268C == 0) {
                func_800379C8("EVENTLOOP ERROR ACT=%d\n", D_800AFD1C);
            }
            return;
        }
        D_800AE2A0[D_800ADC00[D_800B0078->pc]]();
        if (D_800AFFEC == 0) {
            D_800AFC7C = 0xFFFF;
        }
        if (D_800ADB1C != 0 && (D_800ADBE0 == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0)) {
            return;
        }
        if (D_800B00C0 == 1 && D_800AFFEC == D_800B00C0) {
            return;
        }
    }
}

#ifdef NON_MATCHING
void func_800A2030(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 count;
    s32 index;
    s32 i;
    s32 priority;

    if (D_800ADB74 == 1) {
        count = 1;
    } else {
        count = D_800ADBFC;
    }
    D_800ADB68 = 0;
    D_800C4268 = 0;
    for (index = 0; index < count; index++) {
        descriptor = &D_800AF880.components.descriptors[index];
        if (!(descriptor->flags & 0xF00) || (descriptor->actor->layer_flags & 0x100000)) {
            continue;
        }
        if (D_800ADB1C != 0 && (D_800ADBE0 == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0)) {
            continue;
        }
        actor = descriptor->actor;
        actor->flags &= ~0x1000000;
        D_800B06B8 = descriptor;
        D_800AFD1C = index;
        D_800B0078 = actor;
        priority = 0xF;
        if (D_800B2078.party_processing_mode != 0) {
            for (i = 0; i < 3; i++) {
                if (D_8005A444[i] != 0xFF && D_8005A444[i] == index) {
                    goto next;
                }
            }
        }
        for (i = 0; i < 8; i++) {
            if (priority >= D_800B0078->slots[i].priority) {
                priority = D_800B0078->slots[i].priority;
                D_800B0078->slot = i;
            }
        }
        if (priority == 0xF) {
            D_800B0078->slots[0].resume_pc = func_800A3090(index, 1);
            D_800B0078->slots[0].priority = 7;
            D_800B0078->slot = 0;
        }
        D_800B0078->pc = D_800B0078->slots[D_800B0078->slot].resume_pc;
        D_800AFFEC = 1;
        if (!(D_800B0078->flags & 1)) {
            func_800A1EC8(8);
        }
        D_800B0078->slots[D_800B0078->slot].resume_pc = D_800B0078->pc;
    next:;
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A2030);
#endif

s32 func_800A1EC8(s32 limit);
extern s32 D_800AFFEC;

/* Run event `event` of actor 0 immediately with fresh script slots, then
 * restore the actor's record. */
void func_800A22AC(s32 event) {
    FieldActor *saved;
    s32 i;

    D_800B0078 = (D_800B06B8 = D_800AF880.components.descriptors)->actor;
    saved = func_80031BDC(sizeof(FieldActor), 1);
    *saved = *D_800B06B8->actor;
    for (i = 0; i < 8; i++) {
        D_800B0078->slots[i].countdown = 0;
        D_800B0078->slots[i].unk16 = 0;
        D_800B0078->slots[i].priority = 15;
        D_800B0078->slots[i].resume_pc = 0xFFFF;
        D_800B0078->slots[i].unk22 = 0;
        D_800B0078->slots[i].tag = 0xFF;
        D_800B0078->slots[i].value = 0xFFFF;
        D_800B0078->slots[i].move_mode = 0;
    }
    D_800AFD1C = 0;
    D_800ADB1C = 0;
    D_800AFFEC = 0;
    D_800B0078->pc = func_800A3090(0, event);
    func_800A1EC8(0xFFFF);
    D_800ADB1C = 1;
    *D_800B06B8->actor = *saved;
    func_800320E8(saved);
}

void func_800A22AC(s32 mode);

/* Rebuild the party (mode 3) with 800adb8c set. */
void func_800A2488(void) {
    D_800ADB8C = 1;
    func_800A22AC(3);
    func_800ACE24();
    D_800ADB8C = 0;
}

/* After a return to the field: run actor 0's event 2, show reassigned
 * party members, restart each 801e layer's animation, lift the controlled
 * actor 8 units unless its +74 is 0xff, and move the pieces by their
 * accumulated drift (mode 0: non-event pieces, mode 1: all but moving
 * event actors). `i` also holds the +74 byte, as in the original. */
void func_800A24C4(void) {
    u8 unused[0x10]; /* the original frame holds 0x10 unused bytes */
    s32 i;

    if (D_8004F30C != 0) {
        func_800A22AC(2);
        func_800AD898();
        for (i = 0; i < D_800B2078.unk2264; i++) {
            func_801E8330((u16)i, 0, D_800B2078.unk21E4[i]);
        }
        i = D_800AF880.components.descriptors[D_800B2078.controlled].actor->unk074;
        if (i != 0xFF) {
            D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[1] -= 8;
        }
        for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
            if (i < D_800ADBFC) {
                if (D_800AF880.components.descriptors[i].actor->state.word & 3) {
                    continue;
                }
            } else if ((D_800B2078.piece_drift_mode & 0x7F) == 0) {
                D_800AF880.components.descriptors[i].matrix.t[0] += PIECE_DRIFT_TOTAL[0];
                D_800AF880.components.descriptors[i].matrix.t[1] += PIECE_DRIFT_TOTAL[1];
                D_800AF880.components.descriptors[i].matrix.t[2] += PIECE_DRIFT_TOTAL[2];
            }
            if ((D_800B2078.piece_drift_mode & 0x7F) == 1) {
                D_800AF880.components.descriptors[i].matrix.t[0] += PIECE_DRIFT_TOTAL[0];
                D_800AF880.components.descriptors[i].matrix.t[1] += PIECE_DRIFT_TOTAL[1];
                D_800AF880.components.descriptors[i].matrix.t[2] += PIECE_DRIFT_TOTAL[2];
            }
        }
    }
}

extern s32 D_8004F30C;
void func_800A3C8C(void);

/* Reload the actors' extra blocks (file +124 into +120) and hand them to
 * their models, then refresh the field state. */
void func_800A2714(void) {
    FieldActor *actor;
    s32 i;

    if (D_8004F30C != 0) {
        for (i = 0; i < D_800ADBFC; i++) {
            func_80028470(4, 0);
            actor = D_800AF880.components.descriptors[i].actor;
            if (actor->unk124 != -1) {
                D_800B0078 = actor;
                D_800B0078->unk120 = func_80031BDC(func_800288EC(actor->unk124) + 8, 0);
                func_800295D8(D_800B0078->unk124, D_800B0078->unk120, 0, 0x80);
                func_80028A60(0);
            }
        }
        for (i = 0; i < D_800ADBFC; i++) {
            if (D_800AF880.components.descriptors[i].actor->unk124 != -1) {
                func_80021BF0(D_800AF880.components.descriptors[i].model,
                              D_800AF880.components.descriptors[i].actor->unk120);
            }
        }
        func_800A3C8C();
        if (D_800B2078.unk2078 != 0) {
            func_800A484C(1);
        }
        func_800A3074(0x10, 0);
        func_800A30B4();
        for (i = 0; i < D_800ADBFC; i++) {
            func_80072254(i);
        }
    }
}

#ifdef NON_MATCHING
void func_800A28D4(void) {
    s32 i;
    FieldModel *model;
    FieldActor *actor;
    s32 *sprites;

    if (D_8004F30C != 0) {
        func_800A3474();
        for (i = 0; i < D_800ADBFC; i++) {
            actor = D_800AF880.components.descriptors[i].actor;
            if (!(actor->unk126[0] & 0x80)) {
                func_80076AC0(i, actor->unk126[1], D_8005A414[actor->unk126[0]], actor->unk130_28 & 3,
                              actor->unk134 & 0xF, D_800AF880.components.descriptors[i].actor->unk126[0],
                              (D_800AF880.components.descriptors[i].actor->unk134 >> 4) & 1);
            } else {
                sprites = D_800AF880.components.sprites;
                func_80076AC0(i, actor->unk126[1], (u8 *)(sprites[(actor->unk126[0] & 0x7F) + 1] + (s32)sprites),
                              actor->unk130_28 & 3, actor->unk134 & 0xF,
                              D_800AF880.components.descriptors[i].actor->unk126[0],
                              (D_800AF880.components.descriptors[i].actor->unk134 >> 4) & 1);
                switch (D_800AF880.components.descriptors[i].actor->state.bits.unk16) {
                case 1:
                    func_8002303C(D_800AF880.components.descriptors[i].model, 2, 0);
                    D_800AF880.components.descriptors[i].model->animation->unk18[2] = D_800AF880.components.descriptors[i].actor->state.bits.unk18;
                    D_800AF880.components.descriptors[i].model->animation->unk18[3] = D_800AF880.components.descriptors[i].actor->unk130;
                    break;
                case 2:
                    func_8002303C(D_800AF880.components.descriptors[i].model, 3, 0);
                    D_800AF880.components.descriptors[i].model->animation->unk18[2] = D_800AF880.components.descriptors[i].actor->state.bits.unk18;
                    D_800AF880.components.descriptors[i].model->animation->unk18[3] = D_800AF880.components.descriptors[i].actor->unk130;
                    D_800AF880.components.descriptors[i].model->animation->unk18[4] = D_800AF880.components.descriptors[i].actor->unk130_9;
                    D_800AF880.components.descriptors[i].model->animation->unk18[5] = D_800AF880.components.descriptors[i].actor->unk130_19;
                    break;
                }
            }
        }
        if (D_800B2078.unk2268 != 0) {
            for (i = 0; i < 3; i++) {
                if (D_8005A444[i] != 0xFF) {
                    if (D_8005A39C->unk22B1[i] != 0) {
                        model = D_800AF880.components.descriptors[D_8005A444[i]].model;
                        D_800AF880.components.descriptors[D_8005A444[i]].model = D_800AF880.components.descriptors[D_8006F990[i]].model;
                        D_800AF880.components.descriptors[D_8006F990[i]].model = model;
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags |= 0x200;
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags &= ~0x500;
                        D_800AF880.components.descriptors[D_8006F990[i]].flags |= 0x20;
                    } else {
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags |= 0x400;
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags &= ~0x300;
                    }
                }
            }
        }
    } else {
        func_800A3074(0x10, 0);
        func_800A30B4();
        for (i = 0; i < D_800ADBFC; i++) {
            D_800AFD1C = i;
            D_800B06B8 = &D_800AF880.components.descriptors[i];
            D_800B0078 = D_800B06B8->actor;
            D_800B0078->pc = func_800A3090(i, 2);
            if (D_800ADC00[D_800B0078->pc] == 0) {
                D_800B0078->layer_flags |= 0x4000000;
            }
            D_800AFD1C = i;
            D_800B06B8 = &D_800AF880.components.descriptors[i];
            D_800B0078 = D_800B06B8->actor;
            D_800B0078->pc = func_800A3090(i, 0);
        }
        for (i = 0; i < D_800ADBFC; i++) {
            D_800AFD1C = i;
            D_800AFC74 = 0;
            D_800AFFEC = 0;
            D_800B06B8 = &D_800AF880.components.descriptors[i];
            D_800B0078 = D_800B06B8->actor;
            func_800A1EC8(0xFFFF);
            if (D_800AFC74 == 0) {
                sprites = D_800AF880.components.sprites;
                func_80076AC0(i, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 0);
                D_800B0078->layer_flags |= 0x800;
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A28D4);
#endif

/* Advance one byte. */
void func_800A2FC0(void) {
    D_800B0078->pc++;
}

/* -1 when event variable `reference` is read unsigned, else 0. */
s32 func_800A2FE0(s32 reference) {
    if (D_800ADBF8->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        return -1;
    }
    return 0;
}

/* Read event variable `reference` (a byte offset into the bank). */
s32 func_800A3018(s32 reference) {
    s32 value;

    if (D_800ADBF8->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        value = (u16)D_800C3A68[reference >> 1];
    } else {
        value = D_800C3A68[reference >> 1];
    }
    return value;
}

#ifdef NON_MATCHING
/* Write event variable `reference`. The original keeps a temporary
 * (sra a2 / sll v0) that this form does not. */
void func_800A3074(u16 reference, s32 value) {
    D_800C3A68[reference >> 1] = value;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3074);
#endif

/* Entry PC of event `event` of actor `actor`. */
s32 func_800A3090(s32 actor, s32 event) {
    u16 *entries = D_800ADBF8->entries;

    return entries[actor * 32 + event];
}

/* Store the three party members in variables 3e, 40, 42. */
void func_800A30B4(void) {
    func_800A3074(0x3E, D_80062590[0]);
    func_800A3074(0x40, D_80062590[1]);
    func_800A3074(0x42, D_80062590[2]);
}

extern u16 D_8005941C;
extern u8 D_800594D0;
s32 func_8009744C(void);
s32 func_8009A514(void);

/* Record the current map and camera in the game state and variables and
 * save the event variable bank. */
void func_800A30FC(void) {
    s32 i;

    D_8005A39C->unk231A = D_8004F34C;
    D_8005A39C->unk2322 = D_8004F324;
    D_8005A39C->unk2320 = D_8005A39C->vars[1];
    D_8005A39C->unk231C = D_8005A39C->vars[4] << 9;
    func_800A3074(0x44, D_8005941C);
    func_800A3074(0x46, D_800594D0);
    func_800A3074(6, func_8009744C() & 0xFFFF);
    func_800A3074(8, func_8009A514() & 0xFFFF);
    func_800A3074(0x24, (s16)D_800AF880.elevation);
    func_800A3074(0x3C, D_8004F34C);
    func_800A30B4();
    for (i = 0; i < 0x200; i++) {
        D_8005A39C->vars[i] = D_800C3A68[i];
    }
}

/* Update the play record once per frame (not while 800b02c8 is 1): the
 * held buttons seen, the party, the departure data, the party slots'
 * positions, the play clock in variable 10 (minutes:seconds stepped
 * every 31 frames; counting down with 8004f328 bit 2, stopped by bit 7)
 * and the controlled actor's position in variables 1e-22. */
void func_800A31E8(void) {
    s32 i;
    s32 value;
    s32 seconds;
    s32 minutes;

    if (D_800B02C8 == 1) {
        return;
    }
    D_800AFC6C |= D_800AFE9C;
    for (i = 0; i < 3; i++) {
        D_8005A39C->unk1D34[i] = D_80062590[i];
    }
    func_800A30FC();
    D_8004F2F4 = 0;
    D_8004F318++;
    for (i = 0; i < 3; i++) {
        if (D_8005A39C->unk22B1[i] == 1) {
            func_8009FEE4(i);
        }
    }
    if (D_8004F318 > 30) {
        D_8004F318 = 0;
        if (!(D_8004F328 & 0x80)) {
            value = func_800A3018(0xA);
            seconds = value & 0xFF;
            minutes = (value >> 8) & 0xFF;
            if (!(D_8004F328 & 4)) {
                if (seconds != 0xFF3B) { /* never equal: the original's limit check */
                    seconds++;
                    if (seconds > 60) {
                        seconds = 0;
                        minutes++;
                    }
                }
            } else if (seconds == 0) {
                if (minutes != 0) {
                    seconds = 59;
                    minutes--;
                }
            } else {
                seconds--;
            }
            func_800A3074(0xA, (minutes << 8) | (seconds & 0xFF));
        }
    }
    func_800A3074(0xC, D_80059418 | (D_80059420 << 8));
    func_800A3074(0xE, D_80059484);
    func_800A3074(0x1E, WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[0]));
    func_800A3074(0x20, WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[2]));
    func_800A3074(0x22, WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[1]));
}

#ifdef NON_MATCHING
void func_800A3474(void) {
    s32 i;
    s32 flags;
    s32 *list;

    D_800AFC50 = D_8005A4E4;
    D_800AF880.components.descriptor_count = *D_800AFC50;
    D_800AFC50 += 4;
    COPY_BLOCK(&D_800B007C, D_800AFC50, 0x38);
    D_800AFC50 += 0x38;
    COPY_BLOCK(&D_800AF880.world_angles, D_800AFC50, 0x74);
    D_800AFC50 += 0x74;
    COPY_BLOCK(D_800AF880.components.collision_attributes, D_800AFC50, 0x400);
    D_800AFC50 += 0x400;
    COPY_BLOCK(&D_800B2078, D_800AFC50, 0x2E4);
    D_800AFC50 += 0x2E4;
    COPY_BLOCK(&D_800AF880, D_800AFC50, 0x1C8);
    D_800AFC50 += 0x1C8;
    for (i = 0; i < D_800ADBFC; i++) {
        COPY_BLOCK(&D_800AF880.components.descriptors[i].rotation, D_800AFC50, 8);
        D_800AFC50 += 8;
        COPY_BLOCK(&flags, D_800AFC50, 4);
        D_800AFC50 += 4;
        D_800AF880.components.descriptors[i].flags = flags;
        D_800AFC50 += 0x30;
        list = D_800AF880.components.descriptors[i].actor->list;
        COPY_BLOCK(D_800AF880.components.descriptors[i].actor, D_800AFC50, 0x138);
        D_800AF880.components.descriptors[i].actor->list = list;
        D_800AFC50 += 0x138;
        if (D_800AF880.components.descriptors[i].actor->unk134 & 0x80) {
            D_800AF880.components.descriptors[i].actor->unk110 = func_80031BDC(0xC, 0);
            COPY_BLOCK(D_800AF880.components.descriptors[i].actor->unk110, D_800AFC50, 0xC);
            D_800AFC50 += 0xC;
        }
        if (D_800AF880.components.descriptors[i].actor->state.word & 0x1000) {
            D_800AF880.components.descriptors[i].actor->unk114 = func_80031BDC(0x10, 0);
            COPY_BLOCK(D_800AF880.components.descriptors[i].actor->unk114, D_800AFC50, 0x10);
            D_800AFC50 += 0x10;
        }
    }
    COPY_BLOCK(D_800C3A68, D_800AFC50, 0x800);
    D_800AFC50 += 0x800;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3474);
#endif

#ifdef NON_MATCHING
void func_800A3C8C(void) {
    s32 changed;
    s32 i;
    u8 *record;
    FieldDescriptor *descriptor;

    D_800AFC50 = D_8005A4E4;
    D_800AF880.components.descriptor_count = *D_800AFC50;
    D_800AFC50 += 0x3C;
    *(ViewSnapshot *)&D_800AF880.world_angles = *(ViewSnapshot *)D_800AFC50;
    D_800AFC50 += 0x920;
    changed = 0;
    for (i = 0; i < 3; i++) {
        if (D_8005A408[i] != D_8005A39C->unk22B1[i]) {
            changed++;
        }
    }
    for (i = 0; i < D_800ADBFC; i++) {
        record = D_800AFC50;
        D_800AFC50 += 0xC;
        if (D_800AF880.components.descriptors[i].actor->unk124 != -1 && D_800AF880.components.descriptors[i].actor->unk0EA != 0xFF) {
            *(s16 *)(record + 0x20) = D_800AF880.components.descriptors[i].actor->unk0EA;
        }
        descriptor = &D_800AF880.components.descriptors[i];
        if (!(descriptor->actor->layer_flags & 0x1000000)
            && (D_800B2078.unk2268 == 0 || !(descriptor->actor->flags & 0x600) || changed == 0)) {
            func_80021D50(descriptor->model, D_800AFC50);
        }
        record = D_800AFC50;
        descriptor = &D_800AF880.components.descriptors[i];
        D_800AFC50 = record + 0x168;
        if (descriptor->actor->unk134 & 0x80) {
            D_800AFC50 = record + 0x174;
        }
        if (descriptor->actor->state.word & 0x1000) {
            D_800AFC50 += 0x10;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3C8C);
#endif

#ifdef NON_MATCHING
void func_800A3F4C(void) {
    s32 i;
    s32 flags;
    s32 size;
    u8 *snapshot;

    D_800AFC50 = D_8005A4E4;
    *D_800AFC50 = D_800AF880.components.descriptor_count;
    D_800AFC50 += 4;
    COPY_BLOCK(D_800AFC50, &D_800B007C, 0x38);
    D_800AFC50 += 0x38;
    COPY_BLOCK(D_800AFC50, &D_800AF880.world_angles, 0x74);
    D_800AFC50 += 0x74;
    COPY_BLOCK(D_800AFC50, D_800AF880.components.collision_attributes, 0x400);
    D_800AFC50 += 0x400;
    COPY_BLOCK(D_800AFC50, &D_800B2078, 0x2E4);
    D_800AFC50 += 0x2E4;
    COPY_BLOCK(D_800AFC50, &D_800AF880, 0x1C8);
    D_800AFC50 += 0x1C8;
    for (i = 0; i < D_800ADBFC; i++) {
        COPY_BLOCK(D_800AFC50, &D_800AF880.components.descriptors[i].rotation, 8);
        D_800AFC50 += 8;
        flags = D_800AF880.components.descriptors[i].flags;
        COPY_BLOCK(D_800AFC50, &flags, 4);
        D_800AFC50 += 4;
        func_80021EBC(D_800AF880.components.descriptors[i].model, D_800AFC50);
        D_800AFC50 += 0x30;
        COPY_BLOCK(D_800AFC50, D_800AF880.components.descriptors[i].actor, 0x138);
        D_800AFC50 += 0x138;
        if (D_800AF880.components.descriptors[i].actor->unk134 & 0x80) {
            COPY_BLOCK(D_800AFC50, D_800AF880.components.descriptors[i].actor->unk110, 0xC);
            D_800AFC50 += 0xC;
        }
        if (D_800AF880.components.descriptors[i].actor->state.word & 0x1000) {
            COPY_BLOCK(D_800AFC50, D_800AF880.components.descriptors[i].actor->unk114, 0x10);
            D_800AFC50 += 0x10;
        }
    }
    COPY_BLOCK(D_800AFC50, D_800C3A68, 0x800);
    D_800AFC50 += 0x800;
    for (i = 0; i < 3; i++) {
        D_8005A408[i] = D_8005A39C->unk22B1[i];
    }
    snapshot = D_8005A4E4;
    size = D_800AFC50 - snapshot;
    if (D_800C268C == 0) {
        func_800379C8("SAVESIZE=%d %x\n", size, size);
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3F4C);
#endif

void func_800A476C(s32 x, s32 y);

/* Reset the screen effect view with the image moved to (2c0, 100). */
void func_800A4748(void) {
    func_800A476C(0x2C0, 0x100);
}

/* Restore the projection distance and move the 320x224 screen image to
 * (x, y). */
void func_800A476C(s32 x, s32 y) {
    RECT rect;

    rect.w = 0x140;
    rect.y = 0;
    rect.x = 0;
    rect.h = 0xE0;
    SetGeomScreen(0x200);
    MoveImage(&rect, x, y);
    func_800775F8();
}

extern s32 D_800ADB24; /* screen effect buffers allocated */

/* Stop the screen effect and release its buffers. */
void func_800A47D4(void) {
    D_800B2078.unk2078 = 0;
    if (D_800ADB24 != 0) {
        func_800320E8(D_800B2078.effect_buffers[0]);
        func_800320E8(D_800B2078.effect_buffers[1]);
        func_800320E8(D_800B2078.effect_buffers[2]);
        func_800320E8(D_800B2078.effect_buffers[3]);
        D_800ADB24 = 0;
    }
}

#ifdef NON_MATCHING
void func_800A484C(s32 resume) {
    RECT rect;
    s32 row;
    s32 column;
    s32 i;
    POLY_FT4 *poly;
    POLY_FT4 *copy;
    s32 u;
    s32 v;

    D_800B2078.unk2078 = 1;
    if (D_800ADB24 == 0) {
        D_800B2078.effect_buffers[0] = func_80031BDC(0x180, 0);
        D_800B2078.effect_buffers[1] = func_80031BDC(0x180, 0);
        D_800B2078.effect_buffers[2] = func_80031BDC(0x3840, 0);
        D_800B2078.effect_buffers[3] = func_80031BDC(0x3840, 0);
        D_800ADB24 = 1;
        for (row = 0; row < 17; row++) {
            for (column = 0; column < 20; column++) {
                poly = (POLY_FT4 *)D_800B2078.effect_buffers[2] + row * 20 + column;
                copy = (POLY_FT4 *)D_800B2078.effect_buffers[3] + row * 20 + column;
                SetPolyFT4(poly);
                SetSemiTrans(poly, 0);
                poly->r0 = 0x80;
                poly->g0 = 0x80;
                poly->b0 = 0x80;
                poly->x0 = column * 16;
                poly->y0 = row * 16;
                poly->x1 = column * 16 + 16;
                poly->y1 = row * 16;
                poly->x2 = column * 16;
                poly->y2 = row * 16 + 16;
                poly->x3 = column * 16 + 16;
                poly->y3 = row * 16 + 16;
                if (row >= 14) {
                    u = (column * 16) & 0x3F;
                    v = (column >> 2) * 16 + (row - 14) * 80;
                    poly->u0 = u;
                    poly->v0 = v;
                    poly->u1 = u + 16;
                    poly->v1 = v;
                    poly->u2 = u;
                    poly->v2 = v + 16;
                    poly->u3 = u + 16;
                    poly->v3 = v + 16;
                    poly->tpage = GetTPage(2, 0, 0x3C0, 0);
                    *copy = *poly;
                } else {
                    u = (column * 16) & 0x3F;
                    poly->u0 = u;
                    poly->v0 = row * 16;
                    poly->u1 = u + 16;
                    poly->v1 = row * 16;
                    poly->u2 = u;
                    poly->v2 = row * 16 + 16;
                    poly->u3 = u + 16;
                    poly->v3 = row * 16 + 16;
                    poly->tpage = GetTPage(2, 0, (column * 16) & 0xFFC0, 0);
                    *copy = *poly;
                    copy->tpage = GetTPage(2, 0, (column * 16) & 0xFFC0, 0x100);
                }
            }
        }
        rect.x = 0;
        rect.y = 0x20;
        rect.w = 0x140;
        rect.h = 0xC0;
        SetDrawMove(D_800B2078.effect_buffers[0], &rect, 0, 0);
        rect.y = 0x120;
        SetDrawMove(D_800B2078.effect_buffers[1], &rect, 0, 0x100);
        rect.w = 0x40;
        rect.h = 0x10;
        for (i = 0; i < 15; i++) {
            rect.x = D_800AEB24[i].x;
            rect.y = D_800AEB24[i].y;
            SetDrawMove((DR_MOVE *)D_800B2078.effect_buffers[0] + i + 1, &rect, 0x3C0, i * 16);
            rect.y = D_800AEB24[i].y + 0x100;
            SetDrawMove((DR_MOVE *)D_800B2078.effect_buffers[1] + i + 1, &rect, 0x3C0, i * 16);
        }
    }
    if (resume == 0) {
        func_800A4CC4(func_800ACDEC(1), func_800ACDEC(3), func_800ACDEC(5), func_800ACDEC(7),
                      func_800ACDEC(9), func_800ACDEC(11), func_800ACDEC(13));
    }
    D_800B2078.unk207A = 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A484C);
#endif

/* Move the six screen effect values to the given whole targets over
 * `steps` frames. */
void func_800A4CC4(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 steps) {
    s32 step0;
    s32 step1;
    s32 step2;
    s32 step3;
    s32 step4;
    s32 step5;

    if (steps == 0) {
        steps = 1;
    }
    step0 = ((a << 16) - D_800B2078.effect_value[0]) / steps;
    step1 = ((b << 16) - D_800B2078.effect_value[1]) / steps;
    step2 = ((c << 16) - D_800B2078.effect_value[2]) / steps;
    step3 = ((d << 16) - D_800B2078.effect_value[3]) / steps;
    step4 = ((e << 16) - D_800B2078.effect_value[4]) / steps;
    step5 = ((f << 16) - D_800B2078.effect_value[5]) / steps;
    D_800B2078.effect_steps = steps;
    D_800B2078.effect_step[0] = step0;
    D_800B2078.effect_step[1] = step1;
    D_800B2078.effect_step[2] = step2;
    D_800B2078.effect_step[3] = step3;
    D_800B2078.effect_step[4] = step4;
    D_800B2078.effect_step[5] = step5;
}

#ifdef NON_MATCHING
void func_800A4DAC(void) {
    s32 amplitude_x;
    s32 amplitude_y;
    s32 frequency_x;
    s32 frequency_y;
    s32 phase_x;
    s32 phase_y;
    s32 row;
    s32 column;
    s32 wave_x;
    s32 wave_y;
    s32 value;
    POLY_FT4 *grid;
    POLY_FT4 *poly;
    DR_MOVE *moves;

    if (D_800B2078.unk2078 == 0) {
        return;
    }
    if (D_800B2078.effect_steps > 0) {
        D_800B2078.effect_value[0] += D_800B2078.effect_step[0];
        D_800B2078.effect_value[1] += D_800B2078.effect_step[1];
        D_800B2078.effect_steps--;
        D_800B2078.effect_value[2] += D_800B2078.effect_step[2];
        D_800B2078.effect_value[3] += D_800B2078.effect_step[3];
        D_800B2078.effect_value[4] += D_800B2078.effect_step[4];
        D_800B2078.effect_value[5] += D_800B2078.effect_step[5];
    } else if (D_800B2078.unk207A != 0) {
        D_800B2078.effect_value[5] = 0;
        D_800B2078.effect_value[4] = 0;
        D_800B2078.effect_value[3] = 0;
        D_800B2078.effect_value[2] = 0;
        D_800B2078.effect_value[1] = 0;
        D_800B2078.effect_value[0] = 0;
        D_800B2078.unk2078 = 0;
    }
    amplitude_x = WHOLE(D_800B2078.effect_value[0]);
    amplitude_y = WHOLE(D_800B2078.effect_value[1]);
    frequency_x = WHOLE(D_800B2078.effect_value[2]);
    frequency_y = WHOLE(D_800B2078.effect_value[3]);
    EFFECT_PHASE[0] += WHOLE(D_800B2078.effect_value[4]);
    EFFECT_PHASE[1] += WHOLE(D_800B2078.effect_value[5]);

    /* Rows 14-16 (the saved strips) continue the wave below the screen. */
    phase_y = EFFECT_PHASE[1] + frequency_y * 11;
    for (row = 14; row < 17; row++) {
        wave_y = (func_8003F8CC(phase_y) * amplitude_y) >> 12;
        phase_y += frequency_y;
        phase_x = EFFECT_PHASE[0];
        for (column = 0; column < 20; column++) {
            wave_x = (func_8003F8CC(phase_x) * amplitude_x) >> 12;
            phase_x += frequency_x;
            grid = D_800B2078.effect_buffers[2 + D_800ADB08];
            poly = &grid[row * 20 + column];
            if (column != 0) {
                value = column * 16 + wave_x;
                poly[-1].x3 = poly[-1].x1 = poly->x2 = poly->x0 = value;
                if (column == 19) {
                    grid[row * 20 + 19].x3 = grid[row * 20 + 19].x1 = wave_x + 0x140;
                }
            } else {
                grid[row * 20].x2 = grid[row * 20].x0 = 0;
            }
            value = row * 16 + wave_y - 0x30;
            grid = D_800B2078.effect_buffers[2 + D_800ADB08];
            grid[row * 20 + column - 20].y3 = grid[row * 20 + column - 20].y2 =
                grid[row * 20 + column].y1 = grid[row * 20 + column].y0 = value;
            if (row == 16) {
                grid[16 * 20 + column].y3 = grid[16 * 20 + column].y2 = 0xE0;
            }
            addPrim(&D_800C426C->ot[1], poly);
        }
    }
    moves = D_800B2078.effect_buffers[D_800ADB08];
    addPrim(&D_800C426C->ot[1], &moves[0]);

    /* Rows 0-13: the screen itself. */
    phase_y = EFFECT_PHASE[1];
    for (row = 0; row < 14; row++) {
        wave_y = (func_8003F8CC(phase_y) * amplitude_y) >> 12;
        phase_y += frequency_y;
        phase_x = EFFECT_PHASE[0];
        for (column = 0; column < 20; column++) {
            wave_x = (func_8003F8CC(phase_x) * amplitude_x) >> 12;
            phase_x += frequency_x;
            grid = D_800B2078.effect_buffers[2 + D_800ADB08];
            poly = &grid[row * 20 + column];
            if (row != 0) {
                if (row == 13) {
                    value = wave_y + 0xF0;
                    grid[13 * 20 + column].y1 = grid[13 * 20 + column].y0 =
                        grid[12 * 20 + column].y3 = grid[12 * 20 + column].y2 = value;
                    wave_y = (func_8003F8CC(phase_y) * amplitude_y) >> 12;
                    grid = D_800B2078.effect_buffers[2 + D_800ADB08];
                    grid[13 * 20 + column].y3 = grid[13 * 20 + column].y2 = wave_y + 0xF0;
                } else {
                    value = row * 16 + wave_y + 0x20;
                    poly[-20].y3 = poly[-20].y2 = poly->y1 = poly->y0 = value;
                }
            } else {
                grid[column].y1 = grid[column].y0 = 0x20;
            }
            if (column != 0) {
                value = column * 16 + wave_x;
                grid = D_800B2078.effect_buffers[2 + D_800ADB08];
                grid[row * 20 + column - 1].x3 = grid[row * 20 + column - 1].x1 =
                    grid[row * 20 + column].x2 = grid[row * 20 + column].x0 = value;
                if (column == 19) {
                    grid[row * 20 + 19].x3 = grid[row * 20 + 19].x1 = 0x140;
                }
            } else {
                grid = D_800B2078.effect_buffers[2 + D_800ADB08];
                grid[row * 20].x2 = grid[row * 20].x0 = 0;
            }
            addPrim(&D_800C426C->ot[1], poly);
        }
    }
    for (row = 0; row < 15; row++) {
        moves = D_800B2078.effect_buffers[D_800ADB08];
        addPrim(&D_800C426C->ot[1], &moves[row + 1]);
    }
    addPrim(&D_800C426C->ot[1], &D_800B1E18[D_800ADB08]);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A4DAC);
#endif

/* Store three words at +14 of `object`. */
void func_800A55B8(s32 *object, s32 a, s32 b, s32 c) {
    object[5] = a;
    object[6] = b;
    object[7] = c;
}

extern void *D_800AFE80;
extern void *D_800B069C;

/* Release the two blocks at 800afe80 and 800b069c. */
void func_800A55C8(void) {
    func_800320E8(D_800AFE80);
    func_800320E8(D_800B069C);
}

/* The five screen pieces (800b11ac), each with a quad and draw mode per
 * draw buffer. */
typedef struct {
    DR_MODE modes[5][2];   /* 000 */
    RECT windows[5][2];    /* 078: texture windows */
    POLY_FT4 quads[5][2];  /* 0C8 */
    SVECTOR corners[5][4]; /* 258 */
} ScreenPieces;
extern ScreenPieces D_800B11AC;
extern s32 D_800C2684;     /* piece scale, 0x1000 = 1 */
extern SVECTOR D_800B00B8; /* piece rotation */

/* Set the five screen pieces of the next draw buffer to grey `shade`. */
void func_800A5600(s32 shade) {
    s32 i;

    for (i = 0; i < 5; i++) {
        (D_800B11AC.quads[i] + ((D_800ADB08 + 1) & 1))->r0 = shade;
        (D_800B11AC.quads[i] + ((D_800ADB08 + 1) & 1))->g0 = shade;
        (D_800B11AC.quads[i] + ((D_800ADB08 + 1) & 1))->b0 = shade;
    }
}

/* Fade screen channel 0 in from white over `frames` frames. */
void func_800A56A8(s32 frames) {
    s32 step;

    step = -0x10000 / frames;
    D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0xFF00;
    D_800B2078.fades[0].steps = frames + 1;
    D_800B2078.fades[0].abr = D_800B2078.fades[0].active = 1;
    D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = step;
}

/* Fade screen channel 0 up from black over `frames` frames. */
void func_800A5710(s32 frames) {
    s32 step;

    step = 0x10000 / frames;
    D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0;
    D_800B2078.fades[0].steps = frames + 1;
    D_800B2078.fades[0].abr = D_800B2078.fades[0].active = 1;
    D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = step;
}

/* Set the mask bit of every pixel of the 64-pixel-wide VRAM column at
 * (x, y), `h` rows tall. */
void func_800A5774(s32 x, s32 y, s32 h) {
    RECT rect;
    u32 *pixels;
    u32 *p;
    s32 i;

    rect.x = x;
    rect.y = y;
    rect.w = 0x40;
    rect.h = h;
    pixels = func_80031BDC(h << 7, 1);
    StoreImage(&rect, pixels);
    DrawSync(0);
    p = pixels;
    for (i = 0; i < h * 32; i += 8) {
        p[0] |= 0x80008000;
        p[1] |= 0x80008000;
        p[2] |= 0x80008000;
        p[3] |= 0x80008000;
        p[4] |= 0x80008000;
        p[5] |= 0x80008000;
        p[6] |= 0x80008000;
        p[7] |= 0x80008000;
        p += 8;
    }
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_800320E8(pixels);
}

void func_800A663C(s32 semitrans, s32 abr);
void func_800A6408(void);
void func_800A6924(void);

/* Set up the screen pieces, draw them twice, mask the saved screen columns at
 * (2c0..3c0, 100) and draw twice more. */
void func_800A5884(s32 semitrans, s32 abr) {
    s32 i;
    s32 x;

    func_800A663C(semitrans, abr);
    for (i = 0; i < 2; i++) {
        func_80073FE0();
        func_800A6408();
        func_800A6924();
    }
    x = 0x2C0;
    for (i = 0; i < 5; i++) {
        func_800A5774(x, 0x100, 0xE0);
        x += 0x40;
    }
    for (i = 0; i < 2; i++) {
        func_80073FE0();
        func_800A6408();
        func_800A6924();
    }
}

#include "field_screen.h"

/* Run the requested screen transition (800adb38) over 800adb3c frames:
 * 1-2 fade the screen pieces out, 3 fades them in and holds while it
 * stays requested, 4 dissolves the screen grid from the centre. */
void func_800A5924(void) {
    s32 i;
    s32 x;
    s32 level;

    if (D_800ADB38 != 0) {
        func_8003748C();
        func_80070C84();
        func_800A915C();
        if (D_800ADB38 == 1 || D_800ADB38 == 4) {
            func_800A4748();
            DrawSync(0);
            func_80073FE0();
            func_800775F8();
        }
        func_800775F8();
    dispatch:
        switch (D_800ADB38) {
        case 4:
            func_800A5884(1, 1);
            func_800A6E70();
            D_800ADC08 = 1;
            func_80071E58(D_800ADB3C);
            D_800C3A40 = 0;
            for (i = 0; i < D_800ADB3C; i++) {
                func_80077DAC();
                func_800A6C40();
                func_8007554C();
                func_80078B5C();
                D_800C3A40 += 6;
            }
            DrawSync(0);
            func_800A7064();
            break;
        case 1:
        case 2:
            func_800A5884(1, 1);
            D_800ADC08 = 1;
            func_80071E58(D_800ADB3C);
            level = 0x800000;
            for (i = 0; i < D_800ADB3C; i++) {
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
                func_800A5600(level >> 16);
                level -= 0x800000 / D_800ADB3C;
                if (level < 0) {
                    level = 0;
                }
            }
            break;
        case 3:
            func_800A663C(1, 1);
            for (i = 0, x = 0x2C0; i < 5; i++, x += 0x40) {
                func_800A5774(x, 0x100, 0xE0);
            }
            level = 0;
            func_80071DCC(D_800ADB3C);
            func_800A5600(0);
            for (i = 0; i < D_800ADB3C; i++) {
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
                func_800A5600(level >> 16);
                level += 0x800000 / D_800ADB3C;
            }
            for (;;) {
                if (D_800ADB38 != 3) {
                    goto dispatch;
                }
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
            }
        }
        D_800ADB38 = 0;
        func_800A91F0();
        func_80077544();
    }
}

#ifdef NON_MATCHING
/* Reload the field for a map change inside the field mode (800b0048 the
 * transition kind, 800afd14 its frames): stop effects, keep the map
 * read-ahead block across the heap reset, then per kind fade or dissolve
 * out, reload the components (80070cc8), restart the music and fade in.
 * Unrecovered details: the original passes 800adb08 as a second argument
 * to 80071cb4, and uses a jump table. */
void func_800A5C40(void) {
    RECT rect;
    u8 *ahead;
    s32 kind;
    s32 frames;
    s32 level;
    s32 i;

    func_8003748C();
    func_800A9460();
    func_800864F0();
    func_8007FFE8();
    if (D_800B0048 != 6) {
        func_800A915C();
        if (D_800B0048 != 4) {
            func_800A4748();
        }
    }
    DrawSync(0);
    func_80073FE0();
    func_800775F8();
    func_800700B0();
    ahead = func_80031BDC(D_8005A4C0, 0);
    memcpy(ahead, D_8005A4E0, D_8005A4C0);
    func_800320B8(D_8005A4E0);
    func_800320E8(D_8005A4E0);
    if (D_800B0048 != 6) {
        func_800A90B4(1);
    }
    D_8005A4E0 = func_80031BDC(D_8005A4C0, 1);
    memcpy(D_8005A4E0, ahead, D_8005A4C0);
    func_800320A4(D_8005A4E0);
    func_800320E8(ahead);
    switch (D_800B0048) {
    case 6:
        func_80071DCC(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80073FE0();
            func_80071CB4(&D_800C426C->overlay_ot[0]);
            func_800A6924();
        }
    reload:
        func_80073FE0();
        func_800A6924();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        func_80070CC8();
        func_80070488();
        func_80070508();
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        func_80071E58(D_800AFD14);
        break;
    case 0:
        func_800A663C(0, 0);
        func_80071DCC(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80073FE0();
            func_80071CB4(&D_800C426C->overlay_ot[0]);
            func_800A6408();
            func_800A6924();
        }
        goto reload;
    case 1:
        func_800A663C(0, 0);
        func_800A5710(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80073FE0();
            func_80071CB4(&D_800C426C->overlay_ot[0]);
            func_800A6408();
            func_800A6924();
        }
        func_800775F8();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        func_80070CC8();
        func_80070488();
        func_80070508();
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        func_800A56A8(D_800AFD14);
        break;
    case 2:
    case 4:
        func_800A5884(1, 1);
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        func_80070CC8();
        func_80070488();
        if (D_800ADB60 == 1) {
            while (func_800286CC() != 0) {
                func_80073FE0();
                func_800A6408();
                func_800A6924();
                if (D_800C2684 < 0x22C0) {
                    D_800C2684 += 0x20;
                }
            }
            func_800320E8(D_800ADC14);
            D_800ADB60 = 0;
            func_80078C5C();
        }
        D_800AFD04 = 1;
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        level = 0x800000;
        func_80071E58(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
            func_800A5600(level >> 16);
            level -= 0x800000 / D_800AFD14;
            if (level < 0) {
                level = 0;
            }
            if (D_800C2684 < 0x22C0) {
                D_800C2684 += 0x20;
            }
        }
        break;
    case 3:
        func_800A663C(0, 0);
        func_80070488();
        func_80073FE0();
        func_800A6408();
        func_800A6924();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        D_800AFD04 = 1;
        func_80070CC8();
        func_80070508();
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        for (i = 0; i < 4; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
        }
        break;
    case 5:
        func_800A663C(0, 0);
        func_80070488();
        func_80073FE0();
        func_800A6408();
        func_800A6924();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        D_800AFD04 = 1;
        func_80070CC8();
        func_80070508();
        setRECT(&rect, 0x2C0, 0x100, 0x140, 0xFF);
        D_800B0048 = kind;
        D_800AFD14 = frames;
        MoveImage(&rect, 0x140, 0xFF);
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        for (i = 0; i < 4; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
        }
        break;
    }
    if (D_800B0048 != 6) {
        func_800A91F0();
    }
    D_800B0048 = 2;
    D_800AFD14 = 0x20;
    D_800AFD04 = 0;
    func_80077544();
    func_80031FF8();
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5C40);
#endif

/* Rotate and scale the screen pieces (when scaled) into their quads and
 * link the quads and draw modes of the current buffer. */
void func_800A6408(void) {
    MATRIX m;
    VECTOR scale;
    s32 p;
    s32 flag;
    s32 i;

    func_8003F738(&D_800B00B8, &m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    scale.vx = D_800C2684;
    scale.vy = D_800C2684;
    scale.vz = D_800C2684;
    ScaleMatrix(&m, &scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0; i < 5; i++) {
        if (D_800C2684 != 0x1000) {
            RotAverage4(&D_800B11AC.corners[i][0], &D_800B11AC.corners[i][1], &D_800B11AC.corners[i][2],
                        &D_800B11AC.corners[i][3], (s32 *)&D_800B11AC.quads[i][D_800ADB08].x0,
                        (s32 *)&D_800B11AC.quads[i][D_800ADB08].x1, (s32 *)&D_800B11AC.quads[i][D_800ADB08].x2,
                        (s32 *)&D_800B11AC.quads[i][D_800ADB08].x3, &p, &flag);
        }
        addPrim(&D_800C426C->overlay_ot[0], &D_800B11AC.quads[i][D_800ADB08]);
        addPrim(&D_800C426C->overlay_ot[0], &D_800B11AC.modes[i][D_800ADB08]);
    }
}

/* Set up the five 64x224 screen pieces (textured from the screen copy at
 * (2c0, 100)) at full scale with no rotation, with semi-transparency
 * `semitrans` and rate `abr`. */
void func_800A663C(s32 semitrans, s32 abr) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    SVECTOR *corners;
    s32 i;

    D_800C2684 = 0x1000;
    D_800B00B8.vx = 0;
    D_800B00B8.vy = 0;
    D_800B00B8.vz = 0;
    for (i = 0; i < 5; i++) {
        quad = &D_800B11AC.quads[i][0];
        copy = &D_800B11AC.quads[i][1];
        corners = D_800B11AC.corners[i];
        SetPolyFT4(quad);
        corners[0].vx = i * 0x20 - 0x50;
        corners[0].vy = -0x38;
        corners[0].vz = 0;
        corners[1].vx = i * 0x20 - 0x30;
        corners[1].vy = -0x38;
        corners[1].vz = 0;
        corners[2].vx = i * 0x20 - 0x50;
        corners[2].vy = 0x38;
        corners[2].vz = 0;
        corners[3].vx = i * 0x20 - 0x30;
        corners[3].vy = 0x38;
        corners[3].vz = 0;
        quad->x0 = i << 6;
        quad->y0 = 0;
        quad->x1 = (i << 6) + 0x40;
        quad->y1 = 0;
        quad->x2 = i << 6;
        quad->y2 = 0xDF;
        quad->x3 = (i << 6) + 0x40;
        quad->y3 = 0xDF;
        setRECT(&D_800B11AC.windows[i][0], 0, 0, 0xFF, 0xFF);
        setRECT(&D_800B11AC.windows[i][1], 0, 0, 0xFF, 0xFF);
        SetDrawMode(&D_800B11AC.modes[i][0], 0, 0, GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100),
                    &D_800B11AC.windows[i][0]);
        SetDrawMode(&D_800B11AC.modes[i][1], 0, 0, GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100),
                    &D_800B11AC.windows[i][1]);
        setRGB0(quad, 0x80, 0x80, 0x80);
        SetSemiTrans(quad, semitrans);
        quad->u0 = 0;
        quad->v0 = 0;
        quad->u1 = 0x40;
        quad->v1 = 0;
        quad->u2 = 0;
        quad->v2 = 0xDF;
        quad->u3 = 0x40;
        quad->v3 = 0xDF;
        quad->tpage = GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100);
        *copy = *quad;
    }
}

/* Present the current draw block: clear, set environments and draw its
 * overlay ordering table. */
void func_800A6924(void) {
    DrawSync(0);
    VSync(2);
    ClearImage(&D_800C426C->draw.clip, 0, 0, 0);
    PutDrawEnv(&D_800C426C->draw);
    PutDispEnv(&D_800C426C->disp);
    DrawOTag(&D_800C426C->overlay_ot[7]);
}

/* Darken corner `corner` of a grid quad by 6 (to 0) while it lies within
 * `radius` of the screen centre (a0, 70). */
void func_800A6998(POLY_GT4 *quad, s32 corner, s32 radius, s16 *shade) {
    VECTOR offset;
    VECTOR squares;
    s32 centre_x;
    s32 centre_y;

    centre_x = 0xA0;
    centre_y = 0x70;
    offset.vz = 0;
    switch (corner) {
    case 0:
        offset.vx = centre_x - quad->x0;
        offset.vy = centre_y - quad->y0;
        func_8004A414(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r0 = *shade;
            quad->g0 = *shade;
            quad->b0 = *shade;
        }
        break;
    case 1:
        offset.vx = centre_x - quad->x1;
        offset.vy = centre_y - quad->y1;
        func_8004A414(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r1 = *shade;
            quad->g1 = *shade;
            quad->b1 = *shade;
        }
        break;
    case 2:
        offset.vx = centre_x - quad->x2;
        offset.vy = centre_y - quad->y2;
        func_8004A414(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r2 = *shade;
            quad->g2 = *shade;
            quad->b2 = *shade;
        }
        break;
    case 3:
        offset.vx = centre_x - quad->x3;
        offset.vy = centre_y - quad->y3;
        func_8004A414(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r3 = *shade;
            quad->g3 = *shade;
            quad->b3 = *shade;
        }
        break;
    }
}

/* Fade the grid quads of the current buffer and link them, then the grid
 * draw mode, into the overlay ordering table. */
void func_800A6C40(void) {
    POLY_GT4 *quad;
    s32 row;
    s32 column;

    for (row = 0; row < GRID_ROWS; row++) {
        for (column = 0; column < GRID_COLUMNS; column++) {
            quad = &D_800B00C4->quads[D_800ADB08][row * GRID_COLUMNS + column];
            func_800A6998(quad, 0, D_800C3A40, &D_800B00C4->shade[0][row * GRID_COLUMNS + column]);
            func_800A6998(quad, 1, D_800C3A40, &D_800B00C4->shade[1][row * GRID_COLUMNS + column]);
            func_800A6998(quad, 2, D_800C3A40, &D_800B00C4->shade[2][row * GRID_COLUMNS + column]);
            func_800A6998(quad, 3, D_800C3A40, &D_800B00C4->shade[3][row * GRID_COLUMNS + column]);
            addPrim(&D_800C426C->overlay_ot[0], &D_800B00C4->quads[D_800ADB08][row * GRID_COLUMNS + column]);
        }
    }
    addPrim(&D_800C426C->overlay_ot[0], &D_800B1E24[D_800ADB08]);
}

/* Build the screen grid: 16x16 half-bright quads textured from the
 * 15-bit screen copy at (2c0, 100), their corners at full shade. */
void func_800A6E70(void) {
    POLY_GT4 *quad;
    POLY_GT4 *copy;
    s32 row;
    s32 column;
    s32 k;

    D_800B00C4 = func_80031BDC(sizeof(ScreenGrid), 1);
    for (row = 0; row < GRID_ROWS; row++) {
        for (column = 0; column < GRID_COLUMNS; column++) {
            k = row * GRID_COLUMNS + column;
            quad = &D_800B00C4->quads[0][k];
            copy = &D_800B00C4->quads[1][k];
            D_800B00C4->shade[0][k] = 0x80;
            D_800B00C4->shade[1][k] = 0x80;
            D_800B00C4->shade[2][k] = 0x80;
            D_800B00C4->shade[3][k] = 0x80;
            SetPolyGT4(quad);
            setRGB0(quad, 0x80, 0x80, 0x80);
            quad->r1 = 0x80;
            quad->g1 = 0x80;
            quad->b1 = 0x80;
            quad->r2 = 0x80;
            quad->g2 = 0x80;
            quad->b2 = 0x80;
            quad->r3 = 0x80;
            quad->g3 = 0x80;
            quad->b3 = 0x80;
            quad->x0 = column * 16;
            quad->y0 = row * 16;
            quad->x1 = column * 16 + 16;
            quad->y1 = row * 16;
            quad->x2 = column * 16;
            quad->y2 = row * 16 + 16;
            quad->x3 = column * 16 + 16;
            quad->y3 = row * 16 + 16;
            quad->u0 = (column * 16) & 0x3F;
            quad->v0 = row * 16;
            quad->u1 = ((column * 16) & 0x3F) + 16;
            quad->v1 = row * 16;
            quad->u2 = (column * 16) & 0x3F;
            quad->v2 = row * 16 + 16;
            quad->u3 = ((column * 16) & 0x3F) + 16;
            quad->v3 = row * 16 + 16;
            quad->tpage = GetTPage(2, 1, 0x2C0 + column / 4 * 0x40, 0x100);
            SetSemiTrans(quad, 1);
            *copy = *quad;
        }
    }
}

/* Release the screen grid. */
void func_800A7064(void) {
    func_800320E8(D_800B00C4);
}

extern s32 D_801D68B4;
extern void func_801D3538(s32 w, s32 h, s32, s32, s32, s32, s32 rgb24);

/* Set up the movie player for a 320x224 picture. */
void func_800A708C(void) {
    func_80032498(4, 0);
    if (D_800ADB74 == 2) {
        D_801D68B4 = 1;
    } else {
        D_801D68B4 = 0;
    }
    func_801D3538(0x140, 0xE0, 0x80, 0x10, 0x20, 0x800, D_800C3A36);
    D_800ADB6C = 0;
    func_80032498(8, 0);
}

/* Movie frame callback: record the frame, select the draw block for the
 * decoded buffer (24-bit display when set). */
void func_800A7120(u16 frame, s32 unused, u16 buffer) {
    D_800B06A0 = frame;
    D_800B00E4 = 0;
    if (buffer == 0) {
        D_800ADB78 = 1;
    } else {
        D_800ADB78 = 0;
    }
    if (D_800ADB74 == 0 && D_800AFE74 == 0) {
        DrawSync(0);
        D_800C426C = &D_800B249C[D_800ADB78];
        if ((s16)D_800C3A36 == 1) {
            D_800B249C[D_800ADB78 & 1].disp.isrgb24 = 1;
        }
    }
}

extern void func_801D37CC(s32 file, s32, s32, s32, s32, s32 mode, s32, s32, s32, s32, s32, s32 h,
                          void (*callback)(u16, s32, u16));

/* Start the field movie with the current parameters. */
void func_800A7218(void) {
    s32 mode;

    D_800B06A0 = 0;
    func_80032498(4, 0);
    if (D_800ADB6C == 0) {
        func_80028470(0x18, 1);
        mode = 1;
        if (D_800C3A38 != 0xFF || (D_800ADB80 & 0x40)) {
            mode = 3;
        }
        func_801D37CC(D_800C3A20 + 2, D_800C3A2A, D_800C3A2C, D_800C3A2E, 1, mode, D_800C3A3A, D_800C3A22,
                      D_800C3A24, D_800C3A26, D_800C3A28, 0xE0, func_800A7120);
        func_80028470(4, 0);
    }
    func_80032498(8, 0);
}

extern void func_80019CA0(void);
extern void func_801D3F7C(void);
void func_80085678(void);

/* Run `frames` movie frames (with field sound) unless the movie stopped. */
void func_800A732C(s32 frames) {
    s32 i;

    func_80019CA0();
    if (D_800ADB6C == 0) {
        for (i = 0; i < frames; i++) {
            func_801D3F7C();
            func_80085678();
        }
    }
}

/* Keep the field running until the stream is idle and draw buffer 0 is
 * current, then wait for CD data. */
void func_800A7394(void) {
    do {
        do {
            func_80077DAC();
            func_8007554C();
        } while (func_800286CC() != 0);
    } while (D_800ADB08 != 0);
    CdDataSync(0);
}

#include "field_movie.h"

/* After a movie: reload the VRAM kept in party sprite blocks 1 and 2
 * (unless movie mode 2), then release both blocks. */
void func_800A73E8(void) {
    RECT rect;

    if (D_800ADB74 == 2) {
        func_800320B8(D_8005A414[1]);
        func_800320B8(D_8005A41C);
        func_800320E8(D_8005A414[1]);
        func_800320E8(D_8005A41C);
    } else {
        setRECT(&rect, 0x200, 0, 0x140, 0x80);
        LoadImage(&rect, D_8005A414[1]);
        DrawSync(0);
        setRECT(&rect, 0x200, 0x80, 0x140, 0x80);
        LoadImage(&rect, D_8005A41C);
        DrawSync(0);
        func_800320B8(D_8005A414[1]);
        func_800320B8(D_8005A41C);
        func_800320E8(D_8005A414[1]);
        func_800320E8(D_8005A41C);
    }
}

/* Before a movie: allocate party sprite blocks 1 and 2 and save the VRAM
 * at (200, 0) in them; in movie mode 2 instead reload the sprites of party
 * slots 1 and 2 from their character files. */
void func_800A74F8(void) {
    RECT rect;
    MovieFileRequest requests[4];
    s32 count;
    s32 i;

    if (D_800ADB74 == 2) {
        D_8005A414[1] = func_80031BDC(0x14000, 0);
        D_8005A414[2] = func_80031BDC(0x14000, 0);
        func_800320A4(D_8005A414[1]);
        func_800320A4(D_8005A414[2]);
        func_80028470(4, 0);
        count = 0;
        for (i = 1; i < 3; i++) {
            if (D_8006FABC[i] != 0xFF) {
                requests[count].file = D_8006FABC[i] + 5;
                requests[count].destination = D_80065AFC[i] = func_80031BDC(func_800288EC(D_8006FABC[i] + 5), 1);
                count++;
            }
        }
        requests[count].destination = NULL;
        requests[count].file = 0;
        func_80029AFC(requests, 0, 0);
        func_80028A60(0);
        for (i = 1; i < 3; i++) {
            if (D_80062590[i] != 0xFF) {
                func_80032EB4(D_80065AFC[i], D_8005A414[i]);
                func_800320E8(D_80065AFC[i]);
            }
        }
        return;
    }
    D_8005A414[1] = func_80031BDC(0x14000, 0);
    D_8005A414[2] = func_80031BDC(0x14000, 0);
    func_800320A4(D_8005A414[1]);
    func_800320A4(D_8005A414[2]);
    setRECT(&rect, 0x200, 0, 0x140, 0x80);
    StoreImage(&rect, D_8005A414[1]);
    DrawSync(0);
    setRECT(&rect, 0x200, 0x80, 0x140, 0x80);
    StoreImage(&rect, D_8005A414[2]);
    DrawSync(0);
}

extern s32 D_800B14A8; /* nibble counter */
extern u32 *D_800C3904; /* packed stream */
extern u32 D_800C2688; /* current word */

/* Next byte of the packed stream (four per word), scaled down by 8 but
 * at least 1 when nonzero. */
u32 func_800A7744(void) {
    u32 value;

    if ((D_800B14A8 & 3) == 0) {
        D_800C2688 = *D_800C3904;
        D_800C3904++;
    }
    D_800B14A8++;
    value = D_800C2688 & 0xFF;
    D_800C2688 >>= 8;
    if (value != 0) {
        value >>= 3;
        if (value == 0) {
            value = 1;
        }
    }
    return value;
}

extern u32 *D_800C390C; /* converted pixels */

/* Convert the 24-bit screen (five 96-pixel columns at x 0) to 15-bit
 * pixels at (0..0x140, 100). */
void func_800A77C4(void) {
    RECT rect;
    u32 *packed;
    u32 *pixels;
    u32 value;
    s32 i;
    s32 j;

    packed = func_80031BDC(0xA800, 0);
    pixels = func_80031BDC(0x7000, 0);
    for (i = 0; i < 5; i++) {
        rect.x = i * 0x60;
        rect.y = 0;
        rect.w = 0x60;
        rect.h = 0xE0;
        StoreImage(&rect, packed);
        DrawSync(0);
        D_800C3904 = packed;
        D_800C390C = pixels;
        D_800B14A8 = 0;
        for (j = 0; j < 0x1C00; j++) {
            value = func_800A7744();
            value |= func_800A7744() << 5;
            value |= func_800A7744() << 10;
            value |= func_800A7744() << 16;
            value |= func_800A7744() << 21;
            value |= func_800A7744() << 26;
            *D_800C390C = value;
            D_800C390C++;
        }
        rect.x = i << 6;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0xE0;
        LoadImage(&rect, pixels);
        DrawSync(0);
    }
    func_800320E8(packed);
    func_800320E8(pixels);
}

/* While movie frames 687..18e2 play (when the sequence is enabled),
 * switch to 640-wide draw buffers and draw the file 0xab sequence over
 * the movie each frame, then restore the 320-wide buffers. Declared int
 * without a value, as the original's live result register shows. */
s32 func_800A7948(void) {
    RECT rect;

    if (D_8004F300 == 0 || D_800B06A0 < 0x687) {
        return;
    }
    if (D_800B06A0 < 0x18E2) {
        D_800AFE74 = 1;
        D_801E89E0 = 0;
        setRECT(&rect, 0, 0, 0x500, 0x200);
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x280, 0xE0);
        SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x280, 0xE0);
        D_800B249C[1].disp.isrgb24 = 0;
        D_800B249C[0].disp.isrgb24 = 0;
        PutDispEnv(&D_800C426C->disp);
        PutDrawEnv(&D_800C426C->draw);
        setRECT(&rect, 0x300, 0, 0x200, 0x100);
        ClearImage(&rect, 0, 0, 0);
        func_800ACB90();
        VSync(0);
        DrawSync(0);
        while (D_800B06A0 < 0x18E2) {
            func_80019CA0();
            func_80073F50();
            if (D_800B06A0 < 0x18DE) {
                func_800AC99C();
            }
            DrawSync(0);
            VSync(2);
            ClearImage(&D_800C426C->draw.clip, 0, 0, 0);
            PutDispEnv(&D_800C426C->disp);
            PutDrawEnv(&D_800C426C->draw);
            DrawOTag(&D_800C426C->overlay_ot[7]);
            func_800ACCF4();
            func_800A732C(5);
        }
        D_800AFE74 = 0;
        D_801E89E0 = 1;
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x140, 0xE0);
        SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x140, 0xE0);
        SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x140, 0xE0);
        SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x140, 0xE0);
        D_800B249C[1].disp.isrgb24 = 1;
        D_800B249C[0].disp.isrgb24 = 1;
        D_800B249C[1].disp.isinter = 0;
        D_800B249C[0].disp.isinter = 0;
        func_800ACCB0();
    }
}

#ifdef NON_MATCHING
/* Play the requested field movie (800c3a20..): move the movie library
 * (file 0xa9) into place, park the VRAM the movie uses, stop the field's
 * effects and the 801e module, then decode and present frames by movie
 * mode until it ends or is skipped; finally restore VRAM, the display and
 * the field stream. The original addresses the movie parameters as one
 * aggregate (800c3a3a and 800c3a2e off one hoisted base), which field.h
 * does not declare yet. */
void func_800A7C58(void) {
    RECT rect;
    u8 *data;
    u8 *library;
    s32 top;
    s32 i;

    D_800ADB84 = 0;
    D_800B00E4 = 0;
    D_800ADB78 = 0;
    data = func_80031BDC(func_800288EC(0xA9), 0);
    func_800295D8(0xA9, data, 0, 0x80);
    D_800B06A0 = 0;
    D_800AFE74 = 0;
    func_800A7394();
    func_80028470(0x18, 0);
    func_8002A2D0(D_800C3A20);
    func_80028470(4, 0);
    func_800A7394();
    if (D_800B2078.unk2264 != 0) {
        func_801E7FD4();
        func_8007999C();
        func_800775F8();
        func_800320E8(D_800ADB20);
    }
    func_800A9460();
    if (D_800ADB74 != 2) {
        setRECT(&rect, 0x140, 0, 0xC0, 0x100);
        LoadImage(&rect, (u_long *)data);
        DrawSync(0);
        func_800320E8(data);
        data = func_80031BDC(0x18000, 0);
        StoreImage(&rect, (u_long *)data);
    }
    top = D_800ADB30;
    if (D_8004F370 == 0) {
        library = func_80031BDC((top & 0xFFFFFF) - 0x1D3008, 1);
        memcpy(library, data, func_800288EC(0xA9));
    } else {
        library = func_80031BDC(8, 1);
    }
    func_800320E8(data);
    func_80085788();
    func_800ACC58();
    D_800B00E4 = 1;
    func_800A73E8();
    func_8007999C();
    func_80031FF8();
    func_800A708C();
    func_800A7218();
    D_800ADB7C = 1;
    do {
        VSync(0);
        func_800A732C(3);
    } while (D_800B00E4 != 0);
    do {
        switch (D_800ADB74) {
        case 1:
            func_80073F50();
            func_80075910();
            func_800A732C(6);
            break;
        case 0:
            DrawSync(0);
            VSync(0);
            PutDispEnv(&D_800C426C->disp);
            PutDrawEnv(&D_800C426C->draw);
            func_800A732C(3);
            DrawSync(0);
            VSync(0);
            PutDispEnv(&D_800C426C->disp);
            PutDrawEnv(&D_800C426C->draw);
            func_800A732C(3);
            func_800A7948();
            break;
        case 2:
            func_80077DAC();
            func_8007554C();
            func_800A732C(9);
            break;
        }
        if (D_800C268C == 0) {
            if (D_800ADB74 == 2) {
                if ((D_800C3900 & 0x80) || D_800ADB84 != 0) {
                    break;
                }
            } else {
                func_80074700();
                if (D_800C3900 & 0x20) {
                    break;
                }
            }
        } else if (D_800ADB80 & 0x80) {
            func_80074700();
            if (D_800C3900 & 0x20) {
                func_80038D18(0, 10);
                for (i = 0; i < 5; i++) {
                    VSync(0);
                }
                break;
            }
        }
        if (D_800ADB74 == 2 && D_800ADB84 != 0) {
            break;
        }
    } while ((s16)D_800C3A3A != 0 || D_800B06A0 < D_800C3A2E);
    VSync(0);
    DrawSync(0);
    func_801D43B0();
    func_8007999C();
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    VSync(0);
    DrawSync(0);
    func_800320E8(library);
    func_80031FF8();
    func_800A74F8();
    func_80077884();
    setRECT(&rect, 0, D_800ADB78 << 8, 0x1E0, 0xE0);
    MoveImage(&rect, 0, 0);
    DrawSync(0);
    VSync(0);
    D_800C426C = &D_800B249C[1];
    PutDispEnv(&D_800B249C[1].disp);
    PutDrawEnv(&D_800C426C->draw);
    if (D_800ADB74 != 2) {
        func_800A77C4();
    }
    VSync(0);
    D_800B249C[0].disp.isrgb24 = 0;
    D_800C426C = &D_800B249C[0];
    PutDispEnv(&D_800B249C[0].disp);
    PutDrawEnv(&D_800C426C->draw);
    setRECT(&rect, 0, 0x100, 0x140, 0xE0);
    MoveImage(&rect, 0, 0);
    DrawSync(0);
    VSync(0);
    D_800B249C[1].disp.isrgb24 = 0;
    D_800C426C = &D_800B249C[1];
    PutDispEnv(&D_800B249C[1].disp);
    PutDrawEnv(&D_800C426C->draw);
    if (D_800AFE84 != 0) {
        D_800ADB50 = 1;
    } else {
        D_800ADB50 = 0;
    }
    func_80077AB4();
    if (D_800ADB74 != 2) {
        func_80028470(4, 0);
        func_80032498(8, 0);
        D_800ADB60 = 0;
        func_80070488();
        func_80070508();
        D_800ADB3C = 0x20;
        D_800ADB38 = 1;
    }
    func_80085738();
    D_800C3A38 = 0xFF;
    D_800ADB74 = 0;
    D_800ADB6C = -1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7C58);
#endif

void func_80070340(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

/* Load file 0xaa and upload its image to (380, 0) with its CLUT at
 * (0, e8). */
void func_800A8314(void) {
    u32 *data;

    func_80032498(8, 0);
    func_80028470(4, 0);
    data = func_80031BDC(func_800288EC(0xAA), 1);
    func_800295D8(0xAA, data, 0, 0x80);
    func_80028A60(0);
    func_80070340(data, 0x380, 0, 0, 0xE8, 0, 0);
    DrawSync(0);
    func_800320E8(data);
}

extern s32 D_800AF278;
extern POLY_FT4 *D_800AFC60[2];

/* Release the two primitive buffers once allocated. */
void func_800A83B4(void) {
    if (D_800AF278 != 0) {
        D_800AF278 = 0;
        DrawSync(0);
        func_800320E8(D_800AFC60[0]);
        func_800320E8(D_800AFC60[1]);
    }
}

#include "field_panel.h"

#ifdef NON_MATCHING
/* Texture panel piece `index` of the current buffer with its frame moved
 * by (du, dv), flipped vertically. The original passes the coordinates to
 * 8007a44c unconverted (no s16 prototype in scope: a separate unit). */
void func_800A8408(s32 index, s32 du, s32 dv) {
    s32 frame;
    s32 u;
    s32 v;
    s32 w;
    s32 h;
    s32 unused[6]; /* the original frame reserves an unused 24-byte local */

    frame = D_800AEF10[index].frame;
    v = D_800AEB68[frame].v + dv;
    w = D_800AEB68[frame].w;
    u = D_800AEB68[frame].u + du;
    h = D_800AEB68[frame].h;
    func_8007A44C(&D_800AFC60[D_800ADB08][index], u, v + h - 1, u + w, v + h - 1, u, v - 1, u + w, v - 1);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8408);
#endif

/* Draw the status panel: rotate the compass strip by the view angle, the
 * pitch strip by the camera pitch, their digits, the blinking marker and
 * the rest of the pieces. */
void func_800A84C0(void) {
    s32 pitch;
    s32 angle;
    s32 count;
    s32 i;

    if (D_800AF278 != 0) {
        count = PANEL_PIECES;
        pitch = ratan2(func_80099A4C((D_800AF880.target.vx - D_800AF880.eye.vx) >> 16,
                                     (D_800AF880.target.vz - D_800AF880.eye.vz) >> 16),
                       (D_800AF880.target.vy - D_800AF880.eye.vy) >> 16) &
                0xFFF;
        angle = D_800AF880.view_angle & 0xFFF;
        for (i = 0; i < 16; i++) {
            func_800A8408(i, (angle >> 4) & 0xF, 0);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 29; i++) {
            func_800A8408(i, 0, (pitch >> 4) & 0xF);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 34; i++) {
            func_800A8408(i, (angle & 0xF) * 8, 0);
            angle >>= 2;
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 39; i++) {
            func_800A8408(i, (pitch & 0xF) * 8, 0);
            pitch >>= 2;
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 40; i++) {
            if (!(D_800AEB60 & 0xF)) {
                D_800AEB64++;
            }
            if (D_800AEB64 >= 3) {
                D_800AEB64 = 0;
            }
            func_800A8408(i, 0, D_800AEB64 * 8);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 42; i++) {
            func_800A8408(i, (D_800AEB60 >> 2) & 0xF, 0);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 43; i++) {
            if (!(D_800AEB60 & 0x10)) {
                addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
            }
        }
        for (; i < count; i++) {
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        D_800AEB60++;
    }
}

#ifdef NON_MATCHING
/* Build the status panel: load its image, allocate the quads of both
 * buffers and texture each piece from its frame (flip in flags bits 0-3,
 * 4- or 8-bit page by bits 4-7). The original passes the coordinates to
 * 8007a44c unconverted (no s16 prototype in scope: a separate unit). */
void func_800A8BA4(void) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    PanelFrame *frame;
    s32 mode;
    s32 i;

    func_800A8314();
    mode = 0;
    D_800AEB64 = 0;
    func_80032498(8, 0);
    D_800AFC60[0] = func_80031BDC(PANEL_PIECES * sizeof(POLY_FT4), 0);
    D_800AFC60[1] = func_80031BDC(PANEL_PIECES * sizeof(POLY_FT4), 0);
    for (i = 0; i < PANEL_PIECES; i++) {
        quad = &D_800AFC60[0][i];
        copy = &D_800AFC60[1][i];
        SetPolyFT4(quad);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad->clut = GetClut(0, 0xE8);
        switch ((D_800AEF10[i].flags >> 4) & 0xF) {
        case 0:
            mode = 1;
            break;
        case 1:
            mode = 2;
            break;
        }
        quad->tpage = GetTPage(0, mode, 0x380, 0);
        SetSemiTrans(quad, 1);
        frame = &D_800AEB68[D_800AEF10[i].frame];
        quad->x0 = D_800AEF10[i].x;
        quad->y0 = D_800AEF10[i].y;
        quad->y1 = D_800AEF10[i].y;
        quad->x2 = D_800AEF10[i].x;
        quad->x1 = D_800AEF10[i].x + frame->w;
        quad->y2 = D_800AEF10[i].y + frame->h;
        quad->x3 = D_800AEF10[i].x + frame->w;
        quad->y3 = D_800AEF10[i].y + frame->h;
        switch (D_800AEF10[i].flags & 0xF) {
        case 0:
            func_8007A44C(quad, frame->u, frame->v, frame->u + frame->w, frame->v, frame->u,
                          frame->v + frame->h, frame->u + frame->w, frame->v + frame->h);
            break;
        case 1:
            func_8007A44C(quad, frame->u + frame->w - 1, frame->v, frame->u - 1, frame->v,
                          frame->u + frame->w - 1, frame->v + frame->h, frame->u - 1, frame->v + frame->h);
            break;
        case 2:
            func_8007A44C(quad, frame->u, frame->v + frame->h - 1, frame->u + frame->w, frame->v + frame->h - 1,
                          frame->u, frame->v - 1, frame->u + frame->w, frame->v - 1);
            break;
        case 3:
            func_8007A44C(quad, frame->u + frame->w - 1, frame->v + frame->h - 1, frame->u - 1,
                          frame->v + frame->h - 1, frame->u + frame->w - 1, frame->v - 1, frame->u - 1,
                          frame->v - 1);
            break;
        }
        *copy = *quad;
    }
    D_800AF278 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8BA4);
#endif

/* A particle sprite (800af27c): half size, centre, and the four texture
 * corners (u, v) within the sprite page. */
typedef struct {
    s16 half_w;
    s16 half_h;
    s16 x;
    s16 y;
    s16 uv[4][2];
} ParticleSprite;
extern ParticleSprite D_800AF27C[];

#ifdef NON_MATCHING
/* Set up a particle's quad in both buffers from sprite `sprite` with
 * semi-transparency rate `abr`. The original passes the coordinates to
 * 8007a44c unconverted (no s16 prototype in scope: a separate unit). */
void func_800A8EAC(Particle *particle, s32 sprite, s32 abr) {
    POLY_FT4 *quad;
    ParticleSprite *entry;

    quad = &particle->quads[0];
    SetPolyFT4(quad);
    entry = &D_800AF27C[sprite];
    particle->corners[0].vz = 0;
    particle->corners[1].vz = 0;
    particle->corners[2].vz = 0;
    particle->corners[3].vz = 0;
    setRGB0(quad, 0x80, 0x80, 0x80);
    particle->corners[0].vx = entry->x * 16 - entry->half_w * 16;
    particle->corners[0].vy = entry->y * 16 - entry->half_h * 16;
    particle->corners[1].vx = entry->half_w * 16 + entry->x * 16;
    particle->corners[1].vy = particle->corners[0].vy;
    particle->corners[2].vx = particle->corners[0].vx;
    particle->corners[2].vy = entry->half_h * 16 + entry->y * 16;
    particle->corners[3].vx = particle->corners[1].vx;
    particle->corners[3].vy = particle->corners[2].vy;
    func_8007A44C(quad, entry->uv[0][0], entry->uv[0][1] + 0x40, entry->uv[1][0] - 1, entry->uv[1][1] + 0x40,
                  entry->uv[2][0], entry->uv[2][1] + 0x3F, entry->uv[3][0] - 1, entry->uv[3][1] + 0x3F);
    SetSemiTrans(quad, 1);
    quad->tpage = GetTPage(0, abr, 0x3C0, 0x140);
    quad->clut = GetClut(0x100, 0xF7);
    particle->quads[1] = *quad;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8EAC);
#endif

extern RECT D_800AFC28;
typedef struct {
    u32 words[0x2000];
} ScreenColumn; /* a 64x256 16-bit VRAM column */
extern ScreenColumn *D_800AFC70; /* saved screen column */

/* Move the saved screen column into a new block allocated with `flags`. */
void func_800A90B4(s32 flags) {
    ScreenColumn *copy;

    if (D_800ADB34 == 1) {
        func_80032498(8, 0);
        copy = func_80031BDC(0x8000, flags);
        *copy = *D_800AFC70;
        func_800320E8(D_800AFC70);
        D_800AFC70 = copy;
    }
}

/* Save the 64x256 VRAM column at (3c0, 100) once. */
void func_800A915C(void) {
    if (D_800ADB34 != 1) {
        D_800ADB34 = 1;
        func_80032498(8, 0);
        D_800AFC70 = func_80031BDC(0x8000, 1);
        D_800AFC28.x = 0x3C0;
        D_800AFC28.y = 0x100;
        D_800AFC28.w = 0x40;
        D_800AFC28.h = 0x100;
        StoreImage(&D_800AFC28, D_800AFC70->words);
        DrawSync(0);
    }
}

/* Restore the saved VRAM column and release it. */
void func_800A91F0(void) {
    if (D_800ADB34 != 0) {
        D_800AFC28.x = 0x3C0;
        D_800ADB34 = 0;
        D_800AFC28.y = 0x100;
        D_800AFC28.w = 0x40;
        D_800AFC28.h = 0x100;
        LoadImage(&D_800AFC28, D_800AFC70->words);
        DrawSync(0);
        func_800320E8(D_800AFC70);
    }
}

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
    s32 unused[1]; /* the original frame reserves an unused local */

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

#ifdef NON_MATCHING
/* Step a particle: while delayed count down and at launch place it and
 * its velocity in the emitter's frame (0 owner-facing, 1 801e module, 2
 * owner's transform, 3 owner-facing and scaled); afterwards move it,
 * fade its colour, draw it and count its life down. Differs in keeping
 * &m in a saved register for the module and transform frames. */
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
                goto place;
            case 2:
                m = D_800AF880.components.descriptors[emitter->unk52].transform;
            place:
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
            func_80048D7C(&v, &particle->velocity);
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
                v.vy = rotated.vy;
                v.vz += rotated.vz;
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
        particle->unk38.vy += particle->unk40.vy;
        particle->unk38.vx += particle->unk40.vx;
        particle->unk38.vz += particle->unk40.vz;
        particle->position.vx += particle->velocity.vx;
        particle->position.vy += particle->velocity.vy;
        particle->position.vz += particle->velocity.vz;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9F18);
#endif

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

#ifdef NON_MATCHING
/* Set up the picture: four marker sprites (the first 16x16, the rest
 * 8x8) and the three 128x224 picture pieces from the 8-bit pages at
 * (300, 100). Differs only in the scheduling of one constant load. */
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
        quad->y2 = 0xDF;
        quad->x0 = i << 7;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AAF80);
#endif

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
 * clear to the 8-bit page area at (300, 100). The original keeps one
 * pointer per piece field and spills most of its variables. */
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
                for (row = 0; row < D_800AF5C0[i].h; row++) {
                    memcpy(row_pixels, tim.paddr + (D_800AF5C0[i].y + row) * 0x50 + D_800AF5C0[i].x / 4,
                           D_800AF5C0[i].w);
                    row_pixels += D_800AF5C0[i].w / 4 * 4;
                }
                tim.prect->x = D_800AF5C0[i].x / 2 + 0x300;
                tim.prect->y = D_800AF5C0[i].y + 0x100;
                tim.prect->w = D_800AF5C0[i].w / 2;
                tim.prect->h = D_800AF5C0[i].h;
                LoadImage(tim.prect, (u_long *)pixels);
                DrawSync(0);
            }
        }
    }
    func_800320E8(file);
    func_800320E8(pixels);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AB808);
#endif

#ifdef NON_MATCHING
/* Show the current map's picture while its item is held: park the VRAM
 * at (300, 100), load the picture, fade it in, hold until the button, fade
 * it out and restore the VRAM. Differs in addressing the entry: the
 * original keeps the table base and the entry offset apart. */
void func_800ABA98(void) {
    RECT rect;
    Picture *table;
    Picture *entry;
    u8 *saved;
    u32 *file;
    s32 picture;
    s32 i;

    table = D_800AF47C;
    i = 0;
    entry = table;
    for (;;) {
        if (entry->map == 0xFFFF) {
            return;
        }
        if ((D_8004F34C & 0x3FFF) == entry->map) {
            break;
        }
        entry++;
        i++;
    }
    if (func_800AB328(table[i].item) == -1) {
        return;
    }
    picture = table[i].picture;
    setRECT(&rect, 0x300, 0x100, 0xA0, 0x100);
    D_800C3914 = table[i].unk04;
    D_800C3A18 = table[i].unk08;
    D_800AFE78 = table[i].x;
    D_800AFE7C = table[i].y;
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
    if (table[i].pieces == 1) {
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ABA98);
#endif

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
    return (s32)func_800405C4(code);
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

#ifdef NON_MATCHING
/* Set up the text roll: the white top and bottom fades (640 wide, 24
 * tall at 0 and c8) and the 16 lines of sprites. Differs only in when
 * the scheduler loads the constants 0x280 and 0x80. */
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
    D_800AF788[0][0].y3 = D_800AF788[0][0].y2 = 0x18;
    D_800AF788[0][0].r2 = D_800AF788[0][0].g2 = D_800AF788[0][0].b2 = 0;
    D_800AF788[0][0].r3 = D_800AF788[0][0].g3 = D_800AF788[0][0].b3 = 0;
    D_800AF788[1][0].r0 = D_800AF788[1][0].g0 = D_800AF788[1][0].b0 = 0;
    D_800AF788[1][0].r1 = D_800AF788[1][0].g1 = D_800AF788[1][0].b1 = 0;
    D_800AF788[0][0].x0 = 0;
    D_800AF788[0][0].y0 = 0;
    D_800AF788[0][0].x1 = 0x280;
    D_800AF788[0][0].y1 = 0;
    D_800AF788[0][0].x2 = 0;
    D_800AF788[0][0].x3 = 0x280;
    D_800AF788[1][0].x0 = 0;
    D_800AF788[1][0].y0 = 0xC8;
    D_800AF788[1][0].y1 = 0xC8;
    D_800AF788[1][0].y2 = 0xE0;
    D_800AF788[1][0].y3 = 0xE0;
    D_800AF788[1][0].x1 = 0x280;
    D_800AF788[1][0].x2 = 0;
    D_800AF788[1][0].x3 = 0x280;
    D_800AF788[0][0].u0 = 0;
    D_800AF788[0][0].v0 = 0;
    D_800AF788[0][0].u1 = 2;
    D_800AF788[0][0].v1 = 0;
    D_800AF788[0][0].u2 = 0;
    D_800AF788[0][0].v2 = 2;
    D_800AF788[0][0].u3 = 2;
    D_800AF788[0][0].v3 = 2;
    D_800AF788[1][0].u0 = 0;
    D_800AF788[1][0].v0 = 0;
    D_800AF788[1][0].u1 = 2;
    D_800AF788[1][0].v1 = 0;
    D_800AF788[1][0].u2 = 0;
    D_800AF788[1][0].v2 = 2;
    D_800AF788[1][0].u3 = 2;
    D_800AF788[1][0].v3 = 2;
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
        sprite->h = 0x10;
        sprite->u0 = 0;
        sprite->v0 = i * 16;
        sprite->w = 0x80;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AC3AC);
#endif

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

#ifdef NON_MATCHING
/* Return party slot `slot` to its member: swap the models back, hand the
 * stand-in's heading over, and restart both animations. Differs in when
 * the address of 8006f990 is formed. */
void func_800ACFD0(s32 slot) {
    FieldModel *model;

    D_8005A39C->unk22B1[slot] = 0;
    model = DESCRIPTOR(D_8005A444[slot])->model;
    DESCRIPTOR(D_8005A444[slot])->model = DESCRIPTOR(D_8006F990[slot])->model;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACFD0);
#endif

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
