#include "common.h"
#include "field.h"

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8006FDEC);

/* Decode bundle component `index` into `destination`. */
void func_8007008C(s32 unused, s32 index, void *destination) {
    func_80032EB4(index, destination);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800700B0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80070340);

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
        func_800445D0(0);
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
    if (D_800B20C4[channel].active != 0) {
        if (D_800B20C4[channel].steps <= 0) {
            D_800B20C4[channel].steps = 0;
            if (D_800ADC08 != 1 && D_800B20C4[channel].level[0] == 0 &&
                D_800B20C4[channel].level[1] == 0 && D_800B20C4[channel].level[2] == 0) {
                D_800B20C4[channel].active = 0;
            }
        } else {
            D_800B20C4[channel].level[0] = D_800B20C4[channel].level[0] + D_800B20C4[channel].step[0];
            if (D_800B20C4[channel].level[0] >> 8 >= 0x100) {
                D_800B20C4[channel].level[0] = 0xFF00;
            }
            if (D_800B20C4[channel].level[0] < 0) {
                D_800B20C4[channel].level[0] = 0;
            }
            D_800B20C4[channel].level[1] = D_800B20C4[channel].level[1] + D_800B20C4[channel].step[1];
            if (D_800B20C4[channel].level[1] >> 8 >= 0x100) {
                D_800B20C4[channel].level[1] = 0xFF00;
            }
            if (D_800B20C4[channel].level[1] < 0) {
                D_800B20C4[channel].level[1] = 0;
            }
            D_800B20C4[channel].level[2] = D_800B20C4[channel].level[2] + D_800B20C4[channel].step[2];
            if (D_800B20C4[channel].level[2] >> 8 >= 0x100) {
                D_800B20C4[channel].level[2] = 0xFF00;
            }
            if (D_800B20C4[channel].level[2] < 0) {
                D_800B20C4[channel].level[2] = 0;
            }
            D_800B20C4[channel].steps = D_800B20C4[channel].steps - 1;
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
    s32 red_step = ((red << 8) - D_800B20C4[channel].level[0]) / steps;
    s32 green_step = ((green << 8) - D_800B20C4[channel].level[1]) / steps;
    s32 blue_step = ((blue << 8) - D_800B20C4[channel].level[2]) / steps;

    D_800B20C4[channel].steps = steps;
    D_800B20C4[channel].active = 1;
    D_800B20C4[channel].abr = abr;
    D_800B20C4[channel].step[0] = red_step;
    D_800B20C4[channel].step[1] = green_step;
    D_800B20C4[channel].step[2] = blue_step;
}

/* Fade channel 0 out to white over `steps` frames, once. */
void func_80071DCC(s32 steps) {
    s32 rate;

    if (D_800ADC08 != 1) {
        D_800ADC08 = 1;
        if (D_800ADC04 == 2) {
            rate = 0xFF00 / steps;
            D_800B20C4[0].level[0] = D_800B20C4[0].level[1] = D_800B20C4[0].level[2] = 0;
            D_800B20C4[0].steps = steps;
            D_800B20C4[0].active = 1;
            D_800B20C4[0].abr = 2;
            D_800B20C4[0].step[0] = D_800B20C4[0].step[1] = D_800B20C4[0].step[2] = rate;
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
            D_800B20C4[0].level[0] = D_800B20C4[0].level[1] = D_800B20C4[0].level[2] = 0xFF00;
            D_800B20C4[0].active = 1;
            D_800B20C4[0].steps = steps;
            D_800B20C4[0].abr = 2;
            D_800B20C4[0].step[0] = D_800B20C4[0].step[1] = D_800B20C4[0].step[2] = rate;
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80071FB0);

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
    func_8004931C(&D_800AF880.orbit, &D_800AF880.previous_view, &composed);
    func_80074038(&D_800AF880.previous_view, &composed);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.world_matrix);
    func_80072140(&D_800AF880.world_matrix);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.scaled_world);
    func_80049BDC(&D_800AF880.previous_view, &D_800AF880.scaled_world);
    func_80049EFC(&D_800AF880.previous_view);
    func_80049F8C(&D_800AF880.previous_view);
    func_8004A6DC(&D_800AF880.anchor, D_800AF880.scaled_world.t, &flag);
    scale.vx = D_800AF880.scale;
    scale.vy = D_800AF880.scale;
    scale.vz = D_800AF880.scale;
    func_80049DCC(&D_800AF880.scaled_world, &scale);
    func_80049EFC(&D_800AF880.scaled_world);
    func_80049F8C(&D_800AF880.scaled_world);
}

