/* The burst load modes the battle overlay dispatches (800b8098 modes 2
 * and 3: 801e91e8, 801e9594) and their helpers. A unit of its own after
 * load_modes: its jump table follows func_801E8588's at 4 mod 8 without the
 * pad one unit would give it. Built by the Cygnus CDK GCC 2.7.2 like
 * load_modes (see ovl2615.mk). */
#include "battle_setup.h"

/* One burst frame: speed up, count the frame and turn one spin field. */
#define BURST_STEP(burst, frame, dspeed, spin, dspin) \
    do {                                              \
        (burst)->speed += (dspeed);                   \
        (frame) = ++(burst)->frame;                   \
        (burst)->spin += (dspin);                     \
    } while (0)

/* Burst update: variant 1 turns faster and faster, rising and fading after
 * 67 frames; variant 0 twists and rises, fading after 25 frames. The empty
 * loops over the 2x14x20 grid are left from removed work.
 * NON_MATCHING: 4 bytes long. The BURST_STEP block keeps sched1 from
 * hoisting the trans.vz load above the spin store, as in the original, but
 * its end barrier also ties the code after it: in the first branch the frame
 * test cannot fill the trans.vz load delay (a nop instead of the original's
 * slti), and in the second the speed reload stays after the trans.vz load
 * (the original loads it before the frame store). */
