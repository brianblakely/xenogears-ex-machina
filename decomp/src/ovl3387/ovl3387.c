/* ovl3387: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file (D_800591B3
 * + 2) of the battle directory, D_800591B3 being the platform bits (6..11) of
 * the directory header of battle file 2, whenever they differ from the loaded
 * module's (D_800591B2). Battle script opcodes call fixed entry addresses in
 * the loaded module: this one provides 801fc898, called by the opcode handler
 * 800b3f04, which plays a full-screen effect in its own frame loop on a
 * private stack.
 *
 * The module is one unit, its whole file (801fc000-801fce4c), built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (see ovl3387.mk). */
#include "common.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/sprite.h"
#include "battle/area.h"
#include "battle/sprite.h"
#include "burst.h"

u8 D_801FCE14 = 1;
SVECTOR D_801FCE18[3] = {{-80, -80, 0}, {176, -80, 0}, {-80, 176, 0}};
SVECTOR D_801FCE30[3] = {{80, -176, 0}, {80, 80, 0}, {-176, 80, 0}};
u32 *D_801FCE48 = NULL;

/* Advance the effect one frame (two variants), fading it out after 100 or 24
 * frames. The empty loops over a 2x14x20 grid are left from removed work. */
void func_801FC000(Task *node) {
    BurstTask *burst = node->data;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    s32 i, j, k;

    if (D_801FCE14 != 0) {
        burst->speed++;
        burst->angle += 0x80;
        burst->angle += burst->speed >> 2;
        burst->twist += 0x40;
        burst->frame++;
        burst->trans.vz -= 0x1E;
        if (burst->frame > 100) {
            burst->brightness -= 4;
        }
    } else {
        burst->speed += 10;
        burst->twist += 0x600;
        burst->frame++;
        burst->rot.vz += 10;
        burst->trans.vz -= burst->speed;
        if (burst->frame > 24) {
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

/* Draw the captured screen's cells: each corner rises by the sine (variant
 * 1: of the angle plus its distance; otherwise the cosine of its distance)
 * scaled by the twist, and lights up with it; projected with a 512 screen
 * distance about the screen centre. */
void func_801FC11C(Task *node) {
    BurstTask *burst = node->data;
    long ofs[2];
    MATRIX m;
    BurstCell *cell;
    SVECTOR *corner;
    POLY_GT3 *prim;
    s32 twist;
    s32 screen;
    long p, flag;
    s32 wave, light, otz;
    s32 row, half, col, k;

    ReadGeomOffset(&ofs[0], &ofs[1]);
    screen = ReadGeomScreen();
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(0x200);
    func_8003F738(&burst->rot, &m);
    TransMatrix(&m, &burst->trans);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 14; row++) {
            for (col = 0; col != 20; col++) {
                cell = &burst->cells[half][row][col];
                corner = cell->corner;
                prim = &cell->prim[D_800C3EB0.buffer];
                for (k = 0; k != 3; k++) {
                    if (D_801FCE14 != 0) {
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
                otz = RotTransPers3(&corner[0], &corner[1], &corner[2], (long *)&prim->x0,
                                    (long *)&prim->x1, (long *)&prim->x2, &p, &flag);
                otz >>= 6;
                if (!(flag & 0x8000)) {
                    AddPrim(D_801FCE48 + otz, prim);
                }
            }
        }
    }
    SetGeomOffset(ofs[0], ofs[1]);
    SetGeomScreen(screen);
}

/* Wait for drawing to finish and release the effect. */
void func_801FC400(BurstTask *burst) {
    DrawSync(0);
    func_800320E8(burst);
}

/* Unlink a task-registered effect and release it after the frame. */
void func_801FC434(Task *node) {
    func_8001CB48(node + 1);
    func_8001CD94(node);
    func_80025180((u32)node);
}

/* Allocate and set up the effect's state. */
BurstTask *func_801FC470(void) {
    BurstTask *burst = func_80031BDC(sizeof(BurstTask), 1);

    burst->task.data = burst;
    burst->draw.data = burst;
    return func_801FC4A8(burst);
}

/* Set up the effect: the screen as two triangles per 16x16 cell over a
 * 320x224 grid (textured from the copy at 0x2c0,0x100), each corner's
 * distance from the centre (variant 1: twice it; otherwise 3/5 of it). */
BurstTask *func_801FC4A8(BurstTask *burst) {
    s32 row, half;
    BurstCell *cell;
    SVECTOR *triangle;
    POLY_GT3 *prim;
    VECTOR square;
    s32 col, k;
    s32 v, u, u_right;
    s32 second_y;
    s32 v_bottom;

    if (D_801FCE14 != 0) {
        burst->frame = 0;
        burst->brightness = 0x80;
        burst->twist = 0;
        burst->speed = 0x40;
    } else {
        burst->brightness = 0x80;
        burst->frame = 0;
        burst->twist = 0;
        burst->speed = 0x40;
    }
    burst->angle = 0;
    SquareRoot0(0x9500);
    burst->trans.vx = 0;
    burst->trans.vy = 0;
    burst->trans.vz = 0x2000;
    burst->rot.vx = 0;
    burst->rot.vy = 0;
    burst->rot.vz = 0;
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 14; row++) {
            for (col = 0; col != 20; col++) {
                v = row * 16;
                cell = &burst->cells[half][row][col];
                triangle = half == 0 ? D_801FCE18 : D_801FCE30;
                for (k = 0; k != 3; k++) {
                    copyVector(&cell->corner[k], &triangle[k]);
                    second_y = v - 101;
                    if (half == 0) {
                        cell->corner[k].vx += (col * 16 - 155) * 16;
                        cell->corner[k].vy += (row * 16 - 107) * 16;
                    } else {
                        cell->corner[k].vx += (col * 16 - 149) * 16;
                        cell->corner[k].vy += second_y * 16;
                    }
                    copyVector(&square, &cell->corner[k]);
                    Square0(&square, &square);
                    if (D_801FCE14 != 0) {
                        cell->distance[k] = SquareRoot0(square.vx + square.vy) * 2;
                    } else {
                        cell->distance[k] = SquareRoot0(square.vx + square.vy) * 3 / 5;
                    }
                }
                for (k = 0; k != 2; k++) {
                    prim = &cell->prim[k];
                    SetPolyGT3(prim);
                    SetShadeTex(prim, 0);
                    prim->r0 = 0x80;
                    prim->g0 = 0x80;
                    prim->b0 = 0x80;
                    prim->r1 = 0x80;
                    prim->g1 = 0x80;
                    prim->b1 = 0x80;
                    prim->r2 = 0x80;
                    prim->g2 = 0x80;
                    prim->b2 = 0x80;
                    prim->code |= 2;
                    prim->tpage = GetTPage(2, 0, col * 16 + 0x2C0, 0x100);
                    u = (col * 16) & 0x3F;
                    u_right = u + 16;
                    v_bottom = v + 16;
                    if (half == 0) {
                        prim->u0 = u;
                        prim->v0 = v;
                        prim->u1 = u_right;
                        prim->v1 = v;
                        prim->u2 = u;
                        prim->v2 = v_bottom;
                    } else {
                        prim->u0 = u_right;
                        prim->v0 = v;
                        prim->u1 = u_right;
                        prim->v1 = v_bottom;
                        prim->u2 = u;
                        prim->v2 = v_bottom;
                    }
                }
            }
        }
    }
    return burst;
}

/* Opcode entry: run the effect on a private 8 KB stack (its frame loop needs
 * more than the battle's). */
void func_801FC898(void) {
    u8 *stack = func_80031BDC(0x2000, 0);

    /* Push the caller's sp at the new stack top and switch to it. */
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(stack + 0x1F00)
                     : "$8", "memory");
    func_801FC8F4();
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    func_800320E8(stack);
}

