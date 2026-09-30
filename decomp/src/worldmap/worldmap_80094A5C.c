#include "worldmap.h"
#include "psyq/libsn.h"

/* Declared here only: other units call it without a prototype. */
void func_80093354(VECTOR *position);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80094A5C);

/* Probe a move and choose the direction to slide along: 1 when free, 0 when
 * the obstacle deflects it (out holds the slide direction). */
s32 func_800951A8(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s16 mode) {
    s32 result;

    switch (func_80094A5C(position, direction, scale, mode)) {
    case 0:
        result = 1;
        break;
    case 1:
        if (func_80094088(SCRATCH_VECTOR, direction, out) == 0) {
            out->vz = 0;
            out->vy = 0;
            out->vx = 0;
        }
        result = 0;
        break;
    case 2:
        out->vx = direction->vx < 0 ? -0x1000 : 0x1000;
        out->vz = 0;
        out->vy = 0;
        result = 0;
        break;
    case 3:
        out->vy = 0;
        out->vx = 0;
        out->vz = direction->vz < 0 ? -0x1000 : 0x1000;
        result = 0;
        break;
    }
    return result;
}

/* Orient `normal` towards `direction` on the ground plane (zero when
 * perpendicular). */
void func_800952B0(VECTOR *direction, VECTOR *out, VECTOR *normal) {
    s32 dot;

    dot = normal->vx * direction->vx + normal->vz * direction->vz;
    if (dot < 0) {
        out->vx = -normal->vx;
        out->vz = -normal->vz;
    } else if (dot > 0) {
        out->vx = normal->vx;
        out->vz = normal->vz;
    } else {
        out->vz = 0;
        out->vx = 0;
    }
    out->vy = 0;
}

/* Orient the horizontal tangent of a wall normal towards `direction` (zero
 * when perpendicular). */
#ifdef NON_MATCHING /* second product lands in v0 instead of v1 */
void func_80095324(VECTOR *normal, VECTOR *direction, VECTOR *out) {
    s32 tangent;
    s32 dot;

    SCRATCH_VECTOR[1].vz = 0;
    SCRATCH_VECTOR[1].vx = 0;
    SCRATCH_VECTOR[1].vy = -0x1000;
    func_8004A480(normal, &SCRATCH_VECTOR[1], &SCRATCH_VECTOR[2]);
    func_80048D7C(&SCRATCH_VECTOR[2], &SCRATCH_VECTOR[0]);
    tangent = SCRATCH_VECTOR[0].vx;
    dot = tangent * direction->vx + SCRATCH_VECTOR[0].vz * direction->vz;
    if (dot < 0) {
        out->vx = -tangent;
        out->vz = -SCRATCH_VECTOR[0].vz;
    } else if (dot > 0) {
        out->vx = tangent;
        out->vz = SCRATCH_VECTOR[0].vz;
    } else {
        out->vz = 0;
        out->vx = 0;
    }
    out->vy = 0;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095324);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095414);

/* Move a flying position: clamp its height between the ground and the
 * ceiling, bounce back off solid objects, else slide along the terrain. */
#ifdef NON_MATCHING /* the probe address is kept in a saved register */
s32 func_80095CD4(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode) {
    s16 hit;
    s32 floor;
    s32 count;
    s32 i;
    s32 cleared;

    SCRATCH_PROBE->vx = position->vx + ((direction->vx * scale) >> 12);
    SCRATCH_PROBE->vy = position->vy + ((direction->vy * scale) >> 12);
    SCRATCH_PROBE->vz = position->vz + ((direction->vz * scale) >> 12);
    func_80093354(SCRATCH_PROBE);
    out->vx = position->vx + ((direction->vx * scale) >> 12);
    out->vy = position->vy + ((direction->vy * scale) >> 12);
    out->vz = position->vz + ((direction->vz * scale) >> 12);
    func_80093354(out);
    floor = func_80093978(SCRATCH_PROBE->vx, SCRATCH_PROBE->vz) - 0x20000;
    if (floor > 0x20000) {
        floor = 0x20000;
    }
    if ((floor < SCRATCH_PROBE->vy) | (floor < -0x280000)) {
        SCRATCH_PROBE->vy = floor;
        out->vy = floor;
    }
    if ((floor > -0x280000) & (SCRATCH_PROBE->vy < -0x280000)) {
        SCRATCH_PROBE->vy = -0x280000;
        out->vy = -0x280000;
    }
    count = func_80084D00((s32)SCRATCH_PROBE, &hit);
    if (count != 0) {
        cleared = 0;
        for (i = 0; i < count; i += 2) {
            if (func_80085418(SCRATCH_PROBE, 0x70, hit, D_8009D718[i]) == 0) {
                D_8009D718[i] = -1;
                cleared += 2;
            }
        }
        if (cleared != count) {
            out->vx = -direction->vx >> 1;
            out->vy = -direction->vy >> 1;
            out->vz = -direction->vz >> 1;
            return 0;
        }
    }
    return func_800951A8(position, direction, out, scale, mode);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095CD4);
#endif