/* Rebuild a descriptor's matrix from its rotation, scaled by its actor. */
void func_80072254(s32 index) {
    VECTOR scale;

    scale.vx = D_800AFB0C.descriptors[index].actor->scale[0];
    scale.vy = D_800AFB0C.descriptors[index].actor->scale[1];
    scale.vz = D_800AFB0C.descriptors[index].actor->scale[2];
    func_8003F738(&D_800AFB0C.descriptors[index].rotation, &D_800AFB0C.descriptors[index].matrix);
    func_80049DCC(&D_800AFB0C.descriptors[index].matrix, &scale);
}

/* Compose the view and reload the scaled world matrix; 802815b0 runs unless
 * 800c268c is set. */
void func_800722F4(void) {
    func_80072150();
    func_80049EFC(&D_800AF880.scaled_world);
    func_80049F8C(&D_800AF880.scaled_world);
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800723E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007254C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800726E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80072A38);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80072D74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80073230);

/* Rotate `point` in X/Z about `center` by the camera heading angles. */
void func_80073684(VECTOR *point, VECTOR *center) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;

    func_8004960C();
    func_8003F738(&D_800AF880.heading_angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    func_8004947C(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    func_800496AC();
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80073E38);

#ifdef NON_MATCHING
/* Switch to the other draw block and clear its overlay ordering table. */
void func_80073F50(void) {
    if (D_800C268C == 0) {
        __asm__("break 1");
    }
    D_800ADB08 = (D_800ADB08 + 1) % 2;
    D_800C426C = &D_800B249C[D_800ADB08];
    func_80044AD8(D_800C426C->overlay_ot, 8);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80073F50);
#endif

/* Swap the draw buffer and clear its ordering tables. */
void func_80073FE0(void) {
    func_80073F50();
    func_80044AD8(D_800C426C->ot, 0x1000);
    if (D_800ADB4C != 0) {
        func_80044AD8(D_800C426C->ot2, 0x1000);
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

    for (i = 0; i < D_800AFB0C.descriptor_count; i++) {
        flags = D_800AFB0C.descriptors[i].flags;
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007520C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800752C8);

/* Link a table's primitives into `ot` (AddPrims). */
void func_80075458(void *ot, u32 *table, s32 depth) {
    func_80043B84(ot, table + depth, table);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80075484);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007554C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80075910);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800759E4);

/* Pass a colour on to resident 80021b98 unless 800b218e is set. */
void func_80075B08(void *target, u8 *color) {
    if (D_800B218E == 0) {
        func_80021B98(target, color[0], color[1], color[2]);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80075B44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800764B4);

/* Sprite completion callback: flag the sprite's actor (layer bit 16). */
void func_80076A74(FieldSprite *sprite) {
    D_800AFB0C.descriptors[sprite->sequencer->actor].actor->layer_flags |= 0x10000;
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

    func_800471B4(tim);
    while (func_800471C4(&image) != NULL) {
        if (image.caddr != NULL) {
            func_80044894(image.crect, image.caddr);
        }
        if (image.paddr != NULL) {
            func_80044894(image.prect, image.paddr);
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
    func_800445D0(0);
    func_8004B54C(0);
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
    D_800ADBA4 = func_8004B54C(1);
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80077C88);

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

    if (D_800ADBD0 == 1 && D_800B21AC.unk2344 == 0) {
        result = -((D_800AFB0C.descriptors[D_800B21AC.controlled].actor->flags & 0x800) != 0);
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
    func_8003FA38();
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80078C5C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80078D44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80079288);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007954C);

void func_800796F4(void) {
}

/* Switch to the other draw block and put its display and draw environments. */
void func_800796FC(void) {
    D_800ADB08 = (D_800ADB08 + 1) % 2;
    D_800C426C = &D_800B249C[D_800ADB08];
    func_80044E9C(&D_800C426C->disp);
    func_80044C44(&D_800C426C->draw);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80079784);

/* Set the battle-entry flag (80059179): clear only while the controlled
 * actor has neither bit 0x40 nor 0x80 of +14; 800b234c overrides it. */
void func_800798BC(void) {
    if (D_800B21AC.unk2268 != 0 && !(D_800AFB0C.descriptors[D_800B21AC.controlled].actor->unk014 & 0xC0)) {
        D_80059179 = 0;
    } else {
        D_80059179 = 1;
    }
    if (D_800B21AC.battle_override != 0xFF) {
        D_80059179 = D_800B21AC.battle_override;
    }
}

/* Move a VRAM rectangle (MoveImage) and wait for it. */
void func_8007995C(s32 w, s32 h, s32 x, s32 y, s32 to_x, s32 to_y) {
    RECT rect;

    rect.w = w;
    rect.h = h;
    rect.x = x;
    rect.y = y;
    func_8004495C(&rect, to_x, to_y);
    func_800445D0(0);
}

/* Sync, then flush the instruction cache inside a critical section. */
void func_8007999C(void) {
    func_800775F8();
    func_800404D4();
    func_80040454();
    func_800404E4();
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800799D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007A44C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007A5C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007A7F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007AA44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007AB6C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007AC58);

/* Set the pointer's two pad buffers. */
void func_8007AD8C(void *pad0, void *pad1) {
    D_800B0054 = pad0;
    D_800B0058 = pad1;
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007AE78);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007AF74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B07C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B1C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B478);

#ifdef NON_MATCHING
/* The X/Z offset `distance` away at `angle`, scaled by 800b218c. */
void func_8007B614(VECTOR *out, s32 distance, s32 angle) {
    s32 length;

    distance *= 16;
    angle &= 0xFFF;
    length = (distance * D_800B218C) >> 12;
    out->vx = func_8003F8CC(angle) * length;
    out->vy = 0;
    out->vz = -(func_8003F8B0(angle) * length);
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007B614);
#endif

/* The heading of an X/Z offset. */
s32 func_8007B694(VECTOR *v) {
    return -func_8004B32C(v->vz, v->vx) & 0xFFF;
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
void func_8007D818(VECTOR *v, SVECTOR *out) {
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007D93C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007DA44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007DCF8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007DECC);

/* Set a dialogue window's rectangle. */
void func_8007E114(s32 window, s32 x, s32 y, s32 w, s32 h) {
    D_800C2698[window].rect.x = x;
    D_800C2698[window].rect.y = y;
    D_800C2698[window].rect.w = w;
    D_800C2698[window].rect.h = h;
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007F5AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007F6F8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8007F814);

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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800805F4);

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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008083C);

/* The collision attribute under an actor on its layer, or 0 when the layer
 * is switched off for it. */
u32 func_80080968(FieldActor *actor) {
    s16 layer = actor->layer;

    if ((actor->layer_flags >> (layer + 3)) & 1) {
        return 0;
    }
    return D_800AFB0C.collision_attributes[D_800AFB0C.collision_triangles[layer][actor->triangle[layer]].attribute];
}

/* Frames (in 800b14ac, two per step) and height of a jump under the actor's
 * gravity from the fixed launch speed. */
s32 func_800809D0(FieldActor *actor) {
    s32 speed = -0x14D000;
    s32 height = 0;

    D_800B14AC = 0;
    do {
        height += speed;
        speed += actor->gravity;
        D_800B14AC += 2;
    } while (speed <= 0);
    return height >> 16;
}

/* The next word of the current descriptor's actor list. */
s32 func_80080A18(void) {
    return D_800AFB0C.descriptors[D_800ADB58].actor->list[D_800ADB5C++];
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800821F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008237C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80082494);

/* Planar distance between two descriptors' actors (integer positions). */
s32 func_800825AC(s32 from, s32 to) {
    s32 to_x = D_800AFB0C.descriptors[to].actor->position[0] >> 16;
    s32 to_z = D_800AFB0C.descriptors[to].actor->position[2] >> 16;
    s32 from_x = D_800AFB0C.descriptors[from].actor->position[0] >> 16;
    s32 from_z = D_800AFB0C.descriptors[from].actor->position[2] >> 16;

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

    if (actor->unk030 != 0 || actor->unk038 != 0) {
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008492C);

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
        D_800B21AC.last_sound_effect = id;
        func_800855C8(id, 0x7F, 0x40, channel);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085678);

/* Release a movie's sound-effect bank, when one is loaded. */
void func_80085738(void) {
    if (D_800C3A38 != 0xFF) {
        func_80039FF8();
        func_8003852C(D_800B235C);
        func_800320E8(D_800B235C);
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085788);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085890);

/* Unlink and release the field's sound-effect bank. */
void func_80085988(void) {
    func_8003852C(D_8006259C);
    func_800320B8(D_8006259C);
    func_800320E8(D_8006259C);
    D_8004F32C = -1;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800859DC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80085B20);

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

    if (distance > D_800B21AC.emitter_range) {
        distance = D_800B21AC.emitter_range;
    }
    level = 0x80 - (((0x7F0000 / D_800B21AC.emitter_range) * distance) >> 16);
    *out = ((u32)(level << 16) / 127 * volume) >> 16;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800860F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086200);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800862CC);

