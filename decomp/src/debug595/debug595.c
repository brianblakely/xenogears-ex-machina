/* debug595: the field debug monitor (disc 1 file 595, disc 2 file 590), a
 * development tool linked for the dev kit's 8 MB RAM at 0x80280000. The field
 * overlay's setup (80077e88) reads it as directory 4 file 0xad into 0x80280000
 * when D_800C268C and D_8004F370 are clear; on a retail 2 MB unit the image
 * would alias low RAM, so only development hardware runs it. Its screens
 * print player/scene/camera/event/memory/CPU-time state, edit particle
 * emitters, the encounter timer and fog colours, play sound banks, and draw
 * debug lines. */
#include "debug595.h"

/* Reset the two frame counters. */
void func_802811EC(void) {
    D_80285980 = 0;
    D_8028597C = 0;
}

/* Count one event of `kind` and restart the 60-frame display timer. */
void func_80281204(s32 kind) {
    D_8028598C = 1;
    D_80285990 = 60;
    D_80285998 = kind;
    D_80285B28[kind]++;
    D_80285994++;
}

/* Clear the event counters. */
void func_8028125C(void) {
    s32 i;

    D_802859A4 = 0;
    D_8028598C = 0;
    D_80285990 = 30;
    D_80285994 = 0;
    for (i = 15; i >= 0; i--) {
        D_80285B28[i] = 0;
    }
}

#ifdef NON_MATCHING
/* Reset the debug lines to grey-to-white segments at the origin; line 1 is red.
 * Loop strength reduction differs: the original steps &rot and &line[1] and
 * indexes &trans and &line[0] from a base register. */
void func_802812A4(void) {
    s32 i;
    DebugLine *line;

    for (i = 0; i < 16; i++) {
        line = &D_80285B48[i];
        line->rot.vx = 0;
        line->rot.vy = 0;
        line->rot.vz = 0;
        line->trans.vx = 0;
        line->trans.vy = 0;
        line->trans.vz = 0;
        SetLineG2(&line->line[0]);
        line->line[0].r0 = 0x80;
        line->line[0].g0 = 0x80;
        line->line[0].b0 = 0x80;
        line->line[0].r1 = 0xFF;
        line->line[0].g1 = 0xFF;
        line->line[0].b1 = 0xFF;
        line->line[1] = line->line[0];
    }
    D_80285B48[1].line[0].r0 = 0x80;
    D_80285B48[1].line[0].g0 = 0;
    D_80285B48[1].line[0].b0 = 0;
    D_80285B48[1].line[0].r1 = 0xFF;
    D_80285B48[1].line[0].g1 = 0;
    D_80285B48[1].line[0].b1 = 0;
    D_80285B48[1].line[1].r0 = 0x80;
    D_80285B48[1].line[1].g0 = 0;
    D_80285B48[1].line[1].b0 = 0;
    D_80285B48[1].line[1].r1 = 0xFF;
    D_80285B48[1].line[1].g1 = 0;
    D_80285B48[1].line[1].b1 = 0;
}
#else
INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802812A4);
#endif

/* Hide the debug lines; when the monitor drew anything, draw its ordering table. */
void func_80281400(void) {
    D_80285988 = 0;
    if (func_80281B90(D_800C426C + 0x34) != 0) {
        func_80037324(D_800C426C + 0x34);
    }
    D_802859A4 = 0;
}

/* Draw the first twelve debug lines into the current buffer. */
void func_80281450(void) {
    s32 i;

    if (D_800C268C == 0 && D_80285988 != 0) {
        for (i = 0; i < 12; i++) {
            func_802814D4(D_800C426C + 0x33, &D_80285B48[i], &D_80285B48[i].matrix, D_800ADB08);
        }
    }
}

/* Project a debug line's end points with `m` and link its primitive for `buffer`. */
void func_802814D4(u32 *ot, DebugLine *line, MATRIX *m, s32 buffer) {
    LINE_G2 *prim;
    SVECTOR unused;
    s32 sxy2;
    s32 flag;
    s32 p;

    prim = &line->line[buffer];
    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotTransPers3(&line->start, &line->end, &unused, (s32 *)&prim->x0, (s32 *)&prim->x1, &sxy2, &p, &flag);
    addPrim(ot + 1, prim);
    PopMatrix();
}