/* Reset the stream queue and allocate its command buffers (disc or host). */
void func_80095F78(void) {
    s32 first;
    s32 second;
    s32 i;
    s8 *flag;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        D_8009BCB8 = 0;
        D_8009BE44 = 0;
        D_8009CD44 = 0;
        for (i = 0xF; i >= 0; i--) {
            D_8009D788[i] = NULL;
        }
        D_8009BE08 = func_80031BDC(0x4200, 0);
        D_8009D7D4 = func_80031BDC(0x800, 0);
        D_8009D808 = 0;
        for (i = 7, flag = &D_8009C588[7]; i >= 0; i--) {
            *flag-- = 0;
        }
    } else {
        D_8009BCB8 = 0;
        D_8009BE44 = 0;
        D_8009CD44 = 0;
        for (i = 0xF; i >= 0; i--) {
            D_8009C624[i] = NULL;
        }
        D_8009D3C0 = func_80031BDC(0x5800, 0);
        D_8009D7D4 = func_80031BDC(0x800, 0);
        D_8009D808 = 0;
        for (i = 7, flag = &D_8009C588[7]; i >= 0; i--) {
            *flag-- = 0;
        }
    }
}

/* Free the effect command buffers. */
void func_800960BC(void) {
    s32 first;
    s32 second;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        func_800320E8(D_8009BE08);
    } else {
        func_800320E8(D_8009D3C0);
    }
    func_800320E8(D_8009D7D4);
}

/* Wait until the current write slot of the stream queue is free. */
void func_80096130(void) {
    s32 first;
    s32 second;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        while (D_8009D788[D_8009BE44] != NULL) {
            VSync(0);
            func_800967E4();
        }
    } else {
        while (D_8009C624[D_8009BE44] != NULL) {
            VSync(0);
            func_800967E4();
        }
    }
}

/* Append a three-word effect command to the current frame's list. */
s32 func_8009623C(s32 a, s32 b, s32 c) {
    EffectCommand3 *command;
    s32 count;

    count = D_8009D808;
    if (count < 0x58) {
        D_8009D808 = count + 1;
        command = (EffectCommand3 *)((u8 *)D_8009BE08 + D_8009BE44 * 0x420) + count;
        command->a = a;
        command->b = b;
        command->c = c;
        return 0;
    }
    return -1;
}

/* Append a four-word effect command to the current frame's list. */
s32 func_800962B0(s32 a, s32 b, s32 c, s32 d) {
    EffectCommand4 *command;
    s32 count;

    count = D_8009D808;
    if (count < 0x58) {
        D_8009D808 = count + 1;
        command = (EffectCommand4 *)((u8 *)D_8009D3C0 + D_8009BE44 * 0x580) + count;
        command->a = a;
        command->b = b;
        command->c = c;
        command->d = d;
        return 0;
    }
    return -1;
}

/* Submit the current effect command list; -1 when there is nothing to
 * send or its ring slot is still busy. */
s32 func_80096328(void) {
    s32 *list;

    list = (s32 *)((u8 *)D_8009BE08 + D_8009BE44 * 0x420);
    if (*list != 0 && D_8009D788[D_8009BE44] == NULL) {
        func_800963E4(list);
        D_8009D808 = 0;
        D_8009D788[D_8009BE44] = list;
        D_8009BE44 = (D_8009BE44 + 1) & 0xF;
        return 0;
    }
    D_8009D808 = 0;
    return -1;
}

/* Sort a disc request list by sector (insertion sort in place). */
void func_800963E4(s32 *list) {
    EffectCommand3 *first;
    EffectCommand3 *p;
    EffectCommand3 swap;

    p = (EffectCommand3 *)list;
    first = p;
    while (p[1].a != 0) {
        if ((u32)p[0].a > (u32)p[1].a) {
            swap.a = p[0].a;
            swap.b = p[0].b;
            swap.c = p[0].c;
            p[0].a = p[1].a;
            p[0].b = p[1].b;
            p[0].c = p[1].c;
            p[1].a = swap.a;
            p[1].b = swap.b;
            p[1].c = swap.c;
            if (first < p) {
                p--;
            }
        } else {
            p++;
        }
    }
}

/* Sort a host-file request list by offset (insertion sort in place). */
void func_800964B0(s32 *list) {
    EffectCommand4 *first;
    EffectCommand4 *p;
    EffectCommand4 swap;

    p = (EffectCommand4 *)list;
    first = p;
    while (p[1].a != 0) {
        if ((u32)p[0].b > (u32)p[1].b) {
            swap.a = p[0].a;
            swap.b = p[0].b;
            swap.c = p[0].c;
            swap.d = p[0].d;
            p[0].a = p[1].a;
            p[0].b = p[1].b;
            p[0].c = p[1].c;
            p[0].d = p[1].d;
            p[1].a = swap.a;
            p[1].b = swap.b;
            p[1].c = swap.c;
            p[1].d = swap.d;
            if (first < p) {
                p--;
            }
        } else {
            p++;
        }
    }
}

/* Submit the current four-word command list; -1 when there is nothing to
 * send or its ring slot is still busy. */
