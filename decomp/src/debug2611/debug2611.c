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
s32 D_80282034 = 0;          /* heap monitor shown */
s32 D_80282038 = 0;          /* performance counters shown */
s32 D_8028203C = 0;          /* unreferenced */
s32 D_80282040 = 0;          /* camera tool shown */
s32 D_80282044 = 0;          /* geometry offset toggle */
s32 D_80282048 = 0;          /* unreferenced */
char D_8028204C[] = "mem_0"; /* unreferenced dump file name */
s32 D_80282054 = 0;          /* unreferenced */
s32 D_80282058 = 10;         /* unreferenced */
s32 D_8028205C = 0x808D;     /* heap monitor flags */
s32 D_80282060 = 0;          /* heap monitor first block */
s32 D_80282064 = 0;          /* heap monitor scroll repeat delay */
s32 D_80282068 = 1;          /* heap monitor step */

/* The load meter's needles: a triangle across the dial centre reaching 20
 * (GPU) or 30 (CPU) along the rotated x axis. */
SVECTOR D_8028206C[2][3] = {
    {{0, -3, 0}, {0, 3, 0}, {20, 0, 0}},
    {{0, -3, 0}, {0, 3, 0}, {30, 0, 0}},
};

/* The actor tool's control modes. */
char D_8028209C[] = "pos";
char D_802820A0[] = "rot";
char D_802820A4[] = "scale";
char D_802820AC[] = "lgtang";
char D_802820B4[] = "lgtcol";
u8 D_802820BB = 0; /* actor tool shift (right after the 7-byte name) */
u8 D_802820BC = 0; /* actor tool control mode */
char *D_802820C0[] = {D_8028209C, D_802820A0, D_802820A4, D_802820AC, D_802820B4};

s32 D_802820D4 = 0;                         /* memory dump count */
char D_802820D8[] = "c:\\btlmem\\mem_00"; /* memory dump file name */
s32 D_802820EC = 0;                         /* frame counter */

/* Move the camera position with the pad: the directional buttons move it in
 * the camera's frame, R1/L1 (bits 0 and 2) raise and lower it; bit 1 slows
 * and bit 3 speeds the step. */
void func_80280844(s32 buttons) {
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
    gpu_build_rotation_matrix(&D_800D309C.rot, &m);
    ApplyMatrix(&m, &step, &moved);
    D_800D3354.vx += moved.vx;
    D_800D3354.vy += moved.vy;
    D_800D3354.vz += moved.vz;
    if (buttons & 1) {
        D_800D3354.vy += speed;
    }
    if (buttons & 4) {
        D_800D3354.vy -= speed;
    }
}

/* Move the look-at point like the camera position, in the frame of the
 * camera's heading only. */
void func_80280960(s32 buttons) {
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
    rot.vx = D_800D30B0.vx;
    rot.vy = D_800D30B0.vy;
    rot.vz = D_800D30B0.vz;
    rot.vx = 0;
    gpu_build_rotation_matrix(&rot, &m);
    ApplyMatrix(&m, &step, &moved);
    D_800D335C.vx += moved.vx;
    D_800D335C.vy += moved.vy;
    D_800D335C.vz += moved.vz;
    if (buttons & 1) {
        D_800D335C.vy += speed;
    }
    if (buttons & 4) {
        D_800D335C.vy -= speed;
    }
}

/* The tools' frame: the actor tool, then buttons toggle the heap monitor
 * (0x800), the performance counters (0x20) and the camera tool (0x100, which
 * also switches the battle's display mode). The camera tool shows the
 * palette pages, prints the camera, marks the look-at point, emits a marker
 * effect there every 8 frames and moves the camera or look-at point. */
