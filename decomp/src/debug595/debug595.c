/* debug595: the field debug monitor (disc 1 file 595, disc 2 file 590), a
 * development tool linked for the dev kit's 8 MB RAM at 0x80280000. The field
 * overlay's setup (80077e88) reads it as directory 4 file 0xad into 0x80280000
 * when D_800C268C and D_8004F370 are clear; on a retail 2 MB unit the image
 * would alias low RAM, so only development hardware runs it. Its screens
 * print player/scene/camera/event/memory/CPU-time state, edit particle
 * emitters, the encounter timer and fog colours, play sound banks, and draw
 * debug lines.
 *
 * The whole image is this one unit (rodata 80280000-802811EC, text
 * 802811EC-8028596C, data 8028596C-802861C8), built by GCC 2.7.2
 * (debug595.mk); its three jump tables share one phase. */
#include "debug595.h"

/* Monitor statics (all zero in the image). */
s32 D_8028596C = 0; /* RGB calc red */
s32 D_80285970 = 0; /* green */
s32 D_80285974 = 0; /* blue */
u32 D_80285978 = 0; /* RGB calc mode (bits 4..5) */
s32 D_8028597C = 0; /* screen cursor */
s32 D_80285980 = 0; /* second counter, reset with the cursor */
s32 D_80285984 = 0; /* monitor screen */
s32 D_80285988 = 0; /* debug lines shown (set by the field overlay) */
s32 D_8028598C = 0; /* encounter notice shown */
s32 D_80285990 = 0; /* encounter notice frames */
s32 D_80285994 = 0; /* encounters counted */
s32 D_80285998 = 0; /* last encounter number */
s32 D_8028599C = 0; /* particle editor row */
s32 D_802859A0 = 0; /* particle editor column */
s16 D_802859A4 = 0; /* CPU-time marks this frame */
/* The CPU-time marks: 32 records of 12 bytes exactly fill 802859a8-80285b28;
 * the code addresses their time (+4) and name (+8). */
CpuMark D_802859A8[32] = {0};
s16 D_80285B28[16] = {0}; /* encounters per number */
DebugLine D_80285B48[16] = {0}; /* the debug lines */

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

/* Reset the debug lines to grey-to-white segments at the origin; line 1 is red. */
void func_802812A4(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        setVector(&D_80285B48[i].rot, 0, 0, 0);
        setVector(&D_80285B48[i].trans, 0, 0, 0);
        SetLineG2(&D_80285B48[i].line[0]);
        setRGB0(&D_80285B48[i].line[0], 0x80, 0x80, 0x80);
        setRGB1(&D_80285B48[i].line[0], 0xFF, 0xFF, 0xFF);
        D_80285B48[i].line[1] = D_80285B48[i].line[0];
    }
    setRGB0(&D_80285B48[1].line[0], 0x80, 0, 0);
    setRGB1(&D_80285B48[1].line[0], 0xFF, 0, 0);
    setRGB0(&D_80285B48[1].line[1], 0x80, 0, 0);
    setRGB1(&D_80285B48[1].line[1], 0xFF, 0, 0);
}

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
void func_802814D4(u_long *ot, DebugLine *line, MATRIX *m, s32 buffer) {
    LINE_G2 *prim;
    SVECTOR unused;
    long sxy2;
    long flag;
    long p;

    prim = &line->line[buffer];
    PushMatrix();
    SetRotMatrix(m);
    SetTransMatrix(m);
    RotTransPers3(&line->start, &line->end, &unused, (long *)&prim->x0, (long *)&prim->x1, &sxy2, &p, &flag);
    addPrim(ot + 1, prim);
    PopMatrix();
}