s32 func_800965A4(void) {
    s32 *list;

    list = (s32 *)((u8 *)D_8009D3C0 + D_8009BE44 * 0x580);
    if (*list != 0 && D_8009C624[D_8009BE44] == NULL) {
        func_800964B0(list);
        D_8009D808 = 0;
        D_8009C624[D_8009BE44] = list;
        D_8009BE44 = (D_8009BE44 + 1) & 0xF;
        return 0;
    }
    D_8009D808 = 0;
    return -1;
}

/* Frames queued between the writer and reader (ring of 16). */
s32 func_80096668(void) {
    s32 pending;

    pending = D_8009BE44 - D_8009BCB8;
    if (pending < 0) {
        pending += 0x10;
    }
    return pending;
}

/* Drain the queued frames, waiting for vertical sync between them. */
void func_80096694(void) {
    do {
        VSync(0);
        func_800967E4();
    } while (func_80096668() != 0);
}

/* Read a host-file request list, retrying each call up to eight times. */
void func_800966CC(EffectCommand4 *request) {
    s32 fd;
    s32 i;

    D_8009BE48 = 0;
    D_8009CCB0 = 0;
    D_8009CCA8 = 0;
    D_8009CCA0 = 0;
    for (; request->a != 0; request++) {
        for (i = 0; i < 8; i++) {
            fd = PCopen((char *)request->a, 0, 0);
            if (fd != -1) {
                break;
            }
        }
        if (fd == -1) {
            continue;
        }
        PClseek(fd, request->b, 0);
        for (i = 0; i < 8; i++) {
            if (func_8004C398(fd, (void *)request->d, request->c) != 0) {
                break;
            }
        }
        for (i = 0; i < 8; i++) {
            if (PCclose(fd) == 0) {
                break;
            }
        }
    }
}

/* Step the stream queue: from disc, advance the reader and start the next
 * queued list when idle; from the host, read the next list at once. */
s32 func_800967E4(void) {
    s32 first;
    s32 second;
    s32 status;

    status = 0;
    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        status = func_800968E0();
        if (status == 0 && D_8009D788[D_8009BCB8] != NULL) {
            func_8009699C(D_8009D788[D_8009BCB8]);
        }
    } else if (D_8009C624[D_8009BCB8] != NULL) {
        func_800966CC(D_8009C624[D_8009BCB8]);
        D_8009C624[D_8009BCB8] = NULL;
        D_8009BCB8 = (D_8009BCB8 + 1) & 0xF;
    }
    return status;
}

/* Step the stream reader: 0 idle, 1 busy, 2 finished a frame, 3 error. */
s32 func_800968E0(void) {
    switch (D_8009CD44) {
    case 0:
        return 0;
    case 4:
        if (--D_8009BD2C == 0) {
            D_8009CD44++;
        }
    case 1:
    case 2:
    case 3:
        return 1;
    case 5:
        D_8009CD44 = 0;
        D_8009D788[D_8009BCB8] = NULL;
        D_8009BCB8 = (D_8009BCB8 + 1) & 0xF;
        return 2;
    default:
        return 3;
    }
}

/* Start reading a disc request list: seek to its first sector. */
void func_8009699C(EffectCommand3 *request) {
    s32 sector;

    sector = request->a;
    D_8009CD44 = 1;
    D_8009D3BC = request;
    D_8009D3BC = request + 1;
    D_8009BE48 = 0;
    D_8009CCB0 = 0;
    D_8009CCA8 = 0;
    D_8009CCA0 = 0;
    D_8009D7F4 = sector;
    D_8009D614 = sector;
    D_8009D56C = (u32)(request->b + 0x7FF) >> 11;
    D_8009CEB8 = request->b;
    D_8009C590 = request->c;
    CdIntToPos(sector, &D_8009CEBC);
    CdSyncCallback(func_80096A6C);
    CdControlF(CdlSetloc, (u8 *)&D_8009CEBC);
}

/* CD command-complete callback of the stream reader: after the seek start
 * reading, and recover from errors by pausing and seeking again. */
void func_80096A6C(s32 status, u8 *result) {
    if (status == 2) {
        switch (D_8009CD44) {
        case 1:
            D_8009CD44 = 2;
            D_8009BCCC[2] = 0;
            D_8009BCCC[1] = 0;
            D_8009BCCC[0] = 0;
            CdReadyCallback(func_80096C0C);
            CdControlF(0x1B, NULL);
            break;
        case 3:
            if (D_8009D614 == 0) {
                D_8009CD44 = 4;
                D_8009BD2C = 1;
                CdSyncCallback(NULL);
            }
            break;
        case 10:
            if (result[0] & 0x10) {
                CdControlF(1, NULL);
            } else {
                CdControlF(0x13, NULL);
                D_8009CD44 = 0xB;
            }
            break;
        case 11:
            D_8009CD44 = 0xC;
            CdControlF(CdlPause, NULL);
            break;
        case 12:
            D_8009CD44 = 1;
            CdIntToPos(D_8009D7F4, &D_8009CEBC);
            CdControlF(CdlSetloc, (u8 *)&D_8009CEBC);
            break;
        }
    } else if (result[0] & 0x10) {
        D_8009CD44 = 0xA;
        D_8009CCA8++;
        CdControlF(1, NULL);
    } else {
        D_8009CD44 = 0xB;
        CdControlF(0x13, NULL);
    }
}