/* Stop the emitter playing sound `id`, freeing its slot. */
void func_800863E8(s32 id) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].id == id) {
            func_8003A20C(i * 2);
            D_800AFE88[i].owner = 0xFFFF;
            D_800AFE88[i].id = 0xFFFF;
            return;
        }
    }
}

/* The emitter slot already playing `id`, or -1. */
s32 func_80086470(s32 owner, s32 id) {
    s32 i;

    if (owner == -1) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].id == id) {
            return i;
        }
    }
    return -1;
}

/* Clear the emitter slots. */
void func_800864B4(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800AFE88[i].id = 0xFFFF;
        D_800AFE88[i].owner = 0xFFFF;
    }
}

/* Clear the emitter slots and stop the voices of the emitters in use. */
void func_800864F0(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800AFE88[i].owner = 0xFFFF;
        D_800AFE88[i].id = 0xFFFF;
    }
    for (i = 0; i < 4; i++) {
        if (!(D_800B21AC.effects_kept & 1)) {
            func_8003A20C(i * 2);
        }
        D_800B21AC.effects_kept >>= 1;
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086590);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086908);

/* Event opcode fe: run the extended instruction named by the next byte. */
void func_800869B8(void) {
    D_800AE6A0[D_800ADC00[++D_800B0078->pc]]();
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086A1C);