void func_80280A9C(void) {
    SVECTOR angle;
    SVECTOR watch;
    long z;
    POLY_FT4 *page;
    POLY_FT4 *page2;
    TILE_1 *mark;
    SVECTOR *target;
    s32 yaw, pitch;

    func_80281980();
    if (D_800C3EB0.pressed2 & 0x800) {
        D_80282034 = 1 - D_80282034;
    }
    if (D_80282034 != 0) {
        func_802810C4();
    }
    if (D_800C3EB0.pressed2 & 0x20) {
        D_80282038 = 1 - D_80282038;
    }
    if (D_80282038 != 0) {
        console_printf("CPU       %d\n", D_800D309C.cpu);
        console_printf("GPU       %d\n", D_800D309C.gpu);
        console_printf("tasks     %d\n", task_main_count);
        console_printf("polys     %d%%\n", (sprite_queue_block_end - (u8 *)sprite_queue_next_free) * 100 / 20480);
        console_printf("frameRate %d\n", D_800C3EB0.frameTicks + 1);
        {
            /* Unreferenced bitmap format retained in the original rodata. */
            static const char bitmap_format[] = "bitmap: %x\n";
        }
    }
    if (D_800C3EB0.pressed2 & 0x100) {
        D_80282040 = 1 - D_80282040;
        if (D_80282040 == 0) {
            func_800BC2F0(1);
        } else {
            func_800BC2F0(4);
        }
    }
    if (D_80282040 != 0) {
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
        console_printf("lenge:  %d\n", D_800D309C.range);
        console_printf("camera: %d,%d,%d\n", D_800D3354.vx, D_800D3354.vy, D_800D3354.vz);
        console_printf("watch:  %d,%d,%d\n", D_800D335C.vx, D_800D335C.vy, D_800D335C.vz);
        angle.vx = yaw = (D_800D309C.rot.vx & 0xFFF) * 360 / 4096;
        angle.vy = pitch = (D_800D309C.rot.vy & 0xFFF) * 360 / 4096;
        console_printf("angle:  %d,%d(%d)\n", yaw, pitch, (pitch + 90) % 360);
        D_802820EC++;
        SetRotMatrix(&D_800D309C.matrix);
        SetTransMatrix(&D_800D309C.matrix);
        watch.vx = D_800D335C.vx;
        target = &watch;
        watch.vy = D_800D335C.vy;
        watch.vz = D_800D335C.vz;
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
        if ((D_802820EC & 7) == 0) {
            sprite_create_effect(2, (SpriteSource *)sprite_shared_source, target, 0);
        }
        if (D_800C3EB0.pressed2 & 0x80) {
            if (++D_80282044 & 1) {
                SetGeomOffset(0xA0, 0x70);
            } else {
                SetGeomOffset(0xA0, 0xA5);
            }
        }
        if (D_800C3EB0.held2 & 0x40) {
            func_80280960(D_800C3EB0.held2);
            func_80280844(D_800C3EB0.held2);
        } else if (D_800C3EB0.held2 & 0x20) {
            func_80280960(D_800C3EB0.held2);
        } else {
            func_80280844(D_800C3EB0.held2);
        }
    }
    func_8028103C();
}

/* Open the debug text window while any button is pressed. */
void func_8028103C(void) {
    s32 unused[12]; /* unused in the original; reserves 48 bytes */

    if (D_800C3EB0.pressed2 != 0) {
        console_close();
        console_open(0x10, 0x10, 0x140, 0x100, 0x3E8, 0, 0x340, 0, 0x340, 0x20, 0);
        console_load_font_cluts(0x7FFF, 0x8000);
    }
}

/* The heap monitor: buttons toggle its display flags and step, left/right
 * (repeating after 8 frames) scroll its first block. */
void func_802810C4(void) {
    s32 scroll;

    func_8028191C();
    scroll = 0;
    if (D_800C3EB0.pressed2 & 2) {
        D_8028205C ^= 0x10;
    }
    if (D_800C3EB0.pressed2 & 8) {
        D_8028205C ^= 0x20;
    }
    if (D_800C3EB0.pressed2 & 0x10) {
        D_8028205C ^= 0x40;
    }
    if (D_800C3EB0.pressed2 & 0x20) {
        D_8028205C ^= 0x80;
    }
    if (D_800C3EB0.pressed2 & 0x80) {
        D_8028205C ^= 2;
    }
    if (D_800C3EB0.pressed2 & 0x40) {
        D_8028205C ^= 0x8000;
    }
    if (D_800C3EB0.pressed2 & 4) {
        D_80282068++;
    }
    if (D_800C3EB0.pressed2 & 1) {
        if (--D_80282068 < 0) {
            D_80282068 = 0;
        }
    }
    console_printf("\t\t\tdebug heap\n");
    if (D_800C3EB0.held2 & 0x5000) {
        if (++D_80282064 >= 9) {
            D_80282064 = 8;
        }
    } else {
        D_80282064 = 0;
    }
    if ((D_800C3EB0.held2 & 0x1000) && D_80282064 >= 8) {
        scroll--;
    }
    if ((D_800C3EB0.held2 & 0x4000) && D_80282064 >= 8) {
        scroll++;
    }
    if (D_800C3EB0.pressed2 & 0x1000) {
        scroll--;
    }
    if (D_800C3EB0.pressed2 & 0x4000) {
        scroll++;
    }
    if ((D_80282060 += scroll) < 0) {
        D_80282060 = 0;
    }
    heap_print_report(3, D_80282060, D_80282068, D_8028205C);
}

/* Load meter update: ease the averages toward this frame's CPU and GPU times
 * and hold each peak for 80 frames. */
