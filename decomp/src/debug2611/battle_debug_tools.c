/* debug2611: the battle debug tools (disc 1 file 2611, disc 2 file 2606),
 * loaded at 0x80280000 during a battle: text pages of battle state (pages.c),
 * then this unit (text 80280844-80282034, rodata 80280088-8028022C, data
 * 80282034-802820F0): a CPU/GPU load meter, a heap monitor, a camera tool and
 * an actor tool driven by the second pad.
 *
 * This unit was built like the 0x801fc000 battle modules, by the Cygnus CDK
 * GCC 2.7.2 with a later ASPSX (positive li as addiu, see debug2611.mk); the
 * change of toolchain at 80280844 is its boundary with the pages. */
#include "battle_debug.h"

/* Tool statics, in the original's definition order. The words this code
 * never reads (8028203c, 80282048-8028205b) are kept as they were defined. */
s32 battle_debug_heap_monitor_shown = 0;         /* 80282034: heap monitor shown */
s32 battle_debug_performance_counters_shown = 0; /* 80282038: performance counters shown */
s32 battle_debug_unused_word_1 = 0;              /* 8028203C: unreferenced */
s32 battle_debug_camera_tool_shown = 0;          /* 80282040: camera tool shown */
s32 battle_debug_geometry_offset_toggle = 0;     /* 80282044: geometry offset toggle */
s32 battle_debug_unused_word_2 = 0;              /* 80282048: unreferenced */
char battle_debug_unused_file_name[] = "mem_0"; /* 8028204C: unreferenced dump file name */
s32 battle_debug_unused_word_3 = 0;                  /* 80282054: unreferenced */
s32 battle_debug_unused_word_4 = 10;                 /* 80282058: unreferenced */
s32 battle_debug_heap_monitor_column_flags = 0x808D; /* 8028205C: heap monitor flags */
s32 battle_debug_heap_monitor_first_row = 0;         /* 80282060: heap monitor first block */
s32 battle_debug_heap_monitor_repeat_timer = 0;      /* 80282064: heap monitor scroll repeat delay */
s32 battle_debug_heap_monitor_row_count = 1;         /* 80282068: heap monitor step */

/* The load meter's needles: a triangle across the dial centre reaching 20
 * (GPU) or 30 (CPU) along the rotated x axis. */
SVECTOR battle_debug_load_meter_needles[2][3] = { /* 8028206C */
    {{0, -3, 0}, {0, 3, 0}, {20, 0, 0}},
    {{0, -3, 0}, {0, 3, 0}, {30, 0, 0}},
};

/* The actor tool's control modes. */
char battle_debug_actor_tool_pos_name[] = "pos"; /* 8028209C */
char battle_debug_actor_tool_rot_name[] = "rot"; /* 802820A0 */
char battle_debug_actor_tool_scale_name[] = "scale"; /* 802820A4 */
char battle_debug_actor_tool_lgtang_name[] = "lgtang"; /* 802820AC */
char battle_debug_actor_tool_lgtcol_name[] = "lgtcol"; /* 802820B4 */
u8 battle_debug_actor_tool_shift = 0; /* 802820BB: actor tool shift (right after the 7-byte name) */
u8 battle_debug_actor_tool_mode = 0; /* 802820BC: actor tool control mode */
char *battle_debug_actor_tool_mode_names[] = {battle_debug_actor_tool_pos_name, battle_debug_actor_tool_rot_name, battle_debug_actor_tool_scale_name, battle_debug_actor_tool_lgtang_name, battle_debug_actor_tool_lgtcol_name}; /* 802820C0 */

s32 battle_debug_heap_report_count = 0;                         /* 802820D4: memory dump count */
char battle_debug_heap_report_file_name[] = "c:\\btlmem\\mem_00"; /* 802820D8: memory dump file name */
s32 battle_debug_camera_tool_frame_count = 0;                         /* 802820EC: frame counter */

/* 80280844: Move the camera position with the pad: the directional buttons move it in
 * the camera's frame, R1/L1 (bits 0 and 2) raise and lower it; bit 1 slows
 * and bit 3 speeds the step. */
