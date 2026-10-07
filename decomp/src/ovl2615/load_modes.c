/* The battle load modes the battle overlay dispatches (800b8098 modes
 * 1-4: 801e8588, 801e91e8, 801e9594, 801e893c) and their helpers: screen
 * transitions run in their own frame loop while the battle setup phases
 * load. A separate unit built by the Cygnus CDK GCC 2.7.2
 * (see ovl2615.mk). */
#include "battle_setup.h"

/* Flip to the other display buffer and clear its ordering table. */
static inline void swap_buffers(void) {
    BattleWork *work = &D_800C3EB0;
    DrawBuffer *next = &work->buffers[0];

    if (work->current == next) {
        next = &work->buffers[1];
    }
    work->current = next;
    work->ot = next->ot;
    ClearOTagR(next->ot, 0x1000);
}

/* Shatter update: fade every cell and (variant 0) push it away. */
void func_801E7F4C(TaskNode *node) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    ShatterTask *task = node->object;
    ShatterCell *cell;
    POLY_FT3 *prim;
    s32 half, row, col;

    task->frame++;
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 7; row++) {
            for (col = 0; col != 10; col++) {
                cell = &task->cells[half][row][col];
                prim = &cell->prim[D_800C3EB0.buffer];
                prim->r0 = func_80021AD8(prim->r0, -3);
                prim->g0 = func_80021AD8(prim->g0, -3);
                prim->b0 = func_80021AD8(prim->b0, -3);
                if (D_801E963C == 0) {
                    cell->trans.vz -= 0x40;
                }
            }
        }
    }
}

/* Shatter drawing callback: into the current ordering table. */
void func_801E8088(TaskNode *node) {
    D_801E96B8 = D_8005956C;
    func_801E80B4(node);
}

/* Shatter drawing: each cell still in front (z >= 0x40) as its triangle,
 * rotated and moved by the cell, projected with a 512 screen distance
 * about the screen centre. */
void func_801E80B4(TaskNode *node) {
    ShatterTask *task = node->object;
    ShatterCell *cell;
    POLY_FT3 *prim;
    s32 ofx, ofy;
    s32 screen;
    s32 half, row, col;

    ReadGeomOffset(&ofx, &ofy);
    screen = ReadGeomScreen();
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(0x200);
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 7; row++) {
            for (col = 0; col != 10; col++) {
                cell = &task->cells[half][row][col];
                prim = &cell->prim[D_800C3EB0.buffer];
                if (cell->trans.vz >= 0x40) {
                    SVECTOR *triangle;
                    MATRIX m;
                    s32 p, flag;
                    s32 otz;

                    func_8003F738(&cell->rot, &m);
                    TransMatrix(&m, &cell->trans);
                    SetRotMatrix(&m);
                    SetTransMatrix(&m);
                    triangle = half == 0 ? D_801E9640 : D_801E9658;
                    otz = RotTransPers3(&triangle[0], &triangle[1], &triangle[2],
                                        (s32 *)&prim->x0, (s32 *)&prim->x1, (s32 *)&prim->x2,
                                        &p, &flag) >> 6;
                    AddPrim(D_801E96B8 + otz, prim);
                }
            }
        }
    }
    SetGeomOffset(ofx, ofy);
    SetGeomScreen(screen);
}

/* Release the shatter task after the drawing finishes. */
void func_801E827C(void *block) {
    DrawSync(0);
    func_800320E8(block);
}

/* Unlink a task-registered shatter and release it after the frame. */
void func_801E82B0(TaskNode *node) {
    func_8001CB48(node + 1);
    func_8001CD94(node);
    func_80025180(node);
}

/* Allocate and set up the shatter. */
ShatterTask *func_801E82EC(void) {
    ShatterTask *task = func_80031BDC(sizeof(ShatterTask), 1);

    task->task.object = task;
    task->draw.object = task;
    return func_801E8320(task);
}

/* Set up the shatter: the screen as two triangles per 32x32 cell over a
 * 320x224 grid (textured from the copy at 0x2c0,0x100), each cell 0x2000
 * away at its place on the grid. */