void func_80281330(Task *task) {
    LoadMeter *meter = task->data;

    meter->cpu_avg += (D_800D309C.cpu * 16 - meter->cpu_avg) >> 3;
    meter->gpu_avg += (D_800D309C.gpu * 16 - meter->gpu_avg) >> 3;
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

/* Draw a flat triangle through the current matrices. */
void func_802813F4(SVECTOR *v, u8 r, u8 g, u8 b) {
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

/* Draw a dial tick (30 to 35 along the rotated x axis). */
void func_802814F8(u8 r, u8 g, u8 b) {
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

/* Draw a peak mark: a line from the dial centre `length` along the rotated
 * x axis. */
void func_802815E8(s16 length, u8 r, u8 g, u8 b) {
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

/* Load meter drawing: the GPU (blue) and CPU (red) needles with their peak
 * marks, and a tick every 0x100 up to each needle. */
void func_802816AC(Task *task) {
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
    func_802813F4(D_8028206C[0], 0, 0, 0xFF);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->gpu_peak - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    func_802815E8(20, 0, 0, 0xFF);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->cpu - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    func_802813F4(D_8028206C[1], 0xFF, 0, 0);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->cpu_peak - 0x400;
    gpu_build_rotation_matrix(&rot, &m);
    SetRotMatrix(&m);
    func_802815E8(30, 0xFF, 0, 0);
    count = meter->cpu / 1024 + 1;
    for (angle = -0x400; count != 0; count--, angle += 0x100) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = angle;
        gpu_build_rotation_matrix(&rot, &m);
        SetRotMatrix(&m);
        func_802814F8(0xFF, 0, 0);
    }
    count = meter->gpu / 1024 + 1;
    for (angle = 0x400; count != 0; count--, angle += 0x100) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = angle;
        gpu_build_rotation_matrix(&rot, &m);
        SetRotMatrix(&m);
        func_802814F8(0, 0, 0xFF);
    }
}

/* Start the load meter task. */
void func_802818C4(void) {
    LoadMeter *meter = (LoadMeter *)task_alloc_two_node_task(sizeof(LoadMeter), 0, func_80281330, func_802816AC, 0);

    meter->gpu_hold = 1;
    meter->cpu_hold = 1;
    meter->gpu_peak = 0;
    meter->cpu_peak = 0;
    meter->gpu = 0;
    meter->cpu = 0;
    meter->gpu_avg = 0;
    meter->cpu_avg = 0;
}

/* List the loaded wave banks after the first (the sound driver's list at
 * sound_wave_bank_list) on the debug console. */
void func_8028191C(void) {
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

/* The actor tool: for the selected battle actor print its model state, and
 * with the pad move its position, rotation, scale or light (the control
 * mode, cycled by button 2; button 8 cycles the step shift). Declared
 * int-returning (old implicit int) without a value: $v0 stays live at exit,
 * so the switch's default branch slot takes the index shift, not the table
 * address. */
s32 func_80281980(void) {
    Sprite *actor = D_800C3568;
    SVECTOR v;
    VECTOR step;
    long sxy, p, flag;
    s32 otz;

    if (actor == NULL) {
        return;
    }
    if ((D_800C3EB0.pressed2 & 8) && ++D_802820BB >= 7) {
        D_802820BB = 0;
    }
    console_printf("shifts mode: %x\n", D_802820BB);
    if ((D_800C3EB0.pressed2 & 2) && ++D_802820BC >= 5) {
        D_802820BC = 0;
    }
    console_printf("control mode: %s\n", D_802820C0[D_802820BC]);
    v.vx = actor->x >> 16;
    v.vy = actor->y >> 16;
    v.vz = actor->z >> 16;
    SetRotMatrix(&D_800D309C.matrix);
    SetTransMatrix(&D_800D309C.matrix);
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
    if (D_800C3EB0.held2 & 1) {
        step.vz = -1;
    }
    if (D_800C3EB0.held2 & 4) {
        step.vz++;
    }
    if (D_800C3EB0.held2 & 0x1000) {
        step.vy = -1;
    }
    if (D_800C3EB0.held2 & 0x4000) {
        step.vy++;
    }
    if (D_800C3EB0.held2 & 0x8000) {
        step.vx = -1;
    }
    if (D_800C3EB0.held2 & 0x2000) {
        step.vx++;
    }
    step.vx <<= D_802820BB;
    step.vy <<= D_802820BB;
    step.vz <<= D_802820BB;
    switch (D_802820BC) {
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

/* Dump main memory to the next numbered host file (mem_0, mem_1, ...). */
void func_80281F98(void) {
    D_802820D4++;
    D_802820D8[15] = D_802820D4 + '0';
    heap_write_report_file(D_802820D8);
}

/* Run the memory dump on a private 16 KB stack. */
void func_80281FD8(void) {
    u8 *stack = heap_alloc(0x4000, 1);

    /* Push the caller's sp at the new stack top and switch to it. */
    STACK_ENTER(stack + 0x3FC0);
    func_80281F98();
    STACK_LEAVE();
    heap_free(stack);
}