void battle_debug_move_camera_position(s32 buttons) {
    VECTOR moved;
    SVECTOR step;
    MATRIX m;
    s32 speed;

    speed = 0x20;
    if (buttons & 2) {
        speed = 8;
    }
    if (buttons & 8) {
        speed *= 4;
    }
    step.vx = 0;
    step.vy = 0;
    step.vz = 0;
    if (buttons & 0x1000) {
        step.vx = speed;
    }
    if (buttons & 0x4000) {
        step.vx = -speed;
    }
    if (buttons & 0x8000) {
        step.vz = speed;
    }
    if (buttons & 0x2000) {
        step.vz = -speed;
    }
    gpu_build_rotation_matrix(&battle_camera.rot, &m);
    ApplyMatrix(&m, &step, &moved);
    battle_camera_view_eye.vx += moved.vx;
    battle_camera_view_eye.vy += moved.vy;
    battle_camera_view_eye.vz += moved.vz;
    if (buttons & 1) {
        battle_camera_view_eye.vy += speed;
    }
    if (buttons & 4) {
        battle_camera_view_eye.vy -= speed;
    }
}

/* 80280960: Move the look-at point like the camera position, in the frame of the
 * camera's heading only. */
void battle_debug_move_look_at_point(s32 buttons) {
    VECTOR moved;
    SVECTOR step;
    SVECTOR rot;
    MATRIX m;
    s32 speed;

    speed = 0x20;
    if (buttons & 2) {
        speed = 8;
    }
    if (buttons & 8) {
        speed *= 4;
    }
    step.vx = 0;
    step.vy = 0;
    step.vz = 0;
    if (buttons & 0x1000) {
        step.vx = speed;
    }
    if (buttons & 0x4000) {
        step.vx = -speed;
    }
    if (buttons & 0x8000) {
        step.vz = speed;
    }
    if (buttons & 0x2000) {
        step.vz = -speed;
    }
    rot.vx = battle_camera_angles.vx;
    rot.vy = battle_camera_angles.vy;
    rot.vz = battle_camera_angles.vz;
    rot.vx = 0;
    gpu_build_rotation_matrix(&rot, &m);
    ApplyMatrix(&m, &step, &moved);
    battle_camera_view_target.vx += moved.vx;
    battle_camera_view_target.vy += moved.vy;
    battle_camera_view_target.vz += moved.vz;
    if (buttons & 1) {
        battle_camera_view_target.vy += speed;
    }
    if (buttons & 4) {
        battle_camera_view_target.vy -= speed;
    }
}

/* 80280A9C: The tools' frame: the actor tool, then buttons toggle the heap monitor
 * (0x800), the performance counters (0x20) and the camera tool (0x100, which
 * also switches the battle's display mode). The camera tool shows the
 * palette pages, prints the camera, marks the look-at point, emits a marker
 * effect there every 8 frames and moves the camera or look-at point. */
