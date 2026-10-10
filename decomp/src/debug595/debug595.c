/* debug595: the field debug monitor (disc 1 file 595, disc 2 file 590), a
 * development tool linked for the dev kit's 8 MB RAM at 0x80280000. The field
 * overlay's setup (80077e88) reads it as directory 4 file 0xad into 0x80280000
 * when D_800C268C and mode_field_standalone are clear; on a retail 2 MB unit the image
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
s32 D_80285998 = 0; /* the last encounter's formation */
s32 D_8028599C = 0; /* particle editor row */
s32 D_802859A0 = 0; /* particle editor column */
s16 D_802859A4 = 0; /* CPU-time marks this frame */
/* The CPU-time marks: 32 records of 12 bytes exactly fill 802859a8-80285b28;
 * the code addresses their time (+4) and name (+8). */
CpuMark D_802859A8[32] = {0};
s16 D_80285B28[16] = {0}; /* encounters per formation */
DebugLine D_80285B48[16] = {0}; /* the debug lines */

/* Reset the two frame counters. */
void func_802811EC(void) {
    D_80285980 = 0;
    D_8028597C = 0;
}

/* Count an encounter of `formation` (of the map's set, as the field's
 * func_80079288 draws it) and restart the 60-frame notice. */
void func_80281204(s32 formation) {
    D_8028598C = 1;
    D_80285990 = 60;
    D_80285998 = formation;
    D_80285B28[formation]++;
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
        console_flush(D_800C426C + 0x34);
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
            gpu_build_rotation_matrix(&D_80285B48[i].rot, &D_80285B48[i].matrix);
            PushMatrix();
            MulMatrix2(&D_800AFA64, &D_80285B48[i].matrix);
            PopMatrix();
            RotTrans(&D_80285B48[i].trans, (VECTOR *)D_80285B48[i].matrix.t, &flag);
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
        console_report_printf("REC %07d %07d %07d %07d\n", v[0], v[1], v[2], v[3]);
    }
}

/* Print a matrix row by row with its translation. */
void func_802819DC(MATRIX *m) {
    if (D_800C268C != 1) {
        console_report_printf("MTX %06d %06d %06d %06d\n", m->m[0][0], m->m[0][1], m->m[0][2], m->t[0]);
        console_report_printf("    %06d %06d %06d %06d\n", m->m[1][0], m->m[1][1], m->m[1][2], m->t[1]);
        console_report_printf("    %06d %06d %06d %06d\n", m->m[2][0], m->m[2][1], m->m[2][2], m->t[2]);
    }
}

/* Print a long vector. */
void func_80281A78(VECTOR *v) {
    if (D_800C268C != 1) {
        console_report_printf("VEC  %d %d %d\n", v->vx, v->vy, v->vz);
    }
}

/* Print a short vector. */
void func_80281ABC(SVECTOR *v) {
    if (D_800C268C != 1) {
        console_report_printf("SVEC %d %d %d\n", v->vx, v->vy, v->vz);
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
                console_report_printf("MIME ERROR %d\n", i);
            }
        }
    }
    seq = sound_wave_bank_list;
