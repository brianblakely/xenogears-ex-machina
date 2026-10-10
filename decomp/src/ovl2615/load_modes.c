/* The shatter load modes the battle overlay dispatches (800b8098 modes 1
 * and 4: 801e8588, 801e893c) and their helpers: a screen transition run in
 * its own frame loop while the battle setup phases load (rodata
 * 801E4020-801E4034, text 801E7F4C-801E8964, data 801E963C-801E9680, its
 * pointer at 801E96B8). A separate unit built by the Cygnus CDK GCC 2.7.2
 * after the GCC 2.6.3 stage.c (see ovl2615.mk); the burst modes follow in
 * burst_modes.c. */
#include "transitions.h"

u8 battle_setup_shatter_in_place = 0; /* 801E963C */
SVECTOR battle_setup_shatter_upper_left_triangle[3] = {{-160, -160, 0}, {352, -160, 0}, {-160, 352, 0}}; /* 801E9640 */
SVECTOR battle_setup_shatter_lower_right_triangle[3] = {{160, -352, 0}, {160, 160, 0}, {-352, 160, 0}}; /* 801E9658 */
/* A shard's launch velocity (the battle overlay's 800c35c4); this module
 * never reads it. */
VECTOR battle_setup_unused_shatter_launch_velocity = {0, 0, -1536 << 16}; /* 801E9670 */
u32 *battle_setup_shatter_current_ot; /* 801E96B8 */

/* 801E7F4C: Shatter update: fade every cell and (variant 0) push it away. */
void battle_setup_shatter_update(Task *node) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    ShatterTask *task = node->data;
    ShatterCell *cell;
    POLY_FT3 *prim;
    s32 half, row, col;

    task->frame++;
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 7; row++) {
            for (col = 0; col != 10; col++) {
                cell = &task->cells[half][row][col];
                prim = &cell->prim[battle_area.buffer];
                prim->r0 = sprite_add_clamp_byte(prim->r0, -3);
                prim->g0 = sprite_add_clamp_byte(prim->g0, -3);
                prim->b0 = sprite_add_clamp_byte(prim->b0, -3);
                if (battle_setup_shatter_in_place == 0) {
                    cell->trans.vz -= 0x40;
                }
            }
        }
    }
}

/* 801E8088: Shatter drawing callback: into the current ordering table. */
void battle_setup_shatter_draw_task(Task *node) {
    battle_setup_shatter_current_ot = (u32 *)sprite_ot;
    battle_setup_shatter_draw(node);
}

/* 801E80B4: Shatter drawing: each cell still in front (z >= 0x40) as its triangle,
 * rotated and moved by the cell, projected with a 512 screen distance
 * about the screen centre. */
void battle_setup_shatter_draw(Task *node) {
    ShatterTask *task = node->data;
    ShatterCell *cell;
    POLY_FT3 *prim;
    long ofx, ofy;
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
                prim = &cell->prim[battle_area.buffer];
                if (cell->trans.vz >= 0x40) {
                    SVECTOR *triangle;
                    MATRIX m;
                    long p, flag;
                    s32 otz;

                    gpu_build_rotation_matrix(&cell->rot, &m);
                    TransMatrix(&m, &cell->trans);
                    SetRotMatrix(&m);
                    SetTransMatrix(&m);
                    triangle = half == 0 ? battle_setup_shatter_upper_left_triangle : battle_setup_shatter_lower_right_triangle;
                    otz = RotTransPers3(&triangle[0], &triangle[1], &triangle[2],
                                        (long *)&prim->x0, (long *)&prim->x1, (long *)&prim->x2,
                                        &p, &flag) >> 6;
                    AddPrim(battle_setup_shatter_current_ot + otz, prim);
                }
            }
        }
    }
    SetGeomOffset(ofx, ofy);
    SetGeomScreen(screen);
}

/* 801E827C: Release the shatter task after the drawing finishes. */
void battle_setup_shatter_release(void *block) {
    DrawSync(0);
    heap_free(block);
}

/* 801E82B0: Unlink a task-registered shatter and release it after the frame. */
void battle_setup_shatter_destroy_task(Task *node) {
    task_unlink_draw_node(node + 1);
    task_unlink_main_node(node);
    sprite_queue_free_later((u32)node);
}

/* 801E82EC: Allocate and set up the shatter. */
ShatterTask *battle_setup_shatter_create(void) {
    ShatterTask *task = heap_alloc(sizeof(ShatterTask), 1);

    task->task.data = task;
    task->draw.data = task;
    return battle_setup_shatter_init(task);
}

