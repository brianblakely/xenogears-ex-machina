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

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284354);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_8028439C);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284424);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802844BC);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284510);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_8028456C);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802845C0);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_8028461C);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284670);

INCLUDE_RODATA(".local/decomp/debug595/asm/nonmatchings/debug595", D_80280948);

INCLUDE_RODATA(".local/decomp/debug595/asm/nonmatchings/debug595", D_8028094C);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802846CC);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284EA4);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_80284FB4);

INCLUDE_ASM(".local/decomp/debug595/asm/nonmatchings/debug595", func_802851B0);