#ifdef NON_MATCHING
void func_801E8964(TaskNode *node) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    BurstTask *burst = node->object;
    s32 frame;
    s32 i, j, k;

    if (D_801E9680 != 0) {
        BURST_STEP(burst, frame, 1, angle, 0xA0);
        burst->trans.vz -= 0x3C;
        if (frame >= 0x43) {
            burst->brightness -= 0x18;
        } else {
            burst->twist += 0x80;
        }
    } else {
        BURST_STEP(burst, frame, 10, twist, 0x600);
        burst->trans.vz -= burst->speed;
        if (frame >= 0x19) {
            burst->brightness -= 0x14;
        }
    }
    for (k = 0; k != 2; k++) {
        for (j = 0; j != 14; j++) {
            for (i = 0; i != 20; i++) {
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/burst_modes", func_801E8964);
#endif

/* Burst drawing: each corner rises by the sine (variant 1: of the angle
 * plus its distance; otherwise the cosine of its distance) scaled by the
 * twist and lights up with it; projected with a 512 screen distance. */
void func_801E8A64(TaskNode *node) {
    BurstTask *burst = node->object;
    BurstCell *cell;
    POLY_GT3 *prim;
    SVECTOR *corner;
    s32 ofx, ofy, screen;
    s32 p, flag;
    s32 row, half, col, k;
    s32 wave, light, otz;
    s32 twist;

    ReadGeomOffset(&ofx, &ofy);
    screen = ReadGeomScreen();
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(0x200);
    {
        MATRIX m;

        func_8003F738(&burst->rot, &m);
        TransMatrix(&m, &burst->trans);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
    }
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 14; row++) {
            for (col = 0; col != 20; col++) {
                cell = &burst->cells[half][row][col];
                corner = cell->corner;
                prim = &cell->prim[D_800C3EB0.buffer];
                for (k = 0; k != 3; k++) {
                    if (D_801E9680 != 0) {
                        twist = burst->twist;
                        wave = func_8003F8B0(burst->angle + cell->distance[k]);
                    } else {
                        twist = burst->twist;
                        wave = func_8003F8CC(cell->distance[k]);
                    }
                    wave = wave * twist / 4096;
                    corner[k].vz = wave >> 2;
                    light = wave >> 7;
                    light += burst->brightness;
                    if (light < 0) {
                        light = 0;
                    }
                    if (light >= 0x100) {
                        light = 0xFF;
                    }
                    switch (k) {
                    case 0:
                        prim->r0 = prim->g0 = prim->b0 = light;
                        break;
                    case 1:
                        prim->r1 = prim->g1 = prim->b1 = light;
                        break;
                    case 2:
                        prim->r2 = prim->g2 = prim->b2 = light;
                        break;
                    }
                }
                otz = RotTransPers3(&corner[0], &corner[1], &corner[2], (s32 *)&prim->x0,
                                    (s32 *)&prim->x1, (s32 *)&prim->x2, &p, &flag);
                otz >>= 6;
                if (!(flag & 0x8000)) {
                    AddPrim(D_801E96BC + otz, prim);
                }
            }
        }
    }
    SetGeomOffset(ofx, ofy);
    SetGeomScreen(screen);
}

/* Release the burst task after the drawing finishes. */
void func_801E8D48(void *block) {
    DrawSync(0);
    func_800320E8(block);
}

/* Unlink a task-registered burst and release it after the frame. */
void func_801E8D7C(TaskNode *node) {
    func_8001CB48(node + 1);
    func_8001CD94(node);
    func_80025180(node);
}

/* Allocate and set up the burst. */
BurstTask *func_801E8DB8(void) {
    BurstTask *task = func_80031BDC(sizeof(BurstTask), 1);

    task->task.object = task;
    task->draw.object = task;
    return func_801E8DF0(task);
}

/* Set up the burst: the screen as two triangles per 16x16 cell over a
 * 320x224 grid (textured from the copy at 0x2c0,0x100), each corner's
 * distance from the centre (variant 1: twice it; otherwise 3/5 of it).
 * NON_MATCHING: as func_801E8320, the original works from spilled copies of
 * the strength-reduced row/column offsets and keeps 0x80 in s7; register
 * allocation and spills differ (1032 bytes here, 1016 in the original). */
#ifdef NON_MATCHING
BurstTask *func_801E8DF0(BurstTask *burst) {
    BurstCell *cell;
    SVECTOR *triangle;
    POLY_GT3 *prim;
    VECTOR square;
    s32 half, row, col, k;
    s32 v, u, u_right, v_bottom;

    if (D_801E9680 != 0) {
        burst->frame = 0;
        burst->twist = 0x400;
        burst->brightness = 0x80;
        burst->speed = 0x40;
        burst->angle = 0x700;
    } else {
        burst->brightness = 0x80;
        burst->frame = 0;
        burst->twist = 0;
        burst->speed = 0x40;
        burst->angle = 0;
    }
    SquareRoot0(0x9500);
    burst->trans.vx = 0;
    burst->trans.vy = 0;
    burst->trans.vz = 0x2000;
    burst->rot.vx = 0;
    burst->rot.vy = 0;
    burst->rot.vz = 0;
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 14; row++) {
            v = row * 16;
            for (col = 0; col != 20; col++) {
                cell = &burst->cells[half][row][col];
                triangle = half == 0 ? D_801E9684 : D_801E969C;
                for (k = 0; k != 3; k++) {
                    cell->corner[k].vx = triangle[k].vx;
                    cell->corner[k].vy = triangle[k].vy;
                    cell->corner[k].vz = triangle[k].vz;
                    if (half == 0) {
                        cell->corner[k].vx += (s16)(col * 0x100 - 0x9B0);
                        cell->corner[k].vy += (s16)(row * 0x100 - 0x6B0);
                    } else {
                        cell->corner[k].vy += (s16)((v - 101) * 16);
                        cell->corner[k].vx += (s16)(col * 0x100 - 0x950);
                    }
                    square.vx = cell->corner[k].vx;
                    square.vy = cell->corner[k].vy;
                    square.vz = cell->corner[k].vz;
                    func_8004A414(&square, &square);
                    if (D_801E9680 != 0) {
                        cell->distance[k] = SquareRoot0(square.vx + square.vy) * 2;
                    } else {
                        cell->distance[k] = SquareRoot0(square.vx + square.vy) * 3 / 5;
                    }
                }
                u = (col * 16) & 0x3F;
                u_right = u + 16;
                v_bottom = v + 16;
                for (k = 0; k != 2; k++) {
                    prim = &cell->prim[k];
                    SetPolyGT3(prim);
                    SetShadeTex(prim, 0);
                    prim->r0 = prim->g0 = prim->b0 = 0x80;
                    prim->r1 = prim->g1 = prim->b1 = 0x80;
                    prim->r2 = prim->g2 = prim->b2 = 0x80;
                    prim->code |= 2;
                    prim->tpage = GetTPage(2, 0, col * 16 + 0x2C0, 0x100);
                    if (half == 0) {
                        prim->u0 = u;
                        prim->v0 = v;
                        prim->u1 = u_right;
                        prim->v1 = v;
                    } else {
                        prim->u0 = u_right;
                        prim->v0 = v;
                        prim->u1 = u_right;
                        prim->v1 = v_bottom;
                    }
                    prim->u2 = u;
                    prim->v2 = v_bottom;
                }
            }
        }
    }
    return burst;
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/burst_modes", func_801E8DF0);
#endif

