/* debug2611: the battle debug tools (disc 1 file 2611, disc 2 file 2606),
 * loaded at 0x80280000 during a battle: text pages of battle state, a
 * CPU/GPU load meter, a heap monitor, a camera tool and an actor tool driven
 * by the pad.
 *
 * This unit was built like the 0x801fc000 battle modules, by the Cygnus CDK
 * GCC 2.7.2 with a later ASPSX (see debug2611.mk). */
#include "battle_debug.h"

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
    func_8003F738(&D_800D309C.rot, &m);
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
    func_8003F738(&rot, &m);
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
#ifdef NON_MATCHING
void func_80280A9C(void) {
    SVECTOR angle;
    SVECTOR watch;
    s32 sxy, z;
    POLY_FT4 *page;
    POLY_FT4 *page2;
    TILE_1 *mark;
    s32 yaw, pitch;

    func_80281980();
    if (D_800C3EB0.pressed & 0x800) {
        D_80282034 = 1 - D_80282034;
    }
    if (D_80282034 != 0) {
        func_802810C4();
    }
    if (D_800C3EB0.pressed & 0x20) {
        D_80282038 = 1 - D_80282038;
    }
    if (D_80282038 != 0) {
        func_8003700C("CPU       %d\n", D_800D309C.cpu);
        func_8003700C("GPU       %d\n", D_800D309C.gpu);
        func_8003700C("tasks     %d\n", D_80059188);
        func_8003700C("polys     %d%%\n", (D_80059534 - D_80059580) * 100 / 20480);
        func_8003700C("frameRate %d\n", D_800C3EB0.frame_rate + 1);
    }
    if (D_800C3EB0.pressed & 0x100) {
        D_80282040 = 1 - D_80282040;
        if (D_80282040 == 0) {
            func_800BC2F0(1);
        } else {
            func_800BC2F0(4);
        }
    }
    if (D_80282040 != 0) {
        page = (POLY_FT4 *)D_80059580;
        D_80059580 += sizeof(POLY_FT4);
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
        AddPrim(D_8005956C, page);
        page2 = (POLY_FT4 *)D_80059580;
        D_80059580 += sizeof(POLY_FT4);
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
        AddPrim(D_8005956C, page2);
        func_8003700C("lenge:  %d\n", D_800D309C.range);
        func_8003700C("camera: %d,%d,%d\n", D_800D3354.vx, D_800D3354.vy, D_800D3354.vz);
        func_8003700C("watch:  %d,%d,%d\n", D_800D335C.vx, D_800D335C.vy, D_800D335C.vz);
        angle.vx = yaw = (D_800D309C.rot.vx & 0xFFF) * 360 / 4096;
        angle.vy = pitch = (D_800D309C.rot.vy & 0xFFF) * 360 / 4096;
        func_8003700C("angle:  %d,%d(%d)\n", yaw, pitch, (pitch + 90) % 360);
        D_802820EC++;
        SetRotMatrix(&D_800D309C.matrix);
        SetTransMatrix(&D_800D309C.matrix);
        watch.vx = D_800D335C.vx;
        watch.vy = D_800D335C.vy;
        watch.vz = D_800D335C.vz;
        mark = (TILE_1 *)D_80059580;
        D_80059580 += sizeof(TILE_1);
        ((u8 *)mark)[3] = 2;
        mark->code = 0x70;
        mark->r0 = 0xFF;
        mark->g0 = 0xFF;
        mark->b0 = 0;
        RotTransPers(&watch, (s32 *)&mark->x0, &z, &z);
        mark->x0 -= 4;
        mark->y0 -= 4;
        AddPrim(D_8005956C, mark);
        if ((D_802820EC & 7) == 0) {
            func_80023FD8(2, D_8006BE10, &watch, 0);
        }
        if (D_800C3EB0.pressed & 0x80) {
            if (++D_80282044 & 1) {
                SetGeomOffset(0xA0, 0x70);
            } else {
                SetGeomOffset(0xA0, 0xA5);
            }
        }
        if (D_800C3EB0.held & 0x40) {
            func_80280960(D_800C3EB0.held);
            func_80280844(D_800C3EB0.held);
        } else if (D_800C3EB0.held & 0x20) {
            func_80280960(D_800C3EB0.held);
        } else {
            func_80280844(D_800C3EB0.held);
        }
    }
    func_8028103C();
}
#else
INCLUDE_ASM(".local/decomp/debug2611/asm/nonmatchings/debug2611", func_80280A9C);
#endif