void battle_debug_run_tools_frame(void) {
    SVECTOR angle;
    SVECTOR watch;
    long z;
    POLY_FT4 *page;
    POLY_FT4 *page2;
    TILE_1 *mark;
    SVECTOR *target;
    s32 yaw, pitch;

    battle_debug_run_actor_tool();
    if (battle_area.pressed2 & 0x800) {
        battle_debug_heap_monitor_shown = 1 - battle_debug_heap_monitor_shown;
    }
    if (battle_debug_heap_monitor_shown != 0) {
        battle_debug_run_heap_monitor();
    }
    if (battle_area.pressed2 & 0x20) {
        battle_debug_performance_counters_shown = 1 - battle_debug_performance_counters_shown;
    }
    if (battle_debug_performance_counters_shown != 0) {
        console_printf("CPU       %d\n", battle_camera.cpu);
        console_printf("GPU       %d\n", battle_camera.gpu);
        console_printf("tasks     %d\n", task_main_count);
        console_printf("polys     %d%%\n", (sprite_queue_block_end - (u8 *)sprite_queue_next_free) * 100 / 20480);
        console_printf("frameRate %d\n", battle_area.frameTicks + 1);
        {
            /* Unreferenced bitmap format retained in the original rodata. */
            static const char bitmap_format[] = "bitmap: %x\n";
        }
    }
    if (battle_area.pressed2 & 0x100) {
        battle_debug_camera_tool_shown = 1 - battle_debug_camera_tool_shown;
        if (battle_debug_camera_tool_shown == 0) {
            battle_camera_set_mode(1);
        } else {
            battle_camera_set_mode(4);
        }
    }
    if (battle_debug_camera_tool_shown != 0) {
        page = (POLY_FT4 *)sprite_queue_next_free;
        sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(POLY_FT4));
        SetPolyFT4(page);
        SetShadeTex(page, 1);
        page->x0 = 0;
        page->y0 = -0x40;
        page->x1 = 0x40;
        page->y1 = -0x40;
        page->x2 = 0;
        page->y2 = 0xBF;
        page->x3 = 0x40;
        page->y3 = 0xBF;
        page->u0 = 0;
        page->v0 = 0;
        page->u1 = 0x7F;
        page->v1 = 0;
        page->u2 = 0;
        page->v2 = 0xFF;
        page->u3 = 0x7F;
        page->v3 = 0xFF;
        page->tpage = GetTPage(1, 0, 0x3C0, 0);
        page->clut = GetClut(0, 0x1CC);
        AddPrim((u_long *)sprite_ot, page);
        page2 = (POLY_FT4 *)sprite_queue_next_free;
        sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(POLY_FT4));
        SetPolyFT4(page2);
        SetShadeTex(page2, 1);
        page2->x0 = 0x40;
        page2->y0 = -0x40;
        page2->x1 = 0x80;
        page2->y1 = -0x40;
        page2->x2 = 0x40;
        page2->y2 = 0xBF;
        page2->x3 = 0x80;
        page2->y3 = 0xBF;
        page2->u0 = 0;
        page2->v0 = 0;
        page2->u1 = 0x7F;
        page2->v1 = 0;
        page2->u2 = 0;
        page2->v2 = 0xFF;
        page2->u3 = 0x7F;
        page2->v3 = 0xFF;
        page2->tpage = GetTPage(1, 0, 0x340, 0x100);
        page2->clut = GetClut(0, 0x1CC);
        AddPrim((u_long *)sprite_ot, page2);
        console_printf("lenge:  %d\n", battle_camera.range);
        console_printf("camera: %d,%d,%d\n", battle_camera_view_eye.vx, battle_camera_view_eye.vy, battle_camera_view_eye.vz);
        console_printf("watch:  %d,%d,%d\n", battle_camera_view_target.vx, battle_camera_view_target.vy, battle_camera_view_target.vz);
        angle.vx = yaw = (battle_camera.rot.vx & 0xFFF) * 360 / 4096;
        angle.vy = pitch = (battle_camera.rot.vy & 0xFFF) * 360 / 4096;
        console_printf("angle:  %d,%d(%d)\n", yaw, pitch, (pitch + 90) % 360);
        battle_debug_camera_tool_frame_count++;
        SetRotMatrix(&battle_camera.matrix);
        SetTransMatrix(&battle_camera.matrix);
        watch.vx = battle_camera_view_target.vx;
        target = &watch;
        watch.vy = battle_camera_view_target.vy;
        watch.vz = battle_camera_view_target.vz;
        mark = (TILE_1 *)sprite_queue_next_free;
        sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(TILE_1));
        ((u8 *)mark)[3] = 2;
        mark->code = 0x70;
        mark->r0 = 0xFF;
        mark->g0 = 0xFF;
        mark->b0 = 0;
        RotTransPers(target, (long *)&mark->x0, &z, &z);
        mark->x0 -= 4;
        mark->y0 -= 4;
        AddPrim((u_long *)sprite_ot, mark);
        if ((battle_debug_camera_tool_frame_count & 7) == 0) {
            sprite_create_effect(2, (SpriteSource *)sprite_shared_source, target, 0);
        }
        if (battle_area.pressed2 & 0x80) {
            if (++battle_debug_geometry_offset_toggle & 1) {
                SetGeomOffset(0xA0, 0x70);
            } else {
                SetGeomOffset(0xA0, 0xA5);
            }
        }
        if (battle_area.held2 & 0x40) {
            battle_debug_move_look_at_point(battle_area.held2);
            battle_debug_move_camera_position(battle_area.held2);
        } else if (battle_area.held2 & 0x20) {
            battle_debug_move_look_at_point(battle_area.held2);
        } else {
            battle_debug_move_camera_position(battle_area.held2);
        }
    }
    battle_debug_open_text_window();
}