/* Load mode (burst): like the shatter mode, but the background fades before
 * the burst runs on the scratchpad stack. */
void func_801E91E8(void) {
    RECT rect;
    u16 *screen;
    u16 *pixel;
    BattleWork *work;
    DrawBuffer *first;
    DrawBuffer *next;
    BurstTask *burst;
    s32 frames;
    u32 state;
    s32 phase;
    s32 i;

    frames = 0x52;
    func_8001C944();
    state = 1;
    phase = 0;
    screen = func_80031BDC(0x30000, 1);
    pixel = screen;
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xE0;
    StoreImage(&rect, screen);
    DrawSync(0);
    for (i = 0; i != 0x14000; i++) {
        *pixel++ |= 0x8000;
    }
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 0x140;
    rect.h = 0xE0;
    LoadImage(&rect, screen);
    DrawSync(0);
    func_800320E8(screen);
    work = &D_800C3EB0;
    /* Flip as swap_buffers() does, remembering the first buffer. */
    next = &work->buffers[0];
    if (work->current == (first = next)) {
        next = &work->buffers[1];
    }
    work->current = next;
    work->ot = next->ot;
    ClearOTagR(next->ot, 0x1000);
    work->buffer = 0;
    work->current = first;
    work->buffers[0].draw.isbg = 1;
    work->buffers[1].draw.isbg = 1;
    work->buffers[0].draw.r0 = 0;
    work->buffers[1].draw.r0 = 0;
    work->buffers[0].draw.g0 = 0;
    work->buffers[1].draw.g0 = 0;
    work->buffers[0].draw.b0 = 0;
    work->buffers[1].draw.b0 = 0;
    burst = func_801E8DB8();
    while (frames != 0 || state != 5) {
        if (frames > 0) {
            frames--;
        }
        swap_buffers();
        D_800C3EB0.buffer = 1 - D_800C3EB0.buffer;
        D_801E96BC = D_800C3EB0.ot;
        if (func_800286CC() == 0) {
            switch (state) {
            case 0:
                break;
            case 2:
                state++;
                func_8001BB0C();
                break;
            case 1:
            case 3:
            case 4:
                func_801E5840(phase);
                phase++;
                state++;
                break;
            }
        }
        func_80019CA0();
        D_800C3EB0.buffers[D_800C3EB0.buffer].draw.r0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].draw.r0, -12);
        D_800C3EB0.buffers[D_800C3EB0.buffer].draw.g0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].draw.g0, -12);
        D_800C3EB0.buffers[D_800C3EB0.buffer].draw.b0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].draw.b0, -12);
        /* Run the burst on a stack at the top of the scratchpad. */
        __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                         :
                         : "r"(0x1F8003FC)
                         : "$8", "memory");
        func_801E8964(&burst->task);
        func_801E8A64(&burst->task);
        __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
        DrawSync(0);
        VSync(2);
        PutDispEnv(&D_800C3EB0.current->disp);
        PutDrawEnv(&D_800C3EB0.current->draw);
        DrawOTag(&D_800C3EB0.current->ot[0xFFF]);
    }
    func_801E8D48(burst);
    SetDispMask(0);
    func_80028A60(0);
    func_801E5840(3);
}

/* Load mode: the burst's variant 1. */
void func_801E9594(void) {
    D_801E9680 = 1;
    func_801E91E8();
}