/* Open the debug text window while any button is pressed. */
void func_8028103C(void) {
    s32 unused[12]; /* an unreferenced 0x30-byte local: the frame is 0x68 */

    if (D_800C3EB0.pressed != 0) {
        func_8003748C();
        func_800374E8(0x10, 0x10, 0x140, 0x100, 0x3E8, 0, 0x340, 0, 0x340, 0x20, 0);
        func_80036E4C(0x7FFF, 0x8000);
    }
}

/* The heap monitor: buttons toggle its display flags and step, left/right
 * (repeating after 8 frames) scroll its first block. */
void func_802810C4(void) {
    s32 scroll;

    func_8028191C();
    scroll = 0;
    if (D_800C3EB0.pressed & 2) {
        D_8028205C ^= 0x10;
    }
    if (D_800C3EB0.pressed & 8) {
        D_8028205C ^= 0x20;
    }
    if (D_800C3EB0.pressed & 0x10) {
        D_8028205C ^= 0x40;
    }
    if (D_800C3EB0.pressed & 0x20) {
        D_8028205C ^= 0x80;
    }
    if (D_800C3EB0.pressed & 0x80) {
        D_8028205C ^= 2;
    }
    if (D_800C3EB0.pressed & 0x40) {
        D_8028205C ^= 0x8000;
    }
    if (D_800C3EB0.pressed & 4) {
        D_80282068++;
    }
    if (D_800C3EB0.pressed & 1) {
        if (--D_80282068 < 0) {
            D_80282068 = 0;
        }
    }
    func_8003700C("\t\t\tdebug heap\n");
    if (D_800C3EB0.held & 0x5000) {
        if (++D_80282064 >= 9) {
            D_80282064 = 8;
        }
    } else {
        D_80282064 = 0;
    }
    if ((D_800C3EB0.held & 0x1000) && D_80282064 >= 8) {
        scroll--;
    }
    if ((D_800C3EB0.held & 0x4000) && D_80282064 >= 8) {
        scroll++;
    }
    if (D_800C3EB0.pressed & 0x1000) {
        scroll--;
    }
    if (D_800C3EB0.pressed & 0x4000) {
        scroll++;
    }
    if ((D_80282060 += scroll) < 0) {
        D_80282060 = 0;
    }
    func_8003278C(3, D_80282060, D_80282068, D_8028205C);
}

/* Load meter update: ease the averages toward this frame's CPU and GPU times
 * and hold each peak for 80 frames. */
void func_80281330(TaskNode *node) {
    LoadMeter *meter = node->object;

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
    s32 flag;
    POLY_F3 *prim = (POLY_F3 *)D_80059580;

    D_80059580 += sizeof(POLY_F3);
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
    AddPrim(D_8005956C, prim);
}

/* Draw a dial tick (30 to 35 along the rotated x axis). */
void func_802814F8(u8 r, u8 g, u8 b) {
    SVECTOR from, to;
    SVECTOR xy0, xy1;
    s32 flag;
    LINE_F2 *prim = (LINE_F2 *)D_80059580;

    D_80059580 += 0x18; /* reserves more than the 0x10-byte LINE_F2 */
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
    AddPrim(D_8005956C, prim);
}

/* Draw a peak mark: a line from the dial centre `length` along the rotated
 * x axis. */