/* 801E8320: Set up the shatter: the screen as two triangles per 32x32 cell over a
 * 320x224 grid (textured from the copy at 0x2c0,0x100), each cell 0x2000
 * away at its place on the grid (each triangle centred on its centroid). */
ShatterTask *battle_setup_shatter_init(ShatterTask *task) {
    MATRIX m;      /* unused in the original; with pos and angle reserves 0x38 bytes */
    VECTOR pos;
    SVECTOR angle;
    ShatterCell *cell;
    POLY_FT3 *prim;
    s32 half, row, col, k, x, y;
    s32 u, v, u_right, v_bottom;

    task->frame = 0;
    SquareRoot0(0x9500);
    for (half = 0; half != 2; half++) {
        for (row = 0; row != 7; row++) {
            for (col = 0; col != 10; col++) {
                v = row << 5;
                y = row * 0x200;
                cell = &task->cells[half][row][col];
                cell->rot.vx = 0;
                cell->rot.vy = 0;
                cell->rot.vz = 0;
                if (half == 0) {
                    cell->trans.vx = (col - 5) * 0x200 + 0xA0;
                    cell->trans.vy = y - 0x660; /* (v + 10 - 112) * 16 */
                    cell->trans.vz = 0x2000;
                } else {
                    cell->trans.vx = (col - 5) * 0x200 + 0x160;
                    cell->trans.vy = y - 0x5A0; /* (v + 22 - 112) * 16 */
                    cell->trans.vz = 0x2000;
                }
                for (k = 0; k != 2; k++) {
                    prim = &cell->prim[k];
                    x = col * 0x20;
                    u = x & 0x3F;
                    u_right = u + 0x20;
                    v_bottom = v + 0x20;
                    SetPolyFT3(prim);
                    SetShadeTex(prim, 0);
                    prim->r0 = 0x80;
                    prim->g0 = 0x80;
                    prim->b0 = 0x80;
                    prim->code |= 2;
                    prim->tpage = GetTPage(2, 1, x + 0x2C0, 0x100);
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
    return task;
}

/* 801E8588: Load mode (shatter): copy the screen (made semi-transparent) to
 * 0x2c0,0x100, then for at least 82 frames and until the four setup phases
 * are done (one per idle disc frame, 8001bb0c between the first two), fade
 * the background and run the shatter on the scratchpad stack. */
void battle_setup_run_shatter_load_mode(void) {
    RECT rect;
    u16 *screen;
    u16 *pixel;
    BattleArea *work;
    FrameBuffer *first;
    FrameBuffer *next;
    ShatterTask *shatter;
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
    shatter = battle_setup_shatter_create();
    while (frames != 0 || state != 5) {
        if (frames > 0) {
            frames--;
        }
        swap_buffers();
        battle_area.buffer = 1 - battle_area.buffer;
        battle_setup_shatter_current_ot = battle_area.ot;
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
                battle_setup_run_phase(phase);
                phase++;
                state++;
                break;
            }
        }
        boot_check_soft_reset();
        /* Run the shatter on a stack at the top of the scratchpad. */
        STACK_ENTER(0x1F8003FC);
        battle_setup_shatter_update(&shatter->task);
        battle_setup_shatter_draw(&shatter->task);
        STACK_LEAVE();
        DrawSync(0);
        VSync(2);
        battle_area.buffers[battle_area.buffer].drawEnv.r0 =
            sprite_add_clamp_byte(battle_area.buffers[battle_area.buffer].drawEnv.r0, -12);
        battle_area.buffers[battle_area.buffer].drawEnv.g0 =
            sprite_add_clamp_byte(battle_area.buffers[battle_area.buffer].drawEnv.g0, -12);
        battle_area.buffers[battle_area.buffer].drawEnv.b0 =
            sprite_add_clamp_byte(battle_area.buffers[battle_area.buffer].drawEnv.b0, -12);
        PutDispEnv(&battle_area.current->dispEnv);
        PutDrawEnv(&battle_area.current->drawEnv);
        DrawOTag((u_long *)&battle_area.current->ot[0xFFF]);
    }
    battle_setup_shatter_release(shatter);
    SetDispMask(0);
    cd_sync_reads(0);
    battle_setup_run_phase(3);
}

/* 801E893C: Load mode: the shatter's variant 1 (cells fade in place). */
void battle_setup_run_shatter_in_place_load_mode(void) {
    battle_setup_shatter_in_place = 1;
    battle_setup_run_shatter_load_mode();
}