sequences:
    if (seq != NULL) {
        seq = ((SoundSequence *)seq)->next;
        goto sequences;
    }
    seq = sound_effect_bank_list;
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
        console_report_printf("\nCPU=%04d GPU=%04d\n", D_800ADBA0, D_800ADBA4);
        console_report_printf("PolyCount %d / %d\n", model_drawn_primitive_count, model_submitted_primitive_count);
        console_report_printf("Pos X%6d Z=%6d Y=%6d\n", D_800AFB0C.descriptors[D_800B226C].actor->pos[0].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[2].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[1].part.whole);
        goto free_size;
    case 13:
        console_report_printf("RGB CALC\n\n");
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
        if (mode_field_pointer_state[2] != 8) {
            step = 0;
        } else {
            step = mode_field_pointer_state[4];
        }
        if (D_8028597C == 0) {
            D_80285978 += step;
            console_report_printf(">MODE %d\n", (D_80285978 >> 4) & 3);
        } else {
            console_report_printf(" MODE %d\n", (D_80285978 >> 4) & 3);
        }
        if (D_8028597C == 1) {
            D_8028596C = (D_8028596C + step) & 0xFF;
            console_report_printf(">R %d\n", D_8028596C);
        } else {
            console_report_printf(" R %d\n", D_8028596C);
        }
        if (D_8028597C == 2) {
            D_80285970 = (D_80285970 + step) & 0xFF;
            console_report_printf(">G %d\n", D_80285970);
        } else {
            console_report_printf(" G %d\n", D_80285970);
        }
        if (D_8028597C == 3) {
            D_80285974 = (D_80285974 + step) & 0xFF;
            console_report_printf(">B %d\n", D_80285974);
        } else {
            console_report_printf(" B %d\n", D_80285974);
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
        if (mode_field_pointer_state[2] != 8) {
            step = 0;
        } else {
            step = mode_field_pointer_state[4];
        }
        if (D_8028597C == 0) {
            console_report_printf(">NearColor R=%d\n", D_800B2190[0] += step);
        } else {
            console_report_printf(" NearColor R=%d\n", D_800B2190[0]);
        }
        if (D_8028597C == 1) {
            console_report_printf(">          G=%d\n", D_800B2190[1] += step);
        } else {
            console_report_printf("           G=%d\n", D_800B2190[1]);
        }
        if (D_8028597C == 2) {
            console_report_printf(">          B=%d\n", D_800B2190[2] += step);
        } else {
            console_report_printf("           B=%d\n", D_800B2190[2]);
        }
        if (D_8028597C == 3) {
            console_report_printf(">FarColor  R=%d\n", D_800B2194[0] += step);
        } else {
            console_report_printf(" FarColor  R=%d\n", D_800B2194[0]);
        }
        if (D_8028597C == 4) {
            console_report_printf(">          G=%d\n", D_800B2194[1] += step);
        } else {
            console_report_printf("           G=%d\n", D_800B2194[1]);
        }
        if (D_8028597C == 5) {
            console_report_printf(">          B=%d\n", D_800B2194[2] += step);
        } else {
            console_report_printf("           B=%d\n", D_800B2194[2]);
        }
        if (D_8028597C == 6) {
            console_report_printf(">Near        %d\n", D_800B2198[0] += step * 10);
        } else {
            console_report_printf(" Near        %d\n", D_800B2198[0]);
        }
        if (D_8028597C == 7) {
            console_report_printf(">Far         %d\n", D_800B2198[1] += step * 10);
        } else {
            console_report_printf(" Far         %d\n", D_800B2198[1]);
        }
        break;
    case 1:
        console_report_printf("---------- Player Info -----\n");
        console_report_printf("Pos X%6d Z%6d Y%6d\n", D_800AFB0C.descriptors[D_800B226C].actor->pos[0].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[2].part.whole,
                      D_800AFB0C.descriptors[D_800B226C].actor->pos[1].part.whole);
        actor = D_800AFB0C.descriptors[D_800B226C].actor;
        console_report_printf("Pol=%d Pri=%d ID=%x:%x\n", actor->triangle[actor->layer], actor->layer,
                      D_800AFB0C.triangles[actor->layer][actor->triangle[actor->layer]].attribute, actor->id);
        console_report_printf("P0=%d P1=%d P2=%d C=%d\n", D_800AFB0C.descriptors[D_800B226C].actor->triangle[0],
                      D_800AFB0C.descriptors[D_800B226C].actor->triangle[1],
                      D_800AFB0C.descriptors[D_800B226C].actor->triangle[2], D_800ADB02);
        console_report_printf("MFflag=%x MFlag2=%x N=%d\n", D_800AFB0C.descriptors[D_800B226C].actor->flags,
                      D_800AFB0C.descriptors[D_800B226C].actor->flags2, D_800AFB0C.descriptors[D_800B226C].actor->count);
        console_report_printf("\n---------- Scene Info ------\n");
        console_report_printf("SCRZ=%d DIP=%d Scale=%d\n", D_800AF9F8, D_800AF9FC, (s16)D_800AF9FE);
        a = func_8009A514() & 0xFFFF;
        console_report_printf("CamDIR=%d ChrDIR=%d MapNum=%d\n\n", a, func_8009744C() & 0xFFFF,
                      mode_field_map_id & 0x3FFF);
        console_report_printf("Cam AT   X%6d Z%6d Y%6d\n", D_800AF890[0].part.whole, D_800AF890[2].part.whole,
                      D_800AF890[1].part.whole);
        console_report_printf("Cam EYE  X%6d Z%6d Y%6d\n", D_800AF880[0].part.whole, D_800AF880[2].part.whole,
                      D_800AF880[1].part.whole);
        console_report_printf("Cam AT2  X%6d Z%6d Y%6d\n", D_800AF8C0[0].part.whole, D_800AF8C0[2].part.whole,
                      D_800AF8C0[1].part.whole);
        console_report_printf("Cam EYE2 X%6d Z%6d Y%6d\n", D_800AF8B0[0].part.whole, D_800AF8B0[2].part.whole,
                      D_800AF8B0[1].part.whole);
        console_report_printf("DollySet=%02x DollyStop=%02x\n", D_800AF9F4, D_800AF9F5);
        console_report_printf("Angle=%d\n", D_800AF9E6);
        length = D_800AF9F8 * (s16)D_800AF9FE;
        console_report_printf("Length=%d (%d)\n", length >> 12, (length * 2) >> 12);
        console_report_printf("Wave=%02x Music=%02x\n", mode_music_loaded_wave, mode_music_loaded_track);
        console_report_printf("Total Aactor =%d\n", D_800ADBFC);
        console_report_printf("Total Object =%d\n", D_800AFB0C.count);
        break;
    case 2:
        console_report_printf("---------- Memory Info -----\n");
        heap_print_report(0, D_8028597C, 0xF, 0xDC);
        if (D_800C3900 & 1) {
            D_8028597C += 4;
        }
        if (D_800C3900 & 2) {
            D_8028597C -= 4;
        }
free_size:
        console_report_printf("Free Size=%x\n", heap_get_free_total());
        break;
    case 3:
        i = 0;
        console_report_printf("---------- Event Info ------\n");
        console_report_printf("Event  Time=%d:%d\n", func_800A3018(10) >> 8, func_800A3018(10) & 0xFF);
        console_report_printf("System Time=%d:%d:%d\n", func_800A3018(14), func_800A3018(12) >> 8,
                      func_800A3018(12) & 0xFF);
        for (; i < 11; i++) {
            console_report_printf("Num=%x HP=%3d MP=%2d\n", i, game_current_data->characters[i].hp, game_current_data->characters[i].ep);
        }
        console_report_printf("Gold=%d\n", game_current_data->gold);
        console_report_printf("SinarioFlag=%d\n", (u16)D_800C3A68[0]);
        console_report_printf("Party=%d %d %d\n", mode_party_members[0], mode_party_members[1], mode_party_members[2]);
        n = game_current_data->joined;
        console_report_printf("Member ");
        i = 0;
        while (i < 11) {
            if (n & 1) {
                console_report_printf("%d ", i);
            }
            n >>= 1;
            i++;
        }
        console_report_printf("\n");
        n = game_current_data->available;
        console_report_printf("FrMask ");
        i = 0;
        while (i < 11) {
            if (!(n & 1)) {
                console_report_printf("%d ", i);
            }
            n >>= 1;
            i++;
        }
        console_report_printf("\n");
        n = game_current_data->locked;
        console_report_printf("FrLock ");
        i = 0;
        while (i < 11) {
            if (n & 1) {
                console_report_printf("%d ", i);
            }
            n >>= 1;
            i++;
        }
        console_report_printf("\n");
        console_report_printf("GearRide=%d %d %d\n", game_current_data->inGear[0], game_current_data->inGear[1], game_current_data->inGear[2]);
        console_report_printf("GearNum=");
        for (i = 0; i < 3; i++) {
            party = mode_party_members[i];
            if (party == 0xFF) {
                break;
            }
            console_report_printf(" %d", game_current_data->characters[party].gearId);
        }
        console_report_printf("\nTYPE=");
        for (i = 0; i < 3; i++) {
            if (mode_party_members[i] == 0xFF) {
                break;
            }
            if (mode_party_actors[i] == 0xFF) {
                break;
            }
            switch ((D_800AFB0C.descriptors[mode_party_actors[i]].actor->flags >> 8) & 7) {
            case 1:
                console_report_printf("People ");
                break;
            case 2:
                console_report_printf("Robo ");
                break;
            case 4:
                console_report_printf("Play ");
                break;
            default:
                console_report_printf("?%d ", (D_800AFB0C.descriptors[mode_party_actors[i]].actor->flags >> 8) & 7);
                break;
            }
        }
        console_report_printf(" ID=");
        id = D_800AFB0C.descriptors[D_800B226C].actor->id;
        if (!(id & 0x80)) {
            console_report_printf("C");
        } else {
            console_report_printf("-");
        }
        if (!(id & 0x40)) {
            console_report_printf("G");
        } else {
            console_report_printf("-");
        }
        if (!(id & 0x20)) {
            console_report_printf("P");
        } else {
            console_report_printf("-");
        }
        break;
    case 4:
        console_report_printf("---------- Event DEBUG -----\n");
        for (i = D_8028597C, n = 0; i < D_800ADBFC; i++, n++) {
            console_report_printf("ActNum=%3d RUN=%04x\n", i,
                          D_800AFB0C.descriptors[i].actor->threads[D_800AFB0C.descriptors[i].actor->thread].pc);
            console_report_printf("P0=%d P1=%d P2=%d P=%d I=%x:%x\n", D_800AFB0C.descriptors[i].actor->triangle[0],
                          D_800AFB0C.descriptors[i].actor->triangle[1], D_800AFB0C.descriptors[i].actor->triangle[2],
                          D_800AFB0C.descriptors[i].actor->layer,
                          D_800AFB0C.triangles[D_800AFB0C.descriptors[i].actor->layer]
                                    [D_800AFB0C.descriptors[i].actor->triangle[D_800AFB0C.descriptors[i].actor->layer]]
                                        .attribute,
                          D_800AFB0C.descriptors[i].actor->id);
            console_report_printf("Pos X%6d Z%6d Y%6d\n", D_800AFB0C.descriptors[i].actor->pos[0].part.whole,
                          D_800AFB0C.descriptors[i].actor->pos[2].part.whole, D_800AFB0C.descriptors[i].actor->pos[1].part.whole);
            console_report_printf("M1=%x M2=%x", D_800AFB0C.descriptors[i].actor->flags, D_800AFB0C.descriptors[i].actor->flags2);
            if (!(D_800AFB0C.descriptors[i].actor->flags2 & 0x4000000)) {
                console_report_printf("\n\n");
            } else {
                console_report_printf(" TALK OFF\n\n");
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
        console_report_printf("---------- CPU Time --------\n");
        for (i = 0; i < D_802859A4; i++) {
            console_report_printf("%s = %6d\n", D_802859A8[i].name, D_802859A8[i].time);
        }
        console_report_printf("\nCPU=%6d GPU=%6d\n", D_800ADBA0, D_800ADBA4);
        console_report_printf("PolyCount %d / %d\n", model_drawn_primitive_count, model_submitted_primitive_count);
        break;
    case 6:
        console_report_printf("---------- RAM MAP ---------\n");
        for (i = D_8028597C, n = 0; i < 0x400; i++, n++) {
            console_report_printf("ADD %04x:%08x %06d\n", i * 2, func_800A3018(i * 2), func_800A3018(i * 2));
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
        console_report_printf("---------- PARTICLE -----------\n");
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
        console_report_printf("---------- ITEM -------------\n");
        for (i = D_8028597C, n = 0; i < 0x96; i += 4, n++) {
            console_report_printf("%03d=%03d %03d=%03d %03d=%03d %03d=%03d\n", game_current_data->itemIds[i],
                          game_current_data->itemCounts[i], game_current_data->itemIds[i + 1],
                          game_current_data->itemCounts[i + 1], game_current_data->itemIds[i + 2],
                          game_current_data->itemCounts[i + 2], game_current_data->itemIds[i + 3],
                          game_current_data->itemCounts[i + 3]);
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
        console_report_printf("---------- ACC --------------\n");
        for (i = D_8028597C, n = 0; i < 0xC8; i += 4, n++) {
            console_report_printf("%03d=%03d %03d=%03d %03d=%03d %03d=%03d\n", game_current_data->accessoryIds[i],
                          game_current_data->accessoryCounts[i], game_current_data->accessoryIds[i + 1],
                          game_current_data->accessoryCounts[i + 1], game_current_data->accessoryIds[i + 2],
                          game_current_data->accessoryCounts[i + 2], game_current_data->accessoryIds[i + 3],
                          game_current_data->accessoryCounts[i + 3]);
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
        console_report_printf("---------- ENCOUNT -------------\n");
        if (D_800C3900 & 1) {
            D_8028597C++;
        }
        if (D_800C3900 & 2) {
            D_8028597C--;
        }
        rate = formation_encounter_weights;
        for (i = 0; i < 16; i++) {
            console_report_printf("%2d %3d %d\n", i, rate[i], D_80285B28[i]);
        }
        switch (D_8028597C & 3) {
        case 0:
            player = &D_800B2298;
            console_report_printf(">TIME   =%d\n", *player);
            console_report_printf(" ENCOUNT=%d\n", D_800B229C);
            console_report_printf(" SET");
            if (D_800C3900 & 4) {
                (*player)++;
            }
            if (D_800C3900 & 8) {
                (*player)--;
            }
            break;
        case 1:
            console_report_printf(" TIME   =%d\n", D_800B2298);
            console_report_printf(">ENCOUNT=%d\n", D_800B229C);
            console_report_printf(" SET");
            if (D_800C3900 & 4) {
                D_800B229C++;
            }
            if (D_800C3900 & 8) {
                D_800B229C--;
            }
            D_800B229C &= 0x1F;
            break;
        case 2:
            console_report_printf(" TIME   =%d\n", D_800B2298);
            console_report_printf(" ENCOUNT=%d\n", D_800B229C);
            console_report_printf(">SET");
            func_8008E718();
            break;
        }
        if (D_8028598C != 0) {
            if (--D_80285990 == 0) {
                D_8028598C = 0;
            }
            console_report_printf("COUNT=%d NUM=%d\n", D_80285994, D_80285998);
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
        console_report_printf("BANK    = %d\n", D_800B0044);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("MAX     = %d\n", D_800B02CC[D_800B0044].max);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SWAIT   = %d\n", D_800B02CC[D_800B0044].start_wait);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("EWAIT   = %d\n", D_800B02CC[D_800B0044].end_wait);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SPOS    =");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].start_pos.vx);
        func_80284354(1, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].start_pos.vy);
        func_80284354(2, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].start_pos.vz);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("EPOS    =");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].end_pos.vx);
        func_80284354(1, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].end_pos.vy);
        func_80284354(2, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].end_pos.vz);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SPEED   = ");
        func_80284354(0, column, selected);
        console_report_printf("%d * ", D_800B02CC[D_800B0044].speed);
        func_80284354(1, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].speed_scale);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("GRAVITE =");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].gravity.vx);
        func_80284354(1, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].gravity.vy);
        func_80284354(2, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].gravity.vz);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SRANGE  = %d\n", D_800B02CC[D_800B0044].start_range);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("ERANGE  = %d\n", D_800B02CC[D_800B0044].end_range);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("PSWAIT  = %d\n", D_800B02CC[D_800B0044].particle_start_wait);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("PEWAIT  = %d\n", D_800B02CC[D_800B0044].particle_end_wait);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SHAPE   = %d\n", D_800B02CC[D_800B0044].shape);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SCALE   =");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].scale.vx);
        func_80284354(1, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].scale.vy);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SCALEOFS=");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].scale_offset.vx);
        func_80284354(1, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].scale_offset.vy);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("COLOR   =");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].color[0]);
        func_80284354(1, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].color[1]);
        func_80284354(2, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].color[2]);
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("COLOROFS=");
        func_80284354(0, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].color_offset[0]);
        func_80284354(1, column, selected);
        console_report_printf("%d", D_800B02CC[D_800B0044].color_offset[1]);
        func_80284354(2, column, selected);
        console_report_printf("%d\n", D_800B02CC[D_800B0044].color_offset[2]);
        row = func_8028439C(row, cursor, &selected);
        if (!D_800B02CC[D_800B0044].flags.bits.randrot) {
            console_report_printf("RANDROT = OFF\n");
        } else {
            console_report_printf("RANDROT = ON\n");
        }
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("SORT    = ");
        switch (D_800B02CC[D_800B0044].flags.bits.sort) {
        case 0:
            console_report_printf("TOP\n");
            break;
        case 1:
            console_report_printf("MID\n");
            break;
        case 2:
            console_report_printf("NORMAL\n");
            break;
        case 3:
            console_report_printf("BACK\n");
            break;
        }
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("COLMODE = ");
        switch (D_800B02CC[D_800B0044].flags.bits.colmode) {
        case 0:
            console_report_printf("1.0*Bk + 1.0*Fw\n");
            break;
        case 1:
            console_report_printf("1.0*Bk - 1.0*Fw\n");
            break;
        case 2:
            console_report_printf("1.0*Bk + 0.25*Fw\n");
            break;
        case 3:
            console_report_printf("0.5*Bk + 0.5*Fw\n");
            break;
        }
        row = func_8028439C(row, cursor, &selected);
        console_report_printf("ROTANGLE= %d\n", D_800B02CC[D_800B0044].rot_angle);
        func_8028439C(row, cursor, &selected);
        console_report_printf("RANGEMOD= ");
        switch (D_800B02CC[D_800B0044].flags.bits.rangemod) {
        case 0:
            console_report_printf("RANDUM (0)");
            break;
        case 2:
            console_report_printf("CIRCLE (1)");
            break;
        case 1:
            console_report_printf("LINE (2)");
            break;
        }
        console_report_printf("ROTANGLE= %d\n", D_800B02CC[D_800B0044].rot_angle);
    } else {
        row = 22;
        for (i = 0; i < 8; i++) {
            row = func_8028439C(row, cursor, &selected);
            console_report_printf("ANGOFFS%d=", i);
            func_80284354(0, column, selected);
            console_report_printf("%d", D_800B02CC[D_800B0044].angle_offsets[i][0]);
            func_80284354(1, column, selected);
            console_report_printf("%d\n", D_800B02CC[D_800B0044].angle_offsets[i][1]);
        }
    }
    console_set_color(0xFF, 0xFF, 0xFF);
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
        console_report_printf(">");
    } else {
        console_report_printf(" ");
    }
}