/* 8028103C: Open the debug text window while any button is pressed. */
void battle_debug_open_text_window(void) {
    s32 unused[12]; /* unused in the original; reserves 48 bytes */

    if (battle_area.pressed2 != 0) {
        console_close();
        console_open(0x10, 0x10, 0x140, 0x100, 0x3E8, 0, 0x340, 0, 0x340, 0x20, 0);
        console_load_font_cluts(0x7FFF, 0x8000);
    }
}

/* 802810C4: The heap monitor: buttons toggle its display flags and step, left/right
 * (repeating after 8 frames) scroll its first block. */
void battle_debug_run_heap_monitor(void) {
    s32 scroll;

    battle_debug_print_wave_banks();
    scroll = 0;
    if (battle_area.pressed2 & 2) {
        battle_debug_heap_monitor_column_flags ^= 0x10;
    }
    if (battle_area.pressed2 & 8) {
        battle_debug_heap_monitor_column_flags ^= 0x20;
    }
    if (battle_area.pressed2 & 0x10) {
        battle_debug_heap_monitor_column_flags ^= 0x40;
    }
    if (battle_area.pressed2 & 0x20) {
        battle_debug_heap_monitor_column_flags ^= 0x80;
    }
    if (battle_area.pressed2 & 0x80) {
        battle_debug_heap_monitor_column_flags ^= 2;
    }
    if (battle_area.pressed2 & 0x40) {
        battle_debug_heap_monitor_column_flags ^= 0x8000;
    }
    if (battle_area.pressed2 & 4) {
        battle_debug_heap_monitor_row_count++;
    }
    if (battle_area.pressed2 & 1) {
        if (--battle_debug_heap_monitor_row_count < 0) {
            battle_debug_heap_monitor_row_count = 0;
        }
    }
    console_printf("\t\t\tdebug heap\n");
    if (battle_area.held2 & 0x5000) {
        if (++battle_debug_heap_monitor_repeat_timer >= 9) {
            battle_debug_heap_monitor_repeat_timer = 8;
        }
    } else {
        battle_debug_heap_monitor_repeat_timer = 0;
    }
    if ((battle_area.held2 & 0x1000) && battle_debug_heap_monitor_repeat_timer >= 8) {
        scroll--;
    }
    if ((battle_area.held2 & 0x4000) && battle_debug_heap_monitor_repeat_timer >= 8) {
        scroll++;
    }
    if (battle_area.pressed2 & 0x1000) {
        scroll--;
    }
    if (battle_area.pressed2 & 0x4000) {
        scroll++;
    }
    if ((battle_debug_heap_monitor_first_row += scroll) < 0) {
        battle_debug_heap_monitor_first_row = 0;
    }
    heap_print_report(3, battle_debug_heap_monitor_first_row, battle_debug_heap_monitor_row_count, battle_debug_heap_monitor_column_flags);
}

/* 80281330: Load meter update: ease the averages toward this frame's CPU and GPU times
 * and hold each peak for 80 frames. */
