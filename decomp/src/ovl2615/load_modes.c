/* The shatter load modes the battle overlay dispatches (800b8098 modes 1
 * and 4: 801e8588, 801e893c) and their helpers: a screen transition run in
 * its own frame loop while the battle setup phases load. A separate unit
 * built by the Cygnus CDK GCC 2.7.2 (see ovl2615.mk); the burst modes
 * follow in burst_modes.c. */
#include "battle_setup.h"

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
 * away at its place on the grid.
 * NON_MATCHING: the original frame is 0xb0 (0x10-0x47 never accessed) and
 * its column loop reads the row's two heights and col * 0x20 from copies of
 * the strength-reduced row/column values (spilled, like the caller-saved
 * t0-t5 around each call); here those values are used directly, so the
 * frame is 0x70 and the spills differ. */
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
