/* The burst load modes the battle overlay dispatches (800b8098 modes 2
 * and 3: 801e91e8, 801e9594) and their helpers (rodata 801E4034-801E4048,
 * text 801E8964-801E95BC, data 801E9680-801E96B4, its pointer at 801E96BC).
 * A unit of its own after load_modes: its jump table follows
 * func_801E8588's at 4 mod 8 without the pad one unit would give it. Built
 * by the Cygnus CDK GCC 2.7.2 like load_modes (see ovl2615.mk). */
#include "transitions.h"

u8 D_801E9680 = 0;
SVECTOR D_801E9684[3] = {{-80, -80, 0}, {176, -80, 0}, {-80, 176, 0}};
SVECTOR D_801E969C[3] = {{80, -176, 0}, {80, 80, 0}, {-176, 80, 0}};
u32 *D_801E96BC;

/* Advance the two burst variants, keeping the twist variant's current speed
 * for the translation that follows. */
#define BURST_ROTATE_STEP(burst, frame_out)               \
    do {                                                  \
        (burst)->speed += 1;                              \
        (frame_out) = ++(burst)->frame;                   \
        (burst)->angle += 0xA0;                           \
    } while (0)

#define BURST_TWIST_STEP(burst, frame_out, speed_out)     \
    do {                                                  \
        (burst)->speed += 10;                             \
        (frame_out) = ++(burst)->frame;                   \
        (burst)->twist += 0x600;                          \
        (speed_out) = (burst)->speed;                     \
    } while (0)

/* Burst update: variant 1 turns faster and faster, rising and fading after
 * 67 frames; variant 0 twists and rises, fading after 25 frames. The empty
 * loops over the 2x14x20 grid are left from removed work. */