void battle_debug_load_meter_update(Task *task) {
    LoadMeter *meter = task->data;

    meter->cpu_avg += (battle_camera.cpu * 16 - meter->cpu_avg) >> 3;
    meter->gpu_avg += (battle_camera.gpu * 16 - meter->gpu_avg) >> 3;
    meter->cpu = meter->cpu_avg;
    meter->gpu = meter->gpu_avg;
    if (--meter->cpu_hold == 0) {
        meter->cpu_peak = 0;
    }
    if (meter->cpu > meter->cpu_peak) {
        meter->cpu_peak = meter->cpu;
        meter->cpu_hold = 80;
    }
    if (--meter->gpu_hold == 0) {
        meter->gpu_peak = 0;
    }
    if (meter->gpu > meter->gpu_peak) {
        meter->gpu_peak = meter->gpu;
        meter->gpu_hold = 80;
    }
}

/* 802813F4: Draw a flat triangle through the current matrices. */
void battle_debug_draw_flat_triangle(SVECTOR *v, u8 r, u8 g, u8 b) {
    SVECTOR xy0, xy1, xy2;
    long flag;
    POLY_F3 *prim = (POLY_F3 *)sprite_queue_next_free;

    sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(POLY_F3));
    SetPolyF3(prim);
    prim->r0 = r;
    prim->g0 = g;
    prim->b0 = b;
    RotTransSV(&v[0], &xy0, &flag);
    RotTransSV(&v[1], &xy1, &flag);
    RotTransSV(&v[2], &xy2, &flag);
    prim->x0 = xy0.vx;
    prim->y0 = xy0.vy;
    prim->x1 = xy1.vx;
    prim->y1 = xy1.vy;
    prim->x2 = xy2.vx;
    prim->y2 = xy2.vy;
    AddPrim((u_long *)sprite_ot, prim);
}

/* 802814F8: Draw a dial tick (30 to 35 along the rotated x axis). */
void battle_debug_load_meter_draw_tick(u8 r, u8 g, u8 b) {
    SVECTOR from, to;
    SVECTOR xy0, xy1;
    long flag;
    LINE_F2 *prim = (LINE_F2 *)sprite_queue_next_free;

    sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + 0x18); /* more than the 0x10-byte LINE_F2 */
    SetLineF2(prim);
    from.vx = 30;
    from.vy = 0;
    from.vz = 0;
    to.vx = 35;
    to.vy = 0;
    to.vz = 0;
    prim->r0 = r;
    prim->g0 = g;
    prim->b0 = b;
    RotTransSV(&from, &xy0, &flag);
    RotTransSV(&to, &xy1, &flag);
    prim->x0 = xy0.vx;
    prim->y0 = xy0.vy;
    prim->x1 = xy1.vx;
    prim->y1 = xy1.vy;
    AddPrim((u_long *)sprite_ot, prim);
}

/* 802815E8: Draw a peak mark: a line from the dial centre `length` along the rotated
 * x axis. */
void battle_debug_load_meter_draw_peak(s16 length, u8 r, u8 g, u8 b) {
    SVECTOR tip;
    SVECTOR xy;
    long flag;
    LINE_F2 *prim = (LINE_F2 *)sprite_queue_next_free;

    sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + 0x18); /* more than the 0x10-byte LINE_F2 */
    SetLineF2(prim);
    tip.vx = length;
    tip.vy = 0;
    tip.vz = 0;
    prim->r0 = r;
    prim->g0 = g;
    prim->b0 = b;
    RotTransSV(&tip, &xy, &flag);
    prim->x0 = 0x118;
    prim->y0 = 0xC8;
    prim->x1 = xy.vx;
    prim->y1 = xy.vy;
    AddPrim((u_long *)sprite_ot, prim);
}

/* 802816AC: Load meter drawing: the GPU (blue) and CPU (red) needles with their peak
 * marks, and a tick every 0x100 up to each needle. */