/* Update the three positional emitters from their actors' positions. */
void func_80086BA8(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800B21AC.emitter_descriptor[i] != -1) {
            func_80086A1C(i, D_800AFB0C.descriptors[D_800B21AC.emitter_descriptor[i]].actor->position);
        }
    }
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086C34);

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
    D_800B21AC.unk2358 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086E1C);

/* Event: set the current actor's +128 to (op1 << 12) | op3. */
void func_80086F7C(void) {
    s32 high = func_800ACDEC(1);

    D_800B0078->unk128 = (high << 12) | func_800ACDEC(3);
    D_800B0078->pc += 5;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80086FD0);

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

    D_800B21AC.unk225F[index] = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087420);

/* Event: skip a two-byte operand. */
void func_8008752C(void) {
    D_800B0078->pc += 3;
}

/* Event: set game flag 0x4000 of +22b6. */
void func_8008754C(void) {
    D_8005A39C->unk22B6 |= 0x4000;
    D_800B0078->pc++;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087580);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008764C);

/* Event: store 80050622 in variable op1. */
void func_80087800(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_80050622);
    D_800B0078->pc += 3;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087848);

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
    D_800B21AC.unk2357 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set 800b2354 from its byte operand. */
void func_80087A7C(void) {
    D_800B21AC.unk2354 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087AB8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087B5C);

/* Event: set 8004f300. */
void func_80087C0C(void) {
    D_8004F300 = 1;
    D_800B0078->pc++;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087C34);

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
        D_800B21AC.unk2355 = value;
    } else {
        D_800B21AC.unk2356 = value;
    }
    D_800B0078->pc += 4;
}

/* Event: set the battle-entry override (800b234c) from operand 1. */
void func_80087E5C(void) {
    D_800B21AC.battle_override = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80087E98);

/* Event: count 800b2348 up. */
void func_80087FA4(void) {
    D_800B21AC.unk2348++;
    D_800B0078->pc++;
}

/* Event: run 800a8ba4. */
void func_80087FD4(void) {
    func_800A8BA4();
    D_800B0078->pc++;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008800C);