/* Rebuild each debug line's matrix from its rotation and translation in the camera frame. */
void func_802815B0(void) {
    s32 i;
    SVECTOR moved;

    if (D_800C268C == 0) {
        for (i = 0; i < 16; i++) {
            func_8003F738(&D_80285B48[i].rot, &D_80285B48[i].matrix);
            PushMatrix();
            func_80049BDC(&D_800AFA64, &D_80285B48[i].matrix);
            PopMatrix();
            func_8004A6DC(&D_80285B48[i].trans, D_80285B48[i].matrix.t, &moved);
        }
    }
}

/* Place the first twelve debug lines on `actor` and shape eight of them into
 * the outline of its collision box (bottom and top rectangles). */
void func_80281678(DebugActor *actor) {
    s32 i;

    if (D_800C268C == 0) {
        for (i = 0; i < 12; i++) {
            D_80285B48[i].trans.vx = actor->pos.vx >> 16;
            D_80285B48[i].trans.vy = actor->pos.vy >> 16;
            D_80285B48[i].trans.vz = actor->pos.vz >> 16;
            D_80285B48[i].start.vy = 0;
            D_80285B48[i].end.vy = 0;
        }
        D_80285B48[0].start.vx = -actor->size.vx;
        D_80285B48[0].start.vz = -actor->size.vz;
        D_80285B48[0].end.vx = actor->size.vx;
        D_80285B48[0].end.vz = -actor->size.vz;
        D_80285B48[1].start.vx = actor->size.vx;
        D_80285B48[1].start.vz = -actor->size.vz;
        D_80285B48[1].end.vx = actor->size.vx;
        D_80285B48[1].end.vz = actor->size.vz;
        D_80285B48[2].start.vx = actor->size.vx;
        D_80285B48[2].start.vz = actor->size.vz;
        D_80285B48[2].end.vx = -actor->size.vx;
        D_80285B48[2].end.vz = actor->size.vz;
        D_80285B48[3].start.vx = -actor->size.vx;
        D_80285B48[3].start.vz = actor->size.vz;
        D_80285B48[3].end.vx = -actor->size.vx;
        D_80285B48[3].end.vz = -actor->size.vz;
        D_80285B48[4].start.vx = -actor->size.vx;
        D_80285B48[4].start.vz = -actor->size.vz;
        D_80285B48[4].end.vx = actor->size.vx;
        D_80285B48[4].end.vz = -actor->size.vz;
        D_80285B48[4].start.vy = -actor->size.vy;
        D_80285B48[4].end.vy = -actor->size.vy;
        D_80285B48[5].start.vx = actor->size.vx;
        D_80285B48[5].start.vz = -actor->size.vz;
        D_80285B48[5].end.vx = actor->size.vx;
        D_80285B48[5].end.vz = actor->size.vz;
        D_80285B48[5].start.vy = -actor->size.vy;
        D_80285B48[5].end.vy = -actor->size.vy;
        D_80285B48[6].start.vx = actor->size.vx;
        D_80285B48[6].start.vz = actor->size.vz;
        D_80285B48[6].end.vx = -actor->size.vx;
        D_80285B48[6].end.vz = actor->size.vz;
        D_80285B48[6].start.vy = -actor->size.vy;
        D_80285B48[6].end.vy = -actor->size.vy;
        D_80285B48[7].start.vx = -actor->size.vx;
        D_80285B48[7].start.vz = actor->size.vz;
        D_80285B48[7].end.vx = -actor->size.vx;
        D_80285B48[7].end.vz = -actor->size.vz;
        D_80285B48[7].start.vy = -actor->size.vy;
        D_80285B48[7].end.vy = -actor->size.vy;
    }
}

/* Print a four-component record. */
void func_80281994(s16 *v) {
    if (D_800C268C != 1) {
        func_800379C8("REC %07d %07d %07d %07d\n", v[0], v[1], v[2], v[3]);
    }
}