void battle_debug_load_meter_draw(Task *task) {
    MATRIX m;
    SVECTOR rot;
    VECTOR centre;
    LoadMeter *meter = task->data;
    s32 count;
    s16 angle;

    centre.vx = 0x118;
    centre.vy = 0xC8;
    centre.vz = 0;
    TransMatrix(&m, &centre);
    SetTransMatrix(&m);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->gpu - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    battle_debug_draw_flat_triangle(battle_debug_load_meter_needles[0], 0, 0, 0xFF);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->gpu_peak - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    battle_debug_load_meter_draw_peak(20, 0, 0, 0xFF);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->cpu - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    battle_debug_draw_flat_triangle(battle_debug_load_meter_needles[1], 0xFF, 0, 0);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->cpu_peak - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    battle_debug_load_meter_draw_peak(30, 0xFF, 0, 0);
    count = meter->cpu / 1024 + 1;
    for (angle = -0x400; count != 0; count--, angle += 0x100) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = angle;
        gpu_build_rotation_matrix(&rot, &m);
        SetRotMatrix(&m);
        battle_debug_load_meter_draw_tick(0xFF, 0, 0);
    }
    count = meter->gpu / 1024 + 1;
    for (angle = 0x400; count != 0; count--, angle += 0x100) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = angle;
        gpu_build_rotation_matrix(&rot, &m);
        SetRotMatrix(&m);
        battle_debug_load_meter_draw_tick(0, 0, 0xFF);
    }
}

/* 802818C4: Start the load meter task. */
void battle_debug_load_meter_start(void) {
    LoadMeter *meter = (LoadMeter *)task_alloc_two_node_task(sizeof(LoadMeter), 0, battle_debug_load_meter_update, battle_debug_load_meter_draw, 0);

    meter->gpu_hold = 1;
    meter->cpu_hold = 1;
    meter->gpu_peak = 0;
    meter->cpu_peak = 0;
    meter->gpu = 0;
    meter->cpu = 0;
    meter->gpu_avg = 0;
    meter->cpu_avg = 0;
}

/* 8028191C: List the loaded wave banks after the first (the sound driver's list at
 * sound_wave_bank_list) on the debug console. */
void battle_debug_print_wave_banks(void) {
    SoundSequence *bank = sound_wave_bank_list;
    s32 i = 0;

    for (;;) {
        SoundSequence *next = bank->next;

        if (next == NULL) {
            break;
        }
        console_report_printf("%d W=%x\n", i, next);
        bank = bank->next;
        i++;
    }
}

/* 80281980: The actor tool: for the selected battle actor print its model state, and
 * with the pad move its position, rotation, scale or light (the control
 * mode, cycled by button 2; button 8 cycles the step shift). Declared
 * int-returning (old implicit int) without a value: $v0 stays live at exit,
 * so the switch's default branch slot takes the index shift, not the table
 * address. */