/* CD data-ready callback of the stream reader: copy the sector to the
 * request's destination and continue with the next request, seeking when it
 * is not close ahead; pause at the end of the list. */
void func_80096C0C(s32 status, u8 *result) {
    EffectCommand3 *request;
    s32 sector;
    s32 next;

    if (status == 1) {
        CdGetSector(D_8009BCCC, 3);
        sector = CdPosToInt((CdlLOC *)D_8009BCCC);
        if (sector == D_8009D7F4) {
            if (D_8009D614 == sector) {
                if (D_8009CEB8 < 0x800) {
                    CdGetSector((void *)D_8009C590, D_8009CEB8 / 4);
                    CdGetSector(D_8009D7D4, (0x800 - D_8009CEB8) / 4);
                } else {
                    CdGetSector((void *)D_8009C590, 0x200);
                    D_8009CEB8 -= 0x800;
                }
                if (--D_8009D56C != 0) {
                    D_8009D614++;
                    D_8009C590 += 0x800;
                } else {
                    request = D_8009D3BC++;
                    next = request->a;
                    D_8009D614 = next;
                    D_8009D56C = (u32)(request->b + 0x7FF) >> 11;
                    D_8009CEB8 = request->b;
                    D_8009C590 = request->c;
                    if (next != 0) {
                        if (next - D_8009D7F4 >= 0x13) {
                            D_8009D7F4 = next;
                            D_8009CD44 = 1;
                            CdIntToPos(next, &D_8009CEBC);
                            CdControlF(CdlSetloc, (u8 *)&D_8009CEBC);
                            return;
                        }
                    } else {
                        D_8009CD44 = 3;
                        CdReadyCallback(NULL);
                        CdControlF(CdlPause, NULL);
                    }
                }
            }
            D_8009D7F4++;
            return;
        }
        D_8009CCA0++;
        CdReadyCallback(NULL);
        if (result[0] & 0x10) {
            D_8009CD44 = 0xA;
            D_8009CCA8++;
            CdControlF(1, NULL);
        } else {
            D_8009CD44 = 0xB;
            CdControlF(0x13, NULL);
        }
    } else {
        D_8009CCA0++;
        CdReadyCallback(NULL);
        if (result[0] & 0x10) {
            D_8009CD44 = 0xA;
            D_8009CCA8++;
            CdControlF(1, NULL);
        } else {
            D_8009CD44 = 0xB;
            CdControlF(0x13, NULL);
        }
    }
}

/* Place a camera orbiting above a position: look at its height from
 * `distance` along the angle, with the up direction rolled by the angle. */
#ifdef NON_MATCHING /* the scratch vector address is shared by all three x stores */
void func_80096F18(LookAt *view, VECTOR *position, s32 distance, SVECTOR *angle) {
    view->target.vx = 0;
    view->target.vz = 0;
    view->target.vy = position->vy >> 12;
    SCRATCH_SVECTOR->vx = angle->vx;
    SCRATCH_SVECTOR->vz = 0;
    SCRATCH_SVECTOR->vy = angle->vy;
    func_8004A92C(SCRATCH_SVECTOR, SCRATCH_MATRIX_A);
    SCRATCH_VECTOR[0].vx = 0;
    SCRATCH_VECTOR[0].vy = 0;
    SCRATCH_VECTOR[0].vz = -(distance >> 12);
    ApplyMatrixLV(SCRATCH_MATRIX_A, &SCRATCH_VECTOR[0], &SCRATCH_VECTOR[1]);
    view->eye.vx = SCRATCH_VECTOR[1].vx;
    view->eye.vy = view->target.vy + SCRATCH_VECTOR[1].vy;
    view->eye.vz = SCRATCH_VECTOR[1].vz;
    SCRATCH_SVECTOR->vx = 0;
    SCRATCH_SVECTOR->vy = angle->vy;
    SCRATCH_SVECTOR->vz = angle->vz;
    func_8004A92C(SCRATCH_SVECTOR, SCRATCH_MATRIX_A);
    SCRATCH_SVECTOR->vx = 0;
    SCRATCH_SVECTOR->vy = -0x1000;
    SCRATCH_SVECTOR->vz = 0;
    ApplyMatrix(SCRATCH_MATRIX_A, SCRATCH_SVECTOR, &view->up);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80096F18);
#endif