void func_802815E8(s16 length, u8 r, u8 g, u8 b) {
    SVECTOR tip;
    SVECTOR xy;
    s32 flag;
    LINE_F2 *prim = (LINE_F2 *)D_80059580;

    D_80059580 += 0x18; /* reserves more than the 0x10-byte LINE_F2 */
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
    AddPrim(D_8005956C, prim);
}

/* Load meter drawing: the GPU (blue) and CPU (red) needles with their peak
 * marks, and a tick every 0x100 up to each needle. */
void func_802816AC(TaskNode *node) {
    MATRIX m;
    SVECTOR rot;
    VECTOR centre;
    LoadMeter *meter = node->object;
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
    func_8003F738(&rot, &m);
    SetRotMatrix(&m);
    func_802813F4(D_8028206C[0], 0, 0, 0xFF);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->gpu_peak - 0x400;
    func_8003F738(&rot, &m);
    SetRotMatrix(&m);
    func_802815E8(20, 0, 0, 0xFF);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->cpu - 0x400;
    func_8003F738(&rot, &m);
    SetRotMatrix(&m);
    func_802813F4(D_8028206C[1], 0xFF, 0, 0);
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = meter->cpu_peak - 0x400;
    func_8003F738(&rot, &m);
    SetRotMatrix(&m);
    func_802815E8(30, 0xFF, 0, 0);
    count = meter->cpu / 1024 + 1;
    for (angle = -0x400; count != 0; count--, angle += 0x100) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = angle;
        func_8003F738(&rot, &m);
        SetRotMatrix(&m);
        func_802814F8(0xFF, 0, 0);
    }
    count = meter->gpu / 1024 + 1;
    for (angle = 0x400; count != 0; count--, angle += 0x100) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = angle;
        func_8003F738(&rot, &m);
        SetRotMatrix(&m);
        func_802814F8(0, 0, 0xFF);
    }
}

/* Start the load meter task. */
void func_802818C4(void) {
    LoadMeter *meter = func_8001D1D8(sizeof(LoadMeter), 0, func_80281330, func_802816AC, 0);

    meter->gpu_hold = 1;
    meter->cpu_hold = 1;
    meter->gpu_peak = 0;
    meter->cpu_peak = 0;
    meter->gpu = 0;
    meter->cpu = 0;
    meter->gpu_avg = 0;
    meter->cpu_avg = 0;
}

/* List the playing sound sequences on the debug console. */
void func_8028191C(void) {
    DebugSequence *seq = D_80059558;
    s32 i = 0;

    for (;;) {
        DebugSequence *next = seq->next;

        if (next == NULL) {
            break;
        }
        func_800379C8("%d W=%x\n", i, next);
        seq = seq->next;
        i++;
    }
}

/* The actor tool: for the selected battle actor print its model state, and
 * with the pad move its position, rotation, scale or light (the control
 * mode, cycled by button 2; button 8 cycles the step shift). */