#ifdef NON_MATCHING
ShatterTask *func_801E8320(ShatterTask *task) {
    ShatterCell *cell;
    POLY_FT3 *prim;
    s32 half, row, col, k;
    u8 u, v, u_right, v_bottom;

    task->frame = 0;
    SquareRoot0(0x9500);
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 7; row++) {
            v = row << 5;
            for (col = 0; col != 10; col++) {
                cell = &task->cells[half][row][col];
                cell->rot.vx = 0;
                cell->rot.vy = 0;
                cell->rot.vz = 0;
                if (half == 0) {
                    cell->trans.vx = (col - 5) * 0x200 + 0xA0;
                    cell->trans.vz = 0x2000;
                    cell->trans.vy = (row - 3) * 0x200 - 0x60;
                } else {
                    cell->trans.vx = (col - 5) * 0x200 + 0x160;
                    cell->trans.vz = 0x2000;
                    cell->trans.vy = (row - 3) * 0x200 + 0x60;
                }
                u = (col * 0x20) & 0x3F;
                u_right = u + 0x20;
                v_bottom = v + 0x20;
                for (k = 0; k != 2; k++) {
                    prim = &cell->prim[k];
                    SetPolyFT3(prim);
                    SetShadeTex(prim, 0);
                    prim->r0 = 0x80;
                    prim->g0 = 0x80;
                    prim->b0 = 0x80;
                    prim->code |= 2;
                    prim->tpage = GetTPage(2, 1, col * 0x20 + 0x2C0, 0x100);
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
    return task;
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/load_modes", func_801E8320);
#endif

/* Load mode (shatter): copy the screen (made semi-transparent) to
 * 0x2c0,0x100, then for at least 82 frames and until the four setup phases
 * are done (one per idle disc frame, 8001bb0c between the first two), fade
 * the background and run the shatter on the scratchpad stack. */
void func_801E8588(void) {
    RECT rect;
    u16 *screen;
    u16 *pixel;
    BattleWork *work;
    DrawBuffer *first;
    DrawBuffer *next;
    ShatterTask *shatter;
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
    shatter = func_801E82EC();
    while (frames != 0 || state != 5) {
        if (frames > 0) {
            frames--;
        }
        swap_buffers();
        D_800C3EB0.buffer = 1 - D_800C3EB0.buffer;
        D_801E96B8 = D_800C3EB0.ot;
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
        /* Run the shatter on a stack at the top of the scratchpad. */
        __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                         :
                         : "r"(0x1F8003FC)
                         : "$8", "memory");
        func_801E7F4C(&shatter->task);
        func_801E80B4(&shatter->task);
        __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
        DrawSync(0);
        VSync(2);
        D_800C3EB0.buffers[D_800C3EB0.buffer].draw.r0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].draw.r0, -12);
        D_800C3EB0.buffers[D_800C3EB0.buffer].draw.g0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].draw.g0, -12);
        D_800C3EB0.buffers[D_800C3EB0.buffer].draw.b0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].draw.b0, -12);
        PutDispEnv(&D_800C3EB0.current->disp);
        PutDrawEnv(&D_800C3EB0.current->draw);
        DrawOTag(&D_800C3EB0.current->ot[0xFFF]);
    }
    func_801E827C(shatter);
    SetDispMask(0);
    func_80028A60(0);
    func_801E5840(3);
}

/* Load mode: the shatter's variant 1 (cells fade in place). */
void func_801E893C(void) {
    D_801E963C = 1;
    func_801E8588();
}

/* Burst update: variant 1 turns faster and faster, rising and fading after
 * 67 frames; variant 0 twists and rises, fading after 25 frames. The empty
 * loops over the 2x14x20 grid are left from removed work.
 * NON_MATCHING: frame and size now match; the original keeps the task in a1
 * and loads each field after the previous store (here the loads are hoisted
 * and the frame counter takes a0). */
#ifdef NON_MATCHING
void func_801E8964(TaskNode *node) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    BurstTask *burst = node->object;
    s32 frame;
    s32 i, j, k;

    if (D_801E9680 != 0) {
        burst->speed++;
        frame = ++burst->frame;
        burst->angle += 0xA0;
        burst->trans.vz -= 0x3C;
        if (frame >= 0x43) {
            burst->brightness -= 0x18;
        } else {
            burst->twist += 0x80;
        }
    } else {
        burst->speed += 10;
        frame = ++burst->frame;
        burst->twist += 0x600;
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
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/load_modes", func_801E8964);
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
 * distance from the centre (variant 1: twice it; otherwise 3/5 of it). */
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
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/load_modes", func_801E8DF0);
#endif

/* Load mode (burst): like the shatter mode, but the background fades before
 * the burst runs on the scratchpad stack.
 * NON_MATCHING: the instructions match; its jump table follows
 * func_801E8588's at 4 mod 8 without the alignment pad one unit would get,
 * so the original unit boundary lies between the two functions. */
#ifdef NON_MATCHING
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
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/load_modes", func_801E91E8);
#endif

/* Load mode: the burst's variant 1. */
void func_801E9594(void) {
    D_801E9680 = 1;
    func_801E91E8();
}