/* Start menu row `row`: highlight it and mark it with the cursor when selected;
 * returns the next row. */
s32 func_8028439C(s32 row, s32 cursor, s32 *selected) {
    if (cursor == row) {
        console_set_color(0, 0xFF, 0xFF);
        console_report_printf(">");
        *selected = 1;
    } else {
        console_set_color(0x40, 0x40, 0x40);
        console_report_printf(" ");
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
        if (mode_field_pointer_state[2] == 4) {
            D_800ADB98 = 1;
            D_800ADB94 += mode_field_pointer_state[4];
        } else if (mode_field_pointer_state[2] == 8) {
            zoom = D_800AF9FC;
            zoom += ((u32)mode_field_pointer_state[4] << 4) >> 5;
            D_800AF9FC = zoom;
            D_800AF984 = 1;
            D_800AF988 = 1;
        } else {
            D_800AF984 = 1;
            D_800AF988 = 1;
            D_800AF9FE += mode_field_pointer_state[4] << 4;
            D_800AF9F0 += mode_field_pointer_state[3] << 18;
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
        console_report_printf("main_se");
        break;
    case 1:
        console_report_printf("bat_se");
        break;
    case 2:
        console_report_printf("gear_se");
        break;
    case 3:
        console_report_printf("ambi");
        break;
    case 4:
        console_report_printf("ambi2");
        break;
    case 5:
        console_report_printf("ambi3");
        break;
    case 6:
        console_report_printf("ambi4");
        break;
    case 32:
        console_report_printf("minato");
        break;
    case 33:
        console_report_printf("lahan");
        break;
    case 34:
        console_report_printf("jyukai");
        break;
    case 35:
        console_report_printf("shitan");
        break;
    case 36:
        console_report_printf("musi");
        break;
    case 37:
        console_report_printf("church");
        break;
    case 38:
        console_report_printf("battle2");
        break;
    case 39:
        console_report_printf("chuchu");
        break;
    case 40:
        console_report_printf("over");
        break;
    case 41:
        console_report_printf("orgel");
        break;
    case 42:
        console_report_printf("battle3");
        break;
    case 43:
        console_report_printf("ajito");
        break;
    case 44:
        console_report_printf("emerada");
        break;
    case 45:
        console_report_printf("ellie");
        break;
    case 46:
        console_report_printf("world");
        break;
    case 47:
        console_report_printf("sad");
        break;
    case 48:
        console_report_printf("ave");
        break;
    case 49:
        console_report_printf("ellie2");
        break;
    case 50:
        console_report_printf("balto");
        break;
    case 51:
        console_report_printf("dajil");
        break;
    case 52:
        console_report_printf("maria1");
        break;
    case 53:
        console_report_printf("maria2");
        break;
    case 54:
        console_report_printf("heshu");
        break;
    case 55:
        console_report_printf("kaisou");
        break;
    case 56:
        console_report_printf("pinch");
        break;
    case 57:
        console_report_printf("porgan");
        break;
    case 58:
        console_report_printf("babel");
        break;
    case 59:
        console_report_printf("solachu");
        break;
    case 60:
        console_report_printf("shinnyu");
        break;
    case 61:
        console_report_printf("inbou");
        break;
    case 62:
        console_report_printf("ido");
        break;
    case 63:
        console_report_printf("takeoff");
        break;
    case 64:
        console_report_printf("glaerf");
        break;
    case 65:
        console_report_printf("last");
        break;
    case 66:
        console_report_printf("shebat");
        break;
    case 67:
        console_report_printf("dungeon");
        break;
    case 68:
        console_report_printf("lastbat");
        break;
    case 69:
        console_report_printf("solaris");
        break;
    case 181:
        console_report_printf("vomaria");
        break;
    case 182:
        console_report_printf("melmv");
        break;
    case 183:
        console_report_printf("yugumv");
        break;
    case 184:
        console_report_printf("zoharumv");
        break;
    case 185:
        console_report_printf("vomagic5");
        break;
    case 186:
        console_report_printf("vomagic4");
        break;
    case 187:
        console_report_printf("vomagic3");
        break;
    case 188:
        console_report_printf("voivent3");
        break;
    case 189:
        console_report_printf("voivent2");
        break;
    case 190:
        console_report_printf("vobossm");
        break;
    case 191:
        console_report_printf("vobossl");
        break;
    case 192:
        console_report_printf("vochu6");
        break;
    case 193:
        console_report_printf("vomagic2");
        break;
    case 194:
        console_report_printf("vomagic1");
        break;
    case 7:
        console_report_printf("movie14");
        break;
    case 195:
        console_report_printf("movie15");
        break;
    case 196:
        console_report_printf("movie16");
        break;
    case 197:
        console_report_printf("movie18");
        break;
    case 198:
        console_report_printf("voivent");
        break;
    case 199:
        console_report_printf("damage");
        break;
    case 200:
        console_report_printf("vofei");
        break;
    case 201:
        console_report_printf("vofei1");
        break;
    case 202:
        console_report_printf("vofei2");
        break;
    case 203:
        console_report_printf("vofei3");
        break;
    case 204:
        console_report_printf("vofei4");
        break;
    case 205:
        console_report_printf("vofei5");
        break;
    case 206:
        console_report_printf("vofei6");
        break;
    case 207:
        console_report_printf("voellie");
        break;
    case 208:
        console_report_printf("voellie1");
        break;
    case 209:
        console_report_printf("voellie2");
        break;
    case 210:
        console_report_printf("voellie3");
        break;
    case 211:
        console_report_printf("voellie4");
        break;
    case 212:
        console_report_printf("voellie5");
        break;
    case 213:
        console_report_printf("voellie6");
        break;
    case 214:
        console_report_printf("voellie7");
        break;
    case 215:
        console_report_printf("voellie8");
        break;
    case 216:
        console_report_printf("voshita");
        break;
    case 217:
        console_report_printf("voshita1");
        break;
    case 218:
        console_report_printf("voshita2");
        break;
    case 219:
        console_report_printf("voshita3");
        break;
    case 220:
        console_report_printf("voshita4");
        break;
    case 221:
        console_report_printf("voshita5");
        break;
    case 222:
        console_report_printf("voshita6");
        break;
    case 223:
        console_report_printf("vobaluto");
        break;
    case 224:
        console_report_printf("vobalu1");
        break;
    case 225:
        console_report_printf("vobalu2");
        break;
    case 226:
        console_report_printf("vobalu3");
        break;
    case 227:
        console_report_printf("vobalu4");
        break;
    case 228:
        console_report_printf("vobalu5");
        break;
    case 229:
        console_report_printf("vobalu6");
        break;
    case 230:
        console_report_printf("vobalu7");
        break;
    case 231:
        console_report_printf("vorico");
        break;
    case 232:
        console_report_printf("vorico1");
        break;
    case 233:
        console_report_printf("vorico2");
        break;
    case 234:
        console_report_printf("vorico3");
        break;
    case 235:
        console_report_printf("vorico4");
        break;
    case 236:
        console_report_printf("vorico5");
        break;
    case 237:
        console_report_printf("vobilly");
        break;
    case 238:
        console_report_printf("vobilly1");
        break;
    case 239:
        console_report_printf("vobilly2");
        break;
    case 240:
        console_report_printf("vobilly3");
        break;
    case 241:
        console_report_printf("vobilly4");
        break;
    case 242:
        console_report_printf("vobilly5");
        break;
    case 243:
        console_report_printf("voeme");
        break;
    case 244:
        console_report_printf("voeme1");
        break;
    case 245:
        console_report_printf("voeme2");
        break;
    case 246:
        console_report_printf("voeme3");
        break;
    case 247:
        console_report_printf("voeme4");
        break;
    case 248:
        console_report_printf("voeme5");
        break;
    case 249:
        console_report_printf("vochu");
        break;
    case 250:
        console_report_printf("vochu1");
        break;
    case 251:
        console_report_printf("vochu2");
        break;
    case 252:
        console_report_printf("vochu3");
        break;
    case 253:
        console_report_printf("vochu4");
        break;
    case 254:
        console_report_printf("vochu5");
        break;
    }
}