/* Recover rotation angles (yaw, then pitch, then roll) from a matrix. */
void func_80097070(MATRIX *m, SVECTOR *angle) {
    if (m->m[2][0] | m->m[2][2]) {
        angle->vy = ratan2(m->m[2][0], m->m[2][2]) & 0xFFF;
        *SCRATCH_MATRIX_A = *m;
        *SCRATCH_MATRIX_B = *(MATRIX *)&D_8009A180;
        func_8004AFEC(angle->vy, SCRATCH_MATRIX_B);
        MulMatrix0(SCRATCH_MATRIX_A, SCRATCH_MATRIX_B, SCRATCH_MATRIX_C);
        angle->vx = ratan2(SCRATCH_MATRIX_C->m[1][2], SCRATCH_MATRIX_C->m[1][1]);
        *SCRATCH_MATRIX_B = *(MATRIX *)&D_8009A180;
        func_8004AE4C(angle->vx, SCRATCH_MATRIX_B);
        MulMatrix0(SCRATCH_MATRIX_C, SCRATCH_MATRIX_B, SCRATCH_MATRIX_A);
        angle->vz = -ratan2(SCRATCH_MATRIX_A->m[1][0], SCRATCH_MATRIX_A->m[1][1]);
    }
}

/* Build the camera matrix looking from the eye to the target. */
void func_80097244(void *arg) {
    LookAt *view;

    view = arg;
    LOOKAT_SCRATCH->work.vx = -view->eye.vx + view->target.vx;
    LOOKAT_SCRATCH->work.vy = -view->eye.vy + view->target.vy;
    LOOKAT_SCRATCH->work.vz = -view->eye.vz + view->target.vz;
    func_80048D7C(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->forward);
    func_8004A480(&LOOKAT_SCRATCH->forward, &view->up, &LOOKAT_SCRATCH->work);
    func_80048D7C(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->right);
    func_8004A480(&LOOKAT_SCRATCH->forward, &LOOKAT_SCRATCH->right, &LOOKAT_SCRATCH->work);
    func_80048D7C(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->up);
    D_8009C808.m[0][0] = LOOKAT_SCRATCH->right.vx;
    D_8009C808.m[0][1] = LOOKAT_SCRATCH->right.vy;
    D_8009C808.m[0][2] = LOOKAT_SCRATCH->right.vz;
    D_8009C808.m[1][0] = LOOKAT_SCRATCH->up.vx;
    D_8009C808.m[1][1] = LOOKAT_SCRATCH->up.vy;
    D_8009C808.m[1][2] = LOOKAT_SCRATCH->up.vz;
    D_8009C808.m[2][0] = LOOKAT_SCRATCH->forward.vx;
    D_8009C808.m[2][1] = LOOKAT_SCRATCH->forward.vy;
    D_8009C808.m[2][2] = LOOKAT_SCRATCH->forward.vz;
    LOOKAT_SCRATCH->eye.vx = -view->eye.vx;
    LOOKAT_SCRATCH->eye.vy = -view->eye.vy;
    LOOKAT_SCRATCH->eye.vz = -view->eye.vz;
    LOOKAT_SCRATCH->view = D_8009C808;
    ApplyMatrix(&LOOKAT_SCRATCH->view, &LOOKAT_SCRATCH->eye, &LOOKAT_SCRATCH->work);
    TransMatrix(&D_8009C808, &LOOKAT_SCRATCH->work);
}

/* Build the camera matrix from the camera angle and eye position. */
void func_80097440(void *arg) {
    SVECTOR *eye;

    eye = arg;
    *SCRATCH_MATRIX_A = *(MATRIX *)&D_8009A180;
    *SCRATCH_MATRIX_B = *SCRATCH_MATRIX_A;
    *SCRATCH_MATRIX_C = *SCRATCH_MATRIX_A;
    func_8004AE4C(-D_8009BD38.vx, SCRATCH_MATRIX_A);
    func_8004AFEC(-D_8009BD38.vy, SCRATCH_MATRIX_B);
    RotMatrixZ(-D_8009BD38.vz, SCRATCH_MATRIX_C);
    MulMatrix0(SCRATCH_MATRIX_A, SCRATCH_MATRIX_B, SCRATCH_MATRIX_D);
    MulMatrix0(SCRATCH_MATRIX_C, SCRATCH_MATRIX_D, &D_8009C808);
    SCRATCH_SVECTOR->vx = -eye->vx;
    SCRATCH_SVECTOR->vy = -eye->vy;
    SCRATCH_SVECTOR->vz = -eye->vz;
    *SCRATCH_MATRIX_A = D_8009C808;
    ApplyMatrix(SCRATCH_MATRIX_A, SCRATCH_SVECTOR, SCRATCH_VECTOR);
    TransMatrix(&D_8009C808, SCRATCH_VECTOR);
}

/* Allocate and clear the 64 actor slots. */
void func_8009766C(void) {
    D_8009BE24 = func_80031BDC(0x2000, 0);
    func_800976C8();
}

/* Free the actor slots. */
void func_800976A0(void) {
    func_800320E8(D_8009BE24);
}

/* Mark every actor slot free. */
void func_800976C8(void) {
    WorldmapActor *actor;
    s32 i;

    for (i = 0; i < 0x40; i++) {
        actor = &D_8009BE24[i];
        actor->handle = 0;
        actor->kind = 0;
        actor->update = 0;
    }
}

/* Change an actor's kind and clear its command. */
void func_800976FC(s32 kind, s32 index) {
    D_8009BE24[index].command = 0;
    D_8009BE24[index].kind = kind;
}