void func_801E8964(Task *node) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    BurstTask *burst = node->data;
    s32 frame;
    s32 growing;
    s32 speed;
    s32 i, j, k;

    if (D_801E9680 != 0) {
        BURST_ROTATE_STEP(burst, frame);
        burst->trans.vz -= 0x3C;
        growing = frame < 0x43;
        if (!growing) {
            burst->brightness -= 0x18;
        } else {
            burst->twist += 0x80;
        }
    } else {
        BURST_TWIST_STEP(burst, frame, speed);
        burst->trans.vz -= speed;
        growing = frame < 0x19;
        if (!growing) {
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

/* Burst drawing: each corner rises by the sine (variant 1: of the angle
 * plus its distance; otherwise the cosine of its distance) scaled by the
 * twist and lights up with it; projected with a 512 screen distance. */
void func_801E8A64(Task *node) {
    BurstTask *burst = node->data;
    BurstCell *cell;
    POLY_GT3 *prim;
    SVECTOR *corner;
    long ofx, ofy;
    s32 screen;
    long p, flag;
    s32 row, half, col, k;
    s32 wave, light, otz;
    s32 twist;

    ReadGeomOffset(&ofx, &ofy);
    screen = ReadGeomScreen();
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(0x200);
    {
        MATRIX m;

        gpu_build_rotation_matrix(&burst->rot, &m);
        TransMatrix(&m, &burst->trans);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
    }
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 14; row++) {
            for (col = 0; col != 20; col++) {
                cell = &burst->cells[half][row][col];
                corner = cell->corner;
                prim = &cell->prim[battle_area.buffer];
                for (k = 0; k != 3; k++) {
                    if (D_801E9680 != 0) {
                        twist = burst->twist;
                        wave = gpu_get_sin(burst->angle + cell->distance[k]);
                    } else {
                        twist = burst->twist;
                        wave = gpu_get_cos(cell->distance[k]);
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
    heap_free(block);
}

/* Unlink a task-registered burst and release it after the frame. */
void func_801E8D7C(Task *node) {
    task_unlink_draw_node(node + 1);
    task_unlink_main_node(node);
    sprite_queue_free_later((u32)node);
}

/* Allocate and set up the burst. */
BurstTask *func_801E8DB8(void) {
    BurstTask *task = heap_alloc(sizeof(BurstTask), 1);

    task->task.data = task;
    task->draw.data = task;
    return func_801E8DF0(task);
}

/* Set up the burst: the screen as two triangles per 16x16 cell over a
 * 320x224 grid (textured from the copy at 0x2c0,0x100), each corner's
 * distance from the centre (variant 1: twice it; otherwise 3/5 of it). The
 * corners sit around the triangles' centroids: (x + 5 - 160, v + 5 - 112) * 16
 * for the first half, (x + 11 - 160, v + 11 - 112) * 16 for the second. */
BurstTask *func_801E8DF0(BurstTask *burst) {
    SVECTOR *triangle;
    POLY_GT3 *prim;
    VECTOR square;
    s32 row, half, col, k;
    BurstCell *cell;
    s32 v, u, u_right, v_bottom, x, y;

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
            for (col = 0; col != 20; col++) {
                v = row * 16;
                x = col * 16;
                cell = &burst->cells[half][row][col];
                triangle = half == 0 ? D_801E9684 : D_801E969C;
                for (k = 0; k != 3; k++) {
                    copyVector(&cell->corner[k], &triangle[k]);
                    if (half == 0) {
                        cell->corner[k].vx += (s16)(x * 16 - 0x9B0);
                        cell->corner[k].vy += (s16)((v - 107) * 16);
                    } else {
                        cell->corner[k].vx += (s16)(x * 16 - 0x950);
                        y = v - 101; /* v + 11 - 112 */
                        cell->corner[k].vy += (s16)(y * 16);
                    }
                    copyVector(&square, &cell->corner[k]);
                    Square0(&square, &square);
                    if (D_801E9680 != 0) {
                        cell->distance[k] = SquareRoot0(square.vx + square.vy) * 2;
                    } else {
                        cell->distance[k] = SquareRoot0(square.vx + square.vy) * 3 / 5;
                    }
                }
                for (k = 0; k != 2; k++) {
                    prim = &cell->prim[k];
                    u = (col * 16) & 0x3F;
                    u_right = u + 16;
                    v_bottom = v + 16;
                    SetPolyGT3(prim);
                    SetShadeTex(prim, 0);
                    setRGB0(prim, 0x80, 0x80, 0x80);
                    setRGB1(prim, 0x80, 0x80, 0x80);
                    setRGB2(prim, 0x80, 0x80, 0x80);
                    prim->code |= 2;
                    prim->tpage = GetTPage(2, 0, col * 16 + 0x2C0, 0x100);
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

/* Load mode (burst): like the shatter mode, but the background fades before
 * the burst runs on the scratchpad stack. */
void func_801E91E8(void) {
    RECT rect;
    u16 *screen;
    u16 *pixel;
    BattleArea *work;
    FrameBuffer *first;
    FrameBuffer *next;
    BurstTask *burst;
    s32 frames;
    u32 state;
    s32 phase;
    s32 i;

    frames = 0x52;
    task_clear_lists();
    state = 1;
    phase = 0;
    screen = heap_alloc(0x30000, 1);
    pixel = screen;
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xE0;
    StoreImage(&rect, (u_long *)screen);
    DrawSync(0);
    for (i = 0; i != 0x14000; i++) {
        *pixel++ |= 0x8000;
    }
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 0x140;
    rect.h = 0xE0;
    LoadImage(&rect, (u_long *)screen);
    DrawSync(0);
    heap_free(screen);
    work = &battle_area;
    /* Flip as swap_buffers() does, remembering the first buffer. */
    next = &work->buffers[0];
    if (work->current == (first = next)) {
        next = &work->buffers[1];
    }
    work->current = next;
    work->ot = next->ot;
    ClearOTagR((u_long *)next->ot, 0x1000);
    work->buffer = 0;
    work->current = first;
    work->buffers[0].drawEnv.isbg = 1;
    work->buffers[1].drawEnv.isbg = 1;
    work->buffers[0].drawEnv.r0 = 0;
    work->buffers[1].drawEnv.r0 = 0;
    work->buffers[0].drawEnv.g0 = 0;
    work->buffers[1].drawEnv.g0 = 0;
    work->buffers[0].drawEnv.b0 = 0;
    work->buffers[1].drawEnv.b0 = 0;
    burst = func_801E8DB8();
    while (frames != 0 || state != 5) {
        if (frames > 0) {
            frames--;
        }
        swap_buffers();
        battle_area.buffer = 1 - battle_area.buffer;
        D_801E96BC = battle_area.ot;
        if (cd_get_pending_read_count() == 0) {
            switch (state) {
            case 0:
                break;
            case 2:
                state++;
                mode_load_current_battle_stage();
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
        boot_check_soft_reset();
        battle_area.buffers[battle_area.buffer].drawEnv.r0 =
            sprite_add_clamp_byte(battle_area.buffers[battle_area.buffer].drawEnv.r0, -12);
        battle_area.buffers[battle_area.buffer].drawEnv.g0 =
            sprite_add_clamp_byte(battle_area.buffers[battle_area.buffer].drawEnv.g0, -12);
        battle_area.buffers[battle_area.buffer].drawEnv.b0 =
            sprite_add_clamp_byte(battle_area.buffers[battle_area.buffer].drawEnv.b0, -12);
        /* Run the burst on a stack at the top of the scratchpad. */
        STACK_ENTER(0x1F8003FC);
        func_801E8964(&burst->task);
        func_801E8A64(&burst->task);
        STACK_LEAVE();
        DrawSync(0);
        VSync(2);
        PutDispEnv(&battle_area.current->dispEnv);
        PutDrawEnv(&battle_area.current->drawEnv);
        DrawOTag((u_long *)&battle_area.current->ot[0xFFF]);
    }
    func_801E8D48(burst);
    SetDispMask(0);
    cd_sync_reads(0);
    func_801E5840(3);
}

/* Load mode: the burst's variant 1. */
void func_801E9594(void) {
    D_801E9680 = 1;
    func_801E91E8();
}