s32 battle_debug_run_actor_tool(void) {
    Sprite *actor = battle_sprite_for_debugger;
    SVECTOR v;
    VECTOR step;
    long sxy, p, flag;
    s32 otz;

    if (actor == NULL) {
        return;
    }
    if ((battle_area.pressed2 & 8) && ++battle_debug_actor_tool_shift >= 7) {
        battle_debug_actor_tool_shift = 0;
    }
    console_printf("shifts mode: %x\n", battle_debug_actor_tool_shift);
    if ((battle_area.pressed2 & 2) && ++battle_debug_actor_tool_mode >= 5) {
        battle_debug_actor_tool_mode = 0;
    }
    console_printf("control mode: %s\n", battle_debug_actor_tool_mode_names[battle_debug_actor_tool_mode]);
    v.vx = actor->x >> 16;
    v.vy = actor->y >> 16;
    v.vz = actor->z >> 16;
    SetRotMatrix(&battle_camera.matrix);
    SetTransMatrix(&battle_camera.matrix);
    otz = RotTransPers(&v, &sxy, &p, &flag);
    console_printf("shapeno %x\n", actor->frame);
    console_printf("otz     %x\n", otz);
    console_printf("pos xyz %d,%d,%d\n", actor->x >> 17, actor->y >> 17, actor->z >> 17);
    console_printf("vec xyz %x,%x,%x\n", actor->speed_x >> 1, actor->speed_y >> 1, actor->speed_z >> 1);
    v.vz = (actor->renderer->angle_z & 0xFFF) * 360 / 4096;
    v.vx = (actor->renderer->angle_x & 0xFFF) * 360 / 4096;
    v.vy = (actor->renderer->angle_y & 0xFFF) * 360 / 4096;
    console_printf("rot:    %d,%d,%d\n", v.vx, v.vy, v.vz);
    console_printf("scale:  %d,%d,%d\n", actor->renderer->scale_x >> 1, actor->renderer->scale_y >> 1,
                  actor->renderer->scale_z >> 1);
    if ((actor->render.word & 3) == 2) {
        console_printf("lgtang: %d,%d,%d\n", actor->renderer->light_angles.vx, actor->renderer->light_angles.vy,
                      actor->renderer->light_angles.vz);
        console_printf("lgtcol: %d,%d,%d\n", (s16)actor->renderer->light_colour[0],
                      (s16)actor->renderer->light_colour[1], (s16)actor->renderer->light_colour[2]);
    }
    console_printf("gravity %x\n", actor->gravity);
    if (((actor->flags >> 13) & 0xF) == 0xF) {
        console_printf("polys %d\n", ((DebugShape *)((SpriteModelRenderer *)actor->renderer)->model)->polys);
    }
    step.vx = 0;
    step.vy = 0;
    step.vz = 0;
    if (battle_area.held2 & 1) {
        step.vz = -1;
    }
    if (battle_area.held2 & 4) {
        step.vz++;
    }
    if (battle_area.held2 & 0x1000) {
        step.vy = -1;
    }
    if (battle_area.held2 & 0x4000) {
        step.vy++;
    }
    if (battle_area.held2 & 0x8000) {
        step.vx = -1;
    }
    if (battle_area.held2 & 0x2000) {
        step.vx++;
    }
    step.vx <<= battle_debug_actor_tool_shift;
    step.vy <<= battle_debug_actor_tool_shift;
    step.vz <<= battle_debug_actor_tool_shift;
    switch (battle_debug_actor_tool_mode) {
    case 0:
        step.vx <<= 17;
        step.vy <<= 17;
        step.vz <<= 17;
        actor->x += step.vx;
        actor->y += step.vy;
        actor->z += step.vz;
        break;
    case 2:
        step.vx *= 16;
        step.vy *= 16;
        step.vz *= 16;
        actor->renderer->scale_x += step.vx;
        actor->renderer->scale_y += step.vy;
        actor->renderer->scale_z += step.vz;
        actor->render.word |= 0x10000000;
        break;
    case 1:
        actor->renderer->angle_x += step.vy;
        actor->renderer->angle_y += step.vx;
        actor->renderer->angle_z += step.vz;
        actor->render.word |= 0x10000000;
        break;
    case 3:
        if ((actor->render.word & 3) == 2) {
            actor->renderer->light_angles.vx += step.vx;
            actor->renderer->light_angles.vy += step.vy;
            actor->renderer->light_angles.vz += step.vz;
        }
        break;
    case 4:
        if ((actor->render.word & 3) == 2) {
            actor->renderer->light_colour[0] += step.vx;
            actor->renderer->light_colour[1] += step.vy;
            actor->renderer->light_colour[2] += step.vz;
        }
        break;
    }
}

/* 80281F98: Dump main memory to the next numbered host file (mem_0, mem_1, ...). */
void battle_debug_write_heap_report_file(void) {
    battle_debug_heap_report_count++;
    battle_debug_heap_report_file_name[15] = battle_debug_heap_report_count + '0';
    heap_write_report_file(battle_debug_heap_report_file_name);
}

/* 80281FD8: Run the memory dump on a private 16 KB stack. */
void battle_debug_write_heap_report_on_own_stack(void) {
    u8 *stack = heap_alloc(0x4000, 1);

    /* Push the caller's sp at the new stack top and switch to it. */
    STACK_ENTER(stack + 0x3FC0);
    battle_debug_write_heap_report_file();
    STACK_LEAVE();
    heap_free(stack);
}