/* Event: restore every character's two gauges to their maxima. */
void func_80088198(void) {
    s32 i;
    GameState *state = D_8005A39C;

    for (i = 0; i < 20; i++) {
        state->characters[i].points = state->characters[i].points_max;
        state->characters[i].gauge = state->characters[i].gauge_max;
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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800882B8);

/* Event: set byte +4 of party slot op1 to op3. */
void func_80088360(void) {
    s32 slot = func_800ACDEC(1);

    D_8005A39C->party[slot].unk04 = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800883D4);

#ifdef NON_MATCHING
/* Event: set 800b236c to the inverse of its byte operand's low bit. */
void func_8008848C(void) {
    s32 value = D_800ADC00[D_800B0078->pc + 1] ^ 1;

    D_800B0078->pc += 2;
    D_800B21AC.unk236C = value;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008848C);
#endif

/* Event: set the sound-emitter range from operand 1. */
void func_800884CC(void) {
    D_800B21AC.emitter_range = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80088508);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008861C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80088674);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80088790);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800888A4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800889BC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80088B68);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80088C1C);

/* 80088d38 with 0. */
void func_80088CF8(void) {
    func_80088D38(0);
}

/* 80088d38 with 4. */
void func_80088D18(void) {
    func_80088D38(4);
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80088D38);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089004);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089174);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089374);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089574);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800896D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089880);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089A80);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089AE4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089B54);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089BF0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089DCC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089F18);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089F54);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089F94);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80089FD0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A08C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A148);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A244);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A2A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A2E8);

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

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A520);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A558);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A5A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A604);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A640);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A6E0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A790);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A7DC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A93C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A974);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008A9AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008AA60);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008AACC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008ACE8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008AE5C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008AEC8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008AFD8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B0E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B144);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B180);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B210);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B248);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B2F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B328);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B45C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B518);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B5D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B894);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008B978);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008BC80);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008BDD8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008BF38);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008C180);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008C334);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008C7D8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008C84C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008C938);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CA60);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CB4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CC74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CD48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CDD4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CE64);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CED0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CF3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CF9C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008CFEC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D078);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D0F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D180);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D230);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D26C);

void func_8008D2D8(void) {
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D2E0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D30C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D380);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D570);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D5C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D604);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D684);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D700);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D780);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008D808);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DA04);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DAFC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DB2C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DB68);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DBF0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DC74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DD6C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DE64);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DEBC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DF44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008DFCC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E054);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E0DC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E148);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E1B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E298);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E2EC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E340);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E394);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E3E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E414);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E440);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E46C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E498);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E4EC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E518);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E544);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E570);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E59C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E718);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E85C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E8C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008E9F8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EA58);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EC30);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EE14);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EF5C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EFA0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008EFE4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F070);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F0B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F1C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F2D8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F348);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F394);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F3D0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F444);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F4A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F4FC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F558);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F5E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F668);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F6AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F724);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F76C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F7B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008F90C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FA38);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FABC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FB28);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FB98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FC4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FD40);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FDD0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FE2C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FF04);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8008FF90);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090068);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800900C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009019C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090228);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090300);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800903BC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090A10);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090A94);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090B18);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090B9C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090C20);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090CB8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090D50);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090DEC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80090E70);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091008);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800910C0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091318);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800915C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091720);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091944);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091A08);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091A78);

void func_80091AD4(void) {
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091ADC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091BBC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091E00);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091E98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80091F84);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092044);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800920D8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092148);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800921E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800923E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092404);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092424);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800924D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800925A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092628);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092664);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800926C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092768);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092808);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092894);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092C20);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092DFC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092EA0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092F44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80092FB4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093014);

