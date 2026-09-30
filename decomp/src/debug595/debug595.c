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
void func_802844BC(SVECTOR *v, s32 axis, s16 value) {
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
void func_8028456C(u8 *color, s32 channel, u8 value) {
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
void func_8028461C(s8 *v, s32 axis, s8 value) {
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



INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802846CC);

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

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802851B0);