/* The effect's own frame loop (164 frames): keep the battle's texture pages,
 * copy the screen (made semi-transparent) to 0x2c0,0x100, black out the
 * background, run and draw the effect in both display buffers while the
 * background colour fades, then restore the pages, clear the screen and the
 * background colour. */
void func_801FC8F4(void) {
    /* What the effect overwrites and restores afterwards: the background
     * colour and the two texture pages. */
    struct {
        u8 colour[3];
        s32 unused[2]; /* unused in the original; reserves 8 bytes */
        u_long *pages[2];
    } saved;
    RECT rect;
    u16 *screen;
    u16 *p;
    FrameBuffer *next;
    POLY_F4 *prim;
    BurstTask *burst;
    BattleArea *work;
    FrameBuffer *buffers;
    FrameBuffer *shown;
    FrameBuffer *back;
    u8 isbg;
    s32 frames = 0xA4;
    s32 i;

    saved.pages[0] = func_80031BDC(0x8000, 1);
    saved.pages[1] = func_80031BDC(0x8000, 1);
    rect.x = D_800C3668[1].x;
    rect.y = D_800C3668[1].y;
    rect.w = 0x40;
    rect.h = 0x100;
    StoreImage(&rect, saved.pages[0]);
    rect.x = D_800C3668[2].x;
    rect.y = D_800C3668[2].y;
    rect.w = 0x40;
    rect.h = 0x100;
    StoreImage(&rect, saved.pages[1]);
    DrawSync(0);
    screen = func_80031BDC(0x30000, 1);
    p = screen;
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xE0;
    StoreImage(&rect, (u_long *)screen);
    DrawSync(0);
    for (i = 0; i != 0x14000; i++) {
        *p++ |= 0x8000;
    }
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 0x140;
    rect.h = 0xE0;
    LoadImage(&rect, (u_long *)screen);
    DrawSync(0);
    func_800320E8(screen);
    work = &D_800C3EB0;
    back = &work->buffers[0];
    shown = work->current;
    buffers = back;
    if (shown == buffers) {
        back = &buffers[1];
    }
    work->current = back;
    work->ot = back->ot;
    ClearOTagR((u_long *)back->ot, 0x1000);
    saved.colour[0] = work->buffers[1].drawEnv.r0;
    saved.colour[1] = work->buffers[1].drawEnv.g0;
    saved.colour[2] = work->buffers[1].drawEnv.b0;
    isbg = work->buffers[0].drawEnv.isbg;
    work->buffer = 0;
    work->current = &buffers[0];
    work->buffers[0].drawEnv.isbg = 0;
    work->buffers[1].drawEnv.isbg = 0;
    work->buffers[0].drawEnv.r0 = 0;
    work->buffers[1].drawEnv.r0 = 0;
    work->buffers[0].drawEnv.g0 = 0;
    work->buffers[1].drawEnv.g0 = 0;
    work->buffers[0].drawEnv.b0 = 0;
    work->buffers[1].drawEnv.b0 = 0;
    burst = func_801FC470();
    while (frames != 0) {
        if (frames > 0) {
            frames--;
        }
        D_800C3EB0.buffers[D_800C3EB0.buffer].drawEnv.r0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].drawEnv.r0, -12);
        D_800C3EB0.buffers[D_800C3EB0.buffer].drawEnv.g0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].drawEnv.g0, -12);
        D_800C3EB0.buffers[D_800C3EB0.buffer].drawEnv.b0 =
            func_80021AD8(D_800C3EB0.buffers[D_800C3EB0.buffer].drawEnv.b0, -12);
        next = &D_800C3EB0.buffers[0];
        if (D_800C3EB0.current == next) {
            next = &D_800C3EB0.buffers[1];
        }
        D_800C3EB0.current = next;
        D_800C3EB0.ot = next->ot;
        ClearOTagR((u_long *)next->ot, 0x1000);
        D_800C3EB0.buffer = 1 - D_800C3EB0.buffer;
        D_801FCE48 = D_800C3EB0.ot;
        DrawSync(0);
        VSync(2);
        func_801FC000(&burst->task);
        func_801FC11C(&burst->task);
        PutDispEnv(&D_800C3EB0.current->dispEnv);
        PutDrawEnv(&D_800C3EB0.current->drawEnv);
        DrawOTag((u_long *)&D_800C3EB0.current->ot[0xFFF]);
    }
    DrawSync(0);
    VSync(2);
    ClearOTagR((u_long *)D_800C3EB0.ot, 0x1000);
    DrawSync(0);
    VSync(2);
    rect.x = D_800C3668[1].x;
    rect.y = D_800C3668[1].y;
    rect.w = 0x40;
    rect.h = 0x100;
    LoadImage(&rect, saved.pages[0]);
    rect.x = D_800C3668[2].x;
    rect.y = D_800C3668[2].y;
    rect.w = 0x40;
    rect.h = 0x100;
    LoadImage(&rect, saved.pages[1]);
    func_800320E8(saved.pages[0]);
    func_800320E8(saved.pages[1]);
    DrawSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x1C0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    VSync(2);
    prim = (POLY_F4 *)D_80059580;
    D_80059580 = (SpriteQueueEntry *)((u8 *)D_80059580 + sizeof(POLY_F4));
    SetPolyF4(prim);
    prim->r0 = 0;
    prim->g0 = 0;
    prim->b0 = 0;
    prim->x0 = 0;
    prim->y0 = 0;
    prim->x1 = 0x140;
    prim->y1 = 0;
    prim->x2 = 0;
    prim->y2 = 0xF0;
    prim->x3 = 0x140;
    prim->y3 = 0xF0;
    AddPrim(&D_800C3EB0.ot[0xFFE], prim);
    D_800C3EB0.buffers[1].drawEnv.isbg = isbg;
    D_800C3EB0.buffers[0].drawEnv.isbg = isbg;
    D_800C3EB0.buffers[1].drawEnv.r0 = saved.colour[0];
    D_800C3EB0.buffers[0].drawEnv.r0 = saved.colour[0];
    D_800C3EB0.buffers[1].drawEnv.g0 = saved.colour[1];
    D_800C3EB0.buffers[0].drawEnv.g0 = saved.colour[1];
    D_800C3EB0.buffers[1].drawEnv.b0 = saved.colour[2];
    D_800C3EB0.buffers[0].drawEnv.b0 = saved.colour[2];
    func_801FC400(burst);
}