#ifdef NON_MATCHING
void func_80281980(void) {
    DebugActor *actor = D_800C3568;
    SVECTOR v;
    VECTOR step;
    s32 sxy, p, flag;
    s32 otz;

    if (actor == NULL) {
        return;
    }
    if ((D_800C3EB0.pressed & 8) && ++D_802820BB >= 7) {
        D_802820BB = 0;
    }
    func_8003700C("shifts mode: %x\n", D_802820BB);
    if ((D_800C3EB0.pressed & 2) && ++D_802820BC >= 5) {
        D_802820BC = 0;
    }
    func_8003700C("control mode: %s\n", D_802820C0[D_802820BC]);
    v.vx = actor->pos[0].raw >> 16;
    v.vy = actor->pos[1].raw >> 16;
    v.vz = actor->pos[2].raw >> 16;
    SetRotMatrix(&D_800D309C.matrix);
    SetTransMatrix(&D_800D309C.matrix);
    otz = RotTransPers(&v, &sxy, &p, &flag);
    func_8003700C("shapeno %x\n", actor->shape);
    func_8003700C("otz     %x\n", otz);
    func_8003700C("pos xyz %d,%d,%d\n", actor->pos[0].raw >> 17, actor->pos[1].raw >> 17, actor->pos[2].raw >> 17);
    func_8003700C("vec xyz %x,%x,%x\n", actor->vel[0] >> 1, actor->vel[1] >> 1, actor->vel[2] >> 1);
    v.vz = (actor->model->rot.vz & 0xFFF) * 360 / 4096;
    v.vx = (actor->model->rot.vx & 0xFFF) * 360 / 4096;
    v.vy = (actor->model->rot.vy & 0xFFF) * 360 / 4096;
    func_8003700C("rot:    %d,%d,%d\n", v.vx, v.vy, v.vz);
    func_8003700C("scale:  %d,%d,%d\n", actor->model->scale.vx >> 1, actor->model->scale.vy >> 1,
                  actor->model->scale.vz >> 1);
    if ((actor->flags & 3) == 2) {
        func_8003700C("lgtang: %d,%d,%d\n", actor->model->light_angle.vx, actor->model->light_angle.vy,
                      actor->model->light_angle.vz);
        func_8003700C("lgtcol: %d,%d,%d\n", actor->model->light_color.vx, actor->model->light_color.vy,
                      actor->model->light_color.vz);
    }
    func_8003700C("gravity %x\n", actor->gravity);
    if (((actor->state >> 13) & 0xF) == 0xF) {
        func_8003700C("polys %d\n", actor->model->shape->polys);
    }
    step.vx = 0;
    step.vy = 0;
    step.vz = 0;
    if (D_800C3EB0.held & 1) {
        step.vz = -1;
    }
    if (D_800C3EB0.held & 4) {
        step.vz++;
    }
    if (D_800C3EB0.held & 0x1000) {
        step.vy = -1;
    }
    if (D_800C3EB0.held & 0x4000) {
        step.vy++;
    }
    if (D_800C3EB0.held & 0x8000) {
        step.vx = -1;
    }
    if (D_800C3EB0.held & 0x2000) {
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
        actor->pos[0].raw += step.vx;
        actor->pos[1].raw += step.vy;
        actor->pos[2].raw += step.vz;
        break;
    case 2:
        step.vx *= 16;
        step.vy *= 16;
        step.vz *= 16;
        actor->model->scale.vx += step.vx;
        actor->model->scale.vy += step.vy;
        actor->model->scale.vz += step.vz;
        actor->flags |= 0x10000000;
        break;
    case 1:
        actor->model->rot.vx += step.vy;
        actor->model->rot.vy += step.vx;
        actor->model->rot.vz += step.vz;
        actor->flags |= 0x10000000;
        break;
    case 3:
        if ((actor->flags & 3) == 2) {
            actor->model->light_angle.vx += step.vx;
            actor->model->light_angle.vy += step.vy;
            actor->model->light_angle.vz += step.vz;
        }
        break;
    case 4:
        if ((actor->flags & 3) == 2) {
            actor->model->light_color.vx += step.vx;
            actor->model->light_color.vy += step.vy;
            actor->model->light_color.vz += step.vz;
        }
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/debug2611/asm/nonmatchings/debug2611", func_80281980);
#endif

/* Dump main memory to the next numbered host file (mem_0, mem_1, ...). */
void func_80281F98(void) {
    D_802820D4++;
    D_802820D8[15] = D_802820D4 + '0';
    func_80032E04(D_802820D8);
}

/* Run the memory dump on a private 16 KB stack. */
void func_80281FD8(void) {
    u8 *stack = func_80031BDC(0x4000, 1);

    /* Push the caller's sp at the new stack top and switch to it. */
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(stack + 0x3FC0)
                     : "$8", "memory");
    func_80281F98();
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    func_800320E8(stack);
}