/* Start an actor in the first free slot. */
void func_80097718(s32 kind, s32 update) {
    WorldmapActor *actor;
    s32 i;

    for (i = 0; i < 0x40; i++) {
        actor = &D_8009BE24[i];
        if (actor->update == 0) {
            actor->command = 0;
            actor->command_arg = 0;
            actor->unk4 = 0;
            actor->kind = kind;
            actor->update = update;
            actor->state = 0;
            actor->wait = 0;
            return;
        }
    }
}

/* Send command 1 with an argument unless one is pending; 1 when sent. */
s32 func_80097770(s32 index, s32 arg) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 == 0) {
        actor->command = 1;
        actor->unk4 = arg;
        return 1;
    }
    return 0;
}

/* Send command 3. */
void func_800977A8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->command = 3;
}

/* Send command 4. */
void func_800977C4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->command = 4;
}

/* Send command 2 with an argument. */
void func_800977E0(s32 index, s16 arg) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->command = 2;
    actor->command_arg = arg;
}

/* Run every active actor's pending command: 0 start, 1 step, 2 wait, 3 idle,
 * 4 release its handle. */
void func_80097800(void) {
    WorldmapActor *actor;
    s32 i;

    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        if (actor->update != 0) {
            switch (actor->command) {
            case 3:
                break;
            case 0:
                actor->command = ((ActorFunc)actor->kind)(i);
                break;
            case 1:
                actor->command = ((ActorFunc)actor->update)(i);
                break;
            case 2:
                if (--actor->command_arg <= 0) {
                    actor->command = 1;
                }
                break;
            case 4:
                if (actor->handle != 0) {
                    func_800230A8(actor->handle);
                }
                break;
            }
        }
    }
}

/* Allocate both 2048-triangle terrain packet buffers and initialise them. */
#ifdef NON_MATCHING /* loop counter increment scheduled late */
void func_800978FC(void) {
    PolyFT3 *prim;
    s32 i;
    struct {
        s32 words[4];
    } *from, *to, *end;

    D_8009BC38[1] = func_80031BDC(0x10000, 1);
    D_8009BCB0[1] = func_80031BDC(0x10000, 1);
    prim = D_8009BC38[1];
    for (i = 0; i < 0x800; i++) {
        ((u8 *)prim)[3] = 7;
        prim->code = 0x24;
        prim->r0 = 0x80;
        prim->g0 = 0x80;
        prim->b0 = 0x80;
        prim++;
    }
    from = D_8009BC38[1];
    to = D_8009BCB0[1];
    end = (void *)((u8 *)from + 0x10000);
    do {
        *to++ = *from++;
    } while (from != end);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800978FC);
#endif

/* Upload the terrain texture image (its buffer is then reused for the
 * palettes), build the faded terrain palettes and their CLUT and texture
 * page ids. */
void func_800979C8(void) {
    RECT rect;
    u16 *cluts;
    u16 *faded;
    s32 i;
    s32 x;
    s32 y;

    cluts = func_80032E88(D_8009C59C, 1);
    func_8002DD20(cluts);
    DrawSync(0);
    func_800320E8(cluts);
    func_800320E8(D_8009C59C);
    cluts = func_80031BDC(0x400, 1);
    faded = func_80031BDC(0x8000, 1);
    rect.x = 0;
    rect.y = 0x1E0;
    rect.w = 0x100;
    rect.h = 2;
    StoreImage(&rect, cluts);
    DrawSync(0);
    func_800931D8(cluts, faded, 0x20, D_8009BB48);
    func_800931D8(cluts + 0x100, faded + 0x2000, 0x20, D_8009BB48);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x100;
    rect.h = 0x40;
    LoadImage(&rect, faded);
    DrawSync(0);
    for (i = 0; i < 0x40; i++) {
        D_8009CCB4[i] = GetClut(rect.x, rect.y);
        rect.y++;
    }
    for (x = 0x200, y = 0, i = 0; i < 4; i++) {
        D_8009CD54[i] = GetTPage(1, 0, x, y);
        x += 0x80;
    }
    for (x = 0x180, y = 0x100, i = 4; i < 7; i++) {
        D_8009CD54[i] = GetTPage(1, 0, x, y);
        x += 0x80;
    }
    func_800320E8(faded);
    func_800320E8(cluts);
}

/* Reset the terrain loader around a position. */
void func_80097BC0(VECTOR *position) {
    s32 i;

    D_8009D534 = D_8009A180;
    for (i = 0xFF; i >= 0; i--) {
        D_8009C184[i] = NULL;
    }
    TERRAIN_ORIGIN.vx = position->vx & 0x7FFFFF;
    TERRAIN_ORIGIN.vy = 0;
    D_8009C5BC = 0;
    D_8009C618 = 0x400;
    TERRAIN_ORIGIN.vz = position->vz & 0x7FFFFF;
    D_8009C838.vx = 2;
    D_8009C838.vy = 0;
    D_8009C838.vz = 2;
    func_800981C8((Camera *)position);
    func_80097DC0();
}