void func_800931F8(void) {
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093200);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800932D0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800933F8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093568);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093664);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800936E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093740);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093790);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800937E0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093824);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093888);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093930);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800939A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093A04);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093A68);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093A98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093AC8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093B10);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093BB0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093BD4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093BFC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093C20);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093C48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093C6C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093CD0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093D48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093E30);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80093FC0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094158);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800943AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800945D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094650);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009468C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800946BC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094710);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094764);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800947B0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094918);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094A5C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094ACC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094B3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094BAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094C1C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094C8C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094CFC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094D4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094D9C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094DEC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094E3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094E8C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094EDC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094F2C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094F7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80094FCC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009501C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800950A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095124);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800951B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009524C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095284);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095300);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009533C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095520);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095734);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800958C0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095A7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095B3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095C00);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095CC4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095D6C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095E48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095F24);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80095FB8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009601C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096078);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800960E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096150);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096178);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800961A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800961C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800961F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096214);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800962C0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009631C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009635C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009640C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800964B0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096534);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800965A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800965F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096644);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800966B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096724);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096790);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800967E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096844);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800968CC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096920);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800969A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800969FC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096AF4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096B58);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096C40);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096D28);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096E20);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80096F18);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097010);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097108);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097200);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097264);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800972AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800972F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009731C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097364);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800973A4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097410);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009744C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009749C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800975C0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800976A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800977A4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097864);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097954);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800979F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80097A50);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098038);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800980FC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098184);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098274);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098370);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098430);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800984EC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800985BC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009861C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098738);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800988B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009899C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800989F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098A7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098C00);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098C3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80098CAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099214);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099980);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099A04);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099A4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099A8C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099AC0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099EF8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099F48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_80099FC4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A024);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A0FC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A130);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A174);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A1AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A1E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A2A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A34C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A420);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A490);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A514);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A534);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A58C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A5E0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A634);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A670);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A6AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A768);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A824);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A8DC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A904);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009A958);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AA00);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AB08);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AB5C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009ABAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009ABFC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AC34);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AC7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009ACB4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009ACEC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AD6C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009ADDC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AE0C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AE3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009AEE0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B15C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B184);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B210);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B338);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B398);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B664);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B6AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B708);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B7A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B824);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B884);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B8E4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009B9A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BA0C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BA7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BB0C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BC98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BE58);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BE9C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009BF8C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C01C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C0B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C0DC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C104);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C12C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C154);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C538);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009C5A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CCF8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CD18);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CD7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CDB4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CE48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CEE0);

void func_8009CF70(void) {
}

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CF78);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009CFBC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D000);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D044);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D088);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D0CC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D110);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D154);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D198);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D1F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D260);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D2D0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D340);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D3A4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D408);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D4A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D52C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D5B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D644);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D6D8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D768);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D804);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D890);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D91C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D960);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009D9A4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DA1C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DA44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DA70);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DA98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DAC4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DBC8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DC4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DD34);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DDEC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DE94);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DF10);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009DF78);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E014);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E040);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E094);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E10C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E1A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E208);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E248);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E2C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E330);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E35C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E428);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E4BC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E574);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E810);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E83C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009E91C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009EB48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009EB78);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009ED68);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009F0A0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009F424);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009F4CC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009F5A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009F5F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FA00);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FA54);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FB98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FC10);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FC48);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FCAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FD10);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FDD4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FE4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_8009FEE4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0158);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0228);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0524);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A06E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A08B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0C4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0C94);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0D3C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0DC0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0DFC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0E54);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0EB0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0EE8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A0FD8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1364);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A14F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1624);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1730);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A17F4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A18B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A19B0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1A8C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1B70);

INCLUDE_RODATA(".local/decomp/field/asm/nonmatchings/field", D_8006FD44);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1BD0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1E74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1E9C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A1EC8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A2030);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A22AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A2488);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A24C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A2714);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A28D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A2FC0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A2FE0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3018);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3074);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3090);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A30B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A30FC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A31E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3474);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3C8C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A3F4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A4748);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A476C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A47D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A484C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A4CC4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A4DAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A55B8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A55C8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5600);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A56A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5710);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5774);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5884);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5924);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A5C40);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A6408);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A663C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A6924);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A6998);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A6C40);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A6E70);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7064);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A708C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7120);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7218);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A732C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7394);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A73E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A74F8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7744);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A77C4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7948);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A7C58);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8314);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A83B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8408);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A84C0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8BA4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A8EAC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A90B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A915C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A91F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9274);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A92AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9374);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A93CC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9460);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A94A4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9688);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A987C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A98B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A98E8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A99A8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9B1C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9B54);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800A9F18);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AA6B4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AA9DC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AAA74);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AABD8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AAC08);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AADC8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AAE4C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AAF80);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AB328);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AB378);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AB748);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AB808);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ABA98);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ABD18);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ABEC8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ABFDC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AC03C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AC0F0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AC308);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AC3AC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AC99C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACB90);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACC58);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACCB0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACCF4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACD7C);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACDB8);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACDEC);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACE24);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACE90);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800ACFD0);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AD4D4);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AD898);

INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field", func_800AD978);