/* Print a matrix row by row with its translation. */
void func_802819DC(MATRIX *m) {
    if (D_800C268C != 1) {
        func_800379C8("MTX %06d %06d %06d %06d\n", m->m[0][0], m->m[0][1], m->m[0][2], m->t[0]);
        func_800379C8("    %06d %06d %06d %06d\n", m->m[1][0], m->m[1][1], m->m[1][2], m->t[1]);
        func_800379C8("    %06d %06d %06d %06d\n", m->m[2][0], m->m[2][1], m->m[2][2], m->t[2]);
    }
}

/* Print a long vector. */
void func_80281A78(VECTOR *v) {
    if (D_800C268C != 1) {
        func_800379C8("VEC  %d %d %d\n", v->vx, v->vy, v->vz);
    }
}

/* Print a short vector. */
void func_80281ABC(SVECTOR *v) {
    if (D_800C268C != 1) {
        func_800379C8("SVEC %d %d %d\n", v->vx, v->vy, v->vz);
    }
}

/* Record the scanlines spent since the last mark under `name` for the CPU-time screen. */
void func_80281B00(char *name) {
    s32 now;

    if (D_800C268C == 0) {
        now = VSync(1);
        D_802859AC[D_802859A4].time = now - D_800ADB9C;
        D_802859AC[D_802859A4].name = name;
        D_802859A4++;
        D_800ADB9C = VSync(1);
    }
}

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80281B90);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802835E0);

/* Print the cursor mark for `row` when it is the selected row and `blink` is 1. */
void func_80284354(s32 row, s32 cursor, s32 blink) {
    if (cursor == row && blink == 1) {
        func_800379C8(">");
    } else {
        func_800379C8(" ");
    }
}

/* Start menu row `row`: highlight it and mark it with the cursor when selected;
 * returns the next row. */
s32 func_8028439C(s32 row, s32 cursor, s32 *selected) {
    if (cursor == row) {
        func_80036DC8(0, 0xFF, 0xFF);
        func_800379C8(">");
        *selected = 1;
    } else {
        func_80036DC8(0x40, 0x40, 0x40);
        func_800379C8(" ");
        *selected = 0;
    }
    return ++row;
}

/* Step `value` down (Left) or up (Right) within [min, max]; the shoulder
 * buttons pick steps of 10, 100 or 1000. */
s32 func_80284424(s32 value, s32 min, s32 max) {
    s32 step;

    step = 1;
    if (D_800AFEA0 & 4) {
        step = 10;
    }
    if (D_800AFEA0 & 1) {
        step = 100;
    }
    if (D_800AFEA0 & 2) {
        step = 1000;
    }
    if (D_800C3908 & 0x80) {
        value -= step;
        if (value < min) {
            value = min;
        }
    }
    if (D_800C3908 & 0x20) {
        value += step;
        if (max < value) {
            value = max;
        }
    }
    return value;
}

/* Set component `axis` of a short vector. */
void func_802844BC(SVECTOR *v, s32 axis, s32 value) {
    switch (axis) {
    case 0:
        v->vx = value;
        break;
    case 1:
        v->vy = value;
        break;
    case 2:
        v->vz = value;
        break;
    }
}

/* Component `axis` of a short vector, 0 for another axis. */
s32 func_80284510(SVECTOR *v, s32 axis) {
    switch (axis) {
    case 0:
        return v->vx;
    case 1:
        return v->vy;
    case 2:
        return v->vz;
    }
    return 0;
}

/* Set channel `channel` of an unsigned colour triple. */
void func_8028456C(u8 *color, s32 channel, s32 value) {
    switch (channel) {
    case 0:
        color[0] = value;
        break;
    case 1:
        color[1] = value;
        break;
    case 2:
        color[2] = value;
        break;
    }
}

/* Channel `channel` of an unsigned colour triple, 0 for another channel. */
s32 func_802845C0(u8 *color, s32 channel) {
    switch (channel) {
    case 0:
        return color[0];
    case 1:
        return color[1];
    case 2:
        return color[2];
    }
    return 0;
}