/* Reset the terrain loader around the camera. */
void func_80097CB8(Camera *camera) {
    s32 i;

    D_8009D534 = D_8009A180;
    for (i = 0xFF; i >= 0; i--) {
        D_8009C184[i] = NULL;
    }
    D_8009C5BC = 0;
    D_8009C618 = 0x400;
    func_800981C8(camera);
    func_80097DC0();
}

/* Free every loaded terrain block. */
void func_80097D64(void) {
    s32 i;

    for (i = 0; i < 0x100; i++) {
        if (D_8009C184[i] != NULL) {
            func_800320E8(D_8009C184[i]);
        }
    }
}

/* Queue reads of the terrain blocks around the camera that are not loaded:
 * from disc the centre 3x3 first, then the whole 9x9 grid. */
void func_80097DC0(void) {
    s32 first;
    s32 second;
    s32 row;
    s32 column;
    s32 block;
    void *buffer;
    s32 sector;
    s32 path;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        sector = func_800289D0(D_8009BCD8);
        for (row = 3; row < 6; row++) {
            for (column = 3; column < 6; column++) {
                block = D_8009D570.cells[row * 9 + column];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + block, 0x710, (s32)buffer);
                }
            }
        }
        func_8009623C(0, 0, 0);
        func_80096328();
        for (row = 0; row < 9; row++) {
            for (column = 0; column < 9; column++) {
                block = D_8009D570.cells[row * 9 + column];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + block, 0x710, (s32)buffer);
                }
            }
        }
        func_8009623C(0, 0, 0);
        func_80096328();
    } else {
        path = func_80028998(D_8009BCD8);
        for (row = 0; row < 9; row++) {
            for (column = 0; column < 9; column++) {
                block = D_8009D570.cells[row * 9 + column];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_800962B0(path, block << 11, 0x710, (s32)buffer);
                }
            }
        }
        func_800962B0(0, 0, 0, 0);
        func_800965A4();
    }
}

/* Compute the four horizon plane normals. */
void func_80098044(void) {
    OuterProduct0(&D_8009BB6C, &D_8009BB4C, &D_8009C828);
    OuterProduct0(&D_8009BB4C, &D_8009BB7C, &D_8009C844);
    OuterProduct0(&D_8009BB8C, &D_8009BB5C, &D_8009C874);
    OuterProduct0(&D_8009BB5C, &D_8009BB9C, &D_8009C7F0);
}

/* Wrap a position into the map and note the crossed edges (8/4 in x,
 * 2/1 in z); update the camera's block cell. */
void func_800980D4(void *arg) {
    VECTOR *position;
    s32 x;
    s32 z;

    position = arg;
    x = position->vx;
    z = position->vz;
    D_8009D558 = 0;
    if (x < -0x800000) {
        position->vx = x + 0x800000;
        D_8009D558 = 4;
    } else if (x > 0x800000) {
        position->vx = x - 0x800000;
        D_8009D558 = 8;
    }
    if (z < -0x800000) {
        position->vz += 0x800000;
        D_8009D558 |= 1;
    } else if (z > 0x800000) {
        position->vz -= 0x800000;
        D_8009D558 |= 2;
    }
    D_8009C838.vx = (position->vx >> 23) + 2;
    D_8009C838.vz = (position->vz >> 23) + 2;
}

/* Recompute the 9x9 grid of terrain blocks around the camera (keeping the
 * previous grid), wrapping around the map edges. */
void func_800981C8(Camera *camera) {
    s32 width;
    s32 height;
    s32 x;
    s32 z;
    s32 left;
    s32 base;
    s32 i;
    s32 j;
    s16 *cell;

    width = D_8009D160;
    height = D_8009D2B4;
    x = (camera->target.vx >> 12) / 8 - ((D_8009C838.vx + 2) << 8);
    z = (camera->target.vz >> 12) / 8 - ((D_8009C838.vz + 2) << 8);
    if (x < 0) {
        x += width << 8;
    } else if (x > width << 8) {
        x -= width << 8;
    }
    if (z < 0) {
        z += height << 8;
    } else if (z > height << 8) {
        z -= height << 8;
    }
    x >>= 8;
    z >>= 8;
    D_8009D318 = D_8009D570;
    left = x;
    cell = D_8009D570.cells;
    for (j = 8; j != -1; j--) {
        x = left;
        if (z >= height) {
            z = 0;
        }
        base = z * width;
        for (i = 8; i != -1; i--) {
            if (x >= width) {
                x = 0;
            }
            *cell++ = base + x;
            x++;
        }
        z++;
    }
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800983A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800987AC);

/* After the grid moved: free the blocks that left it and queue reads of the
 * new edge blocks, rows from the row-major file and columns from the
 * column-major file; corners from whichever edge changed. */