/* Rebuild each debug line's matrix from its rotation and translation in the camera frame. */
void func_802815B0(void) {
    s32 i;
    long flag;

    if (D_800C268C == 0) {
        for (i = 0; i < 16; i++) {
            func_8003F738(&D_80285B48[i].rot, &D_80285B48[i].matrix);
            PushMatrix();
            MulMatrix2(&D_800AFA64, &D_80285B48[i].matrix);
            PopMatrix();
            func_8004A6DC(&D_80285B48[i].trans, (VECTOR *)D_80285B48[i].matrix.t, &flag);
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
        D_802859A8[D_802859A4].time = now - D_800ADB9C;
        D_802859A8[D_802859A4].name = name;
        D_802859A4++;
        D_800ADB9C = VSync(1);
    }
}

/* The monitor's frame: report mime models whose instance never loaded, cycle
 * the screen (Select, or Select with the debug button for the extra screens;
 * screens 11-12 also toggle the fog editor) and print it: 1 player and
 * scene, 2 memory, 3 event and party state, 4 event actors, 5 CPU time,
 * 6 event variables, 7 particle editor, 8 items, 9 accessories, 10
 * encounters, 11 fog colours, 12 CPU/GPU summary, 13 RGB calculation.
 * Returns the screen shown. */
s32 func_80281B90(u_long *ot) {
    void *seq;
    MonitorActor *actor;
    s32 i, n;
    s32 step;
    s32 a;
    u32 id;
    u8 *rate;
    s32 *player;
    s32 length;
    s32 party;

    if (D_800C268C == 1) {
        return 0;
    }
    for (i = 0; i < D_800AFB0C.count; i++) {
        if (D_800AFB0C.descriptors[i].flags & 0x2000) {
            if (D_800AFB0C.descriptors[i].instance->loaded == 0) {
                func_800379C8("MIME ERROR %d\n", i);
            }
        }
    }
    seq = D_80059558;
sequences:
    if (seq != NULL) {
        seq = ((SoundSequence *)seq)->next;
        goto sequences;
    }
    seq = D_80059440;
channels:
    if (seq != NULL) {
        seq = ((SoundBank *)seq)->next;
        goto channels;
    }
    if (D_80285984 >= 14) {
        D_80285984 = 0;
    }
    if (D_800C3908 & 0x800) {
        if (D_800ADAFC & 1) {
            D_80285984 = 7;
        } else {
            D_80285984 = 0;
        }
        D_800ADAFC = (D_800ADAFC + 1) | 0x8000;
    }
    if ((D_800C3900 & 0x800) && (D_800AFE9C & 0x40)) {
        D_80285984++;
        if (D_80285984 == 8 || D_80285984 == 9) {
            D_80285984 = 10;
        }
        if (D_80285984 == 11 || D_80285984 == 12) {
            if (D_80285984 == 11) {
                D_800B218E = 1;
            } else {
                D_800B218E = 0;
            }
            func_80073E38();
        }
        D_8028597C = 0;
        D_80285980 = 0;
        if (D_80285984 >= 14) {
            D_80285984 = 0;
        }
    }
    switch (D_80285984) {
    case 12:
        func_800379C8("\nCPU=%04d GPU=%04d\n", D_800ADBA0, D_800ADBA4);
        func_800379C8("PolyCount %d / %d\n", D_80059578, D_800595C0);
        func_800379C8("Pos X%6d Z=%6d Y=%6d\n", D_800AFB0C.descriptors[D_800B226C].actor->pos[0].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[2].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[1].part.whole);
        goto free_size;
    case 13:
        func_800379C8("RGB CALC\n\n");
        if (D_800C3900 & 2) {
            if (--D_8028597C < 0) {
                D_8028597C = 3;
            }
        }
        if (D_800C3900 & 1) {
            if (++D_8028597C >= 4) {
                D_8028597C = 0;
            }
        }
        if (D_80065848[2] != 8) {
            step = 0;
        } else {
            step = D_80065848[4];
        }
        if (D_8028597C == 0) {
            D_80285978 += step;
            func_800379C8(">MODE %d\n", (D_80285978 >> 4) & 3);
        } else {
            func_800379C8(" MODE %d\n", (D_80285978 >> 4) & 3);
        }
        if (D_8028597C == 1) {
            D_8028596C = (D_8028596C + step) & 0xFF;
            func_800379C8(">R %d\n", D_8028596C);
        } else {
            func_800379C8(" R %d\n", D_8028596C);
        }
        if (D_8028597C == 2) {
            D_80285970 = (D_80285970 + step) & 0xFF;
            func_800379C8(">G %d\n", D_80285970);
        } else {
            func_800379C8(" G %d\n", D_80285970);
        }
        if (D_8028597C == 3) {
            D_80285974 = (D_80285974 + step) & 0xFF;
            func_800379C8(">B %d\n", D_80285974);
        } else {
            func_800379C8(" B %d\n", D_80285974);
        }
        func_80071D08(1, 1, D_8028596C, D_80285970, D_80285974, (D_80285978 >> 4) & 3);
        break;
    case 11:
        if (D_800C3900 & 2) {
            if (--D_8028597C < 0) {
                D_8028597C = 7;
            }
        }
        if (D_800C3900 & 1) {
            if (++D_8028597C >= 8) {
                D_8028597C = 0;
            }
        }
        if (D_80065848[2] != 8) {
            step = 0;
        } else {
            step = D_80065848[4];
        }
        if (D_8028597C == 0) {
            func_800379C8(">NearColor R=%d\n", D_800B2190[0] += step);
        } else {
            func_800379C8(" NearColor R=%d\n", D_800B2190[0]);
        }
        if (D_8028597C == 1) {
            func_800379C8(">          G=%d\n", D_800B2190[1] += step);
        } else {
            func_800379C8("           G=%d\n", D_800B2190[1]);
        }
        if (D_8028597C == 2) {
            func_800379C8(">          B=%d\n", D_800B2190[2] += step);
        } else {
            func_800379C8("           B=%d\n", D_800B2190[2]);
        }
        if (D_8028597C == 3) {
            func_800379C8(">FarColor  R=%d\n", D_800B2194[0] += step);
        } else {
            func_800379C8(" FarColor  R=%d\n", D_800B2194[0]);
        }
        if (D_8028597C == 4) {
            func_800379C8(">          G=%d\n", D_800B2194[1] += step);
        } else {
            func_800379C8("           G=%d\n", D_800B2194[1]);
        }
        if (D_8028597C == 5) {
            func_800379C8(">          B=%d\n", D_800B2194[2] += step);
        } else {
            func_800379C8("           B=%d\n", D_800B2194[2]);
        }
        if (D_8028597C == 6) {
            func_800379C8(">Near        %d\n", D_800B2198[0] += step * 10);
        } else {
            func_800379C8(" Near        %d\n", D_800B2198[0]);
        }
        if (D_8028597C == 7) {
            func_800379C8(">Far         %d\n", D_800B2198[1] += step * 10);
        } else {
            func_800379C8(" Far         %d\n", D_800B2198[1]);
        }
        break;
    case 1:
        func_800379C8("---------- Player Info -----\n");
        func_800379C8("Pos X%6d Z%6d Y%6d\n", D_800AFB0C.descriptors[D_800B226C].actor->pos[0].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[2].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[1].part.whole);
        actor = D_800AFB0C.descriptors[D_800B226C].actor;
        func_800379C8("Pol=%d Pri=%d ID=%x:%x\n", actor->triangle[actor->layer], actor->layer,
                      D_800AFB0C.triangles[actor->layer][actor->triangle[actor->layer]].attribute, actor->id);
        func_800379C8("P0=%d P1=%d P2=%d C=%d\n", D_800AFB0C.descriptors[D_800B226C].actor->triangle[0],
                      D_800AFB0C.descriptors[D_800B226C].actor->triangle[1],
                      D_800AFB0C.descriptors[D_800B226C].actor->triangle[2], D_800ADB02);
        func_800379C8("MFflag=%x MFlag2=%x N=%d\n", D_800AFB0C.descriptors[D_800B226C].actor->flags,
                      D_800AFB0C.descriptors[D_800B226C].actor->flags2, D_800AFB0C.descriptors[D_800B226C].actor->count);
        func_800379C8("\n---------- Scene Info ------\n");
        func_800379C8("SCRZ=%d DIP=%d Scale=%d\n", D_800AF9F8, D_800AF9FC, (s16)D_800AF9FE);
        a = func_8009A514() & 0xFFFF;
        func_800379C8("CamDIR=%d ChrDIR=%d MapNum=%d\n\n", a, func_8009744C() & 0xFFFF,
                      D_8004F34C & 0x3FFF);
        func_800379C8("Cam AT   X%6d Z%6d Y%6d\n", D_800AF890[0].part.whole, D_800AF890[2].part.whole,
                      D_800AF890[1].part.whole);
        func_800379C8("Cam EYE  X%6d Z%6d Y%6d\n", D_800AF880[0].part.whole, D_800AF880[2].part.whole,
                      D_800AF880[1].part.whole);
        func_800379C8("Cam AT2  X%6d Z%6d Y%6d\n", D_800AF8C0[0].part.whole, D_800AF8C0[2].part.whole,
                      D_800AF8C0[1].part.whole);
        func_800379C8("Cam EYE2 X%6d Z%6d Y%6d\n", D_800AF8B0[0].part.whole, D_800AF8B0[2].part.whole,
                      D_800AF8B0[1].part.whole);
        func_800379C8("DollySet=%02x DollyStop=%02x\n", D_800AF9F4, D_800AF9F5);
        func_800379C8("Angle=%d\n", D_800AF9E6);
        length = D_800AF9F8 * (s16)D_800AF9FE;
        func_800379C8("Length=%d (%d)\n", length >> 12, (length * 2) >> 12);
        func_800379C8("Wave=%02x Music=%02x\n", D_8004F33C, D_8004F338);
        func_800379C8("Total Aactor =%d\n", D_800ADBFC);
        func_800379C8("Total Object =%d\n", D_800AFB0C.count);
        break;
    case 2:
        func_800379C8("---------- Memory Info -----\n");
        func_8003278C(0, D_8028597C, 0xF, 0xDC);
        if (D_800C3900 & 1) {
            D_8028597C += 4;
        }
        if (D_800C3900 & 2) {
            D_8028597C -= 4;
        }
free_size:
        func_800379C8("Free Size=%x\n", func_80032340());
        break;
    case 3:
        i = 0;
        func_800379C8("---------- Event Info ------\n");
        func_800379C8("Event  Time=%d:%d\n", func_800A3018(10) >> 8, func_800A3018(10) & 0xFF);
        func_800379C8("System Time=%d:%d:%d\n", func_800A3018(14), func_800A3018(12) >> 8,
                      func_800A3018(12) & 0xFF);
        for (; i < 11; i++) {
            func_800379C8("Num=%x HP=%3d MP=%2d\n", i, D_8005A39C->characters[i].hp, D_8005A39C->characters[i].ep);
        }
        func_800379C8("Gold=%d\n", D_8005A39C->gold);
        func_800379C8("SinarioFlag=%d\n", (u16)D_800C3A68[0]);
        func_800379C8("Party=%d %d %d\n", D_80062590[0], D_80062590[1], D_80062590[2]);
        n = D_8005A39C->joined;
        func_800379C8("Member ");
        i = 0;
        while (i < 11) {
            if (n & 1) {
                func_800379C8("%d ", i);
            }
            n >>= 1;
            i++;
        }
        func_800379C8("\n");
        n = D_8005A39C->available;
        func_800379C8("FrMask ");
        i = 0;
        while (i < 11) {
            if (!(n & 1)) {
                func_800379C8("%d ", i);
            }
            n >>= 1;
            i++;
        }
        func_800379C8("\n");
        n = D_8005A39C->locked;
        func_800379C8("FrLock ");
        i = 0;
        while (i < 11) {
            if (n & 1) {
                func_800379C8("%d ", i);
            }
            n >>= 1;
            i++;
        }
        func_800379C8("\n");
        func_800379C8("GearRide=%d %d %d\n", D_8005A39C->inGear[0], D_8005A39C->inGear[1], D_8005A39C->inGear[2]);
        func_800379C8("GearNum=");
        for (i = 0; i < 3; i++) {
            party = D_80062590[i];
            if (party == 0xFF) {
                break;
            }
            func_800379C8(" %d", D_8005A39C->characters[party].gearId);
        }
        func_800379C8("\nTYPE=");
        for (i = 0; i < 3; i++) {
            if (D_80062590[i] == 0xFF) {
                break;
            }
            if (D_8005A444[i] == 0xFF) {
                break;
            }
            switch ((D_800AFB0C.descriptors[D_8005A444[i]].actor->flags >> 8) & 7) {
            case 1:
                func_800379C8("People ");
                break;
            case 2:
                func_800379C8("Robo ");
                break;
            case 4:
                func_800379C8("Play ");
                break;
            default:
                func_800379C8("?%d ", (D_800AFB0C.descriptors[D_8005A444[i]].actor->flags >> 8) & 7);
                break;
            }
        }
        func_800379C8(" ID=");
        id = D_800AFB0C.descriptors[D_800B226C].actor->id;
        if (!(id & 0x80)) {
            func_800379C8("C");
        } else {
            func_800379C8("-");
        }
        if (!(id & 0x40)) {
            func_800379C8("G");
        } else {
            func_800379C8("-");
        }
        if (!(id & 0x20)) {
            func_800379C8("P");
        } else {
            func_800379C8("-");
        }
        break;
    case 4:
        func_800379C8("---------- Event DEBUG -----\n");
        for (i = D_8028597C, n = 0; i < D_800ADBFC; i++, n++) {
            func_800379C8("ActNum=%3d RUN=%04x\n", i,
                          D_800AFB0C.descriptors[i].actor->threads[D_800AFB0C.descriptors[i].actor->thread].pc);
            func_800379C8("P0=%d P1=%d P2=%d P=%d I=%x:%x\n", D_800AFB0C.descriptors[i].actor->triangle[0],
                          D_800AFB0C.descriptors[i].actor->triangle[1], D_800AFB0C.descriptors[i].actor->triangle[2],
                          D_800AFB0C.descriptors[i].actor->layer,
                          D_800AFB0C.triangles[D_800AFB0C.descriptors[i].actor->layer]
                                    [D_800AFB0C.descriptors[i].actor->triangle[D_800AFB0C.descriptors[i].actor->layer]]
                                        .attribute,
                          D_800AFB0C.descriptors[i].actor->id);
            func_800379C8("Pos X%6d Z%6d Y%6d\n", D_800AFB0C.descriptors[i].actor->pos[0].part.whole,
                          D_800AFB0C.descriptors[i].actor->pos[2].part.whole, D_800AFB0C.descriptors[i].actor->pos[1].part.whole);
            func_800379C8("M1=%x M2=%x", D_800AFB0C.descriptors[i].actor->flags, D_800AFB0C.descriptors[i].actor->flags2);
            if (!(D_800AFB0C.descriptors[i].actor->flags2 & 0x4000000)) {
                func_800379C8("\n\n");
            } else {
                func_800379C8(" TALK OFF\n\n");
            }
            if (n >= 6) {
                break;
            }
        }
        if (D_800C3900 & 1) {
            D_8028597C++;
        }
        if (D_800C3900 & 2) {
            D_8028597C--;
        }
        break;
    case 5:
        func_800379C8("---------- CPU Time --------\n");
        for (i = 0; i < D_802859A4; i++) {
            func_800379C8("%s = %6d\n", D_802859A8[i].name, D_802859A8[i].time);
        }
        func_800379C8("\nCPU=%6d GPU=%6d\n", D_800ADBA0, D_800ADBA4);
        func_800379C8("PolyCount %d / %d\n", D_80059578, D_800595C0);
        break;
    case 6:
        func_800379C8("---------- RAM MAP ---------\n");
        for (i = D_8028597C, n = 0; i < 0x400; i++, n++) {
            func_800379C8("ADD %04x:%08x %06d\n", i * 2, func_800A3018(i * 2), func_800A3018(i * 2));
            if (n >= 16) {
                break;
            }
        }
        if (D_800C3900 & 1) {
            D_8028597C += 4;
        }
        if (D_800C3900 & 2) {
            D_8028597C -= 4;
        }
        break;
    case 7:
        func_800379C8("---------- PARTICLE -----------\n");
        if (D_800C3908 & 0x100) {
            player = &D_800B226C;
            func_800A98E8(*player, 1);
            for (i = 0; i < 8; i++) {
                if (D_800ADB40 == 0xFF) {
                    D_800B02CC[i].unk50[1] = *player;
                } else {
                    D_800B02CC[i].unk50[1] = D_800ADB40;
                }
            }
            func_800A99A8(D_800B226C);
        }
        func_802835E0();
        break;
    case 8:
        func_800379C8("---------- ITEM -------------\n");
        for (i = D_8028597C, n = 0; i < 0x96; i += 4, n++) {
            func_800379C8("%03d=%03d %03d=%03d %03d=%03d %03d=%03d\n", D_8005A39C->itemIds[i],
                          D_8005A39C->itemCounts[i], D_8005A39C->itemIds[i + 1],
                          D_8005A39C->itemCounts[i + 1], D_8005A39C->itemIds[i + 2],
                          D_8005A39C->itemCounts[i + 2], D_8005A39C->itemIds[i + 3],
                          D_8005A39C->itemCounts[i + 3]);
            if (n >= 16) {
                break;
            }
        }
        if (D_800C3900 & 1) {
            D_8028597C += 4;
        }
        if (D_800C3900 & 2) {
            D_8028597C -= 4;
        }
        break;
    case 9:
        func_800379C8("---------- ACC --------------\n");
        for (i = D_8028597C, n = 0; i < 0xC8; i += 4, n++) {
            func_800379C8("%03d=%03d %03d=%03d %03d=%03d %03d=%03d\n", D_8005A39C->accessoryIds[i],
                          D_8005A39C->accessoryCounts[i], D_8005A39C->accessoryIds[i + 1],
                          D_8005A39C->accessoryCounts[i + 1], D_8005A39C->accessoryIds[i + 2],
                          D_8005A39C->accessoryCounts[i + 2], D_8005A39C->accessoryIds[i + 3],
                          D_8005A39C->accessoryCounts[i + 3]);
            if (n >= 16) {
                break;
            }
        }
        if (D_800C3900 & 1) {
            D_8028597C += 4;
        }
        if (D_800C3900 & 2) {
            D_8028597C -= 4;
        }
        break;
    case 10:
        func_800379C8("---------- ENCOUNT -------------\n");
        if (D_800C3900 & 1) {
            D_8028597C++;
        }
        if (D_800C3900 & 2) {
            D_8028597C--;
        }
        rate = D_80065ADC;
        for (i = 0; i < 16; i++) {
            func_800379C8("%2d %3d %d\n", i, rate[i], D_80285B28[i]);
        }
        switch (D_8028597C & 3) {
        case 0:
            player = &D_800B2298;
            func_800379C8(">TIME   =%d\n", *player);
            func_800379C8(" ENCOUNT=%d\n", D_800B229C);
            func_800379C8(" SET");
            if (D_800C3900 & 4) {
                (*player)++;
            }
            if (D_800C3900 & 8) {
                (*player)--;
            }
            break;
        case 1:
            func_800379C8(" TIME   =%d\n", D_800B2298);
            func_800379C8(">ENCOUNT=%d\n", D_800B229C);
            func_800379C8(" SET");
            if (D_800C3900 & 4) {
                D_800B229C++;
            }
            if (D_800C3900 & 8) {
                D_800B229C--;
            }
            D_800B229C &= 0x1F;
            break;
        case 2:
            func_800379C8(" TIME   =%d\n", D_800B2298);
            func_800379C8(" ENCOUNT=%d\n", D_800B229C);
            func_800379C8(">SET");
            func_8008E718();
            break;
        }
        if (D_8028598C != 0) {
            if (--D_80285990 == 0) {
                D_8028598C = 0;
            }
            func_800379C8("COUNT=%d NUM=%d\n", D_80285994, D_80285998);
        } else {
            D_80285990 = 60;
        }
        func_800A3F4C();
        break;
    }
    if (D_8028597C < 0) {
        D_8028597C = 0;
    }
    return D_80285984;
}

/* Particle emitter editor screen: list the edited emitter's parameters
 * (rows 0-21) or its eight angle offsets (rows 22+), with the cursor row and
 * column marked; Up/Down move the row, Left/Right the column, and the
 * editor steps the selected value. */
void func_802835E0(void) {
    s32 selected;
    s32 row;
    s32 cursor;
    s32 column;
    s32 i;

    cursor = D_8028599C;
    column = D_802859A0;
    if (cursor < 22) {
        row = func_8028439C(0, cursor, &selected);
        func_800379C8("BANK    = %d\n", D_800B0044);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("MAX     = %d\n", D_800B02CC[D_800B0044].max);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SWAIT   = %d\n", D_800B02CC[D_800B0044].start_wait);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("EWAIT   = %d\n", D_800B02CC[D_800B0044].end_wait);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SPOS    =");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].start_pos.vx);
        func_80284354(1, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].start_pos.vy);
        func_80284354(2, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].start_pos.vz);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("EPOS    =");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].end_pos.vx);
        func_80284354(1, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].end_pos.vy);
        func_80284354(2, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].end_pos.vz);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SPEED   = ");
        func_80284354(0, column, selected);
        func_800379C8("%d * ", D_800B02CC[D_800B0044].speed);
        func_80284354(1, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].speed_scale);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("GRAVITE =");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].gravity.vx);
        func_80284354(1, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].gravity.vy);
        func_80284354(2, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].gravity.vz);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SRANGE  = %d\n", D_800B02CC[D_800B0044].start_range);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("ERANGE  = %d\n", D_800B02CC[D_800B0044].end_range);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("PSWAIT  = %d\n", D_800B02CC[D_800B0044].particle_start_wait);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("PEWAIT  = %d\n", D_800B02CC[D_800B0044].particle_end_wait);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SHAPE   = %d\n", D_800B02CC[D_800B0044].shape);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SCALE   =");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].scale.vx);
        func_80284354(1, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].scale.vy);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SCALEOFS=");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].scale_offset.vx);
        func_80284354(1, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].scale_offset.vy);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("COLOR   =");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].color[0]);
        func_80284354(1, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].color[1]);
        func_80284354(2, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].color[2]);
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("COLOROFS=");
        func_80284354(0, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].color_offset[0]);
        func_80284354(1, column, selected);
        func_800379C8("%d", D_800B02CC[D_800B0044].color_offset[1]);
        func_80284354(2, column, selected);
        func_800379C8("%d\n", D_800B02CC[D_800B0044].color_offset[2]);
        row = func_8028439C(row, cursor, &selected);
        if (!D_800B02CC[D_800B0044].flags.bits.randrot) {
            func_800379C8("RANDROT = OFF\n");
        } else {
            func_800379C8("RANDROT = ON\n");
        }
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("SORT    = ");
        switch (D_800B02CC[D_800B0044].flags.bits.sort) {
        case 0:
            func_800379C8("TOP\n");
            break;
        case 1:
            func_800379C8("MID\n");
            break;
        case 2:
            func_800379C8("NORMAL\n");
            break;
        case 3:
            func_800379C8("BACK\n");
            break;
        }
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("COLMODE = ");
        switch (D_800B02CC[D_800B0044].flags.bits.colmode) {
        case 0:
            func_800379C8("1.0*Bk + 1.0*Fw\n");
            break;
        case 1:
            func_800379C8("1.0*Bk - 1.0*Fw\n");
            break;
        case 2:
            func_800379C8("1.0*Bk + 0.25*Fw\n");
            break;
        case 3:
            func_800379C8("0.5*Bk + 0.5*Fw\n");
            break;
        }
        row = func_8028439C(row, cursor, &selected);
        func_800379C8("ROTANGLE= %d\n", D_800B02CC[D_800B0044].rot_angle);
        func_8028439C(row, cursor, &selected);
        func_800379C8("RANGEMOD= ");
        switch (D_800B02CC[D_800B0044].flags.bits.rangemod) {
        case 0:
            func_800379C8("RANDUM (0)");
            break;
        case 2:
            func_800379C8("CIRCLE (1)");
            break;
        case 1:
            func_800379C8("LINE (2)");
            break;
        }
        func_800379C8("ROTANGLE= %d\n", D_800B02CC[D_800B0044].rot_angle);
    } else {
        row = 22;
        for (i = 0; i < 8; i++) {
            row = func_8028439C(row, cursor, &selected);
            func_800379C8("ANGOFFS%d=", i);
            func_80284354(0, column, selected);
            func_800379C8("%d", D_800B02CC[D_800B0044].angle_offsets[i][0]);
            func_80284354(1, column, selected);
            func_800379C8("%d\n", D_800B02CC[D_800B0044].angle_offsets[i][1]);
        }
    }
    func_80036DC8(0xFF, 0xFF, 0xFF);
    if (D_800C3908 & 0x4000) {
        column = 0;
        if (cursor < 29) {
            cursor++;
        }
    }
    if (D_800C3908 & 0x1000) {
        column = 0;
        if (cursor > 0) {
            cursor--;
        }
    }
    if (D_800C3908 & 0x2000) {
        if (cursor == 13 || cursor == 14) {
            if (column < 1) {
                column++;
            }
        } else if (column < 2) {
            column++;
        }
    }
    if ((D_800C3908 & 0x8000) && column > 0) {
        column--;
    }
    func_802846CC(column, cursor);
    D_8028599C = cursor;
    D_802859A0 = column;
}

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