/* Set component `axis` of a signed byte triple. */
void func_8028461C(s8 *v, s32 axis, s32 value) {
    switch (axis) {
    case 0:
        v[0] = value;
        break;
    case 1:
        v[1] = value;
        break;
    case 2:
        v[2] = value;
        break;
    }
}

/* Component `axis` of a signed byte triple, 0 for another axis. */
s32 func_80284670(s8 *v, s32 axis) {
    switch (axis) {
    case 0:
        return v[0];
    case 1:
        return v[1];
    case 2:
        return v[2];
    }
    return 0;
}



#ifdef NON_MATCHING
/* Edit field `item` (component `axis`) of the selected particle emitter.
 * Differs only in the flag cases: the original masks the kept bits before
 * the step call (in its delay slot) where GCC here expands the call first. */
void func_802846CC(s32 axis, u32 item) {
    s16 flags;
    s32 kept;

    switch (item) {
    case 0:
        D_800B0044 = func_80284424(D_800B0044, 0, 7);
        break;
    case 1:
        D_800B02CC[D_800B0044].bank = func_80284424(D_800B02CC[D_800B0044].bank, 0, 0xFF);
        break;
    case 2:
        D_800B02CC[D_800B0044].max = func_80284424(D_800B02CC[D_800B0044].max, 0, 0x7FFF);
        break;
    case 3:
        D_800B02CC[D_800B0044].start_wait =
            func_80284424(D_800B02CC[D_800B0044].start_wait, 1, 0x7FFF);
        break;
    case 4:
        func_802844BC(&D_800B02CC[D_800B0044].start_pos, axis,
                      func_80284424(func_80284510(&D_800B02CC[D_800B0044].start_pos, axis), -0x8000,
                                    0x7FFF));
        break;
    case 5:
        func_802844BC(&D_800B02CC[D_800B0044].end_pos, axis,
                      func_80284424(func_80284510(&D_800B02CC[D_800B0044].end_pos, axis), -0x8000,
                                    0x7FFF));
        break;
    case 6:
        if (axis == 0) {
            D_800B02CC[D_800B0044].speed =
                func_80284424(D_800B02CC[D_800B0044].speed, -0x8000, 0x7FFF);
        } else {
            D_800B02CC[D_800B0044].speed_scale =
                func_80284424(D_800B02CC[D_800B0044].speed_scale, 1, 0x7FFF);
        }
        break;
    case 7:
        func_802844BC(&D_800B02CC[D_800B0044].gravity, axis,
                      func_80284424(func_80284510(&D_800B02CC[D_800B0044].gravity, axis), -0x8000,
                                    0x7FFF));
        break;
    case 8:
        D_800B02CC[D_800B0044].start_range =
            func_80284424(D_800B02CC[D_800B0044].start_range, 0, 0xFFFF);
        break;
    case 9:
        D_800B02CC[D_800B0044].end_range =
            func_80284424(D_800B02CC[D_800B0044].end_range, 0, 0xFFFF);
        break;
    case 10:
        D_800B02CC[D_800B0044].particle_start_wait =
            func_80284424(D_800B02CC[D_800B0044].particle_start_wait, 1, 0x7FFF);
        break;
    case 11:
        D_800B02CC[D_800B0044].particle_end_wait =
            func_80284424(D_800B02CC[D_800B0044].particle_end_wait, 1, 0x7FFF);
        break;
    case 12:
        D_800B02CC[D_800B0044].shape = func_80284424(D_800B02CC[D_800B0044].shape, 0, 0x7FFF);
        break;
    case 13:
        func_802844BC(&D_800B02CC[D_800B0044].scale, axis,
                      func_80284424(func_80284510(&D_800B02CC[D_800B0044].scale, axis), -0x8000,
                                    0x7FFF));
        break;
    case 14:
        func_802844BC(&D_800B02CC[D_800B0044].scale_offset, axis,
                      func_80284424(func_80284510(&D_800B02CC[D_800B0044].scale_offset, axis),
                                    -0x8000, 0x7FFF));
        break;
    case 15:
        func_8028456C(D_800B02CC[D_800B0044].color, axis,
                      func_80284424(func_802845C0(D_800B02CC[D_800B0044].color, axis), 0, 0xFF));
        break;
    case 16:
        func_8028461C(D_800B02CC[D_800B0044].color_offset, axis,
                      func_80284424(func_80284670(D_800B02CC[D_800B0044].color_offset, axis), -0x80,
                                    0x7F));
        break;
    case 17:
        flags = D_800B02CC[D_800B0044].flags;
        D_800B02CC[D_800B0044].flags = (flags & 0xFFFE) | func_80284424(flags & 1, 0, 1);
        break;
    case 18:
        flags = D_800B02CC[D_800B0044].flags;
        D_800B02CC[D_800B0044].flags =
            (flags & 0xFFF9) |
            (func_80284424((flags >> 1) & 3, 0, 3) << 1);
        break;
    case 19:
        flags = D_800B02CC[D_800B0044].flags;
        D_800B02CC[D_800B0044].flags =
            (flags & 0xFCFF) |
            (func_80284424((flags >> 8) & 3, 0, 3) << 8);
        break;
    case 20:
        D_800B02CC[D_800B0044].rot_angle =
            func_80284424(D_800B02CC[D_800B0044].rot_angle, 0, 0xFFF);
        break;
    case 21:
        flags = D_800B02CC[D_800B0044].flags;
        D_800B02CC[D_800B0044].flags =
            (flags & 0xFF3F) |
            (func_80284424((flags >> 6) & 3, 0, 2) << 6);
        break;
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
        if (axis == 0) {
            D_800B02CC[D_800B0044].angle_offsets[item - 22][0] =
                func_80284424(D_800B02CC[D_800B0044].angle_offsets[item - 22][0], -0x8000, 0x7FFF);
        } else {
            D_800B02CC[D_800B0044].angle_offsets[item - 22][1] =
                func_80284424(D_800B02CC[D_800B0044].angle_offsets[item - 22][1], -0x8000, 0x7FFF);
        }
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802846CC);
#endif

#ifdef NON_MATCHING
/* With L2 and the debug button held, move the camera by the pad's analog
 * steps: dolly (mode 4), zoom (mode 8) or rotate and raise (other modes).
 * Differs: the original loads D_800AF9FC with lh before the step and the
 * step with a full lw where this narrows both loads. */
void func_80284EA4(void) {
    if ((D_800AFE9C & 1) && (D_800AFE9C & 0x40)) {
        if (D_80065850 == 4) {
            D_800ADB98 = 1;
            D_800ADB94 += D_80065858;
        } else if (D_80065850 == 8) {
            D_800AF9FC += (u32)(D_80065858 << 4) >> 5;
            D_800AF984 = 1;
            D_800AF988 = 1;
        } else {
            D_800AF984 = 1;
            D_800AF988 = 1;
            D_800AF9FE += D_80065858 << 4;
            D_800AF9F0 += D_80065854 << 18;
            D_800AF9E6 = D_800AF9F0 >> 16;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284EA4);
#endif

#ifdef NON_MATCHING
/* Load an image archive's sections into VRAM; sections of kind 0x1100 and
 * 0x1101 are placed by `mode0`/`mode1`: 1 at the given origin plus the
 * section offset, 2 also plus the section position, else at the position.
 * Differs: the original fills the unknown-kind exit's delay slot with the
 * next comparison (v0 dead there) and takes the product into $t0. */
s32 func_80284FB4(u32 *archive, s16 mode0, s16 x0, s16 y0, s16 mode1, u16 x1, u16 y1) {
    s32 count;
    s32 i;
    u16 *p;
    u32 kind;
    DebugRect rect;

    count = archive[0];
    p = (u16 *)(archive + (count + 1));
    for (i = 0; i < count; i++) {
        kind = *(u32 *)p;
        p += 2;
        if (kind == 0x1100) {
            switch (mode0) {
            case 1:
                rect.x = x0 + p[2];
                rect.y = y0 + p[3];
                break;
            case 2:
                rect.x = p[2] + (x0 + p[0]);
                rect.y = p[3] + (y0 + p[1]);
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        } else if (kind == 0x1101) {
            switch (mode1) {
            case 1:
                rect.x = x1 + p[2];
                rect.y = y1 + p[3];
                break;
            case 2:
                rect.x = p[2] + (x1 + p[0]);
                rect.y = p[3] + (y1 + p[1]);
                break;
            default:
                rect.x = p[0] + p[2];
                rect.y = p[1] + p[3];
                break;
            }
        } else {
            return; /* the original returns no value here */
        }
        p += 4;
        rect.w = *p++;
        rect.h = *p++;
        LoadImage(&rect, (u32 *)p);
        p += rect.w * rect.h;
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284FB4);
#endif

/* Print the name of sound bank `bank` (effects, music and voice banks). */
void func_802851B0(s32 bank) {
    switch (bank) {
    case 0:
        func_800379C8("main_se");
        break;
    case 1:
        func_800379C8("bat_se");
        break;
    case 2:
        func_800379C8("gear_se");
        break;
    case 3:
        func_800379C8("ambi");
        break;
    case 4:
        func_800379C8("ambi2");
        break;
    case 5:
        func_800379C8("ambi3");
        break;
    case 6:
        func_800379C8("ambi4");
        break;
    case 32:
        func_800379C8("minato");
        break;
    case 33:
        func_800379C8("lahan");
        break;
    case 34:
        func_800379C8("jyukai");
        break;
    case 35:
        func_800379C8("shitan");
        break;
    case 36:
        func_800379C8("musi");
        break;
    case 37:
        func_800379C8("church");
        break;
    case 38:
        func_800379C8("battle2");
        break;
    case 39:
        func_800379C8("chuchu");
        break;
    case 40:
        func_800379C8("over");
        break;
    case 41:
        func_800379C8("orgel");
        break;
    case 42:
        func_800379C8("battle3");
        break;
    case 43:
        func_800379C8("ajito");
        break;
    case 44:
        func_800379C8("emerada");
        break;
    case 45:
        func_800379C8("ellie");
        break;
    case 46:
        func_800379C8("world");
        break;
    case 47:
        func_800379C8("sad");
        break;
    case 48:
        func_800379C8("ave");
        break;
    case 49:
        func_800379C8("ellie2");
        break;
    case 50:
        func_800379C8("balto");
        break;
    case 51:
        func_800379C8("dajil");
        break;
    case 52:
        func_800379C8("maria1");
        break;
    case 53:
        func_800379C8("maria2");
        break;
    case 54:
        func_800379C8("heshu");
        break;
    case 55:
        func_800379C8("kaisou");
        break;
    case 56:
        func_800379C8("pinch");
        break;
    case 57:
        func_800379C8("porgan");
        break;
    case 58:
        func_800379C8("babel");
        break;
    case 59:
        func_800379C8("solachu");
        break;
    case 60:
        func_800379C8("shinnyu");
        break;
    case 61:
        func_800379C8("inbou");
        break;
    case 62:
        func_800379C8("ido");
        break;
    case 63:
        func_800379C8("takeoff");
        break;
    case 64:
        func_800379C8("glaerf");
        break;
    case 65:
        func_800379C8("last");
        break;
    case 66:
        func_800379C8("shebat");
        break;
    case 67:
        func_800379C8("dungeon");
        break;
    case 68:
        func_800379C8("lastbat");
        break;
    case 69:
        func_800379C8("solaris");
        break;
    case 181:
        func_800379C8("vomaria");
        break;
    case 182:
        func_800379C8("melmv");
        break;
    case 183:
        func_800379C8("yugumv");
        break;
    case 184:
        func_800379C8("zoharumv");
        break;
    case 185:
        func_800379C8("vomagic5");
        break;
    case 186:
        func_800379C8("vomagic4");
        break;
    case 187:
        func_800379C8("vomagic3");
        break;
    case 188:
        func_800379C8("voivent3");
        break;
    case 189:
        func_800379C8("voivent2");
        break;
    case 190:
        func_800379C8("vobossm");
        break;
    case 191:
        func_800379C8("vobossl");
        break;
    case 192:
        func_800379C8("vochu6");
        break;
    case 193:
        func_800379C8("vomagic2");
        break;
    case 194:
        func_800379C8("vomagic1");
        break;
    case 7:
        func_800379C8("movie14");
        break;
    case 195:
        func_800379C8("movie15");
        break;
    case 196:
        func_800379C8("movie16");
        break;
    case 197:
        func_800379C8("movie18");
        break;
    case 198:
        func_800379C8("voivent");
        break;
    case 199:
        func_800379C8("damage");
        break;
    case 200:
        func_800379C8("vofei");
        break;
    case 201:
        func_800379C8("vofei1");
        break;
    case 202:
        func_800379C8("vofei2");
        break;
    case 203:
        func_800379C8("vofei3");
        break;
    case 204:
        func_800379C8("vofei4");
        break;
    case 205:
        func_800379C8("vofei5");
        break;
    case 206:
        func_800379C8("vofei6");
        break;
    case 207:
        func_800379C8("voellie");
        break;
    case 208:
        func_800379C8("voellie1");
        break;
    case 209:
        func_800379C8("voellie2");
        break;
    case 210:
        func_800379C8("voellie3");
        break;
    case 211:
        func_800379C8("voellie4");
        break;
    case 212:
        func_800379C8("voellie5");
        break;
    case 213:
        func_800379C8("voellie6");
        break;
    case 214:
        func_800379C8("voellie7");
        break;
    case 215:
        func_800379C8("voellie8");
        break;
    case 216:
        func_800379C8("voshita");
        break;
    case 217:
        func_800379C8("voshita1");
        break;
    case 218:
        func_800379C8("voshita2");
        break;
    case 219:
        func_800379C8("voshita3");
        break;
    case 220:
        func_800379C8("voshita4");
        break;
    case 221:
        func_800379C8("voshita5");
        break;
    case 222:
        func_800379C8("voshita6");
        break;
    case 223:
        func_800379C8("vobaluto");
        break;
    case 224:
        func_800379C8("vobalu1");
        break;
    case 225:
        func_800379C8("vobalu2");
        break;
    case 226:
        func_800379C8("vobalu3");
        break;
    case 227:
        func_800379C8("vobalu4");
        break;
    case 228:
        func_800379C8("vobalu5");
        break;
    case 229:
        func_800379C8("vobalu6");
        break;
    case 230:
        func_800379C8("vobalu7");
        break;
    case 231:
        func_800379C8("vorico");
        break;
    case 232:
        func_800379C8("vorico1");
        break;
    case 233:
        func_800379C8("vorico2");
        break;
    case 234:
        func_800379C8("vorico3");
        break;
    case 235:
        func_800379C8("vorico4");
        break;
    case 236:
        func_800379C8("vorico5");
        break;
    case 237:
        func_800379C8("vobilly");
        break;
    case 238:
        func_800379C8("vobilly1");
        break;
    case 239:
        func_800379C8("vobilly2");
        break;
    case 240:
        func_800379C8("vobilly3");
        break;
    case 241:
        func_800379C8("vobilly4");
        break;
    case 242:
        func_800379C8("vobilly5");
        break;
    case 243:
        func_800379C8("voeme");
        break;
    case 244:
        func_800379C8("voeme1");
        break;
    case 245:
        func_800379C8("voeme2");
        break;
    case 246:
        func_800379C8("voeme3");
        break;
    case 247:
        func_800379C8("voeme4");
        break;
    case 248:
        func_800379C8("voeme5");
        break;
    case 249:
        func_800379C8("vochu");
        break;
    case 250:
        func_800379C8("vochu1");
        break;
    case 251:
        func_800379C8("vochu2");
        break;
    case 252:
        func_800379C8("vochu3");
        break;
    case 253:
        func_800379C8("vochu4");
        break;
    case 254:
        func_800379C8("vochu5");
        break;
    }
}