#ifdef NON_MATCHING /* register allocation of the edge loops */
void func_80098CC0(void) {
    s32 first;
    s32 second;
    s32 i;
    s32 j;
    s32 k;
    s32 start;
    s16 *cell;
    s32 block;
    s32 sector;
    s32 path;
    s32 changed;
    void *buffer;

    for (i = 0; i < 81; i++) {
        block = D_8009D318.cells[i];
        if (D_8009C184[block] != NULL) {
            for (j = 0; j < 81; j++) {
                if (D_8009D570.cells[j] == block) {
                    break;
                }
            }
            if (j == 81) {
                func_800320E8(D_8009C184[block]);
                D_8009C184[block] = NULL;
            }
        }
    }
    changed = 0;
    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        sector = func_800289D0(D_8009BCD8);
        start = 1;
        for (j = 1; j != -1; j--) {
            cell = &D_8009D570.cells[start];
            for (k = 6; k != -1; k--, cell++) {
                block = *cell;
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + block, 0x710, (s32)buffer);
                    changed |= 2;
                }
            }
            start = 0x49;
        }
        sector = func_800289D0(D_8009BD08);
        start = 9;
        for (j = 1; j != -1; j--) {
            cell = &D_8009D570.cells[start];
            for (k = 6; k != -1; k--, cell += 9) {
                block = *cell;
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    changed |= 1;
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + (block % D_8009D160) * D_8009D2B4 + block / D_8009D160,
                                  0x710, (s32)buffer);
                }
            }
            start = 0x11;
        }
        for (i = 0; i < 4; i++) {
            block = D_8009D570.cells[D_8009BBAC[i]];
            if (D_8009C184[block] == NULL) {
                D_8009C184[block] = func_80031BDC(0x710, 0);
                if (changed & 2) {
                    func_8009623C(func_800289D0(D_8009BCD8) + block, 0x710, (s32)D_8009C184[block]);
                } else if (changed & 1) {
                    func_8009623C(func_800289D0(D_8009BD08) + (block % D_8009D160) * D_8009D2B4 +
                                      block / D_8009D160,
                                  0x710, (s32)D_8009C184[block]);
                }
            }
        }
        func_8009623C(0, 0, 0);
        func_80096328();
    } else {
        path = func_80028998(D_8009BCD8);
        start = 1;
        for (j = 1; j != -1; j--) {
            cell = &D_8009D570.cells[start];
            for (k = 6; k != -1; k--, cell++) {
                block = *cell;
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_800962B0(path, block << 11, 0x710, (s32)buffer);
                    changed |= 2;
                }
            }
            start = 0x49;
        }
        path = func_80028998(D_8009BD08);
        start = 9;
        for (j = 1; j != -1; j--) {
            cell = &D_8009D570.cells[start];
            for (k = 6; k != -1; k--, cell += 9) {
                block = *cell;
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    changed |= 1;
                    D_8009C184[block] = buffer;
                    func_800962B0(path,
                                  ((block % D_8009D160) << 11) * D_8009D2B4 + ((block / D_8009D160) << 11),
                                  0x710, (s32)buffer);
                }
            }
            start = 0x11;
        }
        for (i = 0; i < 4; i++) {
            block = D_8009D570.cells[D_8009BBAC[i]];
            if (D_8009C184[block] == NULL) {
                D_8009C184[block] = func_80031BDC(0x710, 0);
                if (changed & 2) {
                    func_800962B0(func_80028998(D_8009BCD8), block << 11, 0x710, (s32)D_8009C184[block]);
                } else if (changed & 1) {
                    func_800962B0(func_80028998(D_8009BD08),
                                  ((block % D_8009D160) << 11) * D_8009D2B4 + ((block / D_8009D160) << 11),
                                  0x710, (s32)D_8009C184[block]);
                }
            }
        }
        func_800962B0(0, 0, 0, 0);
        func_800965A4();
    }
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80098CC0);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_8009932C);

/* Build a terrain block's 9x9 vertices in the scratchpad (heights of
 * water cells follow two travelling sine waves), then draw the block. */
#ifdef NON_MATCHING /* register allocation: the draw arguments are kept in a1/a2 */
void func_80099708(u32 *heights, u32 *ot, s32 depth, SVECTOR *origin) {
    SVECTOR *vertex;
    u32 *cell;
    s32 j;
    s16 (*sine)[2];
    s32 row_phase;
    s32 left;
    s32 z;
    s32 x;
    s32 i;
    s32 phase;
    s32 swell;
    s32 y;

    vertex = (SVECTOR *)0x1F800000;
    cell = heights;
    sine = D_800523F0;
    row_phase = D_8009C618;
    left = origin->vx;
    z = origin->vz;
    for (j = 8; j != -1; j--) {
        x = left;
        swell = sine[row_phase & 0xFFF][0] * 2;
        phase = D_8009C5BC;
        for (i = 8; i != -1; i--) {
            if (*cell & 0x1000) {
                y = (((sine[phase & 0xFFF][0] * swell) >> 20) + ((s32)(*cell << 24) >> 21)) << 16;
            } else {
                y = (s32)(*cell << 24) >> 5;
            }
            *(s32 *)&vertex->vx = y | (x & 0xFFFF);
            vertex->vz = z;
            x += 0x80;
            vertex++;
            cell++;
            phase += 0x200;
        }
        z -= 0x80;
        row_phase += 0x200;
    }
    func_8009980C(heights, ot, depth);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80099708);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_8009980C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80099BFC);