/* Edit field `item` (component `axis`) of the selected particle emitter.
 * Preserve the old flag word across the edit call; the destination emitter
 * is selected again after the call. */
void func_802846CC(s32 axis, u32 item) {
    s32 flags;
    s32 value;

    switch (item) {
    case 0:
        D_800B0044 = func_80284424(D_800B0044, 0, 7);
        break;
    case 1:
        D_800B02CC[D_800B0044].max = func_80284424(D_800B02CC[D_800B0044].max, 0, 0xFF);
        break;
    case 2:
        D_800B02CC[D_800B0044].start_wait =
            func_80284424(D_800B02CC[D_800B0044].start_wait, 0, 0x7FFF);
        break;
    case 3:
        D_800B02CC[D_800B0044].end_wait =
            func_80284424(D_800B02CC[D_800B0044].end_wait, 1, 0x7FFF);
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
        flags = D_800B02CC[D_800B0044].flags.value;
        value = flags & 1;
        flags &= 0xFFFE;
        flags |= func_80284424(value, 0, 1);
        D_800B02CC[D_800B0044].flags.value = flags;
        break;
    case 18:
        flags = D_800B02CC[D_800B0044].flags.value;
        value = (flags >> 1) & 3;
        flags &= 0xFFF9;
        flags |= func_80284424(value, 0, 3) << 1;
        D_800B02CC[D_800B0044].flags.value = flags;
        break;
    case 19:
        flags = D_800B02CC[D_800B0044].flags.value;
        value = (flags >> 8) & 3;
        flags &= 0xFCFF;
        flags |= func_80284424(value, 0, 3) << 8;
        D_800B02CC[D_800B0044].flags.value = flags;
        break;
    case 20:
        D_800B02CC[D_800B0044].rot_angle =
            func_80284424(D_800B02CC[D_800B0044].rot_angle, 0, 0xFFF);
        break;
    case 21:
        flags = D_800B02CC[D_800B0044].flags.value;
        value = (flags >> 6) & 3;
        flags &= 0xFF3F;
        flags |= func_80284424(value, 0, 2) << 6;
        D_800B02CC[D_800B0044].flags.value = flags;
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

/* With L2 and the debug button held, move the camera by the pad's analog
 * steps: dolly (mode 4), zoom (mode 8) or rotate and raise (other modes).
 * Accumulate zoom as a signed word and narrow only at its final store. */
void func_80284EA4(void) {
    s32 zoom;

    if ((D_800AFE9C & 1) && (D_800AFE9C & 0x40)) {
        if (D_80065848[2] == 4) {
            D_800ADB98 = 1;
            D_800ADB94 += D_80065848[4];
        } else if (D_80065848[2] == 8) {
            zoom = D_800AF9FC;
            zoom += ((u32)D_80065848[4] << 4) >> 5;
            D_800AF9FC = zoom;
            D_800AF984 = 1;
            D_800AF988 = 1;
        } else {
            D_800AF984 = 1;
            D_800AF988 = 1;
            D_800AF9FE += D_80065848[4] << 4;
            D_800AF9F0 += D_80065848[3] << 18;
            D_800AF9E6 = D_800AF9F0 >> 16;
        }
    }
}

/* Skip a section's position and offset, read its size and upload its
 * pixels at `rect`, leaving `p` past the pixels. */
#define LOAD_SECTION_IMAGE(rect, p) do {     \
        (p) += 4;                            \
        (rect).w = *(p)++;                   \
        (rect).h = *(p)++;                   \
        LoadImage(&(rect), (u_long *)(p));   \
        (p) += (rect).w * (rect).h;          \
    } while (0)

/* Load an image archive's sections into VRAM; sections of kind 0x1100 and
 * 0x1101 are placed by `mode0`/`mode1`: 1 at the given origin plus the
 * section offset, 2 also plus the section position, else at the position.
 * Returns 1 for an unknown section kind, 0 after all sections are loaded. */
s32 func_80284FB4(u32 *archive, s16 mode0, s16 x0, s16 y0, s16 mode1, u16 x1, u16 y1) {
    s32 count;
    s32 i;
    u16 *p;
    u32 kind;
    RECT rect;

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
        } else {
            if (kind != 0x1101) {
                return 1;
            }
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
        }
        LOAD_SECTION_IMAGE(rect, p);
    }
    return 0;
}

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
